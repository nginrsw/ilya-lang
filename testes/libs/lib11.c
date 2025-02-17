#include "irin.h"

/* function from lib1.c */
int lib1_export (irin_State *L);

LUAMOD_API int luaopen_lib11 (irin_State *L) {
  return lib1_export(L);
}


