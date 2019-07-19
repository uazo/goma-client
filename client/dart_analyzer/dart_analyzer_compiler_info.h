// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_ANALYZER_COMPILER_INFO_H_
#define DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_ANALYZER_COMPILER_INFO_H_

#include <memory>

#include "compiler_info.h"
#include "glog/logging.h"

namespace devtools_goma {

class DartAnalyzerCompilerInfo : public CompilerInfo {
 public:
  explicit DartAnalyzerCompilerInfo(std::unique_ptr<CompilerInfoData> data);
  CompilerInfoType type() const override {
    return CompilerInfoType::DartAnalyzer;
  }
};

inline const DartAnalyzerCompilerInfo& ToDartAnalyzerCompilerInfo(
    const CompilerInfo& compiler_info) {
  DCHECK_EQ(CompilerInfoType::DartAnalyzer, compiler_info.type());
  return static_cast<const DartAnalyzerCompilerInfo&>(compiler_info);
}

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_ANALYZER_COMPILER_INFO_H_
