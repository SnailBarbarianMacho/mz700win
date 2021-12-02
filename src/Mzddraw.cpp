//----------------------------------------------------------------------------
// File:MZddraw.cpp
//
// mz700win:DirectDraw Module
// ($Id: Mzddraw.cpp 1 2007-07-27 06:17:19Z maru $)
// Programmed by Takeshi Maruyama
//----------------------------------------------------------------------------

#include <windows.h>

#include "mz700win.h"
#include "win.h"
#include "MZddraw.h"
#include "MZscrn.h"

static TSCRSIZE fullscrsize[] =
{
	{ 320,200,  0,  0,320,200 },
	{ 320,240,  0, 20,320,200 },
	{ 640,400,  0,  0,640,400 },
	{ 640,480,  0, 40,640,400 },
	{ 800,600,  0, 40,640,400 }
	
};

#if USE_DDRAW

/* DirectDraw�\���� */
static LPDIRECTDRAW2           lpDD2 = NULL;           /* DirectDraw2�I�u�W�F�N�g */
static LPDIRECTDRAWSURFACE     lpFRONTBUFFER = NULL;   /* �\�o�b�t�@ */
static LPDIRECTDRAWSURFACE     lpBACKBUFFER = NULL;    /* ���o�b�t�@ */
static LPDIRECTDRAWSURFACE     lpBACKBUFFER2 = NULL;   /* ���o�b�t�@�Q */
static LPDIRECTDRAWSURFACE     lpBITMAP = NULL;        /* �p�^�[�� */
static LPDIRECTDRAWPALETTE     lpPALETTE = NULL;       /* �p���b�g */
static LPDIRECTDRAWCLIPPER     lpCLIP = NULL;			/* �N���b�v */

#endif //USE_DDRAW

static DWORD	nowsynctime;							/* ���݂̃^�C�}�[ */
static DWORD	nextsynctime;							/* ���t���b�v����^�C�}�[�l */

#if USE_DDRAW

/* DirectDraw���[�N */
static DDSURFACEDESC	ddsd;
static DDSCAPS			ddscaps;
static DDBLTFX		    ddbltfx;	
static DDBLTFX			ddbltfx_cls;

#endif //USE_DDRAW

/********************/
/* DirectDraw�̍쐬 */
/********************/
BOOL CreateDirectDraw(HWND hwnd)
{
#if USE_DDRAW
//	DDCAPS	DDDriverCaps;
//	DDCAPS	DDHELCaps;
	LPDIRECTDRAW lpDD = NULL;									/* DirectDraw�I�u�W�F�N�g */
	
    /* DirectDraw�̏����� */
    if (DirectDrawCreate(NULL, &lpDD, NULL) != DD_OK)
	{
    	MessageBox(hwnd, "DirectDraw initialize failed !",
				   "Error", MB_ICONERROR);
		return FALSE;
	}
	
	if (lpDD->QueryInterface(IID_IDirectDraw2,(LPVOID *)&lpDD2) != DD_OK)
	{
		if (lpDD != NULL) { lpDD->Release();lpDD=NULL; }
		return FALSE;
	}

	if (lpDD != NULL) { lpDD->Release();lpDD=NULL; }

	/* DDBLTFX�p���[�N�쐬 */
    ZeroMemory(&ddbltfx, sizeof(DDBLTFX));
    ddbltfx.dwSize = sizeof(DDBLTFX);

    ZeroMemory(&ddbltfx_cls, sizeof(DDBLTFX));							/* ClearScreen�p���[�N */
#endif

	/* �����^�C�}�[�̏����� */
	nowsynctime = get_timer();
	nextsynctime = nowsynctime + SyncTime;

	return TRUE;
}

/****************************************/
/* DirectDraw�̏������i�t���X�N���[���j */
/****************************************/
BOOL InitDirectDrawFull(HWND hwnd, int xsize, int ysize)
{
#if USE_DDRAW
	HRESULT ddchk;

	/* Set Exclusive */
    lpDD2->SetCooperativeLevel(hwnd,
                            DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN);
//                            DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT);
	
	ddchk=lpDD2->SetDisplayMode(xsize, ysize, 8, 0, 0);
	if (ddchk != DD_OK)
	{
		lpDD2->SetCooperativeLevel(hwnd, DDSCL_NORMAL);
		return FALSE;
	}
#endif // USE_DDRAW

	return MakeScreenSurface();
}

/************************************/
/* �t���X�N���[���p�T�[�t�F�X�̍쐬 */
/************************************/
BOOL MakeScreenSurface(void)
{
#if USE_DDRAW
	HRESULT ddchk;
	DDCAPS	DDDriverCaps;
	DDCAPS	DDHELCaps;

	/* DirectDraw�h���C�o�̔\�͂��f���� */
	ZeroMemory(&DDDriverCaps,sizeof(DDDriverCaps));
	DDDriverCaps.dwSize=sizeof(DDDriverCaps);
	ZeroMemory(&DDHELCaps,sizeof(DDHELCaps));
	DDHELCaps.dwSize=sizeof(DDHELCaps);
	
	lpDD2->GetCaps(&DDDriverCaps,&DDHELCaps);

    /* �\�o�b�t�@�̏����� */
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS|DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps =
        DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
	
	/* �u�q�`�l�e�ʂɂ����Back Buffer�̐������߂� */
    ddsd.dwBackBufferCount = 2;
	if (DDDriverCaps.dwVidMemFree <= 0x200000) ddsd.dwBackBufferCount--;

	/* �v���C�}���T�[�t�F�X�̍쐬 */
	lpDD2->CreateSurface(&ddsd, &lpFRONTBUFFER, NULL);

    /* ���o�b�t�@�̏����� */
    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
    ddchk = lpFRONTBUFFER->GetAttachedSurface(&ddscaps, &lpBACKBUFFER);
	if (ddchk != DD_OK)
	{
    	MessageBox(hwndApp, "GetAttachedSurface Error!",
				   "Error", MB_ICONERROR);
		return FALSE;
	}

    /* �p���b�g�̐������t�����g�o�b�t�@�Ɍ��ѕt����B */
	if (lpPALETTE) { lpPALETTE->Release(); lpPALETTE=NULL; }
    lpDD2->CreatePalette(DDPCAPS_8BIT, &LogicalPalette.aEntries[0], &lpPALETTE, NULL);
    lpFRONTBUFFER->SetPalette(lpPALETTE);

	/* �Ƃ肠�����T�[�t�F�X���N���A */
	ClearScreen();
#endif //USE_DDRAW	
	return TRUE;
}

/**********************************************************/
/* �X�v���C�g�T�[�t�F�X�ƃt�����g�o�b�t�@�ƃp���b�g��j�� */
/**********************************************************/
void ReleaseDirectDraw(void)
{
#if USE_DDRAW
////	if (scrnmode==SCRN_WINDOW) lpDD2->RestoreDisplayMode();
//	if (scrnmode==SCRN_WINDOW && lpCLIP!=NULL) lpFRONTBUFFER->SetClipper(0);

    if (lpCLIP != NULL) { lpCLIP->Release();lpCLIP=NULL; }
    if (lpPALETTE != NULL) { lpPALETTE->Release();lpPALETTE=NULL; }
    if (lpBITMAP != NULL) { lpBITMAP->Release();lpBITMAP = NULL; }
    if (lpFRONTBUFFER != NULL) { lpFRONTBUFFER->Release();lpFRONTBUFFER=NULL;lpBACKBUFFER=NULL;lpCLIP=NULL; }
#endif
}

/********************/
/* DirectDraw�̏I�� */
/********************/
void EndDirectDraw(void)
{
	HANDLE hProcess = GetCurrentProcess();
	
	// �E�B���h�E�ł���΁A�v���Z�X�̗D��x��ʏ��
	if (hProcess)
	{
		SetPriorityClass(hProcess, NORMAL_PRIORITY_CLASS);
	}
		
	ReleaseDirectDraw();												/* �X�v���C�g�T�[�t�F�X�ƃt�����g�o�b�t�@�ƃp���b�g��j�� */

#if USE_DDRAW
    if (lpDD2 != NULL) { lpDD2->Release();lpDD2=NULL; }
#endif
}

/**************/
/* ��ʂ̏��� */
/**************/
void ClearScreen(void)
{
#if USE_DDRAW
    ddbltfx_cls.dwSize = sizeof(DDBLTFX);

	lpFRONTBUFFER->Blt(NULL, NULL, NULL,
					  DDBLT_COLORFILL|DDBLT_WAIT, &ddbltfx_cls);
#endif
}

/****************/
/* �S��ʃ��[�h */
/****************/
int ChangeToFullScreen(void)
{
	int result = -1;
	int i;

	for (i=0;i<FULL_END;i++)
	{
		if (InitDirectDrawFull(hwndApp,fullscrsize[i].xscr,fullscrsize[i].yscr))
		{
			result = i;
			break;
		}
	}

	return result;
}



/***********************/
/* Redraw Window Frame */
/***********************/
void redraw_win_frame(HWND hwnd)
{
#if USE_DDRAW
	if (lpFRONTBUFFER != NULL)
	{
		lpDD2->FlipToGDISurface();
		DrawMenuBar(hwnd);
		RedrawWindow(hwnd, NULL, NULL, RDW_FRAME);
	}
#endif
}

/*******************************/
/* GDI��DirectDraw�̂��荇�킹 */
/*******************************/
void flip2gdi(void)
{
#if USE_DDRAW
	lpDD2->FlipToGDISurface();
#endif
}

/********************/
/* �E�B���h�E�ɕ`�� */
/********************/
void update_winscr(void)
{
	HDC hdc;
#if USE_DDRAW
	HRESULT ddchk;
	POINT pt;
	RECT destrect;
	TSCRSIZE *tscrp;
#endif

	if (get_scrnmode()==SCRN_WINDOW)
	{
		/* Normal Window */
		hdc = GetDC(hwndApp);
		if (hdc)
		{
			DrawScreen(hdc, menu.screen);
			ReleaseDC(hwndApp,hdc);
		}
	}
#if USE_DDRAW
	else
	{
		// Full Screen
		if (lpFRONTBUFFER==NULL) return;								/* BACKBUFFER���������Ȃ�return */
	
		/* Full-Screen */
		if (lpFRONTBUFFER->IsLost())
		{
			lpFRONTBUFFER->Release();
			lpFRONTBUFFER=NULL;
			
			if (MakeScreenSurface())
			{
				mz_refresh_screen(REFSC_ALL);
				// �v���Z�X�̗D��x��D��ɖ߂�
				HANDLE hProcess = GetCurrentProcess();
				if (hProcess)
				{
					SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
				}
			}
			else return;
		}

		GetClientRect(hwndApp,&destrect);								/* �����̃E�B���h�E�̋�` */
		pt.x = 0;
		pt.y = 0;
		ClientToScreen(hwndApp,&pt);
		OffsetRect(&destrect,pt.x,pt.y);								/* dest�E�B���h�E�̋�` */

		ddchk = lpFRONTBUFFER->GetDC(&hdc);
		if (ddchk == DD_OK)
		{
			tscrp = &fullscrsize[get_fullmode()];
			if (tscrp->xsiz == FORMWIDTH)
			{
				BitBlt(hdc,tscrp->xpos,tscrp->ypos,FORMWIDTH,FORMHEIGHT,Buffer,0,0,SRCCOPY);
			}
			else
			{
				StretchBlt(hdc,tscrp->xpos,tscrp->ypos,tscrp->xsiz,tscrp->ysiz,
						   Buffer,0,0,FORMWIDTH,FORMHEIGHT,SRCCOPY);

			}
			ddchk = lpFRONTBUFFER->ReleaseDC(hdc);
		}
		
	}
#endif //USE_DDRAW
	
}
