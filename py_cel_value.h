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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_VALUE_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_VALUE_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "py_cel_type.h"
#include "google/protobuf/arena.h"
#include <pybind11/pybind11.h>

namespace cel_python {

class PyCelArena;
class PyCelEnv;
class PyMessageFactory;

// Wraps a cel::Value. Converts the cel::Value to a Python object on demand and
// caches the Python object for the lifetime of the CelValue.
// Retains a reference to the arena and message factory to ensure they outlive
// the cel::Value.
class PyCelValue {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelValue(cel::Value& cel_value, std::shared_ptr<PyCelArena> arena,
             std::shared_ptr<PyCelEnv> env);

  // Move constructor and assignment.
  PyCelValue(PyCelValue&& other) noexcept = default;
  PyCelValue& operator=(PyCelValue&& other) noexcept = default;

  // Disallow copying.
  PyCelValue(const PyCelValue&) = delete;
  PyCelValue& operator=(const PyCelValue&) = delete;

  virtual ~PyCelValue();

  virtual PyCelType Type();
  virtual PyObject* Value();
  virtual PyObject* PlainValue();
  virtual std::string ToString();

  static cel::Value ToCelValue(PyObject* object,
                               std::shared_ptr<PyCelArena> arena,
                               PyMessageFactory* py_message_factory);

 protected:
  cel::Value cel_value_;
  PyObject* object_;
  PyObject* plain_object_;
  std::shared_ptr<PyCelArena> arena_;
  std::shared_ptr<PyCelEnv> env_;
};

// Variant of PyCelValue that is used to access a specific element of a list.
class PyCelListItemAccessor : public PyCelValue {
 public:
  PyCelListItemAccessor(cel::Value celValue, int index,
                        std::shared_ptr<PyCelArena> arena,
                        std::shared_ptr<PyCelEnv> env)
      : PyCelValue(celValue, std::move(arena), std::move(env)), index_(index) {}

  // Move constructor.
  PyCelListItemAccessor(PyCelListItemAccessor&& other) noexcept = default;

  ~PyCelListItemAccessor() override = default;

  // Extracts the element at the given index from the list and caches the
  // result. This is called on demand when the python side accesses the value.
  void ResolveElement();
  PyCelType Type() override;
  PyObject* Value() override;
  PyObject* PlainValue() override;
  std::string ToString() override;

 private:
  int index_;
  bool resolved_ = false;
  cel::Value element_value_;
};

// Variant of PyCelValue that is used to access a specific value from a map.
class PyCelMapItemAccessor : public PyCelValue {
 public:
  PyCelMapItemAccessor(cel::Value celValue, cel::Value key,
                       std::shared_ptr<PyCelArena> arena,
                       std::shared_ptr<PyCelEnv> env)
      : PyCelValue(celValue, std::move(arena), std::move(env)),
        key_(std::move(key)) {}

  // Move constructor.
  PyCelMapItemAccessor(PyCelMapItemAccessor&& other) noexcept = default;

  ~PyCelMapItemAccessor() override = default;

  void ResolveElement();
  PyCelType Type() override;
  PyObject* Value() override;
  PyObject* PlainValue() override;
  std::string ToString() override;

 private:
  cel::Value key_;
  cel::Value element_value_;
  bool resolved_ = false;
};

PyObject* CelValueToPyObject(const cel::Value& cel_value,
                             const std::shared_ptr<PyCelEnv>& env,
                             const std::shared_ptr<PyCelArena>& arena,
                             bool plain_value);

absl::StatusOr<cel::Value> PyObjectToCelValue(
    PyObject* py_object, const PyCelType& expected_type,
    absl::FunctionRef<std::string()> context,
    const std::shared_ptr<PyCelEnv>& env, google::protobuf::Arena* arena,
    bool bypass_type_check = false);

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_VALUE_H_
