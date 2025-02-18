/*
** $Id: lmathlib.c $
** Standard mathematical library
** See Copyright Notice in ilya.h
*/

#define lmathlib_c
#define ILYA_LIB

#include "lprefix.h"


#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "ilya.h"

#include "lauxlib.h"
#include "ilyalib.h"
#include "llimits.h"


#undef PI
#define PI	(l_mathop(3.141592653589793238462643383279502884))


static int math_abs (ilya_State *L) {
  if (ilya_isinteger(L, 1)) {
    ilya_Integer n = ilya_tointeger(L, 1);
    if (n < 0) n = (ilya_Integer)(0u - (ilya_Unsigned)n);
    ilya_pushinteger(L, n);
  }
  else
    ilya_pushnumber(L, l_mathop(fabs)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_sin (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(sin)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_cos (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(cos)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_tan (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(tan)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_asin (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(asin)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_acos (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(acos)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_atan (ilya_State *L) {
  ilya_Number y = ilyaL_checknumber(L, 1);
  ilya_Number x = ilyaL_optnumber(L, 2, 1);
  ilya_pushnumber(L, l_mathop(atan2)(y, x));
  return 1;
}


static int math_toint (ilya_State *L) {
  int valid;
  ilya_Integer n = ilya_tointegerx(L, 1, &valid);
  if (l_likely(valid))
    ilya_pushinteger(L, n);
  else {
    ilyaL_checkany(L, 1);
    ilyaL_pushfail(L);  /* value is not convertible to integer */
  }
  return 1;
}


static void pushnumint (ilya_State *L, ilya_Number d) {
  ilya_Integer n;
  if (ilya_numbertointeger(d, &n))  /* does 'd' fit in an integer? */
    ilya_pushinteger(L, n);  /* result is integer */
  else
    ilya_pushnumber(L, d);  /* result is float */
}


static int math_floor (ilya_State *L) {
  if (ilya_isinteger(L, 1))
    ilya_settop(L, 1);  /* integer is its own floor */
  else {
    ilya_Number d = l_mathop(floor)(ilyaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_ceil (ilya_State *L) {
  if (ilya_isinteger(L, 1))
    ilya_settop(L, 1);  /* integer is its own ceiling */
  else {
    ilya_Number d = l_mathop(ceil)(ilyaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_fmod (ilya_State *L) {
  if (ilya_isinteger(L, 1) && ilya_isinteger(L, 2)) {
    ilya_Integer d = ilya_tointeger(L, 2);
    if ((ilya_Unsigned)d + 1u <= 1u) {  /* special cases: -1 or 0 */
      ilyaL_argcheck(L, d != 0, 2, "zero");
      ilya_pushinteger(L, 0);  /* avoid overflow with 0x80000... / -1 */
    }
    else
      ilya_pushinteger(L, ilya_tointeger(L, 1) % d);
  }
  else
    ilya_pushnumber(L, l_mathop(fmod)(ilyaL_checknumber(L, 1),
                                     ilyaL_checknumber(L, 2)));
  return 1;
}


/*
** next fn does not use 'modf', avoiding problems with 'double*'
** (which is not compatible with 'float*') when ilya_Number is not
** 'double'.
*/
static int math_modf (ilya_State *L) {
  if (ilya_isinteger(L ,1)) {
    ilya_settop(L, 1);  /* number is its own integer part */
    ilya_pushnumber(L, 0);  /* no fractional part */
  }
  else {
    ilya_Number n = ilyaL_checknumber(L, 1);
    /* integer part (rounds toward zero) */
    ilya_Number ip = (n < 0) ? l_mathop(ceil)(n) : l_mathop(floor)(n);
    pushnumint(L, ip);
    /* fractional part (test needed for inf/-inf) */
    ilya_pushnumber(L, (n == ip) ? l_mathop(0.0) : (n - ip));
  }
  return 2;
}


static int math_sqrt (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(sqrt)(ilyaL_checknumber(L, 1)));
  return 1;
}


static int math_ult (ilya_State *L) {
  ilya_Integer a = ilyaL_checkinteger(L, 1);
  ilya_Integer b = ilyaL_checkinteger(L, 2);
  ilya_pushboolean(L, (ilya_Unsigned)a < (ilya_Unsigned)b);
  return 1;
}

static int math_log (ilya_State *L) {
  ilya_Number x = ilyaL_checknumber(L, 1);
  ilya_Number res;
  if (ilya_isnoneornil(L, 2))
    res = l_mathop(log)(x);
  else {
    ilya_Number base = ilyaL_checknumber(L, 2);
#if !defined(ILYA_USE_C89)
    if (base == l_mathop(2.0))
      res = l_mathop(log2)(x);
    else
#endif
    if (base == l_mathop(10.0))
      res = l_mathop(log10)(x);
    else
      res = l_mathop(log)(x)/l_mathop(log)(base);
  }
  ilya_pushnumber(L, res);
  return 1;
}

static int math_exp (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(exp)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_deg (ilya_State *L) {
  ilya_pushnumber(L, ilyaL_checknumber(L, 1) * (l_mathop(180.0) / PI));
  return 1;
}

static int math_rad (ilya_State *L) {
  ilya_pushnumber(L, ilyaL_checknumber(L, 1) * (PI / l_mathop(180.0)));
  return 1;
}


static int math_min (ilya_State *L) {
  int n = ilya_gettop(L);  /* number of arguments */
  int imin = 1;  /* index of current minimum value */
  int i;
  ilyaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (ilya_compare(L, i, imin, ILYA_OPLT))
      imin = i;
  }
  ilya_pushvalue(L, imin);
  return 1;
}


static int math_max (ilya_State *L) {
  int n = ilya_gettop(L);  /* number of arguments */
  int imax = 1;  /* index of current maximum value */
  int i;
  ilyaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (ilya_compare(L, imax, i, ILYA_OPLT))
      imax = i;
  }
  ilya_pushvalue(L, imax);
  return 1;
}


static int math_type (ilya_State *L) {
  if (ilya_type(L, 1) == ILYA_TNUMBER)
    ilya_pushstring(L, (ilya_isinteger(L, 1)) ? "integer" : "float");
  else {
    ilyaL_checkany(L, 1);
    ilyaL_pushfail(L);
  }
  return 1;
}



/*
** {==================================================================
** Pseudo-Random Number Generator based on 'xoshiro256**'.
** ===================================================================
*/

/*
** This code uses lots of shifts. ANSI C does not allow shifts greater
** than or equal to the width of the type being shifted, so some shifts
** are written in convoluted ways to match that restriction. For
** preprocessor tests, it assumes a width of 32 bits, so the maximum
** shift there is 31 bits.
*/


/* number of binary digits in the mantissa of a float */
#define FIGS	l_floatatt(MANT_DIG)

#if FIGS > 64
/* there are only 64 random bits; use them all */
#undef FIGS
#define FIGS	64
#endif


/*
** ILYA_RAND32 forces the use of 32-bit integers in the implementation
** of the PRN generator (mainly for testing).
*/
#if !defined(ILYA_RAND32) && !defined(Rand64)

/* try to find an integer type with at least 64 bits */

#if ((ULONG_MAX >> 31) >> 31) >= 3

/* 'long' has at least 64 bits */
#define Rand64		unsigned long
#define SRand64		long

#elif !defined(ILYA_USE_C89) && defined(LLONG_MAX)

/* there is a 'long long' type (which must have at least 64 bits) */
#define Rand64		unsigned long long
#define SRand64		long long

#elif ((ILYA_MAXUNSIGNED >> 31) >> 31) >= 3

/* 'ilya_Unsigned' has at least 64 bits */
#define Rand64		ilya_Unsigned
#define SRand64		ilya_Integer

#endif

#endif


#if defined(Rand64)  /* { */

/*
** Standard implementation, using 64-bit integers.
** If 'Rand64' has more than 64 bits, the extra bits do not interfere
** with the 64 initial bits, except in a right shift. Moreover, the
** final result has to discard the extra bits.
*/

/* avoid using extra bits when needed */
#define trim64(x)	((x) & 0xffffffffffffffffu)


/* rotate left 'x' by 'n' bits */
static Rand64 rotl (Rand64 x, int n) {
  return (x << n) | (trim64(x) >> (64 - n));
}

static Rand64 nextrand (Rand64 *state) {
  Rand64 state0 = state[0];
  Rand64 state1 = state[1];
  Rand64 state2 = state[2] ^ state0;
  Rand64 state3 = state[3] ^ state1;
  Rand64 res = rotl(state1 * 5, 7) * 9;
  state[0] = state0 ^ state3;
  state[1] = state1 ^ state2;
  state[2] = state2 ^ (state1 << 17);
  state[3] = rotl(state3, 45);
  return res;
}


/*
** Convert bits from a random integer into a float in the
** interval [0,1), getting the higher FIG bits from the
** random unsigned integer and converting that to a float.
** Some old Microsoft compilers cannot cast an unsigned long
** to a floating-point number, so we use a signed long as an
** intermediary. When ilya_Number is float or double, the shift ensures
** that 'sx' is non negative; in that case, a good compiler will remove
** the correction.
*/

/* must throw out the extra (64 - FIGS) bits */
#define shift64_FIG	(64 - FIGS)

/* 2^(-FIGS) == 2^-1 / 2^(FIGS-1) */
#define scaleFIG	(l_mathop(0.5) / ((Rand64)1 << (FIGS - 1)))

static ilya_Number I2d (Rand64 x) {
  SRand64 sx = (SRand64)(trim64(x) >> shift64_FIG);
  ilya_Number res = (ilya_Number)(sx) * scaleFIG;
  if (sx < 0)
    res += l_mathop(1.0);  /* correct the two's complement if negative */
  ilya_assert(0 <= res && res < 1);
  return res;
}

/* convert a 'Rand64' to a 'ilya_Unsigned' */
#define I2UInt(x)	((ilya_Unsigned)trim64(x))

/* convert a 'ilya_Unsigned' to a 'Rand64' */
#define Int2I(x)	((Rand64)(x))


#else	/* no 'Rand64'   }{ */

/*
** Use two 32-bit integers to represent a 64-bit quantity.
*/
typedef struct Rand64 {
  l_uint32 h;  /* higher half */
  l_uint32 l;  /* lower half */
} Rand64;


/*
** If 'l_uint32' has more than 32 bits, the extra bits do not interfere
** with the 32 initial bits, except in a right shift and comparisons.
** Moreover, the final result has to discard the extra bits.
*/

/* avoid using extra bits when needed */
#define trim32(x)	((x) & 0xffffffffu)


/*
** basic operations on 'Rand64' values
*/

/* build a new Rand64 value */
static Rand64 packI (l_uint32 h, l_uint32 l) {
  Rand64 result;
  result.h = h;
  result.l = l;
  return result;
}

/* return i << n */
static Rand64 Ishl (Rand64 i, int n) {
  ilya_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)), i.l << n);
}

/* i1 ^= i2 */
static void Ixor (Rand64 *i1, Rand64 i2) {
  i1->h ^= i2.h;
  i1->l ^= i2.l;
}

/* return i1 + i2 */
static Rand64 Iadd (Rand64 i1, Rand64 i2) {
  Rand64 result = packI(i1.h + i2.h, i1.l + i2.l);
  if (trim32(result.l) < trim32(i1.l))  /* carry? */
    result.h++;
  return result;
}

/* return i * 5 */
static Rand64 times5 (Rand64 i) {
  return Iadd(Ishl(i, 2), i);  /* i * 5 == (i << 2) + i */
}

/* return i * 9 */
static Rand64 times9 (Rand64 i) {
  return Iadd(Ishl(i, 3), i);  /* i * 9 == (i << 3) + i */
}

/* return 'i' rotated left 'n' bits */
static Rand64 rotl (Rand64 i, int n) {
  ilya_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)),
               (trim32(i.h) >> (32 - n)) | (i.l << n));
}

/* for offsets larger than 32, rotate right by 64 - offset */
static Rand64 rotl1 (Rand64 i, int n) {
  ilya_assert(n > 32 && n < 64);
  n = 64 - n;
  return packI((trim32(i.h) >> n) | (i.l << (32 - n)),
               (i.h << (32 - n)) | (trim32(i.l) >> n));
}

/*
** implementation of 'xoshiro256**' algorithm on 'Rand64' values
*/
static Rand64 nextrand (Rand64 *state) {
  Rand64 res = times9(rotl(times5(state[1]), 7));
  Rand64 t = Ishl(state[1], 17);
  Ixor(&state[2], state[0]);
  Ixor(&state[3], state[1]);
  Ixor(&state[1], state[2]);
  Ixor(&state[0], state[3]);
  Ixor(&state[2], t);
  state[3] = rotl1(state[3], 45);
  return res;
}


/*
** Converts a 'Rand64' into a float.
*/

/* an unsigned 1 with proper type */
#define UONE		((l_uint32)1)


#if FIGS <= 32

/* 2^(-FIGS) */
#define scaleFIG       (l_mathop(0.5) / (UONE << (FIGS - 1)))

/*
** get up to 32 bits from higher half, shifting right to
** throw out the extra bits.
*/
static ilya_Number I2d (Rand64 x) {
  ilya_Number h = (ilya_Number)(trim32(x.h) >> (32 - FIGS));
  return h * scaleFIG;
}

#else	/* 32 < FIGS <= 64 */

/* 2^(-FIGS) = 1.0 / 2^30 / 2^3 / 2^(FIGS-33) */
#define scaleFIG  \
    (l_mathop(1.0) / (UONE << 30) / l_mathop(8.0) / (UONE << (FIGS - 33)))

/*
** use FIGS - 32 bits from lower half, throwing out the other
** (32 - (FIGS - 32)) = (64 - FIGS) bits
*/
#define shiftLOW	(64 - FIGS)

/*
** higher 32 bits go after those (FIGS - 32) bits: shiftHI = 2^(FIGS - 32)
*/
#define shiftHI		((ilya_Number)(UONE << (FIGS - 33)) * l_mathop(2.0))


static ilya_Number I2d (Rand64 x) {
  ilya_Number h = (ilya_Number)trim32(x.h) * shiftHI;
  ilya_Number l = (ilya_Number)(trim32(x.l) >> shiftLOW);
  return (h + l) * scaleFIG;
}

#endif


/* convert a 'Rand64' to a 'ilya_Unsigned' */
static ilya_Unsigned I2UInt (Rand64 x) {
  return (((ilya_Unsigned)trim32(x.h) << 31) << 1) | (ilya_Unsigned)trim32(x.l);
}

/* convert a 'ilya_Unsigned' to a 'Rand64' */
static Rand64 Int2I (ilya_Unsigned n) {
  return packI((l_uint32)((n >> 31) >> 1), (l_uint32)n);
}

#endif  /* } */


/*
** A state uses four 'Rand64' values.
*/
typedef struct {
  Rand64 s[4];
} RanState;


/*
** Project the random integer 'ran' into the interval [0, n].
** Because 'ran' has 2^B possible values, the projection can only be
** uniform when the size of the interval is a power of 2 (exact
** division). Otherwise, to get a uniform projection into [0, n], we
** first compute 'lim', the smallest Mersenne number not smaller than
** 'n'. We then project 'ran' into the interval [0, lim].  If the result
** is inside [0, n], we are done. Otherwise, we try with another 'ran',
** until we have a result inside the interval.
*/
static ilya_Unsigned project (ilya_Unsigned ran, ilya_Unsigned n,
                             RanState *state) {
  if ((n & (n + 1)) == 0)  /* is 'n + 1' a power of 2? */
    return ran & n;  /* no bias */
  else {
    ilya_Unsigned lim = n;
    /* compute the smallest (2^b - 1) not smaller than 'n' */
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);
#if (ILYA_MAXUNSIGNED >> 31) >= 3
    lim |= (lim >> 32);  /* integer type has more than 32 bits */
#endif
    ilya_assert((lim & (lim + 1)) == 0  /* 'lim + 1' is a power of 2, */
      && lim >= n  /* not smaller than 'n', */
      && (lim >> 1) < n);  /* and it is the smallest one */
    while ((ran &= lim) > n)  /* project 'ran' into [0..lim] */
      ran = I2UInt(nextrand(state->s));  /* not inside [0..n]? try again */
    return ran;
  }
}


static int math_random (ilya_State *L) {
  ilya_Integer low, up;
  ilya_Unsigned p;
  RanState *state = (RanState *)ilya_touserdata(L, ilya_upvalueindex(1));
  Rand64 rv = nextrand(state->s);  /* next pseudo-random value */
  switch (ilya_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      ilya_pushnumber(L, I2d(rv));  /* float between 0 and 1 */
      return 1;
    }
    case 1: {  /* only upper limit */
      low = 1;
      up = ilyaL_checkinteger(L, 1);
      if (up == 0) {  /* single 0 as argument? */
        ilya_pushinteger(L, l_castU2S(I2UInt(rv)));  /* full random integer */
        return 1;
      }
      break;
    }
    case 2: {  /* lower and upper limits */
      low = ilyaL_checkinteger(L, 1);
      up = ilyaL_checkinteger(L, 2);
      break;
    }
    default: return ilyaL_error(L, "wrong number of arguments");
  }
  /* random integer in the interval [low, up] */
  ilyaL_argcheck(L, low <= up, 1, "interval is empty");
  /* project random integer into the interval [0, up - low] */
  p = project(I2UInt(rv), (ilya_Unsigned)up - (ilya_Unsigned)low, state);
  ilya_pushinteger(L, l_castU2S(p) + low);
  return 1;
}


static void setseed (ilya_State *L, Rand64 *state,
                     ilya_Unsigned n1, ilya_Unsigned n2) {
  int i;
  state[0] = Int2I(n1);
  state[1] = Int2I(0xff);  /* avoid a zero state */
  state[2] = Int2I(n2);
  state[3] = Int2I(0);
  for (i = 0; i < 16; i++)
    nextrand(state);  /* discard initial values to "spread" seed */
  ilya_pushinteger(L, l_castU2S(n1));
  ilya_pushinteger(L, l_castU2S(n2));
}


static int math_randomseed (ilya_State *L) {
  RanState *state = (RanState *)ilya_touserdata(L, ilya_upvalueindex(1));
  ilya_Unsigned n1, n2;
  if (ilya_isnone(L, 1)) {
    n1 = ilyaL_makeseed(L);  /* "random" seed */
    n2 = I2UInt(nextrand(state->s));  /* in case seed is not that random... */
  }
  else {
    n1 = l_castS2U(ilyaL_checkinteger(L, 1));
    n2 = l_castS2U(ilyaL_optinteger(L, 2, 0));
  }
  setseed(L, state->s, n1, n2);
  return 2;  /* return seeds */
}


static const ilyaL_Reg randfuncs[] = {
  {"random", math_random},
  {"randomseed", math_randomseed},
  {NULL, NULL}
};


/*
** Register the random functions and initialize their state.
*/
static void setrandfunc (ilya_State *L) {
  RanState *state = (RanState *)ilya_newuserdatauv(L, sizeof(RanState), 0);
  setseed(L, state->s, ilyaL_makeseed(L), 0);  /* initialize with random seed */
  ilya_pop(L, 2);  /* remove pushed seeds */
  ilyaL_setfuncs(L, randfuncs, 1);
}

/* }================================================================== */


/*
** {==================================================================
** Deprecated functions (for compatibility only)
** ===================================================================
*/
#if defined(ILYA_COMPAT_MATHLIB)

static int math_cosh (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(cosh)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_sinh (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(sinh)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_tanh (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(tanh)(ilyaL_checknumber(L, 1)));
  return 1;
}

static int math_pow (ilya_State *L) {
  ilya_Number x = ilyaL_checknumber(L, 1);
  ilya_Number y = ilyaL_checknumber(L, 2);
  ilya_pushnumber(L, l_mathop(pow)(x, y));
  return 1;
}

static int math_frexp (ilya_State *L) {
  int e;
  ilya_pushnumber(L, l_mathop(frexp)(ilyaL_checknumber(L, 1), &e));
  ilya_pushinteger(L, e);
  return 2;
}

static int math_ldexp (ilya_State *L) {
  ilya_Number x = ilyaL_checknumber(L, 1);
  int ep = (int)ilyaL_checkinteger(L, 2);
  ilya_pushnumber(L, l_mathop(ldexp)(x, ep));
  return 1;
}

static int math_log10 (ilya_State *L) {
  ilya_pushnumber(L, l_mathop(log10)(ilyaL_checknumber(L, 1)));
  return 1;
}

#endif
/* }================================================================== */



static const ilyaL_Reg mathlib[] = {
  {"abs",   math_abs},
  {"acos",  math_acos},
  {"asin",  math_asin},
  {"atan",  math_atan},
  {"ceil",  math_ceil},
  {"cos",   math_cos},
  {"deg",   math_deg},
  {"exp",   math_exp},
  {"tointeger", math_toint},
  {"floor", math_floor},
  {"fmod",   math_fmod},
  {"ult",   math_ult},
  {"log",   math_log},
  {"max",   math_max},
  {"min",   math_min},
  {"modf",   math_modf},
  {"rad",   math_rad},
  {"sin",   math_sin},
  {"sqrt",  math_sqrt},
  {"tan",   math_tan},
  {"type", math_type},
#if defined(ILYA_COMPAT_MATHLIB)
  {"atan2", math_atan},
  {"cosh",   math_cosh},
  {"sinh",   math_sinh},
  {"tanh",   math_tanh},
  {"pow",   math_pow},
  {"frexp", math_frexp},
  {"ldexp", math_ldexp},
  {"log10", math_log10},
#endif
  /* placeholders */
  {"random", NULL},
  {"randomseed", NULL},
  {"pi", NULL},
  {"huge", NULL},
  {"maxinteger", NULL},
  {"mininteger", NULL},
  {NULL, NULL}
};


/*
** Open math library
*/
ILYAMOD_API int ilyaopen_math (ilya_State *L) {
  ilyaL_newlib(L, mathlib);
  ilya_pushnumber(L, PI);
  ilya_setfield(L, -2, "pi");
  ilya_pushnumber(L, (ilya_Number)HUGE_VAL);
  ilya_setfield(L, -2, "huge");
  ilya_pushinteger(L, ILYA_MAXINTEGER);
  ilya_setfield(L, -2, "maxinteger");
  ilya_pushinteger(L, ILYA_MININTEGER);
  ilya_setfield(L, -2, "mininteger");
  setrandfunc(L);
  return 1;
}

