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
#include "StdAfx.h"

#ifdef USE_LINUXINPUT

	#include "LinuxInput.h"

	#include <math.h>

	#include <CrySystem/IConsole.h>
	#include <CrySystem/ILog.h>
	#include <CrySystem/ISystem.h>
	#include <CryRenderer/IRenderer.h>

	#include <SDL.h>

	#include "SDLKeyboard.h"
	#include "SDLMouse.h"
	#include "SDLPad.h"

CLinuxInput::CLinuxInput(ISystem* pSystem) : CBaseInput()
{
	m_pSystem = pSystem;
	m_pLog = pSystem->GetILog();
};

bool CLinuxInput::Init()
{
	m_pLog->Log("Initializing LinuxInput/SDL");

	uint32 flags = SDL_INIT_EVENTS | SDL_INIT_JOYSTICK;
	#if defined(SDL_USE_HAPTIC_FEEDBACK)
	flags |= SDL_INIT_HAPTIC;
	#endif

	if (SDL_InitSubSystem(flags) != 0)
	{
		m_pLog->Log("Error: Initializing SDL Subsystems:%s", SDL_GetError());
		return false;
	}

	if (!CBaseInput::Init())
	{
		m_pLog->Log("Error: CBaseInput::Init failed");
		return false;
	}
	CSDLMouse* pMouse = new CSDLMouse(*this);
	if (AddInputDevice(pMouse))
	{
		m_pMouse = pMouse;
	}
	else
	{
		m_pLog->Log("Error: Initializing SDL Mouse");
		delete pMouse;
		return false;
	}
	CSDLKeyboard* pKeyboard = new CSDLKeyboard(*this);
	if (!AddInputDevice(pKeyboard))
	{
		delete pKeyboard;
		m_pLog->Log("Error: Initializing SDL Keyboard");
		return false;
	}

	m_pPadManager = new CSDLPadManager(*this);
	if (!m_pPadManager->Init())
	{
		delete m_pPadManager;
		m_pLog->Log("Error: Initializing SDL GamePad Manager");
		return false;
	}

	return true;
}

void CLinuxInput::ShutDown()
{

	m_pLog->Log("LinuxInput/SDL Shutdown");
	if (m_pPadManager) delete m_pPadManager;
	CBaseInput::ShutDown();
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC);
	delete this;
}

CLinuxInputDevice::CLinuxInputDevice(
  CLinuxInput& input,
  const char* deviceName) :
	CInputDevice(input, deviceName),
	m_linuxInput(input)
{}

CLinuxInputDevice::~CLinuxInputDevice()
{}

CLinuxInput& CLinuxInputDevice::GetLinuxInput() const
{
	return m_linuxInput;
}

void CLinuxInputDevice::PostEvent(SInputSymbol* pSymbol, unsigned keyMod)
{
	SInputEvent event;
	event.keyName = pSymbol->name;
	event.state = pSymbol->state;
	event.deviceType = pSymbol->deviceType;
	event.modifiers = CSDLKeyboard::ConvertModifiers(keyMod);
	event.value = pSymbol->value;
	event.keyId = pSymbol->keyId;
	GetLinuxInput().PostInputEvent(event);
}

void CLinuxInput::Update(bool bFocus)
{

	SDL_PumpEvents();
	m_pPadManager->Update(bFocus);
	CBaseInput::Update(bFocus);
	SDL_Event eventList[32];
	int nEvents;
	nEvents = SDL_PeepEvents(eventList, 32, SDL_GETEVENT, SDL_QUIT, SDL_QUIT);
	if (nEvents == -1)
	{
		gEnv->pLog->LogError("SDL_GETEVENT error: %s", SDL_GetError());
		return;
	}
	for (int i = 0; i < nEvents; ++i)
	{
		if (eventList[i].type == SDL_QUIT)
		{
			gEnv->pSystem->Quit();
			return;
		}
		else
		{
			// Unexpected event type.
			abort();
		}
	}
}

bool CLinuxInput::GrabInput(bool bGrab)
{
	bool bSuccess = false;
	if (m_pMouse)
	{
		bSuccess = true;
		if (bGrab)
			m_pMouse->GrabInput();
		else
			m_pMouse->UngrabInput();
	}
	return bSuccess;
}

int CLinuxInput::ShowCursor(const bool bShow)
{
	static int displayCounter = 0;
	if (bShow)
		++displayCounter;
	else
		--displayCounter;

	SDL_ShowCursor(displayCounter >= 0 ? SDL_ENABLE : SDL_DISABLE);

	return displayCounter;
}

#endif

// vim:ts=2:sw=2:tw=78
