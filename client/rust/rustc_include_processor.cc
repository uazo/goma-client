// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rust/rustc_include_processor.h"

#include <fstream>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "base/file_dir.h"
#include "file_helper.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "path.h"
#include "util.h"

namespace devtools_goma {

namespace {

// Reads .d file, and lists required_files.
//
// example
//   main: ./main.rs
//   main.d: ./main.rs
//   ./main.rs:
//
// Then, required_files is just "./main.rs".
bool AnalyzeDepsFile(absl::string_view filename,
                     std::set<std::string>* required_files) {
  std::string deps_info;
  if (!ReadFileToString(filename, &deps_info)) {
    LOG(ERROR) << "failed to open " << filename;
    return false;
  }

  std::string error_reason;
  if (!RustcIncludeProcessor::ParseRustcDeps(deps_info, required_files,
                                             &error_reason)) {
    LOG(ERROR) << "failed to parse deps_info due to error " << error_reason;
    return false;
  }
  return true;
}

}  // anonymous namespace

bool RustcIncludeProcessor::Run(const RustcFlags& rustc_flags,
                                const RustcCompilerInfo& rustc_compiler_info,
                                std::set<std::string>* required_files,
                                std::string* error_reason) {
  // NOTE: There is an ongoing attempt to teach rustc --emit=dep-info
  // to include rlibs and native libs. Before this is done, the required_files
  // concluded from deps-info is not sufficient to complete a build. In the
  // mean time, goma shoud put every file it can find in the include paths
  // into required_files.
  // TODO: Put every file in include paths into required_files.

  if (rustc_flags.input_filenames().empty()) {
    *error_reason = "input file is empty";
    return false;
  }

  std::string input_rs = rustc_flags.input_filenames()[0];
  if (!absl::EndsWithIgnoreCase(input_rs, ".rs")) {
    *error_reason = "input file " + input_rs + " is not ended with \".rs\"";
    return false;
  }
  std::string deps_file = input_rs.substr(0, input_rs.size() - 3) + ".d";
  deps_file = file::JoinPathRespectAbsolute(
      rustc_flags.cwd_for_include_processor(), deps_file);

  std::vector<std::string> args;
  if (!RustcIncludeProcessor::RewriteArgs(rustc_flags.args(), deps_file, &args,
                                          error_reason)) {
    LOG(ERROR) << "rustc args generation failed due to error " << error_reason;
    return false;
  }

  int32_t status = 0;
  const std::string& rustc_path = rustc_compiler_info.local_compiler_path();
  // run with empty env. maybe envs must be stored in CompilerFlags or
  // somewhere. Or use compiler_info_envs?
  const std::vector<std::string> envs;
  std::string output = ReadCommandOutput(
      rustc_path, args, envs, rustc_flags.cwd(), MERGE_STDOUT_STDERR, &status);

  if (status != 0) {
    LOG(ERROR) << "ReadCommandOutput exited with non zero status code."
               << " rustc_path=" << rustc_path << " status=" << status
               << " args=" << args << " env=" << envs
               << " cwd=" << rustc_flags.cwd() << " output=" << output;
    *error_reason = "failed to run rust include processor";
    return false;
  }

  if (!AnalyzeDepsFile(deps_file, required_files)) {
    *error_reason = "failed to analyze " + deps_file;
    return false;
  }
  return true;
}

// static
bool RustcIncludeProcessor::ParseRustcDeps(
    absl::string_view deps_info,
    std::set<std::string>* required_files,
    std::string* error_reason) {
  for (auto& current_line :
       absl::StrSplit(deps_info, absl::ByAnyChar("\r\n"))) {
    if (current_line.empty()) {
      continue;
    }
    if (current_line.back() == ':') {
      continue;
    }
    if (absl::StrContains(current_line, ": ")) {
      absl::string_view s =
          current_line.substr(current_line.find_first_of(": ") + 2);
      required_files->insert(std::string(s));
      continue;
    }
    required_files->insert(std::string(current_line));
  }
  return true;
}

// static
bool RustcIncludeProcessor::RewriteArgs(
    const std::vector<std::string>& old_args,
    const std::string& dep_file,
    std::vector<std::string>* new_args,
    std::string* error_reason) {
  new_args->clear();
  for (auto iterator = old_args.begin(); iterator < old_args.end();
       ++iterator) {
    if (absl::StartsWith(*iterator, "--out-dir")) {
      // remove --out-dir as it will conflict with -o
      if (*iterator == "--out-dir") {
        // Not used as "--out-dir=" prefix.
        ++iterator;
      }
      continue;
    }
    if (absl::StartsWith(*iterator, "-o")) {
      // remove existing -o
      if (*iterator == "-o") {
        // Not used as "-o=FILE" or "-oFILE" prefix.
        ++iterator;
      }
      continue;
    }
    if (absl::StartsWith(*iterator, "--emit")) {
      // remomve existing --emit
      if (*iterator == "--emit") {
        // Not used as "--emit=" prefix.
        ++iterator;
      }
      continue;
    }
    new_args->emplace_back(*iterator);
  }
  new_args->emplace_back("--emit=dep-info");
  new_args->emplace_back("-o");
  new_args->emplace_back(dep_file);
  return true;
}

}  // namespace devtools_goma
