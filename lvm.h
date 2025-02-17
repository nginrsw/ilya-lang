/*
** $Id: lvm.h $
** Irin virtual machine
** See Copyright Notice in irin.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(IRIN_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(IRIN_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define IRIN_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(IRIN_FLOORN2I)
#define IRIN_FLOORN2I		F2Ieq
#endif


/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceiling of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */
#define tonumber(o,n) \
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : luaV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : luaV_tointeger(o,i,IRIN_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : luaV_tointegerns(o,i,IRIN_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define luaV_rawequalobj(t1,t2)		luaV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable'
*/
#define luaV_fastget(t,k,res,f, tag) \
  (tag = (!ttistable(t) ? IRIN_VNOTABLE : f(hvalue(t), k, res)))


/*
** Special case of 'luaV_fastget' for integers, inlining the fast case
** of 'luaH_getint'.
*/
#define luaV_fastgeti(t,k,res,tag) \
  if (!ttistable(t)) tag = IRIN_VNOTABLE; \
  else { luaH_fastgeti(hvalue(t), k, res, tag); }


#define luaV_fastset(t,k,val,hres,f) \
  (hres = (!ttistable(t) ? HNOTATABLE : f(hvalue(t), k, val)))

#define luaV_fastseti(t,k,val,hres) \
  if (!ttistable(t)) hres = HNOTATABLE; \
  else { luaH_fastseti(hvalue(t), k, val, hres); }


/*
** Finish a fast set operation (when fast set succeeds).
*/
#define luaV_finishfastset(L,t,v)	luaC_barrierback(L, gcvalue(t), v)


/*
** Shift right is the same as shift left with a negative 'y'
*/
#define luaV_shiftr(x,y)	luaV_shiftl(x,intop(-, 0, y))



LUAI_FUNC int luaV_equalobj (irin_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC int luaV_lessthan (irin_State *L, const TValue *l, const TValue *r);
LUAI_FUNC int luaV_lessequal (irin_State *L, const TValue *l, const TValue *r);
LUAI_FUNC int luaV_tonumber_ (const TValue *obj, irin_Number *n);
LUAI_FUNC int luaV_tointeger (const TValue *obj, irin_Integer *p, F2Imod mode);
LUAI_FUNC int luaV_tointegerns (const TValue *obj, irin_Integer *p,
                                F2Imod mode);
LUAI_FUNC int luaV_flttointeger (irin_Number n, irin_Integer *p, F2Imod mode);
LUAI_FUNC lu_byte luaV_finishget (irin_State *L, const TValue *t, TValue *key,
                                                StkId val, lu_byte tag);
LUAI_FUNC void luaV_finishset (irin_State *L, const TValue *t, TValue *key,
                                             TValue *val, int aux);
LUAI_FUNC void luaV_finishOp (irin_State *L);
LUAI_FUNC void luaV_execute (irin_State *L, CallInfo *ci);
LUAI_FUNC void luaV_concat (irin_State *L, int total);
LUAI_FUNC irin_Integer luaV_idiv (irin_State *L, irin_Integer x, irin_Integer y);
LUAI_FUNC irin_Integer luaV_mod (irin_State *L, irin_Integer x, irin_Integer y);
LUAI_FUNC irin_Number luaV_modf (irin_State *L, irin_Number x, irin_Number y);
LUAI_FUNC irin_Integer luaV_shiftl (irin_Integer x, irin_Integer y);
LUAI_FUNC void luaV_objlen (irin_State *L, StkId ra, const TValue *rb);

#endif
