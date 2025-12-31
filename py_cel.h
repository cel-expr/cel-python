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
#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/statusor.h"
#include "py_cel_activation.h"
#include "py_cel_arena.h"
#include "py_cel_expression.h"
#include "py_cel_function.h"
#include "py_cel_type.h"
#include <pybind11/pybind11.h>

namespace cel {
class ParserBuilder;
class TypeCheckerBuilder;
class RuntimeBuilder;
class RuntimeOptions;
}  // namespace cel

// All classes and functions in this namespace are pybind11-wrapped.
namespace cel_python {

class PyCel;
class PyCelEnv;
class PyCelFunction;
class PyMessageFactory;

// CEL environment. Provides access to the CEL compiler.
class PyCel {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  explicit PyCel(PyObject* descriptor_pool = nullptr,
                 std::unordered_map<std::string, PyCelType> variable_types = {},
                 const std::vector<PyObject*>& extensions = {},
                 std::string container = "");
  ~PyCel();

  absl::StatusOr<std::unique_ptr<PyCelExpression>> Compile(
      const std::string& cel_expr, bool disable_check = false);
  absl::StatusOr<std::unique_ptr<PyCelExpression>> Deserialize(
      const std::string& serialized_expr);
  std::shared_ptr<PyCelActivation> NewActivation(
      const std::unordered_map<std::string, PyObject*>& data,
      const std::vector<std::shared_ptr<PyCelFunction>>& functions,
      const std::shared_ptr<PyCelArena>& arena);

  std::shared_ptr<PyCelEnv> GetEnv() { return env_; }

 private:
  std::shared_ptr<PyCelEnv> env_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_H_
