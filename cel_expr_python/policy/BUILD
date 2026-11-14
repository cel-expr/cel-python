load("@pybind11_bazel//:build_defs.bzl", "pybind_extension")
load("@rules_python//python:py_test.bzl", "py_test")

package(default_visibility = ["//visibility:public"])

pybind_extension(
    name = "cel_policy",
    srcs = [
        "py_cel_policy.cc",
        "py_cel_policy.h",
        "py_cel_policy_module.cc",
        "py_cel_policy_yaml_parser.cc",
        "py_cel_policy_yaml_parser.h",
    ],
    data = [
        "cel_policy.pyi",
        "//cel_expr_python:cel",
    ],
    deps = [
        "//cel_expr_python:cel_compiler_capsule",
        "//cel_expr_python:cel_extension",
        "//cel_expr_python:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_cpp//common:ast_proto",
        "@com_google_cel_cpp//common:source",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//policy:cel_policy",
        "@com_google_cel_cpp//policy:cel_policy_parser",
        "@com_google_cel_cpp//policy:cel_policy_validation_result",
        "@com_google_cel_cpp//policy:compiler",
        "@com_google_cel_cpp//policy:yaml_policy_parser",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@yaml-cpp",
    ],
)

py_test(
    name = "cel_policy_yaml_test",
    srcs = ["cel_policy_yaml_test.py"],
    data = [
        "testdata/cel_policy.yaml",
        "testdata/cel_policy_invalid.yaml",
        ":cel_policy",
        "//cel_expr_python:cel",
        "//cel_expr_python/ext:ext_bindings",
        "//cel_expr_python/ext:ext_optional",
    ],
    main = "cel_policy_yaml_test.py",
    deps = [
        "//testing:proto3_test_all_types_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
        "@rules_python//python/runfiles",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            ":cel_policy",
            "//cel_expr_python:cel",
            "//cel_expr_python/ext:ext_bindings",
            "//cel_expr_python/ext:ext_optional",
        ],
    }),
)

py_test(
    name = "custom_policy_parser_test",
    srcs = ["custom_policy_parser_test.py"],
    data = [
        "testdata/custom_policy_format.yaml",
        "testdata/custom_policy_format_with_errors.yaml",
        ":cel_policy",
        "//cel_expr_python:cel",
    ],
    main = "custom_policy_parser_test.py",
    deps = [
        "@com_google_absl_py//absl/testing:absltest",
        "@rules_python//python/runfiles",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            ":cel_policy",
            "//cel_expr_python:cel",
        ],
    }),
)
