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
