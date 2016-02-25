/*
 * Copyright 2001-2003,2006-2008,2015-2016 BitMover, Inc
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

#include "sccs.h"

/*
 * Embbed the various checkout rules for bk files and fetch a gfile
 * if it is required.
 *
 * getFlags is normally 0, but check.c sometimes passes GET_DTIME
 *
 * if 'bamFiles' is passed then BAM files that can't be found locally
 * will be skipped and their gfile pathnames will be returned in this
 * array.
 *
 * returns -1 on failure
 */
int
do_checkout(sccs *s, u32 getFlags, char ***bamFiles)
{
	ser_t	d;

	if (CSET(s)) return (0);
	if (strneq(s->gfile, "BitKeeper/", 10) &&
	    !strneq(s->gfile, "BitKeeper/triggers/", 19)) {
		return (0);
	}
	if (win32() && S_ISLNK(MODE(s, sccs_top(s)))) {
		/* no symlinks on windows */
		return (0);
	}
	if (bamFiles) getFlags |= GET_NOREMOTE;
	if (streq(s->gfile, "BitKeeper/etc/config")) getFlags |= GET_EXPAND;
	unless (getFlags & (GET_EXPAND|GET_EDIT)) switch (CO(s)) {
	    case CO_GET: getFlags |= GET_EXPAND; break;
	    case CO_EDIT: getFlags |= GET_EDIT; break;
	    case CO_LAST:
		/* XXX - this counts on these flags and it is possible
		 * that they are not set.
		 */
		unless (HAS_GFILE(s)) return (0);
		if (HAS_PFILE(s)) {
			getFlags |= GET_EDIT;
		} else {
			getFlags |= GET_EXPAND;
		}
		break;
	    default: return (0);
	}
	if (getFlags & GET_EDIT) {
		if (HAS_GFILE(s)) {
			if (HAS_PFILE(s)) {
				return (0);
			} else if (!HAS_KEYWORDS(s)) {
				getFlags |= GET_SKIPGET;
				unless (WRITABLE(s) || S_ISLNK(s->mode)) {
					d = sccs_top(s);
					if (MODE(s, d)) s->mode = MODE(s, d);
					chmod(s->gfile, s->mode);
				}
			}
		}
	} else {
		if (HAS_GFILE(s)) return (0);
	}
	if (sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, s->gfile, 0)) {
		if (bamFiles && s->cachemiss) {
			*bamFiles = addLine(*bamFiles, strdup(s->gfile));
		} else {
			return (-1);
		}
	}
	s = sccs_restart(s);
	unless (s) return (-1);
	return (0);
}
