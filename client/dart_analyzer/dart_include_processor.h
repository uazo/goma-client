// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_INCLUDE_PROCESSOR_H_
#define DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_INCLUDE_PROCESSOR_H_

#include <set>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "dart_analyzer/dart_analyzer_compiler_info.h"
#include "dart_analyzer_flags.h"

namespace devtools_goma {

// DartIncludeProcessor will parse the dart source file and packages files
// to determine the files that are required to be uploaded to remote. This
// class will be shared by dart_analyzer and other dart tools in the future.
// TODO: rename generalize "DartAnalyzerFlags" and
// "DartAnalyzerCompilerInfo" so they can be shared by different dart tools
// instead of just dart_analyzer.
class DartIncludeProcessor {
 public:
  bool Run(const DartAnalyzerFlags& dart_analyzer_flags,
           const DartAnalyzerCompilerInfo& dart_analyzer_compiler_info,
           std::set<std::string>* required_files,
           std::string* error_reason);

  static bool ParsePackagesFile(
      absl::string_view packages_spec,
      absl::string_view packages_spec_path,
      absl::flat_hash_map<std::string, std::string>* package_path_map,
      std::string* error_reason);

  static bool ParseDartImports(
      absl::string_view dart_header,
      absl::string_view dart_source_path,
      absl::flat_hash_set<std::pair<std::string, std::string>>* imports,
      std::string* error_reason);

  static bool ParseDartYAML(
      absl::string_view yaml_input,
      absl::string_view yaml_path,
      absl::flat_hash_map<std::string, std::string>* library_path_map,
      std::string* error_reason);

  static bool ResolveImports(
      const absl::flat_hash_map<std::string, std::string>& package_path_map,
      const absl::flat_hash_map<std::string, std::string>& library_path_map,
      const std::pair<std::string, std::string>& import_entry,
      std::string* required_file,
      std::string* error_reason);

  static bool ImportStmtParser(absl::string_view import_stmt,
                               std::vector<std::string>* imports,
                               std::string* error_reason);

  static bool ImportTokenizer(absl::string_view input,
                              std::vector<std::string>* tokens);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_INCLUDE_PROCESSOR_H_
