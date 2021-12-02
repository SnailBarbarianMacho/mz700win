// Windows Header files:
#define WIN32_LEAN_AND_MEAN             // Except the part which isn't used from windows header.
#include <windows.h>

#include "dprintf.h"
#include "MZXInput.h"

#define MAX_CONTROLLERS  4		// XInputが認識できるのは4つまで
#define	Threshold   65535/4		// しきい値

//
typedef struct _CONTROLER_STATE
{
	XINPUT_STATE state;
	bool bConnected;
} CONTROLER_STATE, *pCONTROLER_STATE;

// ワークエリア
static CONTROLER_STATE GAME_PAD[MAX_CONTROLLERS];

/*
* 接続されているか調べつつ、コントローラーの状態を取得する
*/
static HRESULT UpdateControllerState()
{
	DWORD dwResult = 0;
	pCONTROLER_STATE pC;

	for (DWORD i = 0; i < MAX_CONTROLLERS; i++)
	{
		pC = &GAME_PAD[i];

		// Simply get the state of the controller from XInput.
		dwResult = XInputGetState(i, &(pC->state) );

		if (dwResult == ERROR_SUCCESS)
			pC->bConnected = true;
		else
			pC->bConnected = false;
	}

	return S_OK;
}
/*
 * コンストラクタ
 */
BOOL XInput_Init()
{
	dprintf(TEXT("XInput_Init()\n"));

	memset(GAME_PAD, 0, sizeof(CONTROLER_STATE) * MAX_CONTROLLERS);

	return TRUE;
}

/*
 * デストラクタ
 */
BOOL XInput_Cleanup()
{
	dprintf(TEXT("XInput_Cleanup()\n"));

	return TRUE;
}

/*
 *  更新
 */
BOOL XInput_Update()
{
	HRESULT result; 

	result = UpdateControllerState();

	return TRUE;
}

/*
* ゲームパッドが接続されているか
*
* no = GamePad 0-3
*/
BOOL XI_Is_GamePad_Connected(int no)
{
	BOOL result = FALSE;
	pCONTROLER_STATE pC;

	if (no >= 0 && no < MAX_CONTROLLERS)
	{
		pC = &GAME_PAD[no];
		if (pC->bConnected)
			result = TRUE;
	}

	return result;
}

/*
 * ゲームパッドの生入力情報取得
 *
 * no = GamePad 0-3
 */
DWORD XI_Get_GamePad_RAW(int no)
{
	DWORD result = 0;
	pCONTROLER_STATE pC;
	XINPUT_GAMEPAD * pGamepad;
	WORD buttons;

	if (no >= 0 && no < MAX_CONTROLLERS)
	{
		pC = &GAME_PAD[no];
		if (!pC->bConnected)		// 接続されていない
			return result;

		pGamepad = &pC->state.Gamepad;

		// Zero value if thumbsticks are within the dead zone 
		if ((pGamepad->sThumbLX < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
			pGamepad->sThumbLX > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) &&
			(pGamepad->sThumbLY < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
				pGamepad->sThumbLY > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE))
		{
			pGamepad->sThumbLX = 0;
			pGamepad->sThumbLY = 0;
		}

		if ((pGamepad->sThumbRX < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE &&
			pGamepad->sThumbRX > -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) &&
			(pGamepad->sThumbRY < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE &&
				pGamepad->sThumbRY > -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE))
		{
			pGamepad->sThumbRX = 0;
			pGamepad->sThumbRY = 0;
		}

		buttons = pGamepad->wButtons;

		// アナログ方向キー
		if (pGamepad->sThumbLY > Threshold) result |= PAD_RAW_UP;
		if (pGamepad->sThumbLY < -Threshold) result |= PAD_RAW_DOWN;
		if (pGamepad->sThumbLX > Threshold) result |= PAD_RAW_RIGHT;
		if (pGamepad->sThumbLX < -Threshold) result |= PAD_RAW_LEFT;

		// デジタル方向キー
		if (buttons & XINPUT_GAMEPAD_DPAD_UP) result |= PAD_RAW_UP;
		if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) result |= PAD_RAW_DOWN;
		if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) result |= PAD_RAW_LEFT;
		if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) result |= PAD_RAW_RIGHT;

		// ボタン
		if (buttons & XINPUT_GAMEPAD_A) result |= PAD_RAW_A;
		if (buttons & XINPUT_GAMEPAD_B) result |= PAD_RAW_B;
		if (buttons & XINPUT_GAMEPAD_X) result |= PAD_RAW_X;
		if (buttons & XINPUT_GAMEPAD_Y) result |= PAD_RAW_Y;
		if (buttons & XINPUT_GAMEPAD_START) result |= PAD_RAW_START;
		if (buttons & XINPUT_GAMEPAD_BACK)  result |= PAD_RAW_BACK;
//		if (buttons & XINPUT_GAMEPAD_LEFT_THUMB) { } // TODO:左アナログ方向キーをパッド奥に押した場合
//		if (buttons & XINPUT_GAMEPAD_RIGHT_THUMB) { } // TODO:右アナログ方向キーをパッド奥に押した場合
		if (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) result |= PAD_RAW_L;
		if (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) result |= PAD_RAW_R;
		// 左右のアナログトリガー
//		if (&pC->state.Gamepad.bLeftTrigger) { } // TODO
//		if (&pC->state.Gamepad.bRightTrigger) { } // TODO

	}

	return result;
}
