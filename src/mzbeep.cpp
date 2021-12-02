#include "dprintf.h"
#include "mzbeep.h"

static	BOOL bPlaying;
static 	BOOL bPulse;
static	int hw_freq;
static	int freq;
static	int pulse_cou;
static	int pulse_vec;

/*
 * �R���X�g���N�^
 */
void mzbeep_init(int freq) {
//	dprintf("mzbeep()\n");

	hw_freq = freq;
	bPlaying = false;
	bPulse = false;
	pulse_cou = 0;
	pulse_vec = 0;
}

/*
 * �f�X�g���N�^
 */
void mzbeep_clean() {
	bPlaying = false;
//	dprintf("~mzbeep()\n");
}

/*
 * ����
 */
void mzbeep_stop() {
	bPlaying = false;
//	dprintf("mzbeep::stop()\n");
}

/*
 * ���g���̐ݒ�
 */
void mzbeep_setFreq(int arg) {
	bPlaying = true;
	freq = arg;
	pulse_vec = (freq << 16) / hw_freq * 2;

//	dprintf("mzbeep::setFreq(%d)\n",arg);
}

/*
 * �X�V
 */
void mzbeep_update(short* ptr, int cou) {
	int i;

	short dat;
	if (!bPlaying) {
		return;
	}
	
	for (i=0; i<cou*2; i+=2) {
		if ((pulse_cou += pulse_vec) & 0x10000) {
			bPulse ^= true;
			pulse_cou &= 0x0FFFF;
		}

		dat = bPulse ? -0x2000 : 0x2000;
		ptr[i] += dat;
	}
	
}
