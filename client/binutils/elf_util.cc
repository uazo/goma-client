// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "elf_util.h"

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "glog/logging.h"

namespace devtools_goma {

std::vector<std::string> ParseLdSoConf(absl::string_view content) {
  std::vector<std::string> ret;

  for (absl::string_view line :
       absl::StrSplit(content, absl::ByAnyChar("\r\n"), absl::SkipEmpty())) {
    // Omit anything after '#'.
    absl::string_view::size_type pos = line.find('#');
    line = line.substr(0, pos);
    line = absl::StripAsciiWhitespace(line);
    if (line.empty()) {
      continue;
    }
    // TODO: support include and hwcap if we need.
    if (absl::StartsWith(line, "include") || absl::StartsWith(line, "hwcap")) {
      LOG(WARNING) << "non supported line:" << line;
      continue;
    }
    ret.push_back(std::string(line));
  }
  return ret;
}

}  // namespace devtools_goma
