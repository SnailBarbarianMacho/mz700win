#pragma once

#define		DIRECTDRAW_VERSION 0x0200   // DirectX2相当
#define		USE_DDRAW 0					// 1であればdirectdrawが有効

// maru:VS2010ではddraw.libが無くなっているため、全削除
// ※DirectX SDK Feb.2010を別途インストールすることでリンク可能になった。が、動作しない…。
//      ぶっちゃけフルスクリーンにするためにしか使っていない。
#if USE_DDRAW
#include <ddraw.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif 

#define SyncTime		17												/* 1/60 sec. */

enum
{
	SCRN_WINDOW = 0,													/* 00:Window Mode */
	SCRN_FULL															/* 01:Full-Screen Mode */
};

enum
{
	FULL_320_200 = 0,													/* 00:320x200 */
	FULL_320_240,														/* 01:320x240 */
	FULL_640_400,														/* 02:640x400 */
	FULL_640_480,														/* 03:640x480 */
	FULL_800_600,														/* 04:800x600 */

	FULL_END
};

typedef struct
{
	int xscr,yscr;														/* 全画面サイズ */
	int xpos,ypos;														/* 全画面表示位置 */
	int xsiz,ysiz;														/* BLT Size */
	
} TSCRSIZE;

BOOL CreateDirectDraw(HWND hwnd);
BOOL InitDirectDrawFull(HWND hwnd, int xsize, int ysize);
BOOL MakeScreenSurface(void);
void ReleaseDirectDraw(void);
void EndDirectDraw(void);
void ClearScreen(void);
int ChangeToFullScreen(void);

void redraw_win_frame(HWND hwnd);
void flip2gdi(void);
	
void update_winscr(void);

#ifdef __cplusplus
}
#endif 

