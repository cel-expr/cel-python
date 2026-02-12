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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_ARENA_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_ARENA_H_

#include <memory>

#include "absl/status/statusor.h"
#include "google/protobuf/arena.h"
#include <pybind11/pybind11.h>

namespace cel_python {

// Internal class. All implementation details are private. Use `py_cel.Arena()`
// to obtain an instance
class PyCelArena {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  ~PyCelArena();

  // Visible for testing
  static int GetInstanceCount();

  google::protobuf::Arena* GetArena() { return &arena_; };

  static absl::StatusOr<std::shared_ptr<PyCelArena>> FromProtoArena(
      google::protobuf::Arena* proto_arena);

  friend std::shared_ptr<PyCelArena> NewArena();

 private:
  // Private constructor. Use `NewArena()` to obtain an instance.
  PyCelArena();
  google::protobuf::Arena arena_;
};

std::shared_ptr<PyCelArena> NewArena();

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_ARENA_H_
