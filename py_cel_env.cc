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

#include "py_cel_env.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "py_cel_extension.h"
#include "py_cel_python_extension.h"
#include "py_cel_type.h"
#include "py_descriptor_database.h"
#include "py_message_factory.h"
#include "status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include <pybind11/pybind11.h>

namespace cel_python {

PyCelEnv::PyCelEnv(PyObject* descriptor_pool,
                   std::unordered_map<std::string, PyCelType> variableTypes,
                   const std::vector<PyObject*>& extensions,
                   std::string container)
    : py_descriptor_database_(descriptor_pool),
      descriptor_pool_(&py_descriptor_database_),
      message_factory_(&descriptor_pool_),
      py_message_factory_(std::make_shared<PyMessageFactory>(descriptor_pool)),
      variable_types_(std::move(variableTypes)),
      container_(std::move(container)) {
  for (PyObject* ext : extensions) {
    extensions_.push_back(std::make_unique<PyCelExtensionHandle>(ext));
  }
}

// TODO(b/462745713): change the parameter to a const reference once we no
// longer need to pass in a `shared_ptr<PyCelEnv>` to extensions.
absl::StatusOr<const cel::Compiler*> PyCelEnv::GetCompiler(
    const std::shared_ptr<PyCelEnv>& env) {
  if (env->compiler_) {
    return env->compiler_.get();
  }

  cel::CompilerOptions compiler_options;
  compiler_options.parser_options.enable_quoted_identifiers = true;
  PY_CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(&env->descriptor_pool_, compiler_options));
  compiler_builder->GetCheckerBuilder().set_container(env->container_);
  PY_CEL_RETURN_IF_ERROR(
      compiler_builder->AddLibrary(cel::StandardCompilerLibrary()));
  for (std::unique_ptr<PyCelExtensionHandle>& extension_handle :
       env->extensions_) {
    PY_CEL_ASSIGN_OR_RETURN(PyCelExtension * extension,
                            extension_handle->GetExtension(env));
    PY_CEL_RETURN_IF_ERROR(
        extension->ConfigureCompiler(*compiler_builder, env->descriptor_pool_));
  }
  google::protobuf::Arena* arena = compiler_builder->GetCheckerBuilder().arena();
  for (const auto& [name, type] : env->variable_types_) {
    PY_CEL_ASSIGN_OR_RETURN(
        cel::Type cel_type,
        PyCelType::ToCelType(type, arena, env->descriptor_pool_));
    cel::VariableDecl var;
    var.set_name(name);
    var.set_type(cel_type);
    PY_CEL_RETURN_IF_ERROR(
        compiler_builder->GetCheckerBuilder().AddVariable(var));
  }
  PY_CEL_ASSIGN_OR_RETURN(env->compiler_, compiler_builder->Build());
  return env->compiler_.get();
}

absl::StatusOr<const cel::Runtime*> PyCelEnv::GetRuntime(
    const std::shared_ptr<PyCelEnv>& env, RuntimeMode runtime_mode) {
  if (env->runtimes_.contains(runtime_mode)) {
    return env->runtimes_[runtime_mode].get();
  }

  cel::RuntimeOptions opts;
  opts.container = env->container_;
  opts.enable_empty_wrapper_null_unboxing = true;
  opts.enable_qualified_type_identifiers = true;
  opts.enable_timestamp_duration_overflow_errors = true;
  switch (runtime_mode) {
    case kStandard:
      break;
    case kStandardIgnoreWarnings:
      opts.fail_on_warnings = false;
      break;
  }
  PY_CEL_ASSIGN_OR_RETURN(
      cel::RuntimeBuilder builder,
      cel::CreateStandardRuntimeBuilder(&env->descriptor_pool_, opts));
  PY_CEL_RETURN_IF_ERROR(cel::EnableReferenceResolver(
      builder, cel::ReferenceResolverEnabled::kAlways));
  for (std::unique_ptr<PyCelExtensionHandle>& extension_handle :
       env->extensions_) {
    PY_CEL_ASSIGN_OR_RETURN(PyCelExtension * extension,
                            extension_handle->GetExtension(env));
    PY_CEL_RETURN_IF_ERROR(extension->ConfigureRuntime(builder, opts));
  }
  PY_CEL_ASSIGN_OR_RETURN(env->runtimes_[runtime_mode],
                          std::move(builder).Build());
  return env->runtimes_[runtime_mode].get();
}

const PyCelType& PyCelEnv::GetVariableType(const std::string& name) const {
  ABSL_CHECK(PyGILState_Check());
  auto it = variable_types_.find(name);
  if (it != variable_types_.end()) {
    return it->second;
  }
  return PyCelType::Dyn();
}

PyCelExtensionHandle::PyCelExtensionHandle(PyObject* extension)
    : py_extension_(extension), cel_extension_(nullptr) {
  ABSL_CHECK(PyGILState_Check());
  Py_INCREF(py_extension_);
}

PyCelExtensionHandle::~PyCelExtensionHandle() {
  auto gil_state = PyGILState_Ensure();
  Py_DECREF(py_extension_);
  PyGILState_Release(gil_state);
}

absl::StatusOr<PyCelExtension*> PyCelExtensionHandle::GetExtension(
    const std::shared_ptr<PyCelEnv>& env) {
  if (cel_extension_) {
    return cel_extension_;
  }

  if (Py_IsNone(py_extension_)) {
    return absl::InvalidArgumentError("Provided extension is None");
  }

  // First, check if the object is a PyCelExtension (extension implemented in
  // Python)
  absl::Status status_py_cel_extension;
  try {
    pybind11::handle handle = pybind11::handle(py_extension_);
    PyCelPythonExtension* py_cel_extension =
        handle.cast<cel_python::PyCelPythonExtension*>();
    status_py_cel_extension = py_cel_extension->SetEnv(env);
    return py_cel_extension;
  } catch (const pybind11::cast_error& e) {
    status_py_cel_extension = absl::InvalidArgumentError(e.what());
  }

  // If that fails, check if the object is a pybind11 wrapper for
  // PyCelExtension.
  absl::Status status_cc_cel_extension;
  try {
    pybind11::handle handle = pybind11::handle(py_extension_);
    return handle.cast<cel_python::PyCelExtension*>();
  } catch (const pybind11::cast_error& e) {
    status_cc_cel_extension = absl::InvalidArgumentError(e.what());
  }

  PyTypeObject* py_type = Py_TYPE(py_extension_);
  return absl::InternalError(absl::StrCat(
      "Failed to cast ", py_type ? py_type->tp_name : "unknown",
      " either as a Python CelExtension instance (",
      status_py_cel_extension.ToString(), ") or as a pybind11 wrapper (",
      status_cc_cel_extension.ToString(), ")"));
}

}  // namespace cel_python
