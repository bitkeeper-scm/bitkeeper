load("//toolchains/mingw32_wine:repo.bzl", "mingw32_toolchain_repo")

def _mingw32_wine_extension_impl(ctx):
    mingw32_toolchain_repo(
        name = "mingw32_wine_toolchain",
    )

mingw32_wine_extension = module_extension(
    implementation = _mingw32_wine_extension_impl,
)

