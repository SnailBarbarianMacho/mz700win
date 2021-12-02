//----------------------------------------------------------------------------
// File:MZmain.c
// MZ-700/1500 Emulator MZ700WIN for Windows9x/NT/2000
// mz700win:Main Program Module ($Id: Mzmain.c 24 2010-01-30 12:07:43Z maru $)
//
// mz700win by Takeshi Maruyama, based on Russell Marks's 'mz700em'.
// MZ700 emulator 'mz700em' for VGA PCs running Linux (C) 1996 Russell Marks.
// Z80 emulation from 'Z80em' Copyright (C) Marcel de Kogel 1996,1997
//----------------------------------------------------------------------------

#define MZMAIN_H_

#include <windows.h>

#include "resource.h"
#include "mz700win.h"

#include "fileio.h"
#include "win.h"
#include "fileset.h"
#include "dprintf.h"

#include "Z80.h"
#include "Z80codes.h"

#include "defkey.h"
#include "mzmon1em.h"
#include "mzmon2em.h"

#include "MZmain.h"
#include "MZhw.h"
#include "MZddraw.h"
#include "MZscrn.h"
#include "DSOUND.h"
#include "mzbeep.h"
#include "sn76489an.h"
#include "MZXInput.h"

static HANDLE hCpuThread;
static DWORD CpuThreadID   = 0;

static CRITICAL_SECTION CriticalSection;

// ROM選択メニュー用ファイル名
static const char *rom_filelist[] =
{
	NULL,															/* Internal */
	"MZ700.ROM",													/* for Compatible */
	"NEWMON7.ROM",													/* MZ700 NEW-MONITOR */
	"1Z009.ROM",													/* 1Z-009A/B */
	"1Z013.ROM",													/* 1Z-013A/B */
	"NEWMON.ROM",													/* MZ NEW-MONITOR */
	"SP1002.ROM",													/* SP-1002 */
	"MZ1500.ROM",													/* MZ-1500 1Z-009B+9Z-502M */
};
// ROM選択メニュー用項目名
static const char *rom_menulist[] =
{
	"MZ-700(J) / Internal",											/* Internal */
	"User's \x22MZ700.ROM\x22",										/* User's MZ700.ROM */
	"MZ-700(J) / MZ700 NEW-MONITOR",								/* MZ700 NEW-MONITOR */
	"MZ-700(J) / 1Z-009A/B",										/* 1Z-009A/B */
	"MZ-700(E) / 1Z-013A/B",										/* 1Z-013A/B */
	"MZ-80K/C/1200(J) / MZ NEW-MONITOR",							/* MZ NEW-MONITOR */
	"MZ-80K/C/1200(J) / SP-1002",									/* SP-1002 */
	"MZ-1500(J) / 9Z-502M",											/* MZ-1500 1Z-009B+9Z-502M */
};

static int rom_existlist[ROM_KIND_MAX];
static int rom_existnum;

/* kana-key patch table */
static const TPATCH key_patch_data[] =
{
	0x0CEE, 0xAC,
	0x0CEF, 0x9C,
	0x0CF1, 0x99,
	0x0CF2, 0x9A,
	0x0CF3, 0xBC,
	0x0CF4, 0xB8,
	0x0CF5, 0xB0,
	0x0CF9, 0x91,
	0x0CFA, 0x92,
	0x0CFB, 0x93,
	0x0CFC, 0x94,
	0x0CFD, 0x95,
	0x0CFE, 0x96,
	0x0CFF, 0x97,
	0x0D00, 0x98,
	0x0D01, 0x89,
	0x0D02, 0x8A,
	0x0D03, 0x8B,
	0x0D04, 0x8C,
	0x0D05, 0x8D,
	0x0D06, 0x8E,
	0x0D07, 0x8F,
	0x0D08, 0x90,
	0x0D09, 0x81,
	0x0D0A, 0x82,
	0x0D0B, 0x83,
	0x0D0C, 0x84,
	0x0D0D, 0x85,
	0x0D0E, 0x86,
	0x0D0F, 0x87,
	0x0D10, 0x88,
	0x0D11, 0xA1,
	0x0D12, 0xA2,
	0x0D13, 0xA3,
	0x0D14, 0xA4,
	0x0D15, 0xA5,
	0x0D16, 0xA6,
	0x0D17, 0xA7,
	0x0D18, 0xA8,
	0x0D19, 0xBF,
	0x0D1A, 0xAB,
	0x0D1B, 0xAA,
	0x0D1D, 0xA0,
	0x0D1E, 0xA9,
	0x0D1F, 0xAF,
	0x0D20, 0xAE,
	0x0D27, 0x9B,
	0x0D28, 0xAD,
	0xFFFF, 0x00														/* eod */
};

//
LPCSTR stat_head_name = "MZ700WIN_STAT";

//------------------------------------------------
// Memory Allocation for MZ
//------------------------------------------------
int mz_alloc_mem(void)
{
	int result = 0;

	/* Main Memory */
	mem = MEM_alloc((4+6+4+64)*1024);
	if (mem == NULL) return -1;

	/* Junk(Dummy) Memory */
	junk = MEM_alloc(4096);
	if (junk == NULL) return -1;
	
	/* ROM FONT */
	font = MEM_alloc(ROMFONT_SIZE);
	if (font == NULL) result = -1;

	/* PCG-700 FONT */
	pcg700_font = MEM_alloc(PCG700_SIZE);
	if (pcg700_font == NULL) result = -1;

	/* PCG-1500 FONT */
	pcg1500_font_blue = MEM_alloc(PCG1500_SIZE);
	if (pcg1500_font_blue == NULL) result = -1;

	pcg1500_font_red = MEM_alloc(PCG1500_SIZE);
	if (pcg1500_font_red == NULL) result = -1;

	pcg1500_font_green = MEM_alloc(PCG1500_SIZE);
	if (pcg1500_font_green == NULL) result = -1;
	
	/* MZ-1R12 */
	mz1r12_ptr = MEM_alloc(MZ1R12_SIZE);
	if (mz1r12_ptr == NULL) result = -1;

	/* MZ-1R18 */
	mz1r18_ptr = MEM_alloc(MZ1R18_SIZE);
	if (mz1r18_ptr == NULL)
	{
		free(mz1r12_ptr);
		mz1r12_ptr = NULL;
		result = -1;
	}

	return result;
}

//------------------------------------------------
// Release Memory for MZ
//------------------------------------------------
void mz_free_mem(void)
{
	if (mz1r12_ptr) MEM_free(mz1r12_ptr);
	
	if (mz1r18_ptr) MEM_free(mz1r18_ptr);

	if (pcg1500_font_green) MEM_free(pcg1500_font_green);
	
	if (pcg1500_font_red) MEM_free(pcg1500_font_red);
	
	if (pcg1500_font_blue) MEM_free(pcg1500_font_blue);
	
	if (pcg700_font) MEM_free(pcg700_font);
	
	if (font) MEM_free(font);
	
	if (junk) MEM_free(junk);
	
	if (mem) MEM_free(mem);
}

//--------------------------------------------------------
// PATCH ROUTINE for BIOS
//--------------------------------------------------------
void bios_patch(const TPATCH *patch_dat)
{
	TPATCH *p;
	
	p = (TPATCH *)patch_dat;

	while (p->addr != 0xFFFF)
	{
		mem[p->addr] = p->data;
		p++;
	}

}

//--------------------------------------------------------------
// コンボボックスに項目を追加
//--------------------------------------------------------------
void init_rom_combo(HWND hwnd)
{
	int sel = -1;
	int i,a;

	for (i=0; i<rom_existnum; i++)
	{
		a = rom_existlist[i];
		if (menu.selrom == a)
		{
			sel = i;
		}
		SendMessage( hwnd, CB_ADDSTRING, 0, (LPARAM) rom_menulist[a]);
		SendMessage( hwnd, CB_SETITEMDATA, (WPARAM) i, (LPARAM) a);
	}

	if (sel >= 0)
		SendMessage( hwnd, CB_SETCURSEL, (WPARAM) sel, (LPARAM) 0 );
	else
		SendMessage( hwnd, CB_SETCURSEL, (WPARAM) 0, (LPARAM) 0 );
}

//--------------------------------------------------------------
// ＲＯＭモニタの存在チェック
//--------------------------------------------------------------
int rom_check(void)
{
	int result = 0;
	int i;
	const char *p;
	char strtmp[256];

	for (i=0; i< ROM_KIND_MAX; i++)
	{
		p = rom_filelist[i];

		wsprintf(strtmp, "%s%s",RomFileDir, p);
		if (FileExists(strtmp) || p == NULL)
		{
			rom_existlist[result] = i;
			result++;
		}

	}

	rom_existnum = result;

	return result;
}

//--------------------------------------------------------------
// ＲＯＭモニタを読み込む
//--------------------------------------------------------------
int rom_load(unsigned char *x)
{
	FILE_HDL in;
	char strtmp[256];
	int result = MON_EMUL;
	int romlen = 4096;

	rom2_mode = MON_EMUL;

	wsprintf(strtmp,"%s%s",RomFileDir, rom_filelist[menu.selrom]);

	// デフォルト：700の場合 CMOSを使用
	use_cmos = 1;														// 1R12 オン
	
	if (menu.selrom == ROM_KIND_MZ1500)
		romlen = 10240;

	if((in=FILE_ROPEN(strtmp))!=FILE_VAL_ERROR)
	{
		FILE_READ(in, x, romlen);
		FILE_CLOSE(in);
		result = set_romtype();
		if (menu.selrom == ROM_KIND_MZ1500)
		{
			rom2_mode = MON_9Z502;
			use_cmos = 0;
			menu.machine = MACHINE_MZ1500;
		} /* else {
			// 1500のROM以外だったら
			use_cmos = 1;										// 1R12 オン
		} */
	}

	/* フォントデータを読み込む */
	if (font_load(menu.fontset)<0)
	{
    	MessageBox(hwndApp, "Couldn't load font data.",
				   "Error", MB_ICONEXCLAMATION|MB_OK);
		mz_exit(1);
		return result;
	}
	
#ifdef KANJIROM
	/* 漢字ROM, 辞書ROM読み込み */
	if (mz1r23_ptr == NULL) {
		wsprintf(strtmp, "%sMZ1R23.ROM",RomFileDir);
		if((in=FILE_ROPEN(strtmp))!=FILE_VAL_ERROR)
		{
			mz1r23_ptr = MEM_alloc(128 * 1024);
			if (mz1r23_ptr != NULL) {
				FILE_READ(in, mz1r23_ptr, 128 * 1024);
				FILE_CLOSE(in);

				/* 漢字ROMを読めたら、辞書ROMも読んでみる */
				if (mz1r24_ptr == NULL) {
					wsprintf(strtmp, "%sMZ1R24.ROM",RomFileDir);
					if((in=FILE_ROPEN(strtmp))!=FILE_VAL_ERROR)
					{
						mz1r24_ptr = MEM_alloc(256 * 1024);
						if (mz1r24_ptr != NULL) {
							FILE_READ(in, mz1r24_ptr, 256 * 1024);
							FILE_CLOSE(in);
						}
					}
				}
			}
		}
	}
#endif /*KANJIROM*/

	return result;
}

// モニタＲＯＭタイプを判別し、rom1_modeをセット
int set_romtype(void)
{
	if (!strncmp(mem+0x6F3,"1Z-009",5))
	{
		return MON_1Z009;
	}
	else
	if (!strncmp(mem+0x6F3,"1Z-013",5))
	{
		return MON_1Z013;
	}
	else
	if (!strncmp(mem+0x142,"MZ\x90\x37\x30\x30",6))
	{
		return MON_NEWMON700;
	}
	else
	if (!strncmp(mem+0x144,"MZ\x90MONITOR",10))
	{
		return MON_NEWMON80K;
	}

	return MON_OTHERS;
}

//--------------------------------------------------------------
// ＭＺのモニタＲＯＭのセットアップ
//--------------------------------------------------------------
void mz_mon_setup(void)
{
    HMENU hmenu;
	FILE_HDL fh;
	UINT8 strtmp[256];

	const UINT8 * machine_md_str[] =
	{
		"(700Mode)",
		"(1500Mode)",
	};

	// ROMモニタの読み込み
	rom1_mode = rom_load(mem);

#if MZ_SP_PATCH
	if (rom1_mode == MON_EMUL)
	{
		patch_emul_rom1();

		if (menu.machine == MACHINE_MZ1500)
			patch_emul_rom2();
	}
	else
	{
		patch_rom1();
		if (menu.machine == MACHINE_MZ1500)
		{
			if (rom2_mode == MON_9Z502)
				patch_rom2();
			else
				patch_emul_rom2();
		}

		if (rom1_mode == MON_1Z009)
		{
			// Patch Kana Key Table
			if (key_patch)
			{
				// ROM Kana Key Table Patch
				bios_patch(key_patch_data);
			}

		}
	}
#endif

	if (rom1_mode == MON_EMUL)
	{
		mem[ROM_START+L_TMPEX  ] = 0x00;
		mem[ROM_START+L_TMPEX+1] = 0x00;
	}
	
    if (rom1_mode >= MON_1Z009 && rom1_mode <= MON_1Z013)
	{
		// Patch Default Color
		if (bk_color)
		{
			mem[0x007E] = (BYTE) bk_color;
			mem[0x0E49] = (BYTE) bk_color;
			mem[0x0E87] = (BYTE) bk_color;
			mem[0x0EDE] = (BYTE) bk_color;
		}
	}

	ZeroMemory(mem+VID_START,4*1024);
	ZeroMemory(mem+RAM_START,64*1024);

	// メニューとかを反映
    hmenu = GetSubMenu(hmenuApp , 0);									// Keyboard Menu
	

	// 1500モードだったらPCG700を強制的にＯＦＦ
	if (menu.machine == MACHINE_MZ1500)
	{
		menu.pcg700 = 0;
		EnableMenuItem(hmenuApp, MENU_PCG700, MF_BYCOMMAND | MF_GRAYED); // PCG700選択不能
	}
	else
	{
		EnableMenuItem(hmenuApp, MENU_PCG700, MF_BYCOMMAND | MF_ENABLED); // PCG700選択可能
	}

	if (rom2_mode != MON_9Z502)
	{
		EnableMenuItem(hmenuApp, MENU_SETQD, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hmenuApp, MENU_EJECTQD, MF_BYCOMMAND | MF_GRAYED);
//		DeleteMenu(hmenu, MENU_SETQD, MF_BYCOMMAND);
//		DeleteMenu(hmenu, MENU_EJECTQD, MF_BYCOMMAND);
		DrawMenuBar(hwndApp);
	}
	else
	{
		EnableMenuItem(hmenuApp, MENU_SETQD, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(hmenuApp, MENU_EJECTQD, MF_BYCOMMAND | MF_ENABLED);
		DrawMenuBar(hwndApp);

		UpdateQDMenu();
		// Create Blank QD File
		wsprintf(strtmp, "%s\\$qd$.mzt", QDOpenDir);
		if (!FileExists(strtmp))
		{
			fh = FILE_COPEN(strtmp);
			if (fh != FILE_VAL_ERROR)
				FILE_CLOSE(fh);
		}
	}

	UpdateTapeMenu();

	// タイトルバー
	wsprintf(strtmp, "%s %s",szAppName, machine_md_str[menu.machine]);
	SetWindowText(hwndApp, strtmp);
}

//--------------------------------------------------------------
// メイン部
//--------------------------------------------------------------
void mz_main(void)
{
	// ＭＺのモニタのセットアップ
	mz_mon_setup();

	// カレントディレクトリ　設定
	SetCurrentDirectory(LoadOpenDir);

	// メインループ実行
	mainloop();

}

//-------------------------------------------------------------
// 特定の時間　ウェイトを入れる
//   （メッセージループ付き）
//-------------------------------------------------------------
void win_sleep(int t)
{
	DWORD timetmp;

	timetmp = get_timer() + t;

	do
	{
		SystemTask();
	} while (get_timer() < timetmp);

}

//-------------------------------------------------------------
// RAMファイルのセーブ
//-------------------------------------------------------------
// In :	RAMファイル名
// Out:	TRUE (正常終了) / FALSE (エラー)
BOOL save_ramfile(LPCSTR filename)
{
	FILE_HDL	fh;
	int	sz;

	fh = FILE_COPEN(filename);
	if (fh == FILE_VAL_ERROR) return FALSE;

	// Write Image
	sz = FILE_WRITE(fh, mz1r18_ptr, MZ1R18_SIZE);

	// Close
	if (fh != FILE_VAL_ERROR)
		FILE_CLOSE(fh);

	return TRUE;
}

//-------------------------------------------------------------
// RAMファイルロード
//-------------------------------------------------------------
// In :	RAMファイル名
// Out:	TRUE (正常終了) / FALSE (エラー)
BOOL load_ramfile(LPCSTR filename)
{
	FILE_HDL	fh;
	int sz;

	fh = FILE_ROPEN(filename);
	if (fh == FILE_VAL_ERROR) return FALSE;

	// Read Image
	sz = FILE_READ(fh, mz1r18_ptr, MZ1R18_SIZE);

	// Close
	if (fh != FILE_VAL_ERROR)
		FILE_CLOSE(fh);

	return TRUE;
}



//-------------------------------------------------------------
//  データパケットへのポインタを得る
//-------------------------------------------------------------
// In:	num = パケットタイプ番号
//		**datptr =  データパケットへのポインタを返すポインタ
//		*size = データパケットサイズを返すintへのポインタ
// Out:	TRUE (正常終了) / FALSE (エラー)
static BOOL get_packet_ptr(int num, void* *datptr, int *size)
{
	switch (num)
	{
		// Z80
	case MZSBLK_z80:
		*datptr = Z80_GetRegsPtr();
		*size = sizeof(Z80_Regs);
		break;

		// MZ-700 tstates
	case MZSBLK_ts700:
		*datptr = &ts700;
		*size = sizeof(ts700);
		break;

		// MZ-700 hardware status
	case MZSBLK_hw700:
		*datptr = &hw700;
		*size = sizeof(hw700);
		break;

		// 8253
	case MZSBLK_8253dat:
		*datptr = &_8253_dat;
		*size = sizeof(_8253_dat);
		break;

	case MZSBLK_8253stat:
		*datptr = &_8253_stat;
		*size = sizeof(_8253_stat);
		break;

	case MZSBLK_mainram:
		*datptr = mem + RAM_START;
		*size = 0x10000;
		break;

	case MZSBLK_textvram:
		*datptr = mem + VID_START;
		*size = 0x0400;
		break;

	case MZSBLK_colorvram:
		*datptr = mem + VID_START + 0x800;
		*size = 0x0400;
		break;

	case MZSBLK_pcg700_font:
		*datptr = pcg700_font;
		*size = PCG700_SIZE;
		break;

	case MZSBLK_mz1r12:
		*datptr = mz1r12_ptr;
		*size = MZ1R12_SIZE;
		break;

		// MZ-1500 ////
	case MZSBLK_ts1500:
		*datptr = &ts1500;
		*size = sizeof(ts1500);
		break;

		// MZ-1500 hardware status
	case MZSBLK_hw1500:
		*datptr = &hw1500;
		*size = sizeof(hw1500);
		break;

	case MZSBLK_z80pio:
		*datptr = &Z80PIO_stat;
		*size = sizeof(Z80PIO_stat);
		break;

	case MZSBLK_pcgvram:
		*datptr = mem + VID_START + 0x400;
		*size = 0x0400;
		break;

	case MZSBLK_atrvram:
		*datptr = mem + VID_START + 0xC00;
		*size = 0x0400;
		break;

	case MZSBLK_psg:
		*datptr = &mz1500psg;
		*size = sizeof(mz1500psg);
		break;

	case MZSBLK_pcg1500_blue:
		*datptr = pcg1500_font_blue;
		*size = PCG1500_SIZE;
		break;

	case MZSBLK_pcg1500_red:
		*datptr = pcg1500_font_red;
		*size = PCG1500_SIZE;
		break;

	case MZSBLK_pcg1500_green:
		*datptr = pcg1500_font_green;
		*size = PCG1500_SIZE;
		break;

	case MZSBLK_palet:
		*datptr = mzcol_palet;
		*size = sizeof(mzcol_palet);
		break;

	case MZSBLK_mz1r18:
		*datptr = mz1r18_ptr;
		*size = MZ1R18_SIZE;
		break;

	case MZSBLK_EOD:
		*datptr = NULL;
		*size = 0;
		break;

	default:;
		return FALSE;
	}

	return TRUE;
}

// Write Packet
static BOOL write_stat_packet(int num, FILE_HDL fh)
{
	void * wrtptr;
	int		wrtsize;
	int		sz;
	short	blkno;
	BOOL result = TRUE;

	if (!get_packet_ptr(num, &wrtptr, &wrtsize)) return FALSE;

	// Write
	blkno = num;
	// Write Packet Type No.
	sz = FILE_WRITE(fh, &blkno, sizeof(short));
	if (sz != sizeof(short))
		return FALSE;

	// Write Packet Size
	sz = FILE_WRITE(fh, &wrtsize, sizeof(int));
	if (sz != sizeof(int))
		return FALSE;

	// Write Packet
	if (wrtptr)
	{
		sz = FILE_WRITE(fh, wrtptr, wrtsize);
		if (sz != wrtsize) result = FALSE;
	}

	return result;
}

//-------------------------------------------------------------
// STATEファイルのセーブ
//-------------------------------------------------------------
// In :	STATEファイル名
// Out:	TRUE (正常終了) / FALSE (エラー)
BOOL save_state(LPCSTR filename)
{
	TMZS_HEAD	head;
	FILE_HDL	fh;
	int			sz,i;
	BOOL		stat;

	// Make Header
	ZeroMemory(&head, sizeof(head) );
	lstrcpy(head.name, stat_head_name);
	head.rom1 = rom1_mode;
	head.rom2 = rom2_mode;

	fh = FILE_COPEN(filename);
	if (fh == FILE_VAL_ERROR) return FALSE;

	// Write Header
	sz = FILE_WRITE(fh, &head, sizeof(head));

	// Write MZ700 STAT
	for (i=0; i< MZSBLK_MZ700; i++)
	{
		if (menu.machine == MACHINE_MZ1500 && i == MZSBLK_pcg700_font)
			continue;

		stat = write_stat_packet(i,fh);
		if (!stat) break;
	}

	if (menu.machine == MACHINE_MZ1500 && stat)
	{
		// Write MZ1500 STAT
		for (i=MZSBLK_ts1500; i< MZSBLK_MZ1500; i++)
		{
			stat = write_stat_packet(i,fh);
			if (!stat) break;
		}
	}

	// END OF DATA
	if (stat)
		stat = write_stat_packet(MZSBLK_EOD, fh);

	// Close
	if (fh != FILE_VAL_ERROR)
		FILE_CLOSE(fh);

	return TRUE;
}


// Read Packet
static WORD read_stat_packet(FILE_HDL fh)
{
	void * wrtptr;
	int		rdsize;
	int		blksize;
	int		sz;
	short	blkno;
	WORD	result = 0xFFFF;

	// Read Packet Type No.
	sz = FILE_READ(fh, &blkno, sizeof(short));
	if (sz != sizeof(short))
		return result;

	// Read Packet Size
	sz = FILE_READ(fh, &blksize, sizeof(int));
	if (sz != sizeof(int))
		return result;

	if (blksize && !get_packet_ptr(blkno, &wrtptr, &rdsize))
		return result;

	if (blksize)
	{
		// Read Packet
		sz = FILE_READ(fh, wrtptr, rdsize);
		if (sz != rdsize) return result;
	}

	result = blkno;
	return result;
}

//-------------------------------------------------------------
// STATEファイルのロード
//-------------------------------------------------------------
// In :	STATEファイル名
// Out:	TRUE (正常終了) / FALSE (エラー)
BOOL load_state(LPCSTR filename)
{
	TMZS_HEAD	head;
	FILE_HDL	fh;
	int			sz;
	WORD		stat;
	BOOL		result = FALSE;

	fh = FILE_ROPEN(filename);
	if (fh == FILE_VAL_ERROR) return FALSE;

	// Read Header
	sz = FILE_READ(fh, &head, sizeof(head));
	if (sz == sizeof(head))
	{
		// ヘッダが正しいか？
		if (!lstrcmp(head.name, stat_head_name))
		{
			// Check ROM Mode
			if (head.rom1==rom1_mode && head.rom2==rom2_mode)
			{
				// Read MZ700 STAT
				for (;;)
				{
					stat = read_stat_packet(fh);
					if (stat == MZSBLK_EOD)
					{
						result = TRUE;
						break;
					}
					if (stat == 0xFFFF)
						break;
				}
			}
		}
	}

	// Close
	if (fh != FILE_VAL_ERROR)
	{
		FILE_CLOSE(fh);
	}

	return result;
}


//-------------------------------------------------------------
//  mz700win MAINLOOP
//-------------------------------------------------------------
void mainloop(void)
{
	int _synctime = SyncTime;
	int w;
	long timetmp;
	DWORD pad;
	DWORD GetTrg;
	DWORD GetReleaseTrg;
	static DWORD Pad_bak;

#ifdef ENABLE_FDC
	UINT8 strtmp[MAX_PATH];

	// FDC 初期化
	FDC_Init();
	/* カレントディレクトリゲット */
	GetCurrentDirectory(sizeof(strtmp), strtmp);
	strcat_s(strtmp, sizeof(strtmp), "\\test.D88");

	// FDDテスト
	mz_set_fd(0, 0, strtmp);
#endif

	mzbeep_init(44100);
	sn76489an_init(44100, CPU_SPEED_BASE);

	// DirectSound初期化
	if (!DSound_Init(44100, 50)) {
		sound_di = TRUE;
	}
	// DirectSoundの初期化。エラーでも続行（音源無しの場合）
	mzsnd_init();

	// XInput初期化
	XInput_Init();

	// Reset MZ
	mz_reset();
	
//	_iperiod = (CPU_SPEED*CpuSpeed)/(100*IFreq);
//	Z80_IPeriod = _iperiod;
//	Z80_ICount = _iperiod;

	setup_cpuspeed(menu.speed);
	Z80_IRQ = 0;

	// start the CPU emulation
	for (;;SystemTask())
	{
		if (isAppActive())
		{
			timetmp = get_timer();		
			if (!Z80_Execute()) break;
			XInput_Update();						// XInputの状態更新

			if (XI_Is_GamePad_Connected(0))
			{
				// ゲームパッド接続時
				// ゲームパッドが繋がっていたら...
//				dprintf("XI_Is_GamePad_Connected(0)\n");
				// 押されたトリガー取得
				pad = XI_Get_GamePad_RAW(0);
				GetTrg = ~Pad_bak & pad;
				GetReleaseTrg = Pad_bak & ~pad;
				Pad_bak = pad;

				// 方向キー
				if (GetTrg & PAD_RAW_LEFT)
				{
					dprintf("LEFT key pressed\n");
					mz_keydown(VK_LEFT);	// cursor left
				}
				else if (GetReleaseTrg & PAD_RAW_LEFT)
				{
					dprintf("LEFT key released\n");
					mz_keyup(VK_LEFT);	// cursor left
				}

				if (GetTrg & PAD_RAW_RIGHT)
				{
					dprintf("RIGHT key pressed\n");
					mz_keydown(VK_RIGHT);	// cursor right
				}
				else if (GetReleaseTrg & PAD_RAW_RIGHT)
				{
					dprintf("RIGHT key released\n");
					mz_keyup(VK_RIGHT);	// cursor right
				}

				if (GetTrg & PAD_RAW_UP)
				{
					dprintf("UP key pressed\n");
					mz_keydown(VK_UP);	// cursor up
				}
				else if (GetReleaseTrg & PAD_RAW_UP)
				{
					dprintf("UP key released\n");
					mz_keyup(VK_UP);	// cursor up
				}
				if (GetTrg & PAD_RAW_DOWN)
				{
//					dprintf("DOWN key pressed\n");
					mz_keydown(VK_DOWN);	// cursor down
				}
				else if (GetReleaseTrg & PAD_RAW_DOWN)
				{
//					dprintf("DOWN key released\n");
					mz_keyup(VK_DOWN);		// cursor down
				}

				// ボタン
				if (GetTrg & PAD_RAW_A)
				{
					dprintf("A button pressed\n");
					mz_keydown(VK_SPACE);	// space key
				}
				else if (GetReleaseTrg & PAD_RAW_A)
				{
					dprintf("A button released\n");
					mz_keyup(VK_SPACE);		// space key
				}

				if (GetTrg & PAD_RAW_B)
				{
					dprintf("B button pressed\n");
					mz_keydown(VK_SHIFT);	// shift key
				}
				else if (GetReleaseTrg & PAD_RAW_B)
				{
					dprintf("B button released\n");
					mz_keyup(VK_SHIFT);		// shift key
				}
				if (GetTrg & PAD_RAW_X)
				{
					dprintf("X button pressed\n");
					mz_keydown(0x5A);		// Z key
				}
				else if (GetReleaseTrg & PAD_RAW_X)
				{
					dprintf("X button released\n");
					mz_keyup(0x5A);			// Z key
				}
				if (GetTrg & PAD_RAW_Y)
				{
					dprintf("Y button pressed\n");
					mz_keydown(0x58);		// X key
				}
				else if (GetReleaseTrg & PAD_RAW_Y)
				{
					dprintf("Y button released\n");
					mz_keyup(0x58);			// X key
				}
			}
			else
			{
				// ゲームパッド無しの時
				pad = 0; 
			}
			
			w = _synctime - (get_timer() - timetmp);
			if (w > 0)
				win_sleep(w);
		}

	}
	
}

//------------------------------------------------------------
// CPU速度を設定 (10-100)
//------------------------------------------------------------
void setup_cpuspeed(int per) {
	int _iperiod;
	int a;

	_iperiod = (CPU_SPEED*CpuSpeed)/(100*IFreq);

	a = (per * 256) / 100;

	_iperiod *= a;
	_iperiod >>= 8;

	Z80_IPeriod = _iperiod;
	Z80_ICount = _iperiod;

}

//--------------------------------------------------------------
// スレッドの準備
//--------------------------------------------------------------
int create_thread(void)
{
	// Initialize Critical Section
	InitializeCriticalSection(&CriticalSection);

	// Start Screen Thread
	hCpuThread = CreateThread(0,0,(LPTHREAD_START_ROUTINE)scrn_thread,0,CREATE_SUSPENDED,&CpuThreadID);
	if (!hCpuThread) return -1;
	
	SetThreadPriority(hCpuThread,THREAD_PRIORITY_IDLE);

	return 0;
}

//--------------------------------------------------------------
// スレッドの開始
//--------------------------------------------------------------
void start_thread(void)
{
	if (ResumeThread(hCpuThread) != 0xFFFFFFFF)
		SetThreadPriority(hCpuThread,THREAD_PRIORITY_NORMAL);
}

//--------------------------------------------------------------
// スレッドの後始末
//--------------------------------------------------------------
int end_thread(void)
{
	TerminateThread(hCpuThread, 0);
	WaitForSingleObject(hCpuThread, INFINITE);
	CloseHandle(hCpuThread);

	// Delete Critical Section
	DeleteCriticalSection(&CriticalSection);

	return 0;
}

//--------------------------------------------------------------
// 画面描画スレッド 
//--------------------------------------------------------------
void WINAPI scrn_thread(void *arg)
{
	long vsynctmp,timetmp;
	
	for (;;)
	{
		if (isAppActive())
		{
			// 画面更新処理
			hw700.retrace = 1;											/* retrace = 0 : in v-blnk */
			vblnk_start();

			EnterCriticalSection( &CriticalSection );
			timetmp = get_timer();
			update_scrn();												/* 画面描画 */
			LeaveCriticalSection( &CriticalSection );

			vsynctmp = get_timer() - timetmp;
			if ( (SyncTime - vsynctmp) >=0 )
				Sleep(SyncTime - vsynctmp);
		}
		else
		{
			Sleep(SyncTime);
		}

	}
	
}

/////////////////////////////////////////////////
// Unused Code...............
/////////////////////////////////////////////////
#if 0

DWORD WINAPI dummy_thread(LPVOID n)
{
	for (;;)
	{
		Sleep(100);
	}

	return 0;
}

#endif
