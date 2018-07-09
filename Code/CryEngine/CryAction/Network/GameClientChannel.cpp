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
#include "GameClientChannel.h"
#include "GameServerChannel.h"
#include "GameContext.h"
#include "CryAction.h"
#include "GameRulesSystem.h"
#include "GameObjects/GameObject.h"
#include "GameClientNub.h"
#include "ILevelSystem.h"
#include "IActorSystem.h"
#include "ActionGame.h"
#include <CryNetwork/INetworkService.h>
#include <CryNetwork/INetwork.h>
#include "CryActionCVars.h"

#define LOCAL_ACTOR_VARIABLE     "g_localActor"
#define LOCAL_CHANNELID_VARIABLE "g_localChannelId"
#define LOCAL_ACTORID_VARIABLE   "g_localActorId"

//////////////////////////////////////////////////////////////////////////

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)

CClientClock::CClientClock(CGameClientChannel& clientChannel)
	: m_clientChannel(clientChannel)
	, m_pings(0)
	, m_syncState(eSS_NotDone)
{

}

void CClientClock::StartSync()
{
	m_syncState = eSS_Pinging;
	m_pings = 0;

	SendSyncPing(0, gEnv->pTimer->GetAsyncTime());
}

bool CClientClock::OnSyncTimeMessage(const SSyncTimeClient& messageParams)
{
	// Calculate the ping time
	CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
	const CTimeValue roundTripTime = currentTime - messageParams.originTime;
	CTimeValue halfRoundTripTime = roundTripTime;
	halfRoundTripTime /= 2;

	//CryLogAlways("Ping: %f", halfRoundTripTime.GetMilliSeconds());
	m_pings += 1;

	const int initialPings = 8;
	if ((m_syncState == eSS_Pinging) && (m_pings == initialPings))
	{
		m_syncState = eSS_Refining;
	}
	else if (m_syncState == eSS_Refining)
	{
		//float diff = CTimeValue(param.serverTimeActual - param.serverTimeGuess - halfRoundTripTime.GetValue()).GetMilliSeconds();
		//m_diffs.push_back(diff);

		//float avgDiff = std::accumulate(m_diffs.begin(), m_diffs.end(), 0) / float(m_diffs.size());
		//CryLogAlways("Diff avg: %f", avgDiff);

		if (m_pings > 32)
		{
			//CryLogAlways("Offset time: %f", m_OffsetTime.GetMilliSeconds());
			m_syncState = eSS_Done;
			return true;
		}
	}

	m_timeOffsets.push_back(CTimeValue(messageParams.serverTimeActual) - CTimeValue(messageParams.originTime) - halfRoundTripTime);

	CTimeValue averageOffset;
	if (m_timeOffsets.size() > 0)
	{
		std::vector<CTimeValue> offsets = m_timeOffsets;

		if (m_timeOffsets.size() > 3)
		{
			std::sort(offsets.begin(), offsets.end());

			// remove the worst values from the list
			offsets.erase(offsets.end() - 1);
			offsets.erase(offsets.begin());
		}

		for (std::vector<CTimeValue>::iterator it = offsets.begin(); it != offsets.end(); ++it)
		{
			averageOffset += *it;
		}
		averageOffset /= offsets.size();
	}
	m_serverTimeOffset = averageOffset;

	currentTime = gEnv->pTimer->GetAsyncTime();
	const CTimeValue serverTimeEstimate = currentTime + m_serverTimeOffset;
	SendSyncPing(messageParams.id + 1, currentTime, serverTimeEstimate);

	return true;
}

void CClientClock::SendSyncPing(const int id, const CTimeValue currentTime, const CTimeValue serverTime /*= CTimeValue()*/)
{
	INetSendablePtr msg = new CSimpleNetMessage<SSyncTimeServer>(SSyncTimeServer(id, currentTime.GetValue(), serverTime.GetValue()), CGameServerChannel::SyncTimeServer);
	m_clientChannel.GetNetChannel()->AddSendable(msg, 0, NULL, NULL);
}

#endif //GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME

//////////////////////////////////////////////////////////////////////////

CGameClientChannel::CGameClientChannel(INetChannel* pNetChannel, CGameContext* pContext, CGameClientNub* pNub)
	: m_pNub(pNub)
	, m_hasLoadedLevel(false)
#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
	, m_clock(*this)
#endif
{
	pNetChannel->SetClient(pContext->GetNetContext());
	Init(pNetChannel, pContext);

#if !NEW_BANDWIDTH_MANAGEMENT
	INetChannel::SPerformanceMetrics pm;
	if (!gEnv->bMultiplayer)
		pm.pPacketRateDesired = gEnv->pConsole->GetCVar("g_localPacketRate");
	else
		pm.pPacketRateDesired = gEnv->pConsole->GetCVar("cl_packetRate");
	pm.pBitRateDesired = gEnv->pConsole->GetCVar("cl_bandwidth");
	pNetChannel->SetPerformanceMetrics(&pm);
#endif // !NEW_BANDWIDTH_MANAGEMENT

	CRY_ASSERT(pNetChannel);
	if (!CCryAction::GetCryAction()->IsGameSessionMigrating())
	{
		// if we're migrating these globals are already setup to the correct variables
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTORID_VARIABLE);
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTOR_VARIABLE);
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_CHANNELID_VARIABLE);
	}

	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_channelCreated, 0));
}

CGameClientChannel::~CGameClientChannel()
{
	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_channelDestroyed, 0));
	m_pNub->ClientChannelClosed();

	if (!CCryAction::GetCryAction()->IsGameSessionMigrating())
	{
		// if we're migrating these globals are already setup to the correct variables
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTORID_VARIABLE);
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTOR_VARIABLE);
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_CHANNELID_VARIABLE);
	}

	// If we're not migrating the host, restore the cached cvars.  If we *are*
	// migrating the host, we need to preserve the current cvar settings in order
	// to start the new server in the same manner.
	if (!CCryAction::GetCryAction()->IsGameSessionMigrating())
	{
		for (std::map<string, string>::iterator it = m_originalCVarValues.begin(); it != m_originalCVarValues.end(); ++it)
		{
			if (ICVar* pVar = gEnv->pConsole->GetCVar(it->first.c_str()))
			{
				if (it->second != pVar->GetString())
				{
					CryLog("Restore cvar '%s' to '%s'", it->first.c_str(), it->second.c_str());
				}
				pVar->ForceSet(it->second.c_str());
			}
		}
	}
}

void CGameClientChannel::Release()
{
	delete this;
	CryLogAlways("CGameClientChannel::Release");
}

void CGameClientChannel::AddUpdateLevelLoaded(IContextEstablisher* pEst)
{
	if (!m_hasLoadedLevel)
		AddSetValue(pEst, eCVS_InGame, &m_hasLoadedLevel, true, "AllowChaining");
}

bool CGameClientChannel::CheckLevelLoaded() const
{
	return m_hasLoadedLevel;
}

void CGameClientChannel::OnDisconnect(EDisconnectionCause cause, const char* description)
{
	//CryLogAlways("CGameClientChannel::OnDisconnect(%d, %s)", cause, description?description:"");
	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_disconnected, int(cause), description));

	for (INetworkedClientListener* pListener : CCryAction::GetCryAction()->GetNetworkClientListeners())
	{
		pListener->OnLocalClientDisconnected(cause, description);
	}

	if (IInput* pInput = gEnv->pInput)
	{
		pInput->EnableDevice(eIDT_Keyboard, true);
		pInput->EnableDevice(eIDT_Mouse, true);
	}
}

void CGameClientChannel::DefineProtocol(IProtocolBuilder* pBuilder)
{
	pBuilder->AddMessageSink(this, CGameServerChannel::GetProtocolDef(), CGameClientChannel::GetProtocolDef());
	CCryAction* cca = CCryAction::GetCryAction();
	if (cca->GetIGameObjectSystem())
		cca->GetIGameObjectSystem()->DefineProtocol(false, pBuilder);
	if (cca->GetGameContext())
		cca->GetGameContext()->DefineContextProtocols(pBuilder, false);
	cca->DefineProtocolRMI(pBuilder);
}

// message implementation
NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, RegisterEntityClass, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return GetGameContext()->RegisterClassName(param.name, param.id);
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, RegisterEntityClassHash, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CGameContext* pGameContext = GetGameContext();
	uint32 localCrc = pGameContext->GetClassesHash();
	if (localCrc != param.crc)
	{
		pGameContext->DumpClasses();
		disconnectCause = eDC_ClassRegistryMismatch;
		disconnectMessage.Format("Server classes hash '%x' doesn't match local classes hash '%x'", param.crc, localCrc);
		return false;
	}
	return true;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, SetGameType, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	string rulesClass;
	string levelName = param.levelName;
	if (!GetGameContext()->ClassNameFromId(rulesClass, param.rulesClass))
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "No GameRules");
	}

	bool ok = true;
	if (!GetGameContext()->SetImmersive(param.immersive))
		return false;
	if (!bFromDemoSystem)
	{
		CryLogAlways("Game rules class: %s", rulesClass.c_str());
		SGameContextParams params;
		params.levelName = levelName.c_str();
		params.gameRules = rulesClass.c_str();
		ok = GetGameContext()->ChangeContext(false, &params);
	}
	else
	{
		GetGameContext()->SetLevelName(levelName.c_str());
		GetGameContext()->SetGameRules(rulesClass.c_str());
		//
		IActor* pDemoPlayPlayer = gEnv->pGameFramework->GetIActorSystem()->GetOriginalDemoSpectator();
		// clear all entities slot - because render data will be released in LoadLevel and these releases are not clears entities slots data (FogVolume for example)
		CScopedRemoveObjectUnlock unlockRemovals(CCryAction::GetCryAction()->GetGameContext());
		IEntityItPtr i = gEnv->pEntitySystem->GetEntityIterator();
		while (!i->IsEnd())
		{
			IEntity* pEnt = i->Next();
			//
			IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEnt->GetId());
			if (pActor && pActor == pDemoPlayPlayer)
			{
				continue;
			}
			//
			pEnt->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
			gEnv->pEntitySystem->RemoveEntity(pEnt->GetId(), true);
			//
		}
		//
		CCryAction::GetCryAction()->GetIGameRulesSystem()->CreateGameRules(rulesClass.c_str()); // we don't do context establishment tasks when playing back demo
		ok = CCryAction::GetCryAction()->GetILevelSystem()->LoadLevel(levelName.c_str()) != 0;
		//
		ILevelSystem* pLS = CCryAction::GetCryAction()->GetILevelSystem();
		if (pLS->GetCurrentLevel())
		{
			pLS->OnLoadingComplete(pLS->GetCurrentLevel());
		}
		if (pDemoPlayPlayer)
		{
			CCryAction::GetCryAction()->OnActionEvent(eAE_inGame);
		}
	}

	return ok;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, ResetMap, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	GetGameContext()->ResetMap(false);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CGameClientChannel, DefaultSpawn, eNRT_UnreliableOrdered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	bool bFromDemoSystem = nCurSeq == DEMO_PLAYBACK_SEQ_NUMBER && nOldSeq == DEMO_PLAYBACK_SEQ_NUMBER;

#if RESERVE_UNBOUND_ENTITIES
	uint16 netMID;
	ser.Value("partialNetID", netMID, 'eid');
#endif

	SBasicSpawnParams param;
	param.SerializeWith(ser);

	string actorClass;
	if (!GetGameContext()->ClassNameFromId(actorClass, param.classId))
		return false;

	uint16 channelId = bFromDemoSystem ? (param.nChannelId != 0 ? -1 : 0) : param.nChannelId;

	IGameObjectSystem::SEntitySpawnParamsForGameObjectWithPreactivatedExtension userData;
	userData.hookFunction = HookCreateActor;
	userData.pUserData = &channelId;

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	SEntitySpawnParams esp;
	esp.id = 0;
#if RESERVE_UNBOUND_ENTITIES
	esp.id = GetGameContext()->GetNetContext()->RemoveReservedUnboundEntityMapEntry(netMID);
#endif
	esp.nFlags = (param.flags & ~ENTITY_FLAG_TRIGGER_AREAS);
	esp.pClass = pEntitySystem->GetClassRegistry()->FindClass(actorClass);
	if (!esp.pClass)
		return false;
	esp.pUserData = &userData;
	esp.qRotation = param.rotation;
	esp.sName = param.name.c_str();
	esp.vPosition = param.pos;
	esp.vScale = param.scale;
	esp.pSpawnSerializer = &ser;

	if (IEntity* pEntity = pEntitySystem->SpawnEntity(esp, false))
	{
		const EntityId entityId = pEntity->GetId();

		if (!param.baseComponent.IsNull())
		{
			if (pEntity->QueryComponentByInterfaceID(param.baseComponent) == nullptr)
			{
				pEntity->CreateComponentByInterfaceID(param.baseComponent, nullptr);
			}
		}

		if (!pEntitySystem->InitEntity(pEntity, esp))
		{
			return false;
		}

		if (param.bClientActor)
		{
			SetPlayerId(entityId);
		}
		GetGameContext()->GetNetContext()->SpawnedObject(entityId);

		pEntity->SendEvent(SEntityEvent(ENTITY_EVENT_SPAWNED_REMOTELY));

		return true;
	}

	pNetChannel->Disconnect(eDC_ContextCorruption, "Failed to spawn entity");

	return false;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, SetPlayerId_LocalOnly, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	if (!GetNetChannel()->IsLocal())
		return false;
	SetPlayerId(param.id);
	return true;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, SetConsoleVariable, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	if (GetNetChannel()->IsLocal() && !bFromDemoSystem)
		return false;

	return SetConsoleVar(param.key, param.value);
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, SetBatchConsoleVariables, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	if (GetNetChannel()->IsLocal() && !bFromDemoSystem)
		return false;

	bool res = true;

	for (int i = 0; i < param.actual && res; ++i)
		res = SetConsoleVar(param.vars[i].key, param.vars[i].value);

	return res;
}

void CGameClientChannel::SetPlayerId(EntityId id)
{
	CGameChannel::SetPlayerId(id);

	IScriptSystem* pSS = gEnv->pScriptSystem;

	if (id)
	{
		ScriptHandle hdl;
		hdl.n = GetPlayerId();
		pSS->SetGlobalValue(LOCAL_ACTORID_VARIABLE, hdl);
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
		if (pEntity)
		{
			IGameObject* pGameObject = CCryAction::GetCryAction()->GetGameObject(id);
			pSS->SetGlobalValue(LOCAL_ACTOR_VARIABLE, pEntity->GetScriptTable());
			pSS->SetGlobalValue(LOCAL_CHANNELID_VARIABLE, pGameObject ? pGameObject->GetChannelId() : 0);
		}
		else
		{
			pSS->SetGlobalToNull(LOCAL_ACTOR_VARIABLE);
			pSS->SetGlobalToNull(LOCAL_CHANNELID_VARIABLE);
		}

		CallOnSetPlayerId();
	}
	else
	{
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTORID_VARIABLE);
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTOR_VARIABLE);
		gEnv->pScriptSystem->SetGlobalToNull(LOCAL_CHANNELID_VARIABLE);
	}
}

void CGameClientChannel::CallOnSetPlayerId()
{
	IEntity* pPlayer = gEnv->pEntitySystem->GetEntity(GetPlayerId());
	if (!pPlayer)
		return;

	IScriptTable* pScriptTable = pPlayer->GetScriptTable();
	if (pScriptTable)
	{
		SmartScriptTable client;
		if (pScriptTable->GetValue("Client", client))
		{
			if (pScriptTable->GetScriptSystem()->BeginCall(client, "OnSetPlayerId"))
			{
				pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
				pScriptTable->GetScriptSystem()->EndCall();
			}
		}
	}

	if (IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetPlayerId()))
		pActor->InitLocalPlayer();

	SEntityEvent becomeLocalPlayer(ENTITY_EVENT_NET_BECOME_LOCAL_PLAYER);
	pPlayer->SendEvent(becomeLocalPlayer);

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (m_pVoiceController)
		m_pVoiceController->PlayerIdSet(id);
#endif

	pPlayer->AddFlags(ENTITY_FLAG_LOCAL_PLAYER | ENTITY_FLAG_TRIGGER_AREAS);
}

bool CGameClientChannel::HookCreateActor(IEntity* pEntity, IGameObject* pGameObject, void* pUserData)
{
	//[AlexMcC|23.11.09]: Set the ChannelId for remote obejcts at the same time as local objects
	// (which is very early). Setting the ChannelId early like this means that we can trust
	// IActor::IsPlayer() very early, such as in Player::Init().
	// Copied from CActorSystem.cpp
	if (pGameObject)
	{
		pGameObject->SetChannelId(*static_cast<uint16*>(pUserData));
	}
	return true;
}
bool CGameClientChannel::SetConsoleVar(const string& key, const string& val)
{
	IConsole* pConsole = gEnv->pConsole;
	ICVar* pVar = pConsole->GetCVar(key.c_str());
	if (!pVar)
	{
		CryLog("Server sets console variable '%s' to '%s'", key.c_str(), val.c_str());
		CryLog("   cvar not found, ignoring");
		return true;
	}
	int flags = pVar->GetFlags();
	if ((flags & VF_NET_SYNCED) == 0)
	{
		CryLog("Server sets console variable '%s' to '%s'", key.c_str(), val.c_str());
		CryLog("   cvar not synchronized, disconnecting");
		return false;
	}

	if (val != pVar->GetString())
		CryLog("Server sets console variable '%s' to '%s'", key.c_str(), val.c_str());

	std::map<string, string>::iterator orit = m_originalCVarValues.lower_bound(key);
	if (orit == m_originalCVarValues.end() || orit->first != key)
		m_originalCVarValues.insert(std::make_pair(key, pVar->GetString()));

	pVar->ForceSet(val.c_str());

	return true;
}

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, SyncTimeClient, eNRT_ReliableUnordered, eMPF_NoSendDelay)
{
	return m_clock.OnSyncTimeMessage(param);
}

#endif
