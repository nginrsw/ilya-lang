#include "irin.h"
#include "lauxlib.h"

static int id (irin_State *L) {
  irin_pushboolean(L, 1);
  irin_insert(L, 1);
  return irin_gettop(L);
}


static const struct luaL_Reg funcs[] = {
  {"id", id},
  {NULL, NULL}
};


LUAMOD_API int luaopen_lib2 (irin_State *L) {
  irin_settop(L, 2);
  irin_setglobal(L, "y");  /* y gets 2nd parameter */
  irin_setglobal(L, "x");  /* x gets 1st parameter */
  luaL_newlib(L, funcs);
  return 1;
}


