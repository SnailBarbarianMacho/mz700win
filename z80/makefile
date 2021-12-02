#### Makefile for Z80Em
# Following #defines can be used:
# DEBUG			Compile with builtin debugger
# LSB_FIRST		Compile for a low-endian machine
# X86_ASM		Use inline 80x86 assembly (GCC only)

CC	= wcc386	# C compiler used
LD	= wcl386	# Linker used

#CFLAGS	= -Wall -O2 -fomit-frame-pointer -DLSB_FIRST -DDEBUG
CFLAGS	= -5r -zk0 -ort -dLSB_FIRST -dINLINE=_inline -dDEBUG

all:    Z80.obj Z80Debug.obj z80dasm.exe

Z80Dasm.obj:  Z80Dasm.c
z80dasm.exe:    Z80Dasm.obj
	$(LD) -fe=z80dasm.exe Z80Dasm.obj

Z80Debug.obj: Z80Debug.c Z80.h Z80IO.h Z80Dasm.h
Z80.obj:      Z80.c Z80.h Z80Codes.h Z80IO.h Z80DAA.h

%.obj : %.c
	$(CC) $(CFLAGS) $<
