// fileset.h
// $Id: fileset.h 20 2009-05-25 18:20:32Z maru $

void UpdateTapeMenu(void);
void UpdateQDMenu(void);

void mz_load_cmos(void);
void mz_save_cmos(void);

BOOL load_mzt_file(char *);
BOOL set_tape_file(char *);

BOOL OpenFileImage(void);
BOOL SetLoadFileImage(void);
BOOL SetSaveFileImage(void);

BOOL SetQDFileImage(void);
void EjectQDFileImage(void);

BOOL LoadRamFileImage(void);
BOOL SaveRamFileImage(void);

BOOL SaveStateFile(void);
BOOL LoadStateFile(void);

#ifdef _FILESET_H_

UINT8	LoadOpenDir[IniFileStrBuf];             /* �Ō�Ƀ��[�h�����t�H���_�������܂��o�b�t�@ */
UINT8	SaveOpenDir[IniFileStrBuf];             /* �Ō�ɃZ�[�u�����t�H���_�������܂��o�b�t�@ */
UINT8	QDOpenDir[IniFileStrBuf];               /* �p�c�C���[�W�t�@�C���̃t�H���_�������܂��o�b�t�@ */
UINT8	RAMOpenDir[IniFileStrBuf];              /* �q�`�l�t�@�C���C���[�W�̃t�H���_�������܂��o�b�t�@ */
UINT8	SaveTapeFile[MAX_PATH];                 /* �Z�[�u�p�e�[�v�t�@�C���� */
UINT8	StateOpenDir[IniFileStrBuf];            /* �Ō�ɋL�^����state file�t�H���_�������܂��o�b�t�@ */
UINT8	CmosFileStr[MAX_PATH];                  /* CMOS.RAM�̃t���p�X�����܂��o�b�t�@ */

UINT8	statefile[MAX_PATH];					/* state �t�@�C���� */

UINT8	qdfile[MAX_PATH];						/* �p�c�t�@�C���� */
UINT8	tapefile[MAX_PATH];						/* �e�[�v�t�@�C���� */
UINT8	ramfile[MAX_PATH];						/* �q�`�l�t�@�C���� */

#else

extern UINT8	LoadOpenDir[IniFileStrBuf];
extern UINT8	SaveOpenDir[IniFileStrBuf];
extern UINT8	QDOpenDir[IniFileStrBuf];
extern UINT8	RAMOpenDir[IniFileStrBuf];
extern UINT8	SaveTapeFile[MAX_PATH];
extern UINT8	StateOpenDir[IniFileStrBuf];
extern UINT8	CmosFileStr[MAX_PATH];

extern UINT8	statefile[MAX_PATH];			// state �t�@�C����
extern UINT8	qdfile[MAX_PATH];				// �p�c�t�@�C����
extern UINT8	tapefile[MAX_PATH];				// �e�[�v�t�@�C����
extern UINT8	ramfile[MAX_PATH];				// �q�`�l�t�@�C����

#endif
