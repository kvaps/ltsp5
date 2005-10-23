/**************************************************************************
 * LPRng IFHP Filter
 * Copyright 1994-1997 Patrick Powell, San Diego, CA <papowell@sdsu.edu>
 *
 * Based on the CTI printer filters.
 *  See COPYRIGHT for details.
 *
 * $Id: log.c,v 1.1 2004/07/08 05:03:36 jam Exp $
 */

#include "portable.h"
#include "common.h"

#if !defined(ultrix) && defined(HAVE_SYSLOG_H)
# include <syslog.h>
#endif
#if defined(HAVE_SYS_SYSLOG_H)
# include <sys/syslog.h>
#endif

#define PAIR(X) { #X, X }
extern char * Time_str();

struct keys {
	char *name;
	int value;
} log_level[] = {
{"EMERG",LOG_EMERG},
{"ALERT",LOG_ALERT},
{"CRIT",LOG_CRIT},
{"ERR",LOG_ERR},
{"WARNING",LOG_WARNING},
{"NOTICE",LOG_NOTICE},
{"INFO",LOG_INFO},
{"DEBUG",LOG_DEBUG},
{0,0}
};

static char msg_buf[1024];
extern int show_ctrl;

void init_pid()
{
	int len;
	plp_snprintf(msg_buf,sizeof(msg_buf), "%s ", name );
		len = strlen( msg_buf );
		plp_snprintf(msg_buf+len,sizeof(msg_buf)-len,
			"pid [%d] ", (int)(getpid()) );
}

static void logbackend()
{
	int len, original_len;
	original_len = len = strlen( msg_buf );
	len = strlen( msg_buf );
	(void)plp_snprintf(msg_buf+len, sizeof(msg_buf)-len,
		" at %s\n", Time_str());
	(void)fputs(msg_buf, stderr );
	(void)fflush(stderr);
}


/* VARARGS2 */
#ifdef HAVE_STDARGS
void logmsg (int lvl, char *msg,...)
#else
void logmsg (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int lvl;
    char *msg;
#endif
	int len;
    VA_LOCAL_DECL
    VA_START (msg);
    VA_SHIFT (lvl, int);
    VA_SHIFT (msg, char *);

	lvl = show_ctrl;
	show_ctrl = 1;
	init_pid();
	len = strlen(msg_buf);
	(void)vplp_snprintf(msg_buf+len, sizeof(msg_buf)-len, msg, ap );
	logbackend();
	show_ctrl = lvl;
	VA_END;
}

/* VARARGS1 */
#ifdef HAVE_STDARGS
void fatal ( char *msg,...)
#else
void fatal (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int lvl;
    char *msg;
#endif
	int len;
	int lvl;
    VA_LOCAL_DECL
    VA_START (msg);
    VA_SHIFT (msg, char *);

	lvl = show_ctrl;
	show_ctrl = 1;
	init_pid();
	len = strlen(msg_buf);
	(void)vplp_snprintf(msg_buf+len, sizeof(msg_buf)-len, msg, ap );
	logbackend();
	show_ctrl = lvl;
	VA_END;
	exit(errorcode);
}


/* VARARGS2 */
#ifdef HAVE_STDARGS
void logerr (int lvl, char *msg,...)
#else
void logerr (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int lvl;
    char *msg;
#endif
    VA_LOCAL_DECL
	int err = errno;
	int len;

    VA_START (msg);
    VA_SHIFT (lvl, int);
    VA_SHIFT (msg, char *);

	lvl = show_ctrl;
	show_ctrl = 1;

	init_pid();
	len = strlen(msg_buf);
	(void)vplp_snprintf(msg_buf+len, sizeof(msg_buf)-len, msg, ap );
	len = strlen(msg_buf);
	plp_snprintf( msg_buf+len, sizeof(msg_buf)-len, " - %s", Errormsg( err ) );
	logbackend();
	show_ctrl = lvl;
	VA_END;
}


/* VARARGS2 */
#ifdef HAVE_STDARGS
void logerr_die (int lvl, char *msg,...)
#else
void logerr_die (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int lvl;
    char *msg;
#endif
    VA_LOCAL_DECL
    int err = errno;
    int len;

    VA_START (msg);
    VA_SHIFT (lvl, int);
    VA_SHIFT (msg, char *);

	lvl = show_ctrl;
	show_ctrl = 1;

	init_pid();
	len = strlen(msg_buf);
	(void)vplp_snprintf(msg_buf+len, sizeof(msg_buf)-len, msg, ap );
	len = strlen(msg_buf);
	plp_snprintf( msg_buf+len, sizeof(msg_buf)-len, " - %s", Errormsg( err ) );
	logbackend();
    VA_END;
	show_ctrl = lvl;
	exit(errorcode);
}


char * Time_str()
{
	time_t tvec;		/* time */
	char *ctime();
	static char s[40];

	(void)time(&tvec);
	(void)strcpy(s,ctime(&tvec));
	(void)strcpy(s+20,s+24);
	s[strlen(s)-1] = 0;
	return( s+4 );
}

/****************************************************************************
 * char *Errormsg( int err )
 *  returns a printable form of the
 *  errormessage corresponding to the valie of err.
 *  This is the poor man's version of sperror(), not available on all systems
 *  Patrick Powell Tue Apr 11 08:05:05 PDT 1995
 ****************************************************************************/
/****************************************************************************/
#if !defined(HAVE_STRERROR)

# if defined(HAVE_SYS_NERR)
#  if !defined(HAVE_SYS_NERR_DEF)
     extern int sys_nerr;
#  endif
#  define num_errors    (sys_nerr)
# else
#  define num_errors    (-1)            /* always use "errno=%d" */
# endif

# if defined(HAVE_SYS_ERRLIST)
#  if !defined(HAVE_SYS_ERRLIST_DEF)
     extern const char *const sys_errlist[];
#  endif
# else
#  undef  num_errors
#  define num_errors   (-1)            /* always use "errno=%d" */
# endif

#endif

const char * Errormsg ( int err )
{
    const char *cp;

#if defined(HAVE_STRERROR)
	cp = strerror(err);
#else
# if defined(HAVE_SYS_ERRLIST)
    if (err >= 0 && err < num_errors) {
		cp = sys_errlist[err];
    } else
# endif
	{
		static char msgbuf[32];     /* holds "errno=%d". */
		(void) plp_snprintf (msgbuf, sizeof(msgbuf), "errno=%d", err);
		cp = msgbuf;
    }
#endif
    return (cp);
}

/***************************************************************************
 * char *Sigstr(n)
 * Return a printable form the the signal
 ***************************************************************************/

#ifndef HAVE_SYS_SIGLIST

#define PAIR(X) { #X , X }

static struct signame {
    char *str;
    int value;
} signals[] = {
{ "NO SIGNAL", 0 },
#ifdef SIGHUP
PAIR(SIGHUP),
#endif
#ifdef SIGINT
PAIR(SIGINT),
#endif
#ifdef SIGQUIT
PAIR(SIGQUIT),
#endif
#ifdef SIGILL
PAIR(SIGILL),
#endif
#ifdef SIGTRAP
PAIR(SIGTRAP),
#endif
#ifdef SIGIOT
PAIR(SIGIOT),
#endif
#ifdef SIGABRT
PAIR(SIGABRT),
#endif
#ifdef SIGEMT
PAIR(SIGEMT),
#endif
#ifdef SIGFPE
PAIR(SIGFPE),
#endif
#ifdef SIGKILL
PAIR(SIGKILL),
#endif
#ifdef SIGBUS
PAIR(SIGBUS),
#endif
#ifdef SIGSEGV
PAIR(SIGSEGV),
#endif
#ifdef SIGSYS
PAIR(SIGSYS),
#endif
#ifdef SIGPIPE
PAIR(SIGPIPE),
#endif
#ifdef SIGALRM
PAIR(SIGALRM),
#endif
#ifdef SIGTERM
PAIR(SIGTERM),
#endif
#ifdef SIGURG
PAIR(SIGURG),
#endif
#ifdef SIGSTOP
PAIR(SIGSTOP),
#endif
#ifdef SIGTSTP
PAIR(SIGTSTP),
#endif
#ifdef SIGCONT
PAIR(SIGCONT),
#endif
#ifdef SIGCHLD
PAIR(SIGCHLD),
#endif
#ifdef SIGCLD
PAIR(SIGCLD),
#endif
#ifdef SIGTTIN
PAIR(SIGTTIN),
#endif
#ifdef SIGTTOU
PAIR(SIGTTOU),
#endif
#ifdef SIGIO
PAIR(SIGIO),
#endif
#ifdef SIGPOLL
PAIR(SIGPOLL),
#endif
#ifdef SIGXCPU
PAIR(SIGXCPU),
#endif
#ifdef SIGXFSZ
PAIR(SIGXFSZ),
#endif
#ifdef SIGVTALRM
PAIR(SIGVTALRM),
#endif
#ifdef SIGPROF
PAIR(SIGPROF),
#endif
#ifdef SIGWINCH
PAIR(SIGWINCH),
#endif
#ifdef SIGLOST
PAIR(SIGLOST),
#endif
#ifdef SIGUSR1
PAIR(SIGUSR1),
#endif
#ifdef SIGUSR2
PAIR(SIGUSR2),
#endif
{0,0}
    /* that's all */
};

#else /* HAVE_SYS_SIGLIST */
# ifndef HAVE_SYS_SIGLIST_DEF
   extern const char *sys_siglist[];
# endif
#endif

#ifndef NSIG
# define  NSIG 32
#endif

const char *Sigstr (int n)
{
    static char buf[40];
	const char *s = 0;

#ifdef HAVE_SYS_SIGLIST
    if (n < NSIG && n >= 0) {
		s = sys_siglist[n];
	}
#else
	int i;

	for( i = 0; signals[i].str && signals[i].value != n; ++i );
	s = signals[i].str;
#endif
	if( s == 0 ){
		s = buf;
		(void) plp_snprintf (buf, sizeof(buf), "signal %d", n);
	}
    return(s);
}
