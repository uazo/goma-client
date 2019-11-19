// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notification.h"

namespace devtools_goma {

Notification::~Notification() {
  // Make sure that the thread running Notify() exits before the object is
  // destructed.
  AUTOLOCK(lock, &mu_);
}

bool Notification::HasBeenNotified() const {
  AUTOLOCK(lock, &mu_);
  return has_been_notified_;
}

void Notification::WaitForNotification() {
  AUTOLOCK(lock, &mu_);
  while (!has_been_notified_) {
    cv_.Wait(&mu_);
  }
}

void Notification::Notify() {
  AUTOLOCK(lock, &mu_);
  has_been_notified_ = true;
  cv_.Broadcast();
}

}  // namespace devtools_goma
