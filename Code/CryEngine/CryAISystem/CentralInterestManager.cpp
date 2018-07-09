// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CentralInterestManager.h"
#include "PersonalInterestManager.h"
#include "Puppet.h"

// For persistent debugging
#include <CryGame/IGameFramework.h>

const int CIM_RAYBUDGET = 1;                  // Max number of rays the interest system may fire per update; 2 would be extravagant
const float CIM_UPDATE_TIME = .1f;
const uint32 CIM_MAX_PIMS_PER_UPDATE = 2;
const float CIM_MIN_DIST_SQ = 40.f * 40.f;
const uint32 CIM_INITIAL_INTERESTING = 128;

//------------------------------------------------------------------------------------------------------------------------

// Zero singleton pointer
CCentralInterestManager* CCentralInterestManager::m_pCIMInstance = NULL;

//------------------------------------------------------------------------------------------------------------------------

CCentralInterestManager::CCentralInterestManager()
{
	m_bEnabled = false;
	m_pPersistentDebug = NULL;
	m_bEntityEventListenerInstalled = false;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::Init()
{
	m_cvDebugInterest = gEnv->pConsole->GetCVar("ai_DebugInterestSystem");
	m_cvCastRays = gEnv->pConsole->GetCVar("ai_InterestSystemCastRays");

	// Init vectors to stored sizes
	m_InterestingEntities.reserve(CIM_INITIAL_INTERESTING);

	m_lastUpdated = 0;
	m_fUpdateTime = 0.f;
}

//------------------------------------------------------------------------------------------------------------------------

CCentralInterestManager::~CCentralInterestManager()
{
	// Stop listening to all entities
	if (m_bEntityEventListenerInstalled)
	{
		gEnv->pEntitySystem->RemoveSink(this);
		m_bEntityEventListenerInstalled = false;
	}
}

//------------------------------------------------------------------------------------------------------------------------

CCentralInterestManager* CCentralInterestManager::GetInstance()
{
	// Return singleton pointer
	// Create from scratch if need be
	if (!m_pCIMInstance)
	{
		m_pCIMInstance = new CCentralInterestManager;
		m_pCIMInstance->Init();
	}

	return m_pCIMInstance;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::Reset()
{
	// Clear tracking vectors
	for (TVecInteresting::iterator itI = m_InterestingEntities.begin(); itI != m_InterestingEntities.end(); ++itI)
		itI->Invalidate();

	for (TVecPIMs::iterator itP = m_PIMs.begin(); itP != m_PIMs.end(); ++itP)
		itP->Assign(NILREF);

	m_lastUpdated = 0;
	m_fUpdateTime = 0.f;

	stl::free_container(m_Listeners);

	bool bRegisterWithEntitySystem = true;
	if (gEnv->bMultiplayer)
	{
		if (ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem"))
		{
			if (!pEnableAI->GetIVal())
			{
				bRegisterWithEntitySystem = false;
			}
		}
	}

	if (m_bEntityEventListenerInstalled != bRegisterWithEntitySystem)
	{
		if (bRegisterWithEntitySystem)
		{
			// Start listening to all moving entities
			CryLog("Registering CentralInterestManager with EntitySystem");
			gEnv->pEntitySystem->AddSink(this, IEntitySystem::OnSpawn | IEntitySystem::OnRemove);
			m_bEntityEventListenerInstalled = true;
		}
		else
		{
			CryLog("Unregistering CentralInterestManager from EntitySystem");
			gEnv->pEntitySystem->RemoveSink(this);
			m_bEntityEventListenerInstalled = false;
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::Enable(bool bEnable)
{
	if (m_bEnabled != bEnable)
	{
		m_bEnabled = bEnable;

		if (m_bEnabled)
		{
			if (!m_pPersistentDebug)
			{
				if (gEnv->pGameFramework)
				{
					m_pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
				}
			}
		}
	}

	return m_bEnabled;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::Update(float fDelta)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!m_bEnabled)
		return;

	m_fUpdateTime -= fDelta;
	if (m_fUpdateTime > 0.f)
		return;

	m_fUpdateTime = CIM_UPDATE_TIME;

	// Cache camera position
	const Vec3 vCameraPos = gEnv->pSystem->GetViewCamera().GetPosition();

	// Update all the active PIMs
	const uint32 PIMCount = m_PIMs.size();
	const uint32 MaxUpdateCount = std::min<uint32>(PIMCount, CIM_MAX_PIMS_PER_UPDATE);

	uint32 loopCount = 0;
	for (uint32 uCount = 0; (uCount < MaxUpdateCount) && (loopCount < PIMCount); ++loopCount)
	{
		m_lastUpdated = (m_lastUpdated + 1) % PIMCount;

		CPersonalInterestManager& pim = m_PIMs[m_lastUpdated];

		if (!pim.IsReset())
		{
			// Sanity check to only update active non-combat AIs and AIs that have been updated by the AISystem at least once
			// (otherwise this blocks the AISystem update calls for the entity).
			CPuppet* const pPuppet = CastToCPuppetSafe(pim.GetAssigned());
			if (pPuppet)
			{
				IAIActorProxy* pProxy = pPuppet->GetProxy();
				if (pProxy &&
				    !pPuppet->IsAlarmed() &&
				    (pPuppet->GetAlertness() == 0) &&
				    pPuppet->IsActive() &&
				    pPuppet->IsUpdatedOnce() &&
				    !pProxy->IsDead() &&
				    !pProxy->GetLinkedVehicleEntityId() &&
				    !pProxy->IsPlayingSmartObjectAction())
				{
					bool bCloseToCamera = pPuppet->GetPos().GetSquaredDistance2D(vCameraPos) < CIM_MIN_DIST_SQ;
					if (pim.Update(bCloseToCamera))
					{
						++uCount;
					}
				}
				else
				{
					if (pim.ForgetInterestingEntity())
					{
						++uCount;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------
void CCentralInterestManager::Serialize(TSerialize ser)
{
	ser.Value("m_InterestingEntities", m_InterestingEntities);
	ser.Value("m_PIMs", m_PIMs);
}

//------------------------------------------------------------------------------------------------------------------------
bool CCentralInterestManager::GatherData(IEntity* pEntity, SEntityInterest& entityInterest)
{
	assert(pEntity);

	SEntityInterest tempEntityInterest;
	bool bEnabled = false;

	if (IEntityArchetype* pEntityArchetype = pEntity->GetArchetype())
	{
		if (IScriptTable* pProperties = pEntityArchetype->GetProperties())
		{
			bEnabled = ReadDataFromTable(pProperties, tempEntityInterest);
			entityInterest.Set(tempEntityInterest);
		}
	}

	SmartScriptTable ssInstanceTable;
	bool bFound = false;
	if (IScriptTable* pScriptTable = pEntity->GetScriptTable())
	{
		bFound = pScriptTable->GetValue("PropertiesInstance", ssInstanceTable);
		if (!bFound)
		{
			bFound = pScriptTable->GetValue("Properties", ssInstanceTable);
		}
	}

	if (bFound)
	{
		SmartScriptTable ssInterestInstanceTable;
		if (ssInstanceTable->GetValue("Interest", ssInterestInstanceTable))
		{
			bool bOverrideArchetype = true;
			bool bExists = ssInterestInstanceTable->GetValue("bOverrideArchetype", bOverrideArchetype);

			if (!bExists || bOverrideArchetype)
			{
				bEnabled = ReadDataFromTable(ssInstanceTable, tempEntityInterest);
				entityInterest.Set(tempEntityInterest);
			}
		}
	}

	return bEnabled;
}

//------------------------------------------------------------------------------------------------------------------------
bool CCentralInterestManager::GatherData(IEntity* pEntity, SActorInterestSettings& actorInterestSettings)
{
	assert(pEntity);

	SActorInterestSettings tempActorInterestSettings;

	if (IEntityArchetype* pEntityArchetype = pEntity->GetArchetype())
	{
		if (IScriptTable* pProperties = pEntityArchetype->GetProperties())
		{
			ReadDataFromTable(pProperties, tempActorInterestSettings);
			actorInterestSettings = tempActorInterestSettings;
		}
	}

	SmartScriptTable ssInstanceTable;
	bool bFound = pEntity->GetScriptTable()->GetValue("PropertiesInstance", ssInstanceTable);
	if (!bFound)
	{
		bFound = pEntity->GetScriptTable()->GetValue("Properties", ssInstanceTable);
	}

	if (bFound)
	{
		SmartScriptTable ssInterestInstanceTable;
		if (ssInstanceTable->GetValue("Interest", ssInterestInstanceTable))
		{
			bool bOverrideArchetype = true;
			bool bExists = ssInterestInstanceTable->GetValue("bOverrideArchetype", bOverrideArchetype);

			if (!bExists || bOverrideArchetype)
			{
				ReadDataFromTable(ssInstanceTable, tempActorInterestSettings);
				actorInterestSettings = tempActorInterestSettings;
			}
		}
	}

	return actorInterestSettings.m_bEnablePIM;
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::RegisterInterestingEntity(IEntity* pEntity, float fRadius, float fBaseInterest, const char* szActionName, const Vec3& vOffset, float fPause, int nbShared)
{
	SEntityInterest* pInterest = NULL;
	bool bOnlyUpdate = false;
	bool bSomethingChanged = false;

	TVecInteresting::iterator itI = m_InterestingEntities.begin();
	TVecInteresting::iterator itEnd = m_InterestingEntities.end();
	for (; itI != itEnd; ++itI)
	{
		if (itI->m_entityId == pEntity->GetId())
		{
			pInterest = &(*itI);
			bSomethingChanged = itI->Set(pEntity->GetId(), fRadius, fBaseInterest, szActionName, vOffset, fPause, nbShared);
			break;
		}
	}

	if (!pInterest)
	{
		// Find a spare PIM
		itI = m_InterestingEntities.begin();
		for (; itI != itEnd; ++itI)
		{
			if (!(itI->IsValid()))
				break;
		}

		if (itI == itEnd)
		{
			m_InterestingEntities.push_back(SEntityInterest(pEntity->GetId(), fRadius, fBaseInterest, szActionName, vOffset, fPause, nbShared));
			pInterest = &(m_InterestingEntities.back());
		}
		else
		{
			itI->Set(pEntity->GetId(), fRadius, fBaseInterest, szActionName, vOffset, fPause, nbShared);
			pInterest = &(*itI);
		}

		bSomethingChanged = true;
	}

	if (IsDebuggingEnabled() && pInterest && bSomethingChanged)
	{
		string sText;
		sText.Format("Interest: %0.1f Radius: %0.1f Action: %s Pause: %0.1f Shared: %s",
		             pInterest->m_fInterest,
		             pInterest->m_fRadius,
		             pInterest->GetAction(),
		             pInterest->m_fPause,
		             (pInterest->m_nbShared > 0) ? "Yes" : "No");
		AddDebugTag(pInterest->m_entityId, sText.c_str());
	}

	return bSomethingChanged;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::ChangeInterestingEntityProperties(IEntity* pEntity, float fRadius, float fBaseInterest, const char* szActionName, const Vec3& vOffset, float fPause, int nbShared)
{
	assert(pEntity);

	SEntityInterest entityInterest;
	GatherData(pEntity, entityInterest);
	entityInterest.Set(pEntity->GetId(), fRadius, fBaseInterest, szActionName, vOffset, fPause, nbShared);
	RegisterInterestingEntity(pEntity, entityInterest.m_fRadius, entityInterest.m_fInterest, entityInterest.m_sActionName, entityInterest.m_vOffset, entityInterest.m_fPause, entityInterest.m_nbShared);
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::ChangeInterestedAIActorProperties(IEntity* pEntity, float fInterestFilter, float fAngleCos)
{
	assert(pEntity);

	SActorInterestSettings actorInterestSettings;
	GatherData(pEntity, actorInterestSettings);
	actorInterestSettings.Set(actorInterestSettings.m_bEnablePIM, fInterestFilter, fAngleCos);
	RegisterInterestedAIActor(pEntity, actorInterestSettings.m_bEnablePIM, actorInterestSettings.m_fInterestFilter, actorInterestSettings.m_fAngleCos);
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::DeregisterInterestingEntity(IEntity* pEntity)
{
	TVecInteresting::iterator itI = m_InterestingEntities.begin();
	TVecInteresting::iterator itEnd = m_InterestingEntities.end();
	for (; itI != itEnd; ++itI)
	{
		if (itI->m_entityId == pEntity->GetId())
		{
			itI->Invalidate();
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::RegisterInterestedAIActor(IEntity* pEntity, bool bEnablePIM, float fInterestFilter, float fAngleCos)
{
	// Try to get an AI object
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return false;

	// Try to cast to pAIActor
	CAIActor* pAIActor = pAIObject->CastToCAIActor();
	if (!pAIActor)
		return false;

	if (CPersonalInterestManager* pPIM = FindPIM(pEntity))
	{
		pPIM->SetSettings(bEnablePIM, fInterestFilter, fAngleCos);
		return true;
	}

	// Find a spare PIM
	TVecPIMs::iterator it = m_PIMs.begin();
	TVecPIMs::iterator itEnd = m_PIMs.end();
	for (; it != itEnd; ++it)
	{
		CPersonalInterestManager* p0 = &(*it);
		if (it->IsReset())
		{
			it->Assign(pAIActor);
			it->SetSettings(bEnablePIM, fInterestFilter, fAngleCos);
			break;
		}
	}

	if (it == itEnd)
	{
		// m_PIMs is freed on Reset, so this is the most convenient place to preallocate
		m_PIMs.push_back(CPersonalInterestManager(pAIActor));
		m_lastUpdated = 0;

		if (CPersonalInterestManager* pPIM = FindPIM(pEntity))
		{
			pPIM->SetSettings(bEnablePIM, fInterestFilter, fAngleCos);
		}
		else
		{
			assert(false && "Cannot find a just recently created PersonalInterestManager!");
		}
	}

	// Mark this to say we successfully registered it
	return true;
}

//------------------------------------------------------------------------------------------------------------------------

CPersonalInterestManager* CCentralInterestManager::FindPIM(IEntity* pEntity)
{
	assert(pEntity);
	if (pEntity)
	{
		EntityId entityId = pEntity->GetId();

		TVecPIMs::iterator it = m_PIMs.begin();
		TVecPIMs::iterator itEnd = m_PIMs.end();

		for (; it != itEnd; ++it)
		{
			CPersonalInterestManager& pim = *it;
			CAIActor* pAIActor = pim.GetAssigned();
			if ((pAIActor != NULL) && (pAIActor->GetEntityID() == entityId))
				return &pim;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::DeregisterInterestedAIActor(IEntity* pEntity)
{
	// Try to get an AI object
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return false;

	// Try to cast to actor
	CAIActor* pAIActor = pAIObject->CastToCAIActor();
	if (!pAIActor)
		return false;

	if (CPersonalInterestManager* pPIM = FindPIM(pEntity))
	{
		pPIM->Assign(NILREF);
	}

	return true;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::AddDebugTag(EntityId entityId, const char* szString, float fTime)
{
	if (IsDebuggingEnabled())
	{
		string text;
		text.Format("[%s]: %s", gEnv->pEntitySystem->GetEntity(entityId)->GetName(), szString);

		if (!m_pPersistentDebug)
		{
			m_pPersistentDebug = gEnv->pGameFramework->GetIPersistantDebug();
		}

		if (fTime < 0.f)
		{
			m_pPersistentDebug->AddEntityTag(SEntityTagParams(entityId, text.c_str()));
		}
		else
		{
			m_pPersistentDebug->AddEntityTag(SEntityTagParams(entityId, text.c_str(), 1.5f, ColorF(1.f, 1.f, 1.f, 1.f), fTime));
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::DeregisterObject(IEntity* pEntity)
{
	assert(pEntity);
	if (CastToIAIActorSafe(pEntity->GetAI()))
	{
		DeregisterInterestedAIActor(pEntity);
	}

	// Find this entity
	EntityId entityId = pEntity->GetId();
	TVecInteresting::iterator it = m_InterestingEntities.begin();
	TVecInteresting::iterator itEnd = m_InterestingEntities.end();
	for (; it != itEnd; ++it)
	{
		if (it->m_entityId == entityId)
			break;
	}

	if (it != itEnd)
	{
		it->Invalidate();

		// Debugging
		AddDebugTag(entityId, "Deregistered");
	}
}

//------------------------------------------------------------------------------------------------------------------------
void CCentralInterestManager::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	assert(pEntity);

	if ((event.event == ENTITY_EVENT_START_LEVEL) ||
	    (event.event == ENTITY_EVENT_RESET && event.nParam[0] == 1) ||
	    (event.event == ENTITY_EVENT_UNHIDE))
	{
		RegisterObject(pEntity);
	}
	else
	{
		DeregisterObject(pEntity);

		// Is this a potential pAIActor?
		if (CastToIAIActorSafe(pEntity->GetAI()))
		{
			DeregisterInterestedAIActor(pEntity);
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------
void CCentralInterestManager::RegisterObject(IEntity* pEntity)
{
	assert(pEntity);

	if (CastToIAIActorSafe(pEntity->GetAI()))
	{
		SActorInterestSettings actorInterestSettings;
		if (GatherData(pEntity, actorInterestSettings))
		{
			RegisterInterestedAIActor(pEntity, actorInterestSettings.m_bEnablePIM, actorInterestSettings.m_fInterestFilter, actorInterestSettings.m_fAngleCos);
		}
	}
	else
	{
		SEntityInterest entityInterest;
		if (GatherData(pEntity, entityInterest))
		{
			RegisterInterestingEntity(pEntity,
			                          entityInterest.m_fRadius,
			                          entityInterest.m_fInterest,
			                          entityInterest.m_sActionName,
			                          entityInterest.m_vOffset,
			                          entityInterest.m_fPause,
			                          entityInterest.m_nbShared);
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------
bool CCentralInterestManager::ReadDataFromTable(const SmartScriptTable& ssTable, SEntityInterest& entityInterest)
{
	const char* szAction = NULL;
	bool bInteresting = false;

	SmartScriptTable ssInterestTable;
	if (ssTable->GetValue("Interest", ssInterestTable))
	{
		ssInterestTable->GetValue("bInteresting", bInteresting);

		ssInterestTable->GetValue("soaction_Action", szAction);
		if (szAction)
		{
			entityInterest.SetAction(szAction);
		}

		ssInterestTable->GetValue("InterestLevel", entityInterest.m_fInterest);
		ssInterestTable->GetValue("Radius", entityInterest.m_fRadius);
		ssInterestTable->GetValue("vOffset", entityInterest.m_vOffset);
		ssInterestTable->GetValue("Pause", entityInterest.m_fPause);

		bool bShared = false;
		ssInterestTable->GetValue("bShared", bShared);
		entityInterest.m_nbShared = bShared ? 1 : 0;
	}

	return bInteresting;
}

//------------------------------------------------------------------------------------------------------------------------
bool CCentralInterestManager::ReadDataFromTable(const SmartScriptTable& ssTable, SActorInterestSettings& actorInterestSettings)
{
	SmartScriptTable ssInterestTable;

	if (ssTable->GetValue("Interest", ssInterestTable))
	{
		ssInterestTable->GetValue("bInterested", actorInterestSettings.m_bEnablePIM);
		ssInterestTable->GetValue("MinInterestLevel", actorInterestSettings.m_fInterestFilter);
		float fAngleInDegrees;
		ssInterestTable->GetValue("Angle", fAngleInDegrees);
		actorInterestSettings.SetAngleInDegrees(fAngleInDegrees);
	}

	return actorInterestSettings.m_bEnablePIM;
}

//------------------------------------------------------------------------------------------------------------------------
void CCentralInterestManager::OnSpawn(IEntity* pEntity, SEntitySpawnParams&)
{
	assert(pEntity);

	if (IScriptTable* pEntityScriptTable = pEntity->GetScriptTable())
	{
		SmartScriptTable ssInstanceTable;
		bool bFound = pEntityScriptTable->GetValue("PropertiesInstance", ssInstanceTable);
		if (!bFound)
		{
			bFound = pEntityScriptTable->GetValue("Properties", ssInstanceTable);
		}

		if (bFound)
		{
			SmartScriptTable ssInterestInstanceTable;
			if (ssInstanceTable->GetValue("Interest", ssInterestInstanceTable))
			{
				IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
				EntityId entityId = pEntity->GetId();

				pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_START_LEVEL, this);
				pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
				pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_RESET, this);
				pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_HIDE, this);
				pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_UNHIDE, this);
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::OnRemove(IEntity* pEntity)
{
	assert(pEntity);

	DeregisterObject(pEntity);

	// Is this a potential AI Actor?
	if (IAIObject* pAIObject = pEntity->GetAI())
	{
		if (pAIObject->CastToIAIActor())
		{
			DeregisterInterestedAIActor(pEntity);
		}
	}

	return true;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::RegisterListener(IInterestListener* pInterestListener, EntityId idInterestingEntity)
{
	assert(pInterestListener);
	assert(idInterestingEntity);

	if (m_Listeners.find(idInterestingEntity) == m_Listeners.end())
	{
		m_Listeners.insert(TMapListeners::value_type(idInterestingEntity, pInterestListener));
	}
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::UnRegisterListener(IInterestListener* pInterestListener, EntityId idInterestingEntity)
{
	assert(pInterestListener);
	assert(idInterestingEntity);

	TMapListeners::iterator it = m_Listeners.find(idInterestingEntity);

	while (it != m_Listeners.end())
	{
		m_Listeners.erase(it);
		it = m_Listeners.find(idInterestingEntity);
	}
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::OnInterestEvent(IInterestListener::EInterestEvent eInterestEvent, EntityId idActor, EntityId idInterestingEntity)
{
	assert(idActor);
	assert(idInterestingEntity);

	std::pair<TMapListeners::iterator, TMapListeners::iterator> range;
	range = m_Listeners.equal_range(idInterestingEntity);

	TMapListeners::iterator itEnd = m_Listeners.end();
	TMapListeners::iterator it = range.first;
	for (; (it != itEnd) && (it != range.second); ++it)
	{
		it->second->OnInterestEvent(eInterestEvent, idActor, idInterestingEntity);
	}
}

//-----------------------------------------------------------------------------------------------------

bool SEntityInterest::Set(EntityId entityId, float fRadius, float fInterest, const char* szActionName, const Vec3& vOffset, float fPause, int nbShared)
{
	bool bChanged = false;

	if (entityId != m_entityId)
	{
		m_entityId = entityId;
		bChanged = true;
	}

	if (fRadius >= 0.f && (fRadius != m_fRadius))
	{
		m_fRadius = fRadius;
		bChanged = true;
	}

	if (fInterest >= 0.f && (fInterest != m_fInterest))
	{
		m_fInterest = fInterest;
		bChanged = true;
	}

	if (!vOffset.IsZero() && (vOffset != m_vOffset))
	{
		m_vOffset = vOffset;
		bChanged = true;
	}

	if (szActionName && (strlen(szActionName) > 0) && m_sActionName.compare(szActionName))
	{
		SetAction(szActionName);
		bChanged = true;
	}

	if (fPause >= 0.f && (fPause != m_fPause))
	{
		m_fPause = fPause;
		bChanged = true;
	}

	if (nbShared >= 0 && (nbShared != m_nbShared))
	{
		m_nbShared = nbShared;
		bChanged = true;
	}

	if (bChanged && (entityId > 0) && gAIEnv.pSmartObjectManager)
	{
		gAIEnv.pSmartObjectManager->AddSmartObjectState(GetEntity(), "Registered");
	}

	return bChanged;
}

void SEntityInterest::SetAction(const char* szActionName)
{
	m_sActionName = szActionName;

	m_eSupportedActorClasses = 0;

	if (m_sActionName.find("_human_") != string::npos)
	{
		m_eSupportedActorClasses |= eACTOR_CLASS_HUMAN_GRUNT;
	}
	if (m_sActionName.find("_grunt_") != string::npos)
	{
		m_eSupportedActorClasses |= eACTOR_CLASS_ALIEN_GRUNT;
	}
	if (m_sActionName.find("_stalker_") != string::npos)
	{
		m_eSupportedActorClasses |= eACTOR_CLASS_ALIEN_STALKER;
	}

	if (m_eSupportedActorClasses == 0)
	{
		m_eSupportedActorClasses |= eACTOR_CLASS_HUMAN_GRUNT;
		m_eSupportedActorClasses |= eACTOR_CLASS_ALIEN_GRUNT;
		m_eSupportedActorClasses |= eACTOR_CLASS_ALIEN_STALKER;
	}
}

bool SEntityInterest::SupportsActorClass(const char* szActorClass) const
{
	if (!strcmp(szActorClass, "HumanGrunt"))
		return (m_eSupportedActorClasses & eACTOR_CLASS_HUMAN_GRUNT) != 0;
	else if (!strcmp(szActorClass, "AlienGrunt"))
		return (m_eSupportedActorClasses & eACTOR_CLASS_ALIEN_GRUNT) != 0;
	else if (!strcmp(szActorClass, "AlienStalker"))
		return (m_eSupportedActorClasses & eACTOR_CLASS_ALIEN_STALKER) != 0;

	return false;
}
