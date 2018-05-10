// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityObjectWithParticleComponent.h"

#include <../../CryPlugins/CryDefaultEntities/Module/DefaultComponents/Effects/ParticleComponent.h>

REGISTER_CLASS_DESC(CEntityObjectWithParticleComponentClassDesc);
IMPLEMENT_DYNCREATE(CEntityObjectWithParticleComponent, CEntityObject)

using Cry::DefaultComponents::CParticleComponent;

bool CEntityObjectWithParticleComponent::Init(CBaseObject* prev, const string& file)
{
	if (prev != nullptr)
	{
		if (IEntity* pEntity = static_cast<CEntityObject*>(prev)->GetIEntity())
		{
			if (CParticleComponent* pParticleComponent = static_cast<CEntityObject*>(prev)->GetIEntity()->GetComponent<CParticleComponent>())
			{
				m_file = pParticleComponent->GetEffectName();
			}
		}
	}
	else
	{
		m_file = file;
	}

	// Utilize the Default entity class
	return CEntityObject::Init(prev, "Entity");
}

bool CEntityObjectWithParticleComponent::CreateGameObject()
{
	if (!CEntityObject::CreateGameObject())
	{
		return false;
	}

	if (Cry::DefaultComponents::CParticleComponent* pParticleComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CParticleComponent>())
	{
		if (m_file.size() > 0)
		{
			pParticleComponent->SetEffectName(m_file);
			pParticleComponent->LoadEffect(true);
		}

		pParticleComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);
	}

	return true;
}
