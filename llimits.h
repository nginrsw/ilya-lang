/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in ilya.h
*/

#ifndef llimits_h
#define llimits_h


#include <limits.h>
#include <stddef.h>


#include "ilya.h"


/*
** 'l_mem' is a signed integer big enough to count the total memory
** used by Ilya.  (It is signed due to the use of debt in several
** computations.)  Usually, 'ptrdiff_t' should work, but we use 'long'
** for 16-bit machines.
*/
#if defined(ILYAI_MEM)		/* { external definitions? */
typedef ILYAI_MEM l_mem;
typedef ILYAI_UMEM lu_mem;
#elif ILYAI_IS32INT	/* }{ */
typedef ptrdiff_t l_mem;
typedef size_t lu_mem;
#else  /* 16-bit ints */	/* }{ */
typedef long l_mem;
typedef unsigned long lu_mem;
#endif				/* } */

#define MAX_LMEM  \
	cast(l_mem, (cast(lu_mem, 1) << (sizeof(l_mem) * 8 - 1)) - 1)


/* chars used as small naturals (so that 'char' is reserved for characters) */
typedef unsigned char lu_byte;
typedef signed char ls_byte;


/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))

/*
** Maximum size for strings and userdata visible for Ilya; should be
** representable as a ilya_Integer and as a size_t.
*/
#define MAX_SIZE	(sizeof(size_t) < sizeof(ilya_Integer) ? MAX_SIZET \
			  : cast_sizet(ILYA_MAXINTEGER))

/*
** floor of the log2 of the maximum signed value for integral type 't'.
** (That is, maximum 'n' such that '2^n' fits in the given signed type.)
*/
#define log2maxs(t)	cast_int(sizeof(t) * 8 - 2)


/*
** test whether an unsigned value is a power of 2 (or zero)
*/
#define ispow2(x)	(((x) & ((x) - 1)) == 0)


/* number of chars of a literal string without the ending \0 */
#define LL(x)   (sizeof(x)/sizeof(char) - 1)


/*
** conversion of pointer to unsigned integer: this is for hashing only;
** there is no problem if the integer cannot hold the whole pointer
** value. (In strict ISO C this may cause undefined behavior, but no
** actual machine seems to bother.)
*/
#if !defined(ILYA_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(UINTPTR_MAX)  /* even in C99 this type is optional */
#define L_P2I	uintptr_t
#else  /* no 'intptr'? */
#define L_P2I	uintmax_t  /* use the largest available integer */
#endif
#else  /* C89 option */
#define L_P2I	size_t
#endif

#define point2uint(p)	cast_uint((L_P2I)(p) & UINT_MAX)



/* types of 'usual argument conversions' for ilya_Number and ilya_Integer */
typedef ILYAI_UACNUMBER l_uacNumber;
typedef ILYAI_UACINT l_uacInt;


/*
** Internal assertions for in-house debugging
*/
#if defined ILYAI_ASSERT
#undef NDEBUG
#include <assert.h>
#define ilya_assert(c)           assert(c)
#define assert_code(c)		c
#endif

#if defined(ilya_assert)
#else
#define ilya_assert(c)		((void)0)
#define assert_code(c)		((void)0)
#endif

#define check_exp(c,e)		(ilya_assert(c), (e))
/* to avoid problems with conditions too long */
#define ilya_longassert(c)	assert_code((c) ? (void)0 : ilya_assert(0))


/* macro to avoid warnings about unused variables */
#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))
#endif


/* type casts (a macro highlights casts in the code) */
#define cast(t, exp)	((t)(exp))

#define cast_void(i)	cast(void, (i))
#define cast_voidp(i)	cast(void *, (i))
#define cast_num(i)	cast(ilya_Number, (i))
#define cast_int(i)	cast(int, (i))
#define cast_uint(i)	cast(unsigned int, (i))
#define cast_ulong(i)	cast(unsigned long, (i))
#define cast_byte(i)	cast(lu_byte, (i))
#define cast_uchar(i)	cast(unsigned char, (i))
#define cast_char(i)	cast(char, (i))
#define cast_charp(i)	cast(char *, (i))
#define cast_sizet(i)	cast(size_t, (i))


/* cast a signed ilya_Integer to ilya_Unsigned */
#if !defined(l_castS2U)
#define l_castS2U(i)	((ilya_Unsigned)(i))
#endif

/*
** cast a ilya_Unsigned to a signed ilya_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/
#if !defined(l_castU2S)
#define l_castU2S(i)	((ilya_Integer)(i))
#endif

/*
** cast a size_t to ilya_Integer: These casts are always valid for
** sizes of Ilya objects (see MAX_SIZE)
*/
#define cast_st2S(sz)	((ilya_Integer)(sz))

/* Cast a ptrdiff_t to size_t, when it is known that the minuend
** comes from the subtrahend (the base)
*/
#define ct_diff2sz(df)	((size_t)(df))

/* ptrdiff_t to ilya_Integer */
#define ct_diff2S(df)	cast_st2S(ct_diff2sz(df))

/*
** Special type equivalent to '(void*)' for functions (to suppress some
** warnings when converting fn pointers)
*/
typedef void (*voidf)(void);

/*
** Macro to convert pointer-to-void* to pointer-to-fn. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__ (voidf)(p))
#else
#define cast_func(p) ((voidf)(p))
#endif



/*
** non-return type
*/
#if !defined(l_noret)

#if defined(__GNUC__)
#define l_noret		void __attribute__((noreturn))
#elif defined(_MSC_VER) && _MSC_VER >= 1200
#define l_noret		void __declspec(noreturn)
#else
#define l_noret		void
#endif

#endif


/*
** Inline functions
*/
#if !defined(ILYA_USE_C89)
#define l_inline	inline
#elif defined(__GNUC__)
#define l_inline	__inline__
#else
#define l_inline	/* empty */
#endif

#define l_sinline	static l_inline


/*
** An unsigned with (at least) 4 bytes
*/
#if ILYAI_IS32INT
typedef unsigned int l_uint32;
#else
typedef unsigned long l_uint32;
#endif


/*
** The ilyai_num* macros define the primitive operations over numbers.
*/

/* floor division (defined as 'floor(a/b)') */
#if !defined(ilyai_numidiv)
#define ilyai_numidiv(L,a,b)     ((void)L, l_floor(ilyai_numdiv(L,a,b)))
#endif

/* float division */
#if !defined(ilyai_numdiv)
#define ilyai_numdiv(L,a,b)      ((a)/(b))
#endif

/*
** modulo: defined as 'a - floor(a/b)*b'; the direct computation
** using this definition has several problems with rounding errors,
** so it is better to use 'fmod'. 'fmod' gives the result of
** 'a - trunc(a/b)*b', and therefore must be corrected when
** 'trunc(a/b) ~= floor(a/b)'. That happens when the division has a
** non-integer negative result: non-integer result is equivalent to
** a non-zero remainder 'm'; negative result is equivalent to 'a' and
** 'b' with different signs, or 'm' and 'b' with different signs
** (as the result 'm' of 'fmod' has the same sign of 'a').
*/
#if !defined(ilyai_nummod)
#define ilyai_nummod(L,a,b,m)  \
  { (void)L; (m) = l_mathop(fmod)(a,b); \
    if (((m) > 0) ? (b) < 0 : ((m) < 0 && (b) > 0)) (m) += (b); }
#endif

/* exponentiation */
#if !defined(ilyai_numpow)
#define ilyai_numpow(L,a,b)  \
  ((void)L, (b == 2) ? (a)*(a) : l_mathop(pow)(a,b))
#endif

/* the others are quite standard operations */
#if !defined(ilyai_numadd)
#define ilyai_numadd(L,a,b)      ((a)+(b))
#define ilyai_numsub(L,a,b)      ((a)-(b))
#define ilyai_nummul(L,a,b)      ((a)*(b))
#define ilyai_numunm(L,a)        (-(a))
#define ilyai_numeq(a,b)         ((a)==(b))
#define ilyai_numlt(a,b)         ((a)<(b))
#define ilyai_numle(a,b)         ((a)<=(b))
#define ilyai_numgt(a,b)         ((a)>(b))
#define ilyai_numge(a,b)         ((a)>=(b))
#define ilyai_numisnan(a)        (!ilyai_numeq((a), (a)))
#endif


/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(ilya_writestring)
#define ilya_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(ilya_writeline)
#define ilya_writeline()        (ilya_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(ilya_writestringerror)
#define ilya_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */

#endif

