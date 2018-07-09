// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/Utils/BaseEnv_EntityMap.h"

#include <CryEntitySystem/IEntitySystem.h>

namespace SchematycBaseEnv
{
	void CEntityMap::MarkEntityForDestruction(EntityId entityId)
	{
		m_entitiesMarkedForDestruction.push_back(entityId);
	}

	void CEntityMap::DestroyMarkedEntites()
	{
		EntityIds entitiesMarkedForDestruction = m_entitiesMarkedForDestruction;
		m_entitiesMarkedForDestruction.clear();
		for(EntityId entityId : entitiesMarkedForDestruction)
		{
			gEnv->pEntitySystem->RemoveEntity(entityId);
		}
	}

	void CEntityMap::AddObject(EntityId entityId, const Schematyc2::ObjectId& objectId)
	{
		m_entityObjectIds.insert(EntityObjectIds::value_type(entityId, objectId));
	}

	void CEntityMap::RemoveObject(EntityId entityId)
	{
		m_entityObjectIds.erase(entityId);
	}

	Schematyc2::ObjectId CEntityMap::FindObjectId(EntityId entityId) const
	{
		EntityObjectIds::const_iterator itObjectId = m_entityObjectIds.find(entityId);
		return itObjectId != m_entityObjectIds.end() ? itObjectId->second : Schematyc2::ObjectId();
	}

	Schematyc2::IObject* CEntityMap::FindObject(EntityId entityId) const
	{
		return gEnv->pSchematyc2->GetObjectManager().GetObjectById(FindObjectId(entityId));
	}

	void CEntityMap::VisitEntities(const EntityMapVisitor& visitor)
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(EntityObjectIds::value_type& entityObjectId : m_entityObjectIds)
			{
				visitor(entityObjectId.first);
			}
		}
	}
}