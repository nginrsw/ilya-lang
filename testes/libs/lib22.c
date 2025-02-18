#include "ilya.h"
#include "lauxlib.h"

static int id (ilya_State *L) {
  ilya_pushboolean(L, 1);
  ilya_insert(L, 1);
  return ilya_gettop(L);
}


static const struct ilyaL_Reg funcs[] = {
  {"id", id},
  {NULL, NULL}
};


ILYAMOD_API int ilyaopen_lib2 (ilya_State *L) {
  ilya_settop(L, 2);
  ilya_setglobal(L, "y");  /* y gets 2nd parameter */
  ilya_setglobal(L, "x");  /* x gets 1st parameter */
  ilyaL_newlib(L, funcs);
  return 1;
}


