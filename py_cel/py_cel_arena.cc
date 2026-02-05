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

#include "py_cel/py_cel_arena.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <atomic>
#include <cstdint>
#include <memory>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/protobuf/arena.h"
#include <pybind11/pybind11.h>

namespace cel_python {

namespace py = ::pybind11;

void PyCelArena::DefinePythonBindings(py::module_& m) {
  py::class_<PyCelArena, std::shared_ptr<PyCelArena>>(
      m, "_InternalArena",
      R"doc(Use `py_cel.Arena()` to obtain an instance.)doc")
      .def_static("_get_instance_count", &GetInstanceCount);

  m.def("Arena", &NewArena, py::return_value_policy::take_ownership,
        "Creates a new Arena.");
}

// A thread-local map of google::protobuf::Arena* to PyCelArena. It is used when an
// internal CEL API passes a google::protobuf::Arena* but the implementation
// requires a std::shared_ptr<PyCelArena>.
inline static absl::flat_hash_map<google::protobuf::Arena*, std::weak_ptr<PyCelArena>>&
GetArenaMap() {
  static thread_local absl::NoDestructor<
      absl::flat_hash_map<google::protobuf::Arena*, std::weak_ptr<PyCelArena>>>
      arena_map;
  return *arena_map;
}

std::shared_ptr<PyCelArena> NewArena() {
  std::shared_ptr<PyCelArena> anArena(new PyCelArena());
  GetArenaMap()[anArena->GetArena()] = anArena;
  return anArena;
}

static std::atomic<uint64_t> instance_count_ = 0;

PyCelArena::PyCelArena() { instance_count_++; }

PyCelArena::~PyCelArena() {
  GetArenaMap().erase(&arena_);
  instance_count_--;
}

int PyCelArena::GetInstanceCount() { return instance_count_; }

absl::StatusOr<std::shared_ptr<PyCelArena>> PyCelArena::FromProtoArena(
    google::protobuf::Arena* proto_arena) {
  auto it = GetArenaMap().find(proto_arena);
  if (it != GetArenaMap().end()) {
    if (it->second.expired()) {
      // This should never happen as long as the implementation of this class
      // is correct.
      return absl::InternalError("PyCelArena released before google::protobuf::Arena.");
    }
    return it->second.lock();
  }
  return absl::InternalError("No PyCelArena found for google::protobuf::Arena.");
}

}  // namespace cel_python
