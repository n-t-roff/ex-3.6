/* Copyright (c) 1980 Regents of the University of California */
/*
static char *sccsid = "@(#)ex_put.c	6.3 11/3/80";
*/
#include "ex.h"
#include "ex_tty.h"
#include "ex_vis.h"

/*
 * Terminal driving and line formatting routines.
 * Basic motion optimizations are done here as well
 * as formatting of lines (printing of control characters,
 * line numbering and the like).
 */

/*
 * The routines outchar, putchar and pline are actually
 * variables, and these variables point at the current definitions
 * of the routines.  See the routine setflav.
 * We sometimes make outchar be routines which catch the characters
 * to be printed, e.g. if we want to see how long a line is.
 * During open/visual, outchar and putchar will be set to
 * routines in the file ex_vput.c (vputchar, vinschar, etc.).
 */
static void normchar(int);
static void slobber(int);
static void flush2(void);
static int plodput(int);
static int plod(int);
static void ttcharoff(void);
static void normal(ttymode);
static void sTTY(int);

void	(*Outchar)() = termchar;
void	(*Putchar)() = normchar;
void	(*Pline)() = normline;

void (*
setlist(bool t))()
{
	void (*P)();

	listf = t;
	P = Putchar;
	Putchar = t ? listchar : normchar;
	return (P);
}

void (*
setnumb(bool t))()
{
	void (*P)();

	numberf = t;
	P = Pline;
	Pline = t ? numbline : normline;
	return (P);
}

/*
 * Format c for list mode; leave things in common
 * with normal print mode to be done by normchar.
 */
void
listchar(int c)
{

	c &= (TRIM|QUOTE);
	switch (c) {

	case '\t':
	case '\b':
		outchar('^');
		c = ctlof(c);
		break;

	case '\n':
		break;

	case '\n' | QUOTE:
		outchar('$');
		break;

	default:
		if (c & QUOTE)
			break;
		if (c < ' ' && c != '\n' || c == DELETE)
			outchar('^'), c = ctlof(c);
		break;
	}
	normchar(c);
}

/*
 * Format c for printing.  Handle funnies of upper case terminals
 * and crocky hazeltines which don't have ~.
 */
static void
normchar(int c)
{
	register char *colp;

	c &= (TRIM|QUOTE);
	if (c == '~' && HZ) {
		normchar('\\');
		c = '^';
	}
	if (c & QUOTE)
		switch (c) {

		case ' ' | QUOTE:
		case '\b' | QUOTE:
			break;

		case QUOTE:
			return;

		default:
			c &= TRIM;
#ifdef BIT8
			if ((c < ' ' && (c != '\b' || !OS) && c != '\n'
			    && c != '\t') || c == DELETE)
				ex_putchar('^'), c = ctlof(c);
#endif
		}
	else if (c < ' ' && (c != '\b' || !OS) && c != '\n' && c != '\t' || c == DELETE)
		ex_putchar('^'), c = ctlof(c);
	else if (UPPERCASE)
		if (isupper(c)) {
			outchar('\\');
			c = tolower(c);
		} else {
			colp = "({)}!|^~'`";
			while (*colp++)
				if (c == *colp++) {
					outchar('\\');
					c = colp[-2];
					break;
				}
		}
	outchar(c);
}

/*
 * Print a line with a number.
 */
void
numbline(int i)
{

	if (shudclob)
		slobber(' ');
	ex_printf("%6d  ", i);
	normline();
}

/*
 * Normal line output, no numbering.
 */
void
normline(void)
{
	register char *cp;

	if (shudclob)
		slobber(linebuf[0]);
	/* pdp-11 doprnt is not reentrant so can't use "printf" here
	   in case we are tracing */
	for (cp = linebuf; *cp;)
		ex_putchar(*cp++);
	if (!inopen)
		ex_putchar('\n' | QUOTE);
}

/*
 * Given c at the beginning of a line, determine whether
 * the printing of the line will erase or otherwise obliterate
 * the prompt which was printed before.  If it won't, do it now.
 */
static void
slobber(int c)
{

	shudclob = 0;
	switch (c) {

	case '\t':
		if (Putchar == listchar)
			return;
		break;

	default:
		return;

	case ' ':
	case 0:
		break;
	}
	if (OS)
		return;
	flush();
	putch(' ');
	if (BC)
		tputs(BC, 0, putch);
	else
		putch('\b');
}

/*
 * The output buffer is initialized with a useful error
 * message so we don't have to keep it in data space.
 */
static	char linb[66];
static	char *linp = linb;

/*
 * Phadnl records when we have already had a complete line ending with \n.
 * If another line starts without a flush, and the terminal suggests it,
 * we switch into -nl mode so that we can send lineffeeds to avoid
 * a lot of spacing.
 */
static	bool phadnl;

/*
 * Indirect to current definition of putchar.
 */
void
ex_putchar(int c)
{

	(*Putchar)(c);
}

/*
 * Termchar routine for command mode.
 * Watch for possible switching to -nl mode.
 * Otherwise flush into next level of buffering when
 * small buffer fills or at a newline.
 */
void
termchar(int c)
{

	if (pfast == 0 && phadnl)
		pstart();
	if (c == '\n')
		phadnl = 1;
	else if (linp >= &linb[63])
		flush1();
	*linp++ = c;
	if (linp >= &linb[63]) {
		fgoto();
		flush1();
	}
}

void
flush(void)
{

	flush1();
	flush2();
}

/*
 * Flush from small line buffer into output buffer.
 * Work here is destroying motion into positions, and then
 * letting fgoto do the optimized motion.
 */
void
flush1(void)
{
	register char *lp;
	int c;

	*linp = 0;
	lp = linb;
	while (*lp)
		switch (c = *lp++) {

		case '\r':
			destline += destcol / COLUMNS;
			destcol = 0;
			continue;

		case '\b':
			if (destcol)
				destcol--;
			continue;

		case ' ':
			destcol++;
			continue;

		case '\t':
			destcol += value(TABSTOP) - destcol % value(TABSTOP);
			continue;

		case '\n':
			destline += destcol / COLUMNS + 1;
			if (destcol != 0 && destcol % COLUMNS == 0)
				destline--;
			destcol = 0;
			continue;

		default:
			fgoto();
			for (;;) {
				if (AM == 0 && outcol == COLUMNS)
					fgoto();
				c &= TRIM;
				putch(c);
				if (c == '\b') {
					outcol--;
					destcol--;
				} else if (c >= ' ' && c != DELETE) {
					outcol++;
					destcol++;
					if (XN && outcol % COLUMNS == 0)
						putch('\n');
				}
				c = *lp++;
				if (c <= ' ')
					break;
			}
			--lp;
			continue;
		}
	linp = linb;
}

static void
flush2(void)
{

	fgoto();
	flusho();
	pstop();
}

/*
 * Sync the position of the output cursor.
 * Most work here is rounding for terminal boundaries getting the
 * column position implied by wraparound or the lack thereof and
 * rolling up the screen to get destline on the screen.
 */
void
fgoto(void)
{
	register int l, c;

	if (destcol > COLUMNS - 1) {
		destline += destcol / COLUMNS;
		destcol %= COLUMNS;
	}
	if (outcol > COLUMNS - 1) {
		l = (outcol + 1) / COLUMNS;
		outline += l;
		outcol %= COLUMNS;
		if (AM == 0) {
			while (l > 0) {
				if (pfast)
					if (xCR)
						tputs(xCR, 0, putch);
					else
						putch('\r');
				if (xNL)
					tputs(xNL, 0, putch);
				else
					putch('\n');
				l--;
			}
			outcol = 0;
		}
		if (outline > EX_LINES - 1) {
			destline -= outline - (EX_LINES - 1);
			outline = EX_LINES - 1;
		}
	}
	if (destline > EX_LINES - 1) {
		l = destline;
		destline = EX_LINES - 1;
		if (outline < EX_LINES - 1) {
			c = destcol;
			if (pfast == 0 && (!CA || holdcm))
				destcol = 0;
			fgoto();
			destcol = c;
		}
		while (l > EX_LINES - 1) {
			/*
			 * The following linefeed (or simulation thereof)
			 * is supposed to scroll up the screen, since we
			 * are on the bottom line.  We make the assumption
			 * that linefeed will scroll.  If ns is in the
			 * capability list this won't work.  We should
			 * probably have an sc capability but sf will
			 * generally take the place if it works.
			 *
			 * Superbee glitch:  in the middle of the screen we
			 * have to use esc B (down) because linefeed screws up
			 * in "Efficient Paging" (what a joke) mode (which is
			 * essential in some SB's because CRLF mode puts garbage
			 * in at end of memory), but you must use linefeed to
			 * scroll since down arrow won't go past memory end.
			 * I turned this off after recieving Paul Eggert's
			 * Superbee description which wins better.
			 */
			if (xNL /* && !XB */ && pfast)
				tputs(xNL, 0, putch);
			else
				putch('\n');
			l--;
			if (pfast == 0)
				outcol = 0;
		}
	}
	if (destline < outline && !(CA && !holdcm || UP != NOSTR))
		destline = outline;
	if (CA && !holdcm)
		if (plod(costCM) > 0)
			plod(0);
		else
			tputs(tgoto(CM, destcol, destline), 0, putch);
	else
		plod(0);
	outline = destline;
	outcol = destcol;
}

/*
 * Tab to column col by flushing and then setting destcol.
 * Used by "set all".
 */
void
ex_tab(int col)
{

	flush1();
	destcol = col;
}

/*
 * Move (slowly) to destination.
 * Hard thing here is using home cursor on really deficient terminals.
 * Otherwise just use cursor motions, hacking use of tabs and overtabbing
 * and backspace.
 */

static int plodcnt, plodflg;

static int
plodput(int c)
{

	if (plodflg) {
		plodcnt--;
		return c;
	} else
		return putch(c);
}

static int
plod(int cnt)
{
	register int i, j, k;
	register int soutcol, soutline;

	plodcnt = plodflg = cnt;
	soutcol = outcol;
	soutline = outline;
	/*
	 * Consider homing and moving down/right from there, vs moving
	 * directly with local motions to the right spot.
	 */
	if (HO) {
		/*
		 * i is the cost to home and tab/space to the right to
		 * get to the proper column.  This assumes ND space costs
		 * 1 char.  So i+destcol is cost of motion with home.
		 */
		if (GT)
			i = (destcol / value(HARDTABS)) + (destcol % value(HARDTABS));
		else
			i = destcol;
		/*
		 * j is cost to move locally without homing
		 */
		if (destcol >= outcol) {	/* if motion is to the right */
			j = destcol / value(HARDTABS) - outcol / value(HARDTABS);
			if (GT && j)
				j += destcol % value(HARDTABS);
			else
				j = destcol - outcol;
		} else
			/* leftward motion only works if we can backspace. */
			if (outcol - destcol <= i && (BS || BC))
				i = j = outcol - destcol; /* cheaper to backspace */
			else
				j = i + 1; /* impossibly expensive */

		/* k is the absolute value of vertical distance */
		k = outline - destline;
		if (k < 0)
			k = -k;
		j += k;

		/*
		 * Decision.  We may not have a choice if no UP.
		 */
		if (i + destline < j || (!UP && destline < outline)) {
			/*
			 * Cheaper to home.  Do it now and pretend it's a
			 * regular local motion.
			 */
			tputs(HO, 0, plodput);
			outcol = outline = 0;
		} else if (LL) {
			/*
			 * Quickly consider homing down and moving from there.
			 * Assume cost of LL is 2.
			 */
			k = (EX_LINES - 1) - destline;
			if (i + k + 2 < j && (k<=0 || UP)) {
				tputs(LL, 0, plodput);
				outcol = 0;
				outline = EX_LINES - 1;
			}
		}
	} else
	/*
	 * No home and no up means it's impossible, so we return an
	 * incredibly big number to make cursor motion win out.
	 */
		if (!UP && destline < outline)
			return (500);
	if (GT)
		i = destcol % value(HARDTABS)
		    + destcol / value(HARDTABS);
	else
		i = destcol;
/*
	if (BT && outcol > destcol && (j = (((outcol+7) & ~7) - destcol - 1) >> 3)) {
		j *= (k = strlen(BT));
		if ((k += (destcol&7)) > 4)
			j += 8 - (destcol&7);
		else
			j += k;
	} else
*/
		j = outcol - destcol;
	/*
	 * If we will later need a \n which will turn into a \r\n by
	 * the system or the terminal, then don't bother to try to \r.
	 */
	if ((NONL || !pfast) && outline < destline)
		goto dontcr;
	/*
	 * If the terminal will do a \r\n and there isn't room for it,
	 * then we can't afford a \r.
	 */
	if (NC && outline >= destline)
		goto dontcr;
	/*
	 * If it will be cheaper, or if we can't back up, then send
	 * a return preliminarily.
	 */
	if (j > i + 1 || outcol > destcol && !BS && !BC) {
		/*
		 * BUG: this doesn't take the (possibly long) length
		 * of xCR into account.
		 */
		if (xCR)
			tputs(xCR, 0, plodput);
		else
			plodput('\r');
		if (NC) {
			if (xNL)
				tputs(xNL, 0, plodput);
			else
				plodput('\n');
			outline++;
		}
		outcol = 0;
	}
dontcr:
	while (outline < destline) {
		outline++;
		if (xNL && pfast)
			tputs(xNL, 0, plodput);
		else
			plodput('\n');
		if (plodcnt < 0)
			goto out;
		if (NONL || pfast == 0)
			outcol = 0;
	}
	if (BT)
		k = strlen(BT);
	while (outcol > destcol) {
		if (plodcnt < 0)
			goto out;
/*
		if (BT && !insmode && outcol - destcol > 4+k) {
			tputs(BT, 0, plodput);
			outcol--;
			outcol &= ~7;
			continue;
		}
*/
		outcol--;
		if (BC)
			tputs(BC, 0, plodput);
		else
			plodput('\b');
	}
	while (outline > destline) {
		outline--;
		tputs(UP, 0, plodput);
		if (plodcnt < 0)
			goto out;
	}
	if (GT && !insmode && destcol - outcol > 1) {
	for (;;) {
		i = tabcol(outcol, value(HARDTABS));
		if (i > destcol)
			break;
			if (TA)
				tputs(TA, 0, plodput);
			else
				plodput('\t');
			outcol = i;
		}
		if (destcol - outcol > 4 && i < COLUMNS && (BC || BS)) {
			if (TA)
				tputs(TA, 0, plodput);
			else
				plodput('\t');
			outcol = i;
			while (outcol > destcol) {
				outcol--;
				if (BC)
					tputs(BC, 0, plodput);
				else
					plodput('\b');
			}
		}
	}
	while (outcol < destcol) {
		/*
		 * move one char to the right.  We don't use ND space
		 * because it's better to just print the char we are
		 * moving over.  There are various exceptions, however.
		 * If !inopen, vtube contains garbage.  If the char is
		 * a null or a tab we want to print a space.  Other random
		 * chars we use space for instead, too.
		 */
		if (!inopen || vtube[outline]==NULL ||
			(i=vtube[outline][outcol]) < ' ')
			i = ' ';
		if (insmode && ND)
			tputs(ND, 0, plodput);
		else
			plodput(i);
		outcol++;
		if (plodcnt < 0)
			goto out;
	}
out:
	if (plodflg) {
		outcol = soutcol;
		outline = soutline;
	}
	return(plodcnt);
}

/*
 * An input line arrived.
 * Calculate new (approximate) screen line position.
 * Approximate because kill character echoes newline with
 * no feedback and also because of long input lines.
 */
void
noteinp(void)
{

	outline++;
	if (outline > EX_LINES - 1)
		outline = EX_LINES - 1;
	destline = outline;
	destcol = outcol = 0;
}

/*
 * Something weird just happened and we
 * lost track of whats happening out there.
 * Since we cant, in general, read where we are
 * we just reset to some known state.
 * On cursor addressible terminals setting to unknown
 * will force a cursor address soon.
 */
void
termreset(void)
{

	endim();
	if (TI)	/* otherwise it flushes anyway, and 'set tty=dumb' vomits */
		putpad(TI);	 /*adb change -- emit terminal initial sequence */
	destcol = 0;
	destline = EX_LINES - 1;
	if (CA) {
		outcol = UKCOL;
		outline = UKCOL;
	} else {
		outcol = destcol;
		outline = destline;
	}
}

/*
 * Low level buffering, with the ability to drain
 * buffered output without printing it.
 */
char	*obp = obuf;

void
draino(void)
{

	obp = obuf;
}

void
flusho(void)
{

	if (obp != obuf) {
		write(1, obuf, obp - obuf);
		obp = obuf;
	}
}

void
putnl(void)
{

	ex_putchar('\n');
}

#if 0
putS(cp)
	char *cp;
{

	if (cp == NULL)
		return;
	while (*cp)
		putch(*cp++);
}
#endif

int
putch(int c)
{

	c &= 0177;
	*obp++ = c;
	if (obp >= &obuf[sizeof obuf])
		flusho();
	return c;
}

/*
 * Miscellaneous routines related to output.
 */

/*
 * Put with padding
 */
void
putpad(char *cp)
{

	flush();
	tputs(cp, 0, putch);
}

/*
 * Set output through normal command mode routine.
 */
void
setoutt(void)
{

	Outchar = termchar;
}

/*
 * Printf (temporarily) in list mode.
 */
void
lprintf(char *cp, char *dp)
{
	void (*P)();

	P = setlist(1);
	ex_printf(cp, dp);
	Putchar = P;
}

/*
 * Newline + flush.
 */
void
putNFL(void)
{

	putnl();
	flush();
}

/*
 * Try to start -nl mode.
 */
void
pstart(void)
{

	if (NONL)
		return;
 	if (!value(OPTIMIZE))
		return;
	if (ruptible == 0 || pfast)
		return;
	fgoto();
	flusho();
	pfast = 1;
	normtty++;
#ifndef USG3TTY
	tty.sg_flags = normf & ~(ECHO|XTABS|CRMOD);
#else
	tty = normf;
	tty.c_oflag &= ~(ONLCR
# ifdef TAB3
	    |TAB3
# endif
	    );
	tty.c_lflag &= ~ECHO;
#endif
	sTTY(1);
}

/*
 * Stop -nl mode.
 */
void
pstop(void)
{

	if (inopen)
		return;
	phadnl = 0;
	linp = linb;
	draino();
	normal(normf);
	pfast &= ~1;
}

/*
 * Prep tty for open mode.
 */
ttymode
ostart(void)
{
	ttymode f;

	if (!intty)
		error("Open and visual must be used interactively");
	gTTY(1);
	normtty++;
#ifndef USG3TTY
	f = tty.sg_flags;
	tty.sg_flags = (normf &~ (ECHO|XTABS|CRMOD)) |
# ifdef CBREAK
							CBREAK;
# else
							RAW;
# endif
# ifdef TIOCGETC
	ttcharoff();
# endif
#else
	f = tty;
	tty = normf;
	tty.c_iflag &= ~ICRNL;
	tty.c_lflag &= ~(ECHO|ICANON);
# ifdef TAB3
	tty.c_oflag &= ~TAB3;
# endif
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;
	ttcharoff();
#endif
	sTTY(1);
	tostart();
	pfast |= 2;
	return (f);
}

/* actions associated with putting the terminal in open mode */
void
tostart(void)
{
	putpad(VS);
	putpad(KS);
	if (!value(MESG)) {
		if (ttynbuf[0] == 0) {
			register char *tn;
			if ((tn=ttyname(2)) == NULL &&
			    (tn=ttyname(1)) == NULL &&
			    (tn=ttyname(0)) == NULL)
				ttynbuf[0] = 1;
			else
				strcpy(ttynbuf, tn);
		}
		if (ttynbuf[0] != 1) {
			struct stat sbuf;
			stat(ttynbuf, &sbuf);
			ttymesg = sbuf.st_mode & 0777;
			chmod(ttynbuf,
#ifdef UCBV7
	/*
	 * This applies to the UCB V7 Pdp-11 system with the
	 * -u write option only.
	 */
					0611	/* 11 = urgent only allowed */
#else
					0600
#endif
						);
		}
	}
}

/*
 * Turn off start/stop chars if they aren't the default ^S/^Q.
 * This is so idiots who make esc their start/stop don't lose.
 * We always turn off quit since datamedias send ^\ for their
 * right arrow key.
 */
#if defined(TIOCGETC) && !defined(USG3TTY)
ttcharoff()
{
	nttyc.t_quitc = '\377';
	if (nttyc.t_startc != CTRL('q'))
		nttyc.t_startc = '\377';
	if (nttyc.t_stopc != CTRL('s'))
		nttyc.t_stopc = '\377';
# ifdef TIOCLGET
	nlttyc.t_suspc = '\377';	/* ^Z */
	nlttyc.t_dsuspc = '\377';	/* ^Y */
	nlttyc.t_flushc = '\377';	/* ^O */
	nlttyc.t_lnextc = '\377';	/* ^V */
# endif
}
#endif

#ifdef USG3TTY
static void
ttcharoff(void)
{
	long vdisable;

	if ((vdisable = fpathconf(STDIN_FILENO, _PC_VDISABLE)) == -1)
		vdisable = '\377';
	tty.c_cc[VQUIT] = vdisable;
#ifdef VSUSP
	tty.c_cc[VSUSP] = vdisable;
#endif
#ifdef VDSUSP
	tty.c_cc[VDSUSP] = vdisable;
#endif
	/*
	 * The following is sample code if USG ever lets people change
	 * their start/stop chars.  As long as they can't we can't get
	 * into trouble so we just leave them alone.
	 */
# ifdef VSTART
	if (tty.c_cc[VSTART] != CTRL('q'))
		tty.c_cc[VSTART] = vdisable;
# endif
# ifdef VSTOP
	if (tty.c_cc[VSTOP] != CTRL('s'))
		tty.c_cc[VSTOP] = vdisable;
# endif
}
#endif

/*
 * Stop open, restoring tty modes.
 */
void
ostop(ttymode f)
{

#ifndef USG3TTY
	pfast = (f & CRMOD) == 0;
#else
	pfast = (f.c_oflag & OCRNL) == 0;
#endif
	termreset(), fgoto(), flusho();
	normal(f);
	tostop();
}

/* Actions associated with putting the terminal in the right mode. */
void
tostop(void)
{
	putpad(VE);
	putpad(KE);
	if (!value(MESG))
		chmod(ttynbuf, ttymesg);
}

#ifndef CBREAK
/*
 * Into cooked mode for interruptibility.
 */
vcook()
{

	tty.sg_flags &= ~RAW;
	sTTY(1);
}

/*
 * Back into raw mode.
 */
vraw()
{

	tty.sg_flags |= RAW;
	sTTY(1);
}
#endif

/*
 * Restore flags to normal state f.
 */
static void
normal(ttymode f)
{

	if (normtty > 0) {
		setty(f);
		normtty--;
	}
}

/*
 * Straight set of flags to state f.
 */
ttymode
setty(ttymode f)
{
#ifndef USG3TTY
	register int ot = tty.sg_flags;
#else
	ttymode ot;
	ot = tty;
#endif

#ifndef USG3TTY
	if (f == normf) {
		nttyc = ottyc;
# ifdef TIOCLGET
		nlttyc = olttyc;
# endif
	} else
		ttcharoff();
	tty.sg_flags = f;
#else
	if (tty.c_lflag & ICANON)
		ttcharoff();
	tty = f;
#endif
	sTTY(1);
	return (ot);
}

void
gTTY(int i)
{

#ifndef USG3TTY
	ignore(gtty(i, &tty));
# ifdef TIOCGETC
	ioctl(i, TIOCGETC, &ottyc);
	nttyc = ottyc;
# endif
# ifdef TIOCGLTC
	ioctl(i, TIOCGLTC, &olttyc);
	nlttyc = olttyc;
# endif
#else
	tcgetattr(i, &tty);
#endif
}

/*
 * sTTY: set the tty modes on file descriptor i to be what's
 * currently in global "tty".  (Also use nttyc if needed.)
 */
static void
sTTY(int i)
{

#ifndef USG3TTY
# ifdef USG
	/* Bug in USG tty driver, put out a DEL as a patch. */
	if (tty.sg_ospeed >= B1200)
		write(1, "\377", 1);
# endif

# ifdef TIOCSETN
	/* Don't flush typeahead if we don't have to */
	ioctl(i, TIOCSETN, &tty);
# else
	/* We have to.  Too bad. */
	stty(i, &tty);
# endif

# ifdef TIOCGETC
	/* Update the other random chars while we're at it. */
	ioctl(i, TIOCSETC, &nttyc);
# endif
# ifdef TIOCSLTC
	ioctl(i, TIOCSLTC, &nlttyc);
# endif

#else
	/* USG 3 very simple: just set everything */
	tcsetattr(i, TCSAFLUSH, &tty);
#endif
}

/*
 * Print newline, or blank if in open/visual
 */
void
noonl(void)
{

	ex_putchar(Outchar != termchar ? ' ' : '\n');
}

#ifdef SIGTSTP
/*
 * We have just gotten a susp.  Suspend and prepare to resume.
 */
void
onsusp(int i)
{
	ttymode f;

	(void)i;
	f = setty(normf);
	vnfl();
	putpad(TE);
	flush();

	signal(SIGTSTP, SIG_DFL);
	kill(0, SIGTSTP);

	/* the pc stops here */

	signal(SIGTSTP, onsusp);
	vcontin(0);
	setty(f);
	if (!inopen)
		error(0);
	else {
		if (vcnt < 0) {
			vcnt = -vcnt;
			if (state == VISUAL)
				vclear();
			else if (state == CRTOPEN)
				vcnt = 0;
		}
		vdirty(0, EX_LINES);
		vrepaint(cursor);
	}
}
#endif
