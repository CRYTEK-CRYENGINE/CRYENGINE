// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYVOICE_H__
#define __CRYVOICE_H__

#pragma once

#include <CryLobby/ICryVoice.h>
#include "CryLobby.h"
#include "CryMatchMaking.h"

#if USE_CRY_VOICE

	#define MAX_LOCAL_TALKERS    4
	#define MAX_REMOTE_TALKERS   16

	#define NET_MICROPHONE_DEBUG 0

typedef uint32 CryVoiceRemoteUserID;
const CryVoiceRemoteUserID CryVoiceInvalidRemoteUserID = 0xffffffff;

class CCryVoice : public CMultiThreadRefCount, public ICryVoice
{
public:
	CCryVoice(CCryLobby* pLobby, CCryLobbyService* pService);

	virtual void  Mute(uint32 localUser, CryUserID remoteUser, bool mute);
	virtual void  MuteLocalPlayer(uint32 localUser, bool mute);
	virtual bool  IsMuted(uint32 localUser, CryUserID remoteUser);
	virtual void  MuteExternally(uint32 localUser, CryUserID remoteUser, bool mute);
	virtual bool  IsMutedExternally(uint32 localUser, CryUserID remoteUser);
	virtual bool  IsSpeaking(uint32 localUser, CryUserID remoteUser);
	virtual bool  IsSpeaking(uint32 localUser) = 0;
	virtual bool  IsMicrophoneConnected(CryUserID userID);

	virtual void  EnablePushToTalk(uint32 localUser, bool bSet)          {}
	virtual bool  IsPushToTalkEnabled(uint32 localUser)                  { return false; }
	virtual void  PushToTalkButtonDown(uint32 localUser)                 {}
	virtual void  PushToTalkButtonUp(uint32 localUser)                   {}
	virtual void  SetVoicePlaybackVolume(uint32 localUser, float volume) {}
	virtual float GetVoicePlaybackVolume(uint32 localUser)               { return 0.0f; }

protected:
	ECryLobbyError       Initialise();
	CryVoiceRemoteUserID RegisterRemoteUser(CryLobbyConnectionID connectionID, CryUserID remoteUser);
	void                 UnRegisterRemoteUser(CryVoiceRemoteUserID user);
	CryVoiceRemoteUserID GetRemoteUser(CryUserID remoteUser);

	virtual void         SendMicrophoneNotification(uint32 remoteUserID) = 0;
	virtual void         SendMicrophoneRequest(uint32 remoteUserID) = 0;

	void                 Tick(CTimeValue tv);

	enum eUserFlags
	{
		UF_REGISTERED                = BIT(0),
		UF_IS_SPEAKING               = BIT(1),
		UF_IS_MICROPHONE_CONNECTED   = BIT(2),
		UF_REQUEST_MICROPHONE_STATUS = BIT(3),
		UF_SEND_MICROPHONE_STATUS    = BIT(4),

		UF_PLATFORM_FLAGS_START_BIT  = 16   // Not a bit
	};

	enum eLocalUserFlags
	{
		LUF_IS_MUTED = BIT(0),
	};

	struct  SRemoteUser
	{
		CryLobbyConnectionID m_connectionID;
		CryUserID            m_userID;
		uint32               m_remoteUserFlags;
	};

	enum eRelationshipFlags
	{
		RF_MUTED_GAME               = BIT(0),
		RF_MUTED_EXTERNAL           = BIT(1),
		RF_PLAYBACK_ON              = BIT(2),

		RF_PLATFORM_FLAGS_START_BIT = 16    // Not a bit
	};

	struct  SUserRelationship
	{
		uint32 m_relationshipFlags;
	};

	CTimeValue         m_notificationTimer;
	CCryLobby*         m_pLobby;
	CCryLobbyService*  m_pService;
	SRemoteUser*       m_pRemoteUsers[MAX_REMOTE_TALKERS];
	uint16             m_localUserFlags[MAX_LOCAL_TALKERS];
	SUserRelationship* m_pUserRelationship[MAX_LOCAL_TALKERS][MAX_REMOTE_TALKERS];

	uint32             m_numLocalTalkers;
	uint32             m_numRemoteTalkers;

};

#endif // USE_CRY_VOICE
#endif // __CRYVOICE_H__
