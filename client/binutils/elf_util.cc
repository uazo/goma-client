// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "elf_util.h"

#include <glob.h>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "base/path.h"
#include "glog/logging.h"
#include "lib/file_helper.h"
#include "lib/path_util.h"

namespace devtools_goma {

namespace {

bool LoadLdSoConfInternal(const absl::string_view filename,
                          int remaining_depth,
                          absl::flat_hash_set<std::string>* visited_files,
                          std::vector<std::string>* library_paths);

bool ParseInclude(const absl::string_view filename,
                  const absl::string_view pattern,
                  int remaining_depth,
                  absl::flat_hash_set<std::string>* visited_files,
                  std::vector<std::string>* library_paths) {
  std::unique_ptr<glob_t, std::function<void(glob_t*)>> globbuf(new glob_t,
                                                                [](glob_t* g) {
                                                                  globfree(g);
                                                                  delete g;
                                                                });
  int result = glob(
      file::JoinPathRespectAbsolute(file::Dirname(filename), pattern).c_str(),
      0, nullptr, globbuf.get());
  switch (result) {
    case 0:  // success
      for (size_t i = 0; i < globbuf->gl_pathc; ++i) {
        if (!LoadLdSoConfInternal(globbuf->gl_pathv[i], remaining_depth - 1,
                                  visited_files, library_paths)) {
          LOG(WARNING) << "load ld conf internal failed."
                       << " filename=" << filename << " pattern=" << pattern
                       << " path=" << globbuf->gl_pathv[i];
          return false;
        }
      }
      return true;
    case GLOB_NOMATCH:
      // ChromeOS chroot ld.so.conf has a pattern that does not match anything.
      LOG(INFO) << "no files matches."
                << " filename=" << filename << " pattern=" << pattern
                << " result=" << result;
      return true;
    default:
      LOG(WARNING) << "failed to glob."
                   << " filename=" << filename << " pattern=" << pattern
                   << " result=" << result;
      return false;
  }
  // UNREACHABLE.
}

bool LoadLdSoConfInternal(const absl::string_view filename,
                          int remaining_depth,
                          absl::flat_hash_set<std::string>* visited_files,
                          std::vector<std::string>* library_paths) {
  if (remaining_depth <= 0) {
    LOG(ERROR) << "too much nested include.";
    return false;
  }
  if (!visited_files->emplace(filename).second) {
    LOG(INFO) << "already parsed filename=" << filename;
    return true;
  }

  std::string content;
  if (!ReadFileToString(filename, &content)) {
    LOG(ERROR) << "failed to open/read " << filename;
    return false;
  }

  static constexpr absl::string_view kInclude = "include";
  static constexpr absl::string_view kHwcap = "hwcap";

  for (absl::string_view line :
       absl::StrSplit(content, absl::ByAnyChar("\r\n"), absl::SkipEmpty())) {
    // Omit anything after '#'.
    absl::string_view::size_type pos = line.find('#');
    line = line.substr(0, pos);
    line = absl::StripLeadingAsciiWhitespace(line);
    if (line.empty()) {
      continue;
    }
    if (absl::StartsWith(line, kInclude) && line.size() > kInclude.size() &&
        absl::ascii_isspace(line[kInclude.size()])) {
      line.remove_prefix(kInclude.size());
      for (absl::string_view elm :
           absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipEmpty())) {
        if (!ParseInclude(filename, elm, remaining_depth, visited_files,
                          library_paths)) {
          LOG(ERROR) << "failed to parse include."
                     << " filename=" << filename << " line=" << line
                     << " elm=" << elm;
          return false;
        }
      }
      continue;
    }
    if (absl::StartsWith(line, kHwcap) && line.size() > kHwcap.size() &&
        absl::ascii_isspace(line[kHwcap.size()])) {
      // Since we cannot guarantee backend worker spec, we cannot use
      // libraries in hwcap.
      LOG(WARNING) << "non supported line:"
                   << " filename=" << filename << " line=" << line;
      continue;
    }
    library_paths->emplace_back(absl::StripTrailingAsciiWhitespace(line));
  }
  return true;
}

}  // namespace

std::vector<std::string> LoadLdSoConf(const absl::string_view filename) {
  std::vector<std::string> search_paths;
  absl::flat_hash_set<std::string> visited_files;
  DCHECK(IsPosixAbsolutePath(filename));
  static const int kMaxIncludeDepth = 8;
  if (!LoadLdSoConfInternal(filename, kMaxIncludeDepth, &visited_files,
                            &search_paths)) {
    return std::vector<std::string>();
  }
  return search_paths;
}

bool IsInSystemLibraryPath(
    const absl::string_view path,
    const std::vector<std::string>& system_library_paths) {
  static const std::vector<std::string> kTrustedPaths = {"/lib64", "/usr/lib64",
                                                         "/lib", "/usr/lib"};
  if (!IsPosixAbsolutePath(path)) {
    return false;
  }
  // ld.so.
  if (path == "/lib64/ld-linux-x86-64.so.2" || path == "/lib/ld-linux.so.2") {
    return true;
  }
  absl::string_view dirname = file::Dirname(path);
  return (absl::c_find(kTrustedPaths, dirname) != kTrustedPaths.end()) ||
         (absl::c_find(system_library_paths, dirname) !=
          system_library_paths.end());
}

}  // namespace devtools_goma
