// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include <CrySchematyc/Utils/ClassProperties.h>

// Entity property cache saves the values of the component properties when switching to game mode,
// so that they can be restored when switching our of the game mode
// It is only used when running with the Sandbox Editor

#include "CrySchematyc/Utils/ClassProperties.h"

class CEntitiesComponentPropertyCache
{
public:
	void ClearCache();

	void StoreEntities();
	void RestoreEntities();

private:
	void StoreEntity(CEntity& entity);
	void LoadEntity(CEntity& entity);
	void StoreComponent(CEntity& entity, IEntityComponent& component);
	void LoadComponent(CEntity& entity, IEntityComponent& component);

private:
	struct SDoubleGUID
	{
		CryGUID guid1;
		CryGUID guid2;
		bool operator==(const SDoubleGUID& rhs) const { return guid1 == rhs.guid1 && guid2 == rhs.guid2; }
	};
	struct HashDoubleGUID
	{
		size_t operator()(const SDoubleGUID& doubleGuid) const
		{
			std::hash<uint64> hasher;
			return hasher(doubleGuid.guid1.lopart) ^ hasher(doubleGuid.guid1.hipart) ^ hasher(doubleGuid.guid2.lopart) ^ hasher(doubleGuid.guid2.hipart);
		}
	};
	std::unordered_map<SDoubleGUID, std::unique_ptr<Schematyc::CClassProperties>, HashDoubleGUID> m_componentPropertyCache;
};
