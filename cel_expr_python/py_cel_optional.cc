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

#include "cel_expr_python/py_cel_optional.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <string>

#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include <pybind11/pybind11.h>

namespace cel_python {

namespace py = ::pybind11;

void PyCelOptional::DefinePythonBindings(py::module& m) {
  py::class_<PyCelOptional>(m, "Optional")
      .def(py::init<>())
      .def_static(
          "of", [](py::object value) { return PyCelOptional::Of(value); },
          py::arg("value"))
      .def_static("none", &PyCelOptional::None)
      .def("has_value", &PyCelOptional::HasValue)
      .def("value", &PyCelOptional::Value)
      .def("or_value", &PyCelOptional::OrValue, py::arg("default"))
      .def("__bool__", &PyCelOptional::HasValue)
      .def("__eq__", &PyCelOptional::Equals, py::is_operator())
      .def("__repr__", &PyCelOptional::ToString)
      .def_static("__class_getitem__", &PyCelOptional::ClassGetItem,
                  py::arg("item"));
}

PyCelOptional::PyCelOptional(PyObject* value) : value_(value) {
  if (value_ != nullptr) {
    ABSL_CHECK(PyGILState_Check());
    Py_INCREF(value_);
  }
}

PyCelOptional::PyCelOptional(PyCelOptional&& other) noexcept
    : value_(other.value_) {
  other.value_ = nullptr;
}

PyCelOptional::~PyCelOptional() {
  if (value_ != nullptr) {
    if (!PyGILState_Check()) {
      py::gil_scoped_acquire acquire;
      Py_DECREF(value_);
    } else {
      Py_DECREF(value_);
    }
  }
}

py::object PyCelOptional::Equals(py::handle other) const {
  ABSL_CHECK(PyGILState_Check());
  if (!py::isinstance<PyCelOptional>(other)) {
    return py::reinterpret_borrow<py::object>(Py_NotImplemented);
  }
  const PyCelOptional& other_opt = other.cast<const PyCelOptional&>();
  if (value_ == nullptr && other_opt.value_ == nullptr) {
    return py::bool_(true);
  }
  if (value_ == nullptr || other_opt.value_ == nullptr) {
    return py::bool_(false);
  }
  PyObject* res = PyObject_RichCompare(value_, other_opt.value_, Py_EQ);
  if (res == nullptr) {
    throw py::error_already_set();
  }
  return py::reinterpret_steal<py::object>(res);
}

PyCelOptional PyCelOptional::Of(py::handle value) {
  ABSL_CHECK(value.ptr() != nullptr);
  return PyCelOptional(value.ptr());
}

// Enables runtime subscripting (e.g. `cel.Optional[int]`) for type
// annotations on Python < 3.14 and in runtime type-hint inspection.
py::object PyCelOptional::ClassGetItem(py::handle item) {
  ABSL_CHECK(PyGILState_Check());
  PyObject* alias =
      Py_GenericAlias(py::type::of<PyCelOptional>().ptr(), item.ptr());
  if (alias == nullptr) {
    throw py::error_already_set();
  }
  return py::reinterpret_steal<py::object>(alias);
}

py::object PyCelOptional::Value() const {
  ABSL_CHECK(PyGILState_Check());
  if (value_ == nullptr) {
    throw py::value_error("optional.none() dereference");
  }
  return py::reinterpret_borrow<py::object>(value_);
}

py::object PyCelOptional::OrValue(py::object default_value) const {
  ABSL_CHECK(PyGILState_Check());
  if (value_ == nullptr) {
    return default_value;
  }
  return py::reinterpret_borrow<py::object>(value_);
}

std::string PyCelOptional::ToString() const {
  ABSL_CHECK(PyGILState_Check());
  if (value_ == nullptr) {
    return "optional.none()";
  }
  py::str repr_str = py::repr(py::reinterpret_borrow<py::object>(value_));
  return absl::StrCat("optional.of(", static_cast<std::string>(repr_str), ")");
}

}  // namespace cel_python
