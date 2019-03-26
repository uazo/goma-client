// Copyright 2013 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "framework_path_resolver.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <glog/logging.h>
#include <glog/stl_logging.h>

#include "path.h"

#ifdef _WIN32
# include "posix_helper_win.h"
#endif

namespace {
static const char* kFrameworkSuffix = ".framework";
}  // anonymous namespace

namespace devtools_goma {

FrameworkPathResolver::FrameworkPathResolver(std::string cwd)
    : cwd_(std::move(cwd)) {
#ifdef __MACH__
  default_searchpaths_.push_back("/Library/Frameworks");
  default_searchpaths_.push_back("/System/Library/Frameworks");
#endif
}

std::string FrameworkPathResolver::FrameworkFile(
    const std::string& syslibroot,
    const std::string& dirname,
    const std::string& name,
    const std::vector<std::string>& candidates) const {
  const std::string path = file::JoinPath(
      syslibroot, file::JoinPathRespectAbsolute(
                      file::JoinPathRespectAbsolute(cwd_, dirname),
                      name + kFrameworkSuffix));

  for (const auto& candidate : candidates) {
    const std::string filename = file::JoinPath(path, candidate);
    VLOG(2) << "check:" << filename;
    if (access(filename.c_str(), R_OK) == 0) {
      return filename;
    }
  }
  return "";
}

// -framework name[.suffix] to filename.
std::string FrameworkPathResolver::ExpandFrameworkPath(
    const std::string& framework) const {
  std::vector<std::string> candidates;
  std::string name = framework;
  size_t found = framework.find_first_of(',');
  if (found != std::string::npos) {
    // -framework name[,suffix] to try name.framework/name_suffix,
    // then name.framework/name.
    name = framework.substr(0, found);
    const std::string suffix = framework.substr(found + 1);
    candidates.push_back(name + "_" + suffix);
    candidates.push_back(name);
  } else {
    candidates.push_back(framework);
  }

  for (const auto& path : searchpaths_) {
    const std::string file = FrameworkFile("", path, name, candidates);
    if (!file.empty()) {
      return file;
    }
  }

  for (const auto& path : default_searchpaths_) {
    const std::string file = FrameworkFile(syslibroot_, path, name, candidates);
    if (!file.empty()) {
      return file;
    }
  }

  return "";
}

void FrameworkPathResolver::AppendSearchpaths(
    const std::vector<std::string>& searchpaths) {
  copy(searchpaths.begin(), searchpaths.end(), back_inserter(searchpaths_));
}

}  // namespace devtools_goma
