# Test Size & Platform Annotation Guide

Bazel test attributes like `size` and `timeout` are **non-configurable** in Starlark (they cannot use `select()` in `BUILD.bazel`). Because BUILD files cannot read file contents during the loading phase, header annotations in test scripts (e.g. `src/t/t.*`) can be parsed dynamically at workspace evaluation time using a Bazel repository rule.

---

## 1. Test Script Header Annotations

Add `# @size:` annotations near the top of test scripts (e.g., in the first 20 lines).

### Default (all platforms)
```bash
# Copyright 2026 BitMover, Inc.
# @size: medium
```

### Platform-Specific Overrides
You can specify platform overrides (e.g., `windows`, `macos`, `linux`). If a matching platform override is found, it takes precedence:
```bash
# Copyright 2026 BitMover, Inc.
# @size: small
# @size(windows): medium
```

Supported sizes in Bazel: `small` (default, <1 min), `medium` (<5 min), `large` (<15 min), `enormous` (<60 min).

---

## 2. Define the Repository Rule / Module Extension

Create `src/t/test_metadata.bzl` to parse the test directory and generate a `.bzl` file with test size definitions:

```python
"""Repository rule to dynamically extract test size annotations from test script headers."""

def _test_metadata_impl(ctx):
    os_name = ctx.os.name.lower()
    is_windows = "windows" in os_name
    is_macos = "mac" in os_name or "darwin" in os_name
    is_linux = "linux" in os_name

    sizes = {}
    src_dir = ctx.path(ctx.attr.src_dir)

    for f in src_dir.readdir():
        name = f.basename
        if not name.startswith("t.") and not name.startswith("g."):
            continue

        content = ctx.read(f)
        size = "small"  # default size

        # Scan top 25 lines of the test script for @size annotations
        for line in content.splitlines()[:25]:
            line = line.strip()
            if is_windows and line.startswith("# @size(windows):"):
                size = line.split(":", 1)[1].strip()
                break
            elif is_macos and line.startswith("# @size(macos):"):
                size = line.split(":", 1)[1].strip()
                break
            elif is_linux and line.startswith("# @size(linux):"):
                size = line.split(":", 1)[1].strip()
                break
            elif line.startswith("# @size:"):
                size = line.split(":", 1)[1].strip()

        sizes[name] = size

    # Generate BUILD.bazel and sizes.bzl in the external repository
    ctx.file("BUILD.bazel", "")
    ctx.file("sizes.bzl", "TEST_SIZES = %r\n" % sizes)

test_metadata_repo = repository_rule(
    implementation = _test_metadata_impl,
    attrs = {
        "src_dir": attr.string(default = "src/t"),
    },
)

def _test_metadata_extension_impl(module_ctx):
    test_metadata_repo(name = "test_metadata")

test_metadata_extension = module_extension(
    implementation = _test_metadata_extension_impl,
)
```

---

## 3. Register in `MODULE.bazel`

Add the module extension in `MODULE.bazel`:

```python
test_metadata = use_extension("//src/t:test_metadata.bzl", "test_metadata_extension")
use_repo(test_metadata, "test_metadata")
```

---

## 4. Update `src/t/BUILD.bazel`

Load `TEST_SIZES` from `@test_metadata` in `src/t/BUILD.bazel`:

```python
load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("@test_metadata//:sizes.bzl", "TEST_SIZES")

package(default_visibility = ["//visibility:public"])

# ...

[
    sh_test(
        name = t.replace(".", "_"),
        srcs = ["test_runner.sh"],
        args = [t],
        data = TEST_DATA,
        size = TEST_SIZES.get(t, "small"),
        tags = ["exclusive"] if t in SERIAL_TESTS else [],
    )
    for t in ALL_TEST_FILES
]
```
