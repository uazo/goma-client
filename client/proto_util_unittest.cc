// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client/proto_util.h"

#include "absl/time/clock.h"
#include "gtest/gtest.h"

namespace devtools_goma {

TEST(ProtoUtil, Simple) {
  auto now = absl::Now();
  EXPECT_EQ(now, ProtoToTime(TimeToProto(now)));
}

}  // namespace devtools_goma
