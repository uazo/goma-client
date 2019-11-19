// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notification.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "absl/time/clock.h"
#include "atomic_stats_counter.h"
#include "test_worker_thread.h"

namespace devtools_goma {

class Worker : public TestWorkerThread {
 public:
  Worker(Notification* n, StatsCounter* counter) : n_(n), counter_(counter) {}

  void ThreadMain() override {
    n_->WaitForNotification();
    counter_->Add(1);
  }

 private:
  Notification* n_;
  StatsCounter* counter_;
};

TEST(NotificationTest, Basic) {
  constexpr int kNumWorkers = 100;
  Notification n;
  StatsCounter counter;
  std::vector<std::unique_ptr<Worker>> workers;
  for (int i = 0; i < kNumWorkers; ++i) {
    auto worker = absl::make_unique<Worker>(&n, &counter);
    worker->Start();
    workers.push_back(std::move(worker));
  }
  absl::SleepFor(absl::Milliseconds(200));
  EXPECT_EQ(counter.value(), 0);
  n.Notify();
  EXPECT_TRUE(n.HasBeenNotified());
  for (auto& w : workers) {
    w->Join();
  }
  EXPECT_EQ(counter.value(), kNumWorkers);
}

}  // namespace devtools_goma
