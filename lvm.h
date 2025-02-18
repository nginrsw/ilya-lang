/*
** $Id: lvm.h $
** Ilya virtual machine
** See Copyright Notice in ilya.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(ILYA_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(ILYA_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define ILYA_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(ILYA_FLOORN2I)
#define ILYA_FLOORN2I		F2Ieq
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
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : ilyaV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : ilyaV_tointeger(o,i,ILYA_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : ilyaV_tointegerns(o,i,ILYA_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define ilyaV_rawequalobj(t1,t2)		ilyaV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable'
*/
#define ilyaV_fastget(t,k,res,f, tag) \
  (tag = (!ttistable(t) ? ILYA_VNOTABLE : f(hvalue(t), k, res)))


/*
** Special case of 'ilyaV_fastget' for integers, inlining the fast case
** of 'ilyaH_getint'.
*/
#define ilyaV_fastgeti(t,k,res,tag) \
  if (!ttistable(t)) tag = ILYA_VNOTABLE; \
  else { ilyaH_fastgeti(hvalue(t), k, res, tag); }


#define ilyaV_fastset(t,k,val,hres,f) \
  (hres = (!ttistable(t) ? HNOTATABLE : f(hvalue(t), k, val)))

#define ilyaV_fastseti(t,k,val,hres) \
  if (!ttistable(t)) hres = HNOTATABLE; \
  else { ilyaH_fastseti(hvalue(t), k, val, hres); }


/*
** Finish a fast set operation (when fast set succeeds).
*/
#define ilyaV_finishfastset(L,t,v)	ilyaC_barrierback(L, gcvalue(t), v)


/*
** Shift right is the same as shift left with a negative 'y'
*/
#define ilyaV_shiftr(x,y)	ilyaV_shiftl(x,intop(-, 0, y))



ILYAI_FUNC int ilyaV_equalobj (ilya_State *L, const TValue *t1, const TValue *t2);
ILYAI_FUNC int ilyaV_lessthan (ilya_State *L, const TValue *l, const TValue *r);
ILYAI_FUNC int ilyaV_lessequal (ilya_State *L, const TValue *l, const TValue *r);
ILYAI_FUNC int ilyaV_tonumber_ (const TValue *obj, ilya_Number *n);
ILYAI_FUNC int ilyaV_tointeger (const TValue *obj, ilya_Integer *p, F2Imod mode);
ILYAI_FUNC int ilyaV_tointegerns (const TValue *obj, ilya_Integer *p,
                                F2Imod mode);
ILYAI_FUNC int ilyaV_flttointeger (ilya_Number n, ilya_Integer *p, F2Imod mode);
ILYAI_FUNC lu_byte ilyaV_finishget (ilya_State *L, const TValue *t, TValue *key,
                                                StkId val, lu_byte tag);
ILYAI_FUNC void ilyaV_finishset (ilya_State *L, const TValue *t, TValue *key,
                                             TValue *val, int aux);
ILYAI_FUNC void ilyaV_finishOp (ilya_State *L);
ILYAI_FUNC void ilyaV_execute (ilya_State *L, CallInfo *ci);
ILYAI_FUNC void ilyaV_concat (ilya_State *L, int total);
ILYAI_FUNC ilya_Integer ilyaV_idiv (ilya_State *L, ilya_Integer x, ilya_Integer y);
ILYAI_FUNC ilya_Integer ilyaV_mod (ilya_State *L, ilya_Integer x, ilya_Integer y);
ILYAI_FUNC ilya_Number ilyaV_modf (ilya_State *L, ilya_Number x, ilya_Number y);
ILYAI_FUNC ilya_Integer ilyaV_shiftl (ilya_Integer x, ilya_Integer y);
ILYAI_FUNC void ilyaV_objlen (ilya_State *L, StkId ra, const TValue *rb);

#endif
