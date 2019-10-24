// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <string>

#include "http_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  size_t content_length, body_offset;
  bool is_chunked;
  devtools_goma::FindContentLengthAndBodyOffset(input, &content_length,
                                                &body_offset, &is_chunked);

  return 0;
}
