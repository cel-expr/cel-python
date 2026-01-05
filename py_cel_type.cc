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

#include "py_cel_type.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "common/kind.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/types/list_type.h"
#include "common/types/map_type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "py_error_status.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "pybind11_abseil/status_casters.h"

namespace cel_python {

namespace py = ::pybind11;

void PyCelType::DefinePythonBindings(py::module& m) {
  py::class_<PyCelType> type(m, "Type");
  type.def(py::init<const std::string&>(), py::arg("name"))
      .def("name", &PyCelType::GetName)
      .def("is_message", &PyCelType::IsMessage)
      .def("is_assignable_from", &PyCelType::IsAssignableFrom)
      .def("__repr__", &PyCelType::ToString)
      .def("__eq__", &PyCelType::operator==);

  type.def_static("List", &PyCelType::ListType, py::arg("element_type"))
      .def_static("Map", &PyCelType::MapType, py::arg("key_type"),
                  py::arg("element_type"))
      .def_static("Type", &PyCelType::TypeType, py::arg("element_type"))
      .def_static("AbstractType", &PyCelType::AbstractType, py::arg("name"),
                  py::arg("params") = std::vector<PyCelType>());

  type.attr("UNKNOWN") = PyCelType::Unknown();
  type.attr("NULL") = PyCelType::Null();
  type.attr("BOOL") = PyCelType::Bool();
  type.attr("INT") = PyCelType::Int();
  type.attr("UINT") = PyCelType::Uint();
  type.attr("DOUBLE") = PyCelType::Double();
  type.attr("STRING") = PyCelType::String();
  type.attr("BYTES") = PyCelType::Bytes();
  type.attr("DYN") = PyCelType::Dyn();
  type.attr("ERROR") = PyCelType::Error();
  type.attr("LIST") = PyCelType::List();
  type.attr("MAP") = PyCelType::Map();
  type.attr("TIMESTAMP") = PyCelType::Timestamp();
  type.attr("DURATION") = PyCelType::Duration();
  type.attr("TYPE") = PyCelType::Type();
}

PyCelType::PyCelType() : PyCelType(cel::Kind::kDyn, "DYN", {}) {}

PyCelType::PyCelType(const std::string& name) : parameters_({}) {
  if (name == "google.protobuf.Timestamp") {
    kind_ = cel::Kind::kTimestamp;
    name_ = "TIMESTAMP";
  } else if (name == "google.protobuf.Duration") {
    kind_ = cel::Kind::kDuration;
    name_ = "DURATION";
  } else {
    kind_ = cel::Kind::kMessage;
    name_ = std::move(name);
  }
}

PyCelType::PyCelType(cel::Kind kind, const std::string& name)
    : kind_(kind), name_(name), parameters_() {}

PyCelType::PyCelType(cel::Kind kind, const std::string& name,
                     std::vector<PyCelType> parameters)
    : kind_(kind), name_(std::move(name)), parameters_(std::move(parameters)) {}

PyCelType PyCelType::ListType(const PyCelType& element_type) {
  return PyCelType(cel::Kind::kList, "LIST", {element_type});
}

absl::StatusOr<PyCelType> PyCelType::MapType(const PyCelType& key_type,
                                             const PyCelType& value_type) {
  if (key_type.GetKind() != cel::Kind::kBool &&
      key_type.GetKind() != cel::Kind::kInt &&
      key_type.GetKind() != cel::Kind::kUint &&
      key_type.GetKind() != cel::Kind::kString &&
      key_type.GetKind() != cel::Kind::kDyn) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported map key type: %s", key_type.GetName()));
  }

  return PyCelType(cel::Kind::kMap, "MAP", {key_type, value_type});
}

PyCelType PyCelType::TypeType(const PyCelType& parameter) {
  return PyCelType(cel::Kind::kType, "TYPE", {parameter});
}

PyCelType PyCelType::AbstractType(const std::string& name,
                                  const std::vector<PyCelType>& params) {
  return PyCelType(cel::Kind::kType, name, params);
}

bool PyCelType::operator==(const PyCelType& other) const {
  return kind_ == other.kind_ && name_ == other.name_ &&
         parameters_ == other.parameters_;
}

bool PyCelType::IsAssignableFrom(const PyCelType& other) const {
  if (kind_ != other.kind_) {
    return false;
  }

  if (kind_ == cel::Kind::kMessage) {
    return name_ == other.name_;
  }

  if (parameters_.empty()) {
    return true;
  }

  if (parameters_.size() != other.parameters_.size()) {
    return false;
  }

  for (int i = 0; i < parameters_.size(); ++i) {
    if (!parameters_[i].IsAssignableFrom(other.parameters_[i])) {
      return false;
    }
  }

  return true;
}

const PyCelType& PyCelType::Unknown() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kUnknown,
                                                           "UNKNOWN");
  return *kInternalType;
}

const PyCelType& PyCelType::Null() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kNull,
                                                           "NULL");
  return *kInternalType;
}

const PyCelType& PyCelType::Bool() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kBool,
                                                           "BOOL");
  return *kInternalType;
}

const PyCelType& PyCelType::Int() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kInt,
                                                           "INT");
  return *kInternalType;
}

const PyCelType& PyCelType::Uint() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kUint,
                                                           "UINT");
  return *kInternalType;
}

const PyCelType& PyCelType::Double() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kDouble,
                                                           "DOUBLE");
  return *kInternalType;
}

const PyCelType& PyCelType::String() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kString,
                                                           "STRING");
  return *kInternalType;
}

const PyCelType& PyCelType::Bytes() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kBytes,
                                                           "BYTES");
  return *kInternalType;
}

const PyCelType& PyCelType::Error() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kError,
                                                           "ERROR");
  return *kInternalType;
}

const PyCelType& PyCelType::Dyn() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kDyn,
                                                           "DYN");
  return *kInternalType;
}

// cel.Type.List(cel.Type.DYN) aka cel.Type.LIST
const PyCelType& PyCelType::List() {
  static const absl::NoDestructor<PyCelType> kInternalType(
      PyCelType::ListType(PyCelType::Dyn()));
  return *kInternalType;
}

// cel.Type.Map(cel.Type.DYN, cel.Type.DYN) aka cel.Type.MAP
const PyCelType& PyCelType::Map() {
  static const absl::NoDestructor<PyCelType> kInternalType(
      *PyCelType::MapType(PyCelType::Dyn(), PyCelType::Dyn()));
  return *kInternalType;
}

const PyCelType& PyCelType::Timestamp() {
  static const absl::NoDestructor<PyCelType> kInternalType(
      cel::Kind::kTimestamp, "TIMESTAMP");
  return *kInternalType;
}

const PyCelType& PyCelType::Duration() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kDuration,
                                                           "DURATION");
  return *kInternalType;
}

const PyCelType& PyCelType::Type() {
  static const absl::NoDestructor<PyCelType> kInternalType(cel::Kind::kType,
                                                           "TYPE");
  return *kInternalType;
}

const absl::flat_hash_map<std::string_view, PyCelType>& GetPrimitivePyTypes() {
  static const absl::NoDestructor<
      absl::flat_hash_map<std::string_view, PyCelType>>
      kPrimitivePyTypes({
          {"NoneType", PyCelType::Null()},
          {"bool", PyCelType::Bool()},
          {"float", PyCelType::Double()},
          {"str", PyCelType::String()},
          {"bytes", PyCelType::Bytes()},
          {"bytearray", PyCelType::Bytes()},
          {"list", PyCelType::List()},
          {"dict", PyCelType::Map()},
      });
  return *kPrimitivePyTypes;
}

std::string PyCelType::GetName() const {
  if (parameters_.empty()) {
    return name_;
  }

  return name_ + "<" +
         absl::StrJoin(parameters_, ", ",
                       [](std::string* out, const PyCelType& param) {
                         out->append(param.GetName());
                       }) +
         ">";
}

std::string PyCelType::ToString() const { return GetName(); }

PyCelType PyCelType::FromCelType(const cel::Type& cel_type) {
  switch (cel_type.kind()) {
    case cel::TypeKind::kNull:
      return PyCelType::Null();
    case cel::TypeKind::kBool:
      return PyCelType::Bool();
    case cel::TypeKind::kInt:
      return PyCelType::Int();
    case cel::TypeKind::kUint:
      return PyCelType::Uint();
    case cel::TypeKind::kDouble:
      return PyCelType::Double();
    case cel::TypeKind::kString:
      return PyCelType::String();
    case cel::TypeKind::kBytes:
      return PyCelType::Bytes();
    case cel::TypeKind::kMessage:
      return PyCelType(std::string(cel_type.name()));
    case cel::TypeKind::kTimestamp:
      return PyCelType::Timestamp();
    case cel::TypeKind::kDuration:
      return PyCelType::Duration();
    case cel::TypeKind::kList:
      return PyCelType::List();
    case cel::TypeKind::kMap:
      return PyCelType::Map();
    case cel::TypeKind::kError:
      return PyCelType::Error();
    case cel::TypeKind::kType: {
      return PyCelType::Type();
    }
    default:
      // TODO(b/447177883): Add support for all standard types.
      return PyCelType::Error();
  }
}

PyCelType PyCelType::ForCelValue(const cel::Value& cel_value) {
  switch (cel_value.kind()) {
    case cel::ValueKind::kNull:
      return PyCelType::Null();
    case cel::ValueKind::kBool:
      return PyCelType::Bool();
    case cel::ValueKind::kInt:
      return PyCelType::Int();
    case cel::ValueKind::kUint:
      return PyCelType::Uint();
    case cel::ValueKind::kDouble:
      return PyCelType::Double();
    case cel::ValueKind::kString:
      return PyCelType::String();
    case cel::ValueKind::kBytes:
      return PyCelType::Bytes();
    case cel::ValueKind::kMessage:
      return PyCelType(std::string(cel_value.AsMessage()->GetTypeName()));
    case cel::ValueKind::kTimestamp:
      return PyCelType::Timestamp();
    case cel::ValueKind::kDuration:
      return PyCelType::Duration();
    case cel::ValueKind::kList:
      return PyCelType::List();
    case cel::ValueKind::kMap:
      return PyCelType::Map();
    case cel::ValueKind::kError:
      return PyCelType::Error();
    case cel::ValueKind::kUnknown:
      return PyCelType::Unknown();
    case cel::ValueKind::kType:
      return PyCelType::Type();
    default:
      // TODO(b/447177883): Add support for all standard types.
      return PyCelType::Error();
  }
}

absl::StatusOr<PyCelType> PyCelType::ForPyObject(
    PyObject* py_object, const std::function<std::string()>& context) {
  ABSL_CHECK(PyGILState_Check());
  PyTypeObject* py_type = Py_TYPE(py_object);
  const absl::flat_hash_map<std::string_view, PyCelType>& primitive_py_types =
      GetPrimitivePyTypes();
  auto it = primitive_py_types.find(py_type->tp_name);
  if (it != primitive_py_types.end()) {
    return it->second;
  }

  if (PyNumber_Check(py_object)) {
    // Attempt to convert to a signed 64-bit integer.
    PyLong_AsLong(py_object);
    if (!PyErr_Occurred()) {
      return PyCelType::Int();
    }
    PyErr_Clear();

    // If the value is too large for a signed 64-bit integer, try converting
    // to an unsigned 64-bit integer. If successful, return a UINT type.
    PyLong_AsUnsignedLong(py_object);
    if (!PyErr_Occurred()) {
      return PyCelType::Uint();
    }

    PyErr_Clear();
    PyObject* repr = PyObject_Repr(py_object);
    std::string repr_str = PyUnicode_AsUTF8(repr);
    Py_DECREF(repr);
    return absl::InvalidArgumentError(absl::StrFormat(
        "Integer value '%s' out of range for '%s'.", repr_str, context()));
  }

  PyObject* descriptor = PyObject_GetAttrString(py_object, "DESCRIPTOR");
  if (descriptor) {
    PyObject* full_name = PyObject_GetAttrString(descriptor, "full_name");
    Py_DECREF(descriptor);
    if (full_name) {
      std::string type_name = PyUnicode_AsUTF8(full_name);
      Py_DECREF(full_name);
      return PyCelType(type_name);
    } else {
      PyErr_Clear();
    }
  } else {
    PyErr_Clear();
  }

  PyTypeObject* type = Py_TYPE(py_object);
  return absl::InvalidArgumentError(absl::StrFormat(
      "Non-CEL value type for '%s': %s.", context(), type->tp_name));
}

static const PyCelType& ConvertPrimitiveType(
    const cel::expr::Type::PrimitiveType& type) {
  switch (type) {
    case cel::expr::Type::BOOL:
      return PyCelType::Bool();
    case cel::expr::Type::INT64:
      return PyCelType::Int();
    case cel::expr::Type::UINT64:
      return PyCelType::Uint();
    case cel::expr::Type::DOUBLE:
      return PyCelType::Double();
    case cel::expr::Type::STRING:
      return PyCelType::String();
    case cel::expr::Type::BYTES:
      return PyCelType::Bytes();
    default:
      return PyCelType::Error();
  }
}

PyCelType PyCelType::FromTypeProto(const cel::expr::Type& type) {
  switch (type.type_kind_case()) {
    case cel::expr::Type::kDyn:
      return PyCelType::Dyn();
    case cel::expr::Type::kNull:
      return PyCelType::Null();
    case cel::expr::Type::kPrimitive:
      return ConvertPrimitiveType(type.primitive());
    case cel::expr::Type::kWrapper:
      return ConvertPrimitiveType(type.wrapper());
    case cel::expr::Type::kWellKnown:
      switch (type.well_known()) {
        case cel::expr::Type::TIMESTAMP:
          return PyCelType::Timestamp();
        case cel::expr::Type::DURATION:
          return PyCelType::Duration();
        default:
          return PyCelType::Error();
      }
    case cel::expr::Type::kListType:
      return PyCelType::ListType(FromTypeProto(type.list_type().elem_type()));
    case cel::expr::Type::kMapType: {
      absl::StatusOr<PyCelType> map_type =
          PyCelType::MapType(FromTypeProto(type.map_type().key_type()),
                             FromTypeProto(type.map_type().value_type()));
      if (!map_type.ok()) {
        PyErr_Format(PyExc_ValueError, "Failed to create map type: %s",
                     map_type.status().message());
        return PyCelType::Error();
      }
      return *map_type;
    }
    case cel::expr::Type::kMessageType:
      return PyCelType(type.message_type());
    case cel::expr::Type::kType: {
      cel::expr::Type parameter = type.type();
      if (parameter.type_kind_case()) {
        return PyCelType::TypeType(FromTypeProto(parameter));
      }
      return PyCelType::Type();
    }
    case cel::expr::Type::kAbstractType: {
      cel::expr::Type::AbstractType abstract_type =
          type.abstract_type();
      int param_count = abstract_type.parameter_types_size();
      if (param_count == 0) {
        return PyCelType::AbstractType(abstract_type.name());
      }
      std::vector<PyCelType> params;
      for (const cel::expr::Type& param :
           abstract_type.parameter_types()) {
        params.push_back(FromTypeProto(param));
      }
      return PyCelType::AbstractType(abstract_type.name(), params);
    }
    default:
      break;
  }
  return PyCelType::Error();
}

absl::StatusOr<cel::Type> PyCelType::ToCelType(
    const PyCelType& type, google::protobuf::Arena* arena,
    const google::protobuf::DescriptorPool& descriptor_pool) {
  switch (type.GetKind()) {
    case cel::Kind::kBool:
      return cel::BoolType();
    case cel::Kind::kInt:
      return cel::IntType();
    case cel::Kind::kUint:
      return cel::UintType();
    case cel::Kind::kDouble:
      return cel::DoubleType();
    case cel::Kind::kString:
      return cel::StringType();
    case cel::Kind::kBytes:
      return cel::BytesType();
    case cel::Kind::kMessage: {
      const std::string& message_type_name = type.GetName();
      const google::protobuf::Descriptor* descriptor =
          descriptor_pool.FindMessageTypeByName(message_type_name);
      if (descriptor == nullptr) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Message type '%s' not found in descriptor pool.",
                            message_type_name));
      }
      return cel::Type::Message(descriptor);
    }
    case cel::Kind::kList: {
      CEL_PYTHON_ASSIGN_OR_RETURN(
          cel::Type element_type,
          ToCelType(type.GetParam(0), arena, descriptor_pool));
      return cel::ListType(arena, element_type);
    }
    case cel::Kind::kMap: {
      CEL_PYTHON_ASSIGN_OR_RETURN(
          cel::Type key_type,
          ToCelType(type.GetParam(0), arena, descriptor_pool));
      CEL_PYTHON_ASSIGN_OR_RETURN(
          cel::Type value_type,
          ToCelType(type.GetParam(1), arena, descriptor_pool));
      return cel::MapType(arena, key_type, value_type);
    }
    case cel::Kind::kTimestamp:
      return cel::TimestampType();
    case cel::Kind::kDuration:
      return cel::DurationType();
    default:
      return cel::DynType();
  }
}

}  // namespace cel_python
