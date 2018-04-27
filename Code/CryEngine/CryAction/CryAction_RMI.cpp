// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
// EntityRMI-related CryAction code.
// See related: <CryCommon/CryNetwork/Rmi.h> for user-side IEntityComponent helpers.
#include "StdAfx.h"

#include "Network/GameServerChannel.h"
#include "Network/GameServerNub.h"
 
namespace {

SNetMessageDef s_entity_rmi_msg;

static TNetMessageCallbackResult ProcessPacketRMI(
	uint32 userId,
	INetMessageSink* handler,
	TSerialize serialize,
	uint32 curSeq,
	uint32 oldSeq,
	uint32 timeFraction32,
	EntityId* pEntityId,
	INetChannel* pNetChannel)
{
	INetEntity::SRmiIndex rmi_index(0);
	serialize.Value("rmi_index", rmi_index.value); // reading

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(*pEntityId);
	INetEntity::SRmiHandler::DecoderF decoder = pEntity->GetNetEntity()->RmiByIndex(rmi_index);
	INetAtSyncItem* pItem = decoder(&serialize, *pEntityId, pNetChannel);
	return TNetMessageCallbackResult(pItem != 0, pItem);
}

} // anonymous namespace

void CCryAction::DefineProtocolRMI(IProtocolBuilder* pBuilder)
{
	// I need only one RMI message registered for all entities.
	s_entity_rmi_msg.handler = ProcessPacketRMI;
	s_entity_rmi_msg.description = "RMI:Entity";

	SNetProtocolDef protoSendingReceiving;
	protoSendingReceiving.nMessages = 1;
	protoSendingReceiving.vMessages = &s_entity_rmi_msg;

	pBuilder->AddMessageSink(nullptr /* ignored */, protoSendingReceiving, protoSendingReceiving);
}

// Accepts two types of RMIs: of CGameObject and of NetEntity.
// The only distinction between them is that NetEntity RMIs share the one 
// message definition for all of them, which is registered and known only in this file.
// This has previously been CGameObject::DoInvokeRMI.
void CCryAction::DoInvokeRMI(_smart_ptr<IRMIMessageBody> pBody, unsigned where,
	int channel, const bool isGameObjectRmi)
{
	if (!isGameObjectRmi)
	{
		pBody->pMessageDef = &s_entity_rmi_msg;
	}

	// 'where' flag validation
	if (where & eRMI_ToClientChannel)
	{
		if (channel <= 0)
		{
			GameWarning("InvokeRMI: ToClientChannel specified, but no channel specified");
			return;
		}
		if (where & eRMI_ToOwnClient)
		{
			GameWarning("InvokeRMI: ToOwnClient and ToClientChannel specified - not supported");
			return;
		}
	}
	if (where & eRMI_ToOwnClient)
	{
		channel = gEnv->pEntitySystem->GetEntity(pBody->objId)->GetNetEntity()->GetChannelId();
		if (!channel)
		{
			GameWarning("InvokeRMI: ToOwnClient specified, but no own client");
			return;
		}
		where &= ~eRMI_ToOwnClient;
		where |= eRMI_ToClientChannel;
	}
	if (where & eRMI_ToAllClients)
	{
		where &= ~eRMI_ToAllClients;
		where |= eRMI_ToOtherClients;
		channel = -1;
	}

	if (where & eRMI_ToServer)
	{
		if (INetChannel* pNetChannel = gEnv->pGameFramework->GetClientChannel())
		{
			bool isLocal = pNetChannel->IsLocal();
			bool send = true;
			if ((where & eRMI_NoLocalCalls) != 0)
				if (isLocal)
					send = false;
			if ((where & eRMI_NoRemoteCalls) != 0)
				if (!isLocal)
					send = false;
			if (send)
			{
				const IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pBody->objId);
				NET_PROFILE_SCOPE_RMI(pEntity->GetClass()->GetName(), false);
				NET_PROFILE_SCOPE_RMI(pEntity->GetName(), false);
				pNetChannel->DispatchRMI(&*pBody);
			}
		}
		else
		{
			GameWarning("InvokeRMI: RMI via client (to server) requested but we are not a client");
		}
	}
	if (where & (eRMI_ToClientChannel | eRMI_ToOtherClients))
	{
		CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub();
		if (pServerNub)
		{
			TServerChannelMap* pChannelMap = pServerNub->GetServerChannelMap();
			for (TServerChannelMap::iterator iter = pChannelMap->begin(); iter != pChannelMap->end(); ++iter)
			{
				bool isOwn = iter->first == channel;
				if (isOwn && !(where & eRMI_ToClientChannel) && !IsDemoPlayback())
					continue;
				if (!isOwn && !(where & eRMI_ToOtherClients))
					continue;
				INetChannel* pNetChannel = iter->second->GetNetChannel();
				if (!pNetChannel)
					continue;
				bool isLocal = pNetChannel->IsLocal();
				if (isLocal && (where & eRMI_NoLocalCalls) != 0)
					continue;
				if (!isLocal && (where & eRMI_NoRemoteCalls) != 0)
					continue;
				const IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pBody->objId);
				NET_PROFILE_SCOPE_RMI(pEntity->GetClass()->GetName(), false);
				NET_PROFILE_SCOPE_RMI(pEntity->GetName(), false);
				pNetChannel->DispatchRMI(&*pBody);
			}
		}
		else if (gEnv->pGameFramework->GetNetContext() && gEnv->bMultiplayer)
		{
			GameWarning("InvokeRMI: RMI via server (to client) requested but we are not a server");
		}
	}
}
