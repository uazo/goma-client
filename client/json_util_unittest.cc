// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_util.h"

#include <gtest/gtest.h>

#include <string>

TEST(JsonUtilTest, GetStringNoCrash) {
  Json::Reader r;
  Json::Value json;
  EXPECT_TRUE(r.parse("42", json, false));

  std::string val;
  std::string err;
  EXPECT_FALSE(devtools_goma::GetStringFromJson(json, "foo", &val, &err));
  EXPECT_TRUE(val.empty());
  EXPECT_FALSE(err.empty());
}
