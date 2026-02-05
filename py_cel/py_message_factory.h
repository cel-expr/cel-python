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
#ifndef THIRD_PARTY_CEL_PYTHON_PY_MESSAGE_FACTORY_H_
#define THIRD_PARTY_CEL_PYTHON_PY_MESSAGE_FACTORY_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <string>

#include "absl/container/flat_hash_map.h"

namespace cel_python {

class PyMessageFactory {
 public:
  explicit PyMessageFactory(PyObject* descriptor_pool);
  ~PyMessageFactory();
  // Must be called with the GIL held.
  PyObject* FromString(const std::string& message_type,
                       const std::string& serialized_proto);

 private:
  PyObject* GetMessageClass(const std::string& message_type);
  PyObject* py_descriptor_pool_;
  PyObject* py_func_GetMessageClass_;  // NOLINT - Python function name.
  PyObject* py_func_MergeFromString_;  // NOLINT - Python function name.
  absl::flat_hash_map<std::string, PyObject*> message_classes_;
};

}  // namespace cel_python
#endif  // THIRD_PARTY_CEL_PYTHON_PY_MESSAGE_FACTORY_H_
