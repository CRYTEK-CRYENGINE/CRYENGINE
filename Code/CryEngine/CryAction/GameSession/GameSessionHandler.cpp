// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Default session handler implementation.

   -------------------------------------------------------------------------
   History:
   - 08:12:2009 : Created By Ben Johnson

*************************************************************************/
#include "StdAfx.h"
#include <CryCore/Platform/IPlatformOS.h>
#include "GameSessionHandler.h"
#include "CryAction.h"

//-------------------------------------------------------------------------
CGameSessionHandler::CGameSessionHandler()
{
}

//-------------------------------------------------------------------------
CGameSessionHandler::~CGameSessionHandler()
{
}

//-------------------------------------------------------------------------
void CGameSessionHandler::JoinSessionFromConsole(CrySessionID sessionID)
{
}

//-------------------------------------------------------------------------
int CGameSessionHandler::EndSession()
{
	return 1;
}

//-------------------------------------------------------------------------
int CGameSessionHandler::StartSession()
{
	return 1;
}

//-------------------------------------------------------------------------
void CGameSessionHandler::LeaveSession()
{
}

//-------------------------------------------------------------------------
void CGameSessionHandler::OnUserQuit()
{
}

//-------------------------------------------------------------------------
void CGameSessionHandler::OnGameShutdown()
{
}

//-------------------------------------------------------------------------
CrySessionHandle CGameSessionHandler::GetGameSessionHandle() const
{
	return CrySessionInvalidHandle;
}

//-------------------------------------------------------------------------
bool CGameSessionHandler::ShouldMigrateNub() const
{
	return true;
}

//-------------------------------------------------------------------------
bool CGameSessionHandler::IsMultiplayer() const
{
	return false;
}

//-------------------------------------------------------------------------
int CGameSessionHandler::GetNumberOfExpectedClients()
{
	return 0;
}

//-------------------------------------------------------------------------
bool CGameSessionHandler::IsGameSessionMigrating() const
{
	return false;
}

//-------------------------------------------------------------------------
bool CGameSessionHandler::IsMidGameLeaving() const
{
	return false;
}

bool CGameSessionHandler::ShouldCallMapCommand(const char* pLevelName, const char* pGameRules)
{
	IPlatformOS* pPlatform = gEnv->pSystem->GetPlatformOS();

	SStreamingInstallProgress progress;
	pPlatform->QueryStreamingInstallProgressForLevel(pLevelName, &progress);
	const bool bLevelReady = SStreamingInstallProgress::eState_Completed == progress.m_state;
	if (!bLevelReady)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_NOT_READY, (UINT_PTR)pLevelName, 0);
	}
	return bLevelReady;
}
