// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYDURANGOLIVELOBBY_H__
#define __CRYDURANGOLIVELOBBY_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CryLobby.h"

#if USE_DURANGOLIVE

	#pragma warning(push)
	#pragma warning(disable:28204)
	#include <wrl.h>
	#pragma warning(pop)
	#include <robuffer.h>

	#include "CryDurangoLiveMatchMaking.h"
	#include "CryDurangoLiveOnlineStorage.h"
	#include "CryDurangoLiveLobbyUI.h"

	#pragma warning(push)
	#pragma warning(disable:6103)

enum CryDurangoLiveLobbyPacketType
{
	eDurangoLivePT_SessionRequestJoinResult = eLobbyPT_SessionRequestJoinResult,

	eDurangoLivePT_SessionRequestJoin       = CRYONLINELOBBY_PACKET_START,
	eDurangoLivePT_Chat,      // XXX - ATG

	eDurangoLivePT_EndType
};

struct SCryDurangoLiveUserID : public SCrySharedUserID
{
	SCryDurangoLiveUserID(Live::Xuid num)
	{
		xuid = num;
	}

	virtual bool operator==(const SCryUserID& other) const
	{
		return xuid == ((SCryDurangoLiveUserID&)other).xuid;
	}

	virtual bool operator<(const SCryUserID& other) const
	{
		return xuid < ((SCryDurangoLiveUserID&)other).xuid;
	}

	virtual CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> GetGUIDAsString() const
	{
		CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> temp;

		temp.Format("%" PRId64, xuid);

		return temp;
	}

	SCryDurangoLiveUserID& operator=(const SCryDurangoLiveUserID& other)
	{
		xuid = other.xuid;
		return *this;
	}

	Live::Xuid xuid;
};

class CCryDurangoLiveLobbyService : public CCryLobbyService
{
public:
	CCryDurangoLiveLobbyService(CCryLobby* pLobby, ECryLobbyService service);
	virtual ~CCryDurangoLiveLobbyService();

	virtual ECryLobbyError   Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual ECryLobbyError   Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual void             Tick(CTimeValue tv);
	virtual ICryMatchMaking* GetMatchMaking()
	{
	#if USE_CRY_MATCHMAKING
		return m_pMatchmaking;
	#else // USE_CRY_MATCHMAKING
		return NULL;
	#endif // USE_CRY_MATCHMAKING
	}
	virtual ICryVoice*         GetVoice()  { return NULL; }
	virtual ICryReward*        GetReward() { return NULL; }
	virtual ICryStats*         GetStats()  { return NULL; }
	virtual ICryOnlineStorage* GetOnlineStorage()
	{
	#if USE_CRY_ONLINE_STORAGE
		return m_pOnlineStorage;
	#else
		return NULL;
	#endif // USE_CRY_ONLINE_STORAGE
	}
	virtual ICryLobbyUI*           GetLobbyUI()           { return m_pLobbyUI; }
	virtual ICryFriends*           GetFriends()           { return NULL; }
	virtual ICryFriendsManagement* GetFriendsManagement() { return NULL; }
	virtual CryUserID              GetUserID(uint32 user);
	virtual ECryLobbyError         GetUserPrivileges(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyPrivilegeCallback pCB, void* pCBArg);
	virtual ECryLobbyError         GetSystemTime(uint32 user, SCrySystemTime* pSystemTime);

	virtual void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	virtual void                   OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID);
	virtual void                   OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID);
	void                           GetSocketPorts(uint16& connectPort, uint16& listenPort);

	DWORD                          GetTitleID() const { return m_titleID; }
	HRESULT                        GetServiceConfigID(HSTRING* scid) const
	{
		if (!scid)
			return E_INVALIDARG;

		return WindowsDuplicateString(m_serviceConfigId.Get(), scid);
	}
private:
	void StartTaskRunning(CryLobbyServiceTaskID lsTaskID);
	void StopTaskRunning(CryLobbyServiceTaskID lsTaskID);
	void EndTask(CryLobbyServiceTaskID lsTaskID);

	void StartGetUserPrivileges(CryLobbyServiceTaskID lsTaskID);

	DWORD                             m_titleID;
	Microsoft::WRL::Wrappers::HString m_serviceConfigId;

	#if USE_CRY_MATCHMAKING
	_smart_ptr<CCryDurangoLiveMatchMaking> m_pMatchmaking;
	#endif // USE_CRY_MATCHMAKING
	_smart_ptr<CCryDurangoLiveLobbyUI>     m_pLobbyUI;

	#if USE_CRY_ONLINE_STORAGE
	_smart_ptr<CCryDurangoLiveOnlineStorage> m_pOnlineStorage;
	#endif //USE_CRY_ONLINE_STORAGE
};

extern ECryLobbyError CryDurangoLiveLobbyGetErrorFromDurangoLive(HRESULT error);

	#pragma warning(pop)

#endif // USE_DURANGOLIVE
#endif // __CRYDURANGOLIVELOBBY_H__
