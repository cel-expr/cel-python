// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_PYTHON_FREE_THREADING_MUTEX_H_
#define THIRD_PARTY_CEL_PYTHON_FREE_THREADING_MUTEX_H_

#include <Python.h>  // IWYU pragma: keep - Needed for Py_GIL_DISABLED

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"

#ifndef Py_GIL_DISABLED
#else
#include "absl/synchronization/mutex.h"
#endif

namespace cel_python {

// Zero-cost mutex wrapper that compiles away to nothing in standard GIL builds,
// and uses absl::Mutex in free-threaded builds (Py_GIL_DISABLED).
class ABSL_LOCKABLE ABSL_ATTRIBUTE_WARN_UNUSED FreeThreadingMutex {
 public:
  FreeThreadingMutex() = default;
  FreeThreadingMutex(const FreeThreadingMutex&) = delete;
  FreeThreadingMutex& operator=(const FreeThreadingMutex&) = delete;

#ifndef Py_GIL_DISABLED
  // In GIL-enabled builds, this mutex compiles away to zero-cost no-ops while
  // retaining thread-safety annotations for Clang static analysis.
  //
  // Relationship with the GIL:
  // - For operations accessing Python state or cached PyObjects (such as
  //   PyCelValue::Value() or PyMessageFactory::GetMessageClass()), mutual
  //   exclusion between threads is provided by the Python GIL itself.
  // - For pure C++ operations (such as PyCelEnv::Compile() or deserialization),
  //   the GIL is intentionally released (via py::gil_scoped_release) to enable
  //   parallel execution and prevent deadlocks with DescriptorPool's mutex.
  //   Objects may therefore be constructed or moved while the GIL is NOT held.
  // - Consequently, Lock() and Unlock() do not assert PyGILState_Check(),
  //   allowing lock acquisition and move semantics to function safely whether
  //   the GIL is currently held or released. Call sites that strictly require
  //   the GIL explicitly verify or acquire it themselves.
  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() {}
  void Unlock() ABSL_UNLOCK_FUNCTION() {}
#else
  // Free-threaded build: real mutex
  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() { mutex_.Lock(); }
  void Unlock() ABSL_UNLOCK_FUNCTION() { mutex_.Unlock(); }

 private:
  absl::Mutex mutex_;
#endif
};

// RAII lock guard for FreeThreadingMutex.
class ABSL_SCOPED_LOCKABLE FreeThreadingLockGuard {
 public:
  explicit FreeThreadingLockGuard(FreeThreadingMutex& mutex)
      ABSL_EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex_.Lock();
  }
  ~FreeThreadingLockGuard() ABSL_UNLOCK_FUNCTION() { mutex_.Unlock(); }

  FreeThreadingLockGuard(const FreeThreadingLockGuard&) = delete;
  FreeThreadingLockGuard& operator=(const FreeThreadingLockGuard&) = delete;

 private:
  FreeThreadingMutex& mutex_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_FREE_THREADING_MUTEX_H_
