#include "irin.h"


int luaopen_lib2 (irin_State *L);

LUAMOD_API int luaopen_lib21 (irin_State *L) {
  return luaopen_lib2(L);
}


