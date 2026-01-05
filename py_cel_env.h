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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "compiler/compiler.h"
#include "runtime/runtime.h"
#include "py_cel.h"
#include "py_cel_extension.h"
#include "py_cel_type.h"
#include "py_descriptor_database.h"
#include "py_message_factory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"

namespace cel_python {

class PyCelExtensionHandle {
 public:
  explicit PyCelExtensionHandle(PyObject* extension);
  ~PyCelExtensionHandle();

  absl::StatusOr<PyCelExtension*> GetExtension(
      const std::shared_ptr<PyCelEnv>& env);

 private:
  // The Python object that was passed to the constructor and is retained for
  // the duration of the CelExtensionHandle lifetime.
  PyObject* py_extension_;

  // Non-retaining pointer to the CelExtension encapsulated by `py_extension_`.
  // It must be deleted before `py_extension_`.
  PyCelExtension* cel_extension_;
};

// PyCelEnv is a container for internal CEL components not exposed to the python
// side.
class PyCelEnv {
 public:
  PyCelEnv(PyObject* descriptor_pool,
           std::unordered_map<std::string, PyCelType> variableTypes,
           const std::vector<PyObject*>& extensions, std::string container);
  ~PyCelEnv() = default;

  static absl::StatusOr<const cel::Compiler*> GetCompiler(
      const std::shared_ptr<PyCelEnv>& env);

  enum RuntimeMode {
    // Standard CEL runtime with warnings treated as errors.
    kStandard,
    // Standard CEL runtime with warnings ignored. Useful for CEL expressions
    // that bypass type checking.
    kStandardIgnoreWarnings,
  };

  static absl::StatusOr<const cel::Runtime*> GetRuntime(
      const std::shared_ptr<PyCelEnv>& env, RuntimeMode runtime_mode);

  const google::protobuf::DescriptorPool* GetDescriptorPool() const {
    return &descriptor_pool_;
  }

  google::protobuf::MessageFactory* GetMessageFactory() const {
    return const_cast<google::protobuf::DynamicMessageFactory*>(&message_factory_);
  }

  std::shared_ptr<PyMessageFactory> GetPyMessageFactory() const {
    return py_message_factory_;
  }

  const PyCelType& GetVariableType(const std::string& name) const;

 private:
  PyDescriptorDatabase py_descriptor_database_;
  google::protobuf::DescriptorPool descriptor_pool_;
  google::protobuf::DynamicMessageFactory message_factory_;
  std::shared_ptr<PyMessageFactory> py_message_factory_;
  // Synchronized by the GIL.
  std::unordered_map<std::string, PyCelType> variable_types_;
  std::vector<std::unique_ptr<PyCelExtensionHandle>> extensions_;
  std::string container_;
  std::unique_ptr<cel::Compiler> compiler_;
  absl::flat_hash_map<RuntimeMode, std::unique_ptr<const cel::Runtime>>
      runtimes_;

  absl::Status ConfigureStandardExtension(
      cel::CompilerBuilder& compiler_builder, const std::string& extension);

  absl::Status ConfigureStandardExtension(cel::RuntimeBuilder& runtime_builder,
                                          const std::string& extension,
                                          const cel::RuntimeOptions& opts);
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_H_
