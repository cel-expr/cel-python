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
#include "cel_expr_python/py_cel_env_config.h"

#include <memory>
#include <sstream>
#include <string>

#include "env/env_yaml.h"
#include "cel_expr_python/py_error_status.h"
#include <pybind11/pybind11.h>

namespace cel_python {

namespace py = ::pybind11;

void PyCelEnvConfig::DefinePythonBindings(pybind11::module& m) {
  py::class_<PyCelEnvConfig, std::shared_ptr<PyCelEnvConfig>> cel_class(
      m, "EnvConfig");
  m.def("NewEnvConfigFromYaml", &PyCelEnvConfig::FromYaml, py::arg("yaml"));

  cel_class.def("to_yaml", &PyCelEnvConfig::ToYaml);
}

PyCelEnvConfig PyCelEnvConfig::FromYaml(std::string yaml) {
  PyCelEnvConfig config;
  config.config_ = ThrowIfError(cel::EnvConfigFromYaml(yaml));
  return config;
}

std::string PyCelEnvConfig::ToYaml() const {
  std::stringstream ss;
  cel::EnvConfigToYaml(config_, ss);
  return ss.str();
}

}  // namespace cel_python
