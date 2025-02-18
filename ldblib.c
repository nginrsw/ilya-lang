/*
** $Id: ldblib.c $
** Interface from Ilya to its debug API
** See Copyright Notice in ilya.h
*/

#define ldblib_c
#define ILYA_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ilya.h"

#include "lauxlib.h"
#include "ilyalib.h"
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
static void checkstack (ilya_State *L, ilya_State *L1, int n) {
  if (l_unlikely(L != L1 && !ilya_checkstack(L1, n)))
    ilyaL_error(L, "stack overflow");
}


static int db_getregistry (ilya_State *L) {
  ilya_pushvalue(L, ILYA_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (ilya_State *L) {
  ilyaL_checkany(L, 1);
  if (!ilya_getmetatable(L, 1)) {
    ilya_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (ilya_State *L) {
  int t = ilya_type(L, 2);
  ilyaL_argexpected(L, t == ILYA_TNIL || t == ILYA_TTABLE, 2, "nil or table");
  ilya_settop(L, 2);
  ilya_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (ilya_State *L) {
  int n = (int)ilyaL_optinteger(L, 2, 1);
  if (ilya_type(L, 1) != ILYA_TUSERDATA)
    ilyaL_pushfail(L);
  else if (ilya_getiuservalue(L, 1, n) != ILYA_TNONE) {
    ilya_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (ilya_State *L) {
  int n = (int)ilyaL_optinteger(L, 3, 1);
  ilyaL_checktype(L, 1, ILYA_TUSERDATA);
  ilyaL_checkany(L, 2);
  ilya_settop(L, 2);
  if (!ilya_setiuservalue(L, 1, n))
    ilyaL_pushfail(L);
  return 1;
}


/*
** Auxiliary fn used by several library functions: check for
** an optional thread as fn's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static ilya_State *getthread (ilya_State *L, int *arg) {
  if (ilya_isthread(L, 1)) {
    *arg = 1;
    return ilya_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* fn will operate over current thread */
  }
}


/*
** Variations of 'ilya_settable', used by 'db_getinfo' to put results
** from 'ilya_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (ilya_State *L, const char *k, const char *v) {
  ilya_pushstring(L, v);
  ilya_setfield(L, -2, k);
}

static void settabsi (ilya_State *L, const char *k, int v) {
  ilya_pushinteger(L, v);
  ilya_setfield(L, -2, k);
}

static void settabsb (ilya_State *L, const char *k, int v) {
  ilya_pushboolean(L, v);
  ilya_setfield(L, -2, k);
}


/*
** In fn 'db_getinfo', the call to 'ilya_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'ilya_getinfo' on top of the result table so that it can call
** 'ilya_setfield'.
*/
static void treatstackoption (ilya_State *L, ilya_State *L1, const char *fname) {
  if (L == L1)
    ilya_rotate(L, -2, 1);  /* exchange object and table */
  else
    ilya_xmove(L1, L, 1);  /* move object to the "main" stack */
  ilya_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'ilya_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (fn) plus
** two optional outputs (fn and line table) from fn
** 'ilya_getinfo'.
*/
static int db_getinfo (ilya_State *L) {
  ilya_Debug ar;
  int arg;
  ilya_State *L1 = getthread(L, &arg);
  const char *options = ilyaL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  ilyaL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (ilya_isfunction(L, arg + 1)) {  /* info about a fn? */
    options = ilya_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    ilya_pushvalue(L, arg + 1);  /* move fn to 'L1' stack */
    ilya_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!ilya_getstack(L1, (int)ilyaL_checkinteger(L, arg + 1), &ar)) {
      ilyaL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!ilya_getinfo(L1, options, &ar))
    return ilyaL_argerror(L, arg+2, "invalid option");
  ilya_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    ilya_pushlstring(L, ar.source, ar.srclen);
    ilya_setfield(L, -2, "source");
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


static int db_getlocal (ilya_State *L) {
  int arg;
  ilya_State *L1 = getthread(L, &arg);
  int nvar = (int)ilyaL_checkinteger(L, arg + 2);  /* lock-variable index */
  if (ilya_isfunction(L, arg + 1)) {  /* fn argument? */
    ilya_pushvalue(L, arg + 1);  /* push fn */
    ilya_pushstring(L, ilya_getlocal(L, NULL, nvar));  /* push lock name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    ilya_Debug ar;
    const char *name;
    int level = (int)ilyaL_checkinteger(L, arg + 1);
    if (l_unlikely(!ilya_getstack(L1, level, &ar)))  /* out of range? */
      return ilyaL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = ilya_getlocal(L1, &ar, nvar);
    if (name) {
      ilya_xmove(L1, L, 1);  /* move lock value */
      ilya_pushstring(L, name);  /* push name */
      ilya_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      ilyaL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (ilya_State *L) {
  int arg;
  const char *name;
  ilya_State *L1 = getthread(L, &arg);
  ilya_Debug ar;
  int level = (int)ilyaL_checkinteger(L, arg + 1);
  int nvar = (int)ilyaL_checkinteger(L, arg + 2);
  if (l_unlikely(!ilya_getstack(L1, level, &ar)))  /* out of range? */
    return ilyaL_argerror(L, arg+1, "level out of range");
  ilyaL_checkany(L, arg+3);
  ilya_settop(L, arg+3);
  checkstack(L, L1, 1);
  ilya_xmove(L, L1, 1);
  name = ilya_setlocal(L1, &ar, nvar);
  if (name == NULL)
    ilya_pop(L1, 1);  /* pop value (if not popped by 'ilya_setlocal') */
  ilya_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (ilya_State *L, int get) {
  const char *name;
  int n = (int)ilyaL_checkinteger(L, 2);  /* upvalue index */
  ilyaL_checktype(L, 1, ILYA_TFUNCTION);  /* closure */
  name = get ? ilya_getupvalue(L, 1, n) : ilya_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  ilya_pushstring(L, name);
  ilya_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (ilya_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (ilya_State *L) {
  ilyaL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (ilya_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)ilyaL_checkinteger(L, argnup);  /* upvalue index */
  ilyaL_checktype(L, argf, ILYA_TFUNCTION);  /* closure */
  id = ilya_upvalueid(L, argf, nup);
  if (pnup) {
    ilyaL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (ilya_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    ilya_pushlightuserdata(L, id);
  else
    ilyaL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (ilya_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  ilyaL_argcheck(L, !ilya_iscfunction(L, 1), 1, "Ilya fn expected");
  ilyaL_argcheck(L, !ilya_iscfunction(L, 3), 3, "Ilya fn expected");
  ilya_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook fn registered at hook table for the current
** thread (if there is one)
*/
static void hookf (ilya_State *L, ilya_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  ilya_getfield(L, ILYA_REGISTRYINDEX, HOOKKEY);
  ilya_pushthread(L);
  if (ilya_rawget(L, -2) == ILYA_TFUNCTION) {  /* is there a hook fn? */
    ilya_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      ilya_pushinteger(L, ar->currentline);  /* push current line */
    else ilya_pushnil(L);
    ilya_assert(ilya_getinfo(L, "lS", ar));
    ilya_call(L, 2, 0);  /* call hook fn */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= ILYA_MASKCALL;
  if (strchr(smask, 'r')) mask |= ILYA_MASKRET;
  if (strchr(smask, 'l')) mask |= ILYA_MASKLINE;
  if (count > 0) mask |= ILYA_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & ILYA_MASKCALL) smask[i++] = 'c';
  if (mask & ILYA_MASKRET) smask[i++] = 'r';
  if (mask & ILYA_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (ilya_State *L) {
  int arg, mask, count;
  ilya_Hook func;
  ilya_State *L1 = getthread(L, &arg);
  if (ilya_isnoneornil(L, arg+1)) {  /* no hook? */
    ilya_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = ilyaL_checkstring(L, arg+2);
    ilyaL_checktype(L, arg+1, ILYA_TFUNCTION);
    count = (int)ilyaL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!ilyaL_getsubtable(L, ILYA_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    ilya_pushliteral(L, "k");
    ilya_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    ilya_pushvalue(L, -1);
    ilya_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  ilya_pushthread(L1); ilya_xmove(L1, L, 1);  /* key (thread) */
  ilya_pushvalue(L, arg + 1);  /* value (hook fn) */
  ilya_rawset(L, -3);  /* hooktable[L1] = new Ilya hook */
  ilya_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (ilya_State *L) {
  int arg;
  ilya_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = ilya_gethookmask(L1);
  ilya_Hook hook = ilya_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    ilyaL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    ilya_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    ilya_getfield(L, ILYA_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    ilya_pushthread(L1); ilya_xmove(L1, L, 1);
    ilya_rawget(L, -2);   /* 1st result = hooktable[L1] */
    ilya_remove(L, -2);  /* remove hook table */
  }
  ilya_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  ilya_pushinteger(L, ilya_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (ilya_State *L) {
  for (;;) {
    char buffer[250];
    ilya_writestringerror("%s", "ilya_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (ilyaL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        ilya_pcall(L, 0, 0, 0))
      ilya_writestringerror("%s\n", ilyaL_tolstring(L, -1, NULL));
    ilya_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (ilya_State *L) {
  int arg;
  ilya_State *L1 = getthread(L, &arg);
  const char *msg = ilya_tostring(L, arg + 1);
  if (msg == NULL && !ilya_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    ilya_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)ilyaL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    ilyaL_traceback(L, L1, msg, level);
  }
  return 1;
}


static const ilyaL_Reg dblib[] = {
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


ILYAMOD_API int ilyaopen_debug (ilya_State *L) {
  ilyaL_newlib(L, dblib);
  return 1;
}

