/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in irin.h
*/

#define lbaselib_c
#define IRIN_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irin.h"

#include "lauxlib.h"
#include "irinlib.h"
#include "llimits.h"


static int luaB_print (irin_State *L) {
  int n = irin_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = luaL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      irin_writestring("\t", 1);  /* add a tab before it */
    irin_writestring(s, l);  /* print it */
    irin_pop(L, 1);  /* pop result */
  }
  irin_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int luaB_warn (irin_State *L) {
  int n = irin_gettop(L);  /* number of arguments */
  int i;
  luaL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    luaL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    irin_warning(L, irin_tostring(L, i), 1);
  irin_warning(L, irin_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, unsigned base, irin_Integer *pn) {
  irin_Unsigned n = 0;
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
  *pn = (irin_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int luaB_tonumber (irin_State *L) {
  if (irin_isnoneornil(L, 2)) {  /* standard conversion? */
    if (irin_type(L, 1) == IRIN_TNUMBER) {  /* already a number? */
      irin_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = irin_tolstring(L, 1, &l);
      if (s != NULL && irin_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      luaL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    irin_Integer n = 0;  /* to avoid warnings */
    irin_Integer base = luaL_checkinteger(L, 2);
    luaL_checktype(L, 1, IRIN_TSTRING);  /* no numbers as strings */
    s = irin_tolstring(L, 1, &l);
    luaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, cast_uint(base), &n) == s + l) {
      irin_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  luaL_pushfail(L);  /* not a number */
  return 1;
}


static int luaB_error (irin_State *L) {
  int level = (int)luaL_optinteger(L, 2, 1);
  irin_settop(L, 1);
  if (irin_type(L, 1) == IRIN_TSTRING && level > 0) {
    luaL_where(L, level);   /* add extra information */
    irin_pushvalue(L, 1);
    irin_concat(L, 2);
  }
  return irin_error(L);
}


static int luaB_getmetatable (irin_State *L) {
  luaL_checkany(L, 1);
  if (!irin_getmetatable(L, 1)) {
    irin_pushnil(L);
    return 1;  /* no metatable */
  }
  luaL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int luaB_setmetatable (irin_State *L) {
  int t = irin_type(L, 2);
  luaL_checktype(L, 1, IRIN_TTABLE);
  luaL_argexpected(L, t == IRIN_TNIL || t == IRIN_TTABLE, 2, "nil or table");
  if (l_unlikely(luaL_getmetafield(L, 1, "__metatable") != IRIN_TNIL))
    return luaL_error(L, "cannot change a protected metatable");
  irin_settop(L, 2);
  irin_setmetatable(L, 1);
  return 1;
}


static int luaB_rawequal (irin_State *L) {
  luaL_checkany(L, 1);
  luaL_checkany(L, 2);
  irin_pushboolean(L, irin_rawequal(L, 1, 2));
  return 1;
}


static int luaB_rawlen (irin_State *L) {
  int t = irin_type(L, 1);
  luaL_argexpected(L, t == IRIN_TTABLE || t == IRIN_TSTRING, 1,
                      "table or string");
  irin_pushinteger(L, l_castU2S(irin_rawlen(L, 1)));
  return 1;
}


static int luaB_rawget (irin_State *L) {
  luaL_checktype(L, 1, IRIN_TTABLE);
  luaL_checkany(L, 2);
  irin_settop(L, 2);
  irin_rawget(L, 1);
  return 1;
}

static int luaB_rawset (irin_State *L) {
  luaL_checktype(L, 1, IRIN_TTABLE);
  luaL_checkany(L, 2);
  luaL_checkany(L, 3);
  irin_settop(L, 3);
  irin_rawset(L, 1);
  return 1;
}


static int pushmode (irin_State *L, int oldmode) {
  if (oldmode == -1)
    luaL_pushfail(L);  /* invalid call to 'irin_gc' */
  else
    irin_pushstring(L, (oldmode == IRIN_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'irin_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int luaB_collectgarbage (irin_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "isrunning", "generational", "incremental",
    "param", NULL};
  static const char optsnum[] = {IRIN_GCSTOP, IRIN_GCRESTART, IRIN_GCCOLLECT,
    IRIN_GCCOUNT, IRIN_GCSTEP, IRIN_GCISRUNNING, IRIN_GCGEN, IRIN_GCINC,
    IRIN_GCPARAM};
  int o = optsnum[luaL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case IRIN_GCCOUNT: {
      int k = irin_gc(L, o);
      int b = irin_gc(L, IRIN_GCCOUNTB);
      checkvalres(k);
      irin_pushnumber(L, (irin_Number)k + ((irin_Number)b/1024));
      return 1;
    }
    case IRIN_GCSTEP: {
      irin_Integer n = luaL_optinteger(L, 2, 0);
      int res = irin_gc(L, o, cast_sizet(n));
      checkvalres(res);
      irin_pushboolean(L, res);
      return 1;
    }
    case IRIN_GCISRUNNING: {
      int res = irin_gc(L, o);
      checkvalres(res);
      irin_pushboolean(L, res);
      return 1;
    }
    case IRIN_GCGEN: {
      return pushmode(L, irin_gc(L, o));
    }
    case IRIN_GCINC: {
      return pushmode(L, irin_gc(L, o));
    }
    case IRIN_GCPARAM: {
      static const char *const params[] = {
        "minormul", "majorminor", "minormajor",
        "pause", "stepmul", "stepsize", NULL};
      static const char pnum[] = {
        IRIN_GCPMINORMUL, IRIN_GCPMAJORMINOR, IRIN_GCPMINORMAJOR,
        IRIN_GCPPAUSE, IRIN_GCPSTEPMUL, IRIN_GCPSTEPSIZE};
      int p = pnum[luaL_checkoption(L, 2, NULL, params)];
      irin_Integer value = luaL_optinteger(L, 3, -1);
      irin_pushinteger(L, irin_gc(L, o, p, (int)value));
      return 1;
    }
    default: {
      int res = irin_gc(L, o);
      checkvalres(res);
      irin_pushinteger(L, res);
      return 1;
    }
  }
  luaL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int luaB_type (irin_State *L) {
  int t = irin_type(L, 1);
  luaL_argcheck(L, t != IRIN_TNONE, 1, "value expected");
  irin_pushstring(L, irin_typename(L, t));
  return 1;
}


static int luaB_next (irin_State *L) {
  luaL_checktype(L, 1, IRIN_TTABLE);
  irin_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (irin_next(L, 1))
    return 2;
  else {
    irin_pushnil(L);
    return 1;
  }
}


static int pairscont (irin_State *L, int status, irin_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int luaB_pairs (irin_State *L) {
  luaL_checkany(L, 1);
  if (luaL_getmetafield(L, 1, "__pairs") == IRIN_TNIL) {  /* no metamethod? */
    irin_pushcfunction(L, luaB_next);  /* will return generator, */
    irin_pushvalue(L, 1);  /* state, */
    irin_pushnil(L);  /* and initial value */
  }
  else {
    irin_pushvalue(L, 1);  /* argument 'self' to metamethod */
    irin_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal fn for 'ipairs'
*/
static int ipairsaux (irin_State *L) {
  irin_Integer i = luaL_checkinteger(L, 2);
  i = luaL_intop(+, i, 1);
  irin_pushinteger(L, i);
  return (irin_geti(L, 1, i) == IRIN_TNIL) ? 1 : 2;
}


/*
** 'ipairs' fn. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int luaB_ipairs (irin_State *L) {
  luaL_checkany(L, 1);
  irin_pushcfunction(L, ipairsaux);  /* iteration fn */
  irin_pushvalue(L, 1);  /* state */
  irin_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (irin_State *L, int status, int envidx) {
  if (l_likely(status == IRIN_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      irin_pushvalue(L, envidx);  /* environment for loaded fn */
      if (!irin_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        irin_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    luaL_pushfail(L);
    irin_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static const char *getMode (irin_State *L, int idx) {
  const char *mode = luaL_optstring(L, idx, "bt");
  if (strchr(mode, 'B') != NULL)  /* Irin code cannot use fixed buffers */
    luaL_argerror(L, idx, "invalid mode");
  return mode;
}


static int luaB_loadfile (irin_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  const char *mode = getMode(L, 2);
  int env = (!irin_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = luaL_loadfilex(L, fname, mode);
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
** Reader for generic 'load' fn: 'irin_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (irin_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  luaL_checkstack(L, 2, "too many nested functions");
  irin_pushvalue(L, 1);  /* get fn */
  irin_call(L, 0, 1);  /* call it */
  if (irin_isnil(L, -1)) {
    irin_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!irin_isstring(L, -1)))
    luaL_error(L, "reader fn must return a string");
  irin_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return irin_tolstring(L, RESERVEDSLOT, size);
}


static int luaB_load (irin_State *L) {
  int status;
  size_t l;
  const char *s = irin_tolstring(L, 1, &l);
  const char *mode = getMode(L, 3);
  int env = (!irin_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = luaL_optstring(L, 2, s);
    status = luaL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader fn */
    const char *chunkname = luaL_optstring(L, 2, "=(load)");
    luaL_checktype(L, 1, IRIN_TFUNCTION);
    irin_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = irin_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (irin_State *L, int d1, irin_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'irin_Kfunction' prototype */
  return irin_gettop(L) - 1;
}


static int luaB_dofile (irin_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  irin_settop(L, 1);
  if (l_unlikely(luaL_loadfile(L, fname) != IRIN_OK))
    return irin_error(L);
  irin_callk(L, 0, IRIN_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int luaB_assert (irin_State *L) {
  if (l_likely(irin_toboolean(L, 1)))  /* condition is true? */
    return irin_gettop(L);  /* return all arguments */
  else {  /* error */
    luaL_checkany(L, 1);  /* there must be a condition */
    irin_remove(L, 1);  /* remove it */
    irin_pushliteral(L, "assertion failed!");  /* default message */
    irin_settop(L, 1);  /* leave only message (default if no other one) */
    return luaB_error(L);  /* call 'error' */
  }
}


static int luaB_select (irin_State *L) {
  int n = irin_gettop(L);
  if (irin_type(L, 1) == IRIN_TSTRING && *irin_tostring(L, 1) == '#') {
    irin_pushinteger(L, n-1);
    return 1;
  }
  else {
    irin_Integer i = luaL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    luaL_argcheck(L, 1 <= i, 1, "index out of range");
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
static int finishpcall (irin_State *L, int status, irin_KContext extra) {
  if (l_unlikely(status != IRIN_OK && status != IRIN_YIELD)) {  /* error? */
    irin_pushboolean(L, 0);  /* first result (false) */
    irin_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return irin_gettop(L) - (int)extra;  /* return all results */
}


static int luaB_pcall (irin_State *L) {
  int status;
  luaL_checkany(L, 1);
  irin_pushboolean(L, 1);  /* first result if no errors */
  irin_insert(L, 1);  /* put it in place */
  status = irin_pcallk(L, irin_gettop(L) - 2, IRIN_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'irin_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the fn passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int luaB_xpcall (irin_State *L) {
  int status;
  int n = irin_gettop(L);
  luaL_checktype(L, 2, IRIN_TFUNCTION);  /* check error fn */
  irin_pushboolean(L, 1);  /* first result */
  irin_pushvalue(L, 1);  /* fn */
  irin_rotate(L, 3, 2);  /* move them below fn's arguments */
  status = irin_pcallk(L, n - 2, IRIN_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int luaB_tostring (irin_State *L) {
  luaL_checkany(L, 1);
  luaL_tolstring(L, 1, NULL);
  return 1;
}


static const luaL_Reg base_funcs[] = {
  {"assert", luaB_assert},
  {"collectgarbage", luaB_collectgarbage},
  {"dofile", luaB_dofile},
  {"error", luaB_error},
  {"getmetatable", luaB_getmetatable},
  {"ipairs", luaB_ipairs},
  {"loadfile", luaB_loadfile},
  {"load", luaB_load},
  {"next", luaB_next},
  {"pairs", luaB_pairs},
  {"pcall", luaB_pcall},
  {"print", luaB_print},
  {"warn", luaB_warn},
  {"rawequal", luaB_rawequal},
  {"rawlen", luaB_rawlen},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"select", luaB_select},
  {"setmetatable", luaB_setmetatable},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"xpcall", luaB_xpcall},
  /* placeholders */
  {IRIN_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


LUAMOD_API int luaopen_base (irin_State *L) {
  /* open lib into global table */
  irin_pushglobaltable(L);
  luaL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  irin_pushvalue(L, -1);
  irin_setfield(L, -2, IRIN_GNAME);
  /* set global _VERSION */
  irin_pushliteral(L, IRIN_VERSION);
  irin_setfield(L, -2, "_VERSION");
  return 1;
}

