// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <string>

#include "lexer.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  const auto content = devtools_goma::Content::CreateFromString(input);

  std::vector<devtools_goma::modulemap::Token> tokens;
  devtools_goma::modulemap::Lexer::Run(*content, &tokens);

  return 0;
}
