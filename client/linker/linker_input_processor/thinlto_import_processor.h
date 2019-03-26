// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_THINLTO_IMPORT_PROCESSOR_H_
#define DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_THINLTO_IMPORT_PROCESSOR_H_

#include <set>
#include <string>

#include "gcc_flags.h"

namespace devtools_goma {

class ThinLTOImportProcessor {
 public:
  // Get import files.
  static bool GetIncludeFiles(const std::string& thinlto_index,
                              const std::string& cwd,
                              std::set<std::string>* input_files);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_THINLTO_IMPORT_PROCESSOR_H_
