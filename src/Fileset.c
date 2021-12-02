//----------------------------------------------------------------------------
// File:fileset.c
//
// mz700win:Set/Open Image File Module
// ($Id: Fileset.c 20 2009-05-25 18:20:32Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#define _FILESET_H_

#include <windows.h>
#include "resource.h"

#include "mz700win.h"

#include "z80.h"
#include "win.h"
#include "fileio.h"

#include "mzmain.h"
#include "mzmon1em.h"

#include "fileset.h"

#include "MZhw.h"
#include "MZscrn.h"

//////////////////////////
// ファイル名表示用文字列
//////////////////////////
/* for Japanese */
static const UINT8 mzascii_j[]=
{
	"　！”＃＄％＆’（）＊＋，－．／"\
	"０１２３４５６７８９：；＜＝＞？"\
	"＠ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯ"\
	"ＰＱＲＳＴＵＶＷＸＹＺ［＼］↑←"\
	"※※※※※※※※※※※※※※※※"\
	"日月火水木金土生年時分秒円￥￡▼"\
	"↓。「」、．ヲァィゥェォャュョッ"\
	"ーアイウエオカキクケコサシスセソ"\
	"タチツテトナニヌネノハヒフヘホマ"\
	"ミムメモヤユヨラリルレロワン゛゜"\
	"→※※※※※※※※※※※※※※※"\
	"※※※※※※※※※※※※※※※※"\
	"※※※※※※※※※※※※※※※※"\
	"※※※※※※※※※※※※※※※π"

};

/* for Europe */
static const UINT8 mzascii_e[]=
{
	" !\x22#$%&\x27()*+,-./"											/* 20 */
	"0123456789:;<=>?"													/* 30 */
	"@ABCDEFGHIJKLMNO"													/* 40 */
	"PQRSTUVWXYZ[\\]**"													/* 50 */
	"****************"													/* 60 */
	"****************"													/* 70 */
	"}***************"													/* 80 */
	"_**`~***********"													/* 90 */
	"****************"													/* A0 */
	"**************{*"													/* B0 */
	"****************"													/* C0 */
	"****************"													/* D0 */
	"****************"													/* E0 */
	"****************"													/* F0 */
};

// Filter of Tape Image File
LPCSTR MZT_filter = "MZ File Images(*.MZT;*.M12;*.MZF)\x0*.MZT;*.M12;*.MZF\x0\x0";
LPCSTR MZT_ext = "mzt";

// Filter of RAMFILE Image File
LPCSTR MZR_filter = "MZ RAM File Images(*.MZR)\x0*.MZR\x0\x0";
LPCSTR MZR_ext = "mzr";

// Filter of State File
LPCSTR MZS_filter = "MZ State Files(*.MZS)\x0*.MZS\x0\x0";
LPCSTR MZS_ext = "mzs";

// Question Msg
// for Append
LPCSTR APD_msg = "The file %s\ralready exists. Append to existing file?";
// for Overwrite
LPCSTR OVW_msg = "The file %s\ralready exists. Overwrite ?";

/**************************/
/* セーブ用イメージの指定 */
/**************************/
static BOOL setfile_forsave(LPCSTR ttlmsg, UINT8 *fnbuf, UINT8 *opendir,
							LPCSTR askmsg,LPCSTR fltstr, LPCSTR extstr)
{
	OPENFILENAME ofn;
	DWORD ecode;
	UINT8 filebuf[MAX_PATH];
	UINT8 tempdir[MAX_PATH];
	UINT8 msg[512];

	/* ファイルオープンダイアログを開く */
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);								/* 構造体サイズ */
	ofn.hwndOwner = hwndApp;											/* ダイアログのオーナー */
	ofn.Flags = OFN_LONGNAMES|OFN_HIDEREADONLY;
	ofn.lpstrFilter = fltstr;
	ofn.lpstrDefExt = extstr;
	ofn.lpstrTitle = ttlmsg;
	ofn.lpstrFile = filebuf;											/* ファイル名バッファポインタ */
	ofn.nMaxFile = sizeof(filebuf);										/* ファイル名バッファサイズ */

	ZeroMemory(&filebuf,sizeof(filebuf));								/* ファイル名入力バッファ初期化 */
	lstrcpy(filebuf, fnbuf);											/* セーブファイル名 */

	GetCurrentDirectory(sizeof(tempdir), tempdir);						/* カレントディレクトリ取得 */
	ofn.lpstrInitialDir = opendir;									/* カレントディレクトリ設定 */

	if (!GetSaveFileName(&ofn))
	{
		ecode = CommDlgExtendedError();
		SetCurrentDirectory(tempdir);								/* カレントディレクトリ復帰 */
		return FALSE;												/* キャンセル */
	}
	
	if (FileExists(ofn.lpstrFile))
	{
		// ファイルは既に存在するが良いか問い合わせるメッセージ
		wsprintf(msg, askmsg, ofn.lpstrFile);

		if (MessageBox(hwndApp, msg , "MZ700WIN",MB_ICONQUESTION|MB_YESNO) != IDYES)
		{
			return FALSE;
		}
		
	}
	// pathはフルパス。拡張子の補完を忘れずに
	// 
	lstrcpy(fnbuf, ofn.lpstrFile);									/* セーブファイル名をコピー */
	GetCurrentDirectory(IniFileStrBuf, opendir);					/* カレントディレクトリ取得 */
	
	SetCurrentDirectory(tempdir);									/* カレントディレクトリ復帰 */

	return TRUE;
}

/****************************/
/* Ｏｐｅｎ用イメージの指定 */
/****************************/
static BOOL setfile_foropen(LPCSTR ttlmsg, UINT8 *fnbuf, UINT8 *opendir,
							LPCSTR fltstr, LPCSTR extstr)
{
	OPENFILENAME ofn;
	DWORD ecode;
	UINT8 filebuf[MAX_PATH];
	UINT8 tempdir[MAX_PATH];

	/* ファイルオープンダイアログを開く */
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);								/* 構造体サイズ */
	ofn.hwndOwner = hwndApp;											/* ダイアログのオーナー */
	ofn.Flags = OFN_LONGNAMES|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	ofn.lpstrFilter = fltstr;
	ofn.lpstrDefExt = extstr;
	ofn.lpstrTitle = ttlmsg;
	ofn.lpstrFile = filebuf;											/* ファイル名バッファポインタ */
	ofn.nMaxFile = sizeof(filebuf);										/* ファイル名バッファサイズ */

	ZeroMemory(&filebuf,sizeof(filebuf));								/* ファイル名入力バッファ初期化 */
	lstrcpy(filebuf, fnbuf);											/* セーブファイル名 */

	GetCurrentDirectory(sizeof(tempdir), tempdir);						/* カレントディレクトリ取得 */
	ofn.lpstrInitialDir = opendir;										/* カレントディレクトリ設定 */

	if (!GetOpenFileName(&ofn))
	{
		ecode = CommDlgExtendedError();
		SetCurrentDirectory(tempdir);									/* カレントディレクトリ復帰 */
		return FALSE;													/* キャンセル */
	}
	
	// pathはフルパス。拡張子の補完を忘れずに
	// 
	lstrcpy(fnbuf, ofn.lpstrFile);										/* セーブファイル名をコピー */
	GetCurrentDirectory(IniFileStrBuf, opendir);						/* カレントディレクトリ取得 */
	
	SetCurrentDirectory(tempdir);										/* カレントディレクトリ復帰 */

	return TRUE;
}

//////////////////////////
// CMOSのデータを読み込む
//////////////////////////
void mz_load_cmos(void)
{
	HANDLE fh;
	DWORD rbytes;
	int i;

	fh = FILE_ROPEN(CmosFileStr);
	if (fh != FILE_VAL_ERROR)
	{
		rbytes = FILE_READ(fh, mz1r12_ptr, MZ1R12_SIZE);
		FILE_CLOSE(fh);
		if (rbytes < MZ1R12_SIZE)
		{
			for (i=0;i<MZ1R12_SIZE;i++)
				mz1r12_ptr[i]=i&0xFF;
		}
	}
	else
	{
		for (i=0;i<MZ1R12_SIZE;i++)
			mz1r12_ptr[i]=i&0xFF;
	}

}

/////////////////////////////
// Save 'CMOS DATA' to File
/////////////////////////////
void mz_save_cmos(void)
{
	HANDLE fh;
	DWORD wbytes;

	fh = FILE_COPEN(CmosFileStr);
	if (fh != FILE_VAL_ERROR)
	{
		wbytes = FILE_WRITE(fh, mz1r12_ptr, MZ1R12_SIZE);
		FILE_CLOSE(fh);
	}

}

/*************************/
/* mztファイルを読み込む */
/*************************/
BOOL load_mzt_file(char *filename)
{
	MZTHED m12hed;
	FILE_HDL fp;
	int i,c;
	UINT8 *ptr;
	UINT8 strtmp[256];
	UINT8 strtmp2[64];

	fp = FILE_ROPEN(filename);
	if (fp == FILE_VAL_ERROR) return FALSE;

	lstrcpy(tapefile,filename);
	UpdateTapeMenu();

	FILE_READ(fp,&m12hed,128);											/* ヘッダ読み込み */
	FILE_READ(fp,mem+RAM_START+m12hed.mz_topaddr,m12hed.mz_filesize);	/* 本体読み込み */
	FILE_CLOSE(fp);
//	set_tapepos(128 + m12hed.mz_filesize);
	set_tapepos(0);

	/* ファイル名変換 */
	ptr=m12hed.mz_filename;

	for (i=0;;)
	{
		if (menu.fontset==1)
		{
			/* 日本語ファイルネーム処理 */
			c=*(ptr++);
			if (c==0x0D) break;
			
			if (c<0x20) continue;
			
			c-=0x20;
			c<<=1;
			
			strtmp2[i]  =mzascii_j[c];
			strtmp2[i+1]=mzascii_j[c+1];
			i+=2;
		}
		else
		{
			/* 英語ファイルネーム処理 */
			c=*(ptr++);
			if (c==0x0D) break;
			
			if (c<0x20) continue;
			
			c-=0x20;
			
			strtmp2[i]  =mzascii_e[c];
			i++;
		}
		
	}
	/* EOS */
	strtmp2[i]=0;

	/* ファイルインフォメーション表示 */
	wsprintf(strtmp,"Filename:%s\n"\
					"Top Addr :$%04X\n"\
					"File Size:$%04X\n"\
					"Exec Addr:$%04X",
					strtmp2,
					m12hed.mz_topaddr,	
					m12hed.mz_filesize,
					m12hed.mz_execaddr);
			 
	MessageBox(hwndApp, strtmp,
			   "File Information", MB_ICONINFORMATION|MB_OK);

	if (rom1_mode == MON_EMUL)
	{
		mem[ROM_START+L_TMPEX] = (m12hed.mz_execaddr & 0xFF);
		mem[ROM_START+L_TMPEX+1] = (m12hed.mz_execaddr >> 8);
	}
	
	return TRUE;
}

/*********************************************/
/* テープイメージとしてmztファイルををセット */
/*********************************************/
BOOL set_tape_file(char *filename)
{
	if (!FileExists(filename))
		return FALSE;

	lstrcpy(tapefile,filename);

	set_tapepos(0);
	UpdateTapeMenu();
	
	return TRUE;
}

/****************************/
/* ファイルダイアログを開く */
/****************************/
BOOL OpenFileImage(void)
{
	OPENFILENAME ofn;
	UINT8 filebuf[MAX_PATH];

	/* ファイルオープンダイアログを開く */
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);								/* 構造体サイズ */
	ofn.hwndOwner = hwndApp;											/* ダイアログのオーナー */
	ofn.Flags = OFN_LONGNAMES|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	ofn.lpstrFilter = MZT_filter;
#if JAPANESE
	ofn.lpstrTitle = "ファイルのロード - Select File Image";
#else
	ofn.lpstrTitle = "Load Program File - Select File Image";
#endif	
	ofn.lpstrFile = filebuf;											/* ファイル名バッファポインタ */
	ofn.nMaxFile = sizeof(filebuf);										/* ファイル名バッファサイズ */

	ZeroMemory(&filebuf,sizeof(filebuf));								/* ファイル名入力バッファ初期化 */
	ofn.lpstrInitialDir = LoadOpenDir;									/* カレントディレクトリ設定 */
	do
	{
		if (!GetOpenFileName(&ofn)) return FALSE;						/* キャンセル */
	} while (!load_mzt_file(ofn.lpstrFile));							/* イメージ読み込み */

	GetCurrentDirectory(sizeof(LoadOpenDir), LoadOpenDir);				/* カレントディレクトリ取得 */
	mz_reset();
	
	return TRUE;
}

/************************/
/* テープイメージを設定 */
/************************/
BOOL SetLoadFileImage(void)
{
	OPENFILENAME ofn;
	UINT8 filebuf[MAX_PATH];

	/* ファイルオープンダイアログを開く */
	ZeroMemory(&ofn,sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);								/* 構造体サイズ */
	ofn.hwndOwner = hwndApp;											/* ダイアログのオーナー */
	ofn.Flags = OFN_LONGNAMES|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	ofn.lpstrFilter = MZT_filter;
#if JAPANESE
	ofn.lpstrTitle = "テープの入れ替え - Select File Image";
#else
	ofn.lpstrTitle = "Set Tape file - Select File Image";
#endif	
	ofn.lpstrFile = filebuf;											/* ファイル名バッファポインタ */
	ofn.nMaxFile = sizeof(filebuf);										/* ファイル名バッファサイズ */

	ZeroMemory(&filebuf,sizeof(filebuf));								/* ファイル名入力バッファ初期化 */
	ofn.lpstrInitialDir = LoadOpenDir;									/* カレントディレクトリ設定 */

	do
	{
		if (!GetOpenFileName(&ofn)) return FALSE;						/* キャンセル */
	} while (!set_tape_file(ofn.lpstrFile));								/* イメージ読み込み */
	InvalidateRect(ofn.hwndOwner,NULL,TRUE);

	GetCurrentDirectory(sizeof(LoadOpenDir), LoadOpenDir);				/* カレントディレクトリ取得 */
	return TRUE;
}

/********************************/
/* セーブ用テープイメージを設定 */
/*******************************/
BOOL SetSaveFileImage(void)
{
	BOOL result;

	/* ファイルオープンダイアログを開く */
#if JAPANESE
	result = setfile_forsave("テープ用セーブイメージの設定 - Select File Image" , 
								SaveTapeFile, SaveOpenDir,
								APD_msg, MZT_filter, MZT_ext);
#else
	result = setfile_forsave("Set Tape file (Save) - Select File Image", 
								SaveTapeFile, SaveOpenDir,
								APD_msg, MZT_filter, MZT_ext);
#endif
	UpdateTapeMenu();

	return result;
}

/**********************/
/* ＱＤイメージを設定 */
/**********************/
BOOL SetQDFileImage(void)
{
	BOOL result;

	/* ファイルオープンダイアログを開く */
#if JAPANESE
	result = setfile_foropen("ＱＤディスクイメージの設定 - Select File Image" ,
							qdfile, QDOpenDir,
							MZT_filter, MZT_ext);
#else
	result = setfile_foropen("Set QD file - Select File Image",
							qdfile, QDOpenDir,
							MZT_filter, MZT_ext);
#endif
	UpdateQDMenu();

	return result;
}

/**********************/
/* ＱＤイメージを解除 */
/**********************/
void EjectQDFileImage(void)
{
	// string qdfile = NULL;
	qdfile[0] = 0;
	UpdateQDMenu();
}

//---------------------------------
// ＲＡＭファイルイメージを設定
//---------------------------------
BOOL LoadRamFileImage(void)
{
	BOOL result;
	UINT8 strtmp[512];

	// ファイルオープンダイアログを開く
	result = setfile_foropen("Load RAM file - Select File Image",
							ramfile, RAMOpenDir,
							MZR_filter, MZR_ext);
	if (result)
	{
		// load ramfile
		result = load_ramfile(ramfile);
		if (result)
		{
			// Save Ok
			wsprintf(strtmp,"RAM File %s\rLoaded.", ramfile);
			MessageBox(hwndApp, strtmp,
			   "MZ700WIN", MB_ICONINFORMATION|MB_OK);
		}
		else
		{
			// Save Error
			wsprintf(strtmp,"Can't load RAM File\r%s.", ramfile);
			MessageBox(hwndApp, strtmp,
					   "Error", MB_ICONEXCLAMATION|MB_OK);
		}
	}

	return result;
}

//---------------------------------
// ＲＡＭファイルイメージをセーブ
//---------------------------------
BOOL SaveRamFileImage(void)
{
	BOOL result;
	UINT8 strtmp[512];

	result = setfile_forsave("Save RAM file - Select File Image", 
								ramfile, RAMOpenDir,
								OVW_msg, MZR_filter, MZR_ext);
	if (result)
	{
		// save state....
		result = save_ramfile(ramfile);
		if (result)
		{
			// Save Ok
			wsprintf(strtmp,"RAM File %s\rSaved.", ramfile);
			MessageBox(hwndApp, strtmp,
			   "MZ700WIN", MB_ICONINFORMATION|MB_OK);
		}
		else
		{
			// Save Error
			wsprintf(strtmp,"Can't save RAM File\r%s.", ramfile);
			MessageBox(hwndApp, strtmp,
					   "Error", MB_ICONEXCLAMATION|MB_OK);
		}
	}

	return result;
}


/*******************/
/* Redraw TapeMenu */
/*******************/
static void set_tapemenu(UINT id, LPSTR itemstr, LPSTR ifname)
{
    HMENU hmenu;
	UINT8 strtmp[256];
	UINT8 strtmp2[256];
    char drive[ _MAX_DRIVE ];
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];
	BOOL f;

    hmenu = GetSubMenu(hmenuApp , 0);									/* Keyboard Menu */
	f = FileExists(ifname);

	EnableMenuItem(hmenu, id, (!f) ? (MF_BYCOMMAND | MF_GRAYED) : MF_BYCOMMAND);
	lstrcpy(strtmp, itemstr);
	if (ifname[0])
	{
		_splitpath_s(ifname, drive, sizeof(drive), dir, sizeof(dir),
		  fname, sizeof(fname), ext, sizeof(ext) );

		wsprintf(strtmp2, " - \x22%s\x22",fname );
		lstrcat(strtmp, strtmp2);
	}

	ModifyMenu(hmenu, id, MF_BYCOMMAND, id, strtmp);
}

////////////////////////////////////////////
// テープイメージ状態によってメニューを更新
////////////////////////////////////////////
void UpdateTapeMenu(void)
{
	set_tapemenu(MENU_SET, "Set &Load Tape", tapefile);
	set_tapemenu(MENU_TAPESAVE, "Set &Save Tape", SaveTapeFile);
}

////////////////////////////////////////////
// ＱＤイメージ状態によってメニューを更新
////////////////////////////////////////////
void UpdateQDMenu(void)
{
    HMENU hmenu;
	UINT8 strtmp[256];
	UINT8 strtmp2[256];
    char drive[ _MAX_DRIVE ];
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];
	BOOL f;

    hmenu = GetSubMenu(hmenuApp , 0);									/* Keyboard Menu */
	f = FileExists(qdfile);

	EnableMenuItem(hmenu, MENU_EJECTQD, (!f) ? (MF_BYCOMMAND | MF_GRAYED) : MF_BYCOMMAND);
	lstrcpy(strtmp, "Set &QD");
	if (qdfile[0])
	{
		_splitpath_s(qdfile , drive, sizeof(drive), dir, sizeof(dir), fname, sizeof(fname), ext, sizeof(ext) );

		wsprintf(strtmp2, " - \x22%s\x22",fname );
		lstrcat(strtmp, strtmp2);
	}

	ModifyMenu(hmenu, MENU_SETQD, MF_BYCOMMAND, MENU_SETQD, strtmp);
}

/********************/
/* 状態セーブの処理 */
/********************/
BOOL SaveStateFile(void)
{
	BOOL result;
	UINT8 strtmp[512];

	/* ファイルオープンダイアログを開く */
#if JAPANESE
	result = setfile_forsave("状態セーブ - Select State File" , 
								statefile, StateOpenDir,
								OVW_msg, MZS_filter, MZS_ext);
#else
	result = setfile_forsave("Save State - Select State File", 
								statefile, StateOpenDir,
								OVW_msg, MZS_filter, MZS_ext);
#endif
	if (result)
	{
		// save state....
		result = save_state(statefile);
		if (result)
		{
			// Save Ok
			wsprintf(strtmp,"State File %s\rSaved.", statefile);
			MessageBox(hwndApp, strtmp,
			   "MZ700WIN", MB_ICONINFORMATION|MB_OK);
		}
		else
		{
			// Save Error
			wsprintf(strtmp,"Can't save State File\r%s.", statefile);
			MessageBox(hwndApp, strtmp,
					   "Error", MB_ICONEXCLAMATION|MB_OK);
		}
	}

	return result;
}

/********************/
/* 状態ロードの処理 */
/********************/
BOOL LoadStateFile(void)
{
	BOOL result;
	UINT8 strtmp[512];

	/* ファイルオープンダイアログを開く */
#if JAPANESE
	result = setfile_foropen("状態ロード - Select State File", 
							statefile, StateOpenDir,
							MZS_filter, MZS_ext);
#else
	result = setfile_foropen("Load State - Select State File", 
							statefile, StateOpenDir,
							MZS_filter, MZS_ext);
#endif
	if (result)
	{
		// load state....
		result = load_state(statefile);
		if (result)
		{
			// Load Ok
			wsprintf(strtmp,"State File %s\rLoaded.", statefile);
			MessageBox(hwndApp, strtmp,
			   "MZ700WIN", MB_ICONINFORMATION|MB_OK);

			update_membank();
			mz_psg_init();
			play8253();
			playPSG();
			mz_refresh_screen(REFSC_ALL);
		}
		else
		{
			// Load Error
			MessageBox(hwndApp, "Bad State File." ,
					   "Error", MB_ICONEXCLAMATION|MB_OK);
			mz_reset();

		}
	}

	return result;
}
