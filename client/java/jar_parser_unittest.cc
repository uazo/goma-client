// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <limits.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "absl/memory/memory.h"
#include "filesystem.h"
#include "ioutil.h"
#include "jar_parser.h"
#include "mypath.h"
#include "path.h"
#include "unittest_util.h"
#include "util.h"

namespace devtools_goma {

// Note: Do not assume JDK is installed on Windows.  For Windows build, we use
//       prebuilt JAR files for testing.

class JarParserTest : public testing::Test {
 public:
  void SetUp() override {
    tmpdir_util_ = absl::make_unique<TmpdirUtil>("jar_parser_unittest");
    tmpdir_util_->SetCwd("");
  }

 protected:
  std::string CopyArchiveIntoTestDir(const std::string& test_name,
                                     const std::string& archive) {
    // This module is build\Release\jar_parser_unittest.exe (msvs) or
    // out\Release\jar_parser_unittest.exe (ninja).
    const std::string parent_dir = file::JoinPath(GetMyDirectory(), "..");
    const std::string top_dir = file::JoinPath(parent_dir, "..");
    const std::string test_dir = file::JoinPath(top_dir, "test");
    const std::string source_file =
        file::JoinPath(test_dir, test_name + ".jar");
    const std::string output_file = tmpdir_util_->FullPath(archive);
    CHECK(file::Copy(source_file, output_file, file::Defaults()).ok());
    return output_file;
  }

  std::unique_ptr<TmpdirUtil> tmpdir_util_;
};

TEST_F(JarParserTest, Basic) {
  std::vector<std::string> input_jar_files;

  const std::string& jar = CopyArchiveIntoTestDir("Basic", "foo.jar");
  input_jar_files.push_back(jar);

  JarParser parser;
  std::set<std::string> jar_files;
  parser.GetJarFiles(input_jar_files, tmpdir_util_->tmpdir(), &jar_files);
  ASSERT_EQ(1U, jar_files.size());
  EXPECT_EQ(jar, *jar_files.begin());
}

TEST_F(JarParserTest, ReadManifest) {
  std::vector<std::string> input_jar_files;

  std::vector<std::string> files;
  files.push_back("bar.class");
  const std::string& foo_jar = CopyArchiveIntoTestDir("Basic", "foo.jar");
  const std::string& bar_jar =
      CopyArchiveIntoTestDir("ReadManifest", "bar.jar");

  // Dup should be ignored.
  input_jar_files.push_back(bar_jar);
  input_jar_files.push_back(bar_jar);

  JarParser parser;
  std::set<std::string> jar_files_set;
  parser.GetJarFiles(input_jar_files, tmpdir_util_->tmpdir(), &jar_files_set);
  std::vector<std::string> jar_files(jar_files_set.begin(),
                                     jar_files_set.end());
  ASSERT_EQ(2U, jar_files.size());
  EXPECT_EQ(bar_jar, jar_files[0]);
  EXPECT_EQ(foo_jar, jar_files[1]);
}

TEST_F(JarParserTest, GetJarFilesShouldIgnoreNonExistJarFiles) {
  // Note that ReadManifest.jar has foo.jar and bar.jar in MANIFEST class-path.
  // However, we won't create bar.jar to confirm GetJarFiles ignores bar.jar.
  const std::string& base_jar =
      CopyArchiveIntoTestDir("ReadManifest", "base.jar");
  const std::string& foo_jar = CopyArchiveIntoTestDir("Basic", "foo.jar");

  std::vector<std::string> input_jar_files{
      // Also, there are no "nonexist.jar".  GetJarFiles should also ignore
      // nonexist.jar.
      file::JoinPath(tmpdir_util_->tmpdir(), "nonexist.jar"),
      base_jar,
  };
  JarParser parser;
  std::set<std::string> jar_files_set;
  parser.GetJarFiles(input_jar_files, tmpdir_util_->tmpdir(), &jar_files_set);
  std::set<std::string> expected_jar_files_set{
      base_jar,
      foo_jar,
  };
  EXPECT_EQ(expected_jar_files_set, jar_files_set);
}

}  // namespace devtools_goma
