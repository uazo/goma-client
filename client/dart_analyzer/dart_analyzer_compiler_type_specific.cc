// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dart_analyzer_compiler_type_specific.h"

#include "dart_analyzer_compiler_info.h"
#include "dart_analyzer_flags.h"
#include "dart_include_processor.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"

namespace devtools_goma {

DartAnalyzerCompilerTypeSpecific::~DartAnalyzerCompilerTypeSpecific() {}

std::unique_ptr<CompilerInfoData>
DartAnalyzerCompilerTypeSpecific::BuildCompilerInfoData(
    const CompilerFlags& flags,
    const std::string& local_compiler_path,
    const std::vector<std::string>& compiler_info_envs) {
  return compiler_info_builder_.FillFromCompilerOutputs(
      flags, local_compiler_path, compiler_info_envs);
}

CompilerTypeSpecific::IncludeProcessorResult
DartAnalyzerCompilerTypeSpecific::RunIncludeProcessor(
    const std::string& trace_id,
    const CompilerFlags& compiler_flags,
    const CompilerInfo& compiler_info,
    const CommandSpec& command_spec,
    FileStatCache* file_stat_cache) {
  DCHECK_EQ(CompilerFlagType::DartAnalyzer, compiler_flags.type());

  const DartAnalyzerFlags& dart_flags =
      static_cast<const DartAnalyzerFlags&>(compiler_flags);
  const DartAnalyzerCompilerInfo& dartanalyzer_compiler_info =
      ToDartAnalyzerCompilerInfo(compiler_info);
  DartIncludeProcessor include_processor;
  std::set<std::string> required_files;
  std::string error_reason;
  if (!include_processor.Run(dart_flags, dartanalyzer_compiler_info,
                             &required_files, &error_reason)) {
    return IncludeProcessorResult::ErrorToLog(error_reason);
  }

  LOG(INFO) << "dart_analyzer required_files: " << required_files;
  return IncludeProcessorResult::Ok(std::move(required_files));
}

bool DartAnalyzerCompilerTypeSpecific::RemoteCompileSupported(
    const std::string& trace_id,
    const CompilerFlags& flags,
    bool verify_output) const {
  return true;
}

}  // namespace devtools_goma
