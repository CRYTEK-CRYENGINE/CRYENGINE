// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   -------------------------------------------------------------------------
   History:
   - Dec 05,2005:	Major rewrite by Marco Koegler

*************************************************************************/

#include "StdAfx.h"
#include "Keyboard.h"

#ifdef USE_DXINPUT

int CKeyboard::s_disableWinKeys = 0;
CKeyboard* CKeyboard::s_instance = NULL;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CKeyboard::SScanCode CKeyboard::m_scanCodes[256];
SInputSymbol* CKeyboard::Symbol[256] = { 0 };

CKeyboard::CKeyboard(CDXInput& input) :
	CDXInputDevice(input, "keyboard", GUID_SysKeyboard)
{
	m_deviceType = eIDT_Keyboard;
	m_baseflags = DISCL_NONEXCLUSIVE | DISCL_FOREGROUND;
	s_instance = this;
}

//////////////////////////////////////////////////////////////////////////
bool CKeyboard::Init()
{
	gEnv->pLog->LogToFile("Initializing Keyboard\n");

	if (!gEnv->pConsole->GetCVar("g_disableWinKeys"))
	{
		REGISTER_INT_CB("g_disableWinKeys", s_disableWinKeys, VF_NULL, "Determines if Windows keys are disabled", ChangeDisableWinKeysCallback);
		s_disableWinKeys = gEnv->pConsole->GetCVar("g_disableWinKeys")->GetIVal();
	}

	m_baseflags = DISCL_NONEXCLUSIVE | DISCL_FOREGROUND;
	if (!CreateDirectInputDevice(&c_dfDIKeyboard, GetDeviceFlags(), 32))
		return false;

	Acquire();
	SetupKeyNames();

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CKeyboard::OnLanguageChange()
{
	SetupKeyNames();
}

//////////////////////////////////////////////////////////////////////////
bool CKeyboard::SetExclusiveMode(bool value)
{
	Unacquire();

	// get the current toggle key states
	/*
	   if (GetKeyState(VK_NUMLOCK) & 0x01)
	   {
	   m_toggleState |= TOGGLE_NUMLOCK;
	   m_cKeysState[DIK_NUMLOCK] |= 0x01;
	   }
	   else
	   {
	   m_cKeysState[DIK_NUMLOCK] &= ~0x01;
	   }

	   if (GetKeyState(VK_CAPITAL) & 0x01)
	   {
	   m_toggleState |= TOGGLE_CAPSLOCK;
	   m_cKeysState[DIK_CAPSLOCK] |= 0x01;
	   }
	   else
	   {
	   m_cKeysState[DIK_CAPSLOCK] &= ~0x01;
	   }

	   if (GetKeyState(VK_SCROLL) & 0x01)
	   {
	   m_toggleState |= TOGGLE_SCROLLLOCK;
	   m_cKeysState[DIK_SCROLL] |= 0x01;
	   }
	   else
	   {
	   m_cKeysState[DIK_SCROLL] &= ~0x01;
	   }*/

	HRESULT hr;

	// Enable Windows keys if we are not in game mode - to always leave enabled, set to 0
	DWORD winKeyFlags = (!gEnv->IsEditor() || gEnv->IsEditorGameMode() ? DISCL_NOWINKEY : 0);

	if (value)
	{
		m_baseflags = DISCL_EXCLUSIVE | DISCL_FOREGROUND;
		hr = GetDirectInputDevice()->SetCooperativeLevel(GetDXInput().GetHWnd(), GetDeviceFlags());

		if (FAILED(hr))
		{
			gEnv->pLog->LogToFile("Cannot Set Keyboard Exclusive Mode\n");
			return false;
		}
	}
	else
	{
		m_baseflags = DISCL_NONEXCLUSIVE | DISCL_FOREGROUND;
		hr = GetDirectInputDevice()->SetCooperativeLevel(GetDXInput().GetHWnd(), GetDeviceFlags());
		if (FAILED(hr))
		{
			gEnv->pLog->LogToFile("Cannot Set Keyboard Non-Exclusive Mode\n");
			return false;
		}
	}

	if (!Acquire())
		return false;

	return true;
}

void CKeyboard::ChangeDisableWinKeys(ICVar* pVar)
{
	Unacquire();
	s_disableWinKeys = pVar->GetIVal();
	HRESULT hr;
	hr = GetDirectInputDevice()->SetCooperativeLevel(GetDXInput().GetHWnd(), GetDeviceFlags());
	if (FAILED(hr))
	{
		gEnv->pLog->LogToFile("Error changing enabled state of windows keys\n");
	}
	Acquire();
}

DWORD CKeyboard::GetDeviceFlags() const
{
	return m_baseflags | (s_disableWinKeys && (!gEnv->IsEditor() || gEnv->IsEditorGameMode()) ? DISCL_NOWINKEY : 0);
}

//////////////////////////////////////////////////////////////////////////
uint32 CKeyboard::Event2Unicode(const SInputEvent& event)
{
	unsigned int dik = NameToId(event.keyName);

	// not found or out of range
	if (dik >= 256)
		return '\0';

	switch (dik)
	{
	case DIK_DECIMAL:
		return '.';
	case DIK_DIVIDE:
		return '/';
	}
	;

	if (event.modifiers & eMM_NumLock)
	{
		switch (dik)
		{
		case DIK_NUMPAD0:
			return '0';
		case DIK_NUMPAD1:
			return '1';
		case DIK_NUMPAD2:
			return '2';
		case DIK_NUMPAD3:
			return '3';
		case DIK_NUMPAD4:
			return '4';
		case DIK_NUMPAD5:
			return '5';
		case DIK_NUMPAD6:
			return '6';
		case DIK_NUMPAD7:
			return '7';
		case DIK_NUMPAD8:
			return '8';
		case DIK_NUMPAD9:
			return '9';
		}
		;
	}
	if ((event.modifiers & eMM_Ctrl) && (event.modifiers & eMM_Alt) || (event.modifiers & eMM_RAlt))
	{
		return m_scanCodes[dik].ac;
	}
	else if ((event.modifiers & eMM_CapsLock) != 0)
	{
		return m_scanCodes[dik].cl;
	}
	else if ((event.modifiers & eMM_Shift) != 0)
	{
		return m_scanCodes[dik].uc;
	}
	return m_scanCodes[dik].lc;
}

//////////////////////////////////////////////////////////////////////////

void CKeyboard::RebuildScanCodeTable(HKL layout)
{
	wchar_t layoutName[KL_NAMELENGTH];
	if (GetKeyboardLayoutNameW(layoutName) == TRUE)
	{
		const string layoutNameUtf8 = Unicode::Convert<string>(layoutName);
		CryLog("Input locale changed to %s", layoutNameUtf8.c_str());
	}
	else
	{
		CryLog("Input locale changed to [unknown %p]", layout);
	}

	const auto scanCodeToVirtualKey = [layout](UINT scanCode) -> UINT
	{
		return MapVirtualKeyExW(scanCode, MAPVK_VSC_TO_VK, layout);
	};
	const auto virtualKeyToScanCode = [layout](UINT virtualKey) -> UINT
	{
		return MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_VSC, layout);
	};

	const UINT clearVirtualKey = VK_DECIMAL;
	const UINT clearScanCode = virtualKeyToScanCode(clearVirtualKey);
	const auto clearKeyboardBuffer = [=]()
	{
		static const BYTE emptyBuffer[256];
		WCHAR output[3];
		while (ToUnicodeEx(clearVirtualKey, clearScanCode, emptyBuffer, output, 2, 0, layout) < 0)
			;
	};
	clearKeyboardBuffer();

	BYTE keyboardState[256] = { 0 };
	const auto translateScanCode = [&](UINT scanCode) -> uint32
	{
		const UINT virtualKey = scanCodeToVirtualKey(scanCode);
		WCHAR output[2] = { 0 };
		bool bHasOutput = ToUnicodeEx(virtualKey, scanCode, keyboardState, output, 2, 0, layout) != 0;
		if (!bHasOutput)
		{
			keyboardState[scanCode] = 0x81;
			bHasOutput = ToUnicodeEx(virtualKey, scanCode, keyboardState, output, 2, 0, layout) != 0;
			keyboardState[scanCode] = 0x00;
		}
		clearKeyboardBuffer();
		if (!bHasOutput)
		{
			return 0; // No character
		}
		const bool bIsSurrogate = IS_SURROGATE_PAIR(output[0], output[1]);
		if (!bIsSurrogate)
		{
			return output[0]; // BMP character
		}
		return ((output[0] & 0x3FF) << 10) | (output[1] & 0x3FF) | 0x10000; // Surrogate pair
	};

	for (int k = 0; k < 256; k++)
	{
		// lower case
		m_scanCodes[k].lc = translateScanCode(k);

		// upper case
		keyboardState[VK_SHIFT] = 0x80;
		m_scanCodes[k].uc = translateScanCode(k);
		keyboardState[VK_SHIFT] = 0x00;

		// alternate
		keyboardState[VK_CONTROL] = 0x80;
		keyboardState[VK_MENU] = 0x80;
		keyboardState[VK_LCONTROL] = 0x80;
		keyboardState[VK_LMENU] = 0x80;
		m_scanCodes[k].ac = translateScanCode(k);
		keyboardState[VK_CONTROL] = 0x00;
		keyboardState[VK_MENU] = 0x00;
		keyboardState[VK_LCONTROL] = 0x00;
		keyboardState[VK_LMENU] = 0x00;

		// caps lock
		keyboardState[VK_CAPITAL] = 0x01;
		m_scanCodes[k].cl = translateScanCode(k);
		keyboardState[VK_CAPITAL] = 0x00;
	}
}

//////////////////////////////////////////////////////////////////////////

void CKeyboard::SetupKeyNames()
{
	Symbol[DIK_ESCAPE] = MapSymbol(DIK_ESCAPE, eKI_Escape, "escape");
	Symbol[DIK_1] = MapSymbol(DIK_1, eKI_1, "1");
	Symbol[DIK_2] = MapSymbol(DIK_2, eKI_2, "2");
	Symbol[DIK_3] = MapSymbol(DIK_3, eKI_3, "3");
	Symbol[DIK_4] = MapSymbol(DIK_4, eKI_4, "4");
	Symbol[DIK_5] = MapSymbol(DIK_5, eKI_5, "5");
	Symbol[DIK_6] = MapSymbol(DIK_6, eKI_6, "6");
	Symbol[DIK_7] = MapSymbol(DIK_7, eKI_7, "7");
	Symbol[DIK_8] = MapSymbol(DIK_8, eKI_8, "8");
	Symbol[DIK_9] = MapSymbol(DIK_9, eKI_9, "9");
	Symbol[DIK_0] = MapSymbol(DIK_0, eKI_0, "0");
	Symbol[DIK_MINUS] = MapSymbol(DIK_MINUS, eKI_Minus, "minus");
	Symbol[DIK_EQUALS] = MapSymbol(DIK_EQUALS, eKI_Equals, "equals");
	Symbol[DIK_BACK] = MapSymbol(DIK_BACK, eKI_Backspace, "backspace");
	Symbol[DIK_TAB] = MapSymbol(DIK_TAB, eKI_Tab, "tab");
	Symbol[DIK_Q] = MapSymbol(DIK_Q, eKI_Q, "q");
	Symbol[DIK_W] = MapSymbol(DIK_W, eKI_W, "w");
	Symbol[DIK_E] = MapSymbol(DIK_E, eKI_E, "e");
	Symbol[DIK_R] = MapSymbol(DIK_R, eKI_R, "r");
	Symbol[DIK_T] = MapSymbol(DIK_T, eKI_T, "t");
	Symbol[DIK_Y] = MapSymbol(DIK_Y, eKI_Y, "y");
	Symbol[DIK_U] = MapSymbol(DIK_U, eKI_U, "u");
	Symbol[DIK_I] = MapSymbol(DIK_I, eKI_I, "i");
	Symbol[DIK_O] = MapSymbol(DIK_O, eKI_O, "o");
	Symbol[DIK_P] = MapSymbol(DIK_P, eKI_P, "p");
	Symbol[DIK_LBRACKET] = MapSymbol(DIK_LBRACKET, eKI_LBracket, "lbracket");
	Symbol[DIK_RBRACKET] = MapSymbol(DIK_RBRACKET, eKI_RBracket, "rbracket");
	Symbol[DIK_RETURN] = MapSymbol(DIK_RETURN, eKI_Enter, "enter");
	Symbol[DIK_LCONTROL] = MapSymbol(DIK_LCONTROL, eKI_LCtrl, "lctrl", SInputSymbol::Button, eMM_LCtrl);
	Symbol[DIK_A] = MapSymbol(DIK_A, eKI_A, "a");
	Symbol[DIK_S] = MapSymbol(DIK_S, eKI_S, "s");
	Symbol[DIK_D] = MapSymbol(DIK_D, eKI_D, "d");
	Symbol[DIK_F] = MapSymbol(DIK_F, eKI_F, "f");
	Symbol[DIK_G] = MapSymbol(DIK_G, eKI_G, "g");
	Symbol[DIK_H] = MapSymbol(DIK_H, eKI_H, "h");
	Symbol[DIK_J] = MapSymbol(DIK_J, eKI_J, "j");
	Symbol[DIK_K] = MapSymbol(DIK_K, eKI_K, "k");
	Symbol[DIK_L] = MapSymbol(DIK_L, eKI_L, "l");
	Symbol[DIK_SEMICOLON] = MapSymbol(DIK_SEMICOLON, eKI_Semicolon, "semicolon");
	Symbol[DIK_APOSTROPHE] = MapSymbol(DIK_APOSTROPHE, eKI_Apostrophe, "apostrophe");
	Symbol[DIK_GRAVE] = MapSymbol(DIK_GRAVE, eKI_Tilde, "tilde");
	Symbol[DIK_LSHIFT] = MapSymbol(DIK_LSHIFT, eKI_LShift, "lshift", SInputSymbol::Button, eMM_LShift);
	Symbol[DIK_BACKSLASH] = MapSymbol(DIK_BACKSLASH, eKI_Backslash, "backslash");
	Symbol[DIK_Z] = MapSymbol(DIK_Z, eKI_Z, "z");
	Symbol[DIK_X] = MapSymbol(DIK_X, eKI_X, "x");
	Symbol[DIK_C] = MapSymbol(DIK_C, eKI_C, "c");
	Symbol[DIK_V] = MapSymbol(DIK_V, eKI_V, "v");
	Symbol[DIK_B] = MapSymbol(DIK_B, eKI_B, "b");
	Symbol[DIK_N] = MapSymbol(DIK_N, eKI_N, "n");
	Symbol[DIK_M] = MapSymbol(DIK_M, eKI_M, "m");
	Symbol[DIK_COMMA] = MapSymbol(DIK_COMMA, eKI_Comma, "comma");
	Symbol[DIK_PERIOD] = MapSymbol(DIK_PERIOD, eKI_Period, "period");
	Symbol[DIK_SLASH] = MapSymbol(DIK_SLASH, eKI_Slash, "slash");
	Symbol[DIK_RSHIFT] = MapSymbol(DIK_RSHIFT, eKI_RShift, "rshift", SInputSymbol::Button, eMM_RShift);
	Symbol[DIK_MULTIPLY] = MapSymbol(DIK_MULTIPLY, eKI_NP_Multiply, "np_multiply");
	Symbol[DIK_LALT] = MapSymbol(DIK_LALT, eKI_LAlt, "lalt", SInputSymbol::Button, eMM_LAlt);
	Symbol[DIK_SPACE] = MapSymbol(DIK_SPACE, eKI_Space, "space");
	Symbol[DIK_CAPSLOCK] = MapSymbol(DIK_CAPSLOCK, eKI_CapsLock, "capslock", SInputSymbol::Toggle, eMM_CapsLock);
	Symbol[DIK_F1] = MapSymbol(DIK_F1, eKI_F1, "f1");
	Symbol[DIK_F2] = MapSymbol(DIK_F2, eKI_F2, "f2");
	Symbol[DIK_F3] = MapSymbol(DIK_F3, eKI_F3, "f3");
	Symbol[DIK_F4] = MapSymbol(DIK_F4, eKI_F4, "f4");
	Symbol[DIK_F5] = MapSymbol(DIK_F5, eKI_F5, "f5");
	Symbol[DIK_F6] = MapSymbol(DIK_F6, eKI_F6, "f6");
	Symbol[DIK_F7] = MapSymbol(DIK_F7, eKI_F7, "f7");
	Symbol[DIK_F8] = MapSymbol(DIK_F8, eKI_F8, "f8");
	Symbol[DIK_F9] = MapSymbol(DIK_F9, eKI_F9, "f9");
	Symbol[DIK_F10] = MapSymbol(DIK_F10, eKI_F10, "f10");
	Symbol[DIK_NUMLOCK] = MapSymbol(DIK_NUMLOCK, eKI_NumLock, "numlock", SInputSymbol::Toggle, eMM_NumLock);
	Symbol[DIK_SCROLL] = MapSymbol(DIK_SCROLL, eKI_ScrollLock, "scrolllock", SInputSymbol::Toggle, eMM_ScrollLock);
	Symbol[DIK_NUMPAD7] = MapSymbol(DIK_NUMPAD7, eKI_NP_7, "np_7");
	Symbol[DIK_NUMPAD8] = MapSymbol(DIK_NUMPAD8, eKI_NP_8, "np_8");
	Symbol[DIK_NUMPAD9] = MapSymbol(DIK_NUMPAD9, eKI_NP_9, "np_9");
	Symbol[DIK_SUBTRACT] = MapSymbol(DIK_SUBTRACT, eKI_NP_Substract, "np_subtract");
	Symbol[DIK_NUMPAD4] = MapSymbol(DIK_NUMPAD4, eKI_NP_4, "np_4");
	Symbol[DIK_NUMPAD5] = MapSymbol(DIK_NUMPAD5, eKI_NP_5, "np_5");
	Symbol[DIK_NUMPAD6] = MapSymbol(DIK_NUMPAD6, eKI_NP_6, "np_6");
	Symbol[DIK_ADD] = MapSymbol(DIK_ADD, eKI_NP_Add, "np_add");
	Symbol[DIK_NUMPAD1] = MapSymbol(DIK_NUMPAD1, eKI_NP_1, "np_1");
	Symbol[DIK_NUMPAD2] = MapSymbol(DIK_NUMPAD2, eKI_NP_2, "np_2");
	Symbol[DIK_NUMPAD3] = MapSymbol(DIK_NUMPAD3, eKI_NP_3, "np_3");
	Symbol[DIK_NUMPAD0] = MapSymbol(DIK_NUMPAD0, eKI_NP_0, "np_0");
	Symbol[DIK_DECIMAL] = MapSymbol(DIK_DECIMAL, eKI_NP_Period, "np_period");
	Symbol[DIK_F11] = MapSymbol(DIK_F11, eKI_F11, "f11");
	Symbol[DIK_F12] = MapSymbol(DIK_F12, eKI_F12, "f12");
	Symbol[DIK_F13] = MapSymbol(DIK_F13, eKI_F13, "f13");
	Symbol[DIK_F14] = MapSymbol(DIK_F14, eKI_F14, "f14");
	Symbol[DIK_F15] = MapSymbol(DIK_F15, eKI_F15, "f15");
	Symbol[DIK_COLON] = MapSymbol(DIK_COLON, eKI_Colon, "colon");
	Symbol[DIK_UNDERLINE] = MapSymbol(DIK_UNDERLINE, eKI_Underline, "underline");
	Symbol[DIK_NUMPADENTER] = MapSymbol(DIK_NUMPADENTER, eKI_NP_Enter, "np_enter");
	Symbol[DIK_RCONTROL] = MapSymbol(DIK_RCONTROL, eKI_RCtrl, "rctrl", SInputSymbol::Button, eMM_RCtrl);
	Symbol[DIK_DIVIDE] = MapSymbol(DIK_DIVIDE, eKI_NP_Divide, "np_divide");
	Symbol[DIK_SYSRQ] = MapSymbol(DIK_SYSRQ, eKI_Print, "print");
	Symbol[DIK_RALT] = MapSymbol(DIK_RALT, eKI_RAlt, "ralt", SInputSymbol::Button, eMM_RAlt);
	Symbol[DIK_PAUSE] = MapSymbol(DIK_PAUSE, eKI_Pause, "pause");
	Symbol[DIK_HOME] = MapSymbol(DIK_HOME, eKI_Home, "home");
	Symbol[DIK_UP] = MapSymbol(DIK_UP, eKI_Up, "up");
	Symbol[DIK_PGUP] = MapSymbol(DIK_PGUP, eKI_PgUp, "pgup");
	Symbol[DIK_LEFT] = MapSymbol(DIK_LEFT, eKI_Left, "left");
	Symbol[DIK_RIGHT] = MapSymbol(DIK_RIGHT, eKI_Right, "right");
	Symbol[DIK_END] = MapSymbol(DIK_END, eKI_End, "end");
	Symbol[DIK_DOWN] = MapSymbol(DIK_DOWN, eKI_Down, "down");
	Symbol[DIK_PGDN] = MapSymbol(DIK_PGDN, eKI_PgDn, "pgdn");
	Symbol[DIK_INSERT] = MapSymbol(DIK_INSERT, eKI_Insert, "insert");
	Symbol[DIK_DELETE] = MapSymbol(DIK_DELETE, eKI_Delete, "delete");
	//Symbol[DIK_LWIN] = MapSymbol(DIK_LWIN, eKI_LWin,  "lwin");
	//Symbol[DIK_RWIN] = MapSymbol(DIK_RWIN, eKI_RWin,  "rwin");
	//Symbol[DIK_APPS] = MapSymbol(DIK_APPS, eKI_Apps,  "apps");
	Symbol[DIK_OEM_102] = MapSymbol(DIK_OEM_102, eKI_OEM_102, "oem_102");

	RebuildScanCodeTable(GetKeyboardLayout(0));
}

//////////////////////////////////////////////////////////////////////////
void CKeyboard::ProcessKey(uint32 devSpecId, bool pressed)
{
	SInputSymbol* pSymbol = Symbol[devSpecId];

	if (!pSymbol) return;

	EInputState newState;

	int modifiers = GetDXInput().GetModifiers();
	if (pressed)
	{
		if (pSymbol->type == SInputSymbol::Toggle)
		{
			if (modifiers & pSymbol->user)
				modifiers &= ~pSymbol->user;
			else
				modifiers |= pSymbol->user;
		}
		else if (pSymbol->user)
		{
			// must be a regular modifier key
			modifiers |= pSymbol->user;
		}

		newState = eIS_Pressed;
		pSymbol->value = 1.0f;
	}
	else
	{
		if (pSymbol->user && pSymbol->type == SInputSymbol::Button)
		{
			// must be a regular modifier key
			modifiers &= ~pSymbol->user;
		}

		newState = eIS_Released;
		pSymbol->value = 0.0f;
	}

	GetDXInput().SetModifiers(modifiers);

	// check if the state has really changed ... otherwise ignore it
	if (newState == pSymbol->state)
	{
		//gEnv->pLog->Log("Input: Identical key event discarded: '%s' - %s", pSymbol->name.c_str(), newState==eIS_Pressed?"pressed":"released");
		return;
	}
	pSymbol->state = newState;

	// Post input events
	SInputEvent event;
	pSymbol->AssignTo(event, modifiers);

	// if alt+tab was pressed
	// auto-release it, because we lost the focus on the press, so we don't get the up message
	if ((event.keyName == "tab") && (event.state == eIS_Pressed) && (modifiers & eMM_Alt))
	{
		// swallow "alt-tab"
		pSymbol->value = 0;
		pSymbol->state = eIS_Released;
	}
	else
	{
		GetDXInput().PostInputEvent(event);

		// TODO: Make all keyboard devices consistent with OnInputEvent and OnInputEventUI and bring handling of WM_CHAR messages into this class
		/*
		   if (event.state == eIS_Pressed)
		   {
		   static char szAsciiInput[2];
		   szAsciiInput[0] = Event2ASCII(event);
		   szAsciiInput[1] = 0;

		   static wchar_t tmp[2] = { 0 };
		   tmp[0] = 0;
		   MultiByteToWideChar(CP_ACP, 0, (char*)&szAsciiInput, 1, tmp, 2);

		   if (tmp[0])
		   {
		    event.state = eIS_UI;
		    event.inputChar = tmp[0];
		    event.value = 1.0f;
		    GetIInput().PostInputEvent(event);
		   }
		   }
		 */
	}
}

//////////////////////////////////////////////////////////////////////////
void CKeyboard::Update(bool bFocus)
{
	CRY_PROFILE_FUNCTION(PROFILE_INPUT);
	HRESULT hr;
	DIDEVICEOBJECTDATA rgdod[256];
	DWORD dwItems = 256;

	while (GetDirectInputDevice())
	{
		dwItems = 256;
		if (g_pInputCVars->i_bufferedkeys)
		{
			// read buffered data and keep the previous values otherwise keys
			hr = GetDirectInputDevice()->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), rgdod, &dwItems, 0); //0);
		}
		else
		{
			//memset(m_cTempKeys,0,256);
			//hr = GetDirectInputDevice()->GetDeviceState(sizeof(m_cTempKeys),m_cTempKeys);
		}

		if (SUCCEEDED(hr))
		{
			if (g_pInputCVars->i_bufferedkeys)
			{
				if (GetDXInput().IsImeComposing())
				{
					// This is somewhat of a hack, to work around difference between Win32 message input and DirectInput.
					// The problem happens when IME is used to compose character input, some keyboard events such as WM_KEYDOWN/WM_KEYUP are masked.
					// Typically, the IME will set the virtual key to VK_PROCESSKEY if it has consumed the keyboard input, causing the event to be ignored for other purposes.
					// However, DirectInput hooks lower into the input event stream, and thus observes events before the IME has had a chance to mask them.
					// Therefore, we mask all keyboard events here by just ignoring them as long as composition is happening to get similar behavior.
					// Failure to do this will cause weird behavior in Scaleform IME handling, as it can't (for example) deal with backspace input during composition.
					break;
				}

				// go through all buffered items
				for (unsigned int k = 0; k < dwItems; k++)
				{
					int key = rgdod[k].dwOfs;
					bool pressed = ((rgdod[k].dwData & 0x80) != 0);

					ProcessKey(key, pressed);
				}
			}
			break;
		}
		else
		{
			if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
			{
				if (FAILED(hr = GetDirectInputDevice()->Acquire()))
					break;
			}
			else
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CKeyboard::ClearKeyState()
{
	// preserve toggle keys
	int modifiers = GetDXInput().GetModifiers();
	modifiers &= eMM_LockKeys;

	bool bScroll = GetKeyState(VK_SCROLL) & 0x1;
	bool bNumLock = GetKeyState(VK_NUMLOCK) & 0x1;
	bool bCapsLock = GetKeyState(VK_CAPITAL) & 0x1;

	if (bScroll)
		modifiers |= eMM_ScrollLock;
	if (bNumLock)
		modifiers |= eMM_NumLock;
	if (bCapsLock)
		modifiers |= eMM_CapsLock;

	GetDXInput().SetModifiers(modifiers);

	// release pressed or held keys
	CDXInputDevice::ClearKeyState();
}

uint32 CKeyboard::GetInputCharUnicode(const SInputEvent& event)
{
	return Event2Unicode(event);
}

	#define IS_KEYBOARD_KEY(key) ((key)& 0x0000FFFF)

const char* CKeyboard::GetOSKeyName(const SInputEvent& event)
{
	if (IS_KEYBOARD_KEY(event.keyId) && GetDirectInputDevice())
	{
		unsigned int iDIK = NameToId(event.keyName);

		if (iDIK)
		{
			static DIDEVICEOBJECTINSTANCE dido;
			ZeroMemory(&dido, sizeof(DIDEVICEOBJECTINSTANCE));
			dido.dwSize = sizeof(DIDEVICEOBJECTINSTANCE);

			if (GetDirectInputDevice()->GetObjectInfo(&dido, iDIK, DIPH_BYOFFSET) != DI_OK)
				return "";

			return dido.tszName;
		}
	}
	return "";
}

void CKeyboard::ChangeDisableWinKeysCallback(ICVar* pVar)
{
	s_instance->ChangeDisableWinKeys(pVar);
}

#endif // USE_DXINPUT
