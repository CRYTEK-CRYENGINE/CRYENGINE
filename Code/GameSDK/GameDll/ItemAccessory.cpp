// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 8:9:2005   12:52 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Actor.h"
#include "Game.h"
#include "RecordingSystem.h"
#include "ItemAnimation.h"
#include "EquipmentLoadout.h"

//------------------------------------------------------------------------
CItem *CItem::AddAccessory(IEntityClass* pClass)
{
	if (!pClass)
	{
		GameWarning("Trying to add unknown accessory to item '%s'!", GetEntity()->GetName());
		return NULL;
	}
	
	const SAccessoryParams* pAccessoryParams = GetAccessoryParams(pClass);
	if (!pAccessoryParams)
	{
		GameWarning("Trying to add unknown accessory '%s' to item '%s'!", pClass->GetName(), GetEntity()->GetName());
		return NULL;
	}

	char namebuf[128];
	cry_sprintf(namebuf, "%s_%s", GetEntity()->GetName(), pClass->GetName());

	SEntitySpawnParams params;
	params.pClass = pClass;
	params.sName = namebuf;
	params.nFlags = ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CASTSHADOW;

	if (pAccessoryParams->client_only)
		params.nFlags |= ENTITY_FLAG_CLIENT_ONLY;

	EntityId accId=0;
	if (IEntity *pEntity = m_pEntitySystem->SpawnEntity(params))
		accId=pEntity->GetId();

	m_accessories.push_back(SAccessoryInfo(params.pClass, accId));
	if (accId)
		return static_cast<CItem *>(m_pItemSystem->GetItem(accId));
	return 0;
}

//------------------------------------------------------------------------
void CItem::RemoveAccessory(IEntityClass* pClass)
{
	const int numAccessories = m_accessories.size();

	for(int i = 0; i < numAccessories; i++)
	{
		if(m_accessories[i].pClass == pClass)
		{
			IEntity* pEntity = m_pEntitySystem->GetEntity(m_accessories[i].accessoryId);
			if ((pEntity != NULL) && !pEntity->IsGarbage())
			{
				m_pEntitySystem->RemoveEntity(m_accessories[i].accessoryId);
			}
			m_accessories.removeAt(i);
			return;
		}
	}
}


//------------------------------------------------------------------------
void CItem::RemoveAccessoryOnCategory(const ItemString& category)
{
	const int numAccessories = m_accessories.size();

	for(int i = 0; i < numAccessories; i++)
	{
		const SAccessoryParams *params = GetAccessoryParams(m_accessories[i].pClass);
		if (!params || params->category == category)
		{
			AttachAccessory(m_accessories[i].pClass, false, false, true, false);
			return;
		}
	}
}

//------------------------------------------------------------------------
void CItem::RemoveAllAccessories()
{
	const int numAccessories = m_accessories.size();

	for(int i = 0; i < numAccessories; i++)
	{
		EntityId entityId = m_accessories[i].accessoryId;
		IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
		if ((pEntity != NULL) && !pEntity->IsGarbage())
		{
			CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(entityId));
			if (pItem)
				pItem->SetParentId(0);
			m_pEntitySystem->RemoveEntity(m_accessories[i].accessoryId);
		}
	}

	m_accessories.clear();
}

//------------------------------------------------------------------------
void CItem::DetachAllAccessories()
{
	TAccessoryArray accessoriesToDelete;

	for (unsigned int i = 0; i < m_accessories.size(); ++i)
	{
		IEntityClass* pAccessoryClass = m_accessories[i].pClass;

		bool found = false;
		for (unsigned int j = 0; j < m_initialSetup.size(); ++j)
		{
			if (m_initialSetup[j] == pAccessoryClass)
			{
				found = true;
				break;
			}
		}

		if (!found)
			accessoriesToDelete.push_back(m_accessories[i]);
	}

	while (!accessoriesToDelete.empty())
	{
		AttachAccessory(accessoriesToDelete.back().pClass, false, true, true);
		accessoriesToDelete.removeAt(accessoriesToDelete.size()-1);
	}
}



void CItem::AccessoryAttachAction(CItem* pAccessory, const SAccessoryParams* params, bool initialLoadoutSetup)
{
	if (!params->attachToOwner)
	{
		Vec3 position = GetSlotHelperPos(eIGS_ThirdPerson, params->attach_helper.c_str(), false);
		GetEntity()->AttachChild(pAccessory->GetEntity());
		Matrix34 tm(Matrix34::CreateIdentity());
		tm.SetTranslation(position);
		pAccessory->GetEntity()->SetLocalTM(tm);
		pAccessory->SetParentId(GetEntityId());	
	}
	else if(m_stats.viewmode == eIVM_ThirdPerson)
	{
		SetCharacterAttachment(eIGS_ThirdPerson, params->attach_helper.c_str(), pAccessory->GetEntity(), eIGS_ThirdPerson, params->attachToOwner);
	}

	pAccessory->SetViewMode(m_stats.viewmode);
	pAccessory->OnAttach(true);

	AccessoriesChanged(initialLoadoutSetup);

	pAccessory->CloakSync(true);

	if(GetOwnerId() && !IsSelected())
	{
		pAccessory->Hide(true);
	}
}



void CItem::AccessoryDetachAction(CItem* pAccessory, const SAccessoryParams* params)
{
	IEntity* pAccessoryEntity = pAccessory->GetEntity();

	pAccessory->OnAttach(false);
	ResetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), params->attachToOwner);
	pAccessoryEntity->DetachThis();
	pAccessory->SetParentId(0);
	pAccessory->Hide(true);
	RemoveAccessory(pAccessoryEntity->GetClass());
	SetBusy(false);
	AccessoriesChanged(false);
}



void CItem::AttachAccessory(const ItemString& name, bool attach, bool noanim, bool force, bool firstTimeAttached, bool initialLoadoutSetup)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name.c_str());

	if(pClass)
	{
		AttachAccessory(pClass, attach, noanim, force, firstTimeAttached, initialLoadoutSetup);
	}
	else
	{
		GameWarning("Attempting to attach non-existent accessory class '%s' to item '%s'", name.c_str(), GetEntity()->GetName());
	}
}

void CItem::AttachAccessory(IEntityClass* pAccessoryClass, bool attach, bool noanim, bool force, bool firstTimeAttached, bool initialLoadoutSetup)
{
	if (!force && IsBusy())
		return;

	bool anim = !noanim && m_stats.fp;
	const SAccessoryParams *params = GetAccessoryParams(pAccessoryClass);
	if (!params)
		return;

	CActor* pOwner = GetOwnerActor();
	IInventory* pInventory = pOwner ? pOwner->GetInventory() : NULL;
	CItem* pAccessory = GetAccessory(pAccessoryClass);
	
	if (attach && pAccessory == 0)
	{
		bool helperFree = gEnv->bMultiplayer || IsAccessoryHelperFree(params->category); //In MP we allow multiple attachments from the same category
		if (!helperFree)
		{
			if (!force)
				return;
			else
				RemoveAccessoryOnCategory(params->category);
		}

		if (pAccessory = AddAccessory(pAccessoryClass))
		{
			if (params->alsoAttachDefault)
			{
				const int numAccessories = m_sharedparams->accessoryparams.size();

				for(int i = 0; i < numAccessories; i++)
				{
					const SAccessoryParams* pAccessoryParams = &m_sharedparams->accessoryparams[i];

					if (pAccessoryParams 
						&& pAccessoryParams->defaultAccessory 
						&& pAccessoryParams->category == params->category 
						&& pAccessoryParams->attach_helper != params->attach_helper
						&& !HasAccessory(pAccessoryParams->pAccessoryClass))
					{
						if (gEnv->bMultiplayer)
						{
							AttachAccessory(pAccessoryParams->pAccessoryClass, true, true, true);
						}
						else
						{
							AttachDefaultAttachment(pAccessoryParams->pAccessoryClass);
						}
					}
				}
			}

			pAccessory->Physicalize(false, false);
			pAccessory->SetViewMode(m_stats.viewmode);
			if (m_stats.fp)
				pAccessory->OnEnterFirstPerson();
			else
				pAccessory->OnEnterThirdPerson();

			if (!firstTimeAttached)
				pAccessory->ClearItemFlags(eIF_AccessoryAmmoAvailable);

			if(gEnv->bMultiplayer && pInventory) 	  	 
			{
				pAccessory->ProcessAccessoryAmmoCapacities(pInventory, true); 	  	 
				pAccessory->ProcessAccessoryAmmo(pInventory, GetIWeapon());

				const int numAccessories = m_sharedparams->accessoryparams.size();
				unsigned int bit = 1;
				for(int i = 0; i < numAccessories; ++i, bit <<= 1)
				{
					if(m_sharedparams->accessoryparams[i].pAccessoryClass == pAccessoryClass)
					{
						CRY_ASSERT_MESSAGE(i < 16, "CItem::AttachAccessory - attachment history only supports 16 attachments. Need to make m_attachedAccessoryHistory larger.");
						m_attachedAccessoryHistory |= bit;
						break;
					}
				}

				if( GetOwnerId() == gEnv->pGameFramework->GetClientActorId() )
				{
					if( CEquipmentLoadout* pLoadout = g_pGame->GetEquipmentLoadout() )
					{
						pLoadout->UpdateWeaponAttachments( GetEntity()->GetClass(), pAccessoryClass );
					}
				}
			}
		
			SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), pAccessory->GetEntity(), eIGS_FirstPerson, params->attachToOwner);

			AccessoryAttachAction(pAccessory, params, initialLoadoutSetup);
		}
	}
	else if (!attach && pAccessory != 0)
	{
		SetBusy(true);
		AccessoryDetachAction(pAccessory, params);
	}

	ShowAttachmentHelper(eIGS_FirstPerson, params->show_helper.c_str(), attach);
	ShowAttachmentHelper(eIGS_ThirdPerson, params->show_helper.c_str(), attach);

	FixAccessories(params, attach);

	AudioCacheItem(attach, pAccessoryClass, "att_", m_stats.fp ? "_fp" : "_3p");

	//Luciano - send game event
	if (g_pGame) // game gets destroyed before entitysystem, who owns this CItem
	{
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(GetOwner(), GameplayEvent(eGE_AttachedAccessory, pAccessoryClass->GetName(), (float)attach, (void*)(EXPAND_PTR)GetEntityId()));

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnAttachAccessory(this);
		}
	}

	if(pAccessory && (m_itemFlags & eIF_IgnoreHeat))
	{
		IEntityRender* pAccessoryRenderProxy = (pAccessory->GetEntity()->GetRenderInterface());
		if(pAccessoryRenderProxy)
		{
			//pAccessoryRenderProxy->SetIgnoreHeatAmount(attach);
		}
	}
}

//------------------------------------------------------------------------
void CItem::AttachDefaultAttachment(IEntityClass* pAccessoryClass)
{
	eGeometrySlot slot = m_stats.fp ? eIGS_FirstPerson : eIGS_ThirdPerson;
	eViewMode viewMode = m_stats.fp ? eIVM_FirstPerson : eIVM_ThirdPerson;

	const SAccessoryParams *params = GetAccessoryParams(pAccessoryClass);
	if (!params)
		return;
	
	CItem* pAccessory = AddAccessory(pAccessoryClass);
	if (pAccessory)
	{
		pAccessory->Physicalize(false, false);
		pAccessory->SetViewMode(viewMode);
		pAccessory->OnEnterFirstPerson();
		SetCharacterAttachment(slot, params->attach_helper.c_str(), pAccessory->GetEntity(), slot, params->attachToOwner);
	}
}

//------------------------------------------------------------------------
void CItem::ShowAttachmentHelper(int slot, const char* name, bool show)
{
	const size_t nChars = 32;
	const int numSubHelpers = 3;
	CryFixedStringT<nChars> attachmentName[nChars];
	attachmentName[0] = name;
	attachmentName[1].Format("%s_1", name);
	attachmentName[2].Format("%s_2", name);

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(slot);
	if (pCharacter)
	{
		for (int i = 0; i < numSubHelpers; ++i)
		{
			IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
			IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(attachmentName[i].c_str());
			if (pAttachment)
				pAttachment->HideAttachment(show?0:1);
		}
	}
}

//------------------------------------------------------------------------
CItem *CItem::GetAccessory(const ItemString& name)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name.c_str());

	return GetAccessory(pClass);
}

//------------------------------------------------------------------------
CItem *CItem::GetAccessory(IEntityClass* pClass)
{
	const int numAccessories = m_accessories.size();

	for(int i = 0; i < numAccessories; i++)
	{
		if(m_accessories[i].pClass == pClass)
		{
			return static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId));
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CItem::HasAccessory(const ItemString& name)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name.c_str());

	return HasAccessory(pClass);
}

//------------------------------------------------------------------------
bool CItem::HasAccessory(IEntityClass* pClass)
{
	const int numAccessories = m_accessories.size();

	for(int i = 0; i < numAccessories; i++)
	{
		if(m_accessories[i].pClass == pClass)
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
const SAccessoryParams* CItem::GetAccessoryParams(const IEntityClass* pClass) const
{
	const int numParams = m_sharedparams->accessoryparams.size();

	for(int i = 0; i < numParams; i++)
	{
		if(m_sharedparams->accessoryparams[i].pAccessoryClass == pClass)
		{
			return &m_sharedparams->accessoryparams[i];
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CItem::IsFirstTimeAccessoryAttached(IEntityClass* pClass) const
{
	const int numAccessories = m_sharedparams->accessoryparams.size();

	unsigned int bit = 1;
	for(int i = 0; i < numAccessories; ++i, bit <<= 1)
	{
		if(m_sharedparams->accessoryparams[i].pAccessoryClass == pClass)
		{
			return (m_attachedAccessoryHistory & bit) == 0;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CItem::IsAccessoryHelperFree(const ItemString& helper)
{
	const int numAccessories = m_accessories.size();

	for(int i = 0; i < numAccessories; i++)
	{
		const SAccessoryParams *params = GetAccessoryParams(m_accessories[i].pClass);

		if (!params || params->category == helper)
			return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CItem::RemoveOwnerAttachedAccessories()
{
	const int numParams = m_sharedparams->accessoryparams.size();

	for (int i = 0; i < numParams; i++)
	{
		if(m_sharedparams->accessoryparams[i].attachToOwner)
		{
			CItem* pAccessory = GetAccessory(m_sharedparams->accessoryparams[i].pAccessoryClass);
			EntityId accessoryEntId = pAccessory ? pAccessory->GetEntityId() : 0;

			ResetCharacterAttachment(eIGS_FirstPerson, m_sharedparams->accessoryparams[i].attach_helper.c_str(), true, accessoryEntId);
		}
	}
}

//------------------------------------------------------------------------
void CItem::InitialSetup()
{
	if(!(GetISystem()->IsSerializingFile() && GetGameObject()->IsJustExchanging()))
	{
		if (IsServer())
		{
			for (TInitialSetup::iterator it = m_initialSetup.begin(); it != m_initialSetup.end(); ++it)
			{
				AttachAccessory(*it, true, true, true, true);
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::ReAttachAccessories()
{
	const int numAttachments = m_accessories.size();

	for (int i = 0; i < numAttachments; i++)
	{
		ReAttachAccessory(m_accessories[i].pClass);
	}

	AccessoriesChanged(false);
}

//------------------------------------------------------------------------
void CItem::ReAttachAccessory(IEntityClass* pClass)
{
	CItem* pAccessory = GetAccessory(pClass);
	const SAccessoryParams* params = GetAccessoryParams(pClass);

	if (pAccessory && params && (!params->attachToOwner || m_stats.selected))
	{
		SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), pAccessory->GetEntity(), eIGS_FirstPerson, params->attachToOwner);

		if(!params->attachToOwner)
		{
			Vec3 position = GetSlotHelperPos(eIGS_ThirdPerson, params->attach_helper.c_str(), false);
			GetEntity()->AttachChild(pAccessory->GetEntity());
			Matrix34 tm(Matrix34::CreateIdentity());
			tm.SetTranslation(position);
			pAccessory->GetEntity()->SetLocalTM(tm);
			pAccessory->SetParentId(GetEntityId());
			pAccessory->Physicalize(false, false);
		}
		else if(m_stats.viewmode == eIVM_ThirdPerson)
		{
			SetCharacterAttachment(eIGS_ThirdPerson, params->attach_helper.c_str(), pAccessory->GetEntity(), eIGS_ThirdPerson, params->attachToOwner);
		}
	}
}

//------------------------------------------------------------------------
void CItem::AccessoriesChanged(bool initialLoadoutSetup)
{
	if(m_stats.selected)
	{
		CActor* pOwner = GetOwnerActor();
		IActionController* pController = GetActionController();
		if(pController)
		{
			CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();
			const SMannequinItemParams* pParams = mannequinUserParams.FindOrCreateParams<SMannequinItemParams>(pController);
			
			UpdateAccessoryTags(pParams, pController->GetContext().state, true);

			if(pOwner && pOwner->IsClient())
			{
				UpdateScopeContexts(pController);
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::SwitchAccessory(const ItemString& accessory)
{
	uint16 classId = 0;
	bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassId(classId, accessory.c_str());

#if !defined(_RELEASE)
	if(!result)
	{
		char errorMsg[256];
		cry_sprintf(errorMsg, "CItem::SwitchAccessory failed to find network safe class id for %s", accessory.c_str());
		CRY_ASSERT_MESSAGE(result, errorMsg);
	}
#endif

	if(result)
		GetGameObject()->InvokeRMI(SvRequestAttachAccessory(), AccessoryParams(classId), eRMI_ToServer);
}

//------------------------------------------------------------------------
void CItem::DoSwitchAccessory(const ItemString& inAccessory, bool initialLoadoutSetup)
{
	IEntityClass* pNewClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(inAccessory.c_str());
	
	if(pNewClass)
	{
		const SAccessoryParams *params = GetAccessoryParams(pNewClass);

		if (params)
		{

			IEntityClass* pReplacingClass = NULL;

			const int numAccessories = m_accessories.size();

			for (int i = 0; i < numAccessories; i++)
			{
				const SAccessoryParams *p = GetAccessoryParams(m_accessories[i].pClass);

				if (p && p->category == params->category && pNewClass != m_accessories[i].pClass)
				{
					pReplacingClass = m_accessories[i].pClass;
					break;
				}
			}

			if (pReplacingClass)
			{
				if(pReplacingClass != pNewClass)
				{
					//Check if these two accessories aren't meant to be attached at the same time
					const SAccessoryParams* pReplacingParams = GetAccessoryParams(pReplacingClass);
					bool removeOldAccessory = true;
					if(pReplacingParams && pReplacingParams->alsoAttachDefault)
					{
						const SAccessoryParams* pNewParams = GetAccessoryParams(pNewClass);
						if(pNewParams && pNewParams->defaultAccessory)
						{
							removeOldAccessory = false;
						}
					}

					if(removeOldAccessory)
					{
						AttachAccessory(pReplacingClass, false, true, true);
					}
					const bool firstTimeAttached = gEnv->bMultiplayer ? IsFirstTimeAccessoryAttached(pNewClass) : false;
					AttachAccessory(pNewClass, true, true, true, firstTimeAttached, initialLoadoutSetup);
				}
				else
				{
					AttachAccessory(pReplacingClass, false, true, true);
				}
			}
			else
			{
				const bool firstTimeAttached = gEnv->bMultiplayer ? IsFirstTimeAccessoryAttached(pNewClass) : false;
				AttachAccessory(pNewClass, true, true, true, firstTimeAttached, initialLoadoutSetup);
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::DetachAccessory(const ItemString& accessory)
{
	uint16 classId = 0;
	bool result = g_pGame->GetIGameFramework()->GetNetworkSafeClassId(classId, accessory.c_str());

#if !defined(_RELEASE)
	if(!result)
	{
		char errorMsg[256];
		cry_sprintf(errorMsg, "CItem::DetachAccessory failed to find network safe class id for %s", accessory.c_str());
		CRY_ASSERT_MESSAGE(result, errorMsg);
	}
#endif

	if(result)
	{
		GetGameObject()->InvokeRMI(SvRequestDetachAccessory(), AccessoryParams(classId), eRMI_ToServer);
	}
}

//------------------------------------------------------------------------
void CItem::ResetAccessoriesScreen(IActor* pOwner)
{
	ClearItemFlags( eIF_Modifying | eIF_Transitioning);
}

//-----------------------------------------------------------------------
void CItem::PatchInitialSetup()
{
	const char* temp = NULL;
	// check if the initial setup accessories has been overridden in the level
	SmartScriptTable props;
	if (GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("Properties", props))
	{
		if(props->GetValue("initialSetup",temp) && temp !=NULL && temp[0]!=0)
		{
			m_properties.initialSetup = temp;
		}
	}

	//Replace initial setup from weapon xml, with initial setup defined for the entity (if neccesary)
	if(!m_properties.initialSetup.empty())
	{
		m_initialSetup.clear();

		//Different accessory names are separated by ","

		string::size_type lastPos = m_properties.initialSetup.find_first_not_of(",", 0);
		string::size_type pos = m_properties.initialSetup.find_first_of(",", lastPos);

		while ((string::npos != pos || string::npos != lastPos) && (m_initialSetup.size() < m_initialSetup.max_size()))
		{
			//Add to initial setup
			const char *name = m_properties.initialSetup.substr(lastPos, pos - lastPos).c_str();
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			if (pClass)
			{
				m_initialSetup.push_back(pClass);
			}

			lastPos = m_properties.initialSetup.find_first_not_of(",", pos);
			pos = m_properties.initialSetup.find_first_of(",", lastPos);
		}		
	}
}

//------------------------------------------------------------------------
void CItem::OnAttach(bool attach)
{
}
