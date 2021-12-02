//----------------------------------------------------------------------------
// File:MZscrn.c
//
// mz700win:VRAM/CRT Emulation Module
// ($Id: Mzscrn.c 1 2007-07-27 06:17:19Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#define MZSCRN_H_  1

#include <windows.h>
#include "mz700win.h"
#include "win.h"

#include "Z80.h"

#include "MZhw.h"
#include "MZddraw.h"
#include "MZscrn.h"

static UINT8	vidmem_old[4096];
static int		refresh_screen;		// スクリーンのリフレッシュフラグ bit0:VRAM Rewrite/bit1:Window Blt
static int		fullmode;

#ifndef __BORLANDC__
static UINT8	fontbuf[512][16*8]; // Compress Font Work
static UINT8	* vptr = NULL;
#else
UINT8	fontbuf[512][16*8]; // Compress Font Work
UINT8	* vptr = NULL;
#endif

const UINT8		mzcol2dibcol[8] = { 0,11,12,13,14,15,16,17 };
UINT8			mzcol_palet[8];

//
int get_fullmode(void)
{
	return fullmode;	
}

// Change to Full-Screen
BOOL chg_screen_mode(int mode)
{
	HANDLE hProcess;
	
	if (mode == SCRN_FULL)
	{
		if (!CreateDirectDraw(hwndApp))	return FALSE;
		
		// Change to fullscreen
		fullmode = ChangeToFullScreen();

		if (fullmode<0)
		{
			MessageBox(hwndApp, "Display Mode Change Error!",
					   "Error", MB_ICONERROR);
			return FALSE;
		}

		// 全画面であれば、プロセスの優先度をあげる
	    hProcess = GetCurrentProcess();
		if (hProcess)
		{
			SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
		}
		//
		mouse_cursor(FALSE);											/* clear mouse cursor */
	}
	else
	{
		// Window
		EndDirectDraw();
		mouse_cursor(TRUE);												/* mouse cursor on */

	}

	mz_refresh_screen(REFSC_ALL);

	return TRUE;
}

//
void mz_screen_init(void)
{
	mz_palet_init();	/* 1500パレット初期化 */
	
	vptr = CreateOffScreen();
	ZeroMemory(vptr,320*200);
	
	mz_refresh_screen(REFSC_ALL);
}

//
// In:	bit00:Render VRAM on Virtual Screen
//		bit01:Blt Virtual Screen
void mz_refresh_screen(int val)
{
	refresh_screen = val;
}

void mz_blt_screen(void)
{
	refresh_screen |= REFSC_BLT;
}

//
int get_mz_refresh_screen(void)
{
	return refresh_screen;
}

//
void mz_palet(int iVal)
{
	mzcol_palet[iVal>>4] = mzcol2dibcol[iVal&0x0F];
}

void mz_palet_init(void)
{
	/* 1500パレット初期化 */
	CopyMemory(mzcol_palet,mzcol2dibcol,sizeof(mzcol2dibcol));
};

//
int search_bits(int flg,int st,int bit)
{
	int a;
	int r;
	int mask;
	
	mask = 0x80 >> st;

	r=0;
	while (1)
	{
		a = (bit & mask) ? 1 : 0;
		if (a != flg) break;

		r++;
		if (r>8) break;
		mask >>= 1;
		if (!mask) break;
	}

	return r;
}
/************************************/
/* フォントデータを抜き描画用に圧縮 */
/************************************/
void compress_font(void)
{
	unsigned char *orgfont;
	unsigned char *dstfont;
	int a,b,c,dat;
	int	i,j,k,mask;

	/* Buffer Clear */
	ZeroMemory(&fontbuf,sizeof(fontbuf));
	
	for (i=0;i<512;i++)
	{
		dstfont = &fontbuf[i][0];

		/* 縦ループ */
		for (j=0;j<8;j++)
		{
			orgfont = &font[(i<<3)+j];

			/* １行エンコード */
			mask = 0x80;
			k=0;
			while (1)
			{
				dat=(*orgfont);
				a=(dat & mask) ? 1 : 0;
				b=search_bits(a,k,dat);
				if (a)
				{
					c = 0x80|b;
				}
				else
				{
					c = b;
				}
				
				*(dstfont++) = (unsigned char) c;
				k += b;
				if (k>7) break;
				mask >>= b;
				if (!mask) break;
			}
			
			/* １行エンドマーク付ける */
			*(dstfont++) = 0;
		}
		/* １キャラエンドマーク付ける */
		*(dstfont++) = 0;
	}

}
/**********************/
/* redraw the screen */
/*********************/
void update_scrn(void)
{
	static int count=0;
	int x,y,c;
	UINT8 *ptr,*oldptr;

	hw700.retrace=1;
	
	hw700.cursor_cou++;
	if(hw700.cursor_cou>=60)													/* for japan.1/60 */
	{
		hw700.cursor_cou=0;
	}
	
	/* only do it every 1/Nth */
	count++;
	if (count<(menu.scrn_freq+1) )
	{	 
		 return;
//		 if (!refresh_screen) return;
	}
	else
	{
		count=0;
	}

	
	if (vptr==NULL) return;

	ptr = mem+VID_START;
	oldptr = vidmem_old;

//	if (refresh_screen)
	{

		if (menu.machine == MACHINE_MZ700)
		{
			/* mz700モード */
			/* マシンモードによって描画をわける */
			if (!menu.pcg700 || (hw700.pcg700_mode==8))
			{
				/* MZ-700mode Normal描画 */
				for(y=0;y<25;y++)
				{
					for(x=0;x<40;x++,ptr++,oldptr++)
					{
						c=*ptr;
						if(*oldptr!=c || oldptr[2048]!=ptr[2048] || (refresh_screen&REFSC_VRAM))
						{
							mz_chr_put(x,y,c,ptr[2048]);
							*oldptr = c;
							oldptr[2048] = ptr[2048];
							refresh_screen |= REFSC_BLT;
						}
					}
				}
			}
			else
			{
				/* MZ-700mode PCG700描画 */
				for(y=0;y<25;y++)
				{
					for(x=0;x<40;x++,ptr++,oldptr++)
					{
						c=*ptr;
						if(*oldptr!=c || oldptr[2048]!=ptr[2048] || (refresh_screen&REFSC_VRAM))
						{
							if (!(c & 0x80)) mz_chr_put(x,y,c,ptr[2048]);
							else mz_pcg700_put(x,y,c&0x7F,ptr[2048]);
							*oldptr = c;
							oldptr[2048] = ptr[2048];
							refresh_screen |= REFSC_BLT;
						}
					}
				}
			}
			refresh_screen&=(~REFSC_VRAM);
		}
		else
		{
			/* mz1500モード */
			/* TEXT 背景色描画 */
			for(y=0;y<25;y++)
			{
				for(x=0;x<40;x++,ptr++,oldptr++)
				{
					c=*ptr;
					switch (hw1500.prty & 3)
					{
					case 0:
						/* PCG無し */
						mz_chr_put(x,y,c,ptr[2048]);
						*oldptr = c;
						oldptr[2048] = ptr[2048];
						break;
						
					case 1:
						/* TEXT>PCG>BACKCOLOR */
						if (ptr[3072] & 8)
						{
							/* PCG下・テキスト上 */
							mz_pcg1500_put(x,y,ptr[1024],ptr[3072],ptr[2048]);
							mz_text_overlay(x,y,c,ptr[2048]);
						}
						else
						{
							/* PCG無し */
							mz_chr_put(x,y,c,ptr[2048]);
						}
						break;
						
					case 2:
						/* PCG無し */
						mz_chr_put(x,y,c,ptr[2048]);
						break;
						
					case 3:
						/* PCG>TEXT>BACKCOLOR */
						mz_chr_put(x,y,c,ptr[2048]);
						if (ptr[3072] & 8) mz_pcg1500_overlay(x,y,ptr[1024],ptr[3072]);
						break;
						
					default:;
					}
					
					
				}
			}
			
			refresh_screen|=REFSC_BLT;
			refresh_screen&=(~REFSC_VRAM);
		}
		
		/* 画面をウィンドウに反映 */
		if (refresh_screen&REFSC_BLT)
		{
			update_winscr();
			refresh_screen &= (~REFSC_BLT);
		}
		
	} /* if (refresh_screen) */
	
}

#ifndef __BORLANDC__

/*
  MZの画面にテキスト文字を表示
  
  In:	x = chr.X
		y = chr.Y
		code  = Display Code
		color = Color Code
 */
void mz_chr_put(int x,int y,int code,int color)
{
	_asm
	{
		/* src.font address */
		mov		esi,[code]
		mov		eax,[color]
		mov		edi,eax
		and		eax,0x80
		shl		eax,1
		or		esi,eax													/* ecx=キャラクタコード */
		shl		esi,7
		add		esi,offset fontbuf

		mov		ebx,offset mzcol_palet
		/* fg color */	
		mov		eax,edi
		shr		eax,4
		and		eax,0x07
		mov		dl,[ebx+eax]											/* dl=bgcolor */

		/* bg color */	
		mov		eax,edi
		and		eax,0x07
		mov		dh,[ebx+eax]											/* dh=fgcolor */
			
		/* dest.address calc */
		mov		edi,[y]
//		shl		edi,3													/* << 3 */
		shl		edi,6+3													/* << 6 = x64 */
		mov		eax,edi
		shl		edi,2													/* x256 */
		add		edi,eax													/* +(x64)=x320 */

		mov		eax,[x]
		shl		eax,3													/* x8 */
		add		edi,eax													/* edi=vram address */
		add		edi,dword ptr[vptr]										/* +memory base */

		xor		ecx,ecx													/* これが重要だった */
		/*	draw loop */
		mov		ebx,8
	cp_draw_loop:
		lodsb
		or		al,al
		je		cp_draw_nextline
			
		test	al,0x80
		jnz		cp_draw_1

		mov		cl,al
//		movzx	cx,al
		mov		al,dh													/* bgcolor */
		rep		stosb
		jmp		short cp_draw_loop
			
	cp_draw_1:
		and		al,0x0F
		mov		cl,al
		mov		al,dl													/* fgcolor */
		rep		stosb
		jmp		short cp_draw_loop

		/* 次の行へ */	
	cp_draw_nextline:
		add		edi,312
		dec		ebx
		jnz		cp_draw_loop

	};


}

/*
  MZの画面にPCG700文字を表示
  
  In:	x = chr.X
		y = chr.Y
		code  = Display Code
		color = Color Code
 */
void mz_pcg700_put(int x,int y,int code,int color)
{
	_asm
	{
		/* src.font address */
		mov		esi,[code]
		mov		eax,[color]
		mov		edi,eax
		and		eax,0x80
		or		esi,eax													/* ecx=キャラクタコード */
		shl		esi,3													/* x8 */
		add		esi,[pcg700_font]										/* esi=pcg700_font */

		mov		ebx,offset mzcol2dibcol
		/* fg color */	
		mov		eax,edi
		shr		eax,4
		and		eax,0x07
		mov		dl,[ebx+eax]											/* dl=fgcolor */

		/* bg color */	
		mov		eax,edi
		and		eax,0x07
		mov		dh,[ebx+eax]											/* dh=bgcolor */
			
		/* dest.address calc */
		mov		edi,[y]
		shl		edi,6+3													/* << 6 = x64 */
		mov		eax,edi
		shl		edi,2													/* x256 */
		add		edi,eax													/* +(x64)=x320 */

		mov		eax,[x]
		shl		eax,3													/* x8 */
		add		edi,eax													/* edi=vram address */
		add		edi,dword ptr[vptr]										/* +memory base */

		/*	draw loop */
		mov		ecx,8
	pp_draw_loop:
		lodsb

		mov		bl,8	
	pp_draw_loop_x:
		shl		al,1
		jc		pp_draw_bgcol	

		mov		[edi],dh												/* fgcolor */
		jmp		short pp_draw_next
	pp_draw_bgcol:		
		mov		[edi],dl												/* bgcolor */
	pp_draw_next:		
		inc		edi
		dec		bl	
		jnz		pp_draw_loop_x

		/* 次の行へ */	
		add		edi,312
		loop	pp_draw_loop

	};


}

#if 0
/*
  MZの画面にテキスト背景色を表示
  
  In:	x = chr.X
		y = chr.Y
		code  = Display Code
		color = Color Code
 */
void mz_bgcolor_put(int x,int y,int color)
{
	_asm
	{
		/* dest.address calc */
		mov		edi,[y]
		shl		edi,6+3													/* << 6 = x64 */
		mov		eax,edi
		shl		edi,2													/* x256 */
		add		edi,eax													/* +(x64)=x320 */

		mov		eax,[x]
		shl		eax,3													/* x8 */
		add		edi,eax													/* edi=vram address */
		add		edi,dword ptr[vptr]										/* +memory base */

		/* bg color */	
		mov		ecx,[color]
		and		ecx,0x07
		mov		ebx,offset mzcol_palet
		mov		al,[ebx+ecx]
		mov		ah,al													/* dx=bgcolor */
		mov		edx,312
			
		/*	draw loop */
		mov		ecx,8
	bp_draw_loop:
		/* 横に８ドット */
		stosw
		stosw
		stosw
		stosw

		/* 次の行へ */	
		add		edi,edx
		loop	bp_draw_loop

	};

}
#endif

/*
  MZの画面にテキスト文字をオーバーレイ表示
  
  In:	x = chr.X
		y = chr.Y
		code  = Display Code
		color = Color Code
 */
void mz_text_overlay(int x,int y,int code,int color)
{
	_asm
	{
		/* src.font address */
		mov		esi,[code]
		mov		eax,[color]
		mov		edi,eax
		and		eax,0x80
		shl		eax,1
		or		esi,eax													/* ecx=キャラクタコード */
		shl		esi,7
		add		esi,offset fontbuf

		mov		ebx,offset mzcol_palet
		/* fg color */	
		mov		eax,edi
		shr		eax,4
		and		eax,0x07
		mov		dl,[ebx+eax]											/* dl=bgcolor */

		/* dest.address calc */
		mov		edi,[y]
		shl		edi,6+3													/* << 6 = x64 */
		mov		eax,edi
		shl		edi,2													/* x256 */
		add		edi,eax													/* +(x64)=x320 */

		mov		eax,[x]
		shl		eax,3													/* x8 */
		add		edi,eax													/* edi=vram address */
		add		edi,dword ptr[vptr]										/* +memory base */

		xor		ecx,ecx													/* これが重要だった */
		/*	draw loop */
		mov		ebx,8
	to_draw_loop:
		lodsb
		or		al,al
		je		to_draw_nextline
			
		test	al,0x80
		jnz		to_draw_1

		mov		cl,al													/* bg(transparent) */
		add		edi,ecx	
		jmp		short to_draw_loop
			
	to_draw_1:
		and		al,0x0F
		mov		cl,al
		mov		al,dl													/* fgcolor */
		rep		stosb
		jmp		short to_draw_loop

		/* 次の行へ */	
	to_draw_nextline:
		add		edi,312
		dec		ebx
		jnz		to_draw_loop

	};


}

/*
  MZの画面にPCG1500文字を表示
  
  In:	x = chr.X
		y = chr.Y
		code  = Pcg Code
		attr = Attr Code
 */
void mz_pcg1500_overlay(int x,int y,int code,int attr)
{
	_asm
	{
		/* src.font address */
		mov		esi,[code]
		mov		eax,[attr]
		and		eax,0xC0
		shl		eax,2
		or		esi,eax													/* ecx=キャラクタコード */
		shl		esi,3													/* x8 */

		/* dest.address calc */
		mov		edi,[y]
		shl		edi,6+3													/* << 6 = x64 */
		mov		eax,edi
		shl		edi,2													/* x256 */
		add		edi,eax													/* +(x64)=x320 */

		mov		eax,[x]
		shl		eax,3													/* x8 */
		add		edi,eax													/* edi=vram address */
		add		edi,dword ptr[vptr]										/* +memory base */

		/*	draw loop */
		mov		ecx,8
	po_draw_loop:
		mov		eax,esi	
		mov		esi,[pcg1500_font_blue]
		mov		dl,[eax + esi]	
		mov		esi,[pcg1500_font_red]
		mov		bh,[eax + esi]	
		mov		esi,[pcg1500_font_green]
		mov		dh,[eax + esi]	
		inc		eax
		mov		esi,eax	

		mov		bl,8
	po_draw_loop_x:
		xor		eax,eax
		shl		dh,1													/* check green */
		adc		al,ah
			
		shl		bh,1													/* check red */
		adc		al,al
			
		shl		dl,1													/* check blue */
		adc		al,al
			
		/* 透明背景チェック */
		or		al,al
		jz		short po_draw_pcgnext

		mov		al,[mzcol_palet+eax]
		mov		[edi],al												/* bgcolor */
	po_draw_pcgnext:		
		inc		edi
		dec		bl	
		jnz		po_draw_loop_x

		/* to next line */	
		add		edi,312
		loop	po_draw_loop
	};

}

/*
  MZの画面にPCG1500文字を表示
  
  In:	x = chr.X
		y = chr.Y
		code  = Pcg Code
		attr = Attr Code
		bgcolor = bgcolor
 */
void mz_pcg1500_put(int x,int y,int code,int attr,int bgcolor)
{
	_asm
	{
		/* src.font address */
		mov		esi,[code]
		mov		eax,[attr]
		and		eax,0xC0
		shl		eax,2
		or		esi,eax													/* ecx=キャラクタコード */
		shl		esi,3													/* x8 */

		/* dest.address calc */
		mov		edi,[y]
		shl		edi,6+3													/* << 6 = x64 */
		mov		eax,edi
		shl		edi,2													/* x256 */
		add		edi,eax													/* +(x64)=x320 */

		mov		eax,[x]
		shl		eax,3													/* x8 */
		add		edi,eax													/* edi=vram address */
		add		edi,dword ptr[vptr]										/* +memory base */

		/*	draw loop */
		mov		ecx,8
	p15_draw_loop:
		mov		eax,esi	
		mov		esi,[pcg1500_font_blue]
		mov		dl,[eax + esi]	
		mov		esi,[pcg1500_font_red]
		mov		bh,[eax + esi]	
		mov		esi,[pcg1500_font_green]
		mov		dh,[eax + esi]	
		inc		eax
		mov		esi,eax	

		mov		bl,8
	p15_draw_loop_x:
		xor		eax,eax

		shl		dh,1													/* check green */
		adc		al,ah
			
		shl		bh,1													/* check red */
		adc		al,al
			
		shl		dl,1													/* check blue */
		adc		al,al
			
		/* 透明背景チェック */
		or		al,al
		jnz		short p15_draw_pcgnext

		mov		al,byte ptr[bgcolor]									/* 背景色 */
		and		al,0x0F
	p15_draw_pcgnext:		
		mov		al,[mzcol_palet+eax]									/* フォント色 */
		stosb															/* dot */
		dec		bl	
		jnz		p15_draw_loop_x

		/* 次の行へ */	
		add		edi,312
		loop	p15_draw_loop

	};

}
#endif	// __BORLANDC__
