/*
** $Id: linit.c $
** Initialization of libraries for ilya.c and other clients
** See Copyright Notice in ilya.h
*/


#define linit_c
#define ILYA_LIB


#include "lprefix.h"


#include <stddef.h>

#include "ilya.h"

#include "ilyalib.h"
#include "lauxlib.h"
#include "llimits.h"


/*
** Standard Libraries. (Must be listed in the same ORDER of their
** respective constants ILYA_<libname>K.)
*/
static const ilyaL_Reg stdlibs[] = {
  {ILYA_GNAME, ilyaopen_base},
  {ILYA_LOADLIBNAME, ilyaopen_package},
  {ILYA_COLIBNAME, ilyaopen_coroutine},
  {ILYA_DBLIBNAME, ilyaopen_debug},
  {ILYA_IOLIBNAME, ilyaopen_io},
  {ILYA_MATHLIBNAME, ilyaopen_math},
  {ILYA_OSLIBNAME, ilyaopen_os},
  {ILYA_STRLIBNAME, ilyaopen_string},
  {ILYA_TABLIBNAME, ilyaopen_table},
  {ILYA_UTF8LIBNAME, ilyaopen_utf8},
  {NULL, NULL}
};


/*
** require and preload selected standard libraries
*/
ILYALIB_API void ilyaL_openselectedlibs (ilya_State *L, int load, int preload) {
  int mask;
  const ilyaL_Reg *lib;
  ilyaL_getsubtable(L, ILYA_REGISTRYINDEX, ILYA_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {  /* selected? */
      ilyaL_requiref(L, lib->name, lib->func, 1);  /* require library */
      ilya_pop(L, 1);  /* remove result from the stack */
    }
    else if (preload & mask) {  /* selected? */
      ilya_pushcfunction(L, lib->func);
      ilya_setfield(L, -2, lib->name);  /* add library to PRELOAD table */
    }
  }
  ilya_assert((mask >> 1) == ILYA_UTF8LIBK);
  ilya_pop(L, 1);  /* remove PRELOAD table */
}

