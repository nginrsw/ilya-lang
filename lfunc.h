/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in ilya.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define sizeCclosure(n)  \
	(offsetof(CClosure, upvalue) + sizeof(TValue) * cast_uint(n))

#define sizeLclosure(n)  \
	(offsetof(LClosure, upvals) + sizeof(UpVal *) * cast_uint(n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Ilya). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


#define upisopen(up)	((up)->v.p != &(up)->u.value)


#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v.p))


/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10



/* special status to close upvalues preserving the top of the stack */
#define CLOSEKTOP	(-1)


ILYAI_FUNC Proto *ilyaF_newproto (ilya_State *L);
ILYAI_FUNC CClosure *ilyaF_newCclosure (ilya_State *L, int nupvals);
ILYAI_FUNC LClosure *ilyaF_newLclosure (ilya_State *L, int nupvals);
ILYAI_FUNC void ilyaF_initupvals (ilya_State *L, LClosure *cl);
ILYAI_FUNC UpVal *ilyaF_findupval (ilya_State *L, StkId level);
ILYAI_FUNC void ilyaF_newtbcupval (ilya_State *L, StkId level);
ILYAI_FUNC void ilyaF_closeupval (ilya_State *L, StkId level);
ILYAI_FUNC StkId ilyaF_close (ilya_State *L, StkId level, int status, int yy);
ILYAI_FUNC void ilyaF_unlinkupval (UpVal *uv);
ILYAI_FUNC lu_mem ilyaF_protosize (Proto *p);
ILYAI_FUNC void ilyaF_freeproto (ilya_State *L, Proto *f);
ILYAI_FUNC const char *ilyaF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
