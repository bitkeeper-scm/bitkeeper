/*
 * Copyright 1999-2003,2005-2007,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bkd.h"

/* Keep this alphabetical please */
struct cmd cmds[] = {
  { "?", "?", "print this help", cmd_help },
  { "abort", "abort_1.3", "abort resolve", cmd_abort },
  { "bk", "bk_1.3", "simple bk commands", cmd_bk },
  { "cd", "cd_1.3", "change to a new repository root", cmd_cd },
  { "check", "check_1.3", "check repository", cmd_check },
  { "clone", "clone_1.2", "clone the current repository", cmd_clone },
  { "help", "help_1.3", "print this help", cmd_help },
  { "httpget", "httpget_1.3", "http get command", cmd_httpget },
  { "get", "get_1.3", "http get command", cmd_httpget },
  { "pwd", "pwd_1.3", "show current working directory", cmd_pwd },
  { "quit", "quit_1.3", "disconnect and end conversation", cmd_quit },
  { "rootkey", "rootkey_1.3", "show the ChangeSet root key", cmd_rootkey },
  { "status", "status_1.3", "Show status for repository", cmd_status },
  { "version", "version_1.3", "Show bkd version", cmd_version },
  { "putenv", "putenv.1.3", "set up environment variable", cmd_putenv },
  { "push_part1", "push_1.3_part1", 
	"push and apply local changes into remote repository", cmd_push_part1 },
  { "push_part2", "push_1.3_part2", 
	"push and apply local changes into remote repository", cmd_push_part2 },
  { "push_part3", "push_1.3_part3", 
	"push and apply local changes into remote repository", cmd_push_part3 },
  { "pull_part1", "pull_1.3_part1",
    "pull remote changes from current repository into client repository",
    cmd_pull_part1 },
  { "pull_part2", "pull_1.3_part2",
    "pull remote changes from current repository into client repository",
    cmd_pull_part2 },
  { "rclone_part1", "rclone_1.3_part1", 
    "clone local repository to remote repository",
    cmd_rclone_part1 },
  { "rclone_part2", "rclone_1.3_part2", 
    "clone local repository to remote repository",
    cmd_rclone_part2 },
  { "rclone_part3", "rclone_1.3_part3", 
    "clone local repository to remote repository",
    cmd_rclone_part3 },
  { "synckeys", "synckeys_1.3", 
    "sync keys in local repository to remote repository",
    cmd_synckeys },
  { "chg_part1", "chg_1.3", 
    "get new csets in remote repository",
    cmd_chg_part1 },
  { "chg_part2", "chg_1.3", 
    "get new csets in remote repository",
    cmd_chg_part2 },
  { "kill", "kill_1.3", "kill remote bkd", cmd_kill },
  { "rdlock", "rdlock", "read lock the repository", cmd_rdlock },
  { "rdunlock", "rdunlock", "remove read lock from repository", cmd_rdunlock },
  { "nested", "nested", "nested locking support", cmd_nested},
  { "wrlock", "wrlock", "write lock the repository", cmd_wrlock },
  { "wrunlock", "wrunlock", "remove write lock from repository", cmd_wrunlock},
  { 0, 0, 0 }
};
