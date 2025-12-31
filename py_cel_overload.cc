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

#include "py_cel_overload.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "py_cel_type.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "pybind11_abseil/status_casters.h"

namespace cel_python {

namespace py = pybind11;

void PyCelOverload::DefinePythonBindings(py::module_& m) {
  py::class_<PyCelOverload, std::shared_ptr<PyCelOverload>>(m, "Overload")
      .def(py::init([](const std::string& overload_id,
                       const PyCelType& return_type,
                       const std::vector<PyCelType>& parameters, bool is_member,
                       PyObject* impl) {
             return PyCelOverload(overload_id, return_type, parameters,
                                  is_member, impl != Py_None ? impl : nullptr);
           }),
           py::arg("overload_id"), py::arg("return_type"),
           py::arg("parameters"), py::arg("is_member") = false,
           py::arg("impl") = nullptr);
}

PyCelOverload::PyCelOverload(std::string overload_id,
                             const PyCelType& return_type,
                             std::vector<PyCelType> parameters, bool is_member,
                             PyObject* py_function)
    : overload_id_(std::move(overload_id)),
      return_type_(return_type),
      parameters_(std::move(parameters)),
      is_member_(is_member),
      py_function_(py_function) {
  Py_XINCREF(py_function_);
}

PyCelOverload::PyCelOverload(const PyCelOverload& other) {
  overload_id_ = other.overload_id_;
  return_type_ = other.return_type_;
  parameters_ = other.parameters_;
  is_member_ = other.is_member_;
  py_function_ = other.py_function_;
  Py_XINCREF(py_function_);
};

PyCelOverload& PyCelOverload::operator=(const PyCelOverload& other) {
  overload_id_ = other.overload_id_;
  return_type_ = other.return_type_;
  parameters_ = other.parameters_;
  is_member_ = other.is_member_;
  py_function_ = other.py_function_;
  Py_XINCREF(py_function_);
  return *this;
}

PyCelOverload::~PyCelOverload() {
  auto gil_state = PyGILState_Ensure();
  Py_XDECREF(py_function_);
  PyGILState_Release(gil_state);
}

}  // namespace cel_python
