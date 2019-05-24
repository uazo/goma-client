// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_INCLUDE_PROCESSOR_H_
#define DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_INCLUDE_PROCESSOR_H_

#include <set>
#include <string>

#include "rust/rustc_compiler_info.h"
#include "rustc_flags.h"

namespace devtools_goma {

class RustcIncludeProcessor {
 public:
  bool Run(const RustcFlags& rustc_flags,
           const RustcCompilerInfo& rustc_compiler_info,
           std::set<std::string>* required_files,
           std::string* error_reason);

  static bool ParseRustcDeps(absl::string_view deps_info,
                             std::set<std::string>* required_files,
                             std::string* error_reason);

  static bool RewriteArgs(const std::vector<std::string>& old_args,
                          const std::string& dep_file,
                          std::vector<std::string>* new_args,
                          std::string* error_reason);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_INCLUDE_PROCESSOR_H_
