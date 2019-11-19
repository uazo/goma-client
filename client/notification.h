// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVTOOLS_GOMA_CLIENT_NOTIFICATION_H_
#define DEVTOOLS_GOMA_CLIENT_NOTIFICATION_H_

#include "autolock_timer.h"
#include "lockhelper.h"

namespace devtools_goma {
// See absl::Notification for documentation. This is mostly a drop-in
// replacement of absl::Notification.
//
class Notification {
 public:
  Notification() = default;
  ~Notification();

  Notification(const Notification&) = delete;
  Notification& operator=(const Notification&) = delete;

  bool HasBeenNotified() const LOCKS_EXCLUDED(mu_);
  void WaitForNotification() LOCKS_EXCLUDED(mu_);
  void Notify() LOCKS_EXCLUDED(mu_);

 private:
  mutable Lock mu_;
  ConditionVariable cv_ GUARDED_BY(mu_);
  bool has_been_notified_ GUARDED_BY(mu_) = false;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_NOTIFICATION_H_
