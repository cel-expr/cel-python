load("@pybind11_bazel//:build_defs.bzl", "pybind_extension", "pybind_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_python//python:py_test.bzl", "py_test")

package(default_visibility = ["//visibility:private"])

licenses(["notice"])

exports_files(["LICENSE"])

package_group(
    name = "friends",
    includes = [
        ":internal",
    ],
    packages = [
        "//ads/adsml/steelmill/cello/...",
        "//assistant/quality/gia_retrospec/...",
        "//cloud/ai/dialogflow/foodai/ordering/...",
        "//cloud/ai/platform/orcas/groot/...",
        "//cloud/capacity/delos/...",
        "//cloud/databases/console/adk/surface_agents/database_o11y_agent/...",
        "//cloud/duet/cloudassist/event_router/...",
        "//cloud/serverless/genai/...",
        "//communication/rtctools/pcm/core/...",
        "//corp/legal/removals/...",
        "//enterprise/crm/support/intelligence/auto_eval/...",
        "//enterprise/gcbp/genai/agent_sentinel/...",
        "//evaluation/autoraters/wrapper/...",
        "//finance_org/cloud_transformation/audit_bugs/approval_service_integration/...",
        "//gdm/science/verified_agents/policy_dsl/...",
        "//geo/engprod/georails/...",
        "//java/com/google/apps/mcp/testing/...",
        "//java/com/google/cloud/revenue/revrec/...",
        "//java/com/google/maps/trust/firetruck/...",
        "//learning/agents/orcas/framework/...",
        "//learning/tfx/users/gpay_team/tfx_v2/tflex/finserv/consumer/dre/...",
        "//ops/security/ai/agentic/...",
        "//ops/security/investigations/vimes/...",
        "//ops/security/vulncc/agents/vulnact/...",
        "//ux/research/kagi/agent/...",
        "//wireless/android/pixel/testtools/stance/...",
        "//wireless/android/pixel/testtools/systemhealth/...",
    ],
)

# For Python programs using CEL.
pybind_library(
    name = "cel_pybind_lib",
    srcs = [
        "py_cel_activation.cc",
        "py_cel_arena.cc",
        "py_cel_env.cc",
        "py_cel_env_config.cc",
        "py_cel_env_internal.cc",
        "py_cel_expression.cc",
        "py_cel_function.cc",
        "py_cel_function_decl.cc",
        "py_cel_options.cc",
        "py_cel_overload.cc",
        "py_cel_python_extension.cc",
        "py_cel_type.cc",
        "py_cel_value.cc",
        "py_descriptor_database.cc",
        "py_message_factory.cc",
    ],
    hdrs = [
        "free_threading_mutex.h",
        "py_cel_activation.h",
        "py_cel_arena.h",
        "py_cel_env.h",
        "py_cel_env_config.h",
        "py_cel_env_internal.h",
        "py_cel_expression.h",
        "py_cel_function.h",
        "py_cel_function_decl.h",
        "py_cel_options.h",
        "py_cel_overload.h",
        "py_cel_python_extension.h",
        "py_cel_type.h",
        "py_cel_value.h",
        "py_cel_value_provider.h",
        "py_descriptor_database.h",
        "py_message_factory.h",
    ],
    tags = ["freethreading_compatible"],
    visibility = [":__subpackages__"],
    deps = [
        ":cel_extension",
        ":status_macros",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_cpp//checker:type_checker_builder",
        "@com_google_cel_cpp//checker:validation_result",
        "@com_google_cel_cpp//common:ast",
        "@com_google_cel_cpp//common:ast_proto",
        "@com_google_cel_cpp//common:container",
        "@com_google_cel_cpp//common:decl",
        "@com_google_cel_cpp//common:function_descriptor",
        "@com_google_cel_cpp//common:kind",
        "@com_google_cel_cpp//common:minimal_descriptor_pool",
        "@com_google_cel_cpp//common:signature",
        "@com_google_cel_cpp//common:source",
        "@com_google_cel_cpp//common:type",
        "@com_google_cel_cpp//common:type_kind",
        "@com_google_cel_cpp//common:value",
        "@com_google_cel_cpp//common:value_kind",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//env",
        "@com_google_cel_cpp//env:config",
        "@com_google_cel_cpp//env:env_runtime",
        "@com_google_cel_cpp//env:env_std_extensions",
        "@com_google_cel_cpp//env:env_yaml",
        "@com_google_cel_cpp//env:runtime_std_extensions",
        "@com_google_cel_cpp//extensions/protobuf:runtime_adapter",
        "@com_google_cel_cpp//parser:parser_interface",
        "@com_google_cel_cpp//runtime",
        "@com_google_cel_cpp//runtime:activation",
        "@com_google_cel_cpp//runtime:embedder_context",
        "@com_google_cel_cpp//runtime:function",
        "@com_google_cel_cpp//runtime:reference_resolver",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@pybind11_abseil//pybind11_abseil:absl_casters",
    ],
)

pybind_extension(
    name = "cel",
    srcs = [
        "py_cel_module.cc",
    ],
    data = ["cel.pyi"],
    visibility = [
        ":friends",
    ],
    deps = [
        ":cel_extension",
        ":cel_pybind_lib",
    ],
)

# For pybind11-based CEL extensions.
pybind_library(
    name = "cel_extension",
    srcs = [
        "py_error_status.cc",
    ],
    hdrs = [
        "cel_extension.h",
        "py_error_status.h",
    ],
    tags = ["freethreading_compatible"],
    visibility = [":friends"],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
    visibility = [":friends"],
    deps = [
        "@com_google_absl//absl/status",
    ],
)

py_test(
    name = "cel_test",
    srcs = ["cel_test.py"],
    data = [
        ":cel",
    ],
    deps = [
        "//testing:proto2_test_all_types_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:protobuf_python",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [":cel"],
    }),
)

py_test(
    name = "cel_parallel_test",
    srcs = ["cel_parallel_test.py"],
    data = [
        ":cel",
    ],
    deps = [
        "//testing:proto2_test_all_types_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [":cel"],
    }),
)

py_test(
    name = "cel_env_test",
    srcs = ["cel_env_test.py"],
    data = [
        ":cel",
        "//cel_expr_python/ext:ext_bindings",
        "//cel_expr_python/ext:ext_lists",
        "//cel_expr_python/ext:ext_math",
        "//cel_expr_python/ext:ext_optional",
        "//cel_expr_python/ext:ext_strings",
    ],
    deps = [
        "//testing:proto2_test_all_types_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            ":cel",
            "//cel_expr_python/ext:ext_bindings",
            "//cel_expr_python/ext:ext_lists",
            "//cel_expr_python/ext:ext_math",
            "//cel_expr_python/ext:ext_optional",
            "//cel_expr_python/ext:ext_strings",
        ],
    }),
)

# Platform definition for macOS x86_64.
# Used for cross-compiling from arm64 macOS hosts (Apple Silicon) to x86_64.
# This is passed to Bazel via --platforms in setup.py when building x86_64 wheels.
platform(
    name = "macos_x86_64",
    constraint_values = [
        "@platforms//os:osx",
        "@platforms//cpu:x86_64",
    ],
    visibility = ["//visibility:public"],
)
