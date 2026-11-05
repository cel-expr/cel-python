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

// Shares the `cel::Compiler` owned by a `cel.Env` with pybind11 modules that
// are built and loaded separately from the `cel` module (e.g. `cel_policy`)
// without linking against the `cel` module's internals.
//
// The `cel` module exposes the compiler via the private
// `cel.Env._compiler_capsule()` method, which returns a PyCapsule created by
// `MakeCelCompilerCapsule()`. Other modules read it with `GetCelCompiler()`.

#ifndef THIRD_PARTY_CEL_PYTHON_CEL_COMPILER_CAPSULE_H_
#define THIRD_PARTY_CEL_PYTHON_CEL_COMPILER_CAPSULE_H_

#include <Python.h>  // IWYU pragma: keep - Needed for PyCapsule_GetPointer

#include <memory>
#include <utility>

#include "compiler/compiler.h"
#include "cel_expr_python/cel_extension.h"
#include <pybind11/pybind11.h>

namespace cel_python {

// Name of the PyCapsule returned by `cel.Env._compiler_capsule()`. The capsule
// holds a heap-allocated `std::shared_ptr<const cel::Compiler>`, which keeps
// the environment that owns the compiler alive for the capsule's lifetime.
inline constexpr char kCelCompilerCapsuleName[] =
    "cel_python.CompilerSharedPtr";

// Wraps `compiler` in a PyCapsule named `kCelCompilerCapsuleName`. The capsule
// shares ownership of `compiler` and releases it when garbage collected.
inline pybind11::capsule MakeCelCompilerCapsule(
    std::shared_ptr<const cel::Compiler> compiler) {
  return pybind11::capsule(
      new std::shared_ptr<const cel::Compiler>(std::move(compiler)),
      kCelCompilerCapsuleName, [](void* ptr) {
        delete static_cast<std::shared_ptr<const cel::Compiler>*>(ptr);
      });
}

// Returns the compiler owned by `env`, which must be a `cel.Env` instance.
// The returned pointer shares ownership of the compiler, so it remains valid
// even if `env` is garbage collected. Requires the GIL.
//
// Throws `TypeError` if `env` is not a `cel.Env`.
inline std::shared_ptr<const cel::Compiler> GetCelCompiler(
    pybind11::handle env) {
  PYBIND11_CONSTINIT static pybind11::gil_safe_call_once_and_store<
      pybind11::object>
      env_type_storage;
  const pybind11::object& env_type =
      env_type_storage
          .call_once_and_store_result([] {
            return pybind11::module_::import(CEL_MODULE_NAME).attr("Env");
          })
          .get_stored();
  if (!pybind11::isinstance(env, env_type)) {
    throw pybind11::type_error("env must be an instance of cel.Env");
  }
  pybind11::capsule capsule =
      env.attr("_compiler_capsule")().cast<pybind11::capsule>();
  void* ptr = PyCapsule_GetPointer(capsule.ptr(), kCelCompilerCapsuleName);
  if (ptr == nullptr) {
    throw pybind11::error_already_set();
  }
  return *static_cast<std::shared_ptr<const cel::Compiler>*>(ptr);
}

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_CEL_COMPILER_CAPSULE_H_
