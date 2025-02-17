/*
** $Id: ldblib.c $
** Interface from Irin to its debug API
** See Copyright Notice in irin.h
*/

#define ldblib_c
#define IRIN_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irin.h"

#include "lauxlib.h"
#include "irinlib.h"
#include "llimits.h"


/*
** The hook table at registry[HOOKKEY] maps threads to their current
** hook fn.
*/
static const char *const HOOKKEY = "_HOOKKEY";


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (irin_State *L, irin_State *L1, int n) {
  if (l_unlikely(L != L1 && !irin_checkstack(L1, n)))
    luaL_error(L, "stack overflow");
}


static int db_getregistry (irin_State *L) {
  irin_pushvalue(L, IRIN_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (irin_State *L) {
  luaL_checkany(L, 1);
  if (!irin_getmetatable(L, 1)) {
    irin_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (irin_State *L) {
  int t = irin_type(L, 2);
  luaL_argexpected(L, t == IRIN_TNIL || t == IRIN_TTABLE, 2, "nil or table");
  irin_settop(L, 2);
  irin_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (irin_State *L) {
  int n = (int)luaL_optinteger(L, 2, 1);
  if (irin_type(L, 1) != IRIN_TUSERDATA)
    luaL_pushfail(L);
  else if (irin_getiuservalue(L, 1, n) != IRIN_TNONE) {
    irin_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (irin_State *L) {
  int n = (int)luaL_optinteger(L, 3, 1);
  luaL_checktype(L, 1, IRIN_TUSERDATA);
  luaL_checkany(L, 2);
  irin_settop(L, 2);
  if (!irin_setiuservalue(L, 1, n))
    luaL_pushfail(L);
  return 1;
}


/*
** Auxiliary fn used by several library functions: check for
** an optional thread as fn's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static irin_State *getthread (irin_State *L, int *arg) {
  if (irin_isthread(L, 1)) {
    *arg = 1;
    return irin_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* fn will operate over current thread */
  }
}


/*
** Variations of 'irin_settable', used by 'db_getinfo' to put results
** from 'irin_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (irin_State *L, const char *k, const char *v) {
  irin_pushstring(L, v);
  irin_setfield(L, -2, k);
}

static void settabsi (irin_State *L, const char *k, int v) {
  irin_pushinteger(L, v);
  irin_setfield(L, -2, k);
}

static void settabsb (irin_State *L, const char *k, int v) {
  irin_pushboolean(L, v);
  irin_setfield(L, -2, k);
}


/*
** In fn 'db_getinfo', the call to 'irin_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'irin_getinfo' on top of the result table so that it can call
** 'irin_setfield'.
*/
static void treatstackoption (irin_State *L, irin_State *L1, const char *fname) {
  if (L == L1)
    irin_rotate(L, -2, 1);  /* exchange object and table */
  else
    irin_xmove(L1, L, 1);  /* move object to the "main" stack */
  irin_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'irin_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (fn) plus
** two optional outputs (fn and line table) from fn
** 'irin_getinfo'.
*/
static int db_getinfo (irin_State *L) {
  irin_Debug ar;
  int arg;
  irin_State *L1 = getthread(L, &arg);
  const char *options = luaL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  luaL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (irin_isfunction(L, arg + 1)) {  /* info about a fn? */
    options = irin_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    irin_pushvalue(L, arg + 1);  /* move fn to 'L1' stack */
    irin_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!irin_getstack(L1, (int)luaL_checkinteger(L, arg + 1), &ar)) {
      luaL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!irin_getinfo(L1, options, &ar))
    return luaL_argerror(L, arg+2, "invalid option");
  irin_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    irin_pushlstring(L, ar.source, ar.srclen);
    irin_setfield(L, -2, "source");
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'r')) {
    settabsi(L, "ftransfer", ar.ftransfer);
    settabsi(L, "ntransfer", ar.ntransfer);
  }
  if (strchr(options, 't')) {
    settabsb(L, "istailcall", ar.istailcall);
    settabsi(L, "extraargs", ar.extraargs);
  }
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


static int db_getlocal (irin_State *L) {
  int arg;
  irin_State *L1 = getthread(L, &arg);
  int nvar = (int)luaL_checkinteger(L, arg + 2);  /* locked-variable index */
  if (irin_isfunction(L, arg + 1)) {  /* fn argument? */
    irin_pushvalue(L, arg + 1);  /* push fn */
    irin_pushstring(L, irin_getlocal(L, NULL, nvar));  /* push locked name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    irin_Debug ar;
    const char *name;
    int level = (int)luaL_checkinteger(L, arg + 1);
    if (l_unlikely(!irin_getstack(L1, level, &ar)))  /* out of range? */
      return luaL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = irin_getlocal(L1, &ar, nvar);
    if (name) {
      irin_xmove(L1, L, 1);  /* move locked value */
      irin_pushstring(L, name);  /* push name */
      irin_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      luaL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (irin_State *L) {
  int arg;
  const char *name;
  irin_State *L1 = getthread(L, &arg);
  irin_Debug ar;
  int level = (int)luaL_checkinteger(L, arg + 1);
  int nvar = (int)luaL_checkinteger(L, arg + 2);
  if (l_unlikely(!irin_getstack(L1, level, &ar)))  /* out of range? */
    return luaL_argerror(L, arg+1, "level out of range");
  luaL_checkany(L, arg+3);
  irin_settop(L, arg+3);
  checkstack(L, L1, 1);
  irin_xmove(L, L1, 1);
  name = irin_setlocal(L1, &ar, nvar);
  if (name == NULL)
    irin_pop(L1, 1);  /* pop value (if not popped by 'irin_setlocal') */
  irin_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (irin_State *L, int get) {
  const char *name;
  int n = (int)luaL_checkinteger(L, 2);  /* upvalue index */
  luaL_checktype(L, 1, IRIN_TFUNCTION);  /* closure */
  name = get ? irin_getupvalue(L, 1, n) : irin_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  irin_pushstring(L, name);
  irin_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (irin_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (irin_State *L) {
  luaL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (irin_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)luaL_checkinteger(L, argnup);  /* upvalue index */
  luaL_checktype(L, argf, IRIN_TFUNCTION);  /* closure */
  id = irin_upvalueid(L, argf, nup);
  if (pnup) {
    luaL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (irin_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    irin_pushlightuserdata(L, id);
  else
    luaL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (irin_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  luaL_argcheck(L, !irin_iscfunction(L, 1), 1, "Irin fn expected");
  luaL_argcheck(L, !irin_iscfunction(L, 3), 3, "Irin fn expected");
  irin_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook fn registered at hook table for the current
** thread (if there is one)
*/
static void hookf (irin_State *L, irin_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  irin_getfield(L, IRIN_REGISTRYINDEX, HOOKKEY);
  irin_pushthread(L);
  if (irin_rawget(L, -2) == IRIN_TFUNCTION) {  /* is there a hook fn? */
    irin_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      irin_pushinteger(L, ar->currentline);  /* push current line */
    else irin_pushnil(L);
    irin_assert(irin_getinfo(L, "lS", ar));
    irin_call(L, 2, 0);  /* call hook fn */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= IRIN_MASKCALL;
  if (strchr(smask, 'r')) mask |= IRIN_MASKRET;
  if (strchr(smask, 'l')) mask |= IRIN_MASKLINE;
  if (count > 0) mask |= IRIN_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & IRIN_MASKCALL) smask[i++] = 'c';
  if (mask & IRIN_MASKRET) smask[i++] = 'r';
  if (mask & IRIN_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (irin_State *L) {
  int arg, mask, count;
  irin_Hook func;
  irin_State *L1 = getthread(L, &arg);
  if (irin_isnoneornil(L, arg+1)) {  /* no hook? */
    irin_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = luaL_checkstring(L, arg+2);
    luaL_checktype(L, arg+1, IRIN_TFUNCTION);
    count = (int)luaL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!luaL_getsubtable(L, IRIN_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    irin_pushliteral(L, "k");
    irin_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    irin_pushvalue(L, -1);
    irin_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  irin_pushthread(L1); irin_xmove(L1, L, 1);  /* key (thread) */
  irin_pushvalue(L, arg + 1);  /* value (hook fn) */
  irin_rawset(L, -3);  /* hooktable[L1] = new Irin hook */
  irin_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (irin_State *L) {
  int arg;
  irin_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = irin_gethookmask(L1);
  irin_Hook hook = irin_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    luaL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    irin_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    irin_getfield(L, IRIN_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    irin_pushthread(L1); irin_xmove(L1, L, 1);
    irin_rawget(L, -2);   /* 1st result = hooktable[L1] */
    irin_remove(L, -2);  /* remove hook table */
  }
  irin_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  irin_pushinteger(L, irin_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (irin_State *L) {
  for (;;) {
    char buffer[250];
    irin_writestringerror("%s", "irin_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (luaL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        irin_pcall(L, 0, 0, 0))
      irin_writestringerror("%s\n", luaL_tolstring(L, -1, NULL));
    irin_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (irin_State *L) {
  int arg;
  irin_State *L1 = getthread(L, &arg);
  const char *msg = irin_tostring(L, arg + 1);
  if (msg == NULL && !irin_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    irin_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)luaL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    luaL_traceback(L, L1, msg, level);
  }
  return 1;
}


static const luaL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {NULL, NULL}
};


LUAMOD_API int luaopen_debug (irin_State *L) {
  luaL_newlib(L, dblib);
  return 1;
}

