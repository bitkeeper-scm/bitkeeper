#include "bkd.h"

/* Keep this alphabetical please */
struct cmd cmds[] = {
  { "?", "print this help", cmd_help },
  { "cd", "change to a new repository root", cmd_cd },
  { "clone", "clone the current repository", cmd_clone },
  { "help", "print this help", cmd_help },
  { "httpget", "http get command", cmd_httpget },
  { "pull",
    "pull remote changes from current repository into client repository",
    cmd_pull },
  { "push", "push and apply local changes into remote repository", cmd_push },
  { "pwd", "show current working directory", cmd_pwd },
  { "quit", "disconnect and end conversation", cmd_eof },
  { "rootkey", "show the ChangeSet root key", cmd_rootkey },
  { "status", "Show status for repository", cmd_status },
  { "version", "Show bkd version", cmd_version },
  { 0, 0, 0 }
};
