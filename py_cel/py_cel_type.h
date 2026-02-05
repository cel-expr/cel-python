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
#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_TYPE_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_TYPE_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <functional>
#include <string>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "absl/status/statusor.h"
#include "common/kind.h"
#include "common/type.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include <pybind11/pybind11.h>

namespace cel_python {

// Represents a CEL type.
class PyCelType {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  // Creates a dynamic type.
  PyCelType();
  PyCelType(const PyCelType& other) = default;
  PyCelType& operator=(const PyCelType& other) = default;
  PyCelType(PyCelType&& other) = default;
  PyCelType& operator=(PyCelType&& other) = default;

  // Creates a message type.
  explicit PyCelType(const std::string& name);
  PyCelType(cel::Kind kind, const std::string& name);
  PyCelType(cel::Kind kind, const std::string& name,
            std::vector<PyCelType> parameters);
  ~PyCelType() = default;

  static PyCelType ListType(const PyCelType& element_type);
  // May throw if the key type is not supported.
  static PyCelType MapType(const PyCelType& key_type,
                           const PyCelType& value_type);
  static PyCelType TypeType(const PyCelType& parameter);
  static PyCelType AbstractType(const std::string& name,
                                const std::vector<PyCelType>& params = {});

  cel::Kind GetKind() const { return kind_; }
  std::string GetName() const;
  bool IsAssignableFrom(const PyCelType& other) const;
  bool IsMessage() const { return kind_ == cel::Kind::kMessage; }

  const PyCelType& GetParam(int index) const { return parameters_[index]; }

  std::string ToString() const;

  static absl::StatusOr<PyCelType> ForPyObject(
      PyObject* py_object, const std::function<std::string()>& context);
  static PyCelType ForCelValue(const cel::Value& cel_value);
  static PyCelType FromCelType(const cel::Type& cel_type);
  static PyCelType FromTypeProto(const cel::expr::Type& type);
  static absl::StatusOr<cel::Type> ToCelType(
      const PyCelType& type, google::protobuf::Arena* arena,
      const google::protobuf::DescriptorPool& descriptor_pool);

  bool operator==(const PyCelType& other) const;

  static const PyCelType& Unknown();
  static const PyCelType& Null();
  static const PyCelType& Bool();
  static const PyCelType& Int();
  static const PyCelType& Uint();
  static const PyCelType& Double();
  static const PyCelType& Bytes();
  static const PyCelType& String();
  static const PyCelType& Error();
  static const PyCelType& Dyn();
  static const PyCelType& List();
  static const PyCelType& Map();
  static const PyCelType& Timestamp();
  static const PyCelType& Duration();
  static const PyCelType& Type();

 private:
  cel::Kind kind_;
  std::string name_;
  std::vector<PyCelType> parameters_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_TYPE_H_
