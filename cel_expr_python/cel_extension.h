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

#ifndef THIRD_PARTY_CEL_PYTHON_CEL_EXTENSION_H_
#define THIRD_PARTY_CEL_PYTHON_CEL_EXTENSION_H_

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "compiler/compiler.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
// This include causes many issues if included in C++ environments that don't
// support exceptions. The include is only needed for pybind11-based extension
// implementations, so we can safely skip it in other cases.
#include <pybind11/pybind11.h>
#endif

namespace cel_python {

// Base class used for pybind11-based extensions. It is not instantiable from
// Python.
class CelExtension {
 public:
  explicit CelExtension(std::string name) : name_(std::move(name)) {};
  virtual ~CelExtension() = default;

  virtual cel::CompilerLibrary GetCompilerLibrary() {
    return cel::CompilerLibrary(name_, nullptr, nullptr);
  }

  virtual absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                        const cel::RuntimeOptions& opts) {
    return absl::OkStatus();
  }

  std::string name() const { return name_; }

 private:
  std::string name_;
};

#define CEL_MODULE_NAME "cel_expr_python.cel"

// Macro for defining a pybind11 module for a CEL extension. The macro takes two
// arguments: the name of the module and the name of the extension class. It
// must be used after the extension class is defined.
//
// The extension class must implement the `CelExtension` interface defined
// above and provide a public default constructor.
//
// Example:
//
//   class SampleCelExtension : public cel_python::CelExtension {
//     ...
//   };
//
//   CEL_EXTENSION_MODULE(sample_cel_ext, SampleCelExtension);
//
#define CEL_EXTENSION_MODULE(module_name, class_name)                      \
  PYBIND11_MODULE(module_name, m) {                                        \
    pybind11::module_::import(CEL_MODULE_NAME);                            \
    pybind11::class_<class_name, cel_python::CelExtension>(m, #class_name) \
        .def(pybind11::init<>());                                          \
  }

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_CEL_EXTENSION_H_
