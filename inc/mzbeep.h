#ifndef __mzbeep_h__
#define __mzbeep_h__

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void mzbeep_init(int freq);
void mzbeep_clean();
void mzbeep_stop();
void mzbeep_setFreq(int arg);
void mzbeep_update(short* ptr, int cou);

#ifdef __cplusplus
}
#endif

#endif // __mzbeep_h__
