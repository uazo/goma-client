// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/rustc_flags.h"

#include "absl/strings/str_split.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "lib/flag_parser.h"
#include "lib/path_util.h"

namespace devtools_goma {

/* static */
void RustcFlags::DefineFlags(FlagParser* parser) {
  FlagParser::Options* opts = parser->mutable_options();
  opts->flag_prefix = '-';
  static const struct {
    const char* name;
    FlagType flag_type;
  } kFlags[] = {
      {"L",
       kNormal},  // -L native=...; Add a directory to the library search path.
      {"l",
       kNormal},  // Link the generated crate(s) to the specified library NAME.
      {"-cfg", kNormal},         // Configure the compilation environment.
      {"-crate-type", kNormal},  // Comma separated list of types of crates for
                                 // the compiler to emit.
      {"-crate-name", kNormal},  // Specify the name of the crate being built.
      {"-emit", kNormal},   // Configure the output that rustc will produce.
      {"-emit=", kPrefix},  // --emit=...
      {"-print", kNormal},  // Comma separated list of compiler information to
                            // print on stdout.
      {"g", kBool},         // Equivalent to -c debuginfo=2.
      {"O", kBool},         // Equivalent to -c opt-level=2.
      {"-explain", kBool},  // --explain OPT; provide a detailed explaination of
                            // an error message.
      {"-test", kBool},     // Build a test harness.
      {"-target", kNormal},  // --target TARGET; Define target triple for which
                             // the code is compiled.
      {"W", kNormal},        // -W OTP; Set lint warnings.
      {"-warn", kNormal},    // --warn OPT
      {"A", kNormal},        // -A OPT; Set lint allowed.
      {"-allow", kNormal},   // --allow OPT
      {"D", kNormal},        // -D OPT; Set lint denied.
      {"-deny", kNormal},    // --deny OPT
      {"F", kNormal},        // -F OPT; Set lint forbidden.
      {"-forbid", kNormal},  // --forbid OPT
      {"C", kNormal},  // -C FLAG[=VAL]; Set a codegen-related flag to the value
                       // specified.
      {"-codegen", kNormal},            // --codegen FLAG[=VAL]
      {"V", kBool},                     // Print version info and exit.
      {"-version", kBool},              // --version
      {"v", kBool},                     // -v; Use verbose output.
      {"-verbose", kBool},              //  --verbose
      {"-remap-path-prefix", kNormal},  // --remap-path-prefix from=to; Remap
                                        // source path prefixes in all output.
      {"-extern", kNormal},     // --extern NAME=PATH Specify where an external
                                // rust library is loca
      {"-sysroot", kNormal},    // --sysroot PATH; Override the system root.
      {"Z", kNormal},           // -Z FLAG; Set internal debugging options.
      {"-color", kNormal},      // --color auto; Confgure coloring of output.
      {"-cap-lints", kNormal},  // --cap-lints alow; set the maximum possible
                                // lint level for the entire crate.
  };

  for (const auto& f : kFlags) {
    switch (f.flag_type) {
      case kNormal:
        parser->AddFlag(f.name);
        break;
      case kPrefix:
        parser->AddPrefixFlag(f.name);
        break;
      case kBool:
        parser->AddBoolFlag(f.name);
        break;
    }
  }
}

RustcFlags::RustcFlags(const std::vector<std::string>& args,
                       const std::string& cwd)
    : CompilerFlags(args, cwd) {
  // example rustc command line (from cargo)
  //
  // rustc --crate-name rand
  // /home/goma/.cargo/registry/src/github.com-1ecc6299db9ec823\
  // /rand-0.5.3/src/lib.rs
  // --crate-type lib
  // --emit=dep-info,link
  // -C debuginfo=2
  // --cfg feature="alloc"
  // --cfg feature="cloudabi"
  // --cfg feature="default"
  // --cfg feature="fuchsia-zircon"
  // --cfg feature="libc"
  // --cfg feature="rand_core"
  // --cfg feature="std"
  // --cfg feature="winapi"
  // -C metadata=732894137054066a
  // -C extra-filename=-732894137054066a
  // --out-dir /home/goma/tmp/cargo-test/target/debug/deps
  // -L dependency=/home/goma/tmp/cargo-test/target/debug/deps
  // --extern
  // libc=/home/goma/tmp/cargo-test/target/debug/deps\
  // /liblibc-463874d8fa76eafc.rlib
  // --extern
  // rand_core=/home/goma/tmp/cargo-test/target/debug/deps\
  // /librand_core-77ec6d8abf82a269.rlib
  // --cap-lints allow

  FlagParser parser;
  DefineFlags(&parser);
  // Here, output directory is specified instead of listing output files.
  // When -out-dir is not specified, maybe we have to list output files
  // or use cwd as output_dirs?
  parser.AddFlag("-out-dir")
      ->SetValueOutputWithCallback(nullptr, &output_dirs_);

  parser.AddFlag("o")->SetValueOutputWithCallback(nullptr, &output_files_);
  parser.AddPrefixFlag("o")->SetValueOutputWithCallback(nullptr,
                                                        &output_files_);

  FlagParser::Flag* flag_target = parser.AddFlag("-target");

  std::vector<std::string> remained_flags;
  parser.AddNonFlag()->SetOutput(&remained_flags);
  parser.Parse(args_);

  target_ = flag_target->GetLastValue();
  unknown_flags_ = parser.unknown_flag_args();

  for (auto&& arg : remained_flags) {
    if (absl::EndsWith(arg, ".rs")) {
      input_filenames_.push_back(std::move(arg));
    }
  }

  // set is_successful_ = true to indicate everything is OK.
  is_successful_ = true;
  lang_ = "rust";
}

/* static */
bool RustcFlags::IsRustcCommand(absl::string_view arg) {
  const absl::string_view basename = GetBasename(arg);
  return absl::StrContains(basename, "rustc");
}

/* static */
std::string RustcFlags::GetCompilerName(absl::string_view /*arg*/) {
  return "rustc";
}

}  // namespace devtools_goma
