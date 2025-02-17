#include "irin.h"
#include "lauxlib.h"

static int id (irin_State *L) {
  return irin_gettop(L);
}


static const struct luaL_Reg funcs[] = {
  {"id", id},
  {NULL, NULL}
};


/* function used by lib11.c */
LUAMOD_API int lib1_export (irin_State *L) {
  irin_pushstring(L, "exported");
  return 1;
}


LUAMOD_API int onefunction (irin_State *L) {
  luaL_checkversion(L);
  irin_settop(L, 2);
  irin_pushvalue(L, 1);
  return 2;
}


LUAMOD_API int anotherfunc (irin_State *L) {
  luaL_checkversion(L);
  irin_pushfstring(L, "%d%%%d\n", (int)irin_tointeger(L, 1),
                                 (int)irin_tointeger(L, 2));
  return 1;
} 


LUAMOD_API int luaopen_lib1_sub (irin_State *L) {
  irin_setglobal(L, "y");  /* 2nd arg: extra value (file name) */
  irin_setglobal(L, "x");  /* 1st arg: module name */
  luaL_newlib(L, funcs);
  return 1;
}

