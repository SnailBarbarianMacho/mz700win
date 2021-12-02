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

/* DirectSound�\���� */
static BOOL			DSAvailable = FALSE;								/* DirectSound���g�p�\�H */

static LPDIRECTSOUND       lpDS = NULL;									/* DirectSound�I�u�W�F�N�g */
static LPDIRECTSOUNDBUFFER lpPRIMARYBUFFER = NULL;						/* �v���C�}���o�b�t�@ */
static LPDIRECTSOUNDBUFFER lpWAVEBUFFER[WAVE_MAX];						/* �T�E���h�o�b�t�@�������̃\�[�X */

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

	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;

	_no = no+1;
	/* WAVE FORMAT��ݒ� */
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
	/* �T�E���h�o�b�t�@�������̃��b�N */
	lpWAVEBUFFER[_no]->Lock(0, bytes,
						&lpbuf1, &dwbuf1, &lpbuf2, &dwbuf2, 0); 
	/* �����f�[�^�̐ݒ� */
	CopyMemory(lpbuf1, (BYTE*)snoise_runtime, dwbuf1);
	if(dwbuf2 != 0) CopyMemory(lpbuf2,
							(BYTE*)snoise_runtime+dwbuf1, dwbuf2);
	/* �T�E���h�o�b�t�@�̃��b�N���� */
	lpWAVEBUFFER[_no]->Unlock(lpbuf1, dwbuf1, lpbuf2, dwbuf2); 

    return TRUE;
}

/* DirectSound�̏����� */
BOOL InitDirectSound(HWND hwnd)
{
    int i;
    DSBUFFERDESC dsbd;

	/* �T�E���h�g�p�֎~�`�F�b�N */
	if (sound_di) {
		DSAvailable = FALSE;
		return FALSE;
	}

    /* DirectDraw�̏����� */
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

	/* �m�C�Y�e�[�u���̍쐬 */
	InitNoiseTable();
    lpDS->SetCooperativeLevel(hwnd, DSSCL_NORMAL);

    /* �v���C�}���o�b�t�@�̏����� */
    ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
    dsbd.dwSize = sizeof(DSBUFFERDESC);
    dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
    lpDS->CreateSoundBuffer(&dsbd, &lpPRIMARYBUFFER, NULL);

	/* �o�b�t�@�������̏����� */
    for(i = 0; i < WAVE_MAX; i++) lpWAVEBUFFER[i] = NULL;

	DSAvailable = TRUE;
    return TRUE;
}

/* DirectSound�̋֎~ */
void DisableDirectSound(void)
{
	DSAvailable = FALSE;
}

/* DirectSound�̏I�� */
void EndDirectSound(void)
{
    int i;

	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return;

	/* �o�b�t�@�������̊J�� */
    for (i = 0; i < WAVE_MAX; i++)
	{
        if (lpWAVEBUFFER[i] != NULL)
		{
			lpWAVEBUFFER[i]->Stop();
			lpWAVEBUFFER[i]->Release();
		}
    }

	/* �v���C�}���o�b�t�@�̊J�� */
    if(lpPRIMARYBUFFER != NULL) lpPRIMARYBUFFER->Release();

	/* �_�C���N�g�T�E���h�̊J�� */
    if(lpDS != NULL) lpDS->Release();

	/* �m�C�Y�e�[�u���̊J�� */
	if (noisetable)
		MEM_free(noisetable);

	/* �m�C�Y�@�����^�C���쐬�o�b�t�@�̊J�� */
	if (snoise_runtime)
		MEM_free(snoise_runtime);
}

/* �T�E���h�̐ݒ� */
BOOL Set700Sound(int no)
{
    DSBUFFERDESC dsbd;
    LPVOID lpbuf1, lpbuf2;
    DWORD dwbuf1, dwbuf2;
	WAVEFORMATEX wfmt;

	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;

	/* WAVE FORMAT��ݒ� */
	wfmt.wFormatTag=WAVE_FORMAT_PCM;									// 0001?
	wfmt.nChannels=0x0001;
	wfmt.nSamplesPerSec=22050;
	wfmt.nAvgBytesPerSec=22050;
	
	wfmt.nBlockAlign=0x0001;
	wfmt.wBitsPerSample=8;												// 8Bits Sample
	wfmt.cbSize=0;

    /* �T�E���h�o�b�t�@�������̏����� */
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
	/* �T�E���h�o�b�t�@�������̃��b�N */
	lpWAVEBUFFER[no]->Lock(0, sizeof(tone_samples),
						&lpbuf1, &dwbuf1, &lpbuf2, &dwbuf2, 0); 
	/* �����f�[�^�̐ݒ� */
	CopyMemory(lpbuf1, (BYTE*)tone_samples, dwbuf1);
	if(dwbuf2 != 0) CopyMemory(lpbuf2,
							(BYTE*)tone_samples+dwbuf1, dwbuf2);
	/* �T�E���h�o�b�t�@�̃��b�N���� */
	lpWAVEBUFFER[no]->Unlock(lpbuf1, dwbuf1, lpbuf2, dwbuf2); 

    return TRUE;
}

/* �o�^�T�E���h�̒��ڍĐ��i�d�˂��킹�����j */
/* loop=0 or DSBPLAY_LOOPING */
BOOL StartWaveBuffer(int no, DWORD freq , LONG pan ,LONG vol,DWORD loop)
{
	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;
  
	lpWAVEBUFFER[no]->SetFrequency(freq);
	lpWAVEBUFFER[no]->SetPan(pan);
	lpWAVEBUFFER[no]->SetVolume(vol);
	lpWAVEBUFFER[no]->Play(0 , 0 , loop);

  return TRUE;
}

/* �o�^�T�E���h�̒��ڍĐ��i�d�˂��킹�����j */
/* loop=0 or DSBPLAY_LOOPING */
BOOL PlayWaveBuffer(int no, DWORD loop)
{
	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;

	lpWAVEBUFFER[no]->Play(0 , 0 , loop);

	return TRUE;
}

/* �o�^�T�E���h�̒�~ */
BOOL StopWaveBuffer(int no)
{
	HRESULT dsres;
	DWORD   stat;
	
	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;

	dsres = lpWAVEBUFFER[no]->GetStatus(&stat);
	if (stat & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING) ) lpWAVEBUFFER[no]->Stop();
  
	return TRUE;
}
/* �o�^�T�E���h�̃X�e�[�^�X���Q�b�g */
BOOL GetWaveBufferStat(int no)
{
	HRESULT dsres;
	DWORD   stat;
	
	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;

	dsres = lpWAVEBUFFER[no]->GetStatus(&stat);
	if (stat & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING) ) return TRUE;
  
	return FALSE;
}

BOOL SetWaveFreq(int no, DWORD freq)
{
	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;

	lpWAVEBUFFER[no]->SetFrequency(freq);

    return TRUE;
}

BOOL SetWaveVol(int no, DWORD vol)
{
	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return FALSE;

	lpWAVEBUFFER[no]->SetVolume(vol);

    return TRUE;
}

LONG GetWaveVol(int no)
{
	LONG result;

	/* DirectSound���g�p�s�Ȃ�Return */
	if (!DSAvailable) return DSBVOLUME_MIN;

	lpWAVEBUFFER[no]->GetVolume(&result);

    return result;
}
