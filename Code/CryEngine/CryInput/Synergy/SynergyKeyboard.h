// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SYNERGYKEYBOARD_H__
#define __SYNERGYKEYBOARD_H__

#ifdef USE_SYNERGY_INPUT

	#include "InputDevice.h"

struct IInput;
class CSynergyContext;

class CSynergyKeyboard : public CInputDevice
{
public:
	CSynergyKeyboard(IInput& input, CSynergyContext* pContext);
	virtual ~CSynergyKeyboard();

	// IInputDevice overrides
	virtual int    GetDeviceIndex() const { return eIDT_Keyboard; }
	virtual bool   Init();
	virtual void   Update(bool bFocus);
	virtual uint32 GetInputCharUnicode(const SInputEvent& event);
	// ~IInputDevice

private:
	_smart_ptr<CSynergyContext> m_pContext;
	void   SetupKeys();
	void   ProcessKey(uint32 key, bool bPressed, bool bRepeat, uint32 modificators);
	uint32 PackModificators(uint32 modificators);
	void   TypeASCIIString(const char* pString);
};

#endif // USE_SYNERGY_INPUT

#endif //__SYNERGYKEYBOARD_H__
