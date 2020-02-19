// Copyright 2020 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include_file_finder.h"

#include <memory>

#include <gtest/gtest.h>

#include "cpp_include_processor_unittest_helper.h"
#include "file_stat_cache.h"
#include "include_file_utils.h"
#include "path.h"
#include "unittest_util.h"

namespace devtools_goma {

class IncludeFileFinderTest : public testing::Test {
 public:
  void SetUp() override {
    tmpdir_util_ = std::make_unique<TmpdirUtil>("include_file_finder_unittest");
    tmpdir_util_->SetCwd("");
  }

  void CreateTmpFile(const std::string& name, const std::string& content) {
    tmpdir_util_->CreateTmpFile(name, content);
  }

  void CreateTmpDir(const std::string& dirname) {
    tmpdir_util_->MkdirForPath(dirname, true);
  }

 protected:
  std::unique_ptr<TmpdirUtil> tmpdir_util_;
};

TEST_F(IncludeFileFinderTest, LookupFramework) {
  const std::string framework_dir = "EarlGrey.framework";
  CreateTmpDir(framework_dir);
  CreateTmpDir(file::JoinPath(framework_dir, "Headers"));
  const std::string inc =
      file::JoinPath(framework_dir, "Headers", "EarlGrey.h");
  CreateTmpFile(inc, "");

  std::vector<std::string> include_dirs;
  std::vector<std::string> framework_dirs = {tmpdir_util_->realcwd()};
  FileStatCache file_stat_cache;
  IncludeFileFinder finder(tmpdir_util_->realcwd(), /*ignore_case=*/false,
                           &include_dirs, &framework_dirs, &file_stat_cache);

  std::string file_path;
  int dir_index = 0;
  EXPECT_TRUE(finder.Lookup("EarlGrey/EarlGrey.h", &file_path, &dir_index));
  EXPECT_EQ(file::JoinPath(tmpdir_util_->realcwd(), inc), file_path);
  EXPECT_EQ(0, dir_index);
}

TEST_F(IncludeFileFinderTest, LookupFrameworkWithHmap) {
  const std::string framework_dir = "EarlGrey.framework";
  CreateTmpDir(framework_dir);
  CreateTmpDir(file::JoinPath(framework_dir, "Headers"));
  const std::string inc =
      file::JoinPath(framework_dir, "Headers", "EarlGrey.h");
  CreateTmpFile(inc, "");

  CreateTmpDir("hmap_path");
  const std::string other_inc = file::JoinPath("hmap_path", "EarlGrey.h");
  CreateTmpFile(other_inc, "");
  const std::string hmap_path =
      file::JoinPath(tmpdir_util_->realcwd(), "earl_grey.hmap");
  std::vector<std::pair<std::string, std::string>> hmap_entries = {
      std::make_pair("EarlGrey/EarlGrey.h", other_inc),
  };
  EXPECT_TRUE(CreateHeaderMapFile(hmap_path, hmap_entries));

  std::vector<std::string> include_dirs = {
      tmpdir_util_->realcwd(),
      hmap_path,
  };
  std::vector<std::string> framework_dirs = {tmpdir_util_->realcwd()};
  FileStatCache file_stat_cache;
  IncludeFileFinder finder(tmpdir_util_->realcwd(), /*ignore_case=*/false,
                           &include_dirs, &framework_dirs, &file_stat_cache);

  std::string file_path;
  int dir_index = 0;
  EXPECT_TRUE(finder.Lookup("EarlGrey/EarlGrey.h", &file_path, &dir_index));
  // TODO: Update this check once the bug is fixed.
  EXPECT_EQ(other_inc, file_path);
  EXPECT_EQ(1, dir_index);
}

}  // namespace devtools_goma
