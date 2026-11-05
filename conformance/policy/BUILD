load("@rules_python//python:py_test.bzl", "py_test")

package(default_visibility = ["//visibility:private"])

py_test(
    name = "policy_conformance_test",
    srcs = ["policy_conformance_test.py"],
    data = [
        "//cel_expr_python:cel",
        "//cel_expr_python/ext:ext_bindings",
        "//cel_expr_python/ext:ext_optional",
        "//cel_expr_python/policy:cel_policy",
        "@cel_policy//conformance:testdata",
    ],
    env = {
        "TESTDATA_DIR": "+_repo_rules+cel_policy/conformance/testdata/",
        "TEST_FILES": "$(rlocationpaths @cel_policy//conformance:testdata)",
    },
    deps = [
        "//testing:proto3_test_all_types_py_pb2",
        "//testing:suite_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
        "@com_google_cel_spec//proto/cel/expr:checked_py_pb2",
        "@com_google_cel_spec//proto/cel/expr:value_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        "@pyyaml//:yaml",
        "@rules_python//python/runfiles",
    ] + select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            "//cel_expr_python:cel",
            "//cel_expr_python/ext:ext_bindings",
            "//cel_expr_python/ext:ext_optional",
            "//cel_expr_python/policy:cel_policy",
        ],
    }),
)
