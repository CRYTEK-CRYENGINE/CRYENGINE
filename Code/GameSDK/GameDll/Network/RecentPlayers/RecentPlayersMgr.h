// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRECENTPLAYERSMGR_H__
#define __CRECENTPLAYERSMGR_H__

//---------------------------------------------------------------------------

#include "Network/Squad/ISquadEventListener.h"
#include "Network/Lobby/IGameLobbyEventListener.h"
#include <CryCore/Containers/CryFixedArray.h>
#include <CryLobby/ICryLobby.h>
#include "Network/FriendManager/GameFriendData.h"
#include "IRecentPlayersMgrEventListener.h"

//---------------------------------------------------------------------------
// CRecentPlayersMgr
//     A class to manage and store potential new friends a player has encountered since running the game
//---------------------------------------------------------------------------
class CRecentPlayersMgr : public ISquadEventListener, public IGameLobbyEventListener
{
public:
	typedef struct SRecentPlayerData
	{
		CryUserID m_userId;
		TLobbyUserNameString m_name;
		TGameFriendId m_internalId;

		static TGameFriendId __s_friend_internal_ids;

		SRecentPlayerData(CryUserID &inUserId, const char *inNameString)
		{
			m_userId = inUserId;
			m_name = inNameString;
			m_internalId = __s_friend_internal_ids++;
		}
	};
	
	static const int k_maxNumRecentPlayers=50;
	typedef CryFixedArray<SRecentPlayerData, k_maxNumRecentPlayers> TRecentPlayersArray;


	CRecentPlayersMgr();
	virtual ~CRecentPlayersMgr();

	void Update(const float inFrameTime);

	void RegisterRecentPlayersMgrEventListener(IRecentPlayersMgrEventListener *pListener);
	void UnregisterRecentPlayersMgrEventListener(IRecentPlayersMgrEventListener *pListener);

	// ISquadEventListener
	virtual void AddedSquaddie(CryUserID userId);
	virtual void RemovedSquaddie(CryUserID userId) {}
	virtual void UpdatedSquaddie(CryUserID userId);
	virtual void SquadLeaderChanged(CryUserID oldLeaderId, CryUserID newLeaderId) {}
	virtual void SquadNameChanged(const char *pInNewName) { }
	virtual void SquadEvent(ISquadEventListener::ESquadEventType eventType) { }
	// ~ISquadEventListener

	// IGameLobbyEventListener
	virtual void InsertedUser(CryUserID userId, const char *userName);
	virtual void SessionChanged(const CrySessionHandle inOldSession, const CrySessionHandle inNewSession) { }
	// ~IGameLobbyEventListener

	const TRecentPlayersArray *GetRecentPlayers() const { return &m_players; }
	const SRecentPlayerData *FindRecentPlayerDataFromInternalId(TGameFriendId inInternalId);

	void OnUserChanged(CryUserID localUserId);

protected:
	typedef std::vector<IRecentPlayersMgrEventListener*> TRecentPlayersFriendsEventListenersVec;
	TRecentPlayersFriendsEventListenersVec m_recentPlayersEventListeners;

	TRecentPlayersArray m_players;
	CryUserID m_localUserId;

	void AddOrUpdatePlayer(CryUserID &inUserId, const char *inUserName);
	SRecentPlayerData *FindRecentPlayerDataFromUserId(CryUserID &inUserId);
	void EventRecentPlayersUpdated();
};
//---------------------------------------------------------------------------

#endif //__CRECENTPLAYERSMGR_H__
