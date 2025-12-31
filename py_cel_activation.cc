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

#include "py_cel_activation.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "py_cel_env.h"
#include "py_cel_function.h"
#include "py_cel_value_provider.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel_python {

namespace py = pybind11;

void PyCelActivation::DefinePythonBindings(py::module& m) {
  py::class_<PyCelActivation, std::shared_ptr<PyCelActivation>>(m,
                                                                "Activation");
}

PyCelActivation::PyCelActivation(
    std::shared_ptr<PyCelEnv> env,
    const std::unordered_map<std::string, PyObject*>& data,
    const std::vector<std::shared_ptr<PyCelFunction>>& functions,
    const std::shared_ptr<PyCelArena>& arena)
    : env_(std::move(env)), arena_(std::move(arena)) {
  ABSL_CHECK(PyGILState_Check());
  for (const auto& [name, value] : data) {
    auto provider = std::make_unique<PyCelValueProvider>(name, value, env_);
    activation_.InsertOrAssignValueProvider(
        name,
        [p = std::move(provider)](
            absl::string_view, const google::protobuf::DescriptorPool* descriptor_pool,
            google::protobuf::MessageFactory* message_factory, google::protobuf::Arena* arena) {
          return p->Provide(descriptor_pool, message_factory, arena);
        });
  }

  for (const auto& function : functions) {
    std::vector<cel::Kind> parameters;
    for (const auto& parameter : function->parameters()) {
      parameters.push_back(parameter.GetKind());
    }
    cel::FunctionDescriptor func_descriptor(function->function_name(),
                                            function->is_member(), parameters,
                                            /*is_strict=*/true);
    activation_.InsertFunction(
        func_descriptor, std::make_unique<PyCelFunctionAdapter>(
                             env, function->function_name(), function->impl()));
  }
};

PyCelActivation::~PyCelActivation() = default;

std::shared_ptr<PyCelEnv> PyCelActivation::GetEnv() const { return env_; }

}  // namespace cel_python
