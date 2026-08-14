# BitKeeper Regression Tests

The BitKeeper test suite consists of regression test scripts prefixed with `t.` (and GUI tests prefixed with `g.`), located in `src/t/`. Each test script runs against a freshly staged BitKeeper environment in an isolated sandbox.

## Running Tests with Bazel

Test target names mirror the test script filenames with dots replaced by underscores (e.g., `t.basic` corresponds to `//src/t:t_basic`).

### Run all tests
```bash
bazel test //src/t:...
```

### Run a single test
```bash
bazel test //src/t:t_basic
```

### Force re-running a cached test
Bazel caches successful test results. To force re-execution:
```bash
bazel test --nocache_test_results //src/t:t_basic
```

## Debugging and Verbosity Options

The underlying test runner (`doit`) supports verbose and tracing flags, which can be passed from Bazel using `--test_arg` or `--test_env`.

### Display output
By default, Bazel only displays test output when a test fails. Use `--test_output=all` or `--test_output=streamed` to see full output:
```bash
bazel test //src/t:t_basic --test_output=all
```

### Verbose mode (`-v`)
Enables verbose output from commands within the test:
```bash
bazel test //src/t:t_basic --test_arg=-v --test_output=all
```

### Shell trace mode (`-x`)
Enables shell execution tracing (`set -x`) for the test script:
```bash
bazel test //src/t:t_basic --test_arg=-x --test_output=all
```

### Combining flags
Pass multiple `--test_arg` options or use `BK_TEST_FLAGS`:
```bash
bazel test //src/t:t_basic --test_arg=-v --test_arg=-x --test_output=all
```
or:
```bash
bazel test //src/t:t_basic --test_env=BK_TEST_FLAGS="-v -x" --test_output=all
```

## Test Structure and Execution

- **`test_runner.sh`**: Bazel harness that sets up the isolated execution sandbox, stages the built `bk` binary, runtime libraries, GUI components, and helper scripts, and invokes `doit`.
- **`setup`**: Sourced before each test to define helper shell functions (such as `commercial`, `test_failed`, etc.) and set up test repository environments.
- **`doit.sh`**: The test driver that executes the test script within a sandbox repository (`.regression`).
