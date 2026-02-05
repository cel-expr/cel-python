// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_OVERLOAD_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_OVERLOAD_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <string>
#include <vector>

#include "py_cel/py_cel_type.h"
#include <pybind11/pybind11.h>

namespace cel_python {

namespace py = ::pybind11;

class PyCelOverload {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelOverload(const PyCelOverload&) = default;
  PyCelOverload& operator=(const PyCelOverload&) = default;

  PyCelOverload(std::string overload_id, const PyCelType& return_type,
                std::vector<PyCelType> parameters, bool is_member = false,
                py::object py_function = py::none());

  std::string overload_id() const { return overload_id_; }
  PyCelType return_type() const { return return_type_; }
  const std::vector<PyCelType>& parameters() const { return parameters_; }
  bool is_member() const { return is_member_; }
  py::object py_function() const { return py_function_; }

 private:
  std::string overload_id_;
  PyCelType return_type_;
  std::vector<PyCelType> parameters_;
  bool is_member_;
  py::object py_function_ = py::none();
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_OVERLOAD_H_
