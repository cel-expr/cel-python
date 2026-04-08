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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_FUNCTION_DECL_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_FUNCTION_DECL_H_

#include <string>
#include <utility>
#include <vector>

#include "env/config.h"
#include "cel_expr_python/py_cel_overload.h"
#include <pybind11/pybind11.h>

namespace cel_python {

class PyCelFunctionDecl {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelFunctionDecl(std::string name, std::vector<PyCelOverload> overloads)
      : name_(std::move(name)), overloads_(std::move(overloads)) {};
  ~PyCelFunctionDecl() = default;

  std::string name() const { return name_; }
  const std::vector<PyCelOverload>& overloads() const { return overloads_; }

  cel::Config::FunctionConfig ToFunctionConfig() const;

 private:
  std::string name_;
  std::vector<PyCelOverload> overloads_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_FUNCTION_DECL_H_
