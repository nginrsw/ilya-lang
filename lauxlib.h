/*
** $Id: lauxlib.h $
** Auxiliary functions for building Ilya libraries
** See Copyright Notice in ilya.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "ilyaconf.h"
#include "ilya.h"


/* global table */
#define ILYA_GNAME	"_G"


typedef struct luaL_Buffer luaL_Buffer;


/* extra error code for 'luaL_loadfilex' */
#define ILYA_ERRFILE     (ILYA_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define ILYA_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define ILYA_PRELOAD_TABLE	"_PRELOAD"


typedef struct luaL_Reg {
  const char *name;
  ilya_CFunction func;
} luaL_Reg;


#define LUAL_NUMSIZES	(sizeof(ilya_Integer)*16 + sizeof(ilya_Number))

LUALIB_API void (luaL_checkversion_) (ilya_State *L, ilya_Number ver, size_t sz);
#define luaL_checkversion(L)  \
	  luaL_checkversion_(L, ILYA_VERSION_NUM, LUAL_NUMSIZES)

LUALIB_API int (luaL_getmetafield) (ilya_State *L, int obj, const char *e);
LUALIB_API int (luaL_callmeta) (ilya_State *L, int obj, const char *e);
LUALIB_API const char *(luaL_tolstring) (ilya_State *L, int idx, size_t *len);
LUALIB_API int (luaL_argerror) (ilya_State *L, int arg, const char *extramsg);
LUALIB_API int (luaL_typeerror) (ilya_State *L, int arg, const char *tname);
LUALIB_API const char *(luaL_checklstring) (ilya_State *L, int arg,
                                                          size_t *l);
LUALIB_API const char *(luaL_optlstring) (ilya_State *L, int arg,
                                          const char *def, size_t *l);
LUALIB_API ilya_Number (luaL_checknumber) (ilya_State *L, int arg);
LUALIB_API ilya_Number (luaL_optnumber) (ilya_State *L, int arg, ilya_Number def);

LUALIB_API ilya_Integer (luaL_checkinteger) (ilya_State *L, int arg);
LUALIB_API ilya_Integer (luaL_optinteger) (ilya_State *L, int arg,
                                          ilya_Integer def);

LUALIB_API void (luaL_checkstack) (ilya_State *L, int sz, const char *msg);
LUALIB_API void (luaL_checktype) (ilya_State *L, int arg, int t);
LUALIB_API void (luaL_checkany) (ilya_State *L, int arg);

LUALIB_API int   (luaL_newmetatable) (ilya_State *L, const char *tname);
LUALIB_API void  (luaL_setmetatable) (ilya_State *L, const char *tname);
LUALIB_API void *(luaL_testudata) (ilya_State *L, int ud, const char *tname);
LUALIB_API void *(luaL_checkudata) (ilya_State *L, int ud, const char *tname);

LUALIB_API void (luaL_where) (ilya_State *L, int lvl);
LUALIB_API int (luaL_error) (ilya_State *L, const char *fmt, ...);

LUALIB_API int (luaL_checkoption) (ilya_State *L, int arg, const char *def,
                                   const char *const lst[]);

LUALIB_API int (luaL_fileresult) (ilya_State *L, int stat, const char *fname);
LUALIB_API int (luaL_execresult) (ilya_State *L, int stat);


/* predefined references */
#define ILYA_NOREF       (-2)
#define ILYA_REFNIL      (-1)

LUALIB_API int (luaL_ref) (ilya_State *L, int t);
LUALIB_API void (luaL_unref) (ilya_State *L, int t, int ref);

LUALIB_API int (luaL_loadfilex) (ilya_State *L, const char *filename,
                                               const char *mode);

#define luaL_loadfile(L,f)	luaL_loadfilex(L,f,NULL)

LUALIB_API int (luaL_loadbufferx) (ilya_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
LUALIB_API int (luaL_loadstring) (ilya_State *L, const char *s);

LUALIB_API ilya_State *(luaL_newstate) (void);

LUALIB_API unsigned luaL_makeseed (ilya_State *L);

LUALIB_API ilya_Integer (luaL_len) (ilya_State *L, int idx);

LUALIB_API void (luaL_addgsub) (luaL_Buffer *b, const char *s,
                                     const char *p, const char *r);
LUALIB_API const char *(luaL_gsub) (ilya_State *L, const char *s,
                                    const char *p, const char *r);

LUALIB_API void (luaL_setfuncs) (ilya_State *L, const luaL_Reg *l, int nup);

LUALIB_API int (luaL_getsubtable) (ilya_State *L, int idx, const char *fname);

LUALIB_API void (luaL_traceback) (ilya_State *L, ilya_State *L1,
                                  const char *msg, int level);

LUALIB_API void (luaL_requiref) (ilya_State *L, const char *modname,
                                 ilya_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define luaL_newlibtable(L,l)	\
  ilya_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)  \
  (luaL_checkversion(L), luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#define luaL_argcheck(L, cond,arg,extramsg)	\
	((void)(luai_likely(cond) || luaL_argerror(L, (arg), (extramsg))))

#define luaL_argexpected(L,cond,arg,tname)	\
	((void)(luai_likely(cond) || luaL_typeerror(L, (arg), (tname))))

#define luaL_checkstring(L,n)	(luaL_checklstring(L, (n), NULL))
#define luaL_optstring(L,n,d)	(luaL_optlstring(L, (n), (d), NULL))

#define luaL_typename(L,i)	ilya_typename(L, ilya_type(L,(i)))

#define luaL_dofile(L, fn) \
	(luaL_loadfile(L, fn) || ilya_pcall(L, 0, ILYA_MULTRET, 0))

#define luaL_dostring(L, s) \
	(luaL_loadstring(L, s) || ilya_pcall(L, 0, ILYA_MULTRET, 0))

#define luaL_getmetatable(L,n)	(ilya_getfield(L, ILYA_REGISTRYINDEX, (n)))

#define luaL_opt(L,f,n,d)	(ilya_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define luaL_loadbuffer(L,s,sz,n)	luaL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on ilya_Integer values with wrap-around
** semantics, as the Ilya core does.
*/
#define luaL_intop(op,v1,v2)  \
	((ilya_Integer)((ilya_Unsigned)(v1) op (ilya_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define luaL_pushfail(L)	ilya_pushnil(L)



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct luaL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  ilya_State *L;
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

LUALIB_API void (luaL_buffinit) (ilya_State *L, luaL_Buffer *B);
LUALIB_API char *(luaL_prepbuffsize) (luaL_Buffer *B, size_t sz);
LUALIB_API void (luaL_addlstring) (luaL_Buffer *B, const char *s, size_t l);
LUALIB_API void (luaL_addstring) (luaL_Buffer *B, const char *s);
LUALIB_API void (luaL_addvalue) (luaL_Buffer *B);
LUALIB_API void (luaL_pushresult) (luaL_Buffer *B);
LUALIB_API void (luaL_pushresultsize) (luaL_Buffer *B, size_t sz);
LUALIB_API char *(luaL_buffinitsize) (ilya_State *L, luaL_Buffer *B, size_t sz);

#define luaL_prepbuffer(B)	luaL_prepbuffsize(B, LUAL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'ILYA_FILEHANDLE' and
** initial structure 'luaL_Stream' (it may contain other fields
** after that initial structure).
*/

#define ILYA_FILEHANDLE          "FILE*"


typedef struct luaL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  ilya_CFunction closef;  /* to close stream (NULL for closed streams) */
} luaL_Stream;

/* }====================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(ILYA_COMPAT_APIINTCASTS)

#define luaL_checkunsigned(L,a)	((ilya_Unsigned)luaL_checkinteger(L,a))
#define luaL_optunsigned(L,a,d)	\
	((ilya_Unsigned)luaL_optinteger(L,a,(ilya_Integer)(d)))

#define luaL_checkint(L,n)	((int)luaL_checkinteger(L, (n)))
#define luaL_optint(L,n,d)	((int)luaL_optinteger(L, (n), (d)))

#define luaL_checklong(L,n)	((long)luaL_checkinteger(L, (n)))
#define luaL_optlong(L,n,d)	((long)luaL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


