/* tapeload.cpp ver0.14 Copyright (c) 1999-2000,2009 Takeshi Maruyama
 * modified G01 by GORRY.
 *
 * Based on Russel Marks's mzget - read MZ700 tape data
 * outputs header to `header.dat', and data to `out.dat'.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include <windows.h>
#include <mmsystem.h>

#define DEF_BUFSIZE 0x2000000											// �f�t�H���g32MB
#define default_depth 0x20												// �����Ƃ݂Ȃ��U��
#define default_wait_22k 7												// �^�C�~���O(320us)
#define default_wait_44k 15												// �^�C�~���O(320us)
#define default_wait_48k 16												// �^�C�~���O(320us)

#define CP_JAPAN		932												// ���{�̃R�[�h�y�[�W�F�X�R�Q
#define JAPANESE		0												// ���{��g�p�t���O

#define USE_RECORD_16BIT                                                // 16bit�^���g�p


typedef struct _TCHUNK
{
	char   ID[4];														// �`�����N�̎��
	DWORD  size;														// �`�����N�̃T�C�Y
} TCHUNK;

int BUFSIZE;															// WAV�o�b�t�@�T�C�Y

FILE *ifp;																// WAV�t�@�C�����͗p

char infile[256];														// ���̓t�@�C����


int wav_free(void);
void findtone(int);
void file_input_setup(void);

/* �t�@�C�����\���p������ */
/* ���{�� */
static unsigned char mzascii_jp[]=
{
	"�@�I�h���������f�i�j���{�C�|�D�^"\
	"�O�P�Q�R�S�T�U�V�W�X�F�G�������H"\
	"���`�a�b�c�d�e�f�g�h�i�j�k�l�m�n"\
	"�o�p�q�r�s�t�u�v�w�x�y�m�_�n����"\
	"��������������������������������"\
	"�����ΐ��؋��y���N�����b�~������"\
	"���B�u�v�A�D���@�B�D�F�H�������b"\
	"�[�A�C�E�G�I�J�L�N�P�R�T�V�X�Z�\"\
	"�^�`�c�e�g�i�j�k�l�m�n�q�t�w�z�}"\
	"�~���������������������������J�K"\
	"��������������������������������"\
	"��������������������������������"\
	"��������������������������������"\
	"��������������������������������"\

};

/* �C�O */
static unsigned char mzascii_eu[]=
{
	" !\x22#$%&\x27()*+,-./"											/* 20 */
	"0123456789:;<=>?"													/* 30 */
	"@ABCDEFGHIJKLMNO"													/* 40 */
	"PQRSTUVWXYZ[\\]**"													/* 50 */
	"****************"													/* 60 */
	"****************"													/* 70 */
	"}***************"													/* 80 */
	"_**`~***********"													/* 90 */
	"****************"													/* A0 */
	"**************{*"													/* B0 */
	"****************"													/* C0 */
	"****************"													/* D0 */
	"****************"													/* E0 */
	"****************"													/* F0 */
};

unsigned char filedata[65536];  /* as much as you could possibly save */
int sumdt;

MMRESULT mmr;
HWAVEIN hwi;
WAVEFORMATEX wfmt;
WAVEHDR whed;
MMTIME mmt;
BYTE * record_buf = NULL;

int now_ptr;
int read_ptr;

int low,high;

int wav_bits = 8;
int wav_sample = 0;
double wav_secpersample;

int cp;																	// �R�[�h�y�[�W

char opt_b;																// �r�b�g�f�[�^�\��
char opt_v;																// �_���v�f�[�^�\��
char opt_r;																// rev.flag
int opt_d;																// �����Ƃ݂Ȃ��U��
int opt_w;																// �^�C�~���O(Default=7)
int opt_w_cmd;															// �^�C�~���O(Default=7)
int opt_s;																// �X�^�[�g�r�b�g�^�C�~���O
int opt_m;																// �o�b�t�@������
int opt_a[10];
int opt_a_index;
int opt_a_count;

int bitlength[256];

int framingerror;

// �^�����ɃG���[���N���ďI��
void die(char *mes)
{
    Sleep(0);
	fprintf(stderr,mes);
	wav_free();
	exit(1);
}

// stderr�Ƀ��b�Z�[�W�o��
void debug(char *mes)
{
	fprintf(stderr,mes);
}

// �g�p�@ (Usage)
void usage(void)
{
	debug("usage:tapeload [options] <Wavefile[.wav]>\n\n");

	if (cp == CP_JAPAN)
	{
		// Japanese
		debug(
		  "22050Hz �W�r�b�g���m�����ŋL�^���ꂽWAV�t�@�C���܂���\n"\
		  "�T�E���h�J�[�h�̊O�����͒[�q����AMZ-700/1500�̃e�[�v�����[�h���܂�\n"\
		  "�t�@�C�����̎w�肪�����ꍇ�A�O�����͒[�q���烍�[�h���܂�\n"\
		  "���[�h���ꂽ�f�[�^�́Aheader.dat / out.dat �Ƃ����t�@�C���Ƃ���\n"\
		  "�L�^����܂�\n\n"\
		  "      -l[j|e]\t\t��ȃ��b�Z�[�W����{��(j)�^�p��(e)�ŏo�͂���\n"\
		  "      -mn[1~64]\t\t�m�ۂ���o�b�t�@�T�C�Y(Default=32MB)\n"\
		  "      -r\t\t�����f�[�^�̈ʑ��𔽓]���ĉ��߂���\n"\
		  "      -b\t\t�r�b�g�f�[�^��\������\n"\
		  "      -v\t\t�_���v�f�[�^��\������\n"\
		  "      -dn[1~127]\t���̓��x���������l(Default=32)\n"\
		  "      -wn[1~100] \t�f�[�^�ǂݎ��P�ʎ���\n"\
		  "      -a[0000~FFFF[,0000~FFFF[,...]]] \t�ǉ��f�[�^�u���b�N�̃T�C�Y\n"
		);
	}
	else
	{
		// English
		debug(
		  "This program loads the tape of MZ700/800 from the WAV file\n"\
		  "recorded by 22050Hz monophonic 8-bit recording, or a sound card.\n"\
		  "When there is no specification of a file name, it loads from\n"\
		  "a LINE-IN jack.\n"\
		  "Loaded data is recorded as a file called header.dat / out.dat.\n\n"\
		  "      -l[j|e]\t\tThe main messages are outputted in\n"\
		  " \t\t\tJapanese (j) / English (e)\n"\
		  "      -mn[1~64]\t\tSet wave buffer size n megabytes\n"\
		  "                                      (default:n=32/range:1-64)\n"\
		  "      -r\t\tThe phase of voice data is reversed\n"\
		  "      -b\t\tDisplay bit data\n"\
		  "      -v\t\tDisplay read code\n"\
		  "      -dn[1~127]\tAdjust Input Level(default:n=32/range:1-127)\n"\
		  "      -wn[1~100] \tData reading unit time\n");
	}
	
	exit(0);
}

// �}���`���f�B�A�֐��̃G���[�\��
void mmerr(int mr)
{
	char err_str[256];

	waveInGetErrorText(mr, err_str,sizeof(err_str));
	fprintf(stderr,err_str);
}

void filename_put(unsigned char *ptr)
{
	unsigned char *mzascii;													// point mzascii_jp or eu
	int i,c,a;
	
	fprintf(stderr,"FOUND ");

	mzascii = ((cp == CP_JAPAN) ? mzascii_jp : mzascii_eu);				// mzascii�̃|�C���^�擾
	
	for (;;)
	{
		c=*(ptr++);
		if (c==0x0D) break;

		if (c<0x20) continue;

		c-=0x20;
		// ���{��̏ꍇ
		if (cp==CP_JAPAN)
		{
			c<<=1;
			
			fputc(mzascii[c],stderr);
			fputc(mzascii[c+1],stderr);
		}
		else
		{
			// �C�O�̏ꍇ
			fputc(mzascii[c],stderr);
		}
		
	}

	fprintf(stderr,"\n");

}

int getwave(void)
{
	int r;
	static int pos=-1024;

	if (ifp == NULL)
	{
		// WaveIn
		do
		{
			mmr = waveInGetPosition(hwi,&mmt, sizeof(mmt));
			now_ptr = mmt.u.cb;
			if (now_ptr == BUFSIZE)
			{
				die("\nWAV Buffer Full!\n");
			}
			
			if (kbhit())
			{
				die("\nAbort...\n");
			}
			Sleep(0);
		} while (now_ptr<(read_ptr+0x1000));

#ifndef USE_RECORD_16BIT
//		8bit
		r = record_buf[read_ptr++];
#else
//		16bit 
		r = (record_buf[read_ptr+1] << 8) | record_buf[read_ptr];
		read_ptr += 2;
		r >>= 8;
#endif
	}
	else
	{
		// from WAV File
		if ( pos+1024 <= ftell(ifp) )
		{
			pos = ftell(ifp);
			fprintf(stderr, "%d \r", pos );
		}
		r = fgetc(ifp);
		if (r == EOF)
		{
			die("\nReached to end of file....\n");
		}

		if (wav_bits == 16) {
			r += (fgetc(ifp)*256);
			r += 0x8000;
			r &= 0xffff;
			r >>= 8;
		} else
		if (wav_bits == 24) {
			r += (fgetc(ifp)*256);
			r += (fgetc(ifp)*65536);
			r += 0x800000;
			r &= 0xffffff;
			r >>= 16;
		}
		wav_sample++;
		
		if (kbhit())
		{
			die("\nAbort...\n");
		}

	}
	
	if (r<low) low = r;
	if (r>high) high = r;
	
	return r;
}
	
int getaudio(void)
{
	unsigned char c;
	static int r;
	static int r2;
	static int count;

	c = getwave();
	if (c<=(0x80-opt_d)) r=0;
	if (c>=(0x80+opt_d)) r=1;

	if ( r2 != r )
	{
//		printf( "              %d\n", count );
		if ( count < 256 )
		{
			bitlength[count]++;
		}
		r2 = r;
		count = 0;
	}
	count++;

	if (opt_b)
	{
		putchar(r ? '*' : '.');
	}

	return r;
}

void  waitstart(void)
{
	int g;

	if (!opt_s) return;

	for(g=0;g<opt_s;g++) getaudio();

}

/* find a 0 -> 1 edge
 * i.e. wait for 0, then wait for 1
 * (well, in theory... :-))
 */
#if 0
void edge(void)
{
	while (getaudio()!=0);											// WAIT 'L'
	while (getaudio()!=1);											// WAIT 'H'
}
#endif

/* find a 0 -> 1 edge
 * i.e. wait for 0, then wait for 1
 * (well, in theory... :-))
 */
 
 int edge2_lr,edge2_hr;
 
int edge2(void)
{
	int lr,hr;

	lr = hr = 0;

	if (!opt_r)
	{
		/* Normal */
		while (getaudio()) hr++;											// WAIT 'L'
		while (!getaudio()) lr++;											// WAIT 'H'
	}
	else
	{
		/* Reverse */
		while (!getaudio()) lr++;											// WAIT 'H'
		while (getaudio()) hr++;											// WAIT 'L'
	}

	edge2_lr = lr;
	edge2_hr = hr;

	return 0;
}

int  wait320(void)
{
	return ( ((edge2_lr+edge2_hr)/2) >= opt_w );
}

int edge2a(void)
{
	int lr,hr;

	lr = hr = 0;

	if (!opt_r)
	{
		/* Normal */
		while (getaudio()) hr++;											// WAIT 'L'
		while (!getaudio()) lr++;											// WAIT 'H'
	}
	else
	{
		/* Reverse */
		while (!getaudio()) lr++;											// WAIT 'H'
		while (getaudio()) hr++;											// WAIT 'L'
	}

	waitstart();

	if (opt_b) printf("\n");

	return (hr << 16)|lr;
}

int  wait320a(void)
{
	int a,b,g;
	a=0;
	
	/* wait 320us */
	g = 0;
	for ( g=0; g<opt_w; g++ )
	{
		getaudio();
	}
	return ( getaudio() ^ opt_r );
}

int getbyte(void)
{
	int t,f,g,dat;
	int l,h;
	
	dat=0;
	for(f=0;f<8;f++)
	{
		dat<<=1;
		edge2();

		if(wait320()) dat|=1,sumdt++;
	}
	
	edge2();
	f = wait320();

	if (opt_v) printf("[%02X]\n",dat);

	if (opt_b) {
		printf("\n");
	}
	if (!framingerror && !f) {
		int min, sec;
		framingerror = 1;
		min = (int)(wav_secpersample * wav_sample / 60.0);
		sec = (int)(wav_secpersample * wav_sample - (double)min*60);
		fprintf(stderr, "\nframing error at %dmin %dsec %02d\n", min, sec,
			(int)((wav_secpersample * wav_sample - (double)(int)(wav_secpersample * wav_sample)) * 100.0));
	}

	return dat;
}








int getblock(unsigned char *dataptr,int length,int header)
{
	FILE *in;
	int i,spd,f,tmp1,tmp2;
	
	findtone(header);  /* header tone */
	
	/* remainder of routine is like ROM `rtape' */
	
	/* the real rom seems to allow one failure by loading from the 2nd
	 * copy, but I don't here. (I may do later if it seems to be needed.)
	 */

	for (i=0;i<2;i++)													// �G���[���g���C��
	{
		do
		{
			edge2();
		}
		while(!wait320());										// 0

		sumdt=0;
		
		for(f=0;f<length;f++)
			dataptr[f]=getbyte();
		
		tmp1=(sumdt&0xffff);
		
		tmp2=256*getbyte();
		tmp2+=getbyte();
		
		if(tmp1!=tmp2)
		{
			fprintf(stderr,"counted %04X, wanted %04X\n",tmp1,tmp2);
			if (i==1)
			{
				fprintf(stderr,"checksum error!\n");

				wav_free();
				exit(1);
			}
			else
			{
//fprintf(stderr,"Ignore checksum...\n");break;
				fprintf(stderr,"Retry\n");
			}
		}
		else break;
	}
	
	return 0;
}


void findtone(int header)
{
	int t,f,done=0;
	int tmcnt=0x2828,h,l;
	
	if(!header) tmcnt=0x1414;
	
	/* this loop corresponds to `gapck' in ROM */
	do
	{
		done=1;
		for(h=0;h<100;h++)
		{
			edge2a();
			if(wait320a())									// 1
			{
				done=0;
				break;
			}
		}
	}
	while(!done);
	fprintf(stderr,"done gapck\n");
	if (opt_b) printf("\n---done gapck\n");
	
	/* this loop corresponds to `tmark' in ROM */
	do
	{
		done=1;
		for(h=0;h<(tmcnt>>8);h++)
		{
			edge2();
			;
			if(!wait320())										// 0
			{
				done=0;
				break;
			}
		}
		
		if(done)
			for(l=0;l<(tmcnt&255);l++)
			{
				edge2();
				if(wait320())								// 1
				{
					done=0;
					break;
				}
			}
	}
	while(!done);
	fprintf(stderr,"done tmark\n");
	if (opt_b) printf("\n---done tmark\n");
	edge2();
	wait320();
}

int wav_setup(void)
{
	record_buf = (BYTE *) malloc(BUFSIZE);
	if (record_buf==NULL)
	{
		debug((cp == CP_JAPAN) ? "������������܂���B\n" : "Out of memory.\n");
		return 1;
	}

	if (!waveInGetNumDevs())
	{
		debug((cp == CP_JAPAN) ? "wave���̓f�o�C�X������܂���B\n" : "Can't find wave input device.\n");
		return 1;
	}

	/* WAVE FORMAT��ݒ� */
	wfmt.wFormatTag = WAVE_FORMAT_PCM;									// 0001?
	wfmt.nChannels = 0x0001;
	wfmt.nSamplesPerSec = 22050;
	wfmt.nAvgBytesPerSec = 22050;
	
	wfmt.nBlockAlign = 0x0001;
#ifndef USE_RECORD_16BIT
    wfmt.wBitsPerSample = 8;                                            // 8Bits Sample
#else
    wfmt.wBitsPerSample = 16;                                           // 16Bits Sample
#endif
    wfmt.cbSize = 0;

	mmr = waveInOpen(&hwi, WAVE_MAPPER , &wfmt , 0 , 0 , CALLBACK_NULL);
	if (mmr)
	{
		mmerr(mmr);
		return 1;
	}
	mmr = waveInReset(hwi);
//	printf("waveInReset = %d\n",mmr);

	// WAVEHDR
	ZeroMemory(&whed,sizeof(whed));
	whed.lpData = (char *) record_buf;
	whed.dwBufferLength = BUFSIZE;
	whed.dwBytesRecorded = BUFSIZE;
//	whed.dwFlags = WHDR_PREPARED;
	whed.dwFlags = 0;
	whed.dwLoops = 1;

	mmr = waveInPrepareHeader(hwi, &whed, sizeof(whed));
	if (mmr)
	{
		mmerr(mmr);
		return 1;
	}

	mmr = waveInAddBuffer(hwi, &whed, sizeof(whed)); 
	if (mmr)
	{
		mmerr(mmr);
		return 1;
	}

	ZeroMemory(&mmt,sizeof(mmt));
	mmt.wType=TIME_BYTES;
	
	return 0;
}

int wav_free(void)
{

	if (infile[0] !=0)                                                  // change:09/05/29
	{
		// WAV���ړ��͂̏ꍇ
		mmr = waveInStop(hwi);
//	printf("waveInStop = %d\n",mmr);
		
		mmr = waveInUnprepareHeader(hwi, &whed, sizeof(whed));
//	printf("waveInUnprepareHeader = %d\n",mmr);
		
		mmr = waveInClose(hwi);
//	printf("waveInClose = %d\n",mmr);
		free(record_buf);
	}
	else
	{
		// �t�@�C��������͂̏ꍇ
		if (ifp)
		{
			// ���̓t�@�C���N���[�Y
			fclose(ifp);
			ifp=NULL;
		}

	}
	
	fprintf(stderr,"low=0x%02X high=0x%02X\n",low,high);
	if ( opt_v )
	{
		int i;
		for ( i=0; i<256; i++ )
		{
			if ( bitlength[i] )
			{
				fprintf( stderr, "bitlength: %d=%d\n", i, bitlength[i] );
			}
		}
	}

	return 0;
}


void file_input_setup(void)
{
	TCHUNK chunk;
	char formtype[4];
	int a;

	ifp = fopen(infile,"rb");
	if (ifp == NULL)													// File not found.
	{
		fprintf(stderr,"Can't open file '%s'.\n", infile);
		return;
	}

	fprintf(stderr,"Open WAV file '%s'.\n",infile);
	
	a=fread(&chunk, 1, 8, ifp);											// chunk read
	if (a != 8 || strncmp(chunk.ID, "RIFF", 4))
	{
		fprintf(stderr,"This file is not RIFF format.\n");
		ifp=NULL;
		return;
	}
	
	a=fread(formtype, 1, 4, ifp);											// chunk read
	if (a != 4 || strncmp(formtype, "WAVE", 4))
	{
		fprintf(stderr,"This file is not WAVE format.\n");
		ifp=NULL;
		return;
	}
	
	//
	while ((a=fread(&chunk, 1, sizeof(TCHUNK), ifp))==sizeof(TCHUNK))
	{
		if (!strncmp(chunk.ID,"fmt ",4))
		{
			fread(&wfmt, 1, chunk.size, ifp);
			wfmt.cbSize = sizeof(WAVEFORMATEX);

//			printf("freq=%d\n", wfmt.nSamplesPerSec);
			
			// ���m�����ȊO�̃f�[�^��������͂���
			if (wfmt.nChannels != 1)
			{
				debug((cp == CP_JAPAN) ? "���m�����f�[�^�ȊO�͎g���܂���B\n" : "It is not able to use it except for monaural data.\n");
				ifp = NULL;
				break;
			}

			// WAV�t�@�C�����g���`�F�b�N
			fprintf(stderr, "wfmt.nSamplesPerSec=%d\n",wfmt.nSamplesPerSec);
			switch (wfmt.nSamplesPerSec)
			{
			  case 48000:
				opt_w = default_wait_48k;
				wav_secpersample = 1.0/48000;
				break;
			  case 44100:
				opt_w = default_wait_44k;
				wav_secpersample = 1.0/44100;
				break;
			  case 22050:
				opt_w = default_wait_22k;
				wav_secpersample = 1.0/22050;
				break;
			  default:
				debug((cp == CP_JAPAN) ? "warning:WAV�t�@�C���̎��g����44.1KHz�ł͂���܂���B\n" :
					  "warning:Frequency of WAV file is not 48/44.1/22.05KHz.\n");
				ifp = NULL;
				break;
			}
			if ( opt_w_cmd )
			{
				opt_w = opt_w_cmd;
			}
			else
			{
				fprintf(stderr, "set -w%d as default.\n", opt_w );
			}

			// WAV�t�@�C���r�b�g���`�F�b�N
			fprintf(stderr, "wfmt.wBitsPerSample=%d\n",wfmt.wBitsPerSample);
			wav_bits = wfmt.wBitsPerSample;
		}
		else
		if (!strncmp(chunk.ID,"data",4))
		{
			//fdatasize = chunk.size;
			//fwavedata = new BYTE [fwavesize];
			//fread(fwavedata,fdatasize,1,ifp);
			break;

		}
		else
		{
			fseek(ifp, chunk.size, SEEK_CUR);
		}

	}



}

int hexatoi(char *p, int *rp) {
	int r = 0, cnt = 0;
	char c;
	while ((c = *p++) != 0) {
		if (c >= '0' && c <= '9') {
			r <<= 4;
			r += (int)(c - '0');
			continue;
		}
		if (c >= 'A' && c <= 'F') {
			r <<= 4;
			r += (int)(c - 'A' + 10);
			continue;
		}
		if (c >= 'a' && c <= 'f') {
			r <<= 4;
			r += (int)(c - 'a' + 10);
			continue;
		}
		if (c == ' ' || c == '\t') break;
		if (c == ',') {
			*rp++ = r;
			cnt++;
			r = 0;
			continue;
		}
		return 0;
	}
	*rp++ = r;
	cnt++;
	return cnt;
}


int main(int argc,char *argv[])
{
	FILE *out;
	int a,i,j;
	int mode,start,len,exec;
	int infoblock_cnt = 0, datablock_cnt = 0;
	int carrylab_mode = 0;

    char full_path[ _MAX_PATH ];
    char drive[ _MAX_DRIVE ];
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];
    char ofn[ _MAX_FNAME ];


	opt_d = default_depth;
	opt_w = 0;
	opt_b = opt_v = opt_r = opt_s = 0;
	BUFSIZE = DEF_BUFSIZE;

	cp = GetOEMCP();													// �f�t�H���g�R�[�h�y�[�W�̎擾 JP=932
	
	infile[0]=0;														// �t�@�C������NULL��
	
	debug("tapeload.exe for Win32 Version 0.14 Programmed by T.Maruyama\n");
	debug("modified G01 by GORRY.\n");
	
	/* �I�v�V�������� */
	for (i=1;i<argc;i++)
	{
		a=argv[i][0];
		if (a=='-'||a=='/')
		{
		   switch (argv[i][1])
		   {
		   case 'b':
			   opt_b=1;
//			   printf("opt_b=1\n");
			   break;
		   case 'r':
			   opt_r=1;
//			   printf("opt_r=1\n");
			   break;
		   case 'v':
			   opt_v=1;
//			   printf("opt_v=1\n");
			   break;
		   case 'd':
			   opt_d=atoi(argv[i]+2);
//			   printf("opt_d=%d\n",opt_d);
			   if (opt_d<1 || opt_d>127)
			   {
				   debug((cp == CP_JAPAN) ? "�p�����[�^�̒l���͈͊O�ł�\n" : "Out of range.\n");
				   exit(1);
			   }
			   break;
		   case 'm':
			   opt_m=atoi(argv[i]+2);
//			   printf("opt_m=%d\n",opt_m);
			   if (opt_m<1 || opt_m>64)
			   {
				   debug((cp == CP_JAPAN) ? "�p�����[�^�̒l���͈͊O�ł�\n" : "Out of range.\n");
				   exit(1);
			   }
			   BUFSIZE = opt_m * 0x100000;
			   break;
		   case 'w':
			   opt_w_cmd=atoi(argv[i]+2);
//			   printf("opt_w_cmd=%d\n",opt_w_cmd);
			   if (opt_w_cmd<1 || opt_w_cmd>100)
			   {
				   debug((cp == CP_JAPAN) ? "�p�����[�^�̒l���͈͊O�ł�\n" : "Out of range.\n");
				   exit(1);
			   }
			   break;
		   case 's':
			   opt_s=atoi(argv[i]+2);
//			   printf("opt_s=%d\n",opt_s);
			   if (opt_s<1 || opt_s>16)
			   {
				   debug((cp == CP_JAPAN) ? "�p�����[�^�̒l���͈͊O�ł�\n" : "Out of range.\n");
				   exit(1);
			   }
			   break;
		   case 'l':
			   a=argv[i][2];
			   if (a=='e' || a=='E') cp=437;
			   else
			   if (a=='j' || a=='J') cp=CP_JAPAN;
			   break;
			   
		   case 'a':
			   opt_a_count = hexatoi(argv[i]+2, &opt_a[0]);
			   for (j=0; j<opt_a_count; j++) {
				    if (opt_a[j] < 0 || opt_a[j] > 0xFFFF) opt_a_count = 0;
			   }
			   if (opt_a_count == 0)
			   {
				   debug((cp == CP_JAPAN) ? "�p�����[�^�̒l���͈͊O�ł�\n" : "Out of range.\n");
				   exit(1);
			   }
			   break;
		   case 'c':
			   opt_w_cmd = atoi(argv[i]+2);
			   if (opt_w_cmd == 0) opt_w_cmd = 12;
			   carrylab_mode = 1;
			   break;
		   case '?':
			   usage();
		   default:
			   fprintf(stderr,(cp == CP_JAPAN) ? "-%c : �����ȃI�v�V�����ł�\n" : "-%c : Invalid option.\n",argv[i][1]);
			   exit(1);
		   }


		}
		else
		{
			/* �t�@�C�����擾 */
			if (infile[0]=='\x0')
			{
				lstrcpy(infile,argv[i]);
				_splitpath(infile , drive, dir, fname, ext );
				if (!ext[0]) lstrcat(infile, ".wav");					// �g���q������������⊮
			}


		}

	}

	/* �^���X�^�[�g */
	now_ptr = read_ptr = 0;
	low=0x80;
	high=0x80;

	if (!infile[0])
	{
		if (wav_setup()) return 1;
		mmr = waveInStart(hwi);
		if (mmr)
		{
			mmerr(mmr);
			wav_free();
			exit(1);
		}
		
	}
	else
	{
		/* �t�@�C�����͏����� */
		file_input_setup();
		if (ifp == NULL) return 1;
	}

read_infoblock:
	/* ���[�h�J�n */
	debug("Now Searching...\n");

	/* read header */
	if(getblock(filedata,carrylab_mode ? 0x20 : 0x80,1)==-1)
		die("Bad header.\n");

	mode =filedata[0x00];
	len  =filedata[0x12]+256*filedata[0x13];
	start=filedata[0x14]+256*filedata[0x15];
	exec =filedata[0x16]+256*filedata[0x17];

	filename_put(filedata+1);

	fprintf(stderr,"mode:%02X start:%04X len:%04X exec:%04X\n",mode,start,len,exec);

	if (!infoblock_cnt) {
		sprintf(ofn, "header.dat");
	} else {
		sprintf(ofn, "header%d.dat", infoblock_cnt);
	}
	infoblock_cnt++;
	out=fopen(ofn,"wb");
	if(out==NULL)
		die("Couldn't open output file.\n");

	fwrite(filedata,1,carrylab_mode ? 0x20 : 0x80,out);
	fclose(out);

	fprintf(stderr,"'%s' Created.\n", ofn);

read_datablock:
	getblock(filedata,len,0);

	fprintf(stderr,"\n");

	fprintf(stderr,"ok!\n");

	if (!datablock_cnt) {
		sprintf(ofn, "out.dat");
	} else {
		sprintf(ofn, "out%d.dat", datablock_cnt);
	}
	datablock_cnt++;
	out=fopen(ofn,"wb");
	if(out==NULL)
		die("Couldn't open output file.\n");
	fwrite(filedata,1,len,out);
	fclose(out);
	fprintf(stderr,"'%s' Created.\n", ofn);

	if (opt_a_index < opt_a_count) {
		len = opt_a[opt_a_index++];
		if (len == 0) goto read_infoblock;
		goto read_datablock;
	}

	wav_free();

	return 0;
}
