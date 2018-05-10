// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 30:9:2004   14:22 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ItemSystem.h"
#include "IActorSystem.h"
#include "ScriptBind_ItemSystem.h"
#include <CryPhysics/IPhysics.h>

//------------------------------------------------------------------------
CScriptBind_ItemSystem::CScriptBind_ItemSystem(ISystem* pSystem, CItemSystem* pItemSystem, IGameFramework* pGameFramework)
	: m_pItemSystem(pItemSystem),
	m_pGameFramework(pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), pSystem);
	SetGlobalName("ItemSystem");

	RegisterGlobals();
	RegisterMethods();

	m_pEquipmentManager = static_cast<CEquipmentManager*>(pItemSystem->GetIEquipmentManager());
}

//------------------------------------------------------------------------
CScriptBind_ItemSystem::~CScriptBind_ItemSystem()
{
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::Reset(IFunctionHandler* pH)
{
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::GiveItem(IFunctionHandler* pH, const char* itemName)
{
	EntityId actorId = 0;
	if (pH->GetParamType(2) == svtPointer)
	{
		ScriptHandle sh;
		pH->GetParam(2, sh);
		actorId = (EntityId)sh.n;
	}
	else if (pH->GetParamType(2) == svtString)
	{
		const char* name = 0;
		pH->GetParam(2, name);
		if (name)
		{
			IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(name);
			actorId = pEntity->GetId();
		}
	}

	bool sound = true;
	if (pH->GetParamCount() > 2 && (pH->GetParamType(3) == svtNumber || pH->GetParamType(3) == svtBool))
		pH->GetParam(3, sound);

	bool selectItem = true;
	if (pH->GetParamCount() > 3 && (pH->GetParamType(4) == svtBool))
	{
		pH->GetParam(4, selectItem);
	}

	IActor* pActor = 0;
	if (actorId)
		pActor = m_pGameFramework->GetIActorSystem()->GetActor(actorId);
	else
		pActor = m_pGameFramework->GetClientActor();

	if (pActor)
	{
		ScriptHandle result(m_pItemSystem->GiveItem(pActor, itemName, sound, selectItem));
		if (result.n)
			return pH->EndFunction(result);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::SetActorItem(IFunctionHandler* pH, ScriptHandle actorId, ScriptHandle itemId, bool keepHistory)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor((EntityId)actorId.n);
	if (pActor)
		m_pItemSystem->SetActorItem(pActor, (EntityId)itemId.n, keepHistory);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::SetActorItemByName(IFunctionHandler* pH, ScriptHandle actorId, const char* name, bool keepHistory)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor((EntityId)actorId.n);
	if (pActor)
		m_pItemSystem->SetActorItem(pActor, name, keepHistory);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::GiveItemPack(IFunctionHandler* pH, ScriptHandle actorId, const char* packName)
{
	bool bKeepOld = false;
	bool bSelectPrimary = false;
	if (pH->GetParamCount() > 2 && (pH->GetParamType(3) == svtNumber || pH->GetParamType(3) == svtBool))
		pH->GetParam(3, bKeepOld);
	if (pH->GetParamCount() > 3 && (pH->GetParamType(4) == svtNumber || pH->GetParamType(4) == svtBool))
		pH->GetParam(4, bSelectPrimary);

	bool bOK = false;
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor((EntityId)actorId.n);
	if (pActor)
		bOK = m_pItemSystem->GetIEquipmentManager()->GiveEquipmentPack(pActor, packName, bKeepOld, bSelectPrimary);

	return pH->EndFunction(bOK);
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::GetPackPrimaryItem(IFunctionHandler* pH, const char* packName)
{
	CEquipmentManager::SEquipmentPack* pPack = m_pEquipmentManager->GetPack(packName);

	if (pPack != 0)
		return pH->EndFunction(pPack->m_primaryItem.c_str());

	GameWarning("[EquipmentMgr] ItemSystem.GetPackPrimaryItem: Pack '%s' cannot be found.", packName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::GetPackNumItems(IFunctionHandler* pH, const char* packName)
{
	CEquipmentManager::SEquipmentPack* pPack = m_pEquipmentManager->GetPack(packName);

	if (pPack != 0)
		return pH->EndFunction(pPack->NumItems());

	GameWarning("[EquipmentMgr] ItemSystem.GetPackItemByIndex: Pack '%s' cannot be found.", packName);
	return pH->EndFunction(0);
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::GetPackItemByIndex(IFunctionHandler* pH, const char* packName, int index)
{
	CEquipmentManager::SEquipmentPack* pPack = m_pEquipmentManager->GetPack(packName);

	if (pPack != 0)
	{
		const CEquipmentManager::SEquipmentPack::SEquipmentItem* pItem = pPack->GetItemByIndex(index);
		if (pItem)
			return pH->EndFunction(pItem->m_name.c_str());
		else
		{
			GameWarning("[EquipmentMgr] ItemSystem.GetPackItemByIndex: Index %d not valid. Pack '%s' has %d items.", index, packName, pPack->NumItems());
			return pH->EndFunction(false);
		}
	}

	GameWarning("[EquipmentMgr] ItemSystem.GetPackItemByIndex: Pack '%s' cannot be found.", packName);
	return pH->EndFunction(false);
}

//------------------------------------------------------------------------
void CScriptBind_ItemSystem::RegisterGlobals()
{
}

//------------------------------------------------------------------------
int CScriptBind_ItemSystem::SerializePlayerLTLInfo(IFunctionHandler* pH, bool reading)
{
	m_pItemSystem->SerializePlayerLTLInfo(reading);
	return pH->EndFunction(0);
}

//------------------------------------------------------------------------
void CScriptBind_ItemSystem::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_ItemSystem::

	SCRIPT_REG_TEMPLFUNC(Reset, "");
	SCRIPT_REG_TEMPLFUNC(GiveItem, "itemName, [actorName]");
	SCRIPT_REG_TEMPLFUNC(SetActorItem, "actorId, itemId, keepHistory");
	SCRIPT_REG_TEMPLFUNC(SetActorItemByName, "actorId, name, keepHistory");
	SCRIPT_REG_TEMPLFUNC(GiveItemPack, "actorId, packName, [keepOld]");
	SCRIPT_REG_TEMPLFUNC(GetPackPrimaryItem, "packName");
	SCRIPT_REG_TEMPLFUNC(GetPackNumItems, "packName");
	SCRIPT_REG_TEMPLFUNC(GetPackItemByIndex, "packName, index");
	SCRIPT_REG_TEMPLFUNC(SerializePlayerLTLInfo, "reading");
}

//------------------------------------------------------------------------
