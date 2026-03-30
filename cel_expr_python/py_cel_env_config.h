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
#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_CONFIG_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_CONFIG_H_

#include <string>

#include "env/config.h"
#include <pybind11/pybind11.h>

namespace cel_python {

class PyCelEnvConfig {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelEnvConfig() = default;
  explicit PyCelEnvConfig(const cel::Config& config) : config_(config) {}

  static PyCelEnvConfig FromYaml(std::string yaml);
  std::string ToYaml() const;

  const cel::Config& GetConfig() const { return config_; }

 private:
  cel::Config config_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_ENV_CONFIG_H_
