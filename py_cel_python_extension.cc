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

#include "py_cel_python_extension.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "py_cel_env_internal.h"
#include "py_cel_extension.h"
#include "py_cel_function.h"
#include "py_cel_function_decl.h"
#include "py_cel_overload.h"
#include "py_cel_type.h"
#include "status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace cel_python {

namespace py = ::pybind11;

static const cel::FunctionDescriptorOptions kFunctionDescriptorOptions = {
    .is_strict = true, .is_contextual = true};

void PyCelPythonExtension::DefinePythonBindings(py::module_& m) {
  py::class_<PyCelExtension>(m, "CelExtensionBase")
      .def(py::init<std::string>(), py::arg("name"));

  py::class_<PyCelPythonExtension, PyCelExtension>(m, "CelExtension")
      .def(py::init<std::string, std::vector<PyCelFunctionDecl>>(),
           py::arg("name"), py::arg("functions"));
}

PyCelPythonExtension::PyCelPythonExtension(
    std::string name, std::vector<PyCelFunctionDecl> functions)
    : PyCelExtension(std::move(name)), functions_(std::move(functions)) {}

absl::Status PyCelPythonExtension::ConfigureCompiler(
    cel::CompilerBuilder& compiler_builder,
    const google::protobuf::DescriptorPool& descriptor_pool) {
  google::protobuf::Arena* arena = compiler_builder.GetCheckerBuilder().arena();
  for (const PyCelFunctionDecl& function : functions_) {
    cel::FunctionDecl function_decl;
    function_decl.set_name(function.name());
    for (const PyCelOverload& overload : function.overloads()) {
      cel::OverloadDecl overload_decl;
      overload_decl.set_id(overload.overload_id());
      PY_CEL_ASSIGN_OR_RETURN(
          cel::Type cel_type,
          PyCelType::ToCelType(overload.return_type(), arena, descriptor_pool));
      overload_decl.set_result(cel_type);
      overload_decl.set_member(overload.is_member());
      auto& mutable_args = overload_decl.mutable_args();
      mutable_args.reserve(overload.parameters().size());
      for (const auto& arg : overload.parameters()) {
        PY_CEL_ASSIGN_OR_RETURN(
            cel::Type cel_type,
            PyCelType::ToCelType(arg, arena, descriptor_pool));
        mutable_args.push_back(cel_type);
      }
      PY_CEL_RETURN_IF_ERROR(
          function_decl.AddOverload(std::move(overload_decl)));
    }
    PY_CEL_RETURN_IF_ERROR(
        compiler_builder.GetCheckerBuilder().AddFunction(function_decl));
  }

  return absl::OkStatus();
}

absl::Status PyCelPythonExtension::ConfigureRuntime(
    cel::RuntimeBuilder& runtime_builder, const cel::RuntimeOptions& opts) {
  for (const PyCelFunctionDecl& function : functions_) {
    for (const PyCelOverload& overload : function.overloads()) {
      std::vector<cel::Kind> types;
      types.reserve(overload.parameters().size());
      for (const PyCelType& arg : overload.parameters()) {
        types.push_back(arg.GetKind());
      }

      cel::FunctionDescriptor descriptor(function.name(), overload.is_member(),
                                         types, kFunctionDescriptorOptions);
      if (overload.py_function()) {
        PY_CEL_RETURN_IF_ERROR(runtime_builder.function_registry().Register(
            descriptor, std::make_unique<PyCelFunctionAdapter>(
                            function.name(), overload.py_function())));
      } else {
        PY_CEL_RETURN_IF_ERROR(
            runtime_builder.function_registry().RegisterLazyFunction(
                descriptor));
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace cel_python
