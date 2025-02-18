/*
** $Id: lcode.h $
** Code generator for Ilya
** See Copyright Notice in ilya.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)


#define ilyaK_codeABC(fs,o,a,b,c)	ilyaK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define ilyaK_setmultret(fs,e)	ilyaK_setreturns(fs, e, ILYA_MULTRET)

#define ilyaK_jumpto(fs,t)	ilyaK_patchlist(fs, ilyaK_jump(fs), t)

ILYAI_FUNC int ilyaK_code (FuncState *fs, Instruction i);
ILYAI_FUNC int ilyaK_codeABx (FuncState *fs, OpCode o, int A, int Bx);
ILYAI_FUNC int ilyaK_codeABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                            int k);
ILYAI_FUNC int ilyaK_codevABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                             int k);
ILYAI_FUNC int ilyaK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
ILYAI_FUNC void ilyaK_fixline (FuncState *fs, int line);
ILYAI_FUNC void ilyaK_nil (FuncState *fs, int from, int n);
ILYAI_FUNC void ilyaK_reserveregs (FuncState *fs, int n);
ILYAI_FUNC void ilyaK_checkstack (FuncState *fs, int n);
ILYAI_FUNC void ilyaK_int (FuncState *fs, int reg, ilya_Integer n);
ILYAI_FUNC void ilyaK_dischargevars (FuncState *fs, expdesc *e);
ILYAI_FUNC int ilyaK_exp2anyreg (FuncState *fs, expdesc *e);
ILYAI_FUNC void ilyaK_exp2anyregup (FuncState *fs, expdesc *e);
ILYAI_FUNC void ilyaK_exp2nextreg (FuncState *fs, expdesc *e);
ILYAI_FUNC void ilyaK_exp2val (FuncState *fs, expdesc *e);
ILYAI_FUNC void ilyaK_self (FuncState *fs, expdesc *e, expdesc *key);
ILYAI_FUNC void ilyaK_indexed (FuncState *fs, expdesc *t, expdesc *k);
ILYAI_FUNC void ilyaK_goiftrue (FuncState *fs, expdesc *e);
ILYAI_FUNC void ilyaK_goiffalse (FuncState *fs, expdesc *e);
ILYAI_FUNC void ilyaK_storevar (FuncState *fs, expdesc *var, expdesc *e);
ILYAI_FUNC void ilyaK_setreturns (FuncState *fs, expdesc *e, int nresults);
ILYAI_FUNC void ilyaK_setoneret (FuncState *fs, expdesc *e);
ILYAI_FUNC int ilyaK_jump (FuncState *fs);
ILYAI_FUNC void ilyaK_ret (FuncState *fs, int first, int nret);
ILYAI_FUNC void ilyaK_patchlist (FuncState *fs, int list, int target);
ILYAI_FUNC void ilyaK_patchtohere (FuncState *fs, int list);
ILYAI_FUNC void ilyaK_concat (FuncState *fs, int *l1, int l2);
ILYAI_FUNC int ilyaK_getlabel (FuncState *fs);
ILYAI_FUNC void ilyaK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
ILYAI_FUNC void ilyaK_infix (FuncState *fs, BinOpr op, expdesc *v);
ILYAI_FUNC void ilyaK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
ILYAI_FUNC void ilyaK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
ILYAI_FUNC void ilyaK_setlist (FuncState *fs, int base, int nelems, int tostore);
ILYAI_FUNC void ilyaK_finish (FuncState *fs);
ILYAI_FUNC l_noret ilyaK_semerror (LexState *ls, const char *msg);


#endif
