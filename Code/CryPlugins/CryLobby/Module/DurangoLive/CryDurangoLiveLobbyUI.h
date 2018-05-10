// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: CCryLobbyUI implementation for Durango

   -------------------------------------------------------------------------
   History:
   - 13:06:2013 : Created by Yeonwoon JUNG

*************************************************************************/

#ifndef __CRYDURANGOLIVELOBBYUI_H__
#define __CRYDURANGOLIVELOBBYUI_H__

#pragma once

#include "CryLobbyUI.h"

#if USE_DURANGOLIVE

class CCryDurangoLiveLobbyUI : public CCryLobbyUI
{
public:
	CCryDurangoLiveLobbyUI(CCryLobby* pLobby, CCryLobbyService* pService);
	void                   Tick(CTimeValue tv);

	virtual ECryLobbyError ShowGamerCard(uint32 user, CryUserID userID, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                            { return eCLE_InvalidRequest; }
	virtual ECryLobbyError ShowGameInvite(uint32 user, CrySessionHandle h, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg) { return eCLE_InvalidRequest; }
	virtual ECryLobbyError ShowFriends(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                                                { return eCLE_InvalidRequest; }
	virtual ECryLobbyError ShowFriendRequest(uint32 user, CryUserID userID, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                        { return eCLE_InvalidRequest; }
	virtual ECryLobbyError SetRichPresence(uint32 user, SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);

protected:
	enum ETask
	{
		eT_SetRichPresence,
	};

	struct  STask : public CCryLobbyUI::STask
	{
		uint32 user;
	};

	ECryLobbyError StartTask(ETask etask, bool startRunning, uint32 user, CryLobbyUITaskID* pUITaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	void           StartTaskRunning(CryLobbyUITaskID uiTaskID);
	void           EndTask(CryLobbyUITaskID uiTaskID);
	void           StopTaskRunning(CryLobbyUITaskID uiTaskID);

	void           StartSetRichPresence(CryLobbyUITaskID uiTaskID);

	STask m_task[MAX_LOBBYUI_TASKS];

private:
	typedef std::map<int, wstring> TRichPresence;
	TRichPresence m_richPresence;
};

#endif//USE_DURANGOLIVE

#endif // __CRYDURANGOLIVELOBBYUI_H__
