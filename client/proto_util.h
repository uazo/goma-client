// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_PROTO_UTIL_H_
#define DEVTOOLS_GOMA_CLIENT_PROTO_UTIL_H_

#include "absl/time/time.h"
#include "google/protobuf/timestamp.pb.h"

namespace devtools_goma {

absl::Time ProtoToTime(const google::protobuf::Timestamp&);

google::protobuf::Timestamp TimeToProto(const absl::Time&);

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_PROTO_UTIL_H_
