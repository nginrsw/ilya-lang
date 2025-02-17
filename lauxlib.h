/*
** $Id: lauxlib.h $
** Auxiliary functions for building Irin libraries
** See Copyright Notice in irin.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "irinconf.h"
#include "irin.h"


/* global table */
#define IRIN_GNAME	"_G"


typedef struct luaL_Buffer luaL_Buffer;


/* extra error code for 'luaL_loadfilex' */
#define IRIN_ERRFILE     (IRIN_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define IRIN_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define IRIN_PRELOAD_TABLE	"_PRELOAD"


typedef struct luaL_Reg {
  const char *name;
  irin_CFunction func;
} luaL_Reg;


#define LUAL_NUMSIZES	(sizeof(irin_Integer)*16 + sizeof(irin_Number))

LUALIB_API void (luaL_checkversion_) (irin_State *L, irin_Number ver, size_t sz);
#define luaL_checkversion(L)  \
	  luaL_checkversion_(L, IRIN_VERSION_NUM, LUAL_NUMSIZES)

LUALIB_API int (luaL_getmetafield) (irin_State *L, int obj, const char *e);
LUALIB_API int (luaL_callmeta) (irin_State *L, int obj, const char *e);
LUALIB_API const char *(luaL_tolstring) (irin_State *L, int idx, size_t *len);
LUALIB_API int (luaL_argerror) (irin_State *L, int arg, const char *extramsg);
LUALIB_API int (luaL_typeerror) (irin_State *L, int arg, const char *tname);
LUALIB_API const char *(luaL_checklstring) (irin_State *L, int arg,
                                                          size_t *l);
LUALIB_API const char *(luaL_optlstring) (irin_State *L, int arg,
                                          const char *def, size_t *l);
LUALIB_API irin_Number (luaL_checknumber) (irin_State *L, int arg);
LUALIB_API irin_Number (luaL_optnumber) (irin_State *L, int arg, irin_Number def);

LUALIB_API irin_Integer (luaL_checkinteger) (irin_State *L, int arg);
LUALIB_API irin_Integer (luaL_optinteger) (irin_State *L, int arg,
                                          irin_Integer def);

LUALIB_API void (luaL_checkstack) (irin_State *L, int sz, const char *msg);
LUALIB_API void (luaL_checktype) (irin_State *L, int arg, int t);
LUALIB_API void (luaL_checkany) (irin_State *L, int arg);

LUALIB_API int   (luaL_newmetatable) (irin_State *L, const char *tname);
LUALIB_API void  (luaL_setmetatable) (irin_State *L, const char *tname);
LUALIB_API void *(luaL_testudata) (irin_State *L, int ud, const char *tname);
LUALIB_API void *(luaL_checkudata) (irin_State *L, int ud, const char *tname);

LUALIB_API void (luaL_where) (irin_State *L, int lvl);
LUALIB_API int (luaL_error) (irin_State *L, const char *fmt, ...);

LUALIB_API int (luaL_checkoption) (irin_State *L, int arg, const char *def,
                                   const char *const lst[]);

LUALIB_API int (luaL_fileresult) (irin_State *L, int stat, const char *fname);
LUALIB_API int (luaL_execresult) (irin_State *L, int stat);


/* predefined references */
#define IRIN_NOREF       (-2)
#define IRIN_REFNIL      (-1)

LUALIB_API int (luaL_ref) (irin_State *L, int t);
LUALIB_API void (luaL_unref) (irin_State *L, int t, int ref);

LUALIB_API int (luaL_loadfilex) (irin_State *L, const char *filename,
                                               const char *mode);

#define luaL_loadfile(L,f)	luaL_loadfilex(L,f,NULL)

LUALIB_API int (luaL_loadbufferx) (irin_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
LUALIB_API int (luaL_loadstring) (irin_State *L, const char *s);

LUALIB_API irin_State *(luaL_newstate) (void);

LUALIB_API unsigned luaL_makeseed (irin_State *L);

LUALIB_API irin_Integer (luaL_len) (irin_State *L, int idx);

LUALIB_API void (luaL_addgsub) (luaL_Buffer *b, const char *s,
                                     const char *p, const char *r);
LUALIB_API const char *(luaL_gsub) (irin_State *L, const char *s,
                                    const char *p, const char *r);

LUALIB_API void (luaL_setfuncs) (irin_State *L, const luaL_Reg *l, int nup);

LUALIB_API int (luaL_getsubtable) (irin_State *L, int idx, const char *fname);

LUALIB_API void (luaL_traceback) (irin_State *L, irin_State *L1,
                                  const char *msg, int level);

LUALIB_API void (luaL_requiref) (irin_State *L, const char *modname,
                                 irin_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define luaL_newlibtable(L,l)	\
  irin_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)  \
  (luaL_checkversion(L), luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#define luaL_argcheck(L, cond,arg,extramsg)	\
	((void)(luai_likely(cond) || luaL_argerror(L, (arg), (extramsg))))

#define luaL_argexpected(L,cond,arg,tname)	\
	((void)(luai_likely(cond) || luaL_typeerror(L, (arg), (tname))))

#define luaL_checkstring(L,n)	(luaL_checklstring(L, (n), NULL))
#define luaL_optstring(L,n,d)	(luaL_optlstring(L, (n), (d), NULL))

#define luaL_typename(L,i)	irin_typename(L, irin_type(L,(i)))

#define luaL_dofile(L, fn) \
	(luaL_loadfile(L, fn) || irin_pcall(L, 0, IRIN_MULTRET, 0))

#define luaL_dostring(L, s) \
	(luaL_loadstring(L, s) || irin_pcall(L, 0, IRIN_MULTRET, 0))

#define luaL_getmetatable(L,n)	(irin_getfield(L, IRIN_REGISTRYINDEX, (n)))

#define luaL_opt(L,f,n,d)	(irin_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define luaL_loadbuffer(L,s,sz,n)	luaL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on irin_Integer values with wrap-around
** semantics, as the Irin core does.
*/
#define luaL_intop(op,v1,v2)  \
	((irin_Integer)((irin_Unsigned)(v1) op (irin_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define luaL_pushfail(L)	irin_pushnil(L)



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct luaL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  irin_State *L;
  union {
    LUAI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[LUAL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define luaL_bufflen(bf)	((bf)->n)
#define luaL_buffaddr(bf)	((bf)->b)


#define luaL_addchar(B,c) \
  ((void)((B)->n < (B)->size || luaL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define luaL_addsize(B,s)	((B)->n += (s))

#define luaL_buffsub(B,s)	((B)->n -= (s))

LUALIB_API void (luaL_buffinit) (irin_State *L, luaL_Buffer *B);
LUALIB_API char *(luaL_prepbuffsize) (luaL_Buffer *B, size_t sz);
LUALIB_API void (luaL_addlstring) (luaL_Buffer *B, const char *s, size_t l);
LUALIB_API void (luaL_addstring) (luaL_Buffer *B, const char *s);
LUALIB_API void (luaL_addvalue) (luaL_Buffer *B);
LUALIB_API void (luaL_pushresult) (luaL_Buffer *B);
LUALIB_API void (luaL_pushresultsize) (luaL_Buffer *B, size_t sz);
LUALIB_API char *(luaL_buffinitsize) (irin_State *L, luaL_Buffer *B, size_t sz);

#define luaL_prepbuffer(B)	luaL_prepbuffsize(B, LUAL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'IRIN_FILEHANDLE' and
** initial structure 'luaL_Stream' (it may contain other fields
** after that initial structure).
*/

#define IRIN_FILEHANDLE          "FILE*"


typedef struct luaL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  irin_CFunction closef;  /* to close stream (NULL for closed streams) */
} luaL_Stream;

/* }====================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(IRIN_COMPAT_APIINTCASTS)

#define luaL_checkunsigned(L,a)	((irin_Unsigned)luaL_checkinteger(L,a))
#define luaL_optunsigned(L,a,d)	\
	((irin_Unsigned)luaL_optinteger(L,a,(irin_Integer)(d)))

#define luaL_checkint(L,n)	((int)luaL_checkinteger(L, (n)))
#define luaL_optint(L,n,d)	((int)luaL_optinteger(L, (n), (d)))

#define luaL_checklong(L,n)	((long)luaL_checkinteger(L, (n)))
#define luaL_optlong(L,n,d)	((long)luaL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


