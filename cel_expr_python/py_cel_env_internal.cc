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
#include "checker/type_checker_builder_factory.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "env/config.h"
#include "env/env.h"
#include "env/env_std_extensions.h"
#include "env/runtime_std_extensions.h"
#include "env/type_info.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "parser/parser_interface.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "validator/validator.h"
#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/py_cel_env_config.h"
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

// A temporary adapter for cel::CompilerBuilder.  It will be removed once the
// CelExtension class is changed to return a cel::CompilerLibrary directly.
//
// This adapter allows `CelExtension::ConfigureCompiler`, which takes a
// `cel::CompilerBuilder` to be used with the `cel::Env` API, which
// is expressed in terms of the `cel::CompilerLibrary` framework.  The adapter
// splits the `cel::CompilerLibrary` into its constituent parts and feeds them
// to the `cel::CompilerBuilder` interface.
class CompilerBuilderAdapter : public cel::CompilerBuilder {
 public:
  CompilerBuilderAdapter(cel::ParserBuilder* parser_builder,
                         cel::TypeCheckerBuilder* checker_builder)
      : parser_builder_(parser_builder), checker_builder_(checker_builder) {}

  absl::Status AddLibrary(cel::CompilerLibrary library) override {
    if (library.configure_parser != nullptr) {
      CEL_PYTHON_RETURN_IF_ERROR(library.configure_parser(*parser_builder_));
    }
    if (library.configure_checker != nullptr) {
      CEL_PYTHON_RETURN_IF_ERROR(library.configure_checker(*checker_builder_));
    }
    return absl::OkStatus();
  }

  absl::Status AddLibrarySubset(cel::CompilerLibrarySubset subset) override {
    return absl::UnimplementedError("Not implemented");
  }

  cel::ParserBuilder& GetParserBuilder() override {
    ABSL_CHECK(parser_builder_ != nullptr);
    return *parser_builder_;
  }

  cel::TypeCheckerBuilder& GetCheckerBuilder() override {
    ABSL_CHECK(checker_builder_ != nullptr);
    return *checker_builder_;
  }

  cel::Validator& GetValidator() override { return validator_; }

  absl::StatusOr<std::unique_ptr<cel::Compiler>> Build() override {
    return absl::UnimplementedError("Not implemented");
  }

 private:
  cel::ParserBuilder* parser_builder_ = nullptr;
  cel::TypeCheckerBuilder* checker_builder_ = nullptr;
  cel::Validator validator_;
};

// A temporary CompilerLibrary that deals with the interface mismatch between
// CelExtension and cel::CompilerLibrary.
cel::CompilerLibrary MakeCompilerLibrary(
    CelExtension* extension,
    const std::shared_ptr<google::protobuf::DescriptorPool>& descriptor_pool,
    cel::ParserBuilder* passive_parser_builder,
    cel::TypeCheckerBuilder* passive_checker_builder) {
  return cel::CompilerLibrary(
      extension->name(),
      // Safe to capture passive_checker_builder because it outlives the
      // compiler library.
      [extension, descriptor_pool, passive_checker_builder](
          cel::ParserBuilder& parser_builder) -> absl::Status {
        CompilerBuilderAdapter compiler_builder(&parser_builder,
                                                passive_checker_builder);
        return extension->ConfigureCompiler(compiler_builder, *descriptor_pool);
      },
      // Safe to capture passive_parser_builder because it outlives the
      // compiler library.
      [extension, descriptor_pool, passive_parser_builder](
          cel::TypeCheckerBuilder& checker_builder) -> absl::Status {
        CompilerBuilderAdapter compiler_builder(passive_parser_builder,
                                                &checker_builder);
        return extension->ConfigureCompiler(compiler_builder, *descriptor_pool);
      });
}
}  // namespace

PyCelEnvInternal::PyCelEnvInternal(
    const PyCelEnvConfig& env_config, PyObject* py_descriptor_pool,
    std::vector<CelExtensionHandle> extension_handles,
    const std::string& container)
    : env_config_(env_config),
      py_descriptor_database_(py_descriptor_pool),
      descriptor_pool_(
          std::make_shared<google::protobuf::DescriptorPool>(&py_descriptor_database_)),
      message_factory_(descriptor_pool_.get()),
      py_message_factory_(
          std::make_shared<PyMessageFactory>(py_descriptor_pool)),
      extensions_(std::move(extension_handles)),
      container_(std::move(container)) {
  cel_env_.SetDescriptorPool(descriptor_pool_);
  cel_env_.SetConfig(env_config_.GetConfig());
  cel::RegisterStandardExtensions(cel_env_);

  cel_env_runtime_.SetDescriptorPool(descriptor_pool_);
  cel_env_runtime_.SetConfig(env_config_.GetConfig());
  cel::RegisterStandardExtensions(cel_env_runtime_);

  passive_parser_builder_ = cel::NewParserBuilder(cel::ParserOptions());
  passive_checker_builder_ =
      ThrowIfError(cel::CreateTypeCheckerBuilder(descriptor_pool_.get()));
  for (CelExtensionHandle& extension_handle : extensions_) {
    // This should never fail because we have already called GetExtension() once
    // before calling this constructor.
    CelExtension* extension = ThrowIfError(extension_handle.GetExtension());
    cel_env_.RegisterCompilerLibrary(
        extension->name(), extension->name(), 0, [this, extension]() {
          return MakeCompilerLibrary(extension, descriptor_pool_,
                                     passive_parser_builder_.get(),
                                     passive_checker_builder_.get());
        });
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
    const std::vector<PyObject*>& extensions, const std::string& container) {
  cel::Config config = env_config.GetConfig();

  for (const auto& [name, type] : variable_types) {
    CEL_PYTHON_RETURN_IF_ERROR(
        config.AddVariableConfig(cel::Config::VariableConfig{
            .name = name,
            .type_info = PyCelType::ToTypeInfo(type),
        }));
  }

  std::vector<CelExtensionHandle> extension_handles;
  extension_handles.reserve(extensions.size());
  for (PyObject* ext : extensions) {
    CelExtensionHandle handle(ext);
    CEL_PYTHON_ASSIGN_OR_RETURN(CelExtension * extension,
                                handle.GetExtension());
    // TODO(b/498655870): support extension version.
    CEL_PYTHON_RETURN_IF_ERROR(config.AddExtensionConfig(extension->name()));
    extension_handles.push_back(std::move(handle));
  }

  return std::shared_ptr<PyCelEnvInternal>(
      new PyCelEnvInternal(PyCelEnvConfig(config), py_descriptor_pool,
                           std::move(extension_handles), container));
}

absl::StatusOr<const cel::Compiler*> PyCelEnvInternal::GetCompiler(
    const std::shared_ptr<PyCelEnvInternal>& env) {
  ABSL_CHECK(PyGILState_Check());

  if (env->compiler_) {
    return env->compiler_.get();
  }

  CEL_PYTHON_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      env->cel_env_.NewCompilerBuilder());
  compiler_builder->GetCheckerBuilder().set_container(env->container_);

  // Convert variable types from cel::TypeInfo to PyCelType.
  google::protobuf::Arena* arena = compiler_builder->GetCheckerBuilder().arena();
  for (const cel::Config::VariableConfig& variable_config :
       env->env_config_.GetConfig().GetVariableConfigs()) {
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
  CEL_PYTHON_ASSIGN_OR_RETURN(cel::RuntimeBuilder builder,
                              env->cel_env_runtime_.CreateRuntimeBuilder());
  CEL_PYTHON_RETURN_IF_ERROR(cel::EnableReferenceResolver(
      builder, cel::ReferenceResolverEnabled::kAlways));
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

CelExtensionHandle::CelExtensionHandle(CelExtensionHandle&& other)
    : py_extension_(other.py_extension_), cel_extension_(other.cel_extension_) {
  other.py_extension_ = nullptr;
  other.cel_extension_ = nullptr;
}

CelExtensionHandle::~CelExtensionHandle() {
  if (py_extension_ != nullptr) {
    auto gil_state = PyGILState_Ensure();
    Py_DECREF(py_extension_);
    PyGILState_Release(gil_state);
  }
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
