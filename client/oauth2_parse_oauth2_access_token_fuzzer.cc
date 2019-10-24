// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "oauth2.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);

  std::string token_type, access_token;
  absl::Duration expires_in;
  devtools_goma::ParseOAuth2AccessToken(input, &token_type, &access_token,
                                        &expires_in);

  return 0;
}
