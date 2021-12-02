; 
; RAMDISK DUMPER
; 

	ORG 1200h
	
GETL	EQU		0003H
LETNL	EQU		0006H
PRNT	EQU		0012H
MSG		EQU		0015H
QDPCT	EQU		0DDCH
QQKEY	EQU		09B3H
QQADCN	EQU		0BB9H
QQDACN	EQU		0BCEH

START:
ENTRY:
	; CLS
	LD		A,0C6H
	CALL	QDPCT

INP_LOOP:
	; BANK?
	LD		DE,MSG_BANK
	CALL	MSG
	
	CALL	QQKEY
	CALL	QQDACN
	
	CP		030H
	JR		NZ,@f

	LD		B,00H			; ��ʃA�h���X�ݒ�
	JR		RAM_TRANS
	
@@:
	CP		031H
	JR		NZ,@f
	
	LD		B,80H			; ��ʃA�h���X�ݒ�
	JR		RAM_TRANS
	
@@:
	; ���s���ă��g���C
	CALL	LETNL
	JR		INP_LOOP
	
RAM_TRANS:
	; �A�X�L�[�R�[�h��\��
	CALL	PRNT
	CALL	LETNL
	
	;------------------------------------------------
	; �q�`�l�t�@�C������$8000�]��
	;------------------------------------------------
	; �ǂݏo���A�h���X�ݒ�
	LD		A,00H	; L
;	LD		B,00H	; H
	LD		C,0EBh
	OUT		(C),A

	LD		BC,08000H		; �]���T�C�Y
	LD		HL,02000H		; ���C���q�`�l
@@:	
	IN		A,(0EAH)
	LD		(HL),A
	INC		HL
	DEC		BC
	LD		A,C
	OR		B
	JR		NZ,@b

	; �Z�[�u�𑣂����b�Z�[�W��\��
	LD		DE,MSEG_END
	CALL	MSG
	CALL	LETNL

	; ���j�^�[���[�`����
	JP		0EAA7H
	
MSG_BANK:
	DB		'BANK?(0-1)'
	DB		0Dh

MSEG_END:
	DB		'SAVE $2000-$9FFF. EXC.ADRS=$EAA7'
	DB		0Dh

WORK:

	; �s���̓o�b�t�@
LINEBUF:
	
	END
