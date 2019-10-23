// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <string>

#include "cpp_directive_parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  devtools_goma::CppDirectiveParser::ParseFromString(input, "filename_unused");
  return 0;
}
