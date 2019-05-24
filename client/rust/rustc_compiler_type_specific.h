// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_COMPILER_TYPE_SPECIFIC_H_
#define DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_COMPILER_TYPE_SPECIFIC_H_

#include "compiler_type_specific.h"
#include "rust/rustc_compiler_info_builder.h"

namespace devtools_goma {

class RustcCompilerTypeSpecific : public CompilerTypeSpecific {
 public:
  ~RustcCompilerTypeSpecific() override = default;

  RustcCompilerTypeSpecific(const RustcCompilerTypeSpecific&) = delete;
  void operator=(const RustcCompilerTypeSpecific&) = delete;

  bool RemoteCompileSupported(const std::string& trace_id,
                              const CompilerFlags& flags,
                              bool verify_output) const override {
    return true;
  }

  std::unique_ptr<CompilerInfoData> BuildCompilerInfoData(
      const CompilerFlags& flags,
      const std::string& local_compiler_path,
      const std::vector<std::string>& compiler_info_envs) override;

  bool SupportsDepsCache(const CompilerFlags&) const override { return false; }

  // Runs include processor.
  // |trace_id| is passed from compile_task for logging purpose.
  IncludeProcessorResult RunIncludeProcessor(
      const std::string& trace_id,
      const CompilerFlags& compiler_flags,
      const CompilerInfo& compiler_info,
      const CommandSpec& command_spec,
      FileStatCache* file_stat_cache) override;

 private:
  RustcCompilerTypeSpecific() = default;

  RustcCompilerInfoBuilder compiler_info_builder_;

  friend class CompilerTypeSpecificCollection;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_COMPILER_TYPE_SPECIFIC_H_
