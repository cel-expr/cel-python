def _custom_headers_repo_impl(ctx):
    # Resolve label to path
    build_file_path = ctx.path(ctx.attr.build_file_label)
    repo_dir = build_file_path.dirname

    # Symlink the include directory
    ctx.symlink(repo_dir.get_child("include"), "include")

    # Create BUILD file
    ctx.file("BUILD.bazel", """
cc_library(
    name = "headers",
    hdrs = glob(["include/python3.11/**"]),
    includes = ["include/python3.11"],
    visibility = ["//visibility:public"],
)
""")

custom_headers_repo = repository_rule(
    implementation = _custom_headers_repo_impl,
    attrs = {
        "build_file_label": attr.label(mandatory = True),
    },
)

def _local_repo_extension_impl(ctx):
    custom_headers_repo(
        name = "python_headers_custom",
        build_file_label = "@@rules_python++python+python_3_11_x86_64-unknown-linux-gnu//:BUILD.bazel",
    )

local_repo_ext = module_extension(
    implementation = _local_repo_extension_impl,
)
