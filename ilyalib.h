/*
** $Id: ilyalib.h $
** Ilya standard libraries
** See Copyright Notice in ilya.h
*/


#ifndef lualib_h
#define lualib_h

#include "ilya.h"


/* version suffix for environment variable names */
#define ILYA_VERSUFFIX          "_" ILYA_VERSION_MAJOR "_" ILYA_VERSION_MINOR

#define ILYA_GLIBK		1
LUAMOD_API int (luaopen_base) (ilya_State *L);

#define ILYA_LOADLIBNAME	"package"
#define ILYA_LOADLIBK	(ILYA_GLIBK << 1)
LUAMOD_API int (luaopen_package) (ilya_State *L);


#define ILYA_COLIBNAME	"coroutine"
#define ILYA_COLIBK	(ILYA_LOADLIBK << 1)
LUAMOD_API int (luaopen_coroutine) (ilya_State *L);

#define ILYA_DBLIBNAME	"debug"
#define ILYA_DBLIBK	(ILYA_COLIBK << 1)
LUAMOD_API int (luaopen_debug) (ilya_State *L);

#define ILYA_IOLIBNAME	"io"
#define ILYA_IOLIBK	(ILYA_DBLIBK << 1)
LUAMOD_API int (luaopen_io) (ilya_State *L);

#define ILYA_MATHLIBNAME	"math"
#define ILYA_MATHLIBK	(ILYA_IOLIBK << 1)
LUAMOD_API int (luaopen_math) (ilya_State *L);

#define ILYA_OSLIBNAME	"os"
#define ILYA_OSLIBK	(ILYA_MATHLIBK << 1)
LUAMOD_API int (luaopen_os) (ilya_State *L);

#define ILYA_STRLIBNAME	"string"
#define ILYA_STRLIBK	(ILYA_OSLIBK << 1)
LUAMOD_API int (luaopen_string) (ilya_State *L);

#define ILYA_TABLIBNAME	"table"
#define ILYA_TABLIBK	(ILYA_STRLIBK << 1)
LUAMOD_API int (luaopen_table) (ilya_State *L);

#define ILYA_UTF8LIBNAME	"utf8"
#define ILYA_UTF8LIBK	(ILYA_TABLIBK << 1)
LUAMOD_API int (luaopen_utf8) (ilya_State *L);


/* open selected libraries */
LUALIB_API void (luaL_openselectedlibs) (ilya_State *L, int load, int preload);

/* open all libraries */
#define luaL_openlibs(L)	luaL_openselectedlibs(L, ~0, 0)


#endif
