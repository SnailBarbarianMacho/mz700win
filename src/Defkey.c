//----------------------------------------------------------------------------
// File:defkey.c
//
// mz700win:MZ-Keymap Redefine Module
// ($Id: Defkey.c 1 2007-07-27 06:17:19Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

//#include <windows.h>
#include "dprintf.h"

#include "mz700win.h"

#include "z80.h"
#include "win.h"
#include "resource.h"
#include "fileio.h"
#include "defkey.h"

static UINT8 * keymattbl_ptr = NULL;
static int line;

static BYTE *textptr;
static int keymat_max;													// MAX of key-matrix menu
static HMENU hmenu;														// HANDLE of Keyboard Menu

static UINT8 KeyFileStr[MAX_PATH];										// キー定義ファイル名
static TDEFKEY_SECTION defkey_section[KEY_MATRIX_BANK_MAX];

//
UINT8 * get_keymattbl_ptr(void)
{
	return keymattbl_ptr;
}

//
int get_keymat_max(void)
{
	return keymat_max;
}

//-----------
// Line Skip
//-----------
int line_skip(void)
{
	BYTE ch;

	while (1)
	{
		ch = *textptr;
		if (ch == 0 || ch == 0x1A)										/* chk eof */
		{
			return -1;
		}
		
		if ( ch == 0x0D )
		{
			textptr++;
			line++;
			if ( *textptr == 0x0A) textptr++;
			break;
		}
		
		if ( ch == 0x0A )
		{
			line++;
			textptr++;
			break;
		}

		textptr++;
	}

	ch = *textptr;
	return (int)ch;
}

//------------
// Space skip
//------------
int space_skip(void)
{
	BYTE ch;

	while (1)
	{
		ch = *textptr;
		if (ch == 0 || ch == 0x1A)										/* chk eof */
		{
			return -1;
		}

		if (ch == ';' || ch == '/')
		{
			if (line_skip()<0) return -1;
			continue;
		}

		if ( ch == 0x0D )
		{
			textptr++;
			line++;
			if ( (*textptr) == 0x0A) textptr++;
			continue;
		}

		if ( ch == 0x0A )
		{
			textptr++;
			line++;
			continue;
		}

		if (ch > 0x20) break;
		
		textptr++;
	}

	return (int)ch;
}

//---------------------
// compare next letter
//---------------------
int chkchr(int ch)
{
	int a;
	
	if ( (a = space_skip()) < 0 )
	{
		return -1;
	}

	if (a == ch)
		return 1;

	return 0;
};

//------------------------------
// １６進数のテキストを読み込む
//------------------------------
int gethex(int *result)
{
	BYTE ch;
	int err = 0;
	
	*result = 0;

	if ( space_skip() < 0 )
	{
		*result = 0;
		return -1;
	}

	ch = *textptr;
	if (( ch>='0' && ch<='9') ||
		( (( ch>='a') && (ch<='f')) || (( ch>='A') && (ch<='F')) )
		)
	{
		for (; ; textptr++)
		{
			ch = *textptr;

			if ( ch>='0' && ch<='9' )
			{
				*result *= 16;
				*result |= (int) (ch - '0');
				continue;
			}
			else
			if ( ch>='a' && ch<='f' )
			{
				*result *= 16;
				*result |= (int) ( ch - ('a'-10) );
				continue;
			}
			else
			if ( ch>='A' && ch<='F' )
			{
				*result *= 16;
				*result |= (int) ( ch - ('A'-10) );
				continue;
			}
			else
				break;
		}
	}
	else
		err = 1;
	

	return err;
}

//--------------------------
// Get current section name
//--------------------------
int set_current_section(void)
{
	int ch, len;
	int i,ok;
	TDEFKEY_SECTION *defs_p;
	BYTE strtmp[SECTION_NAME_MAX];

	// セクション名をゲット
	textptr++;
	for (len=0 ;; len++)
	{
		ch = *textptr;
		if (ch==0x00 || ch==0x1A) return -1;
		if (ch==0x0D)
		{
			if (*(textptr+1)==0x0A)
			{
				// 途中で改行
				return -1;
			}
		}

		if (ch==0x0A)
		{
			// 途中で改行
			return -1;
		}

		if ( ch==']' || len>=(SECTION_NAME_MAX-2) )
		{
			break;
		}
		
			
		strtmp[len] = ch;
		textptr++;
	}

	strtmp[len] = 0;													/* eof */
#if _DEBUG
//	dprintf("Section=%s\n",strtmp);
#endif

	// Search
	ok = -1;
	for (i=0;i<KEY_MATRIX_BANK_MAX;i++)
	{
		defs_p = &defkey_section[i];
		if ( !strcmp(defs_p->name, strtmp) )
		{
			
			ok = i;
			break;
		}
	}

	// 
	if ( ok<0 )
	{
		if (keymat_max >= KEY_MATRIX_BANK_MAX )
		{
			ok = -1;
			return ok;
		}
		else
		// 新既登録
		for (i=0;i<KEY_MATRIX_BANK_MAX;i++)
		{
			defs_p = &defkey_section[i];
			if ( !defs_p->flag )
			{
				break;
			}
		}
		
		lstrcpy(defs_p->name, strtmp);
		defs_p->flag=1;
		ok = i;
		
		// メニューに追加
		InsertMenu(hmenu, keymat_max , MF_BYPOSITION | MF_STRING,
                   MENU_KEYTYPE_BASE + keymat_max, strtmp );

		keymat_max++;													/* MAX of key-matrix menu */
	}
	
	return ok;
}
	
//---------------------------------------------
// 読み込んだdef.keyを解析し、ワークに取り込む
// Read 'def.key' and define keymap
//---------------------------------------------
int set_defkey( void )
{
	UINT8 *kptr;
	int a,i,j,ch;
	int current_section = 0;
	
	line = 1;

	kptr = keymattbl_ptr;
	for (;;)
	{
		if ( (ch = space_skip()) <= 0) break;

		//セクション定義
		if (ch == '[')
		{
			current_section = set_current_section();
			if (current_section >= 0)
			{
				kptr = keymattbl_ptr + (current_section << 8);

				line_skip();
				continue;
			}
			else
			{
				// Too meny sections
				MessageBox(hwndApp, "Too many section." ,
						   "Error", MB_ICONEXCLAMATION|MB_OK);

				return -2;
			}
		}

		// キーマトリクス読み込み
		// i = MZ-Key matrix
		if (gethex(&i)<0)
		{
			//Error;
			return -1;
		}

		// check '='
		a = chkchr('=');
		textptr++;
		if (a<=0)
		{
			//Error;
			return -1;
		}
			
		// check [']
		a = chkchr(0x27);
		if (a == -1)													/* eof */
		{
			return -1;
		}
		else
		if (a == 0)														/* not equal */
		{
			// hex code
			if (gethex(&j)<0) return -1;
		}
		else															/* equal */
		{
			// letter
			textptr++;
			j = *(textptr++);
			if (j < 0x20) return -1;
			if (*textptr != 0x27) return -1;							/* not closed */

			j = (int) VkKeyScan( (TCHAR)j );
		}

#if _DEBUG
//		dprintf("%5d: Section %2d / %02X = %02X\n", line, current_section, i, j);
#endif
		// define
		if (current_section >= 0)
			kptr[j] = (BYTE) i;

		//
		ch = line_skip();
		if ( ch <= 0 ) break;
	}

	// セパレータを削除
	RemoveMenu( hmenu, current_section+1 , MF_BYPOSITION);

	return 0;
}

/////////////////////////////////////
// キーマトリクス再定義処理　初期化
/////////////////////////////////////
int init_defkey(void)
{
	keymat_max = 0;														/* MAX of key-matrix menu */
	
	keymattbl_ptr = MEM_alloc(KEY_MATRIX_BANK_MAX * 256);
	if (keymattbl_ptr == NULL)
	{
		return -1;														/* Out of Memory */
	}

	FillMemory(keymattbl_ptr, (KEY_MATRIX_BANK_MAX * 256), 0xFF );

	// キー定義ファイル名の作成
	GetCurrentDirectory(sizeof(KeyFileStr), KeyFileStr);
	lstrcat(KeyFileStr, "\\key.def");

	// キー定義セクションステータスの作成
	ZeroMemory(defkey_section, sizeof(defkey_section) );

	return 0;
}

//////////////////////////////////
// キーマトリクス再定義処理　終了
//////////////////////////////////
int end_defkey(void)
{
	// Free key-matrix convert table
	if (keymattbl_ptr)
		MEM_free(keymattbl_ptr);

	
	return 0;
}

///////////////////////////////
// Read key-matrix define file
///////////////////////////////
int read_defkey(void)
{
	BYTE *tptr;
	FILE_HDL fp;
	int length, err;
	UINT8 strtmp[512];

	hmenu = GetSubMenu(hmenuApp , 2);								/* Keyboard Menu */
	
	// 定義ファイルのサイズを調べる
	fp = FILE_ROPEN( KeyFileStr );
	if (fp == FILE_VAL_ERROR)
	{
#if _DEBUG
//		dprintf("Can't open %s.\n", KeyFileStr );
#endif
		// error message
		wsprintf(strtmp, "Couldn't load Key definition file '%s'.", KeyFileStr);
    	MessageBox(hwndApp, strtmp,
				   "Error", MB_ICONEXCLAMATION|MB_OK);

		return -1;														/* File Not Found */
	}

	length = FILE_SEEK( fp, 0, FILE_SEEK_END );
//	length = ftell( fp );
#if _DEBUG
//		dprintf("filelength = %d\n", length );
#endif		

	FILE_SEEK( fp, 0, FILE_SEEK_SET );

	tptr = MEM_alloc( length + 16 );
	textptr = tptr;
	if (textptr)
	{
		FILE_READ( fp, textptr , length);								/* read key.def */
		textptr[length] = 0x1A;
	}

	FILE_CLOSE(fp);

	if ( textptr == NULL ) return -1;

	err = set_defkey();													/* 読み込んだkey.defを解析 */

	if (err == -1)
	{
		// error message
		wsprintf(strtmp, "%s (%d)", KeyFileStr,line);
    	MessageBox(hwndApp, strtmp,
				   "Syntax Error", MB_ICONEXCLAMATION|MB_OK);
#if _DEBUG
//		dprintf("Syntax error in %d\n",line);
#endif		
	}

	MEM_free( tptr );
	
	return err;
};

