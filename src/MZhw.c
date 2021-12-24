//----------------------------------------------------------------------------
// File:MZhw.c
// MZ-700/1500 Emulator MZ700WIN for Windows9x/NT/2000
// mz700win:Hardware Emulation Module
// ($Id: MZhw.c 26 2010-02-16 13:34:31Z maru $)
//
// 'mz700win' by Takeshi Maruyama, based on Russell Marks's 'mz700em'.
// Z80 emulation from 'Z80em' Copyright (C) Marcel de Kogel 1996,1997
//----------------------------------------------------------------------------

#define MZHW_H_

#include <windows.h>
#include "dprintf.h"

#include "resource.h"

#include "mz700win.h"

#include "fileio.h"
#include "win.h"
#include "fileset.h"

#include "Z80.h"
#include "Z80codes.h"

#include "defkey.h"
#include "MZhw.h"
#include "mzmon1em.h"
#include "mzmon2em.h"

#include "MZmain.h"
#include "MZddraw.h"
#include "MZscrn.h"
#include "dssub.h"

#include "mzbeep.h"
#include "sn76489an.h"

#define TEMPO_STROBE_TIME		14								/* テンポビット生成間隔 */
#define PIT_DEBUG 0

/*** for z80.c ***/
int IFreq = 60;
int CpuSpeed=100;
int Z80_IRQ;

/* for memories of MZ */
UINT8	*mem;													/* Main Memory */
UINT8	*junk;													/* to give mmio somewhere to point at */

UINT8 *mz1r12_ptr;												/* pointer for MZ-1R12 */
UINT8 *mz1r18_ptr;												/* pointer for MZ-1R18 (Ram File) */

#ifdef KANJIROM
UINT8 *mz1r23_ptr;												/* pointer for MZ-1R23 */
UINT8 *mz1r24_ptr;												/* pointer for MZ-1R24 */
#endif /*KANJIROM*/

UINT8 *font;													/* Font ROM (4K) */
UINT8 *pcg700_font;												/* PCG700 font (2K) */
UINT8 *pcg1500_font_blue;										/* MZ-1500 PCG-BLUE (8K) */
UINT8 *pcg1500_font_red;										/* MZ-1500 PCG-RED (8K) */
UINT8 *pcg1500_font_green;										/* MZ-1500 PCG-GREEN (8K) */

int rom1_mode;													/* ROM-1 判別用フラグ */
int rom2_mode;													/* ROM-2 判別用フラグ */

/* HARDWARE STATUS WORK */
THW700_STAT		hw700;
T700_TS			ts700;

THW1500_STAT	hw1500;
T1500_TS		ts1500;

/* 8253関連ワーク */
T8253_DAT		_8253_dat;
T8253_STAT		_8253_stat[3];

/* PIO関連ワーク */
TZ80PIO_STAT	Z80PIO_stat[2];

/* MZ-1500PSG */
TMZ1500PSG		mz1500psg[8];											// (3+1)*2

/* for Keyboard Matrix */
UINT8 keyports[10] ={ 0,0,0,0,0, 0,0,0,0,0 };

// MZのメインメモリのバンク－実メモリ対応表
static UINT8	*memptr[32];

//
void update_membank(void)
{
	TMEMBANK *mb;
	UINT8 *baseptr;
	int i;
	UINT8 at;

	for (i=0; i<32; i++)
	{
		mb = &hw700.memctrl[i];
		at = 0;

		switch (mb->base)
		{
		case MB_ROM1:
			baseptr = mem + ROM_START;
			break;

		case MB_ROM2:
			baseptr = mem + ROM2_START;
			break;

		case MB_RAM:
			baseptr = mem + RAM_START;
			at = 1;
			break;

		case MB_VRAM:
			baseptr = mem + VID_START;
			at = 5;
			break;

		case MB_DUMMY:
			baseptr = junk;
			at = 2;
			break;

		case MB_FONT:
			baseptr = font;
			at = 0;
			break;

		case MB_PCGB:
			baseptr = pcg1500_font_blue;
			at = 5;
			break;

		case MB_PCGR:
			baseptr = pcg1500_font_red;
			at = 5;
			break;

		case MB_PCGG:
			baseptr = pcg1500_font_green;
			at = 5;
			break;

		default:
			baseptr = junk;
			break;
		}

		mb->attr = at;
		memptr[i] = baseptr + (UINT32) mb->ofs;
	}

}

//
void vblnk_start(void)
{
	ts700.vsync_tstates = ts700.cpu_tstates;
}

// Keyport Buffer Clear
void mz_keyport_init(void)
{
	FillMemory(keyports, 10, 0xFF );
}

///////////////
// WM_KEYDOWN
///////////////
void mz_keydown(WPARAM wParam)
{
	int i = (int) wParam;
	UINT8 *kptr = get_keymattbl_ptr() + (menu.keytype << 8);
	int n,b;

	if (kptr[i]==0xFF) return;
	
#if _DEBUG	
	if ( Z80_Trace ) return;
#endif
	
	n = kptr[i] >> 4;
	b = kptr[i] & 0x0F;
	keyports[n] &= (~(1 << b));
#if _DEBUG
//	dprintf("keydown : kptr = %02x idx%d bit%d\n",i,n,b);
#endif	
	
}

////////////
// WM_KEYUP
////////////
void mz_keyup(WPARAM wParam)
{
	int i = (int) wParam;
	UINT8 *kptr = get_keymattbl_ptr() + (menu.keytype << 8);
	int n,b;
	
	if (kptr[i]==0xFF) return;

#if _DEBUG	
	if ( Z80_Trace ) return;
#endif	
	
	n = kptr[i] >> 4;
	b = kptr[i] & 0x0F;
	keyports[n] |= (1 << b);
#if _DEBUG
//	dprintf("keyup : kptr = %02x idx%d bit%d\n",i,n,b);
#endif	
}

/* ＭＺをリセットする */
void mz_reset(void)
{
	int i, a;
	TMEMBANK *mp;
	Z80_Regs StartRegs;

	/* ８２５３ワークの初期化 */
	ZeroMemory(&_8253_stat,sizeof(_8253_stat));
	ZeroMemory(&_8253_dat,sizeof(_8253_dat));

	/* ＭＺワークの初期化 */
	ZeroMemory(&hw700,sizeof(hw700));
	ZeroMemory(&ts700,sizeof(ts700));
	ZeroMemory(&hw1500,sizeof(hw1500));
	ZeroMemory(&ts1500,sizeof(ts1500));

	/* ＰＩＴ初期化 */
	pit_init();

	/* Ｚ８０レジスタの初期化 */
	Z80_SetRegs(&StartRegs);

	/* バンク構成を初期化 */
	for (a=0,i=0; i<26; i++)
	{
		mp = &hw700.memctrl[i];
		mp->ofs = a;
		mp->base = MB_RAM;
		a += 0x800;
	}

	Z80_Out(0xE4,0);			// $0000-$0FFF:ROM  $E000-$FFFF:MMIO/ROM2
	update_membank();

	/* 1500ＰＳＧ初期化 */
	mz_psg_init();

	/* キーポートの初期化 */
	mz_keyport_init();

	/* RDINFwork Clear */
	clr_rdinf_buf();

    if (rom1_mode <= MON_OTHERS || rom1_mode >= MON_NEWMON700)
    {
        // モニタワークの初期化
        ZeroMemory(mem + RAM_START + 0x1000, 0x200);
        // VRAMの初期化
        ZeroMemory(mem + VID_START , 0x0400);
        FillMemory(mem + VID_START + 0x800 , 0x0400, (bk_color) ? bk_color : 0x71);
    }
	// 画面bltフラグ
	mz_refresh_screen(REFSC_ALL);

	Z80_intflag=0;
	
	mz_palet_init();											/* 1500パレット初期化 */

//	hw700.pcg700_mode=8;
	hw700.tempo_strobe=1;

	hw700.retrace=1;
	hw700.motor=1;												/* カセットモーター */

	/* サウンド(8253#0)の初期化 */
	_8253_dat.beep_mask=1;
	_8253_dat.int_mask=4;
	
	hw1500.e5_bak = -1;
	
	/* Ｚ８０のリセット */
	Z80_Reset();

	/* スレッド　開始 */
	start_thread();
}

/////////////////////////////////////////////////////////////////
// 8253 PIT
/////////////////////////////////////////////////////////////////

//////////////
// PIT 初期化
//////////////
void pit_init(void)
{
	T8253_STAT *st;
	int i;

	for (i=0;i<2;i++)
	{
		st = &_8253_stat[i];
		st->rl = st->rl_bak = 3;
		st->counter_base = st->counter = st->counter_lat = 0x0000;
		st->counter_out = 0;
		st->lat_flag = 0;
	}

	_8253_dat.int_mask = 0;
	
	_8253_dat.bcd = 0;
	_8253_dat.m = 0;
	_8253_dat.rl = 3;
	_8253_dat.sc = 0;
}

////////////////////////////
// 8253 control word write
////////////////////////////
void write_8253_cw(int cw)
{
	T8253_STAT *st;
	
	_8253_dat.bcd = cw & 1;
	_8253_dat.m = (cw >> 1) & 7;
	_8253_dat.rl = (cw >> 4) & 3;
	_8253_dat.sc = (cw >> 6) & 3;

	st = &_8253_stat[_8253_dat.sc];

	// counter 0 のモードをセットしたら, カウンタが止まるので, 音も止まります. なげやり対応 Snail 2021.12.23
	if (_8253_dat.sc == 0) {
		mzbeep_stop();
	}
	
//	if (_8253_dat.sc==2) st->counter_out = 0;								/* count#2,then clr */
    st->counter_out = 0;								/* count clr */

	/* カウンタ・ラッチ・オペレーション */
	if (_8253_dat.rl==0)
	{
		st->counter_lat = st->counter;
		st->lat_flag = 1;
#if PIT_DEBUG	
	dprintf("Count%d latch\n",_8253_dat.sc);
#endif
		return;
	}

	st->bit_hl = 0;
	st->lat_flag = 0;
	st->running = 0;
	st->rl = _8253_dat.rl;
	st->bcd = _8253_dat.bcd;
	st->mode = _8253_dat.m;

#if PIT_DEBUG	
	dprintf("Count%d mode=%d rl=%d bcd=%d\n",_8253_dat.sc,_8253_dat.m,
		_8253_dat.rl,_8253_dat.bcd);
#endif
	
}

////////////////////////////
// Z80 PIO Control Register
////////////////////////////
void mz_z80pio_ctrl(int port,int value)
{
	TZ80PIO_STAT *pio;
	
	pio = &Z80PIO_stat[port];

	if (pio->cont == 1)
	{
		pio->iosel = value;
		pio->cont = 0;
#if _DEBUG
		dprintf("Z80PIO:%d:iosel=$%02x\n",port,pio->iosel);
#endif		
		return;
	}
	else
	if (pio->cont == 2)
	{
		pio->imask = value;
		pio->cont = 0;
#if _DEBUG
		dprintf("Z80PIO:%d:imask=$%02x\n",port,pio->imask);
#endif		
		return;
	}
		
	if ( (value & 0x0F) == 0x0F)										/* モード・ワードであることの印 */
	{
		pio->mode = (value >> 6) & 3;
#if _DEBUG
		dprintf("Z80PIO:%d:mode=%d\n",port,pio->mode);
#endif
		if (pio->mode == 3)
			pio->cont = 1;												/* mode 2nd */
	}
	else
	if ( (value & 0x0F) == 0x07)										/* 割込み制御ワードであることの印 */
	{
		pio->intw = value & 0xF0;
#if _DEBUG
		dprintf("Z80PIO:%d:intw=$%02x\n",port,pio->intw);
#endif		
		if (pio->intw & 0x10)
			pio->cont = 2;												/* mask follows 2nd */
	}
	else
	if ( (value & 0x0F) == 0x03)										/* 割込み許可ワードであることの印 */
	{
		pio->intw &= 0x70;
		pio->intw |= (value & 0x80);
#if _DEBUG
		dprintf("Z80PIO:%d:intw=$%02x\n",port,pio->intw);
#endif		
	}
	else
	if ( (value & 0x01) == 0x00)										/* ベクトルワードであることの印 */
	{
		pio->intv = value;												/* ベクトル・ワード */
#if _DEBUG
		dprintf("Z80PIO:%d:intv=$%02x\n",port,pio->intv);
#endif		
	}
		
}

// Z80 PIO Data Register
void mz_z80pio_data(int port,int value)
{


}

// Z80PIO 割込み処理実行
void pio_intjob(int out)
{
	if (Z80PIO_stat[0].intw & 0x80)										/* 割り込み使用の有無 */
	{
//		dprintf("intv=$%02x\n",Z80PIO_stat[0].intv);
		Z80_intflag |= 2;
		if (out == 0)
			Z80PIO_stat[0].pin |= 0x10;									/* (INT 0)読み込まれるデータの初期化 */
		else
			Z80PIO_stat[0].pin |= 0x20;									/* (INT 1)読み込まれるデータの初期化 */
		Interrupt((int)(Z80PIO_stat[0].intv));
	}
}

// Z80PIO割り込み用カウンタ
int pio_pitcount(void)
{
	T8253_STAT *st;
	int out;
	
	st = &_8253_stat[0];
	out = 0;

	// pio int=INT1(A4)
	// pit int=INT0(A3) or CPU INT
	if (_8253_dat.makesound)											/* e008 makesound=gate */
	{
		if (st->mode != 3)												/* 方形波の繰り返し波形じゃなかったら処理 */
		{
			if (pitcount_job(0,PIO_TIMER_RESO))
			{
				out = 1;
			}
		}
	}
	
	return out;
}

//
int pitcount_job(int no,int cou)
{
	T8253_STAT *st;
	int out;
	
	st = &_8253_stat[no];
	out = 0;
	if (!st->running) return 0;

	switch (st->mode)
	{
		/* mode 0 */
	case 0:
		if (st->counter <= 0)
		{
			out=1;
			st->counter = 0;
		}
		else
		{
			st->counter-=cou;
		}
		break;

		/* mode 2 */
	case 2:
		st->counter-=cou;
		if (st->counter<=0)
		{
			/* カウンタ初期値セット */
			st->counter = (int) st->counter_base;
//			do{
//			st->counter += (int) st->counter_base;
//			}while (st->counter<0);
			out=1;														/* out pulse */
		}

		break;
		
	case 3:
		break;	

		/* mode 4 */
	case 4:
		st->counter-=cou;
		if (st->counter<=0)
		{
			/* カウンタ初期値セット */
			st->counter = -1;
			out=1;														/* out pulse (1 time only) */
		}
		break;

	default:
		break;
	}

	st->counter_out = out;
	return out;
}

//
int pit_count(void)
{
	int ret = 0;

	/* カウンタ１を進める */
	if (pitcount_job(1,1))
	{
		/* カウンタ２を進める */
		ret = pitcount_job(2,1);
	}

	return ret;
}

/////////////////////////////////////////////////////////////////
// 8253 SOUND
/////////////////////////////////////////////////////////////////
//
void play8253(void)
{
	int freq2,freqtmp;

	if (sound_di) return;

	if ((!_8253_dat.beep_mask) ) {
		if (_8253_dat.setsound)	{
			_8253_dat.setsound = 0;
			mzbeep_stop();
		}
		return;
	}

	// サウンドを鳴らす
	freqtmp = _8253_stat[0].counter_base;		
	if (_8253_dat.makesound == 0) {
	  _8253_dat.setsound = 0;
		mzbeep_stop();
	}
	else
	if (freqtmp > 0) {
		// play
		freq2 = (895000 / freqtmp);
		_8253_dat.setsound = 1;
		mzbeep_setFreq(freq2);
	}
	else
	{
		// stop
		mzbeep_stop();
	}
}

///////////////////////////////
// MZ700,1500 sound initiailze
///////////////////////////////
void mzsnd_init(void)
{
	mzbeep_stop();
}

// PSG Initialize
void mz_psg_init(void)
{
	if (sound_di) return;

	sn76489an_reset();
}

// PSG Out
void mz_psg_out(int port,int value)
{
	// No Use
}

// PSG Play
void playPSG(void)
{
	// No Use
}

////////////////////////////////////////////////////////////
// Memory Access
////////////////////////////////////////////////////////////
// READ
unsigned Z80_RDMEM(dword x)
{
	int page = x >> 11;
#ifdef _DEBUG
	if (32 <= page) {
		_ASSERT(FALSE);
	}
#endif
	int attr = hw700.memctrl[page].attr;

	if (!attr)
	{
		M_CSTATE(1);													/* wait */
		
	}
	if (attr & 4)
	{
//		if (hw700.retrace)												/* not V-blank? */
		{
			M_CSTATE(VRAM_ACCESS_WAIT);									/* wait */
		}
	}
	return (attr&2)?mmio_in(x):memptr[page][x&0x7FF];
}

// WRITE
void Z80_WRMEM(dword x, dword y)
{
	unsigned int page = x >> 11;
#ifdef _DEBUG
	if (32 <= page) {
		_ASSERT(FALSE);
	}
#endif		  
	int attr = hw700.memctrl[page].attr;
		  
	if(attr&2) mmio_out(x,y);
	else
	if(attr)
	{
		memptr[page][x&0x7FF] = (BYTE) y;
		if (attr & 4)
		{
			mz_blt_screen();											/* VRAMアクセスがあればRefresh */
//			if (hw700.retrace)											/* not V-blank? */
			{
				M_CSTATE(VRAM_ACCESS_WAIT);								/* wait */
			}
		}
	}
}

////////////////////////////////////////////////////////////
// Memory-mapped I/O Access
////////////////////////////////////////////////////////////
// IN
int mmio_in(int addr)
{
	int tmp;
	T8253_STAT *st;

	switch(addr)
	{
	case 0xE000:
		return 0xff;
		
	case 0xE001:
		/* read keyboard */
		return keyports[hw700.pb_select];
		
	case 0xE002:
		/* bit 4 - motor (on=1)
		 * bit 5 - tape data
		 * bit 6 - cursor blink timer
		 * bit 7 - vertical blanking signal (retrace?)
		 */
		tmp=((hw700.cursor_cou%25>15) ? 0x40:0);						/* カーソル点滅タイマー */
		tmp=(((hw700.retrace^1)<<7)|(hw700.motor<<4)|tmp|0x0F);						
		return tmp;
		
	case 0xE003:
		return 0xff;
		
		/* ＰＩＴ関連 */
	case 0xE004:
	case 0xE005:
	case 0xE006:
		st = &_8253_stat[addr-0xE004];
		
		if (st->lat_flag)
		{
			/* カウンタラッチオペレーション */
			switch (st->rl)
			{
			case 1:
				/* 下位８ビット呼び出し */
				st->lat_flag = 0;						// counter latch opelation: off
				return (st->counter_lat & 255);
			case 2:
				/* 上位８ビット呼び出し */
				st->lat_flag = 0;						// counter latch opelation: off
				return (st->counter_lat >> 8);
			case 3:
				/* カウンタ・ラッチ・オペレーション */
				if((st->bit_hl^=1)) return (st->counter_lat & 255);			/* 下位 */
				else
				{
					st->lat_flag = 0;					// counter latch opelation: off
					return (st->counter_lat>>8); 	/* 上位 */
				}

				break;
			default:
				return 0x7f;
			}
		}
		else
		{
			switch (st->rl)
			{
			case 1:
				/* 下位８ビット呼び出し */
				return (st->counter & 255);
			case 2:
				/* 上位８ビット呼び出し */
				return (st->counter>>8);
			case 3:
				/* 下位・上位 */
				if((st->bit_hl^=1)) return (st->counter & 255);
				else return (st->counter>>8);
			default:
				return 0xff;
			}
		}
		break;

	case 0xE008:
		/* 音を鳴らすためのタイミングビット生成 */
		return (0x7e | hw700.tempo_strobe);
		
	default:
		/* all unused ones seem to give 7Eh */
		return 0x7e;
	}
}

// OUT
void mmio_out(int addr,int val)
{
	T8253_STAT *st;
	int i;

	switch(addr)
	{
	case 0xE000:
		if (!(val & 0x80)) hw700.cursor_cou = 0;	// cursor blink timer reset

		hw700.pb_select = (val & 15);
		if (hw700.pb_select>9) hw700.pb_select=9;
		break;
		
	case 0xE001:
	  break;
  
	case 0xE002:
		/* bit 0 - 8253 mask
		 * bit 1 - cassete write data
		 * bit 2 - intmsk
		 * bit 3 - motor on
		 */
		_8253_dat.beep_mask = val & 0x01;
		_8253_dat.int_mask=val & 0x04;
#if _DEBUG		
		dprintf("write beep_mask=%d int_mask=%d\n",_8253_dat.beep_mask,_8253_dat.int_mask);
#endif
		play8253();
		break;
		
	case 0xE003:
		// bit0:   1=Set / 0=Reset
		// Bit1-3: PC0-7
		i = (val >> 1) & 7;
		if ((val & 1))
		{
			// SET
			switch (i)
			{
			case 0:
				_8253_dat.beep_mask = 1;
				break;

			case 1:
				// Cassete
				// 1 byte: long:stop bit/ 1=long 0=short *8
				ts700.cmt_tstates = 0;
#if _DEBUG
				dprintf("----\n");
#endif
				break;

			case 2:
				_8253_dat.int_mask = 4;
				break;
			}
		}
		else
		{
			// RESET
			switch (i)
			{
			case 0:
				_8253_dat.beep_mask = 0;
				break;

				// Cassete
				// 1 byte: long:stop bit/ 1=long 0=short *8
			case 1:
#if _DEBUG
				dprintf("cmt_tstates=%d\n",ts700.cmt_tstates);		// short=650,long=1252 ?
#endif
				break;

			case 2:
				_8253_dat.int_mask = 0;
				break;
			}
		}
		if (i==0 || i==2)
		{
#if _DEBUG		
			dprintf("E003:beep_mask=%d int_mask=%d\n",_8253_dat.beep_mask,_8253_dat.int_mask);
#endif
			play8253();
		}

		break;
		
	case 0xE004:
	case 0xE005:
	case 0xE006:
		/* カウンタに書き込み */
		st = &_8253_stat[addr-0xE004];
		if (st->mode == 0)												/* モード０だったら、outをクリア */
		{
			st->counter_out = 0;
		}
		st->lat_flag = 0;						// counter latch opelation: off

		switch(st->rl)
		{
		case 1:
			/* 下位８ビット書き込み */
			st->counter_base = (val&0x00FF);
			if (!st->running || (st->mode != 2 && st->mode != 3)) {
				st->counter = st->counter_base;
			}
#if PIT_DEBUG			
			dprintf("Count%d=$%04X\n",addr-0xe004,st->counter);
#endif
			st->bit_hl=0;
			st->running = 1;
			break;
		case 2:
			/* 上位８ビット書き込み */
			st->counter_base = (val << 8)&0xFF00;
			if (!st->running || (st->mode != 2 && st->mode != 3)) {
				st->counter = st->counter_base;
			}
#if PIT_DEBUG			
			dprintf("Count%d=$%04X\n",addr-0xe004,st->counter);
#endif
			st->bit_hl=0;
			st->running = 1;
			break;
			
		case 3:
			if (st->bit_hl)
			{
				st->counter_base = ((st->counter_base & 0xFF)|(val << 8))&0xFFFF;
				if (!st->running || (st->mode != 2 && st->mode != 3)) {
					st->counter = st->counter_base;
				}
#if PIT_DEBUG			
				dprintf("H:Count%d=$%04X\n",addr-0xe004,st->counter);
#endif
				st->running = 1;
			}
			else
			{
				st->counter_base = (val&0x00FF);
				if (!st->running || (st->mode != 2 && st->mode != 3)) {
					st->counter = st->counter_base;
				}
#if PIT_DEBUG			
				dprintf("L:Count%d=$%04X\n",addr-0xe004,st->counter);
#endif
			}
			st->bit_hl^=1;
			break;
		}

		/* サウンド用のカウンタの場合 */
		if (addr==0xE004)
		{
			if(!st->bit_hl)
			{
//				dprintf("freqtmp=$%04x makesound=%d\n",freqtmp,_8253_dat.makesound);
				play8253();
			}
		}
		break;
		
	case 0xE007:
#if _DEBUG
		dprintf("$E007=$%02X\n",val);
#endif
		/* PIO CONTROL WORD */
		write_8253_cw(val);
		break;
		
	case 0xE008:
#if _DEBUG
		dprintf("$E008=$%02X\n",val);
#endif
		/* bit 0: sound on/off */
		_8253_dat.makesound=(val&1);
		play8253();
		break;

		/* PCG700 Control */
		/* Data */
	case 0xE010:
		hw700.pcg700_data = val;
		break;

		/* Addr-L */
	case 0xE011:
		hw700.pcg700_addr &= 0xFF00;
		hw700.pcg700_addr |= val;
		break;

		/* Addr-H / Control */
	case 0xE012:
		hw700.pcg700_addr &= 0x00FF;
		hw700.pcg700_addr |= (val<<8);
		if ((val & (16+32))==(16+32))
		{
			/* ＣＧ→ＰＣＧコピー */
			i = hw700.pcg700_addr & 2047;
			if (i & 1024) i=2048+i;
			else i=1024+i;
			
			pcg700_font[hw700.pcg700_addr & 2047]=font[i];
		}
		else
		if (val & 16)
		{
			/* ＰＣＧ定義 */
			pcg700_font[hw700.pcg700_addr & 2047]=hw700.pcg700_data;
		}

		/* ＰＣＧ選択 */
		hw700.pcg700_mode = val & 8;
		break;
		
		
	default:;
		break;
	}
}

////////////////////////////////////////////////////////////
// I/O Port Access
////////////////////////////////////////////////////////////
// IN
byte Z80_In (word Port)
{
	int r=255;
	int iPort = Port & 0x00FF;

	switch (iPort)
	{
#ifdef KANJIROM
	case 0xb9:
		/* read data from MZ-1R23,1R24 */
		if (hw1500.mz1r23_ctrl & 0x80) {
			if (mz1r23_ptr != NULL)
				r = mz1r23_ptr[hw1500.mz1r23_addr++];
				hw1500.mz1r23_addr &= 0x1FFFF;
		} else {
			if (mz1r24_ptr != NULL)
				r = mz1r24_ptr[hw1500.mz1r24_addr];
		}
		break;
#endif

#ifdef ENABLE_FDC
		// FDC
	case 0xd8:
	case 0xd9:
	case 0xda:
	case 0xdb:
	case 0xdc:
	case 0xdd:
		r = mz_fdc_r(iPort);
//		dprintf("FDC_R %02x = %02x\n", iPort, r);
#endif //ENABLE_FDC
		break;

	case 0xe8:
		// Voice board
		// 暴走しないようにACK(bit4)に０を返す
		r = 0;
		break;

	case 0xea:
		// read data from MZ-1R18, then addr auto incliment.
		r = mz1r18_ptr[hw1500.mz1r18_addr++];
		hw1500.mz1r18_addr &= (MZ1R18_SIZE-1);
		break;
	case 0xf4:
		/* sio A data */
		break;
	case 0xf5:
		/* sio B data */
		break;
	case 0xf6:
		/* sio A control */
		break;
	case 0xf7:
		/* sio B control */
	    r=0x80;															/* dummy */
		break;
	case 0xf8:
		if (use_cmos)
		{
			/* MZ-1R12 addr counter reset  */
			hw700.mz1r12_addr = 0x0000;
		}
		break;
	case 0xf9:
		if (use_cmos)
		{
			/* read data from MZ-1R12, then addr auto incliment.  */
			r = mz1r12_ptr[hw700.mz1r12_addr];
			hw700.mz1r12_addr = ( hw700.mz1r12_addr + 1) & (MZ1R12_SIZE-1);
		}
		break;
	case 0xfe:
		/* z80 pio Port-A DATA Reg.*/
//		r = 0x10;														/* int0からのわりこみ？ */
//		r = 0x20;														/* int1からのわりこみ？ */
		r = Z80PIO_stat[0].pin;
		Z80PIO_stat[0].pin = 0xC0;										/* 読み込まれるデータの初期化 */
		break;
	case 0xff:
		/* z80 pio Port-B DATA Reg.*/
		r = 0xff;
		break;
	}
	
	return r;
}

// OUT
void Z80_Out(word Port,word Value)
{
	int iPort = (int) (Port & 0xFF);
	int iVal =(int) (Value & 0xFF);

	TMEMBANK *mp;


	switch (iPort) {
#ifdef KANJIROM
	case 0xb8:
		/* MZ-1R23 control */
		hw1500.mz1r23_ctrl = iVal & 0x00C3;
		break;
	case 0xb9:
		/* set address to MZ-1R23,1R24 */
		if (hw1500.mz1r23_ctrl & 0x80) {
			hw1500.mz1r23_addr = (((Port & 0xFF00) | iVal) & 0x0FFF) << 5;
		} else {
			hw1500.mz1r24_addr = ((hw1500.mz1r23_ctrl & 0x03) << 16) | (((Port & 0xFF00) | iVal) & 0xFFFF);
		}
		break;
#endif
		// FDC 
	case 0xd8:
	case 0xd9:
	case 0xda:
	case 0xdb:
	case 0xdc:
	case 0xdd:
#ifdef ENABLE_FDC
		dprintf("FDC_W %02x, %02x\n", iPort, iVal);
		mz_fdc_w(iPort, iVal);
#endif
		break;

	case 0xe0:
		/* make 0000-0FFF RAM */
		mp = &hw700.memctrl[0];
		mp->base = MB_RAM; mp->ofs = 0x000; mp++;
		mp->base = MB_RAM; mp->ofs = 0x800;
		update_membank();
		return;
		
	case 0xe1:
		/* make D000-FFFF RAM */
		mp = &hw700.memctrl[26];
		mp->base = MB_RAM; mp->ofs = 0xD000; mp++;
		mp->base = MB_RAM; mp->ofs = 0xD800; mp++;
		mp->base = MB_RAM; mp->ofs = 0xE000; mp++;
		mp->base = MB_RAM; mp->ofs = 0xE800; mp++;
		mp->base = MB_RAM; mp->ofs = 0xF000; mp++;
		mp->base = MB_RAM; mp->ofs = 0xF800;
		update_membank();
		return;
		
	case 0xe2:
		/* make 0000-0FFF ROM */
		mp = &hw700.memctrl[0];
		mp->base = MB_ROM1; mp->ofs = 0x0000; mp++;
		mp->base = MB_ROM1; mp->ofs = 0x0800;
		update_membank();
		return;
		
	case 0xe3:
		/* make D000-FFFF VRAM/MMIO */
		mp = &hw700.memctrl[26];
		mp->base = MB_VRAM; mp->ofs = 0x0000; mp++;
		mp->base = MB_VRAM; mp->ofs = 0x0800; mp++;
		mp->base = MB_DUMMY; mp->ofs = 0x0000; mp++;
		/* 1500/700と切り替えて動作させるとき */
		if (menu.machine == MACHINE_MZ1500)
		{
			mp->base = MB_ROM2; mp->ofs = 0x0000; mp++;
			mp->base = MB_ROM2; mp->ofs = 0x0800; mp++;
			mp->base = MB_ROM2; mp->ofs = 0x1000;
		}
		else
		{
			mp->base = MB_DUMMY; mp->ofs = 0x0800; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0000; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0800;
		}
		update_membank();
		return;
		
	case 0xe4:
		Z80_Out(0xe2,0);
		Z80_Out(0xe3,0);
		return;
		
	case 0xe5:
		if (hw1500.e5_bak < 0)
		{
			/* Backup Bank Data */
			CopyMemory(&hw1500.memctrl_bak,&hw700.memctrl[26],sizeof(TMEMBANK)*6);
			hw1500.e5_bak = iVal;
			update_membank();
		}
		/* Select PCG Block or CG */
		switch (iVal & 0x03)
		{
		case 0x00:
			/* CG BANK */
			mp = &hw700.memctrl[26];
			mp->base = MB_FONT; mp->ofs = 0x0000; mp++;
			mp->base = MB_FONT; mp->ofs = 0x0800; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0000; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0800; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0000; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0800;
			update_membank();
			break;

		case 0x01:
			/* PCG BLUE */
			mp = &hw700.memctrl[26];
			mp->base = MB_PCGB; mp->ofs = 0x0000; mp++;
			mp->base = MB_PCGB; mp->ofs = 0x0800; mp++;
			mp->base = MB_PCGB; mp->ofs = 0x1000; mp++;
			mp->base = MB_PCGB; mp->ofs = 0x1800; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0000; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0800;
			update_membank();
			break;
			
		case 0x02:
			/* PCG RED */
			mp = &hw700.memctrl[26];
			mp->base = MB_PCGR; mp->ofs = 0x0000; mp++;
			mp->base = MB_PCGR; mp->ofs = 0x0800; mp++;
			mp->base = MB_PCGR; mp->ofs = 0x1000; mp++;
			mp->base = MB_PCGR; mp->ofs = 0x1800; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0000; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0800;
			update_membank();
			break;
			
		case 0x03:
			/* PCG GREEN */
			mp = &hw700.memctrl[26];
			mp->base = MB_PCGG; mp->ofs = 0x0000; mp++;
			mp->base = MB_PCGG; mp->ofs = 0x0800; mp++;
			mp->base = MB_PCGG; mp->ofs = 0x1000; mp++;
			mp->base = MB_PCGG; mp->ofs = 0x1800; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0000; mp++;
			mp->base = MB_DUMMY; mp->ofs = 0x0800;
			update_membank();
			break;

		}
		return;
		
	case 0xe6:
		if (hw1500.e5_bak>=0)
		{
			/* Restore Before $e5 */
			CopyMemory(&hw700.memctrl[26],&hw1500.memctrl_bak,sizeof(TMEMBANK)*6);
			hw1500.e5_bak = -1;
			update_membank();
		}
		return;

	case 0xe9:
		// PSG 1 and 2 Out
		sn76489an_outreg(0, iVal);
		sn76489an_outreg(1, iVal);
		return;

	case 0xea:
		/* RAM FILE OUT */
		/* then addr auto incliment.  */
//		dprintf("out:MZ1R18_ADDR=%04x Out=%02x\n",(int)hw1500.mz1r18_addr, (int)Value);
		mz1r18_ptr[hw1500.mz1r18_addr++] = iVal;
		hw1500.mz1r18_addr &= (MZ1R18_SIZE-1);
		break;
		
	case 0xeb:
//		dprintf("$eb:RMFILE ADDR:Port=%04x Data=%02x\n",(int)Port, (int)Value);
		// RAM FILE
		// set MZ-1R18 addr
		hw1500.mz1r18_addr = (Port & 0xFF00) | iVal;
//		dprintf("$eb:MZ1R18_ADDR=%04x\n",hw1500.mz1r18_addr);
		break;
		
		
	case 0xf0:
		// mz-1500 priority
		hw1500.prty=iVal;

		mz_refresh_screen(REFSC_ALL);
		break;
		
	case 0xf1:
		// mz-1500 palet
		mz_palet(iVal);
		mz_refresh_screen(REFSC_ALL);
		break;

	case 0xf2:
		sn76489an_outreg(0, iVal);
		return;

	case 0xf3:
		sn76489an_outreg(1, iVal);
		return;

	case 0xf4:
		// sio A data
		break;
	case 0xf5:
		// sio B data
		break;
	case 0xf6:
		// sio A control
		break;
	case 0xf7:
		// sio B control
		break;

	case 0xf8:
		if (use_cmos)
		{
			// set MZ-1R12 addr (high)
			hw700.mz1r12_addr = (hw700.mz1r12_addr & 0x00FF) | (iVal << 8);
		}
		break;
	case 0xf9:
		if (use_cmos)
		{
			// set MZ-1R12 addr (low) 
			hw700.mz1r12_addr = (hw700.mz1r12_addr & 0xFF00) | iVal;
		}
		break;
	case 0xfa:
		if (use_cmos)
		{
			// write data from MZ-1R12, then addr auto incliment.
			mz1r12_ptr[hw700.mz1r12_addr] = iVal;
			hw700.mz1r12_addr = ( hw700.mz1r12_addr + 1) & (MZ1R12_SIZE-1);
		}
		break;
		// z80 pio Port-A Ctrl Reg.
	case 0xfc:
	case 0xfd:
		mz_z80pio_ctrl(((iPort&0xFF)-0xfc),iVal);
		break;
	case 0xfe:
		// z80 pio Port-A DATA Reg.
		mz_z80pio_data(0,iVal);
		break;
	}
	
	return;
}

/* Called when RETN occurs */
void Z80_Reti (void)
{
}

/* Called when RETN occurs */
void Z80_Retn (void)
{
}


#define cval (CPU_SPEED_BASE / 16000)
#define pit0cou_val (CPU_SPEED_BASE / (895000 / PIO_TIMER_RESO) )			/* 895kHz */

/* Interrupt Timer */
int Z80_Interrupt(void)
{
	int ret = 0;

	// Pit Interrupt (MZ-700)
	  if ( (ts700.cpu_tstates - ts700.pit_tstates) >= cval) {
		  ts700.pit_tstates += cval;
		  if (pit_count()) {
			  if (_8253_stat[2].counter_out) {
				  if ( _8253_dat.int_mask )
				  {
					  Z80_intflag |= 1;
					  Interrupt(0);
					}
				 }
			  }
		  }

	if (menu.machine == MACHINE_MZ1500)	{
		// Z80 PIO TIMER (MZ-1500)
		if ( (ts700.cpu_tstates - ts1500.pio_tstates) >= pit0cou_val) {
			ts1500.pio_tstates += pit0cou_val;
			if (pio_pitcount())	{
				pio_intjob(0);
			}
		}
	}
  
	return ret;
}

//--------
// NO Use
//--------
void Z80_Patch (Z80_Regs *Regs)
{

}

//----------------
// CARRY flag set
//----------------
void Z80_set_carry(Z80_Regs *Regs, int sw)
{
	if (sw)	{
		Regs->AF.B.l |= (C_FLAG);
	} else {
		Regs->AF.B.l &= (~C_FLAG);
	}
}

//----------------
// CARRY flag get
//----------------
int Z80_get_carry(Z80_Regs *Regs)
{
	return ((Regs->AF.B.l & C_FLAG) ? 1 : 0);
}

//---------------
// ZERO flag get
//---------------
void Z80_set_zero(Z80_Regs *Regs, int sw)
{
	if (sw)	{
		Regs->AF.B.l |= (Z_FLAG);
	} else {
		Regs->AF.B.l &= (~Z_FLAG);
	}
}

//---------------
// ZERO flag get
//---------------
int Z80_get_zero(Z80_Regs *Regs)
{
	return ((Regs->AF.B.l & Z_FLAG) ? 1 : 0);
}
