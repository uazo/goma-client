// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "flag_parser.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

MSVC_PUSH_DISABLE_WARNING_FOR_PROTO()
#include "prototmp/flag_parser_fuzzer.pb.h"
MSVC_POP_WARNING()

DEFINE_PROTO_FUZZER(const devtools_goma::FlagParserFuzzingSession& session) {
  using Session = devtools_goma::FlagParserFuzzingSession;
  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << session.DebugString() << std::endl;
  }

  FlagParser parser;
  auto* options = parser.mutable_options();
  // TODO: Fuzz flag prefix as well?
  options->flag_prefix = '\0';
  options->allows_nonspace_arg = session.allows_nonspace_arg();
  options->allows_equal_arg = session.allows_equal_arg();

  std::vector<std::string> args;
  for (const auto& nv : session.name_values()) {
    if (nv.add_to_parser()) {
      switch (nv.type()) {
        case Session::BOOL:
          parser.AddBoolFlag(nv.name().c_str());
          break;
        case Session::PREFIX:
          parser.AddPrefixFlag(nv.name().c_str());
          break;
        default:
          parser.AddFlag(nv.name().c_str());
          break;
      }
    }
    // If |nv| is not added to |parser|, then at least add it to |args| to make
    // it useful.
    if (nv.add_to_args() || !nv.add_to_parser()) {
      std::stringstream ss;
      switch (nv.style()) {
        case Session::NON_SPACE:
          ss << nv.name() << nv.value();
          break;
        case Session::EQUAL_SIGN:
          ss << nv.name() << '=' << nv.value();
          break;
        default:
          ss << nv.name() << ' ' << nv.value();
          break;
      }
      args.push_back(ss.str());
    }
  }
  parser.Parse(args);
}
