// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryVoice.h"

#if USE_CRY_VOICE

CCryVoice::CCryVoice(CCryLobby* pLobby, CCryLobbyService* pService)
{
	assert(pLobby);
	assert(pService);

	m_pLobby = pLobby;
	m_pService = pService;

	for (uint32 i = 0; i < MAX_REMOTE_TALKERS; i++)
	{
		m_pRemoteUsers[i] = NULL;
	}

	for (uint32 i = 0; i < MAX_LOCAL_TALKERS; i++)
	{
		m_localUserFlags[i] = LUF_IS_MUTED; // Initialize with muted mic so the user doesn't accidentally sends voice data unwanted

		for (uint32 j = 0; j < MAX_REMOTE_TALKERS; j++)
		{
			m_pUserRelationship[i][j] = NULL;
		}
	}
}

ECryLobbyError CCryVoice::Initialise()
{
	m_notificationTimer = CTimeValue(0.0f);

	for (uint32 i = 0; i < m_numRemoteTalkers; i++)
	{
		assert(m_pRemoteUsers[i]);
		m_pRemoteUsers[i]->m_remoteUserFlags = 0;
		m_pRemoteUsers[i]->m_userID = NULL;
	}

	return eCLE_Success;
}

CryVoiceRemoteUserID CCryVoice::RegisterRemoteUser(CryLobbyConnectionID connectionID, CryUserID remoteUserID)
{
	CryVoiceRemoteUserID id = GetRemoteUser(remoteUserID);

	if (id == CryVoiceInvalidRemoteUserID)
	{
		for (uint32 i = 0; i < m_numRemoteTalkers; i++)
		{
			SRemoteUser* pRemoteUser = m_pRemoteUsers[i];

			assert(pRemoteUser);

			if (!(pRemoteUser->m_remoteUserFlags & UF_REGISTERED))
			{
				pRemoteUser->m_remoteUserFlags |= (UF_REGISTERED | UF_REQUEST_MICROPHONE_STATUS);
				pRemoteUser->m_remoteUserFlags &= ~(UF_IS_SPEAKING | UF_IS_MICROPHONE_CONNECTED | UF_SEND_MICROPHONE_STATUS);
				pRemoteUser->m_connectionID = connectionID;
				pRemoteUser->m_userID = remoteUserID;

				for (uint32 j = 0; j < m_numLocalTalkers; j++)
				{
					SUserRelationship* pRelationship = m_pUserRelationship[j][i];

					assert(pRelationship);

					pRelationship->m_relationshipFlags |= (RF_MUTED_GAME | RF_PLAYBACK_ON);
					pRelationship->m_relationshipFlags &= ~RF_MUTED_EXTERNAL;
				}

				return i;
			}
		}
	}

	return id;
}

void CCryVoice::UnRegisterRemoteUser(CryVoiceRemoteUserID user)
{
	assert(m_pRemoteUsers[user]);

	m_pRemoteUsers[user]->m_remoteUserFlags &= ~UF_REGISTERED;
	m_pRemoteUsers[user]->m_remoteUserFlags &= ~UF_SEND_MICROPHONE_STATUS;
	m_pRemoteUsers[user]->m_remoteUserFlags &= ~UF_REQUEST_MICROPHONE_STATUS;
	m_pRemoteUsers[user]->m_userID = NULL;
}

CryVoiceRemoteUserID CCryVoice::GetRemoteUser(CryUserID remoteUser)
{
	for (uint32 i = 0; i < m_numRemoteTalkers; i++)
	{
		SRemoteUser* pRemoteUser = m_pRemoteUsers[i];

		assert(pRemoteUser);

		if ((pRemoteUser->m_remoteUserFlags & UF_REGISTERED) && pRemoteUser->m_userID.IsValid() && (pRemoteUser->m_userID == remoteUser))
		{
			return i;
		}
	}

	return CryVoiceInvalidRemoteUserID;
}

void CCryVoice::Mute(uint32 localUser, CryUserID remoteUser, bool mute)
{
	CryVoiceRemoteUserID remoteUserID = GetRemoteUser(remoteUser);

	if (remoteUserID != CryVoiceInvalidRemoteUserID)
	{
		assert(m_pUserRelationship[localUser][remoteUserID]);
		if (mute)
		{
			m_pUserRelationship[localUser][remoteUserID]->m_relationshipFlags |= RF_MUTED_GAME;
		}
		else
		{
			m_pUserRelationship[localUser][remoteUserID]->m_relationshipFlags &= ~RF_MUTED_GAME;
		}
	}
}

void CCryVoice::MuteLocalPlayer(uint32 localUser, bool mute)
{
	if (mute)
	{
		m_localUserFlags[localUser] |= LUF_IS_MUTED;
	}
	else
	{
		m_localUserFlags[localUser] &= ~LUF_IS_MUTED;
	}
}

bool CCryVoice::IsMuted(uint32 localUser, CryUserID remoteUser)
{
	CryVoiceRemoteUserID remoteUserID = GetRemoteUser(remoteUser);

	if (remoteUserID != CryVoiceInvalidRemoteUserID)
	{
		assert(m_pUserRelationship[localUser][remoteUserID]);
		return ((m_pUserRelationship[localUser][remoteUserID]->m_relationshipFlags & RF_MUTED_GAME) == RF_MUTED_GAME);
	}

	return false;
}

void CCryVoice::MuteExternally(uint32 localUser, CryUserID remoteUser, bool mute)
{
	//-- Does nothing
}

bool CCryVoice::IsMutedExternally(uint32 localUser, CryUserID remoteUser)
{
	CryVoiceRemoteUserID remoteUserID = GetRemoteUser(remoteUser);

	if (remoteUserID != CryVoiceInvalidRemoteUserID)
	{
		assert(m_pUserRelationship[localUser][remoteUserID]);
		return ((m_pUserRelationship[localUser][remoteUserID]->m_relationshipFlags & RF_MUTED_EXTERNAL) == RF_MUTED_EXTERNAL);
	}

	return false;
}

bool CCryVoice::IsSpeaking(uint32 localUser, CryUserID remoteUser)
{
	CryVoiceRemoteUserID remoteUserID = GetRemoteUser(remoteUser);

	if (remoteUserID != CryVoiceInvalidRemoteUserID)
	{
		assert(m_pRemoteUsers[remoteUserID]);
		assert(m_pUserRelationship[localUser][remoteUserID]);

		return (((m_pRemoteUsers[remoteUserID]->m_remoteUserFlags & (UF_REGISTERED | UF_IS_SPEAKING)) == (UF_REGISTERED | UF_IS_SPEAKING)) && ((m_pUserRelationship[localUser][remoteUserID]->m_relationshipFlags & RF_PLAYBACK_ON) == RF_PLAYBACK_ON));
	}
	else
	{
		CryUserID localUserID = m_pService->GetUserID(localUser);
		if (localUserID == remoteUser)
		{
			return IsSpeaking(localUser);
		}
	}

	return false;
}

bool CCryVoice::IsMicrophoneConnected(CryUserID userID)
{
	CryVoiceRemoteUserID remoteUserID = GetRemoteUser(userID);

	if (remoteUserID != CryVoiceInvalidRemoteUserID)
	{
		assert(m_pRemoteUsers[remoteUserID]);

		return ((m_pRemoteUsers[remoteUserID]->m_remoteUserFlags & UF_REGISTERED) && (m_pRemoteUsers[remoteUserID]->m_remoteUserFlags & UF_IS_MICROPHONE_CONNECTED));
	}

	return false;
}

void CCryVoice::Tick(CTimeValue tv)
{
	if (tv.GetSeconds() - m_notificationTimer.GetSeconds() > CLobbyCVars::Get().minMicrophonNotificationInterval)
	{
		for (uint32 i = 0; i < m_numRemoteTalkers; i++)
		{
			SRemoteUser* pRemoteUser = m_pRemoteUsers[i];
			assert(pRemoteUser);

			if (pRemoteUser->m_remoteUserFlags & UF_REGISTERED)
			{
				if (pRemoteUser->m_remoteUserFlags & UF_SEND_MICROPHONE_STATUS)
				{
					SendMicrophoneNotification(i);
				}
				if (pRemoteUser->m_remoteUserFlags & UF_REQUEST_MICROPHONE_STATUS)
				{
					SendMicrophoneRequest(i);
				}
			}
		}

		m_notificationTimer = tv;
	}
}

#endif // USE_CRY_VOICE
