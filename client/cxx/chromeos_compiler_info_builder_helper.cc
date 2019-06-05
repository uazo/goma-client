// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_compiler_info_builder_helper.h"

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "elf_util.h"
#include "glog/logging.h"
#include "lib/file_helper.h"
#include "path.h"

#include <unistd.h>

namespace devtools_goma {

namespace {

constexpr absl::string_view kClang = "/usr/bin/clang";
constexpr absl::string_view kClangxx = "/usr/bin/clang++";

bool IsClangWrapperInChroot(const absl::string_view local_compiler_path) {
  if (file::Dirname(local_compiler_path) != "/usr/bin") {
    return false;
  }
  absl::string_view basename = file::Basename(local_compiler_path);
  return absl::EndsWith(basename, "-clang") ||
         absl::EndsWith(basename, "-clang++");
}

bool IsKnownClangInChroot(const absl::string_view local_compiler_path) {
  return local_compiler_path == kClang || local_compiler_path == kClangxx ||
         IsClangWrapperInChroot(local_compiler_path);
}

bool ParseEnvdPath(absl::string_view envd_path, std::string* path) {
  // content is like
  //
  // ```
  // PATH="/usr/x86_64-pc-linux-gnu/x86_64-cros-linux-gnu/gcc-bin/4.9.x"
  // ROOTPATH="/usr/x86_64-pc-linux-gnu/x86_64-cros-linux-gnu/gcc-bin/4.9.x"
  // ```

  std::string content;
  if (!ReadFileToString(envd_path, &content)) {
    LOG(ERROR) << "failed to open/read " << envd_path;
    return false;
  }

  for (absl::string_view line :
       absl::StrSplit(content, absl::ByAnyChar("\r\n"), absl::SkipEmpty())) {
    if (absl::ConsumePrefix(&line, "PATH=\"") &&
        absl::ConsumeSuffix(&line, "\"")) {
      *path = std::string(line);
      return true;
    }
  }

  return false;
}

}  // anonymous namespace

// static
bool ChromeOSCompilerInfoBuilderHelper::IsSimpleChromeClangCommand(
    absl::string_view local_compiler_path,
    absl::string_view real_compiler_path) {
  if (!(absl::EndsWith(local_compiler_path, "clang") ||
        absl::EndsWith(local_compiler_path, "clang++"))) {
    return false;
  }
  if (!absl::EndsWith(real_compiler_path, ".elf")) {
    return false;
  }

  return true;
}

// static
bool ChromeOSCompilerInfoBuilderHelper::CollectSimpleChromeClangResources(
    const std::string& cwd,
    absl::string_view local_compiler_path,
    absl::string_view real_compiler_path,
    std::vector<std::string>* resource_paths) {
  absl::string_view local_compiler_dir = file::Dirname(local_compiler_path);

  int version;
  if (!EstimateClangMajorVersion(real_compiler_path, &version)) {
    LOG(ERROR) << "failed to estimate clang major version"
               << " real_compiler_path=" << real_compiler_path;
    return false;
  }

  // if local_compiler is clang++, real_compiler is clang-<N>.elf.
  // However, clang++-<N>.elf and clang-<N> are both necessary to run clang++.
  if (absl::EndsWith(local_compiler_path, "clang++")) {
    resource_paths->push_back(file::JoinPath(
        local_compiler_dir, absl::StrCat("clang++-", version, ".elf")));
    resource_paths->push_back(
        file::JoinPath(local_compiler_dir, absl::StrCat("clang-", version)));
  }

  // Please see --library-path argument in simple Chrome's clang wrapper.
  const std::vector<std::string> search_paths = {
      file::JoinPath(local_compiler_dir, "..", "..", "lib"),
      file::JoinPath(local_compiler_dir, "..", "lib64"),
  };
  // TODO: use relative path real_compiler_path and simplify.
  // Since real_compiler_path is absolute path, we cannot use that here.
  const std::string compiler_path = file::JoinPath(
      local_compiler_dir, absl::StrCat("clang-", version, ".elf"));
  // Since the shell script wrapper has --inhibit-rpath '',
  // we should ignore RPATH and RUNPATH specified in ELF.
  ElfDepParser edp(cwd, search_paths, true);
  absl::flat_hash_set<std::string> deps;
  if (!edp.GetDeps(compiler_path, &deps)) {
    LOG(ERROR) << "failed to get library dependencies."
               << " cwd=" << cwd
               << " local_compiler_path=" << local_compiler_path
               << " real_compiler_path=" << real_compiler_path;
    return false;
  }
  for (const auto& path : deps) {
    resource_paths->push_back(path);
  }

  return true;
}

// static
bool ChromeOSCompilerInfoBuilderHelper::EstimateClangMajorVersion(
    absl::string_view real_compiler_path,
    int* version) {
  // Assuming real_compiler_path filename is like
  // `clang-<N>.elf` or `clang-<N>`.

  absl::string_view filename = file::Basename(real_compiler_path);
  if (!absl::ConsumePrefix(&filename, "clang-")) {
    LOG(INFO) << "not start with clang-:" << filename;
    return false;
  }
  // If this has .elf, remove that.
  // If it doesn't exist, it's not an error.
  absl::ConsumeSuffix(&filename, ".elf");

  if (!absl::SimpleAtoi(filename, version)) {
    LOG(INFO) << "not an integer:" << filename;
    return false;
  }

  return true;
}

// static
bool ChromeOSCompilerInfoBuilderHelper::IsClangInChrootEnv(
    absl::string_view local_compiler_path) {
  if (!IsKnownClangInChroot(local_compiler_path)) {
    return false;
  }

  // chromeos chroot env should have /etc/cros_chroot_version.
  if (access("/etc/cros_chroot_version", F_OK) < 0) {
    return false;
  }

  return true;
}

namespace {

bool SetChrootClangResourcePaths(const std::string& cwd,
                                 const std::vector<std::string>& files,
                                 absl::string_view local_compiler_path,
                                 absl::string_view real_compiler_path,
                                 std::vector<std::string>* resource_paths) {
  constexpr absl::string_view kLdSoConfPath = "/etc/ld.so.conf";
  std::string content;
  if (!ReadFileToString(kLdSoConfPath, &content)) {
    LOG(ERROR) << "failed to open/read " << kLdSoConfPath;
    return false;
  }
  std::vector<std::string> searchpath = ParseLdSoConf(content);
  ElfDepParser edp(cwd, searchpath, false);

  absl::flat_hash_set<std::string> exec_deps;
  for (const auto& file : files) {
    if (file != local_compiler_path && file != real_compiler_path) {
      resource_paths->push_back(file);
    }
    if (!IsElfFile(file)) {
      continue;
    }
    if (!edp.GetDeps(file, &exec_deps)) {
      LOG(ERROR) << "failed to get library dependencies for executable."
                 << " file=" << file << " cwd=" << cwd;
      return false;
    }
  }
  for (const auto& path : exec_deps) {
    resource_paths->push_back(path);
  }
  return true;
}

}  // namespace

// static
bool ChromeOSCompilerInfoBuilderHelper::CollectChrootClangResources(
    const std::string& cwd,
    absl::string_view local_compiler_path,
    absl::string_view real_compiler_path,
    std::vector<std::string>* resource_paths) {
  std::vector<std::string> resources = {
      std::string(local_compiler_path),
      std::string(real_compiler_path),
  };

  if (!IsClangWrapperInChroot(local_compiler_path)) {
    return SetChrootClangResourcePaths(cwd, resources, local_compiler_path,
                                       real_compiler_path, resource_paths);
  }

  //
  // Code below list up files needed to run the wrapper.
  //
  if (IsElfFile(std::string(local_compiler_path))) {
    // Assuming |local_compiler_path| is a program to detect a position of
    // the wrapper, and execute.
    // Then, we need to upload files to decide wrapper positions (.NATIVE
    // and 05gcc-*), and the wrapper script itself.
    resources.emplace_back("/etc/env.d/gcc/.NATIVE");
    absl::string_view compile_target = file::Stem(local_compiler_path);
    if (!absl::ConsumeSuffix(&compile_target, "-clang") &&
        !absl::ConsumeSuffix(&compile_target, "-clang++")) {
      PLOG(ERROR) << "compiler name seems not be expected."
                  << " local_compiler_path=" << local_compiler_path;
      return false;
    }
    const std::string envfilename =
        absl::StrCat("/etc/env.d/05gcc-", compile_target);
    if (access(envfilename.c_str(), R_OK) != 0) {
      LOG(ERROR) << "env file not found."
                 << " envfilename=" << envfilename
                 << " local_compiler_path=" << local_compiler_path
                 << " real_compiler_path=" << real_compiler_path;
      return false;
    }
    resources.push_back(envfilename);
    std::string path_from_envd;
    if (!ParseEnvdPath(envfilename, &path_from_envd)) {
      LOG(ERROR) << "Failed to parse env file."
                 << " envfilename=" << envfilename
                 << " local_compiler_path=" << local_compiler_path
                 << " real_compiler_path=" << real_compiler_path;
      return false;
    }

    // Even if <basename> ends with clang++, we also need clang ones.
    absl::string_view base_compiler_path = file::Basename(local_compiler_path);
    resources.push_back(file::JoinPath(path_from_envd, base_compiler_path));
    if (absl::EndsWith(base_compiler_path, "clang++")) {
      resources.push_back(file::JoinPath(
          path_from_envd, absl::StripSuffix(base_compiler_path, "++")));
    }
  }
  // Actually /usr/bin/clang{,++} is called from the wrapper.
  if (absl::EndsWith(local_compiler_path, "clang++")) {
    resources.emplace_back(kClangxx);
  } else {
    resources.emplace_back(kClang);
  }

  // We also need python2 for wrapper.
  // TODO: better way to handle python library usage change.
  std::vector<std::string> python2_deps = {
      "/usr/lib64/python2.7/_abcoll.py",
      "/usr/lib64/python2.7/abc.py",
      "/usr/lib64/python2.7/codecs.py",
      "/usr/lib64/python2.7/copy_reg.py",
      "/usr/lib64/python2.7/encodings/aliases.py",
      "/usr/lib64/python2.7/encodings/__init__.py",
      "/usr/lib64/python2.7/encodings/utf_8.py",
      "/usr/lib64/python2.7/__future__.py",
      "/usr/lib64/python2.7/genericpath.py",
      "/usr/lib64/python2.7/linecache.py",
      "/usr/lib64/python2.7/os.py",
      "/usr/lib64/python2.7/posixpath.py",
      "/usr/lib64/python2.7/stat.py",
      "/usr/lib64/python2.7/types.py",
      "/usr/lib64/python2.7/UserDict.py",
      "/usr/lib64/python2.7/warnings.py",
      "/usr/lib64/python2.7/_weakrefset.py",
      // "lib-dynload" is needed for detecting EXEC_PREFIX in python.
      // The following code make it created in remote.
      "/usr/lib64/python2.7/lib-dynload/../../../bin/python2",
  };
  resources.insert(resources.end(),
                   std::make_move_iterator(python2_deps.begin()),
                   std::make_move_iterator(python2_deps.end()));

  return SetChrootClangResourcePaths(cwd, resources, local_compiler_path,
                                     real_compiler_path, resource_paths);
}

// static
void ChromeOSCompilerInfoBuilderHelper::SetAdditionalFlags(
    absl::string_view local_compiler_path,
    google::protobuf::RepeatedPtrField<std::string>* additional_flags) {
  if (IsClangWrapperInChroot(local_compiler_path)) {
    // Wrapper tries to set up ccache, but it's meaningless in goma.
    // we have to set -noccache.
    additional_flags->Add("-noccache");
  }
}

}  // namespace devtools_goma
