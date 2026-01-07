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

#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/value.h"
#include "runtime/function.h"
#include "py_cel_type.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include <pybind11/pybind11.h>

namespace cel_python {

class PyCelEnv;

// Wrapper for a Python function that implements a CEL late-bound function.
// These function implementations are supplied to Activation.
class PyCelFunction {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelFunction(std::string function_name, std::vector<PyCelType> parameters,
                bool is_member, PyObject* impl);
  ~PyCelFunction();

  std::string function_name() const { return function_name_; }
  const std::vector<PyCelType>& parameters() const { return parameters_; }
  bool is_member() const { return is_member_; }
  PyObject* impl() const { return impl_; }

 private:
  std::string function_name_;
  std::vector<PyCelType> parameters_;
  bool is_member_;
  PyObject* impl_;
};

// Internal wrapper for a Python function that implements a CEL extension
// function.
class PyCelFunctionAdapter : public cel::Function {
 public:
  PyCelFunctionAdapter(std::string function_name, PyObject* py_function);
  ~PyCelFunctionAdapter() override;

  absl::StatusOr<cel::Value> Invoke(
      absl::Span<const cel::Value> args,
      const cel::Function::InvokeContext& context) const final;

 private:
  std::string function_name_;
  PyObject* py_function_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_FUNCTION_H_
