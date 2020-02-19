// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpp_include_processor_unittest_helper.h"

#include <algorithm>
#include <vector>

#include "absl/strings/str_join.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "gtest/gtest.h"
#include "include_file_utils.h"
#include "lib/file_helper.h"
#include "path.h"

namespace devtools_goma {

void CompareFiles(const std::string& compiler,
                  const std::string& include_file,
                  const std::set<std::string>& expected_files,
                  const std::set<std::string>& actual_files,
                  const std::set<std::string>& allowed_extra_files) {
  std::vector<std::string> matched_files;
  std::vector<std::string> missing_files;
  std::vector<std::string> extra_files;
  std::vector<std::string> nonallowed_extra_files;

  std::set_intersection(expected_files.begin(), expected_files.end(),
                        actual_files.begin(), actual_files.end(),
                        back_inserter(matched_files));
  std::set_difference(expected_files.begin(), expected_files.end(),
                      matched_files.begin(), matched_files.end(),
                      back_inserter(missing_files));
  std::set_difference(actual_files.begin(), actual_files.end(),
                      matched_files.begin(), matched_files.end(),
                      back_inserter(extra_files));
  std::set_difference(extra_files.begin(), extra_files.end(),
                      allowed_extra_files.begin(), allowed_extra_files.end(),
                      back_inserter(nonallowed_extra_files));

  LOG(INFO) << "matched:" << matched_files.size()
            << " extra:" << extra_files.size()
            << " nonallowed extra: " << nonallowed_extra_files.size()
            << " missing:" << missing_files.size();
  LOG_IF(INFO, !extra_files.empty())
      << "extra files: " << absl::StrJoin(extra_files, ", ");
  LOG_IF(INFO, !nonallowed_extra_files.empty())
      << "nonallowed extra files: "
      << absl::StrJoin(nonallowed_extra_files, ", ");
  LOG_IF(INFO, !missing_files.empty())
      << "missing files: " << absl::StrJoin(missing_files, ", ");

  std::string test_contents;
  if (missing_files.size() != 0 || !nonallowed_extra_files.empty()) {
    ReadFileToString(include_file, &test_contents);
  }
  EXPECT_EQ(0U, missing_files.size())
      << "missing inputs found:"
      << " source=" << include_file << " compiler=" << compiler
      << " test_contents=" << test_contents << " files=" << missing_files;

#ifdef __MACH__
  // See: b/26573474
  LOG_IF(WARNING, !nonallowed_extra_files.empty())
      << "nonallowed_extra_files found:"
      << " source=" << include_file << " compiler=" << compiler
      << " test_contents=" << test_contents
      << " files=" << nonallowed_extra_files;
#else
  EXPECT_TRUE(nonallowed_extra_files.empty())
      << "nonallowed_extra_files found:"
      << " source=" << include_file << " compiler=" << compiler
      << " test_contents=" << test_contents
      << " files=" << nonallowed_extra_files;
#endif
}

bool CreateHeaderMapFile(
    const std::string& filename,
    const std::vector<std::pair<std::string, std::string>>& entries) {
  HeaderMap hmap;
  hmap.string_offset = sizeof(hmap) - sizeof(hmap.buckets) +
                       entries.size() * sizeof(HeaderMapBucket);
  hmap.hash_capacity = entries.size();

  std::vector<HeaderMapBucket> buckets(entries.size());

  struct FileEntry {
    std::string key;
    std::string prefix;
    std::string suffix;
  };
  std::vector<FileEntry> string_contents(entries.size());
  int index = 0;

  // Set up data of hmap file.
  uint32_t offset = 1;
  for (const auto& entry : entries) {
    const auto& key = entry.first;
    const auto& filename = entry.second;
    std::string prefix = std::string(file::Dirname(filename)) + "/";
    std::string suffix = std::string(file::Basename(filename));

    auto& strings = string_contents[index];
    strings.key = key;
    strings.prefix = prefix;
    strings.suffix = suffix;

    auto& bucket = buckets[index];
    bucket.key = offset;
    bucket.prefix = offset + key.size() + 1;
    bucket.suffix = bucket.prefix + prefix.size() + 1;
    offset = bucket.suffix + suffix.size() + 1;

    ++index;
  }

  // Write all data to a string buffer before writing to file.
  const size_t data_size = hmap.string_offset + offset;
  std::string data(data_size, '\0');

  auto result = std::copy_n(reinterpret_cast<const char*>(&hmap),
                            sizeof(hmap) - sizeof(hmap.buckets), data.begin());
  result = std::copy_n(reinterpret_cast<const char*>(buckets.data()),
                       sizeof(buckets[0]) * buckets.size(), result);
  for (const auto& strings : string_contents) {
    for (const auto& str : {strings.key, strings.prefix, strings.suffix}) {
      // ++result because first string cannot have offset of 0 from
      // |string_offset|, and there needs to be a 0 after each string.
      result = std::copy_n(str.begin(), str.size(), ++result);
    }
  }

  if (!WriteStringToFile(data, filename)) {
    LOG(ERROR) << "Unable to write to " << filename;
    return false;
  }
  return true;
}

}  // namespace devtools_goma
