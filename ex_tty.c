/* Copyright (c) 1980 Regents of the University of California */
/*
static char *sccsid = "@(#)ex_tty.c	6.2 10/30/80";
*/
#include "ex.h"
#include "ex_tty.h"

/*
 * Terminal type initialization routines,
 * and calculation of flags at entry or after
 * a shell escape which may change them.
 */
/* short	ospeed = -1; */

static void zap(void);
static int cost(char *);
static int countnum(int);

void
gettmode(void)
{

#ifndef USG3TTY
	if (gtty(1, &tty) < 0)
		return;
	if (ospeed != tty.sg_ospeed)
		value(SLOWOPEN) = tty.sg_ospeed < B1200;
	ospeed = tty.sg_ospeed;
	normf = tty.sg_flags;
	UPPERCASE = (tty.sg_flags & LCASE) != 0;
	GT = (tty.sg_flags & XTABS) != XTABS && !XT;
	NONL = (tty.sg_flags & CRMOD) == 0;
#else
	if (tcgetattr(1, &tty) < 0)
		return;
	ex_ospeed = cfgetospeed(&tty);
	value(SLOWOPEN) = ex_ospeed < B1200;
	normf = tty;
#ifdef IUCLC
	UPPERCASE = (tty.c_iflag & IUCLC) != 0;
#endif
# ifdef TAB3
	GT = (tty.c_oflag & TABDLY) != TAB3 && !XT;
# endif
	NONL = (tty.c_oflag & OCRNL) == 0;
#endif
}

char *xPC;
char **sstrs[] = {
	&AL, &BC, &BT, &CD, &CE, &CL, &CM, &xCR, &DC, &DL, &DM, &DO, &ED, &EI,
	&F0, &F1, &F2, &F3, &F4, &F5, &F6, &F7, &F8, &F9,
	&HO, &IC, &IM, &IP, &KD, &KE, &KH, &KL, &KR, &KS, &KU, &LL,
	&ND, &xNL, &xPC, &SE, &SF, &SO, &SR, &TA, &TE, &TI, &UP, &VB, &VS, &VE
};
bool *sflags[] = {
	&AM, &BS, &DA, &DB, &EO, &HC, &HZ, &IN, &MI, &NC, &NS, &OS, &UL,
	&XB, &XN, &XT, &XX
};
char **fkeys[10] = {
	&F0, &F1, &F2, &F3, &F4, &F5, &F6, &F7, &F8, &F9
};

void
setterm(char *type)
{
	char *tgoto();
	register int unknown, i;
	register int l;
	char ltcbuf[TCBUFSIZE];

	if (type[0] == 0)
		type = "xx";
	unknown = 0;
	putpad(TE);
	if (tgetent(ltcbuf, type) != 1) {
		unknown++;
		CP(ltcbuf, "xx|dumb:");
	}
	gettmode();
	i = EX_LINES = tgetnum("li");
	if (EX_LINES <= 5)
		EX_LINES = 24;
	if (EX_LINES > TUBELINES)
		EX_LINES = TUBELINES;
	l = EX_LINES;
	if (ex_ospeed < B1200)
		l = 9;	/* including the message line at the bottom */
	else if (ex_ospeed < B2400)
		l = 17;
	if (l > EX_LINES)
		l = EX_LINES;
	aoftspace = tspace;
	zap();
	/*
	 * Initialize keypad arrow keys.
	 */
	arrows[0].cap = KU; arrows[0].mapto = "k"; arrows[0].descr = "up";
	arrows[1].cap = KD; arrows[1].mapto = "j"; arrows[1].descr = "down";
	arrows[2].cap = KL; arrows[2].mapto = "h"; arrows[2].descr = "left";
	arrows[3].cap = KR; arrows[3].mapto = "l"; arrows[3].descr = "right";
	arrows[4].cap = KH; arrows[4].mapto = "H"; arrows[4].descr = "home";

#if 0 /*def TIOCLGET*/
	/*
	 * Now map users susp char to ^Z, being careful that the susp
	 * overrides any arrow key, but only for hackers (=new tty driver).
	 */
	{
		static char sc[2];
		int i, fnd;

		ioctl(0, TIOCGETD, &ldisc);
		if (ldisc == NTTYDISC) {
			sc[0] = olttyc.t_suspc;
			sc[1] = 0;
			if (olttyc.t_suspc == CTRL('z')) {
				for (i=0; i<=4; i++)
					if (arrows[i].cap[0] == CTRL('z'))
						addmac(sc, NULL, NULL, arrows);
			} else
				addmac(sc, "\32", "susp", arrows);
		}
	}
#endif

	options[WINDOW].ovalue = options[WINDOW].odefault = l - 1;
	if (defwind) options[WINDOW].ovalue = defwind;
	options[SCROLL].ovalue = options[SCROLL].odefault = HC ? 11 : ((l-1) / 2);
	COLUMNS = tgetnum("co");
	if (COLUMNS <= 4)
		COLUMNS = 1000;
	if (!CM || tgoto(CM, 2, 2)[0] == 'O')	/* OOPS */
		CA = 0, CM = 0;
	else
		CA = 1, costCM = cost(tgoto(CM, 8, 10));
	costSR = cost(SR);
	costAL = cost(AL);
	PC = xPC ? xPC[0] : 0;
	aoftspace = tspace;
	CP(ttytype, longname(ltcbuf, type));
	if (i <= 0)
		EX_LINES = 2;
	/* proper strings to change tty type */
	termreset();
	value(REDRAW) = AL && DL;
	value(OPTIMIZE) = !CA && !GT;
	if (ex_ospeed == B1200 && !value(REDRAW))
		value(SLOWOPEN) = 1;	/* see also gettmode above */
	if (unknown)
		serror("%s: Unknown terminal type", type);
}

static void
zap(void)
{
	register char *namp;
	register bool **fp;
	register char ***sp;

 	namp = "ambsdadbeohchzinmincnsosulxbxnxtxx";
	fp = sflags;
	do {
		*(*fp++) = tgetflag(namp);
		namp += 2;
	} while (*namp);
	namp = "albcbtcdceclcmcrdcdldmdoedeik0k1k2k3k4k5k6k7k8k9hoicimipkdkekhklkrkskullndnlpcsesfsosrtatetiupvbvsve";
	sp = sstrs;
	do {
		*(*sp++) = tgetstr(namp, &aoftspace);
		namp += 2;
	} while (*namp);
}

char *
longname(char *bp, char *def)
{
	register char *cp;

	while (*bp && *bp != ':' && *bp != '|')
		bp++;
	if (*bp == '|') {
		bp++;
		cp = bp;
		while (*cp && *cp != ':' && *cp != '|')
			cp++;
		*cp = 0;
		return (bp);
	}
	return (def);
}

char *
fkey(int i)
{
	if (0 <= i && i <= 9)
		return(*fkeys[i]);
	else
		return(NOSTR);
}

/*
 * cost figures out how much (in characters) it costs to send the string
 * str to the terminal.  It takes into account padding information, as
 * much as it can, for a typical case.  (Right now the typical case assumes
 * the number of lines affected is the size of the screen, since this is
 * mainly used to decide if AL or SR is better, and this always happens
 * at the top of the screen.  We assume cursor motion (CM) has little
 * padding, if any, required, so that case, which is really more important
 * than AL vs SR, won't be really affected.)
 */
static int costnum;

static int
cost(char *str)
{
	if (str == NULL)
		return 10000;	/* infinity */
	costnum = 0;
	tputs(str, EX_LINES, countnum);
	return costnum;
}

static int
countnum(int ch)
{
	(void)ch;
	return costnum++;
}
