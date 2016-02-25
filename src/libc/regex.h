/*
 * Copyright 2003,2012,2016 BitMover, Inc
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

#ifndef REGEX_H
#define REGEX_H

typedef	struct regex regex;

#define	re_comp(x)	re_comp_oz(x)
#define	re_exec(r,x)	re_exec_oz(r, x)
#define	re_modw(x)	re_modw_oz(x)
#define	re_subs(r,x,y)	re_subs_oz(r, x, y)
#define	re_lasterr	re_lasterr_oz
#define	re_free(r)	re_free_oz(r)

extern regex *re_comp(char *);
extern int re_exec(regex *, char *);
extern void re_free(regex *);
extern char *re_lasterr(void);
extern void re_modw(char *);
extern void re_fail(char *, char);

#endif /* REGEX_H */
