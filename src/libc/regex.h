#ifndef REGEX_H
#define REGEX_H

#define	re_comp(x)	re_comp_oz(x)
#define	re_exec(x)	re_exec_oz(x)
#define	re_modw(x)	re_modw_oz(x)
#define	re_subs(x, y)	re_subs_oz(x, y)

extern char *re_comp(char *);
extern int re_exec(char *);
extern void re_modw(char *);
extern int re_subs(char *, char *);

#endif /* REGEX_H */
