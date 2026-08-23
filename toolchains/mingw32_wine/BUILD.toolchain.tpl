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
        "mingw32/**",
        "libexec/**",
    ]),
)

filegroup(
    name = "compiler_files",
    srcs = glob([
        "bin/**",
        "lib/**",
        "include/**",
        "wrappers/**",
        "mingw32/**",
        "libexec/**",
    ]),
)

filegroup(
    name = "linker_files",
    srcs = glob([
        "bin/**",
        "lib/**",
        "wrappers/**",
        "mingw32/**",
    ]),
)

filegroup(
    name = "ar_files",
    srcs = glob([
        "bin/**",
        "wrappers/**",
    ]),
)

filegroup(
    name = "empty",
    srcs = [],
)

filegroup(
    name = "windres_bin",
    srcs = ["wrappers/windres"],
)

filegroup(
    name = "msys_files",
    srcs = glob([
        "buildenv/**",
        "msys_dist/**",
    ]),
)

filegroup(
    name = "tcltk_dist_files",
    srcs = glob([
        "tcltk_dist/**",
    ]),
)

cc_toolchain_config(
    name = "mingw32_wine_toolchain_config",
    toolchain_repo = "%{TOOLCHAIN_REPO_NAME}",
)

cc_toolchain(
    name = "cc_toolchain_mingw32_wine",
    all_files = ":all_files",
    ar_files = ":ar_files",
    compiler_files = ":compiler_files",
    dwp_files = ":empty",
    linker_files = ":linker_files",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 0,
    toolchain_config = ":mingw32_wine_toolchain_config",
    toolchain_identifier = "mingw32-wine-toolchain",
)

toolchain(
    name = "mingw32_wine_cc_toolchain",
    exec_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    target_compatible_with = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_32",
    ],
    toolchain = ":cc_toolchain_mingw32_wine",
    toolchain_type = "@rules_cc//cc:toolchain_type",
)

