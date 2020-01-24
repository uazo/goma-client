// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "lockhelper.h"

#ifdef _WIN32
# include <stack>
# include <glog/logging.h>
#endif

namespace devtools_goma {
#if defined (_WIN32)

OsDependentLock::OsDependentLock() {
  // The second parameter is the spin count, for short-held locks it avoid the
  // contending thread from going to sleep which helps performance greatly.
  ::InitializeCriticalSectionAndSpinCount(&os_lock_, 2000);
}

OsDependentLock::~OsDependentLock() {
  ::DeleteCriticalSection(&os_lock_);
}

bool OsDependentLock::Try() {
  if (::TryEnterCriticalSection(&os_lock_) != FALSE) {
    return true;
  }
  return false;
}

void OsDependentLock::Acquire() {
  ::EnterCriticalSection(&os_lock_);
}

void OsDependentLock::Release() {
  ::LeaveCriticalSection(&os_lock_);
}

OsDependentRwLock::OsDependentRwLock() {
  ::InitializeSRWLock(&srw_lock_);
}

OsDependentRwLock::~OsDependentRwLock() {}

void OsDependentRwLock::AcquireShared() {
  ::AcquireSRWLockShared(&srw_lock_);
}

void OsDependentRwLock::ReleaseShared() {
  ::ReleaseSRWLockShared(&srw_lock_);
}

void OsDependentRwLock::AcquireExclusive() {
  ::AcquireSRWLockExclusive(&srw_lock_);
}

void OsDependentRwLock::ReleaseExclusive() {
  ::ReleaseSRWLockExclusive(&srw_lock_);
}

OsDependentCondVar::OsDependentCondVar() {
  ::InitializeConditionVariable(&cv_);
}

OsDependentCondVar::~OsDependentCondVar() {}

void OsDependentCondVar::Wait(OsDependentLock* lock) {
  CRITICAL_SECTION* cs = &lock->os_lock_;

  if (FALSE == SleepConditionVariableCS(&cv_, cs, INFINITE)) {
    DCHECK(GetLastError() != WAIT_TIMEOUT);
  }
}

void OsDependentCondVar::Broadcast() {
  WakeAllConditionVariable(&cv_);
}

void OsDependentCondVar::Signal() {
  WakeConditionVariable(&cv_);
}

#else

OsDependentLock::OsDependentLock() {
  pthread_mutex_init(&os_lock_, nullptr);
}

OsDependentLock::~OsDependentLock() {
  pthread_mutex_destroy(&os_lock_);
}

bool OsDependentLock::Try() {
  return (pthread_mutex_trylock(&os_lock_) == 0);
}

void OsDependentLock::Acquire() {
  pthread_mutex_lock(&os_lock_);
}

void OsDependentLock::Release() {
  pthread_mutex_unlock(&os_lock_);
}

OsDependentRwLock::OsDependentRwLock() {
  pthread_rwlock_init(&os_rwlock_, nullptr);
}

OsDependentRwLock::~OsDependentRwLock() {
  pthread_rwlock_destroy(&os_rwlock_);
}

void OsDependentRwLock::AcquireShared() {
  pthread_rwlock_rdlock(&os_rwlock_);
}

void OsDependentRwLock::ReleaseShared() {
  pthread_rwlock_unlock(&os_rwlock_);
}

void OsDependentRwLock::AcquireExclusive() {
  pthread_rwlock_wrlock(&os_rwlock_);
}

void OsDependentRwLock::ReleaseExclusive() {
  pthread_rwlock_unlock(&os_rwlock_);
}

OsDependentCondVar::OsDependentCondVar() {
  pthread_cond_init(&condition_, nullptr);
}

OsDependentCondVar::~OsDependentCondVar() {
  pthread_cond_destroy(&condition_);
}

void OsDependentCondVar::Wait(OsDependentLock* lock) {
  pthread_cond_wait(&condition_, &lock->os_lock_);
}

void OsDependentCondVar::Signal() {
  pthread_cond_signal(&condition_);
}

void OsDependentCondVar::Broadcast() {
  pthread_cond_broadcast(&condition_);
}
#endif  // _WIN32

bool AbslBackedLock::Try() {
  return mu_.TryLock();
}

void AbslBackedLock::Acquire() {
  mu_.Lock();
}

void AbslBackedLock::Release() {
  mu_.Unlock();
}

void AbslBackedLock::AcquireShared() {
  mu_.ReaderLock();
}

void AbslBackedLock::ReleaseShared() {
  mu_.ReaderUnlock();
}

void AbslBackedLock::AcquireExclusive() {
  mu_.Lock();
}

void AbslBackedLock::ReleaseExclusive() {
  mu_.Unlock();
}

void AbslBackedCondVar::Wait(AbslBackedLock* lock) {
  cv_.Wait(&lock->mu_);
}

bool AbslBackedCondVar::WaitWithTimeout(AbslBackedLock* lock,
                                        absl::Duration timeout) {
  return cv_.WaitWithTimeout(&lock->mu_, timeout);
}

void AbslBackedCondVar::Signal() {
  cv_.Signal();
}

void AbslBackedCondVar::Broadcast() {
  cv_.SignalAll();
}

}  // namespace devtools_goma
