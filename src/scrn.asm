; for nasmw
; 
; (with Borland C++ 5.5)
;
	segment code USE32

	extern _fontbuf
	extern _mzcol_palet
	extern _vptr
	extern _pcg700_font
	extern _mzcol2dibcol
	extern _pcg1500_font_blue,_pcg1500_font_red,_pcg1500_font_green

	global	_mz_chr_put
	global	_mz_pcg700_put
	global	_mz_text_overlay
	global	_mz_pcg1500_overlay
	global	_mz_pcg1500_put

;--------------------------------
;引数offset
@x			equ	8
@y			equ 12
@code		equ	16
@color		equ	20
@attr		equ	20
@bgcolor	equ	24


;--------------------------------
;  MZの画面にテキスト文字を表示
;  
;  In:	x = chr.X
;		y = chr.Y
;		code  = Display Code
;		color = Color Code
;--------------------------------
;(int x,int y,int code,int color)
		align	16
_mz_chr_put:
		push	ebp
		mov		ebp,esp

		push	ebx
		push	esi
		push	edi

		; src.font address
		mov		esi,[ebp+@code]
		mov		eax,[ebp+@color]
		mov		edi,eax
		and		eax,0x80
		shl		eax,1
		or		esi,eax													; ecx=キャラクタコード
		shl		esi,7
		add		esi,_fontbuf		; offset

		mov		ebx,_mzcol_palet	; offset
		; fg color
		mov		eax,edi
		shr		eax,4
		and		eax,0x07
		mov		dl,[ebx+eax]											; dl=bgcolor

		; bg color
		mov		eax,edi
		and		eax,0x07
		mov		dh,[ebx+eax]											; dh=fgcolor
			
		; dest.address calc
		mov		edi,[ebp+@y]
		shl		edi,6+3													; << 6 = x64
		mov		eax,edi
		shl		edi,2													; x256
		add		edi,eax													; +(x64)=x320

		mov		eax,[ebp+@x]
		shl		eax,3													; x8
		add		edi,eax													; edi=vram address
		add		edi,[_vptr]												; +memory base (dword ptr)

		xor		ecx,ecx													; これが重要だった
		;	draw loop
		mov		ebx,8
	.draw_loop:
		lodsb
		or		al,al
		je		.draw_nextline
			
		test	al,0x80
		jnz		.draw_1

		mov		cl,al
		mov		al,dh													; bgcolor
		rep		stosb
		jmp		short .draw_loop
			
	.draw_1:
		and		al,0x0F
		mov		cl,al
		mov		al,dl													; fgcolor
		rep		stosb
		jmp		short .draw_loop

		; 次の行へ
	.draw_nextline:
		add		edi,312
		dec		ebx
		jnz		.draw_loop

		pop		edi
		pop		esi
		pop		ebx

		leave
		ret


;-----------------------------------------
;  MZの画面にPCG700文字を表示
;  
;  In:	x = chr.X
;		y = chr.Y
;		code  = Display Code
;		color = Color Code
;
;-----------------------------------------
;(int x,int y,int code,int color)
		align	16
_mz_pcg700_put:
		push	ebp
		mov		ebp,esp

		push	ebx
		push	esi
		push	edi

		; src.font address
		mov		esi,[ebp+@code]
		mov		eax,[ebp+@color]
		mov		edi,eax
		and		eax,0x80
		or		esi,eax													; ecx=キャラクタコード
		shl		esi,3													; x8
		add		esi,[_pcg700_font]										; esi=pcg700_font

		mov		ebx,_mzcol2dibcol
		; fg color	
		mov		eax,edi
		shr		eax,4
		and		eax,0x07
		mov		dl,[ebx+eax]											; dl=fgcolor

		; bg color	
		mov		eax,edi
		and		eax,0x07
		mov		dh,[ebx+eax]											; dh=bgcolor
			
		; dest.address calc
		mov		edi,[ebp+@y]
		shl		edi,6+3													; << 6 = x64
		mov		eax,edi
		shl		edi,2													; x256
		add		edi,eax													; +(x64)=x320

		mov		eax,[ebp+@x]
		shl		eax,3													; x8
		add		edi,eax													; edi=vram address
		add		edi,[_vptr]												; +memory base

		;	draw loop 
		mov		ecx,8
	.draw_loop:
		lodsb

		mov		bl,8
	.draw_loop_x:
		shl		al,1
		jc		.draw_bgcol

		mov		[edi],dh												; fgcolor 
		jmp		short .draw_next
	.draw_bgcol:
		mov		[edi],dl												; bgcolor 
	.draw_next:
		inc		edi
		dec		bl
		jnz		.draw_loop_x

		; 次の行へ
		add		edi,312
		loop	.draw_loop

		pop		edi
		pop		esi
		pop		ebx

		leave
		ret

;-------------------------------------------
;  MZの画面にテキスト文字をオーバーレイ表示
;  
;  In:	x = chr.X
;		y = chr.Y
;		code  = Display Code
;		color = Color Code
;(int x,int y,int code,int color)
;-------------------------------------------
		align	16
_mz_text_overlay:
		push	ebp
		mov		ebp,esp

		push	ebx
		push	esi
		push	edi

		; src.font address
		mov		esi,[ebp+@code]
		mov		eax,[ebp+@color]
		mov		edi,eax
		and		eax,0x80
		shl		eax,1
		or		esi,eax													; ecx=キャラクタコード 
		shl		esi,7
		add		esi,_fontbuf

		mov		ebx,_mzcol_palet
		; fg color	
		mov		eax,edi
		shr		eax,4
		and		eax,0x07
		mov		dl,[ebx+eax]											; dl=bgcolor 

		; dest.address calc
		mov		edi,[ebp+@y]
		shl		edi,6+3													; << 6 = x64 
		mov		eax,edi
		shl		edi,2													; x256 
		add		edi,eax													; +(x64)=x320 

		mov		eax,[ebp+@x]
		shl		eax,3													; x8 
		add		edi,eax													; edi=vram address 
		add		edi,[_vptr]												; +memory base 

		xor		ecx,ecx													; これが重要だった 
		;	draw loop
		mov		ebx,8
	.draw_loop:
		lodsb
		or		al,al
		je		.draw_nextline
			
		test	al,0x80
		jnz		.draw_1

		mov		cl,al													; bg(transparent) 
		add		edi,ecx	
		jmp		short .draw_loop

	.draw_1:
		and		al,0x0F
		mov		cl,al
		mov		al,dl													; fgcolor 
		rep		stosb
		jmp		short .draw_loop

		; 次の行へ
	.draw_nextline:
		add		edi,312
		dec		ebx
		jnz		.draw_loop

		pop		edi
		pop		esi
		pop		ebx

		leave
		ret

;----------------------------------
;  MZの画面にPCG1500文字を表示
;  
;  In:	x = chr.X
;		y = chr.Y
;		code  = Pcg Code
;		attr = Attr Code
;----------------------------------
;(int x,int y,int code,int attr)
		align	16
_mz_pcg1500_overlay:
		push	ebp
		mov		ebp,esp

		push	ebx
		push	esi
		push	edi

		; src.font address
		mov		esi,[ebp+@code]
		mov		eax,[ebp+@attr]
		and		eax,0xC0
		shl		eax,2
		or		esi,eax													; ecx=キャラクタコード 
		shl		esi,3													; x8 

		; dest.address calc 
		mov		edi,[ebp+@y]
		shl		edi,6+3													; << 6 = x64 
		mov		eax,edi
		shl		edi,2													; x256 
		add		edi,eax													; +(x64)=x320 

		mov		eax,[ebp+@x]
		shl		eax,3													; x8 
		add		edi,eax													; edi=vram address 
		add		edi,[_vptr]												; +memory base 

		;	draw loop 
		mov		ecx,8
	.draw_loop:
		mov		eax,esi
		mov		esi,[_pcg1500_font_blue]
		mov		dl,[eax + esi]	
		mov		esi,[_pcg1500_font_red]
		mov		bh,[eax + esi]	
		mov		esi,[_pcg1500_font_green]
		mov		dh,[eax + esi]	
		inc		eax
		mov		esi,eax

		mov		bl,8
	.draw_loop_x:
		xor		eax,eax
		shl		dh,1													; check green 
		adc		al,ah
		
		shl		bh,1													; check red 
		adc		al,al
		
		shl		dl,1													; check blue 
		adc		al,al
		
		; 透明背景チェック 
		or		al,al
		jz		short .draw_pcgnext

		mov		al,[_mzcol_palet+eax]
		mov		[edi],al												; bgcolor 
	.draw_pcgnext:
		inc		edi
		dec		bl
		jnz		.draw_loop_x

		; to next line 	
		add		edi,312
		loop	.draw_loop

		pop		edi
		pop		esi
		pop		ebx

		leave
		ret

;----------------------------------------------
;  MZの画面にPCG1500文字を表示
;  
;  In:	x = chr.X
;		y = chr.Y
;		code  = Pcg Code
;		attr = Attr Code
;		bgcolor = bgcolor
;(int x,int y,int code,int attr,int bgcolor)
;----------------------------------------------
		align	16
_mz_pcg1500_put:
		push	ebp
		mov		ebp,esp

		push	ebx
		push	esi
		push	edi

		; src.font address 
		mov		esi,[ebp+@code]
		mov		eax,[ebp+@attr]
		and		eax,0xC0
		shl		eax,2
		or		esi,eax													; ecx=キャラクタコード 
		shl		esi,3													; x8 

		; dest.address calc 
		mov		edi,[ebp+@y]
		shl		edi,6+3													; << 6 = x64 
		mov		eax,edi
		shl		edi,2													; x256 
		add		edi,eax													; +(x64)=x320 

		mov		eax,[ebp+@x]
		shl		eax,3													; x8 
		add		edi,eax													; edi=vram address 
		add		edi,[_vptr]												; +memory base 

		;	draw loop 
		mov		ecx,8
	.draw_loop:
		mov		eax,esi	
		mov		esi,[_pcg1500_font_blue]
		mov		dl,[eax + esi]	
		mov		esi,[_pcg1500_font_red]
		mov		bh,[eax + esi]	
		mov		esi,[_pcg1500_font_green]
		mov		dh,[eax + esi]	
		inc		eax
		mov		esi,eax	

		mov		bl,8
	.draw_loop_x:
		xor		eax,eax

		shl		dh,1													; check green 
		adc		al,ah
		
		shl		bh,1													; check red 
		adc		al,al
		
		shl		dl,1													; check blue 
		adc		al,al
		
		; 透明背景チェック 
		or		al,al
		jnz		short .draw_pcgnext

		mov		al,[ebp+@bgcolor]									; 背景色 
		and		al,0x0F
	.draw_pcgnext:
		mov		al,[_mzcol_palet+eax]								; フォント色 
		stosb														; dot 
		dec		bl
		jnz		.draw_loop_x

		; 次の行へ 	
		add		edi,312
		loop	.draw_loop

		pop		edi
		pop		esi
		pop		ebx

		leave
		ret

;--------------------------------------------------------------
;[EOF]
