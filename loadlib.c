/*
** $Id: loadlib.c $
** Dynamic library loader for Ilya
** See Copyright Notice in ilya.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define ILYA_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ilya.h"

#include "lauxlib.h"
#include "ilyalib.h"
#include "llimits.h"


/*
** ILYA_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** ILYA_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Ilya loader.
*/
#if !defined(ILYA_CSUBSEP)
#define ILYA_CSUBSEP		ILYA_DIRSEP
#endif

#if !defined(ILYA_LSUBSEP)
#define ILYA_LSUBSEP		ILYA_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define ILYA_POF		"luaopen_"

/* separator for open functions in C libraries */
#define ILYA_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/* cast void* to a Ilya fn */
#define cast_Lfunc(p)	cast(ilya_CFunction, cast_func(p))


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (ilya_State *L, const char *path, int seeglb);

/*
** Try to find a fn named 'sym' in library 'lib'.
** Returns the fn; in case of error, returns NULL plus an
** error string in the stack.
*/
static ilya_CFunction lsys_sym (ilya_State *L, void *lib, const char *sym);




#if defined(ILYA_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface,
** which is available in all POSIX systems.
** =========================================================================
*/

#include <dlfcn.h>


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (ilya_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    ilya_pushstring(L, dlerror());
  return lib;
}


static ilya_CFunction lsys_sym (ilya_State *L, void *lib, const char *sym) {
  ilya_CFunction f = cast_Lfunc(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    ilya_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(ILYA_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(ILYA_LLE_FLAGS)
#define ILYA_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of ILYA_EXEC_DIR with the executable's path.
*/
static void setprogdir (ilya_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    luaL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    luaL_gsub(L, ilya_tostring(L, -1), ILYA_EXEC_DIR, buff);
    ilya_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (ilya_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    ilya_pushstring(L, buffer);
  else
    ilya_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (ilya_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, ILYA_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static ilya_CFunction lsys_sym (ilya_State *L, void *lib, const char *sym) {
  ilya_CFunction f = cast_Lfunc(GetProcAddress((HMODULE)lib, sym));
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your Ilya installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (ilya_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  ilya_pushliteral(L, DLMSG);
  return NULL;
}


static ilya_CFunction lsys_sym (ilya_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  ilya_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** ILYA_PATH_VAR and ILYA_CPATH_VAR are the names of the environment
** variables that Ilya check to set its paths.
*/
#if !defined(ILYA_PATH_VAR)
#define ILYA_PATH_VAR    "ILYA_PATH"
#endif

#if !defined(ILYA_CPATH_VAR)
#define ILYA_CPATH_VAR   "ILYA_CPATH"
#endif



/*
** return registry.ILYA_NOENV as a boolean
*/
static int noenv (ilya_State *L) {
  int b;
  ilya_getfield(L, ILYA_REGISTRYINDEX, "ILYA_NOENV");
  b = ilya_toboolean(L, -1);
  ilya_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path. (If using the default path, assume it is a string
** literal in C and create it as an external string.)
*/
static void setpath (ilya_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = ilya_pushfstring(L, "%s%s", envname, ILYA_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    ilya_pushexternalstring(L, dft, strlen(dft), NULL, NULL);  /* use default */
  else if ((dftmark = strstr(path, ILYA_PATH_SEP ILYA_PATH_SEP)) == NULL)
    ilya_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      luaL_addlstring(&b, path, ct_diff2sz(dftmark - path));  /* add it */
      luaL_addchar(&b, *ILYA_PATH_SEP);
    }
    luaL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      luaL_addchar(&b, *ILYA_PATH_SEP);
      luaL_addlstring(&b, dftmark + 2, ct_diff2sz((path + len - 2) - dftmark));
    }
    luaL_pushresult(&b);
  }
  setprogdir(L);
  ilya_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  ilya_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (ilya_State *L, const char *path) {
  void *plib;
  ilya_getfield(L, ILYA_REGISTRYINDEX, CLIBS);
  ilya_getfield(L, -1, path);
  plib = ilya_touserdata(L, -1);  /* plib = CLIBS[path] */
  ilya_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (ilya_State *L, const char *path, void *plib) {
  ilya_getfield(L, ILYA_REGISTRYINDEX, CLIBS);
  ilya_pushlightuserdata(L, plib);
  ilya_pushvalue(L, -1);
  ilya_setfield(L, -3, path);  /* CLIBS[path] = plib */
  ilya_rawseti(L, -2, luaL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  ilya_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (ilya_State *L) {
  ilya_Integer n = luaL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    ilya_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(ilya_touserdata(L, -1));
    ilya_pop(L, 1);  /* pop handle */
  }
  return 0;
}



/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C fn named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C fn with that symbol.
** Return 0 and 'true' or a fn in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc (ilya_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no fn)? */
    ilya_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    ilya_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find fn */
    ilya_pushcfunction(L, f);  /* else create new fn */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (ilya_State *L) {
  const char *path = luaL_checkstring(L, 1);
  const char *init = luaL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded fn */
  else {  /* error; error message is on stack top */
    luaL_pushfail(L);
    ilya_insert(L, -2);
    ilya_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' fn
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *ILYA_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *ILYA_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name goes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (ilya_State *L, const char *path) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addstring(&b, "no file '");
  luaL_addgsub(&b, path, ILYA_PATH_SEP, "'\n\tno file '");
  luaL_addstring(&b, "'");
  luaL_pushresult(&b);
}


static const char *searchpath (ilya_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  luaL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = luaL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  luaL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  luaL_addgsub(&buff, path, ILYA_PATH_MARK, name);
  luaL_addchar(&buff, '\0');
  pathname = luaL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + luaL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return ilya_pushstring(L, filename);  /* save and return name */
  }
  luaL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, ilya_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (ilya_State *L) {
  const char *f = searchpath(L, luaL_checkstring(L, 1),
                                luaL_checkstring(L, 2),
                                luaL_optstring(L, 3, "."),
                                luaL_optstring(L, 4, ILYA_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    luaL_pushfail(L);
    ilya_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (ilya_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  ilya_getfield(L, ilya_upvalueindex(1), pname);
  path = ilya_tostring(L, -1);
  if (l_unlikely(path == NULL))
    luaL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (ilya_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    ilya_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open fn and file name */
  }
  else
    return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          ilya_tostring(L, 1), filename, ilya_tostring(L, -1));
}


static int searcher_Lua (ilya_State *L) {
  const char *filename;
  const char *name = luaL_checkstring(L, 1);
  filename = findfile(L, name, "path", ILYA_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (luaL_loadfile(L, filename) == ILYA_OK), filename);
}


/*
** Try to find a load fn for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a fn
** name "luaopen_X" and look for it. (For compatibility, if that
** fails, it also tries "luaopen_Y".) If there is no ignore mark,
** look for a fn named "luaopen_modname".
*/
static int loadfunc (ilya_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = luaL_gsub(L, modname, ".", ILYA_OFSEP);
  mark = strchr(modname, *ILYA_IGMARK);
  if (mark) {
    int stat;
    openfunc = ilya_pushlstring(L, modname, ct_diff2sz(mark - modname));
    openfunc = ilya_pushfstring(L, ILYA_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = ilya_pushfstring(L, ILYA_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (ilya_State *L) {
  const char *name = luaL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", ILYA_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (ilya_State *L) {
  const char *filename;
  const char *name = luaL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  ilya_pushlstring(L, name, ct_diff2sz(p - name));
  filename = findfile(L, ilya_tostring(L, -1), "cpath", ILYA_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open fn not found */
      ilya_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  ilya_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (ilya_State *L) {
  const char *name = luaL_checkstring(L, 1);
  ilya_getfield(L, ILYA_REGISTRYINDEX, ILYA_PRELOAD_TABLE);
  if (ilya_getfield(L, -1, name) == ILYA_TNIL) {  /* not found? */
    ilya_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    ilya_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (ilya_State *L, const char *name) {
  int i;
  luaL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(ilya_getfield(L, ilya_upvalueindex(1), "searchers")
                 != ILYA_TTABLE))
    luaL_error(L, "'package.searchers' must be a table");
  luaL_buffinit(L, &msg);
  luaL_addstring(&msg, "\n\t");  /* error-message prefix for first message */
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    if (l_unlikely(ilya_rawgeti(L, 3, i) == ILYA_TNIL)) {  /* no more searchers? */
      ilya_pop(L, 1);  /* remove nil */
      luaL_buffsub(&msg, 2);  /* remove last prefix */
      luaL_pushresult(&msg);  /* create error message */
      luaL_error(L, "module '%s' not found:%s", name, ilya_tostring(L, -1));
    }
    ilya_pushstring(L, name);
    ilya_call(L, 1, 2);  /* call it */
    if (ilya_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (ilya_isstring(L, -2)) {  /* searcher returned error message? */
      ilya_pop(L, 1);  /* remove extra return */
      luaL_addvalue(&msg);  /* concatenate error message */
      luaL_addstring(&msg, "\n\t");  /* prefix for next message */
    }
    else  /* no error message */
      ilya_pop(L, 2);  /* remove both returns */
  }
}


static int ll_require (ilya_State *L) {
  const char *name = luaL_checkstring(L, 1);
  ilya_settop(L, 1);  /* LOADED table will be at index 2 */
  ilya_getfield(L, ILYA_REGISTRYINDEX, ILYA_LOADED_TABLE);
  ilya_getfield(L, 2, name);  /* LOADED[name] */
  if (ilya_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  ilya_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  ilya_rotate(L, -2, 1);  /* fn <-> loader data */
  ilya_pushvalue(L, 1);  /* name is 1st argument to module loader */
  ilya_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader fn; mod. name; loader data */
  ilya_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!ilya_isnil(L, -1))  /* non-nil return? */
    ilya_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    ilya_pop(L, 1);  /* pop nil */
  if (ilya_getfield(L, 2, name) == ILYA_TNIL) {   /* module set no value? */
    ilya_pushboolean(L, 1);  /* use true as result */
    ilya_copy(L, -1, -2);  /* replace loader result */
    ilya_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  ilya_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const luaL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const luaL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (ilya_State *L) {
  static const ilya_CFunction searchers[] = {
    searcher_preload,
    searcher_Lua,
    searcher_C,
    searcher_Croot,
    NULL
  };
  int i;
  /* create 'searchers' table */
  ilya_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    ilya_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    ilya_pushcclosure(L, searchers[i], 1);
    ilya_rawseti(L, -2, i+1);
  }
  ilya_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (ilya_State *L) {
  luaL_getsubtable(L, ILYA_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  ilya_createtable(L, 0, 1);  /* create metatable for CLIBS */
  ilya_pushcfunction(L, gctm);
  ilya_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  ilya_setmetatable(L, -2);
}


LUAMOD_API int luaopen_package (ilya_State *L) {
  createclibstable(L);
  luaL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", ILYA_PATH_VAR, ILYA_PATH_DEFAULT);
  setpath(L, "cpath", ILYA_CPATH_VAR, ILYA_CPATH_DEFAULT);
  /* store config information */
  ilya_pushliteral(L, ILYA_DIRSEP "\n" ILYA_PATH_SEP "\n" ILYA_PATH_MARK "\n"
                     ILYA_EXEC_DIR "\n" ILYA_IGMARK "\n");
  ilya_setfield(L, -2, "config");
  /* set field 'loaded' */
  luaL_getsubtable(L, ILYA_REGISTRYINDEX, ILYA_LOADED_TABLE);
  ilya_setfield(L, -2, "loaded");
  /* set field 'preload' */
  luaL_getsubtable(L, ILYA_REGISTRYINDEX, ILYA_PRELOAD_TABLE);
  ilya_setfield(L, -2, "preload");
  ilya_pushglobaltable(L);
  ilya_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  luaL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  ilya_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

