/*
** $Id: irinlib.h $
** Irin standard libraries
** See Copyright Notice in irin.h
*/


#ifndef lualib_h
#define lualib_h

#include "irin.h"


/* version suffix for environment variable names */
#define IRIN_VERSUFFIX          "_" IRIN_VERSION_MAJOR "_" IRIN_VERSION_MINOR

#define IRIN_GLIBK		1
LUAMOD_API int (luaopen_base) (irin_State *L);

#define IRIN_LOADLIBNAME	"package"
#define IRIN_LOADLIBK	(IRIN_GLIBK << 1)
LUAMOD_API int (luaopen_package) (irin_State *L);


#define IRIN_COLIBNAME	"coroutine"
#define IRIN_COLIBK	(IRIN_LOADLIBK << 1)
LUAMOD_API int (luaopen_coroutine) (irin_State *L);

#define IRIN_DBLIBNAME	"debug"
#define IRIN_DBLIBK	(IRIN_COLIBK << 1)
LUAMOD_API int (luaopen_debug) (irin_State *L);

#define IRIN_IOLIBNAME	"io"
#define IRIN_IOLIBK	(IRIN_DBLIBK << 1)
LUAMOD_API int (luaopen_io) (irin_State *L);

#define IRIN_MATHLIBNAME	"math"
#define IRIN_MATHLIBK	(IRIN_IOLIBK << 1)
LUAMOD_API int (luaopen_math) (irin_State *L);

#define IRIN_OSLIBNAME	"os"
#define IRIN_OSLIBK	(IRIN_MATHLIBK << 1)
LUAMOD_API int (luaopen_os) (irin_State *L);

#define IRIN_STRLIBNAME	"string"
#define IRIN_STRLIBK	(IRIN_OSLIBK << 1)
LUAMOD_API int (luaopen_string) (irin_State *L);

#define IRIN_TABLIBNAME	"table"
#define IRIN_TABLIBK	(IRIN_STRLIBK << 1)
LUAMOD_API int (luaopen_table) (irin_State *L);

#define IRIN_UTF8LIBNAME	"utf8"
#define IRIN_UTF8LIBK	(IRIN_TABLIBK << 1)
LUAMOD_API int (luaopen_utf8) (irin_State *L);


/* open selected libraries */
LUALIB_API void (luaL_openselectedlibs) (irin_State *L, int load, int preload);

/* open all libraries */
#define luaL_openlibs(L)	luaL_openselectedlibs(L, ~0, 0)


#endif
