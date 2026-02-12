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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_PYTHON_EXTENSION_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_PYTHON_EXTENSION_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "compiler/compiler.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/py_cel_function_decl.h"
#include "google/protobuf/descriptor.h"
#include <pybind11/pybind11.h>

namespace cel_python {

class PyCelPythonExtension : public CelExtension {
 public:
  static void DefinePythonBindings(pybind11::module& m);
  PyCelPythonExtension(std::string name,
                       std::vector<PyCelFunctionDecl> functions);

 protected:
  absl::Status ConfigureCompiler(
      cel::CompilerBuilder& compiler_builder,
      const google::protobuf::DescriptorPool& descriptor_pool) override;

  absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override;

 private:
  std::vector<PyCelFunctionDecl> functions_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_PYTHON_EXTENSION_H_
