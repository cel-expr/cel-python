// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cel_expr_python/policy/py_cel_policy.h"

#include <algorithm>
#include <any>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/ast_proto.h"
#include "compiler/compiler.h"
#include "policy/cel_policy.h"
#include "policy/cel_policy_parse_result.h"
#include "policy/cel_policy_validation_result.h"
#include "policy/compiler.h"
#include "cel_expr_python/cel_compiler_capsule.h"
#include "cel_expr_python/py_error_status.h"
#include "cel_expr_python/status_macros.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace cel_python {

namespace py = ::pybind11;

namespace {

std::string IndentBlock(absl::string_view text) {
  if (text.empty()) {
    return "";
  }
  std::vector<std::string> lines;
  for (absl::string_view line : absl::StrSplit(text, '\n')) {
    if (line.empty()) {
      lines.push_back("");
    } else {
      lines.push_back(absl::StrCat("  ", line));
    }
  }
  return absl::StrJoin(lines, "\n");
}

std::string MetadataValueDebugString(std::any value) {
  if (value.type() == typeid(py::object)) {
    return py::str(std::any_cast<py::object>(value)).cast<std::string>();
  }
  if (value.type() == typeid(std::monostate)) {
    return "null";
  }
  if (value.type() == typeid(cel::ValueString)) {
    return std::any_cast<cel::ValueString>(value).DebugString();
  }
  if (value.type() == typeid(bool)) {
    return std::any_cast<bool>(value) ? "true" : "false";
  }
  if (value.type() == typeid(int)) {
    return absl::StrCat(std::any_cast<int>(value));
  }
  if (value.type() == typeid(std::string)) {
    return std::any_cast<std::string>(value);
  }
  return absl::StrCat("typeid: ", value.type().name());
}

// This function is identical to the C++ version in cel_policy.cc except that
// it uses __repr__ to get the string representation of the metadata values.
std::string CelPolicyDebugString(const cel::CelPolicy& policy) {
  std::string result;
  absl::StrAppend(&result, "CelPolicy{\n");
  absl::StrAppend(
      &result,
      "  ===========================================================\n");
  if (policy.source() != nullptr) {
    absl::StrAppend(&result,
                    IndentBlock(IndentBlock(policy.source()->DebugString())),
                    "\n");
  }
  absl::StrAppend(
      &result,
      "  ===========================================================\n");
  absl::StrAppend(&result, "  name: ", policy.name().DebugString(), "\n");
  if (policy.description().has_value()) {
    absl::StrAppend(
        &result, "  description: ", policy.description()->DebugString(), "\n");
  }
  if (policy.display_name().has_value()) {
    absl::StrAppend(&result,
                    "  display_name: ", policy.display_name()->DebugString(),
                    "\n");
  }
  if (!policy.metadata().empty()) {
    std::vector<std::string> sorted_keys;
    for (const auto& [key, _] : policy.metadata()) {
      sorted_keys.push_back(key);
    }
    std::sort(sorted_keys.begin(), sorted_keys.end());

    absl::StrAppend(&result, "  metadata: {\n");
    for (const auto& key : sorted_keys) {
      const auto& value = policy.metadata().at(key);
      absl::StrAppend(&result, "    ", key, ": ",
                      MetadataValueDebugString(value), "\n");
    }
    absl::StrAppend(&result, "  }\n");
  }
  if (!policy.imports().empty()) {
    absl::StrAppend(&result, "  imports:\n");
    for (const auto& import : policy.imports()) {
      absl::StrAppend(&result, "    ", import.DebugString(), "\n");
    }
  }
  absl::StrAppend(&result, IndentBlock(policy.rule().DebugString()), "\n");
  absl::StrAppend(&result, "}");
  return result;
}

}  // namespace

class CelPolicyMetadataWrapper {
 public:
  // The lifetime of CelPolicyMetadataWrapper is tied to the lifetime of the
  // CelPolicy object, see py::keep_alive in the CelPolicy bindings below.
  cel::CelPolicy* policy;

  static void DefineBindings(pybind11::module& m);
};

class PyCelPolicyValidationResult {
 public:
  // `env` is the `cel.Env` the policy was compiled with. The compiled
  // expression is built once here so that `expression()` is a cheap,
  // thread-safe lookup.
  PyCelPolicyValidationResult(cel::CelPolicyValidationResult result,
                              const py::object& env)
      : result_(std::move(result)),
        expression_(BuildExpression(result_, env)) {}

  bool IsValid() const { return result_.IsValid(); }

  absl::Span<const cel::CelPolicyIssue> GetIssues() const {
    return result_.GetIssues();
  }

  std::string FormattedIssues() const { return result_.FormatIssues(); }

  py::object GetExpression() const { return ThrowIfError(expression_); }

 private:
  static absl::StatusOr<py::object> BuildExpression(
      const cel::CelPolicyValidationResult& result, const py::object& env) {
    if (!result.IsValid() || result.GetAst() == nullptr) {
      return absl::InvalidArgumentError(result.FormatIssues());
    }
    cel::expr::CheckedExpr checked_expr;
    CEL_PYTHON_RETURN_IF_ERROR(
        cel::AstToCheckedExpr(*result.GetAst(), &checked_expr));
    // Hand the expression back to the `cel` module in serialized form:
    // generated protobuf descriptors are per shared library, so a CheckedExpr
    // message cannot be passed directly between `cel_policy` and `cel`.
    google::protobuf::Any any;
    if (!any.PackFrom(checked_expr)) {
      return absl::InternalError("Failed to pack compiled policy expression.");
    }
    return env.attr("deserialize")(py::bytes(any.SerializeAsString()));
  }

  cel::CelPolicyValidationResult result_;
  // Must be declared after `result_`, which is used to initialize it.
  absl::StatusOr<py::object> expression_;
};

void DefinePolicyBindings(pybind11::module& m) {
  CelPolicyMetadataWrapper::DefineBindings(m);

  py::class_<cel::ValueString>(m, "ValueString")
      .def(py::init<>())
      .def(py::init<cel::CelPolicyElementId, absl::string_view>())
      .def_property_readonly("id", &cel::ValueString::id)
      .def_property_readonly("value", &cel::ValueString::value)
      .def("__repr__", &cel::ValueString::DebugString);

  py::class_<cel::Import>(m, "Import")
      .def_property_readonly("id", &cel::Import::id)
      .def_property_readonly("name", &cel::Import::name)
      .def("__repr__", &cel::Import::DebugString);

  py::class_<cel::Variable>(m, "Variable")
      .def(py::init<>())
      .def_property("name", &cel::Variable::name, &cel::Variable::set_name)
      .def_property("expression", &cel::Variable::expression,
                    &cel::Variable::set_expression)
      .def_property("description", &cel::Variable::description,
                    &cel::Variable::set_description)
      .def_property("display_name", &cel::Variable::display_name,
                    &cel::Variable::set_display_name)
      .def("__repr__", &cel::Variable::DebugString);

  py::class_<cel::OutputBlock>(m, "OutputBlock")
      .def(py::init<>())
      .def(py::init<cel::ValueString, std::optional<cel::ValueString>>())
      .def_property("output", &cel::OutputBlock::output,
                    &cel::OutputBlock::set_output)
      .def_property("explanation", &cel::OutputBlock::explanation,
                    &cel::OutputBlock::set_explanation)
      .def("__repr__", &cel::OutputBlock::DebugString);

  py::class_<cel::Match>(m, "Match")
      .def(py::init<>())
      .def_property("id", &cel::Match::id, &cel::Match::set_id)
      .def_property("condition", &cel::Match::condition,
                    &cel::Match::set_condition)
      .def_property_readonly("has_output_block", &cel::Match::has_output_block)
      .def_property_readonly(
          "output_block",
          [](const cel::Match& self) -> const cel::OutputBlock& {
            return self.output_block();
          },
          py::return_value_policy::reference_internal)
      .def_property_readonly("has_rule", &cel::Match::has_rule)
      .def_property_readonly(
          "rule",
          [](const cel::Match& self) -> const cel::Rule& {
            return self.rule();
          },
          py::return_value_policy::reference_internal)
      .def("set_result",
           py::overload_cast<cel::OutputBlock>(&cel::Match::set_result))
      .def("set_result",
           [](cel::Match& self, const cel::Rule& rule) {
             self.set_result(std::make_unique<cel::Rule>(rule));
           })
      .def("__repr__", &cel::Match::DebugString);

  py::class_<cel::Rule>(m, "Rule")
      .def(py::init<>())
      .def_property("id", &cel::Rule::id, &cel::Rule::set_id)
      .def_property("rule_id", &cel::Rule::rule_id, &cel::Rule::set_rule_id)
      .def_property("description", &cel::Rule::description,
                    &cel::Rule::set_description)
      .def_property(
          "variables", &cel::Rule::variables,
          [](cel::Rule& self, const std::vector<cel::Variable>& vars) {
            self.mutable_variables() = vars;
          })
      .def_property(
          "matches", &cel::Rule::matches,
          [](cel::Rule& self, const std::vector<cel::Match>& matches) {
            self.mutable_matches() = matches;
          })
      .def_property("is_aggregate", &cel::Rule::is_aggregate,
                    &cel::Rule::set_is_aggregate)
      .def("__repr__", &cel::Rule::DebugString);

  py::class_<cel::CelPolicy>(m, "CelPolicy")
      .def_property("name", &cel::CelPolicy::name, &cel::CelPolicy::set_name)
      .def_property("description", &cel::CelPolicy::description,
                    &cel::CelPolicy::set_description)
      .def_property("display_name", &cel::CelPolicy::display_name,
                    &cel::CelPolicy::set_display_name)
      .def_property(
          "imports", &cel::CelPolicy::imports,
          [](cel::CelPolicy& self, const std::vector<cel::Import>& imports) {
            self.mutable_imports() = imports;
          })
      .def_property("rule", &cel::CelPolicy::rule,
                    [](cel::CelPolicy& self, const cel::Rule& rule) {
                      self.mutable_rule() = rule;
                    })
      .def_property_readonly("metadata",
                             py::cpp_function(
                                 [](cel::CelPolicy& self) {
                                   return CelPolicyMetadataWrapper{&self};
                                 },
                                 // Keep the CelPolicy alive as long as the
                                 // CelPolicyMetadataWrapper is alive. 0 is
                                 // self, 1 is the CelPolicyMetadataWrapper.
                                 py::keep_alive<0, 1>()))
      .def("__repr__", &CelPolicyDebugString);

  py::class_<cel::CelPolicySource, std::shared_ptr<cel::CelPolicySource>>(
      m, "CelPolicySource")
      .def("__repr__", &cel::CelPolicySource::DebugString);

  py::class_<cel::CelPolicyIssue>(m, "CelPolicyIssue")
      .def("to_display_string", [](const cel::CelPolicyIssue& self,
                                   const cel::CelPolicySource& src) {
        // resolve ambiguous overload
        return self.ToDisplayString(src);
      });

  py::class_<cel::CelPolicyParseResult>(m, "CelPolicyParseResult")
      .def_property_readonly("is_valid", &cel::CelPolicyParseResult::IsValid)
      .def("policy", &cel::CelPolicyParseResult::GetPolicy,
           py::return_value_policy::reference_internal)
      .def("issues", &cel::CelPolicyParseResult::GetIssues)
      .def("formatted_issues", &cel::CelPolicyParseResult::FormattedIssues);

  py::class_<PyCelPolicyValidationResult>(m, "CelPolicyValidationResult")
      .def_property_readonly("is_valid", &PyCelPolicyValidationResult::IsValid)
      .def("expression", &PyCelPolicyValidationResult::GetExpression)
      .def("issues", &PyCelPolicyValidationResult::GetIssues)
      .def("formatted_issues", &PyCelPolicyValidationResult::FormattedIssues);

  m.def(
      "compile",
      [](py::object env, const cel::CelPolicy& policy,
         std::optional<py::dict> options_dict) -> PyCelPolicyValidationResult {
        std::shared_ptr<const cel::Compiler> compiler = GetCelCompiler(env);
        cel::CompilePolicyOptions options;
        if (options_dict) {
          if (options_dict->contains("unnesting_height_limit")) {
            options.unnesting_height_limit =
                (*options_dict)["unnesting_height_limit"].cast<int>();
          }
        }
        cel::CelPolicyValidationResult result = [&]() {
          py::gil_scoped_release gil_release;
          return ThrowIfError(cel::CompilePolicy(*compiler, policy, options));
        }();
        return PyCelPolicyValidationResult(std::move(result), env);
      },
      py::arg("env"), py::arg("policy"), py::arg("options") = py::none());
}

void CelPolicyMetadataWrapper::DefineBindings(pybind11::module& m) {
  py::class_<CelPolicyMetadataWrapper>(m, "CelPolicyMetadata")
      .def("__getitem__",
           [](CelPolicyMetadataWrapper& self, const std::string& key) {
             auto it = self.policy->metadata().find(key);
             if (it == self.policy->metadata().end()) {
               throw py::key_error(key);
             }
             if (it->second.type() == typeid(py::object)) {
               return std::any_cast<py::object>(it->second);
             }
             return py::none().cast<py::object>();
           })
      .def("__setitem__",
           [](CelPolicyMetadataWrapper& self, const std::string& key,
              py::object value) {
             self.policy->mutable_metadata()[key] = value;
           })
      .def("__contains__",
           [](CelPolicyMetadataWrapper& self, const std::string& key) {
             return self.policy->metadata().contains(key);
           })
      .def("__len__",
           [](CelPolicyMetadataWrapper& self) {
             return self.policy->metadata().size();
           })
      .def(
          "__iter__",
          [](CelPolicyMetadataWrapper& self) {
            return py::make_key_iterator(self.policy->metadata().begin(),
                                         self.policy->metadata().end());
          },
          py::keep_alive<0, 1>())
      .def("keys",
           [](CelPolicyMetadataWrapper& self) {
             py::list l;
             for (const auto& item : self.policy->metadata()) {
               l.append(item.first);
             }
             return l;
           })
      .def("values",
           [](CelPolicyMetadataWrapper& self) {
             py::list l;
             for (const auto& item : self.policy->metadata()) {
               if (item.second.type() == typeid(py::object)) {
                 l.append(std::any_cast<py::object>(item.second));
               } else {
                 l.append(py::none());
               }
             }
             return l;
           })
      .def("items", [](CelPolicyMetadataWrapper& self) {
        py::list l;
        for (const auto& item : self.policy->metadata()) {
          py::object val = py::none();
          if (item.second.type() == typeid(py::object)) {
            val = std::any_cast<py::object>(item.second);
          }
          l.append(py::make_tuple(item.first, val));
        }
        return l;
      });
}

}  // namespace cel_python
