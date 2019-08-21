// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gcc_compiler_info_builder.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include "absl/algorithm/container.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/path.h"
#include "client/binutils/elf_util.h"
#include "client/mypath.h"
#include "client/subprocess.h"
#include "client/unittest_util.h"
#include "client/util.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "google/protobuf/repeated_field.h"
#include "lib/path_resolver.h"
#include "lib/path_util.h"

namespace devtools_goma {

namespace {

#ifdef __linux__
bool HasHermeticDimensions(
    const google::protobuf::RepeatedPtrField<std::string>& dimensions) {
  for (const auto& d : dimensions) {
    if (absl::StrContains(d, "-hermetic")) {
      return true;
    }
  }
  return false;
}

bool FollowSymlinks(const std::string& path,
                    std::vector<std::string>* paths,
                    int follow_count) {
  if (follow_count <= 0) {
    LOG(ERROR) << "reached max follow count."
               << " path=" << path;
    return false;
  }

  // TODO: merge the code with code in CompilerInfoBuilder?
  struct stat st;
  if (lstat(path.c_str(), &st) < 0) {
    PLOG(WARNING) << "failed to lstat."
                  << " path=" << path;
    return false;
  }
  if (S_ISLNK(st.st_mode)) {
    auto symlink_path(absl::make_unique<char[]>(st.st_size + 1));
    ssize_t size = readlink(path.c_str(), symlink_path.get(), st.st_size + 1);
    if (size < 0) {
      // failed to read symlink
      PLOG(WARNING) << "failed readlink: " << path;
      return false;
    }
    if (size != st.st_size) {
      PLOG(WARNING) << "unexpected symlink size: path=" << path
                    << " actual=" << size << " expected=" << st.st_size;
      return false;
    }
    symlink_path[size] = '\0';
    if (!FollowSymlinks(file::JoinPathRespectAbsolute(file::Dirname(path),
                                                      symlink_path.get()),
                        paths, follow_count - 1)) {
      return false;
    }
  }
  paths->push_back(path);
  return true;
}

enum class LibSelectionPolicy {
  kUseSystemLibs,
  kOmitSystemLibs,
};

bool AppendLibraries(const std::string& cwd,
                     const std::string& compiler,
                     LibSelectionPolicy policy,
                     std::vector<std::string>* expected_executable_binaries) {
  int32_t status = 0;
  std::vector<std::string> cmd = {
      "/usr/bin/ldd",
      compiler,
  };
  const std::string output =
      ReadCommandOutput(cmd[0], cmd, std::vector<std::string>(), cwd,
                        MERGE_STDOUT_STDERR, &status);
  EXPECT_EQ(static_cast<uint32_t>(0), status);
  constexpr absl::string_view kLdSoConfPath = "/etc/ld.so.conf";
  std::vector<std::string> searchpath = LoadLdSoConf(kLdSoConfPath);
  for (auto&& line :
       absl::StrSplit(output, absl::ByAnyChar("\r\n"), absl::SkipEmpty())) {
    // expecting line like:
    // libpthread.so.0 => /lib64/libpthread.so.0 (0x00abcdef)
    absl::string_view::size_type allow_pos = line.find("=>");
    if (allow_pos == absl::string_view::npos) {
      allow_pos = 0;
    } else {
      allow_pos += 2;
    }
    absl::string_view::size_type paren_pos = line.rfind("(");
    // npos
    absl::string_view libname = absl::StripAsciiWhitespace(
        line.substr(allow_pos, paren_pos - allow_pos));
    if (!libname.empty() && IsPosixAbsolutePath(libname)) {
      if (policy == LibSelectionPolicy::kOmitSystemLibs &&
          devtools_goma::IsInSystemLibraryPath(libname, searchpath)) {
        continue;
      }
      // On Linux, MAX_NESTED_LINKS is 8.
      constexpr int kMaxNestedLinks = 8;
      if (!FollowSymlinks(std::string(libname), expected_executable_binaries,
                          kMaxNestedLinks)) {
        return false;
      }
    }
  }
  if (policy == LibSelectionPolicy::kUseSystemLibs) {
    expected_executable_binaries->emplace_back("/etc/ld.so.cache");
  }

  return true;
}

#endif  // __linux__

}  // namespace

class GCCCompilerInfoBuilderTest : public testing::Test {
 protected:
  void SetUp() override { CheckTempDirectory(GetGomaTmpDir()); }

  void AppendPredefinedMacros(const std::string& macro, CompilerInfoData* cid) {
    cid->mutable_cxx()->set_predefined_macros(cid->cxx().predefined_macros() +
                                              macro);
  }

  int FindValue(const std::unordered_map<std::string, int>& map,
                const std::string& key) {
    const auto& it = map.find(key);
    if (it == map.end())
      return 0;
    return it->second;
  }

  std::string TestDir() {
    // This module is in out\Release.
    const std::string parent_dir = file::JoinPath(GetMyDirectory(), "..");
    const std::string top_dir = file::JoinPath(parent_dir, "..");
    return file::JoinPath(top_dir, "test");
  }
};

TEST_F(GCCCompilerInfoBuilderTest, GetExtraSubprogramsClangPlugin) {
  const std::string cwd("/");

  TmpdirUtil tmpdir("get_extra_subprograms_clang_plugin");
  tmpdir.SetCwd(cwd);
  tmpdir.CreateEmptyFile("libPlugin.so");

  std::vector<std::string> args, envs;
  args.push_back("/usr/bin/clang");
  args.push_back("-Xclang");
  args.push_back("-load");
  args.push_back("-Xclang");
  args.push_back(file::JoinPath(tmpdir.tmpdir(), "libPlugin.so"));
  args.push_back("-c");
  args.push_back("hello.c");
  GCCFlags flags(args, cwd);
  std::vector<std::string> clang_plugins;
  std::vector<std::string> B_options;
  bool no_integrated_as = false;
  GCCCompilerInfoBuilder::ParseSubprogramFlags(
      "/usr/bin/clang", flags, &clang_plugins, &B_options, &no_integrated_as);
  std::vector<std::string> expected = {tmpdir.FullPath("libPlugin.so")};
  EXPECT_EQ(expected, clang_plugins);
  EXPECT_TRUE(B_options.empty());
  EXPECT_FALSE(no_integrated_as);
}

TEST_F(GCCCompilerInfoBuilderTest, GetExtraSubprogramsClangPluginRelative) {
  const std::string cwd("/");

  TmpdirUtil tmpdir("get_extra_subprograms_clang_plugin");
  tmpdir.SetCwd(cwd);
  tmpdir.CreateEmptyFile("libPlugin.so");

  std::vector<std::string> args, envs;
  args.push_back("/usr/bin/clang");
  args.push_back("-Xclang");
  args.push_back("-load");
  args.push_back("-Xclang");
  args.push_back("libPlugin.so");
  args.push_back("-c");
  args.push_back("hello.c");
  GCCFlags flags(args, cwd);
  std::vector<std::string> clang_plugins;
  std::vector<std::string> B_options;
  bool no_integrated_as = false;
  GCCCompilerInfoBuilder::ParseSubprogramFlags(
      "/usr/bin/clang", flags, &clang_plugins, &B_options, &no_integrated_as);
  std::vector<std::string> expected = {"libPlugin.so"};
  EXPECT_EQ(expected, clang_plugins);
  EXPECT_TRUE(B_options.empty());
  EXPECT_FALSE(no_integrated_as);
}

TEST_F(GCCCompilerInfoBuilderTest, GetExtraSubprogramsBOptions) {
  const std::string cwd("/");

  TmpdirUtil tmpdir("get_extra_subprograms_clang_plugin");
  tmpdir.SetCwd(cwd);
  tmpdir.CreateEmptyFile("libPlugin.so");

  std::vector<std::string> args, envs;
  args.push_back("/usr/bin/clang");
  args.push_back("-B");
  args.push_back("dummy");
  args.push_back("-c");
  args.push_back("hello.c");
  GCCFlags flags(args, cwd);
  std::vector<std::string> clang_plugins;
  std::vector<std::string> B_options;
  bool no_integrated_as = false;
  GCCCompilerInfoBuilder::ParseSubprogramFlags(
      "/usr/bin/clang", flags, &clang_plugins, &B_options, &no_integrated_as);
  std::vector<std::string> expected = {"dummy"};
  EXPECT_TRUE(clang_plugins.empty());
  EXPECT_EQ(expected, B_options);
  EXPECT_FALSE(no_integrated_as);
}

TEST_F(GCCCompilerInfoBuilderTest, GetCompilerNameUsualCases) {
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"clang", "clang"},
      {"clang++", "clang"},
      {"g++", "g++"},
      {"gcc", "gcc"},
  };

  GCCCompilerInfoBuilder builder;
  for (const auto& tc : test_cases) {
    CompilerInfoData data;
    data.set_local_compiler_path(tc.first);
    data.set_real_compiler_path(tc.second);
    EXPECT_EQ(tc.first, builder.GetCompilerName(data));
  }
}

TEST_F(GCCCompilerInfoBuilderTest, GetCompilerNameCc) {
  std::vector<std::string> test_cases = {"clang", "gcc"};

  GCCCompilerInfoBuilder builder;
  for (const auto& tc : test_cases) {
    CompilerInfoData data;
    data.set_local_compiler_path("cc");
    data.set_real_compiler_path(tc);
    EXPECT_EQ(tc, builder.GetCompilerName(data));
  }
}

TEST_F(GCCCompilerInfoBuilderTest, GetCompilerNameCxx) {
  GCCCompilerInfoBuilder builder;
  CompilerInfoData data;

  data.set_local_compiler_path("c++");
  data.set_real_compiler_path("g++");
  EXPECT_EQ("g++", builder.GetCompilerName(data));

  data.set_local_compiler_path("c++");
  data.set_real_compiler_path("clang");
  EXPECT_EQ("clang++", builder.GetCompilerName(data));
}

TEST_F(GCCCompilerInfoBuilderTest, GetCompilerNameUnsupportedCase) {
  GCCCompilerInfoBuilder builder;

  CompilerInfoData data;
  data.set_local_compiler_path("c++");
  data.set_real_compiler_path("clang++");
  EXPECT_EQ("", builder.GetCompilerName(data));
}

#ifndef _WIN32
// Since we use real clang and subprogram, use non-Win env only.
TEST_F(GCCCompilerInfoBuilderTest, BuildWithRealClang) {
  InstallReadCommandOutputFunc(ReadCommandOutputByPopen);

  const std::string cwd = GetMyDirectory();

  // clang++ is usually a symlink to clang.
  // To check a symlink is correctly working, use clang++ instead of clang.

  const std::string clang = PathResolver::WeakRelativePath(GetClangPath(), cwd);
  // TODO: unittest_util should have GetClangXXPath()?
  const std::string clangxx = clang + "++";

  // Needs to use real .so otherwise clang fails to read the file.
  // Linux has .so, and mac has .dylib.
  // TODO: Remove plugin use? (b/122436038)
#ifdef __MACH__
  const std::string lib_find_bad_constructs_so = file::JoinPath(
      file::Dirname(clangxx), "..", "lib", "libFindBadConstructs.dylib");
#else
  const std::string lib_find_bad_constructs_so = file::JoinPath(
      file::Dirname(clangxx), "..", "lib", "libFindBadConstructs.so");
#endif

  std::vector<std::string> args{
      clangxx,
      "-c",
      "hello.cc",
  };

  if (access(lib_find_bad_constructs_so.c_str(), R_OK) == 0) {
    const std::vector<std::string> plugin_args = {"-Xclang", "-load", "-Xclang",
                                                  lib_find_bad_constructs_so};
    args.insert(args.end(), plugin_args.begin(), plugin_args.end());
  }

  const std::vector<std::string> envs;
  GCCFlags flags(args, cwd);

  GCCCompilerInfoBuilder builder;
  std::unique_ptr<CompilerInfoData> data =
      builder.FillFromCompilerOutputs(flags, clangxx, envs);

  std::vector<std::string> actual_executable_binaries;
  ASSERT_NE(data.get(), nullptr);
  for (const auto& resource : data->resource()) {
    if (resource.type() == CompilerInfoData_ResourceType_EXECUTABLE_BINARY) {
      actual_executable_binaries.push_back(resource.name());
    }
  }

  std::vector<std::string> expected_executable_binaries{
      clangxx,
      clang,
  };
#ifdef __linux__
  if (HasHermeticDimensions(data->dimensions())) {
    AppendLibraries(cwd, clangxx, LibSelectionPolicy::kUseSystemLibs,
                    &expected_executable_binaries);
  } else {
    AppendLibraries(cwd, clangxx, LibSelectionPolicy::kOmitSystemLibs,
                    &expected_executable_binaries);
  }
  absl::c_for_each(expected_executable_binaries, [&cwd](std::string& s) {
    s = PathResolver::WeakRelativePath(s, cwd);
  });
#endif

  if (access(lib_find_bad_constructs_so.c_str(), R_OK) == 0) {
    expected_executable_binaries.push_back(lib_find_bad_constructs_so);
  }

  EXPECT_THAT(actual_executable_binaries,
              testing::UnorderedElementsAreArray(expected_executable_binaries));
}

#ifdef __linux__
// The code should work even if the wrapper is used behind the clang.
// b/138603858
TEST_F(GCCCompilerInfoBuilderTest, ClangWrapper) {
  InstallReadCommandOutputFunc(ReadCommandOutputByPopen);

  const std::string wrapper_clang =
      file::JoinPath("..", "..", "test", "wrapper-clang");
  const std::string cwd = GetMyDirectory();
  std::vector<std::string> args{
      wrapper_clang,
      "-c",
      "hello.cc",
  };
  const std::string& clang = GetClangPath();
  const std::vector<std::string> envs = {
      "GOMATEST_CLANG_PATH=" + clang,
  };
  GCCFlags flags(args, cwd);

  GCCCompilerInfoBuilder builder;
  std::unique_ptr<CompilerInfoData> data =
      builder.FillFromCompilerOutputs(flags, wrapper_clang, envs);

  ASSERT_NE(data.get(), nullptr);
  std::vector<std::string> actual_executable_binaries;
  for (const auto& resource : data->resource()) {
    if (resource.type() == CompilerInfoData_ResourceType_EXECUTABLE_BINARY) {
      actual_executable_binaries.push_back(resource.name());
    }
  }
  std::vector<std::string> expected_executable_binaries{
      wrapper_clang,
      clang,
  };
  if (HasHermeticDimensions(data->dimensions())) {
    AppendLibraries(cwd, clang, LibSelectionPolicy::kUseSystemLibs,
                    &expected_executable_binaries);
  } else {
    AppendLibraries(cwd, clang, LibSelectionPolicy::kOmitSystemLibs,
                    &expected_executable_binaries);
  }
  EXPECT_THAT(actual_executable_binaries,
              testing::UnorderedElementsAreArray(expected_executable_binaries));
}
#endif  // __linux__

#endif

}  // namespace devtools_goma
