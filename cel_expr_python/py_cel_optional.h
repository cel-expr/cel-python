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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_OPTIONAL_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_OPTIONAL_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <string>

#include <pybind11/pybind11.h>

namespace cel_python {

// Represents a CEL optional value in Python (`optional.of(v)` or
// `optional.none()`).
//
// Internally, `value_ == nullptr` represents `optional.none()`, whereas a
// non-null `value_` (including `Py_None` for `optional.of(null)`) represents a
// present value with an owned Python reference.
class PyCelOptional {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  // Creates an empty optional (`optional.none()`).
  PyCelOptional() : value_(nullptr) {}

  static PyCelOptional None() { return PyCelOptional(); }
  static PyCelOptional Of(pybind11::handle value);
  static pybind11::object ClassGetItem(pybind11::handle item);

  PyCelOptional(const PyCelOptional& other) = delete;
  PyCelOptional(PyCelOptional&& other) noexcept;
  PyCelOptional& operator=(const PyCelOptional& other) = delete;
  PyCelOptional& operator=(PyCelOptional&& other) = delete;
  ~PyCelOptional();

  bool HasValue() const { return value_ != nullptr; }
  PyObject* RawValue() const { return value_; }

  pybind11::object Value() const;
  pybind11::object OrValue(pybind11::object default_value) const;
  pybind11::object Equals(pybind11::handle other) const;
  std::string ToString() const;

 private:
  explicit PyCelOptional(PyObject* value);

  PyObject* value_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_OPTIONAL_H_
