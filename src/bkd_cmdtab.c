#include "bkd.h"

/* Keep this alphabetical please */
struct cmd cmds[] = {
  { "?", "print this help", cmd_help, },
  { "clone", "clone the current repository", cmd_clone, },
  { "help", "print this help", cmd_help, },
  { "pull",
    "pull remote changes from current repository into client repository",
    cmd_pull, },
  { "push", "push and apply local changes into remote repository", cmd_push, },
  { "quit", "disconnect and end conversation", cmd_eof, },
  { "root",
    "set the repository to path or key; keys mean masters only", cmd_root, },
  { "status", "Show status for repository", cmd_status, },
  { "version", "Show bkd version", cmd_version, },
  { 0, 0, 0 }
};
