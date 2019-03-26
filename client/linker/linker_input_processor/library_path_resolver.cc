// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "library_path_resolver.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <glog/logging.h>
#include <glog/stl_logging.h>

#include "absl/strings/match.h"
#include "path.h"

#ifdef _WIN32
#include "posix_helper_win.h"
#endif

namespace devtools_goma {

const char* LibraryPathResolver::fakeroot_ = "";

LibraryPathResolver::LibraryPathResolver(std::string cwd)
    : cwd_(std::move(cwd)), static_link_(false) {
#ifdef __MACH__
  fallback_searchdirs_.push_back("/usr/lib");
  fallback_searchdirs_.push_back("/usr/local/lib");
#endif
}

LibraryPathResolver::~LibraryPathResolver() {
}

std::string LibraryPathResolver::ExpandLibraryPath(
    const std::string& value) const {
  std::string lib_name = "lib";
#ifdef __MACH__
  std::string so_name = lib_name + value + ".dylib";
  std::string ar_name = lib_name + value + ".a";
  // See: linker manual of Mac (-lx).
  if (absl::EndsWith(value, ".o")) {
    so_name = value;
    ar_name = value;
  }
#elif defined(_WIN32)
  absl::string_view ext = file::Extension(value);
  std::string so_name = value;
  if (ext != "tlb") {
    so_name = value + ".tlb";
  }

  std::string ar_name = value;
  if (ext != "lib") {
    ar_name = value + ".lib";
  }
#else
  std::string so_name = lib_name + value + ".so";
  std::string ar_name = lib_name + value + ".a";
  // See: GNU linker manual (-l namespace).
  if (absl::StartsWith(value, ":")) {
    so_name = ar_name = value.substr(1);
  }
#endif
  std::string pathname = FindByName(so_name, ar_name);
  if (pathname.empty()) {
    LOG(INFO) << "-l" << value << " not found in " << searchdirs_;
  }
  return pathname;
}

std::string LibraryPathResolver::FindBySoname(const std::string& soname) const {
  return FindByName(soname, "");
}

std::string LibraryPathResolver::ResolveLibraryFilePath(
    const std::string& syslibroot,
    const std::string& dirname,
    const std::string& so_name,
    const std::string& ar_name) const {
  if (!static_link_) {
    const std::string filename =
        fakeroot_ +
        file::JoinPath(
            syslibroot,
            file::JoinPathRespectAbsolute(
                file::JoinPathRespectAbsolute(cwd_, dirname), so_name));
    VLOG(2) << "check:" << filename;
    if (access(filename.c_str(), R_OK) == 0)
      return filename.substr(strlen(fakeroot_));
  }
  if (ar_name.empty())
    return "";
  const std::string filename =
      fakeroot_ +
      file::JoinPath(
          syslibroot,
          file::JoinPathRespectAbsolute(
              file::JoinPathRespectAbsolute(cwd_, dirname), ar_name));
  VLOG(2) << "check:" << filename;
  if (access(filename.c_str(), R_OK) == 0)
    return filename.substr(strlen(fakeroot_));

  return "";
}

std::string LibraryPathResolver::FindByName(const std::string& so_name,
                                            const std::string& ar_name) const {
  for (const auto& dir : searchdirs_) {
    // Inspite of ld(1) manual, ld won't prepend syslibroot to -L options.
    // I have checked it with dtruss(1).
    const std::string filename =
        ResolveLibraryFilePath("", dir, so_name, ar_name);
    if (!filename.empty())
      return filename;
  }

  for (const auto& dir : fallback_searchdirs_) {
    const std::string filename =
        ResolveLibraryFilePath(syslibroot_, dir, so_name, ar_name);
    if (!filename.empty())
      return filename;
  }

  return "";
}

std::string LibraryPathResolver::ResolveFilePath(
    const std::string& syslibroot,
    const std::string& dirname,
    const std::string& basename) const {
  const std::string filename =
      fakeroot_ +
      file::JoinPath(syslibroot, file::JoinPath(file::JoinPathRespectAbsolute(
                                                    cwd_, dirname),
                                                basename));
  VLOG(2) << "check:" << filename;
  if (access(filename.c_str(), R_OK) == 0)
    return filename.substr(strlen(fakeroot_));

  return "";
}

std::string LibraryPathResolver::FindByFullname(const std::string& name) const {
  {
    std::string filename =
        fakeroot_ + file::JoinPathRespectAbsolute(cwd_, name);
    VLOG(2) << "check:" << filename;
    if (access(filename.c_str(), R_OK) == 0)
      return filename.substr(strlen(fakeroot_));
  }

  const std::string search_name = std::string(file::Basename(name));
  for (const auto& dir : searchdirs_) {
    // Inspite of ld(1) manual, ld won't prepend syslibroot to -L options.
    const std::string filename = ResolveFilePath("", dir, search_name);
    if (!filename.empty())
      return filename;
  }

  for (const auto& dir : fallback_searchdirs_) {
    const std::string filename = ResolveFilePath(syslibroot_, dir, search_name);
    if (!filename.empty())
      return filename;
  }

  return "";
}

void LibraryPathResolver::AppendSearchdirs(
    const std::vector<std::string>& searchdirs) {
  copy(searchdirs.begin(), searchdirs.end(), back_inserter(searchdirs_));
}

void LibraryPathResolver::AddSearchdir(const std::string& searchdir) {
  searchdirs_.push_back(searchdir);
}

}  // namespace devtools_goma
