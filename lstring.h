/*
** $Id: lstring.h $
** String table (keep all strings handled by Ilya)
** See Copyright Notice in ilya.h
*/

#ifndef lstring_h
#define lstring_h

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("fn") = 8, #("__newindex") = 10.)
*/
#if !defined(ILYAI_MAXSHORTLEN)
#define ILYAI_MAXSHORTLEN	40
#endif


/*
** Size of a short TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizestrshr(l)  \
	(offsetof(TString, contents) + ((l) + 1) * sizeof(char))


#define ilyaS_newliteral(L, s)	(ilyaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	(strisshr(s) && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == ILYA_VSHRSTR, (a) == (b))


ILYAI_FUNC unsigned ilyaS_hash (const char *str, size_t l, unsigned seed);
ILYAI_FUNC unsigned ilyaS_hashlongstr (TString *ts);
ILYAI_FUNC int ilyaS_eqlngstr (TString *a, TString *b);
ILYAI_FUNC void ilyaS_resize (ilya_State *L, int newsize);
ILYAI_FUNC void ilyaS_clearcache (global_State *g);
ILYAI_FUNC void ilyaS_init (ilya_State *L);
ILYAI_FUNC void ilyaS_remove (ilya_State *L, TString *ts);
ILYAI_FUNC Udata *ilyaS_newudata (ilya_State *L, size_t s,
                                              unsigned short nuvalue);
ILYAI_FUNC TString *ilyaS_newlstr (ilya_State *L, const char *str, size_t l);
ILYAI_FUNC TString *ilyaS_new (ilya_State *L, const char *str);
ILYAI_FUNC TString *ilyaS_createlngstrobj (ilya_State *L, size_t l);
ILYAI_FUNC TString *ilyaS_newextlstr (ilya_State *L,
		const char *s, size_t len, ilya_Alloc falloc, void *ud);
ILYAI_FUNC size_t ilyaS_sizelngstr (size_t len, int kind);

#endif
