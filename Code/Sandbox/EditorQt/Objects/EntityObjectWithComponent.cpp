// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityObjectWithComponent.h"

REGISTER_CLASS_DESC(CEntityWithComponentClassDesc);
IMPLEMENT_DYNCREATE(CEntityObjectWithComponent, CEntityObject)

bool CEntityObjectWithComponent::Init(CBaseObject* prev, const string& file)
{
	if (file.size() > 0)
	{
		m_componentGUID = CryGUID::FromString(file);
	}

	// Utilize the Default entity class
	return CEntityObject::Init(prev, "Entity");
}

bool CEntityObjectWithComponent::CreateGameObject()
{
	if (!CEntityObject::CreateGameObject())
	{
		return false;
	}

	if (!m_componentGUID.IsNull() && m_pEntity->QueryComponentByInterfaceID(m_componentGUID) == nullptr)
	{
		if (IEntityComponent* pComponent = m_pEntity->CreateComponentByInterfaceID(m_componentGUID, false))
		{
			pComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);

			// Refresh inspector to show new component
			GetIEditorImpl()->GetObjectManager()->EmitPopulateInspectorEvent();
		}
	}

	return true;
}
