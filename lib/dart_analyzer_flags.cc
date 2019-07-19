// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/dart_analyzer_flags.h"

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "base/path.h"
#include "lib/flag_parser.h"
#include "lib/path_util.h"

namespace devtools_goma {

DartAnalyzerFlags::DartAnalyzerFlags(const std::vector<std::string>& args,
                                     const std::string& cwd)
    : CompilerFlags(args, cwd) {
  lang_ = "dart";

  if (!CompilerFlags::ExpandPosixArgs(cwd, args, &expanded_args_,
                                      &optional_input_filenames_)) {
    Fail("Unable to expand args");
    return;
  }

  FlagParser parser;
  FlagParser::Options* opts = parser.mutable_options();
  opts->flag_prefix = '-';
  opts->allows_equal_arg = true;
  opts->allows_nonspace_arg = true;

  // General flags
  FlagParser::Flag* flag_dart_sdk = parser.AddFlag("-dart-sdk");
  FlagParser::Flag* flag_options = parser.AddFlag("-options");
  parser.AddBoolFlag("-implicit-casts");
  parser.AddBoolFlag("-no-implicit-casts");
  parser.AddBoolFlag("-no-implicit-dynamic");
  parser.AddPrefixFlag("D");
  parser.AddPrefixFlag("-D");
  FlagParser::Flag* flag_packages = parser.AddFlag("-packages");
  FlagParser::Flag* flag_dart_sdk_summary = parser.AddFlag("-dart-sdk-summary");
  parser.AddBoolFlag("-lints");
  parser.AddBoolFlag("-no-lints");
  parser.AddFlag("-format");
  parser.AddFlag("-enable-experiment");
  parser.AddBoolFlag("-no-hints");
  parser.AddBoolFlag("-fatal-infos");
  parser.AddBoolFlag("-fatal-warnings");
  parser.AddBoolFlag("-help");
  parser.AddBoolFlag("-version");
  parser.AddBoolFlag("-verbose");

  // Build mode flags
  // TODO: Are these necessary to support?
  // parser.AddBoolFlag("-persistent-worker");
  // parser.AddFlag("-build-analysis-output");  // TODO: make this the output
  // parser.AddBoolFlag("-build-mode");
  // parser.AddFlag("-build-summary-input");            // TODO: make this input
  // parser.AddFlag("-build-summary-unlinked-input");   // TODO: make this input
  // parser.AddFlag("-build-summary-output");           // TODO: make this
  // output parser.AddFlag("-build-summary-output-semantic");  // TODO: make
  // this input parser.AddBoolFlag("-build-summary-only");
  // parser.AddBoolFlag("-build-summary-only-unlinked");
  // parser.AddBoolFlag("-build-suppress-exit-code");
  // parser.AddBoolFlag("-color");
  // parser.AddBoolFlag("-no-color");

  // Less frequently used flags
  FlagParser::Flag* flag_perf_report = parser.AddFlag("-x-perf-report");
  std::vector<std::string> raw_url_mappings;
  parser.AddPrefixFlag("-url-mapping")->SetOutput(&raw_url_mappings);
  // parser.AddBoolFlag("-batch"); // TODO: I don't think this
  // mode can be supported?
  parser.AddBoolFlag("-disable-cache-flushing");
  parser.AddBoolFlag("-no-disable-cache-flushing");
  parser.AddBoolFlag("-log");
  parser.AddBoolFlag("-use-analysis-driver-memory-byte-store");
  parser.AddBoolFlag("-fatal-lints");
  parser.AddBoolFlag("-use-fasta-parser");
  parser.AddBoolFlag("-no-use-fasta-parser");
  parser.AddBoolFlag("-preview-dart-2");
  parser.AddBoolFlag("-no-preview-dart-2");
  // parser.AddBoolFlag("-train-snapshot"); // TODO: I don't think
  // this mode can be supported?

  // Deprecated flags.
  FlagParser::Flag* flag_package_root = parser.AddFlag("-package-root");
  parser.AddBoolFlag("-declaration-casts");
  parser.AddBoolFlag("-no-declaration-casts");
  parser.AddBoolFlag("-initializing-formal-access");
  parser.AddFlag("-x-package-warnings-prefix");
  parser.AddBoolFlag("-enable-conditional-directives");
  parser.AddBoolFlag("-show-package-warnings");
  parser.AddBoolFlag("-show-sdk-warnings");
  parser.AddBoolFlag("-enable-assert-initializers");
  parser.AddBoolFlag("-fatal-hints");
  parser.AddBoolFlag("-package-warnings");

  parser.AddNonFlag()->SetOutput(&input_filenames_);
  FlagParser::Flag* flag_ignore_unrecognized_flags =
      parser.AddBoolFlag("-ignore-unrecognized-flags");
  parser.Parse(expanded_args_);
  // TODO: does this work?
  unknown_flags_ = parser.unknown_flag_args();

  if (flag_ignore_unrecognized_flags->seen() && !unknown_flags_.empty()) {
    Fail("unrecognized arguments: " + absl::StrJoin(unknown_flags_, ", "));
  }

  if (flag_dart_sdk->seen()) {
    dart_sdk_ =
        file::JoinPathRespectAbsolute(cwd, flag_dart_sdk->GetLastValue());
  }

  if (flag_options->seen()) {
    input_filenames_.push_back(flag_options->GetLastValue());
  }

  if (flag_packages->seen()) {
    packages_file_ =
        file::JoinPathRespectAbsolute(cwd, flag_packages->GetLastValue());
    input_filenames_.emplace_back(packages_file_);
  }

  if (flag_dart_sdk_summary->seen()) {
    input_filenames_.push_back(flag_dart_sdk_summary->GetLastValue());
  }

  if (flag_perf_report->seen()) {
    output_files_.push_back(flag_perf_report->GetLastValue());
  }

  if (flag_package_root->seen()) {
    package_root_ =
        file::JoinPathRespectAbsolute(cwd, flag_package_root->GetLastValue());
    use_deprecated_package_root_ = true;
  }

  if (!packages_file_.empty() && !package_root_.empty()) {
    Fail("cannot set --packages and --package-root");
  }

  for (const auto& raw : raw_url_mappings) {
    std::pair<std::string, std::string> mapping;
    if (!SplitURIMapping(raw, &mapping)) {
      Fail("cannot split provided url_mapping: " + raw);
    }
    mapping.second = file::JoinPathRespectAbsolute(cwd, mapping.second);
    std::string library_name = mapping.first;
    auto ret = url_mappings_.insert(std::move(mapping));
    if (!ret.second) {
      Fail("duplicate url mapping for the same library: " + library_name);
    }
  }

  // set is_successful_ = true to indicate flags were parsed.
  is_successful_ = true;
}

/* static */
bool DartAnalyzerFlags::IsDartAnalyzerCommand(absl::string_view arg) {
  const absl::string_view basename = GetBasename(arg);
  return absl::StrContains(basename, "dartanalyzer");
}

/* static */
std::string DartAnalyzerFlags::GetCompilerName(absl::string_view /*arg*/) {
  return "dartanalyzer";
}

// TODO: verify input
bool DartAnalyzerFlags::SplitURIMapping(
    absl::string_view raw,
    std::pair<std::string, std::string>* split) const {
  absl::ConsumePrefix(&raw, "--url-mapping=");
  // Due to a limitation in windows build configuration, we cannot directly use
  // *split = absl::StrSplit(raw, ',') here.
  std::vector<std::string> split_vector;
  for (auto&& s : absl::StrSplit(raw, ',')) {
    split_vector.emplace_back(s);
  }
  if (split_vector.size() == 2) {
    split->first = split_vector[0];
    split->second = split_vector[1];
    return true;
  }
  return false;
}

}  // namespace devtools_goma
