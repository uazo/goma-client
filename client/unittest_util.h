// Copyright 2013 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_UNITTEST_UTIL_H_
#define DEVTOOLS_GOMA_CLIENT_UNITTEST_UTIL_H_

#include <string>

#include "absl/time/time.h"

namespace devtools_goma {

class TmpdirUtil {
 public:
  explicit TmpdirUtil(const std::string& id);
  virtual ~TmpdirUtil();
  // Note: avoid CreateFile not to see "CreateFileW not found" on Win.
  virtual void CreateTmpFile(const std::string& path, const std::string& data);
  virtual void CreateEmptyFile(const std::string& path);
  virtual void MkdirForPath(const std::string& path, bool is_dir);

  virtual void RemoveTmpFile(const std::string& path);

  const std::string& tmpdir() const { return tmpdir_; }
  const std::string& cwd() const { return cwd_; }
  std::string realcwd() const;
  void SetCwd(const std::string cwd) { cwd_ = cwd; }
  std::string FullPath(const std::string& path) const;

 private:
  std::string cwd_;
  std::string tmpdir_;
};

std::string GetTestFilePath(const std::string& test_name);

bool UpdateMtime(const std::string& path, absl::Time mtime);

// Takes clang path.
// If GOMATEST_CLANG_PATH is specified, it's preferred.
// Otherwise, we find clang from third_party.
//
// If failed to find clang, empty string is returned.
std::string GetClangPath();

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_UNITTEST_UTIL_H_
