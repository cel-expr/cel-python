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

#include "cel_expr_python/py_cel_value.h"

#include <Python.h>    // IWYU pragma: keep - Needed for PyObject
#include <datetime.h>  // IWYU pragma: keep

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/call_once.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/kind.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "cel_expr_python/py_cel_arena.h"
#include "cel_expr_python/py_cel_env_internal.h"
#include "cel_expr_python/py_cel_type.h"
#include "cel_expr_python/py_cel_value_provider.h"
#include "cel_expr_python/py_error_status.h"
#include "cel_expr_python/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "pybind11_abseil/absl_casters.h"

namespace cel_python {

namespace py = ::pybind11;

void PyCelValue::DefinePythonBindings(py::module& m) {
  py::class_<PyCelValue>(m, "Value")
      .def("type", &PyCelValue::Type)
      .def("value",
           [](PyCelValue& self) {
             return py::reinterpret_borrow<py::object>(self.Value());
           })
      .def("plain_value",
           [](PyCelValue& self) {
             return py::reinterpret_borrow<py::object>(self.PlainValue());
           })
      .def("__repr__", &PyCelValue::ToString);

  py::class_<PyCelListItemAccessor, PyCelValue>(m, "_CelListItemAccessor");
  py::class_<PyCelMapItemAccessor, PyCelValue>(m, "_CelMapItemAccessor");
}

// Should be called with the GIL held.
PyCelValue::PyCelValue(cel::Value& cel_value, std::shared_ptr<PyCelArena> arena,
                       std::shared_ptr<PyCelEnvInternal> env)
    : cel_value_(std::move(cel_value)),
      object_(nullptr),
      plain_object_(nullptr),
      arena_(std::move(arena)),
      env_(std::move(env)) {}

PyCelValue::~PyCelValue() {
  if (object_ || plain_object_) {
    auto gil_state = PyGILState_Ensure();
    Py_XDECREF(object_);
    Py_XDECREF(plain_object_);
    PyGILState_Release(gil_state);
  }
}

PyCelType PyCelValue::Type() { return PyCelType::ForCelValue(cel_value_); }

PyObject* PyCelValue::Value() {
  ABSL_CHECK(PyGILState_Check());
  if (object_) {
    return object_;
  }
  object_ = CelValueToPyObject(cel_value_, env_, arena_,
                               /*plain_value=*/false);
  if (object_ == nullptr) {
    PyErr_SetString(PyExc_AssertionError, "Cannot create object");
  }

  return object_;
}

PyObject* PyCelValue::PlainValue() {
  ABSL_CHECK(PyGILState_Check());
  if (plain_object_) {
    return plain_object_;
  }
  plain_object_ = CelValueToPyObject(cel_value_, env_, arena_,
                                     /*plain_value=*/true);
  if (plain_object_ == nullptr) {
    PyErr_SetString(PyExc_AssertionError, "Cannot create object");
  }

  return plain_object_;
}

std::string PyCelValue::ToString() { return cel_value_.DebugString(); }

PyCelValueProvider::PyCelValueProvider(std::string name, PyObject* value,
                                       std::shared_ptr<PyCelEnvInternal> env)
    : name_(std::move(name)), py_object_(value), env_(std::move(env)) {
  Py_INCREF(py_object_);
}

PyCelValueProvider::~PyCelValueProvider() {
  auto gil_state = PyGILState_Ensure();
  Py_DECREF(py_object_);
  PyGILState_Release(gil_state);
}

cel::Value PyCelValueProvider::Provide(
    const google::protobuf::DescriptorPool* descriptor_pool,
    google::protobuf::MessageFactory* message_factory, google::protobuf::Arena* arena) const {
  ABSL_CHECK(PyGILState_Check());
  const PyCelType& type = env_->GetVariableType(name_);
  absl::StatusOr<cel::Value> converted_value = PyObjectToCelValue(
      py_object_, type, [this]() { return name_; }, env_, arena);
  if (!converted_value.ok()) {
    return cel::ErrorValue(converted_value.status());
  }
  return *converted_value;
}

void PyCelListItemAccessor::ResolveElement() {
  if (resolved_) {
    return;
  }
  cel::ListValue list = cel_value_.GetList();
  absl::StatusOr<cel::Value> element =
      list.Get(index_, env_->GetDescriptorPool(), env_->GetMessageFactory(),
               arena_->GetArena());
  if (!element.ok()) {
    element_value_ = cel::ErrorValue(element.status());
    resolved_ = true;
    return;
  }
  element_value_ = element.value();
  resolved_ = true;
}

PyCelType PyCelListItemAccessor::Type() {
  auto gil_state = PyGILState_Ensure();
  ResolveElement();
  PyGILState_Release(gil_state);
  return PyCelType::ForCelValue(element_value_);
}

PyObject* PyCelListItemAccessor::Value() {
  ABSL_CHECK(PyGILState_Check());
  if (object_) {
    return object_;
  }

  ResolveElement();

  object_ = CelValueToPyObject(element_value_, env_, arena_,
                               /*plain_value=*/false);
  return object_;
}

PyObject* PyCelListItemAccessor::PlainValue() {
  ABSL_CHECK(PyGILState_Check());
  if (plain_object_) {
    return plain_object_;
  }

  ResolveElement();

  plain_object_ =
      CelValueToPyObject(element_value_, env_, arena_, /*plain_value=*/true);
  return plain_object_;
}

std::string PyCelListItemAccessor::ToString() {
  auto gil_state = PyGILState_Ensure();
  ResolveElement();
  std::string result = element_value_.DebugString();
  PyGILState_Release(gil_state);
  return result;
}

void PyCelMapItemAccessor::ResolveElement() {
  if (resolved_) {
    return;
  }
  cel::MapValue map = cel_value_.GetMap();
  absl::Status status =
      map.Get(key_, env_->GetDescriptorPool(), env_->GetMessageFactory(),
              arena_->GetArena(), &element_value_);
  if (!status.ok()) {
    element_value_ = cel::ErrorValue(status);
    resolved_ = true;
    return;
  }
  resolved_ = true;
}

PyCelType PyCelMapItemAccessor::Type() {
  auto gil_state = PyGILState_Ensure();
  ResolveElement();
  PyGILState_Release(gil_state);
  return PyCelType::ForCelValue(element_value_);
}

PyObject* PyCelMapItemAccessor::Value() {
  ABSL_CHECK(PyGILState_Check());
  if (object_) {
    return object_;
  }

  ResolveElement();

  object_ = CelValueToPyObject(element_value_, env_, arena_,
                               /*plain_value=*/false);
  return object_;
}

PyObject* PyCelMapItemAccessor::PlainValue() {
  ABSL_CHECK(PyGILState_Check());
  if (plain_object_) {
    return plain_object_;
  }

  ResolveElement();

  plain_object_ =
      CelValueToPyObject(element_value_, env_, arena_, /*plain_value=*/true);
  return plain_object_;
}

std::string PyCelMapItemAccessor::ToString() {
  auto gil_state = PyGILState_Ensure();
  ResolveElement();
  std::string result = element_value_.DebugString();
  PyGILState_Release(gil_state);
  return result;
}

// This should be called with the GIL held.
PyObject* CelValueToPyObject(const cel::Value& cel_value,
                             const std::shared_ptr<PyCelEnvInternal>& env,
                             const std::shared_ptr<PyCelArena>& arena,
                             bool plain_value) {
  switch (cel_value.kind()) {
    case cel::ValueKind::kNull: {
      return Py_None;
    }
    case cel::ValueKind::kBool: {
      PyObject* py_bool = cel_value.AsBool().value() ? Py_True : Py_False;
      Py_INCREF(py_bool);
      return py_bool;
    }
    case cel::ValueKind::kInt: {
      int64_t value = cel_value.AsInt().value();
      return PyLong_FromLongLong(value);
    }
    case cel::ValueKind::kUint: {
      uint64_t value = cel_value.AsUint().value();
      return PyLong_FromUnsignedLongLong(value);
    }
    case cel::ValueKind::kDouble: {
      double value = cel_value.AsDouble().value();
      return PyFloat_FromDouble(value);
    }
    case cel::ValueKind::kString: {
      std::string value = cel_value.AsString().value().ToString();
      return PyUnicode_FromStringAndSize(value.data(), value.size());
    }
    case cel::ValueKind::kBytes: {
      std::string value = cel_value.AsBytes().value().ToString();
      return PyByteArray_FromStringAndSize(value.data(), value.size());
    }
    case cel::ValueKind::kMessage: {
      cel::MessageValue value = cel_value.AsMessage().value();
      std::string serialized_data;
      if (!value.GetParsed()->SerializePartialToString(&serialized_data)) {
        PyErr_Format(PyExc_RuntimeError, "Cannot serialize message");
        return nullptr;
      }

      return env->GetPyMessageFactory()->FromString(
          std::string(value.GetTypeName()), serialized_data);
    }
    case cel::ValueKind::kList: {
      cel::ListValue list = *cel_value.AsList();
      Py_ssize_t size = *list.Size();
      PyObject* py_list = PyList_New(size);
      absl::Status status = absl::OkStatus();
      for (int i = 0; i < size; ++i) {
        PyObject* py_item;
        if (plain_value) {
          absl::StatusOr<cel::Value> item =
              list.Get(i, env->GetDescriptorPool(), env->GetMessageFactory(),
                       arena->GetArena());
          if (!item.ok()) {
            status = item.status();
            break;
          }
          py_item = CelValueToPyObject(*item, env, arena,
                                       /*plain_value=*/true);
        } else {
          py_item = py::cast(PyCelListItemAccessor(cel_value, i, arena, env))
                        .release()
                        .ptr();
        }
        // PyList_SetItem is special in that it "steals" the reference to the
        // item being set, so we must not decref it.
        // https://docs.python.org/3/c-api/list.html#:~:text=int-,pylist_setitem
        PyList_SetItem(py_list, i, py_item);
      }
      if (!status.ok()) {
        Py_DECREF(py_list);
        return CelValueToPyObject(cel::ErrorValue(status), env, arena,
                                  /*plain_value=*/plain_value);
      }
      return py_list;
    }
    case cel::ValueKind::kMap: {
      cel::MapValue map = *cel_value.AsMap();
      cel::ListValue key_list;
      absl::Status status =
          map.ListKeys(env->GetDescriptorPool(), env->GetMessageFactory(),
                       arena->GetArena(), &key_list);
      if (!status.ok()) {
        PyErr_Format(PyExc_RuntimeError, "Cannot list keys: %s",
                     status.ToString().c_str());
        return nullptr;
      }

      PyObject* py_map = PyDict_New();
      Py_ssize_t size = *key_list.Size();
      for (int i = 0; i < size; ++i) {
        absl::StatusOr<cel::Value> key =
            key_list.Get(i, env->GetDescriptorPool(), env->GetMessageFactory(),
                         arena->GetArena());
        if (!key.ok()) {
          PyErr_Format(PyExc_RuntimeError, "Cannot get key: %s",
                       key.status().ToString().c_str());
          return nullptr;
        }

        PyObject* py_key = CelValueToPyObject(*key, env, arena,
                                              /*plain_value=*/true);
        PyObject* py_value;
        if (plain_value) {
          cel::Value element_value;
          absl::Status status =
              map.Get(*key, env->GetDescriptorPool(), env->GetMessageFactory(),
                      arena->GetArena(), &element_value);
          if (!status.ok()) {
            PyErr_Format(PyExc_RuntimeError, "Cannot get value: %s",
                         status.ToString().c_str());
            Py_DECREF(py_key);
            return nullptr;
          }
          py_value = CelValueToPyObject(element_value, env, arena,
                                        /*plain_value=*/true);
        } else {
          py_value = py::cast(PyCelMapItemAccessor(cel_value, *key, arena, env))
                         .release()
                         .ptr();
        }
        // In contrast to PyList_SetItem, PyDict_SetItem does not "steal" the
        // reference to the key and value being set, so we must decref them.
        // https://docs.python.org/3/c-api/dict.html#:~:text=int-,pydict_setitem
        PyDict_SetItem(py_map, py_key, py_value);
        Py_DECREF(py_key);
        Py_DECREF(py_value);
      }
      return py_map;
    }
    case cel::ValueKind::kTimestamp: {
      absl::optional<cel::TimestampValue> timestamp = cel_value.AsTimestamp();
      if (!timestamp.has_value()) {
        PyErr_Format(PyExc_RuntimeError, "Cannot convert to timestamp");
        return nullptr;
      }
      return py::cast(timestamp->ToTime()).release().ptr();
    }
    case cel::ValueKind::kDuration: {
      absl::optional<cel::DurationValue> duration = cel_value.AsDuration();
      if (!duration.has_value()) {
        PyErr_Format(PyExc_RuntimeError, "Cannot convert to duration");
        return nullptr;
      }
      return py::cast(duration->ToDuration()).release().ptr();
    }
    case cel::ValueKind::kError: {
      std::string value = cel_value.AsError()->ToStatus().ToString();
      return PyUnicode_FromStringAndSize(value.data(), value.size());
    }
    case cel::ValueKind::kType: {
      const cel::Type& type = cel_value.AsType()->type();
      return py::cast(PyCelType::FromCelType(type)).release().ptr();
    }
    default: {
      PyErr_Format(PyExc_RuntimeError, "Unimplemented type: %d",
                   (int)cel_value.kind());
      return nullptr;
    }
  }
}

static absl::string_view NormalizeTypeName(absl::string_view type_name) {
  // 32-bit wrappers are equivalent to 64-bit.
  if (type_name == "google.protobuf.Int32Value") {
    return cel::IntWrapperType::kName;
  } else if (type_name == "google.protobuf.UInt32Value") {
    return cel::UintWrapperType::kName;
  } else if (type_name == "google.protobuf.FloatValue") {
    return cel::DoubleWrapperType::kName;
  }
  return type_name;
}

static cel::ErrorValue InvalidTypeError(
    PyObject* py_object, absl::FunctionRef<std::string()> context,
    const PyCelType& expected_type) {
  PyTypeObject* type = Py_TYPE(py_object);
  return cel::ErrorValue(absl::InvalidArgumentError(
      absl::StrFormat("Unexpected value type for '%s': %s. (Expected %s)",
                      context(), type->tp_name, expected_type.GetName())));
}

static void EnsureDateTimeModuleImported() {
  static absl::once_flag import_py_datetime_once_flag;
  absl::call_once(import_py_datetime_once_flag, []() { PyDateTime_IMPORT; });
}

absl::StatusOr<cel::Value> PyObjectToCelValue(
    PyObject* py_object, const PyCelType& expected_type,
    absl::FunctionRef<std::string()> context,
    const std::shared_ptr<PyCelEnvInternal>& env, google::protobuf::Arena* arena,
    bool bypass_type_check) {
  if (!py_object) {
    return cel::ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("Unexpected None value for '%s'. (Expected %s)",
                        context(), expected_type.GetName())));
  }

  switch (expected_type.GetKind()) {
    case cel::Kind::kDyn: {
      CEL_PYTHON_ASSIGN_OR_RETURN(const PyCelType& type,
                                  PyCelType::ForPyObject(py_object, context));
      return PyObjectToCelValue(py_object, type, context, env, arena,
                                /*bypass_type_check=*/true);
    }
    case cel::Kind::kNull: {
      return cel::NullValue();
    }
    case cel::Kind::kBool: {
      if (py_object == Py_True) {
        return cel::BoolValue(true);
      } else if (py_object == Py_False) {
        return cel::BoolValue(false);
      } else {
        return InvalidTypeError(py_object, context, expected_type);
      }
    }
    case cel::Kind::kInt: {
      if (bypass_type_check || PyNumber_Check(py_object)) {
        int64_t value = PyLong_AsLong(py_object);
        if (PyErr_Occurred()) {
          return cel::ErrorValue(PyErr_toStatus());
        }
        return cel::IntValue(value);
      } else {
        return InvalidTypeError(py_object, context, expected_type);
      }
    }
    case cel::Kind::kUint: {
      if (bypass_type_check || PyNumber_Check(py_object)) {
        uint64_t value = PyLong_AsUnsignedLong(py_object);
        if (PyErr_Occurred()) {
          return cel::ErrorValue(PyErr_toStatus());
        }
        return cel::UintValue(value);
      } else {
        return InvalidTypeError(py_object, context, expected_type);
      }
    }
    case cel::Kind::kDouble: {
      if (bypass_type_check || PyNumber_Check(py_object)) {
        double value = PyFloat_AsDouble(py_object);
        if (PyErr_Occurred()) {
          return cel::ErrorValue(PyErr_toStatus());
        }
        return cel::DoubleValue(value);
      } else {
        return InvalidTypeError(py_object, context, expected_type);
      }
    }
    case cel::Kind::kString: {
      if (PyUnicode_Check(py_object)) {
        std::string value = PyUnicode_AsUTF8(py_object);
        return cel::StringValue::From(value, arena);
      } else if (PyBytes_Check(py_object)) {
        return cel::StringValue::From(PyBytes_AsString(py_object), arena);
      } else {
        return InvalidTypeError(py_object, context, expected_type);
      }
    }
    case cel::Kind::kBytes: {
      if (PyByteArray_Check(py_object)) {
        Py_ssize_t size = PyByteArray_Size(py_object);
        char* bytes = PyByteArray_AsString(py_object);
        return cel::BytesValue::From(std::string(bytes, size), arena);
      } else if (PyBytes_Check(py_object)) {
        return cel::BytesValue::From(PyBytes_AsString(py_object), arena);
      }
      return InvalidTypeError(py_object, context, expected_type);
    }
    case cel::Kind::kTimestamp: {
      EnsureDateTimeModuleImported();
      if (PyDateTime_Check(py_object)) {
        absl::Time time;
        try {
          time = py::cast<absl::Time>(py_object);
        } catch (const py::cast_error& e) {
          PyErr_Clear();
          return absl::UnknownError(absl::StrFormat(
              "Cannot convert %s to absl::Time: %s",
              PyUnicode_AsUTF8(PyObject_Repr(py_object)), e.what()));
        }
        absl::StatusOr<cel::TimestampValue> timestamp_value =
            cel::SafeTimestampValue(time);
        if (!timestamp_value.ok()) {
          return cel::ErrorValue(timestamp_value.status());
        }
        return *timestamp_value;
      } else if (std::string_view(Py_TYPE(py_object)->tp_name) == "Timestamp") {
        PyObject* py_seconds = nullptr;
        PyObject* py_nanos = nullptr;
        int64_t seconds;
        int32_t nanos;
        bool ok = true;
        py_seconds = PyObject_GetAttrString(py_object, "seconds");
        if (PyErr_Occurred() || !PyLong_Check(py_seconds)) {
          ok = false;
        } else {
          seconds = PyLong_AsLongLong(py_seconds);
          if (PyErr_Occurred()) {
            ok = false;
          } else {
            py_nanos = PyObject_GetAttrString(py_object, "nanos");
            if (PyErr_Occurred() || !PyLong_Check(py_nanos)) {
              ok = false;
            } else {
              nanos = PyLong_AsLongLong(py_nanos);
              if (PyErr_Occurred()) {
                ok = false;
              }
            }
          }
        }
        Py_XDECREF(py_seconds);
        Py_XDECREF(py_nanos);
        if (!ok) {
          PyErr_Clear();
          return absl::UnknownError(
              absl::StrFormat("Cannot convert %s to cel.Type.TIMESTAMP",
                              PyUnicode_AsUTF8(PyObject_Repr(py_object))));
        }
        absl::Time time =
            absl::FromUnixSeconds(seconds) + absl::Nanoseconds(nanos);
        absl::StatusOr<cel::TimestampValue> timestamp_value =
            cel::SafeTimestampValue(time);
        if (!timestamp_value.ok()) {
          return cel::ErrorValue(timestamp_value.status());
        }
        return *timestamp_value;
      } else {
        return InvalidTypeError(py_object, context, expected_type);
      }
    }
    case cel::Kind::kDuration: {
      EnsureDateTimeModuleImported();
      if (PyDelta_Check(py_object)) {
        absl::Duration duration;
        try {
          duration = py::cast<absl::Duration>(py_object);
        } catch (const py::cast_error& e) {
          PyErr_Clear();
          return absl::UnknownError(absl::StrFormat(
              "Cannot convert %s to absl::Duration: %s",
              PyUnicode_AsUTF8(PyObject_Repr(py_object)), e.what()));
        }
        absl::StatusOr<cel::DurationValue> duration_value =
            cel::SafeDurationValue(duration);
        if (!duration_value.ok()) {
          return cel::ErrorValue(duration_value.status());
        }
        return *duration_value;
      } else if (std::string_view(Py_TYPE(py_object)->tp_name) == "Duration") {
        PyObject* py_seconds = nullptr;
        PyObject* py_nanos = nullptr;
        int64_t seconds;
        int32_t nanos;
        bool ok = true;
        py_seconds = PyObject_GetAttrString(py_object, "seconds");
        if (PyErr_Occurred() || !PyLong_Check(py_seconds)) {
          ok = false;
        } else {
          seconds = PyLong_AsLongLong(py_seconds);
          if (PyErr_Occurred()) {
            ok = false;
          } else {
            py_nanos = PyObject_GetAttrString(py_object, "nanos");
            if (PyErr_Occurred() || !PyLong_Check(py_nanos)) {
              ok = false;
            } else {
              nanos = PyLong_AsLongLong(py_nanos);
              if (PyErr_Occurred()) {
                ok = false;
              }
            }
          }
        }
        Py_XDECREF(py_seconds);
        Py_XDECREF(py_nanos);
        if (!ok) {
          PyErr_Clear();
          return absl::UnknownError(
              absl::StrFormat("Cannot convert %s to absl::Duration",
                              PyUnicode_AsUTF8(PyObject_Repr(py_object))));
        }
        absl::Duration duration =
            absl::Seconds(seconds) + absl::Nanoseconds(nanos);
        absl::StatusOr<cel::DurationValue> duration_value =
            cel::SafeDurationValue(duration);
        if (!duration_value.ok()) {
          return cel::ErrorValue(duration_value.status());
        }
        return *duration_value;
      } else {
        return InvalidTypeError(py_object, context, expected_type);
      }
    }
    case cel::Kind::kMessage: {
      CEL_PYTHON_ASSIGN_OR_RETURN(const PyCelType& type,
                                  PyCelType::ForPyObject(py_object, context));
      if (!bypass_type_check && type.GetName() != expected_type.GetName() &&
          NormalizeTypeName(type.GetName()) !=
              NormalizeTypeName(expected_type.GetName())) {
        return InvalidTypeError(py_object, context, expected_type);
      }

      PyObject* serialized_bytes =
          PyObject_CallMethod(py_object, "SerializePartialToString", "");
      if (!serialized_bytes) {
        PyErr_Clear();
        return cel::ErrorValue(PyErr_toStatus());
      }

      if (!PyBytes_Check(serialized_bytes)) {
        Py_DECREF(serialized_bytes);
        // Expected SerializePartialToString to return bytes.
        return InvalidTypeError(py_object, context, expected_type);
      }

      const std::string& message_type_name = type.ToString();
      const google::protobuf::Descriptor* descriptor =
          env->GetDescriptorPool()->FindMessageTypeByName(message_type_name);
      if (descriptor == nullptr) {
        return cel::ErrorValue(absl::InvalidArgumentError(absl::StrFormat(
            "Descriptor not found for message type '%s'", message_type_name)));
      }

      const google::protobuf::Message* prototype =
          env->GetMessageFactory()->GetPrototype(descriptor);
      if (prototype == nullptr) {
        return cel::ErrorValue(absl::InvalidArgumentError(absl::StrFormat(
            "Prototype not found for message type '%s'", message_type_name)));
      }
      google::protobuf::Message* message = prototype->New(arena);
      if (message == nullptr) {
        return cel::ErrorValue(absl::InternalError(absl::StrFormat(
            "Failed to create new message of type '%s'", message_type_name)));
      }

      // Create a CodedInputStream to read the serialized bytes directly from
      // the Python bytes object, without copying.
      google::protobuf::io::CodedInputStream coded_input_stream(
          reinterpret_cast<uint8_t*>(PyBytes_AS_STRING(serialized_bytes)),
          PyBytes_GET_SIZE(serialized_bytes));
      if (!message->MergePartialFromCodedStream(&coded_input_stream)) {
        Py_DECREF(serialized_bytes);
        return cel::ErrorValue(absl::InvalidArgumentError(
            absl::StrFormat("Failed to parse serialized data for type '%s' ",
                            message_type_name)));
      }
      Py_DECREF(serialized_bytes);
      // Protobuf parsing may have run into a Python exception in
      // PyDescriptorDatabase, which makes Python native calls.
      absl::Status status = PyErr_toStatus();
      if (!status.ok()) {
        return cel::ErrorValue(status);
      }
      return cel::Value::WrapMessage(message, env->GetDescriptorPool(),
                                     env->GetMessageFactory(), arena);
    }
    case cel::Kind::kList: {
      if (PyList_Check(py_object)) {
        const PyCelType& element_type = expected_type.GetParam(0);
        Py_ssize_t size = PyList_Size(py_object);
        auto builder = cel::NewListValueBuilder(arena);
        for (int i = 0; i < size; ++i) {
          PyObject* item = PyList_GetItem(py_object, i);
          // Note: PyList_GetItem returns a borrowed reference, so we
          // shouldn't DECREF it.
          CEL_PYTHON_ASSIGN_OR_RETURN(cel::Value converted_value,
                                      PyObjectToCelValue(
                                          item, element_type,
                                          [context, i]() {
                                            return absl::StrFormat(
                                                "%s[%d]", context(), i);
                                          },
                                          env, arena));
          CEL_PYTHON_RETURN_IF_ERROR(builder->Add(converted_value));
        }
        return std::move(*builder).Build();
      } else {
        CEL_PYTHON_ASSIGN_OR_RETURN(const PyCelType& object_type,
                                    PyCelType::ForPyObject(py_object, context));
        if (object_type.IsMessage() &&
            object_type.GetName() == "google.protobuf.ListValue") {
          return PyObjectToCelValue(py_object, object_type, context, env, arena,
                                    /*bypass_type_check=*/true);
        }
      }
      return InvalidTypeError(py_object, context, expected_type);
    }
    case cel::Kind::kMap: {
      if (PyDict_Check(py_object)) {
        const PyCelType& key_type = expected_type.GetParam(0);
        const PyCelType& value_type = expected_type.GetParam(1);
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        auto builder = cel::NewMapValueBuilder(arena);
        while (PyDict_Next(py_object, &pos, &key, &value)) {
          // Note: PyDict_Next returns borrowed references, so we shouldn't
          // DECREF them.
          CEL_PYTHON_ASSIGN_OR_RETURN(
              cel::Value converted_key,
              PyObjectToCelValue(
                  key, key_type,
                  [context, key]() {
                    return absl::StrFormat(
                        "%s[%s]", context(),
                        PyUnicode_AsUTF8(PyObject_Repr(key)));
                  },
                  env, arena));

          CEL_PYTHON_ASSIGN_OR_RETURN(
              cel::Value converted_value,
              PyObjectToCelValue(
                  value, value_type,
                  [context, key, value]() {
                    return absl::StrFormat(
                        "%s[%s] -> %s", context(),
                        PyUnicode_AsUTF8(PyObject_Repr(key)),
                        PyUnicode_AsUTF8(PyObject_Repr(value)));
                  },
                  env, arena));
          CEL_PYTHON_RETURN_IF_ERROR(
              builder->Put(converted_key, converted_value));
        }
        return std::move(*builder).Build();
      } else {
        CEL_PYTHON_ASSIGN_OR_RETURN(const PyCelType& object_type,
                                    PyCelType::ForPyObject(py_object, context));
        if (object_type.IsMessage() &&
            object_type.GetName() == "google.protobuf.Struct") {
          return PyObjectToCelValue(py_object, object_type, context, env, arena,
                                    /*bypass_type_check=*/true);
        }
      }
      return InvalidTypeError(py_object, context, expected_type);
    }
    default: {
      return absl::UnimplementedError(
          absl::StrFormat("Unimplemented type: %d", expected_type.GetKind()));
    }
  }
}

}  // namespace cel_python
