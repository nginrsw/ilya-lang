/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in ilya.h
*/

#define lbaselib_c
#define ILYA_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ilya.h"

#include "lauxlib.h"
#include "ilyalib.h"
#include "llimits.h"


static int ilyaB_print (ilya_State *L) {
  int n = ilya_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = ilyaL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      ilya_writestring("\t", 1);  /* add a tab before it */
    ilya_writestring(s, l);  /* print it */
    ilya_pop(L, 1);  /* pop result */
  }
  ilya_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int ilyaB_warn (ilya_State *L) {
  int n = ilya_gettop(L);  /* number of arguments */
  int i;
  ilyaL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    ilyaL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    ilya_warning(L, ilya_tostring(L, i), 1);
  ilya_warning(L, ilya_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, unsigned base, ilya_Integer *pn) {
  ilya_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum(cast_uchar(*s)))  /* no digit? */
    return NULL;
  do {
    unsigned digit = cast_uint(isdigit(cast_uchar(*s))
                               ? *s - '0'
                               : (toupper(cast_uchar(*s)) - 'A') + 10);
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum(cast_uchar(*s)));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (ilya_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int ilyaB_tonumber (ilya_State *L) {
  if (ilya_isnoneornil(L, 2)) {  /* standard conversion? */
    if (ilya_type(L, 1) == ILYA_TNUMBER) {  /* already a number? */
      ilya_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = ilya_tolstring(L, 1, &l);
      if (s != NULL && ilya_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      ilyaL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    ilya_Integer n = 0;  /* to avoid warnings */
    ilya_Integer base = ilyaL_checkinteger(L, 2);
    ilyaL_checktype(L, 1, ILYA_TSTRING);  /* no numbers as strings */
    s = ilya_tolstring(L, 1, &l);
    ilyaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, cast_uint(base), &n) == s + l) {
      ilya_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  ilyaL_pushfail(L);  /* not a number */
  return 1;
}


static int ilyaB_error (ilya_State *L) {
  int level = (int)ilyaL_optinteger(L, 2, 1);
  ilya_settop(L, 1);
  if (ilya_type(L, 1) == ILYA_TSTRING && level > 0) {
    ilyaL_where(L, level);   /* add extra information */
    ilya_pushvalue(L, 1);
    ilya_concat(L, 2);
  }
  return ilya_error(L);
}


static int ilyaB_getmetatable (ilya_State *L) {
  ilyaL_checkany(L, 1);
  if (!ilya_getmetatable(L, 1)) {
    ilya_pushnil(L);
    return 1;  /* no metatable */
  }
  ilyaL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int ilyaB_setmetatable (ilya_State *L) {
  int t = ilya_type(L, 2);
  ilyaL_checktype(L, 1, ILYA_TTABLE);
  ilyaL_argexpected(L, t == ILYA_TNIL || t == ILYA_TTABLE, 2, "nil or table");
  if (l_unlikely(ilyaL_getmetafield(L, 1, "__metatable") != ILYA_TNIL))
    return ilyaL_error(L, "cannot change a protected metatable");
  ilya_settop(L, 2);
  ilya_setmetatable(L, 1);
  return 1;
}


static int ilyaB_rawequal (ilya_State *L) {
  ilyaL_checkany(L, 1);
  ilyaL_checkany(L, 2);
  ilya_pushboolean(L, ilya_rawequal(L, 1, 2));
  return 1;
}


static int ilyaB_rawlen (ilya_State *L) {
  int t = ilya_type(L, 1);
  ilyaL_argexpected(L, t == ILYA_TTABLE || t == ILYA_TSTRING, 1,
                      "table or string");
  ilya_pushinteger(L, l_castU2S(ilya_rawlen(L, 1)));
  return 1;
}


static int ilyaB_rawget (ilya_State *L) {
  ilyaL_checktype(L, 1, ILYA_TTABLE);
  ilyaL_checkany(L, 2);
  ilya_settop(L, 2);
  ilya_rawget(L, 1);
  return 1;
}

static int ilyaB_rawset (ilya_State *L) {
  ilyaL_checktype(L, 1, ILYA_TTABLE);
  ilyaL_checkany(L, 2);
  ilyaL_checkany(L, 3);
  ilya_settop(L, 3);
  ilya_rawset(L, 1);
  return 1;
}


static int pushmode (ilya_State *L, int oldmode) {
  if (oldmode == -1)
    ilyaL_pushfail(L);  /* invalid call to 'ilya_gc' */
  else
    ilya_pushstring(L, (oldmode == ILYA_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'ilya_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int ilyaB_collectgarbage (ilya_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "isrunning", "generational", "incremental",
    "param", NULL};
  static const char optsnum[] = {ILYA_GCSTOP, ILYA_GCRESTART, ILYA_GCCOLLECT,
    ILYA_GCCOUNT, ILYA_GCSTEP, ILYA_GCISRUNNING, ILYA_GCGEN, ILYA_GCINC,
    ILYA_GCPARAM};
  int o = optsnum[ilyaL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case ILYA_GCCOUNT: {
      int k = ilya_gc(L, o);
      int b = ilya_gc(L, ILYA_GCCOUNTB);
      checkvalres(k);
      ilya_pushnumber(L, (ilya_Number)k + ((ilya_Number)b/1024));
      return 1;
    }
    case ILYA_GCSTEP: {
      ilya_Integer n = ilyaL_optinteger(L, 2, 0);
      int res = ilya_gc(L, o, cast_sizet(n));
      checkvalres(res);
      ilya_pushboolean(L, res);
      return 1;
    }
    case ILYA_GCISRUNNING: {
      int res = ilya_gc(L, o);
      checkvalres(res);
      ilya_pushboolean(L, res);
      return 1;
    }
    case ILYA_GCGEN: {
      return pushmode(L, ilya_gc(L, o));
    }
    case ILYA_GCINC: {
      return pushmode(L, ilya_gc(L, o));
    }
    case ILYA_GCPARAM: {
      static const char *const params[] = {
        "minormul", "majorminor", "minormajor",
        "pause", "stepmul", "stepsize", NULL};
      static const char pnum[] = {
        ILYA_GCPMINORMUL, ILYA_GCPMAJORMINOR, ILYA_GCPMINORMAJOR,
        ILYA_GCPPAUSE, ILYA_GCPSTEPMUL, ILYA_GCPSTEPSIZE};
      int p = pnum[ilyaL_checkoption(L, 2, NULL, params)];
      ilya_Integer value = ilyaL_optinteger(L, 3, -1);
      ilya_pushinteger(L, ilya_gc(L, o, p, (int)value));
      return 1;
    }
    default: {
      int res = ilya_gc(L, o);
      checkvalres(res);
      ilya_pushinteger(L, res);
      return 1;
    }
  }
  ilyaL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int ilyaB_type (ilya_State *L) {
  int t = ilya_type(L, 1);
  ilyaL_argcheck(L, t != ILYA_TNONE, 1, "value expected");
  ilya_pushstring(L, ilya_typename(L, t));
  return 1;
}


static int ilyaB_next (ilya_State *L) {
  ilyaL_checktype(L, 1, ILYA_TTABLE);
  ilya_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (ilya_next(L, 1))
    return 2;
  else {
    ilya_pushnil(L);
    return 1;
  }
}


static int pairscont (ilya_State *L, int status, ilya_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int ilyaB_pairs (ilya_State *L) {
  ilyaL_checkany(L, 1);
  if (ilyaL_getmetafield(L, 1, "__pairs") == ILYA_TNIL) {  /* no metamethod? */
    ilya_pushcfunction(L, ilyaB_next);  /* will return generator, */
    ilya_pushvalue(L, 1);  /* state, */
    ilya_pushnil(L);  /* and initial value */
  }
  else {
    ilya_pushvalue(L, 1);  /* argument 'self' to metamethod */
    ilya_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal fn for 'ipairs'
*/
static int ipairsaux (ilya_State *L) {
  ilya_Integer i = ilyaL_checkinteger(L, 2);
  i = ilyaL_intop(+, i, 1);
  ilya_pushinteger(L, i);
  return (ilya_geti(L, 1, i) == ILYA_TNIL) ? 1 : 2;
}


/*
** 'ipairs' fn. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int ilyaB_ipairs (ilya_State *L) {
  ilyaL_checkany(L, 1);
  ilya_pushcfunction(L, ipairsaux);  /* iteration fn */
  ilya_pushvalue(L, 1);  /* state */
  ilya_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (ilya_State *L, int status, int envidx) {
  if (l_likely(status == ILYA_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      ilya_pushvalue(L, envidx);  /* environment for loaded fn */
      if (!ilya_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        ilya_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    ilyaL_pushfail(L);
    ilya_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static const char *getMode (ilya_State *L, int idx) {
  const char *mode = ilyaL_optstring(L, idx, "bt");
  if (strchr(mode, 'B') != NULL)  /* Ilya code cannot use fixed buffers */
    ilyaL_argerror(L, idx, "invalid mode");
  return mode;
}


static int ilyaB_loadfile (ilya_State *L) {
  const char *fname = ilyaL_optstring(L, 1, NULL);
  const char *mode = getMode(L, 2);
  int env = (!ilya_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = ilyaL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read fn
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' fn: 'ilya_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (ilya_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  ilyaL_checkstack(L, 2, "too many nested functions");
  ilya_pushvalue(L, 1);  /* get fn */
  ilya_call(L, 0, 1);  /* call it */
  if (ilya_isnil(L, -1)) {
    ilya_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!ilya_isstring(L, -1)))
    ilyaL_error(L, "reader fn must return a string");
  ilya_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return ilya_tolstring(L, RESERVEDSLOT, size);
}


static int ilyaB_load (ilya_State *L) {
  int status;
  size_t l;
  const char *s = ilya_tolstring(L, 1, &l);
  const char *mode = getMode(L, 3);
  int env = (!ilya_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = ilyaL_optstring(L, 2, s);
    status = ilyaL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader fn */
    const char *chunkname = ilyaL_optstring(L, 2, "=(load)");
    ilyaL_checktype(L, 1, ILYA_TFUNCTION);
    ilya_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = ilya_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (ilya_State *L, int d1, ilya_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'ilya_Kfunction' prototype */
  return ilya_gettop(L) - 1;
}


static int ilyaB_dofile (ilya_State *L) {
  const char *fname = ilyaL_optstring(L, 1, NULL);
  ilya_settop(L, 1);
  if (l_unlikely(ilyaL_loadfile(L, fname) != ILYA_OK))
    return ilya_error(L);
  ilya_callk(L, 0, ILYA_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int ilyaB_assert (ilya_State *L) {
  if (l_likely(ilya_toboolean(L, 1)))  /* condition is true? */
    return ilya_gettop(L);  /* return all arguments */
  else {  /* error */
    ilyaL_checkany(L, 1);  /* there must be a condition */
    ilya_remove(L, 1);  /* remove it */
    ilya_pushliteral(L, "assertion failed!");  /* default message */
    ilya_settop(L, 1);  /* leave only message (default if no other one) */
    return ilyaB_error(L);  /* call 'error' */
  }
}


static int ilyaB_select (ilya_State *L) {
  int n = ilya_gettop(L);
  if (ilya_type(L, 1) == ILYA_TSTRING && *ilya_tostring(L, 1) == '#') {
    ilya_pushinteger(L, n-1);
    return 1;
  }
  else {
    ilya_Integer i = ilyaL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    ilyaL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation fn for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (ilya_State *L, int status, ilya_KContext extra) {
  if (l_unlikely(status != ILYA_OK && status != ILYA_YIELD)) {  /* error? */
    ilya_pushboolean(L, 0);  /* first result (false) */
    ilya_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return ilya_gettop(L) - (int)extra;  /* return all results */
}


static int ilyaB_pcall (ilya_State *L) {
  int status;
  ilyaL_checkany(L, 1);
  ilya_pushboolean(L, 1);  /* first result if no errors */
  ilya_insert(L, 1);  /* put it in place */
  status = ilya_pcallk(L, ilya_gettop(L) - 2, ILYA_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'ilya_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the fn passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int ilyaB_xpcall (ilya_State *L) {
  int status;
  int n = ilya_gettop(L);
  ilyaL_checktype(L, 2, ILYA_TFUNCTION);  /* check error fn */
  ilya_pushboolean(L, 1);  /* first result */
  ilya_pushvalue(L, 1);  /* fn */
  ilya_rotate(L, 3, 2);  /* move them below fn's arguments */
  status = ilya_pcallk(L, n - 2, ILYA_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int ilyaB_tostring (ilya_State *L) {
  ilyaL_checkany(L, 1);
  ilyaL_tolstring(L, 1, NULL);
  return 1;
}


static const ilyaL_Reg base_funcs[] = {
  {"assert", ilyaB_assert},
  {"collectgarbage", ilyaB_collectgarbage},
  {"dofile", ilyaB_dofile},
  {"error", ilyaB_error},
  {"getmetatable", ilyaB_getmetatable},
  {"ipairs", ilyaB_ipairs},
  {"loadfile", ilyaB_loadfile},
  {"load", ilyaB_load},
  {"next", ilyaB_next},
  {"pairs", ilyaB_pairs},
  {"pcall", ilyaB_pcall},
  {"print", ilyaB_print},
  {"warn", ilyaB_warn},
  {"rawequal", ilyaB_rawequal},
  {"rawlen", ilyaB_rawlen},
  {"rawget", ilyaB_rawget},
  {"rawset", ilyaB_rawset},
  {"select", ilyaB_select},
  {"setmetatable", ilyaB_setmetatable},
  {"tonumber", ilyaB_tonumber},
  {"tostring", ilyaB_tostring},
  {"type", ilyaB_type},
  {"xpcall", ilyaB_xpcall},
  /* placeholders */
  {ILYA_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


ILYAMOD_API int ilyaopen_base (ilya_State *L) {
  /* open lib into global table */
  ilya_pushglobaltable(L);
  ilyaL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  ilya_pushvalue(L, -1);
  ilya_setfield(L, -2, ILYA_GNAME);
  /* set global _VERSION */
  ilya_pushliteral(L, ILYA_VERSION);
  ilya_setfield(L, -2, "_VERSION");
  return 1;
}

