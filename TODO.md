# BitKeeper TODO List

## Installer & Runtime Setup
- **Detect and symlink GNU `diff`, `diff3`, and `patch` in installer**:
  The installer script, when running on the user's machine, should test the `diff`, `diff3`, and `patch` binaries present on the system and symlink the correct GNU versions into the BitKeeper installation directory (`bk diff`, `bk merge`, and other commands will use any binary located next to `bk` if it exists). If suitable GNU versions cannot be found, the installer should emit a loud warning informing the user that BitKeeper requires GNU diff/patch utilities to function properly.
