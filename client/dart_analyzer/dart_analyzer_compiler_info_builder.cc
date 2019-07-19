// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dart_analyzer_compiler_info_builder.h"

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "base/file_dir.h"
#include "base/path.h"
#include "counterz.h"
#include "dart_analyzer_flags.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "ioutil.h"
#include "lib/path_resolver.h"
#include "util.h"

#ifdef _WIN32
#include "posix_helper_win.h"
#endif  // _WIN32

namespace devtools_goma {

namespace {
bool IsExecutable(const std::string& path) {
  return access(path.c_str(), X_OK) == 0;
}

// Parses compiler's output. |compiler_output| is the stdout string of compiler.
// Returns true if succeeded, and |version| will contain compiler version.
// Returns false if failed.
bool ParseDartAnalyzerVersion(absl::string_view compiler_output,
                              std::string* version) {
  // output should be like
  // `dartanalyzer version 2.1.1-dev.1.0`.
  if (!absl::ConsumePrefix(&compiler_output, "dartanalyzer version ")) {
    return false;
  }
  *version = std::string(compiler_output);
  return true;
}

// Gets compiler version by invoking `dartanalyzer`.
// |compiler_path| is a path to `dartanalyzer`.
// |compiler_info_envs| is envvars to use to invoke `dartanalyzer`.
// |cwd| is the current directory.
// If succeeded, returns true and |version| will contain compiler version.
// If failed, returns false. The content of |version| is undefined.
bool GetDartAnalyzerVersion(const std::string& compiler_path,
                            const std::vector<std::string>& compiler_info_envs,
                            const std::string& cwd,
                            std::string* version) {
  const std::vector<std::string> argv{
      compiler_path,
      "--version",
  };
  std::vector<std::string> env(compiler_info_envs);
  env.emplace_back("LC_ALL=C");

  int32_t status = 0;
  std::string output = ReadCommandOutput(compiler_path, argv, env, cwd,
                                         MERGE_STDOUT_STDERR, &status);
  if (status != 0) {
    LOG(ERROR) << "ReadCommandOutput exited with non zero status code."
               << " compiler_path=" << compiler_path << " status=" << status
               << " argv=" << argv << " env=" << env << " cwd=" << cwd
               << " output=" << output;
    return false;
  }

  return ParseDartAnalyzerVersion(output, version);
}

// TODO: Merge this function with the one in
// rustc_compiler_info_builder.cc
bool AddFilesFromDirectory(const std::string& dirname,
                           int remaining_depth,
                           std::vector<std::string>* resource_paths) {
  if (remaining_depth <= 0)
    return true;

  std::vector<DirEntry> entries;
  if (!ListDirectory(dirname, &entries)) {
    LOG(ERROR) << "Failed to list contents from directory " << dirname;
    return false;
  }
  for (const auto& entry : entries) {
    if (entry.name == "." || entry.name == "..") {
      continue;
    }
    std::string entry_path = file::JoinPathRespectAbsolute(dirname, entry.name);
    if (entry.is_dir) {
      if (!AddFilesFromDirectory(entry_path, remaining_depth - 1,
                                 resource_paths)) {
        return false;
      }
    } else {
      resource_paths->push_back(std::move(entry_path));
    }
  }
  return true;
}

// TODO: Merge this function with the one in
// rustc_compiler_info_builder.cc
bool AddResourceAsExecutableBinaryInternal(
    const std::string& resource_path,
    const std::string& cwd,
    int remaining_symlink_follow_count,
    absl::flat_hash_set<std::string>* visited_paths,
    CompilerInfoData* data) {
  std::string abs_resource_path =
      file::JoinPathRespectAbsolute(cwd, resource_path);
  if (!visited_paths->insert(PathResolver::ResolvePath(abs_resource_path))
           .second) {
    // This path has been visited before. Abort.
    return true;
  }

  CompilerInfoData::ResourceInfo r;
  if (!CompilerInfoBuilder::ResourceInfoFromPath(
          cwd, resource_path, CompilerInfoData::EXECUTABLE_BINARY, &r)) {
    CompilerInfoBuilder::AddErrorMessage(
        "failed to get resource info for " + resource_path, data);
    LOG(ERROR) << "failed to get resource info for " + resource_path;
    return false;
  }

  if (r.symlink_path().empty()) {
    // Not a symlink, add it as a resource directly.
    *data->add_resource() = std::move(r);
    return true;
  }

  // It's a symlink.
  if (remaining_symlink_follow_count <= 0) {
    // Too many nested symlink. Abort and return an error.
    CompilerInfoBuilder::AddErrorMessage(
        "too deep nested symlink: " + resource_path, data);
    return false;
  }
  std::string symlink_path = file::JoinPathRespectAbsolute(
      file::Dirname(resource_path), r.symlink_path());
  // Implementation Note: the original resource must come first. If the resource
  // is a symlink, the actual file must be added after the symlink. The server
  // assumes the first resource is a compiler used in a command line, even
  // if it's a symlink.
  *data->add_resource() = std::move(r);
  return AddResourceAsExecutableBinaryInternal(
      symlink_path, cwd, remaining_symlink_follow_count - 1, visited_paths,
      data);
}

}  // anonymous namespace

void DartAnalyzerCompilerInfoBuilder::SetTypeSpecificCompilerInfo(
    const CompilerFlags& compiler_flags,
    const std::string& local_compiler_path,
    const std::string& abs_local_compiler_path,
    const std::vector<std::string>& compiler_info_envs,
    CompilerInfoData* data) const {
  // Ensure dart extension exists.
  (void)data->mutable_dart_analyzer();

  // Set target.
#ifdef _WIN32
  data->set_target("x86_64-pc-windows-msvc");
#elif defined(__MACH__)
  data->set_target("x86_64-apple-darwin");
#elif defined(__linux__)
  data->set_target("x86_64-unknown-linux-gnu");
#else
#error "Unsupported platform."
#endif

  DCHECK_EQ(CompilerFlagType::DartAnalyzer, compiler_flags.type());
  const DartAnalyzerFlags& flags =
      static_cast<const DartAnalyzerFlags&>(compiler_flags);

  if (!GetDartAnalyzerVersion(local_compiler_path, compiler_info_envs,
                              flags.cwd(), data->mutable_version())) {
    AddErrorMessage(
        "Failed to get dartanalyzer version for " + local_compiler_path, data);
    return;
  }

  std::vector<std::string> resources_path;
  if (!CollectDartAnalyzerResources(data->real_compiler_path(),
                                    &resources_path)) {
    AddErrorMessage("Failed to get dartanalyzer resources for " +
                        data->real_compiler_path(),
                    data);
    LOG(ERROR) << data->error_message();
    return;
  }

  absl::flat_hash_set<std::string> visited_paths;
  for (const auto& file : resources_path) {
    if (!AddResourceAsExecutableBinary(file, compiler_flags.cwd(),
                                       &visited_paths, data)) {
      return;
    }
    LOG(INFO) << "dartanalyzer resource " << file << " added.";
  }
}

void DartAnalyzerCompilerInfoBuilder::SetLanguageExtension(
    CompilerInfoData* data) const {
  (void)data->mutable_dart_analyzer();
}

void DartAnalyzerCompilerInfoBuilder::SetCompilerPath(
    const CompilerFlags& flags,
    const std::string& local_compiler_path,
    const std::vector<std::string>& compiler_info_envs,
    CompilerInfoData* data) const {
  data->set_local_compiler_path(local_compiler_path);
  data->set_real_compiler_path(local_compiler_path);
  const DartAnalyzerFlags& dartanalyzer_flags =
      static_cast<const DartAnalyzerFlags&>(flags);
  std::string real_dartanalyzer = file::JoinPathRespectAbsolute(
      dartanalyzer_flags.dart_sdk(), "bin", "dartanalyzer");

  if (IsExecutable(real_dartanalyzer)) {
    data->set_real_compiler_path(std::move(real_dartanalyzer));
  }
}

bool DartAnalyzerCompilerInfoBuilder::CollectDartAnalyzerResources(
    absl::string_view real_compiler_path,
    std::vector<std::string>* resource_paths) {
  resource_paths->emplace_back(real_compiler_path);
  std::vector<std::string> dartanalyzer_resource_directories = {
      "lib",
      "bin",
  };
  absl::string_view sdk_path = file::Dirname(file::Dirname(real_compiler_path));
  constexpr int kMaxNestedDirs = 8;
  for (const auto& dir : dartanalyzer_resource_directories) {
    std::string resource_dir = file::JoinPathRespectAbsolute(sdk_path, dir);
    if (!AddFilesFromDirectory(resource_dir, kMaxNestedDirs, resource_paths)) {
      return false;
    }
  }
  return true;
}

bool DartAnalyzerCompilerInfoBuilder::AddResourceAsExecutableBinary(
    const std::string& resource_path,
    const std::string& cwd,
    absl::flat_hash_set<std::string>* visited_paths,
    CompilerInfoData* data) {
  // On Linux, MAX_NESTED_LINKS is 8.
  constexpr int kMaxNestedLinks = 8;
  return AddResourceAsExecutableBinaryInternal(
      resource_path, cwd, kMaxNestedLinks, visited_paths, data);
}
}  // namespace devtools_goma
