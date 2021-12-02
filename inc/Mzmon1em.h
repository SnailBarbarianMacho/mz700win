/* $Id: Mzmon1em.h 1 2007-07-27 06:17:19Z maru $ */

#ifdef __cplusplus
extern "C" {
#endif

// カセットアクセスパッチ処理用通し番号
enum TCPAT
{
	CPAT_ROM = 0,				// for Monitor-ROM
	CPAT_RAM_1Z007B,			// Japanese S-BASIC
	CPAT_RAM_HUBASIC,			// Japanese -BASIC
};

// パッチインデックス
enum TROM1NUM
{
	ROM1_ret = 0,
	ROM1_rdinf,
	ROM1_rddata,
	ROM1_wrinf,
	ROM1_wrdata,
	ROM1_verfy,
	ROM1_qget,
	ROM1_qbrk,
	
	ROM1__mang,
	ROM1_qmode,
	ROM1_timin,
	ROM1_qswep,
	ROM1_qadcn,
	ROM1_qdacn,
	ROM1_qkey,
	ROM1_qpont,
	ROM1_qpnt1,
	ROM1_qdsp,
	ROM1_qprnt,
	ROM1_prnt3,
	ROM1_qprt,
	ROM1_qdpct,

	ROM1_qltnl,
	ROM1_qnl,
	
	ROM1_qmsg,
	ROM1_qmsgx,
	ROM1_mldst,
	ROM1_mldsp,
	ROM1_qprts,
	ROM1_qprtt,
	
	ROM1_qbel,

	ROM1_asc,
	ROM1_hex,
	ROM1_hlhex,
	ROM1_2hex,
	ROM1_sphex,
	ROM1_prthl,
	ROM1_prthx,
	
	ROM1_qsave,
	ROM1_qload,
	ROM1_flkey,
	ROM1_qqkey,
	ROM1_qfls,

	ROM1_qcler,
	ROM1_qclrff,
	ROM1_dsp03,

	ROM1_end = -1

};

void patch1_job(TMONEMU *);
void patch_rom1(void);
void patch_emul_rom1(void);

void set_tapepos(int);
void clr_rdinf_buf(void);

void rdinf_job(Z80_Regs *Regs, int mode);
void rddata_job(Z80_Regs *Regs, int mode);
void wrinf_job(Z80_Regs *Regs, int mode);
void wrdata_job(Z80_Regs *Regs, int mode);

// Monitor Emulation by mz700win
static void pem_scroll(void);
static void pem_qdpct(Z80_Regs *Regs,int a);

//
void MON_ret(Z80_Regs *Regs);
void MON_rdinf(Z80_Regs *Regs);
void MON_rddata(Z80_Regs *Regs);
void MON_wrinf(Z80_Regs *Regs);
void MON_wrdata(Z80_Regs *Regs);
void MON_verfy(Z80_Regs *Regs);

void MON_qget(Z80_Regs *Regs);
void em_qget(Z80_Regs *Regs);

void MON_qmode(Z80_Regs *Regs);
void em_qswep(Z80_Regs *Regs);
void MON_qswep(Z80_Regs *Regs);
void em_qadcn(Z80_Regs *Regs);
void MON_qadcn(Z80_Regs *Regs);
void em_qdacn(Z80_Regs *Regs);
void MON_qdacn(Z80_Regs *Regs);
void em_qkey(Z80_Regs *Regs);
void MON_qkey(Z80_Regs *Regs);
void MON_timin(Z80_Regs *Regs);

void MON_qpont(Z80_Regs *Regs);
void em_qpont(Z80_Regs *Regs);
void MON_qpnt1(Z80_Regs *Regs);
void em_qpnt1(Z80_Regs *Regs);
void MON_qdsp(Z80_Regs *Regs);
void em_qdsp(Z80_Regs *Regs);
void MON_qprnt(Z80_Regs *Regs);
void em_qprnt(Z80_Regs *Regs);
void MON_qprt(Z80_Regs *Regs);
void em_qprt(Z80_Regs *Regs);
void MON_qdpct(Z80_Regs *Regs);
void em_qdpct(Z80_Regs *Regs);
void MON__mang(Z80_Regs *Regs);
void em__mang(Z80_Regs *Regs);

void MON_qltnl(Z80_Regs *Regs);
void em_qltnl(Z80_Regs *Regs);
void MON_qnl(Z80_Regs *Regs);
void em_qnl(Z80_Regs *Regs);

void MON_qmsg(Z80_Regs *Regs);
void em_qmsg(Z80_Regs *Regs);
void MON_qmsgx(Z80_Regs *Regs);
void em_qmsgx(Z80_Regs *Regs);

void MON_mldst(Z80_Regs *Regs);
void em_mldsp(void);
void MON_mldsp(Z80_Regs *Regs);

void em_qbel(void);
void MON_qbel(Z80_Regs *);

void em_qprts(Z80_Regs *);
void MON_qprts(Z80_Regs *);
void em_qprtt(Z80_Regs *);
void MON_qprtt(Z80_Regs *);
void em_prnt3(Z80_Regs *);
void MON_prnt3(Z80_Regs *);

void em_asc(Z80_Regs *);
void MON_asc(Z80_Regs *);
void em_hex(Z80_Regs *);
void MON_hex(Z80_Regs *);
void em_hlhex(Z80_Regs *);
void MON_hlhex(Z80_Regs *);
void em_2hex(Z80_Regs *);
void MON_2hex(Z80_Regs *);
void em_sphex(Z80_Regs *);
void MON_sphex(Z80_Regs *);
void em_prthl(Z80_Regs *);
void MON_prthl(Z80_Regs *);
void em_prthx(Z80_Regs *);
void MON_prthx(Z80_Regs *);

void em_qbrk(Z80_Regs *);
void MON_qbrk(Z80_Regs *);
void em_qsave(Z80_Regs *);
void MON_qsave(Z80_Regs *);
void em_qload(Z80_Regs *);
void MON_qload(Z80_Regs *);

void em_qfls(Z80_Regs *);
void MON_qfls(Z80_Regs *);
void em_flkey(Z80_Regs *);
void MON_flkey(Z80_Regs *);
void em_qqkey(Z80_Regs *);
void MON_qqkey(Z80_Regs *);

void em_qcler(Z80_Regs *, int);
void MON_qcler(Z80_Regs *);
void MON_qclrff(Z80_Regs *);
void em_dsp03(Z80_Regs *);
void MON_dsp03(Z80_Regs *);



#define PAT_ADR_VGOFF			0x0747

// パッチアドレス定義
#define PAT_ADR_RDINF			0x0027
//#define PAT_ADR_RDINF			0x04D8
#define PAT_ADR_RDDATA			0x002A
//#define PAT_ADR_RDDATA			0x04F8
#define PAT_ADR_WRINF			0x0021
#define PAT_ADR_WRDATA			0x0024

#define PAT_ADR_VERFY			0x002D

// for direct patch (jump table)
#define PAT_ADR_GETL			0x0003
#define PAT_ADR_LETNL			0x0006
#define PAT_ADR_NL				0x0009
#define PAT_ADR_PRNTS			0x000C
#define PAT_ADR_PRNTT			0x000F
#define PAT_ADR_PRNT			0x0012
#define PAT_ADR_MSG				0x0015
#define PAT_ADR_MSGX			0x0018
#define PAT_ADR_GETKY			0x001B
#define PAT_ADR_QGET			0x08BD
#define PAT_ADR_BRKEY			0x001E

#define PAT_ADR_VERFY			0x002D
#define PAT_ADR_MELDY			0x0030
#define PAT_ADR_TIMST			0x0033
#define PAT_ADR_TIMRD			0x003B
#define PAT_ADR_BELL			0x003E
#define PAT_ADR_QBEL			0x0577
#define PAT_ADR_QBEL2			0x0155
#define PAT_ADR_XTEMP			0x0041
#define PAT_ADR_MSTA			0x0044
#define PAT_ADR_MSTP			0x0047

#define PAT_ADR_ST1				0x00AD

// for direct patch
#define PAT_ADR_QKEY			0x08CA
#define PAT_ADR_QMODE			0x073E
#define PAT_ADR_TIMIN			0x038D
#define PAT_ADR_DSWEP			0x0854
#define PAT_ADR_QSWEP			0x0A50
#define PAT_ADR_QBLNK			0x0DA6
#define PAT_ADR_QADCN			0x0BB9
#define PAT_ADR_QDACN			0x0BCE
#define PAT_ADR_QPONT			0x0FB1
#define PAT_ADR_QPNT1			0x0FB4
#define PAT_ADR_QDSP			0x0DB5
#define PAT_ADR_QPRNT			0x0935
#define PAT_ADR_QPRT			0x0946
#define PAT_ADR_QDPCT			0x0DDC
#define PAT_ADR__MANG			0x02F3

#define PAT_ADR_MLDST			0x02AB
#define PAT_ADR_MLDSP			0x02BE

#define PAT_ADR_QLTNL			0x090E
#define PAT_ADR_QNL				0x0918

#define PAT_ADR_QPRT			0x0946
#define PAT_ADR_QPRNT			0x0935
#define PAT_ADR_PRNT3			0x096C

#define PAT_ADR_QMSG			0x0893
#define PAT_ADR_QMSGX			0x08A1

#define PAT_ADR_QPRTS			0x0920
#define PAT_ADR_QPRTT			0x0924

#define PAT_ADR_ASC				0x03DA
#define PAT_ADR_HEX				0x03F9
#define PAT_ADR_HLHEX			0x0410
#define PAT_ADR_2HEX			0x041F

#define PAT_ADR_SPHEX			0x03B1
#define PAT_ADR_PRTHL			0x03BA
#define PAT_ADR_PRTHX			0x03C3

#define PAT_ADR_QBRK			0x0A32

#define PAT_ADR_QSAVE			0x0B92
#define PAT_ADR_QLOAD			0x05F0
#define PAT_ADR_FLKEY			0x057E
#define PAT_ADR_QFLS			0x09EC
#define PAT_ADR_QFLAS			0x09FF
#define PAT_ADR_QQKEY			0x09B3

#define PAT_ADR_QCLER			0x0FD8
#define PAT_ADR_QCLRFF			0x0FDB
#define PAT_ADR_DSP03			0x0FAB

#define PAT_ADR_DLY1			0x0759
#define PAT_ADR_DLY2			0x0760
#define PAT_ADR_DLY3			0x0A4A
#define PAT_ADR_DLY12			0x09A6




// ROM1_WORK
#define L_TMPEX	0x0FFE
#define	L_IBUFE 0x10F0			// TAPE BUFFER (128 BYTES) for ROM-MONITOR

#define L_SIZE	0x1102
#define L_DTADR	0x1104
#define L_EXADR	0x1106
#define L_KANAF	0x1170
#define L_DSPXY	0x1171
#define L_MANG	0x1173
#define L_FLASH	0x118E
#define L_FLSDT	0x1192
#define L_DPRNT	0x1194
#define L_SWRK	0x119D
#define L_RATIO	0x11A1

// I/O
#define MIO_KEYPC	0xE002



//	
#ifndef MZMON1EM_H_ 
#define MZMON1EM_H_


	
#endif

#ifdef __cplusplus
}
#endif 
	
