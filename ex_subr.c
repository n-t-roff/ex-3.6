/* Copyright (c) 1980 Regents of the University of California */
/*
static char *sccsid = "@(#)ex_subr.c	6.2 11/6/80";
*/
#include "ex.h"
#include "ex_re.h"
#include "ex_tty.h"
#include "ex_vis.h"

/*
 * Random routines, in alphabetical order.
 */

static void save(line *, line *);
static void merror1(char *);

int
any(int c, char *s)
{
	register int x;

	while ((x = *s++))
		if (x == c)
			return (1);
	return (0);
}

int
backtab(int i)
{
	register int j;

	j = i % value(SHIFTWIDTH);
	if (j == 0)
		j = value(SHIFTWIDTH);
	i -= j;
	if (i < 0)
		i = 0;
	return (i);
}

void
change(void)
{

	tchng++;
	chng = tchng;
}

/*
 * Column returns the number of
 * columns occupied by printing the
 * characters through position cp of the
 * current line.
 */
int
column(char *cp)
{

	if (cp == 0)
		cp = &linebuf[LBSIZE - 2];
	return (qcolumn(cp, (char *) 0));
}

/*
 * Ignore a comment to the end of the line.
 * This routine eats the trailing newline so don't call newline().
 */
void
comment(void)
{
	register int c;

	do {
		c = ex_getchar();
	} while (c != '\n' && c != EOF);
	if (c == EOF)
		ungetchar(c);
}

void
Copy(char *to, char *from, int size)
{

	if (size > 0)
		do
			*to++ = *from++;
		while (--size > 0);
}

void
copyw(line *to, line *from, int size)
{

	if (size > 0)
		do
			*to++ = *from++;
		while (--size > 0);
}

void
copywR(line *to, line *from, int size)
{

	while (--size >= 0)
		to[size] = from[size];
}

int
ctlof(int c)
{

	return (c == TRIM ? '?' : c | ('A' - 1));
}

void
dingdong(void)
{

	if (VB)
		putpad(VB);
	else if (value(ERRORBELLS))
		putch('\207');
}

int
fixindent(int indent)
{
	register int i;
	register char *cp;

	i = whitecnt(genbuf);
	cp = vpastwh(genbuf);
	if (*cp == 0 && i == indent && linebuf[0] == 0) {
		genbuf[0] = 0;
		return (i);
	}
	CP(genindent(i), cp);
	return (i);
}

void
filioerr(char *cp)
{
	register int oerrno = errno;

	lprintf("\"%s\"", cp);
	errno = oerrno;
	syserror();
}

char *
genindent(int indent)
{
	register char *cp;

	for (cp = genbuf; indent >= value(TABSTOP); indent -= value(TABSTOP))
		*cp++ = '\t';
	for (; indent > 0; indent--)
		*cp++ = ' ';
	return (cp);
}

void
getDOT(void)
{

	ex_getline(*dot);
}

line *
getmark(int c)
{
	register line *addr;
	
	for (addr = one; addr <= dol; addr++)
		if (names[c - 'a'] == (*addr &~ 01)) {
			return (addr);
		}
	return (0);
}

int
getn(char *cp)
{
	register int i = 0;

	while (isdigit((int)*cp))
		i = i * 10 + *cp++ - '0';
	if (*cp)
		return (0);
	return (i);
}

void
ignnEOF(void)
{
	register int c = ex_getchar();

	if (c == EOF)
		ungetchar(c);
	else if (c=='"')
		comment();
}

int
iswhite(int c)
{

	return (c == ' ' || c == '\t');
}

int
junk(int c)
{

	if (c && !value(BEAUTIFY))
		return (0);
	if (c >= ' ' && c != TRIM)
		return (0);
	switch (c) {

	case '\t':
	case '\n':
	case '\f':
		return (0);

	default:
		return (1);
	}
}

void
killed(void)
{

	killcnt(addr2 - addr1 + 1);
}

void
killcnt(int cnt)
{

	if (inopen) {
		notecnt = cnt;
		notenam = notesgn = "";
		return;
	}
	if (!notable(cnt))
		return;
	ex_printf("%d lines", cnt);
	if (value(TERSE) == 0) {
		ex_printf(" %c%s", Command[0] | ' ', Command + 1);
		if (Command[strlen(Command) - 1] != 'e')
			ex_putchar('e');
		ex_putchar('d');
	}
	putNFL();
}

int
lineno(line *a)
{

	return (a - zero);
}

int
lineDOL(void)
{

	return (lineno(dol));
}

int
lineDOT(void)
{

	return (lineno(dot));
}

void
markDOT(void)
{

	markpr(dot);
}

void
markpr(line *which)
{

	if ((inglobal == 0 || inopen) && which <= endcore) {
		names['z'-'a'+1] = *which & ~01;
		if (inopen)
			ncols['z'-'a'+1] = cursor;
	}
}

int
markreg(int c)
{

	if (c == '\'' || c == '`')
		return ('z' + 1);
	if (c >= 'a' && c <= 'z')
		return (c);
	return (0);
}

/*
 * Mesg decodes the terse/verbose strings. Thus
 *	'xxx@yyy' -> 'xxx' if terse, else 'xxx yyy'
 *	'xxx|yyy' -> 'xxx' if terse, else 'yyy'
 * All others map to themselves.
 */
char *
mesg(char *str)
{
	register char *cp;

	str = strcpy(genbuf, str);
	for (cp = str; *cp; cp++)
		switch (*cp) {

		case '@':
			if (value(TERSE))
				*cp = 0;
			else
				*cp = ' ';
			break;

		case '|':
			if (value(TERSE) == 0)
				return (cp + 1);
			*cp = 0;
			break;
		}
	return (str);
}

void
merror(char *s) {
	imerror(s, 0);
}

/*VARARGS2*/
void
imerror(char *seekpt, int i)
{
	register char *cp = linebuf;

	if (seekpt == 0)
		return;
	merror1(seekpt);
	if (*cp == '\n')
		putnl(), cp++;
	if (inopen && CE)
		vclreol();
	if (SO && SE)
		putpad(SO);
	ex_printf(mesg(cp), i);
	if (SO && SE)
		putpad(SE);
}

static void
merror1(char *seekpt)
{

	strcpy(linebuf, seekpt);
}

int
morelines(void)
{

#ifdef UNIX_SBRK
	if ((int) sbrk(1024 * sizeof (line)) == -1)
		return (-1);
	endcore += 1024;
	return (0);
#else
	return (-1);
#endif
}

void
nonzero(void)
{

	if (addr1 == zero) {
		notempty();
		error("Nonzero address required@on this command");
	}
}

int
notable(int i)
{

	return (hush == 0 && !inglobal && i > value(REPORT));
}

void
notempty(void)
{

	if (dol == zero)
		error("No lines@in the buffer");
}

void
netchHAD(int cnt)
{

	netchange(lineDOL() - cnt);
}

void
netchange(int i)
{
	register char *cp;

	if (i > 0)
		notesgn = cp = "more ";
	else
		notesgn = cp = "fewer ", i = -i;
	if (inopen) {
		notecnt = i;
		notenam = "";
		return;
	}
	if (!notable(i))
		return;
	ex_printf(mesg("%d %slines@in file after %s"), i, cp, Command);
	putNFL();
}

void
putmark(line *addr)
{

	putmk1(addr, putline());
}

void
putmk1(line *addr, int n)
{
	register line *markp;
	int oldglobmk;

	oldglobmk = *addr & 1;
	*addr &= ~1;
	for (markp = (anymarks ? names : &names['z'-'a'+1]);
	  markp <= &names['z'-'a'+1]; markp++)
		if (*markp == *addr)
			*markp = n;
	*addr = n | oldglobmk;
}

char *
plural(long i)
{

	return (i == 1 ? "" : "s");
}

short	vcntcol;

int
qcolumn(char *lim, char *gp)
{
	int x = 0;
	void (*OO)();

	OO = Outchar;
	Outchar = qcount;
	vcntcol = 0;
	if (lim != NULL)
		x = lim[1], lim[1] = 0;
	pline(0);
	if (lim != NULL)
		lim[1] = x;
	if (gp)
		while (*gp)
			ex_putchar(*gp++);
	Outchar = OO;
	return (vcntcol);
}

void
qcount(int c)
{

	if (c == '\t') {
		vcntcol += value(TABSTOP) - vcntcol % value(TABSTOP);
		return;
	}
	vcntcol++;
}

void
reverse(line *a1, line *a2)
{
	register line t;

	for (;;) {
		t = *--a2;
		if (a2 <= a1)
			return;
		*a2 = *a1;
		*a1++ = t;
	}
}

static void
save(line *a1, line *a2)
{
	register int more;

	if (!FIXUNDO)
		return;
#ifdef TRACE
	if (trace)
		vudump("before save");
#endif
	undkind = UNDNONE;
	undadot = dot;
	more = (a2 - a1 + 1) - (unddol - dol);
	while (more > (endcore - truedol))
		if (morelines() < 0)
			error("Out of memory@saving lines for undo - try using ed");
	if (more)
		(*(more > 0 ? copywR : copyw))(unddol + more + 1, unddol + 1,
		    (truedol - unddol));
	unddol += more;
	truedol += more;
	copyw(dol + 1, a1, a2 - a1 + 1);
	undkind = UNDALL;
	unddel = a1 - 1;
	undap1 = a1;
	undap2 = a2 + 1;
#ifdef TRACE
	if (trace)
		vudump("after save");
#endif
}

void
save12(void)
{

	save(addr1, addr2);
}

void
saveall(void)
{

	save(one, dol);
}

int
span(void)
{

	return (addr2 - addr1 + 1);
}

void
ex_sync(void)
{

	chng = 0;
	tchng = 0;
	xchng = 0;
}

int
skipwh(void)
{
	register int wh;

	wh = 0;
	while (iswhite(peekchar())) {
		wh++;
		ignchar();
	}
	return (wh);
}

/*VARARGS2*/
void
smerror(char *seekpt, char *cp)
{

	if (seekpt == 0)
		return;
	merror1(seekpt);
	if (inopen && CE)
		vclreol();
	if (SO && SE)
		putpad(SO);
	lprintf(mesg(linebuf), cp);
	if (SO && SE)
		putpad(SE);
}

char *
strend(char *cp)
{

	while (*cp)
		cp++;
	return (cp);
}

void
strcLIN(char *dp)
{

	CP(linebuf, dp);
}

void
syserror(void)
{
	dirtcnt = 0;
	ex_putchar(' ');
	edited = 0;	/* for temp file errors, for example */
	error(strerror(errno));
}

/*
 * Return the column number that results from being in column col and
 * hitting a tab, where tabs are set every ts columns.  Work right for
 * the case where col > COLUMNS, even if ts does not divide COLUMNS.
 */
int
tabcol(int col, int ts)
{
	int offset, result;

	if (col >= COLUMNS) {
		offset = COLUMNS * (col/COLUMNS);
		col -= offset;
	} else
		offset = 0;
	result = col + ts - (col % ts) + offset;
	return (result);
}

char *
vfindcol(int i)
{
	register char *cp;
	char *s;
	void (*OO)() = Outchar;

	Outchar = qcount;
	s = linebuf;
	ignore(qcolumn(s - 1, NOSTR));
	for (cp = linebuf; *cp && vcntcol < i; cp++)
		ex_putchar(*cp);
	if (cp != linebuf)
		cp--;
	Outchar = OO;
	return (cp);
}

char *
vskipwh(char *cp)
{

	while (iswhite(*cp) && cp[1])
		cp++;
	return (cp);
}


char *
vpastwh(char *cp)
{

	while (iswhite(*cp))
		cp++;
	return (cp);
}

int
whitecnt(char *cp)
{
	register int i;

	i = 0;
	for (;;)
		switch (*cp++) {

		case '\t':
			i += value(TABSTOP) - i % value(TABSTOP);
			break;

		case ' ':
			i++;
			break;

		default:
			return (i);
		}
}

#ifdef lint
Ignore(a)
	char *a;
{

	a = a;
}

Ignorf(a)
	int (*a)();
{

	a = a;
}
#endif

void
markit(line *addr)
{

	if (addr != dot && addr >= one && addr <= dol)
		markDOT();
}

/*
 * The following code is defensive programming against a bug in the
 * pdp-11 overlay implementation.  Sometimes it goes nuts and asks
 * for an overlay with some garbage number, which generates an emt
 * trap.  This is a less than elegant solution, but it is somewhat
 * better than core dumping and losing your work, leaving your tty
 * in a weird state, etc.
 */
#ifdef SIGEMT
int _ovno;

void
onemt(int i)
{
	(void)i;
	signal(SIGEMT, onemt);
	/* 2 and 3 are valid on 11/40 type vi, so */
	if (_ovno < 0 || _ovno > 3)
		_ovno = 0;
	error("emt trap, _ovno is %d @ - try again");
}
#endif
