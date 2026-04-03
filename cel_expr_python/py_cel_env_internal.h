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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_INTERNAL_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_INTERNAL_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/function_descriptor.h"
#include "compiler/compiler.h"
#include "env/env.h"
#include "env/env_runtime.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/py_cel_env_config.h"
#include "cel_expr_python/py_cel_function.h"
#include "cel_expr_python/py_cel_function_decl.h"
#include "cel_expr_python/py_cel_type.h"
#include "cel_expr_python/py_descriptor_database.h"
#include "cel_expr_python/py_message_factory.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"

namespace cel_python {

class PyCelEnvInternal;

class CelExtensionHandle {
 public:
  explicit CelExtensionHandle(PyObject* extension);
  ~CelExtensionHandle();

  absl::StatusOr<CelExtension*> GetExtension();

 private:
  // The Python object that was passed to the constructor and is retained for
  // the duration of the CelExtensionHandle lifetime.
  PyObject* py_extension_;

  // Non-retaining pointer to the CelExtension encapsulated by `py_extension_`.
  // It must be deleted before `py_extension_`.
  CelExtension* cel_extension_;
};

// PyCelEnvInternal is a container for internal CEL components not exposed to
// the python side.
class PyCelEnvInternal {
 public:
  ~PyCelEnvInternal() = default;
  static absl::StatusOr<std::shared_ptr<PyCelEnvInternal>> NewCelEnvInternal(
      const PyCelEnvConfig& env_config, PyObject* py_descriptor_pool,
      const std::unordered_map<std::string, PyCelType>& variable_types,
      const std::vector<PyObject*>& extensions, const std::string& container,
      const std::vector<std::shared_ptr<PyCelFunctionDecl>>& functions,
      const std::unordered_map<std::string, py::object>& function_impls);

  const PyCelEnvConfig& GetEnvConfig() const { return env_config_; }

  static absl::StatusOr<const cel::Compiler*> GetCompiler(
      const std::shared_ptr<PyCelEnvInternal>& env);

  enum RuntimeMode {
    // Standard CEL runtime with warnings treated as errors.
    kStandard,
    // Standard CEL runtime with warnings ignored. Useful for CEL expressions
    // that bypass type checking.
    kStandardIgnoreWarnings,
  };

  static absl::StatusOr<const cel::Runtime*> GetRuntime(
      const std::shared_ptr<PyCelEnvInternal>& env, RuntimeMode runtime_mode);

  const google::protobuf::DescriptorPool* GetDescriptorPool() const {
    return descriptor_pool_.get();
  }

  google::protobuf::MessageFactory* GetMessageFactory() const {
    return const_cast<google::protobuf::DynamicMessageFactory*>(&message_factory_);
  }

  std::shared_ptr<PyMessageFactory> GetPyMessageFactory() const {
    return py_message_factory_;
  }

  const PyCelType& GetVariableType(const std::string& name) const;

 private:
  // Use NewCelEnvInternal() to create an instance.
  PyCelEnvInternal(const PyCelEnvConfig& env_config,
                   PyObject* py_descriptor_pool,
                   std::vector<std::unique_ptr<CelExtensionHandle>>& extensions,
                   std::unordered_map<std::string, py::object>& function_impls);

  absl::Status ConfigureStandardExtension(
      cel::CompilerBuilder& compiler_builder, const std::string& extension);

  absl::Status ConfigureStandardExtension(cel::RuntimeBuilder& runtime_builder,
                                          const std::string& extension,
                                          const cel::RuntimeOptions& opts);

  google::protobuf::Arena arena_;
  cel::Env cel_env_;
  cel::EnvRuntime cel_env_runtime_;
  PyCelEnvConfig env_config_;
  PyDescriptorDatabase py_descriptor_database_;
  std::shared_ptr<google::protobuf::DescriptorPool> descriptor_pool_;
  google::protobuf::DynamicMessageFactory message_factory_;
  std::shared_ptr<PyMessageFactory> py_message_factory_;
  // Synchronized by the GIL.
  std::unordered_map<std::string, PyCelType> variable_types_;
  std::vector<std::unique_ptr<CelExtensionHandle>> extensions_;
  std::unordered_map<std::string, py::object> function_impls_;
  std::unique_ptr<cel::Compiler> compiler_;
  absl::flat_hash_map<RuntimeMode, std::unique_ptr<const cel::Runtime>>
      runtimes_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_INTERNAL_H_
