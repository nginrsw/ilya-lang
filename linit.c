/*
** $Id: linit.c $
** Initialization of libraries for irin.c and other clients
** See Copyright Notice in irin.h
*/


#define linit_c
#define IRIN_LIB


#include "lprefix.h"


#include <stddef.h>

#include "irin.h"

#include "irinlib.h"
#include "lauxlib.h"
#include "llimits.h"


/*
** Standard Libraries. (Must be listed in the same ORDER of their
** respective constants IRIN_<libname>K.)
*/
static const luaL_Reg stdlibs[] = {
  {IRIN_GNAME, luaopen_base},
  {IRIN_LOADLIBNAME, luaopen_package},
  {IRIN_COLIBNAME, luaopen_coroutine},
  {IRIN_DBLIBNAME, luaopen_debug},
  {IRIN_IOLIBNAME, luaopen_io},
  {IRIN_MATHLIBNAME, luaopen_math},
  {IRIN_OSLIBNAME, luaopen_os},
  {IRIN_STRLIBNAME, luaopen_string},
  {IRIN_TABLIBNAME, luaopen_table},
  {IRIN_UTF8LIBNAME, luaopen_utf8},
  {NULL, NULL}
};


/*
** require and preload selected standard libraries
*/
LUALIB_API void luaL_openselectedlibs (irin_State *L, int load, int preload) {
  int mask;
  const luaL_Reg *lib;
  luaL_getsubtable(L, IRIN_REGISTRYINDEX, IRIN_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {  /* selected? */
      luaL_requiref(L, lib->name, lib->func, 1);  /* require library */
      irin_pop(L, 1);  /* remove result from the stack */
    }
    else if (preload & mask) {  /* selected? */
      irin_pushcfunction(L, lib->func);
      irin_setfield(L, -2, lib->name);  /* add library to PRELOAD table */
    }
  }
  irin_assert((mask >> 1) == IRIN_UTF8LIBK);
  irin_pop(L, 1);  /* remove PRELOAD table */
}

