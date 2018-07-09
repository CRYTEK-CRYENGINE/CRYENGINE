// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Interactor.h"
#include "CryAction.h"
#include "IGameRulesSystem.h"
#include "WorldQuery.h"
#include "IActorSystem.h"
#include "CryActionCVars.h"

CInteractor::CInteractor()
{
	m_pQuery = 0;
	m_pGameRules = 0;

	m_nextOverId = m_overId = 0;
	m_nextOverIdx = m_overIdx = -100;
	m_nextOverTime = m_overTime = 0.0f;
	m_sentLongHover = m_sentMessageHover = false;

	m_funcIsUsable = m_funcAreUsable = m_funcOnNewUsable = m_funcOnUsableMessage = m_funcOnLongHover = 0;

	m_pTimer = gEnv->pTimer;
	m_pEntitySystem = gEnv->pEntitySystem;

	m_useHoverTime = 0.05f;
	m_unUseHoverTime = 0.2f;
	m_messageHoverTime = 0.05f;
	m_longHoverTime = 5.0f;

	m_lockedByEntityId = m_lockEntityId = 0;
	m_lockIdx = 0;

	m_lastRadius = 2.0f;

}

bool CInteractor::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);
	if (!m_pQuery)
	{
		m_pQuery = (CWorldQuery*) GetGameObject()->AcquireExtension("WorldQuery");
		if (!m_pQuery)
			return false;
	}

	m_pQuery->SetProximityRadius(CCryActionCVars::Get().playerInteractorRadius);

	m_pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity()->GetScriptTable();
	m_pGameRules->GetValue("IsUsable", m_funcIsUsable);
	m_pGameRules->GetValue("AreUsable", m_funcAreUsable);
	m_pGameRules->GetValue("OnNewUsable", m_funcOnNewUsable);
	m_pGameRules->GetValue("OnUsableMessage", m_funcOnUsableMessage);
	m_pGameRules->GetValue("OnLongHover", m_funcOnLongHover);

	m_areUsableEntityList.Create(gEnv->pScriptSystem);

	//Should be more than enough for average situation
	m_frameQueryVector.reserve(16);

	m_queryMethods = (m_funcAreUsable) ? "m" : "rb";  //m is optimized for Crysis2, "rb" is Crysis1 compatible

	return true;
}

void CInteractor::PostInit(IGameObject* pGameObject)
{
	pGameObject->EnableUpdateSlot(this, 0);
}

bool CInteractor::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CInteractor::ReloadExtension not implemented");

	return false;
}

CInteractor::~CInteractor()
{
	if (m_pQuery)
		GetGameObject()->ReleaseExtension("WorldQuery");
	gEnv->pScriptSystem->ReleaseFunc(m_funcIsUsable);
	gEnv->pScriptSystem->ReleaseFunc(m_funcAreUsable);
	gEnv->pScriptSystem->ReleaseFunc(m_funcOnNewUsable);
	gEnv->pScriptSystem->ReleaseFunc(m_funcOnUsableMessage);
	gEnv->pScriptSystem->ReleaseFunc(m_funcOnLongHover);
}

void CInteractor::Release()
{
	delete this;
}

ScriptAnyValue CInteractor::EntityIdToScript(EntityId id)
{
	if (id)
	{
		ScriptHandle hdl;
		hdl.n = id;
		return ScriptAnyValue(hdl);
	}
	else
	{
		return ScriptAnyValue();
	}
}

void CInteractor::Update(SEntityUpdateContext&, int)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	EntityId newOverId = INVALID_ENTITYID;
	int usableIdx = 0;

	const EntityId myId = GetGameObject()->GetEntityId();
	const IActor* const pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(myId);
	if (pActor && !pActor->IsInteracting())
	{
		if (m_lockedByEntityId)
		{
			newOverId = m_lockEntityId;
			usableIdx = m_lockIdx;
		}
		else
		{
			if ((CCryActionCVars::Get().playerInteractorRadius != m_lastRadius) && m_pQuery)
			{
				m_lastRadius = CCryActionCVars::Get().playerInteractorRadius;
				m_pQuery->SetProximityRadius(m_lastRadius);
			}

			PerformQueries(newOverId, usableIdx);
		}
	}
	UpdateTimers(newOverId, usableIdx);
}

void CInteractor::PerformQueries(EntityId& id, int& idx)
{
	SQueryResult bestResult;
	bestResult.entityId = 0;
	bestResult.slotIdx = 0;
	bestResult.hitDistance = FLT_MAX;

	for (string::const_iterator iter = m_queryMethods.begin(); iter != m_queryMethods.end(); ++iter)
	{
		bool found;
		SQueryResult result;
		switch (*iter)
		{
		case 'r':
			found = PerformRayCastQuery(result);
			break;
		case 'b':
			found = PerformViewCenterQuery(result);
			break;
		case 'd':
			found = PerformDotFilteredProximityQuery(result, 0.8f);
			break;
		case 'm':
			found = PerformMergedQuery(result, 0.8f);
			break;
		default:
			GameWarning("Interactor: Invalid query mode '%c'", *iter);
			found = false;
		}
		if (found && result.hitDistance < bestResult.hitDistance)
		{
			if (bestResult.entityId && !result.entityId)
				;
			else
				bestResult = result;
		}
	}
	id = bestResult.entityId;
	idx = bestResult.slotIdx;
}

bool CInteractor::PerformDotFilteredProximityQuery(SQueryResult& r, float minDot)
{
	float minDistanceToCenterSq(999.9f);
	IEntity* pNearestEntityToViewCenter(NULL);
	float maxDot = -1.0f;

	Vec3 viewDirection = m_pQuery->GetDir();
	viewDirection.Normalize();

	Vec3 viewPos = m_pQuery->GetPos();

	const Entities& entities = m_pQuery->ProximityQuery();
	for (Entities::const_iterator i = entities.begin(); i != entities.end(); ++i)
	{
		IEntity* pEntity = m_pEntitySystem->GetEntity(*i);
		if (!pEntity)
			continue;

		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		Vec3 itemPos = bbox.GetCenter();

		Vec3 diff = itemPos - viewPos;

		Vec3 dirToItem = diff.GetNormalized();

		float dotToItem = dirToItem.dot(viewDirection);

		if (dotToItem > maxDot)
		{
			maxDot = dotToItem;
			pNearestEntityToViewCenter = pEntity;
			r.hitPosition = itemPos;
			r.hitDistance = (itemPos - viewPos).GetLengthSquared();
		}
	}

	if (pNearestEntityToViewCenter && maxDot > minDot)
	{
		r.hitDistance = sqrt_tpl(r.hitDistance);
		PerformUsableTestAndCompleteIds(pNearestEntityToViewCenter, r);
		return true;
	}
	else
	{
		return false;
	}
}

bool CInteractor::PerformRayCastQuery(SQueryResult& r)
{
	if (const ray_hit* pHit = m_pQuery->RaycastQuery())
	{
		EntityId targetId = m_pQuery->GetLookAtEntityId();
		IEntity* pTargetEntity = m_pEntitySystem->GetEntity(targetId);
		if (pTargetEntity)
		{
			r.hitPosition = pHit->pt;
			r.hitDistance = pHit->dist;
			return PerformUsableTestAndCompleteIds(pTargetEntity, r);
		}
	}
	return false;
}

bool CInteractor::PerformViewCenterQuery(SQueryResult& r)
{

	float minDistanceToCenterSq(999.9f);
	IEntity* pNearestEntityToViewCenter(NULL);

	Vec3 viewDirection = m_pQuery->GetDir();
	viewDirection.Normalize();
	viewDirection *= m_lastRadius;
	Vec3 viewPos = m_pQuery->GetPos();

	Line viewLine(viewPos, viewDirection);

	const Entities& entities = m_pQuery->GetEntitiesInFrontOf();
	for (Entities::const_iterator i = entities.begin(); i != entities.end(); ++i)
	{

		IEntity* pEntity = m_pEntitySystem->GetEntity(*i);
		if (!pEntity)
			continue;

		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		Vec3 itemPos = bbox.GetCenter();

		float dstSqr = LinePointDistanceSqr(viewLine, itemPos);
		if ((dstSqr < minDistanceToCenterSq))
		{
			r.hitPosition = itemPos;
			r.hitDistance = (itemPos - viewPos).GetLengthSquared();
			minDistanceToCenterSq = dstSqr;
			pNearestEntityToViewCenter = pEntity;
		}

	}

	if (pNearestEntityToViewCenter && PerformUsableTestAndCompleteIds(pNearestEntityToViewCenter, r))
	{
		return true;
	}

	return false;
}

bool CInteractor::PerformMergedQuery(SQueryResult& r, float minDot)
{
	//Clear before re-using it
	m_frameQueryVector.clear();

	//Check if owner can interact first
	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntityId());
	bool canUserInteract = pActor ? (!pActor->IsDead()) && (pActor->GetSpectatorMode() == 0) : false;

	if (!canUserInteract)
		return false;

	//1. - Raycast query
	EntityId lookAtEntityId = 0;
	if (const ray_hit* pHit = m_pQuery->RaycastQuery())
	{
		if (IEntity* pTargetEntity = m_pEntitySystem->GetEntity(m_pQuery->GetLookAtEntityId()))
		{
			if (IsEntityUsable(pTargetEntity))
			{
				SQueryResult rayCastResult;
				rayCastResult.entityId = pTargetEntity->GetId();
				rayCastResult.hitDistance = pHit->dist;
				rayCastResult.hitPosition = pHit->pt;
				m_frameQueryVector.push_back(TQueryElement(pTargetEntity, rayCastResult));

				lookAtEntityId = rayCastResult.entityId;
			}
		}
	}

	//2. - Proximity query
	SQueryResult tempQuery;
	const Entities& entities = m_pQuery->ProximityQuery();
	for (Entities::const_iterator entityCit = entities.begin(); entityCit != entities.end(); ++entityCit)
	{
		const EntityId entityId = *entityCit;
		if (IEntity* pEntity = m_pEntitySystem->GetEntity(entityId))
		{
			if (IsEntityUsable(pEntity))
			{
				tempQuery.entityId = entityId;
				m_frameQueryVector.push_back(TQueryElement(pEntity, tempQuery));
			}
		}
	}

	//3. - Perform usable test on all entities at once
	PerformUsableTestOnEntities(m_frameQueryVector);

	//4. - Pick the best result
	const int itemReturnIdx = 725725;

	float minDistanceToCenterSqr(FLT_MAX);
	float bestDotResult = minDot;

	IEntity* pNearestDotEntity(NULL);
	IEntity* pNearestBoxEntity(NULL);
	SQueryResult bestBoxResult;

	const Vec3 viewDirection = m_pQuery->GetDir() * m_lastRadius;
	const Vec3 viewDirectionNormalized = viewDirection.GetNormalized();
	const Vec3 viewPos = m_pQuery->GetPos();

	for (TQueryVector::iterator queryIt = m_frameQueryVector.begin(); queryIt != m_frameQueryVector.end(); ++queryIt)
	{
		IEntity* pEntity = queryIt->first;
		SQueryResult& currentElement = queryIt->second;
		if (currentElement.slotIdx)
		{
			//If it is the ray-cast one, early out here
			if (currentElement.entityId == lookAtEntityId)
			{
				r = currentElement;
				return true;
			}

			AABB bbox;
			pEntity->GetWorldBounds(bbox);

			const Vec3 entityCenterPos = !bbox.IsEmpty() ? bbox.GetCenter() : pEntity->GetWorldPos();

			const Vec3 dirToEntity = (entityCenterPos - viewPos).GetNormalizedSafe();
			const Line viewLine(viewPos, viewDirection);
			const float linePointDistanceSqr = LinePointDistanceSqr(viewLine, entityCenterPos);

			if (linePointDistanceSqr >= minDistanceToCenterSqr)
				continue;

			const float dotToEntity = viewDirectionNormalized.Dot(dirToEntity);

			// TODO: Improve this simple way of detecting if the entity is an item
			bool isPickableItem = (currentElement.slotIdx == itemReturnIdx);

			if (dotToEntity > bestDotResult)
			{
				bestDotResult = dotToEntity;
				r.entityId = currentElement.entityId;
				r.slotIdx = currentElement.slotIdx;
				r.hitPosition = entityCenterPos;
				r.hitDistance = (entityCenterPos - viewPos).GetLength();
				minDistanceToCenterSqr = linePointDistanceSqr;
				pNearestDotEntity = pEntity;
			}
			else if (pNearestDotEntity == NULL && !bbox.IsEmpty())
			{

				//If fails, bbox overlap
				const Lineseg viewLineSeg(viewPos, viewPos + viewDirection);
				if (Overlap::Lineseg_AABB(viewLineSeg, bbox))
				{
					bestBoxResult.entityId = currentElement.entityId;
					bestBoxResult.slotIdx = currentElement.slotIdx;
					bestBoxResult.hitPosition = entityCenterPos;
					bestBoxResult.hitDistance = (entityCenterPos - viewPos).GetLength();
					minDistanceToCenterSqr = linePointDistanceSqr;
					pNearestBoxEntity = pEntity;
				}
				else if (isPickableItem)
				{
					bestBoxResult.entityId = currentElement.entityId;
					bestBoxResult.slotIdx = currentElement.slotIdx;
					bestBoxResult.hitPosition = entityCenterPos;
					bestBoxResult.hitDistance = (entityCenterPos - viewPos).GetLength();
					minDistanceToCenterSqr = linePointDistanceSqr;
					pNearestBoxEntity = pEntity;
				}
			}
		}
	}

	if (pNearestBoxEntity || pNearestDotEntity)
	{
		//	CryLog ("[INTERACTION] Dot winner = '%s', box winner = '%s'!", pNearestDotEntity ? pNearestDotEntity->GetClass()->GetName() : "N/A", pNearestBoxEntity ? pNearestBoxEntity->GetClass()->GetName() : "N/A");

		if (pNearestDotEntity == NULL)
		{
			pNearestDotEntity = pNearestBoxEntity;
			r = bestBoxResult;
		}

		return true;
	}

	return false;
}

float CInteractor::LinePointDistanceSqr(const Line& line, const Vec3& point)
{
	Vec3 x0 = point;
	Vec3 x1 = line.pointonline;
	Vec3 x2 = line.pointonline + line.direction;

	return ((x2 - x1).Cross(x1 - x0)).GetLengthSquared() / (x2 - x1).GetLengthSquared();
}

int CInteractor::PerformUsableTest(IEntity* pEntity) const
{
	if (pEntity && m_funcIsUsable)
	{
		SmartScriptTable pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable.GetPtr())
		{
			int usableIdx;
			bool scriptOk = Script::CallReturn(
			  m_pGameRules->GetScriptSystem(),
			  m_funcIsUsable,
			  m_pGameRules,
			  EntityIdToScript(GetEntityId()),
			  EntityIdToScript(pEntity->GetId()),
			  usableIdx);
			if (scriptOk && usableIdx)
			{
				return usableIdx;
			}
		}
	}
	return 0;
}

void CInteractor::PerformUsableTestOnEntities(TQueryVector& queryList)
{
	if (queryList.empty() || !m_funcAreUsable)
		return;

	//Prepare script parameters

	if (m_areUsableEntityList)
	{
		m_areUsableEntityList->Clear();

		SQueryResult queryResult;
		int tableIndex = 0;

		TQueryVector::const_iterator endCit = queryList.end();
		for (TQueryVector::const_iterator elementCit = queryList.begin(); elementCit != endCit; ++elementCit)
		{
			const TQueryElement& element = *elementCit;

			CRY_ASSERT(element.first);
			CRY_ASSERT(element.first->GetScriptTable());

			m_areUsableEntityList->SetAt(++tableIndex, element.first->GetScriptTable());
		}

		//Call script side
		SmartScriptTable scriptResults;
		bool scriptOk = Script::CallReturn(m_pGameRules->GetScriptSystem(), m_funcAreUsable, m_pGameRules, GetEntity()->GetScriptTable(), m_areUsableEntityList, scriptResults);

		if (scriptOk && scriptResults)
		{
			const int resultCount = scriptResults->Count();
			const int queryCount = queryList.size();

			CRY_ASSERT(queryCount <= resultCount);

			if (queryCount <= resultCount)
			{
				for (int i = 0; i < queryCount; ++i)
				{
					scriptResults->GetAt((i + 1), queryList[i].second.slotIdx);
				}
			}

		}
	}
}

bool CInteractor::PerformUsableTestAndCompleteIds(IEntity* pEntity, SQueryResult& r) const
{
	bool ok = false;
	r.entityId = 0;
	r.slotIdx = 0;
	if (int usableIdx = PerformUsableTest(pEntity))
	{
		r.entityId = pEntity->GetId();
		r.slotIdx = usableIdx;
		ok = true;
	}
	return ok;
}

bool CInteractor::IsUsable(EntityId entityId) const
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);

	static SQueryResult dummy;
	return PerformUsableTestAndCompleteIds(pEntity, dummy);
}

void CInteractor::UpdateTimers(EntityId newOverId, int usableIdx)
{
	CTimeValue now = m_pTimer->GetFrameStartTime();

	if (newOverId != m_nextOverId || usableIdx != m_nextOverIdx)
	{
		m_nextOverId = newOverId;
		m_nextOverIdx = usableIdx;
		m_nextOverTime = now;
	}
	float compareTime = m_nextOverId ? m_useHoverTime : m_unUseHoverTime;
	if ((m_nextOverId != m_overId || m_nextOverIdx != m_overIdx) && (now - m_nextOverTime).GetSeconds() > compareTime)
	{
		m_overId = m_nextOverId;
		m_overIdx = m_nextOverIdx;
		m_overTime = m_nextOverTime;
		m_sentMessageHover = m_sentLongHover = false;
		if (m_funcOnNewUsable)
			Script::CallMethod(m_pGameRules, m_funcOnNewUsable, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overIdx);
	}
	if (m_funcOnUsableMessage && !m_sentMessageHover && (now - m_overTime).GetSeconds() > m_messageHoverTime)
	{
		Script::CallMethod(m_pGameRules, m_funcOnUsableMessage, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overId, m_overIdx);

		m_sentMessageHover = true;
	}
	if (m_funcOnLongHover && !m_sentLongHover && (now - m_overTime).GetSeconds() > m_longHoverTime)
	{
		Script::CallMethod(m_pGameRules, m_funcOnLongHover, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overIdx);
		m_sentLongHover = true;
	}
}

void CInteractor::FullSerialize(TSerialize ser)
{
	ser.Value("useHoverTime", m_useHoverTime);
	ser.Value("unUseHoverTime", m_unUseHoverTime);
	ser.Value("messageHoverTime", m_messageHoverTime);
	ser.Value("longHoverTime", m_longHoverTime);
	ser.Value("queryMethods", m_queryMethods);
	ser.Value("lastRadius", m_lastRadius);

	if (ser.GetSerializationTarget() == eST_Script && ser.IsReading())
	{
		EntityId lockedById = 0, lockId = 0;
		int lockIdx = 0;
		ser.Value("locker", lockedById);
		if (lockedById)
		{
			if (m_lockedByEntityId && lockedById != m_lockedByEntityId)
			{
				GameWarning("Attempt to change lock status by an entity that did not lock us originally");
			}
			else
			{
				ser.Value("lockId", lockId);
				ser.Value("lockIdx", lockIdx);
				if (lockId && lockIdx)
				{
					m_lockedByEntityId = lockedById;
					m_lockEntityId = lockId;
					m_lockIdx = lockIdx;
				}
				else
				{
					m_lockedByEntityId = 0;
					m_lockEntityId = 0;
					m_lockIdx = 0;
				}
			}
		}
	}

	// the following should not be used as parameters
	if (ser.GetSerializationTarget() != eST_Script)
	{
		ser.Value("nextOverId", m_nextOverId);
		ser.Value("nextOverIdx", m_nextOverIdx);
		ser.Value("nextOverTime", m_nextOverTime);
		ser.Value("overId", m_overId);
		ser.Value("overIdx", m_overIdx);
		ser.Value("overTime", m_overTime);
		ser.Value("sentMessageHover", m_sentMessageHover);
		ser.Value("sentLongHover", m_sentLongHover);

		//don't serialize lockEntity, it will lock on usage
		if (ser.IsReading())
		{
			m_lockedByEntityId = 0;
			m_lockEntityId = 0;
			m_lockIdx = 0;
		}
	}
}

bool CInteractor::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	return true;
}

void CInteractor::HandleEvent(const SGameObjectEvent&)
{

}

void CInteractor::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_queryMethods);
	pSizer->AddObject(m_pQuery);
}

void CInteractor::PostSerialize()
{
	//?fix? : was invalid sometimes after QL
	if (!m_pQuery)
		m_pQuery = (CWorldQuery*) GetGameObject()->AcquireExtension("WorldQuery");

	if (m_funcOnNewUsable)
		Script::CallMethod(m_pGameRules, m_funcOnNewUsable, EntityIdToScript(GetEntityId()), EntityIdToScript(m_overId), m_overIdx);
}

bool CInteractor::IsEntityUsable(const IEntity* pEntity)
{
	CRY_ASSERT(pEntity);

	IEntityClass* pEntityClass = pEntity->GetClass();
	TUsableClassesMap::const_iterator cit = m_usableEntityClasses.find(pEntityClass);
	if (cit != m_usableEntityClasses.end())
	{
		return (cit->second && pEntity->GetScriptTable() && !pEntity->IsHidden() && !pEntity->IsInvisible());
	}
	else
	{
		bool hasIsUsableMethod = false;
		if (IScriptTable* pEntityScript = pEntity->GetScriptTable())
		{
			hasIsUsableMethod = pEntityScript->HaveValue("IsUsable");
		}
		m_usableEntityClasses.insert(TUsableClassesMap::value_type(pEntityClass, hasIsUsableMethod));

		return hasIsUsableMethod && !pEntity->IsHidden() && !pEntity->IsInvisible();
	}
}
