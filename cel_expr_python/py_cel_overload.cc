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

#include "cel_expr_python/py_cel_overload.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "env/config.h"
#include "cel_expr_python/py_cel_type.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace cel_python {

namespace py = ::pybind11;

void PyCelOverload::DefinePythonBindings(py::module_& m) {
  py::class_<PyCelOverload, std::shared_ptr<PyCelOverload>>(m, "Overload")
      .def(py::init([](const std::string& overload_id,
                       const PyCelType& return_type,
                       const std::vector<PyCelType>& parameters, bool is_member,
                       py::object impl) {
             return PyCelOverload(overload_id, return_type, parameters,
                                  is_member, std::move(impl));
           }),
           py::arg("overload_id"), py::arg("return_type") = PyCelType::Dyn(),
           py::arg("parameters") = std::vector<PyCelType>{},
           py::arg("is_member") = false, py::arg("impl") = py::none());
}

PyCelOverload::PyCelOverload(std::string overload_id,
                             const PyCelType& return_type,
                             std::vector<PyCelType> parameters, bool is_member,
                             py::object py_function)
    : overload_id_(std::move(overload_id)),
      return_type_(return_type),
      parameters_(std::move(parameters)),
      is_member_(is_member),
      py_function_(std::move(py_function)) {}

cel::Config::FunctionOverloadConfig PyCelOverload::ToFunctionOverloadConfig()
    const {
  cel::Config::FunctionOverloadConfig overload_config;
  overload_config.overload_id = overload_id_;
  overload_config.return_type = PyCelType::ToTypeInfo(return_type_);
  overload_config.parameters.reserve(parameters_.size());
  for (const PyCelType& parameter : parameters_) {
    overload_config.parameters.push_back(PyCelType::ToTypeInfo(parameter));
  }
  overload_config.is_member_function = is_member_;
  return overload_config;
}
}  // namespace cel_python
