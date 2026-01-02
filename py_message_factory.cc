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
#include "py_message_factory.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <string>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"

namespace cel_python {

PyMessageFactory::PyMessageFactory(PyObject* descriptor_pool) {
  py_descriptor_pool_ = descriptor_pool;
  Py_INCREF(py_descriptor_pool_);
  PyObject* pName =
      PyUnicode_DecodeFSDefault("google.protobuf.message_factory");
  PyObject* pModule = PyImport_Import(pName);
  Py_DECREF(pName);
  if (!pModule) {
    ABSL_LOG(FATAL) << "Cannot load module 'google.protobuf.message_factory'";
  }
  py_func_GetMessageClass_ = PyObject_GetAttrString(pModule, "GetMessageClass");
  if (!py_func_GetMessageClass_) {
    ABSL_LOG(FATAL) << "Cannot find function "
                       "'google.protobuf.message_factory.GetMessageClass'";
  }
  Py_INCREF(py_func_GetMessageClass_);
  py_func_MergeFromString_ = PyUnicode_FromString("MergeFromString");
}

PyMessageFactory::~PyMessageFactory() {
  auto gil_state = PyGILState_Ensure();
  Py_DECREF(py_descriptor_pool_);
  Py_XDECREF(py_func_GetMessageClass_);
  Py_XDECREF(py_func_MergeFromString_);
  for (auto const& [key, py_obj] : message_classes_) {
    Py_XDECREF(py_obj);
  }
  PyGILState_Release(gil_state);
}

PyObject* PyMessageFactory::GetMessageClass(const std::string& message_type) {
  auto it = message_classes_.find(message_type);
  if (it != message_classes_.end()) {
    return it->second;
  } else {
    PyObject* descriptor =
        PyObject_CallMethod(py_descriptor_pool_, "FindMessageTypeByName", "s",
                            message_type.c_str());
    if (!descriptor) {
      PyErr_Format(PyExc_TypeError, "Message type not found: %s",
                   message_type.c_str());
      return nullptr;
    }
    PyObject* message_class =
        PyObject_CallFunction(py_func_GetMessageClass_, "O", descriptor);
    Py_DECREF(descriptor);

    if (!message_class) {
      PyErr_Format(PyExc_TypeError, "Couldn't find message class for type: %s",
                   message_type.c_str());
      return nullptr;
    }

    message_classes_[message_type] = message_class;
    return message_class;
  }
}

PyObject* PyMessageFactory::FromString(const std::string& message_type,
                                       const std::string& serialized_proto) {
  ABSL_CHECK(PyGILState_Check());
  PyObject* message_class = GetMessageClass(message_type);
  if (!message_class) {
    return nullptr;
  }

  PyObject* message = PyObject_CallObject(message_class, nullptr);
  if (!message) {
    PyErr_Format(PyExc_RuntimeError, "Cannot create message of type: %s",
                 message_type.c_str());
    return nullptr;
  }

  PyObject* serialized_proto_py =
      PyMemoryView_FromMemory(const_cast<char*>(serialized_proto.data()),
                              serialized_proto.size(), PyBUF_READ);
  if (!serialized_proto_py) {
    Py_DECREF(message);
    return nullptr;
  }

  PyObject* ret = PyObject_CallMethodObjArgs(message, py_func_MergeFromString_,
                                             serialized_proto_py, nullptr);
  if (!ret) {
    Py_DECREF(message);
    Py_DECREF(serialized_proto_py);
    return nullptr;
  }
  Py_DECREF(serialized_proto_py);
  return message;
}

}  // namespace cel_python
