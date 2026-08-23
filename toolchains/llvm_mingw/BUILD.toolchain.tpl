load("@rules_cc//cc:defs.bzl", "cc_toolchain")
load(":cc_toolchain_config.bzl", "cc_toolchain_config")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all_files",
    srcs = glob([
        "bin/**",
        "lib/**",
        "include/**",
        "wrappers/**",
        "i686-w64-mingw32/**",
        "generic-w64-mingw32/**",
        "share/**",
    ], allow_empty = True),
)

filegroup(
    name = "compiler_files",
    srcs = glob([
        "bin/**",
        "lib/**",
        "include/**",
        "wrappers/**",
        "i686-w64-mingw32/**",
        "generic-w64-mingw32/**",
    ], allow_empty = True),
)

filegroup(
    name = "linker_files",
    srcs = glob([
        "bin/**",
        "lib/**",
        "wrappers/**",
        "i686-w64-mingw32/**",
        "generic-w64-mingw32/**",
    ], allow_empty = True),
)

filegroup(
    name = "ar_files",
    srcs = glob([
        "bin/**",
        "wrappers/**",
    ], allow_empty = True),
)

filegroup(
    name = "empty",
    srcs = [],
)

filegroup(
    name = "windres_bin",
    srcs = ["wrappers/windres"],
)

cc_toolchain_config(
    name = "llvm_mingw_toolchain_config",
    toolchain_repo = "%{TOOLCHAIN_REPO_NAME}",
)

cc_toolchain(
    name = "cc_toolchain_llvm_mingw",
    all_files = ":all_files",
    ar_files = ":ar_files",
    compiler_files = ":compiler_files",
    dwp_files = ":empty",
    linker_files = ":linker_files",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 0,
    toolchain_config = ":llvm_mingw_toolchain_config",
    toolchain_identifier = "llvm-mingw-toolchain",
)

toolchain(
    name = "llvm_mingw_cc_toolchain",
    exec_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    target_compatible_with = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_32",
    ],
    toolchain = ":cc_toolchain_llvm_mingw",
    toolchain_type = "@rules_cc//cc:toolchain_type",
)
