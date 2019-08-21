// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client/proto_util.h"

#include "absl/time/time.h"
#include "google/protobuf/timestamp.pb.h"

namespace devtools_goma {

absl::Time ProtoToTime(const google::protobuf::Timestamp& ts) {
  return absl::TimeFromTimespec(timespec{ts.seconds(), ts.nanos()});
}

google::protobuf::Timestamp TimeToProto(const absl::Time& t) {
  google::protobuf::Timestamp timestamp;
  const auto ts = absl::ToTimespec(t);
  timestamp.set_seconds(ts.tv_sec);
  timestamp.set_nanos(ts.tv_nsec);
  return timestamp;
}

}  // namespace devtools_goma
