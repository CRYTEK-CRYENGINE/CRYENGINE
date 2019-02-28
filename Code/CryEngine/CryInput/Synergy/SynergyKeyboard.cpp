// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Winvalid-source-encoding"
#endif
#ifdef USE_SYNERGY_INPUT

	#include "InputDevice.h"
	#include "Synergy/SynergyContext.h"
	#include "Synergy/SynergyKeyboard.h"

CSynergyKeyboard::CSynergyKeyboard(IInput& input, CSynergyContext* pContext) : CInputDevice(input, "RemoteKeyboard")
{
	m_pContext = pContext;
	m_deviceType = eIDT_Keyboard;
}

CSynergyKeyboard::~CSynergyKeyboard()
{
}

bool CSynergyKeyboard::Init()
{
	SetupKeys();
	return true;
}

void CSynergyKeyboard::Update(bool bFocus)
{
	uint32 key, modifiers;
	bool bPressed, bRepeat;
	while (m_pContext->GetKey(key, bPressed, bRepeat, modifiers))
	{
		if (key == 47 && modifiers & SYNERGY_MODIFIER_CTRL)
		{
			if (bPressed)
			{
				TypeASCIIString(m_pContext->GetClipboard());
			}
		}
		else
		{
			ProcessKey(key, bPressed, bRepeat, modifiers);
		}
	}
}

void CSynergyKeyboard::ProcessKey(uint32 key, bool bPressed, bool bRepeat, uint32 modificators)
{
	SInputSymbol* pSymbol = DevSpecIdToSymbol(key);
	if (!pSymbol) return;
	SInputEvent event;

	event.modifiers = PackModificators(modificators);
	event.keyName = pSymbol->name;
	event.deviceType = GetDeviceType();
	event.keyId = pSymbol->keyId;

	if (bPressed || bRepeat)
	{
		uint32 inputChar = 0;
		if (event.modifiers & eMM_LShift)
			inputChar = pSymbol->user & 0xFF;
		else
			inputChar = (pSymbol->user >> 8) & 0xFF;

		if (inputChar)
		{
			SUnicodeEvent unicodeEvent(inputChar);
			GetIInput().PostUnicodeEvent(unicodeEvent);
		}
	}

	if (!bRepeat)
	{
		if (bPressed)
		{
			pSymbol->state = eIS_Pressed;
			pSymbol->value = 1.0f;
		}
		else
		{
			pSymbol->state = eIS_Released;
			pSymbol->value = 0.0f;
		}
		event.state = pSymbol->state;
		event.value = pSymbol->value;
		event.pSymbol = pSymbol;
		GetIInput().PostInputEvent(event);
	}
}

uint32 CSynergyKeyboard::GetInputCharUnicode(const SInputEvent& event)
{
	if (event.pSymbol && event.pSymbol->name)
	{
		if (event.modifiers & eMM_LShift)
			return event.pSymbol->user & 0xFF;
		return (event.pSymbol->user >> 8) & 0xFF;
	}
	return '\0';
}

uint32 CSynergyKeyboard::PackModificators(uint32 modificators)
{
	uint32 ret = 0;
	if (modificators & SYNERGY_MODIFIER_LALT)
		ret |= eMM_LAlt;
	if (modificators & SYNERGY_MODIFIER_RALT)
		ret |= eMM_RAlt;
	if (modificators & SYNERGY_MODIFIER_CTRL)
		ret |= eMM_LCtrl | eMM_RCtrl;
	if (modificators & SYNERGY_MODIFIER_SHIFT)
		ret |= eMM_LShift | eMM_RShift;
	if (modificators & SYNERGY_MODIFIER_WINDOWS)
		ret |= eMM_LWin;
	if (modificators & SYNERGY_MODIFIER_CAPSLOCK)
		ret |= eMM_CapsLock;
	if (modificators & SYNERGY_MODIFIER_NUMLOCK)
		ret |= eMM_NumLock;
	if (modificators & SYNERGY_MODIFIER_SCROLLOCK)
		ret |= eMM_ScrollLock;
	return ret;
}

void CSynergyKeyboard::SetupKeys()
{
	MapSymbol(1, eKI_Escape, "escape", SInputSymbol::Button, '\0\0');
	MapSymbol(2, eKI_1, "1", SInputSymbol::Button, '1!');
	MapSymbol(3, eKI_2, "2", SInputSymbol::Button, '2@');
	MapSymbol(4, eKI_3, "3", SInputSymbol::Button, '3#');
	MapSymbol(5, eKI_4, "4", SInputSymbol::Button, '4$');
	MapSymbol(6, eKI_5, "5", SInputSymbol::Button, '5%');
	MapSymbol(7, eKI_6, "6", SInputSymbol::Button, '6^');
	MapSymbol(8, eKI_7, "7", SInputSymbol::Button, '7&');
	MapSymbol(9, eKI_8, "8", SInputSymbol::Button, '8*');
	MapSymbol(10, eKI_9, "9", SInputSymbol::Button, '9(');
	MapSymbol(11, eKI_0, "0", SInputSymbol::Button, '0)');
	MapSymbol(12, eKI_Minus, "minus", SInputSymbol::Button, '-_');
	MapSymbol(13, eKI_Equals, "equals", SInputSymbol::Button, '=+');
	MapSymbol(14, eKI_Backspace, "backspace", SInputSymbol::Button, '\0\0');
	MapSymbol(15, eKI_Tab, "tab", SInputSymbol::Button, '\t\t');
	MapSymbol(16, eKI_Q, "q", SInputSymbol::Button, 'qQ');
	MapSymbol(17, eKI_W, "w", SInputSymbol::Button, 'wW');
	MapSymbol(18, eKI_E, "e", SInputSymbol::Button, 'eE');
	MapSymbol(19, eKI_R, "r", SInputSymbol::Button, 'rR');
	MapSymbol(20, eKI_T, "t", SInputSymbol::Button, 'tT');
	MapSymbol(21, eKI_Y, "y", SInputSymbol::Button, 'yY');
	MapSymbol(22, eKI_U, "u", SInputSymbol::Button, 'uU');
	MapSymbol(23, eKI_I, "i", SInputSymbol::Button, 'iI');
	MapSymbol(24, eKI_O, "o", SInputSymbol::Button, 'oO');
	MapSymbol(25, eKI_P, "p", SInputSymbol::Button, 'pP');
	MapSymbol(26, eKI_LBracket, "lbracket", SInputSymbol::Button, '[{');
	MapSymbol(27, eKI_RBracket, "rbracket", SInputSymbol::Button, ']}');
	MapSymbol(28, eKI_Enter, "enter", SInputSymbol::Button, '\n\n');
	MapSymbol(29, eKI_LCtrl, "lctrl", SInputSymbol::Button, '\0\0');
	MapSymbol(30, eKI_A, "a", SInputSymbol::Button, 'aA');
	MapSymbol(31, eKI_S, "s", SInputSymbol::Button, 'sS');
	MapSymbol(32, eKI_D, "d", SInputSymbol::Button, 'dD');
	MapSymbol(33, eKI_F, "f", SInputSymbol::Button, 'fF');
	MapSymbol(34, eKI_G, "g", SInputSymbol::Button, 'gG');
	MapSymbol(35, eKI_H, "h", SInputSymbol::Button, 'hH');
	MapSymbol(36, eKI_J, "j", SInputSymbol::Button, 'jJ');
	MapSymbol(37, eKI_K, "k", SInputSymbol::Button, 'kK');
	MapSymbol(38, eKI_L, "l", SInputSymbol::Button, 'lL');
	MapSymbol(39, eKI_Semicolon, "semicolon", SInputSymbol::Button, ';:');
	MapSymbol(40, eKI_Apostrophe, "apostrophe", SInputSymbol::Button, '\'"');
	MapSymbol(41, eKI_Tilde, "tilde", SInputSymbol::Button, '`~');
	MapSymbol(42, eKI_LShift, "lshift", SInputSymbol::Button, '\0\0');
	MapSymbol(44, eKI_Z, "z", SInputSymbol::Button, 'zZ');
	MapSymbol(45, eKI_X, "x", SInputSymbol::Button, 'xX');
	MapSymbol(46, eKI_C, "c", SInputSymbol::Button, 'cC');
	MapSymbol(47, eKI_V, "v", SInputSymbol::Button, 'vV');
	MapSymbol(48, eKI_B, "b", SInputSymbol::Button, 'bB');
	MapSymbol(49, eKI_N, "n", SInputSymbol::Button, 'nN');
	MapSymbol(50, eKI_M, "m", SInputSymbol::Button, 'mM');
	MapSymbol(51, eKI_Comma, "comma", SInputSymbol::Button, ',<');
	MapSymbol(52, eKI_Period, "period", SInputSymbol::Button, '.>');
	MapSymbol(53, eKI_Slash, "slash", SInputSymbol::Button, '/?');
	MapSymbol(54, eKI_RShift, "rshift", SInputSymbol::Button, '\0\0');
	MapSymbol(55, eKI_NP_Multiply, "np_multiply", SInputSymbol::Button, '*\0');
	MapSymbol(56, eKI_LAlt, "lalt", SInputSymbol::Button, '\0\0');
	MapSymbol(57, eKI_Space, "space", SInputSymbol::Button, '  ');
	MapSymbol(58, eKI_CapsLock, "capslock", SInputSymbol::Button, '\0\0');
	MapSymbol(59, eKI_F1, "f1", SInputSymbol::Button, '\0\0');
	MapSymbol(60, eKI_F2, "f2", SInputSymbol::Button, '\0\0');
	MapSymbol(61, eKI_F3, "f3", SInputSymbol::Button, '\0\0');
	MapSymbol(62, eKI_F4, "f4", SInputSymbol::Button, '\0\0');
	MapSymbol(63, eKI_F5, "f5", SInputSymbol::Button, '\0\0');
	MapSymbol(64, eKI_F6, "f6", SInputSymbol::Button, '\0\0');
	MapSymbol(65, eKI_F7, "f7", SInputSymbol::Button, '\0\0');
	MapSymbol(66, eKI_F8, "f8", SInputSymbol::Button, '\0\0');
	MapSymbol(67, eKI_F9, "f9", SInputSymbol::Button, '\0\0');
	MapSymbol(68, eKI_F10, "f10", SInputSymbol::Button, '\0\0');
	MapSymbol(69, eKI_Pause, "pause", SInputSymbol::Button, '\0\0');
	MapSymbol(71, eKI_NP_7, "np_7", SInputSymbol::Button, '7\0');
	MapSymbol(72, eKI_NP_8, "np_8", SInputSymbol::Button, '8\0');
	MapSymbol(73, eKI_NP_9, "np_9", SInputSymbol::Button, '9\0');
	MapSymbol(74, eKI_NP_Substract, "np_subtract", SInputSymbol::Button, '-\0');
	MapSymbol(75, eKI_NP_4, "np_4", SInputSymbol::Button, '4\0');
	MapSymbol(76, eKI_NP_5, "np_5", SInputSymbol::Button, '5\0');
	MapSymbol(77, eKI_NP_6, "np_6", SInputSymbol::Button, '6\0');
	MapSymbol(78, eKI_NP_Add, "np_add", SInputSymbol::Button, '+\0');
	MapSymbol(79, eKI_NP_1, "np_1", SInputSymbol::Button, '1\0');
	MapSymbol(80, eKI_NP_2, "np_2", SInputSymbol::Button, '2\0');
	MapSymbol(81, eKI_NP_3, "np_3", SInputSymbol::Button, '3\0');
	MapSymbol(82, eKI_NP_0, "np_0", SInputSymbol::Button, '0\0');
	MapSymbol(83, eKI_NP_Period, "np_period", SInputSymbol::Button, '.\0');
	MapSymbol(86, eKI_Backslash, "backslash", SInputSymbol::Button, '\\|');
	MapSymbol(87, eKI_F11, "f11", SInputSymbol::Button, '\0\0');
	MapSymbol(88, eKI_F12, "f22", SInputSymbol::Button, '\0\0');
	MapSymbol(284, eKI_NP_Enter, "np_enter", SInputSymbol::Button, '\n\n');
	MapSymbol(309, eKI_NP_Divide, "np_divide", SInputSymbol::Button, '/\0');
	MapSymbol(311, eKI_Print, "print", SInputSymbol::Button, '\0\0');
	MapSymbol(325, eKI_NumLock, "numlock", SInputSymbol::Toggle, eMM_NumLock);
	MapSymbol(327, eKI_Home, "home", SInputSymbol::Button, '\0\0');
	MapSymbol(328, eKI_Up, "up", SInputSymbol::Button, '\0\0');
	MapSymbol(329, eKI_PgUp, "pgup", SInputSymbol::Button, '\0\0');
	MapSymbol(331, eKI_Left, "left", SInputSymbol::Button, ' "\0\0');
	MapSymbol(333, eKI_Right, "right", SInputSymbol::Button, '\0\0');
	MapSymbol(335, eKI_End, "end", SInputSymbol::Button, '\0\0');
	MapSymbol(336, eKI_Down, "down", SInputSymbol::Button, '\0\0');
	MapSymbol(337, eKI_PgDn, "pgdn", SInputSymbol::Button, '\0\0');
	MapSymbol(338, eKI_Insert, "insert", SInputSymbol::Button, '\0\0');
	MapSymbol(339, eKI_Delete, "delete", SInputSymbol::Button, '\0\0');
}

void CSynergyKeyboard::TypeASCIIString(const char* pString)
{
	struct KeyPair
	{
		int         key;
		const char* ascii;
	};
	KeyPair pairs[] = {
		{ 2,  "1!"   },
		{ 3,  "2@"   },
		{ 4,  "3#"   },
		{ 5,  "4$"   },
		{ 6,  "5%"   },
		{ 7,  "6^"   },
		{ 8,  "7&"   },
		{ 9,  "8*"   },
		{ 10, "9("   },
		{ 11, "0)"   },
		{ 12, "-_"   },
		{ 13, "=+"   },
		{ 16, "qQ"   },
		{ 17, "wW"   },
		{ 18, "eE"   },
		{ 19, "rR"   },
		{ 20, "tT"   },
		{ 21, "yY"   },
		{ 22, "uU"   },
		{ 23, "iI"   },
		{ 24, "oO"   },
		{ 25, "pP"   },
		{ 26, "[{"   },
		{ 27, "]}"   },
		{ 28, "\n\n" },
		{ 30, "aA"   },
		{ 31, "sS"   },
		{ 32, "dD"   },
		{ 33, "fF"   },
		{ 34, "gG"   },
		{ 35, "hH"   },
		{ 36, "jJ"   },
		{ 37, "kK"   },
		{ 38, "lL"   },
		{ 39, ";:"   },
		{ 40, "'\""  },
		{ 41, "`~"   },
		{ 44, "zZ"   },
		{ 45, "xX"   },
		{ 46, "cC"   },
		{ 47, "vV"   },
		{ 48, "bB"   },
		{ 49, "nN"   },
		{ 50, "mM"   },
		{ 51, ",<"   },
		{ 52, ".>"   },
		{ 53, "/?"   },
		{ 57, "  "   },
		{ 86, "\\|"  }
	};
	while (pString[0])
	{
		for (int i = 0; i < CRY_ARRAY_COUNT(pairs); i++)
		{
			if (pString[0] == pairs[i].ascii[0] || pString[0] == pairs[i].ascii[1])
			{
				ProcessKey(pairs[i].key, true, false, (pString[0] != pairs[i].ascii[0]) ? SYNERGY_MODIFIER_SHIFT : 0);
				ProcessKey(pairs[i].key, false, false, (pString[0] != pairs[i].ascii[0]) ? SYNERGY_MODIFIER_SHIFT : 0);
				break;
			}
		}
		pString++;
	}
}

#endif // USE_SYNERGY_INPUT
