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
#include "py_error_status.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"  // IWYU pragma: keep - Needed for ABSL_LOG
#include "absl/status/status.h"

#undef LOG_ALL_PY_ERRORS

static absl::Status PyErrorToStatus(PyObject* py_type, PyObject* py_error) {
  // Loose mapping from Python exceptions to absl::Status codes, consistent with
  // the pybind11 mapping.
  static const absl::NoDestructor<
      absl::flat_hash_map<PyObject*, absl::StatusCode>>
      kPyExceptionToStatusMap(
          {{PyExc_MemoryError, absl::StatusCode::kResourceExhausted},
           {PyExc_NotImplementedError, absl::StatusCode::kUnimplemented},
           {PyExc_KeyboardInterrupt, absl::StatusCode::kAborted},
           {PyExc_SystemError, absl::StatusCode::kInternal},
           {PyExc_SyntaxError, absl::StatusCode::kInternal},
           {PyExc_TypeError, absl::StatusCode::kInvalidArgument},
           {PyExc_ValueError, absl::StatusCode::kOutOfRange},
           {PyExc_LookupError, absl::StatusCode::kNotFound}});
  PyObject* pystr = PyObject_Str(py_error);
  const char* message = pystr ? PyUnicode_AsUTF8(pystr) : "Unknown error";
  Py_XDECREF(pystr);

  auto it = kPyExceptionToStatusMap->find(py_type);
  if (it == kPyExceptionToStatusMap->end()) {
    return absl::UnknownError(message);
  }
  return absl::Status(it->second, message);
}

static absl::Status& PendingPyError() {
  static absl::NoDestructor<absl::Status> pending_py_error(absl::OkStatus());
  return *pending_py_error;
}

absl::Status PyErr_toStatus() {
  PyObject* py_error = PyErr_Occurred();
  if (!py_error) {
    absl::Status status = PendingPyError();
    if (!status.ok()) {
      PendingPyError() = absl::OkStatus();
      return status;
    }
    return absl::OkStatus();
  }

  absl::Status status;
  PyObject *ptype, *pvalue, *ptraceback;

  // Fetch the exception details
  PyErr_Fetch(&ptype, &pvalue, &ptraceback);
  PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

  // Get the string representation of the error message
  if (pvalue) {
    status = PyErrorToStatus(ptype, pvalue);
  }

  // Clean up all the fetched objects
  Py_XDECREF(ptype);
  Py_XDECREF(pvalue);
  Py_XDECREF(ptraceback);
  PendingPyError() = absl::OkStatus();
  return status;
}

void PyErr_noteAndClear() {
  if (!PyErr_Occurred()) {
    return;
  }

  // Fetch the exception details
  PyObject *ptype, *pvalue, *ptraceback;
  PyErr_Fetch(&ptype, &pvalue, &ptraceback);
  PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
  PendingPyError() = PyErrorToStatus(ptype, pvalue);
  PyErr_Clear();

  // Clean up all the fetched objects
  Py_XDECREF(ptype);
  Py_XDECREF(pvalue);
  Py_XDECREF(ptraceback);

#ifdef LOG_ALL_PY_ERRORS
  ABSL_LOG(INFO) << "PyErr_Occurred: " << PendingPyError();
#endif
}
