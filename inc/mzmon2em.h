/* $Id: mzmon2em.h 1 2007-07-27 06:17:19Z maru $ */

#ifdef __cplusplus
extern "C" {
#endif

// パッチインデックス
enum TROM2NUM
{
	ROM2_psgrs = 0,
	ROM2_palres,
	ROM2_qdios,
	ROM2_bas_qdios,
	
	ROM2_end = -1
};

//----------------------
void patch2_job(TMONEMU *);
void patch_rom2(void);
void patch_emul_rom2(void);


void MON_psgrs(Z80_Regs *Regs);
void MON_palres(Z80_Regs *Regs);
void MON_pcgres(Z80_Regs *Regs);
	
void MON_qdios(Z80_Regs *Regs);
void MON_qbt(Z80_Regs *Regs);
void MON_bas_qdios(Z80_Regs *Regs);

// for 9Z-502M
#define PAT_ADR_PSGRS0			0xE807
#define PAT_ADR_PSGRS			0xE82F
#define PAT_ADR_PALRS0			0xE80A
#define PAT_ADR_PALRES			0xE881

#define PAT_ADR_QDIOS			0xE80D
#define PAT_ADR_QDIO			0xFA00
#define PAT_ADR_QMEIN			0xFA54
#define PAT_ADR_QBT				0xEA0E
#define PAT_ADR_QL				0xEF06
#define PAT_ADR_EF14			0xEF14

// for 9Z-502M
#define L_NAME	0x10F1			// FILE NAME
#define L_QDPA	0x1130			// IOCS COMMAND
#define L_QDPB	0x1131			// IOCS PARAM
#define L_QDPC	0x1132			// DATA HEAD ADRS(1)
#define L_QDPE	0x1134			// DATA BYTE SIZE(2)
#define L_QDPG	0x1136			// DATA HEAD ADRS(2)
#define L_QDPI	0x1138			// DATA BYTE SIZE(2)
#define L_QDCPB	0x113B
#define L_HDPT	0x113D
#define L_SIZE 0x1102
#define L_QSIZE 0x1104
#define L_QDTADR 0x1106
#define L_QEXADR 0x1108
#define L_RETSP	0x1148

// for BASIC MZ-5Z001
#define BL_QDPA	0x2C53			// IOCS COMMAND
#define BL_QDPB	0x2C54			// IOCS PARAM
#define BL_QDPC	0x2C55			// DATA HEAD ADRS(1)
#define BL_QDPE	0x2C57			// DATA BYTE SIZE(2)
#define BL_QDPG	0x2C59			// DATA HEAD ADRS(2)
#define BL_QDPI	0x2C5B			// DATA BYTE SIZE(2)
#define BL_HDPT	0x2C5D


#define BL_FNUPF 0x2C61
#define BL_FNA 0x2C62
#define BL_FNUPS 0x2C5F

#define BL_RETSP	0x2C67
#define BL_RTYF	0x2C65


#define BL_NAME	0x1001			// FILE NAME
//#define BL_SIZE 0x1102
#define BL_QSIZE 0x1014
#define BL_QDTADR 0x1016
#define BL_QEXADR 0x1018


	
//	
#ifndef MZMONEM2_H_ 
#define MZMONEM2_H_


	
#endif

#ifdef __cplusplus
}
#endif 
	
