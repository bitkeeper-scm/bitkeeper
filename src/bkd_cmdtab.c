#include "bkd.h"

/* Keep this alphabetical please */
struct cmd cmds[] = {
  { "?", "print this help", cmd_help, 1 },
  { "clone", "clone the current repository", cmd_clone, 1 },
  { "compress", "turn on gzip -6 compression", cmd_compress, 1 },
  { "help", "print this help", cmd_help, 1 },
  { "pull",
    "pull remote changes from current repository into client repository",
    cmd_pull, 1 },
  { "push", "push and apply local changes into remote repository", cmd_push, 0},
  { "quit", "disconnect and end conversation", cmd_eof, 1 },
  { "root",
    "set the repository to path or key; keys mean masters only", cmd_root, 1 },
  { "status", "Show status for repository", cmd_status, 1 },
  { "version", "Show bkd version", cmd_version, 1 },
  { 0, 0, 0, 0 }
};
