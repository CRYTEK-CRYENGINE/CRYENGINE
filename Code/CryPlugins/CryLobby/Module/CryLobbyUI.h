// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYLOBBYUI_H__
#define __CRYLOBBYUI_H__

#pragma once

#include <CryLobby/ICryLobbyUI.h>
#include "CryLobby.h"
#include "CryMatchMaking.h"

#define MAX_LOBBYUI_PARAMS 2
#define MAX_LOBBYUI_TASKS  2

typedef uint32 CryLobbyUITaskID;
const CryLobbyUITaskID CryLobbyUIInvalidTaskID = 0xffffffff;

class CCryLobbyUI : public CMultiThreadRefCount, public ICryLobbyUI
{
public:
	CCryLobbyUI(CCryLobby* pLobby, CCryLobbyService* pService);

	virtual ECryLobbyError ShowParty(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                                                                   { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError ShowCommunitySessions(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                                                       { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError ShowOnlineRetailBrowser(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                                                     { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError CheckOnlineRetailStatus(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUIOnlineRetailStatusCallback cb, void* pCbArg)                                                   { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError ShowMessageList(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                                                             { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError GetConsumableOffers(uint32 user, CryLobbyTaskID* pTaskID, const TStoreOfferID* pOfferIDs, uint32 offerIdCount, CryLobbyUIGetConsumableOffersCallback cb, void* pCbArg) { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError ShowDownloadOffer(uint32 user, TStoreOfferID offerId, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                                    { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError GetConsumableAssets(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUIGetConsumableAssetsCallback cb, void* pCbArg)                                                      { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError ConsumeAsset(uint32 user, TStoreAssetID assetID, uint32 quantity, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                        { return eCLE_ServiceNotSupported; }
	virtual void           CancelTask(CryLobbyTaskID lTaskID);

	virtual void           PostLocalizationChecks()                                                                                                                                     {}
	virtual ECryLobbyError AddRecentPlayers(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, const char* gameModeStr, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg) { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError AddActivityFeed(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg)                                                                   { return eCLE_ServiceNotSupported; }

	ECryLobbyError         Initialise();
	ECryLobbyError         Terminate();

	virtual bool           IsDead() const { return false; }

protected:

	struct  STask
	{
		CryLobbyTaskID        lTaskID;
		ECryLobbyError        error;
		uint32                startedTask;
		uint32                subTask;
		CryLobbySessionHandle session;
		void*                 pCb;
		void*                 pCbArg;
		TMemHdl               paramsMem[MAX_LOBBYUI_PARAMS];
		uint32                paramsNum[MAX_LOBBYUI_PARAMS];
		bool                  used;
		bool                  running;
		bool                  canceled;
	};

	ECryLobbyError StartTask(uint32 eTask, bool startRunning, CryLobbyUITaskID* pUITaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	void           FreeTask(CryLobbyUITaskID uiTaskID);
	ECryLobbyError CreateTaskParamMem(CryLobbyUITaskID uiTaskID, uint32 param, const void* pParamData, size_t paramDataSize);
	void           UpdateTaskError(CryLobbyUITaskID uiTaskID, ECryLobbyError error);

	STask*            m_pTask[MAX_LOBBYUI_TASKS];
	CCryLobby*        m_pLobby;
	CCryLobbyService* m_pService;
};

#endif // __CRYLOBBYUI_H__
