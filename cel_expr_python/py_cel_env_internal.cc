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

#include "cel_expr_python/py_cel_env_internal.h"

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
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "env/config.h"
#include "env/env.h"
#include "env/env_std_extensions.h"
#include "env/runtime_std_extensions.h"
#include "env/type_info.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/py_cel_env_config.h"
#include "cel_expr_python/py_cel_function.h"
#include "cel_expr_python/py_cel_function_decl.h"
#include "cel_expr_python/py_cel_overload.h"
#include "cel_expr_python/py_cel_python_extension.h"
#include "cel_expr_python/py_cel_type.h"
#include "cel_expr_python/py_descriptor_database.h"
#include "cel_expr_python/py_error_status.h"
#include "cel_expr_python/py_message_factory.h"
#include "cel_expr_python/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include <pybind11/pybind11.h>

namespace cel_python {

namespace {

static const cel::FunctionDescriptorOptions kFunctionDescriptorOptions = {
    .is_strict = true, .is_contextual = true};

}  // namespace

PyCelEnvInternal::PyCelEnvInternal(
    const PyCelEnvConfig& env_config, PyObject* py_descriptor_pool,
    std::vector<std::unique_ptr<CelExtensionHandle>>& extension_handles,
    std::unordered_map<std::string, py::object>& function_impls)
    : env_config_(env_config),
      py_descriptor_database_(py_descriptor_pool),
      descriptor_pool_(
          std::make_shared<google::protobuf::DescriptorPool>(&py_descriptor_database_)),
      message_factory_(descriptor_pool_.get()),
      py_message_factory_(
          std::make_shared<PyMessageFactory>(py_descriptor_pool)),
      extensions_(std::move(extension_handles)),
      function_impls_(std::move(function_impls)) {
  cel_env_.SetDescriptorPool(descriptor_pool_);
  cel_env_.SetConfig(env_config_.GetConfig());
  cel::RegisterStandardExtensions(cel_env_);

  cel_env_runtime_.SetDescriptorPool(descriptor_pool_);
  cel_env_runtime_.SetConfig(env_config_.GetConfig());
  cel::RegisterStandardExtensions(cel_env_runtime_);

  for (std::unique_ptr<CelExtensionHandle>& extension_handle : extensions_) {
    // This should never fail because we have already called GetExtension() once
    // before calling this constructor.
    CelExtension* extension = ThrowIfError(extension_handle->GetExtension());
    cel_env_.RegisterCompilerLibrary(
        extension->name(), extension->name(), 0,
        [extension]() { return extension->GetCompilerLibrary(); });
    cel_env_runtime_.RegisterExtensionFunctions(
        extension->name(), extension->name(), 0,
        [extension](
            cel::RuntimeBuilder& runtime_builder,
            const cel::RuntimeOptions& runtime_options) -> absl::Status {
          return extension->ConfigureRuntime(runtime_builder, runtime_options);
        });
  }
}

absl::StatusOr<std::shared_ptr<PyCelEnvInternal>>
PyCelEnvInternal::NewCelEnvInternal(
    const PyCelEnvConfig& env_config, PyObject* py_descriptor_pool,
    const std::unordered_map<std::string, PyCelType>& variable_types,
    const std::vector<PyObject*>& extensions, const std::string& container,
    const std::vector<std::shared_ptr<PyCelFunctionDecl>>& functions,
    const std::unordered_map<std::string, py::object>& function_impls) {
  cel::Config config = env_config.GetConfig();

  for (const auto& [name, type] : variable_types) {
    CEL_PYTHON_RETURN_IF_ERROR(
        config.AddVariableConfig(cel::Config::VariableConfig{
            .name = name,
            .type_info = PyCelType::ToTypeInfo(type),
        }));
  }

  std::vector<std::unique_ptr<CelExtensionHandle>> extension_handles;
  for (PyObject* ext : extensions) {
    auto extension_handle = std::make_unique<CelExtensionHandle>(ext);
    CEL_PYTHON_ASSIGN_OR_RETURN(CelExtension * extension,
                                extension_handle->GetExtension());

    std::string name;
    if (!extension->alias().empty() &&
        extension->alias() != extension->name()) {
      // If the configuration lists the extension by name, use the name;
      // otherwise, use the alias.
      name = extension->alias();
      for (const cel::Config::ExtensionConfig& extension_config :
           config.GetExtensionConfigs()) {
        if (extension_config.name == extension->name()) {
          name = extension_config.name;
          break;
        }
      }
    } else {
      name = extension->name();
    }
    CEL_PYTHON_RETURN_IF_ERROR(config.AddExtensionConfig(
        name, extension->version() >= 0
                  ? extension->version()
                  : cel::Config::ExtensionConfig::kLatest));
    extension_handles.push_back(std::move(extension_handle));
  }

  config.SetContainerConfig(cel::Config::ContainerConfig{.name = container});

  std::unordered_map<std::string, py::object> impls;
  for (const std::shared_ptr<PyCelFunctionDecl>& function : functions) {
    CEL_PYTHON_RETURN_IF_ERROR(
        config.AddFunctionConfig(function->ToFunctionConfig()));

    for (const PyCelOverload& overload : function->overloads()) {
      if (overload.py_function().is_none()) {
        continue;
      }
      if (!impls.insert({overload.overload_id(), overload.py_function()})
               .second) {
        return absl::AlreadyExistsError(
            absl::StrCat("An implementation for function overload id '",
                         overload.overload_id(), "' already exists."));
      }
    }
  }

  for (const auto& [overload_id, py_function] : function_impls) {
    if (!impls.insert({overload_id, py_function}).second) {
      return absl::AlreadyExistsError(
          absl::StrCat("An implementation for function overload id '",
                       overload_id, "' already exists."));
    }
  }
  return std::shared_ptr<PyCelEnvInternal>(new PyCelEnvInternal(
      PyCelEnvConfig(config), py_descriptor_pool, extension_handles, impls));
}

absl::StatusOr<const cel::Compiler*> PyCelEnvInternal::GetCompiler(
    const std::shared_ptr<PyCelEnvInternal>& env) {
  ABSL_CHECK(PyGILState_Check());

  if (env->compiler_) {
    return env->compiler_.get();
  }

  const cel::Config& config = env->env_config_.GetConfig();

  CEL_PYTHON_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      env->cel_env_.NewCompilerBuilder());

  cel::TypeCheckerBuilder& checker_builder =
      compiler_builder->GetCheckerBuilder();

  checker_builder.set_container(config.GetContainerConfig().name);

  // Convert variable types from cel::TypeInfo to PyCelType.
  google::protobuf::Arena* arena = checker_builder.arena();
  for (const cel::Config::VariableConfig& variable_config :
       config.GetVariableConfigs()) {
    CEL_PYTHON_ASSIGN_OR_RETURN(
        cel::Type cel_type,
        cel::TypeInfoToType(variable_config.type_info,
                            env->descriptor_pool_.get(), arena));
    PyCelType py_cel_type = PyCelType::FromCelType(cel_type);
    env->variable_types_[variable_config.name] = py_cel_type;
  }

  CEL_PYTHON_ASSIGN_OR_RETURN(env->compiler_, compiler_builder->Build());
  return env->compiler_.get();
}

absl::StatusOr<const cel::Runtime*> PyCelEnvInternal::GetRuntime(
    const std::shared_ptr<PyCelEnvInternal>& env, RuntimeMode runtime_mode) {
  if (auto it = env->runtimes_.find(runtime_mode); it != env->runtimes_.end()) {
    return it->second.get();
  }

  cel::RuntimeOptions& opts = env->cel_env_runtime_.mutable_runtime_options();
  opts.container = env->GetEnvConfig().GetConfig().GetContainerConfig().name;
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
  CEL_PYTHON_ASSIGN_OR_RETURN(cel::RuntimeBuilder builder,
                              env->cel_env_runtime_.CreateRuntimeBuilder());
  CEL_PYTHON_RETURN_IF_ERROR(cel::EnableReferenceResolver(
      builder, cel::ReferenceResolverEnabled::kAlways));

  for (const cel::Config::FunctionConfig& function_config :
       env->GetEnvConfig().GetConfig().GetFunctionConfigs()) {
    for (const cel::Config::FunctionOverloadConfig& overload_config :
         function_config.overload_configs) {
      auto it = env->function_impls_.find(overload_config.overload_id);
      if (it == env->function_impls_.end()) {
        continue;
      }
      py::object py_function = it->second;
      std::vector<cel::Kind> param_kinds;
      param_kinds.reserve(overload_config.parameters.size());
      for (const cel::Config::TypeInfo& parameter :
           overload_config.parameters) {
        CEL_PYTHON_ASSIGN_OR_RETURN(
            cel::Type type,
            cel::TypeInfoToType(parameter, env->descriptor_pool_.get(),
                                &env->arena_));
        param_kinds.push_back(static_cast<cel::Kind>(type.kind()));
      }
      cel::FunctionDescriptor descriptor(
          function_config.name, overload_config.is_member_function, param_kinds,
          kFunctionDescriptorOptions);
      CEL_PYTHON_ASSIGN_OR_RETURN(
          cel::Type return_type,
          cel::TypeInfoToType(overload_config.return_type,
                              env->descriptor_pool_.get(), &env->arena_));
      CEL_PYTHON_RETURN_IF_ERROR(builder.function_registry().Register(
          descriptor, std::make_unique<PyCelFunctionAdapter>(
                          function_config.name,
                          PyCelType::FromCelType(return_type), py_function)));
    }
  }
  CEL_PYTHON_ASSIGN_OR_RETURN(std::unique_ptr<cel::Runtime> runtime,
                              std::move(builder).Build());
  const cel::Runtime* runtime_ptr = runtime.get();
  env->runtimes_[runtime_mode] = std::move(runtime);
  return runtime_ptr;
}

const PyCelType& PyCelEnvInternal::GetVariableType(
    const std::string& name) const {
  ABSL_CHECK(PyGILState_Check());
  auto it = variable_types_.find(name);
  if (it != variable_types_.end()) {
    return it->second;
  }
  return PyCelType::Dyn();
}

CelExtensionHandle::CelExtensionHandle(PyObject* extension)
    : py_extension_(extension), cel_extension_(nullptr) {
  ABSL_CHECK(PyGILState_Check());
  Py_INCREF(py_extension_);
}

CelExtensionHandle::~CelExtensionHandle() {
  auto gil_state = PyGILState_Ensure();
  Py_DECREF(py_extension_);
  PyGILState_Release(gil_state);
}

absl::StatusOr<CelExtension*> CelExtensionHandle::GetExtension() {
  if (cel_extension_) {
    return cel_extension_;
  }

  if (Py_IsNone(py_extension_)) {
    return absl::InvalidArgumentError("Provided extension is None");
  }

  // First, check if the object is a CelExtension (extension implemented in
  // Python)
  absl::Status status_py_cel_extension;
  try {
    pybind11::handle handle = pybind11::handle(py_extension_);
    return handle.cast<PyCelPythonExtension*>();
  } catch (const pybind11::cast_error& e) {
    status_py_cel_extension = absl::InvalidArgumentError(e.what());
  }

  // If that fails, check if the object is a pybind11 wrapper for
  // CelExtension.
  absl::Status status_cc_cel_extension;
  try {
    pybind11::handle handle = pybind11::handle(py_extension_);
    return handle.cast<CelExtension*>();
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
