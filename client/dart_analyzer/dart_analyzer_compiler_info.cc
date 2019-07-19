// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dart_analyzer_compiler_info.h"

namespace devtools_goma {

DartAnalyzerCompilerInfo::DartAnalyzerCompilerInfo(
    std::unique_ptr<CompilerInfoData> data)
    : CompilerInfo(std::move(data)) {
  LOG_IF(DFATAL, !data_->has_dart_analyzer())
      << "No dart_analyzer extension data was found in CompilerInfoData.";
}

}  // namespace devtools_goma
