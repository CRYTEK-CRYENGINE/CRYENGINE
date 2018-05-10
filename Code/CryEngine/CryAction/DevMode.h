// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Helper class for CCryAction implementing developer mode-only
                functionality

   -------------------------------------------------------------------------
   History:
   - 9:2:2005   12:31 : Created by Craig Tiller

*************************************************************************/
#ifndef __DEVMODE_H__
#define __DEVMODE_H__

#pragma once

#include <CryInput/IInput.h>

struct STagFileEntry
{
	Vec3 pos;
	Ang3 ang;
};

class CDevMode : public IInputEventListener, public IRemoteConsoleListener
{
public:
	CDevMode();
	~CDevMode();

	void GotoTagPoint(int i);
	void SaveTagPoint(int i);

	// IInputEventListener
	bool OnInputEvent(const SInputEvent&);
	// ~IInputEventListener

	// IRemoteConsoleListener
	virtual void OnGameplayCommand(const char* cmd);
	// ~IRemoteConsoleListener

	void GetMemoryStatistics(ICrySizer* s) { s->Add(*this); }

private:
	bool m_bSlowDownGameSpeed;
	bool m_bHUD;
	std::vector<STagFileEntry> LoadTagFile();
	void                       SaveTagFile(const std::vector<STagFileEntry>&);
	string                     TagFileName();
	void                       SwitchSlowDownGameSpeed();
	void                       SwitchHUD();
	void                       GotoSpecialSpawnPoint(int i);
};

#endif
