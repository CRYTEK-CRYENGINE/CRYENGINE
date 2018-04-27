// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "BaseEnv/Utils/BaseEnv_EntityComponentBase.h"

#include <CryEntitySystem/IEntitySystem.h>

namespace SchematycBaseEnv
{
	CEntityComponentBase::CEntityComponentBase()
		: m_pObject(nullptr)
		, m_pEntity(nullptr)
	{}

	bool CEntityComponentBase::Init(const Schematyc2::SComponentParams& params)
	{
		m_pObject = &params.object;
		m_pEntity = gEnv->pEntitySystem->GetEntity(params.object.GetEntityId().GetValue());
		return m_pEntity != nullptr;
	}

	Schematyc2::IObject& CEntityComponentBase::GetObject() const
	{
		CRY_ASSERT(m_pObject);
		return *m_pObject;
	}

	IEntity& CEntityComponentBase::GetEntity() const
	{
		CRY_ASSERT(m_pEntity);
		return *m_pEntity;
	}

	EntityId CEntityComponentBase::GetEntityId() const
	{
		CRY_ASSERT(m_pEntity);
		return m_pEntity->GetId();
	}
}