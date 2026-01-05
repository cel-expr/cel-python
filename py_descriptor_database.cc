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
#define PY_SSIZE_T_CLEAN

#include "py_descriptor_database.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <cstdint>

#include "absl/log/absl_check.h"
#include "common/minimal_descriptor_pool.h"
#include "py_error_status.h"
#include "google/protobuf/descriptor.h"

namespace cel_python {

PyDescriptorDatabase::PyDescriptorDatabase(PyObject* py_descriptor_pool)
    : py_descriptor_pool_(py_descriptor_pool),
      standard_pool_(cel::GetMinimalDescriptorPool()) {
  ABSL_CHECK(PyGILState_Check());
  Py_INCREF(py_descriptor_pool_);
}

PyDescriptorDatabase::~PyDescriptorDatabase() {
  auto gil_state = PyGILState_Ensure();
  Py_DECREF(py_descriptor_pool_);
  PyGILState_Release(gil_state);
}

// Find a file by file name.  Fills in in *output and returns true if found.
// Otherwise, returns false, leaving the contents of *output undefined.
bool PyDescriptorDatabase::FindFileByName(StringViewArg filename,
                                          google::protobuf::FileDescriptorProto* output) {
  ABSL_CHECK(PyGILState_Check());
  const google::protobuf::FileDescriptor* file = standard_pool_.FindFileByName(filename);
  if (file != nullptr) {
    file->CopyTo(output);
    return true;
  }

  PyObject* pyfile = PyObject_CallMethod(
      py_descriptor_pool_, "FindFileByName", "s#", filename.data(),
      static_cast<Py_ssize_t>(filename.size()));
  if (pyfile == nullptr) {
    // Ignore KeyError raised by the method, if any
    if (PyErr_Occurred() == PyExc_KeyError) {
      PyErr_Clear();
    } else {
      PyErr_noteAndClear();
    }
    return false;
  }

  PyObject* pyfile_serialized = PyObject_GetAttrString(pyfile, "serialized_pb");
  Py_DECREF(pyfile);
  if (pyfile_serialized == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "Python file has no attribute 'serialized_pb'");
    return false;
  }

  bool ok = output->ParseFromArray(
      reinterpret_cast<uint8_t*>(PyBytes_AS_STRING(pyfile_serialized)),
      PyBytes_GET_SIZE(pyfile_serialized));
  if (!ok) {
    PyErr_Format(PyExc_RuntimeError, "Failed to parse descriptor for %s",
                 filename.data());
    PyErr_noteAndClear();
  }
  Py_DECREF(pyfile_serialized);
  return ok;
}

// Find the file that declares the given fully-qualified symbol name.
// If found, fills in *output and returns true, otherwise returns false
// and leaves *output undefined.
bool PyDescriptorDatabase::FindFileContainingSymbol(
    StringViewArg symbol_name, google::protobuf::FileDescriptorProto* output) {
  ABSL_CHECK(PyGILState_Check());
  const google::protobuf::FileDescriptor* file =
      standard_pool_.FindFileContainingSymbol(symbol_name);
  if (file != nullptr) {
    file->CopyTo(output);
    return true;
  }

  PyObject* pyfile = PyObject_CallMethod(
      py_descriptor_pool_, "FindFileContainingSymbol", "s#", symbol_name.data(),
      static_cast<Py_ssize_t>(symbol_name.size()));
  if (pyfile == nullptr) {
    // Ignore KeyError raised by the method, if any
    if (PyErr_Occurred() == PyExc_KeyError) {
      PyErr_Clear();
    } else {
      PyErr_noteAndClear();
    }
    return false;
  }

  PyObject* pyfile_serialized = PyObject_GetAttrString(pyfile, "serialized_pb");
  Py_DECREF(pyfile);
  if (pyfile_serialized == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "Python file has no attribute 'serialized_pb'");
    return false;
  }

  bool ok = output->ParseFromArray(
      reinterpret_cast<uint8_t*>(PyBytes_AS_STRING(pyfile_serialized)),
      PyBytes_GET_SIZE(pyfile_serialized));
  if (!ok) {
    PyErr_Format(PyExc_RuntimeError, "Failed to parse descriptor for %s",
                 symbol_name.data());
  }
  Py_DECREF(pyfile_serialized);
  return ok;
}

// Find the file which defines an extension extending the given message type
// with the given field number.  If found, fills in *output and returns true,
// otherwise returns false and leaves *output undefined.  containing_type
// must be a fully-qualified type name.
bool PyDescriptorDatabase::FindFileContainingExtension(
    StringViewArg containing_type, int field_number,
    google::protobuf::FileDescriptorProto* output) {
  ABSL_CHECK(PyGILState_Check());
  PyObject* py_containing_type = PyObject_CallMethod(
      py_descriptor_pool_, "FindMessageTypeByName", "s#",
      containing_type.data(), static_cast<Py_ssize_t>(containing_type.size()));
  if (py_containing_type == nullptr) {
    // Ignore KeyError raised by the method, if any
    if (PyErr_Occurred() == PyExc_KeyError) {
      PyErr_Clear();
    } else {
      PyErr_noteAndClear();
    }
    return false;
  }

  PyObject* ext_desc =
      PyObject_CallMethod(py_descriptor_pool_, "FindExtensionByNumber", "Oi",
                          py_containing_type, field_number);
  if (ext_desc == nullptr) {
    // Ignore KeyError raised by the method, if any
    if (PyErr_Occurred() == PyExc_KeyError) {
      PyErr_Clear();
    } else {
      PyErr_noteAndClear();
    }
    return false;
  }

  PyObject* pyfile = PyObject_GetAttrString(ext_desc, "file");
  Py_DECREF(ext_desc);
  if (pyfile == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "Extension descriptor has no attribute 'file'");
    PyErr_noteAndClear();
    return false;
  }

  PyObject* pyfile_serialized = PyObject_GetAttrString(pyfile, "serialized_pb");
  Py_DECREF(pyfile);
  if (pyfile_serialized == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "Python file has no attribute 'serialized_pb'");
    PyErr_noteAndClear();
    return false;
  }

  bool ok = output->ParseFromArray(
      reinterpret_cast<uint8_t*>(PyBytes_AS_STRING(pyfile_serialized)),
      PyBytes_GET_SIZE(pyfile_serialized));
  if (!ok) {
    PyErr_Format(PyExc_RuntimeError,
                 "Failed to parse descriptor for %s, extension %d",
                 containing_type.data(), field_number);
    PyErr_noteAndClear();
  }
  Py_DECREF(pyfile_serialized);
  return ok;
}

}  // namespace cel_python
