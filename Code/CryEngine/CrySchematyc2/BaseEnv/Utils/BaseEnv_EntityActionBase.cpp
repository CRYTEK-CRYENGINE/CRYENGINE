// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/Utils/BaseEnv_EntityActionBase.h"

#include <CryEntitySystem/IEntitySystem.h>

namespace SchematycBaseEnv
{
	CEntityActionBase::CEntityActionBase()
		: m_pObject(nullptr)
		, m_pEntity(nullptr)
	{}

	bool CEntityActionBase::Init(const Schematyc2::SActionParams& params)
	{
		m_pObject = &params.object;
		m_pEntity = gEnv->pEntitySystem->GetEntity(params.object.GetEntityId().GetValue());
		return m_pEntity != nullptr;
	}

	Schematyc2::IObject& CEntityActionBase::GetObject() const
	{
		CRY_ASSERT(m_pObject);
		return *m_pObject;
	}

	inline IEntity& CEntityActionBase::GetEntity() const
	{
		CRY_ASSERT(m_pEntity);
		return *m_pEntity;
	}

	inline EntityId CEntityActionBase::GetEntityId() const
	{
		CRY_ASSERT(m_pEntity);
		return m_pEntity->GetId();
	}
}