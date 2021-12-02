//----------------------------------------------------------------------------
// File:win.c
// MZ-700/1500 Emulator MZ700WIN for Windows9x/NT/2000
//
// mz700win:Window / Menu Module
// ($Id: Win.c 20 2009-05-25 18:20:32Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#define _MAIN_	1

#include <stdio.h>
#ifndef NDEBUG
#include <crtdbg.h>
#endif //NDEBUG

#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include "resource.h"

#include "mz700win.h"
#include "Z80.h"

#include "dprintf.h"
#include "win.h"
#include "fileio.h"
#include "fileset.h"

#include "MZmain.h"
#include "MZhw.h"
#include "MZddraw.h"
#include "MZscrn.h"
#include "DSOUND.h"
#include "MZXInput.h"

#include "mzbeep.h"
#include "sn76489an.h"

#include "defkey.h"

static const UINT8 szClassName[] = "MZ700WINClass";
const UINT8 szAppName[] = "MZ-700 Emulator For Win32";

static UINT8		IniFileStr[IniFileStrBuf];						// ＩＮＩファイル名をしまうバッファ
static UINT8		FontFileStr[2][256];							// FONTファイル名をしまうバッファ

UINT8		RomFileDir[IniFileStrBuf];								// ROMファイルがあるディレクトリ名

HWND      hwndApp;
HMENU     hmenuApp;

static HANDLE		hMutex;
static HPALETTE		hpalApp;
static HINSTANCE	hInstApp;
static HACCEL		hAccelApp;

static long			fullsc_timer;

static HFONT		hFontLink;
static HFONT		hFontOrg;
static WNDPROC		oldLinkProc = NULL;
static HCURSOR		hCurHand;
static HWND			link_hwnd_bak;

static TIMECAPS		tcaps;											// タイマー性能

static HBITMAP		hmenuBitmap;

static TBMPMENU		menu_work[MENU_WORK_MAX];

static header    BufferHeader;
static void      *pBuffer = NULL;			// WinGBitmapサーフェスへのポインタ
HDC			      Buffer = 0;				// WinGBitmapのデバイスコンテキスト
static HBITMAP  gbmOldMonoBitmap = 0;

static int			scrnmode;										// スクリーンモード 0:WINDOW 1:FULL
static int			win_sx,win_sy;
static int			win_xpos,win_ypos;
static int			win_xbak,win_ybak;
static BOOL			fAppActive;

pal LogicalPalette =
{
  0x300,
  256
};

LRESULT CALLBACK linkProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

/* Disable IME関連 */
typedef BOOL (CALLBACK* LPFNDLLFUNC)(HWND,BOOL);
static LPFNDLLFUNC ptr_WINNLSEnableIME = NULL;							/* WINNLSEnableIMEのポインタ */
static HMODULE hUSER32;

static const RGBQUAD d8colors[8]=										/* blue,green,red */
{
	/* b,g,r,padding  */
	{   1,  1,  2,  0 },												/* 0:BLACK */
	{ 253,  1,  1,  0 },												/* 1:BLUE */
	{   1,  1,253,  0 },												/* 2:RED */
	{ 253,  1,253,  0 },												/* 3:PURPLE */
	{   1,253,  1,  0 },												/* 4:GREEN */
	{ 253,254,  1,  0 },												/* 5:CYAN */
	{   1,253,254,  0 },												/* 6:YELLOW */
	{ 253,253,253,  0 }													/* 7:WHITE */
};

//--------------------
// global   variables
//--------------------
TMENUVAL		menu;													/* menu用ワーク */

WORD	sound_di = 0;													/* ＳＥ禁止モード */
WORD	sound_md = 0;													/* サウンドモード */
//WORD	mz1500mode = 0;													/* MZ-1500モード */
WORD	key_patch = 0;													/* KeyTableパッチあてフラグ */
WORD	bk_color = 0;													/* モニタ画面背景色 */
WORD	use_cmos;														/* MZ-1R12 0:OFF 1:ON */

//
BOOL add_menu_bmp(HMENU hmenu, UINT id,UINT bmp)
{
	int i;
	TBMPMENU *tp;
	
	for (i=0;i<MENU_WORK_MAX;i++)
	{
		tp = &menu_work[i];
		if (tp->menuID==0)
		{
			GetMenuString(hmenu,id,tp->menustr,64,MF_BYCOMMAND);
			tp->menuID = id;
			tp->menuBMP = bmp;
			ModifyMenu(hmenu ,id ,MF_BYCOMMAND | MF_OWNERDRAW,id ,NULL );

			return TRUE;
		}

	}

	return FALSE;
}

TBMPMENU * get_menu_work(UINT id)
{
	int i;
	TBMPMENU *tp;
	
	for (i=0;i<MENU_WORK_MAX;i++)
	{
		tp = &menu_work[i];
		if (tp->menuID==id)
			return tp;
	}

	return NULL;
}

void init_menu_work(void)
{
	ZeroMemory(&menu_work,sizeof(menu_work));
}


void InitMenuBitmaps(HWND hw)
{
	HMENU   hmenu;
	LONG    lCheckSize;

	hmenu=GetMenu(hw);												/* メニューハンドル */
	lCheckSize=GetMenuCheckMarkDimensions();

	hmenuBitmap = LoadBitmap(hInstApp,"TOOLBAR");

#if 0
	// メニューＩＤにオーナー描画を指定
	add_menu_bmp(hmenu, MENU_OPEN, 2);
	add_menu_bmp(hmenu, MENU_SET, 0);
	add_menu_bmp(hmenu, MENU_TAPESAVE, 0);
	add_menu_bmp(hmenu, MENU_PCG700, 0);
	add_menu_bmp(hmenu, MENU_RESET, 0);
	add_menu_bmp(hmenu, MENU_EXIT, 5);

	add_menu_bmp(hmenu, MENU_ABOUT, 8);
#endif

	DrawMenuBar(hw);
}

void EndMenuBitmaps(HWND hw)
{
	// Menuビットマップの消去
	DeleteObject(hmenuBitmap);
}

/*
 *  Clear the System Palette so that we can ensure an identity palette 
 *  mapping for fast performance.
 */
void ClearSystemPalette(void)
{
  /* A dummy palette setup */
  struct
  {
    WORD Version;
    WORD NumberOfEntries;
    PALETTEENTRY aEntries[256];
  } Palette =
  {
    0x300,
    256
  };

  HPALETTE ScreenPalette = 0;
  HDC ScreenDC;
  int Counter;
  UINT nMapped = 0;
  BOOL bOK = FALSE;
  int  nOK = 0;
  
  /* Reset everything in the system palette to black */
  for(Counter = 0; Counter < 256; Counter++)
  {
    Palette.aEntries[Counter].peRed = 0;
    Palette.aEntries[Counter].peGreen = 0;
    Palette.aEntries[Counter].peBlue = 0;
    Palette.aEntries[Counter].peFlags = PC_NOCOLLAPSE;
  }

  /* Create, select, realize, deselect, and delete the palette */
  ScreenDC = GetDC(NULL);
  ScreenPalette = CreatePalette((LOGPALETTE *)&Palette);

  if (ScreenPalette)
  {
    ScreenPalette = SelectPalette(ScreenDC,ScreenPalette,FALSE);
    nMapped = RealizePalette(ScreenDC);
    ScreenPalette = SelectPalette(ScreenDC,ScreenPalette,FALSE);
    bOK = DeleteObject(ScreenPalette);
  }

  nOK = ReleaseDC(NULL, ScreenDC);

  return;
}


/*
  仮想画面を作る
 */
UINT8 * CreateOffScreen(void)
{
	/* 新しい WinGDC と 8-bit WinGBitmapを作成 */
	HBITMAP hbm;
	int Counter;
	HDC Screen;

	/*  Get WinG to recommend the fastest DIB format */
	/*  set it up ourselves */

	BufferHeader.Header.biSize = sizeof(BITMAPINFOHEADER);
	BufferHeader.Header.biPlanes = 1;
	BufferHeader.Header.biBitCount = 8;
	BufferHeader.Header.biCompression = BI_RGB;
	BufferHeader.Header.biSizeImage = 0;
	BufferHeader.Header.biClrUsed = 0;
	BufferHeader.Header.biClrImportant = 0;
			
	BufferHeader.Header.biWidth = FORMWIDTH;
//	BufferHeader.Header.biHeight = FORMHEIGHT;							/* bottom-up DIB */
	BufferHeader.Header.biHeight = -FORMHEIGHT;							/* top-down DIB */
			
	/*  create an identity palette from the DIB's color table */

	/*  get the 20 system colors as PALETTEENTRIES */
	Screen = GetDC(NULL);

	GetSystemPaletteEntries(Screen,0,10,LogicalPalette.aEntries);
	GetSystemPaletteEntries(Screen,246,10,
							LogicalPalette.aEntries + 246);
	
	GetSystemPaletteEntries(Screen,10,240,
							LogicalPalette.aEntries + 10);
	
	ReleaseDC(NULL,Screen);

	/* initialize the logical palette and DIB color table */
	for (Counter = 0;Counter < 10;Counter++)
	{
		/* copy the system colors into the DIB header
		 * WinG will do this in WinGRecommendDIBFormat,
		 * but it may have failed above so do it here anyway
		 */
		BufferHeader.aColors[Counter].rgbRed =
			LogicalPalette.aEntries[Counter].peRed;
		BufferHeader.aColors[Counter].rgbGreen =
			LogicalPalette.aEntries[Counter].peGreen;
		BufferHeader.aColors[Counter].rgbBlue =
			LogicalPalette.aEntries[Counter].peBlue;
		BufferHeader.aColors[Counter].rgbReserved = 0;
		
		LogicalPalette.aEntries[Counter].peFlags = 0;
		
		BufferHeader.aColors[Counter + 246].rgbRed =
			LogicalPalette.aEntries[Counter + 246].peRed;
		BufferHeader.aColors[Counter + 246].rgbGreen =
			LogicalPalette.aEntries[Counter + 246].peGreen;
		BufferHeader.aColors[Counter + 246].rgbBlue =
			LogicalPalette.aEntries[Counter + 246].peBlue;
		BufferHeader.aColors[Counter + 246].rgbReserved = 0;
		
		LogicalPalette.aEntries[Counter + 246].peFlags = 0;
	}


	for (Counter = 10;Counter < 246;Counter++)
	{
		BufferHeader.aColors[Counter].rgbRed = LogicalPalette.aEntries[Counter].peRed;
		BufferHeader.aColors[Counter].rgbGreen = LogicalPalette.aEntries[Counter].peGreen;
		BufferHeader.aColors[Counter].rgbBlue = LogicalPalette.aEntries[Counter].peBlue;
		
		BufferHeader.aColors[Counter].rgbReserved = 0;
		LogicalPalette.aEntries[Counter].peFlags = PC_NOCOLLAPSE;
	}
	
	/* デジパチパレット定義 */
	for (Counter = 10;Counter < 18;Counter++)
	{
		/* copy from the original color table to the WinGBitmap's
		 * color table and the logical palette
		 */
		
		BufferHeader.aColors[Counter].rgbRed =
			LogicalPalette.aEntries[Counter].peRed =
			d8colors[Counter-10].rgbRed;
		
		BufferHeader.aColors[Counter].rgbGreen =
			LogicalPalette.aEntries[Counter].peGreen =
			d8colors[Counter-10].rgbGreen;
		
		BufferHeader.aColors[Counter].rgbBlue =
			LogicalPalette.aEntries[Counter].peBlue =
			d8colors[Counter-10].rgbBlue;
		
		BufferHeader.aColors[Counter].rgbReserved = 0;
		LogicalPalette.aEntries[Counter].peFlags = PC_RESERVED;
	}
	
	hpalApp = CreatePalette((LOGPALETTE *)&LogicalPalette);
	
	/*  Create a WinGDC and Bitmap, then select away */
	Buffer = CreateCompatibleDC(NULL);
	hbm = CreateDIBSection(Buffer,
						   (BITMAPINFO *)&BufferHeader,
						   DIB_RGB_COLORS,
						   &pBuffer,
						   NULL,
						   0);
	
	/*  Store the old hbitmap to select back in before deleting */
	gbmOldMonoBitmap = (HBITMAP)SelectObject(Buffer, hbm);

	/* Palette Set */
	SelectPalette(Buffer, hpalApp, FALSE);
	
	/* 仮想画面を真っ黒に */
	PatBlt(Buffer, 0,0,FORMWIDTH,FORMHEIGHT, BLACKNESS);

	return pBuffer;
}

/*
  仮想画面の開放
 */
void FreeOffScreen(void)
{
	HBITMAP hbm;

	if (Buffer)	{
		/*  Retrieve the current WinGBitmap, replace with the original */
		hbm = (HBITMAP)SelectObject(Buffer, gbmOldMonoBitmap);

		/*  And delete the WinGBitmap and WinGDC */
		DeleteObject(hbm);
		DeleteDC(Buffer);
		Buffer = NULL;
	}
}

/*
  実行途中でウィンドウ閉じてプログラムを終了
 */
void mz_exit(int code)
{
	PostMessage(hwndApp,WM_CLOSE,0,0L);
}


/* */
void ShowInfo(void)
{
	UINT8 msg[128];

	wsprintf(msg,"TIMECAPS\r"\
			     "wPeriodMin=%d\r"\
			     "wPeriodMax=%d\r"\
			     "Z80_IPeriod=%d",
			 tcaps.wPeriodMin,tcaps.wPeriodMax,Z80_IPeriod);
	
	MessageBox(hwndApp, msg ,"Debug Information",MB_ICONINFORMATION|MB_OK);
}

/* INIファイル内のセクションに数値を設定 */
BOOL WritePrivateProfileInt(LPCSTR  lpszSection,LPCSTR  lpszKey,int  val,LPCSTR  lpszFile)
{
	UINT8 strtmp[64];
	BOOL fSuccess;

	wsprintf(strtmp,"%d",val);
	fSuccess=WritePrivateProfileString(lpszSection,lpszKey,strtmp,lpszFile);
	
	return fSuccess;
}

/*
   INIファイルの読み込み
 */
void load_inifile(void)
{
	UINT8 current[MAX_PATH];

	/* カレントディレクトリゲット */
	GetCurrentDirectory(sizeof(current),current);

	/* INIファイル名を作成 */
	GetCurrentDirectory(sizeof(IniFileStr),IniFileStr);
	lstrcat(IniFileStr,IniFileName);									/* IniFileStr='MZ700WIN.INI' */

	/* CMOSファイル名を作成 */
	GetCurrentDirectory(sizeof(CmosFileStr),CmosFileStr);
	lstrcat(CmosFileStr,CmosFileName);								/* CmosFileStr='CMOS.DAT' */

	/* ROMファイルがあるディレクトリ名を作成 */
	wsprintf(RomFileDir, "%s\\roms\\", current);

	/* 選択されたＲＯＭの取得 */
	menu.selrom = GetPrivateProfileInt(IniSection_Option,"ROM",
									   0, IniFileStr);

	/* SCREEN倍率のゲット */
	menu.screen = GetPrivateProfileInt(IniSection_Option,"SCREEN",
									   0, IniFileStr);

	/* 実行速度の取得 */
	menu.speed = GetPrivateProfileInt(IniSection_Option, "SPEED",
									   100, IniFileStr);

	/* フレームスキップのゲット */
	menu.scrn_freq = GetPrivateProfileInt(IniSection_Option,"FRAMESKIP",
									   0, IniFileStr);

	/* キーボードタイプのゲット */
	menu.keytype = GetPrivateProfileInt(IniSection_Option,"KEYBOARD",
									   0, IniFileStr);
#if 0
#if JAPANESE										
									   0, IniFileStr);					/* 日本版の場合は106がデフォ */
#else	
									   1, IniFileStr);					/* 国際版の場合は101がデフォ */
#endif
#endif

	/* フォントセットタイプのゲット */
	menu.fontset = GetPrivateProfileInt(IniSection_Option,"FONTSET",
#if JAPANESE										
									   1, IniFileStr);					/* 日本版の場合は日本語フォントがデフォ */
#else	
									   0, IniFileStr);					/* 国際版の場合はヨーロッパフォントがデフォ */
#endif

	/* ＰＣＧ７００スイッチのゲット */
	menu.pcg700 = GetPrivateProfileInt(IniSection_Option,"PCG700",
									   0, IniFileStr);
	/* サウンド処理モードゲット */
	sound_md = GetPrivateProfileInt(IniSection_Option,"SOUND",
									   0, IniFileStr);
	/* ウィンドウ位置ゲット */
	win_xpos = GetPrivateProfileInt(IniSection_Window,"XPOS",
									   -99999, IniFileStr);
	win_ypos = GetPrivateProfileInt(IniSection_Window,"YPOS",
									   -99999, IniFileStr);

	/* フォルダ位置ゲット */
	GetPrivateProfileString(IniSection_Folder,"LOAD",
		current, LoadOpenDir,sizeof(LoadOpenDir), IniFileStr);

	GetPrivateProfileString(IniSection_Folder,"SAVE",
		current, SaveOpenDir,sizeof(SaveOpenDir), IniFileStr);

	GetPrivateProfileString(IniSection_Folder,"QD",
		current, QDOpenDir,sizeof(QDOpenDir), IniFileStr);

	GetPrivateProfileString(IniSection_Folder,"RAMFILE",
		current, RAMOpenDir,sizeof(RAMOpenDir), IniFileStr);

	GetPrivateProfileString(IniSection_Folder,"STATES",
		current, StateOpenDir,sizeof(StateOpenDir), IniFileStr);

	/* セーブ用テープファイル名　仮設定 */
	wsprintf( current, "%s\\$cmt$.mzt", SaveOpenDir);	

	GetPrivateProfileString(IniSection_Folder,"SAVETAPE",
		current, SaveTapeFile,sizeof(SaveTapeFile), IniFileStr);
}

/*
  アプリ開始の時の処理
*/
BOOL AppInit(void)
{
	int errflg = 0;

	/* キーマトリクスの定義ファイルの読み込み */
	errflg = read_defkey();
	if (errflg) return FALSE;

	/* エミュレータで使うメモリの確保 */
	errflg = mz_alloc_mem();
	if (errflg) return FALSE;

	/* screenメニューの設定値をセット */
	set_screen_menu(menu.screen);
	/* fullscreenメニューの設定値をセット */
	set_fullscreen_menu(scrnmode);
	/* リフレッシュ */
	set_select_chk(MENU_REFRESH_EVERY,4,menu.scrn_freq);
	/* keytypeメニューの設定値をセット */
	set_keytype_menu(menu.keytype);
	/* fontsetメニューの設定値をセット */
	set_fontset_menu(menu.fontset);
	/* pcg700-swの設定値をセット */
	set_pcg700_menu(menu.pcg700);
	
	/* CMOS.RAM load */
	mz_load_cmos();
	
	return TRUE;
}
/*
  アプリ終了の時の処理
 */
void AppExit(void)
{
	BOOL fSuccess;

	/* C-MOS内容のセーブ */
	mz_save_cmos();

	/* リソース開放 */
	free_resource();

	/* 選択されたＲＯＭ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"ROM",menu.selrom, IniFileStr);
	/* SCREEN倍率のセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"SCREEN",menu.screen, IniFileStr);
	/* 実行速度のセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"SPEED",menu.speed, IniFileStr);
	/* フレームスキップのセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"FRAMESKIP",menu.scrn_freq, IniFileStr);
	/* キーボードタイプのセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"KEYBOARD",menu.keytype, IniFileStr);
	/* キーボードタイプのセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"FONTSET",menu.fontset, IniFileStr);
	/* ＰＣＧ７００ＳＷのセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"PCG700",menu.pcg700, IniFileStr);
	/* サウンドモードのセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Option,"SOUND",sound_md, IniFileStr);
	/* ウィンドウ位置のセーブ */
	fSuccess=WritePrivateProfileInt(IniSection_Window,"XPOS",win_xpos, IniFileStr);
	fSuccess=WritePrivateProfileInt(IniSection_Window,"YPOS",win_ypos, IniFileStr);

	/* フォルダ位置のセーブ */
	WritePrivateProfileString(IniSection_Folder,"LOAD",
		LoadOpenDir, IniFileStr);
	WritePrivateProfileString(IniSection_Folder,"SAVE",
		SaveOpenDir, IniFileStr);
	WritePrivateProfileString(IniSection_Folder,"QD",
		QDOpenDir, IniFileStr);
	WritePrivateProfileString(IniSection_Folder,"RAM",
		RAMOpenDir, IniFileStr);
	WritePrivateProfileString(IniSection_Folder,"SAVETAPE",
		SaveTapeFile, IniFileStr);
	WritePrivateProfileString(IniSection_Folder,"STATES",
		StateOpenDir, IniFileStr);

	/* パレット削除 */
	if (hpalApp) {
		DeleteObject(hpalApp);
	}

	/* 仮想画面開放 */
	FreeOffScreen();

}
/*
  アプリ実行の時の処理
 */
void AppMain(void)
{
	/* タイマー初期化 */
	create_mmtimer();

	create_thread();

	/* MZ-700 実行開始 */
	rom_check();				// ROMモニタの存在チェック
	mz_main();
}

// マルチメディアタイマ 解像度設定
void create_mmtimer(void)
{
	int res;
	
	/* mmtimer 解像度設定 */
	timeGetDevCaps(&tcaps,sizeof(tcaps));
	res=tcaps.wPeriodMin;

	timeBeginPeriod(res);
}

// マルチメディアタイマ　後始末
void free_mmtimer(void)
{

	timeEndPeriod(tcaps.wPeriodMin);
}

// メニューとキャプションの大きさ込みでサイズを決定。
void get_window_size(int m)
{
	win_sx = FORMWIDTH * (m+1);
	win_sy = FORMHEIGHT * (m+1);
	win_sx += (GetSystemMetrics(SM_CXFIXEDFRAME)*2);
	win_sy += GetSystemMetrics(SM_CYCAPTION)+
			  GetSystemMetrics(SM_CYMENU)+
			  (GetSystemMetrics(SM_CXFIXEDFRAME)*2);
}

// メニューバーが２段になった場合、下が切れる現象の対策
void adjust_window_size(HWND hwnd, int m)
{
	// 実際に表示されたクライアント領域の寸法を調べて, 正しい寸法になるようにウインドウをリサイズする
	RECT rc;
	GetClientRect(hwnd, &rc);

	int dw = FORMWIDTH * (m + 1) - (rc.right - rc.left);
	int dh = FORMHEIGHT * (m + 1) - (rc.bottom - rc.top);

	if (dw | dh) {
		win_sx += dw;
		win_sy += dh;
		MoveWindow(hwnd, win_xpos, win_ypos, win_sx, win_sy, TRUE);
	}
}

/* 汎用メニュー項目設定 */
/*
  In:	base=MENU_xxx
		range=項目数
		idx  = ONにする項目
 */
void set_select_chk(int base,int range,int idx)
{
	int i;
	
	/* メニューのチェックマークを付ける */
	for (i=0;i<range;i++) {
		CheckMenuItem(hmenuApp, base+i,
					  MF_BYCOMMAND | MF_UNCHECKED);
	}
	
	CheckMenuItem(hmenuApp, base+idx,
						MF_BYCOMMAND | MF_CHECKED);

}

/* メニューのScreenを設定 */
void set_screen_menu(int m)
{
	int xbak,ybak;

	xbak = win_xpos;
	ybak = win_ypos;

	/* スクリーンサイズにチェックマーク付ける */
	set_select_chk(MENU_SCREEN_1_1, 4, m);								// screen

	/* ウィンドウサイズの変更 */
	get_window_size(m);

	MoveWindow(hwndApp,xbak,ybak,win_sx,win_sy,TRUE);
}

/* メニューのkeyboardを設定 */
void set_keytype_menu(int m)
{
	set_select_chk(MENU_KEYTYPE_BASE, get_keymat_max() ,m);				// keytype
#if _DEBUG
	printf("keymat_max=%d\n",get_keymat_max() );
#endif	

}

/* メニューのfontsetを設定 */
void set_fontset_menu(int m)
{
	set_select_chk(MENU_FONT_EUROPE,2,m);
}

/* メニューのPCG700-SWを設定 */
void set_pcg700_menu(int m)
{
	CheckMenuItem(hmenuApp, MENU_PCG700,
						MF_BYCOMMAND | (m ? MF_CHECKED : MF_UNCHECKED));

}

/* メニューのFULLSCREENを設定 */
void set_fullscreen_menu(int m)
{
#if USE_DDRAW
	CheckMenuItem(hmenuApp, MENU_FULLSCREEN,
						MF_BYCOMMAND | (m ? MF_CHECKED : MF_UNCHECKED));
#else
	EnableMenuItem(hmenuApp, MENU_FULLSCREEN, MF_BYCOMMAND | MF_GRAYED); // Disable FullScreen
#endif
}

/**********************************/
/* マウスカーソルの表示許可／禁止 */
/**********************************/
void mouse_cursor(BOOL sw)
{
	int a,c;

	c=0;
	/* cursor on */
	if (sw)
	{
		while (1)
		{
			a=ShowCursor(TRUE);
			if (a>=0 || c>15) break;
			c++;
		}
	}
	else
	/* Cursor off*/
	{
		while (1)
		{
			a=ShowCursor(FALSE);
			if (a<0) break;
			c++;
		}
	}


};

//
void DrawScreen(HDC hdc, int mode)
{
	switch (mode)
	{
		/* 1:1 */
	case 0:
		BitBlt(hdc,0,0,FORMWIDTH,FORMHEIGHT,Buffer,0,0,SRCCOPY);
		break;
		/* 2:2 */
	case 1:
		StretchBlt(hdc,0,0,FORMWIDTH*2,FORMHEIGHT*2,
				   Buffer,0,0,FORMWIDTH,FORMHEIGHT,SRCCOPY);
		break;
		/* 3:3 */
	case 2:
		StretchBlt(hdc,0,0,FORMWIDTH*3,FORMHEIGHT*3,
				   Buffer,0,0,FORMWIDTH,FORMHEIGHT,SRCCOPY);
		break;
		/* 4:4 */
	case 3:
		StretchBlt(hdc,0,0,FORMWIDTH*4,FORMHEIGHT*4,
				   Buffer,0,0,FORMWIDTH,FORMHEIGHT,SRCCOPY);
		break;
	}				
}

/*----------------------------------------------------------------------------*\
|   AppPaint(hwnd, hdc)                                                        |
|                                                                              |
|   Description:                                                               |
|       The paint function.                                                    |
|                                                                              |
|   Arguments:                                                                 |
|       hwnd             window painting into                                  |
|       hdc              display context to paint to                           |
|                                                                              |
\*----------------------------------------------------------------------------*/
BOOL AppPaint (HWND hwnd, HDC hdc)
{
	SelectPalette(hdc, hpalApp, FALSE);
	RealizePalette(hdc);

	DrawScreen(hdc, menu.screen);

	return TRUE;
}

//---------------------------------------------------------------------
// AboutDialog : ダイアログ プロシージャ
//---------------------------------------------------------------------
// 以前のリンクを消去
static void clr_link_str(HWND hwnd)
{
	HDC hdc;
	HGDIOBJ hFontbk;
	UINT8 strtmp[128];

	GetWindowText(hwnd, strtmp, sizeof(strtmp) );
	lstrcat(strtmp, " ");

	hdc=GetDC(hwnd);
	hFontbk = SelectObject(hdc, hFontOrg);
	SetTextColor(hdc, RGB(0, 0, 255));	//青
	SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
	SetBkMode(hdc, OPAQUE);
	TextOut(hdc, 0, 0, strtmp, lstrlen(strtmp));
	SelectObject(hdc, hFontbk);
	ReleaseDC(hwnd, hdc);
}

// リンクの下線無し表示
LRESULT CALLBACK LinkProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {

	PAINTSTRUCT	ps;
	HDC			hdc;
	HGDIOBJ		hFontbk;
	UINT8		szTextTmp[128];

	switch(msg) 
	{
		case WM_SETCURSOR:
			SetCursor(hCurHand);

			if (link_hwnd_bak != hWnd)
			{
				if (link_hwnd_bak)
					clr_link_str(link_hwnd_bak);
			}

			GetWindowText(hWnd, szTextTmp, sizeof(szTextTmp) );
			hdc=GetDC(hWnd);
			hFontbk = SelectObject(hdc, hFontLink);
			SetTextColor(hdc, RGB(0, 0, 255));	// Blue
			SetBkMode(hdc, TRANSPARENT);
			TextOut(hdc, 0, 0, szTextTmp, lstrlen(szTextTmp));
			SelectObject(hdc, hFontbk);
			ReleaseDC(hWnd, hdc);

			link_hwnd_bak = hWnd;
			return 0;

		case WM_PAINT:
			GetWindowText(hWnd, szTextTmp, sizeof(szTextTmp) );
			hdc=BeginPaint(hWnd, &ps);
			hFontbk = SelectObject(hdc, hFontOrg);
			SetTextColor(hdc, RGB(0, 0, 255));	//青
			SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
			SetBkMode(hdc, OPAQUE);
			TextOut(hdc, 0, 0, szTextTmp, lstrlen(szTextTmp));
			SelectObject(hdc, hFontbk);
			EndPaint(hWnd, &ps);
			return 0;

	}
	return(CallWindowProc(oldLinkProc, hWnd, msg, wp, lp));
}

/*----------------------------------------------------------------------------*\
|   AppAbout( hDlg, uiMessage, wParam, lParam )                                |
|                                                                              |
|   Description:                                                               |
|       This function handles messages belonging to the "About" dialog box.    |
|       The only message that it looks for is WM_COMMAND, indicating the user  |
|       has pressed the "OK" button.  When this happens, it takes down         |
|       the dialog box.                                                        |
|                                                                              |
|   Arguments:                                                                 |
|       hDlg            window handle of about dialog window                   |
|       uiMessage       message number                                         |
|       wParam          message-dependent                                      |
|       lParam          message-dependent                                      |
|                                                                              |
|   Returns:                                                                   |
|       TRUE if message has been processed, else FALSE                         |
|                                                                              |
\*----------------------------------------------------------------------------*/
BOOL CALLBACK AppAbout(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	LOGFONT logfont;
	UINT8 *ptr;
	UINT8 strtmp[128];
	UINT8 strtmp2[128];

	WPARAM wp;

	wp = LOWORD(wParam);

	switch (msg)
	{
	case WM_COMMAND:
		switch (wp)
		{
		case IDOK:
			PostMessage(hwnd,WM_CLOSE,0,0L);
			break;
		case IDC_WWW:
			GetDlgItemText(hwnd, LOWORD(wParam), strtmp, sizeof(strtmp) );
			ShellExecute(hwnd, NULL, strtmp, NULL, NULL, SW_SHOW);
			break;
		case IDC_MAILTO:
			GetDlgItemText(hwnd, LOWORD(wParam), strtmp, sizeof(strtmp) );
			// メアド部分の取得
			ptr = strstr(strtmp, ": ");
			if (ptr != NULL) {
				sprintf_s(strtmp2, sizeof(strtmp2)-1, "mailto:%s", ptr+2);
				ShellExecute(hwnd, NULL, strtmp2, NULL, NULL, SW_SHOW);
			}
			break;
		}
		break;
		
	case WM_CLOSE:
		EndDialog(hwnd,TRUE);
		break;

	case WM_SYSKEYDOWN:
		if (wParam==VK_ESCAPE)
		{
			/* ESC Keyで抜ける */
			PostMessage(hwnd,WM_CLOSE,0,0L);
		}
		break;
		
	case WM_SETCURSOR:
		// 以前のリンクを消去
		if (link_hwnd_bak)
		{
			clr_link_str(link_hwnd_bak);
			link_hwnd_bak = NULL;
		}
		break;

	case WM_INITDIALOG:
		if (scrnmode==SCRN_FULL) flip2gdi();

		link_hwnd_bak = NULL;

		hFontLink = (HFONT)SendMessage(GetDlgItem(hwnd, IDC_WWW), WM_GETFONT, 0, 0);
		GetObject(hFontLink, sizeof(logfont), &logfont);
		logfont.lfUnderline = TRUE;
		hFontLink = CreateFontIndirect(&logfont);
		SendDlgItemMessage(hwnd, IDC_WWW, WM_SETFONT,(WPARAM)hFontLink, 0);

		hFontOrg = (HFONT)SendMessage(GetDlgItem(hwnd, IDC_MAILTO), WM_GETFONT, 0, 0);
		GetObject(hFontOrg, sizeof(logfont), &logfont);
		hFontOrg = CreateFontIndirect(&logfont);

		oldLinkProc = (WNDPROC) GetWindowLong(GetDlgItem(hwnd, IDC_WWW), GWL_WNDPROC);
		SetWindowLong(GetDlgItem(hwnd, IDC_WWW), GWL_WNDPROC, (LONG)LinkProc);
		SetWindowLong(GetDlgItem(hwnd, IDC_MAILTO), GWL_WNDPROC, (LONG)LinkProc);

		return TRUE;
	}
	return FALSE;
}

//------------------------------------------------------------------------------------
// MONITOR ROM DIALOG
//------------------------------------------------------------------------------------
BOOL CALLBACK AppMonDialog(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	WPARAM wp;
	static HWND hCb = NULL;
	static HWND hRb[2];
	static int selCb = 0;
	int tmp;

	wp = LOWORD(wParam);

	switch (msg)
	{
	case WM_COMMAND:
		switch (wp)
		{
		case IDOK:
			menu.selrom = selCb;				// 選択されたＲＯＭを反映
			// ラジオボタンのチェック
			tmp = SendMessage(hRb[0], BM_GETCHECK, 0, 0);
			if (tmp == BST_CHECKED)
				menu.machine = MACHINE_MZ700;

			// 1500モードか？
			tmp = SendMessage(hRb[1], BM_GETCHECK, 0, 0);
			if (tmp == BST_CHECKED)
				menu.machine = MACHINE_MZ1500;

			// モニタ対応機種によりキータイプを自動選択
			if (menu.keytype < 4)				// not GR key?
			{
				tmp = menu.selrom;
				if (tmp != ROM_KIND_1Z013)		// Japanese ROM?
				{
					if (tmp >= ROM_KIND_NEWMON80K && tmp <= ROM_KIND_SP1002)
					{
						// 80K/C/1200 Keyboard
						menu.keytype |= 2;
						set_keytype_menu(menu.keytype);
					}
					else
					{
						// 700/1500 Keyboard
						menu.keytype &= ~2;
						set_keytype_menu(menu.keytype);
					}
				}
			}


			PostMessage(hwnd,WM_CLOSE,0,0L);
			mz_mon_setup();
			mz_reset();
			break;
		case IDCANCEL:
			PostMessage(hwnd,WM_CLOSE,0,0L);
			break;
		// ROMコンボボックス
		case IDC_COMBO_ROM:
			switch( HIWORD(wParam) )
			{
				case CBN_SELCHANGE:	// ROMが選択されたら
					tmp = SendMessage( hCb, CB_GETCURSEL, 0, 0 );
					selCb = SendMessage( hCb, CB_GETITEMDATA, tmp, 0 );
					break;
			}
			break;
		}
		break;
		
	case WM_CLOSE:
		EndDialog(hwnd,TRUE);
		break;

	case WM_SYSKEYDOWN:
		if (wParam==VK_ESCAPE)
		{
			/* ESC Keyで抜ける */
			PostMessage(hwnd,WM_CLOSE,0,0L);
		}
		break;
		
	case WM_INITDIALOG:
		if (scrnmode==SCRN_FULL) flip2gdi();
		selCb = menu.selrom;

		// コンボボックス初期化
		hCb = GetDlgItem( hwnd, IDC_COMBO_ROM );
		init_rom_combo(hCb);							// コンボボックスの初期化
		// ラジオボタン初期化
		hRb[0] = GetDlgItem( hwnd, IDC_RADIO_700 );
		hRb[1] = GetDlgItem( hwnd, IDC_RADIO_1500 );
		if (menu.machine == MACHINE_MZ700)
		{
			SendMessage(hRb[0], BM_SETCHECK, (WPARAM) BST_CHECKED, 0);
			SendMessage(hRb[1], BM_SETCHECK, (WPARAM) BST_UNCHECKED, 0);
		}
		else
		{
			SendMessage(hRb[0], BM_SETCHECK, (WPARAM) BST_UNCHECKED, 0);
			SendMessage(hRb[1], BM_SETCHECK, (WPARAM) BST_CHECKED, 0);
		}

		return TRUE;
	}
	return FALSE;
}

//------------------------------------------------------------------------------------
// SPEED CONTROL DIALOG
//------------------------------------------------------------------------------------
BOOL CALLBACK AppSpeedDialog(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	UINT8 strtmp[128];
	WPARAM wp;
	static HWND hSld = NULL;
	int tmp;

// スピードが何％か表示設定	
#define set_spd_text(v)	sprintf_s(strtmp, sizeof(strtmp), "Speed : %d%%", v); SetDlgItemText(hwnd, IDC_STATIC_SPD, strtmp);
	
	wp = LOWORD(wParam);

	switch (msg){
	  case WM_COMMAND:
		switch (wp) {
			// OKボタン
		  case IDOK:
			// 実行速度を反映
			menu.speed = (int)SendMessage( hSld, TBM_GETPOS, 0, 0);
			// Z80コアに速度を反映
			setup_cpuspeed(menu.speed);

			// ダイアログクローズ
			PostMessage(hwnd,WM_CLOSE,0,0L);
			break;

			// キャンセルボタン
		  case IDCANCEL:
			// ダイアログクローズ
			PostMessage(hwnd,WM_CLOSE,0,0L);
			break;
		}
		break;

      case WM_HSCROLL:
		tmp = (int)SendMessage( hSld, TBM_GETPOS, 0, 0);
		set_spd_text(tmp);
//		sprintf_s(strtmp, sizeof(strtmp), "Speed : %d%%", tmp);
//		SetDlgItemText(hwnd, IDC_STATIC_SPD, strtmp);
		dprintf("x=%d\n", tmp);
		return TRUE;

	  case WM_CLOSE:
		EndDialog(hwnd,TRUE);
		break;

	  case WM_SYSKEYDOWN:
		if (wParam==VK_ESCAPE) {
			/* ESC Keyで抜ける */
			PostMessage(hwnd,WM_CLOSE,0,0L);
		}
		break;

	  case WM_INITDIALOG:
		/* ダイアログ初期化 */
		if (scrnmode == SCRN_FULL) {
			flip2gdi();
		}
		/*  スライダの初期設定 */
		hSld = GetDlgItem( hwnd, IDC_SLIDER1 );
		SendMessage( hSld, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELPARAM(10, 200) );    //  範囲は [0, 255]
		SendMessage( hSld, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)menu.speed );    //  スライダの初期位置は speed
		SendMessage( hSld, TBM_SETTICFREQ, (WPARAM)10, (LPARAM)0 );    //  メモリの間隔は 10
		set_spd_text(menu.speed);										// Static Text (SPEED) 反映
		
		return TRUE;
	}
	return FALSE;
}

/*----------------------------------------------------------------------------*\
|   AppCommand(hwnd, msg, wParam, lParam )                                     |
|                                                                              |
|   Description:                                                               |
|     Handles WM_COMMAND messages for the main window (hwndApp)                |
|   of the parent window's messages.                                           |
|                                                                              |
\*----------------------------------------------------------------------------*/
/*	
	WM_COMMAND 

	wNotifyCode = HIWORD(wParam); // notification code 
	wID = LOWORD(wParam);         // item, control, or accelerator identifier 
	hwndCtl = (HWND) lParam;      // handle of control 
*/
LONG AppCommand (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	int i;
#if USE_DDRAW
	long t;
	BOOL fActbak;
#endif // USE_DDRAW
	
	i = LOWORD(wParam);

	// Select keyboard matrix type?
	if (i>=MENU_KEYTYPE_BASE && i<(MENU_KEYTYPE_BASE + get_keymat_max() ) )
	{
		i -= MENU_KEYTYPE_BASE;
		if (i != menu.keytype)
		{
			menu.keytype=i;
			set_keytype_menu(menu.keytype);
		}
		return 0;
	}

	// other menu
	switch (i)
	{
	case MENU_ABOUT:
		DialogBox(hInstApp,MAKEINTRESOURCE(IDD_DIALOG1),hwnd,(DLGPROC)AppAbout);
		break;
		
	case MENU_ROM:
		DialogBox(hInstApp,MAKEINTRESOURCE(IDD_ROMDIALOG),hwnd,(DLGPROC)AppMonDialog);
		break;
		
	case MENU_SPEED:
		DialogBox(hInstApp,MAKEINTRESOURCE(IDD_SPDDIALOG),hwnd,(DLGPROC)AppSpeedDialog);
		break;
		
	case MENU_EXIT:
		PostMessage(hwnd,WM_CLOSE,0,0L);
		break;
		
	case MENU_OPEN:
		OpenFileImage();
		break;
		
	case MENU_SET:
		SetLoadFileImage();
		break;
		
	case MENU_TAPESAVE:
		SetSaveFileImage();
		break;
		
	case MENU_SETQD:
		SetQDFileImage();
		break;
		
	case MENU_EJECTQD:
		EjectQDFileImage();
		break;
		
	case MENU_LOADRAMFILE:
		LoadRamFileImage();
		break;

	case MENU_SAVERAMFILE:
		SaveRamFileImage();
		break;

	case MENU_PCG700:
		menu.pcg700^=1;
		set_pcg700_menu(menu.pcg700);
		mz_refresh_screen(REFSC_VRAM);
		DrawMenuBar(hwnd);
		break;
		
	case MENU_RESET:
		mz_reset();
		break;

	case MENU_LOAD_STATE:
		LoadStateFile();
		break;

	case MENU_SAVE_STATE:
		SaveStateFile();
		break;

	case MENU_DB_INFO:
		ShowInfo();
		break;
		
#if _DEBUG		
	case MENU_DB_BREAK:
		if (Z80_Trace==0)
			MessageBox(hwndApp, "Entering TRACE MODE" ,
					   "Information", MB_ICONINFORMATION|MB_OK);
		Z80_Trace=1;
		break;
		
	case MENU_DB_TRACE:
		if (Z80_Trace==0)
			MessageBox(hwndApp, "TRACE" ,
					   "Information", MB_ICONINFORMATION|MB_OK);
		Z80_Trace=3;
		break;

	case MENU_DB_CONTINUE:
		Z80_Trace=0;
		break;
#endif		


		/* Screen */
	case MENU_SCREEN_1_1:
	case MENU_SCREEN_2_2:
	case MENU_SCREEN_3_3:
	case MENU_SCREEN_4_4:
		i=LOWORD(wParam)-MENU_SCREEN_1_1;
		if (i != menu.screen)
		{
			menu.screen=i;
			mz_refresh_screen(REFSC_ALL);
			set_screen_menu(menu.screen);
		}
		break;
		
		/* Refresh */
	case MENU_REFRESH_EVERY:
	case MENU_REFRESH_2:
	case MENU_REFRESH_3:
	case MENU_REFRESH_4:
		i=LOWORD(wParam)-MENU_REFRESH_EVERY;
		if (i != menu.scrn_freq)
		{
			menu.scrn_freq=i;
			mz_refresh_screen(REFSC_ALL);
			set_select_chk(MENU_REFRESH_EVERY,4,menu.scrn_freq);
		}
		break;

	case MENU_FULLSCREEN:
#if USE_DDRAW
		t = get_timer();
		if (t < (fullsc_timer + 3000))
		{
			return TRUE;
		}
		fullsc_timer = t;

		fActbak = fAppActive;
		fAppActive = FALSE;

		switch (scrnmode)
		{
		case SCRN_WINDOW:
			win_xbak = win_xpos;
			win_ybak = win_ypos;
			
			if (chg_screen_mode(SCRN_FULL))
			{
				scrnmode = SCRN_FULL;
				set_fullscreen_menu(TRUE);
			}
			break;
			
		case SCRN_FULL:
			if (chg_screen_mode(SCRN_WINDOW))
			{
				scrnmode = SCRN_WINDOW;
				set_fullscreen_menu(FALSE);
				win_xpos = win_xbak;
				win_ypos = win_ybak;
				set_screen_menu(menu.screen);
			}
			break;
		}
		
		fAppActive = fActbak;
#endif //USE_DDRAW
		break;
		
		/* FontSet */
	case MENU_FONT_EUROPE:
	case MENU_FONT_JAPAN:
		i=LOWORD(wParam)-MENU_FONT_EUROPE;
		if (i != menu.fontset)
		{
			// ここにロードチェックを入れる
			if (!font_load(i))
			{
				menu.fontset=i;
				set_fontset_menu(menu.fontset);
			}
		}
		break;
		
	}
	return 0L;
}

/******************/
/* システムタスク */
/******************/
BOOL SystemTask(void)
{
    MSG msg;
	HANDLE hProcess;

	/* メッセージ処理 */
    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		/* アクセラレータキーの処理 */
		if (hAccelApp)
			TranslateAccelerator(hwndApp, hAccelApp, &msg);

		/* Alt+Tab で、タスク切り替えされた時 */
		if (msg.message == WM_SYSKEYDOWN)
		{
			if ((msg.wParam == VK_MENU)) 
			{
				if (!(HIWORD(msg.lParam) & 0x8000))
				{
					if (scrnmode == SCRN_FULL && (fAppActive) )
					{
						// 全画面であれば、プロセスの優先度を通常に
						hProcess = GetCurrentProcess();
						if (hProcess)
							SetPriorityClass(hProcess, NORMAL_PRIORITY_CLASS);
					}
				}
			}
		}
		
		/* 通常のメッセージ処理 */
        if (!GetMessage(&msg, NULL, 0, 0))
		{
			/* プログラム終了時 */
			AppExit();													/* プログラム終了 */
			exit(msg.wParam);
		}

        TranslateMessage(&msg);
		DispatchMessage(&msg);

		return TRUE;
    }
	else
	{
		/* メッセージが無いとき */
	    return FALSE;													/* メインタスクへ */
	}
}

//-----------------------------------------------------
// メニュー用ビットマップボタンの自前描画
//-----------------------------------------------------
void putbutton(HDC hdc,int x,int y,int no,BOOL sw)
{
	HGDIOBJ bkGDIOBJ;
	HPEN hpen;
	HDC hmdc;

    hmdc = CreateCompatibleDC(hdc);
	// icon
	bkGDIOBJ = SelectObject( hmdc, hmenuBitmap );
	BitBlt(hdc,x+1,y+1,MENU_BMP_WIDTH,MENU_BMP_HEIGHT,
			   hmdc,MENU_BMP_WIDTH*no,0,SRCCOPY);

	SelectObject( hmdc, bkGDIOBJ);
	DeleteDC(hmdc);

	// 3D Frame
	if (sw)
	{
		hpen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DHILIGHT));
		bkGDIOBJ = SelectObject(hdc,hpen);
		MoveToEx(hdc,x,y,NULL);
		LineTo(hdc,x+MENU_BMP_WIDTH+2,y);
		DeleteObject(hpen);
		hpen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DDKSHADOW));
		SelectObject(hdc,hpen);
		LineTo(hdc,x+MENU_BMP_WIDTH+2,y+MENU_BMP_HEIGHT+2);
		DeleteObject(hpen);
		hpen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DSHADOW));
		SelectObject(hdc,hpen);
		LineTo(hdc,x,y+MENU_BMP_HEIGHT+2);
		DeleteObject(hpen);
		hpen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DLIGHT));
		SelectObject(hdc,hpen);
		LineTo(hdc,x,y);
		SelectObject(hdc,bkGDIOBJ);
		DeleteObject(hpen);
	}

}

//-----------------------------------------------------
// メニュー用テキスト用自前描画
//-----------------------------------------------------
BOOL MENU_TextOut(HWND hwnd,HDC hdc, int x, int y,LPCTSTR string, int cbString)
{
	LOGFONT logfont;
	HFONT hfontBK,hfont,hfont_u;
	HFONT hfdummy;
	SIZE  sz;
	int i,xt,tab;
	BYTE ch;
	BOOL under;

	xt = 0;
	tab =0;
	under = FALSE;

	//
	hfdummy = GetStockObject(DEFAULT_GUI_FONT);
	if (hfdummy == NULL)
		hfdummy = GetStockObject(DEVICE_DEFAULT_FONT);

	hfont = SelectObject(hdc,hfdummy);
	GetObject(hfont, sizeof(logfont), &logfont);
	logfont.lfUnderline = TRUE;
	hfont_u = CreateFontIndirect(&logfont);

	//
	for (i=0;i<cbString;i++)
	{
		ch = string[i];
		if (ch == '\t')
		{
			tab = (xt & (~0x3F)) + 0x60;
			continue;
		}

		if (ch == '&')
		{
			under = TRUE;
			continue;
		}
		else
		{
			if (under)
				hfontBK = SelectObject(hdc, hfont_u);

			// TAB
			if (tab)
			{
				while (tab > xt)
				{
					GetTextExtentPoint32(hdc," ",1,&sz);
					xt += sz.cx;
				}
				xt = tab;
				tab = 0;
			}
			// Put TEXT
			if (IsDBCSLeadByte(string[i]))
			{
				// DBCS
				GetTextExtentPoint32(hdc,string+i,2,&sz);
				TextOut(hdc, x+xt, y,string+i,2);
				i++;
			}
			else
			{
				// ANSI
				GetTextExtentPoint32(hdc,string+i,1,&sz);
				TextOut(hdc, x+xt, y,string+i,1);
			}

			if (under)
			{
				SelectObject(hdc, hfontBK);
				under = FALSE;
			}
			xt += sz.cx;
		}
	}

	SelectObject(hdc,hfdummy);
	DeleteObject(hfont_u);
	return TRUE;
}

//
void MENU_GetTextExtentPoint(HDC hdc,LPCSTR string, int len, SIZE *szr)
{
	LOGFONT logfont;
	HFONT hfont,hfont_u;
	HFONT hfdummy;
	SIZE  sz;
	UINT8 strtmp[128];
	int len2;
	int i,xt,yt;
	UINT8 ch;

	// &の削除
	for (xt=0,i=0;i<len;i++)
	{
		if (string[i]<0x20) continue;
		if (string[i]=='&') continue;
		strtmp[xt++]=string[i];
	}
	strtmp[xt]=0;

	//
	xt = yt = 0;

	//
	hfdummy = GetStockObject(DEFAULT_GUI_FONT);
	if (hfdummy == NULL)
		hfdummy = GetStockObject(DEVICE_DEFAULT_FONT);

	hfont = SelectObject(hdc,hfdummy);

	GetTextExtentPoint32(hdc,strtmp,lstrlen(strtmp),szr);
	SelectObject(hdc,hfdummy);
	return;
	
	GetObject(hfont, sizeof(logfont), &logfont);
	logfont.lfUnderline = TRUE;
	hfont_u = CreateFontIndirect(&logfont);

	//
	len2 = lstrlen(strtmp);
	for (i=0; i<len2; i++)
	{
		ch = strtmp[i];

		if (ch == '\t')
		{
			xt = (xt & (~0x3F))+0x50;
			continue;
		}
		
			// Put TEXT
			if (IsDBCSLeadByte(strtmp[i]))
			{
				// DBCS
				GetTextExtentPoint32(hdc,strtmp+i,2,&sz);
				i++;
			}
			else
			{
				// ANSI
				GetTextExtentPoint32(hdc,strtmp+i,1,&sz);
			}

		xt += sz.cx;
		if (yt < sz.cy)
			yt = sz.cy;
	}

	if (szr)
	{
		szr->cx = xt;
		szr->cy = yt;
	}

	SelectObject(hdc,hfdummy);
	DeleteObject(hfont_u);
}


/* ウィンドウプロシージャ */
LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WPARAM wtmp;
	PAINTSTRUCT ps;
	HDC hdc;
	WINDOWPOS *pwp;
	LPMEASUREITEMSTRUCT lpMI;
	LPDRAWITEMSTRUCT lpDI;

	LPCSTR st;
	RECT rc;
    SIZE sz;
	TBMPMENU *tp;
	BOOL f;
	
	wtmp = wParam&0xFFF0;


	switch (message)
    {
	case WM_SYSCOMMAND:
		/* フルスクリーンモード時の時の処理 */
		if (scrnmode == SCRN_FULL)
		{
			/* 何だかこれでスクリーンセーバーが動かないようになるらしい。 */
			if((wtmp==SC_SCREENSAVE || wtmp==SC_MONITORPOWER))
				return FALSE;

			if (fAppActive)
			{
				if (wtmp==SC_MOUSEMENU) return FALSE;
				if (wtmp==SC_KEYMENU)
				{
					return FALSE;
				}
			}
			
		}
			
		break;
		
	case WM_COMMAND:
		return AppCommand(hwnd,message,wParam,lParam);

	case WM_ENTERIDLE:
	case WM_ENTERMENULOOP:
		fAppActive = FALSE;
		break;

	case WM_MEASUREITEM:
		lpMI = (LPMEASUREITEMSTRUCT)lParam;
		if (lpMI->CtlType == ODT_MENU)
		{
			hdc = GetDC(hwnd);
			tp = get_menu_work(lpMI->itemID);
			MENU_GetTextExtentPoint(hdc,tp->menustr,lstrlen(tp->menustr),&sz);
			if (sz.cy < MENU_BMP_HEIGHT) sz.cy = MENU_BMP_HEIGHT;
			lpMI->itemWidth = sz.cx;
			lpMI->itemHeight= (sz.cy + 2);
			lpMI->itemWidth += (MENU_BMP_WIDTH+6);
		    ReleaseDC(hwnd, hdc);
			return TRUE;
		}
		break;

	case WM_DRAWITEM:
		lpDI = (LPDRAWITEMSTRUCT)lParam;
		st = (LPCSTR)lpDI->itemData;
		hdc = lpDI->hDC;
		rc = lpDI->rcItem;

		tp = get_menu_work(lpDI->itemID);

		// selected color
        if (lpDI->itemState & ODS_SELECTED)
		{
			if ((tp && tp->menuBMP) || (lpDI->itemState & ODS_CHECKED) )
				rc.left+=(MENU_BMP_WIDTH+6);
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
			FillRect(hdc,&rc,(HBRUSH)(COLOR_HIGHLIGHT+1) );
        } 
		else
		{
			FillRect(hdc,&rc,(HBRUSH)(COLOR_MENU+1) );
		}

		if (lpDI->itemState & ODS_GRAYED)
		{
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
		}

		rc = lpDI->rcItem;
		if (tp)
		{
			// text
			MENU_TextOut(lpDI->hwndItem,hdc, rc.left+MENU_BMP_WIDTH+6, rc.top+2,
						 (LPCSTR)tp->menustr, lstrlen(tp->menustr));
			// icon
			if (tp->menuBMP)
				putbutton(hdc,rc.left+1,rc.top+1,tp->menuBMP-1,
						  (BOOL)(lpDI->itemState & ODS_SELECTED));
			// check
			if (lpDI->itemState & ODS_CHECKED)
				putbutton(hdc,rc.left+1,rc.top+1,5, TRUE);

		}
		//MENU_TextOut(lpDI->hwndItem,hdc, rc.left+MENU_BMP_WIDTH+6, rc.top+4,(LPCSTR)"開く(&O)",lstrlen((LPCSTR)"開く(&O)"));

//		GetMenuString(GetMenu(hwnd),lpDI->itemID,strmenu,sizeof(strmenu),MF_BYCOMMAND);
//		GetMenuString(hmenuApp,MENU_OPEN,strmenu,sizeof(strmenu),MF_BYCOMMAND);
		break;

	case WM_ENABLE:
		if ((BOOL)wParam)
		{
			fAppActive = TRUE;
			mz_keyport_init();
		}
		break;
		
	case WM_EXITMENULOOP:
		fAppActive = TRUE;
		mz_keyport_init();
		break;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == hwnd) break;
		
		/* fall through to WM_QUERYNEWPALETTE */

	case WM_QUERYNEWPALETTE:
		if (scrnmode != SCRN_FULL)
		{
			hdc = GetDC(hwnd);
		
			if (hpalApp) SelectPalette(hdc, hpalApp, FALSE);
		
			f = RealizePalette(hdc);
			ReleaseDC(hwnd,hdc);
			
			if (f) InvalidateRect(hwnd,NULL,TRUE);
		}
		
		return f;

		/* NC Paint */
	case WM_NCPAINT:
		if (scrnmode == SCRN_FULL)
		{
			return FALSE;
		}
		break;
		
		/* Paint */
	case WM_PAINT:
		if (scrnmode != SCRN_FULL)
		{
			hdc = BeginPaint(hwnd,&ps);
			AppPaint (hwnd,hdc);
			EndPaint(hwnd,&ps);
		}
		else
		{
			break;
		}

		return FALSE;

		/* DirectInputを復帰 */
	case WM_ACTIVATE:
//		ReacquireMouse();
		break;

	case WM_ACTIVATEAPP:
		if (hwnd == hwndApp) fAppActive = (BOOL)wParam;
//		printf("fAppActive = %d\n",fAppActive);
		break;

	case WM_SETCURSOR:
		break;

	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
		{
			mouse_cursor(TRUE);
		}
		else
		if (wParam == SIZE_RESTORED)
		{
			if (scrnmode == SCRN_FULL)
			{
				mouse_cursor(FALSE);
			}
		}
		break;

	case WM_KEYDOWN:
		mz_keydown(wParam);
		break;
		
	case WM_KEYUP:
		mz_keyup(wParam);
		break;
		
	case WM_WINDOWPOSCHANGED:
		if (scrnmode == SCRN_WINDOW)
		{
			pwp = (WINDOWPOS *) lParam;
			win_xpos = pwp->x;
			win_ypos = pwp->y;
			adjust_window_size(hwnd, menu.screen);			// メニューバーが２段になった場合、下が切れる現象の対策
		}
		break;

	case WM_QUERYENDSESSION:
		return TRUE;

	case WM_ENDSESSION:
		PostMessage(hwnd,WM_CLOSE,0,0);

	case WM_CLOSE:
		break;

	case WM_CREATE:
		init_menu_work();
		InitMenuBitmaps(hwnd);

		/* IME使用禁止 */
		if (ptr_WINNLSEnableIME) {
			(*ptr_WINNLSEnableIME)( hwnd , FALSE );
		}
		/* 画面初期化 */
		mz_screen_init();

		break;

	case WM_DESTROY:
		EndMenuBitmaps(hwnd);

		fAppActive = FALSE;
		
		PostQuitMessage(0);
		return TRUE;
    }

	return DefWindowProc (hwnd, message, wParam, lParam) ;
}

/******************/
/* 各種あとしまつ */
/******************/
void free_resource(void)
{
	HWND hDest;

	XInput_Cleanup();

	DSound_Cleanup();												// DirectSound 停止

	sn76489an_cleanup();
	mzbeep_clean();

	end_thread();
	free_mmtimer();													/* マルチメディアタイマー終了 */

	EndDirectDraw();												/* DirectDraw 終了 */
	mz_free_mem();													/* エミュレータ用メモリの開放 */
	end_defkey();													/* キーマトリクス変換終了 */
	
	// タスクバーに空のアイコンが残る事がある現象の回避
	// のつもりがいまいちうまくいかない・・・
	hDest=FindWindow(szClassName, NULL);
	if (hDest) {
		ShowWindow(hDest,SW_HIDE);
	}
	
	// 全画面だったらウィンドウ位置bkupを戻す
	if (scrnmode==SCRN_FULL) {
		win_xpos = win_xbak;
		win_ypos = win_ybak;
	}
	
	if (hUSER32) {
		FreeLibrary(hUSER32);
	}

}

/* ＷｉｎＭａｉｎ */
int APIENTRY WinMain (HINSTANCE hInstance, 
	     HINSTANCE hPrevInstance,
	     char * lpszCmdParam,
	     int nCmdShow)
{
	HWND  hwnd;
	HDC	  hdc;
	HANDLE hPrevMutex;
	WNDCLASS wndclass;
	DWORD dwStyle;														/* ウィンドウ スタイル */
	RECT destrect;
	int a,i;

#ifndef NDEBUG
	// ＶＣランタイムデバッグモードＯＮ
	int nFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	nFlag &= ~(_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_DELAY_FREE_MEM_DF|_CRTDBG_LEAK_CHECK_DF|
		_CRTDBG_CHECK_EVERY_16_DF|_CRTDBG_CHECK_EVERY_128_DF|_CRTDBG_CHECK_EVERY_1024_DF);

	nFlag |= (_CRTDBG_ALLOC_MEM_DF| /*_CRTDBG_CHECK_CRT_DF |*/ _CRTDBG_LEAK_CHECK_DF|_CRTDBG_CHECK_ALWAYS_DF);
	_CrtSetDbgFlag(nFlag);
#endif

	/* コマンドラインチェック */
	for (i=0;;i++) {
		a = lpszCmdParam[i];
		
		if (a=='\x00') break;											// コマンドラインの終了チェック
		if (a!='-' && a!='/') continue;									// [-|/] チェック
		
		a = lpszCmdParam[++i];											// 次の文字をチェック
		if (a=='\x00') break;											// コマンドラインの終了チェック
		switch (a) {
		case 'c':
			if (sscanf_s(lpszCmdParam+i+1,"%2hx",&bk_color) != 1) {
				bk_color = 0x70;
			}

			/* 簡易MZ-80K/C/1200モード */
			i+=2;
			break;
			
	  case 's':
		  /* 禁止モード判定 */
		  sound_di = 1;
		  break;

	  case '5':
		  /* MZ-1500モード許可 */
		  menu.machine = MACHINE_MZ1500;
		  break;

	  case 'k':
		  /* Key Table Patch許可 */
		  key_patch = 1;
		  break;
		}
		
	}
	
	/* Load accelerators */
	hAccelApp = LoadAccelerators(hInstance, "AppMenu");

	/* 二重起動チェック */
	hPrevMutex = OpenMutex(MUTEX_ALL_ACCESS,FALSE,szAppName);
	/*もし、オープンできれば以前のアプリケーションが起動している */
	if (hPrevMutex)
	{
		CloseHandle(hPrevMutex);
		return FALSE;
	}
	
	hMutex = CreateMutex(FALSE,0,szAppName);
	
	/* ウインドウクラスの生成 */
//	wndclass.style   = CS_BYTEALIGNCLIENT | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wndclass.style   = CS_BYTEALIGNCLIENT;
	wndclass.lpfnWndProc   = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance  = hInstance;
	wndclass.hIcon   = LoadIcon (hInstance, "AppIcon");
	wndclass.hCursor    = LoadCursor (NULL, IDC_ARROW);
	wndclass.hbrBackground = GetStockObject (BLACK_BRUSH);
	wndclass.lpszMenuName  = "AppMenu";
	wndclass.lpszClassName = szClassName;
	
	/* Get Hand-Cursor */
	hCurHand = LoadCursor(hInstance, MAKEINTRESOURCE(IDC_CURHAND));

	if (!RegisterClass (&wndclass))
	{
// 		ReleaseMutex(hMutex);
		return FALSE;
	}

	/* LoadLibrary */
	hUSER32 = GetModuleHandle("USER32.DLL");
	if (hUSER32)
	{
		// WINNLSEnableIMEがあるのはDouble-Byte言語圏のみ
		ptr_WINNLSEnableIME = (LPFNDLLFUNC) GetProcAddress(hUSER32, "WINNLSEnableIME");
//		if (ptr_WINNLSEnableIME == NULL)
//			MessageBox(hwndApp, "Error GetProcAddress" ,"Debug Information",MB_ICONINFORMATION|MB_OK);
		
	}
	else
	{
		MessageBox(hwndApp, "Error GetModuleHandle" ,"Debug Information",MB_ICONINFORMATION|MB_OK);
	}
		
	/* INIファイルの設定を読み込み */
	load_inifile();	
	get_window_size(menu.screen);										/* win_sx,win_sy calc */
	
	/* スクリーンの広さを調べる */
	GetClientRect(GetDesktopWindow(),&destrect);
	if (win_xpos == -99999) win_xpos = (destrect.right-win_sx)>>1;
	if (win_ypos == -99999) win_ypos = (destrect.bottom-win_sy)>>1;

	/* Windowはみ出しチェック */
	if (win_xpos >= (destrect.right-2)) win_xpos = (destrect.right-win_sx)>>1; // destrect.right-2;
	if (win_ypos >= (destrect.bottom-2)) win_xpos = (destrect.bottom-win_sy)>>1; // destrect.bottom-2;
		
	win_xbak = win_xpos;
	win_ybak = win_ypos;
	
	dwStyle = WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;

	/* キーマトリクス関連のワークを初期化 */
	init_defkey();

	/* グローバルなウィンドウ関連の変数をセット */
	hInstApp=hInstance;													/* hInstApp=アプリのインスタンス */

	/* ウインドウの生成 */
	hwnd = CreateWindowEx (
		WS_EX_LEFT,
		szClassName,									/* Class */
		szAppName,										/* caption */
		dwStyle,										/* style */
		win_xpos,										/* XPos: */
		win_ypos,										/* Ypos: */
		win_sx,										/* Window Xsize */
		win_sy,										/* Window YSize */
		NULL,
		NULL,
		hInstance,
		NULL);

	hwndApp=hwnd;														/* hwndApp=アプリのウィンドウハンドル */
	hmenuApp=GetMenu(hwnd);												/* メニューハンドル */

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	/* ミューテクスのリリース */
//	ReleaseMutex(hMutex);

	// rom_load()内で設定される
//	use_cmos = 1;														// 1R12 オン

	/* 画面モードを調べる */
	hdc = GetDC(hwnd);
    a = GetDeviceCaps(hdc, BITSPIXEL);
	ReleaseDC(hwnd,hdc);
	
	SystemTask();

	/* ウィンドウ状態変数初期化 */
	fAppActive = TRUE;

	/* ２つのフォントファイル名作成 */
	GetCurrentDirectory(sizeof(FontFileStr[0]),FontFileStr[0]);
	lstrcat(FontFileStr[0],"\\mz700fon.dat");
	GetCurrentDirectory(sizeof(FontFileStr[1]),FontFileStr[1]);
	lstrcat(FontFileStr[1],"\\mz700fon.jp");

	/* フォントメニュー選択可能かチェック */
	/* EUROPE */
	if (!FileExists(FontFileStr[0]))
	{
		EnableMenuItem(hmenuApp, MENU_FONT_EUROPE, MF_BYCOMMAND | MF_GRAYED); // EUROPE
		if (menu.fontset==0) menu.fontset=1;
	}
	
	/* JAPAN */
	if (!FileExists(FontFileStr[1]))
	{
		EnableMenuItem(hmenuApp, MENU_FONT_JAPAN, MF_BYCOMMAND | MF_GRAYED); // JAPAN選択不能
		if (menu.fontset==1) menu.fontset=0;
	}
	
	fullsc_timer = 0;
	
	/* アプリ初期化を呼ぶ */
	if ( !AppInit() )
	{
		mz_exit(1);
		return FALSE;
	}

	__try
	{
		/* メインループ開始 */
		AppMain();
	}
	__finally
	{
		/* リソース開放 */
		free_resource();
	}
	
	return 0;
}

//
BOOL isAppActive(void)
{
	return fAppActive;
}

//
int get_scrnmode(void)
{
	return scrnmode;
}

//--------------------------------------------------------------
// フォントデータを読み込む
//--------------------------------------------------------------
/*
  In:	 0 = Europe
		 1 = Japan
  Out:	 0 = OK
		-1 = NG
 */
int font_load(int md)
{
	int result = 0;
	FILE_HDL fp;

	/* フォントデータを読み込む */
	fp = FILE_ROPEN(FontFileStr[md]);
	if (fp != FILE_VAL_ERROR)
	{
		FILE_READ(fp, font, 4096);
		FILE_CLOSE(fp);
	}
	else
		result = -1;
	
	// 高速文字表示のために圧縮
	compress_font();

	// VRAM Rebuild & BLT
	mz_refresh_screen(REFSC_ALL);

	return result;
}
