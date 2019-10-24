// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>

#include "flag_parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMaxFlagLength = 4;

  FuzzedDataProvider provider(data, size);
  FlagParser flag_parser, prefix_flag_parser, bool_flag_parser;
  if (provider.remaining_bytes() > 0) {
    std::string flag;
    do {
      flag = provider.ConsumeRandomLengthString(kMaxFlagLength);
    } while (flag.empty());
    flag_parser.AddFlag(flag.c_str());
    bool_flag_parser.AddBoolFlag(flag.c_str());
    prefix_flag_parser.AddPrefixFlag(flag.c_str());
  }

  std::vector<std::string> input;
  size_t remaining = provider.remaining_bytes();
  // We want to have at least two input strings, but also avoid zero max length,
  // which will never exit the loop.
  const size_t max_input_length = std::max(1UL, remaining / 2);
  while (provider.remaining_bytes() > 0) {
    input.push_back(provider.ConsumeRandomLengthString(max_input_length));
  }

  flag_parser.Parse(input);
  bool_flag_parser.Parse(input);
  prefix_flag_parser.Parse(input);

  return 0;
}
