/*
 * Copyright 2015-2016 BitMover, Inc
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

#ifndef	_CONFIG_H_
#define	_CONFIG_H_
#include "system.h"
#include "sccs.h"
#include "proj.h"
#include "bam.h"

// config variables access via enum
#define CONFIGVAR(def, ...) CONFIG_##def,
enum {
#include "configvars.def"
};
#undef CONFIGVAR

char	*config_def(int idx);
char	*config_alias(char *name);	/* return canonical name */
char	*config_str(project *p, int idx);
int	config_bool(project *p, int idx);
u64	config_size(project *p, int idx);
i64	config_int(project *p, int idx);
int	config_findVar(char *name);
MDBM	*config_loadSetup(MDBM *db);
void	config_printDefaults(MDBM *db, MDBM *defs, MDBM *merge);
#endif	/* _CONFIG_H_ */
