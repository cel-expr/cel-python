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

#include "py_cel.h"
#include "py_cel_activation.h"
#include "py_cel_arena.h"
#include "py_cel_expression.h"
#include "py_cel_function.h"
#include "py_cel_function_decl.h"
#include "py_cel_overload.h"
#include "py_cel_python_extension.h"
#include "py_cel_type.h"
#include "py_cel_value.h"
#include <pybind11/pybind11.h>
#include "pybind11_abseil/import_status_module.h"

namespace cel_python {

PYBIND11_MODULE(py_cel, m) {
  pybind11::google::ImportStatusModule();
  m.doc() = "Python bindings for CEL.";

  PyCelArena::DefinePythonBindings(m);
  PyCelType::DefinePythonBindings(m);
  PyCelValue::DefinePythonBindings(m);
  PyCelActivation::DefinePythonBindings(m);
  PyCelExpression::DefinePythonBindings(m);
  PyCelOverload::DefinePythonBindings(m);
  PyCelFunctionDecl::DefinePythonBindings(m);
  PyCelPythonExtension::DefinePythonBindings(m);
  PyCelFunction::DefinePythonBindings(m);
  PyCel::DefinePythonBindings(m);
}

}  // namespace cel_python
