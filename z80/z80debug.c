/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                              Z80Debug.c                              ***/
/***                                                                      ***/
/*** This file contains a simple single step debugger which is called     ***/
/*** when DEBUG is #defined and Z80_Trace is true                         ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

#ifdef _DEBUG

#include "Z80.h"
#include <stdio.h>
#include <string.h>
#include "Z80Dasm.h"

void Z80_Debug (Z80_Regs *R)
{
    byte buf[5];
    char flags[] = "SZ.H.VNC";
    char S[20], f[9];
    int i;
    buf[0] = Z80_RDOP(R->PC.W.l); i = 1;
    /* Figure out which bytes are opcode arguments and which are not */
    switch (buf[0])
    {
    case 0xCB:
    case 0xED:
        buf[1] = Z80_RDOP((R->PC.W.l + 1) & 0xFFFF); i = 2;
        break;
    case 0xDD:
    case 0xFD:
        buf[1] = Z80_RDOP((R->PC.W.l + 1) & 0xFFFF); i = 2;
        if (buf[1] == 0xCB)
        {
            buf[2] = Z80_RDOP_ARG((R->PC.W.l + 2) & 0xFFFF);
            buf[3] = Z80_RDOP((R->PC.W.l + 3) & 0xFFFF);
            i = 4;
        }
    }
    for (; i < 5; ++i) { buf[i] = Z80_RDOP_ARG((R->PC.W.l + i) & 0xFFFF); }
    Z80_Dasm(buf, S, sizeof(S), R->PC.W.l);
    for (i = strlen(S); i < 19; ++i) S[i] = ' ';
    S[19] = '\0';
    for (i = 7; i >= 0; --i) { f[7 - i] = (R->AF.W.l & (1 << i)) ? flags[7 - i] : '.'; }
    f[8] = '\0';
    printf("%04X: %s  AT SP: %04X %04X  FLAGS: %s\n", R->PC.W.l, S,
        Z80_RDMEM(R->SP.W.l) + Z80_RDMEM((R->SP.W.l + 1) & 0xFFFF) * 256,
        Z80_RDMEM((R->SP.W.l + 2) & 0xFFFF) + Z80_RDMEM((R->SP.W.l + 3) & 0xFFFF) * 256, f);
    printf("%27sAF:%04X BC:%04X DE:%04X HL:%04X IX:%04X IY:%04X\n",
        "", R->AF.W.l, R->BC.W.l, R->DE.W.l, R->HL.W.l, R->IX.W.l, R->IY.W.l);
}

#endif
