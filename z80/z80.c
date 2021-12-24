/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                                 Z80.c                                ***/
/***                                                                      ***/
/*** This file contains the emulation code                                ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crtdbg.h>

#include "Z80.h"
#include "mz700win.h"

#include "MZhw.h"

#define M_RDMEM(A)      Z80_RDMEM(A)
#define M_WRMEM(A,V)    Z80_WRMEM(A,V)
#define M_RDOP(A)       Z80_RDOP(A)
#define M_RDOP_ARG(A)   Z80_RDOP_ARG(A)
#define M_RDSTACK(A)    Z80_RDSTACK(A)
#define M_WRSTACK(A,V)  Z80_WRSTACK(A,V)

void exec_rom1(Z80_Regs *Regs);
void exec_rom2(Z80_Regs *Regs);
void exec_ram(Z80_Regs *Regs);

static Z80_Regs R;
int Z80_Running=1;

int Z80_IPeriod;
int Z80_ICount;

int Z80_intflag = 0;

#ifdef _DEBUG
dword Z80_Trace=0;
dword Z80_Trap=0xFFFFFFFF;
#endif
#ifdef TRACE
static unsigned pc_trace[256];
static unsigned pc_count=0;
#endif

static byte PTable[512];
static byte ZSTable[512];
static byte ZSPTable[512];
static byte ZSPBitTable[256]; // Used only BIT instructions
#include "Z80DAA.h"

typedef void (*opcode_fn) (void);

#define M_C     (R.AF.B.l & C_FLAG)
#define M_NC    (!M_C)
#define M_Z     (R.AF.B.l & Z_FLAG)
#define M_NZ    (!M_Z)
#define M_M     (R.AF.B.l & S_FLAG)
#define M_P     (!M_M)
#define M_PE    (R.AF.B.l & V_FLAG)
#define M_PO    (!M_PE)

#ifdef Z80_EMU_XY
#define M_SET_WZ(val) do { R.WZ.W.l = (val); } while (FALSE)
#define M_SET_WZ_LH(lo, hi) do { R.WZ.B.l = lo; R.WZ.B.h = hi; } while (FALSE)
#define M_XY_FLAG(val) (val & (X_FLAG | Y_FLAG))
#else
#define M_SET_WZ(val)
#define M_SET_WZ_LH(lo, hi)
#define M_XY_FLAG(val)   0
#endif

/* Get next opcode argument and increment program counter */
INLINE byte read_mem_opcode(void)
{
    byte val = M_RDOP_ARG(R.PC.W.l);
    R.PC.W.l++;
    return val;
}

INLINE word read_mem_word(word addr)
{
    word data = M_RDMEM(addr);
    data     += M_RDMEM(((addr) + 1) & 0xffff) << 8;
    return data;
}

INLINE void write_mem_word(word addr, dword V)
{
    M_WRMEM(addr, V & 255);
    M_WRMEM((addr + 1) & 0xffff, V >> 8);
}

INLINE word read_mem_opcode_word(void)
{
    word data = read_mem_opcode();
    data     += read_mem_opcode() << 8;
    return data;
}

INLINE word make_ea_ix() { /* ix+d */
    word ea = (R.IX.W.l + (offset)read_mem_opcode());// &0xffff;
    M_SET_WZ(ea);
    return ea;
}
INLINE word make_ea_iy() { /* iy+d */
    word ea = (R.IY.W.l + (offset)read_mem_opcode());// &
    M_SET_WZ(ea);
    return ea;
}
#define M_RD_HL   M_RDMEM(R.HL.W.l)

INLINE byte read_mem_ix(void) /* (ix+d) */
{
    word addr = make_ea_ix();
    return M_RDMEM(addr);
}
INLINE byte read_mem_iy(void) /* (iy+d) */
{
    word addr = make_ea_iy();
    return M_RDMEM(addr);
}
INLINE void write_mem_ix(byte data)
{
    word addr = make_ea_ix();
    M_WRMEM(addr, data);
}
INLINE void write_mem_iy(byte data)
{
    word addr = make_ea_iy();
    M_WRMEM(addr, data);
}

#include "Z80Codes.h"

// ---------------------------------------------------------------- CPU control instructions
// -------------------------------- nop, halt

static void nop(void) { };
static void halt(void)
{
    --R.PC.W.l;
    R.HALT = 1;
    if (Z80_ICount > 0) { Z80_ICount = 0; }
}

// -------------------------------- Interrupt 

static void im_0(void) { R.IM = 0; }
static void im_1(void) { R.IM = 1; }
static void im_2(void) { R.IM = 2; }

static void di(void) { R.IFF1 = R.IFF2 = 0; }
// ei() is below...

// -------------------------------- C flag

// **** NOTE ****
// The behavior of the XY flag of the SCF / CCF instruction changes depending on the previous instruction. 
// If the flag does not change in the previous instruction, the contents of the A register are ORed, 
// but if the flag changes, it is copied. (Patrik Rak, 2012)
// Emulation can't handle that much 

static void scf(void)
{
    R.AF.B.l =
#ifdef Z80_EMU_XY
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG | X_FLAG | Y_FLAG)) | // SGS/SHARP/ZILOG NMOS Z80
        M_XY_FLAG(R.AF.B.h) |
#else
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG)) |
#endif
        (0 & (N_FLAG | H_FLAG)) |
        C_FLAG;
}

static void ccf(void)
{
    R.AF.B.l =
#ifdef Z80_EMU_XY
        (R.AF.B.l & (S_FLAG | Z_FLAG | X_FLAG | Y_FLAG | V_FLAG | C_FLAG)) |// SGS/SHARP/ZILOG NMOS Z80
        M_XY_FLAG(R.AF.B.h) |
#else
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG | C_FLAG)) |
#endif
        ((R.AF.B.l & C_FLAG) << (H_FLAG_SHIFT - C_FLAG_SHIFT)) |
        (0 & N_FLAG);
    R.AF.B.l ^= C_FLAG;
}

// ---------------------------------------------------------------- 8-bit load instructions
// -------------------------------- ld

static void ld_a_inn(void) { word addr = read_mem_opcode_word(); R.AF.B.h = M_RDMEM(addr); M_SET_WZ(addr + 1); }
static void ld_inn_a(void) { word addr = read_mem_opcode_word(); M_WRMEM(addr, R.AF.B.h);  M_SET_WZ_LH(addr + 1, R.AF.B.h); }

static void ld_a_ibc(void) { R.AF.B.h = M_RDMEM(R.BC.W.l); M_SET_WZ(R.BC.W.l + 1); }
static void ld_a_ide(void) { R.AF.B.h = M_RDMEM(R.DE.W.l); M_SET_WZ(R.DE.W.l + 1); }
static void ld_ibc_a(void) { M_WRMEM(R.BC.W.l, R.AF.B.h); M_SET_WZ_LH(R.BC.W.l + 1, R.AF.B.h); }
static void ld_ide_a(void) { M_WRMEM(R.DE.W.l, R.AF.B.h); M_SET_WZ_LH(R.DE.W.l + 1, R.AF.B.h); }

static void ld_b_n(void) { R.BC.B.h = read_mem_opcode(); }
static void ld_c_n(void) { R.BC.B.l = read_mem_opcode(); }
static void ld_d_n(void) { R.DE.B.h = read_mem_opcode(); }
static void ld_e_n(void) { R.DE.B.l = read_mem_opcode(); }
static void ld_h_n(void) { R.HL.B.h = read_mem_opcode(); }
static void ld_l_n(void) { R.HL.B.l = read_mem_opcode(); }
static void ld_ihl_n(void) { byte data = read_mem_opcode(); M_WRMEM(R.HL.W.l, data); }
static void ld_a_n(void) { R.AF.B.h = read_mem_opcode(); }

static void ld_b_b(void) { }
static void ld_b_a(void) { R.BC.B.h = R.AF.B.h; }
static void ld_b_c(void) { R.BC.B.h = R.BC.B.l; }
static void ld_b_d(void) { R.BC.B.h = R.DE.B.h; }
static void ld_b_e(void) { R.BC.B.h = R.DE.B.l; }
static void ld_b_h(void) { R.BC.B.h = R.HL.B.h; }
static void ld_b_ihl(void) { R.BC.B.h = M_RD_HL; }
static void ld_b_l(void) { R.BC.B.h = R.HL.B.l; }

static void ld_c_b(void) { R.BC.B.l = R.BC.B.h; }
static void ld_c_c(void) { }
static void ld_c_d(void) { R.BC.B.l = R.DE.B.h; }
static void ld_c_e(void) { R.BC.B.l = R.DE.B.l; }
static void ld_c_h(void) { R.BC.B.l = R.HL.B.h; }
static void ld_c_l(void) { R.BC.B.l = R.HL.B.l; }
static void ld_c_ihl(void) { R.BC.B.l = M_RD_HL; }
static void ld_c_a(void) { R.BC.B.l = R.AF.B.h; }

static void ld_d_b(void) { R.DE.B.h = R.BC.B.h; }
static void ld_d_c(void) { R.DE.B.h = R.BC.B.l; }
static void ld_d_d(void) { }
static void ld_d_e(void) { R.DE.B.h = R.DE.B.l; }
static void ld_d_h(void) { R.DE.B.h = R.HL.B.h; }
static void ld_d_l(void) { R.DE.B.h = R.HL.B.l; }
static void ld_d_ihl(void) { R.DE.B.h = M_RD_HL; }
static void ld_d_a(void) { R.DE.B.h = R.AF.B.h; }

static void ld_e_b(void) { R.DE.B.l = R.BC.B.h; }
static void ld_e_c(void) { R.DE.B.l = R.BC.B.l; }
static void ld_e_d(void) { R.DE.B.l = R.DE.B.h; }
static void ld_e_e(void) { }
static void ld_e_h(void) { R.DE.B.l = R.HL.B.h; }
static void ld_e_l(void) { R.DE.B.l = R.HL.B.l; }
static void ld_e_ihl(void) { R.DE.B.l = M_RD_HL; }
static void ld_e_a(void) { R.DE.B.l = R.AF.B.h; }

static void ld_h_b(void) { R.HL.B.h = R.BC.B.h; }
static void ld_h_c(void) { R.HL.B.h = R.BC.B.l; }
static void ld_h_d(void) { R.HL.B.h = R.DE.B.h; }
static void ld_h_e(void) { R.HL.B.h = R.DE.B.l; }
static void ld_h_h(void) { }
static void ld_h_l(void) { R.HL.B.h = R.HL.B.l; }
static void ld_h_ihl(void) { R.HL.B.h = M_RD_HL; }
static void ld_h_a(void) { R.HL.B.h = R.AF.B.h; }

static void ld_l_b(void) { R.HL.B.l = R.BC.B.h; }
static void ld_l_c(void) { R.HL.B.l = R.BC.B.l; }
static void ld_l_d(void) { R.HL.B.l = R.DE.B.h; }
static void ld_l_e(void) { R.HL.B.l = R.DE.B.l; }
static void ld_l_h(void) { R.HL.B.l = R.HL.B.h; }
static void ld_l_l(void) { }
static void ld_l_ihl(void) { R.HL.B.l = M_RD_HL; }
static void ld_l_a(void) { R.HL.B.l = R.AF.B.h; }

static void ld_ihl_b(void) { M_WRMEM(R.HL.W.l, R.BC.B.h); }
static void ld_ihl_c(void) { M_WRMEM(R.HL.W.l, R.BC.B.l); }
static void ld_ihl_d(void) { M_WRMEM(R.HL.W.l, R.DE.B.h); }
static void ld_ihl_e(void) { M_WRMEM(R.HL.W.l, R.DE.B.l); }
static void ld_ihl_h(void) { M_WRMEM(R.HL.W.l, R.HL.B.h); }
static void ld_ihl_l(void) { M_WRMEM(R.HL.W.l, R.HL.B.l); }
static void ld_ihl_a(void) { M_WRMEM(R.HL.W.l, R.AF.B.h); }

static void ld_a_a(void) { }
static void ld_a_b(void) { R.AF.B.h = R.BC.B.h; }
static void ld_a_c(void) { R.AF.B.h = R.BC.B.l; }
static void ld_a_d(void) { R.AF.B.h = R.DE.B.h; }
static void ld_a_e(void) { R.AF.B.h = R.DE.B.l; }
static void ld_a_h(void) { R.AF.B.h = R.HL.B.h; }
static void ld_a_ihl(void) { R.AF.B.h = M_RD_HL; }
static void ld_a_l(void) { R.AF.B.h = R.HL.B.l; }

static void ld_ixh_n(void) { R.IX.B.h = read_mem_opcode(); }
static void ld_ixl_n(void) { R.IX.B.l = read_mem_opcode(); }
static void ld_iyh_n(void) { R.IY.B.h = read_mem_opcode(); }
static void ld_iyl_n(void) { R.IY.B.l = read_mem_opcode(); }

static void ld_b_iix(void) { R.BC.B.h = read_mem_ix(); }
static void ld_c_iix(void) { R.BC.B.l = read_mem_ix(); }
static void ld_d_iix(void) { R.DE.B.h = read_mem_ix(); }
static void ld_e_iix(void) { R.DE.B.l = read_mem_ix(); }
static void ld_h_iix(void) { R.HL.B.h = read_mem_ix(); }
static void ld_l_iix(void) { R.HL.B.l = read_mem_ix(); }
static void ld_a_iix(void) { R.AF.B.h = read_mem_ix(); }

static void ld_b_iiy(void) { R.BC.B.h = read_mem_iy(); }
static void ld_c_iiy(void) { R.BC.B.l = read_mem_iy(); }
static void ld_d_iiy(void) { R.DE.B.h = read_mem_iy(); }
static void ld_e_iiy(void) { R.DE.B.l = read_mem_iy(); }
static void ld_h_iiy(void) { R.HL.B.h = read_mem_iy(); }
static void ld_l_iiy(void) { R.HL.B.l = read_mem_iy(); }
static void ld_a_iiy(void) { R.AF.B.h = read_mem_iy(); }

static void ld_iix_b(void) { write_mem_ix(R.BC.B.h); }
static void ld_iix_c(void) { write_mem_ix(R.BC.B.l); }
static void ld_iix_d(void) { write_mem_ix(R.DE.B.h); }
static void ld_iix_e(void) { write_mem_ix(R.DE.B.l); }
static void ld_iix_h(void) { write_mem_ix(R.HL.B.h); }
static void ld_iix_l(void) { write_mem_ix(R.HL.B.l); }
static void ld_iix_a(void) { write_mem_ix(R.AF.B.h); }

static void ld_iiy_a(void) { write_mem_iy(R.AF.B.h); }
static void ld_iiy_b(void) { write_mem_iy(R.BC.B.h); }
static void ld_iiy_c(void) { write_mem_iy(R.BC.B.l); }
static void ld_iiy_d(void) { write_mem_iy(R.DE.B.h); }
static void ld_iiy_e(void) { write_mem_iy(R.DE.B.l); }
static void ld_iiy_h(void) { write_mem_iy(R.HL.B.h); }
static void ld_iiy_l(void) { write_mem_iy(R.HL.B.l); }

static void ld_b_ixh(void) { R.BC.B.h = R.IX.B.h; }
static void ld_b_ixl(void) { R.BC.B.h = R.IX.B.l; }
static void ld_b_iyh(void) { R.BC.B.h = R.IY.B.h; }
static void ld_b_iyl(void) { R.BC.B.h = R.IY.B.l; }

static void ld_c_ixh(void) { R.BC.B.l = R.IX.B.h; }
static void ld_c_ixl(void) { R.BC.B.l = R.IX.B.l; }
static void ld_c_iyh(void) { R.BC.B.l = R.IY.B.h; }
static void ld_c_iyl(void) { R.BC.B.l = R.IY.B.l; }

static void ld_d_ixh(void) { R.DE.B.h = R.IX.B.h; }
static void ld_d_ixl(void) { R.DE.B.h = R.IX.B.l; }
static void ld_d_iyh(void) { R.DE.B.h = R.IY.B.h; }
static void ld_d_iyl(void) { R.DE.B.h = R.IY.B.l; }

static void ld_e_ixh(void) { R.DE.B.l = R.IX.B.h; }
static void ld_e_ixl(void) { R.DE.B.l = R.IX.B.l; }
static void ld_e_iyh(void) { R.DE.B.l = R.IY.B.h; }
static void ld_e_iyl(void) { R.DE.B.l = R.IY.B.l; }

static void ld_a_ixh(void) { R.AF.B.h = R.IX.B.h; }
static void ld_a_ixl(void) { R.AF.B.h = R.IX.B.l; }
static void ld_a_iyh(void) { R.AF.B.h = R.IY.B.h; }
static void ld_a_iyl(void) { R.AF.B.h = R.IY.B.l; }

static void ld_iix_n(void) { word addr = make_ea_ix(); byte data = read_mem_opcode(); M_WRMEM(addr, data); }
static void ld_iiy_n(void) { word addr = make_ea_iy(); byte data = read_mem_opcode(); M_WRMEM(addr, data); }

static void ld_ixh_b(void) { R.IX.B.h = R.BC.B.h; }
static void ld_ixh_c(void) { R.IX.B.h = R.BC.B.l; }
static void ld_ixh_d(void) { R.IX.B.h = R.DE.B.h; }
static void ld_ixh_e(void) { R.IX.B.h = R.DE.B.l; }
static void ld_ixh_ixh(void) { }
static void ld_ixh_ixl(void) { R.IX.B.h = R.IX.B.l; }
static void ld_ixh_a(void) { R.IX.B.h = R.AF.B.h; }

static void ld_ixl_b(void) { R.IX.B.l = R.BC.B.h; }
static void ld_ixl_c(void) { R.IX.B.l = R.BC.B.l; }
static void ld_ixl_d(void) { R.IX.B.l = R.DE.B.h; }
static void ld_ixl_e(void) { R.IX.B.l = R.DE.B.l; }
static void ld_ixl_ixh(void) { R.IX.B.l = R.IX.B.h; }
static void ld_ixl_ixl(void) { }
static void ld_ixl_a(void) { R.IX.B.l = R.AF.B.h; }

static void ld_iyh_b(void) { R.IY.B.h = R.BC.B.h; }
static void ld_iyh_c(void) { R.IY.B.h = R.BC.B.l; }
static void ld_iyh_d(void) { R.IY.B.h = R.DE.B.h; }
static void ld_iyh_e(void) { R.IY.B.h = R.DE.B.l; }
static void ld_iyh_iyh(void) { }
static void ld_iyh_iyl(void) { R.IY.B.h = R.IY.B.l; }
static void ld_iyh_a(void) { R.IY.B.h = R.AF.B.h; }

static void ld_iyl_b(void) { R.IY.B.l = R.BC.B.h; }
static void ld_iyl_c(void) { R.IY.B.l = R.BC.B.l; }
static void ld_iyl_d(void) { R.IY.B.l = R.DE.B.h; }
static void ld_iyl_e(void) { R.IY.B.l = R.DE.B.l; }
static void ld_iyl_iyh(void) { R.IY.B.l = R.IY.B.h; }
static void ld_iyl_iyl(void) { }
static void ld_iyl_a(void) { R.IY.B.l = R.AF.B.h; }

// -------------------------------- ldi/ldd

static void ldi(void)
{
    byte data = M_RDMEM(R.HL.W.l);
    M_WRMEM(R.DE.W.l, data);
    ++R.DE.W.l;
    ++R.HL.W.l;
    --R.BC.W.l;
    data += R.AF.B.h;
    R.AF.B.l =
        (R.AF.B.l & (S_FLAG | Z_FLAG | C_FLAG)) |
        (0 & (H_FLAG | N_FLAG)) |
#ifdef Z80_EMU_XY
        ((data & (1 << 1)) << (Y_FLAG_SHIFT - 1)) |
        (data & X_FLAG) |
#endif
        (R.BC.W.l ? V_FLAG : 0);
}

static void ldd(void)
{
    byte data = M_RDMEM(R.HL.W.l);
    M_WRMEM(R.DE.W.l, data);
    --R.DE.W.l;
    --R.HL.W.l;
    --R.BC.W.l;
    data += R.AF.B.h;
    R.AF.B.l =
        (R.AF.B.l & (S_FLAG | Z_FLAG | C_FLAG)) |
        (0 & (H_FLAG | N_FLAG)) |
#ifdef Z80_EMU_XY
        ((data & (1 << 1)) << (Y_FLAG_SHIFT - 1)) |
        (data & X_FLAG) |
#endif
        (R.BC.W.l ? V_FLAG : 0);
}

static void ldir(void)
{
    ldi();
    if (R.BC.W.l) {
        R.PC.W.l -= 2;
        M_SET_WZ(R.PC.W.l + 1);
        M_CSTATE(5);
    }
}

static void lddr(void)
{
    ldd();
    if (R.BC.W.l) { 
        R.PC.W.l -= 2; 
        M_SET_WZ(R.PC.W.l + 1);
        M_CSTATE(5);
    }
}

// -------------------------------- interrupt and refresh

INLINE void ld_ai(const byte data)
{
    R.AF.B.h = data;
    R.AF.B.l =
        (R.AF.B.l & C_FLAG) |
        ZSTable[R.AF.B.h] |
        M_XY_FLAG(R.AF.B.h) |
        (R.IFF2 << V_FLAG);
}

static void ld_a_i(void) { ld_ai(R.I); }
static void ld_a_r(void) { ld_ai((R.R & 0x7f) | (R.R2 & 0x80)); }
static void ld_i_a(void) { R.I = R.AF.B.h; }
static void ld_r_a(void) { R.R = R.R2 = R.AF.B.h; }

// ---------------------------------------------------------------- 16-bit load instructions
// -------------------------------- ld16

static void ld_bc_nn(void) { R.BC.W.l = read_mem_opcode_word(); }
static void ld_de_nn(void) { R.DE.W.l = read_mem_opcode_word(); }
static void ld_hl_nn(void) { R.HL.W.l = read_mem_opcode_word(); }
static void ld_ix_nn(void) { R.IX.W.l = read_mem_opcode_word(); }
static void ld_iy_nn(void) { R.IY.W.l = read_mem_opcode_word(); }
static void ld_sp_nn(void) { R.SP.W.l = read_mem_opcode_word(); }

INLINE word ld_rr_inn(void)
{
    word addr = read_mem_opcode_word();
    M_SET_WZ(addr + 1);
    return read_mem_word(addr);
}

static void ld_bc_inn(void) { R.BC.W.l = ld_rr_inn(); }
static void ld_de_inn(void) { R.DE.W.l = ld_rr_inn(); }
static void ld_hl_inn(void) { R.HL.W.l = ld_rr_inn(); }
static void ld_ix_inn(void) { R.IX.W.l = ld_rr_inn(); }
static void ld_iy_inn(void) { R.IY.W.l = ld_rr_inn(); }
static void ld_sp_inn(void) { R.SP.W.l = ld_rr_inn(); }

static void ld_inn_rr(const word data)
{
    word addr = read_mem_opcode_word(); 
    write_mem_word(addr, data);
    M_SET_WZ(addr + 1);
}

static void ld_inn_bc(void) { ld_inn_rr(R.BC.W.l); }
static void ld_inn_de(void) { ld_inn_rr(R.DE.W.l); }
static void ld_inn_hl(void) { ld_inn_rr(R.HL.W.l); }
static void ld_inn_ix(void) { ld_inn_rr(R.IX.W.l); }
static void ld_inn_iy(void) { ld_inn_rr(R.IY.W.l); }
static void ld_inn_sp(void) { ld_inn_rr(R.SP.W.l); }

static void ld_sp_hl(void)  { R.SP.W.l = R.HL.W.l; }
static void ld_sp_ix(void)  { R.SP.W.l = R.IX.W.l; }
static void ld_sp_iy(void)  { R.SP.W.l = R.IY.W.l; }

// -------------------------------- ex

static void ex_af_af(void) { word tmp = R.AF.W.l; R.AF.W.l = R.AF2.W.l; R.AF2.W.l = tmp; }
static void ex_de_hl(void) { word tmp = R.DE.W.l; R.DE.W.l = R.HL.W.l;  R.HL.W.l  = tmp; }
static void exx(void) 
{
    word tmp;
    tmp = R.BC.W.l; R.BC.W.l = R.BC2.W.l; R.BC2.W.l = tmp;
    tmp = R.DE.W.l; R.DE.W.l = R.DE2.W.l; R.DE2.W.l = tmp;
    tmp = R.HL.W.l; R.HL.W.l = R.HL2.W.l; R.HL2.W.l = tmp;
}

static void ex_isp_hl(void) { word tmp = read_mem_word(R.SP.W.l); write_mem_word(R.SP.W.l, R.HL.W.l); R.HL.W.l = tmp; M_SET_WZ(tmp); }
static void ex_isp_ix(void) { word tmp = read_mem_word(R.SP.W.l); write_mem_word(R.SP.W.l, R.IX.W.l); R.IX.W.l = tmp; M_SET_WZ(tmp); }
static void ex_isp_iy(void) { word tmp = read_mem_word(R.SP.W.l); write_mem_word(R.SP.W.l, R.IY.W.l); R.IY.W.l = tmp; M_SET_WZ(tmp); }

// -------------------------------- push/pop

INLINE void push(const word data)
{
    R.SP.W.l -= 2;
    M_WRSTACK(R.SP.W.l, data);
    M_WRSTACK((R.SP.W.l + 1) & 0xffff, data >> 8);
}

INLINE word pop()
{
    word data = M_RDSTACK(R.SP.W.l) + (M_RDSTACK((R.SP.W.l + 1) & 0xffff) << 8);
    R.SP.W.l += 2;
    return data;
}

static void push_af(void) { push(R.AF.W.l); }
static void push_bc(void) { push(R.BC.W.l); }
static void push_de(void) { push(R.DE.W.l); }
static void push_hl(void) { push(R.HL.W.l); }
static void push_ix(void) { push(R.IX.W.l); }
static void push_iy(void) { push(R.IY.W.l); }

static void pop_af(void) { R.AF.W.l = pop(); }
static void pop_bc(void) { R.BC.W.l = pop(); }
static void pop_de(void) { R.DE.W.l = pop(); }
static void pop_hl(void) { R.HL.W.l = pop(); }
static void pop_ix(void) { R.IX.W.l = pop(); }
static void pop_iy(void) { R.IY.W.l = pop(); }

// ---------------------------------------------------------------- I/O instructions
// -------------------------------- out
INLINE void out_ic_r(const byte data)
{
    Z80_Out(R.BC.W.l, data);
    M_SET_WZ(R.BC.W.l + 1);
}

static void out_ic_b(void) { out_ic_r(R.BC.B.h); }
static void out_ic_c(void) { out_ic_r(R.BC.B.l); }
static void out_ic_d(void) { out_ic_r(R.DE.B.h); }
static void out_ic_e(void) { out_ic_r(R.DE.B.l); }
static void out_ic_h(void) { out_ic_r(R.HL.B.h); }
static void out_ic_l(void) { out_ic_r(R.HL.B.l); }
static void out_ic_0(void) { out_ic_r(0x00); }
static void out_ic_a(void) { out_ic_r(R.AF.B.h); }

static void out_inn_a(void) { 
    byte port = read_mem_opcode(); 
    M_SET_WZ_LH(port + 1, R.AF.B.h);
    Z80_Out((word)(R.AF.B.h << 8) | port, R.AF.B.h);
}

// -------------------------------- in

INLINE byte in_r_ic()
{
    byte data = Z80_In(R.BC.W.l);
    M_SET_WZ(R.BC.W.l + 1);
    R.AF.B.l =
        ZSPTable[data] |
        (0 & (H_FLAG | N_FLAG)) |
        M_XY_FLAG(data) |
        (R.AF.B.l & C_FLAG);
    return data;
}

static void in_b_ic(void) { R.BC.B.h = in_r_ic(); }
static void in_c_ic(void) { R.BC.B.l = in_r_ic(); }
static void in_d_ic(void) { R.DE.B.h = in_r_ic(); }
static void in_e_ic(void) { R.DE.B.l = in_r_ic(); }
static void in_h_ic(void) { R.HL.B.h = in_r_ic(); }
static void in_l_ic(void) { R.HL.B.l = in_r_ic(); }
static void in_0_ic(void) {            in_r_ic(); }
static void in_a_ic(void) { R.AF.B.h = in_r_ic(); }

static void in_a_inn(void) 
{ 
    word port = (R.AF.B.h << 8) | read_mem_opcode();
    M_SET_WZ(port + 1);
    R.AF.B.h = Z80_In(port);
}

// -------------------------------- outi/outd
static void outi(void)
{
    byte data = M_RDMEM(R.HL.W.l);
    --R.BC.B.h;
    Z80_Out(R.BC.W.l, data);
    ++R.HL.W.l;
    M_SET_WZ(R.BC.W.l + 1);

    word tmp = data + R.HL.B.l;
    R.AF.B.l =
        ZSTable[R.BC.B.h] |
        M_XY_FLAG(R.BC.B.h) |
        ((data >> (7 - N_FLAG_SHIFT)) & N_FLAG) |
        ((tmp & 0x100) ? (H_FLAG | C_FLAG) : 0) |
        PTable[(tmp & 0x07) ^ R.BC.B.h];
}
static void outd(void)
{
    byte data = M_RDMEM(R.HL.W.l);
    --R.BC.B.h;
    Z80_Out(R.BC.W.l, data);
    --R.HL.W.l;
    M_SET_WZ(R.BC.W.l - 1);

    word tmp = data + R.HL.B.l;
    R.AF.B.l =
        ZSTable[R.BC.B.h] |
        M_XY_FLAG(R.BC.B.h) |
        ((data >> (7 - N_FLAG_SHIFT)) & N_FLAG) |
        ((tmp & 0x100) ? (H_FLAG | C_FLAG) : 0) |
        PTable[(tmp & 0x07) ^ R.BC.B.h];
}
static void otir(void) { outi(); if (R.BC.B.h) { M_CSTATE(5); R.PC.W.l -= 2; } }
static void otdr(void) { outd(); if (R.BC.B.h) { M_CSTATE(5); R.PC.W.l -= 2; } }

// -------------------------------- ini/ind

static void ini(void)
{
    byte data = Z80_In(R.BC.W.l);
    M_SET_WZ(R.BC.W.l + 1);
    //dprintf("ini port:%04x data:%02x WZ:%04x\n", R.BC.W.l, data, R.WZ.W.l);
    --R.BC.B.h;
    M_WRMEM(R.HL.W.l, data);
    ++R.HL.W.l;

    word tmp = data + ((R.BC.B.l + 1) & 0xff);
    R.AF.B.l =
        ZSTable[R.BC.B.h] |
        M_XY_FLAG(R.BC.B.h) |
        ((data >> (7 - N_FLAG_SHIFT)) & N_FLAG) |
        ((tmp & 0x100) ? (H_FLAG | C_FLAG) : 0) |
        PTable[(tmp & 0x07) ^ R.BC.B.h];
}
static void ind(void)
{
    M_SET_WZ(R.BC.W.l - 1);
    byte data = Z80_In(R.BC.W.l);
    //dprintf("ind port:%04x data:%02x WZ:%04x\n", R.BC.W.l, data, R.WZ.W.l);
    --R.BC.B.h;
    M_WRMEM(R.HL.W.l, data);
    --R.HL.W.l;

    word tmp = data + ((R.BC.B.l - 1) & 0xff);
    R.AF.B.l =
        ZSTable[R.BC.B.h] |
        M_XY_FLAG(R.BC.B.h) |
        ((data >> (7 - N_FLAG_SHIFT)) & N_FLAG) |
        ((tmp & 0x100) ? (H_FLAG | C_FLAG) : 0) |
        PTable[(tmp & 0x07) ^ R.BC.B.h];
}

static void inir(void) { ini(); if (R.BC.B.h) { M_CSTATE(5); R.PC.W.l -= 2; } }
static void indr(void) { ind(); if (R.BC.B.h) { M_CSTATE(5); R.PC.W.l -= 2; } }


// ---------------------------------------------------------------- Jump / return instructions
// -------------------------------- jp

INLINE void jp_cc(const byte cc)
{
#ifdef Z80_EMU_XY
    word pc = M_RDOP_ARG(R.PC.W.l) + ((M_RDOP_ARG((R.PC.W.l + 1) & 0xffff)) << 8);
    M_SET_WZ(pc);
    if (cc) { R.PC.W.l = pc; }
    else { R.PC.W.l += 2; }
#else
    if (cc) { R.PC.D = M_RDOP_ARG(R.PC.W.l) + ((M_RDOP_ARG((R.PC.W.l + 1) & 0xffff)) << 8); }
    else { R.PC.W.l += 2; }
#endif
    //		 if (R.PC.W.l >= 0xE000)
    //	dprintf("JP $%04x\n",R.PC.W.l);
}

static void jp(void)    { jp_cc(TRUE); }
static void jp_hl(void) { R.PC.W.l = R.HL.W.l; }
static void jp_ix(void) { R.PC.W.l = R.IX.W.l; }
static void jp_iy(void) { R.PC.W.l = R.IY.W.l; }
static void jp_c(void)  { jp_cc(M_C); }
static void jp_m(void)  { jp_cc(M_M); }
static void jp_nc(void) { jp_cc(M_NC); }
static void jp_nz(void) { jp_cc(M_NZ); }
static void jp_p(void)  { jp_cc(M_P); }
static void jp_pe(void) { jp_cc(M_PE); }
static void jp_po(void) { jp_cc(M_PO); }
static void jp_z(void)  { jp_cc(M_Z); }

// -------------------------------- jr

INLINE void jr_cc(const byte cc)
{
    if (cc) {
        R.PC.W.l += ((offset)M_RDOP_ARG(R.PC.W.l)) + 1;
        M_SET_WZ(R.PC.W.l);
        M_CSTATE(5);
    } else {
        R.PC.W.l += 1;
    }
}

static void jr(void)    { jr_cc(TRUE); }
static void jr_c(void)  { jr_cc(M_C);  }
static void jr_nc(void) { jr_cc(M_NC); }
static void jr_nz(void) { jr_cc(M_NZ); }
static void jr_z(void)  { jr_cc(M_Z);  }

static void djnz(void) { jr_cc(--R.BC.B.h); }

// -------------------------------- call

INLINE void call_cc(const byte cc) 
{
#ifdef Z80_EMU_XY
    word addr = read_mem_opcode_word();
    M_SET_WZ(addr);
    if (cc) {
        push(R.PC.W.l);
        R.PC.W.l = addr;
        M_CSTATE(7);
    }
#else
    if (cc) {
        word addr = read_mem_opcode_word();
        push(R.PC.W.l);
        R.PC.W.l = addr;
        M_CSTATE(7);
    } else {
        R.PC.W.l += 2;
    }
#endif
}

static void call(void)    { call_cc(TRUE); }
static void call_c(void)  { call_cc(M_C);  }
static void call_m(void)  { call_cc(M_M);  }
static void call_nc(void) { call_cc(M_NC); }
static void call_nz(void) { call_cc(M_NZ); }
static void call_p(void)  { call_cc(M_P);  }
static void call_pe(void) { call_cc(M_PE); }
static void call_po(void) { call_cc(M_PO); }
static void call_z(void)  { call_cc(M_Z);  }

// -------------------------------- ret, reti, retn

INLINE void ret_cc(const byte cc)
{
    if (cc) {
        R.PC.W.l = pop();
        M_SET_WZ(R.PC.W.l);
        M_CSTATE(6);
    }
}

void        ret(void)    { ret_cc(TRUE); }
static void ret_c(void)  { ret_cc(M_C);  }
static void ret_m(void)  { ret_cc(M_M);  }
static void ret_nc(void) { ret_cc(M_NC); }
static void ret_nz(void) { ret_cc(M_NZ); }
static void ret_p(void)  { ret_cc(M_P);  }
static void ret_pe(void) { ret_cc(M_PE); }
static void ret_po(void) { ret_cc(M_PO); }
static void ret_z(void)  { ret_cc(M_Z);  }

static void reti(void) { Z80_Reti(); ret_cc(TRUE); }
static void retn(void) { R.IFF1 = R.IFF2; Z80_Retn(); ret_cc(TRUE); }

// -------------------------------- rst

INLINE void rst_n(const word addr) 
{
    push(R.PC.W.l);
    R.PC.W.l = addr;
    M_SET_WZ(R.PC.W.l);
    // dprintf("rst $%x\n", addr)
}

static void rst_00(void) { rst_n(0x00); }
static void rst_08(void) { rst_n(0x08); }
static void rst_10(void) { rst_n(0x10); }
static void rst_18(void) { rst_n(0x18); }
static void rst_20(void) { rst_n(0x20); }
static void rst_28(void) { rst_n(0x28); }
static void rst_30(void) { rst_n(0x30); }
static void rst_38(void) { rst_n(0x38); }

// ---------------------------------------------------------------- 8-bit arithmetic operation instructions
// -------------------------------- add
// A = B + C.  If (MSB(C ^ B) == 0) and (MSB(C ^ A) == 1) then overflow.

INLINE void add8(const byte data)
{
    word val = R.AF.B.h + data;
    R.AF.B.l =
        ZSTable[val & 0xff] |
        ((val & 0x100) >> (8 - C_FLAG_SHIFT)) |
        M_XY_FLAG(val) |
        ((R.AF.B.h ^ val ^ data) & H_FLAG) |
        (((data ^ R.AF.B.h ^ 0x80) & (data ^ val) & 0x80) >> (7 - V_FLAG_SHIFT));
    R.AF.B.h = (byte)val;
}

static void add_a_n(void) { byte data = read_mem_opcode(); add8(data); }

static void add_a_b(void)   { add8(R.BC.B.h); }
static void add_a_c(void)   { add8(R.BC.B.l); }
static void add_a_d(void)   { add8(R.DE.B.h); }
static void add_a_e(void)   { add8(R.DE.B.l); }
static void add_a_h(void)   { add8(R.HL.B.h); }
static void add_a_l(void)   { add8(R.HL.B.l); }
static void add_a_ihl(void) { add8(M_RD_HL); }
static void add_a_a(void)   { add8(R.AF.B.h); }

static void add_a_iix(void) { add8(read_mem_ix()); }
static void add_a_iiy(void) { add8(read_mem_iy()); }

static void add_a_ixl(void) { add8(R.IX.B.l); }
static void add_a_ixh(void) { add8(R.IX.B.h); }

static void add_a_iyl(void) { add8(R.IY.B.l); }
static void add_a_iyh(void) { add8(R.IY.B.h); }

// -------------------------------- adc

INLINE void adc8(const byte data)
{
    word val = R.AF.B.h + data + (R.AF.B.l & C_FLAG);
    R.AF.B.l =
        ZSTable[val & 0xff] |
        ((val & 0x100) >> (8 - C_FLAG_SHIFT)) |
        M_XY_FLAG(val) |
        ((R.AF.B.h ^ val ^ data) & H_FLAG) |
        (((data ^ R.AF.B.h ^ 0x80) & (data ^ val) & 0x80) >> (7 - V_FLAG_SHIFT));
    R.AF.B.h = (byte)val;
}

static void adc_a_n(void) { byte d = read_mem_opcode(); adc8(d); }

static void adc_a_b(void)   { adc8(R.BC.B.h); }
static void adc_a_c(void)   { adc8(R.BC.B.l); }
static void adc_a_d(void)   { adc8(R.DE.B.h); }
static void adc_a_e(void)   { adc8(R.DE.B.l); }
static void adc_a_h(void)   { adc8(R.HL.B.h); }
static void adc_a_l(void)   { adc8(R.HL.B.l); }
static void adc_a_ihl(void) { adc8(M_RD_HL); }
static void adc_a_a(void)   { adc8(R.AF.B.h); }

static void adc_a_iix(void) { adc8(read_mem_ix()); }
static void adc_a_iiy(void) { adc8(read_mem_iy()); }

static void adc_a_ixl(void) { adc8(R.IX.B.l); }
static void adc_a_ixh(void) { adc8(R.IX.B.h); }

static void adc_a_iyl(void) { adc8(R.IY.B.l); }
static void adc_a_iyh(void) { adc8(R.IY.B.h); }

// -------------------------------- sub
// A = B - C.  If (MSB(C ^ B) == 1) and (MSB(C ^ A) == 0) then overflow.

INLINE void sub8(const byte data)
{
    word val = R.AF.B.h - data;
    R.AF.B.l =
        ZSTable[val & 0xff] |
        M_XY_FLAG(val) |
        ((val & 0x100) >> (8 - C_FLAG_SHIFT)) |
        N_FLAG |
        ((R.AF.B.h ^ val ^ data) & H_FLAG) |
        (((data ^ R.AF.B.h) & (data ^ val ^ 0x80) & 0x80) >> (7 - V_FLAG_SHIFT));
    R.AF.B.h = (byte)val;
}

static void sub_a_n(void) { byte d = read_mem_opcode(); sub8(d); }

static void sub_a_b(void)   { sub8(R.BC.B.h); }
static void sub_a_c(void)   { sub8(R.BC.B.l); }
static void sub_a_d(void)   { sub8(R.DE.B.h); }
static void sub_a_e(void)   { sub8(R.DE.B.l); }
static void sub_a_h(void)   { sub8(R.HL.B.h); }
static void sub_a_l(void)   { sub8(R.HL.B.l); }
static void sub_a_ihl(void) { sub8(M_RD_HL); }
static void sub_a_a(void)   { sub8(R.AF.B.h); }

static void sub_a_iix(void) { sub8(read_mem_ix()); }
static void sub_a_iiy(void) { sub8(read_mem_iy()); }

static void sub_a_ixh(void) { sub8(R.IX.B.h); }
static void sub_a_ixl(void) { sub8(R.IX.B.l); }

static void sub_a_iyh(void) { sub8(R.IY.B.h); }
static void sub_a_iyl(void) { sub8(R.IY.B.l); }

// -------------------------------- sbc

INLINE void sbc8(const byte data)
{
    word val = R.AF.B.h - data - (R.AF.B.l & C_FLAG);
    R.AF.B.l =
        ZSTable[val & 0xff] |
        M_XY_FLAG(val) |
        ((val & 0x100) >> (8 - C_FLAG_SHIFT)) |
        N_FLAG |
        ((R.AF.B.h ^ val ^ data) & H_FLAG) |
        (((data ^ R.AF.B.h) & (data ^ val ^ 0x80) & 0x80) >> (7 - V_FLAG_SHIFT));
    R.AF.B.h = (byte)val;
}

static void sbc_a_n(void) { byte d = read_mem_opcode(); sbc8(d); }

static void sbc_a_b(void)   { sbc8(R.BC.B.h); }
static void sbc_a_c(void)   { sbc8(R.BC.B.l); }
static void sbc_a_d(void)   { sbc8(R.DE.B.h); }
static void sbc_a_e(void)   { sbc8(R.DE.B.l); }
static void sbc_a_h(void)   { sbc8(R.HL.B.h); }
static void sbc_a_l(void)   { sbc8(R.HL.B.l); }
static void sbc_a_ihl(void) { sbc8(M_RD_HL); }
static void sbc_a_a(void)   { sbc8(R.AF.B.h); }

static void sbc_a_iix(void) { sbc8(read_mem_ix()); }
static void sbc_a_iiy(void) { sbc8(read_mem_iy()); }

static void sbc_a_ixh(void) { sbc8(R.IX.B.h); }
static void sbc_a_ixl(void) { sbc8(R.IX.B.l); }

static void sbc_a_iyh(void) { sbc8(R.IY.B.h); }
static void sbc_a_iyl(void) { sbc8(R.IY.B.l); }

// -------------------------------- neg, daa

static void neg(void)
{
    byte val = R.AF.B.h;
    R.AF.B.h = 0;
    sub8(val);
}

static void daa(void)
{
    int val = R.AF.B.h;
    if (R.AF.B.l & C_FLAG) { val |= 256; }
    if (R.AF.B.l & H_FLAG) { val |= 512; }
    if (R.AF.B.l & N_FLAG) { val |= 1024; }
    R.AF.W.l = DAATable[val];
};

// -------------------------------- inc

#define M_INC(Reg) do {                                 \
    ++(Reg);                                            \
    R.AF.B.l =                                          \
        (R.AF.B.l & C_FLAG) |                           \
        ZSTable[Reg] |                                  \
        (((Reg) == 0x80) ? V_FLAG : 0) |                \
        M_XY_FLAG(Reg) |                                \
        (0 & N_FLAG) |                                  \
        (((Reg) & 0x0f) ? 0 : H_FLAG);                  \
} while (FALSE)

static void inc_b(void) { M_INC(R.BC.B.h); }
static void inc_c(void) { M_INC(R.BC.B.l); }
static void inc_d(void) { M_INC(R.DE.B.h); }
static void inc_e(void) { M_INC(R.DE.B.l); }
static void inc_h(void) { M_INC(R.HL.B.h); }
static void inc_l(void) { M_INC(R.HL.B.l); }
static void inc_ihl(void) { byte data = M_RDMEM(R.HL.W.l); M_INC(data); M_WRMEM(R.HL.W.l, data); }
static void inc_a(void) { M_INC(R.AF.B.h); }

static void inc_iix(void) { word addr = make_ea_ix(); byte data = M_RDMEM(addr); M_INC(data); M_WRMEM(addr, data); }
static void inc_iiy(void) { word addr = make_ea_iy(); byte data = M_RDMEM(addr); M_INC(data); M_WRMEM(addr, data); }

static void inc_ixh(void) { M_INC(R.IX.B.h); }
static void inc_ixl(void) { M_INC(R.IX.B.l); }

static void inc_iyh(void) { M_INC(R.IY.B.h); }
static void inc_iyl(void) { M_INC(R.IY.B.l); }

// -------------------------------- dec

#define M_DEC(Reg) do {                                 \
   byte r = (Reg);                                      \
    --(Reg);                                            \
    R.AF.B.l =                                          \
        (R.AF.B.l & C_FLAG) |                           \
        ZSTable[Reg] |                                  \
        ((r == 0x80) ? V_FLAG : 0) |                    \
        M_XY_FLAG(Reg) |                                \
        N_FLAG |                                        \
        ((r & 0x0f) ? 0 : H_FLAG);                      \
} while (FALSE)

static void dec_b(void) { M_DEC(R.BC.B.h); }
static void dec_c(void) { M_DEC(R.BC.B.l); }
static void dec_d(void) { M_DEC(R.DE.B.h); }
static void dec_e(void) { M_DEC(R.DE.B.l); }
static void dec_h(void) { M_DEC(R.HL.B.h); }
static void dec_l(void) { M_DEC(R.HL.B.l); }
static void dec_ihl(void) { byte data = M_RDMEM(R.HL.W.l); M_DEC(data); M_WRMEM(R.HL.W.l, data); }
static void dec_a(void) { M_DEC(R.AF.B.h); }

static void dec_xix(void) { word addr = make_ea_ix(); byte data = M_RDMEM(addr); M_DEC(data); M_WRMEM(addr, data); }
static void dec_xiy(void) { word addr = make_ea_iy(); byte data = M_RDMEM(addr); M_DEC(data); M_WRMEM(addr, data); }

static void dec_ixh(void) { M_DEC(R.IX.B.h); }
static void dec_ixl(void) { M_DEC(R.IX.B.l); }

static void dec_iyh(void) { M_DEC(R.IY.B.h); }
static void dec_iyl(void) { M_DEC(R.IY.B.l); }

// ---------------------------------------------------------------- 16-bit arithmetic operation instructions
// -------------------------------- add16

#define M_ADD16(Reg1, Reg2) do {                        \
    M_SET_WZ(R.Reg1.W.l + 1);                           \
    dword val = R.Reg1.W.l + R.Reg2.W.l; /* Uses bit 16 to check overflow */\
    R.AF.B.l =                                          \
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG)) |       \
        M_XY_FLAG(val >> 8) |                           \
        (((R.Reg1.W.l ^ val ^ R.Reg2.W.l) >> 8) & H_FLAG) | \
        ((val >> 16) & C_FLAG);                         \
    R.Reg1.W.l = (word)val;                             \
} while (FALSE)

static void add_hl_bc(void) { M_ADD16(HL, BC); }
static void add_hl_de(void) { M_ADD16(HL, DE); }
static void add_hl_hl(void) { M_ADD16(HL, HL); }
static void add_hl_sp(void) { M_ADD16(HL, SP); }
static void add_ix_bc(void) { M_ADD16(IX, BC); }
static void add_ix_de(void) { M_ADD16(IX, DE); }
static void add_ix_ix(void) { M_ADD16(IX, IX); }
static void add_ix_sp(void) { M_ADD16(IX, SP); }
static void add_iy_bc(void) { M_ADD16(IY, BC); }
static void add_iy_de(void) { M_ADD16(IY, DE); }
static void add_iy_iy(void) { M_ADD16(IY, IY); }
static void add_iy_sp(void) { M_ADD16(IY, SP); }

// -------------------------------- adc16

INLINE void adc16(const word data)
{
    M_SET_WZ(R.HL.W.l + 1);
    dword tmp = data + (R.AF.W.l & C_FLAG);
    dword val = R.HL.W.l + tmp; // Uses bit 16 to check overflow
    R.AF.B.l =
        (((R.HL.W.l ^ val ^ tmp) >> 8) & H_FLAG) |
        ((val >> 16) & C_FLAG) |
        ((val & 0xffff) ? 0 : Z_FLAG) |
#ifdef Z80_EMU_XY
        ((val >> 8) & (S_FLAG | X_FLAG | Y_FLAG)) |
#else
        ((val >> 8) & S_FLAG) |
#endif
        (((tmp ^ R.HL.W.l ^ 0x8000) & (tmp ^ val) & 0x8000) >> (15 - V_FLAG_SHIFT));
    R.HL.W.l = val;
}

static void adc_hl_bc(void) { adc16(R.BC.W.l); }
static void adc_hl_de(void) { adc16(R.DE.W.l); }
static void adc_hl_hl(void) { adc16(R.HL.W.l); }
static void adc_hl_sp(void) { adc16(R.SP.W.l); }

// -------------------------------- sbc16

INLINE void sbc16(const word data)
{
    M_SET_WZ(R.HL.W.l + 1);
    dword tmp = data + (R.AF.W.l & C_FLAG);
    dword val = R.HL.W.l - tmp; // Uses bit 16 to check overflow
    R.AF.B.l =
        (((R.HL.W.l ^ val ^ tmp) & 0x1000) >> 8) |
        ((val >> 16) & C_FLAG) |
        ((val & 0xffff) ? 0 : Z_FLAG) |
#ifdef Z80_EMU_XY
        ((val >> 8) & (S_FLAG | X_FLAG | Y_FLAG)) |
#else
        ((val >> 8) & S_FLAG) |
#endif
        (((tmp ^ R.HL.W.l) & (tmp ^ val ^ 0x8000) & 0x8000) >> (15 - V_FLAG_SHIFT)) |
        N_FLAG;
    R.HL.W.l = val;
}
static void sbc_hl_bc(void) { sbc16(R.BC.W.l); }
static void sbc_hl_de(void) { sbc16(R.DE.W.l); }
static void sbc_hl_hl(void) { sbc16(R.HL.W.l); }
static void sbc_hl_sp(void) { sbc16(R.SP.W.l); }

// -------------------------------- inc16

static void inc_bc(void) { ++R.BC.W.l; }
static void inc_de(void) { ++R.DE.W.l; }
static void inc_hl(void) { ++R.HL.W.l; }
static void inc_ix(void) { ++R.IX.W.l; }
static void inc_iy(void) { ++R.IY.W.l; }
static void inc_sp(void) { ++R.SP.W.l; }

// -------------------------------- dec16

static void dec_bc(void) { --R.BC.W.l; }
static void dec_de(void) { --R.DE.W.l; }
static void dec_hl(void) { --R.HL.W.l; }
static void dec_ix(void) { --R.IX.W.l; }
static void dec_iy(void) { --R.IY.W.l; }
static void dec_sp(void) { --R.SP.W.l; }

// ---------------------------------------------------------------- Compare operation instructions
// -------------------------------- cp

INLINE void cp(const byte data)
{
    word val = R.AF.B.h - data;// Uses bit 8 to check overflow
    R.AF.B.l =
        ZSTable[val & 0xff] |
        M_XY_FLAG(data) |
        ((val & 0x100) >> (8 - C_FLAG_SHIFT)) |
        N_FLAG |
        ((R.AF.B.h ^ val ^ data) & H_FLAG) |
        (((data ^ R.AF.B.h) & (data ^ val ^ 0x80) & 0x80) >> (7 - V_FLAG_SHIFT));
}

static void cp_n(void) { cp(read_mem_opcode()); }

static void cp_b(void) { cp(R.BC.B.h); }
static void cp_c(void) { cp(R.BC.B.l); }
static void cp_d(void) { cp(R.DE.B.h); }
static void cp_e(void) { cp(R.DE.B.l); }
static void cp_h(void) { cp(R.HL.B.h); }
static void cp_l(void) { cp(R.HL.B.l); }
static void cp_ihl(void) { cp(M_RD_HL); }
static void cp_a(void) { cp(R.AF.B.h); }

static void cp_iix(void) { cp(read_mem_ix()); }
static void cp_iiy(void) { cp(read_mem_iy()); }

static void cp_ixh(void) { cp(R.IX.B.h); }
static void cp_ixl(void) { cp(R.IX.B.l); }

static void cp_iyh(void) { cp(R.IY.B.h); }
static void cp_iyl(void) { cp(R.IY.B.l); }

// -------------------------------- cpi / cpd

static void cpi(void)
{
    M_SET_WZ(R.WZ.W.l + 1);
    byte data = M_RDMEM(R.HL.W.l);
    byte tmp = R.AF.B.h - data;
    ++R.HL.W.l;
    --R.BC.W.l;
    R.AF.B.l =
        (R.AF.B.l & C_FLAG) |
        ZSTable[tmp] |
        ((R.AF.B.h ^ data ^ tmp) & H_FLAG) |
        (R.BC.W.l ? V_FLAG : 0) |
        N_FLAG;
    if (R.AF.B.l & H_FLAG) { tmp--; }
#ifdef Z80_EMU_XY
    R.AF.B.l |=
        ((tmp & (1 << 1)) << (Y_FLAG_SHIFT - 1)) |
        (tmp & X_FLAG);
#endif
}

static void cpd(void)
{
    M_SET_WZ(R.WZ.W.l - 1);
    byte data = M_RDMEM(R.HL.W.l);
    byte tmp = R.AF.B.h - data;
    --R.HL.W.l;
    --R.BC.W.l;
    R.AF.B.l =
        (R.AF.B.l & C_FLAG) |
        ZSTable[tmp] |
        ((R.AF.B.h ^ data ^ tmp) & H_FLAG) |
        (R.BC.W.l ? V_FLAG : 0) |
        N_FLAG;
    if (R.AF.B.l & H_FLAG) { tmp--; }
#ifdef Z80_EMU_XY
    R.AF.B.l |=
        ((tmp & (1 << 1)) << (Y_FLAG_SHIFT - 1)) |
        (tmp & X_FLAG);
#endif
}

static void cpir(void)
{
    cpi();
    if (R.BC.W.l && !(R.AF.B.l & Z_FLAG)) {
        R.PC.W.l -= 2; 
        M_SET_WZ(R.PC.W.l + 1);
        M_CSTATE(5);
    }
}

static void cpdr(void)
{
    cpd();
    // if (R.BC.D && !(R.AF.B.l&Z_FLAG)) { Z80_ICount-=5; R.PC.W.l-=2; }
    if (R.BC.W.l && !(R.AF.B.l & Z_FLAG)) {
        R.PC.W.l -= 2;
        M_SET_WZ(R.PC.W.l + 1);
        M_CSTATE(5);
    }
}

// ---------------------------------------------------------------- Logical operation instructions
// -------------------------------- and
INLINE void and(const byte data) {
    R.AF.B.h &= data;
    R.AF.B.l =
        ZSPTable[R.AF.B.h] |
        M_XY_FLAG(R.AF.B.h) |
        H_FLAG;
}

static void and_n(void)   { and(read_mem_opcode()); }

static void and_b(void)   { and(R.BC.B.h); }
static void and_c(void)   { and(R.BC.B.l); }
static void and_d(void)   { and(R.DE.B.h); }
static void and_e(void)   { and(R.DE.B.l); }
static void and_h(void)   { and(R.HL.B.h); }
static void and_l(void)   { and(R.HL.B.l); }
static void and_ihl(void) { and(M_RD_HL); }
static void and_a(void)   { and(R.AF.B.h); }

static void and_iix(void) { and(read_mem_ix()); }
static void and_iiy(void) { and(read_mem_iy()); }

static void and_ixh(void) { and(R.IX.B.h); }
static void and_ixl(void) { and(R.IX.B.l); }

static void and_iyh(void) { and(R.IY.B.h); }
static void and_iyl(void) { and(R.IY.B.l); }

// -------------------------------- xor

INLINE void xor (const byte data) {
    R.AF.B.h ^= data;
    R.AF.B.l =
        ZSPTable[R.AF.B.h] |
        M_XY_FLAG(R.AF.B.h);
}

static void xor_n(void)   { xor(read_mem_opcode()); }

static void xor_b(void)   { xor(R.BC.B.h); }
static void xor_c(void)   { xor(R.BC.B.l); }
static void xor_d(void)   { xor(R.DE.B.h); }
static void xor_e(void)   { xor(R.DE.B.l); }
static void xor_h(void)   { xor(R.HL.B.h); }
static void xor_l(void)   { xor(R.HL.B.l); }
static void xor_ihl(void) { xor(M_RD_HL); }
static void xor_a(void)   { xor(R.AF.B.h); }

static void xor_iix(void) { xor(read_mem_ix()); }
static void xor_iiy(void) { xor(read_mem_iy()); }

static void xor_ixh(void) { xor(R.IX.B.h); }
static void xor_ixl(void) { xor(R.IX.B.l); }

static void xor_iyh(void) { xor(R.IY.B.h); }
static void xor_iyl(void) { xor(R.IY.B.l); }

// -------------------------------- or

INLINE void or(const byte data) {
    R.AF.B.h |= data;
    R.AF.B.l =
        ZSPTable[R.AF.B.h] |
        M_XY_FLAG(R.AF.B.h);
}

static void or_n(void)   { or(read_mem_opcode()); }

static void or_b(void)   { or(R.BC.B.h); }
static void or_c(void)   { or(R.BC.B.l); }
static void or_d(void)   { or(R.DE.B.h); }
static void or_e(void)   { or(R.DE.B.l); }
static void or_h(void)   { or(R.HL.B.h); }
static void or_l(void)   { or(R.HL.B.l); }
static void or_ihl(void) { or(M_RD_HL); }
static void or_a(void)   { or(R.AF.B.h); }

static void or_iix(void) { or(read_mem_ix()); }
static void or_iiy(void) { or(read_mem_iy()); }

static void or_ixh(void) { or(R.IX.B.h); }
static void or_ixl(void) { or(R.IX.B.l); }

static void or_iyh(void) { or(R.IY.B.h); }
static void or_iyl(void) { or(R.IY.B.l); }

// -------------------------------- cpl (not)
static void cpl(void)
{
    R.AF.B.h ^= 0xff;
    R.AF.B.l =
#ifdef Z80_EMU_XY
        (R.AF.B.l & ~(X_FLAG | Y_FLAG)) |
        M_XY_FLAG(R.AF.B.h) |
#else
        R.AF.B.l |
#endif
        H_FLAG |
        N_FLAG;
}

// ---------------------------------------------------------------- Rotate operation instructions
// -------------------------------- rlc

static void rlca(void)
{
    R.AF.B.h = (R.AF.B.h << 1) | ((R.AF.B.h & 0x80) >> 7);
    R.AF.B.l =
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG)) |
        M_XY_FLAG(R.AF.B.h) |
        (0 & (H_FLAG | N_FLAG)) |
        (R.AF.B.h & C_FLAG);
}

#define M_RLC(Reg) do {                                 \
    byte c = (Reg) >> (7 - C_FLAG_SHIFT);               \
    Reg = ((Reg) << 1) | c;                             \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
} while (FALSE)

INLINE byte rlc_inn(word addr)
{
    byte data = M_RDMEM(addr);
    M_RLC(data);
    M_WRMEM(addr, data);
    return data;
}

static void rlc_b(void)   { M_RLC(  R.BC.B.h); }
static void rlc_c(void)   { M_RLC(  R.BC.B.l); }
static void rlc_d(void)   { M_RLC(  R.DE.B.h); }
static void rlc_e(void)   { M_RLC(  R.DE.B.l); }
static void rlc_h(void)   { M_RLC(  R.HL.B.h); }
static void rlc_l(void)   { M_RLC(  R.HL.B.l); }
static void rlc_ihl(void) { rlc_inn(R.HL.W.l); }
static void rlc_a(void)   { M_RLC(  R.AF.B.h); }

static void rlc_iix_b(void) { R.BC.B.h = rlc_inn(make_ea_ix()); }
static void rlc_iix_c(void) { R.BC.B.l = rlc_inn(make_ea_ix()); }
static void rlc_iix_d(void) { R.DE.B.h = rlc_inn(make_ea_ix()); }
static void rlc_iix_e(void) { R.DE.B.l = rlc_inn(make_ea_ix()); }
static void rlc_iix_h(void) { R.HL.B.h = rlc_inn(make_ea_ix()); }
static void rlc_iix_l(void) { R.HL.B.l = rlc_inn(make_ea_ix()); }
static void rlc_iix(void)   {            rlc_inn(make_ea_ix()); }
static void rlc_iix_a(void) { R.AF.B.h = rlc_inn(make_ea_ix()); }

static void rlc_iiy_b(void) { R.BC.B.h = rlc_inn(make_ea_iy()); }
static void rlc_iiy_c(void) { R.BC.B.l = rlc_inn(make_ea_iy()); }
static void rlc_iiy_d(void) { R.DE.B.h = rlc_inn(make_ea_iy()); }
static void rlc_iiy_e(void) { R.DE.B.l = rlc_inn(make_ea_iy()); }
static void rlc_iiy_h(void) { R.HL.B.h = rlc_inn(make_ea_iy()); }
static void rlc_iiy_l(void) { R.HL.B.l = rlc_inn(make_ea_iy()); }
static void rlc_iiy(void)   {            rlc_inn(make_ea_iy()); }
static void rlc_iiy_a(void) { R.AF.B.h = rlc_inn(make_ea_iy()); }

// -------------------------------- rrc

static void rrca(void)
{
    byte val = R.AF.B.h;
    R.AF.B.h = (val >> 1) | (val << 7);
    R.AF.B.l =
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG)) |
        M_XY_FLAG(R.AF.B.h) |
        (0 & (H_FLAG | N_FLAG)) |
        (val & C_FLAG);
}

#define M_RRC(Reg) do {                                 \
    byte c = Reg & C_FLAG;                              \
    Reg = (Reg >> 1) | (c << (7 - C_FLAG_SHIFT));       \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
} while (FALSE)

INLINE byte rrc_inn(word addr)
{
    byte data = M_RDMEM(addr);
    M_RRC(data);
    M_WRMEM(addr, data);
    return data;
}

static void rrc_b(void)   { M_RRC(  R.BC.B.h); }
static void rrc_c(void)   { M_RRC(  R.BC.B.l); }
static void rrc_d(void)   { M_RRC(  R.DE.B.h); }
static void rrc_e(void)   { M_RRC(  R.DE.B.l); }
static void rrc_h(void)   { M_RRC(  R.HL.B.h); }
static void rrc_l(void)   { M_RRC(  R.HL.B.l); }
static void rrc_ihl(void) { rrc_inn(R.HL.W.l); }
static void rrc_a(void)   { M_RRC(  R.AF.B.h); }

static void rrc_iix_b(void) { R.BC.B.h = rrc_inn(make_ea_ix()); }
static void rrc_iix_c(void) { R.BC.B.l = rrc_inn(make_ea_ix()); }
static void rrc_iix_d(void) { R.DE.B.h = rrc_inn(make_ea_ix()); }
static void rrc_iix_e(void) { R.DE.B.l = rrc_inn(make_ea_ix()); }
static void rrc_iix_h(void) { R.HL.B.h = rrc_inn(make_ea_ix()); }
static void rrc_iix_l(void) { R.HL.B.l = rrc_inn(make_ea_ix()); }
static void rrc_iix(void)   {            rrc_inn(make_ea_ix()); }
static void rrc_iix_a(void) { R.AF.B.h = rrc_inn(make_ea_ix()); }

static void rrc_iiy_b(void) { R.BC.B.h = rrc_inn(make_ea_iy()); }
static void rrc_iiy_c(void) { R.BC.B.l = rrc_inn(make_ea_iy()); }
static void rrc_iiy_d(void) { R.DE.B.h = rrc_inn(make_ea_iy()); }
static void rrc_iiy_e(void) { R.DE.B.l = rrc_inn(make_ea_iy()); }
static void rrc_iiy_h(void) { R.HL.B.h = rrc_inn(make_ea_iy()); }
static void rrc_iiy_l(void) { R.HL.B.l = rrc_inn(make_ea_iy()); }
static void rrc_iiy(void)   {            rrc_inn(make_ea_iy()); }
static void rrc_iiy_a(void) { R.AF.B.h = rrc_inn(make_ea_iy()); }

// -------------------------------- rl

static void rla(void) 
{ 
    byte c1 = R.AF.B.l & C_FLAG;
    byte c2 = (R.AF.B.h & 0x80) >> (7 - C_FLAG_SHIFT);
    R.AF.B.h = (R.AF.B.h << 1) | c1;
    R.AF.B.l =
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG)) |
        M_XY_FLAG(R.AF.B.h) |
        (0 & (H_FLAG | N_FLAG)) |
        c2;
}

#define M_RL(Reg) do {                                  \
    byte c = Reg >> (7 - C_FLAG_SHIFT);                 \
    Reg = (Reg << 1) | (R.AF.B.l & 1);                  \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
} while (FALSE)

INLINE byte rl_inn(const word addr) { 
    byte data = M_RDMEM(addr);
    M_RL(data);
    M_WRMEM(addr, data);
    return data;
}

static void rl_b(void)   { M_RL(  R.BC.B.h); }
static void rl_c(void)   { M_RL(  R.BC.B.l); }
static void rl_d(void)   { M_RL(  R.DE.B.h); }
static void rl_e(void)   { M_RL(  R.DE.B.l); }
static void rl_h(void)   { M_RL(  R.HL.B.h); }
static void rl_l(void)   { M_RL(  R.HL.B.l); }
static void rl_ihl(void) { rl_inn(R.HL.W.l); }
static void rl_a(void)   { M_RL(  R.AF.B.h); }

static void rl_iix_b(void) { R.BC.B.h = rl_inn(make_ea_ix()); }
static void rl_iix_c(void) { R.BC.B.l = rl_inn(make_ea_ix()); }
static void rl_iix_d(void) { R.DE.B.h = rl_inn(make_ea_ix()); }
static void rl_iix_e(void) { R.DE.B.l = rl_inn(make_ea_ix()); }
static void rl_iix_h(void) { R.HL.B.h = rl_inn(make_ea_ix()); }
static void rl_iix_l(void) { R.HL.B.l = rl_inn(make_ea_ix()); }
static void rl_iix(void)   {            rl_inn(make_ea_ix()); }
static void rl_iix_a(void) { R.AF.B.h = rl_inn(make_ea_ix()); }

static void rl_iiy_b(void) { R.BC.B.h = rl_inn(make_ea_iy()); }
static void rl_iiy_c(void) { R.BC.B.l = rl_inn(make_ea_iy()); }
static void rl_iiy_d(void) { R.DE.B.h = rl_inn(make_ea_iy()); }
static void rl_iiy_e(void) { R.DE.B.l = rl_inn(make_ea_iy()); }
static void rl_iiy_h(void) { R.HL.B.h = rl_inn(make_ea_iy()); }
static void rl_iiy_l(void) { R.HL.B.l = rl_inn(make_ea_iy()); }
static void rl_iiy(void)   {            rl_inn(make_ea_iy()); }
static void rl_iiy_a(void) { R.AF.B.h = rl_inn(make_ea_iy()); }

// -------------------------------- rr

static void rra(void)
{
    byte c1 = R.AF.B.l & C_FLAG;
    byte c2 = R.AF.B.h & C_FLAG;
    R.AF.B.h = (R.AF.B.h >> 1) | (c1 << (7 - C_FLAG_SHIFT));
    R.AF.B.l =
        (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG)) |
        M_XY_FLAG(R.AF.B.h) |
        (0 & (H_FLAG | N_FLAG)) |
        c2;
}

#define M_RR(Reg) do {                                  \
    byte c = Reg & C_FLAG;                              \
    Reg = (Reg >> 1) | (R.AF.B.l << 7);                 \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
} while (FALSE)

INLINE byte rr_inn(const word addr) 
{
    byte data = M_RDMEM(addr);
    M_RR(data);
    M_WRMEM(addr, data);
    return data;
}

static void rr_b(void)   { M_RR(  R.BC.B.h); }
static void rr_c(void)   { M_RR(  R.BC.B.l); }
static void rr_d(void)   { M_RR(  R.DE.B.h); }
static void rr_e(void)   { M_RR(  R.DE.B.l); }
static void rr_h(void)   { M_RR(  R.HL.B.h); }
static void rr_l(void)   { M_RR(  R.HL.B.l); }
static void rr_ihl(void) { rr_inn(R.HL.W.l); }
static void rr_a(void)   { M_RR(  R.AF.B.h); }

static void rr_iix_b(void) { R.BC.B.h = rr_inn(make_ea_ix()); }
static void rr_iix_c(void) { R.BC.B.l = rr_inn(make_ea_ix()); }
static void rr_iix_d(void) { R.DE.B.h = rr_inn(make_ea_ix()); }
static void rr_iix_e(void) { R.DE.B.l = rr_inn(make_ea_ix()); }
static void rr_iix_h(void) { R.HL.B.h = rr_inn(make_ea_ix()); }
static void rr_iix_l(void) { R.HL.B.l = rr_inn(make_ea_ix()); }
static void rr_iix(void)   {            rr_inn(make_ea_ix()); }
static void rr_iix_a(void) { R.AF.B.h = rr_inn(make_ea_ix()); }

static void rr_iiy_b(void) { R.BC.B.h = rr_inn(make_ea_iy()); }
static void rr_iiy_c(void) { R.BC.B.l = rr_inn(make_ea_iy()); }
static void rr_iiy_d(void) { R.DE.B.h = rr_inn(make_ea_iy()); }
static void rr_iiy_e(void) { R.DE.B.l = rr_inn(make_ea_iy()); }
static void rr_iiy_h(void) { R.HL.B.h = rr_inn(make_ea_iy()); }
static void rr_iiy_l(void) { R.HL.B.l = rr_inn(make_ea_iy()); }
static void rr_iiy(void)   {            rr_inn(make_ea_iy()); }
static void rr_iiy_a(void) { R.AF.B.h = rr_inn(make_ea_iy()); }

// -------------------------------- rrd, rld

static void rld(void)
{
    byte data = M_RDMEM(R.HL.W.l);
    M_SET_WZ(R.HL.W.l + 1);
    M_WRMEM(R.HL.W.l, (data << 4) | (R.AF.B.h & 0x0f));
    R.AF.B.h = (R.AF.B.h & 0xf0) | (data >> 4);
    R.AF.B.l =
        ZSPTable[R.AF.B.h] |
        M_XY_FLAG(R.AF.B.h) |
        (0 & (H_FLAG | N_FLAG)) |
        (R.AF.B.l & C_FLAG);
}

static void rrd(void)
{
    byte data = M_RDMEM(R.HL.W.l);
    M_SET_WZ(R.HL.W.l + 1);
    M_WRMEM(R.HL.W.l, (data >> 4) | (R.AF.B.h << 4));
    R.AF.B.h = (R.AF.B.h & 0xF0) | (data & 0x0f);
    R.AF.B.l =
        ZSPTable[R.AF.B.h] |
        M_XY_FLAG(R.AF.B.h) |
        (0 & (H_FLAG | N_FLAG)) |
        (R.AF.B.l & C_FLAG);
}

// ---------------------------------------------------------------- Shift operation instructions
// -------------------------------- sla

#define M_SLA(Reg) do {                                 \
    byte c = Reg >> (7 - C_FLAG_SHIFT);                 \
    Reg <<= 1;                                          \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
}while (FALSE)

INLINE byte sla_inn(const word addr)
{
    byte data = M_RDMEM(addr);
    M_SLA(data);
    M_WRMEM(addr, data);
    return data;
}

static void sla_b(void)   { M_SLA(  R.BC.B.h); }
static void sla_c(void)   { M_SLA(  R.BC.B.l); }
static void sla_d(void)   { M_SLA(  R.DE.B.h); }
static void sla_e(void)   { M_SLA(  R.DE.B.l); }
static void sla_h(void)   { M_SLA(  R.HL.B.h); }
static void sla_l(void)   { M_SLA(  R.HL.B.l); }
static void sla_ihl(void) { sla_inn(R.HL.W.l); }
static void sla_a(void)   { M_SLA(  R.AF.B.h); }

static void sla_iix_b(void) { R.BC.B.h = sla_inn(make_ea_ix()); }
static void sla_iix_c(void) { R.BC.B.l = sla_inn(make_ea_ix()); }
static void sla_iix_d(void) { R.DE.B.h = sla_inn(make_ea_ix()); }
static void sla_iix_e(void) { R.DE.B.l = sla_inn(make_ea_ix()); }
static void sla_iix_h(void) { R.HL.B.h = sla_inn(make_ea_ix()); }
static void sla_iix_l(void) { R.HL.B.l = sla_inn(make_ea_ix()); }
static void sla_iix(void)   {            sla_inn(make_ea_ix()); }
static void sla_iix_a(void) { R.AF.B.h = sla_inn(make_ea_ix()); }

static void sla_iiy_b(void) { R.BC.B.h = sla_inn(make_ea_iy()); }
static void sla_iiy_c(void) { R.BC.B.l = sla_inn(make_ea_iy()); }
static void sla_iiy_d(void) { R.DE.B.h = sla_inn(make_ea_iy()); }
static void sla_iiy_e(void) { R.DE.B.l = sla_inn(make_ea_iy()); }
static void sla_iiy_h(void) { R.HL.B.h = sla_inn(make_ea_iy()); }
static void sla_iiy_l(void) { R.HL.B.l = sla_inn(make_ea_iy()); }
static void sla_iiy(void)   {            sla_inn(make_ea_iy()); }
static void sla_iiy_a(void) { R.AF.B.h = sla_inn(make_ea_iy()); }

// -------------------------------- sra

#define M_SRA(Reg) do {                                 \
    byte c = Reg & C_FLAG;                              \
    Reg = (Reg >> 1) | (Reg & 0x80);                    \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
} while (FALSE)

INLINE byte sra_inn(const word addr)
{
    byte data = M_RDMEM(addr);
    M_SRA(data);
    M_WRMEM(addr, data);
    return data;
}

static void sra_b(void)   { M_SRA(  R.BC.B.h); }
static void sra_c(void)   { M_SRA(  R.BC.B.l); }
static void sra_d(void)   { M_SRA(  R.DE.B.h); }
static void sra_e(void)   { M_SRA(  R.DE.B.l); }
static void sra_h(void)   { M_SRA(  R.HL.B.h); }
static void sra_l(void)   { M_SRA(  R.HL.B.l); }
static void sra_ihl(void) { sra_inn(R.HL.W.l); }
static void sra_a(void)   { M_SRA(  R.AF.B.h); }

static void sra_iix_b(void) { R.BC.B.h = sra_inn(make_ea_ix()); }
static void sra_iix_c(void) { R.BC.B.l = sra_inn(make_ea_ix()); }
static void sra_iix_d(void) { R.DE.B.h = sra_inn(make_ea_ix()); }
static void sra_iix_e(void) { R.DE.B.l = sra_inn(make_ea_ix()); }
static void sra_iix_h(void) { R.HL.B.h = sra_inn(make_ea_ix()); }
static void sra_iix_l(void) { R.HL.B.l = sra_inn(make_ea_ix()); }
static void sra_iix(void)   {            sra_inn(make_ea_ix()); }
static void sra_iix_a(void) { R.AF.B.h = sra_inn(make_ea_ix()); }

static void sra_iiy_b(void) { R.BC.B.h = sra_inn(make_ea_iy()); }
static void sra_iiy_c(void) { R.BC.B.l = sra_inn(make_ea_iy()); }
static void sra_iiy_d(void) { R.DE.B.h = sra_inn(make_ea_iy()); }
static void sra_iiy_e(void) { R.DE.B.l = sra_inn(make_ea_iy()); }
static void sra_iiy_h(void) { R.HL.B.h = sra_inn(make_ea_iy()); }
static void sra_iiy_l(void) { R.HL.B.l = sra_inn(make_ea_iy()); }
static void sra_iiy(void)   {            sra_inn(make_ea_iy()); }
static void sra_iiy_a(void) { R.AF.B.h = sra_inn(make_ea_iy()); }

// -------------------------------- sll

#define M_SLL(Reg) do {                                 \
    byte c = Reg >> (7 - C_FLAG_SHIFT);                 \
    Reg = (Reg << 1) | 1;                               \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
} while (FALSE)

INLINE byte sll_inn(const word addr) 
{
    byte data = M_RDMEM(addr);
    M_SLL(data);
    M_WRMEM(addr, data);
    return data;
}

static void sll_b(void)   { M_SLL(  R.BC.B.h); }
static void sll_c(void)   { M_SLL(  R.BC.B.l); }
static void sll_d(void)   { M_SLL(  R.DE.B.h); }
static void sll_e(void)   { M_SLL(  R.DE.B.l); }
static void sll_h(void)   { M_SLL(  R.HL.B.h); }
static void sll_l(void)   { M_SLL(  R.HL.B.l); }
static void sll_ihl(void) { sll_inn(R.HL.W.l); }
static void sll_a(void)   { M_SLL(  R.AF.B.h); }

static void sll_iix_b(void) { R.BC.B.h = sll_inn(make_ea_ix()); }
static void sll_iix_c(void) { R.BC.B.l = sll_inn(make_ea_ix()); }
static void sll_iix_d(void) { R.DE.B.h = sll_inn(make_ea_ix()); }
static void sll_iix_e(void) { R.DE.B.l = sll_inn(make_ea_ix()); }
static void sll_iix_h(void) { R.HL.B.h = sll_inn(make_ea_ix()); }
static void sll_iix_l(void) { R.HL.B.l = sll_inn(make_ea_ix()); }
static void sll_iix(void)   {            sll_inn(make_ea_ix()); }
static void sll_iix_a(void) { R.AF.B.h = sll_inn(make_ea_ix()); }

static void sll_iiy_b(void) { R.BC.B.h = sll_inn(make_ea_iy()); }
static void sll_iiy_c(void) { R.BC.B.l = sll_inn(make_ea_iy()); }
static void sll_iiy_d(void) { R.DE.B.h = sll_inn(make_ea_iy()); }
static void sll_iiy_e(void) { R.DE.B.l = sll_inn(make_ea_iy()); }
static void sll_iiy_h(void) { R.HL.B.h = sll_inn(make_ea_iy()); }
static void sll_iiy_l(void) { R.HL.B.l = sll_inn(make_ea_iy()); }
static void sll_iiy(void)   {            sll_inn(make_ea_iy()); }
static void sll_iiy_a(void) { R.AF.B.h = sll_inn(make_ea_iy()); }

// -------------------------------- srl

#define M_SRL(Reg) do {                                 \
    byte c = Reg & C_FLAG;                              \
    Reg >>= 1;                                          \
    R.AF.B.l =                                          \
        ZSPTable[Reg] |                                 \
        M_XY_FLAG(Reg) |                                \
        (0 & (H_FLAG | N_FLAG)) |                       \
        c;                                              \
} while (FALSE)

INLINE byte srl_inn(const word addr)
{
    byte data = M_RDMEM(addr);
    M_SRL(data);
    M_WRMEM(addr, data);
    return data;
}

static void srl_b(void)   { M_SRL(  R.BC.B.h); }
static void srl_c(void)   { M_SRL(  R.BC.B.l); }
static void srl_d(void)   { M_SRL(  R.DE.B.h); }
static void srl_e(void)   { M_SRL(  R.DE.B.l); }
static void srl_h(void)   { M_SRL(  R.HL.B.h); }
static void srl_l(void)   { M_SRL(  R.HL.B.l); }
static void srl_ihl(void) { srl_inn(R.HL.W.l); }
static void srl_a(void)   { M_SRL(  R.AF.B.h); }

static void srl_iix_b(void) { R.BC.B.h = srl_inn(make_ea_ix()); }
static void srl_iix_c(void) { R.BC.B.l = srl_inn(make_ea_ix()); }
static void srl_iix_d(void) { R.DE.B.h = srl_inn(make_ea_ix()); }
static void srl_iix_e(void) { R.DE.B.l = srl_inn(make_ea_ix()); }
static void srl_iix_h(void) { R.HL.B.h = srl_inn(make_ea_ix()); }
static void srl_iix_l(void) { R.HL.B.l = srl_inn(make_ea_ix()); }
static void srl_iix(void)   {            srl_inn(make_ea_ix()); }
static void srl_iix_a(void) { R.AF.B.h = srl_inn(make_ea_ix()); }

static void srl_iiy_b(void) { R.BC.B.h = srl_inn(make_ea_iy()); }
static void srl_iiy_c(void) { R.BC.B.l = srl_inn(make_ea_iy()); }
static void srl_iiy_d(void) { R.DE.B.h = srl_inn(make_ea_iy()); }
static void srl_iiy_e(void) { R.DE.B.l = srl_inn(make_ea_iy()); }
static void srl_iiy_h(void) { R.HL.B.h = srl_inn(make_ea_iy()); }
static void srl_iiy_l(void) { R.HL.B.l = srl_inn(make_ea_iy()); }
static void srl_iiy(void)   {            srl_inn(make_ea_iy()); }
static void srl_iiy_a(void) { R.AF.B.h = srl_inn(make_ea_iy()); }

// ---------------------------------------------------------------- Bit instructions
// -------------------------------- bit

INLINE void bit_b_rr(const byte bit, const byte data)
{
    byte tmp = data & (1 << bit);
    R.AF.B.l =
        (R.AF.B.l & C_FLAG) |
        H_FLAG |
        (0 & N_FLAG) |
        ZSPBitTable[tmp] |
        M_XY_FLAG(data);
}

INLINE void bit_b_ihl(const byte bit)
{
    word addr = R.HL.W.l;
    byte tmp = M_RDMEM(addr) & (1 << bit);
    R.AF.B.l =
        (R.AF.B.l & C_FLAG) |
        H_FLAG |
        (0 & N_FLAG) |
        ZSPBitTable[tmp] |
        M_XY_FLAG(R.WZ.B.h);
}

INLINE void bit_b_inn(const byte bit, const word addr)
{
    byte tmp = M_RDMEM(addr) & (1 << bit);
    R.AF.B.l =
        (R.AF.B.l & C_FLAG) |
        H_FLAG |
        (0 & N_FLAG) |
        ZSPBitTable[tmp] |
        M_XY_FLAG(addr >> 8);
}

#define M_BIT_INSTRUCTIONS(Bit) \
static void bit_##Bit##_b(void)   { bit_b_rr(Bit, R.BC.B.h); }\
static void bit_##Bit##_c(void)   { bit_b_rr(Bit, R.BC.B.l); }\
static void bit_##Bit##_d(void)   { bit_b_rr(Bit, R.DE.B.h); }\
static void bit_##Bit##_e(void)   { bit_b_rr(Bit, R.DE.B.l); }\
static void bit_##Bit##_h(void)   { bit_b_rr(Bit, R.HL.B.h); }\
static void bit_##Bit##_l(void)   { bit_b_rr(Bit, R.HL.B.l); }\
static void bit_##Bit##_ihl(void) { bit_b_ihl(Bit); }\
static void bit_##Bit##_a(void)   { bit_b_rr(Bit, R.AF.B.h); }\
\
static void bit_##Bit##_iix(void) { bit_b_inn(Bit, make_ea_ix()); }\
static void bit_##Bit##_iiy(void) { bit_b_inn(Bit, make_ea_iy()); }

M_BIT_INSTRUCTIONS(0)
M_BIT_INSTRUCTIONS(1)
M_BIT_INSTRUCTIONS(2)
M_BIT_INSTRUCTIONS(3)
M_BIT_INSTRUCTIONS(4)
M_BIT_INSTRUCTIONS(5)
M_BIT_INSTRUCTIONS(6)
M_BIT_INSTRUCTIONS(7)

// -------------------------------- res

#define M_RES(Bit, Reg) do {                            \
    Reg &= ~(1 << Bit);                                 \
} while (FALSE)

INLINE byte res_b_inn(const byte bit, const word addr)
{
    byte data = M_RDMEM(addr);
    M_RES(bit, data);
    M_WRMEM(addr, data);
    return data;
}

#define M_RES_INSTRUCTIONS(Bit) \
static void res_##Bit##_b(void)     { M_RES(    Bit, R.BC.B.h); }\
static void res_##Bit##_c(void)     { M_RES(    Bit, R.BC.B.l); }\
static void res_##Bit##_d(void)     { M_RES(    Bit, R.DE.B.h); }\
static void res_##Bit##_e(void)     { M_RES(    Bit, R.DE.B.l); }\
static void res_##Bit##_h(void)     { M_RES(    Bit, R.HL.B.h); }\
static void res_##Bit##_l(void)     { M_RES(    Bit, R.HL.B.l); }\
static void res_##Bit##_ihl(void)   { res_b_inn(Bit, R.HL.W.l); }\
static void res_##Bit##_a(void)     { M_RES(    Bit, R.AF.B.h); }\
\
static void res_##Bit##_iix_b(void) { R.BC.B.h = res_b_inn(Bit, make_ea_ix()); }\
static void res_##Bit##_iix_c(void) { R.BC.B.l = res_b_inn(Bit, make_ea_ix()); }\
static void res_##Bit##_iix_d(void) { R.DE.B.h = res_b_inn(Bit, make_ea_ix()); }\
static void res_##Bit##_iix_e(void) { R.DE.B.l = res_b_inn(Bit, make_ea_ix()); }\
static void res_##Bit##_iix_h(void) { R.HL.B.h = res_b_inn(Bit, make_ea_ix()); }\
static void res_##Bit##_iix_l(void) { R.HL.B.l = res_b_inn(Bit, make_ea_ix()); }\
static void res_##Bit##_iix(void)   {            res_b_inn(Bit, make_ea_ix()); }\
static void res_##Bit##_iix_a(void) { R.AF.B.h = res_b_inn(Bit, make_ea_ix()); }\
\
static void res_##Bit##_iiy_b(void) { R.BC.B.h = res_b_inn(Bit, make_ea_iy()); }\
static void res_##Bit##_iiy_c(void) { R.BC.B.l = res_b_inn(Bit, make_ea_iy()); }\
static void res_##Bit##_iiy_d(void) { R.DE.B.h = res_b_inn(Bit, make_ea_iy()); }\
static void res_##Bit##_iiy_e(void) { R.DE.B.l = res_b_inn(Bit, make_ea_iy()); }\
static void res_##Bit##_iiy_h(void) { R.HL.B.h = res_b_inn(Bit, make_ea_iy()); }\
static void res_##Bit##_iiy_l(void) { R.HL.B.l = res_b_inn(Bit, make_ea_iy()); }\
static void res_##Bit##_iiy(void)   {            res_b_inn(Bit, make_ea_iy()); }\
static void res_##Bit##_iiy_a(void) { R.AF.B.h = res_b_inn(Bit, make_ea_iy()); }

M_RES_INSTRUCTIONS(0)
M_RES_INSTRUCTIONS(1)
M_RES_INSTRUCTIONS(2)
M_RES_INSTRUCTIONS(3)
M_RES_INSTRUCTIONS(4)
M_RES_INSTRUCTIONS(5)
M_RES_INSTRUCTIONS(6)
M_RES_INSTRUCTIONS(7)

// -------------------------------- set

#define M_SET(Bit, Reg) do {                            \
    Reg |= 1 << Bit;                                    \
} while (FALSE)

INLINE byte set_b_inn(const byte bit, const word addr)
{
    byte data = M_RDMEM(addr);
    M_SET(bit, data);
    M_WRMEM(addr, data);
    return data;
}

#define M_SET_INSTRUCTIONS(Bit) \
static void set_##Bit##_b(void)     { M_SET(    Bit, R.BC.B.h); }\
static void set_##Bit##_c(void)     { M_SET(    Bit, R.BC.B.l); }\
static void set_##Bit##_d(void)     { M_SET(    Bit, R.DE.B.h); }\
static void set_##Bit##_e(void)     { M_SET(    Bit, R.DE.B.l); }\
static void set_##Bit##_h(void)     { M_SET(    Bit, R.HL.B.h); }\
static void set_##Bit##_l(void)     { M_SET(    Bit, R.HL.B.l); }\
static void set_##Bit##_ihl(void)   { set_b_inn(Bit, R.HL.W.l); }\
static void set_##Bit##_a(void)     { M_SET(    Bit, R.AF.B.h); }\
\
static void set_##Bit##_iix_b(void) { R.BC.B.h = set_b_inn(Bit, make_ea_ix()); }\
static void set_##Bit##_iix_c(void) { R.BC.B.l = set_b_inn(Bit, make_ea_ix()); }\
static void set_##Bit##_iix_d(void) { R.DE.B.h = set_b_inn(Bit, make_ea_ix()); }\
static void set_##Bit##_iix_e(void) { R.DE.B.l = set_b_inn(Bit, make_ea_ix()); }\
static void set_##Bit##_iix_h(void) { R.HL.B.h = set_b_inn(Bit, make_ea_ix()); }\
static void set_##Bit##_iix_l(void) { R.HL.B.l = set_b_inn(Bit, make_ea_ix()); }\
static void set_##Bit##_iix(void)   {            set_b_inn(Bit, make_ea_ix()); }\
static void set_##Bit##_iix_a(void) { R.AF.B.h = set_b_inn(Bit, make_ea_ix()); }\
\
static void set_##Bit##_iiy_b(void) { R.BC.B.h = set_b_inn(Bit, make_ea_iy()); }\
static void set_##Bit##_iiy_c(void) { R.BC.B.l = set_b_inn(Bit, make_ea_iy()); }\
static void set_##Bit##_iiy_d(void) { R.DE.B.h = set_b_inn(Bit, make_ea_iy()); }\
static void set_##Bit##_iiy_e(void) { R.DE.B.l = set_b_inn(Bit, make_ea_iy()); }\
static void set_##Bit##_iiy_h(void) { R.HL.B.h = set_b_inn(Bit, make_ea_iy()); }\
static void set_##Bit##_iiy_l(void) { R.HL.B.l = set_b_inn(Bit, make_ea_iy()); }\
static void set_##Bit##_iiy(void)   {            set_b_inn(Bit, make_ea_iy()); }\
static void set_##Bit##_iiy_a(void) { R.AF.B.h = set_b_inn(Bit, make_ea_iy()); }

M_SET_INSTRUCTIONS(0)
M_SET_INSTRUCTIONS(1)
M_SET_INSTRUCTIONS(2)
M_SET_INSTRUCTIONS(3)
M_SET_INSTRUCTIONS(4)
M_SET_INSTRUCTIONS(5)
M_SET_INSTRUCTIONS(6)
M_SET_INSTRUCTIONS(7)

// ---------------------------------------------------------------- Emulator instructions

static void no_op(void)
{
    --R.PC.W.l;
}
static void emu_rom1(void) { exec_rom1(&R); }
static void emu_rom2(void) { exec_rom2(&R); }
static void emu_ram(void)  { exec_ram(&R); }
static void patch(void) { Z80_Patch(&R); }

// ---------------------------------------------------------------- T-Cycles
static unsigned cycles_main[256] = {
4,10,7,6,4,4,7,4,
4,11,7,6,4,4,7,4,
8,10,7,6,4,4,7,4,
7,11,7,6,4,4,7,4,
7,10,16,6,4,4,7,4,
7,11,16,6,4,4,7,4,
7,10,13,6,11,11,10,4,
7,11,13,6,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
7,7,7,7,7,7,4,7,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
4,4,4,4,4,4,7,4,
5,10,10,10,10,11,7,11,
5,4,10,0,10,10,7,11,
5,10,10,11,10,11,7,11,
5,4,10,11,10,0,7,11,
5,10,10,19,10,11,7,11,
5,4,10,4,10,0,7,11,
5,10,10,4,10,11,7,11,
5,6,10,4,10,0,7,11
};

static unsigned cycles_cb[256] = {
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,12,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8,
8,8,8,8,8,8,15,8
};
static unsigned cycles_xx_cb[] = {
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,
};
static unsigned cycles_xx[256] = {
0,0,0,0,0,0,0,0,
0,15,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,15,0,0,0,0,0,0,
0,14,20,10,9,9,9,0,
0,15,20,10,9,9,9,0,
0,0,0,0,23,23,19,0,
0,15,0,0,0,0,0,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
9,9,9,9,9,9,9,9,
9,9,9,9,9,9,9,9,
19,19,19,19,19,19,19,19,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,9,9,19,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,14, 0, 23, 0,15, 0, 0,
0,8,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,10, 0, 0, 0, 0, 0, 0
};
static unsigned cycles_ed[256] = {
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8,18,
12,12,15,20, 8, 8, 8,18,
12,12,15,20, 8, 8, 8, 8,
12,12,15,20, 8, 8, 8, 8,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
16,16,16,16, 0, 0, 0, 0,
16,16,16,16, 0, 0, 0, 0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0
};

// ---------------------------------------------------------------- Instruction tables
static opcode_fn opcode_dd_cb[256] = {
rlc_iix_b,   rlc_iix_c,   rlc_iix_d,   rlc_iix_e,   rlc_iix_h,   rlc_iix_l,   rlc_iix,   rlc_iix_a,
rrc_iix_b,   rrc_iix_c,   rrc_iix_d,   rrc_iix_e,   rrc_iix_h,   rrc_iix_l,   rrc_iix,   rrc_iix_a,
rl_iix_b,    rl_iix_c,    rl_iix_d,    rl_iix_e,    rl_iix_h,    rl_iix_l,    rl_iix,    rl_iix_a,
rr_iix_b,    rr_iix_c,    rr_iix_d,    rr_iix_e,    rr_iix_h,    rr_iix_l,    rr_iix,    rr_iix_a,
sla_iix_b,   sla_iix_c,   sla_iix_d,   sla_iix_e,   sla_iix_h,   sla_iix_l,   sla_iix,   sla_iix_a,
sra_iix_b,   sra_iix_c,   sra_iix_d,   sra_iix_e,   sra_iix_h,   sra_iix_l,   sra_iix,   sra_iix_a,
sll_iix_b,   sll_iix_c,   sll_iix_d,   sll_iix_e,   sll_iix_h,   sll_iix_l,   sll_iix,   sll_iix_a,
srl_iix_b,   srl_iix_c,   srl_iix_d,   srl_iix_e,   srl_iix_h,   srl_iix_l,   srl_iix,   srl_iix_a,
bit_0_iix,   bit_0_iix,   bit_0_iix,   bit_0_iix,   bit_0_iix,   bit_0_iix,   bit_0_iix, bit_0_iix,
bit_1_iix,   bit_1_iix,   bit_1_iix,   bit_1_iix,   bit_1_iix,   bit_1_iix,   bit_1_iix, bit_1_iix,
bit_2_iix,   bit_2_iix,   bit_2_iix,   bit_2_iix,   bit_2_iix,   bit_2_iix,   bit_2_iix, bit_2_iix,
bit_3_iix,   bit_3_iix,   bit_3_iix,   bit_3_iix,   bit_3_iix,   bit_3_iix,   bit_3_iix, bit_3_iix,
bit_4_iix,   bit_4_iix,   bit_4_iix,   bit_4_iix,   bit_4_iix,   bit_4_iix,   bit_4_iix, bit_4_iix,
bit_5_iix,   bit_5_iix,   bit_5_iix,   bit_5_iix,   bit_5_iix,   bit_5_iix,   bit_5_iix, bit_5_iix,
bit_6_iix,   bit_6_iix,   bit_6_iix,   bit_6_iix,   bit_6_iix,   bit_6_iix,   bit_6_iix, bit_6_iix,
bit_7_iix,   bit_7_iix,   bit_7_iix,   bit_7_iix,   bit_7_iix,   bit_7_iix,   bit_7_iix, bit_7_iix,
res_0_iix_b, res_0_iix_c, res_0_iix_d, res_0_iix_e, res_0_iix_h, res_0_iix_l, res_0_iix, res_0_iix_a,
res_1_iix_b, res_1_iix_c, res_1_iix_d, res_1_iix_e, res_1_iix_h, res_1_iix_l, res_1_iix, res_1_iix_a,
res_2_iix_b, res_2_iix_c, res_2_iix_d, res_2_iix_e, res_2_iix_h, res_2_iix_l, res_2_iix, res_2_iix_a,
res_3_iix_b, res_3_iix_c, res_3_iix_d, res_3_iix_e, res_3_iix_h, res_3_iix_l, res_3_iix, res_3_iix_a,
res_4_iix_b, res_4_iix_c, res_4_iix_d, res_4_iix_e, res_4_iix_h, res_4_iix_l, res_4_iix, res_4_iix_a,
res_5_iix_b, res_5_iix_c, res_5_iix_d, res_5_iix_e, res_5_iix_h, res_5_iix_l, res_5_iix, res_5_iix_a,
res_6_iix_b, res_6_iix_c, res_6_iix_d, res_6_iix_e, res_6_iix_h, res_6_iix_l, res_6_iix, res_6_iix_a,
res_7_iix_b, res_7_iix_c, res_7_iix_d, res_7_iix_e, res_7_iix_h, res_7_iix_l, res_7_iix, res_7_iix_a,
set_0_iix_b, set_0_iix_c, set_0_iix_d, set_0_iix_e, set_0_iix_h, set_0_iix_l, set_0_iix, set_0_iix_a,
set_1_iix_b, set_1_iix_c, set_1_iix_d, set_1_iix_e, set_1_iix_h, set_1_iix_l, set_1_iix, set_1_iix_a,
set_2_iix_b, set_2_iix_c, set_2_iix_d, set_2_iix_e, set_2_iix_h, set_2_iix_l, set_2_iix, set_2_iix_a,
set_3_iix_b, set_3_iix_c, set_3_iix_d, set_3_iix_e, set_3_iix_h, set_3_iix_l, set_3_iix, set_3_iix_a,
set_4_iix_b, set_4_iix_c, set_4_iix_d, set_4_iix_e, set_4_iix_h, set_4_iix_l, set_4_iix, set_4_iix_a,
set_5_iix_b, set_5_iix_c, set_5_iix_d, set_5_iix_e, set_5_iix_h, set_5_iix_l, set_5_iix, set_5_iix_a,
set_6_iix_b, set_6_iix_c, set_6_iix_d, set_6_iix_e, set_6_iix_h, set_6_iix_l, set_6_iix, set_6_iix_a,
set_7_iix_b, set_7_iix_c, set_7_iix_d, set_7_iix_e, set_7_iix_h, set_7_iix_l, set_7_iix, set_7_iix_a,
};

static opcode_fn opcode_fd_cb[256] = {
rlc_iiy_b,   rlc_iiy_c,   rlc_iiy_d,   rlc_iiy_e,   rlc_iiy_h,   rlc_iiy_l,   rlc_iiy,   rlc_iiy_a,
rrc_iiy_b,   rrc_iiy_c,   rrc_iiy_d,   rrc_iiy_e,   rrc_iiy_h,   rrc_iiy_l,   rrc_iiy,   rrc_iiy_a,
rl_iiy_b,    rl_iiy_c,    rl_iiy_d,    rl_iiy_e,    rl_iiy_h,    rl_iiy_l,    rl_iiy,    rl_iiy_a,
rr_iiy_b,    rr_iiy_c,    rr_iiy_d,    rr_iiy_e,    rr_iiy_h,    rr_iiy_l,    rr_iiy,    rr_iiy_a,
sla_iiy_b,   sla_iiy_c,   sla_iiy_d,   sla_iiy_e,   sla_iiy_h,   sla_iiy_l,   sla_iiy,   sla_iiy_a,
sra_iiy_b,   sra_iiy_c,   sra_iiy_d,   sra_iiy_e,   sra_iiy_h,   sra_iiy_l,   sra_iiy,   sra_iiy_a,
sll_iiy_b,   sll_iiy_c,   sll_iiy_d,   sll_iiy_e,   sll_iiy_h,   sll_iiy_l,   sll_iiy,   sll_iiy_a,
srl_iiy_b,   srl_iiy_c,   srl_iiy_d,   srl_iiy_e,   srl_iiy_h,   srl_iiy_l,   srl_iiy,   srl_iiy_a,
bit_0_iiy,   bit_0_iiy,   bit_0_iiy,   bit_0_iiy,   bit_0_iiy,   bit_0_iiy,   bit_0_iiy, bit_0_iiy,
bit_1_iiy,   bit_1_iiy,   bit_1_iiy,   bit_1_iiy,   bit_1_iiy,   bit_1_iiy,   bit_1_iiy, bit_1_iiy,
bit_2_iiy,   bit_2_iiy,   bit_2_iiy,   bit_2_iiy,   bit_2_iiy,   bit_2_iiy,   bit_2_iiy, bit_2_iiy,
bit_3_iiy,   bit_3_iiy,   bit_3_iiy,   bit_3_iiy,   bit_3_iiy,   bit_3_iiy,   bit_3_iiy, bit_3_iiy,
bit_4_iiy,   bit_4_iiy,   bit_4_iiy,   bit_4_iiy,   bit_4_iiy,   bit_4_iiy,   bit_4_iiy, bit_4_iiy,
bit_5_iiy,   bit_5_iiy,   bit_5_iiy,   bit_5_iiy,   bit_5_iiy,   bit_5_iiy,   bit_5_iiy, bit_5_iiy,
bit_6_iiy,   bit_6_iiy,   bit_6_iiy,   bit_6_iiy,   bit_6_iiy,   bit_6_iiy,   bit_6_iiy, bit_6_iiy,
bit_7_iiy,   bit_7_iiy,   bit_7_iiy,   bit_7_iiy,   bit_7_iiy,   bit_7_iiy,   bit_7_iiy, bit_7_iiy,
res_0_iiy_b, res_0_iiy_c, res_0_iiy_d, res_0_iiy_e, res_0_iiy_h, res_0_iiy_l, res_0_iiy, res_0_iiy_a,
res_1_iiy_b, res_1_iiy_c, res_1_iiy_d, res_1_iiy_e, res_1_iiy_h, res_1_iiy_l, res_1_iiy, res_1_iiy_a,
res_2_iiy_b, res_2_iiy_c, res_2_iiy_d, res_2_iiy_e, res_2_iiy_h, res_2_iiy_l, res_2_iiy, res_2_iiy_a,
res_3_iiy_b, res_3_iiy_c, res_3_iiy_d, res_3_iiy_e, res_3_iiy_h, res_3_iiy_l, res_3_iiy, res_3_iiy_a,
res_4_iiy_b, res_4_iiy_c, res_4_iiy_d, res_4_iiy_e, res_4_iiy_h, res_4_iiy_l, res_4_iiy, res_4_iiy_a,
res_5_iiy_b, res_5_iiy_c, res_5_iiy_d, res_5_iiy_e, res_5_iiy_h, res_5_iiy_l, res_5_iiy, res_5_iiy_a,
res_6_iiy_b, res_6_iiy_c, res_6_iiy_d, res_6_iiy_e, res_6_iiy_h, res_6_iiy_l, res_6_iiy, res_6_iiy_a,
res_7_iiy_b, res_7_iiy_c, res_7_iiy_d, res_7_iiy_e, res_7_iiy_h, res_7_iiy_l, res_7_iiy, res_7_iiy_a,
set_0_iiy_b, set_0_iiy_c, set_0_iiy_d, set_0_iiy_e, set_0_iiy_h, set_0_iiy_l, set_0_iiy, set_0_iiy_a,
set_1_iiy_b, set_1_iiy_c, set_1_iiy_d, set_1_iiy_e, set_1_iiy_h, set_1_iiy_l, set_1_iiy, set_1_iiy_a,
set_2_iiy_b, set_2_iiy_c, set_2_iiy_d, set_2_iiy_e, set_2_iiy_h, set_2_iiy_l, set_2_iiy, set_2_iiy_a,
set_3_iiy_b, set_3_iiy_c, set_3_iiy_d, set_3_iiy_e, set_3_iiy_h, set_3_iiy_l, set_3_iiy, set_3_iiy_a,
set_4_iiy_b, set_4_iiy_c, set_4_iiy_d, set_4_iiy_e, set_4_iiy_h, set_4_iiy_l, set_4_iiy, set_4_iiy_a,
set_5_iiy_b, set_5_iiy_c, set_5_iiy_d, set_5_iiy_e, set_5_iiy_h, set_5_iiy_l, set_5_iiy, set_5_iiy_a,
set_6_iiy_b, set_6_iiy_c, set_6_iiy_d, set_6_iiy_e, set_6_iiy_h, set_6_iiy_l, set_6_iiy, set_6_iiy_a,
set_7_iiy_b, set_7_iiy_c, set_7_iiy_d, set_7_iiy_e, set_7_iiy_h, set_7_iiy_l, set_7_iiy, set_7_iiy_a,
};

static void dd_cb(void)
{
    unsigned opcode;
    opcode = M_RDOP_ARG((R.PC.W.l + 1) & 0xffff);
    M_CSTATE(cycles_xx_cb[opcode]);
    (*(opcode_dd_cb[opcode]))();
    ++R.PC.W.l;
};
static void fd_cb(void)
{
    unsigned opcode;
    opcode = M_RDOP_ARG((R.PC.W.l + 1) & 0xffff);
    M_CSTATE(cycles_xx_cb[opcode]);
    (*(opcode_fd_cb[opcode]))();
    ++R.PC.W.l;
};

static opcode_fn opcode_cb[256] = {
rlc_b  , rlc_c  , rlc_d  , rlc_e  , rlc_h  , rlc_l  , rlc_ihl  , rlc_a  ,
rrc_b  , rrc_c  , rrc_d  , rrc_e  , rrc_h  , rrc_l  , rrc_ihl  , rrc_a  ,
rl_b   , rl_c   , rl_d   , rl_e   , rl_h   , rl_l   , rl_ihl   , rl_a   ,
rr_b   , rr_c   , rr_d   , rr_e   , rr_h   , rr_l   , rr_ihl   , rr_a   ,
sla_b  , sla_c  , sla_d  , sla_e  , sla_h  , sla_l  , sla_ihl  , sla_a  ,
sra_b  , sra_c  , sra_d  , sra_e  , sra_h  , sra_l  , sra_ihl  , sra_a  ,
sll_b  , sll_c  , sll_d  , sll_e  , sll_h  , sll_l  , sll_ihl  , sll_a  ,
srl_b  , srl_c  , srl_d  , srl_e  , srl_h  , srl_l  , srl_ihl  , srl_a  ,
bit_0_b, bit_0_c, bit_0_d, bit_0_e, bit_0_h, bit_0_l, bit_0_ihl, bit_0_a,
bit_1_b, bit_1_c, bit_1_d, bit_1_e, bit_1_h, bit_1_l, bit_1_ihl, bit_1_a,
bit_2_b, bit_2_c, bit_2_d, bit_2_e, bit_2_h, bit_2_l, bit_2_ihl, bit_2_a,
bit_3_b, bit_3_c, bit_3_d, bit_3_e, bit_3_h, bit_3_l, bit_3_ihl, bit_3_a,
bit_4_b, bit_4_c, bit_4_d, bit_4_e, bit_4_h, bit_4_l, bit_4_ihl, bit_4_a,
bit_5_b, bit_5_c, bit_5_d, bit_5_e, bit_5_h, bit_5_l, bit_5_ihl, bit_5_a,
bit_6_b, bit_6_c, bit_6_d, bit_6_e, bit_6_h, bit_6_l, bit_6_ihl, bit_6_a,
bit_7_b, bit_7_c, bit_7_d, bit_7_e, bit_7_h, bit_7_l, bit_7_ihl, bit_7_a,
res_0_b, res_0_c, res_0_d, res_0_e, res_0_h, res_0_l, res_0_ihl, res_0_a,
res_1_b, res_1_c, res_1_d, res_1_e, res_1_h, res_1_l, res_1_ihl, res_1_a,
res_2_b, res_2_c, res_2_d, res_2_e, res_2_h, res_2_l, res_2_ihl, res_2_a,
res_3_b, res_3_c, res_3_d, res_3_e, res_3_h, res_3_l, res_3_ihl, res_3_a,
res_4_b, res_4_c, res_4_d, res_4_e, res_4_h, res_4_l, res_4_ihl, res_4_a,
res_5_b, res_5_c, res_5_d, res_5_e, res_5_h, res_5_l, res_5_ihl, res_5_a,
res_6_b, res_6_c, res_6_d, res_6_e, res_6_h, res_6_l, res_6_ihl, res_6_a,
res_7_b, res_7_c, res_7_d, res_7_e, res_7_h, res_7_l, res_7_ihl, res_7_a,
set_0_b, set_0_c, set_0_d, set_0_e, set_0_h, set_0_l, set_0_ihl, set_0_a,
set_1_b, set_1_c, set_1_d, set_1_e, set_1_h, set_1_l, set_1_ihl, set_1_a,
set_2_b, set_2_c, set_2_d, set_2_e, set_2_h, set_2_l, set_2_ihl, set_2_a,
set_3_b, set_3_c, set_3_d, set_3_e, set_3_h, set_3_l, set_3_ihl, set_3_a,
set_4_b, set_4_c, set_4_d, set_4_e, set_4_h, set_4_l, set_4_ihl, set_4_a,
set_5_b, set_5_c, set_5_d, set_5_e, set_5_h, set_5_l, set_5_ihl, set_5_a,
set_6_b, set_6_c, set_6_d, set_6_e, set_6_h, set_6_l, set_6_ihl, set_6_a,
set_7_b, set_7_c, set_7_d, set_7_e, set_7_h, set_7_l, set_7_ihl, set_7_a
};

static opcode_fn opcode_dd[256] = {
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    add_ix_bc,  no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    add_ix_de,  no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    ld_ix_nn,   ld_inn_ix,   inc_ix,    inc_ixh,    dec_ixh,    ld_ixh_n,  no_op,
no_op,    add_ix_ix,  ld_ix_inn,   dec_ix,    inc_ixl,    dec_ixl,    ld_ixl_n,  no_op,
no_op,    no_op,      no_op,       no_op,     inc_iix,    dec_xix,    ld_iix_n,  no_op,
no_op,    add_ix_sp,  no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     ld_b_ixh,   ld_b_ixl,   ld_b_iix,  no_op,
no_op,    no_op,      no_op,       no_op,     ld_c_ixh,   ld_c_ixl,   ld_c_iix,  no_op,
no_op,    no_op,      no_op,       no_op,     ld_d_ixh,   ld_d_ixl,   ld_d_iix,  no_op,
no_op,    no_op,      no_op,       no_op,     ld_e_ixh,   ld_e_ixl,   ld_e_iix,  no_op,
ld_ixh_b, ld_ixh_c,   ld_ixh_d,    ld_ixh_e,  ld_ixh_ixh, ld_ixh_ixl, ld_h_iix,  ld_ixh_a,
ld_ixl_b, ld_ixl_c,   ld_ixl_d,    ld_ixl_e,  ld_ixl_ixh, ld_ixl_ixl, ld_l_iix,  ld_ixl_a,
ld_iix_b, ld_iix_c,   ld_iix_d,    ld_iix_e,  ld_iix_h,   ld_iix_l,   no_op,     ld_iix_a,
no_op,    no_op,      no_op,       no_op,     ld_a_ixh,   ld_a_ixl,   ld_a_iix,  no_op,
no_op,    no_op,      no_op,       no_op,     add_a_ixh,  add_a_ixl,  add_a_iix, no_op,
no_op,    no_op,      no_op,       no_op,     adc_a_ixh,  adc_a_ixl,  adc_a_iix, no_op,
no_op,    no_op,      no_op,       no_op,     sub_a_ixh,  sub_a_ixl,  sub_a_iix, no_op,
no_op,    no_op,      no_op,       no_op,     sbc_a_ixh,  sbc_a_ixl,  sbc_a_iix, no_op,
no_op,    no_op,      no_op,       no_op,     and_ixh,    and_ixl,    and_iix,   no_op,
no_op,    no_op,      no_op,       no_op,     xor_ixh,    xor_ixl,    xor_iix,   no_op,
no_op,    no_op,      no_op,       no_op,     or_ixh,     or_ixl,     or_iix,    no_op,
no_op,    no_op,      no_op,       no_op,     cp_ixh,     cp_ixl,     cp_iix,    no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       dd_cb,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    pop_ix,     no_op,       ex_isp_ix, no_op,      push_ix,    no_op,     no_op,
no_op,    jp_ix,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    ld_sp_ix,   no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
};

static opcode_fn opcode_ed[256] = {
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
in_b_ic,  out_ic_b, sbc_hl_bc, ld_inn_bc,   neg,     retn, im_0,  ld_i_a,
in_c_ic,  out_ic_c, adc_hl_bc, ld_bc_inn,   neg,     reti, im_0,  ld_r_a,
in_d_ic,  out_ic_d, sbc_hl_de, ld_inn_de,   neg,     retn, im_1,  ld_a_i,
in_e_ic,  out_ic_e, adc_hl_de, ld_de_inn,   neg,     reti, im_2,  ld_a_r,
in_h_ic,  out_ic_h, sbc_hl_hl, ld_inn_hl,   neg,     retn, im_0,  rrd,
in_l_ic,  out_ic_l, adc_hl_hl, ld_hl_inn,   neg,     reti, im_0,  rld,
in_0_ic,  out_ic_0, sbc_hl_sp, ld_inn_sp,   neg,     retn, im_1,  nop,
in_a_ic,  out_ic_a, adc_hl_sp, ld_sp_inn,   neg,     reti, im_2,  nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
ldi,      cpi,      ini,       outi,        nop,     nop,  nop,   nop,
ldd,      cpd,      ind,       outd,        nop,     nop,  nop,   nop,
ldir,     cpir,     inir,      otir,        nop,     nop,  nop,   nop,
lddr,     cpdr,     indr,      otdr,        nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  nop,   nop,
emu_rom1, nop,      emu_rom2,  nop,         emu_ram, nop,  nop,   nop,
nop,      nop,      nop,       nop,         nop,     nop,  patch, nop,
};

static opcode_fn opcode_fd[256] = {
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    add_iy_bc,  no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    add_iy_de,  no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    ld_iy_nn,   ld_inn_iy,   inc_iy,    inc_iyh,    dec_iyh,    ld_iyh_n,  no_op,
no_op,    add_iy_iy,  ld_iy_inn,   dec_iy,    inc_iyl,    dec_iyl,    ld_iyl_n,  no_op,
no_op,    no_op,      no_op,       no_op,     inc_iiy,    dec_xiy,    ld_iiy_n,  no_op,
no_op,    add_iy_sp,  no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     ld_b_iyh,   ld_b_iyl,   ld_b_iiy,  no_op,
no_op,    no_op,      no_op,       no_op,     ld_c_iyh,   ld_c_iyl,   ld_c_iiy,  no_op,
no_op,    no_op,      no_op,       no_op,     ld_d_iyh,   ld_d_iyl,   ld_d_iiy,  no_op,
no_op,    no_op,      no_op,       no_op,     ld_e_iyh,   ld_e_iyl,   ld_e_iiy,  no_op,
ld_iyh_b, ld_iyh_c,   ld_iyh_d,    ld_iyh_e,  ld_iyh_iyh, ld_iyh_iyl, ld_h_iiy,  ld_iyh_a,
ld_iyl_b, ld_iyl_c,   ld_iyl_d,    ld_iyl_e,  ld_iyl_iyh, ld_iyl_iyl, ld_l_iiy,  ld_iyl_a,
ld_iiy_b, ld_iiy_c,   ld_iiy_d,    ld_iiy_e,  ld_iiy_h,   ld_iiy_l,   no_op,     ld_iiy_a,
no_op,    no_op,      no_op,       no_op,     ld_a_iyh,   ld_a_iyl,   ld_a_iiy,  no_op,
no_op,    no_op,      no_op,       no_op,     add_a_iyh,  add_a_iyl,  add_a_iiy, no_op,
no_op,    no_op,      no_op,       no_op,     adc_a_iyh,  adc_a_iyl,  adc_a_iiy, no_op,
no_op,    no_op,      no_op,       no_op,     sub_a_iyh,  sub_a_iyl,  sub_a_iiy, no_op,
no_op,    no_op,      no_op,       no_op,     sbc_a_iyh,  sbc_a_iyl,  sbc_a_iiy, no_op,
no_op,    no_op,      no_op,       no_op,     and_iyh,    and_iyl,    and_iiy,   no_op,
no_op,    no_op,      no_op,       no_op,     xor_iyh,    xor_iyl,    xor_iiy,   no_op,
no_op,    no_op,      no_op,       no_op,     or_iyh,     or_iyl,     or_iiy,    no_op,
no_op,    no_op,      no_op,       no_op,     cp_iyh,     cp_iyl,     cp_iiy,    no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       fd_cb,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    pop_iy,     no_op,       ex_isp_iy, no_op,      push_iy,    no_op,     no_op,
no_op,    jp_iy,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    no_op,      no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
no_op,    ld_sp_iy,   no_op,       no_op,     no_op,      no_op,      no_op,     no_op,
};

static void cb(void)
{
    ++R.R;
    byte opcode = M_RDOP(R.PC.W.l);
    R.PC.W.l++;
    M_CSTATE(cycles_cb[opcode]);
    (*(opcode_cb[opcode]))();
}
static void dd(void)
{
    ++R.R;
    byte opcode = M_RDOP(R.PC.W.l);
    R.PC.W.l++;
    M_CSTATE(cycles_xx[opcode]);
    (*(opcode_dd[opcode]))();
}
static void ed(void)
{
    ++R.R;
    byte opcode = M_RDOP(R.PC.W.l);
    R.PC.W.l++;
    M_CSTATE(cycles_ed[opcode]);
    (*(opcode_ed[opcode]))();
}
static void fd (void)
{
    ++R.R;
    byte opcode = M_RDOP(R.PC.W.l);
    R.PC.W.l++;
    M_CSTATE(cycles_xx[opcode]);
    (*(opcode_fd[opcode]))();
}

static opcode_fn opcode_main[256] = {
nop,      ld_bc_nn,  ld_ibc_a,  inc_bc,    inc_b,    dec_b,    ld_b_n,    rlca,
ex_af_af, add_hl_bc, ld_a_ibc,  dec_bc,    inc_c,    dec_c,    ld_c_n,    rrca,
djnz,     ld_de_nn,  ld_ide_a,  inc_de,    inc_d,    dec_d,    ld_d_n,    rla,
jr,       add_hl_de, ld_a_ide,  dec_de,    inc_e,    dec_e,    ld_e_n,    rra,
jr_nz,    ld_hl_nn,  ld_inn_hl, inc_hl,    inc_h,    dec_h,    ld_h_n,    daa,
jr_z,     add_hl_hl, ld_hl_inn, dec_hl,    inc_l,    dec_l,    ld_l_n,    cpl,
jr_nc,    ld_sp_nn,  ld_inn_a,  inc_sp,    inc_ihl,  dec_ihl,  ld_ihl_n,  scf,
jr_c,     add_hl_sp, ld_a_inn,  dec_sp,    inc_a,    dec_a,    ld_a_n,    ccf,
ld_b_b,   ld_b_c,    ld_b_d,    ld_b_e,    ld_b_h,   ld_b_l,   ld_b_ihl,  ld_b_a,
ld_c_b,   ld_c_c,    ld_c_d,    ld_c_e,    ld_c_h,   ld_c_l,   ld_c_ihl,  ld_c_a,
ld_d_b,   ld_d_c,    ld_d_d,    ld_d_e,    ld_d_h,   ld_d_l,   ld_d_ihl,  ld_d_a,
ld_e_b,   ld_e_c,    ld_e_d,    ld_e_e,    ld_e_h,   ld_e_l,   ld_e_ihl,  ld_e_a,
ld_h_b,   ld_h_c,    ld_h_d,    ld_h_e,    ld_h_h,   ld_h_l,   ld_h_ihl,  ld_h_a,
ld_l_b,   ld_l_c,    ld_l_d,    ld_l_e,    ld_l_h,   ld_l_l,   ld_l_ihl,  ld_l_a,
ld_ihl_b, ld_ihl_c,  ld_ihl_d,  ld_ihl_e,  ld_ihl_h, ld_ihl_l, halt,      ld_ihl_a,
ld_a_b,   ld_a_c,    ld_a_d,    ld_a_e,    ld_a_h,   ld_a_l,   ld_a_ihl,  ld_a_a,
add_a_b,  add_a_c,   add_a_d,   add_a_e,   add_a_h,  add_a_l,  add_a_ihl, add_a_a,
adc_a_b,  adc_a_c,   adc_a_d,   adc_a_e,   adc_a_h,  adc_a_l,  adc_a_ihl, adc_a_a,
sub_a_b,  sub_a_c,   sub_a_d,   sub_a_e,   sub_a_h,  sub_a_l,  sub_a_ihl, sub_a_a,
sbc_a_b,  sbc_a_c,   sbc_a_d,   sbc_a_e,   sbc_a_h,  sbc_a_l,  sbc_a_ihl, sbc_a_a,
and_b,    and_c,     and_d,     and_e,     and_h,    and_l,    and_ihl,   and_a,
xor_b,    xor_c,     xor_d,     xor_e,     xor_h,    xor_l,    xor_ihl,   xor_a,
or_b,     or_c,      or_d,      or_e,      or_h,     or_l,     or_ihl,    or_a,
cp_b,     cp_c,      cp_d,      cp_e,      cp_h,     cp_l,     cp_ihl,    cp_a,
ret_nz,   pop_bc,    jp_nz,     jp,        call_nz,  push_bc,  add_a_n,   rst_00,
ret_z,    ret,       jp_z,      cb,        call_z,   call,     adc_a_n,   rst_08,
ret_nc,   pop_de,    jp_nc,     out_inn_a, call_nc,  push_de,  sub_a_n,   rst_10,
ret_c,    exx,       jp_c,      in_a_inn,  call_c,   dd,       sbc_a_n,   rst_18,
ret_po,   pop_hl,    jp_po,     ex_isp_hl, call_po,  push_hl,  and_n,     rst_20,
ret_pe,   jp_hl,     jp_pe,     ex_de_hl,  call_pe,  ed,       xor_n,     rst_28,
ret_p,    pop_af,    jp_p,      di,        call_p,   push_af,  or_n,      rst_30,
ret_m,    ld_sp_hl,  jp_m,      ei,        call_m,   fd,       cp_n,      rst_38,
};


// ---------------------------------------------------------------- Interryupt, Init, etc.
void ei(void)
{
    /* If interrupts were disabled, execute one more instruction and check the */
    /* IRQ line. If not, simply set interrupt flip-flop 2                      */
    if (!R.IFF1) {
#ifdef _DEBUG
        if (R.PC.W.l == Z80_Trap) Z80_Trace = 1;
        if (Z80_Trace) Z80_Debug(&R);
#endif
        R.IFF1 = R.IFF2 = 1;
        ++R.R;
        byte opcode = M_RDOP(R.PC.W.l);
        R.PC.W.l++;

        //  Z80_ICount-=cycles_main[opcode];
        M_CSTATE(cycles_main[opcode]);

        (*(opcode_main[opcode]))();

        Z80_Interrupt();

        if (R.IM == 1) {
        }
        else {
            if (R.IM == 2) {
            }
        }
    }
    else {
        R.IFF2 = 1;
    }
}

/****************************************************************************/
/* Reset registers to their initial values                                  */
/****************************************************************************/
void Z80_Reset (void)
{
    memset(&R, 0, sizeof(Z80_Regs));
    R.SP.W.l = 0xf000;
    R.R = rand();
    Z80_ICount = Z80_IPeriod;
}

/****************************************************************************/
/* Initialise the various lookup tables used by the emulation code          */
/****************************************************************************/
static void InitTables (void)
{
    static int InitTables_virgin = 1;
    byte zs;
    int i, p;
    if (!InitTables_virgin) return;
    InitTables_virgin = 0;
    for (i = 0; i < 256; ++i) {
        zs = 0;
        if (i == 0) { zs |= Z_FLAG; }
        if (i & 0x80) { zs |= S_FLAG; }
        p = 0;
        if (i & 1) { ++p; }
        if (i & 2) { ++p; }
        if (i & 4) { ++p; }
        if (i & 8) { ++p; }
        if (i & 16) { ++p; }
        if (i & 32) { ++p; }
        if (i & 64) { ++p; }
        if (i & 128) { ++p; }
        PTable[i] = (p & 1) ? 0 : V_FLAG;
        ZSTable[i] = zs;
        ZSPTable[i] = zs | PTable[i];
#ifdef Z80_EMU_XY
       ZSPBitTable[i] = ((i ? (i & S_FLAG) : (Z_FLAG | V_FLAG))) & ~(X_FLAG | Y_FLAG);
#else
       ZSPBitTable[i] = (i ? (i & S_FLAG) : (Z_FLAG | V_FLAG));
#endif
    }
    for (i = 0; i < 256; ++i) {
        ZSTable[i + 256] = ZSTable[i] | C_FLAG;
        ZSPTable[i + 256] = ZSPTable[i] | C_FLAG;
        PTable[i + 256] = PTable[i] | C_FLAG;
    }
}

/****************************************************************************/
/* Issue an interrupt if necessary                                          */
/****************************************************************************/
void Interrupt (int j)
{
    if (j == Z80_IGNORE_INT) { return; }

    if (j == Z80_NMI_INT || R.IFF1) {
        /* Clear interrupt flip-flop 1 */
        R.IFF1 = 0;
        /* Check if processor was halted */
        if (R.HALT) {
            ++R.PC.W.l;
            R.HALT = 0;
        }
        if (j == Z80_NMI_INT) {
            push(R.PC.W.l);
            R.PC.W.l = 0x0066;
            M_SET_WZ(R.PC.W.l);
        }
        else {
            /* Interrupt mode 2. Cal[R.I:databyte] */
#if 1
            if (R.IM == 2) {
                if (Z80_intflag & 2) {
                    Z80_intflag &= (~2);
                    // printf("IM 2\n");
                    push(R.PC.W.l);
                    R.PC.W.l = read_mem_word((j & 255) | (R.I << 8));
                    M_SET_WZ(R.PC.W.l);
                    // printf("vector adr=($%04x) $%04x\n",((j&255)|(R.I<<8)),(int)R.PC.W.l);
                }
            }
            else
#endif	   
                /* Interrupt mode 1. RST 38h */
                if (R.IM == 1) {
                    if (Z80_intflag & 1) {
                        Z80_intflag &= (~1);
                        
                        // printf("IM 1\n");

                        M_CSTATE(cycles_main[0xff]);
                        (*(opcode_main[0xff]))();  // RST 38h
                    }
                }
#if 1
                else
                    /* Interrupt mode 0. We check for CALL and JP instructions, if neithera
                     * of these were found we assume a 1 byte opcode was placed on the
                     * databus
                     */
                {
                    // printf("IM 0\n");
                    switch (j & 0xff0000) {
                    case 0xCD:
                        push(R.PC.W.l);
                    case 0xC3:
                        R.PC.W.l = j & 0xffff;
                        break;
                    default:
                        j &= 255;
                        // Z80_ICount-=cycles_main[j];
                        M_CSTATE(cycles_main[j]);
                        (*(opcode_main[j]))();
                        break;
                    }
                }
#endif   
        }
    }
}

/****************************************************************************/
/* Set all registers to given values                                        */
/****************************************************************************/
void Z80_SetRegs (Z80_Regs *Regs)
{
    R = *Regs;
}

/****************************************************************************/
/* Get all registers in given buffer                                        */
/****************************************************************************/
void Z80_GetRegs (Z80_Regs *Regs)
{
    *Regs = R;
}

/****************************************************************************/
/* Get pointer of static Registers buffer                                   */
/****************************************************************************/
Z80_Regs * Z80_GetRegsPtr(void)
{
    return &R;
}

/****************************************************************************/
/* Return program counter                                                   */
/****************************************************************************/
unsigned Z80_GetPC (void)
{
    return R.PC.W.l;
}

/****************************************************************************/
/* Execute IPeriod T-States. Return 0 if emulation should be stopped        */
/****************************************************************************/
int Z80_Execute (void)
{
    unsigned opcode;
    extern T8253_STAT _8253_stat[3];
 
    Z80_Running = 1;
    InitTables();
    do {
#ifdef TRACE
        pc_trace[pc_count] = R.PC.D;
        pc_count = (pc_count + 1) & 255;
#endif
#ifdef _DEBUG
        if (R.PC.W.l == Z80_Trap) { Z80_Trace = 1; }
        if (Z80_Trace) { Z80_Debug(&R); }
        if (!Z80_Running) { break; }
#endif

        ++R.R;

        // Interrupt Timer
        Z80_Interrupt();
        
        //
        opcode = M_RDOP(R.PC.W.l);
        R.PC.W.l++;
        M_CSTATE(cycles_main[opcode]);
  
        (*(opcode_main[opcode]))();
  
#if _DEBUG
        if (Z80_Trace) break;
#endif
    } while (Z80_ICount > 0);
 
    Z80_ICount += Z80_IPeriod;

    return Z80_Running;
}

/****************************************************************************/
/* Interpret Z80 code                                                       */
/****************************************************************************/
word Z80 (void)
{
    while (Z80_Execute());
    return(R.PC.W.l);
}

/****************************************************************************/
/* Dump register contents and (optionally) a PC trace to stdout             */
/****************************************************************************/
void Z80_RegisterDump (void)
{
    int i;
    printf
    (
        "AF:%04X HL:%04X DE:%04X BC:%04X PC:%04X SP:%04X IX:%04X IY:%04X\n",
        R.AF.W.l, R.HL.W.l, R.DE.W.l, R.BC.W.l, R.PC.W.l, R.SP.W.l, R.IX.W.l, R.IY.W.l
    );
    printf("STACK: ");
    for (i = 0; i < 10; ++i) { printf("%04X ", read_mem_word((R.SP.W.l + i * 2) & 0xFFFF)); }
    puts("");
#ifdef TRACE
    puts("PC TRACE:");
    for (i = 1; i <= 256; ++i) printf("%04X\n", pc_trace[(pc_count - i) & 255]);
#endif
}

/****************************************************************************/
/* Set number of memory refresh wait states (i.e. extra cycles inserted     */
/* when the refresh register is being incremented)                          */
/****************************************************************************/
void Z80_SetWaitStates (int n)
{
    int i;
    for (i = 0; i < 256; ++i) {
        cycles_main[i] += n;
        cycles_cb[i] += n;
        cycles_ed[i] += n;
        cycles_xx[i] += n;
    }
}
