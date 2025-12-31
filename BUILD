load("@pybind11_bazel//:build_defs.bzl", "pybind_extension", "pybind_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_python//python:defs.bzl", "py_test")

package(default_visibility = ["//visibility:private"])

licenses(["notice"])

exports_files(["LICENSE"])

# For Python programs using CEL.
pybind_extension(
    name = "py_cel",
    srcs = [
        "py_cel.cc",
        "py_cel.h",
        "py_cel_activation.cc",
        "py_cel_activation.h",
        "py_cel_arena.cc",
        "py_cel_arena.h",
        "py_cel_env.cc",
        "py_cel_env.h",
        "py_cel_expression.cc",
        "py_cel_expression.h",
        "py_cel_function.cc",
        "py_cel_function.h",
        "py_cel_function_decl.cc",
        "py_cel_function_decl.h",
        "py_cel_module.cc",
        "py_cel_overload.cc",
        "py_cel_overload.h",
        "py_cel_python_extension.cc",
        "py_cel_python_extension.h",
        "py_cel_type.cc",
        "py_cel_type.h",
        "py_cel_value.cc",
        "py_cel_value.h",
        "py_cel_value_provider.h",
        "py_descriptor_database.cc",
        "py_descriptor_database.h",
        "py_error_status.cc",
        "py_error_status.h",
        "py_message_factory.cc",
        "py_message_factory.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":py_cel_extension",
        ":status_macros",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_cpp//checker:type_checker_builder",
        "@com_google_cel_cpp//checker:validation_result",
        "@com_google_cel_cpp//common:ast",
        "@com_google_cel_cpp//common:ast_proto",
        "@com_google_cel_cpp//common:decl",
        "@com_google_cel_cpp//common:function_descriptor",
        "@com_google_cel_cpp//common:kind",
        "@com_google_cel_cpp//common:minimal_descriptor_pool",
        "@com_google_cel_cpp//common:source",
        "@com_google_cel_cpp//common:type",
        "@com_google_cel_cpp//common:type_kind",
        "@com_google_cel_cpp//common:value",
        "@com_google_cel_cpp//common:value_kind",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//compiler:compiler_factory",
        "@com_google_cel_cpp//compiler:standard_library",
        "@com_google_cel_cpp//extensions/protobuf:runtime_adapter",
        "@com_google_cel_cpp//parser:parser_interface",
        "@com_google_cel_cpp//runtime",
        "@com_google_cel_cpp//runtime:activation",
        "@com_google_cel_cpp//runtime:function",
        "@com_google_cel_cpp//runtime:reference_resolver",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_cel_cpp//runtime:standard_runtime_builder_factory",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@pybind11_abseil//pybind11_abseil:absl_casters",
        "@pybind11_abseil//pybind11_abseil:import_status_module",
        "@pybind11_abseil//pybind11_abseil:status_casters",
    ],
)

# For pybind11-based CEL extensions.
pybind_library(
    name = "py_cel_extension",
    hdrs = ["py_cel_extension.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status",
    ],
)

py_test(
    name = "py_cel_test",
    srcs = ["py_cel_test.py"],
    deps = [
        ":py_cel",
        "//testing:proto2_test_all_types_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
        "@com_google_protobuf//:protobuf",
    ],
)
