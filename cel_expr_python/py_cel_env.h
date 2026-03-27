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
#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cel_expr_python/py_cel_activation.h"
#include "cel_expr_python/py_cel_arena.h"
#include "cel_expr_python/py_cel_env_config.h"
#include "cel_expr_python/py_cel_expression.h"
#include "cel_expr_python/py_cel_function.h"
#include "cel_expr_python/py_cel_type.h"
#include <pybind11/pybind11.h>

namespace cel {
class ParserBuilder;
class TypeCheckerBuilder;
class RuntimeBuilder;
class RuntimeOptions;
}  // namespace cel

// All classes and functions in this namespace are pybind11-wrapped.
namespace cel_python {

class PyCelEnvInternal;
class PyCelFunction;
class PyMessageFactory;

// CEL environment. Provides access to the CEL compiler.
class PyCelEnv {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  ~PyCelEnv();

  // May throw exceptions.
  PyCelExpression Compile(const std::string& cel_expr,
                          bool disable_check = false);
  // May throw exceptions.
  PyCelExpression Deserialize(const std::string& serialized_expr);
  std::shared_ptr<PyCelActivation> NewActivation(
      const std::unordered_map<std::string, PyObject*>& data,
      const std::vector<std::shared_ptr<PyCelFunction>>& functions,
      const std::shared_ptr<PyCelArena>& arena);

  std::shared_ptr<PyCelEnvInternal> GetEnv() { return env_; }

 private:
  // Private constructor. Use `py_cel.NewEnv()` in python to obtain an instance.
  PyCelEnv(const PyCelEnvConfig& config, PyObject* descriptor_pool,
           std::unordered_map<std::string, PyCelType> variable_types,
           const std::vector<PyObject*>& extensions, std::string container);

  std::shared_ptr<PyCelEnvInternal> env_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_H_
