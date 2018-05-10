// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

namespace SchematycBaseEnv
{
	typedef TemplateUtils::CDelegate<void (const EntityId)> EntityMapVisitor;

	class CEntityMap
	{
	public:

		void MarkEntityForDestruction(EntityId entityId);
		void DestroyMarkedEntites();
		void AddObject(EntityId entityId, const Schematyc2::ObjectId& objectId);
		void RemoveObject(EntityId entityId);
		Schematyc2::ObjectId FindObjectId(EntityId entityId) const;
		Schematyc2::IObject* FindObject(EntityId entityId) const;
		void VisitEntities(const EntityMapVisitor& visitor);

	private:

		typedef std::vector<EntityId>                             EntityIds;
		typedef std::unordered_map<EntityId, Schematyc2::ObjectId> EntityObjectIds;

		EntityIds       m_entitiesMarkedForDestruction;
		EntityObjectIds m_entityObjectIds;
	};
}