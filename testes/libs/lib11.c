#include "ilya.h"

/* fn from lib1.c */
int lib1_export (ilya_State *L);

LUAMOD_API int luaopen_lib11 (ilya_State *L) {
  return lib1_export(L);
}


