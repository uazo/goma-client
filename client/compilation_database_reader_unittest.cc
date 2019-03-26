// Copyright 2016 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include <glog/logging.h>
#include <glog/stl_logging.h>
#include <gtest/gtest.h>
#include <json/json.h>

#include "compilation_database_reader.h"
#include "file_dir.h"
#include "file_helper.h"
#include "path.h"
#include "unittest_util.h"

namespace devtools_goma {

class CompilationDatabaseReaderTest : public testing::Test {
 protected:
  static bool AddCompileOptions(const std::string& source,
                                const std::string& db_path,
                                std::vector<std::string>* clang_args,
                                std::string* build_dir) {
    return CompilationDatabaseReader::AddCompileOptions(
        source, db_path, clang_args, build_dir);
  }

  static bool MakeClangArgsFromCommandLine(
      bool seen_hyphen_hyphen,
      const std::vector<std::string>& args_after_hyphen_hyphen,
      const std::string& input_file,
      const std::string& cwd,
      const std::string& build_path,
      const std::vector<std::string>& extra_arg,
      const std::vector<std::string>& extra_arg_before,
      const std::string& compdb_path,
      std::vector<std::string>* clang_args,
      std::string* build_dir) {
    return CompilationDatabaseReader::MakeClangArgsFromCommandLine(
        seen_hyphen_hyphen, args_after_hyphen_hyphen, input_file, cwd,
        build_path, extra_arg, extra_arg_before, compdb_path, clang_args,
        build_dir);
  }

  static std::string MakeCompilationDatabaseContent(
      const std::string& directory,
      const std::string& command,
      const std::string& file) {
    Json::Value comp;
    comp["directory"] = directory;
    comp["command"] = command;
    comp["file"] = file;

    Json::Value root;
    root.append(comp);

    Json::FastWriter writer;
    return writer.write(root);
  }
};

TEST_F(CompilationDatabaseReaderTest, FindCompilationDatabase) {
  TmpdirUtil tmpdir("compdb_unittest_fcd");
  tmpdir.SetCwd("/");

  std::string ab_rel = file::JoinPath("a", "b");
  std::string ab_abs = tmpdir.FullPath(ab_rel);

  std::string compdb_content = MakeCompilationDatabaseContent(
      ab_abs, "clang -IA -IB -c foo.cc", "foo.cc");

  // The following directories and file are created.
  // /a/b/
  // /c/d/
  //   /compile_commands.json

  tmpdir.MkdirForPath(ab_rel, true);
  tmpdir.MkdirForPath(file::JoinPath("c", "d"), true);
  tmpdir.CreateTmpFile(file::JoinPath("c", "compile_commands.json"),
                       compdb_content);

  const std::string c_abs = tmpdir.FullPath("c");
  const std::string cd_abs = tmpdir.FullPath(file::JoinPath("c", "d"));
  const std::string expected_compdb_path =
      file::JoinPath(c_abs, "compile_commands.json");

  // Set build_path is /c, first input file dir is /a/b
  {
    std::string compdb_path =
        CompilationDatabaseReader::FindCompilationDatabase(c_abs, ab_abs);
    EXPECT_EQ(expected_compdb_path, compdb_path);
  }

  // Set build_path is empty, first input file dir is /c/d.
  {
    std::string compdb_path =
        CompilationDatabaseReader::FindCompilationDatabase(std::string(),
                                                           cd_abs);
    EXPECT_EQ(expected_compdb_path, compdb_path);
  }

  // Set build_path id /c/d, first input file dir is /a/b.
  // Since we shouldn't search ancestor directory of build_path,
  // compilation database should not be found.
  {
    std::string dbpath =
        CompilationDatabaseReader::FindCompilationDatabase(cd_abs, ab_abs);
    EXPECT_TRUE(dbpath.empty());
  }
}

TEST_F(CompilationDatabaseReaderTest, WithCompilationDatabase) {
  TmpdirUtil tmpdir("compdb_unittest");
  tmpdir.SetCwd("/");

  // Make the following directories and files, and move cwd to /a/b.
  // /a/b/
  // /compile_commands.json

  std::string ab_rel = file::JoinPath("a", "b");
  std::string ab_abs = tmpdir.FullPath(ab_rel);

  tmpdir.MkdirForPath(ab_rel, true);

  std::string compdb_content = MakeCompilationDatabaseContent(
      ab_abs, "clang -IA -IB -c foo.cc", "foo.cc");
  tmpdir.CreateTmpFile("compile_commands.json", compdb_content);

  std::string compile_commands_json = tmpdir.FullPath("compile_commands.json");

  std::string dbpath =
      CompilationDatabaseReader::FindCompilationDatabase("", ab_abs);
  EXPECT_EQ(compile_commands_json, dbpath);

  tmpdir.SetCwd(ab_rel);
  std::string foo_path = tmpdir.FullPath("foo.cc");

  std::vector<std::string> clang_args{
      "clang++",
  };
  std::string build_dir;
  EXPECT_TRUE(AddCompileOptions(foo_path, dbpath, &clang_args, &build_dir));

  std::vector<std::string> expected_clang_args{"clang++", "-IA", "-IB", "-c",
                                               "foo.cc"};
  EXPECT_EQ(expected_clang_args, clang_args);
  EXPECT_EQ(ab_abs, build_dir);
}

TEST_F(CompilationDatabaseReaderTest, WithCompilationDatabaseHavingGomaCC) {
  TmpdirUtil tmpdir("compdb_unittest");
  tmpdir.SetCwd("/");

  // Make the following directories and files, and move cwd to /a/b.
  // /a/b/
  // /compile_commands.json

  std::string ab_rel = file::JoinPath("a", "b");
  std::string ab_abs = tmpdir.FullPath(ab_rel);

  tmpdir.MkdirForPath(ab_rel, true);

  std::string compdb_content = MakeCompilationDatabaseContent(
      ab_abs, "/home/goma/goma/gomacc clang -IA -IB -c foo.cc", "foo.cc");
  tmpdir.CreateTmpFile("compile_commands.json", compdb_content);

  std::string compile_commands_json = tmpdir.FullPath("compile_commands.json");

  std::string dbpath =
      CompilationDatabaseReader::FindCompilationDatabase("", ab_abs);
  EXPECT_EQ(compile_commands_json, dbpath);

  tmpdir.SetCwd(ab_rel);
  std::string foo_path = tmpdir.FullPath("foo.cc");

  std::vector<std::string> clang_args{
      "clang++",
  };
  std::string build_dir;
  EXPECT_TRUE(AddCompileOptions(foo_path, dbpath, &clang_args, &build_dir));

  std::vector<std::string> expected_clang_args{"clang++", "-IA", "-IB", "-c",
                                               "foo.cc"};
  EXPECT_EQ(expected_clang_args, clang_args);
  EXPECT_EQ(ab_abs, build_dir);
}

TEST_F(CompilationDatabaseReaderTest,
       WithCompilationDatabaseHavingGomaCCCapitalCaseWithExtension) {
  TmpdirUtil tmpdir("compdb_unittest");
  tmpdir.SetCwd("/");

  // Make the following directories and files, and move cwd to /a/b.
  // /a/b/
  // /compile_commands.json

  std::string ab_rel = file::JoinPath("a", "b");
  std::string ab_abs = tmpdir.FullPath(ab_rel);

  tmpdir.MkdirForPath(ab_rel, true);

  std::string compdb_content = MakeCompilationDatabaseContent(
      ab_abs, "/home/goma/goma/GOMACC.exe clang -IA -IB -c foo.cc", "foo.cc");
  tmpdir.CreateTmpFile("compile_commands.json", compdb_content);

  std::string compile_commands_json = tmpdir.FullPath("compile_commands.json");

  std::string dbpath =
      CompilationDatabaseReader::FindCompilationDatabase("", ab_abs);
  EXPECT_EQ(compile_commands_json, dbpath);

  tmpdir.SetCwd(ab_rel);
  std::string foo_path = tmpdir.FullPath("foo.cc");

  std::vector<std::string> clang_args{
      "clang++",
  };
  std::string build_dir;
  EXPECT_TRUE(AddCompileOptions(foo_path, dbpath, &clang_args, &build_dir));

  std::vector<std::string> expected_clang_args{"clang++", "-IA", "-IB", "-c",
                                               "foo.cc"};
  EXPECT_EQ(expected_clang_args, clang_args);
  EXPECT_EQ(ab_abs, build_dir);
}

TEST_F(CompilationDatabaseReaderTest, WithoutCompilationDatabase) {
  std::vector<std::string> args_after_hyphen_hyphen{"-IA", "-IB"};
  std::string cwd = "/";
  std::vector<std::string> extra_arg{"-IC"};
  std::vector<std::string> extra_arg_before{"-ID"};

  std::vector<std::string> clang_args{"clang"};
  std::string build_dir;
  EXPECT_TRUE(MakeClangArgsFromCommandLine(true, args_after_hyphen_hyphen,
                                           "foo.cc", cwd, "", extra_arg,
                                           extra_arg_before,
                                           "", &clang_args, &build_dir));

  std::vector<std::string> expected_clang_args{"clang", "-ID", "-IA",   "-IB",
                                               "-IC",   "-c",  "foo.cc"};

  EXPECT_EQ(expected_clang_args, clang_args);
  EXPECT_EQ(cwd, build_dir);
}

}  // namespace devtools_goma
