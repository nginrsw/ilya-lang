/*
** $Id: ldo.h $
** Stack and Call structure of Ilya
** See Copyright Notice in ilya.h
*/

#ifndef ldo_h
#define ldo_h


#include "llimits.h"
#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/

#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; ilyaD_reallocstack((L), sz_, 0); pos; }
#endif

#define ilyaD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last.p - L->top.p <= (n))) \
	  { pre; ilyaD_growstack(L, n, 1); pos; } \
	else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define ilyaD_checkstack(L,n)	ilyaD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,pt)		(cast_charp(pt) - cast_charp(L->stack.p))
#define restorestack(L,n)	cast(StkId, cast_charp(L->stack.p) + (n))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  ilyaD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p),  /* save 'p' */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(ILYAI_MAXCCALLS)
#define ILYAI_MAXCCALLS		200
#endif


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (ilya_State *L, void *ud);

ILYAI_FUNC void ilyaD_seterrorobj (ilya_State *L, int errcode, StkId oldtop);
ILYAI_FUNC int ilyaD_protectedparser (ilya_State *L, ZIO *z, const char *name,
                                                  const char *mode);
ILYAI_FUNC void ilyaD_hook (ilya_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
ILYAI_FUNC void ilyaD_hookcall (ilya_State *L, CallInfo *ci);
ILYAI_FUNC int ilyaD_pretailcall (ilya_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
ILYAI_FUNC CallInfo *ilyaD_precall (ilya_State *L, StkId func, int nResults);
ILYAI_FUNC void ilyaD_call (ilya_State *L, StkId func, int nResults);
ILYAI_FUNC void ilyaD_callnoyield (ilya_State *L, StkId func, int nResults);
ILYAI_FUNC int ilyaD_closeprotected (ilya_State *L, ptrdiff_t level, int status);
ILYAI_FUNC int ilyaD_pcall (ilya_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
ILYAI_FUNC void ilyaD_poscall (ilya_State *L, CallInfo *ci, int nres);
ILYAI_FUNC int ilyaD_reallocstack (ilya_State *L, int newsize, int raiseerror);
ILYAI_FUNC int ilyaD_growstack (ilya_State *L, int n, int raiseerror);
ILYAI_FUNC void ilyaD_shrinkstack (ilya_State *L);
ILYAI_FUNC void ilyaD_inctop (ilya_State *L);

ILYAI_FUNC l_noret ilyaD_throw (ilya_State *L, int errcode);
ILYAI_FUNC int ilyaD_rawrunprotected (ilya_State *L, Pfunc f, void *ud);

#endif

