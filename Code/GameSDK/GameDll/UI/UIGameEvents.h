// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIGameEvents.h
//  Version:     v1.00
//  Created:     19/03/2012 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __UIGameEvents_H__
#define __UIGameEvents_H__

#include "IUIGameEventSystem.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include <ILevelSystem.h>

class CUIGameEvents 
	: public IUIGameEventSystem
{
public:
	CUIGameEvents();

	// IUIGameEventSystem
	UIEVENTSYSTEM( "UIGameEvents" );
	virtual void InitEventSystem() override;
	virtual void UnloadEventSystem() override;

private:
	// UI events
	void OnLoadLevel( const char* mapname, bool isServer, const char* gamerules );
	void OnReloadLevel();
	void OnSaveGame( bool shouldResume );
	void OnLoadGame( bool shouldResume );
	void OnPauseGame();
	void OnResumeGame();
	void OnExitGame();
	void OnStartGame();

private:
	IUIEventSystem* m_pUIEvents;
	SUIEventReceiverDispatcher<CUIGameEvents> m_eventDispatcher;

	IGameFramework* m_pGameFramework;
	ILevelSystem* m_pLevelSystem;
};


#endif // __UIGameEvents_H__
