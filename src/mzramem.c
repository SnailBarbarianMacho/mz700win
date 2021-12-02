//----------------------------------------------------------------------------
// File:MZramem.c
//
// mz700win:1Z-00007B (Japanese S-BASIC) / HuBASIC Cassete Emulation ....
// ($Id: mzramem.c 1 2007-07-27 06:17:19Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#define MZRAMEM_H_

#include "dprintf.h"
#include "mz700win.h"

#include "Z80.h"
#include "win.h"
#include "mzhw.h"
#include "mzmon1em.h"
#include "mzramem.h"

//-----------------
// RAM PATCH JUMPTBL
static void (* ram_functbl[])(Z80_Regs *) =
{
	MON_ret,					// $00
	//
	S_1Z007B_rdinf,				// $01
	S_1Z007B_rddata,			// $02
	S_1Z007B_wrinf,				// $03
	S_1Z007B_wrdata,			// $04
	S_HUBAS_rdinf,				// $05
	S_HUBAS_rddata,				// $06
	S_HUBAS_wrinf,				// $07
	S_HUBAS_wrdata,				// $08
};

void patchram_job(TMONEMU *pattbl)
{
	TMONEMU *ptbl;
	unsigned int ad,ad2;
	
	ptbl = pattbl;
	
	for (;;ptbl++)
	{
		ad = (unsigned int) ptbl->addr;
		if (ad == 0xFFFF) break;
		// if jump table........
		if (mem[RAM_START + ad] == 0xC3)
		{
			ad2 = mem[RAM_START + ad+1] | ( mem[RAM_START + ad+2] << 8);
//			dprintf("Jump Addr = $%04X\n",ad2);
			mem[RAM_START + ad2  ] = 0xED;
			mem[RAM_START + ad2+1] = 0xF4;
			mem[RAM_START + ad2+2] = (unsigned char) ptbl->code;
		}

		//
		mem[RAM_START + ad  ] = 0xED;
		mem[RAM_START + ad+1] = 0xF4;
		mem[RAM_START + ad+2] = (unsigned char) ptbl->code;
//		dprintf("Patch $%04x code=%02X\n",ad,ptbl->code);
	}

}

/*
    Patch RAM
 */
void patch_ram(unsigned char *buf)
{
	unsigned char *ptr = buf + HED_PAT_OFS;
	DWORD *sig = (DWORD *) ptr;
	unsigned int i,ofs;
	WORD addr;
	BYTE len;

	if (*sig != RAM_PATCH_SIG) return;		// パッチシグネチャチェック
	// ＯＫだったらシグネチャをつぶす
	(*ptr) = '\x0';
	ptr += 4;

	// パッチ当て
	for (;;)
	{
		addr = (WORD) (ptr[0] | (ptr[1] << 8) );	// パッチアドレスを取得
		if (addr == 0xFFFF) return;					// EOD

		len = *(ptr+2);								// パッチ長
		ptr += 3;

		ofs = RAM_START + addr;
		for (i=0; i<(unsigned int)len; i++)
			mem[ofs++] = *(ptr++);
	}

}

void exec_ram(Z80_Regs *Regs)
{
	int code;

	code = Z80_RDMEM(Regs->PC.W.l++);

//	dprintf("exec_ram(%d)\n",code);
	(*(ram_functbl[code]))(Regs);
}


//
//
//

/* RDINF */
void S_1Z007B_rdinf (Z80_Regs *Regs)
{
	 dprintf("S_1Z007B_rdinf()\n");

	rdinf_job(Regs, CPAT_RAM_1Z007B);
}

/* RDDATA */
void S_1Z007B_rddata (Z80_Regs *Regs)
{
	dprintf("S_1Z007B_rddata()\n");

	rddata_job(Regs, CPAT_RAM_1Z007B);
}

/* WRINF */
void S_1Z007B_wrinf (Z80_Regs *Regs)
{
	 dprintf("S_1Z007B_wrinf()\n");

	wrinf_job(Regs, CPAT_RAM_1Z007B);
}

/* WRDATA */
void S_1Z007B_wrdata (Z80_Regs *Regs)
{
	dprintf("S_1Z007B_wrdata()\n");

	wrdata_job(Regs, CPAT_RAM_1Z007B);
}



/* RDINF */
void S_HUBAS_rdinf(Z80_Regs *Regs)
{
	 dprintf("S_HUBAS_rdinf()\n");

	rdinf_job(Regs, CPAT_RAM_HUBASIC);
}

/* RDDATA */
void S_HUBAS_rddata(Z80_Regs *Regs)
{
	dprintf("S_HUBAS_rddata()\n");

	rddata_job(Regs, CPAT_RAM_HUBASIC);
}

/* WRINF */
void S_HUBAS_wrinf(Z80_Regs *Regs)
{
	 dprintf("S_HUBAS_wrinf()\n");

	wrinf_job(Regs, CPAT_RAM_HUBASIC);
}

/* WRDATA */
void S_HUBAS_wrdata(Z80_Regs *Regs)
{
	dprintf("S_HUBAS_wrdata()\n");

	wrdata_job(Regs, CPAT_RAM_HUBASIC);
}
