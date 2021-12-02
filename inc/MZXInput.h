//
// XInput.h by Takeshi Maruyama
// 2016/09/01-
// now digital pad only....
//

//#pragma once
// à⁄êAê´ÇÃÇΩÇﬂÅApragma once ÇÕégÇÌÇ»Ç¢Ç±Ç∆Ç…ÇµÇΩ

#ifndef __MZXINPUT_H__

#define __MZXINPUT_H__

//#define WIN32_LEAN_AND_MEAN             // Except the part which isn't used from windows header.
// Windows Header files:
//#include <windows.h>
#include <Xinput.h>

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

// Forward
BOOL XInput_Init();
BOOL XInput_Cleanup();
BOOL XInput_Update();

BOOL XI_Is_GamePad_Connected(int);
DWORD XI_Get_GamePad_RAW(int);

#ifdef __cplusplus
}
#endif //__cplusplus

// DEFINE
#define PAD_RAW_LEFT	0x00000001			// LEFT
#define PAD_RAW_RIGHT	0x00000002			// RIGHT
#define PAD_RAW_UP		0x00000004			// UP
#define PAD_RAW_DOWN	0x00000008			// DOWN

#define PAD_RAW_A		0x00000010			// A button
#define PAD_RAW_B		0x00000020			// B button
#define PAD_RAW_X		0x00000040			// X button
#define PAD_RAW_Y		0x00000080			// Y button
#define PAD_RAW_L		0x00000100			// L button
#define PAD_RAW_R		0x00000200			// R button
#define PAD_RAW_BACK	0x00000400			// BACK button
#define PAD_RAW_START	0x00000800			// START button

#endif // __MZXINPUT_H__
