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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_FUNCTION_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_FUNCTION_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/value.h"
#include "runtime/function.h"
#include "py_cel/py_cel_type.h"
#include <pybind11/pybind11.h>

namespace cel_python {

namespace py = ::pybind11;

class PyCelEnvInternal;

// Wrapper for a Python function that implements a CEL late-bound function.
// These function implementations are supplied to Activation.
class PyCelFunction {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelFunction(std::string function_name, std::vector<PyCelType> parameters,
                bool is_member, py::object impl, PyCelType return_type);

  std::string function_name() const { return function_name_; }
  const std::vector<PyCelType>& parameters() const { return parameters_; }
  bool is_member() const { return is_member_; }
  py::object impl() const { return impl_; }
  const PyCelType& return_type() const { return return_type_; }

 private:
  std::string function_name_;
  PyCelType return_type_;
  std::vector<PyCelType> parameters_;
  bool is_member_;
  py::object impl_;
};

// Internal wrapper for a Python function that implements a CEL extension
// function.
class PyCelFunctionAdapter : public cel::Function {
 public:
  PyCelFunctionAdapter(std::string function_name, PyCelType return_type,
                       py::object py_function);

  absl::StatusOr<cel::Value> Invoke(
      absl::Span<const cel::Value> args,
      const cel::Function::InvokeContext& context) const final;

 private:
  std::string function_name_;
  PyCelType return_type_;
  py::object py_function_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_FUNCTION_H_
