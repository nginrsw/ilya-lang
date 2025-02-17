/*
** $Id: ltests.h $
** Internal Header for Debugging of the Ilya Implementation
** See Copyright Notice in ilya.h
*/

#ifndef ltests_h
#define ltests_h


#include <stdio.h>
#include <stdlib.h>

/* test Ilya with compatibility code */
#define ILYA_COMPAT_MATHLIB
#define ILYA_COMPAT_LT_LE


#define ILYA_DEBUG


/* turn on assertions */
#define LUAI_ASSERT


/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* test for sizes in 'l_sprintf' (make sure whole buffer is available) */
#undef l_sprintf
#if !defined(ILYA_USE_C89)
#define l_sprintf(s,sz,f,i)	(memset(s,0xAB,sz), snprintf(s,sz,f,i))
#else
#define l_sprintf(s,sz,f,i)	(memset(s,0xAB,sz), sprintf(s,f,i))
#endif


/* get a chance to test code without jump tables */
#define ILYA_USE_JUMPTABLE	0


/* use 32-bit integers in random generator */
#define ILYA_RAND32


/* memory-allocator control variables */
typedef struct Memcontrol {
  int failnext;
  unsigned long numblocks;
  unsigned long total;
  unsigned long maxmem;
  unsigned long memlimit;
  unsigned long countlimit;
  unsigned long objcount[ILYA_NUMTYPES];
} Memcontrol;

ILYA_API Memcontrol l_memcontrol;


#define luai_tracegc(L,f)		luai_tracegctest(L, f)
LUAI_FUNC void luai_tracegctest (ilya_State *L, int first);


/*
** generic variable for debug tricks
*/
extern void *l_Trick;


/*
** Function to traverse and check all memory used by Ilya
*/
LUAI_FUNC int ilya_checkmemory (ilya_State *L);

/*
** Function to print an object GC-friendly
*/
struct GCObject;
LUAI_FUNC void ilya_printobj (ilya_State *L, struct GCObject *o);


/*
** Function to print a value
*/
struct TValue;
LUAI_FUNC void ilya_printvalue (struct TValue *v);

/*
** Function to print the stack
*/
LUAI_FUNC void ilya_printstack (ilya_State *L);


/* test for lock/unlock */

struct L_EXTRA { int lock; int *plock; };
#undef ILYA_EXTRASPACE
#define ILYA_EXTRASPACE	sizeof(struct L_EXTRA)
#define getlock(l)	cast(struct L_EXTRA*, ilya_getextraspace(l))
#define luai_userstateopen(l)  \
	(getlock(l)->lock = 0, getlock(l)->plock = &(getlock(l)->lock))
#define luai_userstateclose(l)  \
  ilya_assert(getlock(l)->lock == 1 && getlock(l)->plock == &(getlock(l)->lock))
#define luai_userstatethread(l,l1) \
  ilya_assert(getlock(l1)->plock == getlock(l)->plock)
#define luai_userstatefree(l,l1) \
  ilya_assert(getlock(l)->plock == getlock(l1)->plock)
#define ilya_lock(l)     ilya_assert((*getlock(l)->plock)++ == 0)
#define ilya_unlock(l)   ilya_assert(--(*getlock(l)->plock) == 0)



ILYA_API int luaB_opentests (ilya_State *L);

ILYA_API void *debug_realloc (void *ud, void *block,
                             size_t osize, size_t nsize);

#if defined(ilya_c)
#define luaL_newstate()  \
	ilya_newstate(debug_realloc, &l_memcontrol, luaL_makeseed(NULL))
#define luai_openlibs(L)  \
  {  luaL_openlibs(L); \
     luaL_requiref(L, "T", luaB_opentests, 1); \
     ilya_pop(L, 1); }
#endif



/* change some sizes to give some bugs a chance */

#undef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE		23
#define MINSTRTABSIZE		2
#define MAXIWTHABS		3

#define STRCACHE_N	23
#define STRCACHE_M	5

#undef LUAI_USER_ALIGNMENT_T
#define LUAI_USER_ALIGNMENT_T   union { char b[sizeof(void*) * 8]; }


/*
** This one is not compatible with tests for opcode optimizations,
** as it blocks some optimizations
#define MAXINDEXRK	0
*/


/* make stack-overflow tests run faster */
#undef LUAI_MAXSTACK
#define LUAI_MAXSTACK   50000


/* test mode uses more stack space */
#undef LUAI_MAXCCALLS
#define LUAI_MAXCCALLS	180


/* force Ilya to use its own implementations */
#undef ilya_strx2number
#undef ilya_number2strx


#endif

