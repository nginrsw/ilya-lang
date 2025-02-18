#include "ilya.h"
#include "lauxlib.h"

static int id (ilya_State *L) {
  return ilya_gettop(L);
}


static const struct ilyaL_Reg funcs[] = {
  {"id", id},
  {NULL, NULL}
};


/* fn used by lib11.c */
ILYAMOD_API int lib1_export (ilya_State *L) {
  ilya_pushstring(L, "exported");
  return 1;
}


ILYAMOD_API int onefunction (ilya_State *L) {
  ilyaL_checkversion(L);
  ilya_settop(L, 2);
  ilya_pushvalue(L, 1);
  return 2;
}


ILYAMOD_API int anotherfunc (ilya_State *L) {
  ilyaL_checkversion(L);
  ilya_pushfstring(L, "%d%%%d\n", (int)ilya_tointeger(L, 1),
                                 (int)ilya_tointeger(L, 2));
  return 1;
} 


ILYAMOD_API int ilyaopen_lib1_sub (ilya_State *L) {
  ilya_setglobal(L, "y");  /* 2nd arg: extra value (file name) */
  ilya_setglobal(L, "x");  /* 1st arg: module name */
  ilyaL_newlib(L, funcs);
  return 1;
}

