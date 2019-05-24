// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_LIB_RUSTC_FLAGS_H_
#define DEVTOOLS_GOMA_LIB_RUSTC_FLAGS_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "lib/compiler_flags.h"
#include "lib/flag_parser.h"

namespace devtools_goma {

class RustcFlags : public CompilerFlags {
 public:
  RustcFlags(const std::vector<std::string>& args, const std::string& cwd);
  ~RustcFlags() override = default;

  std::string compiler_name() const override { return "rustc"; }
  CompilerFlagType type() const override { return CompilerFlagType::Rustc; }

  const std::string& target() const { return target_; }

  // Maybe needs to send RUSTFLAGS
  bool IsClientImportantEnv(const char* env) const override { return false; }
  bool IsServerImportantEnv(const char* env) const override { return false; }

  static bool IsRustcCommand(absl::string_view arg);
  static std::string GetCompilerName(absl::string_view arg);

 private:
  std::string target_;
  static void DefineFlags(FlagParser* parser);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_LIB_RUSTC_FLAGS_H_
