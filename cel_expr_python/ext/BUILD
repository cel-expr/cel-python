load("@pybind11_bazel//:build_defs.bzl", "pybind_extension")

package(default_visibility = ["//visibility:private"])

pybind_extension(
    name = "ext_bindings",
    srcs = [
        "ext_bindings.cc",
    ],
    data = [
        "//cel_expr_python:cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cel_expr_python:cel_extension",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:bindings_ext",
    ],
)

pybind_extension(
    name = "ext_encoders",
    srcs = [
        "ext_encoders.cc",
    ],
    data = [
        "//cel_expr_python:cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cel_expr_python:cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:encoders",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
    ],
)

pybind_extension(
    name = "ext_math",
    srcs = [
        "ext_math.cc",
    ],
    data = [
        "//cel_expr_python:cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cel_expr_python:cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:math_ext",
        "@com_google_cel_cpp//extensions:math_ext_decls",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
    ],
)

pybind_extension(
    name = "ext_optional",
    srcs = [
        "ext_optional.cc",
    ],
    data = [
        "//cel_expr_python:cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cel_expr_python:cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//compiler:optional",
        "@com_google_cel_cpp//runtime:optional_types",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
    ],
)

pybind_extension(
    name = "ext_proto",
    srcs = [
        "ext_proto.cc",
    ],
    data = [
        "//cel_expr_python:cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cel_expr_python:cel_extension",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:proto_ext",
    ],
)

pybind_extension(
    name = "ext_string",
    srcs = [
        "ext_string.cc",
    ],
    data = [
        "//cel_expr_python:cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//cel_expr_python:cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:strings",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
    ],
)
