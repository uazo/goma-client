// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rust/rustc_compiler_info_builder.h"

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "counterz.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "ioutil.h"
#include "rustc_flags.h"
#include "util.h"

namespace devtools_goma {

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

}  // namespace devtools_goma
