load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

def _cc_headers_impl(ctx):
    cc_info = ctx.attr.dep[CcInfo]
    real_headers = [h for h in cc_info.compilation_context.headers.to_list() if "_virtual_includes" not in h.path]
    return [DefaultInfo(files = depset(real_headers))]

cc_headers = rule(
    implementation = _cc_headers_impl,
    attrs = {
        "dep": attr.label(providers = [CcInfo]),
    },
)





