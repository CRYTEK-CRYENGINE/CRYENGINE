// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

namespace SchematycBaseEnv
{
	class CEntityActionBase : public Schematyc2::IAction
	{
	public:

		CEntityActionBase();

		bool Init(const Schematyc2::SActionParams& params);
		Schematyc2::IObject& GetObject() const;
		IEntity& GetEntity() const;
		inline EntityId GetEntityId() const;

	private:

		Schematyc2::IObject* m_pObject;
		IEntity*            m_pEntity;
	};
}