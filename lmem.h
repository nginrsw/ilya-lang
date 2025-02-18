/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in ilya.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "ilya.h"


#define ilyaM_error(L)	ilyaD_throw(L, ILYA_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define ilyaM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define ilyaM_checksize(L,n,e)  \
	(ilyaM_testsize(n,e) ? ilyaM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' and that 'int' is not larger than 'size_t'.)
*/
#define ilyaM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_int((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define ilyaM_reallocvchar(L,b,on,n)  \
  cast_charp(ilyaM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define ilyaM_freemem(L, b, s)	ilyaM_free_(L, (b), (s))
#define ilyaM_free(L, b)		ilyaM_free_(L, (b), sizeof(*(b)))
#define ilyaM_freearray(L, b, n)   ilyaM_free_(L, (b), (n)*sizeof(*(b)))

#define ilyaM_new(L,t)		cast(t*, ilyaM_malloc_(L, sizeof(t), 0))
#define ilyaM_newvector(L,n,t)	cast(t*, ilyaM_malloc_(L, (n)*sizeof(t), 0))
#define ilyaM_newvectorchecked(L,n,t) \
  (ilyaM_checksize(L,n,sizeof(t)), ilyaM_newvector(L,n,t))

#define ilyaM_newobject(L,tag,s)	ilyaM_malloc_(L, (s), tag)

#define ilyaM_newblock(L, size)	ilyaM_newvector(L, size, char)

#define ilyaM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, ilyaM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         ilyaM_limitN(limit,t),e)))

#define ilyaM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, ilyaM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define ilyaM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, ilyaM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

ILYAI_FUNC l_noret ilyaM_toobig (ilya_State *L);

/* not to be called directly */
ILYAI_FUNC void *ilyaM_realloc_ (ilya_State *L, void *block, size_t oldsize,
                                                          size_t size);
ILYAI_FUNC void *ilyaM_saferealloc_ (ilya_State *L, void *block, size_t oldsize,
                                                              size_t size);
ILYAI_FUNC void ilyaM_free_ (ilya_State *L, void *block, size_t osize);
ILYAI_FUNC void *ilyaM_growaux_ (ilya_State *L, void *block, int nelems,
                               int *size, unsigned size_elem, int limit,
                               const char *what);
ILYAI_FUNC void *ilyaM_shrinkvector_ (ilya_State *L, void *block, int *nelem,
                                    int final_n, unsigned size_elem);
ILYAI_FUNC void *ilyaM_malloc_ (ilya_State *L, size_t size, int tag);

#endif

