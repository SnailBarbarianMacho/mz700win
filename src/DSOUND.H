#ifndef mz700win_dsound_h
#define mz700win_dsound_h

#define DSBCAPS_CTRLDEFAULT		(DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLFREQUENCY)

BOOL DSound_Init(unsigned long rate, unsigned long length);
BOOL DSound_Cleanup(void);

void CALLBACK DSound_TimeProc(unsigned int uid, unsigned int tmp, DWORD user, DWORD tmp0, DWORD tmp1);
void DSound_Send(void);

#endif //mz700win_dsound_h
