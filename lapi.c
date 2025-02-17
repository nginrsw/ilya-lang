/*
** $Id: lapi.c $
** Irin API
** See Copyright Notice in irin.h
*/

#define lapi_c
#define IRIN_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "irin.h"

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



const char irin_ident[] =
  "$LuaVersion: " IRIN_COPYRIGHT " $"
  "$LuaAuthors: " IRIN_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
*/
#define isvalid(L, o)	((o) != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= IRIN_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < IRIN_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (irin_State *L, int idx) {
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
  else if (idx == IRIN_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = IRIN_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func.p))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func.p));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C fn or Irin fn (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func.p)), "caller not a C fn");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
static StkId index2stack (irin_State *L, int idx) {
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


IRIN_API int irin_checkstack (irin_State *L, int n) {
  int res;
  CallInfo *ci;
  irin_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last.p - L->top.p > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = luaD_growstack(L, n, 0);
  if (res && ci->top.p < L->top.p + n)
    ci->top.p = L->top.p + n;  /* adjust frame top */
  irin_unlock(L);
  return res;
}


IRIN_API void irin_xmove (irin_State *from, irin_State *to, int n) {
  int i;
  if (from == to) return;
  irin_lock(to);
  api_checkpop(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top.p - to->top.p >= n, "stack overflow");
  from->top.p -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top.p, from->top.p + i);
    to->top.p++;  /* stack already checked by previous 'api_check' */
  }
  irin_unlock(to);
}


IRIN_API irin_CFunction irin_atpanic (irin_State *L, irin_CFunction panicf) {
  irin_CFunction old;
  irin_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  irin_unlock(L);
  return old;
}


IRIN_API irin_Number irin_version (irin_State *L) {
  UNUSED(L);
  return IRIN_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
IRIN_API int irin_absindex (irin_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top.p - L->ci->func.p) + idx;
}


IRIN_API int irin_gettop (irin_State *L) {
  return cast_int(L->top.p - (L->ci->func.p + 1));
}


IRIN_API void irin_settop (irin_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  irin_lock(L);
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
    irin_assert(ci->callstatus & CIST_TBC);
    newtop = luaF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top.p = newtop;  /* correct top only after closing any upvalue */
  irin_unlock(L);
}


IRIN_API void irin_closeslot (irin_State *L, int idx) {
  StkId level;
  irin_lock(L);
  level = index2stack(L, idx);
  api_check(L, (L->ci->callstatus & CIST_TBC) && (L->tbclist.p == level),
     "no variable to close at given level");
  level = luaF_close(L, level, CLOSEKTOP, 0);
  setnilvalue(s2v(level));
  irin_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'irin_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
static void reverse (irin_State *L, StkId from, StkId to) {
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
IRIN_API void irin_rotate (irin_State *L, int idx, int n) {
  StkId p, t, m;
  irin_lock(L);
  t = L->top.p - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, L->tbclist.p < p, "moving a to-be-closed slot");
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  irin_unlock(L);
}


IRIN_API void irin_copy (irin_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  irin_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* fn upvalue? */
    luaC_barrier(L, clCvalue(s2v(L->ci->func.p)), fr);
  /* IRIN_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  irin_unlock(L);
}


IRIN_API void irin_pushvalue (irin_State *L, int idx) {
  irin_lock(L);
  setobj2s(L, L->top.p, index2value(L, idx));
  api_incr_top(L);
  irin_unlock(L);
}



/*
** access functions (stack -> C)
*/


IRIN_API int irin_type (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : IRIN_TNONE);
}


IRIN_API const char *irin_typename (irin_State *L, int t) {
  UNUSED(L);
  api_check(L, IRIN_TNONE <= t && t < IRIN_NUMTYPES, "invalid type");
  return ttypename(t);
}


IRIN_API int irin_iscfunction (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


IRIN_API int irin_isinteger (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


IRIN_API int irin_isnumber (irin_State *L, int idx) {
  irin_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


IRIN_API int irin_isstring (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


IRIN_API int irin_isuserdata (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


IRIN_API int irin_rawequal (irin_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? luaV_rawequalobj(o1, o2) : 0;
}


IRIN_API void irin_arith (irin_State *L, int op) {
  irin_lock(L);
  if (op != IRIN_OPUNM && op != IRIN_OPBNOT)
    api_checkpop(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checkpop(L, 1);
    setobjs2s(L, L->top.p, L->top.p - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  luaO_arith(L, op, s2v(L->top.p - 2), s2v(L->top.p - 1), L->top.p - 2);
  L->top.p--;  /* pop second operand */
  irin_unlock(L);
}


IRIN_API int irin_compare (irin_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  irin_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case IRIN_OPEQ: i = luaV_equalobj(L, o1, o2); break;
      case IRIN_OPLT: i = luaV_lessthan(L, o1, o2); break;
      case IRIN_OPLE: i = luaV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  irin_unlock(L);
  return i;
}


IRIN_API unsigned (irin_numbertocstring) (irin_State *L, int idx, char *buff) {
  const TValue *o = index2value(L, idx);
  if (ttisnumber(o)) {
    unsigned len = luaO_tostringbuff(o, buff);
    buff[len++] = '\0';  /* add final zero */
    return len;
  }
  else
    return 0;
}


IRIN_API size_t irin_stringtonumber (irin_State *L, const char *s) {
  size_t sz = luaO_str2num(s, s2v(L->top.p));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


IRIN_API irin_Number irin_tonumberx (irin_State *L, int idx, int *pisnum) {
  irin_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


IRIN_API irin_Integer irin_tointegerx (irin_State *L, int idx, int *pisnum) {
  irin_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


IRIN_API int irin_toboolean (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


IRIN_API const char *irin_tolstring (irin_State *L, int idx, size_t *len) {
  TValue *o;
  irin_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      irin_unlock(L);
      return NULL;
    }
    luaO_tostring(L, o);
    luaC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  irin_unlock(L);
  if (len != NULL)
    return getlstr(tsvalue(o), *len);
  else
    return getstr(tsvalue(o));
}


IRIN_API irin_Unsigned irin_rawlen (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case IRIN_VSHRSTR: return cast(irin_Unsigned, tsvalue(o)->shrlen);
    case IRIN_VLNGSTR: return cast(irin_Unsigned, tsvalue(o)->u.lnglen);
    case IRIN_VUSERDATA: return cast(irin_Unsigned, uvalue(o)->len);
    case IRIN_VTABLE: return luaH_getn(hvalue(o));
    default: return 0;
  }
}


IRIN_API irin_CFunction irin_tocfunction (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C fn */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case IRIN_TUSERDATA: return getudatamem(uvalue(o));
    case IRIN_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


IRIN_API void *irin_touserdata (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


IRIN_API irin_State *irin_tothread (irin_State *L, int idx) {
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
IRIN_API const void *irin_topointer (irin_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case IRIN_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case IRIN_VUSERDATA: case IRIN_VLIGHTUSERDATA:
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


IRIN_API void irin_pushnil (irin_State *L) {
  irin_lock(L);
  setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  irin_unlock(L);
}


IRIN_API void irin_pushnumber (irin_State *L, irin_Number n) {
  irin_lock(L);
  setfltvalue(s2v(L->top.p), n);
  api_incr_top(L);
  irin_unlock(L);
}


IRIN_API void irin_pushinteger (irin_State *L, irin_Integer n) {
  irin_lock(L);
  setivalue(s2v(L->top.p), n);
  api_incr_top(L);
  irin_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
IRIN_API const char *irin_pushlstring (irin_State *L, const char *s, size_t len) {
  TString *ts;
  irin_lock(L);
  ts = (len == 0) ? luaS_new(L, "") : luaS_newlstr(L, s, len);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  luaC_checkGC(L);
  irin_unlock(L);
  return getstr(ts);
}


IRIN_API const char *irin_pushexternalstring (irin_State *L,
	        const char *s, size_t len, irin_Alloc falloc, void *ud) {
  TString *ts;
  irin_lock(L);
  api_check(L, len <= MAX_SIZE, "string too large");
  api_check(L, s[len] == '\0', "string not ending with zero");
  ts = luaS_newextlstr (L, s, len, falloc, ud);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  luaC_checkGC(L);
  irin_unlock(L);
  return getstr(ts);
}


IRIN_API const char *irin_pushstring (irin_State *L, const char *s) {
  irin_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top.p));
  else {
    TString *ts;
    ts = luaS_new(L, s);
    setsvalue2s(L, L->top.p, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  luaC_checkGC(L);
  irin_unlock(L);
  return s;
}


IRIN_API const char *irin_pushvfstring (irin_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  irin_lock(L);
  ret = luaO_pushvfstring(L, fmt, argp);
  luaC_checkGC(L);
  irin_unlock(L);
  return ret;
}


IRIN_API const char *irin_pushfstring (irin_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  irin_lock(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  luaC_checkGC(L);
  if (ret == NULL)  /* error? */
    luaD_throw(L, IRIN_ERRMEM);
  irin_unlock(L);
  return ret;
}


IRIN_API void irin_pushcclosure (irin_State *L, irin_CFunction fn, int n) {
  irin_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top.p), fn);
    api_incr_top(L);
  }
  else {
    int i;
    CClosure *cl;
    api_checkpop(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = luaF_newCclosure(L, n);
    cl->f = fn;
    for (i = 0; i < n; i++) {
      setobj2n(L, &cl->upvalue[i], s2v(L->top.p - n + i));
      /* does not need barrier because closure is white */
      irin_assert(iswhite(cl));
    }
    L->top.p -= n;
    setclCvalue(L, s2v(L->top.p), cl);
    api_incr_top(L);
    luaC_checkGC(L);
  }
  irin_unlock(L);
}


IRIN_API void irin_pushboolean (irin_State *L, int b) {
  irin_lock(L);
  if (b)
    setbtvalue(s2v(L->top.p));
  else
    setbfvalue(s2v(L->top.p));
  api_incr_top(L);
  irin_unlock(L);
}


IRIN_API void irin_pushlightuserdata (irin_State *L, void *p) {
  irin_lock(L);
  setpvalue(s2v(L->top.p), p);
  api_incr_top(L);
  irin_unlock(L);
}


IRIN_API int irin_pushthread (irin_State *L) {
  irin_lock(L);
  setthvalue(L, s2v(L->top.p), L);
  api_incr_top(L);
  irin_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Irin -> stack)
*/


static int auxgetstr (irin_State *L, const TValue *t, const char *k) {
  lu_byte tag;
  TString *str = luaS_new(L, k);
  luaV_fastget(t, str, s2v(L->top.p), luaH_getstr, tag);
  if (!tagisempty(tag))
    api_incr_top(L);
  else {
    setsvalue2s(L, L->top.p, str);
    api_incr_top(L);
    tag = luaV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  }
  irin_unlock(L);
  return novariant(tag);
}


static void getGlobalTable (irin_State *L, TValue *gt) {
  Table *registry = hvalue(&G(L)->l_registry);
  lu_byte tag = luaH_getint(registry, IRIN_RIDX_GLOBALS, gt);
  (void)tag;  /* avoid not-used warnings when checks are off */
  api_check(L, novariant(tag) == IRIN_TTABLE, "global table must exist");
}


IRIN_API int irin_getglobal (irin_State *L, const char *name) {
  TValue gt;
  irin_lock(L);
  getGlobalTable(L, &gt);
  return auxgetstr(L, &gt, name);
}


IRIN_API int irin_gettable (irin_State *L, int idx) {
  lu_byte tag;
  TValue *t;
  irin_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  luaV_fastget(t, s2v(L->top.p - 1), s2v(L->top.p - 1), luaH_get, tag);
  if (tagisempty(tag))
    tag = luaV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  irin_unlock(L);
  return novariant(tag);
}


IRIN_API int irin_getfield (irin_State *L, int idx, const char *k) {
  irin_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


IRIN_API int irin_geti (irin_State *L, int idx, irin_Integer n) {
  TValue *t;
  lu_byte tag;
  irin_lock(L);
  t = index2value(L, idx);
  luaV_fastgeti(t, n, s2v(L->top.p), tag);
  if (tagisempty(tag)) {
    TValue key;
    setivalue(&key, n);
    tag = luaV_finishget(L, t, &key, L->top.p, tag);
  }
  api_incr_top(L);
  irin_unlock(L);
  return novariant(tag);
}


static int finishrawget (irin_State *L, lu_byte tag) {
  if (tagisempty(tag))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  irin_unlock(L);
  return novariant(tag);
}


l_sinline Table *gettable (irin_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


IRIN_API int irin_rawget (irin_State *L, int idx) {
  Table *t;
  lu_byte tag;
  irin_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  tag = luaH_get(t, s2v(L->top.p - 1), s2v(L->top.p - 1));
  L->top.p--;  /* pop key */
  return finishrawget(L, tag);
}


IRIN_API int irin_rawgeti (irin_State *L, int idx, irin_Integer n) {
  Table *t;
  lu_byte tag;
  irin_lock(L);
  t = gettable(L, idx);
  luaH_fastgeti(t, n, s2v(L->top.p), tag);
  return finishrawget(L, tag);
}


IRIN_API int irin_rawgetp (irin_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  irin_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, luaH_get(t, &k, s2v(L->top.p)));
}


IRIN_API void irin_createtable (irin_State *L, int narray, int nrec) {
  Table *t;
  irin_lock(L);
  t = luaH_new(L);
  sethvalue2s(L, L->top.p, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    luaH_resize(L, t, cast_uint(narray), cast_uint(nrec));
  luaC_checkGC(L);
  irin_unlock(L);
}


IRIN_API int irin_getmetatable (irin_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  irin_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case IRIN_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case IRIN_TUSERDATA:
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
  irin_unlock(L);
  return res;
}


IRIN_API int irin_getiuservalue (irin_State *L, int idx, int n) {
  TValue *o;
  int t;
  irin_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top.p));
    t = IRIN_TNONE;
  }
  else {
    setobj2s(L, L->top.p, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top.p));
  }
  api_incr_top(L);
  irin_unlock(L);
  return t;
}


/*
** set functions (stack -> Irin)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (irin_State *L, const TValue *t, const char *k) {
  int hres;
  TString *str = luaS_new(L, k);
  api_checkpop(L, 1);
  luaV_fastset(t, str, s2v(L->top.p - 1), hres, luaH_psetstr);
  if (hres == HOK) {
    luaV_finishfastset(L, t, s2v(L->top.p - 1));
    L->top.p--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top.p, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    luaV_finishset(L, t, s2v(L->top.p - 1), s2v(L->top.p - 2), hres);
    L->top.p -= 2;  /* pop value and key */
  }
  irin_unlock(L);  /* lock done by caller */
}


IRIN_API void irin_setglobal (irin_State *L, const char *name) {
  TValue gt;
  irin_lock(L);  /* unlock done in 'auxsetstr' */
  getGlobalTable(L, &gt);
  auxsetstr(L, &gt, name);
}


IRIN_API void irin_settable (irin_State *L, int idx) {
  TValue *t;
  int hres;
  irin_lock(L);
  api_checkpop(L, 2);
  t = index2value(L, idx);
  luaV_fastset(t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres, luaH_pset);
  if (hres == HOK) {
    luaV_finishfastset(L, t, s2v(L->top.p - 1));
  }
  else
    luaV_finishset(L, t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres);
  L->top.p -= 2;  /* pop index and value */
  irin_unlock(L);
}


IRIN_API void irin_setfield (irin_State *L, int idx, const char *k) {
  irin_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


IRIN_API void irin_seti (irin_State *L, int idx, irin_Integer n) {
  TValue *t;
  int hres;
  irin_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  luaV_fastseti(t, n, s2v(L->top.p - 1), hres);
  if (hres == HOK)
    luaV_finishfastset(L, t, s2v(L->top.p - 1));
  else {
    TValue temp;
    setivalue(&temp, n);
    luaV_finishset(L, t, &temp, s2v(L->top.p - 1), hres);
  }
  L->top.p--;  /* pop value */
  irin_unlock(L);
}


static void aux_rawset (irin_State *L, int idx, TValue *key, int n) {
  Table *t;
  irin_lock(L);
  api_checkpop(L, n);
  t = gettable(L, idx);
  luaH_set(L, t, key, s2v(L->top.p - 1));
  invalidateTMcache(t);
  luaC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p -= n;
  irin_unlock(L);
}


IRIN_API void irin_rawset (irin_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top.p - 2), 2);
}


IRIN_API void irin_rawsetp (irin_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


IRIN_API void irin_rawseti (irin_State *L, int idx, irin_Integer n) {
  Table *t;
  irin_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  luaH_setint(L, t, n, s2v(L->top.p - 1));
  luaC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p--;
  irin_unlock(L);
}


IRIN_API int irin_setmetatable (irin_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  irin_lock(L);
  api_checkpop(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top.p - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top.p - 1)), "table expected");
    mt = hvalue(s2v(L->top.p - 1));
  }
  switch (ttype(obj)) {
    case IRIN_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        luaC_objbarrier(L, gcvalue(obj), mt);
        luaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case IRIN_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        luaC_objbarrier(L, uvalue(obj), mt);
        luaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top.p--;
  irin_unlock(L);
  return 1;
}


IRIN_API int irin_setiuservalue (irin_State *L, int idx, int n) {
  TValue *o;
  int res;
  irin_lock(L);
  api_checkpop(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top.p - 1));
    luaC_barrierback(L, gcvalue(o), s2v(L->top.p - 1));
    res = 1;
  }
  L->top.p--;
  irin_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Irin code)
*/


#define checkresults(L,na,nr) \
     (api_check(L, (nr) == IRIN_MULTRET \
               || (L->ci->top.p - L->top.p >= (nr) - (na)), \
	"results from fn overflow current stack size"), \
      api_check(L, IRIN_MULTRET <= (nr) && (nr) <= MAXRESULTS,  \
                   "invalid number of results"))


IRIN_API void irin_callk (irin_State *L, int nargs, int nresults,
                        irin_KContext ctx, irin_KFunction k) {
  StkId func;
  irin_lock(L);
  api_check(L, k == NULL || !isLua(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == IRIN_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top.p - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    luaD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    luaD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  irin_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (irin_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_callnoyield(L, c->func, c->nresults);
}



IRIN_API int irin_pcallk (irin_State *L, int nargs, int nresults, int errfunc,
                        irin_KContext ctx, irin_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  irin_lock(L);
  api_check(L, k == NULL || !isLua(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == IRIN_OK, "cannot do calls on non-normal thread");
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
    status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
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
    luaD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = IRIN_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  irin_unlock(L);
  return status;
}


IRIN_API int irin_load (irin_State *L, irin_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  irin_lock(L);
  if (!chunkname) chunkname = "?";
  luaZ_init(L, &z, reader, data);
  status = luaD_protectedparser(L, &z, chunkname, mode);
  if (status == IRIN_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top.p - 1));  /* get new fn */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      TValue gt;
      getGlobalTable(L, &gt);
      /* set global table as 1st upvalue of 'f' (may be IRIN_ENV) */
      setobj(L, f->upvals[0]->v.p, &gt);
      luaC_barrier(L, f->upvals[0], &gt);
    }
  }
  irin_unlock(L);
  return status;
}


/*
** Dump a Irin fn, calling 'writer' to write its parts. Ensure
** the stack returns with its original size.
*/
IRIN_API int irin_dump (irin_State *L, irin_Writer writer, void *data, int strip) {
  int status;
  ptrdiff_t otop = savestack(L, L->top.p);  /* original top */
  TValue *f = s2v(L->top.p - 1);  /* fn to be dumped */
  irin_lock(L);
  api_checkpop(L, 1);
  api_check(L, isLfunction(f), "Irin fn expected");
  status = luaU_dump(L, clLvalue(f)->p, writer, data, strip);
  L->top.p = restorestack(L, otop);  /* restore top */
  irin_unlock(L);
  return status;
}


IRIN_API int irin_status (irin_State *L) {
  return L->status;
}


/*
** Garbage-collection fn
*/
IRIN_API int irin_gc (irin_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & (GCSTPGC | GCSTPCLS))  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  irin_lock(L);
  va_start(argp, what);
  switch (what) {
    case IRIN_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case IRIN_GCRESTART: {
      luaE_setdebt(g, 0);
      g->gcstp = 0;  /* (other bits must be zero here) */
      break;
    }
    case IRIN_GCCOLLECT: {
      luaC_fullgc(L, 0);
      break;
    }
    case IRIN_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case IRIN_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case IRIN_GCSTEP: {
      lu_byte oldstp = g->gcstp;
      l_mem n = cast(l_mem, va_arg(argp, size_t));
      int work = 0;  /* true if GC did some work */
      g->gcstp = 0;  /* allow GC to run (other bits must be zero here) */
      if (n <= 0)
        n = g->GCdebt;  /* force to run one basic step */
      luaE_setdebt(g, g->GCdebt - n);
      luaC_condGC(L, (void)0, work = 1);
      if (work && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      g->gcstp = oldstp;  /* restore previous state */
      break;
    }
    case IRIN_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case IRIN_GCGEN: {
      res = (g->gckind == KGC_INC) ? IRIN_GCINC : IRIN_GCGEN;
      luaC_changemode(L, KGC_GENMINOR);
      break;
    }
    case IRIN_GCINC: {
      res = (g->gckind == KGC_INC) ? IRIN_GCINC : IRIN_GCGEN;
      luaC_changemode(L, KGC_INC);
      break;
    }
    case IRIN_GCPARAM: {
      int param = va_arg(argp, int);
      int value = va_arg(argp, int);
      api_check(L, 0 <= param && param < IRIN_GCPN, "invalid parameter");
      res = cast_int(luaO_applyparam(g->gcparams[param], 100));
      if (value >= 0)
        g->gcparams[param] = luaO_codeparam(cast_uint(value));
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  irin_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


IRIN_API int irin_error (irin_State *L) {
  TValue *errobj;
  irin_lock(L);
  errobj = s2v(L->top.p - 1);
  api_checkpop(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    luaM_error(L);  /* raise a memory error */
  else
    luaG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


IRIN_API int irin_next (irin_State *L, int idx) {
  Table *t;
  int more;
  irin_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  more = luaH_next(L, t, L->top.p - 1);
  if (more)
    api_incr_top(L);
  else  /* no more elements */
    L->top.p--;  /* pop key */
  irin_unlock(L);
  return more;
}


IRIN_API void irin_toclose (irin_State *L, int idx) {
  StkId o;
  irin_lock(L);
  o = index2stack(L, idx);
  api_check(L, L->tbclist.p < o, "given index below or equal a marked one");
  luaF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  L->ci->callstatus |= CIST_TBC;  /* mark that fn has TBC slots */
  irin_unlock(L);
}


IRIN_API void irin_concat (irin_State *L, int n) {
  irin_lock(L);
  api_checknelems(L, n);
  if (n > 0) {
    luaV_concat(L, n);
    luaC_checkGC(L);
  }
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top.p, luaS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  irin_unlock(L);
}


IRIN_API void irin_len (irin_State *L, int idx) {
  TValue *t;
  irin_lock(L);
  t = index2value(L, idx);
  luaV_objlen(L, L->top.p, t);
  api_incr_top(L);
  irin_unlock(L);
}


IRIN_API irin_Alloc irin_getallocf (irin_State *L, void **ud) {
  irin_Alloc f;
  irin_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  irin_unlock(L);
  return f;
}


IRIN_API void irin_setallocf (irin_State *L, irin_Alloc f, void *ud) {
  irin_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  irin_unlock(L);
}


void irin_setwarnf (irin_State *L, irin_WarnFunction f, void *ud) {
  irin_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  irin_unlock(L);
}


void irin_warning (irin_State *L, const char *msg, int tocont) {
  irin_lock(L);
  luaE_warning(L, msg, tocont);
  irin_unlock(L);
}



IRIN_API void *irin_newuserdatauv (irin_State *L, size_t size, int nuvalue) {
  Udata *u;
  irin_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < SHRT_MAX, "invalid value");
  u = luaS_newudata(L, size, cast(unsigned short, nuvalue));
  setuvalue(L, s2v(L->top.p), u);
  api_incr_top(L);
  luaC_checkGC(L);
  irin_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case IRIN_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case IRIN_VLCL: {  /* Irin closure */
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


IRIN_API const char *irin_getupvalue (irin_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  irin_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top.p, val);
    api_incr_top(L);
  }
  irin_unlock(L);
  return name;
}


IRIN_API const char *irin_setupvalue (irin_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  irin_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top.p--;
    setobj(L, val, s2v(L->top.p));
    luaC_barrier(L, owner, val);
  }
  irin_unlock(L);
  return name;
}


static UpVal **getupvalref (irin_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Irin fn expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


IRIN_API void *irin_upvalueid (irin_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case IRIN_VLCL: {  /* irin closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case IRIN_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case IRIN_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "fn expected");
      return NULL;
    }
  }
}


IRIN_API void irin_upvaluejoin (irin_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  luaC_objbarrier(L, f1, *up1);
}


