// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_LOBBYUI_H__
#define __CRYPSN2_LOBBYUI_H__
#pragma once

#include "CryLobbyUI.h"

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#define PSN_INVALID_COMMERCE_CONTEXT    0
		#define PSN_INVALID_COMMERCE_REQUEST_ID 0

class CCryPSNLobbyUI : public CCryLobbyUI
{
public:
	CCryPSNLobbyUI(CCryLobby* pLobby, CCryLobbyService* pService, CCryPSNSupport* pSupport);

	ECryLobbyError         Initialise();
	ECryLobbyError         Terminate();
	void                   Tick(CTimeValue tv);

	virtual ECryLobbyError ShowGamerCard(uint32 user, CryUserID userID, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError ShowGameInvite(uint32 user, CrySessionHandle h, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError ShowFriends(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError ShowFriendRequest(uint32 user, CryUserID userID, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError SetRichPresence(uint32 user, SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError ShowOnlineRetailBrowser(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError CheckOnlineRetailStatus(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUIOnlineRetailStatusCallback cb, void* pCbArg);
	virtual ECryLobbyError ShowMessageList(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual void           PostLocalizationChecks();
	virtual ECryLobbyError AddRecentPlayers(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, const char* gameModeStr, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError AddActivityFeed(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);

	virtual ECryLobbyError GetConsumableOffers(uint32 user, CryLobbyTaskID* pTaskID, const TStoreOfferID* pOfferIDs, uint32 offerIdCount, CryLobbyUIGetConsumableOffersCallback cb, void* pCbArg);
	virtual ECryLobbyError ShowDownloadOffer(uint32 user, TStoreOfferID offerId, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);
	virtual ECryLobbyError GetConsumableAssets(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyUIGetConsumableAssetsCallback cb, void* pCbArg);
	virtual ECryLobbyError ConsumeAsset(uint32 user, TStoreAssetID assetID, uint32 quantity, CryLobbyTaskID* pTaskID, CryLobbyUICallback cb, void* pCbArg);

	virtual bool           IsDead() const { return false; }

	static void            SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg);

protected:
	enum ETask
	{
		eT_ShowGamerCard,
		eT_ShowGameInvite,
		eT_ShowFriends,
		eT_ShowFriendRequest,
		eT_SetRichPresence,
		eST_WaitingForRichPresenceCallback,
		eT_ShowOnlineRetailBrowser,
		eT_CheckOnlineRetailStatus,
		eT_JoinFriendsGame,
		eT_ShowMessageList,
		eT_AddRecentPlayers,
		eST_WaitingForAddRecentPlayersCallback,
		eT_AddActivityFeed,
		eST_WaitingForAddActivityFeedCallback,
		eT_GetConsumableOffers,
		eST_WaitingForConsumableOffersCallback,
		eT_GetConsumableAssets,
		eST_WaitingForConsumableAssetsCallback,
		eT_ConsumeAsset,
		eST_WaitingForConsumeAssetCallback,
		eT_ShowDownloadOffer,
	};

	ECryLobbyError StartTask(ETask etask, bool startRunning, uint32 user, CryLobbyUITaskID* pUITaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	void           StartSubTask(ETask etask, CryLobbyUITaskID sTaskID);
	void           StartTaskRunning(CryLobbyUITaskID uiTaskID);
	void           EndTask(CryLobbyUITaskID uiTaskID);
	void           StopTaskRunning(CryLobbyUITaskID uiTaskID);

	void           StartShowGamerCard(CryLobbyUITaskID uiTaskID);
	void           TickShowGamerCard(CryLobbyUITaskID uiTaskID);
	void           EndShowGamerCard(CryLobbyUITaskID uiTaskID);

	void           StartShowGameInvite(CryLobbyUITaskID uiTaskID);
	void           TickShowGameInvite(CryLobbyUITaskID uiTaskID);
	void           EndShowGameInvite(CryLobbyUITaskID uiTaskID);

	void           StartShowFriends(CryLobbyUITaskID uiTaskID);
	void           TickShowFriends(CryLobbyUITaskID uiTaskID);
	void           EndShowFriends(CryLobbyUITaskID uiTaskID);

	void           StartShowFriendRequest(CryLobbyUITaskID uiTaskID);

	void           StartSetRichPresence(CryLobbyUITaskID uiTaskID);
	void           TickSetRichPresence(CryLobbyUITaskID uiTaskID);

	void           StartShowOnlineRetailBrowser(CryLobbyUITaskID uiTaskID);
	void           StartShowMessageList(CryLobbyUITaskID uiTaskID);

	void           StartCheckOnlineRetailStatus(CryLobbyUITaskID uiTaskID);
	void           EndCheckOnlineRetailStatus(CryLobbyUITaskID uiTaskID);

	void           JoinFriendGame(const SceNpId* pNpId);
	void           StartJoinFriendGame(CryLobbyUITaskID uiTaskID);
	void           DispatchJoinEvent(CrySessionID session, ECryLobbyError error);

	void           StartAddRecentPlayers(CryLobbyUITaskID uiTaskID);
	void           TickAddRecentPlayers(CryLobbyUITaskID uiTaskID);

	void           StartAddActivityFeed(CryLobbyUITaskID uiTaskID);
	void           TickAddActivityFeed(CryLobbyUITaskID uiTaskID);

	void           StartGetConsumableOffers(CryLobbyUITaskID uiTaskID);
	void           TickGetConsumableOffers(CryLobbyUITaskID uiTaskID);
	void           EventWebApiGetConsumableOffers(CryLobbyUITaskID uiTaskID, SCryPSNSupportCallbackEventData& data);
	void           EndGetConsumableOffers(CryLobbyUITaskID uiTaskID);

	void           StartGetConsumableAssets(CryLobbyUITaskID uiTaskID);
	void           TickGetConsumableAssets(CryLobbyUITaskID uiTaskID);
	void           EventWebApiGetConsumableAssets(CryLobbyUITaskID uiTaskID, SCryPSNSupportCallbackEventData& data);
	void           EndGetConsumableAssets(CryLobbyUITaskID uiTaskID);

	void           StartConsumeAsset(CryLobbyUITaskID uiTaskID);
	void           TickConsumeAsset(CryLobbyUITaskID uiTaskID);
	void           EventWebApiConsumeAsset(CryLobbyUITaskID uiTaskID, SCryPSNSupportCallbackEventData& data);
	void           EndConsumeAsset(CryLobbyUITaskID uiTaskID);

	void           StartShowDownloadOffer(CryLobbyUITaskID uiTaskID);
	void           TickShowDownloadOffer(CryLobbyUITaskID uiTaskID);
	void           EndShowDownloadOffer(CryLobbyUITaskID uiTaskID);

	STask           m_task[MAX_LOBBYUI_TASKS];

	CCryPSNSupport* m_pPSNSupport;
};

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_LOBBYUI_H__
