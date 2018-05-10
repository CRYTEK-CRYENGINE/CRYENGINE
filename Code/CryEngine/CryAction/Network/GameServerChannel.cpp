// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 11:8:2004   11:40 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameServerChannel.h"
#include "GameClientChannel.h"
#include "GameServerNub.h"
#include "GameContext.h"
#include "CryAction.h"
#include "GameRulesSystem.h"

ICVar* CGameServerChannel::sv_timeout_disconnect = 0;

CGameServerChannel::CGameServerChannel(INetChannel* pNetChannel, CGameContext* pGameContext, CGameServerNub* pServerNub)
	: m_pServerNub(pServerNub), m_channelId(0), m_hasLoadedLevel(false), m_onHold(false)
{
	Init(pNetChannel, pGameContext);
	CRY_ASSERT(pNetChannel);

#if NEW_BANDWIDTH_MANAGEMENT
	pNetChannel->SetServer(GetGameContext()->GetNetContext());
#else
	SetupNetChannel(pNetChannel);
#endif // NEW_BANDWIDTH_MANAGEMENT

	if (!sv_timeout_disconnect)
		sv_timeout_disconnect = gEnv->pConsole->GetCVar("sv_timeout_disconnect");

	gEnv->pConsole->AddConsoleVarSink(this);
	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_channelCreated, 1));
}

CGameServerChannel::~CGameServerChannel()
{
	gEnv->pConsole->RemoveConsoleVarSink(this);
	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_channelDestroyed, 1));

	Cleanup();
}

#if !NEW_BANDWIDTH_MANAGEMENT
void CGameServerChannel::SetupNetChannel(INetChannel* pNetChannel)
{
	pNetChannel->SetServer(GetGameContext()->GetNetContext(), true);
	INetChannel::SPerformanceMetrics pm;
	if (!gEnv->bMultiplayer)
		pm.pPacketRateDesired = gEnv->pConsole->GetCVar("g_localPacketRate");
	else
		pm.pPacketRateDesired = gEnv->pConsole->GetCVar("sv_packetRate");
	pm.pBitRateDesired = gEnv->pConsole->GetCVar("sv_bandwidth");
	pNetChannel->SetPerformanceMetrics(&pm);
}
#endif // !NEW_BANDWIDTH_MANAGEMENT

void CGameServerChannel::Release()
{
	if (GetNetChannel())
		delete this;
}

bool CGameServerChannel::OnBeforeVarChange(ICVar* pVar, const char* sNewValue)
{
	// This code is useful for debugging issues with networked cvars, but it's
	// very spammy so #if'ing out for now.
#if 0 // LOG_CVAR_USAGE
	int flags = pVar->GetFlags();
	bool netSynced = ((flags & VF_NET_SYNCED) != 0);

	CryLog("[CVARS]: [CHANGED] CGameServerChannel::OnBeforeVarChange(): variable [%s] (%smarked VF_NET_SYNCED) with a value of [%s]; SERVER changing to [%s]",
	       pVar->GetName(),
	       (netSynced) ? "" : "not ",
	       pVar->GetString(),
	       sNewValue);
#endif // LOG_CVAR_USAGE

	return true;
}

void CGameServerChannel::OnAfterVarChange(ICVar* pVar)
{
	if (pVar->GetFlags() & VF_NET_SYNCED)
	{
		if (GetNetChannel() && !GetNetChannel()->IsLocal())
		{
			SClientConsoleVariableParams params(pVar->GetName(), pVar->GetString());
#if FAST_CVAR_SYNC
			SSendableHandle& id = GetConsoleStreamId(params.key);
			INetSendablePtr pSendable = new CSimpleNetMessage<SClientConsoleVariableParams>(params, CGameClientChannel::SetConsoleVariable);
			pSendable->SetGroup('cvar');
			GetNetChannel()->SubstituteSendable(pSendable, 1, &id, &id);
#else
			INetSendablePtr pSendable = new CSimpleNetMessage<SClientConsoleVariableParams>(params, CGameClientChannel::SetConsoleVariable);
			pSendable->SetGroup('cvar');
			GetNetChannel()->AddSendable(pSendable, 1, &m_consoleVarSendable, &m_consoleVarSendable);
#endif
		}
	}
}

void CGameServerChannel::OnDisconnect(EDisconnectionCause cause, const char* description)
{
	//CryLogAlways("CGameServerChannel::OnDisconnect(%d, '%s')", cause, description?description:"");
	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_clientDisconnected, int(cause), description));

	if (!IsOnHold() && sv_timeout_disconnect && sv_timeout_disconnect->GetIVal() > 0)
	{
		// Check if any clients want to keep this player
		for (INetworkedClientListener* pListener : CCryAction::GetCryAction()->GetNetworkClientListeners())
		{
			if(!pListener->OnClientTimingOut(GetChannelId(), cause, description))
			{
				if (m_pServerNub->PutChannelOnHold(this))
				{
					for (INetworkedClientListener* pRecursiveListener : CCryAction::GetCryAction()->GetNetworkClientListeners())
					{
						pRecursiveListener->OnClientDisconnected(GetChannelId(), cause, description, true);
					}

					m_hasLoadedLevel = false;
					return;
				}

				break;
			}
		}
	}

	for (INetworkedClientListener* pListener : CCryAction::GetCryAction()->GetNetworkClientListeners())
	{
		pListener->OnClientDisconnected(GetChannelId(), cause, description, false);
	}

	Cleanup();
}

void CGameServerChannel::Cleanup()
{
	m_pServerNub->RemoveChannel(GetChannelId());

	if (GetPlayerId())
	{
		gEnv->pEntitySystem->RemoveEntity(GetPlayerId(), true);
	}
}

void CGameServerChannel::DefineProtocol(IProtocolBuilder* pBuilder)
{
	pBuilder->AddMessageSink(this, CGameClientChannel::GetProtocolDef(), CGameServerChannel::GetProtocolDef());
	CCryAction* cca = CCryAction::GetCryAction();
	if (cca->GetIGameObjectSystem())
		cca->GetIGameObjectSystem()->DefineProtocol(true, pBuilder);
	if (cca->GetGameContext())
		cca->GetGameContext()->DefineContextProtocols(pBuilder, true);
	cca->DefineProtocolRMI(pBuilder);
}

void CGameServerChannel::SetPlayerId(EntityId playerId)
{
	//check for banned status here
	if (m_pServerNub->CheckBanned(GetNetChannel()))
		return;

	if (CGameServerChannel* pServerChannel = m_pServerNub->GetOnHoldChannelFor(GetNetChannel()))
	{
		//cleanup onhold channel if it was not associated with us
		//normally it should be taken while creating channel, but for now, this doesn't happen
		m_pServerNub->RemoveOnHoldChannel(pServerChannel, false);
	}

	CGameChannel::SetPlayerId(playerId);
	if (GetNetChannel()->IsLocal())
		CGameClientChannel::SendSetPlayerId_LocalOnlyWith(playerId, GetNetChannel());
}

bool CGameServerChannel::CheckLevelLoaded() const
{
	return m_hasLoadedLevel;
}

void CGameServerChannel::AddUpdateLevelLoaded(IContextEstablisher* pEst)
{
	if (!m_hasLoadedLevel)
		AddSetValue(pEst, eCVS_InGame, &m_hasLoadedLevel, true, "AllowChaining");
}

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameServerChannel, MutePlayer, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	if (GetNetChannel()->IsLocal())
		return true;

	CCryAction::GetCryAction()->GetGameContext()->GetNetContext()->GetVoiceContext()->Mute(param.requestor, param.id, param.mute);

	return true;
}
#endif

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameServerChannel, SyncTimeServer, eNRT_ReliableOrdered, eMPF_NoSendDelay)
{
	CTimeValue value = gEnv->pTimer->GetAsyncTime();
	INetSendablePtr msg = new CSimpleNetMessage<SSyncTimeClient>(SSyncTimeClient(param.id, param.clientTime, param.serverTime, value.GetValue()), CGameClientChannel::SyncTimeClient);
	GetNetChannel()->AddSendable(msg, 0, NULL, NULL);

	return true;
}
#endif
