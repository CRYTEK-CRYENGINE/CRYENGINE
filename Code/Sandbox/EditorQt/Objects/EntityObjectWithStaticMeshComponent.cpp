// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityObjectWithStaticMeshComponent.h"

#include "Objects/BrushObject.h"

#include <DefaultComponents/Geometry/StaticMeshComponent.h>
#include <DefaultComponents/Physics/RigidBodyComponent.h>

using Cry::DefaultComponents::CStaticMeshComponent;
using Cry::DefaultComponents::CRigidBodyComponent;

REGISTER_CLASS_DESC(CEntityWithStaticMeshComponentClassDesc);
IMPLEMENT_DYNCREATE(CEntityObjectWithStaticMeshComponent, CEntityObject)

bool CEntityObjectWithStaticMeshComponent::Init(CBaseObject* prev, const string& file)
{
	if (prev != nullptr)
	{
		if (IEntity* pEntity = static_cast<CEntityObject*>(prev)->GetIEntity())
		{
			if (CStaticMeshComponent* pMeshComponent = static_cast<CEntityObject*>(prev)->GetIEntity()->GetComponent<CStaticMeshComponent>())
			{
				m_file = pMeshComponent->GetFilePath();
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

bool CEntityObjectWithStaticMeshComponent::CreateGameObject()
{
	if (!CEntityObject::CreateGameObject())
	{
		return false;
	}

	if (CStaticMeshComponent* pMeshComponent = m_pEntity->GetOrCreateComponent<CStaticMeshComponent>())
	{
		if (m_file.size() > 0)
		{
			pMeshComponent->SetObject(m_file.c_str(), true);

			pMeshComponent->ResetObject();
		}

		pMeshComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);
	}

	// Physicalize the object by default
	if (CRigidBodyComponent* pPhysicsComponent = m_pEntity->GetOrCreateComponent<CRigidBodyComponent>())
	{
		pPhysicsComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);
	}

	return true;
}

bool CEntityObjectWithStaticMeshComponent::ConvertFromObject(CBaseObject* pObject)
{
	bool bResult = CEntityObject::ConvertFromObject(pObject);

	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntityObject = static_cast<CEntityObject*>(pObject);
		IEntity* pIEntity = pEntityObject->GetIEntity();
		if (pIEntity == nullptr)
			return false;

		IStatObj* pStatObj = pIEntity->GetStatObj(0);
		if (pStatObj == nullptr)
			return false;

		m_file = pStatObj->GetFilePath();

		if (CStaticMeshComponent* pMeshComponent = m_pEntity->GetOrCreateComponent<CStaticMeshComponent>())
		{
			pMeshComponent->SetObjectDirect(pStatObj, true);
			pMeshComponent->ResetObject();

			pMeshComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);
		}

		// Physicalize the object by default
		if (CRigidBodyComponent* pPhysicsComponent = m_pEntity->GetOrCreateComponent<CRigidBodyComponent>())
		{
			pPhysicsComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);
		}

		return true;
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject* pBrushObject = static_cast<CBrushObject*>(pObject);
		
		IStatObj* pStatObj = pBrushObject->GetIStatObj();
		if (pStatObj == nullptr)
			return false;

		m_file = pStatObj->GetFilePath();
		
		if (CStaticMeshComponent* pMeshComponent = m_pEntity->GetOrCreateComponent<CStaticMeshComponent>())
		{
			pMeshComponent->SetObjectDirect(pStatObj, true);
			pMeshComponent->LoadFromDisk();

			pMeshComponent->ResetObject();

			pMeshComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);
		}

		// Physicalize the object by default
		if (CRigidBodyComponent* pPhysicsComponent = m_pEntity->GetOrCreateComponent<CRigidBodyComponent>())
		{
			pPhysicsComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);
		}

		return true;
	}

	return bResult;
}
