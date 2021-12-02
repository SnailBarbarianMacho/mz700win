/*  $Id: MZhw.h 24 2010-01-30 12:07:43Z maru $ */

#ifdef __cplusplus
extern "C" {
#endif 

/* MZ-1R23,1R24サポート */
#define KANJIROM

//
enum
{
  MB_ROM1 = 0,
  MB_ROM2,
  MB_RAM,
  MB_VRAM,
  MB_DUMMY,
  MB_FONT,
  MB_PCGB,
  MB_PCGR,
  MB_PCGG,
};

//
typedef struct
{
	UINT16		ofs;									// メモリオフセット
	UINT8		base;									// メモリブロック番号
	UINT8		attr;									// メモリ読み書きアトリビュート
} TMEMBANK;

/* HARDWARE STATUS (MZ-700) */
typedef struct
{
	TMEMBANK	memctrl[32];			/* Memory Bank Controller */
	int		pb_select;					/* selected kport to read */
	int		mz1r12_addr;				/* MZ-1R12 (C-MOS RAM Board) */
	int		pcg700_data;
	int		pcg700_addr;
	int		cursor_cou;
	UINT16	pcg700_mode;
	UINT8	tempo_strobe;
	UINT8	motor;						/* bit0:カセットモーター bit2:データレコーダのスイッチセンス*/
	UINT8	retrace;
} THW700_STAT;

/* TSTATES (MZ-700) */
typedef struct
{
	int		cpu_tstates;
	int		tempo_tstates;
	int		vsync_tstates;
	int		pit_tstates;
	int		cmt_tstates;
} T700_TS;

/* HARDWARE STATUS (MZ-1500) */
typedef struct
{
	int			e5_bak;
	int			mz1r18_addr;			/* MZ-1R18 (RAM FILE Board)アドレス */
#ifdef KANJIROM
	int			mz1r23_ctrl;			/* MZ-1R23 (漢字ROM)コントロール */
	int			mz1r23_addr;			/* MZ-1R23 (漢字ROM)アドレス */
	int			mz1r24_addr;			/* MZ-1R24 (辞書ROM)アドレス */
#endif /*KANJIROM*/
	TMEMBANK	memctrl_bak[6];			/* Memory Bank Controller バックアップ */
	UINT16		prty;					/* プライオリティ */

} THW1500_STAT;

/* TSTATES (MZ-700) */
typedef struct
{
	int		pio_tstates;
} T1500_TS;

/* ８２５３ステータス型 */
typedef struct
{
	UINT8 bcd;															/* BCD */
	UINT8 mode;															/* MODE */
	UINT8 rl;															/* RL */
	UINT8 rl_bak;														/* RL Backup */
	int lat_flag;														/* latch flag */
	int running;
	int counter;														/* Counter */
	WORD counter_base;													/* CounterBase */
	WORD counter_out;													/* Out */
	WORD counter_lat;													/* Counter latch */
	WORD bit_hl;														/* H/L */

} T8253_STAT;

typedef struct
{
	UINT8 bcd;
	UINT8 m;
	UINT8 rl;
	UINT8 sc;
	UINT8 int_mask;
	UINT8 beep_mask;
	UINT8 makesound;
	UINT8 setsound;
} T8253_DAT;

/* Z80PIO */
typedef struct
{
	UINT8 mode;															/* mode word */
	UINT8 intw;															/* inturrupt control word: bit4-7 */
	UINT8 intv;															/* interrupt vector */
	UINT8 iosel;														/* I/O select */
	UINT8 imask;														/* interrupt mask */
	UINT8 cont;															/* control flag */
	
	UINT8 pin;															/* IN DATA */
	UINT8 pout;															/* OUT DATA */
	
} TZ80PIO_STAT;

/* MZ-1500PSG */
typedef struct
{
	UINT16 dosound;
	UINT8  setsound;
	UINT8  padding;
	short  psgvol;
	int	   psgfreq;
	int	   psgfreq_bak;
	
} TMZ1500PSG;

#define DOSND_NULL	0x0000
#define DOSND_FREQ	0x0001
#define DOSND_VOL	0x0002
#define DOSND_ALL	0x0003
#define DOSND_STOP	0x0004

void update_membank(void);
//
int mmio_in(int);
void mmio_out(int,int);

//---- Z80 ------------
void calc_state(int codest);

#ifdef __cplusplus
extern "C"  {
#endif
void Z80_Reti(void);
void Z80_Retn(void);
int Z80_Interrupt(void);
#ifdef __cplusplus
};
#endif

void Z80_set_carry(Z80_Regs *, int);
int Z80_get_carry(Z80_Regs *);
void Z80_set_zero(Z80_Regs *, int);
int Z80_get_zero(Z80_Regs *);

//------------------
void mzsnd_init(void);
void mz_reset(void);
void mz_main(void);
void mz_psg_init(void);

void mz_keyport_init(void);
void mz_keydown(WPARAM);
void mz_keyup(WPARAM);

void pit_init(void);
int pio_pitcount(void);
int pitcount_job(int,int);

void play8253(void);
void playPSG(void);

int pit_count(void);
void vblnk_start(void);

//	
#ifndef MZHW_H_  

extern int IFreq;
extern int CpuSpeed;

/* HARDWARE STATUS WORK */
extern THW700_STAT		hw700;
extern T700_TS			ts700;
extern THW1500_STAT		hw1500;
extern T1500_TS			ts1500;

/* 8253関連ワーク */
extern T8253_DAT		_8253_dat;
extern T8253_STAT		_8253_stat[3];

/* PIO関連ワーク */
extern TZ80PIO_STAT		Z80PIO_stat[2];

/* MZ-1500PSG */
extern TMZ1500PSG		mz1500psg[8];									// (3+1)*2
	
extern int rom1_mode;													/* ROM-1 判別用フラグ */
extern int rom2_mode;													/* ROM-2 判別用フラグ */

/* memory */
extern UINT8	*mem;													/* Main Memory */
extern UINT8	*junk;													/* to give mmio somewhere to point at */

extern UINT8	*mz1r12_ptr;											/* pointer for MZ-1R12 */
extern UINT8	*mz1r18_ptr;											/* pointer for MZ-1R18 (Ram File) */

/* FONT ROM/RAM */	
extern UINT8 *font;														/* Font ROM (4K) */
extern UINT8 *pcg700_font;												/* PCG700 font (2K) */
extern UINT8 *pcg1500_font_blue;										/* MZ-1500 PCG-BLUE (8K) */
extern UINT8 *pcg1500_font_red;											/* MZ-1500 PCG-RED (8K) */
extern UINT8 *pcg1500_font_green;										/* MZ-1500 PCG-GREEN (8K) */

#ifdef KANJIROM
extern UINT8 *mz1r23_ptr;												/* pointer for MZ-1R23 */
extern UINT8 *mz1r24_ptr;												/* pointer for MZ-1R24 */
#endif /*KANJIROM*/


	
extern UINT8	keyports[10];
#endif

#ifdef __cplusplus
}
#endif 
