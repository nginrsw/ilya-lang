/*
** $Id: lauxlib.c $
** Auxiliary functions for building Ilya libraries
** See Copyright Notice in ilya.h
*/

#define lauxlib_c
#define ILYA_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Ilya.
** Any fn declared here could be written as an application fn.
*/

#include "ilya.h"

#include "lauxlib.h"
#include "llimits.h"


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (ilya_State *L, int objidx, int level) {
  if (level == 0 || !ilya_istable(L, -1))
    return 0;  /* not found */
  ilya_pushnil(L);  /* start 'next' loop */
  while (ilya_next(L, -2)) {  /* for each pair in table */
    if (ilya_type(L, -2) == ILYA_TSTRING) {  /* ignore non-string keys */
      if (ilya_rawequal(L, objidx, -1)) {  /* found object? */
        ilya_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        ilya_pushliteral(L, ".");  /* place '.' between the two names */
        ilya_replace(L, -3);  /* (in the slot occupied by table) */
        ilya_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    ilya_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a fn in all loaded modules
*/
static int pushglobalfuncname (ilya_State *L, ilya_Debug *ar) {
  int top = ilya_gettop(L);
  ilya_getinfo(L, "f", ar);  /* push fn */
  ilya_getfield(L, ILYA_REGISTRYINDEX, ILYA_LOADED_TABLE);
  ilyaL_checkstack(L, 6, "not enough stack");  /* slots for 'findfield' */
  if (findfield(L, top + 1, 2)) {
    const char *name = ilya_tostring(L, -1);
    if (strncmp(name, ILYA_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      ilya_pushstring(L, name + 3);  /* push name without prefix */
      ilya_remove(L, -2);  /* remove original name */
    }
    ilya_copy(L, -1, top + 1);  /* copy name to proper place */
    ilya_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    ilya_settop(L, top);  /* remove fn and global table */
    return 0;
  }
}


static void pushfuncname (ilya_State *L, ilya_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    ilya_pushfstring(L, "fn '%s'", ilya_tostring(L, -1));
    ilya_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    ilya_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      ilya_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Ilya functions, use <file:line> */
    ilya_pushfstring(L, "fn <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    ilya_pushliteral(L, "?");
}


static int lastlevel (ilya_State *L) {
  ilya_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (ilya_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (ilya_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


ILYALIB_API void ilyaL_traceback (ilya_State *L, ilya_State *L1,
                                const char *msg, int level) {
  ilyaL_Buffer b;
  ilya_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  ilyaL_buffinit(L, &b);
  if (msg) {
    ilyaL_addstring(&b, msg);
    ilyaL_addchar(&b, '\n');
  }
  ilyaL_addstring(&b, "stack traceback:");
  while (ilya_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      ilya_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      ilyaL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      ilya_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        ilya_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        ilya_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      ilyaL_addvalue(&b);
      pushfuncname(L, &ar);
      ilyaL_addvalue(&b);
      if (ar.istailcall)
        ilyaL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  ilyaL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

ILYALIB_API int ilyaL_argerror (ilya_State *L, int arg, const char *extramsg) {
  ilya_Debug ar;
  const char *argword;
  if (!ilya_getstack(L, 0, &ar))  /* no stack frame? */
    return ilyaL_error(L, "bad argument #%d (%s)", arg, extramsg);
  ilya_getinfo(L, "nt", &ar);
  if (arg <= ar.extraargs)  /* error in an extra argument? */
    argword =  "extra argument";
  else {
    arg -= ar.extraargs;  /* do not count extra arguments */
    if (strcmp(ar.namewhat, "method") == 0) {  /* colon syntax? */
      arg--;  /* do not count (extra) self argument */
      if (arg == 0)  /* error in self argument? */
        return ilyaL_error(L, "calling '%s' on bad self (%s)",
                               ar.name, extramsg);
      /* else go through; error in a regular argument */
    }
    argword = "argument";
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? ilya_tostring(L, -1) : "?";
  return ilyaL_error(L, "bad %s #%d to '%s' (%s)",
                       argword, arg, ar.name, extramsg);
}


ILYALIB_API int ilyaL_typeerror (ilya_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (ilyaL_getmetafield(L, arg, "__name") == ILYA_TSTRING)
    typearg = ilya_tostring(L, -1);  /* use the given type name */
  else if (ilya_type(L, arg) == ILYA_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = ilyaL_typename(L, arg);  /* standard name */
  msg = ilya_pushfstring(L, "%s expected, got %s", tname, typearg);
  return ilyaL_argerror(L, arg, msg);
}


static void tag_error (ilya_State *L, int arg, int tag) {
  ilyaL_typeerror(L, arg, ilya_typename(L, tag));
}


/*
** The use of 'ilya_pushfstring' ensures this fn does not
** need reserved stack space when called.
*/
ILYALIB_API void ilyaL_where (ilya_State *L, int level) {
  ilya_Debug ar;
  if (ilya_getstack(L, level, &ar)) {  /* check fn at level */
    ilya_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      ilya_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  ilya_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'ilya_pushvfstring' ensures this fn does
** not need reserved stack space when called. (At worst, it generates
** a memory error instead of the given message.)
*/
ILYALIB_API int ilyaL_error (ilya_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  ilyaL_where(L, 1);
  ilya_pushvfstring(L, fmt, argp);
  va_end(argp);
  ilya_concat(L, 2);
  return ilya_error(L);
}


ILYALIB_API int ilyaL_fileresult (ilya_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Ilya API may change this value */
  if (stat) {
    ilya_pushboolean(L, 1);
    return 1;
  }
  else {
    const char *msg;
    ilyaL_pushfail(L);
    msg = (en != 0) ? strerror(en) : "(no extra info)";
    if (fname)
      ilya_pushfstring(L, "%s: %s", fname, msg);
    else
      ilya_pushstring(L, msg);
    ilya_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(ILYA_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


ILYALIB_API int ilyaL_execresult (ilya_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return ilyaL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      ilya_pushboolean(L, 1);
    else
      ilyaL_pushfail(L);
    ilya_pushstring(L, what);
    ilya_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

ILYALIB_API int ilyaL_newmetatable (ilya_State *L, const char *tname) {
  if (ilyaL_getmetatable(L, tname) != ILYA_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  ilya_pop(L, 1);
  ilya_createtable(L, 0, 2);  /* create metatable */
  ilya_pushstring(L, tname);
  ilya_setfield(L, -2, "__name");  /* metatable.__name = tname */
  ilya_pushvalue(L, -1);
  ilya_setfield(L, ILYA_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


ILYALIB_API void ilyaL_setmetatable (ilya_State *L, const char *tname) {
  ilyaL_getmetatable(L, tname);
  ilya_setmetatable(L, -2);
}


ILYALIB_API void *ilyaL_testudata (ilya_State *L, int ud, const char *tname) {
  void *p = ilya_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (ilya_getmetatable(L, ud)) {  /* does it have a metatable? */
      ilyaL_getmetatable(L, tname);  /* get correct metatable */
      if (!ilya_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      ilya_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


ILYALIB_API void *ilyaL_checkudata (ilya_State *L, int ud, const char *tname) {
  void *p = ilyaL_testudata(L, ud, tname);
  ilyaL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

ILYALIB_API int ilyaL_checkoption (ilya_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? ilyaL_optstring(L, arg, def) :
                             ilyaL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return ilyaL_argerror(L, arg,
                       ilya_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Ilya will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
ILYALIB_API void ilyaL_checkstack (ilya_State *L, int space, const char *msg) {
  if (l_unlikely(!ilya_checkstack(L, space))) {
    if (msg)
      ilyaL_error(L, "stack overflow (%s)", msg);
    else
      ilyaL_error(L, "stack overflow");
  }
}


ILYALIB_API void ilyaL_checktype (ilya_State *L, int arg, int t) {
  if (l_unlikely(ilya_type(L, arg) != t))
    tag_error(L, arg, t);
}


ILYALIB_API void ilyaL_checkany (ilya_State *L, int arg) {
  if (l_unlikely(ilya_type(L, arg) == ILYA_TNONE))
    ilyaL_argerror(L, arg, "value expected");
}


ILYALIB_API const char *ilyaL_checklstring (ilya_State *L, int arg, size_t *len) {
  const char *s = ilya_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, ILYA_TSTRING);
  return s;
}


ILYALIB_API const char *ilyaL_optlstring (ilya_State *L, int arg,
                                        const char *def, size_t *len) {
  if (ilya_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return ilyaL_checklstring(L, arg, len);
}


ILYALIB_API ilya_Number ilyaL_checknumber (ilya_State *L, int arg) {
  int isnum;
  ilya_Number d = ilya_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, ILYA_TNUMBER);
  return d;
}


ILYALIB_API ilya_Number ilyaL_optnumber (ilya_State *L, int arg, ilya_Number def) {
  return ilyaL_opt(L, ilyaL_checknumber, arg, def);
}


static void interror (ilya_State *L, int arg) {
  if (ilya_isnumber(L, arg))
    ilyaL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, ILYA_TNUMBER);
}


ILYALIB_API ilya_Integer ilyaL_checkinteger (ilya_State *L, int arg) {
  int isnum;
  ilya_Integer d = ilya_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


ILYALIB_API ilya_Integer ilyaL_optinteger (ilya_State *L, int arg,
                                                      ilya_Integer def) {
  return ilyaL_opt(L, ilyaL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


/* Resize the buffer used by a box. Optimize for the common case of
** resizing to the old size. (For instance, __gc will resize the box
** to 0 even after it was closed. 'pushresult' may also resize it to a
** final size that is equal to the one set when the buffer was created.)
*/
static void *resizebox (ilya_State *L, int idx, size_t newsize) {
  UBox *box = (UBox *)ilya_touserdata(L, idx);
  if (box->bsize == newsize)  /* not changing size? */
    return box->box;  /* keep the buffer */
  else {
    void *ud;
    ilya_Alloc allocf = ilya_getallocf(L, &ud);
    void *temp = allocf(ud, box->box, box->bsize, newsize);
    if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
      ilya_pushliteral(L, "not enough memory");
      ilya_error(L);  /* raise a memory error */
    }
    box->box = temp;
    box->bsize = newsize;
    return temp;
  }
}


static int boxgc (ilya_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const ilyaL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (ilya_State *L) {
  UBox *box = (UBox *)ilya_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (ilyaL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    ilyaL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  ilya_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  ilya_assert(buffonstack(B) ? ilya_touserdata(B->L, idx) != NULL  \
                            : ilya_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes plus one for a terminating zero. (The test for "not big enough"
** also gets the case when the computation of 'newsize' overflows.)
*/
static size_t newbuffsize (ilyaL_Buffer *B, size_t sz) {
  size_t newsize = (B->size / 2) * 3;  /* buffer size * 1.5 */
  if (l_unlikely(sz > MAX_SIZE - B->n - 1))
    return cast_sizet(ilyaL_error(B->L, "resulting string too large"));
  if (newsize < B->n + sz + 1 || newsize > MAX_SIZE) {
    /* newsize was not big enough or too big */
    newsize = B->n + sz + 1;
  }
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (ilyaL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    ilya_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      ilya_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      ilya_insert(L, boxidx);  /* move box to its intended position */
      ilya_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
ILYALIB_API char *ilyaL_prepbuffsize (ilyaL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


ILYALIB_API void ilyaL_addlstring (ilyaL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    ilyaL_addsize(B, l);
  }
}


ILYALIB_API void ilyaL_addstring (ilyaL_Buffer *B, const char *s) {
  ilyaL_addlstring(B, s, strlen(s));
}


ILYALIB_API void ilyaL_pushresult (ilyaL_Buffer *B) {
  ilya_State *L = B->L;
  checkbufferlevel(B, -1);
  if (!buffonstack(B))  /* using static buffer? */
    ilya_pushlstring(L, B->b, B->n);  /* save result as regular string */
  else {  /* reuse buffer already allocated */
    UBox *box = (UBox *)ilya_touserdata(L, -1);
    void *ud;
    ilya_Alloc allocf = ilya_getallocf(L, &ud);  /* fn to free buffer */
    size_t len = B->n;  /* final string length */
    char *s;
    resizebox(L, -1, len + 1);  /* adjust box size to content size */
    s = (char*)box->box;  /* final buffer address */
    s[len] = '\0';  /* add ending zero */
    /* clear box, as Ilya will take control of the buffer */
    box->bsize = 0;  box->box = NULL;
    ilya_pushexternalstring(L, s, len, allocf, ud);
    ilya_closeslot(L, -2);  /* close the box */
    ilya_gc(L, ILYA_GCSTEP, len);
  }
  ilya_remove(L, -2);  /* remove box or placeholder from the stack */
}


ILYALIB_API void ilyaL_pushresultsize (ilyaL_Buffer *B, size_t sz) {
  ilyaL_addsize(B, sz);
  ilyaL_pushresult(B);
}


/*
** 'ilyaL_addvalue' is the only fn in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'ilyaL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
ILYALIB_API void ilyaL_addvalue (ilyaL_Buffer *B) {
  ilya_State *L = B->L;
  size_t len;
  const char *s = ilya_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  ilyaL_addsize(B, len);
  ilya_pop(L, 1);  /* pop string */
}


ILYALIB_API void ilyaL_buffinit (ilya_State *L, ilyaL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = ILYAL_BUFFERSIZE;
  ilya_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


ILYALIB_API char *ilyaL_buffinitsize (ilya_State *L, ilyaL_Buffer *B, size_t sz) {
  ilyaL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/*
** The previously freed references form a linked list: t[1] is the index
** of a first free index, t[t[1]] is the index of the second element,
** etc. A zero signals the end of the list.
*/
ILYALIB_API int ilyaL_ref (ilya_State *L, int t) {
  int ref;
  if (ilya_isnil(L, -1)) {
    ilya_pop(L, 1);  /* remove from stack */
    return ILYA_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = ilya_absindex(L, t);
  if (ilya_rawgeti(L, t, 1) == ILYA_TNUMBER)  /* already initialized? */
    ref = (int)ilya_tointeger(L, -1);  /* ref = t[1] */
  else {  /* first access */
    ilya_assert(!ilya_toboolean(L, -1));  /* must be nil or false */
    ref = 0;  /* list is empty */
    ilya_pushinteger(L, 0);  /* initialize as an empty list */
    ilya_rawseti(L, t, 1);  /* ref = t[1] = 0 */
  }
  ilya_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    ilya_rawgeti(L, t, ref);  /* remove it from list */
    ilya_rawseti(L, t, 1);  /* (t[1] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)ilya_rawlen(L, t) + 1;  /* get a new reference */
  ilya_rawseti(L, t, ref);
  return ref;
}


ILYALIB_API void ilyaL_unref (ilya_State *L, int t, int ref) {
  if (ref >= 0) {
    t = ilya_absindex(L, t);
    ilya_rawgeti(L, t, 1);
    ilya_assert(ilya_isinteger(L, -1));
    ilya_rawseti(L, t, ref);  /* t[ref] = t[1] */
    ilya_pushinteger(L, ref);
    ilya_rawseti(L, t, 1);  /* t[1] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  unsigned n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (ilya_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  (void)L;  /* not used */
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (ilya_State *L, const char *what, int fnameindex) {
  int err = errno;
  const char *filename = ilya_tostring(L, fnameindex) + 1;
  if (err != 0)
    ilya_pushfstring(L, "cannot %s %s: %s", what, filename, strerror(err));
  else
    ilya_pushfstring(L, "cannot %s %s", what, filename);
  ilya_remove(L, fnameindex);
  return ILYA_ERRFILE;
}


/*
** Skip an optional BOM at the start of a stream. If there is an
** incomplete BOM (the first character is correct but the rest is
** not), returns the first character anyway to force an error
** (as no chunk can start with 0xEF).
*/
static int skipBOM (FILE *f) {
  int c = getc(f);  /* read first character */
  if (c == 0xEF && getc(f) == 0xBB && getc(f) == 0xBF)  /* correct BOM? */
    return getc(f);  /* ignore BOM and return next char */
  else  /* no (valid) BOM */
    return c;  /* return first character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (FILE *f, int *cp) {
  int c = *cp = skipBOM(f);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(f);
    } while (c != EOF && c != '\n');
    *cp = getc(f);  /* next character after comment, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


ILYALIB_API int ilyaL_loadfilex (ilya_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = ilya_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    ilya_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    ilya_pushfstring(L, "@%s", filename);
    errno = 0;
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  lf.n = 0;
  if (skipcomment(lf.f, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
  if (c == ILYA_SIGNATURE[0]) {  /* binary file? */
    lf.n = 0;  /* remove possible newline */
    if (filename) {  /* "real" file? */
      errno = 0;
      lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
      if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
      skipcomment(lf.f, &c);  /* re-read initial portion */
    }
  }
  if (c != EOF)
    lf.buff[lf.n++] = cast_char(c);  /* 'c' is the first character */
  status = ilya_load(L, getF, &lf, ilya_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  errno = 0;  /* no useful error number until here */
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    ilya_settop(L, fnameindex);  /* ignore results from 'ilya_load' */
    return errfile(L, "read", fnameindex);
  }
  ilya_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (ilya_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


ILYALIB_API int ilyaL_loadbufferx (ilya_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return ilya_load(L, getS, &ls, name, mode);
}


ILYALIB_API int ilyaL_loadstring (ilya_State *L, const char *s) {
  return ilyaL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



ILYALIB_API int ilyaL_getmetafield (ilya_State *L, int obj, const char *event) {
  if (!ilya_getmetatable(L, obj))  /* no metatable? */
    return ILYA_TNIL;
  else {
    int tt;
    ilya_pushstring(L, event);
    tt = ilya_rawget(L, -2);
    if (tt == ILYA_TNIL)  /* is metafield nil? */
      ilya_pop(L, 2);  /* remove metatable and metafield */
    else
      ilya_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


ILYALIB_API int ilyaL_callmeta (ilya_State *L, int obj, const char *event) {
  obj = ilya_absindex(L, obj);
  if (ilyaL_getmetafield(L, obj, event) == ILYA_TNIL)  /* no metafield? */
    return 0;
  ilya_pushvalue(L, obj);
  ilya_call(L, 1, 1);
  return 1;
}


ILYALIB_API ilya_Integer ilyaL_len (ilya_State *L, int idx) {
  ilya_Integer l;
  int isnum;
  ilya_len(L, idx);
  l = ilya_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    ilyaL_error(L, "object length is not an integer");
  ilya_pop(L, 1);  /* remove object */
  return l;
}


ILYALIB_API const char *ilyaL_tolstring (ilya_State *L, int idx, size_t *len) {
  idx = ilya_absindex(L,idx);
  if (ilyaL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!ilya_isstring(L, -1))
      ilyaL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (ilya_type(L, idx)) {
      case ILYA_TNUMBER: {
        char buff[ILYA_N2SBUFFSZ];
        ilya_numbertocstring(L, idx, buff);
        ilya_pushstring(L, buff);
        break;
      }
      case ILYA_TSTRING:
        ilya_pushvalue(L, idx);
        break;
      case ILYA_TBOOLEAN:
        ilya_pushstring(L, (ilya_toboolean(L, idx) ? "true" : "false"));
        break;
      case ILYA_TNIL:
        ilya_pushliteral(L, "nil");
        break;
      default: {
        int tt = ilyaL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == ILYA_TSTRING) ? ilya_tostring(L, -1) :
                                                 ilyaL_typename(L, idx);
        ilya_pushfstring(L, "%s: %p", kind, ilya_topointer(L, idx));
        if (tt != ILYA_TNIL)
          ilya_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return ilya_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** fn gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
ILYALIB_API void ilyaL_setfuncs (ilya_State *L, const ilyaL_Reg *l, int nup) {
  ilyaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* placeholder? */
      ilya_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        ilya_pushvalue(L, -nup);
      ilya_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    ilya_setfield(L, -(nup + 2), l->name);
  }
  ilya_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
ILYALIB_API int ilyaL_getsubtable (ilya_State *L, int idx, const char *fname) {
  if (ilya_getfield(L, idx, fname) == ILYA_TTABLE)
    return 1;  /* table already there */
  else {
    ilya_pop(L, 1);  /* remove previous result */
    idx = ilya_absindex(L, idx);
    ilya_newtable(L);
    ilya_pushvalue(L, -1);  /* copy to be left at top */
    ilya_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
ILYALIB_API void ilyaL_requiref (ilya_State *L, const char *modname,
                               ilya_CFunction openf, int glb) {
  ilyaL_getsubtable(L, ILYA_REGISTRYINDEX, ILYA_LOADED_TABLE);
  ilya_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!ilya_toboolean(L, -1)) {  /* package not already loaded? */
    ilya_pop(L, 1);  /* remove field */
    ilya_pushcfunction(L, openf);
    ilya_pushstring(L, modname);  /* argument to open fn */
    ilya_call(L, 1, 1);  /* call 'openf' to open module */
    ilya_pushvalue(L, -1);  /* make copy of module (call result) */
    ilya_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  ilya_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    ilya_pushvalue(L, -1);  /* copy of module */
    ilya_setglobal(L, modname);  /* _G[modname] = module */
  }
}


ILYALIB_API void ilyaL_addgsub (ilyaL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    ilyaL_addlstring(b, s, ct_diff2sz(wild - s));  /* push prefix */
    ilyaL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  ilyaL_addstring(b, s);  /* push last suffix */
}


ILYALIB_API const char *ilyaL_gsub (ilya_State *L, const char *s,
                                  const char *p, const char *r) {
  ilyaL_Buffer b;
  ilyaL_buffinit(L, &b);
  ilyaL_addgsub(&b, s, p, r);
  ilyaL_pushresult(&b);
  return ilya_tostring(L, -1);
}


static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


/*
** Standard panic fn just prints an error message. The test
** with 'ilya_type' avoids possible memory errors in 'ilya_tostring'.
*/
static int panic (ilya_State *L) {
  const char *msg = (ilya_type(L, -1) == ILYA_TSTRING)
                  ? ilya_tostring(L, -1)
                  : "error object is not a string";
  ilya_writestringerror("PANIC: unprotected error in call to Ilya API (%s)\n",
                        msg);
  return 0;  /* return to Ilya to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (ilya_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      ilya_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      ilya_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((ilya_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn fn.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  ilya_State *L = (ilya_State *)ud;
  ilya_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    ilya_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    ilya_writestringerror("%s", "\n");  /* finish message with end-of-line */
    ilya_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((ilya_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  ilya_writestringerror("%s", "Ilya warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}



/*
** A fn to compute an unsigned int with some level of
** randomness. Rely on Address Space Layout Randomization (if present)
** and the current time.
*/
#if !defined(ilyai_makeseed)

#include <time.h>


/* Size for the buffer, in bytes */
#define BUFSEEDB	(sizeof(void*) + sizeof(time_t))

/* Size for the buffer in int's, rounded up */
#define BUFSEED		((BUFSEEDB + sizeof(int) - 1) / sizeof(int))

/*
** Copy the contents of variable 'v' into the buffer pointed by 'b'.
** (The '&b[0]' disguises 'b' to fix an absurd warning from clang.)
*/
#define addbuff(b,v)	(memcpy(&b[0], &(v), sizeof(v)), b += sizeof(v))


static unsigned int ilyai_makeseed (void) {
  unsigned int buff[BUFSEED];
  unsigned int res;
  unsigned int i;
  time_t t = time(NULL);
  char *b = (char*)buff;
  addbuff(b, b);  /* lock variable's address */
  addbuff(b, t);  /* time */
  /* fill (rare but possible) remain of the buffer with zeros */
  memset(b, 0, sizeof(buff) - BUFSEEDB);
  res = buff[0];
  for (i = 1; i < BUFSEED; i++)
    res ^= (res >> 3) + (res << 7) + buff[i];
  return res;
}

#endif


ILYALIB_API unsigned int ilyaL_makeseed (ilya_State *L) {
  (void)L;  /* unused */
  return ilyai_makeseed();
}


ILYALIB_API ilya_State *ilyaL_newstate (void) {
  ilya_State *L = ilya_newstate(l_alloc, NULL, ilyai_makeseed());
  if (l_likely(L)) {
    ilya_atpanic(L, &panic);
    ilya_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


ILYALIB_API void ilyaL_checkversion_ (ilya_State *L, ilya_Number ver, size_t sz) {
  ilya_Number v = ilya_version(L);
  if (sz != ILYAL_NUMSIZES)  /* check numeric types */
    ilyaL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    ilyaL_error(L, "version mismatch: app. needs %f, Ilya core provides %f",
                  (ILYAI_UACNUMBER)ver, (ILYAI_UACNUMBER)v);
}

