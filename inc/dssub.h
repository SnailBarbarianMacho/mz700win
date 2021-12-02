#define DIRECTSOUND_VERSION  0x0200  // DirectX2ëäìñÅIÅI

#include <dsound.h>

// For DirectX7 SDK
#ifndef DSBCAPS_CTRLDEFAULT
#define DSBCAPS_CTRLDEFAULT        (DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME)
#endif

#ifndef DSBVOLUME_MIN
#define DSBVOLUME_MIN               -10000
#endif
#ifndef DSBVOLUME_MAX
#define DSBVOLUME_MAX               0
#endif

#define WAVE_MAX		9												/* CHANNEL NUMBER of S.E. */
#define noisetablesize 8192												/* Noise Table Size */
#define noisebufsize 4096												/* Noise Buffer Size */

#ifdef __cplusplus
extern "C" {
#endif 

void MakeNoise(BYTE *, int, int, int);
BOOL SetRuntimeNoise(int, int, int);

/* ÉwÉãÉpä÷êî */
BOOL InitDirectSound(HWND hwnd);
void EndDirectSound(void);
void DisableDirectSound(void);
BOOL Set700Sound(int no);
BOOL StartWaveBuffer(int no, DWORD freq , LONG pan ,LONG vol,DWORD loop);
BOOL PlayWaveBuffer(int no, DWORD loop);
BOOL StopWaveBuffer(int no);
BOOL GetWaveBufferStat(int no);
BOOL SetWaveFreq(int no, DWORD);
BOOL SetWaveVol(int no, DWORD);
LONG GetWaveVol(int no);

#ifdef __cplusplus
}
#endif 
