/*
 * Copyright 2011 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "platform_thread.h"

#include <string>

#include "glog/logging.h"

#ifdef _WIN32
#include <process.h>
#endif

#if HAVE_CPU_PROFILER
#include <gperftools/profiler.h>
#endif

namespace devtools_goma {

#if defined (_WIN32)

unsigned __stdcall ThreadFunc(void* params) {
  PlatformThread::Delegate* delegate =
      static_cast<PlatformThread::Delegate*>(params);
  delegate->ThreadMain();
  return 0;
}

// Return a wide (UTF-16 or UTF-32) format version of UTF-8-format |string|.
std::wstring UTF8ToWide(absl::string_view string) {
  if (string.empty())
    return std::wstring();
  const int length = string.length();

  // Compute the length of the buffer to hold the wide string.
  const int wide_length =
      MultiByteToWideChar(CP_UTF8, 0, string.data(), length, nullptr, 0);
  if (wide_length == 0)
    return std::wstring();

  std::wstring wide;
  wide.resize(wide_length);
  MultiByteToWideChar(CP_UTF8, 0, string.data(), length, &wide[0], wide_length);
  return wide;
}

// static
bool PlatformThread::Create(Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  CHECK(thread_handle);
  uintptr_t r = _beginthreadex(nullptr, 0, ThreadFunc, delegate, 0, nullptr);
  if (r == 0) {
    return false;
  }

  *thread_handle = reinterpret_cast<HANDLE>(r);
  return true;
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  CHECK(thread_handle);
  DWORD result = WaitForSingleObject(thread_handle, INFINITE);
  CHECK(result == WAIT_OBJECT_0);
  CloseHandle(thread_handle);
}

// static
void PlatformThread::SetName(PlatformThreadHandle thread_handle,
                             absl::string_view name) {
  // Get a function pointer to Windows-API call ::SetThreadDescription(), added
  // in Windows 10 version 1607.
  auto set_thread_description_func =
      reinterpret_cast<decltype(::SetThreadDescription)*>(::GetProcAddress(
          ::GetModuleHandle(L"Kernel32.dll"), "SetThreadDescription"));
  // Set thread name if function ::SetThreadDescription() exists on this system.
  if (set_thread_description_func) {
    set_thread_description_func(thread_handle, UTF8ToWide(name).c_str());
  }
}

#else

void* ThreadFunc(void* params) {
#if HAVE_CPU_PROFILER
  ProfilerRegisterThread();
#endif
  PlatformThread::Delegate* delegate =
      static_cast<PlatformThread::Delegate*>(params);
  delegate->ThreadMain();
  return nullptr;
}

// static
bool PlatformThread::Create(Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  CHECK(thread_handle);

  bool success = false;
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  success = !pthread_create(thread_handle, &attributes, ThreadFunc, delegate);
  PLOG_IF(ERROR, !success) << "pthread_create";
  pthread_attr_destroy(&attributes);

  return success;
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  CHECK(thread_handle);
  pthread_join(thread_handle, nullptr);
}

// static
void PlatformThread::SetName(PlatformThreadHandle thread_handle,
                             absl::string_view name) {
  if (name.empty())
    return;

  // Truncate |name| to 15 characters, the maximum length for a pthread name.
  const int pthread_name_max_length = 15;
  if (name.length() > pthread_name_max_length)
    name = name.substr(0, pthread_name_max_length);

#if defined(__MACH__)
  pthread_setname_np(std::string(name).c_str());
#else
  pthread_setname_np(thread_handle, std::string(name).c_str());
#endif  // __MACH__
}

#endif  // _WIN32

}  // namespace devtools_goma
