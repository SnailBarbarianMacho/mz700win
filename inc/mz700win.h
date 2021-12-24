/* $Id: mz700win.h 20 2009-05-25 18:20:32Z maru $ */

/* Typedef */
typedef		unsigned int UINT32;
typedef		unsigned short UINT16;
typedef		unsigned char UINT8;

// FDC有効？
//#define ENABLE_FDC

/* メッセージで日本語を使うか */
#define JAPANESE		0

/* */
#define ROM_START	0			/* monitor rom */
#define ROM2_START	(4*1024)	/* monitor rom-2 */
#define VID_START	(10*1024)	/* video ram */
#define RAM_START	(14*1024)	/* 64k normal ram */

/* */
#define ROM1_ADDR 0x0000		/* ROM1 mz addr */
#define ROM2_ADDR 0xE800		/* ROM2 mz addr */

#define MZ1R12_SIZE 0x8000		/* MZ-1R12のサイズ(32k) */
#define MZ1R12_MAX	64			/* MZ-1R12の最大装着枚数 */
#define MZ1R18_SIZE 0x10000		/* MZ-1R18のサイズ(64k) */

#define ROMFONT_SIZE 4096		/* ROMFONT SIZE  */
#define PCG700_SIZE 2048		/* PCG-700 SIZE  */
#define PCG1500_SIZE 8192		/* PCG 1BANK SIZE  */

#define CPU_SPEED 3580000		/* 3.580 MHz */
#define CPU_SPEED_BASE 3580000	/* 3.580 MHz */
#define FRAMES_PER_SECOND 60
#define PIO_TIMER_RESO 10

#define VRAM_ACCESS_WAIT 42

#define TEMPO_TSTATES ((CPU_SPEED*10)/381/2)
#define VBLANK_TSTATES 45547	

/* モニタＲＯＭ１のタイプ定数 */
enum TMON1_TYPE
{
	MON_EMUL = 0,														/*  0:EMUL ROM(NO ROM FILE) */
	MON_OTHERS,															/*  1:OTHERS */
	MON_1Z009,															/*  2:1Z-009A/B */
	MON_1Z013,															/*  3:1Z-013A/B */
	MON_NEWMON700,														/*  4:MZ700-NEW MONITOR */
	MON_NEWMON80K,														/*  5:MZ-NEW MONITOR */

	MON_LAST,
};

/* モニタＲＯＭ２のタイプ定数 */
enum TMON2_TYPE
{
	MON_9Z502 = 1,														/*  1:9Z-502M */
};

/* MZTファイルのヘッダ */
typedef struct
{
	BYTE mz_fmode;														/* ＭＺのファイルモード */
	BYTE mz_filename[17];												/* ＭＺのファイル名 */
	WORD mz_filesize;													/* ファイルサイズ */
	WORD mz_topaddr;													/* 先頭アドレス */
	WORD mz_execaddr;													/* 実行アドレス */
	
	BYTE dummy[0x80-0x18];												/* パディング */
} MZTHED;

/* キーテーブル Patch用構造体 */
typedef struct
{
	WORD addr;															/* patch address */
	BYTE data;															/* patch data */
	
} TPATCH;

/* for MONITOR PATCH */	
typedef struct
{
	WORD addr;															/* patch addr */
	unsigned short code;												/* em code */
} TMONEMU;
