/*
 * Copyright 2007-2011,2016 BitMover, Inc
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

#ifndef	_BAM_H
#define	_BAM_H
/* functions only used by bam.c and bkd_bam.c */

#define	BAM_ROOT	"BitKeeper/BAM"
#define	BAM_DB		"index.db"
#define	BAM_MARKER	"BitKeeper/log/BAM"
#define	BAM_SERVER	"BitKeeper/log/BAM_SERVER"
// This is what we return if they didn't set a size
#define	BAM_SIZE	(64<<10)

char	*bp_lookupkeys(project *p, char *keys);
int	bp_logUpdate(project *p, char *key, char *val);
int	bp_hashgfile(char *gfile, char **hashp, sum_t *sump);
int	bp_index_check(int quiet);
int	bp_check_hash(char *want, char ***missing, int fast, int quiet);
int	bp_check_findMissing(int quiet, char **missing);
char	*bp_dataroot(project *proj, char *buf);
char	*bp_indexfile(project *proj, char *buf);
int	bp_rename(project *proj, char *old, char *new);
int	bp_link(project *oproj, char *old, project *nproj, char *new);
int	bam_converted(int ispull);
#endif
