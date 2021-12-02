//----------------------------------------------------------------------------
// File:dssub.cpp
//
// mz700win:DirectSound Module
// ($Id: Dssub.cpp 13 2009-02-04 18:45:32Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#include <stdio.h>
#include <windows.h>
#include <stdlib.h>

#define _DSSUB_ 1

#include "mz700win.h"
#include "dssub.h"
#include "win.h"

/* DirectSound構造体 */
static BOOL			DSAvailable = FALSE;								/* DirectSoundが使用可能？ */

static LPDIRECTSOUND       lpDS = NULL;									/* DirectSoundオブジェクト */
static LPDIRECTSOUNDBUFFER lpPRIMARYBUFFER = NULL;						/* プライマリバッファ */
static LPDIRECTSOUNDBUFFER lpWAVEBUFFER[WAVE_MAX];						/* サウンドバッファメモリのソース */

static int noiseptr = 0;

static const BYTE tone_samples[] =
{
	0xA0,0x9C,0x92,0x8E,0x8D,0x8B,0x89,0x8A,0x8A,0x88,0x88,0x89,0x65,0x56,0x5D,0x63,
	0x64,0x66,0x67,0x66,0x65,0x67,0x67,0x66,0x68
};

// noise table
static UINT8 * noisetable;

// runtime noise
static UINT8 * snoise_runtime;

// make noise table
void InitNoiseTable(void)
{
	int i;

	for (i=0; i<noisetablesize; i++)
		noisetable[i] = (rand() & 0x7F) + 0x40;
}

// make noise data
void MakeNoise(BYTE *work, int *bytes, int prm, int ch3freq)
{
	int sync;
	int frqmd;
	int a,dat;
	int ofs;
	DWORD noisesync;
	float frq,frqtmp;

	static const int frqtbl[]={ 6990, 3490, 1750 };

	sync = (prm >> 2)&1;
	frqmd = prm & 3;
	if (frqmd < 3)
		frqtmp = (float) frqtbl[frqmd];
	else
	{
		a = (ch3freq & 0x3FF) * 32;
		if (!a) a++;
		dat = CPU_SPEED_BASE / a;
		frqtmp = (float) dat;
	}

	frq = 22050.0f / frqtmp / 2.0f;
	frqtmp = frq;

	noisesync = 0x00030003;

	for (ofs=0; ofs<noisebufsize; )
	{
		if ( (noisesync & 0x00000001) || (!sync) )
		{
			dat = noisetable[noiseptr++];
			noiseptr &= (noisetablesize-1);
		}
		while (frqtmp>0.0f)
		{
			work[ofs++] = dat;
			frqtmp -= 1.0f;
			if (ofs >= noisebufsize) break;
		}
		frqtmp += frq;

#ifndef	__BORLANDC__
		_asm
		{
			mov		eax,[noisesync]
			ror		eax,1
			mov		[noisesync],eax
		}
#else
		if (noisesync & 1)
		{
			noisesync >>= 1;
			noisesync |= 0x80000000;
		}
		else
		{
			noisesync >>= 1;
		}
#endif	// __BORLANDC__
	}

	*bytes = ofs;
}

BOOL SetRuntimeNoise(int no, int prm, int ch3freq)
{
    DSBUFFERDESC dsbd;
    LPVOID lpbuf1, lpbuf2;
    DWORD dwbuf1, dwbuf2;
	WAVEFORMATEX wfmt;
	int bytes;
	int _no;

	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;

	_no = no+1;
	/* WAVE FORMATを設定 */
	wfmt.wFormatTag=WAVE_FORMAT_PCM;									// 0001?
	wfmt.nChannels=0x0001;
	wfmt.nSamplesPerSec=22050;
	wfmt.nAvgBytesPerSec=22050;
	
	wfmt.nBlockAlign=0x0001;
	wfmt.wBitsPerSample=8;												// 8Bits Sample
	wfmt.cbSize=0;

	MakeNoise(snoise_runtime, &bytes, prm, ch3freq);

    /* noise */
	ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_STATIC|DSBCAPS_CTRLDEFAULT; 
	dsbd.dwBufferBytes = bytes;
	dsbd.dwReserved = 0;
	dsbd.lpwfxFormat = &wfmt; 

	lpWAVEBUFFER[_no]->Stop();
	lpWAVEBUFFER[_no]->Release();
	lpWAVEBUFFER[_no] = NULL;

	if(lpDS->CreateSoundBuffer(&dsbd, &lpWAVEBUFFER[_no],
									NULL) != DS_OK) return FALSE;
	/* サウンドバッファメモリのロック */
	lpWAVEBUFFER[_no]->Lock(0, bytes,
						&lpbuf1, &dwbuf1, &lpbuf2, &dwbuf2, 0); 
	/* 音源データの設定 */
	CopyMemory(lpbuf1, (BYTE*)snoise_runtime, dwbuf1);
	if(dwbuf2 != 0) CopyMemory(lpbuf2,
							(BYTE*)snoise_runtime+dwbuf1, dwbuf2);
	/* サウンドバッファのロック解除 */
	lpWAVEBUFFER[_no]->Unlock(lpbuf1, dwbuf1, lpbuf2, dwbuf2); 

    return TRUE;
}

/* DirectSoundの初期化 */
BOOL InitDirectSound(HWND hwnd)
{
    int i;
    DSBUFFERDESC dsbd;

	/* サウンド使用禁止チェック */
	if (sound_di) {
		DSAvailable = FALSE;
		return FALSE;
	}

    /* DirectDrawの初期化 */
    if (DirectSoundCreate(NULL, &lpDS, NULL) != DS_OK)	{
		DSAvailable = FALSE;
		return FALSE;
	}

	noisetable = (UINT8 *) MEM_alloc(noisetablesize);
	if (noisetable == NULL)
		return FALSE;
	else
	{
		snoise_runtime = (UINT8 *) MEM_alloc(noisebufsize);
		if (snoise_runtime == NULL)
		{
			MEM_free(noisetable);
			noisetable = NULL;
			return FALSE;
		}
	}

	/* ノイズテーブルの作成 */
	InitNoiseTable();
    lpDS->SetCooperativeLevel(hwnd, DSSCL_NORMAL);

    /* プライマリバッファの初期化 */
    ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
    dsbd.dwSize = sizeof(DSBUFFERDESC);
    dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
    lpDS->CreateSoundBuffer(&dsbd, &lpPRIMARYBUFFER, NULL);

	/* バッファメモリの初期化 */
    for(i = 0; i < WAVE_MAX; i++) lpWAVEBUFFER[i] = NULL;

	DSAvailable = TRUE;
    return TRUE;
}

/* DirectSoundの禁止 */
void DisableDirectSound(void)
{
	DSAvailable = FALSE;
}

/* DirectSoundの終了 */
void EndDirectSound(void)
{
    int i;

	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return;

	/* バッファメモリの開放 */
    for (i = 0; i < WAVE_MAX; i++)
	{
        if (lpWAVEBUFFER[i] != NULL)
		{
			lpWAVEBUFFER[i]->Stop();
			lpWAVEBUFFER[i]->Release();
		}
    }

	/* プライマリバッファの開放 */
    if(lpPRIMARYBUFFER != NULL) lpPRIMARYBUFFER->Release();

	/* ダイレクトサウンドの開放 */
    if(lpDS != NULL) lpDS->Release();

	/* ノイズテーブルの開放 */
	if (noisetable)
		MEM_free(noisetable);

	/* ノイズ　ランタイム作成バッファの開放 */
	if (snoise_runtime)
		MEM_free(snoise_runtime);
}

/* サウンドの設定 */
BOOL Set700Sound(int no)
{
    DSBUFFERDESC dsbd;
    LPVOID lpbuf1, lpbuf2;
    DWORD dwbuf1, dwbuf2;
	WAVEFORMATEX wfmt;

	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;

	/* WAVE FORMATを設定 */
	wfmt.wFormatTag=WAVE_FORMAT_PCM;									// 0001?
	wfmt.nChannels=0x0001;
	wfmt.nSamplesPerSec=22050;
	wfmt.nAvgBytesPerSec=22050;
	
	wfmt.nBlockAlign=0x0001;
	wfmt.wBitsPerSample=8;												// 8Bits Sample
	wfmt.cbSize=0;

    /* サウンドバッファメモリの初期化 */
	/* tone */
	ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_STATIC|DSBCAPS_CTRLDEFAULT; 
	dsbd.dwBufferBytes = sizeof(tone_samples);
	dsbd.dwReserved = 0;
//    dsbd.lpwfxFormat = (LPWAVEFORMATEX)(lpdword+5); 
	dsbd.lpwfxFormat = &wfmt; 
	if(lpDS->CreateSoundBuffer(&dsbd, &lpWAVEBUFFER[no],
									NULL) != DS_OK) return FALSE;
	/* サウンドバッファメモリのロック */
	lpWAVEBUFFER[no]->Lock(0, sizeof(tone_samples),
						&lpbuf1, &dwbuf1, &lpbuf2, &dwbuf2, 0); 
	/* 音源データの設定 */
	CopyMemory(lpbuf1, (BYTE*)tone_samples, dwbuf1);
	if(dwbuf2 != 0) CopyMemory(lpbuf2,
							(BYTE*)tone_samples+dwbuf1, dwbuf2);
	/* サウンドバッファのロック解除 */
	lpWAVEBUFFER[no]->Unlock(lpbuf1, dwbuf1, lpbuf2, dwbuf2); 

    return TRUE;
}

/* 登録サウンドの直接再生（重ねあわせ無し） */
/* loop=0 or DSBPLAY_LOOPING */
BOOL StartWaveBuffer(int no, DWORD freq , LONG pan ,LONG vol,DWORD loop)
{
	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;
  
	lpWAVEBUFFER[no]->SetFrequency(freq);
	lpWAVEBUFFER[no]->SetPan(pan);
	lpWAVEBUFFER[no]->SetVolume(vol);
	lpWAVEBUFFER[no]->Play(0 , 0 , loop);

  return TRUE;
}

/* 登録サウンドの直接再生（重ねあわせ無し） */
/* loop=0 or DSBPLAY_LOOPING */
BOOL PlayWaveBuffer(int no, DWORD loop)
{
	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;

	lpWAVEBUFFER[no]->Play(0 , 0 , loop);

	return TRUE;
}

/* 登録サウンドの停止 */
BOOL StopWaveBuffer(int no)
{
	HRESULT dsres;
	DWORD   stat;
	
	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;

	dsres = lpWAVEBUFFER[no]->GetStatus(&stat);
	if (stat & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING) ) lpWAVEBUFFER[no]->Stop();
  
	return TRUE;
}
/* 登録サウンドのステータスをゲット */
BOOL GetWaveBufferStat(int no)
{
	HRESULT dsres;
	DWORD   stat;
	
	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;

	dsres = lpWAVEBUFFER[no]->GetStatus(&stat);
	if (stat & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING) ) return TRUE;
  
	return FALSE;
}

BOOL SetWaveFreq(int no, DWORD freq)
{
	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;

	lpWAVEBUFFER[no]->SetFrequency(freq);

    return TRUE;
}

BOOL SetWaveVol(int no, DWORD vol)
{
	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return FALSE;

	lpWAVEBUFFER[no]->SetVolume(vol);

    return TRUE;
}

LONG GetWaveVol(int no)
{
	LONG result;

	/* DirectSoundが使用不可ならReturn */
	if (!DSAvailable) return DSBVOLUME_MIN;

	lpWAVEBUFFER[no]->GetVolume(&result);

    return result;
}
