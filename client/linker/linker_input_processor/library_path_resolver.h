// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_LIBRARY_PATH_RESOLVER_H_
#define DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_LIBRARY_PATH_RESOLVER_H_

#include <string>
#include <vector>

#include "basictypes.h"

namespace devtools_goma {

class LibraryPathResolverTest;
class LinkerInputProcessorTest;

// Expands library name to full path name (e.g. -lfoo => /usr/lib/libfoo.so).
class LibraryPathResolver {
 public:
  explicit LibraryPathResolver(std::string cwd);
  ~LibraryPathResolver();

  // for -lfoo flag, value is "foo".
  std::string ExpandLibraryPath(const std::string& value) const;
  // e.g. soname = "libc.so.6"
  std::string FindBySoname(const std::string& soname) const;
  std::string FindByFullname(const std::string& fullname) const;
  void PreventSharedLibrary() { static_link_ = true; }
  void SetSyslibroot(const std::string& path) { syslibroot_ = path; }
  void SetSysroot(const std::string& path) { sysroot_ = path; }
  void AppendSearchdirs(const std::vector<std::string>& paths);
  void AddSearchdir(const std::string& path);

  const std::vector<std::string>& searchdirs() const { return searchdirs_; }
  const std::string& cwd() const { return cwd_; }
  const std::string& sysroot() const { return sysroot_; }
  const std::string& syslibroot() const { return syslibroot_; }

 private:
  friend class LibraryPathResolverTest;
  friend class LinkerInputProcessorTest;

  std::string FindByName(const std::string& so_name,
                         const std::string& ar_name) const;
  std::string ResolveLibraryFilePath(const std::string& syslibroot,
                                     const std::string& dirname,
                                     const std::string& so_name,
                                     const std::string& ar_name) const;
  std::string ResolveFilePath(const std::string& syslibroot,
                              const std::string& dirname,
                              const std::string& filename) const;

  std::vector<std::string> searchdirs_;
  std::vector<std::string> fallback_searchdirs_;
  const std::string cwd_;
  bool static_link_;
  // For mac -syslibroot option.
  std::string syslibroot_;
  std::string sysroot_;

  static const char* fakeroot_;

  DISALLOW_COPY_AND_ASSIGN(LibraryPathResolver);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_LIBRARY_PATH_RESOLVER_H_
