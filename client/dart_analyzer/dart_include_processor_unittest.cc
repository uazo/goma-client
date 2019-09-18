// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dart_analyzer/dart_include_processor.h"

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "lib/path_resolver.h"
#include "path.h"

namespace devtools_goma {
TEST(DartIncludeProcessorTest, ParsePackageFile) {
  const std::string kPackageSpec =
      "async:" +
      PathResolver::ResolvePath(file::JoinPathRespectAbsolute("async", "lib")) +
      "\n" + "collection:" +
      PathResolver::ResolvePath(
          file::JoinPathRespectAbsolute("..", "..", "collection")) +
      "\n";
  std::string packages_spec_path = PathResolver::ResolvePath(
      file::JoinPathRespectAbsolute("dart", "lib", "package.packages"));
  absl::string_view cwd = file::Dirname(packages_spec_path);

  absl::flat_hash_map<std::string, std::string> expected_packages_map = {
      {"async", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                    cwd, PathResolver::ResolvePath(
                             file::JoinPathRespectAbsolute("async", "lib"))))},
      {"collection",
       PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
           cwd, PathResolver::ResolvePath(
                    file::JoinPathRespectAbsolute("..", "..", "collection"))))},
  };

  absl::flat_hash_map<std::string, std::string> parsed_map;
  std::string error_reason;
  EXPECT_TRUE(DartIncludeProcessor::ParsePackagesFile(
      kPackageSpec, packages_spec_path, &parsed_map, &error_reason));
  EXPECT_EQ(parsed_map, expected_packages_map);
}

TEST(DartIncludeProcessorTest, ParseDartSourceFile) {
  const std::string kDartHeader =
      R"(// Copyright (c) 2013, the Dart project authors.

@deprecated
library analyzer;

import 'dart:io';

import 'package:analyzer/dart/analysis/features.dart';
import 'package:analyzer/dart/ast/ast.dart'; // Example Comment
import 'package:analyzer/error/error.dart';

import 'package:path/path.dart' as pathos;
import 'lib/library.dart';
import 'src/fakes/a.dart' if (dart.library.zircon) 'src/fakes/b.dart';
part 'src/channel.dart';
export 'src/authentication_challenge.dart';
import 'package:vm/kernel_front_end.dart'
    show createCompilerArgParser, runCompiler, successExitCode; // Example Comment2
import '../import.dart' show Import;
)";
  std::string dart_source_path = PathResolver::ResolvePath(
      file::JoinPathRespectAbsolute("dart", "lib", "analyzer.dart"));
  absl::string_view cwd = file::Dirname(dart_source_path);

  absl::flat_hash_set<std::pair<std::string, std::string>> expected_set = {
      {"dart:io", ""},
      {"analyzer", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                       "dart", "analysis", "features.dart"))},
      {"analyzer", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                       "dart", "ast", "ast.dart"))},
      {"analyzer", PathResolver::ResolvePath(
                       file::JoinPathRespectAbsolute("error", "error.dart"))},
      {"path", "path.dart"},
      {"", PathResolver::ResolvePath(
               file::JoinPathRespectAbsolute(cwd, "lib", "library.dart"))},
      {"", PathResolver::ResolvePath(
               file::JoinPathRespectAbsolute(cwd, "src", "fakes", "a.dart"))},
      {"", PathResolver::ResolvePath(
               file::JoinPathRespectAbsolute(cwd, "src", "fakes", "b.dart"))},
      {"", PathResolver::ResolvePath(
               file::JoinPathRespectAbsolute(cwd, "src", "channel.dart"))},
      {"", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
               cwd, "src", "authentication_challenge.dart"))},
      {"vm", PathResolver::ResolvePath(
                 file::JoinPathRespectAbsolute("kernel_front_end.dart"))},
      {"", PathResolver::ResolvePath(
               file::JoinPathRespectAbsolute(cwd, "..", "import.dart"))},

  };

  absl::flat_hash_set<std::pair<std::string, std::string>> parsed_set;
  std::string error_reason;
  EXPECT_TRUE(DartIncludeProcessor::ParseDartImports(
      kDartHeader, dart_source_path, &parsed_set, &error_reason));
  EXPECT_TRUE(error_reason.empty());
  EXPECT_EQ(parsed_set, expected_set);
}

TEST(DartIncludeProcessorTest, ResolveImports) {
  const absl::flat_hash_map<std::string, std::string> kPackagePathMap = {
      {"analyzer", PathResolver::ResolvePath(
                       file::JoinPathRespectAbsolute("lib", "analyzer"))},
      {"path",
       PathResolver::ResolvePath(file::JoinPathRespectAbsolute("lib", "path"))},
  };
  const absl::flat_hash_map<std::string, std::string> kLibraryPathMap = {
      {"dart:ui", PathResolver::ResolvePath(
                      file::JoinPathRespectAbsolute("dart", "ui", "ui.dart"))}};
  std::vector<std::pair<std::string, std::string>> imports = {
      {"analyzer", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                       "dart", "analysis", "features.dart"))},
      {"analyzer", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                       "dart", "ast", "ast.dart"))},
      {"path", "path.dart"},
      {"", PathResolver::ResolvePath(
               file::JoinPathRespectAbsolute("lib", "library.dart"))},
      {"dart:ui", ""},
  };
  std::vector<std::string> expected_required_files = {
      PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
          "lib", "analyzer", "dart", "analysis", "features.dart")),
      PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
          "lib", "analyzer", "dart", "ast", "ast.dart")),
      PathResolver::ResolvePath(
          file::JoinPathRespectAbsolute("lib", "path", "path.dart")),
      PathResolver::ResolvePath(
          file::JoinPathRespectAbsolute("lib", "library.dart")),
      PathResolver::ResolvePath(
          file::JoinPathRespectAbsolute("dart", "ui", "ui.dart")),
  };
  std::vector<std::string> required_files;
  for (const auto& import_entry : imports) {
    std::string required_file;
    std::string error_reason;
    EXPECT_TRUE(DartIncludeProcessor::ResolveImports(
        kPackagePathMap, kLibraryPathMap, import_entry, &required_file,
        &error_reason));
    EXPECT_TRUE(error_reason.empty());
    required_files.push_back(std::move(required_file));
  }
  EXPECT_EQ(required_files, expected_required_files);
}

TEST(DartIncludeProcessorTest, ImportTokenizer) {
  std::vector<std::string> testStmts = {
      "import 'dart:io';",
      "import 'package:analyzer/dart/analysis/features.dart';",
      "import 'package:path/path.dart' as pathos;",
      "import 'src/fakes/a.dart' if ( dart.library.zircon ) "
      "'src/fakes/b.dart';",
      "part 'src/channel.dart';",
      "import 'package:vm/kernel_front_end.dart' show createCompilerArgParser, "
      "runCompiler, successExitCode;",
  };
  std::vector<std::vector<std::string>> expected = {
      {"import", "'dart:io'", ";"},
      {"import", "'package:analyzer/dart/analysis/features.dart'", ";"},
      {"import", "'package:path/path.dart'", "as", "pathos", ";"},
      {"import", "'src/fakes/a.dart'", "if", "( dart.library.zircon )",
       "'src/fakes/b.dart'", ";"},
      {"part", "'src/channel.dart'", ";"},
      {"import", "'package:vm/kernel_front_end.dart'", "show",
       "createCompilerArgParser", ",", "runCompiler", ",", "successExitCode",
       ";"},
  };
  for (int i = 0; i < testStmts.size() && i < expected.size(); i++) {
    std::vector<std::string> tokens;
    EXPECT_TRUE(DartIncludeProcessor::ImportTokenizer(testStmts[i], &tokens));
    EXPECT_EQ(tokens, expected[i]);
  }
}

TEST(DartIncludeProcessorTest, ImportStmtParserValid) {
  std::vector<std::string> testStmts = {
      "import 'dart:io';",
      "import 'package:analyzer/dart/analysis/features.dart';",
      "import 'package:path/path.dart' as pathos;",
      "import 'src/fakes/a.dart' if ( dart.library.zircon ) "
      "'src/fakes/b.dart';",
      "part 'src/channel.dart';",
      "import 'package:vm/kernel_front_end.dart' show createCompilerArgParser, "
      "runCompiler, successExitCode hide A, B, C;",
      "part of 'fuchsia:a.dart';",
      "import 'fake/a.dart' if ( condB ) 'fake/b.dart' if ( condC ) "
      "'fake/c.dart' as fake;",
      "export 'fake/a.dart' show A, B, C;"};
  std::vector<std::vector<std::string>> expectedImports = {
      {"'dart:io'"},
      {"'package:analyzer/dart/analysis/features.dart'"},
      {"'package:path/path.dart'"},
      {"'src/fakes/a.dart'", "'src/fakes/b.dart'"},
      {"'src/channel.dart'"},
      {"'package:vm/kernel_front_end.dart'"},
      {},
      {"'fake/a.dart'", "'fake/b.dart'", "'fake/c.dart'"},
      {"'fake/a.dart'"}};
  for (int i = 0; i < testStmts.size(); i++) {
    std::vector<std::string> imports;
    std::string error_reason;
    EXPECT_TRUE(DartIncludeProcessor::ImportStmtParser(testStmts[i], &imports,
                                                       &error_reason));
    EXPECT_EQ(imports, expectedImports[i]);
    EXPECT_TRUE(error_reason.empty());
  }
}

TEST(DartIncludeProcessorTest, ImportStmtParserInvalid) {
  std::vector<std::string> testStmts = {
      "import dart:io;",
      "import \"dart:io\" as io1 if (dart.library.io) \"dart:core\";",
      "import 'src/fakes/a.dart' if (dart.library.zircon "
      "'src/fakes/b.dart';",
      "part 'src/channel.dart' if (dart.library.io) 'src/channel2.dart';",
      "import 'package:vm/kernel_front_end.dart' show createCompilerArgParser, "
      "hide A, B, C;",
      "export 'fake/a.dart' as A;",
  };
  for (const auto& stmt : testStmts) {
    std::vector<std::string> imports;
    std::string error_reason;
    EXPECT_FALSE(
        DartIncludeProcessor::ImportStmtParser(stmt, &imports, &error_reason));
  }
}

TEST(DartIncludeProcessorTest, ParseDartYAML) {
  const std::string test_yaml = R"(embedded_libs:
  "dart:async": "async/async.dart"
  "dart:collection": "collection/collection.dart"
  "dart:convert": "convert/convert.dart"
  "dart:core": "core/core.dart"
  "dart:developer": "developer/developer.dart"
  "dart:io": "io/io.dart"
  "dart:isolate": "isolate/isolate.dart"
  "dart:math": "math/math.dart"
  "dart:typed_data": "typed_data/typed_data.dart"
  "dart:ui": "ui/ui.dart"
  # The internal library is needed as some implementations bleed into the public
  # API, e.g. List being Iterable by virtue of implementing
  # EfficientLengthIterable.
  # Not including this library yields analysis errors.
  "dart:_http": "_http/http.dart"
  "dart:_internal": "internal/internal.dart"
  "dart:nativewrappers": "_empty.dart"

analyzer:
  language:
    enableSuperMixins: true
)";
  std::string dart_yaml_path = PathResolver::ResolvePath(
      file::JoinPathRespectAbsolute("dart", "lib", "_embedder.yaml"));
  absl::string_view cwd = file::Dirname(dart_yaml_path);
  absl::flat_hash_map<std::string, std::string> expected_path_map = {
      {"dart:async", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                         cwd, "async", "async.dart"))},
      {"dart:collection",
       PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
           cwd, "collection", "collection.dart"))},
      {"dart:convert", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                           cwd, "convert", "convert.dart"))},
      {"dart:core", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                        cwd, "core", "core.dart"))},
      {"dart:developer",
       PathResolver::ResolvePath(
           file::JoinPathRespectAbsolute(cwd, "developer", "developer.dart"))},
      {"dart:io", PathResolver::ResolvePath(
                      file::JoinPathRespectAbsolute(cwd, "io", "io.dart"))},
      {"dart:isolate", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                           cwd, "isolate", "isolate.dart"))},
      {"dart:math", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                        cwd, "math", "math.dart"))},
      {"dart:typed_data",
       PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
           cwd, "typed_data", "typed_data.dart"))},
      {"dart:ui", PathResolver::ResolvePath(
                      file::JoinPathRespectAbsolute(cwd, "ui", "ui.dart"))},
      {"dart:_http", PathResolver::ResolvePath(file::JoinPathRespectAbsolute(
                         cwd, "_http", "http.dart"))},
      {"dart:_internal",
       PathResolver::ResolvePath(
           file::JoinPathRespectAbsolute(cwd, "internal", "internal.dart"))},
      {"dart:nativewrappers",
       PathResolver::ResolvePath(
           file::JoinPathRespectAbsolute(cwd, "_empty.dart"))},
  };
  absl::flat_hash_map<std::string, std::string> library_path_map;
  std::string error_reason;
  EXPECT_TRUE(DartIncludeProcessor::ParseDartYAML(
      test_yaml, dart_yaml_path, &library_path_map, &error_reason));
  EXPECT_EQ(library_path_map, expected_path_map);
  EXPECT_TRUE(error_reason.empty());
}

}  // namespace devtools_goma
