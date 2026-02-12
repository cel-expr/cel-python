/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_VALUE_PROVIDER_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_VALUE_PROVIDER_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>

#include "common/value.h"
#include "cel_expr_python/py_cel_env_internal.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel_python {

// A CelValueProducer that produces a CelValue from a Python object on demand.
// Since CelActivation only invokes a CelValueProducer when a variable is used,
// this allows us to delay the conversion until evaluation time, and avoid doing
// the conversion if the variable is not used.
class PyCelValueProvider {
 public:
  PyCelValueProvider(std::string name, PyObject* value,
                     std::shared_ptr<PyCelEnvInternal> env);
  ~PyCelValueProvider();

  cel::Value Provide(const google::protobuf::DescriptorPool* descriptor_pool,
                     google::protobuf::MessageFactory* message_factory,
                     google::protobuf::Arena* arena) const;

 private:
  std::string name_;
  PyObject* py_object_;
  std::shared_ptr<PyCelEnvInternal> env_;

  cel::ErrorValue InvalidTypeError() const;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_VALUE_PROVIDER_H_
