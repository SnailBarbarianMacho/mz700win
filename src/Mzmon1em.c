//----------------------------------------------------------------------------
// File:MZmonem1.c
//
// mz700win:1Z-009A (For Japanese MZ-700) Emulation / Patch ....
// ($Id: Mzmon1em.c 1 2007-07-27 06:17:19Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#define MZMONEM_H_

#include <stdio.h>
#include <windows.h>
#include "mz700win.h"

#include "dprintf.h"
#include "fileio.h"
#include "Z80.h"
#include "Z80codes.h"
#include "win.h"
#include "fileset.h"

#include "mzmain.h"
#include "mzhw.h"
#include "mzmon1em.h"
#include "mzramem.h"
#include "mzscrn.h"

static const byte hextbl[] = { "0123456789ABCDEF" };

static int tapepos = 0;													// テープシーク位置
static int inf_start;
static int inf_len;

static unsigned char rdinf_buf[128];

//-----------------
// ROM1 PATCH JUMPTBL
static void (* rom1_functbl[])(Z80_Regs *) =
{
//
	MON_ret,					// 00
	MON_rdinf,					// 01
	MON_rddata,					//
	MON_wrinf,					//
	MON_wrdata,					//
	MON_verfy,					//
	MON_qget,					//
	MON_qbrk,					//
//
	MON__mang,					//
	MON_qmode,					//
	MON_timin,					//
	MON_qswep,					//
	MON_qadcn,					//
	MON_qdacn,					//
	MON_qkey,					//
	MON_qpont,					//
	MON_qpnt1,					//
	MON_qdsp,					//
	MON_qprnt,					//
	MON_prnt3,					//
	MON_qprt,					//
	MON_qdpct,					//

	MON_qltnl,					//
	MON_qnl,					//

	MON_qmsg,					//
	MON_qmsgx,					//

	MON_mldst,					//
	MON_mldsp,					//
	MON_qprts,					//
	MON_qprtt,					//

	MON_qbel,					//

	MON_asc,					//
	MON_hex,					//
	MON_hlhex,					//
	MON_2hex,					//
	MON_sphex,					//
	MON_prthl,					//
	MON_prthx,					//

	MON_qsave,					//
	MON_qload,					//
	MON_flkey,					//
	MON_qqkey,					//
	MON_qfls,					//

	MON_qcler,					//
	MON_qclrff,					//
	MON_dsp03,					//
};

// ROM1 PATCH TABLE
static TMONEMU rom1_pattbl[] =
{
	{ PAT_ADR_RDINF, ROM1_rdinf },
	{ PAT_ADR_RDDATA, ROM1_rddata },
	{ PAT_ADR_WRINF, ROM1_wrinf },
	{ PAT_ADR_WRDATA, ROM1_wrdata },
	{ PAT_ADR_VERFY, ROM1_verfy },

	//================================ for debug
#if MZ_ROM1_TEST
	
	{ PAT_ADR_QBRK, ROM1_qbrk },
	{ PAT_ADR_QSAVE, ROM1_qsave },
	{ PAT_ADR_QLOAD, ROM1_qload },
	{ PAT_ADR_QADCN, ROM1_qadcn },
	{ PAT_ADR_QDACN, ROM1_qdacn },
	{ PAT_ADR_QKEY, ROM1_qkey },
	{ PAT_ADR_QFLS, ROM1_qfls },
	{ PAT_ADR_QGET, ROM1_qget },

	{ PAT_ADR_SPHEX, ROM1_sphex },
	{ PAT_ADR_PRTHL, ROM1_prthl },
	{ PAT_ADR_PRTHX, ROM1_prthx },
#endif
	{ 0xFFFF, ROM1_end }										/* eod */
};

// ROM1 EMUL pATCH TABLE
static TMONEMU rom1_emultbl[] =
{
	{ PAT_ADR_QGET, ROM1_qget },

	//
	{ PAT_ADR_QMODE, ROM1_qmode },
	{ PAT_ADR_QSWEP, ROM1_qswep },
	{ PAT_ADR_TIMIN, ROM1_timin },

	{ PAT_ADR_QADCN, ROM1_qadcn },
	{ PAT_ADR_QDACN, ROM1_qdacn },
	{ PAT_ADR_QBRK, ROM1_qbrk },
	{ PAT_ADR_QSAVE, ROM1_qsave },
	{ PAT_ADR_QLOAD, ROM1_qload },

	{ PAT_ADR_QKEY, ROM1_qkey },
	{ PAT_ADR_QPONT, ROM1_qpont },
	{ PAT_ADR_QPNT1, ROM1_qpnt1 },
	{ PAT_ADR_QDSP, ROM1_qdsp },
	{ PAT_ADR_QPRT+1, ROM1_qprt },
	{ PAT_ADR_QPRNT, ROM1_qprnt },
	{ PAT_ADR_PRNT3, ROM1_prnt3 },
	{ PAT_ADR_QDPCT, ROM1_qdpct },
	{ PAT_ADR__MANG, ROM1__mang },

	{ PAT_ADR_QLTNL, ROM1_qltnl },
	{ PAT_ADR_QNL, ROM1_qnl },

	{ PAT_ADR_QMSG, ROM1_qmsg },
	{ PAT_ADR_QMSGX, ROM1_qmsgx },

	{ PAT_ADR_MLDST, ROM1_mldst },
	{ PAT_ADR_MLDSP, ROM1_mldsp },

	{ PAT_ADR_QBEL2, ROM1_qbel },

	{ PAT_ADR_QPRTS, ROM1_qprts },
	{ PAT_ADR_QPRTT, ROM1_qprtt },

	{ PAT_ADR_ASC, ROM1_asc },
	{ PAT_ADR_HEX, ROM1_hex },
	{ PAT_ADR_HLHEX, ROM1_hlhex },
	{ PAT_ADR_2HEX, ROM1_2hex },
	{ PAT_ADR_SPHEX, ROM1_sphex },
	{ PAT_ADR_PRTHL, ROM1_prthl },
	{ PAT_ADR_PRTHX, ROM1_prthx },

	{ PAT_ADR_FLKEY, ROM1_flkey },
	{ PAT_ADR_QFLS, ROM1_qfls },
//	{ PAT_ADR_QQKEY, ROM1_qqkey },

	{ PAT_ADR_QCLER, ROM1_qcler },
	{ PAT_ADR_QCLRFF, ROM1_qclrff },
	{ PAT_ADR_DSP03, ROM1_dsp03 },

	{ 0xFFFF, ROM1_end }										/* eod */
};

//----------------
// ASCII TO DISPLAY CODE TBL [Japanese]
static const byte asc2disp_j[]=
{
	0xF0,0xF0,0xF0,0xF3,0xF0,0xF5,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
	0xF0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
	0x00,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6B,0x6A,0x2F,0x2A,0x2E,0x2D,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x4F,0x2C,0x51,0x2B,0x57,0x49,
	0x55,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x52,0x59,0x54,0x50,0x45,
	0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xDF,0xE7,0xE8,0xE9,0xEA,0xEC,0xED,
	0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xC0,
	0x80,0xBD,0x9D,0xB1,0xB5,0xB9,0xB4,0x9E,0xB2,0xB6,0xBA,0xBE,0x9F,0xB3,0xB7,0xBB,
	0xBF,0xA3,0x85,0xA4,0xA5,0xA6,0x94,0x87,0x88,0x9C,0x82,0x98,0x84,0x92,0x90,0x83,
	0x91,0x81,0x9A,0x97,0x93,0x95,0x89,0xA1,0xAF,0x8B,0x86,0x96,0xA2,0xAB,0xAA,0x8A,
	0x8E,0xB0,0xAD,0x8D,0xA7,0xA8,0xA9,0x8F,0x8C,0xAE,0xAC,0x9B,0xA0,0x99,0xBC,0xB8,
	0x40,0x3B,0x3A,0x70,0x3C,0x71,0x5A,0x3D,0x43,0x56,0x3F,0x1E,0x4A,0x1C,0x5D,0x3E,
	0x5C,0x1F,0x5F,0x5E,0x37,0x7B,0x7F,0x36,0x7A,0x7E,0x33,0x4B,0x4C,0x1D,0x6C,0x5B,
	0x78,0x41,0x35,0x34,0x74,0x30,0x38,0x75,0x39,0x4D,0x6F,0x6E,0x32,0x77,0x76,0x72,
	0x73,0x47,0x7C,0x53,0x31,0x4E,0x6D,0x48,0x46,0x7D,0x44,0x1B,0x58,0x79,0x42,0x60
};
//----------------
// KEY MATRIX TO DISPLAY CODE TBL [Japanese]
static const byte k2disp_j[]=
{
	//KTBL
	0xCA,0xF0,0x2B,0xC9,0xF0,0x2C,0x4F,0xCD,	// 00
	0x19,0x1A,0x55,0x68,0x69,0xF0,0xF0,0xF0,	// 08
	0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,	// 10
	0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,	// 18
	0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,	// 20
	0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,	// 28
	0x6B,0x6A,0x2A,0x00,0x20,0x29,0x2F,0x2E,	// 30
	0xC8,0xC7,0xC2,0xC1,0xC3,0xC4,0x49,0x2D,	// 38

	// KTBL SHIFT ON
	0xCA,0xF0,0x2B,0xC9,0xF0,0x2C,0x4F,0xCD,	// 40
	0x19,0x1A,0x55,0x50,0x80,0xF0,0xF0,0xF0,	// 48
	0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,	// 50
	0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,	// 58
	0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,	// 60
	0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x52,	// 68
	0x59,0xDE,0xDD,0x00,0x60,0x54,0x51,0x57,	// 70
	0xC6,0xC5,0xC2,0xC1,0xC3,0xC4,0x40,0x45,	// 78

	// GRAPHIC
	0xCA,0xF0,0x3E,0xC9,0xF0,0x79,0x1B,0xCD,	// 80
	0x3D,0x5A,0x6D,0x36,0x4A,0xF0,0xF0,0xF0,	// 88
	0x72,0x33,0x5D,0x71,0x70,0x46,0x73,0x53,	// 90
	0x3C,0x1E,0x5E,0x78,0x56,0x43,0x77,0x76,	// 98
	0x5C,0x41,0x44,0x1C,0x32,0x1D,0x1F,0x5F,	// A0
	0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,	// A8
	0xDC,0xDB,0xDA,0x00,0xD9,0xD8,0x42,0x4D,	// B0
	0xC6,0xC5,0xC2,0xC1,0xC3,0xC4,0x47,0x4E,	// B8

	// CONTROL CODE
	0xF3,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,	// C0
	0xF0,0x5A,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,	// C8
	0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xF0,0xF0,	// D0
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,	// D8
	0xF0,0xF0,0xF3,0xF0,0xF5,0xF0,0xF0,0xF0,	// E0
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,	// E8
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,	// F0
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,	// F8

	// KANA
	0xCA,0xF0,0x99,0xC9,0xF0,0x8D,0xBC,0xCD,	// 100
	0x86,0x91,0xA0,0xB4,0xB8,0xF0,0xF0,0xF0,	// 108
	0x94,0x9C,0x84,0x82,0x96,0x97,0x87,0x81,	// 110
	0xA2,0x8E,0xB0,0xAD,0x8C,0x8F,0xAB,0xAA,	// 118
	0x98,0x93,0x9A,0x92,0x88,0x90,0x83,0x8A,	// 120
	0xA3,0x85,0xA4,0xA5,0xA6,0x95,0x89,0xA1,	// 128
	0xA9,0xA8,0xA7,0x00,0x8B,0xAF,0xAE,0xAC,	// 130
	0xC8,0xC7,0xC2,0xC1,0xC3,0xC4,0xBF,0x9B,	// 138

	// SMALL KANA
	0xBB,0xF0,0xF0,0xF0,0xF0,					// 140
	0xF0,0x9E,0xB2,0xB6,0xBA,0xBE,0xF0,0xF0,0xF0,
	0xB7,0xB3,0x9F,0x00,0xF0,0xF0,0xB5,0xBD,
	0xC6,0xC5,0xC2,0xC1,0xC3,0xC4,0xF0,0xF0
};

//
static const TPATCH initial_code[] =
{
	// initial
	0x0000,0xC3,		// 
	0x0001,0x4A,		// 
	0x0002,0x00,		// 

	PAT_ADR_LETNL  ,0xC3,					//
	PAT_ADR_LETNL+1,PAT_ADR_QLTNL & 0xFF,	//
	PAT_ADR_LETNL+2,PAT_ADR_QLTNL >> 8,		//

	PAT_ADR_NL  ,0xC3,					//
	PAT_ADR_NL+1,PAT_ADR_QNL & 0xFF,	//
	PAT_ADR_NL+2,PAT_ADR_QNL >> 8,		//

	PAT_ADR_MSG  ,0xC3,					//
	PAT_ADR_MSG+1,PAT_ADR_QMSG & 0xFF,	//
	PAT_ADR_MSG+2,PAT_ADR_QMSG >> 8,		//

	PAT_ADR_MSGX  ,0xC3,					//
	PAT_ADR_MSGX+1,PAT_ADR_QMSGX & 0xFF,	//
	PAT_ADR_MSGX+2,PAT_ADR_QMSGX >> 8,		//

	PAT_ADR_BRKEY  ,0xC3,					//
	PAT_ADR_BRKEY+1,PAT_ADR_QBRK & 0xFF,	//
	PAT_ADR_BRKEY+2,PAT_ADR_QBRK >> 8,		//

	PAT_ADR_GETKY  ,0xC3,					//
	PAT_ADR_GETKY+1,PAT_ADR_QGET & 0xFF,	//
	PAT_ADR_GETKY+2,PAT_ADR_QGET >> 8,		//

	PAT_ADR_MSTA  ,0xC3,					//
	PAT_ADR_MSTA+1,PAT_ADR_MLDST & 0xFF,	//
	PAT_ADR_MSTA+2,PAT_ADR_MLDST >> 8,		//

	PAT_ADR_MSTP  ,0xC3,					//
	PAT_ADR_MSTP+1,PAT_ADR_MLDSP & 0xFF,	//
	PAT_ADR_MSTP+2,PAT_ADR_MLDSP >> 8,		//

	PAT_ADR_BELL  ,0xC3,		//
	PAT_ADR_BELL+1,PAT_ADR_QBEL & 0xFF,	//
	PAT_ADR_BELL+2,PAT_ADR_QBEL >> 8,	//

	PAT_ADR_PRNT  ,0xC3,		//
	PAT_ADR_PRNT+1,PAT_ADR_QPRNT & 0xFF,	//
	PAT_ADR_PRNT+2,PAT_ADR_QPRNT >> 8,	//

	PAT_ADR_PRNTS  ,0xC3,		//
	PAT_ADR_PRNTS+1,PAT_ADR_QPRTS & 0xFF,	//
	PAT_ADR_PRNTS+2,PAT_ADR_QPRTS >> 8,	//

	PAT_ADR_PRNTT  ,0xC3,		//
	PAT_ADR_PRNTT+1,PAT_ADR_QPRTT & 0xFF,	//
	PAT_ADR_PRNTT+2,PAT_ADR_QPRTT >> 8,	//

	PAT_ADR_DSWEP  ,0xC3,
	PAT_ADR_DSWEP+1,PAT_ADR_QSWEP & 0xFF,	//
	PAT_ADR_DSWEP+2,PAT_ADR_QSWEP >> 8,	//

	PAT_ADR_QFLAS	,0x18,		// JR
	PAT_ADR_QFLAS+1	,(PAT_ADR_QFLS - PAT_ADR_QFLAS - 2),

	PAT_ADR_QPRT	,0x79,		// LD A,C

	PAT_ADR_DLY3	,0x3E,		// LD A,$45
	PAT_ADR_DLY3+1  ,0x45,
	PAT_ADR_DLY3+2	,0xC3,		// JP $0762
	PAT_ADR_DLY3+3  ,0x62,
	PAT_ADR_DLY3+4  ,0x07,

	0x004A,0x31,		// LD SP,$10F0
	0x004B,0xF0,		// 
	0x004C,0x10,		// 
	0x004D,0xF3,		// DI
	0x004E,0xED,		// IM 1
	0x004F,0x56,		// 
	0x0050,0xCD,		// 
	0x0051,0x3E,		// 
	0x0052,0x07,		// 

	0x0053,0xCD,		// CALL BELL
	0x0054,PAT_ADR_BELL & 0xFF,
	0x0055,PAT_ADR_BELL >> 8,
	0x0056,0xC3,		// JP ST1
	0x0057,PAT_ADR_ST1 & 0xFF,
	0x0058,PAT_ADR_ST1 >> 8,

	// INTERRUPT JUMP ROUTINE
	0x0038,0xC3,
	0x0039,0x38,
	0x003A,0x10,

	// ?BEL:
	PAT_ADR_QBEL+ 0,0xC3,		// JP ?BEL2
	PAT_ADR_QBEL+ 1,PAT_ADR_QBEL2&0xFF,
	PAT_ADR_QBEL+ 2,PAT_ADR_QBEL2>>8,

	0xFFFF, 0x00
};

// ??KEY:
static const byte qqkey_code[] =
{
	0xE5,						// QQKEY:	PUSH    HL
	0xCD,						//			CALL    QSAVE
	PAT_ADR_QSAVE & 0xFF,
	PAT_ADR_QSAVE >> 8,

	0xCD,						// L1:		CALL    FLKEY
	PAT_ADR_FLKEY & 0xFF,
	PAT_ADR_FLKEY >> 8,
	0x20,-5,					//			JR      NZ,L1		; KEY IN THEN JUMP

	0xCD,						// L2:		CALL    FLKEY
	PAT_ADR_FLKEY & 0xFF,
	PAT_ADR_FLKEY >> 8,
	0x28,-5,					//			JR      Z,L2		; NOT KEY IN THEN JUMP
	0x67,						//			LD      H,A

	0xCD,						//			CALL    QBLNK		; DELAY CHATTER
	PAT_ADR_QBLNK & 0xFF,
	PAT_ADR_QBLNK >> 8,
	0xCD,						//			CALL    QKEY
	PAT_ADR_QKEY & 0xFF,
	PAT_ADR_QKEY >> 8,
	0xF5,						//			PUSH    AF
	0xBC,						//			CP      H			; CHATTER CHECK
	0xE1,						//			POP     HL
	0x20,-17,					//			JR      NZ,L2
	0xCB,0x45,					//			BIT		0,L
	0x21,						//			LD		HL,SWRK
	L_SWRK & 0xFF,L_SWRK >>8,
	0xCB,0xBE,					//			RES		7,(HL)
	0x28,0x02,					//			JR		Z,+4
	0xCB,0xFE,					//			SET		7,(HL)
	0xCD,						//			CALL    QLOAD		; FLASHING DATA LOAD
	PAT_ADR_QLOAD & 0xFF,
	PAT_ADR_QLOAD >> 8,
	0xE1,						//			POP     HL
	0xC9,						//			RET     
};

// DLY1: , DLY2:
static const byte dly1_2_code[] =
{
	0x3E,0x15,0x3D,0xC2,0x5B,0x07,0xC9,		// DLY1:loop wait1
	0x3E,0x13,0x3D,0xC2,0x5B,0x07,0xC9,		// DLY2:loop wait2
};

// DLY12:
static const byte dly12_code[] =
{
	0xC5,0x06,0x15,0xCD,					// DLY12:
	PAT_ADR_DLY3 & 0xFF,PAT_ADR_DLY3 >> 8,
	0x10,0xFB,0xC1,0xC9,
};

// ?BLNK:
// ROMモニタのコードと同一ですが、誰がどう書いてもこうなりますよね。
static const byte qblnk_code[] =
{
	0xF5,		// PUSH AF
	0x3A,		// LD A,($E002)
	0x02,		//
	0xE0,		//
	0x07,		// RCLA
	0x30,		// JR NC,-6
	0xFA,		//
	0x3A,		// LD A,($E002)
	0x02,		//
	0xE0,		//
	0x07,		// RCLA
	0x38,		// JR C,-6
	0xFA,		//
	0xF1,		// POP AF
	0xC9,		// RET
};

// QBEL2(+3):
static const byte qbel2_code[] =
{
	0xC5,		// PUSH BC
	0x06,0x05,	// LD B,5
	0xCD,		// CALL ?BLNK
	PAT_ADR_QBLNK&0xFF,
	PAT_ADR_QBLNK>>8,
	0x10,0xFB,	// DJNZ -5
	0xCD,		// CALL MSTP
	PAT_ADR_MSTP&0xFF,
	PAT_ADR_MSTP>>8,
	0xC1,		// POP BC
	0xC9,		// RET
};

// ST1:-
static const byte st1_code[] =
{
	0x11,
	(PAT_ADR_ST1+13)&0xFF,(PAT_ADR_ST1+13)>>8,
	0xCD,
	(PAT_ADR_MSG)&0xFF,(PAT_ADR_MSG)>>8,
	0xCD,
	(PAT_ADR_QQKEY)&0xFF,(PAT_ADR_QQKEY)>>8,
	0x2A,										// LD HL,(EXADR)
	(L_TMPEX)&0xFF,(L_TMPEX)>>8,
	0xE9,										// JP (HL)

	'*','*',' ',
	'M','Z','7','0','0','W','I','N', ' ',
	'*','*',' ',
	0x0D
};

/*
    Patch for ROM1 Emulation
 */
void patch_emul_rom1(void)
{
	dprintf("patch_emul_rom1()\n");

	FillMemory(mem + ROM_START,0x0400, 0xC9);							// Fill by 'RET'
	bios_patch(initial_code);
	CopyMemory(mem + ROM_START + PAT_ADR_QQKEY ,qqkey_code, sizeof(qqkey_code) );
	CopyMemory(mem + ROM_START + PAT_ADR_QBLNK ,qblnk_code, sizeof(qblnk_code) );
	CopyMemory(mem + ROM_START + PAT_ADR_QBEL2+3 ,qbel2_code, sizeof(qbel2_code) );
	CopyMemory(mem + ROM_START + PAT_ADR_DLY1  ,dly1_2_code, sizeof(dly1_2_code) );
	CopyMemory(mem + ROM_START + PAT_ADR_DLY12  ,dly12_code, sizeof(dly12_code) );
	CopyMemory(mem + ROM_START + PAT_ADR_ST1  ,st1_code, sizeof(st1_code) );

	// INTERRUPT JUMP ROUTINE
	mem[RAM_START+0x1038] = 0xC3;		// 
	mem[RAM_START+0x1039] = PAT_ADR_TIMIN & 0xFF;		//
	mem[RAM_START+0x103A] = PAT_ADR_TIMIN >> 8;			//

	patch_rom1();
   // for emul rom1
	patch1_job(rom1_emultbl);

}

void patch1_job(TMONEMU *pattbl)
{
	TMONEMU *ptbl;
	unsigned int ad,ad2;
	
	ptbl = pattbl;
	
	for (;;ptbl++)
	{
		ad = (unsigned int) ptbl->addr;
		if (ad == 0xFFFF) break;
		// if jump table........
		if (mem[ad] == 0xC3)
		{
			ad2 = mem[ad+1] | ( mem[ad+2] << 8);
//			dprintf("Jump Addr = $%04X\n",ad2);
			mem[ad2  ] = 0xED;
			mem[ad2+1] = 0xF0;
			mem[ad2+2] = (unsigned char) ptbl->code;
		}
		else
		{
			//
			mem[ad  ] = 0xED;
			mem[ad+1] = 0xF0;
			mem[ad+2] = (unsigned char) ptbl->code;
//		dprintf("Patch $%04x code=%02X\n",ad,ptbl->code);
		}
	}


}

/*
    Patch ROM1
 */
void patch_rom1(void)
{
	patch1_job(rom1_pattbl);
}

void exec_rom1(Z80_Regs *Regs)
{
	int code;

	code = Z80_RDMEM(Regs->PC.W.l++);

//	dprintf("exec_rom1(%d)\n",code);
	(*(rom1_functbl[code]))(Regs);
}

//
void set_tapepos(int pos)
{
	tapepos = pos;
}

//
void clr_rdinf_buf(void)
{
	ZeroMemory(rdinf_buf, sizeof(rdinf_buf) );
}

//////////////////////////////////////////////////
// VRAM SCROLL UP
//////////////////////////////////////////////////
static void pem_scroll(void)
{
	// TEXT SCROLL
	CopyMemory(mem + VID_START, mem + VID_START + 40, (40*24) );
	// COLOR SCROLL
	CopyMemory(mem + VID_START + 0x800, mem + VID_START + 0x800 + 40, (40*24) );

	// 最下行の消去
	ZeroMemory(mem + VID_START + (24*40) , 40);
	FillMemory(mem + VID_START + 0x800 + (24*40), 40, (bk_color) ? bk_color : 0x71);

	mz_refresh_screen(REFSC_ALL);
}

//////////////////////////////////////////////////
// EXEC CONTROL CODE
//////////////////////////////////////////////////
static void pem_qdpct(Z80_Regs *Regs, int a)
{
	int x,y;

	x = Z80_RDMEM(L_DSPXY);
	y = Z80_RDMEM(L_DSPXY + 1);

	Regs->AF.B.h = a;

	switch (a)
	{
		case 0xC0:
			//SCROL
			pem_scroll();
			break;

		case 0xC1:
			//CURSD
			if (y == 24)
				pem_scroll();
			else
				y++;

			break;

		case 0xC2:
			//CURSU
			if (y) 
				y--;
			break;

		case 0xC3:
			//CURSR
			x++;
			if (x>39) 
			{
				x=0;
				if (y == 24)
					pem_scroll();
				else
					y++;
			}
			break;

		case 0xC4:
			//CURSL
			x--;
			if (x<0)
			{
				if (y)
				{
					x=39;
					y--;
				}
				else x=0;
			}
			break;

		case 0xC5:
			//HOME
			x = y = 0;
			break;

		case 0xC6:
			//CLRS
			x = y = 0;
			ZeroMemory(mem + RAM_START + L_MANG, 27);
			ZeroMemory(mem + VID_START , 0x400);
			FillMemory(mem + VID_START + 0x800, 0x400, (bk_color) ? bk_color : 0x71);
			mz_refresh_screen(REFSC_ALL);
			break;

		case 0xC7:
			//DEL
			break;

		case 0xC8:
			//INST
			break;

		case 0xC9:
			//ALPHA
			Z80_WRMEM(L_KANAF, 0);
			break;

		case 0xCA:
			//KANA
			Z80_WRMEM(L_KANAF, 1);
			break;

		case 0xCB:
			//?RSTR
			break;

		case 0xCC:
			//?RSTR
			break;

		case 0xCD:
			//CR
			a = Z80_RDMEM(L_SWRK);
			Z80_WRMEM(L_SWRK, (a & 0x7F) );		// SHIFT CLR
			em__mang(Regs);
			if (Regs->AF.B.h & 1)
			{
				x=0;
				y++;
			}
			else
			{
				x=0;
				y++;
				if (y>=25)
				{
					y=24;
					pem_scroll();
				}
			}
			break;

		case 0xCE:
			//?RSTR
			break;

		case 0xCF:
			//?RSTR
			break;

		default:;
			return;
	}

	Z80_WRMEM(L_DSPXY, x);
	Z80_WRMEM(L_DSPXY + 1, y);
}


/* Return only */
void MON_ret(Z80_Regs *Regs)
{
	ret();
}

/* */
void rdinf_job(Z80_Regs *Regs, int mode)
{
	int r = 0;
	FILE_HDL tfp;													// テープ用ファイルハンドル
	
	 dprintf("rdinf_job()\n");

	 //
	tfp = FILE_ROPEN(tapefile);
	if (tfp != FILE_VAL_ERROR)
	{
		FILE_SEEK(tfp,tapepos,FILE_SEEK_SET);
		r = FILE_READ(tfp,rdinf_buf,0x80);							/* read header */
	}

	dprintf("tapepos=%d r=%d\n",tapepos,r);
	 
	if (tfp==FILE_VAL_ERROR || (r!=0x80))
	{
		dprintf("seek error\n");
		/* シークエラー処理 */
		if (tfp != FILE_VAL_ERROR)
		{
			FILE_CLOSE(tfp);										/* すでにOpenしてたらClose */
			tfp=FILE_VAL_ERROR;
		}

		/* エラーです */
		Z80_set_carry(Regs,1);
		Regs->AF.B.h = 1;
		ret();														/* Load Error */
		return;
	}

	/* ファイルのクローズ */
	FILE_CLOSE(tfp);
	tfp = FILE_VAL_ERROR;

	//
	switch (mode)
	{
	case CPAT_ROM:
		// ROM MONITOR
		CopyMemory(mem+RAM_START+L_IBUFE, rdinf_buf, 0x80);
		break;
	case CPAT_RAM_1Z007B:
		// S-BASIC 1Z-007B
		CopyMemory(mem+RAM_START+Regs->HL.D, rdinf_buf, 0x80);
		break;
	case CPAT_RAM_HUBASIC:
		// HuBASIC 2.0
		CopyMemory(mem+RAM_START+Regs->HL.D, rdinf_buf, 0x80);
		break;
	}
	tapepos+=128;
	
	/* エラー無し */
	Z80_set_carry(Regs, 0);
	Regs->AF.B.h = 0;

	ret();
}

/* RDINF */
void MON_rdinf (Z80_Regs *Regs)
{
	 dprintf("rdinf()\n");
	rdinf_job(Regs, CPAT_ROM);
}

/* RDDATA */
void rddata_job(Z80_Regs *Regs, int mode)
{
	FILE_HDL tfp = FILE_VAL_ERROR;									// テープ用ファイルハンドル
	int errflg = 0;
	int r = 0;
	
	dprintf("rddata_job()\n");
	
	switch (mode)
	{
	case CPAT_ROM:
		//	ROM MONITOR
		inf_start = mem[RAM_START+L_IBUFE+0x14] | (mem[RAM_START+L_IBUFE+0x15] << 8);
		inf_len  = mem[RAM_START+L_IBUFE+0x12] | (mem[RAM_START+L_IBUFE+0x13] << 8);
		break;

	case CPAT_RAM_1Z007B:
		//	S-BASIC 1Z-007B
		inf_start = Regs->HL.D;
		inf_len = Regs->BC.D;
		break;

	case CPAT_RAM_HUBASIC:
		// HuBASIC
		inf_start = Regs->HL.D;
		inf_len = Regs->BC.D;
		break;
	}

	//
	 tfp = FILE_ROPEN(tapefile);
	 if (tfp != FILE_VAL_ERROR)
	 {
		 FILE_SEEK(tfp,tapepos,FILE_SEEK_SET);
		 r = FILE_READ(tfp, mem+RAM_START+inf_start,inf_len);					/* read the rest */
	 }
	 else
	 {
		errflg = 1;
	 }

	 dprintf("tapepos=%d r=%d\n",tapepos,r);

	 if ((errflg) || (inf_len != r))
	 {
		 if (tfp != FILE_VAL_ERROR)
		 {
			 FILE_CLOSE(tfp);
			 tfp = FILE_VAL_ERROR;
		 }

		 // error
		dprintf("read error\n");
		 Z80_set_carry(Regs,1);
		 Regs->AF.B.h = 1;
		 ret();
		 return;
	 }

	 /* ok */
	 /* close */
	 FILE_CLOSE(tfp);
	 tfp = FILE_VAL_ERROR;
	 tapepos += r;
	
	 /* RAM_PATCH */
	 if (mode == CPAT_ROM)
	 {
		patch_ram(rdinf_buf);
	 }

	 Z80_set_carry(Regs,0);
	 Regs->AF.B.h = 0;
	 ret();
}

/* RDDATA */
void MON_rddata (Z80_Regs *Regs)
{
	dprintf("rddata()\n");

	rddata_job(Regs, CPAT_ROM);
}

/* WRINF */
void wrinf_job(Z80_Regs *Regs, int mode)
{
	int r;
	unsigned char buf[128];
	FILE_HDL ofp;
	
	 dprintf("wrinf_job()\n");

	ofp = FILE_AOPEN(SaveTapeFile);
	if (ofp != FILE_VAL_ERROR)
	{
		switch (mode)
		{
		case CPAT_ROM:
			CopyMemory(buf, mem+RAM_START+L_IBUFE, 0x80);
			break;
		case CPAT_RAM_1Z007B:
			CopyMemory(buf, mem+RAM_START+Regs->HL.D, 0x80);
			break;
		case CPAT_RAM_HUBASIC:
			CopyMemory(buf, mem+RAM_START+Regs->HL.D, 0x80);
			break;
		}

		r = FILE_WRITE(ofp, buf,0x80);								/* write header */
		FILE_CLOSE(ofp);
	}

	ret();
}

/* WRINF */
void MON_wrinf (Z80_Regs *Regs)
{
	 dprintf("wrinf()\n");

	 wrinf_job(Regs, CPAT_ROM);
}

/* WRDATA */
void wrdata_job(Z80_Regs *Regs, int mode)
{
	int a,i,cou;
	FILE_HDL ofp;
	BYTE outbuf[1024];
	int len,addr;
	int errflg;
	
	dprintf("wrdata_job()\n");

	errflg = 0;
	
	switch (mode)
	{
	case CPAT_ROM:
		// ROM MONITOR
		inf_start = mem[RAM_START+L_IBUFE+0x14] | (mem[RAM_START+L_IBUFE+0x15] << 8);
		inf_len  = len = mem[RAM_START+L_IBUFE+0x12] | (mem[RAM_START+L_IBUFE+0x13] << 8);
		break;
	case CPAT_RAM_1Z007B:
		// S-BASIC 1Z-007B
		inf_start = Regs->HL.D;
		inf_len  = len = Regs->BC.D;
		break;
	case CPAT_RAM_HUBASIC:
		// HuBASIC
		inf_start = Regs->HL.D;
		inf_len  = len = Regs->BC.D;
		break;
	}
	addr = inf_start;

	ofp = FILE_AOPEN(SaveTapeFile);
	if (ofp != FILE_VAL_ERROR)
	{
		do
		{
			if (!len) break;

			cou=0;
			for (i=0; i<1024; i++)
			{
				outbuf[i] = Z80_RDMEM(addr++);
				cou++; len--;
				if (!len) break;
			}
			a = FILE_WRITE(ofp, outbuf, cou);
			if (a != cou)
			{
				dprintf("error\n");
				FILE_CLOSE(ofp);

				Z80_set_carry(Regs,1);
				Regs->AF.B.h = 1;
				ret();
				return;
			}
				
		} while (len>0);
		
		/* ok */
		FILE_CLOSE(ofp);
	}
	else
	{
		errflg = 1;														/* error */
		dprintf("error\n");
	}

	
	//----
	if (!errflg)
	{
		Z80_set_carry(Regs,0);
		Regs->AF.B.h = 0;
	}
	else
	{
		Z80_set_carry(Regs,1);
		Regs->AF.B.h = 1;
	}
	 ret();
}

/* WRDATA */
void MON_wrdata (Z80_Regs *Regs)
{
	dprintf("wrdata()\n");

	wrdata_job(Regs, CPAT_ROM);
}

/* VERIFY */
void MON_verfy (Z80_Regs *Regs)
{
	dprintf("verify()\n");

	/* dummy */
	Z80_set_carry(Regs, 0);		// cflag clr
	Regs->AF.B.h = 0;
}

/* ?MODE */
void MON_qmode (Z80_Regs *Regs)
{
	dprintf("?MODE:\n");

	mmio_out(0xE003,0x8A);
	mmio_out(0xE003,0x07);
	mmio_out(0xE003,0x05);

	ret();
}

// TIMIN
void MON_timin(Z80_Regs *Regs)
{
	byte val1,val2;
	word val_w;
	dprintf("TIMIN:\n");

	mem[RAM_START+0x119B] ^= 1;		// AMPM
	mmio_out(0xE007,0x80);
	val1 = mmio_in(0xE006);
	val2 = mmio_in(0xE006);
	val_w = val1 | (val2<<8);
	val_w += (0xA8C0 - 2);
	mmio_out(0xE006,val1);
	mmio_out(0xE006,val2);
	
	ei();
	ret();
}

//------------------------------
//	GETKEY
//	NO ECHO BACK
//	EXIT ACC=ASCII CODE
void em_qget(Z80_Regs *Regs)
{
	byte res,val;

	res = 0;

	em_qkey(Regs);
	if (Z80_get_carry(Regs))
//	if (Regs->AF.B.l & C_FLAG)
	{
		val = Z80_RDMEM(L_SWRK);
		Z80_WRMEM(L_SWRK, (val | 0x80) );
	}

	val = Regs->AF.B.h;
	if (val == 0xF0)
	{
		res = 0;
		Z80_set_carry(Regs, 0);		// cflag clr
		Z80_set_zero(Regs, 1);		// zflag set
	}
	else
	{
		em_qdacn(Regs);		// DISPLAY TO ASCII CODE
		res = Regs->AF.B.h;
		Z80_set_zero(Regs, 0);		// zflag clr
		Z80_set_carry(Regs, 1);		// cflag set
	}

	Regs->AF.B.h = res;

	M_CSTATE(1200);
}

void MON_qget(Z80_Regs *Regs)
{
//	dprintf("QGET:\n");
	em_qget(Regs);
	ret();
}

//---------------------------------------
//	1 KEY INPUT
//	IN   B=KEY MODE (SHIFT, CTRL, BREAK)
//	     C=KEY DATA (COLUMN & ROW)
//	EXIT ACC=DISPLAY CODE
//	     IF NO KEY  ACC=F0H
//	     IF CY=1 THEN ATTRIBUTE ON
//	                  (SMALL, HIRAKANA)
void em_qkey(Z80_Regs *Regs)
{
	int k;
	byte sf,md;
	word bc_bak,de_bak,hl_bak;

	bc_bak = Regs->BC.D;
	de_bak = Regs->DE.D;
	hl_bak = Regs->HL.D;

	Z80_set_carry(Regs,0);		// CARRY CLR
	em_qswep(Regs);

	sf = Regs->BC.B.h;

//	M_CSTATE(120);

	if (!(sf & 0x80))				// NO KEY
	{
		Regs->AF.B.h = 0xF0;
		Regs->BC.D = bc_bak;
		Regs->DE.D = de_bak;
		Regs->HL.D = hl_bak;
		return;
	}

	if (sf == 0x88)					// SHIFT & BRK
	{
		Regs->AF.B.h = 0xCB;		// BREAK CODE
		Regs->BC.D = bc_bak;
		Regs->DE.D = de_bak;
		Regs->HL.D = hl_bak;
		return;
	}

	k = (int) Regs->BC.B.l;

	if (sf & 0x20)		// CTRL
	{
		Regs->AF.B.h = k2disp_j[k + 0xC0];
	}
	else
	{
		md = Z80_RDMEM(L_KANAF);
		if (md & 1)				// KANA
		{
			if (sf & 0x40)		// SHIFT
			{
				// SMALL KANA
				if (k >=0x22)
				{
					k -= 0x22;
					Regs->AF.B.h = k2disp_j[k + 0x140];
				}
				else
					Regs->AF.B.h = 0xF0;
			}
			else
			{
				Regs->AF.B.h = k2disp_j[k + 0x100];
			}
		}
		else
		if (md & 2)
		{
			// GRAPH
			Regs->AF.B.h = k2disp_j[k + 0x80];
		}
		else
		{
			// NORMAL
			Regs->AF.B.h = k2disp_j[k];
			if (sf & 0x40)				// shift
			{
				Z80_set_carry(Regs, 1);
			}
		}

	}

	Regs->BC.D = bc_bak;
	Regs->DE.D = de_bak;
	Regs->HL.D = hl_bak;
}

void MON_qkey(Z80_Regs *Regs)
{
//	dprintf("?KEY:\n");
	em_qkey(Regs);
	ret();
}


//------------------------------
//	KEY BOARD SWEEP :$
//	EXIT B,D7=0  NO DATA
//	         =1  DATA
//	       D6=0  SHIFT OFF
//			 =1  SHIFT ON
//	       D5=0  CTRL OFF
//			 =1  CTRL ON
//		   D4=0  SHIFT+CTRL OFF
//			 =1  SHIFT+CTRL ON
//		 C   = ROW & COLUMN
//		7 6 5 4 3 2 1 0
//		* * ^ ^ ^ < < <
//------------------------------
void em_qswep(Z80_Regs *Regs)
{
	int col,bit,i;

	Regs->BC.B.h = 0;	// SHIFT,CTRL
	Regs->BC.B.l = 0;	// 00_RRR_CCC

	// Break Check
	if (!(keyports[8] & 0x81)) // Shift+Break
	{
		Regs->BC.B.h = 0x88;
		return;
	}

	//---- Shift + CTRL Check
	if (!(keyports[8] & 0x41)) // Shift+Ctrl
		Regs->BC.B.h |= 0x10;
	else
	if (!(keyports[8] & 0x40)) // Ctrl
		Regs->BC.B.h |= 0x20;
	else
	if (!(keyports[8] & 0x01)) // Shift
		Regs->BC.B.h |= 0x40;

	//---- Row & columns Check
	//
	for (col = 0; col < 8; col++)
	{
		if (keyports[col] != 0xFF)
		{
			for (i = 0, bit = 0x80; i < 8; i++ )
			{
				if (!(keyports[col] & bit))
				{
					Regs->BC.B.h |= 0x80;
					Regs->BC.B.l = ((col << 3) | i);	// 00_CCC_RRR
					return;

				}
				bit >>= 1;
			}
			break;
		}

	}

	return;
}

//
void MON_qswep(Z80_Regs *Regs)
{
	dprintf("?SWEP:\n");

	em_qswep(Regs);
	ret();
}

//---------------------------------------
//	BREAK KEY CHECK
//	AND SHIFT, CTRL KEY CHECK
//	EXIT BREAK ON : ZERO=1
//	           OFF: ZERO=0
//	     NO KEY   : CY  =0
//	     KEY IN   : CY  =1
//	      A D6=1  : SHIFT ON
//	          =0  :       OFF
//	        D5=1  : CTRL ON
//	          =0  :      OFF
//			D4=1  : SHIFT+CNT ON
//			  =0  :           OFF
void em_qbrk(Z80_Regs *Regs)
{
	Z80_set_carry(Regs, 0);			// cflag clr
	Z80_set_zero(Regs, 0);			// zflag clr
	
	// Break Check
	if (!(keyports[8] & 0x81)) // Shift+Break
	{
		Regs->AF.B.h = 0x00;
		Z80_set_zero(Regs, 1);		// zflag set
		return;
	}

	//---- Shift + CTRL Check
	if (!(keyports[8] & 0x41)) // Shift+Ctrl
	{
		Regs->AF.B.h = 0x10;
		Z80_set_carry(Regs, 1);		// cflag set
	}
	else
	if (!(keyports[8] & 0x40)) // Ctrl
	{
		Regs->AF.B.h = 0x20;
		Z80_set_carry(Regs, 1);		// cflag set
	}
	else
	if (!(keyports[8] & 0x01)) // Shift
	{
		Regs->AF.B.h = 0x40;
		Z80_set_carry(Regs, 1);		// cflag set
	}

}
	
void MON_qbrk(Z80_Regs *Regs)
{
//	dprintf("?BRK:\n");

	em_qbrk(Regs);
	ret();
}

//------------------------------------
//	ASCII TO DISPLAY CODE CONVERT
//	IN ACC:ASCII
//	EXIT ACC:DISPLAY CODE
void em_qadcn(Z80_Regs *Regs)
{
	byte in;

	in = Regs->AF.B.h;

	Regs->AF.B.h = asc2disp_j[in];
}
void MON_qadcn(Z80_Regs *Regs)
{
	em_qadcn(Regs);
	ret();
}

//------------------------------------------
//	DISPLAY CODE TO ASCII CONVERSION
//	IN ACC=DISPLAY CODE
//	EXIT ACC=ASCII
void em_qdacn(Z80_Regs *Regs)
{
	int i;
	byte in;

	in = Regs->AF.B.h;

	for (i=0;i<256;i++)
	{
		if (in == asc2disp_j[i])
		{
			Regs->AF.B.h = (byte) i;
			return;
		}
	}

}
void MON_qdacn(Z80_Regs *Regs)
{
	em_qdacn(Regs);
	ret();
}

//-------------------------------------
//	COMPUTE POINT ADDRESS
//	HL=SCREEN COORDINATE (x,y)
//	EXIT HL=POINT ADDRESS ON SCREEN
//-------------------------------------
void em_qpont(Z80_Regs *Regs)
{
	int x,y;
	word vrm;

	x = mem[RAM_START + L_DSPXY];
	y = mem[RAM_START + L_DSPXY + 1];
//	y = Regs->HL.B.h;
//	x = Regs->HL.B.l;

	vrm = 0xD000 + x + (y*40);

	Regs->HL.B.h = (byte) (vrm >> 8);
	Regs->HL.B.l = (byte) (vrm & 0xFF);
}

void MON_qpont(Z80_Regs *Regs)
{
	dprintf("?PONT:\n");

	em_qpont(Regs);
	ret();
}

//-------------------------------------
//	COMPUTE POINT ADDRESS
//	HL=SCREEN COORDINATE (x,y)
//	EXIT HL=POINT ADDRESS ON SCREEN
//-------------------------------------
void em_qpnt1(Z80_Regs *Regs)
{
	int x,y;
	word vrm;

	y = Regs->HL.B.h;
	x = Regs->HL.B.l;

	vrm = 0xD000 + x + (y*40);

	Regs->HL.B.h = (byte) (vrm >> 8);
	Regs->HL.B.l = (byte) (vrm & 0xFF);
}

void MON_qpnt1(Z80_Regs *Regs)
{
	dprintf("?PNT1:\n");

	em_qpnt1(Regs);
	ret();
}


//------------------------------------
// ?DSP:
//	DISPLAY ON POINTER
//	ACC=DISPLAY CODE
//	EXCEPT F0H
//------------------------------------
void em_qdsp(Z80_Regs *Regs)
{
	int vrm;
	word af,bc,de,hl;

	byte x,y;
	byte val,a,sml;

	// KEEP Regs
	af = Regs->AF.D;
	bc = Regs->BC.D;
	de = Regs->DE.D;
	hl = Regs->HL.D;

	//
	em_qpont(Regs);

	sml = 0;

	vrm = Regs->HL.D;
	if (Regs->AF.B.h == 0xF5)		// small/large
	{
		x = Z80_RDMEM(L_SWRK);
		x ^= 0x40;
//		Z80_WRMEM(L_SWRK, x & 0x3F);
		Z80_WRMEM(L_SWRK, x);

		// Restore Regs
		Regs->AF.D = af;
		Regs->BC.D = bc;
		Regs->DE.D = de;
		Regs->HL.D = hl;
		return;
	}

	//--
	a = Regs->AF.B.h;

	x = Z80_RDMEM(L_SWRK);		// 0x80 = shift / 0x40 = LargeSmall Flg
	if (x & 0xC0) sml = 1;

	val = Z80_RDMEM((dword)vrm + 0x800);			// color RAM
	Z80_WRMEM(vrm + 0x800 , ((!sml) ? (val & 0x7F) : (val | 0x80)) );		// ATTR
	Z80_WRMEM(vrm , a);						// TEXT VRAM

	//--
	x = Z80_RDMEM(L_DSPXY);
	y = Z80_RDMEM(L_DSPXY + 1);
	if (x==39)
	{
		em__mang(Regs);
//		if (!(Regs->AF.B.l & C_FLAG))
		if (!Z80_get_carry(Regs))
		{
			Z80_WRMEM(L_MANG,1);
			Z80_WRMEM(L_MANG+1,0);
		}
	}
	

	Regs->AF.B.h = 0xC3;		// CURSL
	em_qdpct(Regs);

	// Restore Regs
	Regs->AF.D = af;
	Regs->BC.D = bc;
	Regs->DE.D = de;
	Regs->HL.D = hl;
}

void MON_qdsp(Z80_Regs *Regs)
{
	dprintf("?DSP:\n");

	em_qdsp(Regs);
	ret();
}

//------------------------------------
// ?PRNT:
void em_qprnt(Z80_Regs *Regs)
{
	word wk;
	byte v;

	wk = Regs->BC.D;

	v = Regs->AF.B.h;
	if (v == 0x0D)
	{
		em_qltnl(Regs);
		Regs->BC.D = wk;
		return;
	}

	Regs->BC.B.h = v;
	Regs->BC.B.l = v;
	Regs->AF.B.h = Regs->BC.B.l;		// A=C
	em_qprt(Regs);

	Regs->AF.B.h = Regs->BC.B.h;

	Regs->BC.D = wk;
}

void MON_qprnt(Z80_Regs *Regs)
{
	dprintf("?PRNT:\n");
	em_qprnt(Regs);
	ret();
}

void em_prnt3(Z80_Regs *Regs)
{
	int x;

	em_qdsp(Regs);
	x = Z80_RDMEM(L_DPRNT);
	x++;
	if (x >= 80)
		x -= 80;
	Z80_WRMEM(L_DPRNT,x);
}

void MON_prnt3(Z80_Regs *Regs)
{
	dprintf("PRNT3:\n");
	em_prnt3(Regs);
	ret();
}

//------------------------------------
//	PRINT ROUTINE
//	1 CHARACTER
//	INPUT:C=ASCII DATA (QDSP+QDPCT)
void em_qprt(Z80_Regs *Regs)
{
	byte a;

	em_qadcn(Regs);
	a = Regs->AF.B.h;
	Regs->BC.B.l = a;					// C=A

	if (a == 0xF0) return;

	if ( (a >= 0xC0) && (a <= 0xC6) )
	{
		// control code
		em_qdpct(Regs);
	}
	else
	{
		em_prnt3(Regs);
	}

}

void MON_qprt(Z80_Regs *Regs)
{
	dprintf("?PRT:\n");
	em_qprt(Regs);
	ret();
}

//-----------------------------
//	DISPLAY CONTROL
//	ACC=CONTROL CODE
void em_qdpct(Z80_Regs *Regs)
{
	pem_qdpct(Regs, (int)Regs->AF.B.h);
}

void MON_qdpct(Z80_Regs *Regs)
{
	dprintf("?DPCT:\n");
	em_qdpct(Regs);
	ret();
}

//	CRT MANAGEMENT
//	EXIT HL:DSPXY H=Y,L=X
//	DE:MANG ADR. (ON DSPXY)
//	A :MANG DATA
//	CY:MANG=1
void em__mang(Z80_Regs *Regs)
{
	byte x,y;
	byte v,a;
	int ofs;

	x = Z80_RDMEM(L_DSPXY);
	y = Z80_RDMEM(L_DSPXY + 1);

	ofs = L_MANG + (int) y;
	
	v = Z80_RDMEM(ofs);

	ofs++;
	a = Z80_RDMEM(ofs);
	a <<= 1;
	Z80_WRMEM(ofs, a);

	v |= a;

	a = Z80_RDMEM(ofs);
	a >>= 1;
	Z80_WRMEM(ofs, a);

	if (v & 1)
	{
		v = (0x80)|(v>>1);
//		Regs->AF.B.l |= (C_FLAG);
		Z80_set_carry(Regs,1);
	}
	else
	{
		v >>= 1;
//		Regs->AF.B.l &= (~C_FLAG);
		Z80_set_carry(Regs,0);
	}
	Regs->AF.B.h = v;

	Regs->DE.B.h = (ofs >> 8);
	Regs->DE.B.l = (ofs & 0xFF);

	Regs->HL.B.h = y;
	Regs->HL.B.l = x;
}

void MON__mang(Z80_Regs *Regs)
{
	dprintf(".MANG:\n");
	em__mang(Regs);
	ret();
}


// NEWLINE
void em_qltnl(Z80_Regs *Regs)
{
	Z80_WRMEM(L_DPRNT, 0);		// ROW POINTER=0
	pem_qdpct(Regs, 0xCD);		// 0xCD = CR
}

void MON_qltnl(Z80_Regs *Regs)
{
	dprintf("?LTNL:\n");
	em_qltnl(Regs);
	ret();
}

// NEWLINE
void em_qnl(Z80_Regs *Regs)
{
	int x;

	x = Z80_RDMEM(L_DPRNT);			// ROW POINTER
	if (x) em_qltnl(Regs);
}

void MON_qnl(Z80_Regs *Regs)
{
	dprintf("?NL:\n");
	em_qnl(Regs);
	ret();
}

// ?MSG
void em_qmsg(Z80_Regs *Regs)
{
	word af_bak,bc_bak,de_bak;
	byte val;

	af_bak = Regs->AF.D;
	bc_bak = Regs->BC.D;
	de_bak = Regs->DE.D;

	for (;;)
	{
		val = Z80_RDMEM(Regs->DE.D);
		if (val == 0x0D) break;

		Regs->AF.B.h = val;
		em_qprnt(Regs);

		Regs->DE.D++;
	}
	Regs->DE.D = de_bak;
	Regs->BC.D = bc_bak;
	Regs->AF.D = af_bak;
}

void MON_qmsg(Z80_Regs *Regs)
{
	dprintf("?MSG:\n");
	em_qmsg(Regs);
	ret();
}

// ?MSGX
void em_qmsgx(Z80_Regs *Regs)
{
	word af_bak,bc_bak,de_bak;
	byte val;

	af_bak = Regs->AF.D;
	bc_bak = Regs->BC.D;
	de_bak = Regs->DE.D;
	for (;;)
	{
		val = Z80_RDMEM(Regs->DE.D);
		if (val == 0x0D) break;

		Regs->AF.B.h = val;
		em_qadcn(Regs);
		em_qdsp(Regs);

		val = Z80_RDMEM(L_DPRNT);			// ROW POINTER
		val++;
		if (val >= 80)
			val = 0;
		Z80_WRMEM(L_DPRNT, val);

		Regs->DE.D++;
	}
	Regs->DE.D = de_bak;
	Regs->BC.D = bc_bak;
	Regs->AF.D = af_bak;

}

void MON_qmsgx(Z80_Regs *Regs)
{
	dprintf("?MSGX:\n");
	em_qmsgx(Regs);
	ret();
}

//-----------------------------------------------------------------
// PRINT SPACE
//-----------------------------------------------------------------
void em_qprts(Z80_Regs *Regs)
{
	Regs->AF.B.h = 0x20;
	em_qprnt(Regs);
}

void MON_qprts(Z80_Regs *Regs)
{
	dprintf("?PRTS:\n");
	em_qprts(Regs);
	ret();
}

//-----------------------------------------------------------------
// PRINT TAB
//-----------------------------------------------------------------
void em_qprtt(Z80_Regs *Regs)
{
	int val;

	for (;;)
	{
		em_qprts(Regs);
		val = (int) Z80_RDMEM(L_DPRNT);
		if (!val) return;
		for (;;)
		{
			val -= 10;
			if (val<0) break;		// goto ?PRTS
			if (!val)
			{
				em_qprnt(Regs);
				return;
			}
		}
	}

}

void MON_qprtt(Z80_Regs *Regs)
{
	dprintf("?PRTT:\n");
	em_qprtt(Regs);
	ret();
}

void MON_mldst(Z80_Regs *Regs)
{
	byte val_h,val_l;
	
//	dprintf("MLDST:\n");

	val_l = Z80_RDMEM(L_RATIO);
	val_h = Z80_RDMEM(L_RATIO+1);

	if (!val_h)
		em_mldsp();
	else
	{
		Z80_WRMEM(0xE004,val_l);
		Z80_WRMEM(0xE004,val_h);
		Z80_WRMEM(0xE008,0x01);
	}

	ret();
}
void em_mldsp(void)
{
	Z80_WRMEM(0xE007,0x36);
	Z80_WRMEM(0xE008,0x00);
}

void MON_mldsp(Z80_Regs *Regs)
{
	dprintf("MLDSP:\n");

	em_mldsp();
	ret();
}

void em_qbel(void)
{
	const word prm = 0x03F8;
	
	Z80_WRMEM(0xE004, (prm & 0xFF));
	Z80_WRMEM(0xE004, (prm >> 8));
	Z80_WRMEM(0xE008, (0x01) );

}

void MON_qbel(Z80_Regs *Regs)
{
	dprintf("?BEL:\n");

	em_qbel();
}

//	HEXADECIMAL TO ASCII
//	IN  : ACC (D3-D0)=HEXADECIMAL
//	EXIT: ACC = ASCII
void em_asc(Z80_Regs *Regs)
{
	int val;

	val = (int) ( Regs->AF.B.h & 0x0F);
	Regs->AF.B.h = hextbl[val];
}

void MON_asc(Z80_Regs *Regs)
{
	dprintf("ASC:\n");

	em_asc(Regs);
	ret();
}

//	ASCII TO HEXADECIMAL
//	IN  : ACC = ASCII
//	EXIT: ACC = HEXADECIMAL
//	      CY  = 1 ERROR
void em_hex(Z80_Regs *Regs)
{
	int i;

	for (i=0;i<16;i++)
	{
		if (Regs->AF.B.h == hextbl[i])
		{
			Regs->AF.B.h = hextbl[i];
			Z80_set_carry(Regs,0);
			return;
		}
	}
	
	Z80_set_carry(Regs,1);
}

void MON_hex(Z80_Regs *Regs)
{
	dprintf("HEX:\n");

	em_hex(Regs);
	ret();
}

//
int pem_gethex(Z80_Regs *Regs, int b)
{
	int i;
	int val;
	int res = 0;

	for (i=0;i<b;i++)
	{
		res <<= 4;
		val = (int) Z80_RDMEM(Regs->DE.D++);
		if ( ((val >= '0') && (val <='9')) |
			 ((val >= 'A') && (val <='F')) )
		{
			if ((val >= '0') && (val <='9'))
			{
				val -= '0';
			}
			else
			{
				val -= ('A' - 10);
			}

			res |= val;
		}
		else
		{
			Z80_set_carry(Regs,1);
			return res;
		}

	}

	Z80_set_carry(Regs,0);
	return res;
}


//	4 ASCII TO (HL)
//	IN  DE=DATA LOW ADDRESS
//	EXIT CF=0 : OK
//	       =1 : OUT
void em_hlhex(Z80_Regs *Regs)
{
	word de_bak;

	de_bak = Regs->DE.D;
	Regs->HL.D = (dword) pem_gethex(Regs, 4);
	Regs->DE.D = de_bak;
}

void MON_hlhex(Z80_Regs *Regs)
{
	dprintf("HLHEX:\n");

	em_hlhex(Regs);
	ret();
}

//	2 ASCII TO (ACC)
//	IN  DE=DATA LOW ADRRESS
//	EXIT CF=0 : OK
//	       =1 : OUT
void em_2hex(Z80_Regs *Regs)
{
	Regs->AF.B.h = (byte) pem_gethex(Regs, 2);
}

void MON_2hex(Z80_Regs *Regs)
{
	dprintf("2HEX:\n");

	em_2hex(Regs);
	ret();
}

void em_prthx(Z80_Regs *Regs)
{
	word af_bak;

	af_bak = Regs->AF.D;
	Regs->AF.B.h >>=4;
	em_asc(Regs);
	em_qprnt(Regs);
	Regs->AF.D = af_bak;

	em_asc(Regs);
	em_qprnt(Regs);
}

void MON_prthx(Z80_Regs *Regs)
{
	dprintf("PRTHX:\n");
	em_prthx(Regs);
	ret();
}

void em_prthl(Z80_Regs *Regs)
{
	Regs->AF.B.h = Regs->HL.B.h;
	em_prthx(Regs);
	Regs->AF.B.h = Regs->HL.B.l;
	em_prthx(Regs);
}

void MON_prthl(Z80_Regs *Regs)
{
	dprintf("PRTHL:\n");
	em_prthl(Regs);
	ret();
}

void em_sphex(Z80_Regs *Regs)
{
	em_qprts(Regs);
	Regs->AF.B.h = Z80_RDMEM(Regs->HL.D);
	em_prthx(Regs);
	Regs->AF.B.h = Z80_RDMEM(Regs->HL.D);
}

void MON_sphex(Z80_Regs *Regs)
{
	dprintf("SPHEX:\n");
	em_sphex(Regs);
	ret();
}


// FLASHING DATA SAVE
void em_qsave(Z80_Regs *Regs)
{
	byte val,flsdt;

	val = Z80_RDMEM(L_KANAF);
	if (val & 1)
	{
		flsdt = 0x43;		// KANA cursor
	}
	else
	if (val & 2)
	{
		flsdt = 0xFF;		// GRAPH cursor
	}
	else
		flsdt = 0xEF;		// NORMAL cursor

	Z80_WRMEM(L_FLSDT, flsdt);

	em_qpont(Regs);
	val = Z80_RDMEM(Regs->HL.D);
	Z80_WRMEM(L_FLASH, val);

	Z80_WRMEM(Regs->HL.D, flsdt);		// Write CURSOR

	Z80_WRMEM(0xE000, 0x00);
	Z80_WRMEM(0xE000, 0xFF);

	Regs->AF.B.h = 0xFF;
	Regs->HL.D = 0xE000;

}

void MON_qsave(Z80_Regs *Regs)
{
	dprintf("?SAVE:\n");

	em_qsave(Regs);
	ret();
}

// FLASHING DATA LOAD
void em_qload(Z80_Regs *Regs)
{
	word af_bak;
	byte val;

	af_bak = Regs->AF.D;

	val = Z80_RDMEM(L_FLASH);
	em_qpont(Regs);
	Z80_WRMEM(Regs->HL.D, val);

	Regs->AF.D = af_bak;
}

void MON_qload(Z80_Regs *Regs)
{
	dprintf("?LOAD:\n");

	em_qload(Regs);
	ret();
}

// FLASHING 2
void em_qfls(Z80_Regs *Regs)
{
	word hl_bak;
	byte val,flsdt;

	hl_bak = Regs->HL.D;

	val = Z80_RDMEM(MIO_KEYPC);
	if (val & 0x40)
	{
		flsdt = Z80_RDMEM(L_FLSDT);
	}
	else
	{
		flsdt = Z80_RDMEM(L_FLASH);
	}

	em_qpont(Regs);
	Z80_WRMEM(Regs->HL.D, flsdt);

	Regs->HL.D = hl_bak;
}

void MON_qfls(Z80_Regs *Regs)
{
//	dprintf("?FLS:\n");

	em_qfls(Regs);
	ret();
}

// FLKEY
void em_flkey(Z80_Regs *Regs)
{
	em_qfls(Regs);
	em_qkey(Regs);
	Z80_set_zero(Regs, (Regs->AF.B.h == 0xF0));
	Z80_set_carry(Regs, (Regs->AF.B.h < 0xF0));

}

void MON_flkey(Z80_Regs *Regs)
{
//	dprintf("FLKEY:\n");

	em_flkey(Regs);
	ret();
}

// ??KEY
//	KEY BOARD SEARCH
//	& DISPLAY CODE CONVERSION
//	EXIT A=DISPLAY CODE
//	     CY=GRAPH MODE
//	WITH CURSOR DISPLAY
void em_qqkey(Z80_Regs *Regs)
{
	word af_bak,hl_bak;

	hl_bak = Regs->HL.D;
	af_bak = Regs->AF.D;

	em_qsave(Regs);
	em_flkey(Regs);

	
	
	em_qfls(Regs);
	em_qkey(Regs);
	Z80_set_zero(Regs, (Regs->AF.B.h == 0xF0));
	Z80_set_carry(Regs, (Regs->AF.B.h < 0xF0));

	Regs->HL.D = hl_bak;
	Regs->AF.D = af_bak;
}

void MON_qqkey(Z80_Regs *Regs)
{
	dprintf("??KEY:\n");

	em_qqkey(Regs);
}

void em_qcler(Z80_Regs *Regs, int val)
{
	int i,lp;

	lp = (int) Regs->BC.B.h;
	for (i=0;i<lp;i++)
	{
		Z80_WRMEM(Regs->HL.D++,val);
	}

	Regs->AF.B.h = 0;
	Regs->BC.B.h = 0;
}

void MON_qcler(Z80_Regs *Regs)
{
	dprintf("?CLER:\n");

	em_qcler(Regs, 0);
	ret();
}

void MON_qclrff(Z80_Regs *Regs)
{
	dprintf("?CLRFF:\n");

	em_qcler(Regs, 0xFF);
	ret();
}

void em_dsp03(Z80_Regs *Regs)
{
	word wk;
	byte val,c;

	val = Z80_RDMEM(Regs->HL.D);
	c = (val & 0x80) ? 1 : 0;
	val <<=1;
	Regs->BC.B.h = val | c;
	Z80_set_carry(Regs,c);

	wk = Regs->HL.D;
	Regs->HL.D = Regs->DE.D;
	Regs->DE.D = wk;

	Regs->AF.B.h = 0;
}

void MON_dsp03(Z80_Regs *Regs)
{
	dprintf("DSP03:\n");

	em_dsp03(Regs);
	ret();
}

