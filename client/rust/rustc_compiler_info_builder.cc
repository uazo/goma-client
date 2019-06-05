// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rust/rustc_compiler_info_builder.h"

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "base/file_dir.h"
#include "base/path.h"
#include "counterz.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "ioutil.h"
#include "lib/path_resolver.h"
#include "rustc_flags.h"
#include "util.h"

#ifdef _WIN32
#include "posix_helper_win.h"
#endif  // _WIN32

namespace devtools_goma {

namespace {
bool IsExecutable(const std::string& path) {
  return access(path.c_str(), X_OK) == 0;
}

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
}  // namespace

void RustcCompilerInfoBuilder::SetTypeSpecificCompilerInfo(
    const CompilerFlags& compiler_flags,
    const std::string& local_compiler_path,
    const std::string& abs_local_compiler_path,
    const std::vector<std::string>& compiler_info_envs,
    CompilerInfoData* data) const {
  // Ensure rustc extension exists.
  (void)data->mutable_rustc();

  DCHECK_EQ(CompilerFlagType::Rustc, compiler_flags.type());
  const RustcFlags& flags = static_cast<const RustcFlags&>(compiler_flags);

  std::string host;
  if (!GetRustcVersionHost(local_compiler_path, compiler_info_envs, flags.cwd(),
                           data->mutable_version(), &host)) {
    AddErrorMessage("Failed to get rustc version for " + local_compiler_path,
                    data);
    LOG(ERROR) << data->error_message();
    return;
  }
  data->set_target(std::move(host));

  std::vector<std::string> resources_path;
  if (!CollectRustcResources(data->real_compiler_path(), &resources_path)) {
    AddErrorMessage(
        "Failed to get rustc resources for " + data->real_compiler_path(),
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
  }
}

void RustcCompilerInfoBuilder::SetLanguageExtension(
    CompilerInfoData* data) const {
  (void)data->mutable_rustc();
}

bool RustcCompilerInfoBuilder::GetRustcVersionHost(
    const std::string& rustc_path,
    const std::vector<std::string>& compiler_info_envs,
    const std::string& cwd,
    std::string* version,
    std::string* host) const {
  const std::vector<std::string> args{
      rustc_path,
      "--version",
      "-v",
  };

  std::vector<std::string> env(compiler_info_envs);
  env.emplace_back("LC_ALL=C");

  int32_t status = 0;
  std::string output = ReadCommandOutput(rustc_path, args, env, cwd,
                                         MERGE_STDOUT_STDERR, &status);
  if (status != 0) {
    LOG(ERROR) << "ReadCommandOutput exited with non zero status code."
               << " rustc_path=" << rustc_path << " status=" << status
               << " args=" << args << " env=" << env << " cwd=" << cwd
               << " output=" << output;
    return false;
  }

  LOG(INFO) << "output=" << output;

  return ParseRustcVersionHost(output, version, host);
}

bool RustcCompilerInfoBuilder::ParseRustcVersionHost(
    absl::string_view compiler_output,
    std::string* version,
    std::string* host) const {
  // output example:
  //
  // rustc 1.29.0-nightly (9bd8458c9 2018-07-09)
  // binary: rustc
  // commit-hash: 9bd8458c92f7166b827e4eb5cf5effba8c0e615d
  // commit-date: 2018-07-09
  // host: x86_64-unknown-linux-gnu
  // release: 1.29.0-nightly
  // LLVM version: 6.0

  bool first = true;
  bool host_was_found = false;

  for (auto line : absl::StrSplit(compiler_output, absl::ByAnyChar("\r\n"),
                                  absl::SkipEmpty())) {
    if (first) {
      // first line must be version.
      if (!absl::ConsumePrefix(&line, "rustc ")) {
        return false;
      }
      *version = std::string(line);
      first = false;
      continue;
    }

    // not sure `host: ` is correct.
    if (absl::ConsumePrefix(&line, "host: ")) {
      *host = std::string(line);
      host_was_found = true;
      break;
    }
  }

  return host_was_found;
}

void RustcCompilerInfoBuilder::SetCompilerPath(
    const CompilerFlags& flags,
    const std::string& local_compiler_path,
    const std::vector<std::string>& compiler_info_envs,
    CompilerInfoData* data) const {
  // If rustc is installed by rustup, the local_compiler_path
  // is actually pointing to a rustup wrapper, which shouldn't
  // be used for hashing. The real rustc
  // should be located at `rustc --print sysroot`/bin/rustc
  data->set_local_compiler_path(local_compiler_path);
  data->set_real_compiler_path(local_compiler_path);
  const std::vector<std::string> args{
      local_compiler_path,
      "--print",
      "sysroot",
  };
  std::vector<std::string> env(compiler_info_envs);
  env.emplace_back("LC_ALL=C");

  int32_t status = 0;
  std::string output =
      ReadCommandOutput(local_compiler_path, args, env, flags.cwd(),
                        MERGE_STDOUT_STDERR, &status);
  LOG(INFO) << "Rust sysroot " << output;
  if (status != 0) {
    LOG(ERROR) << "ReadCommandOutput exited with non zero status code."
               << " rustc_path=" << local_compiler_path << " status=" << status
               << " args=" << args << " env=" << env << " cwd=" << flags.cwd()
               << " output=" << output;
    return;
  }
  std::string real_rustc = file::JoinPathRespectAbsolute(
      absl::StripTrailingAsciiWhitespace(output), "bin/rustc");
  if (IsExecutable(real_rustc)) {
    data->set_real_compiler_path(std::move(real_rustc));
  }
}

// static
bool RustcCompilerInfoBuilder::CollectRustcResources(
    absl::string_view real_compiler_path,
    std::vector<std::string>* resource_paths) {
  resource_paths->emplace_back(real_compiler_path);
  std::vector<std::string> rustc_resource_directories = {"lib"};

  // The real toolchain is located at
  // ~/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin/rustc
  // We should use ~/.rustup/toolchains/stable-x86_64-unknown-linux-gnu
  // as rust_root.
  absl::string_view rust_root =
      file::Dirname(file::Dirname(real_compiler_path));
  constexpr int kMaxNestedDirs = 8;
  for (const auto& dir : rustc_resource_directories) {
    std::string resource_dir = file::JoinPathRespectAbsolute(rust_root, dir);
    if (!AddFilesFromDirectory(resource_dir, kMaxNestedDirs, resource_paths)) {
      return false;
    }
  }
  return true;
}

// static
bool RustcCompilerInfoBuilder::AddResourceAsExecutableBinary(
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
