load("@rules_python//python:defs.bzl", "py_test")

package(default_visibility = ["//visibility:private"])

_ALL_TESTS = [
    "@com_google_cel_spec//tests/simple:testdata/basic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/bindings_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/comparisons.textproto",
    "@com_google_cel_spec//tests/simple:testdata/conversions.textproto",
    "@com_google_cel_spec//tests/simple:testdata/dynamic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/encoders_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/enums.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fields.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fp_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/integer_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/lists.textproto",
    "@com_google_cel_spec//tests/simple:testdata/logic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/macros.textproto",
    "@com_google_cel_spec//tests/simple:testdata/math_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/namespace.textproto",
    "@com_google_cel_spec//tests/simple:testdata/optionals.textproto",
    "@com_google_cel_spec//tests/simple:testdata/parse.textproto",
    "@com_google_cel_spec//tests/simple:testdata/plumbing.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto2.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto3.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto2_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/timestamps.textproto",
    "@com_google_cel_spec//tests/simple:testdata/type_deduction.textproto",
    "@com_google_cel_spec//tests/simple:testdata/unknowns.textproto",
    "@com_google_cel_spec//tests/simple:testdata/wrappers.textproto",
]

py_test(
    name = "conformance_test",
    srcs = ["conformance_test.py"],
    data = _ALL_TESTS,
    deps = [
        "@com_google_cel_spec//proto/cel/expr:checked_py_pb2",
        "@com_google_cel_spec//proto/cel/expr:value_py_pb2",
        "//:py_cel",
        "//ext:ext_bindings",
        "//ext:ext_encoders",
        "//ext:ext_math",
        "//ext:ext_optional",
        "//ext:ext_proto",
        "//ext:ext_string",
        "@com_google_absl_py//absl/testing:absltest",
        "//testing:proto2_test_all_types_py_pb2",
        "//testing:proto3_test_all_types_py_pb2",
        "//testing:simple_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        "@rules_python//python/runfiles",

        #        "//third_party/py/google/protobuf:use_fast_cpp_protos",  # Automatically added go/proto_python_upb_flip
    ],
)
