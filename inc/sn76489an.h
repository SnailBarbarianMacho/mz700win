#include <windows.h>

#ifndef __sn76489an_h__
#define __sn76489an_h__

#ifdef __cplusplus
extern "C" {
#endif 

#define FB_WNOISE 0x14002
#define FB_PNOISE 0x08000
#define NG_PRESET 0x00f35

typedef struct
{
	BOOL bPulse;
	BOOL bMute;
	int freq;
	int vol;
	DWORD pulse_cou;
	DWORD pulse_vec;
} TPSGCH;

void sn76489an_init(int freq, int bclock);
void sn76489an_cleanup();
void sn76489an_reset();
void sn76489an_make_voltbl(int volume);
void sn76489an_setFreqTone(int iNo, int iCh, int arg);
void sn76489an_setFreqNoise(int iNo, int arg);
void sn76489an_update(int iNo, short* ptr, int cou);
void sn76489an_outreg(int iNo, int value);

#ifdef __cplusplus
}
#endif 

#endif // __sn76489an_h__
