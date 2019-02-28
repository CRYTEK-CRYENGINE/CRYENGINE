// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AreaManager.h"
#include "Area.h"
#include "Entity.h"
#include <CryRenderer/IRenderAuxGeom.h>

//////////////////////////////////////////////////////////////////////////
CAreaManager::CAreaManager()
	: m_bAreasDirty(true)
{
	// Minimize run-time allocations.
	m_mapEntitiesToUpdate.reserve(32);
	m_areasAtPos[Threads::Audio].reserve(16);
	m_areasAtPos[Threads::Main].reserve(16);

	if (ISystemEventDispatcher* pSystemEventDispatcher = gEnv->pSystem->GetISystemEventDispatcher())
	{
		pSystemEventDispatcher->RegisterListener(this, "CAreaManager");
	}
}

//////////////////////////////////////////////////////////////////////////
CAreaManager::~CAreaManager()
{
	CRY_ASSERT(m_areas.size() == 0);

	if (ISystemEventDispatcher* pSystemEventDispatcher = gEnv->pSystem->GetISystemEventDispatcher())
	{
		pSystemEventDispatcher->RemoveListener(this);
	}
}

//////////////////////////////////////////////////////////////////////////
CArea* CAreaManager::CreateArea()
{
	CryAutoCriticalSectionNoRecursive lock(m_accessAreas);
	CArea* const pArea = new CArea(this);
	m_areas.push_back(pArea);
	m_areaGrid.Reset();
	m_bAreasDirty = true;
	return pArea;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::Unregister(CArea const* const pArea)
{
	CryAutoCriticalSectionNoRecursive lock(m_accessAreas);

	// Remove the area reference from the entity's area cache.
	m_mapAreaCache.erase_if(SRemoveIfNoAreasLeft(pArea, m_areas, m_areas.size()));

	// Also remove the area reference itself.
	TAreaPointers::iterator IterAreas(m_areas.begin());
	TAreaPointers::const_iterator const IterAreasEnd(m_areas.end());

	for (; IterAreas != IterAreasEnd; ++IterAreas)
	{
		if (pArea == (*IterAreas))
		{
			m_areas.erase(IterAreas);
			break;
		}
	}

	if (m_areas.empty())
	{
		stl::free_container(m_areas);
	}

	m_areaGrid.Reset();
	m_bAreasDirty = true;
}

//////////////////////////////////////////////////////////////////////////
IArea const* const CAreaManager::GetArea(size_t const nAreaIndex) const
{
#if defined(DEBUG_AREAMANAGER)
	if (nAreaIndex >= m_areas.size())
	{
		CryFatalError("<AreaManager>: GetArea index out of bounds (Count: %d Index: %d)!", static_cast<int>(m_areas.size()), static_cast<int>(nAreaIndex));
	}
#endif // DEBUG_AREAMANAGER

	return static_cast<IArea*>(m_areas.at(nAreaIndex));
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::DrawLinkedAreas(EntityId linkedId) const
{
	std::vector<CArea*> areas;
	size_t const nNumAreas = GetLinkedAreas(linkedId, -1, areas);

	for (size_t iIdx = 0; iIdx < nNumAreas; ++iIdx)
	{
		areas[iIdx]->Draw(iIdx);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAreaManager::GetLinkedAreas(EntityId linkedId, EntityId* pOutArray, int& outAndMaxResults) const
{
	int nMaxResults = outAndMaxResults;
	int nArrayIndex = 0;
	size_t const nCount = m_areas.size();

	for (size_t aIdx = 0; aIdx < nCount; aIdx++)
	{
		if (CArea* pArea = m_areas[aIdx])
		{
			const std::vector<EntityId>& ids = *pArea->GetEntities();

			if (!ids.empty())
			{
				size_t const nidCount = ids.size();

				for (size_t eIdx = 0; eIdx < nidCount; eIdx++)
				{
					if (ids[eIdx] == linkedId)
					{
						if (nArrayIndex < nMaxResults)
						{
							EntityId areaId = pArea->GetEntityID();
							pOutArray[nArrayIndex] = areaId;
							nArrayIndex++;
						}
						else
						{
							outAndMaxResults = nArrayIndex;
							return false;
						}
					}
				}
			}
		}
	}

	outAndMaxResults = nArrayIndex;
	return true;
}

//////////////////////////////////////////////////////////////////////////
size_t CAreaManager::GetLinkedAreas(EntityId linkedId, int areaId, std::vector<CArea*>& areas) const
{
	size_t const nCount = m_areas.size();

	for (size_t aIdx = 0; aIdx < nCount; aIdx++)
	{
		if (CArea* pArea = m_areas[aIdx])
		{
			const std::vector<EntityId>& ids = *pArea->GetEntities();

			if (!ids.empty())
			{
				size_t const nidCount = ids.size();

				for (size_t eIdx = 0; eIdx < nidCount; eIdx++)
				{
					if (ids[eIdx] == linkedId)
					{
						if (areaId == -1 || areaId == pArea->GetID())
							areas.push_back(pArea);
					}
				}
			}
		}
	}

	return areas.size();
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::MarkEntityForUpdate(EntityId const entityId)
{
	const size_t framesToUpdate = 1;
	TEntitiesToUpdateMap::iterator const iter(m_mapEntitiesToUpdate.find(entityId));

	if (iter != m_mapEntitiesToUpdate.end())
	{
		iter->second = framesToUpdate;
	}
	else
	{
		m_mapEntitiesToUpdate.insert(std::make_pair(entityId, framesToUpdate));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::TriggerAudioListenerUpdate(IArea const* const _pArea)
{
	CArea const* const pArea = static_cast<CArea const* const>(_pArea);
	IEntityItPtr const pIt = g_pIEntitySystem->GetEntityIterator();

	while (!pIt->IsEnd())
	{
		CEntity const* const pIEntity = static_cast<CEntity*>(pIt->Next());

		// Do this for all audio listener entities.
		if (pIEntity != nullptr && (pIEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)
		{
			EntityId const entityId = pIEntity->GetId();
			ExitArea(entityId, pArea);
			MarkEntityForUpdate(entityId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	// Update the area grid data structure.
	UpdateDirtyAreas();

	if (!m_mapEntitiesToUpdate.empty())
	{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
		float const debugPosX = 650.0f;
		float debugPosY = 500.0f;
		float const fColor[4] = { 0.0f, 1.0f, 0.0f, 0.7f };
		bool const bDrawDebug = CVar::pDrawAreaDebug->GetIVal() != 0;

		if (bDrawDebug)
		{
			IRenderAuxText::Draw2dLabel(debugPosX, debugPosY, 1.35f, fColor, false, "Entities to update: %d\n", static_cast<int>(m_mapEntitiesToUpdate.size()));
			debugPosY += 12.0f;
		}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

		// We need to swap here as UpdateEntity could recursively
		// add to the container that we are iterating over.
		TEntitiesToUpdateMap mapEntitiesToUpdate;
		mapEntitiesToUpdate.swap(m_mapEntitiesToUpdate);

		for (auto& entityIdPair : mapEntitiesToUpdate)
		{
			CRY_ASSERT(entityIdPair.second > 0);
			--(entityIdPair.second);

			CEntity* const pIEntity = g_pIEntitySystem->GetEntityFromID(entityIdPair.first);

			if (pIEntity != nullptr)
			{
				// Add a Z offset of at least 0.11 to be slightly above the offset of 0.1 set through "CShapeObject::GetShapeZOffset".
				Vec3 const position(pIEntity->GetWorldPos() + Vec3(0.0f, 0.0f, 0.11f));
				UpdateEntity(position, pIEntity);

#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
				if (bDrawDebug)
				{
					IRenderAuxText::Draw2dLabel(debugPosX + 10.0f, debugPosY, 1.35f, fColor, false, "Entity: %d (%s) Pos: (%.2f, %.2f, %.2f)\n", entityIdPair.first, pIEntity ? pIEntity->GetName() : "nullptr", position.x, position.y, position.z);
					debugPosY += 12.0f;
				}
#endif  // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
			}
		}

		mapEntitiesToUpdate.erase_if(IsDoneUpdating);
		m_mapEntitiesToUpdate.insert(mapEntitiesToUpdate.begin(), mapEntitiesToUpdate.end());
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::UpdateEntity(Vec3 const& position, CEntity* const pIEntity)
{
	EntityId const entityId = pIEntity->GetId();
	SAreasCache* pAreaCache = GetAreaCache(entityId);

	// Create a new area cache if necessary.
	if (pAreaCache == nullptr)
	{
		pAreaCache = MakeAreaCache(entityId);
	}

	CRY_ASSERT(pAreaCache != nullptr);

	// Audio listeners and moving entities affected by environment changes need to update more often
	// to ensure smooth fading.
	uint32 const extendedFlags = pIEntity->GetFlagsExtended();
	float const posDelta =
	  ((extendedFlags & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0) || ((extendedFlags & ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE) != 0)
	  ? 0.01f
	  : CVar::es_EntityUpdatePosDelta;

	if (pAreaCache != nullptr && !position.IsEquivalent(pAreaCache->lastUpdatePos, posDelta))
	{
		pAreaCache->lastUpdatePos = position;

		// First mark all cache entries that as if they are not in the grid.
		for (SAreaCacheEntry& areaCacheEntry : pAreaCache->entries)
		{
			CArea* const pArea = areaCacheEntry.pArea;

#if defined(DEBUG_AREAMANAGER)
			CheckArea(pArea);
#endif // DEBUG_AREAMANAGER

			areaCacheEntry.bInGrid = false;

			// Now pre-calculate position data.
			pArea->InvalidateCachedAreaData(entityId);
			pArea->CalcPosType(entityId, position);
		}

		CRY_ASSERT(m_areasAtPos[Threads::Main].empty());

		if (m_areaGrid.GetAreas(position, m_areasAtPos[Threads::Main]))
		{
			for (CArea* const pArea : m_areasAtPos[Threads::Main])
			{
				// Mark cache entries as if they are in the grid.
				SAreaCacheEntry* pAreaCacheEntry = nullptr;

				if (pAreaCache->GetCacheEntry(pArea, &pAreaCacheEntry))
				{
					// cppcheck-suppress nullPointer
					pAreaCacheEntry->bInGrid = true;
				}
				else
				{
					// if they are not yet in the cache, add them
					pAreaCache->entries.emplace_back(pArea, false, false);
					pArea->OnAddedToAreaCache(entityId);
				}

#if defined(DEBUG_AREAMANAGER)
				CheckArea(pArea);
#endif  // DEBUG_AREAMANAGER
			}

			m_areasAtPos[Threads::Main].clear();
		}

		AreaEnvironments areaEnvironments;

		// Go through all cache entries and process the areas.
		for (SAreaCacheEntry& areaCacheEntry : pAreaCache->entries)
		{
			CArea* const pArea = areaCacheEntry.pArea;

#if defined(DEBUG_AREAMANAGER)
			CheckArea(pArea);
#endif // DEBUG_AREAMANAGER

			// check if Area is hidden
			CEntity const* const pAreaEntity = g_pIEntitySystem->GetEntityFromID(pArea->GetEntityID());
			bool const bIsHidden = (pAreaEntity && pAreaEntity->IsHidden());

			// area was just hidden
			if (bIsHidden && (pArea->GetState() & Cry::AreaManager::EAreaState::Hidden) == 0)
			{
				pArea->LeaveArea(entityId);
				pArea->LeaveNearArea(entityId);
				areaCacheEntry.bNear = false;
				areaCacheEntry.bInside = false;
				pArea->AddState(Cry::AreaManager::EAreaState::Hidden);
				continue;
			}

			// area was just unhidden
			if (!bIsHidden && (pArea->GetState() & Cry::AreaManager::EAreaState::Hidden) != 0)
			{
				// ProcessArea will take care of properly setting cache entry data.
				areaCacheEntry.bNear = false;
				areaCacheEntry.bInside = false;
				pArea->RemoveState(Cry::AreaManager::EAreaState::Hidden);
			}

			// We process only for active areas in which grid we are.
			// Areas in our cache in which grid we are not get removed down below anyhow.
			if (!bIsHidden)
			{
				ProcessArea(pArea, areaCacheEntry, pAreaCache, position, pIEntity, areaEnvironments);
			}
		}

		// Go through all areas again and send accumulated events. (needs to be done in a separate step)
		for (SAreaCacheEntry& areaCacheEntry : pAreaCache->entries)
		{
#if defined(DEBUG_AREAMANAGER)
			CheckArea(areaCacheEntry.pArea);
#endif // DEBUG_AREAMANAGER

			areaCacheEntry.pArea->SendCachedEventsFor(entityId);
		}

		auto pIEntityAudioComponent = pIEntity->GetComponent<IEntityAudioComponent>();

		if (pIEntityAudioComponent != nullptr)
		{
			for (auto const& areaEnvironment : areaEnvironments)
			{
				pIEntityAudioComponent->SetEnvironmentAmount(areaEnvironment.audioEnvironmentId, areaEnvironment.amount, CryAudio::InvalidAuxObjectId);
			}
		}

		// Remove all entries in the cache which are no longer in the grid.
		if (!pAreaCache->entries.empty())
		{
			pAreaCache->entries.erase(std::remove_if(pAreaCache->entries.begin(), pAreaCache->entries.end(), SIsNotInGrid(entityId, m_areas, m_areas.size())), pAreaCache->entries.end());
		}

		if (pAreaCache->entries.empty())
		{
			DeleteAreaCache(entityId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAreaManager::QueryAudioAreas(Vec3 const& pos, SAudioAreaInfo* const pResults, size_t const numMaxResults, size_t& outNumResults)
{
	CryAutoCriticalSectionNoRecursive lock(m_accessAreas);

	outNumResults = 0;

	if (pResults != nullptr && numMaxResults > 0)
	{
		// Add a Z offset of at least 0.11 to be slightly above the offset of 0.1 set through "CShapeObject::GetShapeZOffset".
		Vec3 const position(pos + Vec3(0.0f, 0.0f, 0.11f));

		uint32 numAreas = 0;
		CRY_ASSERT(m_areasAtPos[Threads::Audio].empty());

		if (m_areaGrid.GetAreas(position, m_areasAtPos[Threads::Audio]))
		{
			SAreasCache areaCache;

			for (CArea* const pArea : m_areasAtPos[Threads::Audio])
			{
#if defined(DEBUG_AREAMANAGER)
				CheckArea(pArea);
#endif  // DEBUG_AREAMANAGER

				SAreaCacheEntry areaCacheEntry(pArea, false, false);
				areaCache.entries.push_back(areaCacheEntry);
			}

			AreaEnvironments areaEnvironments;

			for (SAreaCacheEntry& areaCacheEntry : areaCache.entries)
			{
				CArea* const pArea = areaCacheEntry.pArea;
				CEntity const* const pAreaEntity = g_pIEntitySystem->GetEntityFromID(pArea->GetEntityID());

				if (pAreaEntity && !pAreaEntity->IsHidden())
				{
					size_t const attachedEntities = pArea->GetEntityAmount();

					if (attachedEntities > 0)
					{
						for (size_t i = 0; i < attachedEntities; ++i)
						{
							CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(pArea->GetEntityByIdx(i));

							if (pIEntity != nullptr)
							{
								GetEnvFadeValue(areaCache, areaCacheEntry, position, pIEntity->GetId(), areaEnvironments);
							}
						}
					}
				}
			}

			for (auto const& areaEnvironment : areaEnvironments)
			{
				pResults[outNumResults].amount = areaEnvironment.amount;
				pResults[outNumResults].audioEnvironmentId = areaEnvironment.audioEnvironmentId;
				pResults[outNumResults].envProvidingEntityId = areaEnvironment.envProvidingEntityId;
				++outNumResults;
			}

			CRY_ASSERT(outNumResults == areaEnvironments.size());
			m_areasAtPos[Threads::Audio].clear();
		}
	}

	return outNumResults > 0;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::ProcessArea(
  CArea* const pArea,
  SAreaCacheEntry& areaCacheEntry,
  SAreasCache* const pAreaCache,
  Vec3 const& pos,
  CEntity const* const pIEntity,
  AreaEnvironments& areaEnvironments)
{
	Vec3 Closest3d(ZERO);
	bool bExclusiveUpdate = false;
	EntityId const entityId = pIEntity->GetId();
	bool const bIsPointWithin = (pArea->CalcPosType(entityId, pos) == AREA_POS_TYPE_2DINSIDE_ZINSIDE);

	if (bIsPointWithin)
	{
		// Was far/near but is inside now.
		if (!areaCacheEntry.bInside)
		{
			if (areaCacheEntry.bNear)
			{
				// Was near before only Enter needed.
				pArea->EnterArea(entityId);
				areaCacheEntry.bNear = false;
			}
			else
			{
				// Was far before both EnterNear and Enter needed.
				// This is optimized internally and might not recalculate but rather retrieve the cached data.
				pArea->CalcPointNearDistSq(entityId, pos, Closest3d, false);
				pArea->EnterNearArea(entityId, Closest3d, 0.0f);
				pArea->EnterArea(entityId);
			}

			areaCacheEntry.bInside = true;
		}

		uint32 const nEntityFlagsExtended = pIEntity->GetFlagsExtended();

		if ((nEntityFlagsExtended & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0 || (nEntityFlagsExtended & ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE) > 0)
		{
			// This updates the distance to closest border when inside the area. (TODO: Can this be called once from within CalcPosType when position is within area?)
			pArea->CalcPointWithinDist(entityId, pos, false);

			bExclusiveUpdate = ProceedExclusiveUpdateByHigherArea(pAreaCache, entityId, pos, pArea, pos, areaEnvironments); // TODO: Double check why "pos" is passed twice!

			if (!bExclusiveUpdate)
			{
				if (areaCacheEntry.bInside)
				{
					RetrieveEnvironmentAmount(pArea, 1.0f, 0.0f, INVALID_ENTITYID, areaEnvironments);

					SEntityEvent event(entityId, 0, 0, 0, 1.0f, 0.0f, 0.0f, pos);
					event.event = ENTITY_EVENT_MOVEINSIDEAREA;
					pArea->AddCachedEvent(event);
				}
			}
		}
	}
	else
	{
		// This is optimized internally and might not recalculate but rather retrieve the cached data.
		float const distanceSq = pArea->CalcPointNearDistSq(entityId, pos, Closest3d, false);
		float const greatestFadeDistance = pArea->GetGreatestFadeDistance();
		bool const isNear = ((distanceSq > 0.0f) && (distanceSq < greatestFadeDistance * greatestFadeDistance));

		// Was near or inside but is either far or hidden now.
		if (areaCacheEntry.bInside)
		{
			pArea->LeaveArea(entityId);
			areaCacheEntry.bInside = false;

			// Needs to be temporarily near again.
			areaCacheEntry.bNear = true;
		}

		if (isNear)
		{
			if (!areaCacheEntry.bNear)
			{
				float const distance = sqrt_tpl(distanceSq);
				pArea->EnterNearArea(entityId, Closest3d, distance);
				areaCacheEntry.bNear = true;
			}

			// is near now
			bExclusiveUpdate = ProceedExclusiveUpdateByHigherArea(pAreaCache, entityId, pos, pArea, Closest3d, areaEnvironments);

			// if there is no cached event waiting, Fade can be overwritten
			if (!bExclusiveUpdate)
			{
				float const distance = sqrt_tpl(distanceSq);
				RetrieveEnvironmentAmount(pArea, 0.0f, distance, INVALID_ENTITYID, areaEnvironments);

				CRY_ASSERT(areaCacheEntry.bNear && !areaCacheEntry.bInside); // We must be near but not inside yet!
				SEntityEvent event(entityId, 0, 0, 0, distance, 0.0f, 0.0f, Closest3d);
				event.event = ENTITY_EVENT_MOVENEARAREA;
				pArea->AddCachedEvent(event);
			}
		}
		else if (areaCacheEntry.bNear)
		{
			pArea->LeaveNearArea(entityId);
			areaCacheEntry.bNear = false;

			RetrieveEnvironmentAmount(pArea, 0.0f, 0.0f, INVALID_ENTITYID, areaEnvironments);
		}
	}
}

//	checks for higher areas in the same group, the player is also in
//	finds multiple point candidates and picks the furthers
//	change fade value the deeper player is inside
//////////////////////////////////////////////////////////////////////////
bool CAreaManager::ProceedExclusiveUpdateByHigherArea(
  SAreasCache* const pAreaCache,
  EntityId const entityId,
  Vec3 const& entityPos,
  CArea* const pArea,
  Vec3 const& onLowerHull,
  AreaEnvironments& areaEnvironments)
{
	// we try to catch 4 cases here.
	// 1) not-inside-low, not-inside-high: both areas a close, it depends on which area is closer to the player
	// 2) inside-low, not-inside-high: typical approaching high area scenario
	// 3) not-inside-low, inside-high: reversed approach from within a high area to fade in a low area
	// 4) inside-low, inside-high: both inside, so we fade in the lower area on the higher area hull, if that point is also in the lower
	bool bResult = false;
	int const currentGroupId = pArea->GetGroup();

	if (currentGroupId > 0)
	{
		TAreaPointers apAreasOfSameGroup;

		//	Find areas of higher priority that belong to the same group as the passed pArea.
		int const minPriority = pArea->GetPriority();

		for (SAreaCacheEntry const& areaCacheEntry : pAreaCache->entries)
		{
			CArea* const pHigherPrioArea = areaCacheEntry.pArea;

#if defined(DEBUG_AREAMANAGER)
			CheckArea(pHigherPrioArea);
#endif // DEBUG_AREAMANAGER

			// Must be of same group.
			if (currentGroupId == pHigherPrioArea->GetGroup())
			{
				// Only check areas which are active (not hidden).
				if (minPriority < pHigherPrioArea->GetPriority() && (pHigherPrioArea->GetState() & Cry::AreaManager::EAreaState::Hidden) == 0)
				{
					apAreasOfSameGroup.push_back(pHigherPrioArea);
				}
			}
		}

		// Continue only if there were areas found of higher priority.
		if (!apAreasOfSameGroup.empty())
		{
			bool bPosInLowerArea = false;
			SAreaCacheEntry* pAreaCachEntry = nullptr;

			if (pAreaCache->GetCacheEntry(pArea, &pAreaCachEntry))
			{
				// cppcheck-suppress nullPointer
				bPosInLowerArea = pAreaCachEntry->bInside;
			}

			float largestDistanceSq = bPosInLowerArea ? 0.0f : FLT_MAX;
			CArea* pHigherAreaWithLargestDistance = nullptr;
			Vec3 higherClosest3d(ZERO);
			bool bPosInHighestArea = false;

			TAreaPointers::const_iterator const IterAreaPointersEnd(apAreasOfSameGroup.end());

			for (TAreaPointers::const_iterator IterAreaPointers(apAreasOfSameGroup.begin()); IterAreaPointers != IterAreaPointersEnd; ++IterAreaPointers)
			{
				CArea* const pHigherArea = (*IterAreaPointers);
				Vec3 tempHigherClosest3d(ZERO);

				// This is optimized internally and might not recalculate but rather retrieve the cached data.
				bool const bPosInHighArea = pHigherArea->CalcPointWithin(entityId, entityPos);
				float const distanceSq = pHigherArea->CalcPointNearDistSq(entityId, entityPos, tempHigherClosest3d, false);

				bool bUseThisArea = distanceSq > 0.0f && ((largestDistanceSq < distanceSq && bPosInLowerArea) || (distanceSq < largestDistanceSq && !bPosInLowerArea));
				// reject cases when Pos is not inside new High Area and we already found a high area
				// and add the case where Pos is inside new High Area, but would be rejected by Higher Area,
				bUseThisArea = (bUseThisArea && !(!bPosInHighArea && bPosInHighestArea)) || (bPosInHighArea && !bPosInHighestArea);

				if (bUseThisArea)
				{
					largestDistanceSq = distanceSq;
					pHigherAreaWithLargestDistance = pHigherArea;
					higherClosest3d = tempHigherClosest3d;
					bPosInHighestArea = bPosInHighArea;
				}
			}

			// case 2) if we are in the low area, but not inside the high one, then no need to update exclusive
			if (!bPosInLowerArea || bPosInHighestArea)
			{
				// did we find the best Higher Area to control this Area?
				if (pHigherAreaWithLargestDistance != nullptr)
				{
					bool const bHighest3dPointInLowerArea = pArea->CalcPointWithin(INVALID_ENTITYID, higherClosest3d);
					bool const bLower3dPointInHighestArea = pHigherAreaWithLargestDistance->CalcPointWithin(entityId, onLowerHull);

					// case 1) where approaching the lower area is closer than the higher
					if (!bPosInLowerArea && !bPosInHighestArea)
					{
						float const distanceToLowerSq = onLowerHull.GetSquaredDistance(entityPos);
						float const distanceToHigherSq = higherClosest3d.GetSquaredDistance(entityPos);

						if (distanceToLowerSq < distanceToHigherSq || (!bHighest3dPointInLowerArea && !bLower3dPointInHighestArea))
						{
							return false;
						}
						else
						{
							// best thing would be to take onLowerHull and use that Position to calculate a point
							// on HigherHull, check again, if that would be inside the lower, and fade from there.
						}
					}

					// case 4)
					if (bPosInLowerArea && bPosInHighestArea && bHighest3dPointInLowerArea)
					{
						float const distance = sqrt_tpl(largestDistanceSq);
						float const innerFadeDistance = pHigherAreaWithLargestDistance->GetInnerFadeDistance();
						float const fade = (innerFadeDistance > 0.0f) ? std::max<float>(0.0f, (innerFadeDistance - distance) / innerFadeDistance) : 0.0f;
						pArea->ExclusiveUpdateAreaInside(entityId, pHigherAreaWithLargestDistance->GetEntityID(), fade);
						RetrieveEnvironmentAmount(pArea, 0.0f, distance, INVALID_ENTITYID, areaEnvironments);
					}
					else
					{
						// if point on higher hull is not the same as point on lower hull, then lets not fade in the effect
						if (bPosInHighestArea && onLowerHull.GetDistance(higherClosest3d) > 0.01f)
						{
							pArea->ExclusiveUpdateAreaNear(entityId, pHigherAreaWithLargestDistance->GetEntityID(), 0.0f, onLowerHull);
						}
						else
						{
							float const distanceToLowerSq = onLowerHull.GetSquaredDistance(entityPos);
							float const distance = sqrt_tpl(distanceToLowerSq);
							pArea->ExclusiveUpdateAreaNear(entityId, pHigherAreaWithLargestDistance->GetEntityID(), distance, onLowerHull);
						}
					}

					bResult = true;
				}
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::ExitArea(EntityId const entityId, CArea const* const pArea)
{
	if (pArea != nullptr)
	{
		SAreasCache* const pAreaCache = GetAreaCache(entityId);

		if (pAreaCache != nullptr)
		{
			pAreaCache->lastUpdatePos = ZERO;

			if (!pAreaCache->entries.empty())
			{
				TAreaCacheVector::iterator Iter(pAreaCache->entries.begin());
				TAreaCacheVector::const_iterator const IterEnd(pAreaCache->entries.end());

				for (; Iter != IterEnd; ++Iter)
				{
					SAreaCacheEntry const& areaCacheEntry = *Iter;

#if defined(DEBUG_AREAMANAGER)
					CheckArea(areaCacheEntry.pArea);
#endif    // DEBUG_AREAMANAGER

					if (pArea == areaCacheEntry.pArea)
					{
						if (areaCacheEntry.bInside)
						{
							areaCacheEntry.pArea->LeaveArea(entityId);
							areaCacheEntry.pArea->LeaveNearArea(entityId);
						}
						else if (areaCacheEntry.bNear)
						{
							areaCacheEntry.pArea->LeaveNearArea(entityId);
						}

						areaCacheEntry.pArea->OnRemovedFromAreaCache(entityId);
						pAreaCache->entries.erase(Iter);

						break;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAreaManager::GetEnvFadeValue(
  SAreasCache const& areaCache,
  SAreaCacheEntry& areaCacheEntry,
  Vec3 const& entityPos,
  EntityId const envProvidingEntityId,
  AreaEnvironments& areaEnvironments)
{
	bool bSuccess = false;
	bool const bIsPointWithin = (areaCacheEntry.pArea->CalcPosType(INVALID_ENTITYID, entityPos) == AREA_POS_TYPE_2DINSIDE_ZINSIDE);

	if (bIsPointWithin)
	{
		areaCacheEntry.bInside = true;

		// This updates the distance to closest border when inside the area.
		areaCacheEntry.pArea->CalcPointWithinDist(INVALID_ENTITYID, entityPos, false);
		Vec3 temp(ZERO);
		bSuccess = GetEnvFadeValueInner(areaCache, areaCacheEntry, entityPos, temp, envProvidingEntityId, areaEnvironments);

		if (!bSuccess)
		{
			bSuccess = RetrieveEnvironmentAmount(areaCacheEntry.pArea, 1.0f, 0.0f, envProvidingEntityId, areaEnvironments);
		}
	}
	else
	{
		areaCacheEntry.bInside = false;

		// This is optimized internally and might not recalculate but rather retrieve the cached data.
		Vec3 posOnLowerArea(ZERO);
		float const distanceSq = areaCacheEntry.pArea->CalcPointNearDistSq(INVALID_ENTITYID, entityPos, posOnLowerArea, false);
		float const greatestFadeDistance = areaCacheEntry.pArea->GetGreatestFadeDistance();
		bool const isNear = ((distanceSq > 0) && (distanceSq < greatestFadeDistance * greatestFadeDistance));

		if (isNear)
		{
			bSuccess = GetEnvFadeValueInner(areaCache, areaCacheEntry, entityPos, posOnLowerArea, envProvidingEntityId, areaEnvironments);

			if (!bSuccess)
			{
				float const distance = sqrt_tpl(distanceSq);
				bSuccess = RetrieveEnvironmentAmount(areaCacheEntry.pArea, 0.0f, distance, envProvidingEntityId, areaEnvironments);
			}
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CAreaManager::GetEnvFadeValueInner(
  SAreasCache const& areaCache,
  SAreaCacheEntry const& areaCacheEntry,
  Vec3 const& entityPos,
  Vec3 const& posOnLowerArea,
  EntityId const envProvidingEntityId,
  AreaEnvironments& areaEnvironments)
{
	bool bSuccess = false;
	int const currentGroupId = areaCacheEntry.pArea->GetGroup();

	if (currentGroupId > 0)
	{
		// Find areas of higher priority that belong to the same group as the passed pArea.
		int const minPriority = areaCacheEntry.pArea->GetPriority();
		TAreaPointers areasOfSameGroup;

		for (SAreaCacheEntry const& tempAreaCacheEntry : areaCache.entries)
		{
			CArea* const pHigherPrioArea = tempAreaCacheEntry.pArea;

#if defined(DEBUG_AREAMANAGER)
			CheckArea(pHigherPrioArea);
#endif // DEBUG_AREAMANAGER

			// Must be of same group.
			if (currentGroupId == pHigherPrioArea->GetGroup())
			{
				// Only check areas which are active (not hidden).
				if (minPriority < pHigherPrioArea->GetPriority() && (pHigherPrioArea->GetState() & Cry::AreaManager::EAreaState::Hidden) == 0)
				{
					areasOfSameGroup.push_back(pHigherPrioArea);
				}
			}
		}

		// Continue only if there were areas found of higher priority.
		if (!areasOfSameGroup.empty())
		{
			bool const bPosInLowerArea = areaCacheEntry.bInside;
			float largestDistanceSq = bPosInLowerArea ? 0.0f : FLT_MAX;
			CArea* pHigherAreaWithLargestDistance = nullptr;
			Vec3 higherClosest3d(ZERO);
			Vec3 tempHigherClosest3d(ZERO);
			bool bPosInHighestArea = false;

			for (CArea* const pHigherArea : areasOfSameGroup)
			{
				// This is optimized internally and might not recalculate but rather retrieve the cached data.
				bool const bPosInHighArea = pHigherArea->CalcPointWithin(INVALID_ENTITYID, entityPos);
				float const distanceSq = pHigherArea->CalcPointNearDistSq(INVALID_ENTITYID, entityPos, tempHigherClosest3d, false);

				bool bUseThisArea = distanceSq > 0.0f && ((largestDistanceSq < distanceSq && bPosInLowerArea) || (distanceSq < largestDistanceSq && !bPosInLowerArea));
				// reject cases when Pos is not inside new High Area and we already found a high area
				// and add the case where Pos is inside new High Area, but would be rejected by Higher Area,
				bUseThisArea = (bUseThisArea && !(!bPosInHighArea && bPosInHighestArea)) || (bPosInHighArea && !bPosInHighestArea);

				if (bUseThisArea)
				{
					largestDistanceSq = distanceSq;
					pHigherAreaWithLargestDistance = pHigherArea;
					higherClosest3d = tempHigherClosest3d;
					bPosInHighestArea = bPosInHighArea;
				}
			}

			// case 2) if we are in the low area, but not inside the high one, then no need to update exclusive
			if (!bPosInLowerArea || bPosInHighestArea)
			{
				// did we find the best Higher Area to control this Area?
				if (pHigherAreaWithLargestDistance != nullptr)
				{
					bool const bHighest3dPointInLowerArea = areaCacheEntry.pArea->CalcPointWithin(INVALID_ENTITYID, higherClosest3d);
					bool const bLower3dPointInHighestArea = pHigherAreaWithLargestDistance->CalcPointWithin(INVALID_ENTITYID, entityPos);
					float distanceSq = largestDistanceSq;

					// case 1) where approaching the lower area is closer than the higher
					if (!bPosInLowerArea && !bPosInHighestArea)
					{
						float const distanceToLowerSq = posOnLowerArea.GetSquaredDistance(entityPos);
						float const distanceToHigherSq = higherClosest3d.GetSquaredDistance(entityPos);

						if (distanceToLowerSq < distanceToHigherSq || (!bHighest3dPointInLowerArea && !bLower3dPointInHighestArea))
						{
							return false;
						}
						else
						{
							// best thing would be to take onLowerHull and use that Position to calculate a point
							// on HigherHull, check again, if that would be inside the lower, and fade from there.
						}
					}

					float const distance = sqrt_tpl(distanceSq);
					bSuccess = RetrieveEnvironmentAmount(areaCacheEntry.pArea, 0.0f, distance, envProvidingEntityId, areaEnvironments);
				}
			}
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CAreaManager::RetrieveEnvironmentAmount(
  CArea const* const pArea,
  float const amount,
  float const distance,
  EntityId const envProvidingEntityId,
  AreaEnvironments& areaEnvironments)
{
	bool bSuccess = false;
	size_t const attachedEntities = pArea->GetEntityAmount();

	if (attachedEntities > 0)
	{
		for (size_t i = 0; i < attachedEntities; ++i)
		{
			CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(pArea->GetEntityByIdx(i));

			if (pIEntity != nullptr)
			{
				auto pIEntityAudioComponent = pIEntity->GetComponent<IEntityAudioComponent>();

				if (pIEntityAudioComponent != nullptr)
				{
					CryAudio::EnvironmentId const audioEnvironmentId = pIEntityAudioComponent->GetEnvironmentId();

					if (audioEnvironmentId != CryAudio::InvalidEnvironmentId)
					{
						float finalAmount = amount;

						if (distance > 0.0f)
						{
							finalAmount = pIEntityAudioComponent->GetEnvironmentFadeDistance();
							finalAmount = (finalAmount > 0.0f) ? std::max<float>(0.0f, (finalAmount - distance) / finalAmount) : 0.0f;
						}

						bool bNewElement = true;

						for (auto& areaEnvironment : areaEnvironments)
						{
							if (areaEnvironment.audioEnvironmentId == audioEnvironmentId)
							{
								areaEnvironment.amount = std::max<float>(areaEnvironment.amount, finalAmount);
								bNewElement = false;
								break;
							}
						}

						if (bNewElement)
						{
							SAudioAreaInfo const areaInfo(finalAmount, audioEnvironmentId, envProvidingEntityId);
							areaEnvironments.push_back(areaInfo);
						}

						bSuccess = true;
					}
				}
			}
		}
	}

	return bSuccess;
}

// do onexit for all areas pEntity is in - do it before kill pEntity
//////////////////////////////////////////////////////////////////////////
void CAreaManager::ExitAllAreas(EntityId const entityId)
{
	SAreasCache* const pAreaCache = GetAreaCache(entityId);

	if (pAreaCache != nullptr && !pAreaCache->entries.empty())
	{
		for (SAreaCacheEntry const& areaCacheEntry : pAreaCache->entries)
		{
#if defined(DEBUG_AREAMANAGER)
			CheckArea(areaCacheEntry.pArea);
#endif // DEBUG_AREAMANAGER

			if (areaCacheEntry.bInside)
			{
				areaCacheEntry.pArea->LeaveArea(entityId);
				areaCacheEntry.pArea->LeaveNearArea(entityId);
			}
			else if (areaCacheEntry.bNear)
			{
				areaCacheEntry.pArea->LeaveNearArea(entityId);
			}

			areaCacheEntry.pArea->OnRemovedFromAreaCache(entityId);
		}

		pAreaCache->entries.clear();
		DeleteAreaCache(entityId);
	}

	m_mapEntitiesToUpdate.erase(entityId);
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::DrawAreas(ISystem const* const pSystem)
{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
	bool bDraw = CVar::pDrawAreas->GetIVal() != 0;

	if (bDraw)
	{
		// Used to give the drawn area a distinct color.
		size_t i = 0;

		for (auto const pArea : m_areas)
		{
			pArea->Draw(i++);
		}
	}

	int const drawDebugValue = CVar::pDrawAreaDebug->GetIVal();
	bDraw = drawDebugValue != 0;
	float const color[4] = { 0.0f, 1.0f, 0.0f, 0.7f };

	if (bDraw)
	{
		float posY = 10.0f;
		IRenderAuxText::Draw2dLabel(10.0f, posY, 1.5f, color, false, "<AreaManager>");
		posY += 20.0f;
		IRenderAuxText::Draw2dLabel(30.0f, posY, 1.3f, color, false, "Entities: %d | Areas in Grid: %d", static_cast<int>(m_mapAreaCache.size()), static_cast<int>(m_areaGrid.GetNumAreas()));
		posY += 20.0f;

		for (auto const& areaCachePair : m_mapAreaCache)
		{
			CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(areaCachePair.first);

			if (pIEntity != nullptr)
			{
				EntityId const entityId = pIEntity->GetId();

				if (drawDebugValue == 1 || drawDebugValue == 2 || entityId == static_cast<EntityId>(drawDebugValue))
				{
					SAreasCache const& areaCache(areaCachePair.second);
					Vec3 const& pos(areaCache.lastUpdatePos);

					IRenderAuxText::Draw2dLabel(30.0f, posY, 1.3f, color, false, "Entity: %d (%s) Pos: (%.2f, %.2f, %.2f) Areas in AreaCache: %d", entityId, pIEntity->GetName(), pos.x, pos.y, pos.z, static_cast<int>(areaCache.entries.size()));
					posY += 12.0f;

					// Invalidate grid flag in area cache
					for (SAreaCacheEntry const& areaCacheEntry : areaCache.entries)
					{
						CArea* const pArea = areaCacheEntry.pArea;

	#if defined(DEBUG_AREAMANAGER)
						CheckArea(pArea);
	#endif    // DEBUG_AREAMANAGER

						// This is optimized internally and might not recalculate but rather retrieve the cached data.
						float const distanceSq = pArea->CalcPointNearDistSq(entityId, pos, false, false);
						bool const bIsPointWithin = (pArea->CalcPosType(entityId, pos) == AREA_POS_TYPE_2DINSIDE_ZINSIDE);

						CryFixedStringT<16> areaTypeName("unknown");
						EEntityAreaType const areaType = pArea->GetAreaType();

						switch (areaType)
						{
						case ENTITY_AREA_TYPE_SHAPE:
							areaTypeName = "Shape";
							break;
						case ENTITY_AREA_TYPE_BOX:
							areaTypeName = "Box";
							break;
						case ENTITY_AREA_TYPE_SPHERE:
							areaTypeName = "Sphere";
							break;
						case ENTITY_AREA_TYPE_GRAVITYVOLUME:
							areaTypeName = "GravityVolume";
							break;
						case ENTITY_AREA_TYPE_SOLID:
							areaTypeName = "Solid";
							break;
						}

						CryFixedStringT<16> const state(bIsPointWithin ? "Inside" : (areaCacheEntry.bNear ? "Near" : "Far"));
						IRenderAuxText::Draw2dLabel(30.0f, posY, 1.3f, color, false, "Name: %s AreaID: %d GroupID: %d Priority: %d Type: %s Distance: %.2f State: %s Entities: %d", pArea->GetAreaEntityName(), pArea->GetID(), pArea->GetGroup(), pArea->GetPriority(), areaTypeName.c_str(), sqrt_tpl(distanceSq > 0.0f ? distanceSq : 0.0f), state.c_str(), static_cast<int>(pArea->GetCacheEntityCount()));
						posY += 12.0f;
					}

					// Next entity.
					posY += 12.0f;
				}
			}
		}
	}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::DrawGrid()
{
	m_areaGrid.Draw();
}

//////////////////////////////////////////////////////////////////////////
size_t CAreaManager::MemStat()
{
	size_t memSize = sizeof(*this);

	for (auto const pArea : m_areas)
	{
		memSize += pArea->MemStat();
	}

	return memSize;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::ResetAreas()
{
	for (auto const& cacheIt : m_mapAreaCache)
	{
		EntityId const entityId = cacheIt.first;
		SAreasCache const& areaCache(cacheIt.second);
		CEntity* const pEntity = g_pIEntitySystem->GetEntityFromID(entityId);

		if (pEntity != nullptr)
		{
			for (SAreaCacheEntry const& areaCacheEntry : areaCache.entries)
			{
				CArea* const pArea = areaCacheEntry.pArea;

#if defined(DEBUG_AREAMANAGER)
				CheckArea(pArea);
#endif  // DEBUG_AREAMANAGER

				if (areaCacheEntry.bInside)
				{
					pArea->LeaveArea(entityId);
					pArea->LeaveNearArea(entityId);
				}
				else if (areaCacheEntry.bNear)
				{
					pArea->LeaveNearArea(entityId);
				}
			}
		}
	}

	m_mapAreaCache.clear();
	stl::free_container(m_mapEntitiesToUpdate);

	// invalidate cached event + data
	for (auto const pArea : m_areas)
	{
		pArea->ClearCachedEvents();
		pArea->ReleaseCachedAreaData();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::UnloadAreas()
{
	ResetAreas();
	m_areaGrid.Reset();
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::SetAreasDirty()
{
	m_bAreasDirty = true;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::SetAreaDirty(IArea* pArea)
{
	if (!m_bAreasDirty)
	{
		bool const success = m_areaGrid.ResetArea(static_cast<CArea*>(pArea));

		if (!success)
			m_bAreasDirty = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::UpdateDirtyAreas()
{
	if (m_bAreasDirty)
	{
		m_areaGrid.Compile(m_areas);
		m_bAreasDirty = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::AddEventListener(IAreaManagerEventListener* pListener)
{
	stl::push_back_unique(m_EventListeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::RemoveEventListener(IAreaManagerEventListener* pListener)
{
	stl::find_and_erase(m_EventListeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::OnEvent(EEntityEvent event, EntityId TriggerEntityID, IArea* pArea)
{
	for (auto const pListener : m_EventListeners)
	{
		pListener->OnAreaManagerEvent(event, TriggerEntityID, pArea);
	}
}

//////////////////////////////////////////////////////////////////////////
int CAreaManager::GetNumberOfPlayersNearOrInArea(CArea const* const pArea)
{
	// Find the area index
	for (auto const pRegisteredArea : m_areas)
	{
		if (pRegisteredArea == pArea)
		{
			// Now find how many players are actually inside the area
			int numPlayersInArea = 0;

			for (auto& cachePair : m_mapAreaCache)
			{
				SAreasCache& areaCache(cachePair.second);
				SAreaCacheEntry* pAreaCacheEntry = nullptr;

				if (areaCache.GetCacheEntry(pArea, &pAreaCacheEntry) && (pAreaCacheEntry->bInside || pAreaCacheEntry->bNear))
				{
					++numPlayersInArea;
				}
			}

			return numPlayersInArea;
		}
	}

	// If we get here then we've failed to find the area - should never happen
	return 0;
}

//////////////////////////////////////////////////////////////////////////
size_t CAreaManager::GetOverlappingAreas(const AABB& bb, PodArray<IArea*>& list) const
{
	CArea::TAreaBoxes const& boxes = CArea::GetBoxHolders();

	for (CArea::SBoxHolder const& holder : boxes)
	{
		if (!holder.box.Overlaps2D(bb))
			continue;
		list.push_back(holder.area);
	}

	return list.Size();
}

//////////////////////////////////////////////////////////////////////////
bool CAreaManager::SIsNotInGrid::operator()(SAreaCacheEntry const& cacheEntry) const
{
	bool bResult = false;

#if defined(DEBUG_AREAMANAGER)
	if (!stl::find(rapAreas, cacheEntry.pArea))
	{
		CryFatalError("<AreaManager>: area not found in overall areas list!");
	}
#endif // DEBUG_AREAMANAGER

	if (!cacheEntry.bInGrid)
	{
		cacheEntry.pArea->OnRemovedFromAreaCache(entityId);
		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
template<typename K, typename V>
bool CAreaManager::SRemoveIfNoAreasLeft::operator()(std::pair<K, V>& cacheEntry) const
{
	bool bResult = false;
	TAreaCacheVector::iterator Iter(cacheEntry.second.entries.begin());
	TAreaCacheVector::const_iterator const IterEnd(cacheEntry.second.entries.end());

	for (; Iter != IterEnd; ++Iter)
	{
		SAreaCacheEntry const& areaCacheEntry = (*Iter);

#if defined(DEBUG_AREAMANAGER)
		if (!stl::find(areas, areaCacheEntry.pArea))
		{
			CryFatalError("<AreaManager>: area not found in overall areas list!");
		}
#endif // DEBUG_AREAMANAGER

		if (areaCacheEntry.pArea == pArea)
		{
			cacheEntry.second.entries.erase(Iter);
			bResult = cacheEntry.second.entries.empty();

			break;
		}
	}

	return bResult;
}
//////////////////////////////////////////////////////////////////////////
void CAreaManager::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("CAreaManager::OnSystemEvent ESYSTEM_EVENT_LEVEL_LOAD_END");
			for (auto const pArea : m_areas)
			{
				pArea->ResolveEntityIds();
			}
		}
		break;
	}
}
