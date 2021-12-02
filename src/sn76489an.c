#include "dprintf.h"
#include "sn76489an.h"

//
static	int hw_freq[2];
static	int base_clock[2];
static	int reg_cur[2];

static	short voltbl[16];
static	TPSGCH ch[2][4];
	
static	DWORD noise_gen[2];
static	DWORD noise_fb[2];
static	DWORD noise_md[2];

/*
 * 音量あふれチェックwav加算
 */
__inline int ADD_SATULATE_WAV(int mix, int val)
{
	__asm
	{
		mov eax,mix;
		add eax,val;
		cmp eax,-32768;
		jg L01;
		mov eax,-32768;
		jp L02;
L01:
		cmp eax,32767;
		jl L02;
		mov eax,32767;
L02:
		mov mix,eax
	}
	return mix;
}

/*
 * コンストラクタ
 */
void sn76489an_init(int freq, int bclock) {
	
	int i;

	for (i=0; i<2; i++) {
		hw_freq[i] = freq;
		base_clock[i] = bclock;
	}
	
	sn76489an_reset();

	// ボリュームテーブル作成
	sn76489an_make_voltbl(0x1E00);
}

/*
 * デストラクタ
 */
void sn76489an_cleanup() {
}

/*
 * リセット
 */
void sn76489an_reset() {
	int i,j;
	int freq2;

	for (j=0; j<2; j++) {
		reg_cur[j] = 0;
		
		// チャンネルワークの初期化
		ZeroMemory(ch[j], sizeof(ch[0]));
		
		// ボリュームリセット
		for (i=0; i<4; i++) {
			ch[j][i].vol = 0;
		}
		noise_gen[j] = NG_PRESET;
		noise_md[j] = 0;
		noise_fb[j] = FB_PNOISE;
		ch[j][3].bPulse = (NG_PRESET & 1) ? TRUE : FALSE;
		freq2 = (base_clock[j] / 512);
		ch[j][3].pulse_vec = ((freq2 << 16) / hw_freq[j]);
	}
}

/*
 * ボリュームテーブルの作成
 */
void sn76489an_make_voltbl(int volume) {
	// create gain
	double vol = volume;
	int i;

	for(i = 0; i < 15; i++) {
		voltbl[i] = (short)vol;
		vol /= 1.258925412;
	}
	voltbl[15] = 0;
}

/*
 * トーン周波数の設定
 */
void sn76489an_setFreqTone(int iNo, int iCh, int arg) {
	TPSGCH * psgch = &ch[iNo][iCh];
	int freq2;

	if (arg <= 2) {
		psgch->bMute = TRUE;
		return;
	}
	
	psgch->bMute = FALSE;
	freq2 = base_clock[iNo] / (arg * 32);
	psgch->pulse_vec = ((freq2 << 16) / hw_freq[iNo]) * 2;
}

/*
 * ノイズ周波数の設定
 */
void sn76489an_setFreqNoise(int iNo, int arg) {
	ch[iNo][3].bMute = FALSE;
	ch[iNo][3].pulse_vec = ((arg << 16) / hw_freq[iNo]);
}

/*
 * 更新
 */
void sn76489an_update(int iNo, short* ptr, int cou) {
	volatile short dat;
	TPSGCH * psgch;
	int i, j;
	
	for (i=iNo; i<cou*2; i+=2) {
		dat = 0;
		for (j=0; j<=3; j++) {
			psgch = &ch[iNo][j];
			if (psgch->vol == 0 || psgch->bMute) {
				continue;
			}
			if ((psgch->pulse_cou += psgch->pulse_vec) & 0x10000) {
				psgch->pulse_cou &= 0xFFFF;
				if (j != 3) {
					// Tone
					psgch->bPulse ^= TRUE;
				} else {
					// Noise
					if(noise_gen[iNo] & 1) {
						noise_gen[iNo] ^= noise_fb[iNo];
					}
					noise_gen[iNo] >>= 1;
					psgch->bPulse = (noise_gen[iNo] & 1) ? TRUE : FALSE;
				}
			}
			dat += (psgch->bPulse ? -psgch->vol: psgch->vol);
		} // j
//		ptr[i] += dat;
		ptr[i] =ADD_SATULATE_WAV(ptr[i], dat);
	
	} //i
	
}

void sn76489an_outreg(int iNo, int value)
{
	TPSGCH *psgch;

	if (value & 128) {
		// 下位
		psgch = &ch[iNo][(value >> 5) & 3];
		if (value & 16)	{
			// 音量
			reg_cur[iNo] = (value >> 5) & 3;
			psgch->vol = voltbl[value & 15];
			//		dprintf("psgout::value=%02x\n", psgp->vol);
			return;
		}
		// 操作するレジスタを設定
		reg_cur[iNo] = (value >> 5) & 3;
		psgch = &ch[iNo][reg_cur[iNo]];
		if (reg_cur[iNo] < 3) {
			// tone 周波数
			psgch->freq &= 0x3F0;
			psgch->freq |= (value & 0x0F);
		} else {
			// noise
			noise_md[iNo] = value & 7;
			noise_fb[iNo] = (value & 4) ? FB_WNOISE : FB_PNOISE;
			value &= 3;
			if (value == 3) {
				sn76489an_setFreqTone(iNo, 3, ch[iNo][2].freq);
			} else {
				sn76489an_setFreqNoise(iNo, (base_clock[iNo] / 512) >> value);
			}
			noise_gen[iNo] = NG_PRESET;
			ch[iNo][3].bPulse = (NG_PRESET & 1) ? TRUE : FALSE;
		}
	} else {
		// 上位
		psgch = &(ch[iNo][reg_cur[iNo]]);
		psgch->freq &= 0x0F;
		psgch->freq |= ((value & 0x3F)<<4);
//		dprintf("psgout::hi :port=%02x\n",value);
		if (reg_cur[iNo] != 3) {
			// TONE
			sn76489an_setFreqTone(iNo,reg_cur[iNo], psgch->freq);
		} else { 
			// update noise shift frequency
			if((noise_md[iNo] & 3) == 0x03) {
				sn76489an_setFreqTone(iNo, 3, ch[iNo][2].freq);
//				ch[3].pulse_vec = (ch[2].pulse_vec >> 1);
//				ch[3].pulse_cou = 0;
			}
		}
	}
	
	
}

