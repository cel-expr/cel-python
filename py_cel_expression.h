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

#ifndef THIRD_PARTY_CEL_PYTHON_PY_CEL_EXPRESSION_H_
#define THIRD_PARTY_CEL_PYTHON_PY_CEL_EXPRESSION_H_

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/status/statusor.h"
#include "runtime/runtime.h"
#include "py_cel_activation.h"
#include "py_cel_type.h"
#include "py_cel_value.h"
#include <pybind11/pybind11.h>

namespace cel_python {

class PyCelEnvInternal;

// Wraps a CelExpression. Supports serialization and deserialization.
class PyCelExpression {
 public:
  static void DefinePythonBindings(pybind11::module& m);

  PyCelExpression(PyCelExpression&& other) = default;

  PyCelExpression(const cel::expr::ParsedExpr& parsed_expr,
                  std::shared_ptr<PyCelEnvInternal> env)
      : expr_(std::move(parsed_expr)), env_(std::move(env)) {}
  PyCelExpression(const cel::expr::CheckedExpr& checked_expr,
                  std::shared_ptr<PyCelEnvInternal> env)
      : expr_(std::move(checked_expr)), env_(std::move(env)) {}

  PyCelType GetReturnType();

  absl::StatusOr<PyCelValue> Eval(const PyCelActivation& activation);

  std::string Serialize() const;

  static absl::StatusOr<PyCelExpression> Compile(
      const std::shared_ptr<PyCelEnvInternal>& env, const std::string& cel_expr,
      bool disable_check);

  static absl::StatusOr<PyCelExpression> Deserialize(
      const std::shared_ptr<PyCelEnvInternal>& env,
      const std::string& serialized_expr);

 private:
  std::variant<cel::expr::ParsedExpr, cel::expr::CheckedExpr>
      expr_;
  std::shared_ptr<PyCelEnvInternal> env_;
  std::unique_ptr<cel::Program> cel_program_;
};

}  // namespace cel_python

#endif  // THIRD_PARTY_CEL_PYTHON_PY_CEL_EXPRESSION_H_
