/*
** $Id: irinconf.h $
** Configuration file for Irin
** See Copyright Notice in irin.h
*/


#ifndef luaconf_h
#define luaconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Irin
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Irin ABI (by making the changes here, you ensure that all software
** connected to Irin, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Irin to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ IRIN_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Irin to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define IRIN_USE_C89 */


/*
** By default, Irin on Windows use (some) specific Windows features
*/
#if !defined(IRIN_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define IRIN_USE_WINDOWS  /* enable goodies for regular Windows */
#endif


#if defined(IRIN_USE_WINDOWS)
#define IRIN_DL_DLL	/* enable support for DLL */
#define IRIN_USE_C89	/* broadly, Windows is C89 */
#endif


/*
** When Posix DLL ('IRIN_USE_DLOPEN') is enabled, the Irin stand-alone
** application will try to dynamically link a 'readline' facility
** for its REPL.  In that case, IRIN_READLINELIB is the name of the
** library it will look for those facilities.  If irin.c cannot open
** the specified library, it will generate a warning and then run
** without 'readline'.  If that macro is not defined, irin.c will not
** use 'readline'.
*/
#if defined(IRIN_USE_LINUX)
#define IRIN_USE_POSIX
#define IRIN_USE_DLOPEN		/* needs an extra library: -ldl */
#define IRIN_READLINELIB		"libreadline.so"
#endif


#if defined(IRIN_USE_MACOSX)
#define IRIN_USE_POSIX
#define IRIN_USE_DLOPEN		/* MacOS does not need -ldl */
#define IRIN_READLINELIB		"libedit.dylib"
#endif


#if defined(IRIN_USE_IOS)
#define IRIN_USE_POSIX
#define IRIN_USE_DLOPEN
#endif


#if defined(IRIN_USE_C89) && defined(IRIN_USE_POSIX)
#error "Posix is not compatible with C89"
#endif


/*
@@ LUAI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define LUAI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Irin must
** use the same configuration.
** ===================================================================
*/

/*
@@ IRIN_INT_TYPE defines the type for Irin integers.
@@ IRIN_FLOAT_TYPE defines the type for Irin floats.
** Irin should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for IRIN_INT_TYPE */
#define IRIN_INT_INT		1
#define IRIN_INT_LONG		2
#define IRIN_INT_LONGLONG	3

/* predefined options for IRIN_FLOAT_TYPE */
#define IRIN_FLOAT_FLOAT		1
#define IRIN_FLOAT_DOUBLE	2
#define IRIN_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Irin) */
#define IRIN_INT_DEFAULT		IRIN_INT_LONGLONG
#define IRIN_FLOAT_DEFAULT	IRIN_FLOAT_DOUBLE


/*
@@ IRIN_32BITS enables Irin with 32-bit integers and 32-bit floats.
*/
#define IRIN_32BITS	0


/*
@@ IRIN_C89_NUMBERS ensures that Irin uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(IRIN_USE_C89) && !defined(IRIN_USE_WINDOWS)
#define IRIN_C89_NUMBERS		1
#else
#define IRIN_C89_NUMBERS		0
#endif


#if IRIN_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if LUAI_IS32INT  /* use 'int' if big enough */
#define IRIN_INT_TYPE	IRIN_INT_INT
#else  /* otherwise use 'long' */
#define IRIN_INT_TYPE	IRIN_INT_LONG
#endif
#define IRIN_FLOAT_TYPE	IRIN_FLOAT_FLOAT

#elif IRIN_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define IRIN_INT_TYPE	IRIN_INT_LONG
#define IRIN_FLOAT_TYPE	IRIN_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define IRIN_INT_TYPE	IRIN_INT_DEFAULT
#define IRIN_FLOAT_TYPE	IRIN_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** IRIN_PATH_SEP is the character that separates templates in a path.
** IRIN_PATH_MARK is the string that marks the substitution points in a
** template.
** IRIN_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define IRIN_PATH_SEP            ";"
#define IRIN_PATH_MARK           "?"
#define IRIN_EXEC_DIR            "!"


/*
@@ IRIN_PATH_DEFAULT is the default path that Irin uses to look for
** Irin libraries.
@@ IRIN_CPATH_DEFAULT is the default path that Irin uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define IRIN_VDIR	IRIN_VERSION_MAJOR "." IRIN_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define IRIN_LDIR	"!\\irin\\"
#define IRIN_CDIR	"!\\"
#define IRIN_SHRDIR	"!\\..\\share\\irin\\" IRIN_VDIR "\\"

#if !defined(IRIN_PATH_DEFAULT)
#define IRIN_PATH_DEFAULT  \
		IRIN_LDIR"?.irin;"  IRIN_LDIR"?\\init.irin;" \
		IRIN_CDIR"?.irin;"  IRIN_CDIR"?\\init.irin;" \
		IRIN_SHRDIR"?.irin;" IRIN_SHRDIR"?\\init.irin;" \
		".\\?.irin;" ".\\?\\init.irin"
#endif

#if !defined(IRIN_CPATH_DEFAULT)
#define IRIN_CPATH_DEFAULT \
		IRIN_CDIR"?.dll;" \
		IRIN_CDIR"..\\lib\\irin\\" IRIN_VDIR "\\?.dll;" \
		IRIN_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define IRIN_ROOT	"/usr/locked/"
#define IRIN_LDIR	IRIN_ROOT "share/irin/" IRIN_VDIR "/"
#define IRIN_CDIR	IRIN_ROOT "lib/irin/" IRIN_VDIR "/"

#if !defined(IRIN_PATH_DEFAULT)
#define IRIN_PATH_DEFAULT  \
		IRIN_LDIR"?.irin;"  IRIN_LDIR"?/init.irin;" \
		IRIN_CDIR"?.irin;"  IRIN_CDIR"?/init.irin;" \
		"./?.irin;" "./?/init.irin"
#endif

#if !defined(IRIN_CPATH_DEFAULT)
#define IRIN_CPATH_DEFAULT \
		IRIN_CDIR"?.so;" IRIN_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ IRIN_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Irin automatically uses "\".)
*/
#if !defined(IRIN_DIRSEP)

#if defined(_WIN32)
#define IRIN_DIRSEP	"\\"
#else
#define IRIN_DIRSEP	"/"
#endif

#endif


/*
** IRIN_IGMARK is a mark to ignore all after it when building the
** module name (e.g., used to build the luaopen_ fn name).
** Typically, the suffix after the mark is the module version,
** as in "mod-v1.2.so".
*/
#define IRIN_IGMARK		"-"

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ IRIN_API is a mark for all core API functions.
@@ LUALIB_API is a mark for all auxiliary library functions.
@@ LUAMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** IRIN_BUILD_AS_DLL to get it).
*/
#if defined(IRIN_BUILD_AS_DLL)	/* { */

#if defined(IRIN_CORE) || defined(IRIN_LIB)	/* { */
#define IRIN_API __declspec(dllexport)
#else						/* }{ */
#define IRIN_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define IRIN_API		extern

#endif				/* } */


/*
** More often than not the libs go together with the core.
*/
#define LUALIB_API	IRIN_API
#define LUAMOD_API	IRIN_API


/*
@@ LUAI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ LUAI_DDEF and LUAI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (LUAI_DDEF for
** definitions and LUAI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Irin is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define LUAI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define LUAI_FUNC	extern
#endif				/* } */

#define LUAI_DDEC(dec)	LUAI_FUNC dec
#define LUAI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ IRIN_COMPAT_5_3 controls other macros for compatibility with Irin 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(IRIN_COMPAT_5_3)	/* { */

/*
@@ IRIN_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define IRIN_COMPAT_MATHLIB

/*
@@ IRIN_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (irin_pushunsigned, irin_tounsigned,
** luaL_checkint, luaL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define IRIN_COMPAT_APIINTCASTS


/*
@@ IRIN_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define IRIN_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define irin_strlen(L,i)		irin_rawlen(L, (i))

#define irin_objlen(L,i)		irin_rawlen(L, (i))

#define irin_equal(L,idx1,idx2)		irin_compare(L,(idx1),(idx2),IRIN_OPEQ)
#define irin_lessthan(L,idx1,idx2)	irin_compare(L,(idx1),(idx2),IRIN_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined IRIN_FLOAT_* / IRIN_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ LUAI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ IRIN_NUMBER_FRMLEN is the length modifier for writing floats.
@@ IRIN_NUMBER_FMT is the format for writing floats with the maximum
** number of digits that respects tostring(tonumber(numeral)) == numeral.
** (That would be floor(log10(2^n)), where n is the number of bits in
** the float mantissa.)
@@ IRIN_NUMBER_FMT_N is the format for writing floats with the minimum
** number of digits that ensures tonumber(tostring(number)) == number.
** (That would be IRIN_NUMBER_FMT+2.)
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ irin_str2number converts a decimal numeral to a number.
*/


/* The following definitions are good for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))


/*
@@ irin_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a irin_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define irin_numbertointeger(n,p) \
  ((n) >= (IRIN_NUMBER)(IRIN_MININTEGER) && \
   (n) < -(IRIN_NUMBER)(IRIN_MININTEGER) && \
      (*(p) = (IRIN_INTEGER)(n), 1))


/* now the variable definitions */

#if IRIN_FLOAT_TYPE == IRIN_FLOAT_FLOAT		/* { single float */

#define IRIN_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define LUAI_UACNUMBER	double

#define IRIN_NUMBER_FRMLEN	""
#define IRIN_NUMBER_FMT		"%.7g"
#define IRIN_NUMBER_FMT_N	"%.9g"

#define l_mathop(op)		op##f

#define irin_str2number(s,p)	strtof((s), (p))


#elif IRIN_FLOAT_TYPE == IRIN_FLOAT_LONGDOUBLE	/* }{ long double */

#define IRIN_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define LUAI_UACNUMBER	long double

#define IRIN_NUMBER_FRMLEN	"L"
#define IRIN_NUMBER_FMT		"%.19Lg"
#define IRIN_NUMBER_FMT_N	"%.21Lg"

#define l_mathop(op)		op##l

#define irin_str2number(s,p)	strtold((s), (p))

#elif IRIN_FLOAT_TYPE == IRIN_FLOAT_DOUBLE	/* }{ double */

#define IRIN_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define LUAI_UACNUMBER	double

#define IRIN_NUMBER_FRMLEN	""
#define IRIN_NUMBER_FMT		"%.15g"
#define IRIN_NUMBER_FMT_N	"%.17g"

#define l_mathop(op)		op

#define irin_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ IRIN_UNSIGNED is the unsigned version of IRIN_INTEGER.
@@ LUAI_UACINT is the result of a 'default argument promotion'
@@ over a IRIN_INTEGER.
@@ IRIN_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ IRIN_INTEGER_FMT is the format for writing integers.
@@ IRIN_MAXINTEGER is the maximum value for a IRIN_INTEGER.
@@ IRIN_MININTEGER is the minimum value for a IRIN_INTEGER.
@@ IRIN_MAXUNSIGNED is the maximum value for a IRIN_UNSIGNED.
@@ irin_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */

#define IRIN_INTEGER_FMT		"%" IRIN_INTEGER_FRMLEN "d"

#define LUAI_UACINT		IRIN_INTEGER

#define irin_integer2str(s,sz,n)  \
	l_sprintf((s), sz, IRIN_INTEGER_FMT, (LUAI_UACINT)(n))

/*
** use LUAI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define IRIN_UNSIGNED		unsigned LUAI_UACINT


/* now the variable definitions */

#if IRIN_INT_TYPE == IRIN_INT_INT		/* { int */

#define IRIN_INTEGER		int
#define IRIN_INTEGER_FRMLEN	""

#define IRIN_MAXINTEGER		INT_MAX
#define IRIN_MININTEGER		INT_MIN

#define IRIN_MAXUNSIGNED		UINT_MAX

#elif IRIN_INT_TYPE == IRIN_INT_LONG	/* }{ long */

#define IRIN_INTEGER		long
#define IRIN_INTEGER_FRMLEN	"l"

#define IRIN_MAXINTEGER		LONG_MAX
#define IRIN_MININTEGER		LONG_MIN

#define IRIN_MAXUNSIGNED		ULONG_MAX

#elif IRIN_INT_TYPE == IRIN_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define IRIN_INTEGER		long long
#define IRIN_INTEGER_FRMLEN	"ll"

#define IRIN_MAXINTEGER		LLONG_MAX
#define IRIN_MININTEGER		LLONG_MIN

#define IRIN_MAXUNSIGNED		ULLONG_MAX

#elif defined(IRIN_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define IRIN_INTEGER		__int64
#define IRIN_INTEGER_FRMLEN	"I64"

#define IRIN_MAXINTEGER		_I64_MAX
#define IRIN_MININTEGER		_I64_MIN

#define IRIN_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DLUA_32BITS' \
  or '-DLUA_C89_NUMBERS' (see file 'irinconf.h' for details)"

#endif				/* } */

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in Irin have only one format item.)
*/
#if !defined(IRIN_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ irin_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'irin_strx2number' undefined and Irin will provide its own
** implementation.
*/
#if !defined(IRIN_USE_C89)
#define irin_strx2number(s,p)		irin_str2number(s,p)
#endif


/*
@@ irin_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define irin_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ irin_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'irin_number2strx' undefined and Irin will
** provide its own implementation.
*/
#if !defined(IRIN_USE_C89)
#define irin_number2strx(L,b,sz,f,n)  \
	((void)L, l_sprintf(b,sz,f,(LUAI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(IRIN_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef irin_str2number
#define l_mathop(op)		(irin_Number)op  /* no variant */
#define irin_str2number(s,p)	((irin_Number)strtod((s), (p)))
#endif


/*
@@ IRIN_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Irin will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define IRIN_KCONTEXT	ptrdiff_t

#if !defined(IRIN_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef IRIN_KCONTEXT
#define IRIN_KCONTEXT	intptr_t
#endif
#endif


/*
@@ irin_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(irin_getlocaledecpoint)
#define irin_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Irin API use these macros.
** Define IRIN_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(luai_likely)

#if defined(__GNUC__) && !defined(IRIN_NOBUILTIN)
#define luai_likely(x)		(__builtin_expect(((x) != 0), 1))
#define luai_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define luai_likely(x)		(x)
#define luai_unlikely(x)	(x)
#endif

#endif


#if defined(IRIN_CORE) || defined(IRIN_LIB)
/* shorter names for Irin's own use */
#define l_likely(x)	luai_likely(x)
#define l_unlikely(x)	luai_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ IRIN_NOCVTN2S/IRIN_NOCVTS2N control how Irin performs some
** coercions. Define IRIN_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define IRIN_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define IRIN_NOCVTN2S */
/* #define IRIN_NOCVTS2N */


/*
@@ IRIN_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
/* #define IRIN_USE_APICHECK */

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Irin and when you compile code that links to
** Irin).
** =====================================================================
*/

/*
@@ LUAI_MAXSTACK limits the size of the Irin stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Irin from consuming unlimited stack
** space and to reserve some numbers for pseudo-indices.
** (It must fit into max(int)/2.)
*/
#if 1000000 < (INT_MAX / 2)
#define LUAI_MAXSTACK		1000000
#else
#define LUAI_MAXSTACK		(INT_MAX / 2u)
#endif


/*
@@ IRIN_EXTRASPACE defines the size of a raw memory area associated with
** a Irin state with very fast access.
** CHANGE it if you need a different size.
*/
#define IRIN_EXTRASPACE		(sizeof(void *))


/*
@@ IRIN_IDSIZE gives the maximum size for the description of the source
** of a fn in debug information.
** CHANGE it if you want a different size.
*/
#define IRIN_IDSIZE	60


/*
@@ LUAL_BUFFERSIZE is the initial buffer size used by the lauxlib
** buffer system.
*/
#define LUAL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(irin_Number)))


/*
@@ LUAI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define LUAI_MAXALIGN  irin_Number n; double u; void *s; irin_Integer i; long l

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

