/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in irin.h
*/

#define lcorolib_c
#define IRIN_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "irin.h"

#include "lauxlib.h"
#include "irinlib.h"
#include "llimits.h"


static irin_State *getco (irin_State *L) {
  irin_State *co = irin_tothread(L, 1);
  luaL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (irin_State *L, irin_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!irin_checkstack(co, narg))) {
    irin_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  irin_xmove(L, co, narg);
  status = irin_resume(co, L, narg, &nres);
  if (l_likely(status == IRIN_OK || status == IRIN_YIELD)) {
    if (l_unlikely(!irin_checkstack(L, nres + 1))) {
      irin_pop(co, nres);  /* remove results anyway */
      irin_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    irin_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    irin_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int luaB_coresume (irin_State *L) {
  irin_State *co = getco(L);
  int r;
  r = auxresume(L, co, irin_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    irin_pushboolean(L, 0);
    irin_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    irin_pushboolean(L, 1);
    irin_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int luaB_auxwrap (irin_State *L) {
  irin_State *co = irin_tothread(L, irin_upvalueindex(1));
  int r = auxresume(L, co, irin_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = irin_status(co);
    if (stat != IRIN_OK && stat != IRIN_YIELD) {  /* error in the coroutine? */
      stat = irin_closethread(co, L);  /* close its tbc variables */
      irin_assert(stat != IRIN_OK);
      irin_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != IRIN_ERRMEM &&  /* not a memory error and ... */
        irin_type(L, -1) == IRIN_TSTRING) {  /* ... error object is a string? */
      luaL_where(L, 1);  /* add extra info, if available */
      irin_insert(L, -2);
      irin_concat(L, 2);
    }
    return irin_error(L);  /* propagate error */
  }
  return r;
}


static int luaB_cocreate (irin_State *L) {
  irin_State *NL;
  luaL_checktype(L, 1, IRIN_TFUNCTION);
  NL = irin_newthread(L);
  irin_pushvalue(L, 1);  /* move fn to top */
  irin_xmove(L, NL, 1);  /* move fn from L to NL */
  return 1;
}


static int luaB_cowrap (irin_State *L) {
  luaB_cocreate(L);
  irin_pushcclosure(L, luaB_auxwrap, 1);
  return 1;
}


static int luaB_yield (irin_State *L) {
  return irin_yield(L, irin_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (irin_State *L, irin_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (irin_status(co)) {
      case IRIN_YIELD:
        return COS_YIELD;
      case IRIN_OK: {
        irin_Debug ar;
        if (irin_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (irin_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int luaB_costatus (irin_State *L) {
  irin_State *co = getco(L);
  irin_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int luaB_yieldable (irin_State *L) {
  irin_State *co = irin_isnone(L, 1) ? L : getco(L);
  irin_pushboolean(L, irin_isyieldable(co));
  return 1;
}


static int luaB_corunning (irin_State *L) {
  int ismain = irin_pushthread(L);
  irin_pushboolean(L, ismain);
  return 2;
}


static int luaB_close (irin_State *L) {
  irin_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = irin_closethread(co, L);
      if (status == IRIN_OK) {
        irin_pushboolean(L, 1);
        return 1;
      }
      else {
        irin_pushboolean(L, 0);
        irin_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return luaL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const luaL_Reg co_funcs[] = {
  {"create", luaB_cocreate},
  {"resume", luaB_coresume},
  {"running", luaB_corunning},
  {"status", luaB_costatus},
  {"wrap", luaB_cowrap},
  {"yield", luaB_yield},
  {"isyieldable", luaB_yieldable},
  {"close", luaB_close},
  {NULL, NULL}
};



LUAMOD_API int luaopen_coroutine (irin_State *L) {
  luaL_newlib(L, co_funcs);
  return 1;
}

