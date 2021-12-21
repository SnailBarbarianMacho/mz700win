/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                                 Z80.h                                ***/
/***                                                                      ***/
/*** This file contains the function prototypes and variable declarations ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/
#ifndef _Z80_H
#define _Z80_H

/****************************************************************************/
/*** Coding Rule                                                          ***/
/****************************************************************************/
// M_XXXX            macro
// ihl, iix, iiy     indirect HL, IX, IY, memory. (HL), (IX+d), (IY+d), (nn)
// MEM               memory

/****************************************************************************/
/*** If you don't need support X/Y flags and WZ reg, undef it             ***/
/****************************************************************************/
#define Z80_EMU_XY

/****************************************************************************/
/*** Machine dependent definitions                                        ***/
/****************************************************************************/
/* #define DEBUG      */              /* Compile debugging version          */
/* #define X86_ASM    */              /* Compile optimised GCC/x86 version  */
#define LSB_FIRST		              /* Compile for low-endian CPU         */
/* #define __64BIT__  */              /* Compile for 64 bit machines        */
/* #define __128BIT__ */              /* Compile for 128 bit machines       */

/****************************************************************************/
/* If your compiler doesn't know about inlined functions, uncomment this    */
/****************************************************************************/
#ifdef __BORLANDC__
#define INLINE static
#else
// If GCC, __attribute__((always_inline)) 
#define INLINE __forceinline
#endif

#ifndef EMU_TYPES
#define EMU_TYPES

/****************************************************************************/
/* sizeof(byte)=1, sizeof(word)=2, sizeof(dword)>=4                         */
/****************************************************************************/
typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned       dword;
typedef signed char    offset;

typedef unsigned char  u8;
typedef signed   char  s8;
typedef unsigned short u16;
typedef unsigned int   u32;

/****************************************************************************/
/* Define a Z80 word. Upper bytes are always zero                           */
/****************************************************************************/
typedef union
{
#ifdef __128BIT__
 #ifdef LSB_FIRST
   struct { byte l,h,h2,h3,h4,h5,h6,h7,
                 h8,h9,h10,h11,h12,h13,h14,h15; } B;
   struct { word l,h,h2,h3,h4,h5,h6,h7; } W;
   //dword D;
 #else
   struct { byte h15,h14,h13,h12,h11,h10,h9,h8,
                 h7,h6,h5,h4,h3,h2,h,l; } B;
   struct { word h7,h6,h5,h4,h3,h2,h,l; } W;
   //dword D;
 #endif
#elif __64BIT__
 #ifdef LSB_FIRST
   struct { byte l,h,h2,h3,h4,h5,h6,h7; } B;
   struct { word l,h,h2,h3; } W;
   //dword D;
 #else
   struct { byte h7,h6,h5,h4,h3,h2,h,l; } B;
   struct { word h3,h2,h,l; } W;
   //dword D;
 #endif
#else
 #ifdef LSB_FIRST
   struct { byte l,h,h2,h3; } B;
   struct { word l,h; } W;
   //dword D;
 #else
   struct { byte h3,h2,h,l; } B;
   struct { word h,l; } W;
   //dword D;
 #endif
#endif
} pair;

#endif /* EMU_TYPES */

/****************************************************************************/
/*** End of machine dependent definitions                                 ***/
/****************************************************************************/

#ifndef INLINE
#define INLINE static inline
#endif

/****************************************************************************/
/* The Z80 registers. HALT is set to 1 when the CPU is halted, the refresh  */
/* register is calculated as follows: refresh=(Regs.R&127)|(Regs.R2&128)    */
/****************************************************************************/
typedef struct
{
  pair AF,BC,DE,HL,IX,IY,PC,SP;
  pair AF2,BC2,DE2,HL2;
#ifdef Z80_EMU_XY
  pair WZ;
#endif
  unsigned IFF1,IFF2,HALT,IM,I,R,R2;
} Z80_Regs;

#define S_FLAG_SHIFT    7
#define Z_FLAG_SHIFT    6
#define H_FLAG_SHIFT    4
#define V_FLAG_SHIFT    2
#define N_FLAG_SHIFT    1
#define C_FLAG_SHIFT    0

#define S_FLAG          (1 << S_FLAG_SHIFT)
#define Z_FLAG          (1 << Z_FLAG_SHIFT)
#define H_FLAG          (1 << H_FLAG_SHIFT)
#define V_FLAG          (1 << V_FLAG_SHIFT)
#define N_FLAG          (1 << N_FLAG_SHIFT)
#define C_FLAG          (1 << C_FLAG_SHIFT)

#ifdef Z80_EMU_XY
#define Y_FLAG_SHIFT    5
#define X_FLAG_SHIFT    3
#define Y_FLAG          (1 << Y_FLAG_SHIFT)
#define X_FLAG          (1 << X_FLAG_SHIFT)
#endif

/****************************************************************************/
/* Set Z80_Trace to 1 when PC==Z80_Trap. When trace is on, Z80_Debug() is   */
/* called after every instruction                                           */
/****************************************************************************/
#ifdef _DEBUG
extern dword Z80_Trace;
extern dword Z80_Trap;
void Z80_Debug(Z80_Regs *R);
#endif

void ret(void);
void ei(void);

extern int Z80_Running;      /* When 0, emulation terminates                */
extern int Z80_IPeriod;      /* Number of T-states per interrupt            */
extern int Z80_ICount;       /* T-state count                               */
extern int Z80_IRQ;          /* Current IRQ status. Checked after EI occurs */
extern int Z80_intflag;

#define Z80_IGNORE_INT  -1   /* Ignore interrupt                            */
#define Z80_NMI_INT     -2   /* Execute NMI                                 */

void Interrupt(int j);

Z80_Regs * Z80_GetRegsPtr(void);
unsigned Z80_GetPC (void);         /* Get program counter                   */
void Z80_GetRegs (Z80_Regs *Regs); /* Get registers                         */
void Z80_SetRegs (Z80_Regs *Regs); /* Set registers                         */
void Z80_Reset (void);             /* Reset registers to the initial values */
int  Z80_Execute (void);           /* Execute IPeriod T-States              */
word Z80 (void);                   /* Execute until Z80_Running==0          */
void Z80_RegisterDump (void);      /* Prints a dump to stdout               */
void Z80_SetWaitStates (int n);    /* Set number of memory wait states.     */
                                   /* This only affects opcode fetching, so */
                                   /* wait state adjustment is still        */
                                   /* necessary in Z80_RDMEM, Z80_RDOP_ARG, */
                                   /* Z80_RDSTACK and Z80_WRSTACK           */
void Z80_Patch (Z80_Regs *Regs);   /* Called when ED FE occurs. Can be used */
                                   /* to emulate disk access etc.           */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
int Z80_Interrupt(void);           /* This is called after IPeriod T-States */
                                   /* have been executed. It should return  */
                                   /* Z80_IGNORE_INT, Z80_NMI_INT or a byte */
                                   /* identifying the device (most often    */
                                   /* 0xFF)                                 */
void Z80_Reti (void);              /* Called when RETI occurs               */
void Z80_Retn (void);              /* Called when RETN occurs               */
#ifdef __cplusplus
}
#endif // __cplusplus

/****************************************************************************/
/* Definitions of functions to read/write memory and I/O ports              */
/* You can replace these with your own, inlined if necessary                */
/****************************************************************************/
#include "Z80IO.h"

#endif /* _Z80_H */
