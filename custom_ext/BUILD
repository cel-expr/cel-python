load("@pybind11_bazel//:build_defs.bzl", "pybind_extension")
load("@rules_python//python:defs.bzl", "py_test")
load("@rules_python//python:py_library.bzl", "py_library")

package(default_visibility = ["//visibility:private"])

pybind_extension(
    name = "sample_cel_ext_cc",
    srcs = ["sample_cel_ext.cc"],
    data = [
        "//cel_expr_python:cel",
    ],
    deps = [
        "//cel_expr_python:cel_extension",
        "//cel_expr_python:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_cel_cpp//checker:type_checker_builder",
        "@com_google_cel_cpp//common:decl",
        "@com_google_cel_cpp//common:type",
        "@com_google_cel_cpp//common:value",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//runtime:function_adapter",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_protobuf//:protobuf",
    ],
)

py_library(
    name = "sample_cel_ext_py",
    srcs = ["sample_cel_ext.py"],
    deps = [
        "//cel_expr_python:cel",
    ],
)

py_test(
    name = "custom_ext_test",
    srcs = ["custom_ext_test.py"],
    deps = [
        ":sample_cel_ext_cc",
        ":sample_cel_ext_py",
        "//cel_expr_python:cel",
        "@com_google_absl_py//absl/testing:absltest",
        "@com_google_absl_py//absl/testing:parameterized",
        "@com_google_protobuf//:protobuf_python",
    ],
)
