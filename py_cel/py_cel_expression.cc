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

#include "py_cel/py_cel_expression.h"

#include <Python.h>  // IWYU pragma: keep - Needed for PyObject

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/ast_proto.h"
#include "common/source.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "parser/parser_interface.h"
#include "runtime/embedder_context.h"
#include "runtime/runtime.h"
#include "py_cel/py_cel_activation.h"
#include "py_cel/py_cel_arena.h"
#include "py_cel/py_cel_env_internal.h"
#include "py_cel/py_cel_function.h"
#include "py_cel/py_cel_type.h"
#include "py_cel/py_cel_value.h"
#include "py_cel/py_error_status.h"
#include "py_cel/status_macros.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace cel_python {

using ::cel::expr::CheckedExpr;
using ::cel::expr::ParsedExpr;

namespace py = ::pybind11;

void PyCelExpression::DefinePythonBindings(py::module& m) {
  py::class_<PyCelExpression>(m, "Expression")
      .def("return_type", &PyCelExpression::GetReturnType)
      .def("serialize",
           [](PyCelExpression& self) {
             // Wrap the string in py::bytes to avoid triggering an automatic
             // decoder from std::string to Python `str`.
             return py::bytes(self.PyCelExpression::Serialize());
           })
      .def(
          "eval",
          [](PyCelExpression& self,
             std::optional<std::shared_ptr<PyCelActivation>> activation,
             const std::optional<std::unordered_map<std::string, py::object>>&
                 data,
             const std::optional<std::vector<std::shared_ptr<PyCelFunction>>>&
                 functions,
             const std::shared_ptr<PyCelArena>& arena = nullptr) -> PyCelValue {
            if (activation) {
              if (data || functions || arena) {
                throw StatusToException(absl::InvalidArgumentError(
                    "Cannot provide both activation and any other arguments."));
              }
              return ThrowIfError(self.Eval(**activation));
            }
            std::unordered_map<std::string, PyObject*> data_ptrs;
            if (data) {
              for (auto const& [key, val] : *data) {
                data_ptrs[key] = val.ptr();
              }
            }
            return ThrowIfError(self.Eval(PyCelActivation(
                self.env_, data_ptrs,
                functions.value_or(
                    std::vector<std::shared_ptr<PyCelFunction>>{}),
                arena ? arena : NewArena())));
          },
          py::arg("activation") = py::none(), py::arg("data") = py::none(),
          py::arg("functions") = py::none(), py::arg("arena") = nullptr);
}

absl::StatusOr<PyCelExpression> PyCelExpression::Compile(
    const std::shared_ptr<PyCelEnvInternal>& env, const std::string& cel_expr,
    bool disable_check) {
  ABSL_CHECK(PyGILState_Check());

  CEL_PYTHON_ASSIGN_OR_RETURN(const cel::Compiler* compiler,
                              PyCelEnvInternal::GetCompiler(env));

  if (disable_check) {
    CEL_PYTHON_ASSIGN_OR_RETURN(auto s, cel::NewSource(cel_expr, "<input>"));
    PY_CEL_PYTHON_ASSIGN_OR_RETURN(auto ast, compiler->GetParser().Parse(*s));
    ParsedExpr parsed_expr;
    CEL_PYTHON_RETURN_IF_ERROR(cel::AstToParsedExpr(*ast, &parsed_expr));
    return PyCelExpression(parsed_expr, env);
  }

  PY_CEL_PYTHON_ASSIGN_OR_RETURN(auto validation, compiler->Compile(cel_expr));
  if (!validation.IsValid() || validation.GetAst() == nullptr) {
    return absl::InvalidArgumentError(validation.FormatError());
  }
  CEL_PYTHON_ASSIGN_OR_RETURN(std::unique_ptr<cel::Ast> ast,
                              validation.ReleaseAst());
  CheckedExpr checked_expr;
  CEL_PYTHON_RETURN_IF_ERROR(cel::AstToCheckedExpr(*ast, &checked_expr));
  return PyCelExpression(checked_expr, env);
}

PyCelType PyCelExpression::GetReturnType() {
  if (!std::holds_alternative<CheckedExpr>(expr_)) {
    return PyCelType::Dyn();
  }

  CheckedExpr& checked_expr = std::get<CheckedExpr>(expr_);
  // The deduced return type can be found in the type map under the root expr
  // id.
  int64_t root_id = checked_expr.expr().id();
  auto it = checked_expr.type_map().find(root_id);
  if (it == checked_expr.type_map().end()) {
    return PyCelType::Dyn();
  }

  return PyCelType::FromTypeProto(it->second);
}

absl::StatusOr<PyCelValue> PyCelExpression::Eval(
    const PyCelActivation& activation) {
  ABSL_CHECK(PyGILState_Check());
  if (cel_program_ == nullptr) {
    if (std::holds_alternative<ParsedExpr>(expr_)) {
      PY_CEL_PYTHON_ASSIGN_OR_RETURN(
          const cel::Runtime* runtime,
          PyCelEnvInternal::GetRuntime(
              env_, PyCelEnvInternal::kStandardIgnoreWarnings));
      PY_CEL_PYTHON_ASSIGN_OR_RETURN(
          cel_program_, cel::extensions::ProtobufRuntimeAdapter::CreateProgram(
                            *runtime, std::get<ParsedExpr>(expr_)));
    } else {
      PY_CEL_PYTHON_ASSIGN_OR_RETURN(
          const cel::Runtime* runtime,
          PyCelEnvInternal::GetRuntime(env_, PyCelEnvInternal::kStandard));
      PY_CEL_PYTHON_ASSIGN_OR_RETURN(
          cel_program_, cel::extensions::ProtobufRuntimeAdapter::CreateProgram(
                            *runtime, std::get<CheckedExpr>(expr_)));
    }
  }
  std::shared_ptr<PyCelArena> arena = activation.GetArena();
  std::shared_ptr<PyCelEnvInternal> env = activation.GetEnv();
  cel::EmbedderContext embedder_context = cel::EmbedderContext::From(&env);
  cel::EvaluateOptions options;
  options.message_factory = env->GetMessageFactory();
  options.embedder_context = &embedder_context;
  PY_CEL_PYTHON_ASSIGN_OR_RETURN(
      cel::Value result,
      cel_program_->Evaluate(arena->GetArena(), *activation.GetActivation(),
                             std::move(options)));
  return PyCelValue(result, arena, std::move(env));
}

std::string PyCelExpression::Serialize() const {
  google::protobuf::Any any;
  if (std::holds_alternative<ParsedExpr>(expr_)) {
    any.PackFrom(std::get<ParsedExpr>(expr_));
  } else {
    any.PackFrom(std::get<CheckedExpr>(expr_));
  }
  return any.SerializeAsString();
}

absl::StatusOr<PyCelExpression> PyCelExpression::Deserialize(
    const std::shared_ptr<PyCelEnvInternal>& env,
    const std::string& serialized_expr) {
  ABSL_CHECK(PyGILState_Check());
  google::protobuf::Any any;
  if (!any.ParseFromString(serialized_expr)) {
    return absl::InvalidArgumentError(
        "Cannot parse serialized CEL expression. "
        "Failed to parse google.protobuf.Any proto.");
  }

  CheckedExpr checked_expr;
  if (any.UnpackTo(&checked_expr)) {
    return PyCelExpression(checked_expr, env);
  }
  ParsedExpr parsed_expr;
  if (any.UnpackTo(&parsed_expr)) {
    return PyCelExpression(parsed_expr, env);
  }

  return absl::InvalidArgumentError(
      absl::StrFormat("Cannot parse serialized CEL expression. It contains "
                      "neither a google.api.expr.CheckedExpr nor a "
                      "google.api.expr.ParsedExpr."));
}

}  // namespace cel_python
