// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "EntityComponentsCache.h"
#include "Entity.h"

//////////////////////////////////////////////////////////////////////////
void CEntitiesComponentPropertyCache::StoreEntities()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	IEntityItPtr it = GetISystem()->GetIEntitySystem()->GetEntityIterator();
	while (IEntity* pEntity = it->Next())
	{
		if (!pEntity->IsGarbage())
		{
			StoreEntity(*static_cast<CEntity*>(pEntity));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitiesComponentPropertyCache::RestoreEntities()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	IEntityItPtr it = GetISystem()->GetIEntitySystem()->GetEntityIterator();
	while (IEntity* pEntity = it->Next())
	{
		if (!pEntity->IsGarbage())
		{
			LoadEntity(*static_cast<CEntity*>(pEntity));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitiesComponentPropertyCache::StoreComponent(CEntity& entity, IEntityComponent& component)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	// Only Schematyc editable and user added components are eligible for storing
	if (!component.GetComponentFlags().Check(EEntityComponentFlags::SchematycEditable) &&
	    !component.GetComponentFlags().Check(EEntityComponentFlags::UserAdded))
	{
		return;
	}

	std::unique_ptr<Schematyc::CClassProperties> pClassProperties(new Schematyc::CClassProperties());
	pClassProperties->Read(component.GetClassDesc(), &component);

	SDoubleGUID doubleGuid = { entity.GetGuid(), component.GetGUID() };
	m_componentPropertyCache.insert(std::make_pair(doubleGuid, std::move(pClassProperties)));
}

//////////////////////////////////////////////////////////////////////////
void CEntitiesComponentPropertyCache::LoadComponent(CEntity& entity, IEntityComponent& component)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	// Only Schematyc editable and user added components are eligible for storing
	if (!component.GetComponentFlags().Check(EEntityComponentFlags::SchematycEditable) &&
	    !component.GetComponentFlags().Check(EEntityComponentFlags::UserAdded))
	{
		return;
	}

	auto iter = m_componentPropertyCache.find( { entity.GetGuid(), component.GetGUID() } );
	if (iter != m_componentPropertyCache.end())
	{
		auto& pClassProperties = iter->second;
		bool bMembersChanged = !pClassProperties->Compare(component.GetClassDesc(), &component);
		if (bMembersChanged)
		{
			// Restore members from the previously recorded class properties
			pClassProperties->Apply(component.GetClassDesc(), &component);
			// Notify this component about changed property.
			SEntityEvent entityEvent(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
			component.SendEvent(entityEvent);
		}
	}
}

void CEntitiesComponentPropertyCache::StoreEntity(CEntity& entity)
{
	entity.GetComponentsVector().ForEach(
	  [&](const SEntityComponentRecord& rec)
	{
		if (!rec.pComponent->GetClassDesc().GetName().IsEmpty())
		{
			StoreComponent(entity, *rec.pComponent.get());
		}
	});
}

void CEntitiesComponentPropertyCache::LoadEntity(CEntity& entity)
{
	entity.GetComponentsVector().ForEach(
	  [&](const SEntityComponentRecord& rec)
	{
		if (!rec.pComponent->GetClassDesc().GetName().IsEmpty())
		{
			LoadComponent(entity, *rec.pComponent.get());
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CEntitiesComponentPropertyCache::ClearCache()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);
	m_componentPropertyCache.clear();
}
