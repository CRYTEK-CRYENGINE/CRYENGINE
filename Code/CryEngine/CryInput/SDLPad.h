// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	GamePad Input implementation for Linux using SDL
   -------------------------------------------------------------------------
   History:
   - Jan 20,2014:	Created by Leander Beernaert

*************************************************************************/

#pragma once

#if defined(USE_LINUXINPUT)

	#include <SDL.h>
	#include "LinuxInput.h"
// We need a manager, since all the input for each Game Pad is collected
// in the same queue. If we were to update the game pads seperately
// they would consume each other's events.

class CSDLPad : public CLinuxInputDevice
{
public:

	CSDLPad(CLinuxInput& input, int device);

	virtual ~CSDLPad();

	// IInputDevice overrides
	virtual int  GetDeviceIndex() const { return m_deviceNo; }
	virtual bool Init();
	virtual void Update(bool bFocus);
	virtual void ClearAnalogKeyState(TInputSymbols& clearedSymbols);
	virtual void ClearKeyState();
	virtual bool SetForceFeedback(IFFParams params);
	// ~IInputDevice

	int          GetInstanceId() const;

	void         HandleAxisEvent(const SDL_JoyAxisEvent& evt);

	void         HandleHatEvent(const SDL_JoyHatEvent& evt);

	void         HandleButtonEvent(const SDL_JoyButtonEvent& evt);

	void         HandleConnectionState(const bool connected);
private:
	static float DeadZoneFilter(int input);

	bool         OpenDevice();

	void         CloseDevice();

private:
	SDL_Joystick* m_pSDLDevice;
	SDL_Haptic*   m_pHapticDevice;
	int           m_curHapticEffect;
	int           m_deviceNo;
	int           m_handle;
	bool          m_connected;
	bool          m_supportsFeedback;
	float         m_vibrateTime;
};

class CSDLPadManager
{
public:

	CSDLPadManager(CLinuxInput& input);

	~CSDLPadManager();

	bool Init();

	void Update(bool bFocus);

private:
	bool     AddGamePad(int deviceIndex);

	bool     RemovGamePad(int instanceId);

	CSDLPad* FindPadByInstanceId(int instanceId);
	CSDLPad* FindPadByDeviceIndex(int deviceIndex);
private:

	typedef std::vector<CSDLPad*> GamePadVector;

	CLinuxInput&  m_rLinuxInput;
	GamePadVector m_gamePads;
};

#endif
