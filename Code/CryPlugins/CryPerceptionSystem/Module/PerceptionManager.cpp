// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PerceptionManager.h"
#include "ScriptBind_PerceptionManager.h"

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IActorLookUp.h>
#include <CryAISystem/IAIObjectManager.h>
#include <CryAISystem/IAIActorProxy.h>

#include <CryNetwork/ISerialize.h>

#include <CryPhysics/RayCastQueue.h>

//#include "DebugDrawContext.h"

// AI Stimulus names for debugging
static const char* g_szAIStimulusType[EAIStimulusType::AISTIM_LAST] =
{
	"SOUND",
	"COLLISION",
	"EXPLOSION",
	"BULLET_WHIZZ",
	"BULLET_HIT",
	"GRENADE"
};
static const char* g_szAISoundStimType[AISOUND_LAST] =
{
	" GENERIC",
	" COLLISION",
	" COLLISION_LOUD",
	" MOVEMENT",
	" MOVEMENT_LOUD",
	" WEAPON",
	" EXPLOSION"
};
static const char* g_szAIGrenadeStimType[AIGRENADE_LAST] =
{
	" THROWN",
	" COLLISION",
	" FLASH_BANG",
	" SMOKE"
};

CPerceptionManager* CPerceptionManager::s_pInstance = nullptr;

//-----------------------------------------------------------------------------------------------------------
CPerceptionManager::CPerceptionManager() :
	m_pScriptBind(nullptr),
	m_bRegistered(false)
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CPerceptionManager");
	
	Reset(IAISystem::RESET_INTERNAL);
	for (unsigned i = 0; i < AI_MAX_STIMULI; ++i)
	{
		m_stimulusTypes[i].Reset();
	}

	m_cVars.Init();

	CRY_ASSERT(s_pInstance == nullptr);
	s_pInstance = this;
}

//-----------------------------------------------------------------------------------------------------------
CPerceptionManager::~CPerceptionManager()
{
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
	
	if (gEnv->pAISystem)
	{
		gEnv->pAISystem->Callbacks().ObjectCreated().Remove(functor(*this, &CPerceptionManager::OnAIObjectCreated));
		gEnv->pAISystem->Callbacks().ObjectRemoved().Remove(functor(*this, &CPerceptionManager::OnAIObjectRemoved));
		gEnv->pAISystem->UnregisterSystemComponent(this);
	}
	delete m_pScriptBind;
}

void CPerceptionManager::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT:
	{
		CRY_ASSERT(gEnv->pAISystem);
		if (!m_bRegistered && gEnv->pAISystem)
		{
			gEnv->pAISystem->RegisterSystemComponent(this);
			gEnv->pAISystem->Callbacks().ObjectCreated().Add(functor(*this, &CPerceptionManager::OnAIObjectCreated));
			gEnv->pAISystem->Callbacks().ObjectRemoved().Add(functor(*this, &CPerceptionManager::OnAIObjectRemoved));

			m_bRegistered = true;
		}
		if (!m_pScriptBind)
		{
			m_pScriptBind = new CScriptBind_PerceptionManager(gEnv->pSystem);
		}
	}
	default:
		break;
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::InitCommonTypeDescs()
{
	SAIStimulusTypeDesc desc;

	// Sound
	desc.Reset();
	desc.SetName("Sound");
	desc.processDelay = 0.15f;
	desc.duration[AISOUND_GENERIC] = 2.0f;
	desc.duration[AISOUND_COLLISION] = 4.0f;
	desc.duration[AISOUND_COLLISION_LOUD] = 4.0f;
	desc.duration[AISOUND_MOVEMENT] = 2.0f;
	desc.duration[AISOUND_MOVEMENT_LOUD] = 4.0f;
	desc.duration[AISOUND_WEAPON] = 4.0f;
	desc.duration[AISOUND_EXPLOSION] = 6.0f;
	desc.filterTypes = 0;
	desc.nFilters = 0;
	RegisterStimulusDesc(AISTIM_SOUND, desc);

	// Collision
	desc.Reset();
	desc.SetName("Collision");
	desc.processDelay = 0.15f;
	desc.duration[AICOL_SMALL] = 7.0f;
	desc.duration[AICOL_MEDIUM] = 7.0f;
	desc.duration[AICOL_LARGE] = 7.0f;
	desc.filterTypes = (1 << AISTIM_COLLISION) | (1 << AISTIM_EXPLOSION);
	desc.nFilters = 2;
	desc.filters[0].Set(AISTIM_COLLISION, 0, AISTIMFILTER_MERGE_AND_DISCARD, 0.9f); // Merge nearby collisions
	desc.filters[1].Set(AISTIM_EXPLOSION, 0, AISTIMFILTER_DISCARD, 2.5f);           // Discard collision near explosions
	RegisterStimulusDesc(AISTIM_COLLISION, desc);

	// Explosion
	desc.Reset();
	desc.SetName("Explosion");
	desc.processDelay = 0.15f;
	desc.duration[0] = 7.0f;
	desc.filterTypes = (1 << AISTIM_EXPLOSION);
	desc.nFilters = 1;
	desc.filters[0].Set(AISTIM_EXPLOSION, 0, AISTIMFILTER_MERGE_AND_DISCARD, 0.5f); // Merge nearby explosions
	RegisterStimulusDesc(AISTIM_EXPLOSION, desc);

	// Bullet Whizz
	desc.Reset();
	desc.SetName("BulletWhizz");
	desc.processDelay = 0.01f;
	desc.duration[0] = 0.5f;
	desc.filterTypes = 0;
	desc.nFilters = 0;
	RegisterStimulusDesc(AISTIM_BULLET_WHIZZ, desc);

	// Bullet Hit
	desc.Reset();
	desc.SetName("BulletHit");
	desc.processDelay = 0.15f;
	desc.duration[0] = 0.5f;
	desc.filterTypes = (1 << AISTIM_BULLET_HIT);
	desc.nFilters = 1;
	desc.filters[0].Set(AISTIM_BULLET_HIT, 0, AISTIMFILTER_MERGE_AND_DISCARD, 0.5f); // Merge nearby hits
	RegisterStimulusDesc(AISTIM_BULLET_HIT, desc);

	// Grenade
	desc.Reset();
	desc.SetName("Grenade");
	desc.processDelay = 0.15f;
	desc.duration[AIGRENADE_THROWN] = 6.0f;
	desc.duration[AIGRENADE_COLLISION] = 6.0f;
	desc.duration[AIGRENADE_FLASH_BANG] = 6.0f;
	desc.duration[AIGRENADE_SMOKE] = 6.0f;
	desc.filterTypes = (1 << AISTIM_GRENADE);
	desc.nFilters = 1;
	desc.filters[0].Set(AISTIM_GRENADE, AIGRENADE_COLLISION, AISTIMFILTER_MERGE_AND_DISCARD, 1.0f); // Merge nearby collisions
	RegisterStimulusDesc(AISTIM_GRENADE, desc);

}

//-----------------------------------------------------------------------------------------------------------
bool CPerceptionManager::RegisterStimulusDesc(EAIStimulusType type, const SAIStimulusTypeDesc& desc)
{
	m_stimulusTypes[type] = desc;
	return true;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::Reset(IAISystem::EResetReason reason)
{
	m_visBroadPhaseDt = 0;

	if (reason == IAISystem::RESET_UNLOAD_LEVEL)
	{
		stl::free_container(m_incomingStimuli);
		for (unsigned int i = 0; i < AI_MAX_STIMULI; ++i)
		{
			stl::free_container(m_stimuli[i]);
			m_ignoreStimuliFrom[i].clear();
		}

		if (m_eventListeners.empty())
			stl::free_container(m_eventListeners);

		stl::free_container(m_priorityTargets);
		stl::free_container(m_targetEntities);
		stl::free_container(m_probableTargets);
		stl::free_container(m_actorComponents);
	}
	else
	{
		for (unsigned i = 0; i < AI_MAX_STIMULI; ++i)
		{
			m_stimuli[i].reserve(64);
			m_stimuli[i].clear();
			m_ignoreStimuliFrom[i].clear();
		}
		m_incomingStimuli.reserve(64);
		m_incomingStimuli.clear();

		m_targetEntities.reserve(10);
		m_probableTargets.reserve(32);

		InitCommonTypeDescs();
	}

	for (auto& actorIt : m_actorComponents)
	{
		actorIt.second.Reset();
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::SetPriorityObjectType(uint16 type)
{
	if (std::find(m_priorityObjectTypes.begin(), m_priorityObjectTypes.end(), type) == m_priorityObjectTypes.end())
		m_priorityObjectTypes.push_back(type);
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::OnAIObjectCreated(IAIObject* pAIObject)
{
	CRY_ASSERT(pAIObject->GetAIObjectID() != INVALID_AIOBJECTID);
	
	IAIActor* pAIActor = pAIObject->CastToIAIActor();
	if (pAIActor)
	{
		CPerceptionActor actorComponent = CPerceptionActor(pAIActor);
		actorComponent.SetMeleeRange(pAIActor->GetParameters().m_fMeleeRange);
		
		m_actorComponents[pAIObject->GetAIObjectID()] = actorComponent;
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::OnAIObjectRemoved(IAIObject* pAIObject)
{
	CRY_ASSERT(pAIObject->GetAIObjectID() != INVALID_AIOBJECTID);

	IAIActor* pAIActor = pAIObject->CastToIAIActor();
	if (pAIActor)
	{
		m_actorComponents.erase(pAIObject->GetAIObjectID());
	}

	for (size_t i = 0, c = m_targetEntities.size(); i < c; ++i)
	{
		if (pAIObject == m_targetEntities[i])
		{
			m_targetEntities[i] = m_targetEntities[c - 1];
			m_targetEntities.pop_back();
			break;
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
CPerceptionActor* CPerceptionManager::GetPerceptionActor(IAIObject* pAIObject)
{
	auto it = m_actorComponents.find(pAIObject->GetAIObjectID());
	if (it == m_actorComponents.end())
	{
		CRY_ASSERT(0);
		return nullptr;
	}
	return &it->second;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::SetActorState(IAIObject* pAIObject, bool enabled)
{
	CPerceptionActor* perceptionActor = GetPerceptionActor(pAIObject);
	if (perceptionActor)
	{
		perceptionActor->Enable(enabled);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::ResetActor(IAIObject* pAIObject)
{
	CPerceptionActor* perceptionActor = GetPerceptionActor(pAIObject);
	if (perceptionActor)
	{
		perceptionActor->Reset();
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::ActorUpdate(IAIObject* pAIObject, IAIObject::EUpdateType type, float frameDelta)
{
	if (type != IAIObject::Full)
		return;
	
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	IAIActor* pAIActor = pAIObject->CastToIAIActor();
	CRY_ASSERT(pAIActor);

	m_stats.trackers[PERFTRACK_UPDATES].Inc();

	if (!pAIObject->IsEnabled())
		return;

	CPerceptionActor* perceptionActor = GetPerceptionActor(pAIObject);
	if (!perceptionActor || !perceptionActor->IsEnabled())
		return;

	perceptionActor->Update(frameDelta);

	GatherProbableTargetsForActor(pAIActor);

	const Vec3& actorPos = pAIObject->GetPos();

	{
		CRY_PROFILE_SECTION(PROFILE_AI, "AIPlayerVisibilityCheck");

		// Priority targets.
		for (IAIObject* pTarget : m_priorityTargets)
		{
			if (!pAIObject->IsHostile(pTarget))
				continue;

			if (pAIActor->IsDevalued(pTarget))
				continue;

			if (!pAIActor->CanAcquireTarget(pTarget))
				continue;

			IAIObject::EFieldOfViewResult viewResult = pAIObject->IsObjectInFOV(pTarget);
			if (IAIObject::eFOV_Outside == viewResult)
				continue;

			pTarget->SetObservable(true);

			m_stats.trackers[PERFTRACK_VIS_CHECKS].Inc();

			const Vec3& targetPos = pTarget->GetPos();

			if (!gEnv->pAISystem->RayOcclusionPlaneIntersection(actorPos, targetPos))
			{
				if (pAIActor->CanSee(pTarget->GetVisionID()))
				{
					// Notify visual perception.
					SAIEVENT event;
					event.sourceId = pAIObject->GetPerceivedEntityID();
					event.bFuzzySight = (viewResult == IAIObject::eFOV_Secondary);
					event.vPosition = targetPos;
					pAIObject->Event(AIEVENT_ONVISUALSTIMULUS, &event);
				}
			}

			switch (pTarget->GetAIType())
			{
			case AIOBJECT_PLAYER:
			case AIOBJECT_ACTOR:
			{
				if (!pTarget->IsEnabled())
					break;

				if (pAIActor->GetAttentionTarget() != pTarget)
					break;

				perceptionActor->CheckCloseContact(pTarget);
			}
			}
		}
	}

	{
		CRY_PROFILE_SECTION(PROFILE_AI, "ProbableTargets");
		// Probable targets.
		for (IAIObject* pProbTarget : m_probableTargets)
		{
			if (IAIActor* pTargetActor = pProbTarget->CastToIAIActor())
			{
				if (pTargetActor->GetParameters().m_bInvisible || pAIActor->IsDevalued(pProbTarget))
					continue;
			}

			if (!pAIActor->CanAcquireTarget(pProbTarget))
				continue;

			IAIObject::EFieldOfViewResult viewResult = pAIObject->IsObjectInFOV(pProbTarget);
			if (IAIObject::eFOV_Outside == viewResult)
				continue;

			pProbTarget->SetObservable(true);

			const Vec3& vProbTargetPos = pProbTarget->GetPos();

			bool visible = false;
			if (!gEnv->pAISystem->RayOcclusionPlaneIntersection(actorPos, vProbTargetPos))
				visible = pAIActor->CanSee(pProbTarget->GetVisionID());

			if (visible)
			{
				SAIEVENT event;
				event.sourceId = pProbTarget->GetPerceivedEntityID();
				event.bFuzzySight = (viewResult == IAIObject::eFOV_Secondary);
				event.fThreat = 1.f;
				event.vPosition = vProbTargetPos;
				pAIObject->Event(AIEVENT_ONVISUALSTIMULUS, &event);

				perceptionActor->CheckCloseContact(pProbTarget);
			}
		}
	}
};

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::GatherProbableTargetsForActor(IAIActor* pAIActor)
{
	m_probableTargets.clear();
	
	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::Position);

	IAIObject* pAIObject = pAIActor->CastToIAIObject();
	const Vec3& actorPos = pAIObject->GetPos();

	// Check against other AIs
	const size_t activeActorCount = lookUp.GetActiveCount();
	for (size_t idx = 0; idx < activeActorCount; ++idx)
	{
		IAIActor* pOther = lookUp.GetActor<IAIActor>(idx);
		IAIObject* pOtherObj = pOther->CastToIAIObject();

		if (!pAIObject->IsHostile(pOtherObj) || pOther == pAIActor)
			continue;

		const Vec3 secondPos = lookUp.GetPosition(idx);

		float distSq = Distance::Point_PointSq(actorPos, secondPos);
		if (distSq < sqr(pAIActor->GetMaxTargetVisibleRange(pOtherObj)))
			m_probableTargets.push_back(pOtherObj);
	}

	for (IAIObject* pObject : m_targetEntities)
	{
		if (!pAIObject->IsHostile(pObject))
			continue;

		float distSq = Distance::Point_PointSq(actorPos, pObject->GetPos());
		if (distSq < sqr(pAIActor->GetMaxTargetVisibleRange(pObject)))
			m_probableTargets.push_back(pObject);
	}
}

//-----------------------------------------------------------------------------------------------------------
bool CPerceptionManager::FilterStimulus(SAIStimulus* stim)
{
	const SAIStimulusTypeDesc* desc = &m_stimulusTypes[stim->type];
	const SAIStimulusFilter* filters[SAIStimulusTypeDesc::AI_MAX_FILTERS];

	// Merge and filter with current active stimuli.
	for (unsigned int i = 0; i < AI_MAX_STIMULI; ++i)
	{
		unsigned int mask = (1 << i);
		if ((mask & desc->filterTypes) == 0) continue;

		// Collect filters for this stimuli type
		unsigned int nFilters = 0;
		for (unsigned j = 0; j < desc->nFilters; ++j)
		{
			const SAIStimulusFilter* filter = &desc->filters[j];
			if ((unsigned int)filter->type != stim->type) continue;
			if (filter->subType && (unsigned int)filter->subType != stim->subType) continue;
			filters[nFilters++] = filter;
		}
		if (nFilters == 0)
			continue;

		std::vector<SStimulusRecord>& stimuli = m_stimuli[i];
		for (unsigned j = 0, nj = stimuli.size(); j < nj; ++j)
		{
			SStimulusRecord& s = stimuli[j];
			Vec3 diff = stim->pos - s.pos;
			float distSq = diff.GetLengthSquared();

			// Apply filters
			for (unsigned int k = 0; k < nFilters; ++k)
			{
				const SAIStimulusFilter* filter = filters[k];
				if (filter->subType && (filter->subType & (1 << s.subType))) continue;

				if (filter->merge == AISTIMFILTER_MERGE_AND_DISCARD)
				{
					// Merge stimuli
					// Allow to merge stimuli before they are processed.
					const float duration = desc->duration[stim->subType];
					if (distSq < sqr(s.radius * filter->scale))
					{
						if ((duration - s.t) < desc->processDelay)
						{
							float dist = sqrtf(distSq);
							// Merge spheres into a larger one.
							if (dist > 0.00001f)
							{
								diff *= 1.0f / dist;
								// Calc new radius
								float r = s.radius;
								s.radius = (dist + r + stim->radius) / 2;
								// Calc new location
								s.pos += diff * (s.radius - r);
							}
							else
							{
								// Spheres are at same location, merge radii.
								s.radius = max(s.radius, stim->radius);
							}
						}
						return true;
					}
				}
				else
				{
					// Discard stimuli
					if (distSq < sqr(s.radius * filter->scale))
						return true;
				}
			}
		}

	}

	// Not filtered nor merged, should create new stimulus.
	return false;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::Update(float deltaTime)
{
	UpdateIncomingStimuli();
	UpdateStimuli(deltaTime);

	VisCheckBroadPhase(deltaTime);	
	UpdatePriorityTargets();

	// Update stats
	m_stats.Update();
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::UpdateIncomingStimuli()
{
	// Update stats about the incoming stimulus.
	m_stats.trackers[PERFTRACK_INCOMING_STIMS].Inc(m_incomingStimuli.size());

	// Process incoming stimuli
	bool previousDiscarded = false;
	for (unsigned i = 0, ni = m_incomingStimuli.size(); i < ni; ++i)
	{
		SAIStimulus& is = m_incomingStimuli[i];
		// Check if stimulus should be discarded because it is set linked to the discard rules
		// of the previous stimulus.
		bool discardWithPrevious = previousDiscarded && (is.flags & AISTIMPROC_FILTER_LINK_WITH_PREVIOUS);
		if (!discardWithPrevious && !FilterStimulus(&is))
		{
			const SAIStimulusTypeDesc* desc = &m_stimulusTypes[is.type];
			std::vector<SStimulusRecord>& stimuli = m_stimuli[is.type];

			// Create new Stimulus
			stimuli.resize(stimuli.size() + 1);
			SStimulusRecord& stim = stimuli.back();
			stim.sourceId = is.sourceId;
			stim.targetId = is.targetId;
			stim.pos = is.pos;
			stim.dir = is.dir;
			stim.radius = is.radius;
			stim.t = desc->duration[is.subType];
			stim.type = is.type;
			stim.subType = is.subType;
			stim.flags = is.flags;
			stim.dispatched = 0;

			previousDiscarded = false;
		}
		else
		{
			previousDiscarded = true;
		}
	}
	m_incomingStimuli.clear();
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::UpdateStimuli(float deltaTime)
{
	// Update stimuli
	// Merge and filter with current active stimuli.
	for (unsigned int i = 0; i < AI_MAX_STIMULI; ++i)
	{
		std::vector<SStimulusRecord>& stims = m_stimuli[i];
		for (unsigned j = 0; j < stims.size(); )
		{
			SStimulusRecord& stim = stims[j];
			const SAIStimulusTypeDesc* desc = &m_stimulusTypes[stim.type];

			// Dispatch
			if (!stim.dispatched && stim.t < (desc->duration[stim.subType] - desc->processDelay))
			{
				float threat = 1.0f;
				switch (stim.type)
				{
				case AISTIM_SOUND:
					HandleSound(stim);
					switch (stim.subType)
					{
					case AISOUND_GENERIC:
						threat = 0.3f;
						break;
					case AISOUND_COLLISION:
						threat = 0.3f;
						break;
					case AISOUND_COLLISION_LOUD:
						threat = 0.3f;
						break;
					case AISOUND_MOVEMENT:
						threat = 0.5f;
						break;
					case AISOUND_MOVEMENT_LOUD:
						threat = 0.5f;
						break;
					case AISOUND_WEAPON:
						threat = 1.0f;
						break;
					case AISOUND_EXPLOSION:
						threat = 1.0f;
						break;
					}
					break;
				case AISTIM_COLLISION:
					HandleCollision(stim);
					threat = 0.2f;
					break;
				case AISTIM_EXPLOSION:
					HandleExplosion(stim);
					threat = 1.0f;
					break;
				case AISTIM_BULLET_WHIZZ:
					HandleBulletWhizz(stim);
					threat = 0.7f;
					break;
				case AISTIM_BULLET_HIT:
					HandleBulletHit(stim);
					threat = 1.0f;
					break;
				case AISTIM_GRENADE:
					HandleGrenade(stim);
					threat = 0.7f;
					break;
				default:
					// Invalid type
					CRY_ASSERT(0);
					break;
				}

				NotifyAIEventListeners(stim, threat);

				stim.dispatched = 1;
			}

			// Update stimulus time.
			stim.t -= deltaTime;
			if (stim.t < 0.0f)
			{
				// The stimuli has timed out, delete it.
				if (&stims[j] != &stims.back())
					stims[j] = stims.back();
				stims.pop_back();
			}
			else
			{
				// Advance
				++j;
			}
		}

		// Update stats about the stimulus.
		m_stats.trackers[PERFTRACK_STIMS].Inc(stims.size());

		// Update the ignores.
		if (!m_ignoreStimuliFrom[i].empty())
		{
			StimulusIgnoreMap::iterator it = m_ignoreStimuliFrom[i].begin();
			while (it != m_ignoreStimuliFrom[i].end())
			{
				it->second -= deltaTime;
				if (it->second <= 0.0f)
				{
					StimulusIgnoreMap::iterator del = it;
					++it;
					m_ignoreStimuliFrom[i].erase(del);
				}
				else
					++it;
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::VisCheckBroadPhase(float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::Position | IActorLookUp::EntityID);

	const float UPDATE_DELTA_TIME = 0.2f;

	m_visBroadPhaseDt += deltaTime;
	if (m_visBroadPhaseDt < UPDATE_DELTA_TIME)
		return;
	m_visBroadPhaseDt -= UPDATE_DELTA_TIME;
	if (m_visBroadPhaseDt > UPDATE_DELTA_TIME) m_visBroadPhaseDt = 0.0f;

	size_t activeActorCount = lookUp.GetActiveCount();

	if (!activeActorCount)
		return;


	m_targetEntities.clear();
	// Find player vehicles (driver inside but disabled).
	for (auto it = gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_VEHICLE); it->GetObject(); it->Next())
	{
		IAIObject* pVehicle = it->GetObject();
		if (!pVehicle->IsEnabled())
		{
			IAIActorProxy* pProxy = pVehicle->GetProxy();
			if (pProxy && pProxy->GetLinkedDriverEntityId() != 0)
			{
				m_targetEntities.push_back(pVehicle);
			}
		}
	}

	// Find target entities
	for (auto it = gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_TARGET); it->GetObject(); it->Next())
	{
		IAIObject* pTarget = it->GetObject();
		if (pTarget->IsEnabled())
		{
			m_targetEntities.push_back(pTarget);
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::UpdatePriorityTargets()
{
	// Collect list of enabled priority targets (players, grenades, projectiles, etc).
	m_priorityTargets.resize(0);

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	if (gEnv->pAISystem->GetActorLookup()->GetActiveCount() > 0)
	{
		for (unsigned i = 0, ni = m_priorityObjectTypes.size(); i < ni; ++i)
		{
			int16 type = m_priorityObjectTypes[i];
			for (auto it = gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, type); it->GetObject(); it->Next())
			{
				IAIObject* pTarget = it->GetObject();
				if (!pTarget->IsEnabled())
					continue;

				m_priorityTargets.push_back(pTarget);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::RegisterStimulus(const SAIStimulus& stim)
{
	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;
	
	// Check if we should ignore stimulus from the source.
	StimulusIgnoreMap& ignore = m_ignoreStimuliFrom[stim.type];
	if (ignore.find(stim.sourceId) != ignore.end())
		return;

	m_incomingStimuli.push_back(stim);
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::IgnoreStimulusFrom(EntityId sourceId, EAIStimulusType type, float time)
{
	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;
	
	StimulusIgnoreMap& ignore = m_ignoreStimuliFrom[(int)type];
	StimulusIgnoreMap::iterator it = ignore.find(sourceId);
	if (it != ignore.end())
		it->second = max(it->second, time);
	else
		ignore[sourceId] = time;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::HandleSound(const SStimulusRecord& stim)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::EntityID);

	IEntity* pSourceEntity = gEnv->pEntitySystem->GetEntity(stim.sourceId);
	IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(stim.targetId);

	IAIObject* pSourceObject = pSourceEntity ? pSourceEntity->GetAI() : nullptr;
	IAIActor* pSourceAI = pSourceObject ? pSourceObject->CastToIAIActor() : nullptr;

	// If this is a collision sound
	if (stim.type == AISOUND_COLLISION || stim.type == AISOUND_COLLISION_LOUD)
	{
		IAIActor* pTargetAI = pTargetEntity && pTargetEntity->GetAI() ? pTargetEntity->GetAI()->CastToIAIActor() : nullptr;

		// Do not report collision sounds between two AI
		if (pSourceAI || pTargetAI)
			return;
	}

	// perform suppression of the sound from any nearby suppressors
	float rad = SupressSound(stim.pos, stim.radius);

	EntityId throwingEntityID = 0;

	// If the sound events comes from an object thrown by the player
	auto playerIt = gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_PLAYER);
	if (IAIObject* pObj = playerIt->GetObject())
	{
		if (IAIActor* pPlayer = pObj->CastToIAIActor())
		{
			if (pPlayer->HasThrown(stim.sourceId))
			{
				// Record the thrower (to use as targetID - required for target tracking)
				throwingEntityID = pObj->GetEntityID();
			}
		}
	}

	size_t activeCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
	{
		// Do not report sounds to the sound source.
		if (lookUp.GetEntityID(actorIndex) == stim.sourceId)
			continue;
		
		IAIActor* pReceiverActor = lookUp.GetActor<IAIActor>(actorIndex);
		IAIObject* pReceiverObject = pReceiverActor->CastToIAIObject();

		CPerceptionActor* perceptionActor = GetPerceptionActor(pReceiverObject);
		if (!perceptionActor || !perceptionActor->IsEnabled())
			continue;
		
		if (pSourceAI)
		{
			const bool isHostile = pSourceObject->IsHostile(pReceiverObject);

			// Ignore impact or explosion sounds from the same species and group
			switch (stim.subType)
			{
			case AISOUND_COLLISION:
			case AISOUND_COLLISION_LOUD:
			case AISOUND_EXPLOSION:

				if (!isHostile && pReceiverObject->GetGroupId() == pSourceObject->GetGroupId())
					continue;
			}

			// No vehicle sounds for same species - destructs convoys
			if (!isHostile && pSourceObject->GetAIType() == AIOBJECT_VEHICLE)
				continue;
		}

		if (IsSoundOccluded(pReceiverActor, stim.pos))
			continue;

		// Send event.
		SAIEVENT event;
		event.vPosition = stim.pos;
		event.fThreat = rad;
		event.nType = (int)stim.subType;
		event.nFlags = stim.flags;
		event.sourceId = throwingEntityID ? throwingEntityID : (pSourceObject ? pSourceObject->GetPerceivedEntityID() : stim.sourceId);
		pReceiverObject->Event(AIEVENT_ONSOUNDEVENT, &event);

		RecordStimulusEvent(stim, event.fThreat, *pReceiverObject);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::HandleCollision(const SStimulusRecord& stim)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::Position);

	IEntity* pCollider = gEnv->pEntitySystem->GetEntity(stim.sourceId);
	IEntity* pTarget = gEnv->pEntitySystem->GetEntity(stim.targetId);

	IAIObject* pColliderObject = pCollider ? pCollider->GetAI() : nullptr;
	IAIObject* pTargetObject = pTarget ? pTarget->GetAI() : nullptr;

	IAIActor* pColliderActor = pColliderObject ? pColliderObject->CastToIAIActor() : nullptr;
	IAIActor* pTargetActor = pTargetObject ? pTargetObject->CastToIAIActor() : nullptr;

	// Do not report AI collisions
	if (pColliderActor || pTargetActor)
		return;

	bool thrownByPlayer = false;
	if (pCollider)
	{
		auto playerIt = gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_PLAYER);
		if (IAIObject* pObj = playerIt->GetObject())
		{
			if (IAIActor* pPlayer = pObj->CastToIAIActor())
			{
				thrownByPlayer = pPlayer->HasThrown(pCollider->GetId());
			}
		}
	}

	// Allow to react to larger objects which collide nearby.
	size_t activeCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
	{
		IAIActor* pReceiverActor = lookUp.GetActor<IAIActor>(actorIndex);
		IAIObject* pReceiverObject = pReceiverActor->CastToIAIObject();

		const float scale = pReceiverActor->GetParameters().m_PerceptionParams.collisionReactionScale;
		const float rangeSq = sqr(stim.radius * scale);

		float distSq = Distance::Point_PointSq(stim.pos, lookUp.GetPosition(actorIndex));
		if (distSq > rangeSq)
			continue;

		CPerceptionActor* perceptionActor = GetPerceptionActor(pReceiverObject);
		if (!perceptionActor || !perceptionActor->IsEnabled())
			continue;

		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->iValue = thrownByPlayer ? 1 : 0;
		pData->fValue = sqrtf(distSq);
		pData->point = stim.pos;
		pReceiverActor->SetSignal(0, "OnCloseCollision", 0, pData);

		if (thrownByPlayer)
		{
			IPuppet* pReceiverPuppet = pReceiverObject->CastToIPuppet();
			if (pReceiverPuppet)
			{
				pReceiverPuppet->SetAlarmed();
			}
		}
		RecordStimulusEvent(stim, 0.0f, *pReceiverObject);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::HandleExplosion(const SStimulusRecord& stim)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::Position);

	// React to explosions.
	size_t activeCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
	{
		IAIActor* pReceiverActor = lookUp.GetActor<IAIActor>(actorIndex);
		IAIObject* pReceiverObject = pReceiverActor->CastToIAIObject();

		const float scale = pReceiverActor->GetParameters().m_PerceptionParams.collisionReactionScale;
		const float rangeSq = sqr(stim.radius * scale);

		float distSq = Distance::Point_PointSq(stim.pos, lookUp.GetPosition(actorIndex));
		if (distSq > rangeSq)
			continue;

		CPerceptionActor* perceptionActor = GetPerceptionActor(pReceiverObject);
		if (!perceptionActor || !perceptionActor->IsEnabled())
			continue;
		
		if (stim.flags & AISTIMPROC_ONLY_IF_VISIBLE)
		{
			if (!IsStimulusVisible(stim, pReceiverObject))
				continue;
		}

		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->point = stim.pos;

		SetLastExplosionPosition(stim.pos, pReceiverObject);
		pReceiverActor->SetSignal(0, "OnExposedToExplosion", 0, pData);

		if (IPuppet* pReceiverPuppet = pReceiverObject->CastToIPuppet())
			pReceiverPuppet->SetAlarmed();

		RecordStimulusEvent(stim, 0.0f, *pReceiverObject);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::HandleBulletHit(const SStimulusRecord& stim)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::Position | IActorLookUp::EntityID);

	IEntity* pShooterEnt = gEnv->pEntitySystem->GetEntity(stim.sourceId);
	IAIObject* pShooterObject = pShooterEnt ? pShooterEnt->GetAI() : nullptr;
	IAIActor* pShooterActor = pShooterObject ? pShooterObject->CastToIAIActor() : nullptr;

	// Send bullet events
	size_t activeCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
	{
		IAIActor* pReceiverActor = lookUp.GetActor<IAIActor>(actorIndex);
		IAIObject* pReceiverObject = pReceiverActor->CastToIAIObject();

		// Skip own fire.
		if (stim.sourceId == lookUp.GetEntityID(actorIndex))
			continue;

		CPerceptionActor* perceptionActor = GetPerceptionActor(pReceiverObject);
		if (!perceptionActor || !perceptionActor->IsEnabled())
			continue;

		// Skip non-hostile bullets.
		if (!pReceiverObject->IsHostile(pShooterObject))
			continue;

		AABB bounds;
		pReceiverObject->GetEntity()->GetWorldBounds(bounds);
		const float d = Distance::Point_AABBSq(stim.pos, bounds);
		const float r = max(stim.radius, pReceiverActor->GetParameters().m_PerceptionParams.bulletHitRadius);

		if (d < sqr(r))
		{
			// Send event.
			SAIEVENT event;
			if (pShooterObject)
			{
				event.sourceId = pShooterObject->GetPerceivedEntityID();
				event.vPosition = pShooterObject->GetFirePos();
			}
			else if (pShooterEnt)
			{
				event.sourceId = pShooterEnt->GetId();
				event.vPosition = pShooterEnt->GetWorldPos();
			}
			else
			{
				event.sourceId = 0;
				event.vPosition = stim.pos;
			}
			event.vStimPos = stim.pos;
			event.nFlags = stim.flags;
			event.fThreat = 1.0f; // pressureMultiplier

			IPuppet* pActorPuppet = pShooterObject->CastToIPuppet();
			if (pActorPuppet)
				event.fThreat = pActorPuppet->GetCurrentWeaponDescriptor().pressureMultiplier;

			pReceiverObject->Event(AIEVENT_ONBULLETRAIN, &event);

			RecordStimulusEvent(stim, 0.0f, *pReceiverObject);
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::HandleBulletWhizz(const SStimulusRecord& stim)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::Position | IActorLookUp::EntityID);

	IEntity* pShooterEnt = gEnv->pEntitySystem->GetEntity(stim.sourceId);
	IAIObject* pShooterAI = pShooterEnt ? pShooterEnt->GetAI() : nullptr;

	Lineseg lof(stim.pos, stim.pos + stim.dir * stim.radius);

	size_t activeCount = lookUp.GetActiveCount();
	for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
	{
		IAIActor* pReceiverActor = lookUp.GetActor<IAIActor>(actorIndex);
		IAIObject* pReceiverObject = pReceiverActor->CastToIAIObject();

		// Skip own fire.
		if (lookUp.GetEntityID(actorIndex) == stim.sourceId)
			continue;

		CPerceptionActor* perceptionActor = GetPerceptionActor(pReceiverObject);
		if (!perceptionActor || !perceptionActor->IsEnabled())
			continue;

		// Skip non-hostile bullets.
		if (!pReceiverObject->IsHostile(pShooterAI))
			continue;

		float t;
		const float r = 5.0f;
		if (Distance::Point_LinesegSq(lookUp.GetPosition(actorIndex), lof, t) < sqr(r))
		{
			const Vec3 hitPos = lof.GetPoint(t);
			SAIStimulus hitStim(AISTIM_BULLET_HIT, 0, stim.sourceId, 0, hitPos, ZERO, r);
			RegisterStimulus(hitStim);
		}
		RecordStimulusEvent(stim, 0.0f, *pReceiverObject);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::HandleGrenade(const SStimulusRecord& stim)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	EntityId shooterId = stim.sourceId;

	IEntity* pShooter = gEnv->pEntitySystem->GetEntity(shooterId);
	if (!pShooter)
		return;

	CRY_ASSERT(gEnv->pAISystem->GetActorLookup());
	IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
	lookUp.Prepare(IActorLookUp::Position | IActorLookUp::EntityID);

	IAIObject* pShooterObject = pShooter->GetAI();

	switch (stim.subType)
	{
	case AIGRENADE_THROWN:
		{
			float radSq = sqr(stim.radius);
			Vec3 throwPos = pShooterObject ? pShooterObject->GetFirePos() : stim.pos; // Grenade position
			// Inform the AI that sees the throw
			size_t activeCount = lookUp.GetActiveCount();

			for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
			{
				if (lookUp.GetEntityID(actorIndex) == shooterId)
					continue;

				IAIObject* pAIObject = lookUp.GetActor<IAIObject>(actorIndex);

				CPerceptionActor* perceptionActor = GetPerceptionActor(pAIObject);
				if (!perceptionActor || !perceptionActor->IsEnabled())
					continue;

				// Inform enemies only.
				if(!pShooterObject || !pShooterObject->IsHostile(pAIObject))
					continue;

				const Vec3 vAIActorPos = lookUp.GetPosition(actorIndex);

				// If the puppet is not close to the predicted position, skip.
				if (Distance::Point_PointSq(stim.pos, vAIActorPos) > radSq)
					continue;

				// Only sense grenades that are on front of the AI and visible when thrown.
				// Another signal is sent when the grenade hits the ground.
				Vec3 delta = throwPos - vAIActorPos;  // grenade to AI
				float dist = delta.NormalizeSafe();
				const float thr = cosf(DEG2RAD(160.0f));
				if (delta.Dot(pAIObject->GetViewDir()) > thr)
				{
					static const int objTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;
					static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
					
					const RayCastResult& result = gEnv->pAISystem->GetGlobalRaycaster()->Cast(RayCastRequest(vAIActorPos, delta * dist, objTypes, flags));
					if (result)
						throwPos = result[0].pt;

					gEnv->pAISystem->AddDebugLine(vAIActorPos, throwPos, 255, 0, 0, 15.0f);

					if (!result || result[0].dist > dist * 0.9f)
					{
						IAISignalExtraData* pEData = gEnv->pAISystem->CreateSignalExtraData(); // no leak - this will be deleted inside SendAnonymousSignal
						pEData->point = stim.pos;                                            // avoid predicted pos
						pEData->nID = shooterId;
						pEData->iValue = 1;

						SetLastExplosionPosition(stim.pos, pAIObject);
						gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnGrenadeDanger", pAIObject, pEData);

						RecordStimulusEvent(stim, 0.0f, *pAIObject);
					}
				}
			}
		}
		break;
	case AIGRENADE_COLLISION:
		{
			float radSq = sqr(stim.radius);

			size_t activeCount = lookUp.GetActiveCount();

			for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
			{
				// If the puppet is not close to the grenade, skip.
				if (Distance::Point_PointSq(stim.pos, lookUp.GetPosition(actorIndex)) > radSq)
					continue;

				IAIObject* pAIObject = lookUp.GetActor<IAIObject>(actorIndex);

				IAISignalExtraData* pEData = gEnv->pAISystem->CreateSignalExtraData();  // no leak - this will be deleted inside SendAnonymousSignal
				pEData->point = stim.pos;
				pEData->nID = shooterId;
				pEData->iValue = 2;

				SetLastExplosionPosition(stim.pos, pAIObject);
				gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnGrenadeDanger", pAIObject, pEData);

				RecordStimulusEvent(stim, 0.0f, *pAIObject);
			}
		}
		break;
	case AIGRENADE_FLASH_BANG:
		{
			float radSq = sqr(stim.radius);

			size_t activeCount = lookUp.GetActiveCount();

			for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
			{
				if (lookUp.GetEntityID(actorIndex) == shooterId)
					continue;

				const Vec3 vAIActorPos = lookUp.GetPosition(actorIndex);

				// If AI Actor is not close to the flash, skip.
				if (Distance::Point_PointSq(stim.pos, vAIActorPos) > radSq)
					continue;

				IAIObject* pAIObject = lookUp.GetActor<IAIObject>(actorIndex);

				// Only sense grenades that are on front of the AI and visible when thrown.
				// Another signal is sent when the grenade hits the ground.
				Vec3 delta = stim.pos - vAIActorPos;  // grenade to AI
				float dist = delta.NormalizeSafe();
				const float thr = cosf(DEG2RAD(160.0f));
				if (delta.Dot(pAIObject->GetViewDir()) > thr)
				{
					static const int objTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;
					static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;

					if (!gEnv->pAISystem->GetGlobalRaycaster()->Cast(RayCastRequest(vAIActorPos, delta * dist, objTypes, flags)))
					{
						IAISignalExtraData* pExtraData = gEnv->pAISystem->CreateSignalExtraData();
						pExtraData->iValue = 0;
						gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnExposedToFlashBang", pAIObject, pExtraData);

						RecordStimulusEvent(stim, 0.0f, *pAIObject);
					}
				}
			}
		}
		break;
	case AIGRENADE_SMOKE:
		{
			float radSq = sqr(stim.radius);
			size_t activeCount = lookUp.GetActiveCount();

			for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
			{
				IAIObject* pAIObject = lookUp.GetActor<IAIObject>(actorIndex);

				// If the puppet is not close to the smoke, skip.
				if (Distance::Point_PointSq(stim.pos, lookUp.GetPosition(actorIndex)) > radSq)
					continue;

				gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnExposedToSmoke", pAIObject);

				RecordStimulusEvent(stim, 0.0f, *pAIObject);
			}
		}
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::SetLastExplosionPosition(const Vec3& position, IAIObject* pAIObject) const
{
	CRY_ASSERT(pAIObject);
	if (pAIObject)
	{
		IScriptTable* pActorScriptTable = pAIObject->GetEntity()->GetScriptTable();
		Vec3 tempPos(ZERO);
		if (pActorScriptTable->GetValue("lastExplosiveThreatPos", tempPos))
		{
			pActorScriptTable->SetValue("lastExplosiveThreatPos", position);
		}
	}

}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::RegisterAIStimulusEventListener(const EventCallback& eventCallback, const SAIStimulusEventListenerParams& params)
{
	m_eventListeners[eventCallback] = params;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::UnregisterAIStimulusEventListener(const EventCallback& eventCallback)
{ 
	m_eventListeners.erase(eventCallback);
}

//-----------------------------------------------------------------------------------------------------------
bool CPerceptionManager::IsPointInRadiusOfStimulus(EAIStimulusType type, const Vec3& pos) const
{
	const std::vector<SStimulusRecord>& stims = m_stimuli[(int)type];
	for (unsigned i = 0, ni = stims.size(); i < ni; ++i)
	{
		const SStimulusRecord& s = stims[i];
		if (Distance::Point_PointSq(s.pos, pos) < sqr(s.radius))
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::NotifyAIEventListeners(const SStimulusRecord& stim, float threat)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	int flags = 1 << (int)stim.type;

	for (auto it = m_eventListeners.begin(); it != m_eventListeners.end(); ++it)
	{
		const SAIStimulusEventListenerParams& listener = it->second;
		if (listener.flags & flags)
		{
			if (Distance::Point_PointSq(stim.pos, listener.pos) < sqr(listener.radius + stim.radius))
			{
				SAIStimulusParams params;
				params.type = (EAIStimulusType)stim.type;
				params.sender = stim.sourceId;
				params.position = stim.pos;
				params.radius = stim.radius;
				params.threat = threat;
				it->first(params);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
float CPerceptionManager::SupressSound(const Vec3& pos, float radius)
{
	float minScale = 1.0f;

	for (auto it = gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_SNDSUPRESSOR); it->GetObject(); it->Next())
	{
		IAIObject* pObject = it->GetObject();
		if (!pObject->IsEnabled())
			continue;

		const float r = pObject->GetRadius();
		const float silenceRadius = r * 0.3f;
		const float distSqr = Distance::Point_PointSq(pos, pObject->GetPos());

		if (distSqr > sqr(r))
			continue;

		if (distSqr < sqr(silenceRadius))
			return 0.0f;

		const float dist = sqrtf(distSqr);
		const float scale = (dist - silenceRadius) / (r - silenceRadius);
		minScale = min(minScale, scale);
	}
	return radius * minScale;
}

//-----------------------------------------------------------------------------------------------------------
bool CPerceptionManager::IsSoundOccluded(IAIActor* pAIActor, const Vec3& vSoundPos)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);

	// The sound is occluded because of the buildings.
	// But not now yet
	return false;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::Serialize(TSerialize ser)
{
	if (ser.IsReading())
	{
		m_probableTargets.clear();
		m_targetEntities.clear();
		m_priorityTargets.clear();
		m_actorComponents.clear();
	}
	
	ser.BeginGroup("PerceptionManager");
	for (unsigned i = 0; i < AI_MAX_STIMULI; ++i)
	{
		char name[64];
		cry_sprintf(name, "m_stimuli%02d", i);
		ser.Value(name, m_stimuli[i]);
		cry_sprintf(name, "m_ignoreStimuliFrom%02d", i);
		ser.Value(name, m_ignoreStimuliFrom[i]);
	}

	ser.Value("m_priorityObjectTypes", m_priorityObjectTypes);

	//ser.Value("ActorComponents", m_actorComponents);

	{
		ser.BeginGroup("ActorComponents");

		uint32 actorsCount = (uint32)m_actorComponents.size();
		ser.Value("Count", actorsCount);

		if (ser.IsReading())
		{
			tAIObjectID objID;
			for (uint32 i = 0; i < actorsCount; ++i)
			{
				ser.BeginGroup("Entry");
				ser.Value("Id", objID);

				IAIObject* pAIObject = gEnv->pAISystem->GetAIObjectManager()->GetAIObject(objID);
				if (pAIObject)
				{
					IAIActor* pAIActor = pAIObject->CastToIAIActor();
					if (pAIActor)
					{
						CPerceptionActor actorComponent(pAIActor);
						actorComponent.Serialize(ser);
						
						m_actorComponents[objID] = actorComponent;
					}
				}
				ser.EndGroup();
			}
		}
		else
		{
			for (auto& actorIt : m_actorComponents)
			{
				ser.BeginGroup("Entry");
				tAIObjectID id = actorIt.first;
				ser.Value("Id", id);
				actorIt.second.Serialize(ser);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}

	ser.EndGroup();
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::RecordStimulusEvent(const SStimulusRecord& stim, float fRadius, IAIObject& receiver) const
{
#ifdef CRYAISYSTEM_DEBUG
	stack_string sType = g_szAIStimulusType[stim.type];
	if (stim.type == AISTIM_SOUND)
		sType += g_szAISoundStimType[stim.subType];
	else if (stim.type == AISTIM_GRENADE)
		sType += g_szAIGrenadeStimType[stim.subType];

	IEntity* pSource = gEnv->pEntitySystem->GetEntity(stim.sourceId);
	IEntity* pTarget = gEnv->pEntitySystem->GetEntity(stim.targetId);
	stack_string sDebugLine;
	sDebugLine.Format("%s from %s to %s", sType.c_str(), pSource ? pSource->GetName() : "Unknown", pTarget ? pTarget->GetName() : "All");

	// Record event
	if (gEnv->pAISystem->IsRecording(&receiver, IAIRecordable::E_REGISTERSTIMULUS))
	{
		gEnv->pAISystem->Record(&receiver, IAIRecordable::E_REGISTERSTIMULUS, sDebugLine.c_str());
	}

	IAIRecordable::RecorderEventData recorderEventData(sDebugLine.c_str());
	receiver.RecordEvent(IAIRecordable::E_REGISTERSTIMULUS, &recorderEventData);
#endif
}

//-----------------------------------------------------------------------------------------------------------
bool CPerceptionManager::IsStimulusVisible(const SStimulusRecord& stim, const IAIObject* pAIObject)
{
	const Vec3 stimPos = stim.pos;
	const Vec3& vAIActorPos = pAIObject->GetPos();
	Vec3 delta = stimPos - vAIActorPos;
	const float dist = delta.NormalizeSafe();
	const float thr = cosf(DEG2RAD(110.0f));
	if (delta.dot(pAIObject->GetViewDir()) > thr)
	{
		static const int objTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;
		static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
		const RayCastResult& result = gEnv->pAISystem->GetGlobalRaycaster()->Cast(RayCastRequest(vAIActorPos, delta * dist, objTypes, flags));

		gEnv->pAISystem->AddDebugLine(vAIActorPos, result ? result[0].pt : stimPos, 255, 0, 0, 15.0f);

		if (!result || result[0].dist > dist * 0.9f)
		{
			return true;
		}
	}
	return false;
}

void CPerceptionManager::CVars::Init()
{
	DefineConstIntCVarName("ai_DebugPerceptionManager", DebugPerceptionManager, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws perception manager performance overlay. 0=disable, 1=vis checks, 2=stimulus");

	DefineConstIntCVarName("ai_DrawCollisionEvents", DrawCollisionEvents, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debug draw the collision events the AI system processes. 0=disable, 1=enable.");
	DefineConstIntCVarName("ai_DrawBulletEvents", DrawBulletEvents, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debug draw the bullet events the AI system processes. 0=disable, 1=enable.");
	DefineConstIntCVarName("ai_DrawSoundEvents", DrawSoundEvents, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debug draw the sound events the AI system processes. 0=disable, 1=enable.");
	DefineConstIntCVarName("ai_DrawGrenadeEvents", DrawGrenadeEvents, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debug draw the grenade events the AI system processes. 0=disable, 1=enable.");
	DefineConstIntCVarName("ai_DrawExplosions", DrawExplosions, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debug draw the explosion events the AI system processes. 0=disable, 1=enable.");
}

#include <CryAISystem/IAIDebugRenderer.h>

//===================================================================
// Helper functions for DebugDraw
//===================================================================
ILINE int bit(unsigned b, unsigned x)
{
	return (x >> b) & 1;
}

//-----------------------------------------------------------------------------------------------------------
ILINE ColorB GetColorFromId(unsigned x)
{
	unsigned r = (bit(0, x) << 7) | (bit(4, x) << 5) | (bit(7, x) << 3);
	unsigned g = (bit(1, x) << 7) | (bit(5, x) << 5) | (bit(8, x) << 3);
	unsigned b = (bit(2, x) << 7) | (bit(6, x) << 5) | (bit(9, x) << 3);
	return ColorB(255 - r, 255 - g, 255 - b);
}

//-----------------------------------------------------------------------------------------------------------
ILINE size_t HashFromFloat(float key, float tol, float invTol)
{
	float val = key;

	if (tol > 0.0f)
	{
		val *= invTol;
		val = floor_tpl(val);
		val *= tol;
	}

	union f32_u
	{
		float  floatVal;
		size_t uintVal;
	};

	f32_u u;
	u.floatVal = val;

	size_t hash = u.uintVal;
	hash += ~(hash << 15);
	hash ^= (hash >> 10);
	hash += (hash << 3);
	hash ^= (hash >> 6);
	hash += ~(hash << 11);
	hash ^= (hash >> 16);

	return hash;
}

//-----------------------------------------------------------------------------------------------------------
inline size_t HashFromVec3(const Vec3& v, float tol, float invTol)
{
	return HashFromFloat(v.x, tol, invTol) + HashFromFloat(v.y, tol, invTol) + HashFromFloat(v.z, tol, invTol);
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::DebugDraw(IAIDebugRenderer* pDebugRenderer)
{
	if (m_cVars.DebugPerceptionManager > 0)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		DebugDrawPerception(pDebugRenderer, m_cVars.DebugPerceptionManager);
		DebugDrawPerformance(pDebugRenderer, m_cVars.DebugPerceptionManager);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::DebugDrawPerception(IAIDebugRenderer* pDebugRenderer, int mode)
{
	IAIActor* pPlayer = nullptr;
	auto playerIt = gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_PLAYER);
	if (IAIObject* pObj = playerIt->GetObject())
	{
		pPlayer = pObj->CastToIAIActor();
	}
	
	if (!pPlayer)
		return;

	typedef std::map<unsigned, unsigned> OccMap;
	OccMap occupied;

	for (unsigned i = 0; i < AI_MAX_STIMULI; ++i)
	{
		switch (i)
		{
		case AISTIM_SOUND:
			if (m_cVars.DrawSoundEvents == 0) continue;
			break;
		case AISTIM_COLLISION:
			if (m_cVars.DrawCollisionEvents == 0) continue;
			break;
		case AISTIM_EXPLOSION:
			if (m_cVars.DrawExplosions == 0) continue;
			break;
		case AISTIM_BULLET_WHIZZ:
		case AISTIM_BULLET_HIT:
			if (m_cVars.DrawBulletEvents == 0) continue;
			break;
		case AISTIM_GRENADE:
			if (m_cVars.DrawGrenadeEvents == 0) continue;
			break;
		}

		const SAIStimulusTypeDesc* desc = &m_stimulusTypes[i];
		std::vector<SStimulusRecord>& stimuli = m_stimuli[i];
		for (unsigned j = 0, nj = stimuli.size(); j < nj; ++j)
		{
			SStimulusRecord& s = stimuli[j];

			CRY_ASSERT(s.subType < SAIStimulusTypeDesc::AI_MAX_SUBTYPES);
			float tmax = desc->duration[s.subType];
			float a = clamp_tpl((s.t - tmax / 2) / (tmax / 2), 0.0f, 1.0f);

			int row = 0;
			unsigned hash = HashFromVec3(s.pos, 0.1f, 1.0f / 0.1f);
			OccMap::iterator it = occupied.find(hash);
			if (it != occupied.end())
			{
				row = it->second;
				it->second++;
			}
			else
			{
				occupied[hash] = 1;
			}
			if (row > 5) row = 5;

			bool thrownByPlayer = pPlayer ? pPlayer->HasThrown(s.sourceId) : false;

			ColorB color = thrownByPlayer ? ColorB(240, 16, 0) : GetColorFromId(i);
			ColorB colorTrans(color);
			color.a = 10 + (uint8)(240 * a);
			colorTrans.a = 10 + (uint8)(128 * a);

			char rowTxt[] = "\n\n\n\n\n\n\n";
			rowTxt[row] = '\0';

			char subTypeTxt[128] = "";
			switch (s.type)
			{
			case AISTIM_SOUND:
				switch (s.subType)
				{
				case AISOUND_GENERIC:
					cry_sprintf(subTypeTxt, "GENERIC  R=%.1f", s.radius);
					break;
				case AISOUND_COLLISION:
					cry_sprintf(subTypeTxt, "COLLISION  R=%.1f", s.radius);
					break;
				case AISOUND_COLLISION_LOUD:
					cry_sprintf(subTypeTxt, "COLLISION LOUD  R=%.1f", s.radius);
					break;
				case AISOUND_MOVEMENT:
					cry_sprintf(subTypeTxt, "MOVEMENT  R=%.1f", s.radius);
					break;
				case AISOUND_MOVEMENT_LOUD:
					cry_sprintf(subTypeTxt, "MOVEMENT LOUD  R=%.1f", s.radius);
					break;
				case AISOUND_WEAPON:
					cry_sprintf(subTypeTxt, "WEAPON\nR=%.1f", s.radius);
					break;
				case AISOUND_EXPLOSION:
					cry_sprintf(subTypeTxt, "EXPLOSION  R=%.1f", s.radius);
					break;
				}
				break;
			case AISTIM_COLLISION:
				switch (s.subType)
				{
				case AICOL_SMALL:
					cry_sprintf(subTypeTxt, "SMALL  R=%.1f", s.radius);
					break;
				case AICOL_MEDIUM:
					cry_sprintf(subTypeTxt, "MEDIUM  R=%.1f", s.radius);
					break;
				case AICOL_LARGE:
					cry_sprintf(subTypeTxt, "LARGE  R=%.1f", s.radius);
					break;
				}
				;
				break;
			case AISTIM_EXPLOSION:
			case AISTIM_BULLET_WHIZZ:
			case AISTIM_BULLET_HIT:
			case AISTIM_GRENADE:
				cry_sprintf(subTypeTxt, "R=%.1f", s.radius);
				break;
			default:
				break;
			}

			Vec3 ext(0, 0, s.radius);
			if (s.dir.GetLengthSquared() > 0.1f)
				ext = s.dir * s.radius;

			pDebugRenderer->DrawSphere(s.pos, 0.15f, colorTrans);
			pDebugRenderer->DrawLine(s.pos, color, s.pos + ext, color);
			pDebugRenderer->DrawWireSphere(s.pos, s.radius, color);
			pDebugRenderer->Draw3dLabel(s.pos, 1.1f, "%s%s  %s", rowTxt, desc->name, subTypeTxt);
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionManager::DebugDrawPerformance(IAIDebugRenderer* pDebugRenderer, int mode)
{
	ColorB white(255, 255, 255);

	static std::vector<Vec3> points;

	if (mode == 1)
	{
		// Draw visibility performance

		const int visChecks = m_stats.trackers[PERFTRACK_VIS_CHECKS].GetCount();
		const int visChecksMax = m_stats.trackers[PERFTRACK_VIS_CHECKS].GetCountMax();
		const int updates = m_stats.trackers[PERFTRACK_UPDATES].GetCount();
		const int updatesMax = m_stats.trackers[PERFTRACK_UPDATES].GetCountMax();

		pDebugRenderer->Draw2dLabel(50, 200, 2, white, false, "Updates:");
		pDebugRenderer->Draw2dLabel(175, 200, 2, white, false, "%d", updates);
		pDebugRenderer->Draw2dLabel(215, 200 + 5, 1, white, false, "max:%d", updatesMax);

		pDebugRenderer->Draw2dLabel(50, 225, 2, white, false, "Vis checks:");
		pDebugRenderer->Draw2dLabel(175, 225, 2, white, false, "%d", visChecks);
		pDebugRenderer->Draw2dLabel(215, 225 + 5, 1, white, false, "max:%d", visChecksMax);

		{
			pDebugRenderer->Init2DMode();
			pDebugRenderer->SetAlphaBlended(true);
			pDebugRenderer->SetBackFaceCulling(false);
			pDebugRenderer->SetDepthTest(false);

			float rw = (float)pDebugRenderer->GetWidth();
			float rh = (float)pDebugRenderer->GetHeight();
			//		float	as = rh/rw;
			float scale = 1.0f / rh;

			// Divider lines every 10 units.
			for (unsigned i = 0; i <= 10; ++i)
			{
				int v = i * 10;
				pDebugRenderer->DrawLine(Vec3(0.1f, 0.9f - v * scale, 0), ColorB(255, 255, 255, 128), Vec3(0.9f, 0.9f - v * scale, 0), ColorB(255, 255, 255, 128));
				pDebugRenderer->Draw2dLabel(0.1f * rw - 20, rh * 0.9f - v * scale * rh - 6, 1.0f, white, false, "%d", i * 10);
			}

			int idx[2] = { PERFTRACK_UPDATES, PERFTRACK_VIS_CHECKS };
			ColorB colors[3] = { ColorB(255, 0, 0), ColorB(0, 196, 255), ColorB(255, 255, 255) };

			for (int i = 0; i < 2; ++i)
			{
				const CValueHistory<int>& hist = m_stats.trackers[idx[i]].GetHist();
				unsigned n = hist.GetSampleCount();
				if (n < 2) continue;

				points.resize(n);

				for (unsigned j = 0; j < n; ++j)
				{
					float t = (float)j / (float)hist.GetMaxSampleCount();
					points[j].x = 0.1f + t * 0.8f;
					points[j].y = 0.9f - hist.GetSample(j) * scale;
					points[j].z = 0;
				}
				pDebugRenderer->DrawPolyline(&points[0], n, false, colors[i]);
			}
		}

		IActorLookUp& lookUp = *gEnv->pAISystem->GetActorLookup();
		size_t activeActorCount = lookUp.GetActiveCount();

		for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
		{
			IAIActor* pAIActor = lookUp.GetActor<IAIActor>(actorIndex);
			pDebugRenderer->DrawSphere(lookUp.GetPosition(actorIndex), 1.0f, ColorB(255, 0, 0));
		}
	}
	else if (mode == 2)
	{
		// Draw stims performance

		const int incoming = m_stats.trackers[PERFTRACK_INCOMING_STIMS].GetCount();
		const int incomingMax = m_stats.trackers[PERFTRACK_INCOMING_STIMS].GetCountMax();
		const int stims = m_stats.trackers[PERFTRACK_STIMS].GetCount();
		const int stimsMax = m_stats.trackers[PERFTRACK_STIMS].GetCountMax();

		pDebugRenderer->Draw2dLabel(50, 200, 2, white, false, "Incoming:");
		pDebugRenderer->Draw2dLabel(175, 200, 2, white, false, "%d", incoming);
		pDebugRenderer->Draw2dLabel(215, 200 + 5, 1, white, false, "max:%d", incomingMax);

		pDebugRenderer->Draw2dLabel(50, 225, 2, white, false, "Stims:");
		pDebugRenderer->Draw2dLabel(175, 225, 2, white, false, "%d", stims);
		pDebugRenderer->Draw2dLabel(215, 225 + 5, 1, white, false, "max:%d", stimsMax);

		pDebugRenderer->Init2DMode();
		pDebugRenderer->SetAlphaBlended(true);
		pDebugRenderer->SetBackFaceCulling(false);
		pDebugRenderer->SetDepthTest(false);

		float rw = (float)pDebugRenderer->GetWidth();
		float rh = (float)pDebugRenderer->GetHeight();
		//		float	as = rh/rw;
		float scale = 1.0f / rh;

		// Divider lines every 10 units.
		for (unsigned i = 0; i <= 10; ++i)
		{
			int v = i * 10;
			pDebugRenderer->DrawLine(Vec3(0.1f, 0.9f - v * scale, 0), ColorB(255, 255, 255, 128), Vec3(0.9f, 0.9f - v * scale, 0), ColorB(255, 255, 255, 128));
			pDebugRenderer->Draw2dLabel(0.1f * rw - 20, rh * 0.9f - v * scale * rh - 6, 1.0f, white, false, "%d", i * 10);
		}

		int idx[2] = { PERFTRACK_INCOMING_STIMS, PERFTRACK_STIMS };
		ColorB colors[2] = { ColorB(0, 196, 255), ColorB(255, 255, 255) };

		for (int i = 0; i < 2; ++i)
		{
			const CValueHistory<int>& hist = m_stats.trackers[idx[i]].GetHist();
			unsigned n = hist.GetSampleCount();
			if (n < 2) continue;

			points.resize(n);

			for (unsigned j = 0; j < n; ++j)
			{
				float t = (float)j / (float)hist.GetMaxSampleCount();
				points[j].x = 0.1f + t * 0.8f;
				points[j].y = 0.9f - hist.GetSample(j) * scale;
				points[j].z = 0;
			}
			pDebugRenderer->DrawPolyline(&points[0], n, false, colors[i]);
		}
	}
}
