load("@rules_python//python:py_test.bzl", "py_test")

package(default_visibility = ["//visibility:private"])

_ALL_TESTS = [
    "@cel_conformance//tests/simple:testdata/basic.textproto",
    "@cel_conformance//tests/simple:testdata/bindings_ext.textproto",
    "@cel_conformance//tests/simple:testdata/comparisons.textproto",
    "@cel_conformance//tests/simple:testdata/conversions.textproto",
    "@cel_conformance//tests/simple:testdata/dynamic.textproto",
    "@cel_conformance//tests/simple:testdata/encoders_ext.textproto",
    "@cel_conformance//tests/simple:testdata/enums.textproto",
    "@cel_conformance//tests/simple:testdata/fields.textproto",
    "@cel_conformance//tests/simple:testdata/fp_math.textproto",
    "@cel_conformance//tests/simple:testdata/integer_math.textproto",
    "@cel_conformance//tests/simple:testdata/lists.textproto",
    "@cel_conformance//tests/simple:testdata/lists_ext.textproto",
    "@cel_conformance//tests/simple:testdata/logic.textproto",
    "@cel_conformance//tests/simple:testdata/macros.textproto",
    "@cel_conformance//tests/simple:testdata/math_ext.textproto",
    "@cel_conformance//tests/simple:testdata/namespace.textproto",
    "@cel_conformance//tests/simple:testdata/optionals.textproto",
    "@cel_conformance//tests/simple:testdata/parse.textproto",
    "@cel_conformance//tests/simple:testdata/plumbing.textproto",
    "@cel_conformance//tests/simple:testdata/proto2.textproto",
    "@cel_conformance//tests/simple:testdata/proto3.textproto",
    "@cel_conformance//tests/simple:testdata/proto2_ext.textproto",
    "@cel_conformance//tests/simple:testdata/string.textproto",
    "@cel_conformance//tests/simple:testdata/string_ext.textproto",
    "@cel_conformance//tests/simple:testdata/timestamps.textproto",
    "@cel_conformance//tests/simple:testdata/type_deduction.textproto",
    "@cel_conformance//tests/simple:testdata/unknowns.textproto",
    "@cel_conformance//tests/simple:testdata/wrappers.textproto",
]

py_test(
    name = "conformance_test",
    srcs = ["conformance_test.py"],
    data = _ALL_TESTS + [
        "//cel_expr_python:cel",
        "//cel_expr_python/ext:ext_bindings",
        "//cel_expr_python/ext:ext_encoders",
        "//cel_expr_python/ext:ext_lists",
        "//cel_expr_python/ext:ext_math",
        "//cel_expr_python/ext:ext_optional",
        "//cel_expr_python/ext:ext_proto",
        "//cel_expr_python/ext:ext_strings",
    ],
    env = {
        "TEST_FILES": ",".join(["$(rlocationpath %s)" % t for t in _ALL_TESTS]),
    },
    deps = [
        "//testing:proto2_test_all_types_py_pb2",
        "//testing:proto3_test_all_types_py_pb2",
        "//testing:simple_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
        "@com_google_cel_spec//proto/cel/expr:checked_py_pb2",
        "@com_google_cel_spec//proto/cel/expr:value_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        "@rules_python//python/runfiles",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            "//cel_expr_python:cel",
            "//cel_expr_python/ext:ext_bindings",
            "//cel_expr_python/ext:ext_encoders",
            "//cel_expr_python/ext:ext_lists",
            "//cel_expr_python/ext:ext_math",
            "//cel_expr_python/ext:ext_optional",
            "//cel_expr_python/ext:ext_proto",
            "//cel_expr_python/ext:ext_strings",
        ],
    }),
)
