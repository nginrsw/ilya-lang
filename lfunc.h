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


LUAI_FUNC Proto *luaF_newproto (ilya_State *L);
LUAI_FUNC CClosure *luaF_newCclosure (ilya_State *L, int nupvals);
LUAI_FUNC LClosure *luaF_newLclosure (ilya_State *L, int nupvals);
LUAI_FUNC void luaF_initupvals (ilya_State *L, LClosure *cl);
LUAI_FUNC UpVal *luaF_findupval (ilya_State *L, StkId level);
LUAI_FUNC void luaF_newtbcupval (ilya_State *L, StkId level);
LUAI_FUNC void luaF_closeupval (ilya_State *L, StkId level);
LUAI_FUNC StkId luaF_close (ilya_State *L, StkId level, int status, int yy);
LUAI_FUNC void luaF_unlinkupval (UpVal *uv);
LUAI_FUNC lu_mem luaF_protosize (Proto *p);
LUAI_FUNC void luaF_freeproto (ilya_State *L, Proto *f);
LUAI_FUNC const char *luaF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
