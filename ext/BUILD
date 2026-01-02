load("@pybind11_bazel//:build_defs.bzl", "pybind_extension")

package(default_visibility = ["//visibility:private"])

pybind_extension(
    name = "ext_bindings",
    srcs = [
        "ext_bindings.cc",
    ],
    data = [
        "//:py_cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//:py_cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:bindings_ext",
        "@com_google_protobuf//:protobuf",
    ],
)

pybind_extension(
    name = "ext_encoders",
    srcs = [
        "ext_encoders.cc",
    ],
    data = [
        "//:py_cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//:py_cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//checker:type_checker_builder",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:encoders",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_protobuf//:protobuf",
    ],
)

pybind_extension(
    name = "ext_math",
    srcs = [
        "ext_math.cc",
    ],
    data = [
        "//:py_cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//:py_cel_extension",
        "//:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_cpp//checker:type_checker_builder",
        "@com_google_cel_cpp//common:decl",
        "@com_google_cel_cpp//common:function_descriptor",
        "@com_google_cel_cpp//common:kind",
        "@com_google_cel_cpp//common:type",
        "@com_google_cel_cpp//common:value",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:math_ext",
        "@com_google_cel_cpp//extensions:math_ext_decls",
        "@com_google_cel_cpp//runtime:function",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

pybind_extension(
    name = "ext_optional",
    srcs = [
        "ext_optional.cc",
    ],
    data = [
        "//:py_cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//:py_cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//compiler:optional",
        "@com_google_cel_cpp//runtime:optional_types",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_protobuf//:protobuf",
    ],
)

pybind_extension(
    name = "ext_proto",
    srcs = [
        "ext_proto.cc",
    ],
    data = [
        "//:py_cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//:py_cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:proto_ext",
        "@com_google_protobuf//:protobuf",
    ],
)

pybind_extension(
    name = "ext_string",
    srcs = [
        "ext_string.cc",
    ],
    data = [
        "//:py_cel",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//:py_cel_extension",
        "@com_google_absl//absl/status",
        "@com_google_cel_cpp//compiler",
        "@com_google_cel_cpp//extensions:strings",
        "@com_google_cel_cpp//runtime:runtime_builder",
        "@com_google_cel_cpp//runtime:runtime_options",
        "@com_google_protobuf//:protobuf",
    ],
)
