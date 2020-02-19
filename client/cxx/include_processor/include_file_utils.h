// Copyright 2017 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_INCLUDE_FILE_UTILS_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_INCLUDE_FILE_UTILS_H_

#include <string>
#include <utility>
#include <vector>

namespace devtools_goma {

struct HeaderMapBucket {
  uint32_t key = 0;
  uint32_t prefix = 0;
  uint32_t suffix = 0;
};

struct HeaderMap {
  char magic[4] = {'p', 'a', 'm', 'h'};
  uint16_t version = 1;
  uint16_t reserved = 0;
  uint32_t string_offset = 0;
  uint32_t string_count = 0;
  uint32_t hash_capacity = 0;
  uint32_t max_value_length = 0;
  HeaderMapBucket buckets[1] = {{0}};
};

extern const char* GOMA_GCH_SUFFIX;

bool CreateSubframeworkIncludeFilename(
    const std::string& fwdir, const std::string& current_directory,
    const std::string& include_name, std::string* filename);

bool ReadHeaderMapContent(
    const std::string& hmap_filename,
    std::vector<std::pair<std::string, std::string>>* entries);

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_INCLUDE_FILE_UTILS_H_
