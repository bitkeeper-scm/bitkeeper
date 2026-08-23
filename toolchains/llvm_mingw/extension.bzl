load("//toolchains/llvm_mingw:repo.bzl", "llvm_mingw_toolchain_repo")

def _llvm_mingw_extension_impl(ctx):
    llvm_mingw_toolchain_repo(
        name = "llvm_mingw_toolchain",
    )

llvm_mingw_extension = module_extension(
    implementation = _llvm_mingw_extension_impl,
)
