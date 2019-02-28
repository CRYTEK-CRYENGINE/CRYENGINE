// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProtEntityObject.h"

#include "EntityPrototype.h"
#include "EntityPrototypeManager.h"
#include "Material\MaterialManager.h"
#include "Objects/ObjectLoader.h"

#include <CrySystem/ICryLink.h>
#include <CryGame/IGameFramework.h>

REGISTER_CLASS_DESC(CProtEntityObjectClassDesc);
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CProtEntityObject, CEntityObject)

//////////////////////////////////////////////////////////////////////////
// CProtEntityObject implementation.
//////////////////////////////////////////////////////////////////////////
CProtEntityObject::CProtEntityObject()
{
	ZeroStruct(m_prototypeGUID);
	m_prototypeName = "Unknown Archetype";
}

//////////////////////////////////////////////////////////////////////////
bool CProtEntityObject::Init(CBaseObject* prev, const string& file)
{
	bool result = CEntityObject::Init(prev, "");

	if (prev)
	{
		CProtEntityObject* pe = (CProtEntityObject*)prev;
		if (pe->m_prototype)
			SetPrototype(pe->m_prototype, true, true);
		else
		{
			m_prototypeGUID = pe->m_prototypeGUID;
			m_prototypeName = pe->m_prototypeName;
			m_prototypeLibrary = pe->m_prototypeLibrary;
		}
	}
	else if (!file.IsEmpty())
	{
		SetPrototype(CryGUID::FromString(file));
		SetUniqName(m_prototypeName);
	}

	//////////////////////////////////////////////////////////////////////////
	// Disable variables that are in archetype.
	//////////////////////////////////////////////////////////////////////////
	mv_castShadow.SetFlags(mv_castShadow.GetFlags() | IVariable::UI_DISABLED);
	mv_castShadowMinSpec->SetFlags(mv_castShadowMinSpec->GetFlags() | IVariable::UI_DISABLED);

	mv_ratioLOD.SetFlags(mv_ratioLOD.GetFlags() | IVariable::UI_DISABLED);
	mv_ratioViewDist.SetFlags(mv_ratioViewDist.GetFlags() | IVariable::UI_DISABLED);
	mv_recvWind.SetFlags(mv_recvWind.GetFlags() | IVariable::UI_DISABLED);

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::Done()
{
	if (m_prototype)
		m_prototype->RemoveUpdateListener(functor(*this, &CProtEntityObject::OnPrototypeUpdate));
	m_prototype = 0;
	ZeroStruct(m_prototypeGUID);
	CEntityObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CProtEntityObject::CreateGameObject()
{
	if (m_prototype)
	{
		return CEntityObject::CreateGameObject();
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SpawnEntity()
{
	if (m_prototype)
	{
		CEntityObject::SpawnEntity();

		PostSpawnEntity();
	}
}
//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::PostSpawnEntity()
{
	// Here we adjust any additional property.
	if (m_prototype && m_pEntity)
	{
		if (!GetMaterial())
		{
			string strPrototypeMaterial;
			CVarBlock* poVarBlock = m_prototype->GetProperties();
			if (poVarBlock)
			{
				IVariable* poPrototypeMaterial = poVarBlock->FindVariable("PrototypeMaterial");
				if (poPrototypeMaterial)
				{
					poPrototypeMaterial->Get(strPrototypeMaterial);
					if (!strPrototypeMaterial.IsEmpty())
					{
						CMaterial* poMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(strPrototypeMaterial);
						if (poMaterial)
						{
							// We must add a reference here, as when calling GetMatInfo()...
							// ... it will decrease the refcount, and since LoadMaterial doesn't
							// increase the refcount, it would get wrongly deallocated.
							poMaterial->AddRef();
							m_pEntity->SetMaterial(poMaterial->GetMatInfo());
						}
						else
						{
							Log("[EDITOR][CEntity::SpawnEntity()]Failed to load material %s", strPrototypeMaterial.GetBuffer());
						}
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SetPrototype(CryGUID guid, bool bForceReload)
{
	if (m_prototypeGUID == guid && bForceReload == false)
		return;

	m_prototypeGUID = guid;

	//m_fullPrototypeName = prototypeName;
	CEntityPrototypeManager* protMan = GetIEditorImpl()->GetEntityProtManager();
	CEntityPrototype* prototype = protMan->FindPrototype(guid);
	if (!prototype)
	{
		m_prototypeName = "Unknown Archetype";

		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot find Entity Archetype: %s for Entity %s %s", guid.ToString(), (const char*)GetName(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName().GetString()));
		return;
	}
	SetPrototype(prototype, bForceReload);
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::Serialize(CObjectArchive& ar)
{
	if (ar.bLoading)
	{
		// Serialize name at first.
		string name;
		ar.node->getAttr("Name", name);
		SetName(name);

		// Loading.
		CryGUID guid;
		ar.node->getAttr("Prototype", guid);
		SetPrototype(guid);
		if (ar.bUndo)
		{
			SpawnEntity();
		}

		string library;
		ar.node->getAttr("PrototypeLibrary", library);
	}
	else
	{
		// Saving.
		ar.node->setAttr("Name", m_prototypeName);
		ar.node->setAttr("Prototype", m_prototypeGUID);
		ar.node->setAttr("PrototypeLibrary", m_prototypeLibrary);
	}
	CEntityObject::Serialize(ar);

	if (ar.bLoading)
	{
		// If baseobject loading overided some variables.
		SyncVariablesFromPrototype(true);
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CProtEntityObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	return CEntityObject::Export(levelPath, xmlNode);
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::OnPrototypeUpdate()
{
	// Callback from prototype.
	OnPropertyChange(0);

	if (m_prototype)
	{
		m_prototypeName = m_prototype->GetName();
		SyncVariablesFromPrototype(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SetPrototype(CEntityPrototype* prototype, bool bForceReload, bool bOnlyDisabled)
{
	assert(prototype);

	if (prototype == m_prototype && !bForceReload)
		return;

	bool bRespawn = m_pEntity != 0;

	StoreUndo("Set Archetype");

	if (m_prototype)
		m_prototype->RemoveUpdateListener(functor(*this, &CProtEntityObject::OnPrototypeUpdate));

	m_prototype = prototype;
	m_prototype->AddUpdateListener(functor(*this, &CProtEntityObject::OnPrototypeUpdate));
	m_pLuaProperties = 0;
	m_prototypeGUID = m_prototype->GetGUID();
	m_prototypeName = m_prototype->GetName();
	IDataBaseLibrary* const pLibrary = m_prototype->GetLibrary();
	if (pLibrary)
	{
		m_prototypeLibrary = pLibrary->GetName();
	}

	SyncVariablesFromPrototype(bOnlyDisabled);

	SetClass(m_prototype->GetEntityClassName(), bForceReload);
	if (bRespawn)
		SpawnEntity();
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SyncVariablesFromPrototype(bool bOnlyDisabled)
{
	if (m_prototype)
	{
		CVarBlock* pVarsInPrototype = m_prototype->GetObjectVarBlock();
		mv_castShadow.CopyValue(pVarsInPrototype->FindVariable(mv_castShadow.GetName()));
		mv_castShadowMinSpec->CopyValue(pVarsInPrototype->FindVariable(mv_castShadowMinSpec->GetName()));
		mv_ratioLOD.CopyValue(pVarsInPrototype->FindVariable(mv_ratioLOD.GetName()));
		mv_ratioViewDist.CopyValue(pVarsInPrototype->FindVariable(mv_ratioViewDist.GetName()));
		mv_recvWind.CopyValue(pVarsInPrototype->FindVariable(mv_recvWind.GetName()));
		if (!bOnlyDisabled)
		{
			mv_outdoor.CopyValue(pVarsInPrototype->FindVariable(mv_outdoor.GetName()));
			mv_hiddenInGame.CopyValue(pVarsInPrototype->FindVariable(mv_hiddenInGame.GetName()));
			mv_renderNearest.CopyValue(pVarsInPrototype->FindVariable(mv_renderNearest.GetName()));
			mv_noDecals.CopyValue(pVarsInPrototype->FindVariable(mv_noDecals.GetName()));
		}
	}
}

void CProtEntityObjectClassDesc::EnumerateObjects(IObjectEnumerator* pEnumerator)
{
	RegisterAsDataBaseListener(EDB_TYPE_ENTITY_ARCHETYPE);
	GetIEditorImpl()->GetDBItemManager(EDB_TYPE_ENTITY_ARCHETYPE)->EnumerateObjects(pEnumerator);
}

bool CProtEntityObjectClassDesc::IsCreatable() const
{
	// Only allow use in legacy projects
	return gEnv->pGameFramework->GetIGame() != nullptr;
}

SPreviewDesc CProtEntityObjectClassDesc::GetPreviewDesc(const char* id) const
{
	auto getString = [](IVariable* pVariable)
	{
		string resultString;
		pVariable->Get(resultString);
		return resultString;
	};
	const auto pEntityPrototypeManager = GetIEditorImpl()->GetEntityProtManager();
	const auto guid = CryGUID::FromString(id);
	auto pPrototype = pEntityPrototypeManager->FindPrototype(guid);
	SPreviewDesc previewDesc;
	if (pPrototype)
	{
		previewDesc.materialFile = string(pPrototype->GetMaterial().toStdString().c_str()); // string copies contents of parameter
		previewDesc.modelFile = string(pPrototype->GetVisualObject().toStdString().c_str());
	}
	return previewDesc;
}

