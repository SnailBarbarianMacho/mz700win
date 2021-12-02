//----------------------------------------------------------------------------
// File:fileio.c
//
// mz700win:File Access Module
// ($Id: fileio.c 17 2009-02-07 19:20:32Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#include <windows.h>
#include "fileio.h"

//----------------------------------------------------------
// ファイル読み書き用Open
//----------------------------------------------------------
// Ret: ファイルハンドル
//      FILE_VAL_ERROR = Open Error
FILE_HDL FILE_OPEN(LPCSTR filename) {

	FILE_HDL	ret;

	if ((ret = CreateFile((char *)filename, GENERIC_READ | GENERIC_WRITE,
						0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
											== INVALID_HANDLE_VALUE) {
		if ((ret = CreateFile((char *)filename, GENERIC_READ,
						0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
											== INVALID_HANDLE_VALUE) {
			return ((FILE_HDL)FALSE);
		}
	}
	return(ret);
}

//----------------------------------------------------------
// ファイル読み込み用Open
//----------------------------------------------------------
// Ret: ファイルハンドル
//      FILE_VAL_ERROR = Open Error
FILE_HDL FILE_ROPEN(LPCSTR filename)
{
    FILE_HDL fh;

	fh = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    return fh;
}

//----------------------------------------------------------
// ファイル新規作成用Open
//----------------------------------------------------------
// Ret: ファイルハンドル
//      FILE_VAL_ERROR = Open Error
FILE_HDL FILE_COPEN(LPCSTR filename)
{
    FILE_HDL fh;

	fh = CreateFile(filename, GENERIC_WRITE, 0, 0,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    return fh;
}

//----------------------------------------------------------
// ファイル追加作成用Open
//----------------------------------------------------------
// Ret: ファイルハンドル
//      FILE_VAL_ERROR = Open Error
FILE_HDL FILE_AOPEN(LPCSTR filename)
{
    FILE_HDL fh;

    fh = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, 0,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE)
    {
        fh = CreateFile(filename, GENERIC_WRITE, 0, 0,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
     }

    if (fh != INVALID_HANDLE_VALUE)
    {
        SetFilePointer(fh, 0, NULL, FILE_END);
    }

    return fh;
}

//----------------------------------------------------------
// ファイル読み込み
//----------------------------------------------------------
// Ret: 読み込みバイト数：－１＝エラー
int FILE_READ(FILE_HDL fh, void * ptr, int sz)
{
    BOOL result;
    DWORD rbytes;

	result = ReadFile(fh, ptr, sz, &rbytes, NULL);

    return (result) ? rbytes : -1;
}

//----------------------------------------------------------
// ファイル書き込み
//----------------------------------------------------------
// Ret: 書き込みバイト数：－１＝エラー
int FILE_WRITE(FILE_HDL fh, void * ptr, int sz)
{
    BOOL result;
    DWORD wbytes;

	result = WriteFile(fh, ptr, sz, &wbytes, NULL);

    return (result) ? wbytes : -1;
}

//----------------------------------------------------------
// ファイルクローズ
//----------------------------------------------------------
// Ret: 書き込みバイト数：－１＝エラー
BOOL FILE_CLOSE(FILE_HDL fh)
{
    return CloseHandle(fh);
}


//----------------------------------------------------------
// ファイルシーク
//----------------------------------------------------------
// Ret: ファイルのカレント位置：－１＝エラー
int FILE_SEEK(FILE_HDL fh, long pointer, int mode)
{
	return SetFilePointer(fh, pointer, NULL, mode);
}

//----------------------------------------------------------
// ファイルが存在するかどうかチェックする
//----------------------------------------------------------
BOOL FileExists(LPCSTR filename)
{
	FILE_HDL fp;

	fp = FILE_ROPEN(filename);
	if (fp == FILE_VAL_ERROR) return FALSE;

	FILE_CLOSE(fp);
	return TRUE;
}

//----------------------------------------------------------
// ファイルのアトリビュートをチェックする
//----------------------------------------------------------
short FILE_ATTR(LPCSTR filename) {

	return ((short)GetFileAttributes((char *)filename));
}
