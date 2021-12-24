/* $Id: mz700win.h 20 2009-05-25 18:20:32Z maru $ */

/* Typedef */
typedef		unsigned int UINT32;
typedef		unsigned short UINT16;
typedef		unsigned char UINT8;

// FDC�L���H
//#define ENABLE_FDC

/* ���b�Z�[�W�œ��{����g���� */
#define JAPANESE		0

/* */
#define ROM_START	0			/* monitor rom */
#define ROM2_START	(4*1024)	/* monitor rom-2 */
#define VID_START	(10*1024)	/* video ram */
#define RAM_START	(14*1024)	/* 64k normal ram */

/* */
#define ROM1_ADDR 0x0000		/* ROM1 mz addr */
#define ROM2_ADDR 0xE800		/* ROM2 mz addr */

#define MZ1R12_SIZE 0x8000		/* MZ-1R12�̃T�C�Y(32k) */
#define MZ1R12_MAX	64			/* MZ-1R12�̍ő呕������ */
#define MZ1R18_SIZE 0x10000		/* MZ-1R18�̃T�C�Y(64k) */

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

/* ���j�^�q�n�l�P�̃^�C�v�萔 */
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

/* ���j�^�q�n�l�Q�̃^�C�v�萔 */
enum TMON2_TYPE
{
	MON_9Z502 = 1,														/*  1:9Z-502M */
};

/* MZT�t�@�C���̃w�b�_ */
typedef struct
{
	BYTE mz_fmode;														/* �l�y�̃t�@�C�����[�h */
	BYTE mz_filename[17];												/* �l�y�̃t�@�C���� */
	WORD mz_filesize;													/* �t�@�C���T�C�Y */
	WORD mz_topaddr;													/* �擪�A�h���X */
	WORD mz_execaddr;													/* ���s�A�h���X */
	
	BYTE dummy[0x80-0x18];												/* �p�f�B���O */
} MZTHED;

/* �L�[�e�[�u�� Patch�p�\���� */
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
