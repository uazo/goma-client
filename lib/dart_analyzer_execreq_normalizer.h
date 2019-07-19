// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_LIB_DART_ANALYZER_EXECREQ_NORMALIZER_H_
#define DEVTOOLS_GOMA_LIB_DART_ANALYZER_EXECREQ_NORMALIZER_H_

#include "lib/execreq_normalizer.h"

namespace devtools_goma {

class DartAnalyzerExecReqNormalizer : public ConfigurableExecReqNormalizer {
 protected:
  Config Configure(
      int id,
      const std::vector<std::string>& args,
      bool normalize_include_path,
      bool is_linking,
      const std::vector<std::string>& normalize_weak_relative_for_arg,
      const std::map<std::string, std::string>& debug_prefix_map,
      const ExecReq* req) const override;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_LIB_DART_ANALYZER_EXECREQ_NORMALIZER_H_
