// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Input implementation for Linux using SDL
   -------------------------------------------------------------------------
   History:
   - Jun 09,2006:	Created by Sascha Demetrio

*************************************************************************/
#ifndef __LINUXINPUT_H__
#define __LINUXINPUT_H__
#pragma once

#ifdef USE_LINUXINPUT

	#include "BaseInput.h"
	#include "InputDevice.h"

class CSDLPadManager;
class CSDLMouse;
struct ILog;

	#if !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_IOS
		#define SDL_USE_HAPTIC_FEEDBACK
	#endif

class CLinuxInput : public CBaseInput
{
public:
	CLinuxInput(ISystem* pSystem);

	virtual bool Init() override;
	virtual void ShutDown() override;
	virtual void Update(bool focus) override;
	virtual bool GrabInput(bool bGrab) override;
	virtual int  ShowCursor(const bool bShow) override;

private:
	ISystem*        m_pSystem;
	ILog*           m_pLog;
	CSDLPadManager* m_pPadManager;
	CSDLMouse*      m_pMouse;
};

class CLinuxInputDevice : public CInputDevice
{
public:
	CLinuxInputDevice(CLinuxInput& input, const char* deviceName);
	virtual ~CLinuxInputDevice();

	CLinuxInput& GetLinuxInput() const;
protected:
	void         PostEvent(SInputSymbol* pSymbol, unsigned keyMod = ~0);
private:
	CLinuxInput& m_linuxInput;

};

struct ILog;
struct ICVar;

#endif

#endif

// vim:ts=2:sw=2:tw=78
