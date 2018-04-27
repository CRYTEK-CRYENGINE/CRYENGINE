// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Helper to allow AI to see objects which have just been thrown
  
 -------------------------------------------------------------------------
  History:
  - 29:04:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "VisibleObjectsHelper.h"
#include "Agent.h"
#include <CryAISystem/IAIObjectManager.h>
#include <CryAISystem/ITargetTrackManager.h>

//////////////////////////////////////////////////////////////////////////
CVisibleObjectsHelper::CVisibleObjectsHelper()
{
	
}

//////////////////////////////////////////////////////////////////////////
CVisibleObjectsHelper::~CVisibleObjectsHelper()
{
	ClearAllObjects();
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::RegisterObject(EntityId objectId, int factionId, uint32 visibleObjectRule, 
										   TObjectVisibleFunc pObjectVisibleFunc, void *pObjectVisibleFuncArg)
{
	bool bResult = false;

	TVisibleObjects::iterator itFind = m_VisibleObjects.find(objectId);
	if (itFind == m_VisibleObjects.end())
	{
		IEntity *pObject = gEnv->pEntitySystem->GetEntity(objectId);
		if (pObject)
		{
			SVisibleObject visibleObject;
			visibleObject.entityId = objectId;
			visibleObject.rule = visibleObjectRule;
			visibleObject.pFunc = pObjectVisibleFunc;
			visibleObject.pFuncArg = pObjectVisibleFuncArg;

			IPhysicalEntity *pPhysics = pObject->GetPhysics();

			// Basic observable parameters
			ObservableParams observableParams;
			observableParams.faction = factionId;
			observableParams.typeMask = General;
			observableParams.observablePositionsCount = 1;
			observableParams.observablePositions[0] = pObject->GetWorldPos();
			observableParams.skipListSize = pPhysics ? 1 : 0;
			observableParams.skipList[0] = pPhysics;

			RegisterVisibility(visibleObject, observableParams);

			TVisibleObjects::value_type newEntry(objectId, visibleObject);
			m_VisibleObjects.insert(newEntry);
			bResult = true;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::RegisterObject(EntityId objectId, const ObservableParams &observableParams, uint32 visibleObjectRule, 
										   TObjectVisibleFunc pObjectVisibleFunc, void *pObjectVisibleFuncArg)
{
	bool bResult = false;

	TVisibleObjects::iterator itFind = m_VisibleObjects.find(objectId);
	if (itFind == m_VisibleObjects.end())
	{
		SVisibleObject visibleObject;
		visibleObject.entityId = objectId;
		visibleObject.rule = visibleObjectRule;
		visibleObject.pFunc = pObjectVisibleFunc;
		visibleObject.pFuncArg = pObjectVisibleFuncArg;

		RegisterVisibility(visibleObject, observableParams);

		TVisibleObjects::value_type newEntry(objectId, visibleObject);
		m_VisibleObjects.insert(newEntry);
		bResult = true;
	}
	
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::UnregisterObject(EntityId objectId)
{
	bool bResult = false;

	TVisibleObjects::iterator itFind = m_VisibleObjects.find(objectId);
	if (itFind != m_VisibleObjects.end())
	{
		SVisibleObject &visibleObject = itFind->second;
		UnregisterVisibility(visibleObject);

		m_VisibleObjects.erase(itFind);
		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::SetObjectVisibleParams(EntityId objectId, const ObservableParams &observableParams)
{
	bool bResult = false;

	TVisibleObjects::iterator itFind = m_VisibleObjects.find(objectId);
	if (itFind != m_VisibleObjects.end())
	{
		SVisibleObject &visibleObject = itFind->second;

		IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();
		visionMap.ObservableChanged(visibleObject.visionId, observableParams, eChangedAll);

		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::SetObjectVisibleRule(EntityId objectId, uint32 visibleObjectRule)
{
	bool bResult = false;

	TVisibleObjects::iterator itFind = m_VisibleObjects.find(objectId);
	if (itFind != m_VisibleObjects.end())
	{
		SVisibleObject &visibleObject = itFind->second;
		visibleObject.rule = visibleObjectRule;
		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::SetObjectVisibleFunc(EntityId objectId, TObjectVisibleFunc pObjectVisibleFunc, void *pObjectVisibleFuncArg)
{
	bool bResult = false;

	TVisibleObjects::iterator itFind = m_VisibleObjects.find(objectId);
	if (itFind != m_VisibleObjects.end())
	{
		SVisibleObject &visibleObject = itFind->second;
		visibleObject.pFunc = pObjectVisibleFunc;
		visibleObject.pFuncArg = pObjectVisibleFuncArg;
		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CVisibleObjectsHelper::RegisterVisibility(SVisibleObject &visibleObject, const ObservableParams &observableParams) const
{
	IEntity *pObject = gEnv->pEntitySystem->GetEntity(visibleObject.entityId);
	if (pObject)
	{
		IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

		visibleObject.visionId = visionMap.CreateVisionID(pObject->GetName());
		visionMap.RegisterObservable(visibleObject.visionId, observableParams);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVisibleObjectsHelper::UnregisterVisibility(SVisibleObject &visibleObject) const
{
	IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

	if (visibleObject.visionId)
	{
		visionMap.UnregisterObservable(visibleObject.visionId);
		visibleObject.visionId = VisionID();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::IsObjectVisible(const Agent& agent, EntityId objectId) const
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	assert(agent.IsValid());

	bool bIsVisible = false;

	TVisibleObjects::const_iterator itFind = m_VisibleObjects.find(objectId);
	if (itFind != m_VisibleObjects.end())
	{
		const SVisibleObject &visibleObject = itFind->second;
		bIsVisible = IsObjectVisible(agent, visibleObject);
	}

	return bIsVisible;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::IsObjectVisible(const Agent& agent, const SVisibleObject &visibleObject) const
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	assert(agent.IsValid());

	bool bIsVisible = false;

	if (visibleObject.bIsObservable)
	{
		bIsVisible = agent.CanSee(visibleObject.visionId);

		if (bIsVisible && eVOR_UseMaxViewDist == (visibleObject.rule & eVOR_UseMaxViewDist))
		{
			bIsVisible = CheckObjectViewDist(agent, visibleObject);
		}
	}

	return bIsVisible;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::CheckObjectViewDist(const Agent& agent, const SVisibleObject &visibleObject) const
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	assert(agent.IsValid());

	bool bInViewDist = true;

	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	IEntity *pAIEntity = pEntitySystem->GetEntity(agent.GetEntityID());
	IEntity *pObjectEntity = pEntitySystem->GetEntity(visibleObject.entityId);

	IEntityRender *pObjectRenderProxy = (pAIEntity != NULL && pObjectEntity ? (pObjectEntity->GetRenderInterface()) : NULL);
	if (pObjectRenderProxy != NULL)
	{
		IRenderNode *pObjectRenderNode = pObjectRenderProxy->GetRenderNode();
		if (pObjectRenderNode != NULL)
		{
			const float fDistanceSq = pAIEntity->GetWorldPos().GetSquaredDistance(pObjectEntity->GetWorldPos());
			const float fMaxViewDistSq = sqr(pObjectRenderNode->GetMaxViewDist());
			
			bInViewDist = (fDistanceSq <= fMaxViewDistSq);
		}
	}

	return bInViewDist;
}

//////////////////////////////////////////////////////////////////////////
void CVisibleObjectsHelper::ClearAllObjects()
{
	TVisibleObjects::iterator itObject = m_VisibleObjects.begin();
	TVisibleObjects::iterator itObjectEnd = m_VisibleObjects.end();
	for (; itObject != itObjectEnd; ++itObject)
	{
		SVisibleObject &visibleObject = itObject->second;
		UnregisterVisibility(visibleObject);
	}

	m_VisibleObjects.clear();
}

//////////////////////////////////////////////////////////////////////////
void CVisibleObjectsHelper::Reset()
{
	ClearAllObjects();
}

//////////////////////////////////////////////////////////////////////////
void CVisibleObjectsHelper::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const float fCurrTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();

	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	TActiveVisibleObjects activeVisibleObjects;
	activeVisibleObjects.reserve(m_VisibleObjects.size());

	TVisibleObjects::iterator itNext = m_VisibleObjects.begin();
	while (itNext != m_VisibleObjects.end())
	{
		TVisibleObjects::iterator itObject = itNext++;
		
		EntityId objectId = itObject->first;
		SVisibleObject &visibleObject = itObject->second;

		IEntity *pObject = pEntitySystem->GetEntity(objectId);
		if (pObject)
		{
			const bool bIsStillActive = (fCurrTime - visibleObject.fLastActiveTime <= 1.0f);
			const bool bPassesRule = (pObject && CheckVisibilityRule(pObject, visibleObject, fCurrTime));

			if (bPassesRule)
				visibleObject.fLastActiveTime = fCurrTime;
			visibleObject.bIsObservable = (bPassesRule || bIsStillActive);

			if (visibleObject.bIsObservable)
			{
				ObservableParams observableParams;
				observableParams.observablePositionsCount = 1;
				observableParams.observablePositions[0] = pObject->GetWorldPos();
				visionMap.ObservableChanged(visibleObject.visionId, observableParams, eChangedPosition);

				if (visibleObject.pFunc || eVOR_FlagNotifyOnSeen == (visibleObject.rule & eVOR_FlagNotifyOnSeen))
					activeVisibleObjects.push_back(&visibleObject);
			}
			else if (eVOR_FlagDropOnceInvisible == (visibleObject.rule & eVOR_FlagDropOnceInvisible))
			{
				// No longer visible, so kill it

				// TODO: Kevin, please take a look here and see if you're happy with how i unregister it /Jonas
				UnregisterVisibility(visibleObject);
				m_VisibleObjects.erase(itObject);
			}
		}
		else
		{
			// TODO: Kevin, please take a look here and see if you're happy with how i unregister it /Jonas
			UnregisterVisibility(visibleObject);
			m_VisibleObjects.erase(itObject);
		}
	}

	IAIObjectManager* pAIObjectManager = !activeVisibleObjects.empty() ? gEnv->pAISystem->GetAIObjectManager() : NULL;
	IAIObjectIter* pAIObjectIter = pAIObjectManager ? pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_ACTOR) : NULL;
	if (pAIObjectIter)
	{
		while (IAIObject *pAIObject = pAIObjectIter->GetObject())
		{
			Agent agent(pAIObject);
			if (agent.IsValid())
			{
				CheckVisibilityToAI(activeVisibleObjects, agent);
			}

			pAIObjectIter->Next();
		}

		pAIObjectIter->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::CheckVisibilityRule(IEntity *pObject, SVisibleObject &visibleObject, float fCurrTime) const
{
	assert(pObject);

	bool bIsActive = true;
	const uint32 checkRule = (visibleObject.rule & eVOR_TYPE_MASK);

	// Moving check
	if (eVOR_OnlyWhenMoving == (checkRule & eVOR_OnlyWhenMoving))
	{
		bIsActive = CheckVisibilityRule_OnlyWhenMoving(pObject, visibleObject);
	}

	return bIsActive;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleObjectsHelper::CheckVisibilityRule_OnlyWhenMoving(IEntity *pObject, const SVisibleObject &visibleObject) const
{
	bool bResult = false;

	pe_status_dynamics dyn;
	IPhysicalEntity *pPhysics = pObject->GetPhysics();
	if (pPhysics != NULL && pPhysics->GetStatus(&dyn))
	{
		bResult = (dyn.v.GetLengthSquared() > 0.1f);
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CVisibleObjectsHelper::CheckVisibilityToAI(const TActiveVisibleObjects &activeVisibleObjects, const Agent& agent) const
{
	assert(agent.IsValid());

	IScriptSystem *pSS = gEnv->pScriptSystem;
	assert(pSS);

	IEntity *pAIEntity = gEnv->pEntitySystem->GetEntity(agent.GetEntityID());

	TActiveVisibleObjects::const_iterator itObject = activeVisibleObjects.begin();
	TActiveVisibleObjects::const_iterator itObjectEnd = activeVisibleObjects.end();
	for (; itObject != itObjectEnd; ++itObject)
	{
		const SVisibleObject *visibleObject = (*itObject);
		assert(visibleObject);

		const bool bVisible = IsObjectVisible(agent, *visibleObject);
		if (bVisible)
		{
			// Callback function
			if (visibleObject->pFunc)
			{
				visibleObject->pFunc(agent, visibleObject->entityId, visibleObject->pFuncArg);
			}

			// Notify on seen
			if (pAIEntity != NULL && eVOR_FlagNotifyOnSeen == (visibleObject->rule & eVOR_FlagNotifyOnSeen))
			{
				IVisionMap& visionMap = *gEnv->pAISystem->GetVisionMap();
				const ObservableParams* pObservableParams = visionMap.GetObservableParams(visibleObject->visionId);

				if (pObservableParams != NULL)
				{
					IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(pObservableParams->entityId);
					ITargetTrackManager *pTargetTrackManager = gEnv->pAISystem->GetTargetTrackManager();
					if (pTargetEntity != NULL && pTargetTrackManager != NULL)
					{
						const tAIObjectID aiOwnerId = pAIEntity->GetAIObjectID();
						const tAIObjectID aiTargetId = pTargetEntity->GetAIObjectID();

						TargetTrackHelpers::SStimulusEvent eventInfo;
						eventInfo.vPos = pTargetEntity->GetWorldPos();
						eventInfo.eStimulusType = TargetTrackHelpers::eEST_Visual;
						eventInfo.eTargetThreat = AITHREAT_INTERESTING;
						pTargetTrackManager->HandleStimulusEventForAgent(aiOwnerId, aiTargetId, "SeeThrownObject", eventInfo);

						IAIObject* pAIObjectSender = pAIEntity->GetAI();
						if (pAIObjectSender)
						{
							IAISignalExtraData *pSignalData = gEnv->pAISystem->CreateSignalExtraData();
							if (pSignalData)
							{
								pSignalData->nID = visibleObject->entityId;
							}
							gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnSawObjectMove", pAIObjectSender, pSignalData);
						}
					}
				}
			}
		}
	}
}
