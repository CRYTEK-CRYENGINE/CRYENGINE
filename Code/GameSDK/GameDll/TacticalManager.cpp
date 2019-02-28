// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "TacticalManager.h"

#include "GameCVars.h"
#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"
#include "ActorDefinitions.h"

static const int MAX_TACTICAL_ENTITIES = 256;

//////////////////////////////////////////////////////////////////////////

void CTacticalManager::STacticalInterestPoint::Reset()
{
	m_scanned = eScanned_None;
	m_overrideIconType = eIconType_NumIcons;
	m_tagged = false;
	m_visible = false;
	m_pinged = false;
}

//////////////////////////////////////////////////////////////////////////

CTacticalManager::CTacticalManager()
{
	for (int i = 0; i < eTacticalEntity_Last; i++)
		m_allTacticalPoints.push_back(TInterestPoints());
}



CTacticalManager::~CTacticalManager()
{
}


void CTacticalManager::Init()
{
	for (int i = 0; i < eTacticalEntity_Last; i++)
		m_allTacticalPoints[i].reserve(MAX_TACTICAL_ENTITIES);
}

void CTacticalManager::Reset()
{
	for (int i = 0; i < eTacticalEntity_Last; i++)
		stl::free_container(m_allTacticalPoints[i]);

	
	ResetClassScanningData();
}

void CTacticalManager::STacticalInterestPoint::Serialize(TSerialize ser)
{
	ser.Value("m_entityId", m_entityId);
	ser.Value("m_scanned", m_scanned);
	ser.Value("m_overrideIconType", m_overrideIconType);
	ser.Value("m_tagged", m_tagged);
	ser.Value("m_visible", m_visible);
	ser.Value("m_pinged", m_pinged);
}


void CTacticalManager::Serialize(TSerialize ser)
{
	if (ser.IsReading())
	{
		// Serialize tactical points
		ClearAllTacticalPoints();

		uint32 numTacInfoPointsGroups = 0;
		STacticalInterestPoint interestPoint;
		ser.Value("numTacInfoPointsGroups", numTacInfoPointsGroups);
		for (uint32 i = 0; i < numTacInfoPointsGroups; i++) // Reads in according to ETacticalEntityType
		{
			ser.BeginGroup("TacInfoPointsGroup");
			uint32 numTacInfoPoints = 0;
			ser.Value("numTacInfoPoints", numTacInfoPoints);
			if (numTacInfoPoints > 0)
			{
				// Go through all the tac points
				for (size_t j = 0; j < numTacInfoPoints; j++)
				{
					ser.BeginGroup("TacInfoPoint");
					interestPoint.Serialize(ser);
					ser.EndGroup();
					AddTacticalInfoPointData((ETacticalEntityType)i, interestPoint);
				}
			}

			ser.EndGroup();
		}

		// Serialize tac override entities
		m_tacEntityToOverrideEntities.clear();
		uint32 numTacOverrideData = 0;
		ser.Value("numTacOverrideData", numTacOverrideData);
		for (uint32 i = 0; i < numTacOverrideData; i++)
		{
			ser.BeginGroup("TacOverrideData");
			EntityId origEntity = 0;
			EntityId overrideEntity = 0;
			ser.Value("origEntity", origEntity);
			ser.Value("overrideEntity", overrideEntity);
			AddOverrideEntity(origEntity, overrideEntity);

			ser.EndGroup();
		}		

		// Serialize class scanned data
		m_classes.clear();
		uint32 numInterestClasses = 0;
		string interestClassName;
		ser.Value("numInterestClasses", numInterestClasses);
		for (uint32 i = 0; i < numInterestClasses; i++)
		{
			TScanningCount scanningCount;
			ser.BeginGroup("InterestClassData");
			ser.Value("name", interestClassName);
			ser.Value("scanningCount", scanningCount);

			IEntityClassRegistry* pEntityClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
			CRY_ASSERT(pEntityClassRegistry != NULL);
			IEntityClass* pEntityClass = pEntityClassRegistry->FindClass(interestClassName);
			CRY_ASSERT(pEntityClass);
			if (pEntityClass)
			{
				m_classes[pEntityClass] = scanningCount;
			}

			ser.EndGroup();
		}
	}
	else
	{
		// Serialize tactical points
		uint32 numTacInfoPointsGroups = eTacticalEntity_Last;
		ser.Value("numTacInfoPointsGroups", numTacInfoPointsGroups);
		for (uint32 i = 0; i < numTacInfoPointsGroups; i++) // Writes in according to ETacticalEntityType
		{
			ser.BeginGroup("TacInfoPointsGroup");
			TInterestPoints& interestPoints = m_allTacticalPoints[i];
			uint32 numTacInfoPoints = interestPoints.size();
			ser.Value("numTacInfoPoints", numTacInfoPoints);
			if (numTacInfoPoints > 0)
			{
				// Go through all the tac points
				for (uint32 j = 0; j < numTacInfoPoints; j++)
				{
					ser.BeginGroup("TacInfoPoint");
					STacticalInterestPoint& interestPoint = interestPoints[j];
					interestPoint.Serialize(ser);
					ser.EndGroup();
				}
			}

			ser.EndGroup();
		}

		// Serialize tac override entities
		uint32 numTacOverrideData = m_tacEntityToOverrideEntities.size();
		ser.Value("numTacOverrideData", numTacOverrideData);
		TTacticalEntityToOverrideEntities::iterator tactOverrideDataIter = m_tacEntityToOverrideEntities.begin();
		const TTacticalEntityToOverrideEntities::const_iterator tactOverrideDataIterEnd = m_tacEntityToOverrideEntities.end();
		while (tactOverrideDataIter != tactOverrideDataIterEnd)
		{
			EntityId origEntity = tactOverrideDataIter->first;
			EntityId overrideEntity = tactOverrideDataIter->second;
			ser.BeginGroup("TacOverrideData");
			ser.Value("origEntity", origEntity);
			ser.Value("overrideEntity", overrideEntity);
			ser.EndGroup();
			++tactOverrideDataIter;
		}

		// Serialize class scanned data
		uint32 numInterestClasses = m_classes.size();
		ser.Value("numInterestClasses", numInterestClasses);
		TInterestClasses::iterator interestClassesIter = m_classes.begin();
		const TInterestClasses::const_iterator interestClassesIterEnd = m_classes.end();
		while (interestClassesIter != interestClassesIterEnd)
		{
			const IEntityClass* pEntityClass = interestClassesIter->first;
			CRY_ASSERT(pEntityClass);
			TScanningCount scanningCount = interestClassesIter->second;
			ser.BeginGroup("InterestClassData");
			ser.Value("name", pEntityClass->GetName());
			ser.Value("scanningCount", scanningCount);
			ser.EndGroup();
			++interestClassesIter;
		}
	}
}

void CTacticalManager::PostSerialize()
{
	// Radar needs player client entity to be set otherwise edge entity items won't show
	// ENTITY_EVENT_UNHIDE isn't called when loading, since state is already unhided, that usually triggers CActor::ProcessEvent with event type ENTITY_EVENT_UNHIDE
	// Call the hud event here instead
	const EntityId playerEntityID = gEnv->pGameFramework->GetClientActorId();
	AddEntity(playerEntityID, CTacticalManager::eTacticalEntity_Unit);
	SHUDEvent hudevent(eHUDEvent_AddEntity);
	hudevent.AddData(SHUDEventData((int)playerEntityID));
	CHUDEventDispatcher::CallEvent(hudevent);
}

void CTacticalManager::AddEntity(const EntityId id, ETacticalEntityType type)
{
	CRY_ASSERT(m_allTacticalPoints.size() == eTacticalEntity_Last); // Will always be the case
	CRY_ASSERT(type < eTacticalEntity_Last);
	if (type == eTacticalEntity_Last)
		return;

	TInterestPoints& points = m_allTacticalPoints[type];

	TInterestPoints::const_iterator it = points.begin();
	TInterestPoints::const_iterator end = points.end();
	for(; it!=end; ++it)
	{
		if(it->m_entityId == id)
		{
			return;
		}
	}
	points.push_back(STacticalInterestPoint(id));

	if(!gEnv->bMultiplayer)
	{
		if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
		{
			SetClassScanned(pEntity->GetClass(), eScanned_None);
		}
	}
}



void CTacticalManager::RemoveEntity(const EntityId id, ETacticalEntityType type)
{
	CRY_ASSERT(m_allTacticalPoints.size() == eTacticalEntity_Last); // Will always be the case
	CRY_ASSERT(type < eTacticalEntity_Last);
	if (type == eTacticalEntity_Last)
		return;

	TInterestPoints& points = m_allTacticalPoints[type];
	TInterestPoints::iterator it = points.begin();
	TInterestPoints::const_iterator end = points.end();
	for(; it!=end; ++it)
	{
		if(it->m_entityId == id)
		{
			//Removes the silhouette in case it's an actor
			SHUDEvent event(eEHUDEvent_OnScannedEnemyRemove);
			event.AddData(SHUDEventData((int)id));
			CHUDEventDispatcher::CallEvent(event);
			points.erase(it);

			return;
		}
	}
}



void CTacticalManager::SetEntityScanned(const EntityId id, ETacticalEntityType type)
{
	CRY_ASSERT(m_allTacticalPoints.size() == eTacticalEntity_Last); // Will always be the case
	CRY_ASSERT(type < eTacticalEntity_Last);
	if (type == eTacticalEntity_Last)
		return;

	TInterestPoints& points = m_allTacticalPoints[type];
	TInterestPoints::iterator it = points.begin();
	TInterestPoints::const_iterator end = points.end();
	for(; it!=end; ++it)
	{
		if(it->m_entityId == id)
		{
			TScanningCount scanned = min((int)eScanned_Max, it->m_scanned + 1);

			it->m_scanned = scanned;
			if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
			{
				SetClassScanned(pEntity->GetClass(), scanned);
			}
			return;
		}
	}
}



void CTacticalManager::SetEntityScanned(const EntityId id)
{
	for(int i=0; i<eTacticalEntity_Last; ++i)
	{
		SetEntityScanned(id, (ETacticalEntityType)i);
	}
}



CTacticalManager::TScanningCount CTacticalManager::GetEntityScanned(const EntityId id) const
{
	TScanningCount scanningCount = eScanned_None;
	bool bKeepSearching = true;

	TAllTacticalPoints::const_iterator iter = m_allTacticalPoints.begin();
	const TAllTacticalPoints::const_iterator iterEnd = m_allTacticalPoints.end();

	while (iter != iterEnd && bKeepSearching)
	{
		const CTacticalManager::TInterestPoints& interestPoints = *iter;

		const size_t numInterestPoints = interestPoints.size();
		for (size_t i = 0; i < numInterestPoints; i++)
		{
			const STacticalInterestPoint& interestPoint = interestPoints[i];

			const EntityId entityId = interestPoint.m_entityId;
			if (entityId == id)
			{
				scanningCount = interestPoint.m_scanned;
				bKeepSearching = false;
				break;
			}
		}

		++iter;
	}

	return scanningCount;
}



void CTacticalManager::SetEntityTagged(const EntityId id, const bool bTagged)
{
	for(int i=0; i<eTacticalEntity_Last; ++i)
	{
		SetEntityTagged(id, (ETacticalEntityType)i, bTagged);
	}
}



void CTacticalManager::SetEntityTagged(const EntityId id, ETacticalEntityType type, const bool bTagged)
{
	CRY_ASSERT(m_allTacticalPoints.size() == eTacticalEntity_Last); // Will always be the case
	CRY_ASSERT(type < eTacticalEntity_Last);
	if (type == eTacticalEntity_Last)
		return;

	TInterestPoints& points = m_allTacticalPoints[type];
	TInterestPoints::iterator it = points.begin();
	TInterestPoints::const_iterator end = points.end();
	for(; it!=end; ++it)
	{
		if(it->m_entityId == id)
		{
			it->m_tagged = bTagged;
			return;
		}
	}
}



bool CTacticalManager::IsEntityTagged(const EntityId id) const
{
	bool bTagged = false;
	bool bKeepSearching = true;

	TAllTacticalPoints::const_iterator iter = m_allTacticalPoints.begin();
	const TAllTacticalPoints::const_iterator iterEnd = m_allTacticalPoints.end();

	while (iter != iterEnd && bKeepSearching)
	{
		const CTacticalManager::TInterestPoints& interestPoints = *iter;

		const size_t numInterestPoints = interestPoints.size();
		for (size_t i = 0; i < numInterestPoints; i++)
		{
			const STacticalInterestPoint& interestPoint = interestPoints[i];

			const EntityId entityId = interestPoint.m_entityId;
			if (entityId == id)
			{
				bTagged = interestPoint.m_tagged;
				bKeepSearching = false;
				break;
			}
		}

		++iter;
	}

	return bTagged;
}



void CTacticalManager::SetClassScanned(IEntityClass* pClass, const TScanningCount scanned)
{
	TInterestClasses::iterator it = m_classes.begin();
	TInterestClasses::iterator end = m_classes.end();
	for(; it!=end; ++it)
	{
		if(it->first == pClass)
		{
			if(it->second < scanned)
			{
				it->second = scanned;
			}
			return;
		}
	}
	m_classes[pClass] = scanned;
}



CTacticalManager::TScanningCount CTacticalManager::GetClassScanned(IEntityClass* pClass) const
{
	TInterestClasses::const_iterator it = m_classes.begin();
	TInterestClasses::const_iterator end = m_classes.end();
	for(; it!=end; ++it)
	{
		if(it->first == pClass)
		{
			return it->second;
		}
	}
	return eScanned_None;
}



void CTacticalManager::ClearAllTacticalPoints()
{
	// Go through all types of tactical points
	CTacticalManager::TAllTacticalPoints::iterator allPointsIt = m_allTacticalPoints.begin();
	CTacticalManager::TAllTacticalPoints::iterator allPointsItEnd = m_allTacticalPoints.end();
	for(; allPointsIt!=allPointsItEnd; ++allPointsIt)
	{
		CTacticalManager::TInterestPoints &points = (*allPointsIt); // Category of points
		points.clear();
	}	
}



void CTacticalManager::ResetAllTacticalPoints()
{
	// Go through all types of tactical points
	CTacticalManager::TAllTacticalPoints::iterator allPointsIt = m_allTacticalPoints.begin();
	CTacticalManager::TAllTacticalPoints::iterator allPointsItEnd = m_allTacticalPoints.end();
	TInterestPoints::iterator interestPointsIt;
	for(; allPointsIt!=allPointsItEnd; ++allPointsIt)
	{
		CTacticalManager::TInterestPoints &points = (*allPointsIt); // Category of points

		for(interestPointsIt = points.begin(); interestPointsIt!=points.end(); ++interestPointsIt)
		{
			STacticalInterestPoint &entry = (*interestPointsIt);
			entry.Reset();
		}	
	}	
}



void CTacticalManager::ResetClassScanningData()
{
	stl::free_container(m_classes);
}



CTacticalManager::TInterestPoints& CTacticalManager::GetTacticalPoints(const ETacticalEntityType type)
{
	CRY_ASSERT(m_allTacticalPoints.size() == eTacticalEntity_Last); // Will always be the case
	CRY_ASSERT(type < eTacticalEntity_Last); // Shouldn't happen
	if (type == eTacticalEntity_Last)
		return m_allTacticalPoints[eTacticalEntity_Custom];

	return m_allTacticalPoints[type];
}



const CTacticalManager::TInterestPoints& CTacticalManager::GetTacticalPoints(const ETacticalEntityType type) const
{
	CRY_ASSERT(m_allTacticalPoints.size() == eTacticalEntity_Last); // Will always be the case
	CRY_ASSERT(type < eTacticalEntity_Last); // Shouldn't happen
	if (type == eTacticalEntity_Last)
		return m_allTacticalPoints[eTacticalEntity_Custom];

	return m_allTacticalPoints[type];
}



void CTacticalManager::Ping(const float fPingDistance)
{
	Ping(eTacticalEntity_Custom, fPingDistance);
}



void CTacticalManager::Ping(const ETacticalEntityType type, const float fPingDistance)
{
	const Vec3& clientPos = CHUDUtils::GetClientPos();
	AABB entityBox;

	const float pingDistanceSq = fPingDistance*fPingDistance;

	TInterestPoints& points = GetTacticalPoints(type);

	TInterestPoints::iterator pointsIt = points.begin();
	TInterestPoints::iterator pointsItEnd = points.end();

	for(; pointsIt!=pointsItEnd; ++pointsIt)
	{
		STacticalInterestPoint &entry = (*pointsIt);

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entry.m_entityId);
		if (!pEntity)
			continue;

		if(pEntity->IsHidden())
			continue;

		pEntity->GetWorldBounds(entityBox);

		if(clientPos.GetSquaredDistance(entityBox.GetCenter()) > pingDistanceSq) // Outside ping range
			continue;

		if (entry.m_pinged != true)
		{
			entry.m_pinged = true;
		}
	}
}



void CTacticalManager::AddOverrideEntity(const EntityId tacticalEntity, const EntityId overrideEntity)
{
	CRY_ASSERT(tacticalEntity != 0);
	CRY_ASSERT(overrideEntity != 0);

	TTacticalEntityToOverrideEntities::iterator iter = m_tacEntityToOverrideEntities.find(tacticalEntity);
	if (iter == m_tacEntityToOverrideEntities.end())
	{
		m_tacEntityToOverrideEntities[tacticalEntity] = overrideEntity;
	}
	else
	{
		GameWarning("CTacticalManager::AddOverrideEntity: Entity: %u is the parent of more than one TacticalOverride entity, ignoring entity: %u", tacticalEntity, overrideEntity);
	}
}



void CTacticalManager::RemoveOverrideEntity(const EntityId tacticalEntity, const EntityId overrideEntity)
{
	TTacticalEntityToOverrideEntities::iterator iter = m_tacEntityToOverrideEntities.find(tacticalEntity);
	if (iter != m_tacEntityToOverrideEntities.end())
	{
		const EntityId entityId = iter->second;
		CRY_ASSERT(entityId == overrideEntity);
		m_tacEntityToOverrideEntities.erase(iter);
	}
	else
	{
		GameWarning("CTacticalManager::RemoveOverrideEntity: Tried to remove override entity but wasn't added");
	}
}



void CTacticalManager::RemoveOverrideEntity(const EntityId overrideEntity)
{
	TTacticalEntityToOverrideEntities::iterator iter = m_tacEntityToOverrideEntities.begin();
	const TTacticalEntityToOverrideEntities::const_iterator iterEnd = m_tacEntityToOverrideEntities.end();
	while (iter != iterEnd)
	{
		const EntityId curOverrideEntityId = iter->second;
		if (curOverrideEntityId == overrideEntity)
		{
			TTacticalEntityToOverrideEntities::iterator deleteIter = iter;
			++iter;
			m_tacEntityToOverrideEntities.erase(deleteIter);
		}
		else
		{
			++iter;
		}
	}
}



EntityId CTacticalManager::GetOverrideEntity(const EntityId tacticalEntity) const
{
	TTacticalEntityToOverrideEntities::const_iterator iter = m_tacEntityToOverrideEntities.find(tacticalEntity);
	if (iter == m_tacEntityToOverrideEntities.end())
	{
		return (EntityId)0;
	}
	else
	{
		return iter->second;
	}
}



const Vec3& CTacticalManager::GetTacticalIconWorldPos(const EntityId tacticalEntityId, IEntity* pTacticalEntity, bool& inOutIsHeadBone)
{
	CRY_ASSERT(tacticalEntityId != 0);
	CRY_ASSERT(pTacticalEntity != NULL);
	if (pTacticalEntity == NULL)
	{
		return m_tempVec3.Set(0.0f,0.0f,0.0f);
	}

	// Try to get pos from headbone if don't have an override pos
	TTacticalEntityToOverrideEntities::const_iterator iter = m_tacEntityToOverrideEntities.find(tacticalEntityId);
	if (iter == m_tacEntityToOverrideEntities.end()) 
	{
		static IEntityClass* s_pTurretEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Turret");

		IEntityClass* pEntityClass = pTacticalEntity->GetClass();
		if (pEntityClass != NULL && pEntityClass == s_pTurretEntityClass)
		{
			ICharacterInstance* pCharacterInstance = pTacticalEntity->GetCharacter(0);
			if (pCharacterInstance != NULL)
			{
				IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
				ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
				CRY_ASSERT(pSkeletonPose != NULL);

				const int16 jointId = rIDefaultSkeleton.GetJointIDByName("arcjoint");
				if (0 <= jointId)
				{
					QuatT boneLocation = pSkeletonPose->GetAbsJointByID(jointId);
					m_tempVec3 = pTacticalEntity->GetWorldTM().TransformPoint(boneLocation.t);
					return m_tempVec3;
				}
			}
		}


		CUICVars* pCVars = g_pGame->GetUI()->GetCVars();
		if (pCVars->hud_InterestPointsAtActorsHeads == 1)
		{
			CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(tacticalEntityId)); // Only want units to go from headbone, since vehicles have headbone as well
			if(pActor)
			{
				if(pActor->HasBoneID(BONE_HEAD))
				{
					QuatT boneLocation = pActor->GetBoneTransform(BONE_HEAD);
					m_tempVec3 = pTacticalEntity->GetWorldTM().TransformPoint(boneLocation.t) + Vec3(0.0f, 0.0f, 0.25f);
					inOutIsHeadBone = true;
					return m_tempVec3;
				}
				else if(pActor->HasBoneID(BONE_SPINE))
				{
					QuatT boneLocation = pActor->GetBoneTransform(BONE_SPINE);
					m_tempVec3 = pTacticalEntity->GetWorldTM().TransformPoint(boneLocation.t);
					return m_tempVec3;
				}
			}
		}

		return GetTacticalIconCenterBBoxWorldPos(pTacticalEntity);
	}
	else // Override entity exists so it determines position
	{
		IEntity* pOverrideEntity = gEnv->pEntitySystem->GetEntity(iter->second);
		if (pOverrideEntity)
		{
			m_tempVec3 = pOverrideEntity->GetWorldPos();
			return m_tempVec3;
		}
		else
		{
			GameWarning("CTacticalManager::GetTacticalIconWorldPos: ID exists in mapping but failed to find entity, defaulting to center bounding box position");
			return GetTacticalIconCenterBBoxWorldPos(pTacticalEntity);
		}
	}
}



void CTacticalManager::SetEntityOverrideIcon(const EntityId id, const uint8 overrideIconType)
{
	bool bKeepSearching = true;

	TAllTacticalPoints::iterator iter = m_allTacticalPoints.begin();
	const TAllTacticalPoints::const_iterator iterEnd = m_allTacticalPoints.end();

	while (iter != iterEnd && bKeepSearching)
	{
		CTacticalManager::TInterestPoints& interestPoints = *iter;
		const size_t numInterestPoints = interestPoints.size();
		for (size_t i = 0; i < numInterestPoints; i++)
		{
			STacticalInterestPoint& interestPoint = interestPoints[i];

			const EntityId entityId = interestPoint.m_entityId;
			if (entityId == id)
			{
				interestPoint.m_overrideIconType = overrideIconType;
				bKeepSearching = false;
				break;
			}
		}

		++iter;
	}
}



uint8 CTacticalManager::GetEntityOverrideIcon(const EntityId id) const
{
	uint8 overrideIcon = eIconType_NumIcons;
	bool bKeepSearching = true;

	TAllTacticalPoints::const_iterator iter = m_allTacticalPoints.begin();
	const TAllTacticalPoints::const_iterator iterEnd = m_allTacticalPoints.end();

	while (iter != iterEnd && bKeepSearching)
	{
		const CTacticalManager::TInterestPoints& interestPoints = *iter;

		const size_t numInterestPoints = interestPoints.size();
		for (size_t i = 0; i < numInterestPoints; i++)
		{
			const STacticalInterestPoint& interestPoint = interestPoints[i];

			const EntityId entityId = interestPoint.m_entityId;
			if (entityId == id)
			{
				overrideIcon = interestPoint.m_overrideIconType;
				bKeepSearching = false;
				break;
			}
		}

		++iter;
	}

	return overrideIcon;
}



uint8 CTacticalManager::GetOverrideIconType(const char* szOverrideIconName) const
{
	return eIconType_NumIcons;
}



const Vec3&	CTacticalManager::GetTacticalIconCenterBBoxWorldPos(IEntity* pTacticalEntity)
{
	CRY_ASSERT(pTacticalEntity != NULL);

	AABB box;
	pTacticalEntity->GetWorldBounds(box);
	m_tempVec3 = box.GetCenter();

	return m_tempVec3;
}



void CTacticalManager::AddTacticalInfoPointData(const ETacticalEntityType type, const STacticalInterestPoint& point)
{
	CRY_ASSERT(m_allTacticalPoints.size() == eTacticalEntity_Last); // Will always be the case
	CRY_ASSERT(type < eTacticalEntity_Last);
	if (type == eTacticalEntity_Last)
		return;

	const EntityId entityId = point.m_entityId;
	TInterestPoints& points = m_allTacticalPoints[type];

	TInterestPoints::const_iterator it = points.begin();
	TInterestPoints::const_iterator end = points.end();
	for(; it!=end; ++it)
	{
		if(it->m_entityId == entityId)
		{
			return;
		}
	}
	points.push_back(point);
}

//////////////////////////////////////////////////////////////////////////
