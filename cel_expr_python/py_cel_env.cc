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

#include "cel_expr_python/py_cel_env.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/log/absl_check.h"
#include "cel_expr_python/py_cel_activation.h"
#include "cel_expr_python/py_cel_arena.h"
#include "cel_expr_python/py_cel_env_internal.h"
#include "cel_expr_python/py_cel_expression.h"
#include "cel_expr_python/py_cel_type.h"
#include "cel_expr_python/py_error_status.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace cel_python {

namespace py = ::pybind11;

void PyCelEnv::DefinePythonBindings(pybind11::module& m) {
  py::class_<PyCelEnv, std::shared_ptr<PyCelEnv>> cel_class(m, "Env");
  m.def(
      "NewEnv",
      [](py::object descriptor_pool,
         std::optional<std::unordered_map<std::string, PyCelType>> variables,
         std::optional<std::vector<py::object>> extensions,
         const std::optional<std::string>& container) {
        PyObject* pool_ptr;
        if (descriptor_pool.is_none()) {
          // Replicates python's `descriptor_pool.Default()`
          try {
            pool_ptr = py::module::import("google.protobuf.descriptor_pool")
                           .attr("Default")()
                           .ptr();
          } catch (const std::exception& e) {
            // google.protobuf.descriptor_pool is not available.
            pool_ptr = nullptr;
            PyErr_Clear();  // Clear the Python error state.
          }
        } else {
          pool_ptr = descriptor_pool.ptr();
        }

        std::vector<PyObject*> ext_ptrs;
        if (extensions) {
          ext_ptrs.reserve(extensions->size());
          for (const auto& ext : *extensions) {
            ext_ptrs.push_back(ext.ptr());
          }
        }

        return PyCelEnv(pool_ptr,
                        std::move(variables).value_or(
                            std::unordered_map<std::string, PyCelType>{}),
                        ext_ptrs, container.value_or(""));
      },
      py::arg("descriptor_pool") = py::none(),
      py::arg("variables") = py::none(), py::arg("extensions") = py::none(),
      py::arg("container") = py::none());
  cel_class
      .def("compile", &PyCelEnv::Compile, py::arg("expression"),
           py::arg("disable_check") = false)
      .def("deserialize", &PyCelEnv::Deserialize, py::arg("serialized"))
      .def(
          "Activation",
          [](PyCelEnv& self,
             std::optional<std::unordered_map<std::string, py::object>> data,
             const std::optional<std::vector<std::shared_ptr<PyCelFunction>>>&
                 functions,
             std::shared_ptr<PyCelArena> arena = nullptr) {
            if (!arena) {
              arena = NewArena();
            }
            std::unordered_map<std::string, PyObject*> data_ptrs;
            if (data) {
              for (auto const& [key, val] : *data) {
                data_ptrs[key] = val.ptr();
              }
            }
            return self.NewActivation(
                data_ptrs,
                functions.value_or(
                    std::vector<std::shared_ptr<PyCelFunction>>{}),
                arena);
          },
          py::arg("data") = py::none(), py::arg("functions") = py::none(),
          py::arg("arena") = nullptr);
}

PyCelEnv::PyCelEnv(PyObject* descriptor_pool,
                   std::unordered_map<std::string, PyCelType> variable_types,
                   const std::vector<PyObject*>& extensions,
                   std::string container)
    : env_(std::make_unique<PyCelEnvInternal>(
          descriptor_pool, std::move(variable_types), extensions,
          std::move(container))) {
  ABSL_CHECK(PyGILState_Check());
}

PyCelEnv::~PyCelEnv() = default;

std::shared_ptr<PyCelActivation> PyCelEnv::NewActivation(
    const std::unordered_map<std::string, PyObject*>& data,
    const std::vector<std::shared_ptr<PyCelFunction>>& functions,
    const std::shared_ptr<PyCelArena>& arena) {
  return std::make_shared<PyCelActivation>(env_, data, functions, arena);
}

PyCelExpression PyCelEnv::Compile(const std::string& cel_expr,
                                  bool disable_check) {
  return ThrowIfError(PyCelExpression::Compile(env_, cel_expr, disable_check));
}

PyCelExpression PyCelEnv::Deserialize(const std::string& serialized_expr) {
  return ThrowIfError(PyCelExpression::Deserialize(env_, serialized_expr));
}

}  // namespace cel_python
