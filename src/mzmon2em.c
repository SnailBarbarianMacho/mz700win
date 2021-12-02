//----------------------------------------------------------------------------
// File:MZmonem2.c
//
// mz700win:9Z-502M (for MZ-1500) Emulation / Patch ....
// ($Id: mzmon2em.c 2 2007-07-27 06:58:01Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#define MZMON2EM_H_

#include <stdio.h>
#include <windows.h>

#include "mz700win.h"
#include "dprintf.h"
#include "fileio.h"
#include "Z80.h"
#include "win.h"
#include "fileset.h"

#include "mzhw.h"
#include "mzmon2em.h"
#include "mzramem.h"
#include "mzscrn.h"

static unsigned char qdios_buf[128];

static int qdios_idx_bak = -1;
static int qdpos = 0;													// ＱＤシーク位置
static FILE_HDL qfp = FILE_VAL_ERROR;									// ＱＤ用ファイルハンドル

// ROM2 PATCH JUMPTBL
static void (* rom2_functbl[])(Z80_Regs *) =
{
	MON_psgrs,				//
	MON_palres,				//
	MON_qdios,				//
	MON_bas_qdios,			//
};

// ROM2 PATCH TABLE (PSG/PALET)
static TMONEMU rom2_pattbl_norom[] =
{
	{ PAT_ADR_PSGRS0, ROM2_psgrs },
	{ PAT_ADR_PSGRS, ROM2_psgrs },
	{ PAT_ADR_PALRS0, ROM2_palres },
	{ PAT_ADR_PALRES, ROM2_palres },

	{ 0xFFFF, ROM2_end }										/* eod */
};

// ROM2 PATCH TABLE (QD)
static TMONEMU rom2_pattbl_qd[] =
{
	{ PAT_ADR_QDIOS, ROM2_qdios },
	{ PAT_ADR_QDIO, ROM2_qdios },

	{ 0xFFFF, ROM2_end }										/* eod */
};

// QBT:
static const byte qbt_code[] =
{
	0xD3,0xE2,					// QBT:		OUT ($E2),A		; $0000-$0FFF:ROM1
	0xC3,0x00,0x00,				//			JP  $0000
};


/*
    Patch for ROM2 Emulation
 */
void patch_emul_rom2(void)
{
	dprintf("patch_emul_rom2()\n");

	FillMemory(mem + ROM2_START,0x1800, 0xC9);							// Fill by 'RET'
	CopyMemory(mem + ROM2_START + (PAT_ADR_QBT - 0xE800) ,qbt_code, sizeof(qbt_code) );

	// QL -> Reset
	FillMemory(mem + ROM2_START + (PAT_ADR_QL - 0xE800) , (PAT_ADR_EF14-PAT_ADR_QL+1), 0x00);							// Fill by 'RET'
	CopyMemory(mem + ROM2_START + (PAT_ADR_EF14 - 0xE800) ,qbt_code, sizeof(qbt_code) );

	patch2_job(rom2_pattbl_norom);										// PAL & PSG INIT
	patch2_job(rom2_pattbl_qd);											// QD ACCESS
}



void patch2_job(TMONEMU *pattbl)
{
	TMONEMU *ptbl;
	unsigned int ad,ad2;
	
	ptbl = pattbl;

	for (;;ptbl++)
	{
		ad = (unsigned int) ptbl->addr;
		if (ad == 0xFFFF) break;
		if (ad >= ROM2_ADDR)
			ad = (ad - ROM2_ADDR) + ROM2_START;
		// if jump table........
		if (mem[ad] == 0xC3)
		{
			ad2 = mem[ad+1] | ( mem[ad+2] << 8);
			if (ad2 >=ROM2_ADDR)
			{
				ad2 = (ad2 - ROM2_ADDR) + ROM2_START;
			}
			mem[ad2  ] = 0xED;
			mem[ad2+1] = 0xF2;
			mem[ad2+2] = (unsigned char) ptbl->code;
		}

		//
		mem[ad  ] = 0xED;
		mem[ad+1] = 0xF2;
		mem[ad+2] = (unsigned char) ptbl->code;
//		dprintf("Patch $%04x code=%02X\n",ad,ptbl->code);
	}

}

/*
    Patch ROM2
 */
void patch_rom2(void)
{
	patch2_job(rom2_pattbl_qd);
}

void exec_rom2(Z80_Regs *Regs)
{
	int code;

	code = Z80_RDMEM(Regs->PC.W.l++);

	(*(rom2_functbl[code]))(Regs);
}



// PSG RESET
void MON_psgrs(Z80_Regs *Regs)
{
	int i;
	byte a;
	dprintf("PSG Reset\n");

	a = 0x9F;
	for (i=0;i<4;i++)
	{
		Z80_Out(0xE9,a);
		a += 0x20;
	}

	ret();
}

// PALET Reset
void MON_palres(Z80_Regs *Regs)
{
	int i;
	byte a;
	dprintf("PAL Reset\n");

	a = 0;

	for (i=0;i<8;i++)
	{
		Z80_Out(0xF1,a);
		a += 0x11;
	}

	ret();
}

// PCG Reset
void MON_pcgres(Z80_Regs *Regs)
{
	dprintf("PCG Reset\n");

	ZeroMemory(pcg1500_font_blue,PCG1500_SIZE);
	ZeroMemory(pcg1500_font_red,PCG1500_SIZE);
	ZeroMemory(pcg1500_font_green,PCG1500_SIZE);

	ret();
}

//////////////////////////////////////////////////////////////////////////
// 9Z-502M用 QDIOS
//////////////////////////////////////////////////////////////////////////
void MON_qdios(Z80_Regs *Regs)
{
	int cmd = (int) Regs->AF.B.h;
	int sz,ex,r;
	int ofs,a;
	FILE_HDL fh;
    char drive[ _MAX_DRIVE ];
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];

	cmd = mem[RAM_START+L_QDPA];
	dprintf("QDIOS:");
	dprintf("ACC=%d\n", cmd);
	
	// $01:Ready Check
	// $02:Format
	// $03:Read
	// $04:Write
	// $05:Head Point Clear
	// others:Motor Off

	Z80_set_carry(Regs, 0);								// NO error

#if _DEBUG
		dprintf("QDPA = $%02X / QDPB = $%02X\n",mem[RAM_START+L_QDPA],mem[RAM_START+L_QDPB]);
		dprintf("QDPC = $%04X / QDPE = $%04X\n",mem[RAM_START+L_QDPC]|(mem[RAM_START+L_QDPC+1]*256),
				mem[RAM_START+L_QDPE]|(mem[RAM_START+L_QDPE+1]*256) );
		dprintf("QDPG = $%04X / QDPI = $%04X\n",mem[RAM_START+L_QDPG]|(mem[RAM_START+L_QDPG+1]*256),
				mem[RAM_START+L_QDPI]|(mem[RAM_START+L_QDPI+1]*256) );
#endif

	mem[RAM_START+L_RETSP  ] = Regs->SP.B.l;
	mem[RAM_START+L_RETSP+1] = Regs->SP.B.h;

	switch (cmd)
	{
	case 01:		// ready check
		if (!FileExists(qdfile))
		{
			Regs->AF.B.h = 50;							// not ready
			Z80_set_carry(Regs,1);						// error
			break;
		}

		if (mem[RAM_START+L_QDPB])
		{
			// write protect check

			// Overwrite filename
			_splitpath_s(qdfile, drive, sizeof(drive), dir, sizeof(dir), fname, sizeof(fname), ext, sizeof(ext) );
			if ( lstrcmpi("$qd$", fname) )
			{
				Regs->AF.B.h = 46;							// write protect
				Z80_set_carry(Regs,1);						// error
				break;
			}
		}
		Regs->AF.B.h = 0;							// no error
		break;

	case 02:		// $02:Format
		fh = FILE_COPEN(qdfile);
		FILE_CLOSE(fh);

		if (fh == FILE_VAL_ERROR)
		{
			Regs->AF.B.h = 41;								// Hard Error
			Z80_set_carry(Regs,1);
		}
		else
			Regs->AF.B.h = 0;								// no error
		break;

	
	case 03:		// $03:Read
		// QDPA=$03
		// QDPB=IDX
		// QDPE=SIZE
		// QDPC=ADDR
		if (qfp == FILE_VAL_ERROR)
		{
			qfp = FILE_ROPEN(qdfile);
			if (qfp != FILE_VAL_ERROR)
				FILE_SEEK(qfp,qdpos,FILE_SEEK_SET);
		}

		if (qfp != FILE_VAL_ERROR)
		{
			a = mem[RAM_START+L_QDPB];
			if (!a)
			{
				//////////////////
				// 00:read HEADER
				//////////////////
				if (qdios_idx_bak == a)
				{
					sz = (qdios_buf[18]|(qdios_buf[19]<<8));
					FILE_SEEK(qfp,sz,FILE_SEEK_CUR);			/* SKIP data */
				}
				else
				{
					FILE_SEEK(qfp,qdpos,FILE_SEEK_SET);		/*  */
				}
				sz = mem[RAM_START+L_QDPE]|(mem[RAM_START+L_QDPE+1]<<8);
				ofs = mem[RAM_START+L_QDPC]|(mem[RAM_START+L_QDPC+1]<<8);
//				if (sz == 0x40 && ofs == 0x10F0)
				if (sz == 0x40)
				{
					/* read header 64bytes (CMT->QD) convert */
					r = FILE_READ(qfp,qdios_buf,128);
					if (!r)
					{
						FILE_CLOSE(qfp);
						qfp=FILE_VAL_ERROR;
						Regs->AF.B.h = 0x28;								// not found
						Z80_set_carry(Regs, 1);								// error
						break;
					}
					CopyMemory(mem+RAM_START+ofs,qdios_buf,18);		// copy header
					mem[RAM_START+L_QSIZE  ]=qdios_buf[18];
					mem[RAM_START+L_QSIZE+1]=qdios_buf[19];
					
					mem[RAM_START+L_QDTADR  ]=qdios_buf[20];
					mem[RAM_START+L_QDTADR+1]=qdios_buf[21];
					mem[RAM_START+L_QEXADR  ]=qdios_buf[22];
					mem[RAM_START+L_QEXADR+1]=qdios_buf[23];
	#if _DEBUG
					dprintf("QSIZE = $%04X / QDTADR = $%04X / QEXADR = $%04X\n",
						mem[RAM_START+L_QSIZE]|(mem[RAM_START+L_QSIZE+1]<<8),
						mem[RAM_START+L_QDTADR]|(mem[RAM_START+L_QDTADR+1]<<8),
						mem[RAM_START+L_QEXADR]|(mem[RAM_START+L_QEXADR+1]<<8) );
	#endif
					sz=128;
				}
				else
				{
					/* read data (normal) */
					ofs = mem[RAM_START+L_QDPC]|(mem[RAM_START+L_QDPC+1]<<8);
					ex = mem[RAM_START+L_QDPE]|(mem[RAM_START+L_QDPE+1]<<8);
					sz = (qdios_buf[18]|(qdios_buf[19]<<8));

					r = FILE_READ(qfp,mem+RAM_START+ofs,sz);		/* read data */
				}
				mem[RAM_START+L_HDPT]++;

			}
			else
			{
				////////////////
				// 01:read DATA
				////////////////
				ofs = mem[RAM_START+L_QDPC]|(mem[RAM_START+L_QDPC+1]<<8);
				ex = mem[RAM_START+L_QDPE]|(mem[RAM_START+L_QDPE+1]<<8);
				sz = (qdios_buf[18]|(qdios_buf[19]<<8));

				r = FILE_READ(qfp,mem+RAM_START+ofs,sz);		/* read data */
				// patch
				patch_ram(qdios_buf);
			}
			qdpos = FILE_SEEK(qfp,0,FILE_SEEK_CUR);
			qdios_idx_bak = a;
		}

		dprintf("qdpos=%d r=%d\n",qdpos,r);
		 
		if (qfp==FILE_VAL_ERROR || (r!=sz))
		{
			qdios_idx_bak = -1;
			dprintf("seek error\n");
			/* シークエラー */
			Z80_set_carry(Regs, 1);								// error
			Regs->AF.B.h = 0x40;								// hard err
//			Regs->AF.B.h = 0x28;								// not found
		}

		if (qfp)
		{
			FILE_CLOSE(qfp);
			qfp=FILE_VAL_ERROR;
		}

		Regs->AF.B.h = 0;										// no error
		break;

	case 04:		// $04:Write
		// QDPA=$04 QDPB=??
		// QDPE=SIZE QDPC=ADDR (HEAD)
		// QDPG=TOP  QDPI=SIZE (DATA)

		/// HEAD ////
		fh = FILE_AOPEN(qdfile);
		if (fh == FILE_VAL_ERROR)
		{
			Regs->AF.B.h = 0x40;								// hard err
			Z80_set_carry(Regs, 1);								// error
			break;
		}

		ofs = mem[RAM_START+L_QDPC]|(mem[RAM_START+L_QDPC+1]<<8);
		sz = mem[RAM_START+L_QDPE]|(mem[RAM_START+L_QDPE+1]<<8);

		// QDPC == 0x10F0, DQPE == 0x0040 の時、コンバート。
		CopyMemory(qdios_buf,mem+RAM_START+ofs,18);				// copy header
		qdios_buf[18]=mem[RAM_START+L_QSIZE  ];
		qdios_buf[19]=mem[RAM_START+L_QSIZE+1];
		qdios_buf[20]=mem[RAM_START+L_QDTADR  ];
		qdios_buf[21]=mem[RAM_START+L_QDTADR+1];
		qdios_buf[22]=mem[RAM_START+L_QEXADR  ];
		qdios_buf[23]=mem[RAM_START+L_QEXADR+1];
		r = FILE_WRITE(fh,qdios_buf,128);

		/// DATA ////
		ofs = mem[RAM_START+L_QDPG]|(mem[RAM_START+L_QDPG+1]<<8);
		sz = mem[RAM_START+L_QDPI]|(mem[RAM_START+L_QDPI+1]<<8);
		r = FILE_WRITE(fh,mem+RAM_START+ofs,sz);
		FILE_CLOSE(fh);

		Regs->AF.B.h = 0;									// no error
		break;

	case 05:		// $05:Head Point Clear
		Z80_WRMEM(L_HDPT,0);
		ZeroMemory(qdios_buf,sizeof(qdios_buf));
		qdios_idx_bak = -1;
 		qdpos = 0;
		FILE_SEEK(qfp,qdpos,FILE_SEEK_SET);					// REW POINT
		Regs->AF.B.h = 0;									// no error
		break;

	default:;
		Regs->AF.B.h = 0;									// no error
	}
	
	ret();
}

//////////////////////////////////////////////////////////////////////////
// 5Z001用 QDIOS
//////////////////////////////////////////////////////////////////////////
void MON_bas_qdios(Z80_Regs *Regs)
{
	int cmd = (int) Regs->AF.B.h;
	int sz,ex,r;
	int ofs,a;
	int ftype;
	BYTE *ptr;
	FILE_HDL fh;
    char drive[ _MAX_DRIVE ];
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];

	cmd = mem[RAM_START+BL_QDPA];
	dprintf("QDIOS:");
	dprintf("ACC=%d\n", cmd);

	
	// $01:Ready Check
	// $02:Format
	// $03:Read
	// $04:Write
	// $05:Head Point Clear
	// others:Motor Off


#if _DEBUG
		dprintf("QDPA = $%02X / QDPB = $%02X\n",mem[RAM_START+BL_QDPA],mem[RAM_START+BL_QDPB]);
		dprintf("QDPC = $%04X / QDPE = $%04X\n",mem[RAM_START+BL_QDPC]|(mem[RAM_START+BL_QDPC+1]*256),
				mem[RAM_START+BL_QDPE]|(mem[RAM_START+BL_QDPE+1]*256) );
		dprintf("QDPG = $%04X / QDPI = $%04X\n",mem[RAM_START+BL_QDPG]|(mem[RAM_START+BL_QDPG+1]*256),
				mem[RAM_START+BL_QDPI]|(mem[RAM_START+BL_QDPI+1]*256) );
#endif

//	mem[RAM_START+BL_RETSP  ] = Regs->SP.B.l;
//	mem[RAM_START+BL_RETSP+1] = Regs->SP.B.h;

	switch (cmd)
	{
	case 01:		// ready check
		Z80_set_carry(Regs, 0);							// NO error

		if (!FileExists(qdfile))
		{
			Regs->AF.B.h = 50;							// not ready
			Z80_set_carry(Regs,1);						// error
			break;
		}

		if (mem[RAM_START+BL_QDPB])
		{
			// write protect check

			// Overwrite filename
			_splitpath_s(qdfile, drive, sizeof(drive), dir, sizeof(dir), fname, sizeof(fname), ext, sizeof(ext) );
			if ( lstrcmpi("$qd$", fname) )
			{
				Regs->AF.B.h = 46;							// write protect
				Z80_set_carry(Regs,1);						// error
				break;
			}
		}
		Regs->AF.B.h = 0;							// no error
		break;

	case 02:		// $02:Format
		Z80_set_carry(Regs, 0);							// NO error

		fh = FILE_COPEN(qdfile);
		FILE_CLOSE(fh);

		if (fh == FILE_VAL_ERROR)
		{
			Regs->AF.B.h = 41;								// Hard Error
			Z80_set_carry(Regs,1);
		}
		else
			Regs->AF.B.h = 0;								// no error
		break;

	
	case 03:		// $03:Read
		Z80_set_carry(Regs, 0);							// NO error

		// QDPA=$03
		// QDPB=IDX
		// QDPE=SIZE
		// QDPC=ADDR
		if (qfp == FILE_VAL_ERROR)
		{
			qfp = FILE_ROPEN(qdfile);
			if (qfp != FILE_VAL_ERROR)
				FILE_SEEK(qfp,qdpos,FILE_SEEK_SET);
		}

		if (qfp != FILE_VAL_ERROR)
		{
			a = mem[RAM_START+BL_QDPB];
			if (!a)
			{
				//////////////////
				// 00:read HEADER
				//////////////////
				if (qdios_idx_bak == a)
				{
					sz = (qdios_buf[18]|(qdios_buf[19]<<8));
					FILE_SEEK(qfp,sz,FILE_SEEK_CUR);			/* SKIP data */
				}
				else
				{
					FILE_SEEK(qfp,qdpos,FILE_SEEK_SET);		/*  */
				}
				sz = mem[RAM_START+BL_QDPE]|(mem[RAM_START+BL_QDPE+1]<<8);
				ofs = mem[RAM_START+BL_QDPC]|(mem[RAM_START+BL_QDPC+1]<<8);
				if (sz == 0x40)
				{
					/* read header 64bytes (CMT->QD) convert */
					r = FILE_READ(qfp,qdios_buf,128);
					if (!r)
					{
						FILE_CLOSE(qfp);
						qfp=FILE_VAL_ERROR;
						Regs->AF.B.h = 0x28;								// not found
						Z80_set_carry(Regs, 1);								// error
						break;
					}

					// ofs=11a4, 2000?
					CopyMemory(mem+RAM_START+ofs,qdios_buf,18);		// copy header
					ptr = mem+RAM_START+ofs;
					ptr[20] = qdios_buf[18];						// Set File Size
					ptr[21] = qdios_buf[19];
					ptr[22] = qdios_buf[20];						// Set Data Address
					ptr[23] = qdios_buf[21];
					ptr[24] = qdios_buf[22];						// Set Exec Address
					ptr[25] = qdios_buf[23];
					ptr[18] = 0;									// ロック？何かに使ってるらしい？
					ptr[19] = 0;									// システムID?何かに使ってるらしい？
					CopyMemory(mem+RAM_START+BL_QSIZE, ptr+20, 6);

	#if _DEBUG
					dprintf("QSIZE = $%04X / QDTADR = $%04X / QEXADR = $%04X\n",
						mem[RAM_START+BL_QSIZE]|(mem[RAM_START+BL_QSIZE+1]<<8),
						mem[RAM_START+BL_QDTADR]|(mem[RAM_START+BL_QDTADR+1]<<8),
						mem[RAM_START+BL_QEXADR]|(mem[RAM_START+BL_QEXADR+1]<<8) );
	#endif
					sz=128;
				}
				else
				{
					/* read data (normal) */
					ofs = mem[RAM_START+BL_QDPC]|(mem[RAM_START+BL_QDPC+1]<<8);
					ex = mem[RAM_START+BL_QDPE]|(mem[RAM_START+BL_QDPE+1]<<8);
//					sz = (qdios_buf[18]|(qdios_buf[19]<<8));
					sz = (mem[RAM_START+BL_QSIZE]|(mem[RAM_START+BL_QSIZE+1]<<8));

					r = FILE_READ(qfp,mem+RAM_START+ofs,sz);		/* read data */
				}
				mem[RAM_START+BL_HDPT]++;

			}
			else
			{
				////////////////
				// 01:read DATA
				////////////////
				ofs = mem[RAM_START+BL_QDPC]|(mem[RAM_START+BL_QDPC+1]<<8);
				ex = mem[RAM_START+BL_QDPE]|(mem[RAM_START+BL_QDPE+1]<<8);
				sz = (qdios_buf[18]|(qdios_buf[19]<<8));

				r = FILE_READ(qfp,mem+RAM_START+ofs,sz);		/* read data */
			}
			qdpos = FILE_SEEK(qfp,0,FILE_SEEK_CUR);
			qdios_idx_bak = a;
		}

		dprintf("qdpos=%d r=%d\n",qdpos,r);
		 
		if (qfp==FILE_VAL_ERROR || (r!=sz))
		{
			qdios_idx_bak = -1;
			dprintf("seek error\n");
			/* シークエラー */
			Z80_set_carry(Regs, 1);								// error
			Regs->AF.B.h = 0x40;								// hard err
//			Regs->AF.B.h = 0x28;								// not found
		}

		if (qfp)
		{
			FILE_CLOSE(qfp);
			qfp=FILE_VAL_ERROR;
		}

		Regs->AF.B.h = 0;										// no error
		break;

	case 04:		// $04:Write
		Z80_set_carry(Regs, 0);							// NO error
		// QDPA=$04 QDPB=??
		// QDPE=SIZE QDPC=ADDR (HEAD)
		// QDPG=TOP  QDPI=SIZE (DATA)

		/// HEAD ////
		fh = FILE_AOPEN(qdfile);
		if (fh == FILE_VAL_ERROR)
		{
			Regs->AF.B.h = 0x40;								// hard err
			Z80_set_carry(Regs, 1);								// error
			break;
		}

		ofs = mem[RAM_START+BL_QDPC]|(mem[RAM_START+BL_QDPC+1]<<8);
		sz = mem[RAM_START+BL_QDPE]|(mem[RAM_START+BL_QDPE+1]<<8);

		// QDPC == 0x10F0, DQPE == 0x0040 の時、コンバート。
		CopyMemory(qdios_buf,mem+RAM_START+ofs,18);				// copy header
		ftype = (int) qdios_buf[0] & 0xFF; 

		qdios_buf[18]=mem[RAM_START+BL_QDPI  ];
		qdios_buf[19]=mem[RAM_START+BL_QDPI+1];
		qdios_buf[20]=mem[RAM_START+BL_QDPG  ];
		qdios_buf[21]=mem[RAM_START+BL_QDPG+1];
		qdios_buf[22]=mem[RAM_START+BL_QEXADR  ];
		qdios_buf[23]=mem[RAM_START+BL_QEXADR+1];
		r = FILE_WRITE(fh,qdios_buf,128);

		/// DATA ////
		ofs = mem[RAM_START+BL_QDPG]|(mem[RAM_START+BL_QDPG+1]<<8);
		sz = mem[RAM_START+BL_QDPI]|(mem[RAM_START+BL_QDPI+1]<<8);
		r = FILE_WRITE(fh,mem+RAM_START+ofs,sz);

		FILE_CLOSE(fh);

		Regs->AF.B.h = 0;									// no error
		break;

	case 05:		// $05:Head Point Clear
		Z80_set_carry(Regs, 0);							// NO error

		Z80_WRMEM(BL_HDPT,0);
		ZeroMemory(qdios_buf,sizeof(qdios_buf));
		qdios_idx_bak = -1;
 		qdpos = 0;
		FILE_SEEK(qfp,qdpos,FILE_SEEK_SET);					// REW POINT
		Regs->AF.B.h = 0;									// no error
		break;

	case 06:		// $06 Motor stop:
		mem[RAM_START+BL_QDPB] = 0;
		mem[RAM_START+BL_FNUPF] = 0;
		break;

	default:;
		break;
	}
	
	ret();
}

