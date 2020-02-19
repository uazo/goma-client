// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INCLUDE_PROCESSOR_UNITTEST_HELPER_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INCLUDE_PROCESSOR_UNITTEST_HELPER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace devtools_goma {

void CompareFiles(const std::string& compiler,
                  const std::string& include_file,
                  const std::set<std::string>& expected_files,
                  const std::set<std::string>& actual_files,
                  const std::set<std::string>& allowed_extra_files);

bool CreateHeaderMapFile(
    const std::string& hmap_filename,
    const std::vector<std::pair<std::string, std::string>>& entries);

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INCLUDE_PROCESSOR_UNITTEST_HELPER_H_
