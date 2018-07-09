// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 8:12:2005   11:26 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Actor.h"


//------------------------------------------------------------------------
EntityId CItem::NetGetOwnerId() const
{
	return m_owner.GetId();
}

//------------------------------------------------------------------------
void CItem::NetSetOwnerId(EntityId id)
{
	if (id==m_owner.GetId())
		return;

	CryLogAlways("%s::NetSetOwnerId(%s)", GetEntity()->GetName(), GetActor(id)?GetActor(id)->GetEntity()->GetName():"null");

	if (id)
		PickUp(id, true);
	else
	{
		Drop();

		CActor *pActor=GetOwnerActor();
		if (pActor)
			pActor->GetInventory()->SetCurrentItem(0);
	}
}

//------------------------------------------------------------------------
void CItem::InitClient(int channelId)
{
	const int numAccessories = m_accessories.size();
	
	// send the differences between the current, and the initial setup
	for (int i = 0; i < numAccessories; i++)
	{
		uint16 classId = 0;
		bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassId(classId, m_accessories[i].pClass->GetName());

#if !defined(_RELEASE)
		if(!result)
		{
			char errorMsg[256];
			cry_sprintf(errorMsg, "CItem::InitClient failed to find network safe class id for %s", m_accessories[i].pClass->GetName());
			CRY_ASSERT_MESSAGE(result, errorMsg);
		}
#endif
		if(result)
			GetGameObject()->InvokeRMI(ClAttachInitialAccessory(), AccessoryParams(classId), eRMI_ToClientChannel, channelId);
	}

	IActor *pOwner=GetOwnerActor();
	if (!pOwner)
		return;

	// only send the pickup message if the player is connecting
	// for items spawned during gameplay, CItem::PickUp is already sending the pickup message
	INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(channelId);
	if (pNetChannel && pNetChannel->GetContextViewState()<eCVS_InGame)
	{
		if (!m_stats.mounted && !m_stats.used)
		{
			pOwner->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClPickUp(), 
				CActor::PickItemParams(GetEntityId(), m_stats.selected, false), eRMI_ToClientChannel, GetEntityId(), channelId);
			//GetOwnerActor()->GetGameObject()->InvokeRMI(CActor::ClPickUp(), 
			//	CActor::PickItemParams(GetEntityId(), m_stats.selected, false), eRMI_ToClientChannel, channelId);
		}
	}

	if (m_stats.mounted && m_stats.used)
	{
		pOwner->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClStartUse(), 
			CActor::ItemIdParam(GetEntityId()), eRMI_ToClientChannel, GetEntityId(), channelId);
	}
}

//------------------------------------------------------------------------
void CItem::PostInitClient(int channelId)
{
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, SvRequestAttachAccessory)
{
	if (IInventory *pInventory=GetActorInventory(GetOwnerActor()))
	{
		char accessoryName[128];
		bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassName(accessoryName, sizeof(accessoryName), params.accessoryClassId);

#if !defined(_RELEASE)
	if(!result)
	{
		char errorMsg[256];
		cry_sprintf(errorMsg, "CItem::SvRequestAttachAccessory failed to find network safe class name for id %d", params.accessoryClassId);
		CRY_ASSERT_MESSAGE(result, errorMsg);
	}
#endif

		if (accessoryName[0] != '\0' && pInventory->GetCountOfClass(accessoryName)>0)
		{
			DoSwitchAccessory(accessoryName);
			GetGameObject()->InvokeRMI(ClAttachAccessory(), params, eRMI_ToAllClients|eRMI_NoLocalCalls);

			return true;
		}
	}
	
	return true; // set this to false later
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClAttachAccessory)
{
	char accessoryName[128];
	bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassName(accessoryName, sizeof(accessoryName), params.accessoryClassId);

#if !defined(_RELEASE)
	if(!result)
	{
		char errorMsg[256];
		cry_sprintf(errorMsg, "CItem::ClAttachAccessory failed to find network safe class name for id %d", params.accessoryClassId);
		CRY_ASSERT_MESSAGE(result, errorMsg);
	}
#endif

	DoSwitchAccessory(accessoryName, false);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClAttachInitialAccessory)
{
	char accessoryName[128];
	bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassName(accessoryName, sizeof(accessoryName), params.accessoryClassId);

#if !defined(_RELEASE)
	if(!result)
	{
		char errorMsg[256];
		cry_sprintf(errorMsg, "CItem::ClAttachAccessory failed to find network safe class name for id %d", params.accessoryClassId);
		CRY_ASSERT_MESSAGE(result, errorMsg);
	}
#endif

	DoSwitchAccessory(accessoryName, true);

	return true;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, SvRequestDetachAccessory)
{
	if (IInventory *pInventory=GetActorInventory(GetOwnerActor()))
	{
		char accessoryName[128];
		bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassName(accessoryName, sizeof(accessoryName), params.accessoryClassId);

#if !defined(_RELEASE)
		if(!result)
		{
			char errorMsg[256];
			cry_sprintf(errorMsg, "CItem::SvRequestDetachAccessory failed to find network safe class name for id %d", params.accessoryClassId);
			CRY_ASSERT_MESSAGE(result, errorMsg);
		}
#endif

		if (accessoryName[0] != '\0')
		{
			AttachAccessory(accessoryName, false, true, true);
			GetGameObject()->InvokeRMI(ClDetachAccessory(), params, eRMI_ToRemoteClients);

			return true;
		}
	}
	return true; 
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClDetachAccessory)
{
	char accessoryName[128];
	bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassName(accessoryName, sizeof(accessoryName), params.accessoryClassId);

#if !defined(_RELEASE)
	if(!result)
	{
		char errorMsg[256];
		cry_sprintf(errorMsg, "CItem::ClDetachAccessory failed to find network safe class name for id %d", params.accessoryClassId);
		CRY_ASSERT_MESSAGE(result, errorMsg);
	}
#endif

	AttachAccessory(accessoryName, false, true, true);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, SvRequestEnterModify)
{
	GetGameObject()->InvokeRMI(ClEnterModify(), params, eRMI_ToOtherClients, m_pGameFramework->GetGameChannelId(pNetChannel));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, SvRequestLeaveModify)
{
	GetGameObject()->InvokeRMI(ClLeaveModify(), params, eRMI_ToOtherClients, m_pGameFramework->GetGameChannelId(pNetChannel));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClEnterModify)
{
	PlayAction(GetFragmentIds().enter_modify, 0, false, eIPAF_Default );

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClLeaveModify)
{
	PlayAction(GetFragmentIds().leave_modify, 0);

	return true;
}
