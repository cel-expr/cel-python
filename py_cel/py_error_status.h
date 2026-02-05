/*
 * Copyright 2025 Google LLC
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
#ifndef THIRD_PARTY_CEL_PYTHON_PY_ERROR_STATUS_H_
#define THIRD_PARTY_CEL_PYTHON_PY_ERROR_STATUS_H_

#include <stdexcept>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "py_cel/status_macros.h"
#include <pybind11/pybind11.h>

#define CEL_PYTHON_ASSIGN_OR_RETURN(...)    \
  PY_CEL_RETURN_IF_ERROR(PyErr_toStatus()); \
  PY_CEL_ASSIGN_OR_RETURN(__VA_ARGS__);     \
  PY_CEL_RETURN_IF_ERROR(PyErr_toStatus());

namespace cel_python {

std::runtime_error StatusToException(const absl::Status& status);

template <typename T>
T ThrowIfError(absl::StatusOr<T> status_or) {
  if (!status_or.ok()) {
    throw cel_python::StatusToException(status_or.status());
  }
  return std::move(*status_or);
}

absl::Status PyErr_toStatus();

// Checks if there is a pending Python exception, and if so, stores it for a
// later `PyErr_toStatus` call and clears the exception in order for the
// runtime to be able to continue.
void PyErr_noteAndClear();

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_ERROR_STATUS_H_
