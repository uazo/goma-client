// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dart_analyzer/dart_include_processor.h"

#include <queue>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "content.h"
#include "lib/path_resolver.h"
#include "path.h"
#include "yaml.h"

namespace devtools_goma {
namespace {

enum class DFA {
  INIT,              // Initial state.
  EXPECT_FIRST_URI,  // Expecting the first package uri in either <uri> or
                     // <configurableUri>
  EXPECT_CONFIGURATION_URI_OR_AS_OR_DEFERRED,  // Expecting <configurationUri>
                                               // or 'as' or 'deferred'.
  EXPECT_URI_IN_CONFIGURATION,                 // Expecting the package uri in
                                               // <configurationUri>.
  EXPECT_CONDITION,         // Expecting a condition expression.
  EXPECT_URI_ONLY,          // Expecting a dart package uri. There won't be more
                            // package_name tokens after leaving this state.
  EXPECT_AS,                // Expecting an 'as' token.
  EXPECT_IDENTIFIER_IN_AS,  // Expecting a single identifier token after 'as'
                            // token.
  EXPECT_FIRST_IDENTIFIER_IN_LIST,  // Expecting the first identifier from an
                                    // identifier list in <combinator>.
  EXPECT_IDENTIFIER_IN_LIST,  // Expecting following identifiers from an an
                              // identifier list in <combinator>.
  EXPECT_COMMA,        // Expecting the ',' between identifiers in <combinator>.
  EXPECT_COMBINATORS,  // Expecting <combinator>.
  EXPECT_SEMICOLON,    // Expecting the final semicolon.
  FINAL,               // Final state.
};

inline bool IsQuoted(absl::string_view expr) {
  return (absl::StartsWith(expr, "\"") && absl::EndsWith(expr, "\"")) ||
         (absl::StartsWith(expr, "\'") && absl::EndsWith(expr, "\'"));
}

absl::string_view RemoveComment(absl::string_view stmt) {
  if (absl::StrContains(stmt, " // ")) {
    return stmt.substr(0, stmt.find(" // "));
  }
  return stmt;
}

inline bool InsertURIToken(const std::string& token,
                           std::vector<std::string>* imports,
                           std::string* error_reason) {
  if (!IsQuoted(token)) {
    *error_reason = "package name " + token + " is not quoted.";
    return false;
  }
  imports->push_back(token);
  return true;
}

bool ReadEmbededLibs(
    yaml_document_t* document,
    yaml_node_t* node,
    absl::string_view dir,
    absl::flat_hash_map<std::string, std::string>* library_path_map,
    std::string* error_reason) {
  DCHECK(document);
  DCHECK(node);
  if (node->type != YAML_MAPPING_NODE) {
    *error_reason = "embedded_libs should be a mapping node, but it's not.";
    return false;
  }
  for (yaml_node_pair_t* item = node->data.mapping.pairs.start;
       item != node->data.mapping.pairs.top; ++item) {
    yaml_node_t* key = yaml_document_get_node(document, item->key);
    yaml_node_t* val = yaml_document_get_node(document, item->value);
    if (!key || !val) {
      *error_reason = absl::StrCat("null node encountered at index ", item->key,
                                   ":", item->value);
      return false;
    }
    if (key->type != YAML_SCALAR_NODE || val->type != YAML_SCALAR_NODE) {
      *error_reason = absl::StrCat("expecting SCALAR type nodes, but got ",
                                   key->type, ":", val->type);
      return false;
    }
    absl::string_view key_string(
        reinterpret_cast<const char*>(key->data.scalar.value),
        key->data.scalar.length);
    std::string val_string(
        reinterpret_cast<const char*>(val->data.scalar.value),
        val->data.scalar.length);
#ifdef _WIN32
    absl::c_replace(val_string, '/', '\\');
#endif
    library_path_map->emplace(
        key_string, PathResolver::ResolvePath(
                        file::JoinPathRespectAbsolute(dir, val_string)));
  }
  return true;
}

bool ReadPackageEmbededYAML(
    const absl::flat_hash_map<std::string, std::string>& package_path_map,
    absl::flat_hash_map<std::string, std::string>* library_path_map,
    std::set<std::string>* required_files,
    std::string* error_reason) {
  for (const auto& package : package_path_map) {
    std::string yaml_path = PathResolver::ResolvePath(
        file::JoinPathRespectAbsolute(package.second, "_embedder.yaml"));
    std::unique_ptr<Content> yaml_content = Content::CreateFromFile(yaml_path);
    if (!yaml_content) {
      // _embedder.yaml is optional. Continue if it is not readable.
      continue;
    }
    if (!DartIncludeProcessor::ParseDartYAML(yaml_content->ToStringView(),
                                             yaml_path, library_path_map,
                                             error_reason)) {
      return false;
    }
    required_files->insert(std::move(yaml_path));
  }
  return true;
}
}  // namespace

bool DartIncludeProcessor::Run(
    const DartAnalyzerFlags& dart_analyzer_flags,
    const DartAnalyzerCompilerInfo& dart_analyzer_compiler_info,
    std::set<std::string>* required_files,
    std::string* error_reason) {
  absl::flat_hash_map<std::string, std::string> package_path_map;
  absl::flat_hash_map<std::string, std::string> library_path_map;

  // Read packages file if it exists to build package->path map.
  if (!dart_analyzer_flags.packages_file().empty()) {
    std::unique_ptr<Content> package_file_content =
        Content::CreateFromFile(dart_analyzer_flags.packages_file());
    // TODO, parse _embedder.yaml file after parsing .packages
    // file, as it contains additional package->path relationship
    // information.
    if (!DartIncludeProcessor::ParsePackagesFile(
            package_file_content->ToStringView(),
            dart_analyzer_flags.packages_file(), &package_path_map,
            error_reason)) {
      *error_reason = "failed to parse packages file " +
                      dart_analyzer_flags.packages_file() +
                      "due to error: " + *error_reason;
      return false;
    }
    if (!ReadPackageEmbededYAML(package_path_map, &library_path_map,
                                required_files, error_reason)) {
      *error_reason =
          "failed to parse embedded YAML due to error: " + *error_reason;
      return false;
    }
  }
  // Read dart imports in BFS manner.
  std::queue<std::string> work_list;
  for (const auto& dart_source : dart_analyzer_flags.input_filenames()) {
    work_list.emplace(dart_source);
  }
  while (!work_list.empty()) {
    std::string next = work_list.front();
    work_list.pop();
    if (required_files->find(next) != required_files->end()) {
      continue;
    }
    required_files->emplace(next);
    LOG(INFO) << "Read " << next << " from dart include processor work list";
    std::unique_ptr<Content> dart_source_content =
        Content::CreateFromFile(next);
    if (dart_source_content == nullptr) {
      // Dart standard library may not located in desired path. They
      // are part of sdk so it's OK it is not accessible by goma.
      LOG(WARNING) << "dart source " << next << " cannot be read.";
      continue;
    }
    absl::flat_hash_set<std::pair<std::string, std::string>> imports;
    if (!DartIncludeProcessor::ParseDartImports(
            dart_source_content->ToStringView(), next, &imports,
            error_reason)) {
      *error_reason = "failed to parse dart source " + next +
                      " due to error: " + *error_reason;
      return false;
    }

    for (const auto& import_entry : imports) {
      std::string file;
      if (!DartIncludeProcessor::ResolveImports(package_path_map,
                                                library_path_map, import_entry,
                                                &file, error_reason)) {
        *error_reason = "failed to resolve import " + import_entry.first + ":" +
                        import_entry.second + " due to error: " + *error_reason;
        return false;
      }
      if (file.empty()) {
        // Some library imports do not contain resolvable file name. E.g.
        // 'dart:io' is a builtin library which is part of dart sdk. In this
        // case, ResolveImports returns an empty file and it should be skipped.
        continue;
      }
      LOG(INFO) << "Add " << file << " into dart include processor work list";
      work_list.push(std::move(file));
    }
  }
  return true;
}

// static
bool DartIncludeProcessor::ParsePackagesFile(
    absl::string_view packages_spec,
    absl::string_view packages_spec_path,
    absl::flat_hash_map<std::string, std::string>* package_path_map,
    std::string* error_reason) {
  for (const auto& current_line :
       absl::StrSplit(packages_spec, absl::ByAnyChar("\r\n"))) {
    if (current_line.empty()) {
      continue;
    }
    if (current_line.back() == ':') {
      continue;
    }
    if (absl::StrContains(current_line, ":")) {
      absl::string_view path = absl::StripAsciiWhitespace(
          current_line.substr(current_line.find(":") + 1));
      absl::string_view name = absl::StripAsciiWhitespace(
          current_line.substr(0, current_line.find(":")));
      package_path_map->emplace(
          name, PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                    file::Dirname(packages_spec_path), path)));
      continue;
    }
  }
  return true;
}

// static
bool DartIncludeProcessor::ParseDartImports(
    absl::string_view dart_header,
    absl::string_view dart_source_path,
    absl::flat_hash_set<std::pair<std::string, std::string>>* imports,
    std::string* error_reason) {
  // Read dart source line by line instead of using a proper lexer and parser
  // as it is unlikely to have ill-formatted source files due
  // to strict code style requirements in fuchsia.
  // e.g.
  //
  // import 'dart:io';
  // import 'package:analyzer/dart/analysis/features.dart';
  // import 'package:analyzer/dart/ast/ast.dart';
  // import 'package:analyzer/error/error.dart';
  // import 'package:path/path.dart' as pathos;
  // import 'src/fakes/zircon_fakes.dart' if (dart.library.zircon)
  // 'dart:zircon'; part 'src/channel.dart'; import
  // 'package:vm/kernel_front_end.dart'
  //  show createCompilerArgParser, runCompiler, successExitCode;
  // part of one;
  std::string current_line;
  for (const auto& current_line_const :
       absl::StrSplit(dart_header, absl::ByAnyChar("\r\n"))) {
    absl::string_view current_line_stripped =
        absl::StripAsciiWhitespace(RemoveComment(current_line_const));
    if (current_line_stripped.empty())
      continue;
    if (!current_line.empty())
      current_line += " ";
    current_line.append(current_line_stripped.begin(),
                        current_line_stripped.end());
    if (!(absl::StartsWith(current_line, "import ") ||
          absl::StartsWith(current_line, "part ") ||
          absl::StartsWith(current_line, "export "))) {
      current_line.clear();
      continue;
    } else if (!absl::EndsWith(current_line, ";")) {
      // multi-line import
      continue;
    }
    std::vector<std::string> current_imports;
    LOG(INFO) << "Processing import stmt:" << current_line;
    std::string parser_error_reason;
    if (!DartIncludeProcessor::ImportStmtParser(current_line, &current_imports,
                                                &parser_error_reason)) {
      LOG(WARNING) << "import related stmt " << current_line
                   << "cannot be parsed due to error: " + parser_error_reason;
      continue;
    }
    current_line.clear();
    for (const auto& current_import_const : current_imports) {
      absl::string_view current_import = current_import_const;
      if (IsQuoted(current_import)) {
        current_import = current_import.substr(1, current_import.length() - 2);
      } else {
        LOG(ERROR) << "import " << current_import << "should be quoted";
        continue;
      }
      if (absl::StrContains(current_import, ":")) {
        if (absl::StartsWith(current_import, "package:")) {
          // package_name/relative_path
          std::string package_name(current_import.substr(
              current_import.find(":") + 1,
              current_import.find("/") - current_import.find(":") - 1));
          std::string path(
              std::string(current_import.substr(current_import.find("/") + 1)));
#ifdef _WIN32
          absl::c_replace(path, '/', '\\');
#endif
          imports->emplace(package_name, path);
          continue;
        }
        // It's a library import. It will be resolved later using _embedder.yaml
        imports->emplace(current_import, "");
        continue;
      }
      // relative_path like '../filename.dart'
      std::string path = std::string(current_import);
#ifdef _WIN32
      absl::c_replace(path, '/', '\\');
#endif
      imports->emplace("",
                       PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                           file::Dirname(dart_source_path), path)));
      continue;
    }
  }
  return true;
}

// static
bool DartIncludeProcessor::ParseDartYAML(
    absl::string_view yaml_input,
    absl::string_view yaml_path,
    absl::flat_hash_map<std::string, std::string>* library_path_map,
    std::string* error_reason) {
  // Example _embedder.yaml content:
  //
  // embedded_libs:
  //   "dart:async": "async/async.dart"
  //   "dart:collection": "collection/collection.dart"
  //   "dart:convert": "convert/convert.dart"
  //   "dart:core": "core/core.dart"
  //   "dart:developer": "developer/developer.dart"
  //   "dart:io": "io/io.dart"
  //   "dart:isolate": "isolate/isolate.dart"
  //   "dart:math": "math/math.dart"
  //   "dart:typed_data": "typed_data/typed_data.dart"
  //   "dart:ui": "ui/ui.dart"
  //   # The internal library is needed as some implementations bleed into the
  //   public # API, e.g. List being Iterable by virtue of implementing #
  //   EfficientLengthIterable. # Not including this library yields analysis
  //   errors. "dart:_http": "_http/http.dart" "dart:_internal":
  //   "internal/internal.dart" "dart:nativewrappers": "_empty.dart"
  //
  // analyzer:
  //   language:
  //     enableSuperMixins: true
  std::unique_ptr<yaml_parser_t, std::function<void(yaml_parser_t*)>> parser(
      new yaml_parser_t, [](yaml_parser_t* parser) {
        yaml_parser_delete(parser);
        delete parser;
      });

  if (!yaml_parser_initialize(parser.get())) {
    *error_reason = "failed to initialize YAML parser";
    return false;
  }

  yaml_parser_set_input_string(
      parser.get(), reinterpret_cast<const unsigned char*>(yaml_input.data()),
      yaml_input.length());

  std::unique_ptr<yaml_document_t, std::function<void(yaml_document_t*)>>
      document(new yaml_document_t, [](yaml_document_t* document) {
        yaml_document_delete(document);
        delete document;
      });

  if (!yaml_parser_load(parser.get(), document.get())) {
    *error_reason = "yaml failed to load";
    return false;
  }

  yaml_node_t* root = yaml_document_get_root_node(document.get());
  if (!root) {
    *error_reason = "yaml root node does not exist";
    return false;
  }

  if (root->type != YAML_MAPPING_NODE) {
    *error_reason =
        absl::StrCat("yaml root node is not a map node: ", root->type);
    return false;
  }

  for (yaml_node_pair_t* item = root->data.mapping.pairs.start;
       item != root->data.mapping.pairs.top; ++item) {
    yaml_node_t* key = yaml_document_get_node(document.get(), item->key);
    yaml_node_t* val = yaml_document_get_node(document.get(), item->value);
    if (key->type != YAML_SCALAR_NODE) {
      *error_reason = absl::StrCat(
          "expecting scalar type for key node, but got ", key->type);
      return false;
    }
    absl::string_view key_str(
        reinterpret_cast<const char*>(key->data.scalar.value),
        key->data.scalar.length);
    if (key_str == "embedded_libs") {
      if (!ReadEmbededLibs(document.get(), val, file::Dirname(yaml_path),
                           library_path_map, error_reason)) {
        return false;
      }
    }
  }

  return true;
}

// static
bool DartIncludeProcessor::ResolveImports(
    const absl::flat_hash_map<std::string, std::string>& package_path_map,
    const absl::flat_hash_map<std::string, std::string>& library_path_map,
    const std::pair<std::string, std::string>& import_entry,
    std::string* required_file,
    std::string* error_reason) {
  if (import_entry.first.empty()) {
    *required_file = import_entry.second;
    return true;
  }
  if (import_entry.second.empty()) {
    const auto it = library_path_map.find(import_entry.first);
    if (it != library_path_map.end()) {
      *required_file = it->second;
    }
    // Libraries from SDK will not be found in library_path_map. Return
    // true to skip them.
    return true;
  }

  const auto it = package_path_map.find(import_entry.first);
  if (it == package_path_map.end()) {
    *error_reason = "dart package " + import_entry.first + " not found.";
    return false;
  }
  *required_file = PathResolver::ResolvePath(
      file::JoinPathRespectAbsolute(it->second, import_entry.second));
  return true;
}

// static
// ImportStmtParser parses a import related statement using DFA.
bool DartIncludeProcessor::ImportStmtParser(absl::string_view import_stmt,
                                            std::vector<std::string>* imports,
                                            std::string* error_reason) {
  std::vector<std::string> tokens;
  if (!ImportTokenizer(import_stmt, &tokens)) {
    *error_reason =
        absl::StrCat("failed to tokenize import statment ", import_stmt);
    return false;
  }
  bool isExport = false;
  DFA state = DFA::INIT;
  for (const auto& token : tokens) {
    switch (state) {
      case DFA::INIT:
        if (token == "import") {
          state = DFA::EXPECT_FIRST_URI;
        } else if (token == "export") {
          isExport = true;
          state = DFA::EXPECT_FIRST_URI;
        } else if (token == "part") {
          state = DFA::EXPECT_URI_ONLY;
        }
        continue;

      case DFA::EXPECT_FIRST_URI:
        if (!InsertURIToken(token, imports, error_reason)) {
          return false;
        }
        state = DFA::EXPECT_CONFIGURATION_URI_OR_AS_OR_DEFERRED;
        continue;

      case DFA::EXPECT_CONFIGURATION_URI_OR_AS_OR_DEFERRED:
        if (token == "if") {
          // 'import "a.dart" if (condition) "b.dart"
          // or 'export "a.dart" if (condition) "b.dart"
          state = DFA::EXPECT_CONDITION;
          continue;
        }
        if (token == ";") {
          state = DFA::FINAL;
          continue;
        }
        if (token == "show" || token == "hide") {
          state = DFA::EXPECT_FIRST_IDENTIFIER_IN_LIST;
          continue;
        }
        if (!isExport) {
          if (token == "deferred") {
            state = DFA::EXPECT_AS;
            continue;
          }
          if (token == "as") {
            state = DFA::EXPECT_IDENTIFIER_IN_AS;
            continue;
          }
          *error_reason =
              "Unknown token \"" + token + "\" when expecting as/deferred/if/;";
        }
        *error_reason = "Unknown token \"" + token +
                        "\" when expecting if/; in export statement";
        return false;

      case DFA::EXPECT_URI_IN_CONFIGURATION:
        if (!InsertURIToken(token, imports, error_reason)) {
          return false;
        }
        state = DFA::EXPECT_CONFIGURATION_URI_OR_AS_OR_DEFERRED;
        continue;

      case DFA::EXPECT_CONDITION:
        state = DFA::EXPECT_URI_IN_CONFIGURATION;
        continue;

      case DFA::EXPECT_URI_ONLY:
        if (token == "of") {
          // 'part of ...' statement won't introduce new imports.
          // Ignore this statement completely.
          return true;
        }
        if (!InsertURIToken(token, imports, error_reason)) {
          return false;
        }
        state = DFA::EXPECT_SEMICOLON;
        continue;

      case DFA::EXPECT_AS:
        if (token != "as") {
          *error_reason = "expecting token 'as', but got \"" + token + "\"";
          return false;
        }
        state = DFA::EXPECT_IDENTIFIER_IN_AS;
        continue;

      case DFA::EXPECT_IDENTIFIER_IN_AS:
        state = DFA::EXPECT_COMBINATORS;
        continue;

      case DFA::EXPECT_COMBINATORS:
        if (token == ";") {
          state = DFA::FINAL;
          continue;
        }
        if (token == "show" || token == "hide") {
          state = DFA::EXPECT_FIRST_IDENTIFIER_IN_LIST;
          continue;
        }
        *error_reason = "expecting hide/show/;, got \"" + token + "\"";
        return false;

      case DFA::EXPECT_FIRST_IDENTIFIER_IN_LIST:
        if (token == ";" || token == "show" || token == "hide") {
          *error_reason =
              "expecting an indentifier in <conbinator>, got \"" + token + "\"";
          return false;
        }
        state = DFA::EXPECT_COMMA;
        continue;

      case DFA::EXPECT_IDENTIFIER_IN_LIST:
        if (token == "show" || token == "hide") {
          *error_reason =
              "expecting an indentifier in <conbinator>, got \"" + token + "\"";
          return false;
        }
        state = DFA::EXPECT_COMMA;
        continue;

      case DFA::EXPECT_COMMA:
        if (token == ";") {
          state = DFA::FINAL;
          continue;
        }
        if (token == "show" || token == "hide") {
          state = DFA::EXPECT_FIRST_IDENTIFIER_IN_LIST;
          continue;
        }
        if (token == ",") {
          state = DFA::EXPECT_IDENTIFIER_IN_LIST;
          continue;
        }
        *error_reason = "expecting show/hide/,/;, got \"" + token + "\"";
        return false;

      case DFA::EXPECT_SEMICOLON:
        if (token == ";") {
          state = DFA::FINAL;
        } else {
          *error_reason = "expecting ';', but got " + token;
          return false;
        }
        continue;
      default:
        *error_reason = "unknown token " + token;
        return false;
    }
  }
  if (state != DFA::FINAL) {
    *error_reason = "illegal import statement";
    return false;
  }
  return true;
}

// This is not a generic tokenizer. It only process dart import related
// statements. It does not process escape characters as they shall not
// be used in import statements.
bool DartIncludeProcessor::ImportTokenizer(absl::string_view input,
                                           std::vector<std::string>* tokens) {
  std::string current_token = "";
  for (int i = 0; i < input.length(); ++i) {
    if (current_token.empty() &&
        (input[i] == '\"' || input[i] == '\'' || input[i] == '(')) {
      char end = input[i];
      if (end == '(') {
        end = ')';
      }
      current_token += input[i];
      for (++i; i < input.length() && input[i] != end; ++i) {
        current_token += input[i];
      }
      current_token += end;
      tokens->push_back(std::move(current_token));
      current_token.clear();
      continue;
    }
    if (input[i] == ' ' || input[i] == ';' || input[i] == ',') {
      if (!current_token.empty()) {
        tokens->push_back(std::move(current_token));
        current_token.clear();
      }
      if (input[i] != ' ') {
        tokens->emplace_back(1, input[i]);
      }
      continue;
    }
    current_token += input[i];
  }
  if (!current_token.empty()) {
    tokens->push_back(std::move(current_token));
  }
  return true;
}
}  // namespace devtools_goma