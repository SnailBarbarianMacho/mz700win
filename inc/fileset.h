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

UINT8	LoadOpenDir[IniFileStrBuf];             /* 最後にロードしたフォルダ名をしまうバッファ */
UINT8	SaveOpenDir[IniFileStrBuf];             /* 最後にセーブしたフォルダ名をしまうバッファ */
UINT8	QDOpenDir[IniFileStrBuf];               /* ＱＤイメージファイルのフォルダ名をしまうバッファ */
UINT8	RAMOpenDir[IniFileStrBuf];              /* ＲＡＭファイルイメージのフォルダ名をしまうバッファ */
UINT8	SaveTapeFile[MAX_PATH];                 /* セーブ用テープファイル名 */
UINT8	StateOpenDir[IniFileStrBuf];            /* 最後に記録したstate fileフォルダ名をしまうバッファ */
UINT8	CmosFileStr[MAX_PATH];                  /* CMOS.RAMのフルパスをしまうバッファ */

UINT8	statefile[MAX_PATH];					/* state ファイル名 */

UINT8	qdfile[MAX_PATH];						/* ＱＤファイル名 */
UINT8	tapefile[MAX_PATH];						/* テープファイル名 */
UINT8	ramfile[MAX_PATH];						/* ＲＡＭファイル名 */

#else

extern UINT8	LoadOpenDir[IniFileStrBuf];
extern UINT8	SaveOpenDir[IniFileStrBuf];
extern UINT8	QDOpenDir[IniFileStrBuf];
extern UINT8	RAMOpenDir[IniFileStrBuf];
extern UINT8	SaveTapeFile[MAX_PATH];
extern UINT8	StateOpenDir[IniFileStrBuf];
extern UINT8	CmosFileStr[MAX_PATH];

extern UINT8	statefile[MAX_PATH];			// state ファイル名
extern UINT8	qdfile[MAX_PATH];				// ＱＤファイル名
extern UINT8	tapefile[MAX_PATH];				// テープファイル名
extern UINT8	ramfile[MAX_PATH];				// ＲＡＭファイル名

#endif
