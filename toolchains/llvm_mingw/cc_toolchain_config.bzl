load(
    "@rules_cc//cc/private/toolchain_config:cc_toolchain_config_info.bzl",
    "create_cc_toolchain_config_info",
    "CcToolchainConfigInfo",
)
load(
    "@rules_cc//cc:cc_toolchain_config_lib.bzl",
    "action_config",
    "feature",
    "flag_group",
    "flag_set",
    "tool",
    "tool_path",
)
load("@rules_cc//cc:action_names.bzl", "ACTION_NAMES")

all_compile_actions = [
    ACTION_NAMES.c_compile,
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.linkstamp_compile,
    ACTION_NAMES.assemble,
    ACTION_NAMES.preprocess_assemble,
    ACTION_NAMES.cpp_header_parsing,
    ACTION_NAMES.cpp_module_compile,
    ACTION_NAMES.cpp_module_codegen,
    ACTION_NAMES.clif_match,
    ACTION_NAMES.lto_backend,
]

all_link_actions = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

def _cc_toolchain_config_impl(ctx):
    repo_name = getattr(ctx.attr, "toolchain_repo", "llvm_mingw_toolchain")
    tool_paths = [
        tool_path(name = "gcc", path = "wrappers/gcc"),
        tool_path(name = "g++", path = "wrappers/g++"),
        tool_path(name = "cpp", path = "wrappers/cpp"),
        tool_path(name = "ar", path = "wrappers/ar"),
        tool_path(name = "as", path = "wrappers/as"),
        tool_path(name = "ld", path = "wrappers/ld"),
        tool_path(name = "nm", path = "wrappers/nm"),
        tool_path(name = "objdump", path = "wrappers/objdump"),
        tool_path(name = "strip", path = "wrappers/strip"),
        tool_path(name = "gcov", path = "wrappers/gcov"),
    ]

    features = [
        feature(
        name = "default_compile_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = all_compile_actions,
                flag_groups = [
                    flag_group(
                        flags = [
                            "-no-canonical-prefixes",
                            "-Wno-incompatible-pointer-types",
                            "-Wno-int-conversion",
                        ],
                    ),
                ],
            ),
        ],
        ),
        feature(
            name = "default_link_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = all_link_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-no-canonical-prefixes",
                            ],
                        ),
                    ],
                ),
            ],
        ),
        feature(
            name = "supports_pic",
            enabled = False,
        ),
    ]

    return create_cc_toolchain_config_info(
        ctx = ctx,
        features = features,
        action_configs = [],
        artifact_name_patterns = [],
        cxx_builtin_include_directories = [
            "%crosstool_top%/include",
            "%crosstool_top%/i686-w64-mingw32/include",
            "%crosstool_top%/generic-w64-mingw32/include",
            "%crosstool_top%/lib/clang/22/include",
            "%crosstool_top%/lib/clang/22",
            "%crosstool_top%/lib/clang",
            "%crosstool_top%/lib",
        ],
        toolchain_identifier = "llvm-mingw-toolchain",
        host_system_name = "x86_64-unknown-linux-gnu",
        target_system_name = "i686-w64-windows-gnu",
        target_cpu = "x86_32",
        target_libc = "msvcrt",
        compiler = "clang",
        abi_version = "mingw",
        abi_libc_version = "msvcrt",
        tool_paths = tool_paths,
    )

cc_toolchain_config = rule(
    implementation = _cc_toolchain_config_impl,
    attrs = {
        "toolchain_repo": attr.string(default = "llvm_mingw_toolchain"),
    },
    provides = [CcToolchainConfigInfo],
)
