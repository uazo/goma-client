// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_LIB_DART_ANALYZER_FLAGS_H_
#define DEVTOOLS_GOMA_LIB_DART_ANALYZER_FLAGS_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "lib/compiler_flags.h"
#include "lib/flag_parser.h"

namespace devtools_goma {

class DartAnalyzerFlags : public CompilerFlags {
 public:
  DartAnalyzerFlags(const std::vector<std::string>& args,
                    const std::string& cwd);
  ~DartAnalyzerFlags() override = default;

  std::string compiler_name() const override { return "dartanalyzer"; }
  CompilerFlagType type() const override {
    return CompilerFlagType::DartAnalyzer;
  }

  // TODO: Confirm that the analyzer doesn't care about env vars.
  bool IsClientImportantEnv(const char* env) const override { return false; }
  bool IsServerImportantEnv(const char* env) const override { return false; }

  static bool IsDartAnalyzerCommand(absl::string_view arg);
  static std::string GetCompilerName(absl::string_view arg);

  std::string dart_sdk() const { return dart_sdk_; }
  bool use_deprecated_package_root() const {
    return use_deprecated_package_root_;
  }
  std::string packages_file() const { return packages_file_; }
  std::string package_root() const { return package_root_; }
  const absl::flat_hash_map<std::string, std::string>& url_mappings() const {
    return url_mappings_;
  }

 private:
  bool SplitURIMapping(absl::string_view raw,
                       std::pair<std::string, std::string>* split) const;

  std::string dart_sdk_;
  bool use_deprecated_package_root_ = false;
  std::string packages_file_;
  std::string package_root_;
  absl::flat_hash_map<std::string, std::string> url_mappings_;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_LIB_DART_ANALYZER_FLAGS_H_
