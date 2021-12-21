/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                              Z80Codes.h                              ***/
/***                                                                      ***/
/*** This file contains various macros used by the emulation engine       ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

#define M_CSTATE(codest) do {                           \
    Z80_ICount -= codest;                               \
    ts700.cpu_tstates += codest;                        \
    if ( (ts700.cpu_tstates - ts700.tempo_tstates) >= TEMPO_TSTATES) {\
        ts700.tempo_tstates += TEMPO_TSTATES;           \
        hw700.tempo_strobe^=1;                          \
    }                                                   \
    if ( (ts700.cpu_tstates - ts700.vsync_tstates) >= (59666 - VBLANK_TSTATES) ) {\
        hw700.retrace = 0;                              \
    }                                                   \
} while (FALSE)
