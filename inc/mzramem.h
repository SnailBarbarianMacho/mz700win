/* $Id: mzramem.h 1 2007-07-27 06:17:19Z maru $ */

#ifdef __cplusplus
extern "C" {
#endif

#define RAM_PATCH_SIG	':TAP'
#define HED_PAT_OFS		0x40
//
void patchram_job(TMONEMU *);
void patch_ram(unsigned char *buf);
void patch_emul_ram(void);

//
void S_1Z007B_rdinf(Z80_Regs *Regs);
void S_1Z007B_rddata(Z80_Regs *Regs);
void S_1Z007B_wrinf(Z80_Regs *Regs);
void S_1Z007B_wrdata(Z80_Regs *Regs);

void S_HUBAS_rdinf(Z80_Regs *Regs);
void S_HUBAS_rddata(Z80_Regs *Regs);
void S_HUBAS_wrinf(Z80_Regs *Regs);
void S_HUBAS_wrdata(Z80_Regs *Regs);

//	
#ifndef MZRAMEM_H_ 
#define MZRAMEM_H_
#endif

#ifdef __cplusplus
}
#endif 
