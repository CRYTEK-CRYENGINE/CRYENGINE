// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSTEAMVOICE_H__
#define __CRYSTEAMVOICE_H__

#ifdef USE_FMOD
	#define USE_STEAM_VOICE 1
#else
	#define USE_STEAM_VOICE 0
#endif // USE_FMOD

#pragma once

#if USE_CRY_VOICE && USE_STEAM && USE_STEAM_VOICE

	#include "CryVoice.h"
	#include "IAudioDevice.h"

	#define MAX_STEAM_LOCAL_USERS    1
	#define MAX_STEAM_REMOTE_TALKERS 16

class ISoundPlayer;

class CCrySteamVoice : public CCryVoice
{
public:
	CCrySteamVoice(CCryLobby* pLobby, CCryLobbyService* pService);
	virtual ~CCrySteamVoice();
	void                 OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	ECryLobbyError       Initialise();
	ECryLobbyError       Terminate();
	virtual bool         IsSpeaking(uint32 localUser)            { return false; }
	virtual bool         IsMicrophoneConnected(CryUserID userID) { return false; }

	virtual void         Mute(uint32 localUser, CryUserID remoteUser, bool mute);
	virtual void         MuteLocalPlayer(uint32 localUser, bool mute);
	void                 Tick(CTimeValue tv);

	CryVoiceRemoteUserID RegisterRemoteUser(CryLobbyConnectionID connectionID, CryUserID remoteUserID) { return CCryVoice::RegisterRemoteUser(connectionID, remoteUserID); }
protected:

	virtual void SendMicrophoneNotification(uint32 remoteUserID) {};
	virtual void SendMicrophoneRequest(uint32 remoteUserID)      {};

private:
	ISoundPlayer*     m_pSoundPlayer;
	SRemoteUser       m_SteamRemoteUsers[MAX_STEAM_REMOTE_TALKERS];
	SUserRelationship m_userRelationship[MAX_STEAM_LOCAL_USERS][MAX_STEAM_REMOTE_TALKERS];
};

#endif // USE_CRY_VOICE
#endif // __CRYSTEAMVOICE_H__
