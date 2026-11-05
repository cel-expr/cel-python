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

#include "cel_expr_python/policy/py_cel_policy_yaml_parser.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/source.h"
#include "policy/cel_policy.h"
#include "policy/cel_policy_parse_context.h"
#include "policy/cel_policy_parse_result.h"
#include "policy/yaml_policy_parser.h"
#include "cel_expr_python/py_error_status.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/parse.h"
#include "yaml-cpp/yaml.h"  // IWYU pragma: keep

namespace cel_python {

namespace py = ::pybind11;

class PyCelPolicyYamlParser : public cel::YamlPolicyParser {
 public:
  using cel::YamlPolicyParser::YamlPolicyParser;

  absl::StatusOr<bool> ParsePolicyTag(cel::CelPolicyParseContext& ctx,
                                      const cel::ValueString& tag_name,
                                      const YAML::Node& node) const override {
    pybind11::gil_scoped_acquire gil;
    pybind11::function override =
        pybind11::get_override(this, "parse_policy_tag");
    if (override) {
      auto py_result = override(
          pybind11::cast(&ctx, pybind11::return_value_policy::reference),
          tag_name, node);
      return py_result.cast<bool>();
    }
    pybind11::gil_scoped_release gil_release;
    return cel::YamlPolicyParser::ParsePolicyTag(ctx, tag_name, node);
  }

  absl::StatusOr<bool> SuperParsePolicyTag(cel::CelPolicyParseContext& ctx,
                                           const cel::ValueString& tag_name,
                                           const YAML::Node& node) const {
    return cel::YamlPolicyParser::ParsePolicyTag(ctx, tag_name, node);
  }

  absl::Status ParseRule(cel::CelPolicyParseContext& ctx,
                         const YAML::Node& node,
                         cel::Rule& rule) const override {
    pybind11::gil_scoped_acquire gil;
    pybind11::function override = pybind11::get_override(this, "parse_rule");
    if (override) {
      override(pybind11::cast(&ctx, pybind11::return_value_policy::reference),
               node,
               pybind11::cast(&rule, pybind11::return_value_policy::reference));
      return absl::OkStatus();
    }
    pybind11::gil_scoped_release gil_release;
    return cel::YamlPolicyParser::ParseRule(ctx, node, rule);
  }

  absl::Status SuperParseRule(cel::CelPolicyParseContext& ctx,
                              const YAML::Node& node, cel::Rule& rule) const {
    return cel::YamlPolicyParser::ParseRule(ctx, node, rule);
  }

  absl::StatusOr<bool> ParseRuleTag(cel::CelPolicyParseContext& ctx,
                                    const cel::ValueString& tag_name,
                                    const YAML::Node& node,
                                    cel::Rule& rule) const override {
    pybind11::gil_scoped_acquire gil;
    pybind11::function override =
        pybind11::get_override(this, "parse_rule_tag");
    if (override) {
      auto py_result = override(
          pybind11::cast(&ctx, pybind11::return_value_policy::reference),
          tag_name, node,
          pybind11::cast(&rule, pybind11::return_value_policy::reference));
      return py_result.cast<bool>();
    }
    pybind11::gil_scoped_release gil_release;
    return cel::YamlPolicyParser::ParseRuleTag(ctx, tag_name, node, rule);
  }

  absl::StatusOr<bool> SuperParseRuleTag(cel::CelPolicyParseContext& ctx,
                                         const cel::ValueString& tag_name,
                                         const YAML::Node& node,
                                         cel::Rule& rule) const {
    return cel::YamlPolicyParser::ParseRuleTag(ctx, tag_name, node, rule);
  }

  absl::StatusOr<cel::Match> ParseMatch(cel::CelPolicyParseContext& ctx,
                                        const YAML::Node& node,
                                        cel::Rule& rule) const override {
    pybind11::gil_scoped_acquire gil;
    pybind11::function override = pybind11::get_override(this, "parse_match");
    if (override) {
      auto py_result = override(
          pybind11::cast(&ctx, pybind11::return_value_policy::reference), node,
          pybind11::cast(&rule, pybind11::return_value_policy::reference));
      return py_result.cast<cel::Match>();
    }
    pybind11::gil_scoped_release gil_release;
    return cel::YamlPolicyParser::ParseMatch(ctx, node, rule);
  }

  absl::StatusOr<cel::Match> SuperParseMatch(cel::CelPolicyParseContext& ctx,
                                             const YAML::Node& node,
                                             cel::Rule& rule) const {
    return cel::YamlPolicyParser::ParseMatch(ctx, node, rule);
  }

  absl::StatusOr<bool> ParseMatchTag(cel::CelPolicyParseContext& ctx,
                                     const cel::ValueString& tag_name,
                                     const YAML::Node& node, cel::Match& match,
                                     cel::Rule& rule) const override {
    pybind11::gil_scoped_acquire gil;
    pybind11::function override =
        pybind11::get_override(this, "parse_match_tag");
    if (override) {
      auto py_result = override(
          pybind11::cast(&ctx, pybind11::return_value_policy::reference),
          tag_name, node,
          pybind11::cast(&match, pybind11::return_value_policy::reference),
          pybind11::cast(&rule, pybind11::return_value_policy::reference));
      return py_result.cast<bool>();
    }
    pybind11::gil_scoped_release gil_release;
    return cel::YamlPolicyParser::ParseMatchTag(ctx, tag_name, node, match,
                                                rule);
  }

  absl::StatusOr<bool> SuperParseMatchTag(cel::CelPolicyParseContext& ctx,
                                          const cel::ValueString& tag_name,
                                          const YAML::Node& node,
                                          cel::Match& match,
                                          cel::Rule& rule) const {
    return cel::YamlPolicyParser::ParseMatchTag(ctx, tag_name, node, match,
                                                rule);
  }

  absl::StatusOr<cel::Variable> ParseVariable(cel::CelPolicyParseContext& ctx,
                                              const YAML::Node& node,
                                              cel::Rule& rule) const override {
    pybind11::gil_scoped_acquire gil;
    pybind11::function override =
        pybind11::get_override(this, "parse_variable");
    if (override) {
      auto py_result = override(
          pybind11::cast(&ctx, pybind11::return_value_policy::reference), node,
          pybind11::cast(&rule, pybind11::return_value_policy::reference));
      return py_result.cast<cel::Variable>();
    }
    pybind11::gil_scoped_release gil_release;
    return cel::YamlPolicyParser::ParseVariable(ctx, node, rule);
  }

  absl::StatusOr<cel::Variable> SuperParseVariable(
      cel::CelPolicyParseContext& ctx, const YAML::Node& node,
      cel::Rule& rule) const {
    return cel::YamlPolicyParser::ParseVariable(ctx, node, rule);
  }

  absl::StatusOr<bool> ParseVariableTag(
      cel::CelPolicyParseContext& ctx, const cel::ValueString& tag_name,
      const YAML::Node& node, cel::Variable& variable) const override {
    pybind11::gil_scoped_acquire gil;
    pybind11::function override =
        pybind11::get_override(this, "parse_variable_tag");
    if (override) {
      auto py_result = override(
          pybind11::cast(&ctx, pybind11::return_value_policy::reference),
          tag_name, node,
          pybind11::cast(&variable, pybind11::return_value_policy::reference));
      return py_result.cast<bool>();
    }
    pybind11::gil_scoped_release gil_release;
    return cel::YamlPolicyParser::ParseVariableTag(ctx, tag_name, node,
                                                   variable);
  }

  absl::StatusOr<bool> SuperParseVariableTag(cel::CelPolicyParseContext& ctx,
                                             const cel::ValueString& tag_name,
                                             const YAML::Node& node,
                                             cel::Variable& variable) const {
    return cel::YamlPolicyParser::ParseVariableTag(ctx, tag_name, node,
                                                   variable);
  }
};

// Memory safety with YAML::Node across C++ and Python boundaries is guaranteed
// by yaml-cpp's internal shared reference counting:
// 1. Shared Memory Arena: Every YAML::Node references a shared memory holder
//    (`detail::shared_memory_holder`) that manages the underlying document
//    nodes.
// 2. Automatic Reference Counting: Slices or individual node references stored
//    in Python hold a strong reference to this memory arena, preventing
//    premature deallocation even after the C++ parse session finishes.
// 3. Garbage Collection: Once all Python references are released, Python's
//    garbage collector invokes the C++ destructor, which drops the reference
//    count and safely reclaims the memory.
static void DefineYamlNodeBindings(pybind11::module& m) {
  // Force the linker to include YAML::Node destructor symbols in static
  // linkage.
  if (m.attr("__name__").cast<std::string>() == "___never___") {
    YAML::Node n = YAML::Load("[]");
  }
  py::class_<YAML::Node>(m, "YAMLNode")
      .def(py::init<>())
      .def("is_scalar", [](const YAML::Node& node) { return node.IsScalar(); })
      .def("is_sequence",
           [](const YAML::Node& node) { return node.IsSequence(); })
      .def("is_map", [](const YAML::Node& node) { return node.IsMap(); })
      .def("__len__", [](const YAML::Node& node) { return node.size(); })
      .def("scalar", [](const YAML::Node& node) { return node.Scalar(); })
      .def("__getitem__",
           [](const YAML::Node& node, int index) {
             if (index < 0 || static_cast<size_t>(index) >= node.size()) {
               throw py::index_error();
             }
             return node[index];
           })
      .def("__getitem__", [](const YAML::Node& node, const std::string& key) {
        return node[key];
      });
}

static void DefineCelPolicyParseContextBindings(pybind11::module& m) {
  py::class_<cel::CelPolicyParseContext>(m, "CelPolicyParseContext")
      .def_property_readonly(
          "policy",
          [](cel::CelPolicyParseContext& self) -> cel::CelPolicy& {
            return self.policy();
          },
          py::return_value_policy::reference_internal)
      .def("report_error",
           [](cel::CelPolicyParseContext& self, cel::CelPolicyElementId id,
              const std::string& message) { self.ReportError(id, message); })
      .def("collect_metadata",
           [](cel::CelPolicyParseContext& self, const YAML::Node& node) {
             cel::CelPolicyElementId id = self.next_element_id();
             self.policy_source().NoteSourcePosition(id, node.Mark().pos);
             return id;
           });
}

static void DefineYamlPolicyParserBindings(pybind11::module& m) {
  py::class_<cel::YamlPolicyParser, PyCelPolicyYamlParser>(
      m, "CelPolicyYamlParser")
      .def(py::init<>())
      .def("parse",
           [](const cel::YamlPolicyParser& self,
              absl::string_view yaml_content) {
             auto source = ThrowIfError(cel::NewSource(yaml_content));
             auto policy_source =
                 std::make_shared<cel::CelPolicySource>(std::move(source));
             cel::CelPolicyParseResult result = [&]() {
               py::gil_scoped_release gil_release;
               return ThrowIfError(
                   cel::ParseYamlCelPolicy(policy_source, self));
             }();
             return result;
           })
      .def(
          "parse_policy_tag",
          [](const cel::YamlPolicyParser& self, cel::CelPolicyParseContext& ctx,
             const cel::ValueString& tag_name, const YAML::Node& node) {
            return ThrowIfError(static_cast<const PyCelPolicyYamlParser&>(self)
                                    .SuperParsePolicyTag(ctx, tag_name, node));
          })
      .def(
          "parse_rule",
          [](const cel::YamlPolicyParser& self, cel::CelPolicyParseContext& ctx,
             const YAML::Node& node, cel::Rule& rule) {
            auto status =
                static_cast<const PyCelPolicyYamlParser&>(self).SuperParseRule(
                    ctx, node, rule);
            if (!status.ok()) ThrowIfError(absl::StatusOr<bool>(status));
          })
      .def("parse_rule_tag",
           [](const cel::YamlPolicyParser& self,
              cel::CelPolicyParseContext& ctx, const cel::ValueString& tag_name,
              const YAML::Node& node, cel::Rule& rule) {
             return ThrowIfError(
                 static_cast<const PyCelPolicyYamlParser&>(self)
                     .SuperParseRuleTag(ctx, tag_name, node, rule));
           })
      .def(
          "parse_match",
          [](const cel::YamlPolicyParser& self, cel::CelPolicyParseContext& ctx,
             const YAML::Node& node, cel::Rule& rule) {
            return ThrowIfError(
                static_cast<const PyCelPolicyYamlParser&>(self).SuperParseMatch(
                    ctx, node, rule));
          })
      .def("parse_match_tag",
           [](const cel::YamlPolicyParser& self,
              cel::CelPolicyParseContext& ctx, const cel::ValueString& tag_name,
              const YAML::Node& node, cel::Match& match, cel::Rule& rule) {
             return ThrowIfError(
                 static_cast<const PyCelPolicyYamlParser&>(self)
                     .SuperParseMatchTag(ctx, tag_name, node, match, rule));
           })
      .def(
          "parse_variable",
          [](const cel::YamlPolicyParser& self, cel::CelPolicyParseContext& ctx,
             const YAML::Node& node, cel::Rule& rule) {
            return ThrowIfError(static_cast<const PyCelPolicyYamlParser&>(self)
                                    .SuperParseVariable(ctx, node, rule));
          })
      .def("parse_variable_tag",
           [](const cel::YamlPolicyParser& self,
              cel::CelPolicyParseContext& ctx, const cel::ValueString& tag_name,
              const YAML::Node& node, cel::Variable& variable) {
             return ThrowIfError(
                 static_cast<const PyCelPolicyYamlParser&>(self)
                     .SuperParseVariableTag(ctx, tag_name, node, variable));
           })
      .def(
          "get_value_string",
          [](const cel::YamlPolicyParser& self, cel::CelPolicyParseContext& ctx,
             const YAML::Node& node, const std::string& error_message) {
            return self.GetValueString(ctx, node, error_message);
          });
}

void DefineCelPolicyYamlParserBindings(pybind11::module& m) {
  DefineYamlNodeBindings(m);
  DefineCelPolicyParseContextBindings(m);
  DefineYamlPolicyParserBindings(m);

  m.def(
      "parse_yaml_cel_policy",
      [](absl::string_view yaml_content, const cel::YamlPolicyParser* parser,
         std::optional<std::string> description) {
        auto source = ThrowIfError(
            cel::NewSource(yaml_content, description.value_or("<input>")));
        auto policy_source =
            std::make_shared<cel::CelPolicySource>(std::move(source));
        if (parser) {
          return ThrowIfError(cel::ParseYamlCelPolicy(policy_source, *parser));
        } else {
          return ThrowIfError(cel::ParseYamlCelPolicy(policy_source));
        }
      },
      py::arg("yaml_content"), py::arg("parser") = py::none(),
      py::arg("description") = py::none());
}

}  // namespace cel_python
