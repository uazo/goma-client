// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "elf_util.h"

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "client/unittest_util.h"
#include "gtest/gtest.h"

namespace devtools_goma {

TEST(ElfUtilTest, LoadLdSoConfBasic) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("ld.so.conf", "/lib64\n");
  std::vector<std::string> expected_result = {
      "/lib64",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, LoadLdSoConfComment) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("ld.so.conf",
                            "#comment only line\n"
                            " \t#comment after whitespace\n"
                            "/lib64# comment\n");
  std::vector<std::string> expected_result = {
      "/lib64",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, LoadLdSoConfExtraSpace) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("ld.so.conf",
                            "\n"
                            " \t \n"
                            " \t/lib64 \t\n");
  std::vector<std::string> expected_result = {
      "/lib64",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, LoadLdSoConfInclude) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("ld.so.conf",
                            "include ld.so.conf.another\n"
                            "include\tld.so.conf.yetanother\n"
                            "include ld.so.nonexist\n"
                            "include/usr/lib\n");
  tmpdir_util.CreateTmpFile("ld.so.conf.another", "/lib64\n");
  tmpdir_util.CreateTmpFile("ld.so.conf.yetanother", "/lib\n");
  std::vector<std::string> expected_result = {
      "/lib64",
      "/lib",
      "include/usr/lib",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, LoadLdSoConfIncludeWhileCard) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("ld.so.conf",
                            "include subdir/ld.so.*\n"
                            "include nonexist/ld.so.*\n");
  tmpdir_util.CreateTmpFile("subdir/ld.so.0", "/lib64");
  tmpdir_util.CreateTmpFile("subdir/ld.so.1", "/usr/lib");
  tmpdir_util.CreateTmpFile("subdir/ld.so.2", "/lib32");
  std::vector<std::string> expected_result = {
      "/lib64",
      "/usr/lib",
      "/lib32",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, LoadLdSoConfNestedInclude) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("ld.so.conf", "include ld.so.conf.another\n");
  tmpdir_util.CreateTmpFile("ld.so.conf.another",
                            "include ld.so.conf.yetanother\n");
  tmpdir_util.CreateTmpFile("ld.so.conf.yetanother", "/lib64\n");
  std::vector<std::string> expected_result = {
      "/lib64",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, LoadLdSoConfNestedIncludeLoop) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("ld.so.conf",
                            "/lib64\n"
                            "include ld.so.conf.another\n");
  tmpdir_util.CreateTmpFile("ld.so.conf.another",
                            "/lib32\n"
                            "include ld.so.conf.yetanother\n");
  tmpdir_util.CreateTmpFile("ld.so.conf.yetanother",
                            "/usr/lib\n"
                            "include ld.so.conf\n");
  std::vector<std::string> expected_result = {
      "/lib64",
      "/lib32",
      "/usr/lib",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, LoadLdSoConfComplex) {
  TmpdirUtil tmpdir_util("ld_so_conf_test");
  tmpdir_util.CreateTmpFile("subdir/ld.so.0", "/subdir/0");
  tmpdir_util.CreateTmpFile("subdir/ld.so.1", "/subdir/1");
  tmpdir_util.CreateTmpFile("subdir/ld.so.2", "/subdir/2\t ");
  tmpdir_util.CreateTmpFile("ld.so.conf.another",
                            "include \tsubdir/ld.so.*\t \n"
                            "include/usr/lib\n");
  tmpdir_util.CreateTmpFile("ld.so.conf",
                            "/lib64 # having comment after the lib to load\n"
                            " #comment must be ignored\n"
                            "\n"
                            "include\t ld.so.conf.another\t # comment\n");
  std::vector<std::string> expected_result = {
      "/lib64", "/subdir/0", "/subdir/1", "/subdir/2", "include/usr/lib",
  };
  EXPECT_EQ(expected_result, LoadLdSoConf(tmpdir_util.FullPath("ld.so.conf")));
}

TEST(ElfUtilTest, IsInSystemLibraryPath) {
  std::vector<std::string> system_library_paths = {
      "/lib64",
  };
  EXPECT_TRUE(IsInSystemLibraryPath("/lib64/ld-linux-x86-64.so.2",
                                    system_library_paths));
}

TEST(ElfUtilTest, IsInSystemLibraryPathShouldReturnFalseForNonAbsolute) {
  std::vector<std::string> system_library_paths = {
      "lib",
  };
  EXPECT_FALSE(
      IsInSystemLibraryPath("lib/libpthread.so.0", system_library_paths));
}

TEST(ElfUtilTest, IsInSystemLibraryPathShouldReturnFalseForNonMatch) {
  std::vector<std::string> system_library_paths = {
      "/lib",
  };
  EXPECT_FALSE(IsInSystemLibraryPath("/home/goma/lib64/libc.so.6",
                                     system_library_paths));
}

TEST(ElfUtilTest, IsInSystemLibraryPathShouldReturnFalseForNonExactMatch) {
  std::vector<std::string> system_library_paths = {
      "/lib",
  };
  EXPECT_FALSE(IsInSystemLibraryPath("/lib/x86_64-linux-gnu/libpthread.so.0",
                                     system_library_paths));
}

TEST(ElfUtilTest, IsInSystemLibraryPathShouldReturnTrueForLdSo) {
  std::vector<std::string> system_library_paths = {};
  EXPECT_TRUE(
      IsInSystemLibraryPath("/lib/ld-linux.so.2", system_library_paths));
  EXPECT_TRUE(IsInSystemLibraryPath("/lib64/ld-linux-x86-64.so.2",
                                    system_library_paths));
}

}  // namespace devtools_goma
