/*
** $Id: lapi.c $
** Ilya API
** See Copyright Notice in ilya.h
*/

#define lapi_c
#define ILYA_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "ilya.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char ilya_ident[] =
  "$IlyaVersion: " ILYA_COPYRIGHT " $"
  "$IlyaAuthors: " ILYA_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
*/
#define isvalid(L, o)	((o) != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= ILYA_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < ILYA_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (ilya_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, idx <= ci->top.p - (ci->func.p + 1), "unacceptable index");
    if (o >= L->top.p) return &G(L)->nilvalue;
    else return s2v(o);
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    return s2v(L->top.p + idx);
  }
  else if (idx == ILYA_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = ILYA_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func.p))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func.p));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C fn or Ilya fn (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func.p)), "caller not a C fn");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
static StkId index2stack (ilya_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, o < L->top.p, "invalid index");
    return o;
  }
  else {    /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top.p + idx;
  }
}


ILYA_API int ilya_checkstack (ilya_State *L, int n) {
  int res;
  CallInfo *ci;
  ilya_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last.p - L->top.p > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = ilyaD_growstack(L, n, 0);
  if (res && ci->top.p < L->top.p + n)
    ci->top.p = L->top.p + n;  /* adjust frame top */
  ilya_unlock(L);
  return res;
}


ILYA_API void ilya_xmove (ilya_State *from, ilya_State *to, int n) {
  int i;
  if (from == to) return;
  ilya_lock(to);
  api_checkpop(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top.p - to->top.p >= n, "stack overflow");
  from->top.p -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top.p, from->top.p + i);
    to->top.p++;  /* stack already checked by previous 'api_check' */
  }
  ilya_unlock(to);
}


ILYA_API ilya_CFunction ilya_atpanic (ilya_State *L, ilya_CFunction panicf) {
  ilya_CFunction old;
  ilya_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  ilya_unlock(L);
  return old;
}


ILYA_API ilya_Number ilya_version (ilya_State *L) {
  UNUSED(L);
  return ILYA_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
ILYA_API int ilya_absindex (ilya_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top.p - L->ci->func.p) + idx;
}


ILYA_API int ilya_gettop (ilya_State *L) {
  return cast_int(L->top.p - (L->ci->func.p + 1));
}


ILYA_API void ilya_settop (ilya_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  ilya_lock(L);
  ci = L->ci;
  func = ci->func.p;
  if (idx >= 0) {
    api_check(L, idx <= ci->top.p - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top.p;
    for (; diff > 0; diff--)
      setnilvalue(s2v(L->top.p++));  /* clear new slots */
  }
  else {
    api_check(L, -(idx+1) <= (L->top.p - (func + 1)), "invalid new top");
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  newtop = L->top.p + diff;
  if (diff < 0 && L->tbclist.p >= newtop) {
    ilya_assert(ci->callstatus & CIST_TBC);
    newtop = ilyaF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top.p = newtop;  /* correct top only after closing any upvalue */
  ilya_unlock(L);
}


ILYA_API void ilya_closeslot (ilya_State *L, int idx) {
  StkId level;
  ilya_lock(L);
  level = index2stack(L, idx);
  api_check(L, (L->ci->callstatus & CIST_TBC) && (L->tbclist.p == level),
     "no variable to close at given level");
  level = ilyaF_close(L, level, CLOSEKTOP, 0);
  setnilvalue(s2v(level));
  ilya_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'ilya_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
static void reverse (ilya_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
ILYA_API void ilya_rotate (ilya_State *L, int idx, int n) {
  StkId p, t, m;
  ilya_lock(L);
  t = L->top.p - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, L->tbclist.p < p, "moving a to-be-closed slot");
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  ilya_unlock(L);
}


ILYA_API void ilya_copy (ilya_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  ilya_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* fn upvalue? */
    ilyaC_barrier(L, clCvalue(s2v(L->ci->func.p)), fr);
  /* ILYA_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  ilya_unlock(L);
}


ILYA_API void ilya_pushvalue (ilya_State *L, int idx) {
  ilya_lock(L);
  setobj2s(L, L->top.p, index2value(L, idx));
  api_incr_top(L);
  ilya_unlock(L);
}



/*
** access functions (stack -> C)
*/


ILYA_API int ilya_type (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : ILYA_TNONE);
}


ILYA_API const char *ilya_typename (ilya_State *L, int t) {
  UNUSED(L);
  api_check(L, ILYA_TNONE <= t && t < ILYA_NUMTYPES, "invalid type");
  return ttypename(t);
}


ILYA_API int ilya_iscfunction (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


ILYA_API int ilya_isinteger (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


ILYA_API int ilya_isnumber (ilya_State *L, int idx) {
  ilya_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


ILYA_API int ilya_isstring (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


ILYA_API int ilya_isuserdata (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


ILYA_API int ilya_rawequal (ilya_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? ilyaV_rawequalobj(o1, o2) : 0;
}


ILYA_API void ilya_arith (ilya_State *L, int op) {
  ilya_lock(L);
  if (op != ILYA_OPUNM && op != ILYA_OPBNOT)
    api_checkpop(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checkpop(L, 1);
    setobjs2s(L, L->top.p, L->top.p - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  ilyaO_arith(L, op, s2v(L->top.p - 2), s2v(L->top.p - 1), L->top.p - 2);
  L->top.p--;  /* pop second operand */
  ilya_unlock(L);
}


ILYA_API int ilya_compare (ilya_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  ilya_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case ILYA_OPEQ: i = ilyaV_equalobj(L, o1, o2); break;
      case ILYA_OPLT: i = ilyaV_lessthan(L, o1, o2); break;
      case ILYA_OPLE: i = ilyaV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  ilya_unlock(L);
  return i;
}


ILYA_API unsigned (ilya_numbertocstring) (ilya_State *L, int idx, char *buff) {
  const TValue *o = index2value(L, idx);
  if (ttisnumber(o)) {
    unsigned len = ilyaO_tostringbuff(o, buff);
    buff[len++] = '\0';  /* add final zero */
    return len;
  }
  else
    return 0;
}


ILYA_API size_t ilya_stringtonumber (ilya_State *L, const char *s) {
  size_t sz = ilyaO_str2num(s, s2v(L->top.p));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


ILYA_API ilya_Number ilya_tonumberx (ilya_State *L, int idx, int *pisnum) {
  ilya_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


ILYA_API ilya_Integer ilya_tointegerx (ilya_State *L, int idx, int *pisnum) {
  ilya_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


ILYA_API int ilya_toboolean (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


ILYA_API const char *ilya_tolstring (ilya_State *L, int idx, size_t *len) {
  TValue *o;
  ilya_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      ilya_unlock(L);
      return NULL;
    }
    ilyaO_tostring(L, o);
    ilyaC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  ilya_unlock(L);
  if (len != NULL)
    return getlstr(tsvalue(o), *len);
  else
    return getstr(tsvalue(o));
}


ILYA_API ilya_Unsigned ilya_rawlen (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case ILYA_VSHRSTR: return cast(ilya_Unsigned, tsvalue(o)->shrlen);
    case ILYA_VLNGSTR: return cast(ilya_Unsigned, tsvalue(o)->u.lnglen);
    case ILYA_VUSERDATA: return cast(ilya_Unsigned, uvalue(o)->len);
    case ILYA_VTABLE: return ilyaH_getn(hvalue(o));
    default: return 0;
  }
}


ILYA_API ilya_CFunction ilya_tocfunction (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C fn */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case ILYA_TUSERDATA: return getudatamem(uvalue(o));
    case ILYA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


ILYA_API void *ilya_touserdata (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


ILYA_API ilya_State *ilya_tothread (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** fn to a 'void*', so the conversion here goes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
ILYA_API const void *ilya_topointer (ilya_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case ILYA_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case ILYA_VUSERDATA: case ILYA_VLIGHTUSERDATA:
      return touserdata(o);
    default: {
      if (iscollectable(o))
        return gcvalue(o);
      else
        return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


ILYA_API void ilya_pushnil (ilya_State *L) {
  ilya_lock(L);
  setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  ilya_unlock(L);
}


ILYA_API void ilya_pushnumber (ilya_State *L, ilya_Number n) {
  ilya_lock(L);
  setfltvalue(s2v(L->top.p), n);
  api_incr_top(L);
  ilya_unlock(L);
}


ILYA_API void ilya_pushinteger (ilya_State *L, ilya_Integer n) {
  ilya_lock(L);
  setivalue(s2v(L->top.p), n);
  api_incr_top(L);
  ilya_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
ILYA_API const char *ilya_pushlstring (ilya_State *L, const char *s, size_t len) {
  TString *ts;
  ilya_lock(L);
  ts = (len == 0) ? ilyaS_new(L, "") : ilyaS_newlstr(L, s, len);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  ilyaC_checkGC(L);
  ilya_unlock(L);
  return getstr(ts);
}


ILYA_API const char *ilya_pushexternalstring (ilya_State *L,
	        const char *s, size_t len, ilya_Alloc falloc, void *ud) {
  TString *ts;
  ilya_lock(L);
  api_check(L, len <= MAX_SIZE, "string too large");
  api_check(L, s[len] == '\0', "string not ending with zero");
  ts = ilyaS_newextlstr (L, s, len, falloc, ud);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  ilyaC_checkGC(L);
  ilya_unlock(L);
  return getstr(ts);
}


ILYA_API const char *ilya_pushstring (ilya_State *L, const char *s) {
  ilya_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top.p));
  else {
    TString *ts;
    ts = ilyaS_new(L, s);
    setsvalue2s(L, L->top.p, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  ilyaC_checkGC(L);
  ilya_unlock(L);
  return s;
}


ILYA_API const char *ilya_pushvfstring (ilya_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  ilya_lock(L);
  ret = ilyaO_pushvfstring(L, fmt, argp);
  ilyaC_checkGC(L);
  ilya_unlock(L);
  return ret;
}


ILYA_API const char *ilya_pushfstring (ilya_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  ilya_lock(L);
  va_start(argp, fmt);
  ret = ilyaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  ilyaC_checkGC(L);
  if (ret == NULL)  /* error? */
    ilyaD_throw(L, ILYA_ERRMEM);
  ilya_unlock(L);
  return ret;
}


ILYA_API void ilya_pushcclosure (ilya_State *L, ilya_CFunction fn, int n) {
  ilya_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top.p), fn);
    api_incr_top(L);
  }
  else {
    int i;
    CClosure *cl;
    api_checkpop(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = ilyaF_newCclosure(L, n);
    cl->f = fn;
    for (i = 0; i < n; i++) {
      setobj2n(L, &cl->upvalue[i], s2v(L->top.p - n + i));
      /* does not need barrier because closure is white */
      ilya_assert(iswhite(cl));
    }
    L->top.p -= n;
    setclCvalue(L, s2v(L->top.p), cl);
    api_incr_top(L);
    ilyaC_checkGC(L);
  }
  ilya_unlock(L);
}


ILYA_API void ilya_pushboolean (ilya_State *L, int b) {
  ilya_lock(L);
  if (b)
    setbtvalue(s2v(L->top.p));
  else
    setbfvalue(s2v(L->top.p));
  api_incr_top(L);
  ilya_unlock(L);
}


ILYA_API void ilya_pushlightuserdata (ilya_State *L, void *p) {
  ilya_lock(L);
  setpvalue(s2v(L->top.p), p);
  api_incr_top(L);
  ilya_unlock(L);
}


ILYA_API int ilya_pushthread (ilya_State *L) {
  ilya_lock(L);
  setthvalue(L, s2v(L->top.p), L);
  api_incr_top(L);
  ilya_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Ilya -> stack)
*/


static int auxgetstr (ilya_State *L, const TValue *t, const char *k) {
  lu_byte tag;
  TString *str = ilyaS_new(L, k);
  ilyaV_fastget(t, str, s2v(L->top.p), ilyaH_getstr, tag);
  if (!tagisempty(tag))
    api_incr_top(L);
  else {
    setsvalue2s(L, L->top.p, str);
    api_incr_top(L);
    tag = ilyaV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  }
  ilya_unlock(L);
  return novariant(tag);
}


static void getGlobalTable (ilya_State *L, TValue *gt) {
  Table *registry = hvalue(&G(L)->l_registry);
  lu_byte tag = ilyaH_getint(registry, ILYA_RIDX_GLOBALS, gt);
  (void)tag;  /* avoid not-used warnings when checks are off */
  api_check(L, novariant(tag) == ILYA_TTABLE, "global table must exist");
}


ILYA_API int ilya_getglobal (ilya_State *L, const char *name) {
  TValue gt;
  ilya_lock(L);
  getGlobalTable(L, &gt);
  return auxgetstr(L, &gt, name);
}


ILYA_API int ilya_gettable (ilya_State *L, int idx) {
  lu_byte tag;
  TValue *t;
  ilya_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  ilyaV_fastget(t, s2v(L->top.p - 1), s2v(L->top.p - 1), ilyaH_get, tag);
  if (tagisempty(tag))
    tag = ilyaV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  ilya_unlock(L);
  return novariant(tag);
}


ILYA_API int ilya_getfield (ilya_State *L, int idx, const char *k) {
  ilya_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


ILYA_API int ilya_geti (ilya_State *L, int idx, ilya_Integer n) {
  TValue *t;
  lu_byte tag;
  ilya_lock(L);
  t = index2value(L, idx);
  ilyaV_fastgeti(t, n, s2v(L->top.p), tag);
  if (tagisempty(tag)) {
    TValue key;
    setivalue(&key, n);
    tag = ilyaV_finishget(L, t, &key, L->top.p, tag);
  }
  api_incr_top(L);
  ilya_unlock(L);
  return novariant(tag);
}


static int finishrawget (ilya_State *L, lu_byte tag) {
  if (tagisempty(tag))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  ilya_unlock(L);
  return novariant(tag);
}


l_sinline Table *gettable (ilya_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


ILYA_API int ilya_rawget (ilya_State *L, int idx) {
  Table *t;
  lu_byte tag;
  ilya_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  tag = ilyaH_get(t, s2v(L->top.p - 1), s2v(L->top.p - 1));
  L->top.p--;  /* pop key */
  return finishrawget(L, tag);
}


ILYA_API int ilya_rawgeti (ilya_State *L, int idx, ilya_Integer n) {
  Table *t;
  lu_byte tag;
  ilya_lock(L);
  t = gettable(L, idx);
  ilyaH_fastgeti(t, n, s2v(L->top.p), tag);
  return finishrawget(L, tag);
}


ILYA_API int ilya_rawgetp (ilya_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  ilya_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, ilyaH_get(t, &k, s2v(L->top.p)));
}


ILYA_API void ilya_createtable (ilya_State *L, int narray, int nrec) {
  Table *t;
  ilya_lock(L);
  t = ilyaH_new(L);
  sethvalue2s(L, L->top.p, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    ilyaH_resize(L, t, cast_uint(narray), cast_uint(nrec));
  ilyaC_checkGC(L);
  ilya_unlock(L);
}


ILYA_API int ilya_getmetatable (ilya_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  ilya_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case ILYA_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case ILYA_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue2s(L, L->top.p, mt);
    api_incr_top(L);
    res = 1;
  }
  ilya_unlock(L);
  return res;
}


ILYA_API int ilya_getiuservalue (ilya_State *L, int idx, int n) {
  TValue *o;
  int t;
  ilya_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top.p));
    t = ILYA_TNONE;
  }
  else {
    setobj2s(L, L->top.p, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top.p));
  }
  api_incr_top(L);
  ilya_unlock(L);
  return t;
}


/*
** set functions (stack -> Ilya)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (ilya_State *L, const TValue *t, const char *k) {
  int hres;
  TString *str = ilyaS_new(L, k);
  api_checkpop(L, 1);
  ilyaV_fastset(t, str, s2v(L->top.p - 1), hres, ilyaH_psetstr);
  if (hres == HOK) {
    ilyaV_finishfastset(L, t, s2v(L->top.p - 1));
    L->top.p--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top.p, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    ilyaV_finishset(L, t, s2v(L->top.p - 1), s2v(L->top.p - 2), hres);
    L->top.p -= 2;  /* pop value and key */
  }
  ilya_unlock(L);  /* locked done by caller */
}


ILYA_API void ilya_setglobal (ilya_State *L, const char *name) {
  TValue gt;
  ilya_lock(L);  /* unlock done in 'auxsetstr' */
  getGlobalTable(L, &gt);
  auxsetstr(L, &gt, name);
}


ILYA_API void ilya_settable (ilya_State *L, int idx) {
  TValue *t;
  int hres;
  ilya_lock(L);
  api_checkpop(L, 2);
  t = index2value(L, idx);
  ilyaV_fastset(t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres, ilyaH_pset);
  if (hres == HOK) {
    ilyaV_finishfastset(L, t, s2v(L->top.p - 1));
  }
  else
    ilyaV_finishset(L, t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres);
  L->top.p -= 2;  /* pop index and value */
  ilya_unlock(L);
}


ILYA_API void ilya_setfield (ilya_State *L, int idx, const char *k) {
  ilya_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


ILYA_API void ilya_seti (ilya_State *L, int idx, ilya_Integer n) {
  TValue *t;
  int hres;
  ilya_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  ilyaV_fastseti(t, n, s2v(L->top.p - 1), hres);
  if (hres == HOK)
    ilyaV_finishfastset(L, t, s2v(L->top.p - 1));
  else {
    TValue temp;
    setivalue(&temp, n);
    ilyaV_finishset(L, t, &temp, s2v(L->top.p - 1), hres);
  }
  L->top.p--;  /* pop value */
  ilya_unlock(L);
}


static void aux_rawset (ilya_State *L, int idx, TValue *key, int n) {
  Table *t;
  ilya_lock(L);
  api_checkpop(L, n);
  t = gettable(L, idx);
  ilyaH_set(L, t, key, s2v(L->top.p - 1));
  invalidateTMcache(t);
  ilyaC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p -= n;
  ilya_unlock(L);
}


ILYA_API void ilya_rawset (ilya_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top.p - 2), 2);
}


ILYA_API void ilya_rawsetp (ilya_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


ILYA_API void ilya_rawseti (ilya_State *L, int idx, ilya_Integer n) {
  Table *t;
  ilya_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  ilyaH_setint(L, t, n, s2v(L->top.p - 1));
  ilyaC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p--;
  ilya_unlock(L);
}


ILYA_API int ilya_setmetatable (ilya_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  ilya_lock(L);
  api_checkpop(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top.p - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top.p - 1)), "table expected");
    mt = hvalue(s2v(L->top.p - 1));
  }
  switch (ttype(obj)) {
    case ILYA_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        ilyaC_objbarrier(L, gcvalue(obj), mt);
        ilyaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case ILYA_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        ilyaC_objbarrier(L, uvalue(obj), mt);
        ilyaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top.p--;
  ilya_unlock(L);
  return 1;
}


ILYA_API int ilya_setiuservalue (ilya_State *L, int idx, int n) {
  TValue *o;
  int res;
  ilya_lock(L);
  api_checkpop(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top.p - 1));
    ilyaC_barrierback(L, gcvalue(o), s2v(L->top.p - 1));
    res = 1;
  }
  L->top.p--;
  ilya_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Ilya code)
*/


#define checkresults(L,na,nr) \
     (api_check(L, (nr) == ILYA_MULTRET \
               || (L->ci->top.p - L->top.p >= (nr) - (na)), \
	"results from fn overflow current stack size"), \
      api_check(L, ILYA_MULTRET <= (nr) && (nr) <= MAXRESULTS,  \
                   "invalid number of results"))


ILYA_API void ilya_callk (ilya_State *L, int nargs, int nresults,
                        ilya_KContext ctx, ilya_KFunction k) {
  StkId func;
  ilya_lock(L);
  api_check(L, k == NULL || !isIlya(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == ILYA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top.p - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    ilyaD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    ilyaD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  ilya_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (ilya_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  ilyaD_callnoyield(L, c->func, c->nresults);
}



ILYA_API int ilya_pcallk (ilya_State *L, int nargs, int nresults, int errfunc,
                        ilya_KContext ctx, ilya_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  ilya_lock(L);
  api_check(L, k == NULL || !isIlya(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == ILYA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a fn");
    func = savestack(L, o);
  }
  c.func = L->top.p - (nargs+1);  /* fn to be called */
  if (k == NULL || !yieldable(L)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = ilyaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* fn can do error recovery */
    ilyaD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = ILYA_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  ilya_unlock(L);
  return status;
}


ILYA_API int ilya_load (ilya_State *L, ilya_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  ilya_lock(L);
  if (!chunkname) chunkname = "?";
  ilyaZ_init(L, &z, reader, data);
  status = ilyaD_protectedparser(L, &z, chunkname, mode);
  if (status == ILYA_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top.p - 1));  /* get new fn */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      TValue gt;
      getGlobalTable(L, &gt);
      /* set global table as 1st upvalue of 'f' (may be ILYA_ENV) */
      setobj(L, f->upvals[0]->v.p, &gt);
      ilyaC_barrier(L, f->upvals[0], &gt);
    }
  }
  ilya_unlock(L);
  return status;
}


/*
** Dump a Ilya fn, calling 'writer' to write its parts. Ensure
** the stack returns with its original size.
*/
ILYA_API int ilya_dump (ilya_State *L, ilya_Writer writer, void *data, int strip) {
  int status;
  ptrdiff_t otop = savestack(L, L->top.p);  /* original top */
  TValue *f = s2v(L->top.p - 1);  /* fn to be dumped */
  ilya_lock(L);
  api_checkpop(L, 1);
  api_check(L, isLfunction(f), "Ilya fn expected");
  status = ilyaU_dump(L, clLvalue(f)->p, writer, data, strip);
  L->top.p = restorestack(L, otop);  /* restore top */
  ilya_unlock(L);
  return status;
}


ILYA_API int ilya_status (ilya_State *L) {
  return L->status;
}


/*
** Garbage-collection fn
*/
ILYA_API int ilya_gc (ilya_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & (GCSTPGC | GCSTPCLS))  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  ilya_lock(L);
  va_start(argp, what);
  switch (what) {
    case ILYA_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case ILYA_GCRESTART: {
      ilyaE_setdebt(g, 0);
      g->gcstp = 0;  /* (other bits must be zero here) */
      break;
    }
    case ILYA_GCCOLLECT: {
      ilyaC_fullgc(L, 0);
      break;
    }
    case ILYA_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case ILYA_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case ILYA_GCSTEP: {
      lu_byte oldstp = g->gcstp;
      l_mem n = cast(l_mem, va_arg(argp, size_t));
      int work = 0;  /* true if GC did some work */
      g->gcstp = 0;  /* allow GC to run (other bits must be zero here) */
      if (n <= 0)
        n = g->GCdebt;  /* force to run one basic step */
      ilyaE_setdebt(g, g->GCdebt - n);
      ilyaC_condGC(L, (void)0, work = 1);
      if (work && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      g->gcstp = oldstp;  /* restore previous state */
      break;
    }
    case ILYA_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case ILYA_GCGEN: {
      res = (g->gckind == KGC_INC) ? ILYA_GCINC : ILYA_GCGEN;
      ilyaC_changemode(L, KGC_GENMINOR);
      break;
    }
    case ILYA_GCINC: {
      res = (g->gckind == KGC_INC) ? ILYA_GCINC : ILYA_GCGEN;
      ilyaC_changemode(L, KGC_INC);
      break;
    }
    case ILYA_GCPARAM: {
      int param = va_arg(argp, int);
      int value = va_arg(argp, int);
      api_check(L, 0 <= param && param < ILYA_GCPN, "invalid parameter");
      res = cast_int(ilyaO_applyparam(g->gcparams[param], 100));
      if (value >= 0)
        g->gcparams[param] = ilyaO_codeparam(cast_uint(value));
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  ilya_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


ILYA_API int ilya_error (ilya_State *L) {
  TValue *errobj;
  ilya_lock(L);
  errobj = s2v(L->top.p - 1);
  api_checkpop(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    ilyaM_error(L);  /* raise a memory error */
  else
    ilyaG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


ILYA_API int ilya_next (ilya_State *L, int idx) {
  Table *t;
  int more;
  ilya_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  more = ilyaH_next(L, t, L->top.p - 1);
  if (more)
    api_incr_top(L);
  else  /* no more elements */
    L->top.p--;  /* pop key */
  ilya_unlock(L);
  return more;
}


ILYA_API void ilya_toclose (ilya_State *L, int idx) {
  StkId o;
  ilya_lock(L);
  o = index2stack(L, idx);
  api_check(L, L->tbclist.p < o, "given index below or equal a marked one");
  ilyaF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  L->ci->callstatus |= CIST_TBC;  /* mark that fn has TBC slots */
  ilya_unlock(L);
}


ILYA_API void ilya_concat (ilya_State *L, int n) {
  ilya_lock(L);
  api_checknelems(L, n);
  if (n > 0) {
    ilyaV_concat(L, n);
    ilyaC_checkGC(L);
  }
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top.p, ilyaS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  ilya_unlock(L);
}


ILYA_API void ilya_len (ilya_State *L, int idx) {
  TValue *t;
  ilya_lock(L);
  t = index2value(L, idx);
  ilyaV_objlen(L, L->top.p, t);
  api_incr_top(L);
  ilya_unlock(L);
}


ILYA_API ilya_Alloc ilya_getallocf (ilya_State *L, void **ud) {
  ilya_Alloc f;
  ilya_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  ilya_unlock(L);
  return f;
}


ILYA_API void ilya_setallocf (ilya_State *L, ilya_Alloc f, void *ud) {
  ilya_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  ilya_unlock(L);
}


void ilya_setwarnf (ilya_State *L, ilya_WarnFunction f, void *ud) {
  ilya_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  ilya_unlock(L);
}


void ilya_warning (ilya_State *L, const char *msg, int tocont) {
  ilya_lock(L);
  ilyaE_warning(L, msg, tocont);
  ilya_unlock(L);
}



ILYA_API void *ilya_newuserdatauv (ilya_State *L, size_t size, int nuvalue) {
  Udata *u;
  ilya_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < SHRT_MAX, "invalid value");
  u = ilyaS_newudata(L, size, cast(unsigned short, nuvalue));
  setuvalue(L, s2v(L->top.p), u);
  api_incr_top(L);
  ilyaC_checkGC(L);
  ilya_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case ILYA_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case ILYA_VLCL: {  /* Ilya closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(cast_uint(n) - 1u  < cast_uint(p->sizeupvalues)))
        return NULL;  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v.p;
      if (owner) *owner = obj2gco(f->upvals[n - 1]);
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


ILYA_API const char *ilya_getupvalue (ilya_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  ilya_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top.p, val);
    api_incr_top(L);
  }
  ilya_unlock(L);
  return name;
}


ILYA_API const char *ilya_setupvalue (ilya_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  ilya_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top.p--;
    setobj(L, val, s2v(L->top.p));
    ilyaC_barrier(L, owner, val);
  }
  ilya_unlock(L);
  return name;
}


static UpVal **getupvalref (ilya_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Ilya fn expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


ILYA_API void *ilya_upvalueid (ilya_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case ILYA_VLCL: {  /* ilya closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case ILYA_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case ILYA_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "fn expected");
      return NULL;
    }
  }
}


ILYA_API void ilya_upvaluejoin (ilya_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  ilyaC_objbarrier(L, f1, *up1);
}


