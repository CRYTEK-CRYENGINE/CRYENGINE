// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Keyboard Input implementation for Linux using SDL
   -------------------------------------------------------------------------
   History:
   - Aug 02,2013:	Created by Leander Beernaert

*************************************************************************/
#include "StdAfx.h"

#if defined(USE_LINUXINPUT)
	#include "SDLKeyboard.h"
	#include "SDLMouse.h"
	#include <SDL.h>

	#include <CryString/StringUtils.h>
	#include <CryString/UnicodeIterator.h>

	#define KEYBOARD_MAX_PEEP 64

CSDLKeyboard::CSDLKeyboard(CLinuxInput& input) :
	CLinuxInputDevice(input, "SDL Keyboard")
{
	m_deviceType = eIDT_Keyboard;
}

CSDLKeyboard::~CSDLKeyboard()
{

}

bool CSDLKeyboard::Init()
{
	SetupKeyNames();

	//SDL_StopTextInput();
	return true;
}

void CSDLKeyboard::Update(bool focus)
{
	SDL_Event eventList[KEYBOARD_MAX_PEEP];

	int nEvents;
	unsigned type = 0;
	#if !defined(_RELEASE)
	// Stop SDL2 from generating text input event when pressing the console
	// key to hide/show the in-game console
	bool didPressTild = false;
	#endif
	SInputSymbol* pSymbol = NULL;

	nEvents = SDL_PeepEvents(eventList, KEYBOARD_MAX_PEEP, SDL_GETEVENT, SDL_KEYDOWN, SDL_TEXTINPUT);
	if (nEvents == -1)
	{
		gEnv->pLog->LogError("SDL_GETEVENT error: %s", SDL_GetError());
		return;
	}
	for (int i = 0; i < nEvents; ++i)
	{
		type = eventList[i].type;
		if (type == SDL_KEYDOWN || type == SDL_KEYUP)
		{
			SDL_KeyboardEvent* const keyEvent = &eventList[i].key;
			m_lastKeySym = keyEvent->keysym.sym;
			m_lastMod = ConvertModifiers(keyEvent->keysym.mod);
		}
		// ignore this event. We can't skip it since the events are sampeled
		// in intervals and the value of SDL_TEXTEDITING is lower than
		// SDL_TEXTINPUT and higher than SDL_KEYUP
		if (type == SDL_TEXTEDITING) continue;

		if (type == SDL_KEYDOWN)
		{
			SDL_KeyboardEvent* const keyEvent = &eventList[i].key;
			SDL_Scancode keySym = keyEvent->keysym.scancode;
	#if !defined(_RELEASE)
			didPressTild = (keySym == SDL_SCANCODE_GRAVE);
	#endif
			pSymbol = DevSpecIdToSymbol((uint32)keySym);
			if (pSymbol == NULL)
				continue;
			pSymbol->state = eIS_Pressed;
			pSymbol->value = 1.f;
			// Check for CTRL+ALT combintaions
			if ((keyEvent->keysym.mod & KMOD_ALT) && (keyEvent->keysym.mod & KMOD_CTRL))
			{
				// CTRL + ALT + SHIFT + g = ungrab
				if (keySym == SDL_SCANCODE_G && keyEvent->keysym.mod & KMOD_SHIFT)
					GetLinuxInput().GrabInput(false);
				// CTRL + ALT + g  = grab
				else if (keySym == SDL_SCANCODE_G)
					GetLinuxInput().GrabInput(true);
			}

			PostEvent(pSymbol, keyEvent->keysym.mod);

			// TODO: Make consistent with other keyboard devices and fire OnInputEventUI with unicode char
			/*
			       wchar_t unicode = keyEvent->keysym.unicode
			       if (unicode)
			       {
			       SInputEvent event;
			       event.keyName = pSymbol->name;
			       event.deviceId = eDI_Mouse;
			       event.timestamp = GetTickCount();
			       if (keyMod != ~0)
			       event.modifiers = ConvertModifiers(keyMod);
			       else
			       event.modifiers = 0; // FIXMCSDLMouse* pMouseE add mouse modifier support

			       event.state = eIS_UI;
			       event.inputChar = (wchar_t)unicode;
			       event.value = 1.0f;
			       event.pSymbol = pSymbol;
			       GetIInput().PostInputEvent(event);
			       }
			 */
		}
		else if (type == SDL_KEYUP)
		{
			SDL_KeyboardEvent* const keyEvent = &eventList[i].key;
			SDL_Scancode keySym = keyEvent->keysym.scancode;
			pSymbol = DevSpecIdToSymbol((uint32)keySym);
			if (pSymbol == NULL)
				continue;
			pSymbol->state = eIS_Released;
			pSymbol->value = 0.f;
			PostEvent(pSymbol, keyEvent->keysym.mod);
		}
		else if (type == SDL_TEXTINPUT)
		{
	#if !defined(_RELEASE)
			if (!didPressTild)
			{
	#endif
			SDL_TextInputEvent* const txtEvent = &eventList[i].text;
			uint32_t codepoint;
			for (Unicode::CIterator<const char*, false> it(txtEvent->text); codepoint = *it; ++it)
			{
				SUnicodeEvent event(codepoint);
				GetLinuxInput().PostUnicodeEvent(event);
			}
	#if !defined(_RELEASE)
		}
	#endif
		}
		else
		{
			// Unexpected event type.
			abort();
		}
	}
}

char CSDLKeyboard::GetInputCharAscii(const SInputEvent& event)
{
	SDL_Keycode keySym = (SDL_Keycode)NameToId(event.keyName);
	if (keySym == m_lastKeySym && event.modifiers == m_lastMod)
	{
		unsigned code = 0;    //m_lastUNICODE;
		if (code > 0 && code < 255)
		{
			return (char)code;
		}
	}
	return Event2ASCII(event);
}

unsigned char CSDLKeyboard::Event2ASCII(const SInputEvent& event)
{

	SDL_Keycode keySym = (SDL_Keycode)NameToId(event.keyName);
	char ascii = 0;
	bool alpha = false;

	switch (keySym)
	{
	case SDLK_BACKSPACE:
		ascii = '\b';
		break;
	case SDLK_TAB:
		ascii = '\t';
		break;
	case SDLK_RETURN:
		ascii = '\r';
		break;
	case SDLK_ESCAPE:
		ascii = '\x1b';
		break;
	case SDLK_SPACE:
		ascii = ' ';
		break;
	case SDLK_EXCLAIM:
		ascii = '!';
		break;
	case SDLK_QUOTEDBL:
		ascii = '"';
		break;
	case SDLK_HASH:
		ascii = '#';
		break;
	case SDLK_DOLLAR:
		ascii = '$';
		break;
	case SDLK_AMPERSAND:
		ascii = '&';
		break;
	case SDLK_QUOTE:
		ascii = '\'';
		break;
	case SDLK_LEFTPAREN:
		ascii = '(';
		break;
	case SDLK_RIGHTPAREN:
		ascii = ')';
		break;
	case SDLK_ASTERISK:
		ascii = '*';
		break;
	case SDLK_PLUS:
		ascii = '+';
		break;
	case SDLK_COMMA:
		ascii = ',';
		break;
	case SDLK_MINUS:
		ascii = '-';
		break;
	case SDLK_PERIOD:
		ascii = '.';
		break;
	case SDLK_SLASH:
		ascii = '/';
		break;
	case SDLK_0:
		ascii = '0';
		break;
	case SDLK_1:
		ascii = '1';
		break;
	case SDLK_2:
		ascii = '2';
		break;
	case SDLK_3:
		ascii = '3';
		break;
	case SDLK_4:
		ascii = '4';
		break;
	case SDLK_5:
		ascii = '5';
		break;
	case SDLK_6:
		ascii = '6';
		break;
	case SDLK_7:
		ascii = '7';
		break;
	case SDLK_8:
		ascii = '8';
		break;
	case SDLK_9:
		ascii = '9';
		break;
	case SDLK_COLON:
		ascii = ':';
		break;
	case SDLK_SEMICOLON:
		ascii = ';';
		break;
	case SDLK_LESS:
		ascii = '<';
		break;
	case SDLK_EQUALS:
		ascii = '=';
		break;
	case SDLK_GREATER:
		ascii = '>';
		break;
	case SDLK_QUESTION:
		ascii = '?';
		break;
	case SDLK_AT:
		ascii = '@';
		break;
	case SDLK_LEFTBRACKET:
		ascii = '[';
		break;
	case SDLK_BACKSLASH:
		ascii = '\\';
		break;
	case SDLK_RIGHTBRACKET:
		ascii = ']';
		break;
	case SDLK_CARET:
		ascii = '^';
		break;
	case SDLK_UNDERSCORE:
		ascii = '_';
		break;
	case SDLK_BACKQUOTE:
		ascii = '`';
		break;
	case SDLK_a:
		ascii = 'a';
		alpha = true;
		break;
	case SDLK_b:
		ascii = 'b';
		alpha = true;
		break;
	case SDLK_c:
		ascii = 'c';
		alpha = true;
		break;
	case SDLK_d:
		ascii = 'd';
		alpha = true;
		break;
	case SDLK_e:
		ascii = 'e';
		alpha = true;
		break;
	case SDLK_f:
		ascii = 'f';
		alpha = true;
		break;
	case SDLK_g:
		ascii = 'g';
		alpha = true;
		break;
	case SDLK_h:
		ascii = 'h';
		alpha = true;
		break;
	case SDLK_i:
		ascii = 'i';
		alpha = true;
		break;
	case SDLK_j:
		ascii = 'j';
		alpha = true;
		break;
	case SDLK_k:
		ascii = 'k';
		alpha = true;
		break;
	case SDLK_l:
		ascii = 'l';
		alpha = true;
		break;
	case SDLK_m:
		ascii = 'm';
		alpha = true;
		break;
	case SDLK_n:
		ascii = 'n';
		alpha = true;
		break;
	case SDLK_o:
		ascii = 'o';
		alpha = true;
		break;
	case SDLK_p:
		ascii = 'p';
		alpha = true;
		break;
	case SDLK_q:
		ascii = 'q';
		alpha = true;
		break;
	case SDLK_r:
		ascii = 'r';
		alpha = true;
		break;
	case SDLK_s:
		ascii = 's';
		alpha = true;
		break;
	case SDLK_t:
		ascii = 't';
		alpha = true;
		break;
	case SDLK_u:
		ascii = 'u';
		alpha = true;
		break;
	case SDLK_v:
		ascii = 'v';
		alpha = true;
		break;
	case SDLK_w:
		ascii = 'w';
		alpha = true;
		break;
	case SDLK_x:
		ascii = 'x';
		alpha = true;
		break;
	case SDLK_y:
		ascii = 'y';
		alpha = true;
		break;
	case SDLK_z:
		ascii = 'z';
		alpha = true;
		break;
	case SDLK_DELETE:
		ascii = '\x7f';
		break;
	case SDLK_KP_0:
		ascii = '0';
		break;
	case SDLK_KP_1:
		ascii = '1';
		break;
	case SDLK_KP_2:
		ascii = '2';
		break;
	case SDLK_KP_3:
		ascii = '3';
		break;
	case SDLK_KP_4:
		ascii = '4';
		break;
	case SDLK_KP_5:
		ascii = '5';
		break;
	case SDLK_KP_6:
		ascii = '6';
		break;
	case SDLK_KP_7:
		ascii = '7';
		break;
	case SDLK_KP_8:
		ascii = '8';
		break;
	case SDLK_KP_9:
		ascii = '9';
		break;
	case SDLK_KP_PERIOD:
		ascii = '.';
		break;
	case SDLK_KP_DIVIDE:
		ascii = '/';
		break;
	case SDLK_KP_MULTIPLY:
		ascii = '*';
		break;
	case SDLK_KP_MINUS:
		ascii = '-';
		break;
	case SDLK_KP_PLUS:
		ascii = '+';
		break;
	case SDLK_KP_ENTER:
		ascii = '\r';
		break;
	case SDLK_KP_EQUALS:
		ascii = '=';
		break;
	default:
		ascii = 0;
		break;
	}
	if (ascii && (event.modifiers & (eMM_LShift | eMM_RShift | eMM_CapsLock)))
		ascii = toupper(ascii);
	return ascii;
}

int CSDLKeyboard::ConvertModifiers(unsigned keyEventModifiers)
{
	int modifiers = 0;

	if (keyEventModifiers & KMOD_LCTRL)
		modifiers |= eMM_LCtrl;
	if (keyEventModifiers & KMOD_LSHIFT)
		modifiers |= eMM_LShift;
	if (keyEventModifiers & KMOD_LALT)
		modifiers |= eMM_LAlt;
	if (keyEventModifiers & KMOD_LGUI)
		modifiers |= eMM_LWin;
	if (keyEventModifiers & KMOD_RCTRL)
		modifiers |= eMM_RCtrl;
	if (keyEventModifiers & KMOD_RSHIFT)
		modifiers |= eMM_RShift;
	if (keyEventModifiers & KMOD_RALT)
		modifiers |= eMM_RAlt;
	if (keyEventModifiers & KMOD_RGUI)
		modifiers |= eMM_RWin;
	if (keyEventModifiers & KMOD_NUM)
		modifiers |= eMM_NumLock;
	if (keyEventModifiers & KMOD_CAPS)
		modifiers |= eMM_CapsLock;
	if (keyEventModifiers & KMOD_MODE)
		modifiers |= eMM_ScrollLock;
	return modifiers;
}

void CSDLKeyboard::SetupKeyNames()
{
	MapSymbol(SDL_SCANCODE_BACKSPACE, eKI_Backspace, "backspace");
	MapSymbol(SDL_SCANCODE_TAB, eKI_Tab, "tab");
	// MapSymbol(SDLK_CLEAR clear);
	MapSymbol(SDL_SCANCODE_RETURN, eKI_Enter, "enter");
	MapSymbol(SDL_SCANCODE_PAUSE, eKI_Pause, "pause");
	MapSymbol(SDL_SCANCODE_ESCAPE, eKI_Escape, "escape");
	MapSymbol(SDL_SCANCODE_SPACE, eKI_Space, "space");
	//MapSymbol(SDLK_EXCLAIM '!' exclamation mark);
	//MapSymbol(SDLK_QUOTEDBL '"' double quote);
	//MapSymbol(SDLK_HASH '#' hash);
	//MapSymbol(SDLK_DOLLAR '$' dollar);
	//MapSymbol(SDLK_AMPERSAND '&' ampersand);
	//MapSymbol(SDLK_QUOTE '\'' single quote);
	//MapSymbol(SDLK_LEFTPAREN '(' left parenthesis);
	//MapSymbol(SDLK_RIGHTPAREN ')' right parenthesis);
	//MapSymbol(SDLK_ASTERISK '*' asterisk);
	//MapSymbol(SDLK_PLUS '+' plus sign);
	MapSymbol(SDL_SCANCODE_COMMA, eKI_Comma, "comma");
	MapSymbol(SDL_SCANCODE_MINUS, eKI_Minus, "minus");
	MapSymbol(SDL_SCANCODE_PERIOD, eKI_Period, "period");
	MapSymbol(SDL_SCANCODE_SLASH, eKI_Slash, "slash");
	MapSymbol(SDL_SCANCODE_0, eKI_0, "0");
	MapSymbol(SDL_SCANCODE_1, eKI_1, "1");
	MapSymbol(SDL_SCANCODE_2, eKI_2, "2");
	MapSymbol(SDL_SCANCODE_3, eKI_3, "3");
	MapSymbol(SDL_SCANCODE_4, eKI_4, "4");
	MapSymbol(SDL_SCANCODE_5, eKI_5, "5");
	MapSymbol(SDL_SCANCODE_6, eKI_6, "6");
	MapSymbol(SDL_SCANCODE_7, eKI_7, "7");
	MapSymbol(SDL_SCANCODE_8, eKI_8, "8");
	MapSymbol(SDL_SCANCODE_9, eKI_9, "9");
	//MapSymbol(SDL_SCANCODE_COLON, eKI_Colon, "colon");
	MapSymbol(SDL_SCANCODE_SEMICOLON, eKI_Semicolon, "semicolon");
	//MapSymbol(SDL_SCANCODE_LESS '<' less-than sign);
	MapSymbol(SDL_SCANCODE_EQUALS, eKI_Equals, "equals");
	//MapSymbol(SDL_SCANCODE_GREATER '>' greater-than sign);
	//MapSymbol(SDL_SCANCODE_QUESTION '?' question mark);
	//MapSymbol(SDL_SCANCODE_AT '@' at);
	MapSymbol(SDL_SCANCODE_LEFTBRACKET, eKI_LBracket, "lbracket");
	MapSymbol(SDL_SCANCODE_BACKSLASH, eKI_Backslash, "backslash");
	MapSymbol(SDL_SCANCODE_RIGHTBRACKET, eKI_RBracket, "rbracket");
	//MapSymbol(SDL_SCANCODE_CARET '^' caret);
	//MapSymbol(SDL_SCANCODE_UNDERSCORE, eKI_Underline, "underline");
	MapSymbol(SDL_SCANCODE_GRAVE, eKI_Tilde, "tilde"); // Yes, this is correct
	MapSymbol(SDL_SCANCODE_A, eKI_A, "a");
	MapSymbol(SDL_SCANCODE_B, eKI_B, "b");
	MapSymbol(SDL_SCANCODE_C, eKI_C, "c");
	MapSymbol(SDL_SCANCODE_D, eKI_D, "d");
	MapSymbol(SDL_SCANCODE_E, eKI_E, "e");
	MapSymbol(SDL_SCANCODE_F, eKI_F, "f");
	MapSymbol(SDL_SCANCODE_G, eKI_G, "g");
	MapSymbol(SDL_SCANCODE_H, eKI_H, "h");
	MapSymbol(SDL_SCANCODE_I, eKI_I, "i");
	MapSymbol(SDL_SCANCODE_J, eKI_J, "j");
	MapSymbol(SDL_SCANCODE_K, eKI_K, "k");
	MapSymbol(SDL_SCANCODE_L, eKI_L, "l");
	MapSymbol(SDL_SCANCODE_M, eKI_M, "m");
	MapSymbol(SDL_SCANCODE_N, eKI_N, "n");
	MapSymbol(SDL_SCANCODE_O, eKI_O, "o");
	MapSymbol(SDL_SCANCODE_P, eKI_P, "p");
	MapSymbol(SDL_SCANCODE_Q, eKI_Q, "q");
	MapSymbol(SDL_SCANCODE_R, eKI_R, "r");
	MapSymbol(SDL_SCANCODE_S, eKI_S, "s");
	MapSymbol(SDL_SCANCODE_T, eKI_T, "t");
	MapSymbol(SDL_SCANCODE_U, eKI_U, "u");
	MapSymbol(SDL_SCANCODE_V, eKI_V, "v");
	MapSymbol(SDL_SCANCODE_W, eKI_W, "w");
	MapSymbol(SDL_SCANCODE_X, eKI_X, "x");
	MapSymbol(SDL_SCANCODE_Y, eKI_Y, "y");
	MapSymbol(SDL_SCANCODE_Z, eKI_Z, "z");
	MapSymbol(SDL_SCANCODE_DELETE, eKI_Delete, "delete");
	//MapSymbol(SDLK_WORLD_0 world 0);
	//MapSymbol(SDLK_WORLD_1 world 1);
	//MapSymbol(SDLK_WORLD_2 world 2);
	//MapSymbol(SDLK_WORLD_3 world 3);
	//MapSymbol(SDLK_WORLD_4 world 4);
	//MapSymbol(SDLK_WORLD_5 world 5);
	//MapSymbol(SDLK_WORLD_6 world 6);
	//MapSymbol(SDLK_WORLD_7 world 7);
	//MapSymbol(SDLK_WORLD_8 world 8);
	//MapSymbol(SDLK_WORLD_9 world 9);
	//MapSymbol(SDLK_WORLD_10 world 10);
	//MapSymbol(SDLK_WORLD_11 world 11);
	//MapSymbol(SDLK_WORLD_12 world 12);
	//MapSymbol(SDLK_WORLD_13 world 13);
	//MapSymbol(SDLK_WORLD_14 world 14);
	//MapSymbol(SDLK_WORLD_15 world 15);
	//MapSymbol(SDLK_WORLD_16 world 16);
	//MapSymbol(SDLK_WORLD_17 world 17);
	//MapSymbol(SDLK_WORLD_18 world 18);
	//MapSymbol(SDLK_WORLD_19 world 19);
	//MapSymbol(SDLK_WORLD_20 world 20);
	//MapSymbol(SDLK_WORLD_21 world 21);
	//MapSymbol(SDLK_WORLD_22 world 22);
	//MapSymbol(SDLK_WORLD_23 world 23);
	//MapSymbol(SDLK_WORLD_24 world 24);
	//MapSymbol(SDLK_WORLD_25 world 25);
	//MapSymbol(SDLK_WORLD_26 world 26);
	//MapSymbol(SDLK_WORLD_27 world 27);
	//MapSymbol(SDLK_WORLD_28 world 28);
	//MapSymbol(SDLK_WORLD_29 world 29);
	//MapSymbol(SDLK_WORLD_30 world 30);
	//MapSymbol(SDLK_WORLD_31 world 31);
	//MapSymbol(SDLK_WORLD_32 world 32);
	//MapSymbol(SDLK_WORLD_33 world 33);
	//MapSymbol(SDLK_WORLD_34 world 34);
	//MapSymbol(SDLK_WORLD_35 world 35);
	//MapSymbol(SDLK_WORLD_36 world 36);
	//MapSymbol(SDLK_WORLD_37 world 37);
	//MapSymbol(SDLK_WORLD_38 world 38);
	//MapSymbol(SDLK_WORLD_39 world 39);
	//MapSymbol(SDLK_WORLD_40 world 40);
	//MapSymbol(SDLK_WORLD_41 world 41);
	//MapSymbol(SDLK_WORLD_42 world 42);
	//MapSymbol(SDLK_WORLD_43 world 43);
	//MapSymbol(SDLK_WORLD_44 world 44);
	//MapSymbol(SDLK_WORLD_45 world 45);
	//MapSymbol(SDLK_WORLD_46 world 46);
	//MapSymbol(SDLK_WORLD_47 world 47);
	//MapSymbol(SDLK_WORLD_48 world 48);
	//MapSymbol(SDLK_WORLD_49 world 49);
	//MapSymbol(SDLK_WORLD_50 world 50);
	//MapSymbol(SDLK_WORLD_51 world 51);
	//MapSymbol(SDLK_WORLD_52 world 52);
	//MapSymbol(SDLK_WORLD_53 world 53);
	//MapSymbol(SDLK_WORLD_54 world 54);
	//MapSymbol(SDLK_WORLD_55 world 55);
	//MapSymbol(SDLK_WORLD_56 world 56);
	//MapSymbol(SDLK_WORLD_57 world 57);
	//MapSymbol(SDLK_WORLD_58 world 58);
	//MapSymbol(SDLK_WORLD_59 world 59);
	//MapSymbol(SDLK_WORLD_60 world 60);
	//MapSymbol(SDLK_WORLD_61 world 61);
	//MapSymbol(SDLK_WORLD_62 world 62);
	//MapSymbol(SDLK_WORLD_63 world 63);
	//MapSymbol(SDLK_WORLD_64 world 64);
	//MapSymbol(SDLK_WORLD_65 world 65);
	//MapSymbol(SDLK_WORLD_66 world 66);
	//MapSymbol(SDLK_WORLD_67 world 67);
	//MapSymbol(SDLK_WORLD_68 world 68);
	//MapSymbol(SDLK_WORLD_69 world 69);
	//MapSymbol(SDLK_WORLD_70 world 70);
	//MapSymbol(SDLK_WORLD_71 world 71);
	//MapSymbol(SDLK_WORLD_72 world 72);
	//MapSymbol(SDLK_WORLD_73 world 73);
	//MapSymbol(SDLK_WORLD_74 world 74);
	//MapSymbol(SDLK_WORLD_75 world 75);
	//MapSymbol(SDLK_WORLD_76 world 76);
	//MapSymbol(SDLK_WORLD_77 world 77);
	//MapSymbol(SDLK_WORLD_78 world 78);
	//MapSymbol(SDLK_WORLD_79 world 79);
	//MapSymbol(SDLK_WORLD_80 world 80);
	//MapSymbol(SDLK_WORLD_81 world 81);
	//MapSymbol(SDLK_WORLD_82 world 82);
	//MapSymbol(SDLK_WORLD_83 world 83);
	//MapSymbol(SDLK_WORLD_84 world 84);
	//MapSymbol(SDLK_WORLD_85 world 85);
	//MapSymbol(SDLK_WORLD_86 world 86);
	//MapSymbol(SDLK_WORLD_87 world 87);
	//MapSymbol(SDLK_WORLD_88 world 88);
	//MapSymbol(SDLK_WORLD_89 world 89);
	//MapSymbol(SDLK_WORLD_90 world 90);
	//MapSymbol(SDLK_WORLD_91 world 91);
	//MapSymbol(SDLK_WORLD_92 world 92);
	//MapSymbol(SDLK_WORLD_93 world 93);
	//MapSymbol(SDLK_WORLD_94 world 94);
	//MapSymbol(SDLK_WORLD_95 world 95);
	MapSymbol(SDL_SCANCODE_KP_0, eKI_NP_0, "np_0");
	MapSymbol(SDL_SCANCODE_KP_1, eKI_NP_1, "np_1");
	MapSymbol(SDL_SCANCODE_KP_2, eKI_NP_2, "np_2");
	MapSymbol(SDL_SCANCODE_KP_3, eKI_NP_3, "np_3");
	MapSymbol(SDL_SCANCODE_KP_4, eKI_NP_4, "np_4");
	MapSymbol(SDL_SCANCODE_KP_5, eKI_NP_5, "np_5");
	MapSymbol(SDL_SCANCODE_KP_6, eKI_NP_6, "np_6");
	MapSymbol(SDL_SCANCODE_KP_7, eKI_NP_7, "np_7");
	MapSymbol(SDL_SCANCODE_KP_8, eKI_NP_8, "np_8");
	MapSymbol(SDL_SCANCODE_KP_9, eKI_NP_9, "np_9");
	MapSymbol(SDL_SCANCODE_KP_PERIOD, eKI_NP_Period, "np_period");
	MapSymbol(SDL_SCANCODE_KP_DIVIDE, eKI_NP_Divide, "np_divide");
	MapSymbol(SDL_SCANCODE_KP_MULTIPLY, eKI_NP_Multiply, "np_multiply");
	MapSymbol(SDL_SCANCODE_KP_MINUS, eKI_NP_Substract, "np_subtract");
	MapSymbol(SDL_SCANCODE_KP_PLUS, eKI_NP_Add, "np_add");
	MapSymbol(SDL_SCANCODE_KP_ENTER, eKI_NP_Enter, "np_enter");
	MapSymbol(SDL_SCANCODE_KP_EQUALS, eKI_NP_Enter, "np_enter");
	MapSymbol(SDL_SCANCODE_UP, eKI_Up, "up");
	MapSymbol(SDL_SCANCODE_DOWN, eKI_Down, "down");
	MapSymbol(SDL_SCANCODE_RIGHT, eKI_Right, "right");
	MapSymbol(SDL_SCANCODE_LEFT, eKI_Left, "left");
	MapSymbol(SDL_SCANCODE_INSERT, eKI_Insert, "insert");
	MapSymbol(SDL_SCANCODE_HOME, eKI_Home, "home");
	MapSymbol(SDL_SCANCODE_END, eKI_End, "end");
	MapSymbol(SDL_SCANCODE_PAGEUP, eKI_PgUp, "pgup");
	MapSymbol(SDL_SCANCODE_PAGEDOWN, eKI_PgDn, "pgdn");
	MapSymbol(SDL_SCANCODE_F1, eKI_F1, "f1");
	MapSymbol(SDL_SCANCODE_F2, eKI_F2, "f2");
	MapSymbol(SDL_SCANCODE_F3, eKI_F3, "f3");
	MapSymbol(SDL_SCANCODE_F4, eKI_F4, "f4");
	MapSymbol(SDL_SCANCODE_F5, eKI_F5, "f5");
	MapSymbol(SDL_SCANCODE_F6, eKI_F6, "f6");
	MapSymbol(SDL_SCANCODE_F7, eKI_F7, "f7");
	MapSymbol(SDL_SCANCODE_F8, eKI_F8, "f8");
	MapSymbol(SDL_SCANCODE_F9, eKI_F9, "f9");
	MapSymbol(SDL_SCANCODE_F10, eKI_F10, "f10");
	MapSymbol(SDL_SCANCODE_F11, eKI_F11, "f11");
	MapSymbol(SDL_SCANCODE_F12, eKI_F12, "f12");
	MapSymbol(SDL_SCANCODE_F13, eKI_F13, "f13");
	MapSymbol(SDL_SCANCODE_F14, eKI_F14, "f14");
	MapSymbol(SDL_SCANCODE_F15, eKI_F15, "f15");
	MapSymbol(SDL_SCANCODE_NUMLOCKCLEAR, eKI_NumLock, "numlock", SInputSymbol::Toggle, eMM_NumLock);
	MapSymbol(SDL_SCANCODE_CAPSLOCK, eKI_CapsLock, "capslock", SInputSymbol::Toggle, eMM_CapsLock);
	MapSymbol(SDL_SCANCODE_SCROLLLOCK, eKI_ScrollLock, "scrolllock", SInputSymbol::Toggle, eMM_ScrollLock);
	MapSymbol(SDL_SCANCODE_RSHIFT, eKI_RShift, "rshift", SInputSymbol::Button, eMM_RShift);
	MapSymbol(SDL_SCANCODE_LSHIFT, eKI_LShift, "lshift", SInputSymbol::Button, eMM_LShift);
	MapSymbol(SDL_SCANCODE_RCTRL, eKI_RCtrl, "rctrl", SInputSymbol::Button, eMM_RCtrl);
	MapSymbol(SDL_SCANCODE_LCTRL, eKI_LCtrl, "lctrl", SInputSymbol::Button, eMM_LCtrl);
	MapSymbol(SDL_SCANCODE_RALT, eKI_RAlt, "ralt", SInputSymbol::Button, eMM_RAlt);
	MapSymbol(SDL_SCANCODE_LALT, eKI_LAlt, "lalt", SInputSymbol::Button, eMM_LAlt);
	//MapSymbol(SDL_SCANCODE_RMETA right meta);
	//MapSymbol(SDL_SCANCODE_LMETA left meta);
	MapSymbol(SDL_SCANCODE_LGUI, eKI_LWin, "lwin", SInputSymbol::Button, eMM_LWin);
	MapSymbol(SDL_SCANCODE_RGUI, eKI_RWin, "rwin", SInputSymbol::Button, eMM_RWin);
	//MapSymbol(SDL_SCANCODE_MODE mode shift);
	//MapSymbol(SDL_SCANCODE_COMPOSE compose);
	//MapSymbol(SDL_SCANCODE_HELP help);
	MapSymbol(SDL_SCANCODE_PRINTSCREEN, eKI_Print, "print");
	//MapSymbol(SDL_SCANCODE_SYSREQ SysRq);
	MapSymbol(SDL_SCANCODE_PAUSE, eKI_Pause, "pause");
	//MapSymbol(SDL_SCANCODE_MENU menu);
	//MapSymbol(SDL_SCANCODE_POWER power);
	//MapSymbol(SDL_SCANCODE_EURO euro);
	//MapSymbol(SDL_SCANCODE_UNDO undo);
}

#endif
