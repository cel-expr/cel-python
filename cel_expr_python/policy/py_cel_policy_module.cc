// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/policy/py_cel_policy.h"
#include "cel_expr_python/policy/py_cel_policy_yaml_parser.h"
#include <pybind11/pybind11.h>

namespace cel_python {

PYBIND11_MODULE(cel_policy, m, pybind11::mod_gil_not_used()) {
  pybind11::module_::import(CEL_MODULE_NAME);
  m.doc() = "Python bindings for CEL Policy.";

  DefinePolicyBindings(m);
  DefineCelPolicyYamlParserBindings(m);
}

}  // namespace cel_python
