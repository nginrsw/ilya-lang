/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in ilya.h
*/

#define lcorolib_c
#define ILYA_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "ilya.h"

#include "lauxlib.h"
#include "ilyalib.h"
#include "llimits.h"


static ilya_State *getco (ilya_State *L) {
  ilya_State *co = ilya_tothread(L, 1);
  ilyaL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (ilya_State *L, ilya_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!ilya_checkstack(co, narg))) {
    ilya_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  ilya_xmove(L, co, narg);
  status = ilya_resume(co, L, narg, &nres);
  if (l_likely(status == ILYA_OK || status == ILYA_YIELD)) {
    if (l_unlikely(!ilya_checkstack(L, nres + 1))) {
      ilya_pop(co, nres);  /* remove results anyway */
      ilya_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    ilya_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    ilya_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int ilyaB_coresume (ilya_State *L) {
  ilya_State *co = getco(L);
  int r;
  r = auxresume(L, co, ilya_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    ilya_pushboolean(L, 0);
    ilya_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    ilya_pushboolean(L, 1);
    ilya_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int ilyaB_auxwrap (ilya_State *L) {
  ilya_State *co = ilya_tothread(L, ilya_upvalueindex(1));
  int r = auxresume(L, co, ilya_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = ilya_status(co);
    if (stat != ILYA_OK && stat != ILYA_YIELD) {  /* error in the coroutine? */
      stat = ilya_closethread(co, L);  /* close its tbc variables */
      ilya_assert(stat != ILYA_OK);
      ilya_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != ILYA_ERRMEM &&  /* not a memory error and ... */
        ilya_type(L, -1) == ILYA_TSTRING) {  /* ... error object is a string? */
      ilyaL_where(L, 1);  /* add extra info, if available */
      ilya_insert(L, -2);
      ilya_concat(L, 2);
    }
    return ilya_error(L);  /* propagate error */
  }
  return r;
}


static int ilyaB_cocreate (ilya_State *L) {
  ilya_State *NL;
  ilyaL_checktype(L, 1, ILYA_TFUNCTION);
  NL = ilya_newthread(L);
  ilya_pushvalue(L, 1);  /* move fn to top */
  ilya_xmove(L, NL, 1);  /* move fn from L to NL */
  return 1;
}


static int ilyaB_cowrap (ilya_State *L) {
  ilyaB_cocreate(L);
  ilya_pushcclosure(L, ilyaB_auxwrap, 1);
  return 1;
}


static int ilyaB_yield (ilya_State *L) {
  return ilya_yield(L, ilya_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (ilya_State *L, ilya_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (ilya_status(co)) {
      case ILYA_YIELD:
        return COS_YIELD;
      case ILYA_OK: {
        ilya_Debug ar;
        if (ilya_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (ilya_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int ilyaB_costatus (ilya_State *L) {
  ilya_State *co = getco(L);
  ilya_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int ilyaB_yieldable (ilya_State *L) {
  ilya_State *co = ilya_isnone(L, 1) ? L : getco(L);
  ilya_pushboolean(L, ilya_isyieldable(co));
  return 1;
}


static int ilyaB_corunning (ilya_State *L) {
  int ismain = ilya_pushthread(L);
  ilya_pushboolean(L, ismain);
  return 2;
}


static int ilyaB_close (ilya_State *L) {
  ilya_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = ilya_closethread(co, L);
      if (status == ILYA_OK) {
        ilya_pushboolean(L, 1);
        return 1;
      }
      else {
        ilya_pushboolean(L, 0);
        ilya_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return ilyaL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const ilyaL_Reg co_funcs[] = {
  {"create", ilyaB_cocreate},
  {"resume", ilyaB_coresume},
  {"running", ilyaB_corunning},
  {"status", ilyaB_costatus},
  {"wrap", ilyaB_cowrap},
  {"yield", ilyaB_yield},
  {"isyieldable", ilyaB_yieldable},
  {"close", ilyaB_close},
  {NULL, NULL}
};



ILYAMOD_API int ilyaopen_coroutine (ilya_State *L) {
  ilyaL_newlib(L, co_funcs);
  return 1;
}

