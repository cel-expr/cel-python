/*
 * Copyright 2026 Google LLC
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

#include "cel_expr_python/py_cel_options.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include <pybind11/pybind11.h>

namespace cel_python {

namespace py = ::pybind11;

namespace {

std::string GenericRepr(py::handle self) {
  std::vector<std::string> parts;
  py::handle cls = self.get_type();
  std::string class_name = cls.attr("__name__").cast<std::string>();
  py::dict dict = cls.attr("__dict__");
  for (const auto& item : dict) {
    std::string name = item.first.cast<std::string>();
    if (!name.empty() && name[0] == '_') {
      continue;
    }
    py::object val = self.attr(item.first);
    parts.push_back(
        absl::StrFormat("%s=%s", name, py::repr(val).cast<std::string>()));
  }
  std::sort(parts.begin(), parts.end());
  return absl::StrFormat("%s(%s)", class_name, absl::StrJoin(parts, ", "));
}

}  // namespace

void PyCelOptions::DefinePythonBindings(pybind11::module& m) {
  py::class_<PyCelOptions, std::shared_ptr<PyCelOptions>>(m, "Options")
      .def(py::init([](bool enable_pratt_parser) {
             return PyCelOptions{.enable_pratt_parser = enable_pratt_parser};
           }),
           py::arg("enable_pratt_parser") = false)
      .def_readwrite("enable_pratt_parser", &PyCelOptions::enable_pratt_parser)
      .def("__repr__", &GenericRepr);
}

}  // namespace cel_python
