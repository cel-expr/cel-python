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

#include "py_cel/py_cel_function.h"

#include <Python.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "common/value.h"
#include "runtime/embedder_context.h"
#include "runtime/function.h"
#include "py_cel/py_cel_arena.h"
#include "py_cel/py_cel_env_internal.h"
#include "py_cel/py_cel_type.h"
#include "py_cel/py_cel_value.h"
#include "py_cel/py_error_status.h"
#include "py_cel/status_macros.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace cel_python {

namespace py = ::pybind11;

namespace {

static std::shared_ptr<PyCelEnvInternal> GetEnvFromContext(
    const cel::Function::InvokeContext& context) {
  ABSL_CHECK(context.embedder_context());  // Crash OK: all call sites are local
                                           // to the library.
  return *context.embedder_context()->Get<std::shared_ptr<PyCelEnvInternal>*>();
}

}  // namespace

void PyCelFunction::DefinePythonBindings(pybind11::module& m) {
  py::class_<PyCelFunction, std::shared_ptr<PyCelFunction>>(m, "Function")
      .def(py::init<std::string, std::vector<PyCelType>, bool, py::object,
                    PyCelType>(),
           py::arg("function_name"), py::arg("parameters"),
           py::arg("is_member"), py::arg("impl"),
           py::arg("return_type") = PyCelType::Dyn());
}

PyCelFunction::PyCelFunction(std::string function_name,
                             std::vector<PyCelType> parameters, bool is_member,
                             py::object impl, PyCelType return_type)
    : function_name_(std::move(function_name)),
      return_type_(std::move(return_type)),
      parameters_(std::move(parameters)),
      is_member_(is_member),
      impl_(std::move(impl)) {
  ABSL_CHECK(PyGILState_Check());
}

PyCelFunctionAdapter::PyCelFunctionAdapter(std::string function_name,
                                           PyCelType return_type,
                                           py::object py_function)
    : function_name_(std::move(function_name)),
      return_type_(std::move(return_type)),
      py_function_(std::move(py_function)) {}

absl::StatusOr<cel::Value> PyCelFunctionAdapter::Invoke(
    absl::Span<const cel::Value> args,
    const cel::Function::InvokeContext& context) const {
  ABSL_CHECK(PyGILState_Check());

  std::shared_ptr<PyCelEnvInternal> env = GetEnvFromContext(context);
  PY_CEL_ASSIGN_OR_RETURN(auto py_arena,
                          PyCelArena::FromProtoArena(context.arena()));
  PyObject* py_args = PyTuple_New(args.size());
  for (int i = 0; i < args.size(); ++i) {
    PyTuple_SetItem(py_args, i,
                    CelValueToPyObject(args[i], env, py_arena,
                                       /*plain_value=*/true));
  }
  PyObject* result = PyObject_CallObject(py_function_.ptr(), py_args);
  absl::Status status = PyErr_toStatus();
  if (!status.ok()) {
    return cel::ErrorValue(status);
  }

  return PyObjectToCelValue(
      result, return_type_,
      [this]() {
        return absl::StrFormat(
            "Python function '%s'",
            PyUnicode_AsUTF8(PyObject_Repr(py_function_.ptr())));
      },
      env, context.arena());
};

}  // namespace cel_python
