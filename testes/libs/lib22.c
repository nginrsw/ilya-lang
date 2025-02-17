#include "ilya.h"
#include "lauxlib.h"

static int id (ilya_State *L) {
  ilya_pushboolean(L, 1);
  ilya_insert(L, 1);
  return ilya_gettop(L);
}


static const struct luaL_Reg funcs[] = {
  {"id", id},
  {NULL, NULL}
};


LUAMOD_API int luaopen_lib2 (ilya_State *L) {
  ilya_settop(L, 2);
  ilya_setglobal(L, "y");  /* y gets 2nd parameter */
  ilya_setglobal(L, "x");  /* x gets 1st parameter */
  luaL_newlib(L, funcs);
  return 1;
}


