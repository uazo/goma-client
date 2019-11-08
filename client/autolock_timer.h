// Copyright 2012 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_AUTOLOCK_TIMER_H_
#define DEVTOOLS_GOMA_CLIENT_AUTOLOCK_TIMER_H_

#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "basictypes.h"
#include "lockhelper.h"
#include "simple_timer.h"

namespace devtools_goma {

class AutoLockStat {
 public:
  explicit AutoLockStat(const char* auto_lock_name)
      : name(auto_lock_name), count_(0) {}
  const char* name;

  void GetStats(int* count,
                absl::Duration* total_wait_time,
                absl::Duration* max_wait_time,
                absl::Duration* total_hold_time,
                absl::Duration* max_hold_time) {
    AutoFastLock lock(&lock_);
    *count = count_;
    *total_wait_time = total_wait_time_;
    *max_wait_time = max_wait_time_;
    *total_hold_time = total_hold_time_;
    *max_hold_time = max_hold_time_;
  }

  void UpdateWaitTime(absl::Duration wait_time) {
    AutoFastLock lock(&lock_);
    ++count_;
    total_wait_time_ += wait_time;
    if (wait_time > max_wait_time_)
      max_wait_time_ = wait_time;
  }

  void UpdateHoldTime(absl::Duration hold_time) {
    AutoFastLock lock(&lock_);
    total_hold_time_ += hold_time;
    if (hold_time > max_hold_time_)
      max_hold_time_ = hold_time;
  }

 private:
  FastLock lock_;
  int count_ GUARDED_BY(lock_);
  absl::Duration total_wait_time_ GUARDED_BY(lock_);
  absl::Duration max_wait_time_ GUARDED_BY(lock_);
  absl::Duration total_hold_time_ GUARDED_BY(lock_);
  absl::Duration max_hold_time_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(AutoLockStat);
};

class AutoLockStats {
 public:
  AutoLockStats() {}

  // Return initialized AutoLockStat for |name|.
  // |name| should be string literal (it must not be released).
  // This should be called once in a location.
  // e.g.
  //   static AutoLockStat* stat = g_auto_lock_stats_->NewStat(name);
  //
  AutoLockStat* NewStat(const char* name);

  void Report(std::ostringstream* ss,
              const absl::flat_hash_set<std::string>& skip_names);
  void TextReport(std::ostringstream* ss);

 private:
  mutable Lock mu_;
  std::vector<std::unique_ptr<AutoLockStat>> stats_ GUARDED_BY(mu_);
  DISALLOW_COPY_AND_ASSIGN(AutoLockStats);
};

extern AutoLockStats* g_auto_lock_stats;

class MutexAcquireStrategy {
 public:
  static void Acquire(Lock* lock) EXCLUSIVE_LOCK_FUNCTION(lock) {
    lock->Acquire();
  }

  static void Release(Lock* lock) UNLOCK_FUNCTION(lock) {
    lock->Release();
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MutexAcquireStrategy);
};

class ReadWriteLockAcquireSharedStrategy {
 public:
  static void Acquire(ReadWriteLock* lock) SHARED_LOCK_FUNCTION(lock) {
    lock->AcquireShared();
  }

  static void Release(ReadWriteLock* lock) UNLOCK_FUNCTION(lock) {
    lock->ReleaseShared();
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ReadWriteLockAcquireSharedStrategy);
};

class ReadWriteLockAcquireExclusiveStrategy {
 public:
  static void Acquire(ReadWriteLock* lock) EXCLUSIVE_LOCK_FUNCTION(lock) {
    lock->AcquireExclusive();
  }

  static void Release(ReadWriteLock* lock) UNLOCK_FUNCTION(lock) {
    lock->ReleaseExclusive();
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ReadWriteLockAcquireExclusiveStrategy);
};

class AbslMutexAcquireSharedStrategy {
 public:
  static void Acquire(absl::Mutex* lock) SHARED_LOCK_FUNCTION(lock) {
    lock->ReaderLock();
  }

  static void Release(absl::Mutex* lock) UNLOCK_FUNCTION(lock) {
    lock->ReaderUnlock();
  }

  AbslMutexAcquireSharedStrategy() = delete;
  AbslMutexAcquireSharedStrategy(const AbslMutexAcquireSharedStrategy&) =
      delete;
  AbslMutexAcquireSharedStrategy& operator=(
      const AbslMutexAcquireSharedStrategy&) = delete;
};

class AbslMutexAcquireExclusiveStrategy {
 public:
  static void Acquire(absl::Mutex* lock) EXCLUSIVE_LOCK_FUNCTION(lock) {
    lock->Lock();
  }

  static void Release(absl::Mutex* lock) UNLOCK_FUNCTION(lock) {
    lock->Unlock();
  }

  AbslMutexAcquireExclusiveStrategy() = delete;
  AbslMutexAcquireExclusiveStrategy(const AbslMutexAcquireExclusiveStrategy&) =
      delete;
  AbslMutexAcquireExclusiveStrategy& operator=(
      const AbslMutexAcquireExclusiveStrategy&) = delete;
};

template<typename LockType, typename LockAcquireStrategy>
class AutoLockTimerBase {
 public:
  // Auto lock on |lock| with stats of |name|.
  // |name| must be string literal. It must not be deleted.
  // If |statp| is NULL, it doesn't collect stats (i.e. it works as
  // almost same as AutoLock).
  // If |statp| is not NULL, it holds stats for lock wait/hold time.
  AutoLockTimerBase(LockType* lock, AutoLockStat* statp)
      : lock_(lock), stat_(nullptr), timer_(SimpleTimer::NO_START) {
    if (statp)
      timer_.Start();
    LockAcquireStrategy::Acquire(lock_);
    if (statp) {
      stat_ = statp;
      stat_->UpdateWaitTime(timer_.GetDuration());
      timer_.Start();
    }
  }

  ~AutoLockTimerBase() {
    if (stat_) {
      stat_->UpdateHoldTime(timer_.GetDuration());
    }
    LockAcquireStrategy::Release(lock_);
  }

 private:
  LockType* lock_;
  AutoLockStat* stat_;
  SimpleTimer timer_;
  DISALLOW_COPY_AND_ASSIGN(AutoLockTimerBase);
};

class SCOPED_LOCKABLE AutoLockTimer
    : private AutoLockTimerBase<Lock, MutexAcquireStrategy> {
 public:
  AutoLockTimer(Lock* lock,
                AutoLockStat* statp) EXCLUSIVE_LOCK_FUNCTION(lock)
      : AutoLockTimerBase(lock, statp) {
  }

  ~AutoLockTimer() UNLOCK_FUNCTION() {
  }
};

class SCOPED_LOCKABLE AutoAbslMutexTimer
    : private AutoLockTimerBase<absl::Mutex,
                                AbslMutexAcquireExclusiveStrategy> {
 public:
  AutoAbslMutexTimer(absl::Mutex* lock, AutoLockStat* statp)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : AutoLockTimerBase(lock, statp) {}

  ~AutoAbslMutexTimer() UNLOCK_FUNCTION() {}
};

class SCOPED_LOCKABLE AutoReadWriteLockSharedTimer
    : private AutoLockTimerBase<ReadWriteLock,
                                ReadWriteLockAcquireSharedStrategy> {
 public:
  AutoReadWriteLockSharedTimer(ReadWriteLock* lock,
                               AutoLockStat* statp) SHARED_LOCK_FUNCTION(lock)
      : AutoLockTimerBase(lock, statp) {
  }

  ~AutoReadWriteLockSharedTimer() UNLOCK_FUNCTION() {}
};

class SCOPED_LOCKABLE AutoReadWriteLockExclusiveTimer
    : private AutoLockTimerBase<ReadWriteLock,
                                ReadWriteLockAcquireExclusiveStrategy> {
 public:
  AutoReadWriteLockExclusiveTimer(ReadWriteLock* lock,
                                  AutoLockStat* statp)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : AutoLockTimerBase(lock, statp) {
  }

  ~AutoReadWriteLockExclusiveTimer() UNLOCK_FUNCTION() {}
};

class SCOPED_LOCKABLE AutoAbslMutexSharedTimer
    : private AutoLockTimerBase<absl::Mutex, AbslMutexAcquireSharedStrategy> {
 public:
  AutoAbslMutexSharedTimer(absl::Mutex* lock, AutoLockStat* statp)
      SHARED_LOCK_FUNCTION(lock)
      : AutoLockTimerBase(lock, statp) {}

  ~AutoAbslMutexSharedTimer() UNLOCK_FUNCTION() {}
};

class SCOPED_LOCKABLE AutoAbslMutexExclusiveTimer
    : private AutoLockTimerBase<absl::Mutex,
                                AbslMutexAcquireExclusiveStrategy> {
 public:
  AutoAbslMutexExclusiveTimer(absl::Mutex* lock, AutoLockStat* statp)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : AutoLockTimerBase(lock, statp) {}

  ~AutoAbslMutexExclusiveTimer() UNLOCK_FUNCTION() {}
};

namespace internal {

template <typename T>
using DecayPtr =
    typename std::decay<typename std::remove_pointer<T>::type>::type;

template <typename T>
struct AutoLockTrait;

template <typename T>
struct AutoRwLockTrait;

#ifdef NO_AUTOLOCK_STAT
template <>
struct AutoLockTrait<Lock> {
  using Type = AutoLock;
};

template <>
struct AutoLockTrait<absl::Mutex> {
  using Type = absl::MutexLock;
};

template <>
struct AutoRwLockTrait<ReadWriteLock> {
  using Shared = AutoSharedLock;
  using Exclusive = AutoExclusiveLock;
};

template <>
struct AutoRwLockTrait<absl::Mutex> {
  using Shared = absl::ReaderMutexLock;
  using Exclusive = absl::MutexLock;
};
#else
template <>
struct AutoLockTrait<Lock> {
  using Type = AutoLockTimer;
};

template <>
struct AutoLockTrait<absl::Mutex> {
  using Type = AutoAbslMutexTimer;
};

template <>
struct AutoRwLockTrait<ReadWriteLock> {
  using Shared = AutoReadWriteLockSharedTimer;
  using Exclusive = AutoReadWriteLockExclusiveTimer;
};

template <>
struct AutoRwLockTrait<absl::Mutex> {
  using Shared = AutoAbslMutexSharedTimer;
  using Exclusive = AutoAbslMutexExclusiveTimer;
};
#endif  // NO_AUTOLOCK_STAT

template <typename T>
using GetAutoLockType = typename AutoLockTrait<DecayPtr<T>>::Type;

template <typename T>
using GetAutoRwLockSharedType = typename AutoRwLockTrait<DecayPtr<T>>::Shared;

template <typename T>
using GetAutoRwLockExclusiveType =
    typename AutoRwLockTrait<DecayPtr<T>>::Exclusive;

}  // namespace internal

#define GOMA_AUTOLOCK_TIMER_STRINGFY(i) #i
#define GOMA_AUTOLOCK_TIMER_STR(i) GOMA_AUTOLOCK_TIMER_STRINGFY(i)
// #define NO_AUTOLOCK_STAT
#ifdef NO_AUTOLOCK_STAT
#define AUTOLOCK(lock, mu) internal::GetAutoLockType<decltype(mu)> lock(mu)
#define AUTOLOCK_WITH_STAT(lock, mu, statp) \
  internal::GetAutoLockType<decltype(mu)> lock(mu)
#define AUTO_SHARED_LOCK(lock, rwlock) \
  internal::GetAutoRwLockSharedType<decltype(rwlock)> lock(rwlock)
#define AUTO_EXCLUSIVE_LOCK(lock, rwlock) \
  internal::GetAutoRwLockExclusiveType<decltype(rwlock)> lock(rwlock)
#else
#define AUTOLOCK(lock, mu)                                                  \
  static AutoLockStat* auto_lock_stat_for_the_source_location =             \
      g_auto_lock_stats                                                     \
          ? g_auto_lock_stats->NewStat(                                     \
                __FILE__ ":" GOMA_AUTOLOCK_TIMER_STR(__LINE__) "(" #mu ")") \
          : nullptr;                                                        \
  internal::GetAutoLockType<decltype(mu)> lock(                             \
      mu, auto_lock_stat_for_the_source_location);

#define AUTOLOCK_WITH_STAT(lock, mu, statp) \
  internal::GetAutoLockType<decltype(mu)> lock(mu, statp);
#define AUTO_SHARED_LOCK(lock, rwlock)                                       \
  static AutoLockStat* auto_lock_stat_for_the_source_location =              \
      g_auto_lock_stats                                                      \
          ? g_auto_lock_stats->NewStat(__FILE__ ":" GOMA_AUTOLOCK_TIMER_STR( \
                __LINE__) "(" #rwlock ":r)")                                 \
          : nullptr;                                                         \
  internal::GetAutoRwLockSharedType<decltype(rwlock)> lock(                  \
      rwlock, auto_lock_stat_for_the_source_location);
#define AUTO_EXCLUSIVE_LOCK(lock, rwlock)                                    \
  static AutoLockStat* auto_lock_stat_for_the_source_location =              \
      g_auto_lock_stats                                                      \
          ? g_auto_lock_stats->NewStat(__FILE__ ":" GOMA_AUTOLOCK_TIMER_STR( \
                __LINE__) "(" #rwlock ":w)")                                 \
          : nullptr;                                                         \
  internal::GetAutoRwLockExclusiveType<decltype(rwlock)> lock(               \
      rwlock, auto_lock_stat_for_the_source_location);
#endif  // NO_AUTOLOCK_STAT

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_AUTOLOCK_TIMER_H_
