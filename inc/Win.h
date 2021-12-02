#ifdef __cplusplus
extern "C"
{
#endif 

#define MZ_SP_PATCH 1							// 純正ＲＯＭ時のパッチを有効にするか 1=する
#define MZ_ROM1_TEST 0							// ROM1有効時にentry emulのテストをするか 1=する

//
#define FORMWIDTH		320
#define FORMHEIGHT		200

// ＩＮＩファイル名とセクション
#define		IniFileStrBuf	144					// ＩＮＩファイル名をしまうバッファサイズ
#define		IniFileName	"\\MZ700WIN.INI"
#define		CmosFileName	"\\CMOS.RAM"
#define		IniSection_Option	"OPTIONS"
#define		IniSection_Window	"WINDOW"
#define		IniSection_Folder    "FOLDER"

// Alias
#define		get_timer		timeGetTime
#define		MEM_alloc(sz)	(LPVOID)GlobalAlloc(GPTR , sz)
#define		MEM_free(ptr)	GlobalFree((HGLOBAL)ptr)

// Typedef
typedef     LPBITMAPINFOHEADER PDIB;
typedef     HANDLE             HDIB;

// Structure
typedef struct pal
{
  WORD Version;
  WORD NumberOfEntries;
  PALETTEENTRY aEntries[256];
} pal;

typedef struct header
{
  BITMAPINFOHEADER  Header;
  RGBQUAD           aColors[256];
} header;

extern pal LogicalPalette;

// MENUに出したかったビットマップ関係（未使用
#define MENU_BMP_WIDTH  16
#define MENU_BMP_HEIGHT 15
#define MENU_BMP_MAX (144/MENU_BMP_WIDTH)		// Number of Menu BITMAP ICON

#define MENU_WORK_MAX	64						// ワーク確保数

//
typedef struct
{
	char menustr[64];							// メニュー文字列
	UINT menuID;								// メニューＩＤ
	UINT menuBMP;								// メニューＢＭＰ
} TBMPMENU;

// options setting
typedef struct
{
	int		selrom;								// 選択されたＲＯＭ
	int		machine;							// マシンモード
    int     speed;                              // 実行速度(10-100)
	int		screen;								// 画面倍率
	int		scrn_freq;							// 画面更新カウンタ
	int		keytype;							// キーボードタイプ
	int		fontset;							// フォントタイプ 0:Europe / 1:Japan
	int		pcg700;								// PCG700 0:OFF 1:ON
} TMENUVAL;

// menuセッティングのmachineメンバ用
enum
{
	MACHINE_MZ700,
	MACHINE_MZ1500,
};

extern UINT8		RomFileDir[IniFileStrBuf];	// ROMファイルがあるディレクトリ名

// prototype....
void InitMenuBitmaps(HWND);
void EndMenuBitmaps(HWND);

void MENU_GetTextExtentPoint(HDC hdc,LPCSTR string, int len, SIZE *szr);
BOOL MENU_TextOut(HWND hwnd,HDC hdc, int x, int y,LPCTSTR string, int cbString);

TBMPMENU * get_menu_work(UINT);
void init_menu_work(void);
BOOL add_menu_bmp(HMENU hmenu,UINT id,UINT);

// win.c prototype........
BOOL AppPaint(HWND, HDC);
BOOL SystemTask(void);

void DrawScreen(HDC,int);

void update_winscr(void);

void mz_exit(int);
void free_resource(void);

BOOL isAppActive(void);
int get_scrnmode(void);
int font_load(int);

UINT8 * CreateOffScreen(void);
void mouse_cursor(BOOL);

void create_mmtimer(void);
void free_mmtimer(void);
void get_window_size(int);
void set_select_chk(int,int,int);
void set_screen_menu(int);
void set_keytype_menu(int);
void set_fontset_menu(int);
void set_pcg700_menu(int);
void set_fullscreen_menu(int);

#ifndef _MAIN_

extern const UINT8 szAppName[];

extern HWND      hwndApp;
extern HMENU     hmenuApp;
extern HDC       Buffer;

extern WORD		sound_di;												/* ＳＥ禁止モード */
extern WORD		sound_md;												/* サウンドモード */
//extern WORD		mz1500mode;												/* MZ-1500モード */
extern WORD		key_patch;												/* KeyTableパッチあてフラグ */
extern WORD		bk_color;												/* 簡易MZ-80K/C/1200パッチモード */
extern WORD		use_cmos;												/* MZ-1R12 0:OFF 1:ON */

extern TMENUVAL	menu;

#endif

#ifdef __cplusplus
}
#endif 
