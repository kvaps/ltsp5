/**************************************************************************
 * LPRng IFHP Filter
 * Copyright 1994-1997 Patrick Powell, San Diego, CA <papowell@sdsu.edu>
 *
 * Based on the CTI printer filters.
 *  See COPYRIGHT for details.
 *
 * $Id: common.h,v 1.1 2004/07/08 05:03:36 jam Exp $
*/


/****************************************************************************
 * Modification History:
 *  Extracted from ifhp4.c
 *
 * 	Revision 1.11	95/08/22	15:01:07
 *		 Version 1.2 initiated and Porting to Solaris.
 *
 */

#ifndef _COMMON_H
#define _COMMON_H 1

#ifdef EXTERN
# undef EXTERN
# undef DEFINE
# define EXTERN
# define DEFINE(X) X
#else
# undef EXTERN
# undef DEFINE
# define EXTERN extern
# define DEFINE(X)
#endif

#undef _PARMS__
#ifdef __STDC__
#define _PARMS__(X) X
#else
#define _PARMS__(X) ()
#endif

#if defined(HAVE_STDARGS)
void log _PARMS__( (int kind, char *msg,...) );
void fatal _PARMS__( ( char *msg,...) );
void logerr _PARMS__( (int kind, char *msg,...) );
void logerr_die _PARMS__( (int kind, char *msg,...) );
int plp_snprintf (char *str, size_t count, const char *fmt, ...);
int vplp_snprintf (char *str, size_t count, const char *fmt, va_list arg);
#else
void log _PARMS__( (void) );
void fatal _PARMS__( (void) );
void logerr _PARMS__( (void) );
void logerr_die _PARMS__( (void) );
int plp_snprintf ();
int vplp_snprintf ();
#endif

#define JABORT 1
EXTERN char *name;
EXTERN int debug;
EXTERN int errorcode;
void Do_stty( int fd, char *Stty_command );

const char * Errormsg ( int err );

#endif /* _COMMON_H */
