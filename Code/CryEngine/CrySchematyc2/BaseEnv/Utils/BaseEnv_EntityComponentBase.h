// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

namespace SchematycBaseEnv
{
	class CEntityComponentBase
	{
	public:

		CEntityComponentBase();

		bool Init(const Schematyc2::SComponentParams& params);
		Schematyc2::IObject& GetObject() const;
		IEntity& GetEntity() const;
		EntityId GetEntityId() const;

	private:

		Schematyc2::IObject* m_pObject;
		IEntity*            m_pEntity;
	};
}