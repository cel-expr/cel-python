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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_ACTIVATION_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_ACTIVATION_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/activation.h"
#include "py_cel_arena.h"
#include <pybind11/pybind11.h>

namespace cel_python {

class PyCelEnv;
class PyCelFunction;

// Wraps a CelActivation. Supports evaluation of CEL expressions.
class PyCelActivation {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelActivation(std::shared_ptr<PyCelEnv> env,
                  const std::unordered_map<std::string, PyObject*>& data,
                  const std::vector<std::shared_ptr<PyCelFunction>>& functions,
                  const std::shared_ptr<PyCelArena>& arena);
  ~PyCelActivation();

  std::shared_ptr<PyCelEnv> GetEnv() const;
  std::shared_ptr<PyCelArena> GetArena() const { return arena_; }
  const cel::Activation* GetActivation() const { return &activation_; }

 private:
  std::shared_ptr<PyCelEnv> env_;
  std::shared_ptr<PyCelArena> arena_;
  cel::Activation activation_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_ACTIVATION_H_
