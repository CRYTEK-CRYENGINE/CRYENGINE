// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Keyboard for Windows/DirectX
   -------------------------------------------------------------------------
   History:
   - Dec 05,2005:	Major rewrite by Marco Koegler

*************************************************************************/

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__
#pragma once

#ifdef USE_DXINPUT

	#include "DXInputDevice.h"

class CDXInput;
struct  SInputSymbol;

class CKeyboard : public CDXInputDevice
{
	struct SScanCode
	{
		uint32 lc;    // lowercase
		uint32 uc;    // uppercase
		uint32 ac;    // alt gr
		uint32 cl;    // caps lock (differs slightly from uppercase)
	};
public:
	CKeyboard(CDXInput& input);

	// IInputDevice overrides
	virtual int         GetDeviceIndex() const { return 0; } //Assume only one keyboard
	virtual bool        Init();
	virtual void        Update(bool bFocus);
	virtual bool        SetExclusiveMode(bool value);
	virtual void        ClearKeyState();
	virtual uint32      GetInputCharUnicode(const SInputEvent& event);
	virtual const char* GetOSKeyName(const SInputEvent& event);
	virtual void        OnLanguageChange();
	// ~IInputDevice

public:
	uint32 Event2Unicode(const SInputEvent& event);
	void   RebuildScanCodeTable(HKL layout);

protected:
	void SetupKeyNames();
	void ProcessKey(uint32 devSpecId, bool pressed);

private:
	static SScanCode     m_scanCodes[256];
	static SInputSymbol* Symbol[256];
	DWORD                m_baseflags;
	static int           s_disableWinKeys;
	static CKeyboard*    s_instance;

	void        ChangeDisableWinKeys(ICVar* pVar);
	static void ChangeDisableWinKeysCallback(ICVar* pVar);
	DWORD       GetDeviceFlags() const;

};

#endif // USE_DXINPUT

#endif // __KEYBOARD_H__
