/* Copyright (c) 1998 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");

/*
 * range.c - get endpoints of a range of deltas
 *
 * The point of this routine is to handle the various ways that people
 * can specify a range of deltas.  Ranges consist of either one or two
 * deltas, specified as either a date, a symbol, or a revision.
 *
 * The various forms of the information passed in:
 *	d1..d2 -> d1 to d2, inclusive.
 *	d1,,d2 -> d1 to d2, exclusive.
 *	d1,.d2 -> after d1, up to and including d2.
 *	.d2 -> first delta at or before d2.
 *	,d2 -> first delta before d2.
 *	d1. -> d1 or first delta after d1.
 *	d1, -> first delta after d1.
 *	d1 -> delta at d1.
 *
 *	d1,d2 is an alias for d1,.d2 since that is such a common thing
 *	to do i.e., Alpha3,.Alpha4
 *
 * All these forms can be used with revisions or symbols, and they
 * may be mixed and matched.  Dates may be specified as a tag; revisions
 * always have to be just revisions.
 *
 * Date format: yymmddhhmmss
 *	Rounds up at the end, rounds down at the beginning.
 *	For month, day, hour, minute, second fields, any
 *	value that is too large, is truncated back to its
 *	highest value.  This is so you can always use "31"
 *	as the last day of the month and the right thing happens.
 *
 * Interfaces:
 *	rangeReset(sccs *sc) - reset for a new range
 *	rangeAdd(sccs *sc, char *rev, char *date) - add rev or date (symbol)
 */

#define	OP(c)	((c == '.') || (c == ','))
#define	RTH(c)	((c == '(') || (c == '[') || (c == ')') || (c == ']'))

void
rangeReset(sccs *sc)
{
	sc->rstart = sc->rstop = 0;
}

/*
 * Return 0 if OK, -1 if not.
 */
int
rangeAdd(sccs *sc, char *rev, char *date)
{
	char	*s = rev ? rev : date;
	char	*last;
	char	save;
	delta	*tmp;
	char	*comma;

	assert(sc);
	if ((!rev || !*rev) && !date) {
		tmp = findrev(sc, 0);
		goto out;
	}
	assert(rev || date);
	assert(!(rev && date));
	if (sc->rstart && sc->rstop) return (-1);

	/*
	 * Support Richard's notation of (ddd,ddd] and also
	 * (ddd
	 * [ddd
	 * ddd)
	 * ddd]
	 */
	for (last = s, comma = 0; last[1]; last++) {
		if (last[0] == ',') comma = last;
	}
	if ((RTH(*s) || RTH(*last)) ||
	    (comma && (comma != s) && (RTH(comma[-1]) || RTH(comma[1])))) {
		if (comma) {
			*comma = 0;
			if (rangeAdd(sc, rev, date)) return (-1);
			*comma++ = ',';
			if (rev) {
				rev = comma;
			} else {
				date = comma;
			}
			return (rangeAdd(sc, rev, date));
		}
	}

	/*
	 * Figure out if we have both endpoints; if so, split them up
	 * and then call ourselves recursively.
	 */
	for (s++; *s; s++) {
		if (OP(s[0]) && OP(s[1])) {
			save = s[1]; s[1] = 0;
			if (rangeAdd(sc, rev, date)) {
				s[1] = save;
				return (-1);
			}
			s[1] = save;
			if (rev) {
				rev = &s[1];
			} else {
				date = &s[1];
			}
			if (rangeAdd(sc, rev, date)) return (-1);
			return (0);
		} else if (s[0] == ',' && s[1]) {  /* , -> ,. */
			save = s[1]; s[1] = 0;
			if (rangeAdd(sc, rev, date)) {
				s[1] = save;
				return (-1);
			}
			s[1] = save; s[0] = '.';
			if (rev) {
				rev = s;
			} else {
				date = s;
			}
			if (rangeAdd(sc, rev, date)) {
				s[0] = ',';
				return (-1);
			}
			return (0);
		}
	}
	tmp = sccs_getrev(sc, rev, date, sc->rstart ? ROUNDUP: ROUNDDOWN);
out:	if (!tmp) return (-1);
	if (!sc->rstart) {
		sc->rstart = tmp;
	} else {
		sc->rstop = tmp;
		/*
		 * Make sure that the right end is after the left end.
		 */
		for (tmp = sc->rstop;
		    tmp && tmp != sc->rstart; tmp = tmp->parent);
		if (!tmp) sc->rstart = sc->rstart = 0;
	}
	return (0);
}

/*
 * Figure out if we have 1 or 2 tokens.
 * Note this works for A..B and [A,B] forms.  Got lucky on that one.
 * But not on 1.1 - which should return 1.
 */
int
tokens(char *s)
{
	for (s++; *s; s++) {
		if (OP(s[0]) && OP(s[1])) return (2);
		if ((*s == ',') && s[1]) return (2);
	}
	return (1);
}

int
roundType(char *s)
{
	char	*last;

	if (!s || !*s) return (EXACT);
	for (last = s; last[1]; last++);
	if ((s[0] == ',') || (s[0] == '.') ||
	    (last[0] == ')') || (last[0] == ']')) {
		return (ROUNDUP);
	} else if ((s[0] == '(') || (s[0] == '[') ||
	    (last[0] == ',') || (last[0] == '.')) {
		return (ROUNDDOWN);
	}
	return (EXACT);
}
