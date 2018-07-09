// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Contains and updates all target tracks owned by agents used
        for target selection

   -------------------------------------------------------------------------
   History:
   - 02:01:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "TargetTrackManager.h"
#include "TargetTrackGroup.h"
#include "TargetTrack.h"
#include "TargetTrackModifiers.h"
#include "ObjectContainer.h"

#include <CryAISystem/IAgent.h>
#include "Puppet.h"
#include <CryString/StringUtils.h>

#ifdef TARGET_TRACK_DEBUG
	#include "DebugDrawContext.h"
#endif //TARGET_TRACK_DEBUG

//#pragma optimize("", off)
//#pragma inline_depth(0)

static const uint32 g_uTargetStimulusConfig_Version = 1;
static const char* g_szTargetStimulusConfig_XmlPath = "Libs/AITargetStimulusConfig.xml";
static const uint32 g_uTargetTracksPoolListSize = 16;

namespace TargetTrackHelpers
{
const char* GetSoundStimulusNameFromType(const int stimulusType)
{
	static const char* s_soundStimulusNames[AISOUND_LAST] =
	{
		"SoundGeneric",       // AISOUND_GENERIC
		"SoundCollision",     // AISOUND_COLLISION
		"SoundCollisionLoud", // AISOUND_COLLISION_LOUD
		"SoundMovement",      // AISOUND_MOVEMENT
		"SoundMovementLoud",  // AISOUND_MOVEMENT_LOUD
		"SoundWeapon",        // AISOUND_WEAPON
		"SoundExplosion",     // AISOUND_EXPLOSION
	};

	if (stimulusType >= 0 && stimulusType < AISOUND_LAST)
	{
		return s_soundStimulusNames[stimulusType];
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CTargetTrackManager::TranslateSoundStimulus Unhandled sound stimulus type");
		return "Unknown";
	}
};

//////////////////////////////////////////////////////////////////////////
void ApplyAudioPerceptionScalingParameters(const CPuppet* pPuppet, float& fSoundThreatLevel)
{
	// Attenuate by perception parameter scaling.
	const AgentParameters& parameters = pPuppet->GetParameters();
	const float fGlobalAudioPerceptionScale = gEnv->pAISystem->GetGlobalAudioScale(pPuppet);
	const float complessiveSoundScale = parameters.m_PerceptionParams.audioScale * parameters.m_PerceptionParams.perceptionScale.audio * fGlobalAudioPerceptionScale;
	fSoundThreatLevel *= complessiveSoundScale;
}

//////////////////////////////////////////////////////////////////////////
void ApplyVisualPerceptionScalingParameters(const CPuppet* pPuppet, float& fVisualScale)
{
	// Attenuate by perception parameter scaling.
	const AgentParameters& parameters = pPuppet->GetParameters();
	const float fGlobalVisualPerceptionScale = gEnv->pAISystem->GetGlobalVisualScale(pPuppet);
	const float complessiveVisualScale = parameters.m_PerceptionParams.perceptionScale.audio * fGlobalVisualPerceptionScale;
	fVisualScale *= complessiveVisualScale;
}

//////////////////////////////////////////////////////////////////////////
EAITargetThreat TranslateTargetThreatFromThreatValue(float fStimulusThreatLevel, float fPuppetDistanceFromEvent, float fEventRadius)
{
	EAITargetThreat targetThreat = AITHREAT_NONE;
	if (fStimulusThreatLevel > FLT_EPSILON && fPuppetDistanceFromEvent <= fEventRadius)
	{
		if (fStimulusThreatLevel > PERCEPTION_AGGRESSIVE_THR)
		{
			targetThreat = AITHREAT_AGGRESSIVE;
		}
		else if (fStimulusThreatLevel > PERCEPTION_THREATENING_THR)
		{
			targetThreat = AITHREAT_THREATENING;
		}
		else if (fStimulusThreatLevel > PERCEPTION_INTERESTING_THR)
		{
			targetThreat = AITHREAT_INTERESTING;
		}
		else if (fStimulusThreatLevel > PERCEPTION_SUSPECT_THR)
		{
			targetThreat = AITHREAT_SUSPECT;
		}
	}

	return targetThreat;
}

EAITargetThreat GetSoundStimulusThreatForNonPuppetEntities(const int soundType)
{
	assert(soundType > 0 && soundType < AISOUND_LAST);

	if (soundType > 0 && soundType < AISOUND_LAST)
	{
		static EAITargetThreat s_eSoundStimulusThreat[AISOUND_LAST] =
		{
			AITHREAT_INTERESTING,   // AISOUND_GENERIC
			AITHREAT_INTERESTING,   // AISOUND_COLLISION
			AITHREAT_THREATENING,   // AISOUND_COLLISION_LOUD
			AITHREAT_INTERESTING,   // AISOUND_MOVEMENT
			AITHREAT_THREATENING,   // AISOUND_MOVEMENT_LOUD
			AITHREAT_THREATENING,   // AISOUND_WEAPON
			AITHREAT_THREATENING,   // AISOUND_EXPLOSION
		};

		return s_eSoundStimulusThreat[soundType];
	}

	return AITHREAT_NONE;
}

//////////////////////////////////////////////////////////////////////////
bool ShouldVisualStimulusBeHandled(tAIObjectID objectID, const Vec3 eventPosition)
{
	if (gAIEnv.CVars.IgnoreVisualStimulus != 0)
		return false;

	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(objectID);
	assert(refObject.IsValid());

	CPuppet* pPuppet = CastToCPuppetSafe(refObject.GetAIObject());
	if (pPuppet)
	{
		float fVisualPerceptionScale = 1.0f;   // The default value is 1.0f that will not apply any scaling
		ApplyVisualPerceptionScalingParameters(pPuppet, fVisualPerceptionScale);
		const AgentParameters& parameters = pPuppet->GetParameters();

		if (parameters.m_bAiIgnoreFgNode || fVisualPerceptionScale <= .0f)
			return false;

		const bool isVisualStimulusVisible = pPuppet->IsPointInFOV(eventPosition, fVisualPerceptionScale) != IAIObject::eFOV_Outside;
		return isVisualStimulusVisible;
	}

	return false;
}

bool ShouldSoundStimulusBeHandled(tAIObjectID objectID, const Vec3 eventPosition, const float maxSoundDistanceAllowed)
{
	if (gAIEnv.CVars.IgnoreSoundStimulus != 0)
		return false;

	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(objectID);
	assert(refObject.IsValid());

	CPuppet* pPuppet = CastToCPuppetSafe(refObject.GetAIObject());
	if (pPuppet)
	{
		float fAudioPerceptionScale = 1.0f;
		ApplyAudioPerceptionScalingParameters(pPuppet, fAudioPerceptionScale);
		const AgentParameters& parameters = pPuppet->GetParameters();

		if (parameters.m_bAiIgnoreFgNode || fAudioPerceptionScale <= .0f)
			return false;

		const Vec3 puppetPosition = pPuppet->GetPos();
		const float fSoundDistance = puppetPosition.GetDistance(eventPosition) * (1.0f / fAudioPerceptionScale);
		if (fSoundDistance <= maxSoundDistanceAllowed)
		{
			return true;
		}
	}

	return false;
}

bool ShouldBulletRainStimulusBeHandled(tAIObjectID objectID)
{
	if (gAIEnv.CVars.IgnoreBulletRainStimulus != 0)
		return false;

	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(objectID);
	assert(refObject.IsValid());

	CPuppet* pPuppet = CastToCPuppetSafe(refObject.GetAIObject());
	if (pPuppet)
	{
		const AgentParameters& parameters = pPuppet->GetParameters();
		return !parameters.m_bAiIgnoreFgNode;
	}

	return false;
}

bool ShouldGenericStimulusBeHandled(tAIObjectID objectID)
{
	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(objectID);
	assert(refObject.IsValid());

	CPuppet* pPuppet = CastToCPuppetSafe(refObject.GetAIObject());
	if (pPuppet)
	{
		const AgentParameters& parameters = pPuppet->GetParameters();
		return !parameters.m_bAiIgnoreFgNode;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::CTargetTrackConfigProxy::ModifyTargetThreat(IAIObject& ownerAI, IAIObject& targetAI, const ITargetTrack& track,
                                                                      float& outThreatRatio, EAITargetThreat& outThreat) const
{
	ITargetTrackThreatModifier* pThreatModifier = m_pManager->GetTargetTrackThreatModifier();
	if (pThreatModifier)
	{
		pThreatModifier->ModifyTargetThreat(ownerAI, targetAI, track, outThreatRatio, outThreat);
	}
}

//////////////////////////////////////////////////////////////////////////
CTargetTrackManager::CTargetTrackManager()
	: m_pThreatModifier()
	, m_pTrackPoolProxy()
	, m_pTrackConfigProxy()
{
	m_pTrackPoolProxy = new CTargetTrackPoolProxy(this);
	assert(m_pTrackPoolProxy);

	m_pTrackConfigProxy = new CTargetTrackConfigProxy(this);
	assert(m_pTrackConfigProxy);

	PrepareModifiers();
	ResetFreshestTargetData();

#ifdef TARGET_TRACK_DEBUG
	m_uLastDebugAgent = 0;
#endif //TARGET_TRACK_DEBUG
}

//////////////////////////////////////////////////////////////////////////
CTargetTrackManager::~CTargetTrackManager()
{
	Shutdown();

	std::for_each(m_TargetTrackPool.begin(), m_TargetTrackPool.end(), stl::container_object_deleter());

	std::for_each(m_Modifiers.begin(), m_Modifiers.end(), stl::container_object_deleter());

	SAFE_DELETE(m_pTrackPoolProxy);
	SAFE_DELETE(m_pTrackConfigProxy);
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::PrepareModifiers()
{
	m_Modifiers.reserve(5);

	m_Modifiers.push_back(new CTargetTrackDistanceModifier());
	m_Modifiers.push_back(new CTargetTrackHostileModifier());
	m_Modifiers.push_back(new CTargetTrackClassThreatModifier());
	m_Modifiers.push_back(new CTargetTrackDistanceIgnoreModifier());
	m_Modifiers.push_back(new CTargetTrackPlayerModifier());
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::IsEnabled() const
{
	// Currently based on cvar
	return (gAIEnv.CVars.TargetTracking != 0);
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::SetTargetTrackThreatModifier(ITargetTrackThreatModifier* pModifier)
{
	assert(pModifier);
	m_pThreatModifier = pModifier;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::ClearTargetTrackThreatModifier()
{
	m_pThreatModifier = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::Init()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!ReloadConfig())
	{
		AIWarning("CTargetTrackManager::Init() Warning: Failed to load configuration file \'%s\'", g_szTargetStimulusConfig_XmlPath);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::Shutdown()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	DeleteConfigs();
	DeleteAgents();
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::Reset(IAISystem::EResetReason reason)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	switch (reason)
	{
	case IAISystem::RESET_ENTER_GAME:
		break;

	case IAISystem::RESET_LOAD_LEVEL:
		{
			if (m_TargetTrackPool.empty())
			{
				m_TargetTrackPool.resize(g_uTargetTracksPoolListSize);
				for (uint32 i = 0; i < g_uTargetTracksPoolListSize; ++i)
				{
					m_TargetTrackPool[i] = new CTargetTrack();
				}
			}
		}
		break;

	case IAISystem::RESET_UNLOAD_LEVEL:
		{
			MEMSTAT_LABEL_SCOPED("CTargetTrackManager::Reset(Unload)");
			DeleteAgents();

			std::for_each(m_TargetTrackPool.begin(), m_TargetTrackPool.end(), stl::container_object_deleter());
			stl::free_container(m_TargetTrackPool);
			stl::free_container(m_ClassThreatValues);
			ResetFreshestTargetData();
		}
		break;

	default:
		// Delete any agents registered
		DeleteAgents();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::OnObjectRemoved(CAIObject* pObject)
{
	assert(pObject);

	if (pObject)
		UnregisterAgent(pObject->GetAIObjectID());
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::Serialize(TSerialize ser)
{
	ser.BeginGroup("AI_TargetTrackManager");
	{
		ser.Value("m_ClassThreatValues", m_ClassThreatValues);

		if (ser.IsWriting())
		{
			Serialize_Write(ser);
		}
		else if (ser.IsReading())
		{
			Serialize_Read(ser);
		}
	}
	ser.EndGroup();
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::Serialize_Write(TSerialize ser)
{
	assert(ser.IsWriting());

	int iAgentCount = m_Agents.size();
	ser.Value("iAgentCount", iAgentCount);

	TAgentContainer::iterator itAgent = m_Agents.begin();
	TAgentContainer::iterator itAgentEnd = m_Agents.end();
	for (; itAgent != itAgentEnd; ++itAgent)
	{
		const tAIObjectID agentId = itAgent->first;
		CTargetTrackGroup* pGroup = itAgent->second;
		assert(agentId > 0 && pGroup);

		CWeakRef<CAIObject> refAgent = gAIEnv.pObjectContainer->GetWeakRef(agentId);
		assert(refAgent.IsValid());

		uint32 uConfigHash = pGroup->GetConfigHash();
		int nTargetLimit = pGroup->GetTargetLimit();
		assert(uConfigHash > 0);

		ser.BeginGroup("Agent");
		{
			refAgent.Serialize(ser);
			ser.Value("configHash", uConfigHash);
			ser.Value("targetLimit", nTargetLimit);

			pGroup->Serialize_Write(ser);
		}
		ser.EndGroup();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::Serialize_Read(TSerialize ser)
{
	assert(ser.IsReading());

	int iAgentCount = 0;
	ser.Value("iAgentCount", iAgentCount);

	DeleteAgents();

	for (int iAgent = 0; iAgent < iAgentCount; ++iAgent)
	{
		CWeakRef<CAIObject> refAgent;
		uint32 uConfigHash = 0;
		int nTargetLimit = 0;

		ser.BeginGroup("Agent");
		{
			refAgent.Serialize(ser);
			ser.Value("configHash", uConfigHash);
			ser.Value("targetLimit", nTargetLimit);
			assert(refAgent.IsValid() && uConfigHash > 0);

			const tAIObjectID agentId = refAgent.GetObjectID();
			const bool bRegistered = RegisterAgent(agentId, uConfigHash, nTargetLimit);
			assert(bRegistered);
			if (bRegistered)
			{
				TAgentContainer::iterator itAgent = m_Agents.find(agentId);
				CTargetTrackGroup* pGroup = (itAgent != m_Agents.end() ? itAgent->second : NULL);
				assert(pGroup);
				PREFAST_ASSUME(pGroup);

				pGroup->Serialize_Read(ser);
			}
		}
		ser.EndGroup();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::RegisterAgent(tAIObjectID aiObjectId, uint32 uConfigHash, int nTargetLimit)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(aiObjectId > 0);
	assert(uConfigHash > 0);

	bool bResult = false;

	if (aiObjectId > 0)
	{
		TAgentContainer::const_iterator itAgent = m_Agents.find(aiObjectId);
		if (itAgent == m_Agents.end())
		{
			// Check if configuration exists
			TConfigContainer::const_iterator itConfig = m_Configs.find(uConfigHash);
			if (itConfig != m_Configs.end())
			{
				m_Agents[aiObjectId] = new CTargetTrackGroup(m_pTrackPoolProxy, aiObjectId, itConfig->first, nTargetLimit);
				bResult = true;
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::RegisterAgent(tAIObjectID aiObjectId, const char* szConfig, int nTargetLimit)
{
	assert(szConfig && szConfig[0]);

	const uint32 uConfigHash = GetConfigNameHash(szConfig);

	return RegisterAgent(aiObjectId, uConfigHash, nTargetLimit);
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::UnregisterAgent(tAIObjectID aiObjectId)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(aiObjectId > 0);

	bool bResult = false;

	if (aiObjectId > 0)
	{
		TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
		if (itAgent != m_Agents.end())
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);

#ifdef TARGET_TRACK_DEBUG
			if (m_uLastDebugAgent > 0 && pGroup->GetConfigHash() == m_uLastDebugAgent)
				m_uLastDebugAgent = 0;
#endif //TARGET_TRACK_DEBUG

			SAFE_DELETE(pGroup);

			m_Agents.erase(itAgent);
			bResult = true;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::ResetAgent(tAIObjectID aiObjectId)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(aiObjectId > 0);

	bool bResult = false;

	if (aiObjectId > 0)
	{
		TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
		if (itAgent != m_Agents.end())
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);

			pGroup->Reset();
			bResult = true;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::SetAgentEnabled(tAIObjectID aiObjectId, bool bEnable)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(aiObjectId > 0);

	bool bResult = false;

	if (aiObjectId > 0)
	{
		TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
		if (itAgent != m_Agents.end())
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);

			pGroup->SetEnabled(bEnable);
			bResult = true;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::SetTargetClassThreat(tAIObjectID aiObjectId, float fClassThreat)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(aiObjectId > 0);

	bool bResult = false;

	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(aiObjectId);
	if (refObject.IsSet())
	{
		m_ClassThreatValues[aiObjectId] = fClassThreat;
		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrackManager::GetTargetClassThreat(tAIObjectID aiObjectId) const
{
	assert(aiObjectId > 0);

	float fResult = 1.0f;

	TClassThreatContainer::const_iterator itValue = m_ClassThreatValues.find(aiObjectId);
	if (itValue != m_ClassThreatValues.end())
	{
		fResult = itValue->second;
	}

	return fResult;
}

//////////////////////////////////////////////////////////////////////////
int CTargetTrackManager::GetTargetLimit(tAIObjectID aiObjectId) const
{
	assert(aiObjectId > 0);

	int nResult = gAIEnv.CVars.TargetTracks_GlobalTargetLimit;

	TAgentContainer::const_iterator itAgent = m_Agents.find(aiObjectId);
	if (itAgent != m_Agents.end())
	{
		const CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		const int iGroupTargetLimit = pGroup->GetTargetLimit();
		nResult = nResult > 0 ? (iGroupTargetLimit > 0 ? min(nResult, iGroupTargetLimit) : nResult) : iGroupTargetLimit;
	}

	return max(nResult, 0);
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::HandleStimulusEventInRange(tAIObjectID aiTargetId, const char* szStimulusName, const TargetTrackHelpers::SStimulusEvent& eventInfo, float fRadius)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	assert(szStimulusName && szStimulusName[0]);

	if (fRadius > FLT_EPSILON)
	{
		bResult = true;
		const float fRadiusSq = fRadius * fRadius;

		TAgentContainer::iterator itAgent = m_Agents.begin();
		TAgentContainer::iterator itAgentEnd = m_Agents.end();
		for (; itAgent != itAgentEnd; ++itAgent)
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);

			CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(pGroup->GetAIObjectID());
			CAIObject* pObject = refObject.GetAIObject();
			assert(pObject);

			if (pObject && pObject->IsEnabled() && eventInfo.vPos.GetSquaredDistance(pObject->GetPos()) <= fRadiusSq)
			{
				const bool handleStimulus = ShouldStimulusBeHandled(refObject.GetObjectID(), eventInfo, fRadius);
				if (!handleStimulus)
					continue;

				TargetTrackHelpers::STargetTrackStimulusEvent stimulusEvent(refObject.GetObjectID(), aiTargetId, szStimulusName, eventInfo);
				bResult &= HandleStimulusEvent(pGroup, stimulusEvent);
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::HandleStimulusEventForAgent(tAIObjectID aiAgentId, tAIObjectID aiTargetId, const char* szStimulusName, const TargetTrackHelpers::SStimulusEvent& eventInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	assert(aiAgentId > 0);
	assert(szStimulusName && szStimulusName[0]);

	if (aiAgentId > 0)
	{
		TAgentContainer::iterator itAgent = m_Agents.find(aiAgentId);
		if (itAgent != m_Agents.end())
		{
			const bool handleStimulus = ShouldStimulusBeHandled(aiAgentId, eventInfo);
			if (handleStimulus)
			{
				CTargetTrackGroup* pGroup = itAgent->second;
				assert(pGroup);

				TargetTrackHelpers::STargetTrackStimulusEvent stimulusEvent(aiAgentId, aiTargetId, szStimulusName, eventInfo);
				bResult = HandleStimulusEvent(pGroup, stimulusEvent);
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::HandleStimulusFromAIEvent(tAIObjectID aiObjectId, const SAIEVENT* pAIEvent, TargetTrackHelpers::EAIEventStimulusType eType)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	assert(aiObjectId > 0);
	assert(pAIEvent);

	if (aiObjectId > 0 && pAIEvent)
	{
		TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
		if (itAgent != m_Agents.end())
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);
			bool successfullyTranslated = false;
			TargetTrackHelpers::STargetTrackStimulusEvent stimulusEvent(aiObjectId);
			switch (eType)
			{
			case TargetTrackHelpers::eEST_Visual:
				{
					successfullyTranslated = TranslateVisualStimulusIfCanBeHandled(stimulusEvent, pAIEvent);
				}
				break;

			case TargetTrackHelpers::eEST_Sound:
				{
					successfullyTranslated = TranslateSoundStimulusIfCanBeHandled(stimulusEvent, pAIEvent);
				}
				break;

			case TargetTrackHelpers::eEST_BulletRain:
				{
					successfullyTranslated = TranslateBulletRainStimulusIfCanBeHandled(stimulusEvent, pAIEvent);
				}
				break;

			case TargetTrackHelpers::eEST_Generic:
			default:
				CRY_ASSERT_MESSAGE(0, "CTargetTrackManager::HandleStimulusEvent Unhandled AIEvent stimulus type received");
				break;
			}

			if (successfullyTranslated)
				bResult = HandleStimulusEvent(pGroup, stimulusEvent);
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::ShouldStimulusBeHandled(tAIObjectID aiObjectID, const TargetTrackHelpers::SStimulusEvent& stimulusEvent, const float maxRadius /* = FLT_MAX */)
{
	bool shouldHandleTheStimulus = false;
	switch (stimulusEvent.eStimulusType)
	{
	case TargetTrackHelpers::eEST_Visual:
		{
			shouldHandleTheStimulus = TargetTrackHelpers::ShouldVisualStimulusBeHandled(aiObjectID, stimulusEvent.vPos);
		}
		break;

	case TargetTrackHelpers::eEST_Sound:
		{
			shouldHandleTheStimulus = TargetTrackHelpers::ShouldSoundStimulusBeHandled(aiObjectID, stimulusEvent.vPos, maxRadius);
		}
		break;

	case TargetTrackHelpers::eEST_BulletRain:
		{
			shouldHandleTheStimulus = TargetTrackHelpers::ShouldBulletRainStimulusBeHandled(aiObjectID);
		}
		break;

	case TargetTrackHelpers::eEST_Generic:
	default:
		{
			shouldHandleTheStimulus = TargetTrackHelpers::ShouldGenericStimulusBeHandled(aiObjectID);
		}
		break;
	}

	return shouldHandleTheStimulus;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::HandleStimulusEvent(CTargetTrackGroup* pGroup, TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent)
{
	bool bResult = false;

	assert(pGroup);

	if (!pGroup->IsEnabled())
		return false;

	const uint32 uConfigHash = pGroup->GetConfigHash();
	const uint32 uStimulusNameHash = GetStimulusNameHash(stimulusEvent.m_sStimulusName);

	if (!CheckConfigUsesStimulus(uConfigHash, uStimulusNameHash))
		return false;

	if (stimulusEvent.m_targetId != 0 && !CheckStimulusHostile(stimulusEvent.m_ownerId, stimulusEvent.m_targetId, uConfigHash, uStimulusNameHash))
		return false;

	return pGroup->HandleStimulusEvent(stimulusEvent, uStimulusNameHash);
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::TriggerPulse(tAIObjectID aiObjectId, tAIObjectID targetId, const char* szStimulusName,
                                       const char* szPulseName)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	assert(aiObjectId > 0);
	assert(szStimulusName && szStimulusName[0]);
	assert(szPulseName && szPulseName[0]);

	if (aiObjectId > 0)
	{
		TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
		if (itAgent != m_Agents.end())
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);

			// Look up the stimulus config for processing aid
			const uint32 uStimulusNameHash = GetStimulusNameHash(szStimulusName);
			if (CheckConfigUsesStimulus(pGroup->GetConfigHash(), uStimulusNameHash))
			{
				const uint32 uPulseNameHash = GetPulseNameHash(szPulseName);
				bResult = pGroup->TriggerPulse(targetId, uStimulusNameHash, uPulseNameHash);
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
tAIObjectID CTargetTrackManager::GetAIObjectId(EntityId entityId)
{
	IEntity* pEntity = entityId ? gEnv->pEntitySystem->GetEntity(entityId) : NULL;
	IAIObject* pAI = pEntity ? pEntity->GetAI() : NULL;

	return (pAI ? pAI->GetAIObjectID() : INVALID_AIOBJECTID);
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::CheckConfigUsesStimulus(uint32 uConfigHash, uint32 uStimulusNameHash) const
{
	assert(uConfigHash > 0);
	assert(uStimulusNameHash > 0);

	bool bResult = false;

	const TargetTrackHelpers::STargetTrackConfig* pConfig = NULL;
	if (GetTargetTrackConfig(uConfigHash, pConfig))
	{
		assert(pConfig);

		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itStimulus = pConfig->m_stimuli.find(uStimulusNameHash);
		bResult = (itStimulus != pConfig->m_stimuli.end());
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::CheckStimulusHostile(tAIObjectID aiObjectId, tAIObjectID aiTargetId, uint32 uConfigHash, uint32 uStimulusNameHash) const
{
	assert(aiObjectId > 0);
	assert(uStimulusNameHash > 0);

	const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig = NULL;
	if (GetTargetTrackStimulusConfig(uConfigHash, uStimulusNameHash, pStimulusConfig))
	{
		if (pStimulusConfig->m_bHostileOnly)
		{
			CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(aiObjectId);
			CWeakRef<CAIObject> refTarget = gAIEnv.pObjectContainer->GetWeakRef(aiTargetId);
			assert(refObject.IsValid() && refTarget.IsValid());

			CAIObject* pObject = refObject.GetAIObject();
			CAIObject* pTarget = refTarget.GetAIObject();
			return (pObject != NULL) && pTarget && pObject->IsHostile(pTarget);
		}
		else
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::TranslateVisualStimulusIfCanBeHandled(TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, const SAIEVENT* pAIEvent) const
{
	assert(pAIEvent);
	const Vec3& eventPosition = pAIEvent->vPosition;
	bool handleVisualStimulus = TargetTrackHelpers::ShouldVisualStimulusBeHandled(stimulusEvent.m_ownerId, eventPosition);

	if (!handleVisualStimulus)
		return false;

	stimulusEvent.m_eStimulusType = TargetTrackHelpers::eEST_Visual;
	stimulusEvent.m_targetId = GetAIObjectId(pAIEvent->sourceId);
	stimulusEvent.m_vTargetPos = eventPosition;

	if (pAIEvent->bFuzzySight)
	{
		stimulusEvent.m_eTargetThreat = AITHREAT_THREATENING;
		stimulusEvent.m_sStimulusName = "VisualSecondary";
	}
	else
	{
		stimulusEvent.m_eTargetThreat = AITHREAT_AGGRESSIVE;
		stimulusEvent.m_sStimulusName = "VisualPrimary";
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::TranslateSoundStimulusIfCanBeHandled(TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, const SAIEVENT* pAIEvent) const
{
	assert(pAIEvent);

	bool handleSoundStimulus = TargetTrackHelpers::ShouldSoundStimulusBeHandled(stimulusEvent.m_ownerId, pAIEvent->vPosition, pAIEvent->fThreat);

	if (!handleSoundStimulus)
		return false;

	stimulusEvent.m_eStimulusType = TargetTrackHelpers::eEST_Sound;

	CWeakRef<CAIObject> refObject = gAIEnv.pObjectContainer->GetWeakRef(stimulusEvent.m_ownerId);
	assert(refObject.IsValid());

	CPuppet* pPuppet = CastToCPuppetSafe(refObject.GetAIObject());
	if (pPuppet)
	{
		// Get descriptor
		SSoundPerceptionDescriptor sDescriptor;
		if (!pPuppet->GetSoundPerceptionDescriptor((EAISoundStimType)pAIEvent->nType, sDescriptor))
			CRY_ASSERT_MESSAGE(0, "Missing Sound Perception Descriptor when handling a sound event");

		float fEventRadius = pAIEvent->fThreat;
		float fSoundThreatLevel = 0.0f;
		float fSoundTime = 0.0f;
		const float fDistanceToEvent = Distance::Point_Point(pAIEvent->vPosition, pPuppet->GetPos());

		// Check minimum distance
		if (fabsf(sDescriptor.fMinDist) <= FLT_EPSILON || fDistanceToEvent > sDescriptor.fMinDist)
		{
			fEventRadius *= sDescriptor.fRadiusScale;
			const float fDistNorm = (fEventRadius > FLT_EPSILON ? fDistanceToEvent / fEventRadius : 0.0f);
			fSoundThreatLevel = sDescriptor.fBaseThreat * LinStep(sDescriptor.fLinStepMin, sDescriptor.fLinStepMax, fDistNorm);
			fSoundTime = sDescriptor.fSoundTime;
		}

		// Make sure that at max level the value reaches over the threshold.
		fSoundThreatLevel *= 1.1f;

		TargetTrackHelpers::ApplyAudioPerceptionScalingParameters(pPuppet, fSoundThreatLevel);

		// Skip really quiet sounds or too far away sounds
		stimulusEvent.m_eTargetThreat = TargetTrackHelpers::TranslateTargetThreatFromThreatValue(fSoundThreatLevel, fDistanceToEvent, fEventRadius);
	}
	else
	{
		stimulusEvent.m_eTargetThreat = TargetTrackHelpers::GetSoundStimulusThreatForNonPuppetEntities(pAIEvent->nType);
	}

	stimulusEvent.m_sStimulusName = TargetTrackHelpers::GetSoundStimulusNameFromType(pAIEvent->nType);

	stimulusEvent.m_targetId = GetAIObjectId(pAIEvent->sourceId);
	stimulusEvent.m_vTargetPos = pAIEvent->vPosition;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::TranslateBulletRainStimulusIfCanBeHandled(TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, const SAIEVENT* pAIEvent) const
{
	assert(pAIEvent);
	const bool handleBulletRainStimulus = TargetTrackHelpers::ShouldBulletRainStimulusBeHandled(stimulusEvent.m_ownerId);
	if (!handleBulletRainStimulus)
		return false;

	stimulusEvent.m_eStimulusType = TargetTrackHelpers::eEST_BulletRain;
	stimulusEvent.m_eTargetThreat = AITHREAT_AGGRESSIVE;
	stimulusEvent.m_sStimulusName = "BulletRain";

	stimulusEvent.m_targetId = GetAIObjectId(pAIEvent->sourceId);
	stimulusEvent.m_vTargetPos = pAIEvent->vPosition;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::Update(tAIObjectID aiObjectId)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(aiObjectId > 0);

	TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
	if (itAgent != m_Agents.end())
	{
		CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		pGroup->Update(m_pTrackConfigProxy);
	}
}

void CTargetTrackManager::ResetFreshestTargetData()
{
	stl::free_container(m_dataPerTarget);
}

// Note that target data updating happens across factions and groups
void CTargetTrackManager::ShareFreshestTargetData()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Find the freshest visual stimulus data for each target
	m_dataPerTarget.clear();
	m_dataPerTarget.reserve(16);
	{
		TAgentContainer::iterator agentIt = m_Agents.begin();
		TAgentContainer::iterator agentEnd = m_Agents.end();

		for (; agentIt != agentEnd; ++agentIt)
		{
			CTargetTrackGroup* group = agentIt->second;

			CTargetTrackGroup::TTargetTrackContainer& tracks = group->GetTargetTracks();

			CTargetTrackGroup::TTargetTrackContainer::iterator trackIt = tracks.begin();
			CTargetTrackGroup::TTargetTrackContainer::iterator trackEnd = tracks.end();

			for (; trackIt != trackEnd; ++trackIt)
			{
				CTargetTrack* track = trackIt->second;

				CTargetTrack::TStimuliInvocationContainer& invocations = track->GetInvocations();

				CTargetTrack::TStimuliInvocationContainer::iterator invocationIt = invocations.begin();
				CTargetTrack::TStimuliInvocationContainer::iterator invocationEnd = invocations.end();

				for (; invocationIt != invocationEnd; ++invocationIt)
				{
					CTargetTrack::SStimulusInvocation& invocation = invocationIt->second;

					if (invocation.m_eStimulusType == TargetTrackHelpers::eEST_Visual)
					{
						FreshData& data = m_dataPerTarget[track->GetAIObject().GetObjectID()];
						const float invokeTime = invocation.m_envelopeData.m_fLastInvokeTime;
						if (invokeTime > data.timeOfFreshestVisualStimulus)
						{
							data.timeOfFreshestVisualStimulus = invokeTime;
							data.freshestVisualPosition = invocation.m_vLastPos;
							data.freshestVisualDirection = invocation.m_vLastDir;
						}
					}
				}
			}
		}
	}

	// Write back the freshest invocation data
	{
		TAgentContainer::iterator agentIt = m_Agents.begin();
		TAgentContainer::iterator agentEnd = m_Agents.end();

		for (; agentIt != agentEnd; ++agentIt)
		{
			CTargetTrackGroup* group = agentIt->second;
			CTargetTrackGroup::TTargetTrackContainer& tracks = group->GetTargetTracks();

			CTargetTrackGroup::TTargetTrackContainer::iterator trackIt = tracks.begin();
			CTargetTrackGroup::TTargetTrackContainer::iterator trackEnd = tracks.end();

			for (; trackIt != trackEnd; ++trackIt)
			{
				CTargetTrack* track = trackIt->second;

				DataPerTarget::iterator dataIt = m_dataPerTarget.find(track->GetAIObject().GetObjectID());
				if (dataIt != m_dataPerTarget.end())
				{
					FreshData& data = dataIt->second;

					CTargetTrack::TStimuliInvocationContainer& invocations = track->GetInvocations();

					CTargetTrack::TStimuliInvocationContainer::iterator invocationIt = invocations.begin();
					CTargetTrack::TStimuliInvocationContainer::iterator invocationEnd = invocations.end();

					for (; invocationIt != invocationEnd; ++invocationIt)
					{
						CTargetTrack::SStimulusInvocation& invocation = invocationIt->second;

						if (invocation.m_eStimulusType == TargetTrackHelpers::eEST_Visual)
						{
							invocation.m_vLastPos = data.freshestVisualPosition;
							invocation.m_vLastDir = data.freshestVisualDirection;
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::PullDownThreatLevel(const tAIObjectID aiObjectIdForTargetTrackGroup, const EAITargetThreat maxAllowedThreat)
{
	TAgentContainer::iterator agentIt = m_Agents.begin();
	TAgentContainer::iterator agentEnd = m_Agents.end();

	for (; agentIt != agentEnd; ++agentIt)
	{
		CTargetTrackGroup* group = agentIt->second;

		if (group->GetAIObjectID() == aiObjectIdForTargetTrackGroup)
		{
			CTargetTrackGroup::TTargetTrackContainer& tracks = group->GetTargetTracks();

			CTargetTrackGroup::TTargetTrackContainer::iterator trackIt = tracks.begin();
			CTargetTrackGroup::TTargetTrackContainer::iterator trackEnd = tracks.end();

			for (; trackIt != trackEnd; ++trackIt)
			{
				CTargetTrack* track = trackIt->second;

				CTargetTrack::TStimuliInvocationContainer& invocations = track->GetInvocations();

				CTargetTrack::TStimuliInvocationContainer::iterator invocationIt = invocations.begin();
				CTargetTrack::TStimuliInvocationContainer::iterator invocationEnd = invocations.end();

				for (; invocationIt != invocationEnd; ++invocationIt)
				{
					CTargetTrack::SStimulusInvocation& invocation = invocationIt->second;

					if (invocation.m_eTargetThreat > maxAllowedThreat)
					{
						invocation.m_eTargetThreat = maxAllowedThreat;
					}
				}
			}

			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::GetDesiredTarget(tAIObjectID aiObjectId, uint32 uDesiredTargetMethod, CWeakRef<CAIObject>& outTarget, SAIPotentialTarget*& pOutTargetInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	assert(aiObjectId > 0);
	outTarget.Reset();

	TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
	if (itAgent != m_Agents.end())
	{
		CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		bResult = pGroup->GetDesiredTarget((TargetTrackHelpers::EDesiredTargetMethod)uDesiredTargetMethod, outTarget, pOutTargetInfo);
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
uint32 CTargetTrackManager::GetBestTargets(tAIObjectID aiObjectId, uint32 uDesiredTargetMethod,
                                           tAIObjectID* bestTargets, uint32 maxCount)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	uint32 count = 0;

	TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);

	if (itAgent != m_Agents.end())
	{
		CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		const uint32 MaxTracks = 8;
		CTargetTrack* tracks[MaxTracks] = { 0 };
		assert(maxCount <= MaxTracks);

		uint32 trackCount = pGroup->GetBestTrack((TargetTrackHelpers::EDesiredTargetMethod)uDesiredTargetMethod, tracks,
		                                         MaxTracks);

		for (uint32 i = 0; (i < trackCount) && (count < maxCount); ++i)
		{
			if (tracks[i])
			{
				if (tAIObjectID objectID = tracks[i]->GetAITarget().GetObjectID())
					bestTargets[count++] = objectID;
			}
		}
	}

	return count;
}

//////////////////////////////////////////////////////////////////////////
int CTargetTrackManager::GetDesiredTargetCount(tAIObjectID aiTargetId, tAIObjectID aiIgnoreId) const
{
	assert(aiTargetId > 0);

	int iCount = 0;

	TAgentContainer::const_iterator itAgent = m_Agents.begin();
	TAgentContainer::const_iterator itAgentEnd = m_Agents.end();
	for (; itAgent != itAgentEnd; ++itAgent)
	{
		const CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		if (pGroup->GetAIObjectID() != aiIgnoreId && pGroup->IsDesiredTarget(aiTargetId))
			++iCount;
	}

	return iCount;
}

//////////////////////////////////////////////////////////////////////////
int CTargetTrackManager::GetPotentialTargetCount(tAIObjectID aiTargetId, tAIObjectID aiIgnoreId) const
{
	assert(aiTargetId > 0);

	int iCount = 0;

	TAgentContainer::const_iterator itAgent = m_Agents.begin();
	TAgentContainer::const_iterator itAgentEnd = m_Agents.end();
	for (; itAgent != itAgentEnd; ++itAgent)
	{
		const CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		if (pGroup->GetAIObjectID() != aiIgnoreId && pGroup->IsPotentialTarget(aiTargetId))
			++iCount;
	}

	return iCount;
}

//////////////////////////////////////////////////////////////////////////
int CTargetTrackManager::GetPotentialTargetCountFromFaction(tAIObjectID aiTargetId, const char* factionName, tAIObjectID aiIgnoreId) const
{
	assert(aiTargetId > 0);

	int iCount = 0;
	const uint8 factionID = gAIEnv.pFactionMap->GetFactionID(factionName);

	TAgentContainer::const_iterator itAgent = m_Agents.begin();
	TAgentContainer::const_iterator itAgentEnd = m_Agents.end();
	for (; itAgent != itAgentEnd; ++itAgent)
	{
		const CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		if (pGroup->GetAIObjectID() != aiIgnoreId && pGroup->IsPotentialTarget(aiTargetId))
		{
			CAIActor* pTargetActor = CastToCAIActorSafe(gAIEnv.pObjectContainer->GetAIObject(pGroup->GetAIObjectID()));
			if ((pTargetActor != NULL) && pTargetActor->GetFactionID() == factionID)
				++iCount;
		}
	}

	return iCount;
}

//////////////////////////////////////////////////////////////////////////
CTargetTrack* CTargetTrackManager::GetUnusedTargetTrackFromPool()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CTargetTrack* pTrack = NULL;

	if (!m_TargetTrackPool.empty())
	{
		pTrack = *m_TargetTrackPool.rbegin();
		m_TargetTrackPool.pop_back();
	}
	else
	{
		pTrack = new CTargetTrack();
	}

	return pTrack;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::AddTargetTrackToPool(CTargetTrack* pTrack)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pTrack);

	if (pTrack)
	{
		pTrack->ResetForPool();

		if (m_TargetTrackPool.capacity())
		{
			// Only return the track to the pool whilst the pool is active.
			m_TargetTrackPool.push_back(pTrack);
		}
		else
		{
			delete pTrack;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::GetTargetTrackConfig(uint32 uNameHash, TargetTrackHelpers::STargetTrackConfig const*& pOutConfig) const
{
	assert(uNameHash > 0);

	bool bResult = false;

	TConfigContainer::const_iterator itConfig = m_Configs.find(uNameHash);
	if (itConfig != m_Configs.end())
	{
		pOutConfig = itConfig->second;
		assert(pOutConfig);

		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::GetTargetTrackStimulusConfig(uint32 uNameHash, uint32 uStimulusHash, TargetTrackHelpers::STargetTrackStimulusConfig const*& pOutConfig) const
{
	assert(uNameHash > 0);
	assert(uStimulusHash > 0);

	bool bResult = false;

	const TargetTrackHelpers::STargetTrackConfig* pConfig = NULL;
	if (GetTargetTrackConfig(uNameHash, pConfig))
	{
		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itStimulus = pConfig->m_stimuli.find(uStimulusHash);
		if (itStimulus != pConfig->m_stimuli.end())
		{
			pOutConfig = &(itStimulus->second);
			bResult = true;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
const ITargetTrackModifier* CTargetTrackManager::GetTargetTrackModifier(uint32 uId) const
{
	ITargetTrackModifier* pResult = NULL;

	TModifierContainer::const_iterator itMod = m_Modifiers.begin();
	TModifierContainer::const_iterator itModEnd = m_Modifiers.end();
	for (; itMod != itModEnd; ++itMod)
	{
		ITargetTrackModifier* pMod = *itMod;
		assert(pMod);

		if (pMod && pMod->GetUniqueId() == uId)
		{
			pResult = pMod;
			break;
		}
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::DeleteConfigs()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	TConfigContainer::iterator itConfig = m_Configs.begin();
	TConfigContainer::iterator itConfigEnd = m_Configs.end();
	for (; itConfig != itConfigEnd; ++itConfig)
	{
		TargetTrackHelpers::STargetTrackConfig* pConfig = itConfig->second;
		SAFE_DELETE(pConfig);
	}
	m_Configs.clear();
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::DeleteAgents()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	TAgentContainer::iterator itAgent = m_Agents.begin();
	TAgentContainer::iterator itAgentEnd = m_Agents.end();
	for (; itAgent != itAgentEnd; ++itAgent)
	{
		CTargetTrackGroup* pGroup = itAgent->second;
		assert(pGroup);

		SAFE_DELETE(pGroup);
	}
	m_Agents.clear();

#ifdef TARGET_TRACK_DEBUG
	m_uLastDebugAgent = 0;
#endif //TARGET_TRACK_DEBUG
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::ReloadConfig()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	// Remove the old configurations
	DeleteConfigs();

	XmlNodeRef pRoot = gEnv->pSystem->LoadXmlFromFile(g_szTargetStimulusConfig_XmlPath);
	if (pRoot && !stricmp(pRoot->getTag(), "AITargetStimulusConfig"))
	{
		// Check version
		uint32 uVersion = 0;
		if (!pRoot->getAttr("version", uVersion) || uVersion < g_uTargetStimulusConfig_Version)
		{
			AIWarning("CTargetTrackManager::ReloadConfig() Warning: Invalid version for configuration file \'%s\'", g_szTargetStimulusConfig_XmlPath);
		}
		else
		{
			bResult = LoadConfigs(pRoot);
			bResult &= ApplyStimulusTemplates();
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::LoadConfigs(XmlNodeRef& pRoot)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = true;

	const int iChildCount = (pRoot ? pRoot->getChildCount() : 0);
	for (int iChild = 0; iChild < iChildCount; ++iChild)
	{
		XmlNodeRef pConfigElement = pRoot->getChild(iChild);
		if (!pConfigElement || stricmp(pConfigElement->getTag(), "Config"))
			continue;

		bool bSuccess = false;

		XmlString szName;
		if (pConfigElement->getAttr("name", szName) && !szName.empty())
		{
			TargetTrackHelpers::STargetTrackConfig* pConfig = new TargetTrackHelpers::STargetTrackConfig(szName);

			XmlString szTemplate;
			if (pConfigElement->getAttr("template", szTemplate) && !szTemplate.empty())
			{
				pConfig->m_sTemplate = szTemplate;
			}
			else
			{
				// No template means it's been applied
				pConfig->m_bTemplateApplied = true;
			}

			XmlNodeRef pStimuliElement = pConfigElement->findChild("Stimuli");
			if (pStimuliElement)
			{
				bSuccess = LoadConfigStimuli(pConfig, pStimuliElement, !pConfig->m_bTemplateApplied);
			}

			// Add to container using hash
			if (bSuccess)
			{
				const uint32 uHash = GetConfigNameHash(szName);
				TConfigContainer::iterator itConfig = m_Configs.find(uHash);
				if (itConfig != m_Configs.end())
				{
					AIWarning("CTargetTrackManager::LoadConfigs() Warning: Hash clash with config \'%s\'... please use a different name!", szName.c_str());
					SAFE_DELETE(pConfig);
					bSuccess = false;
				}
				else
				{
					m_Configs[uHash] = pConfig;
				}
			}
		}

		if (!bSuccess)
		{
			AIWarning("CTargetTrackManager::LoadConfigs() Warning: Error while parsing target stimulus configuration \'%s\'", szName.c_str());
		}

		bResult &= bSuccess;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::LoadConfigStimuli(TargetTrackHelpers::STargetTrackConfig* pConfig, XmlNodeRef& pStimuliElement, bool bHasTemplate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pConfig && pStimuliElement);

	bool bResult = true;

	const int iChildCount = (pStimuliElement ? pStimuliElement->getChildCount() : 0);
	for (int iChild = 0; iChild < iChildCount; ++iChild)
	{
		XmlNodeRef pStimulusElement = pStimuliElement->getChild(iChild);
		if (!pStimulusElement || stricmp(pStimulusElement->getTag(), "Stimulus"))
			continue;

		bool bSuccess = false;

		XmlString szName;
		if (pStimulusElement->getAttr("name", szName) && !szName.empty())
		{
			float fPeak = 0.0f;
			float fAttack = 0.0f;
			float fDecay = 0.0f;
			float fSustainRatio = 0.0f;
			float fRelease = 0.0f;
			float fIgnore = 0.0f;
			bool bHostileOnly = false;

			bool bIsValid = pStimulusElement->getAttr("peak", fPeak);
			if (!bIsValid || fPeak <= FLT_EPSILON)
			{
				// Can only be invalid if we inherit from our template
				if (bHasTemplate)
				{
					fPeak = TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE;
					bIsValid = true;
				}
				else
				{
					bIsValid = false;
				}
			}

			if (bIsValid)
			{
				if (!pStimulusElement->getAttr("attack", fAttack))
					fAttack = TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE;
				if (!pStimulusElement->getAttr("decay", fDecay))
					fDecay = TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE;
				if (!pStimulusElement->getAttr("sustain", fSustainRatio))
					fSustainRatio = TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE;
				if (!pStimulusElement->getAttr("release", fRelease))
					fRelease = TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE;
				if (!pStimulusElement->getAttr("ignore", fIgnore))
					fIgnore = TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE;
				pStimulusElement->getAttr("hostileOnly", bHostileOnly);

				// Add to container using hash
				const uint32 uHash = GetStimulusNameHash(szName);
				TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::iterator itStimulusDef = pConfig->m_stimuli.find(uHash);
				if (itStimulusDef != pConfig->m_stimuli.end())
				{
					AIWarning("CTargetTrackManager::LoadConfigStimuli() Warning: Hash clash with stimulus \'%s\' in configuration \'%s\'... please use a different name!",
					          szName.c_str(), pConfig->m_sName.c_str());
				}
				else
				{
					TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::value_type stimulusPair(uHash, TargetTrackHelpers::STargetTrackStimulusConfig(szName, bHostileOnly, fPeak, fSustainRatio, fAttack, fDecay, fRelease, fIgnore));
					pConfig->m_stimuli.insert(stimulusPair);

					TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig = &(pConfig->m_stimuli.find(uHash)->second);

					// Load modifiers
					XmlNodeRef pModifiersElement = pStimulusElement->findChild("Modifiers");
					if (pModifiersElement && !LoadConfigModifiers(pStimulusConfig, pModifiersElement))
					{
						AIWarning("CTargetTrackManager::LoadConfigStimuli() Warning: Failed to load all modifiers for stimulus \'%s\' in configuration \'%s\' - stimulus still loaded", szName.c_str(), pConfig->m_sName.c_str());
					}

					// Load pulses
					XmlNodeRef pPulsesElement = pStimulusElement->findChild("Pulses");
					if (pPulsesElement && !LoadConfigPulses(pStimulusConfig, pPulsesElement))
					{
						AIWarning("CTargetTrackManager::LoadConfigStimuli() Warning: Failed to load all pulses for stimulus \'%s\' in configuration \'%s\' - stimulus still loaded", szName.c_str(), pConfig->m_sName.c_str());
					}

					// Load threat levels
					XmlNodeRef pReleaseThreatLevelsElement = pStimulusElement->findChild("ReleaseThreatLevels");
					if (pReleaseThreatLevelsElement && !LoadConfigReleaseThreatLevels(pStimulusConfig, pReleaseThreatLevelsElement))
					{
						AIWarning("CTargetTrackManager::LoadConfigStimuli() Warning: Failed to load all threat levels for stimulus \'%s\' in configuration \'%s\' - stimulus still loaded", szName.c_str(), pConfig->m_sName.c_str());
					}

					bSuccess = true;
				}
			}
		}

		if (!bSuccess)
		{
			AIWarning("CTargetTrackManager::LoadConfigStimuli() Warning: Error while parsing stimulus \'%s\' in configuration \'%s\'", szName.c_str(), pConfig->m_sName.c_str());
		}

		bResult &= bSuccess;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::LoadConfigModifiers(TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig, XmlNodeRef& pModifiersElement)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pStimulusConfig && pModifiersElement);

	bool bResult = true;

	const int iChildCount = (pModifiersElement ? pModifiersElement->getChildCount() : 0);
	for (int iChild = 0; iChild < iChildCount; ++iChild)
	{
		XmlNodeRef pModifierElement = pModifiersElement->getChild(iChild);
		if (!pModifierElement)
			continue;

		bool bSuccess = false;
		bool bFoundMod = false;

		const char* szModifierTag = pModifierElement->getTag();

		TModifierContainer::iterator itModifier = m_Modifiers.begin();
		TModifierContainer::iterator itModifierEnd = m_Modifiers.end();
		for (; itModifier != itModifierEnd; ++itModifier)
		{
			ITargetTrackModifier* pModifier = *itModifier;
			assert(pModifier);

			if (pModifier->IsMatchingTag(szModifierTag))
			{
				bFoundMod = true;

				const uint32 uId = pModifier->GetUniqueId();
				float fValue = 1.0f;
				float fLimit = 0.0f;

				if (!pModifierElement->getAttr("value", fValue))
					fValue = 1.0f;
				pModifierElement->getAttr("limit", fLimit);

				TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::iterator itModDef = pStimulusConfig->m_modifiers.find(uId);
				if (itModDef != pStimulusConfig->m_modifiers.end())
				{
					AIWarning("CTargetTrackManager::LoadConfigModifiers() Warning: Redefinition of modifier \'%s\' in stimulus configuration \'%s\'",
					          szModifierTag, pStimulusConfig->m_sStimulus.c_str());
				}
				else
				{
					TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::value_type modPair(uId, TargetTrackHelpers::STargetTrackModifierConfig(uId, fValue, fLimit));
					pStimulusConfig->m_modifiers.insert(modPair);
					bSuccess = true;
				}
			}
		}

		if (!bFoundMod)
		{
			AIWarning("CTargetTrackManager::LoadConfigModifiers() Warning: No matching modifier with the name \'%s\' in stimulus configuration \'%s\'",
			          szModifierTag, pStimulusConfig->m_sStimulus.c_str());
		}

		bResult &= bSuccess;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::LoadConfigPulses(TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig, XmlNodeRef& pPulsesElement)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pStimulusConfig && pPulsesElement);

	bool bResult = true;

	const int iChildCount = (pPulsesElement ? pPulsesElement->getChildCount() : 0);
	for (int iChild = 0; iChild < iChildCount; ++iChild)
	{
		XmlNodeRef pPulseElement = pPulsesElement->getChild(iChild);
		if (!pPulseElement || stricmp(pPulseElement->getTag(), "Pulse"))
			continue;

		bool bSuccess = false;

		XmlString szName;
		if (pPulseElement->getAttr("name", szName) && !szName.empty())
		{
			float fValue = 0.0f;
			float fDuration = 0.0f;

			pPulseElement->getAttr("value", fValue);
			pPulseElement->getAttr("duration", fDuration);

			const uint32 uHash = GetPulseNameHash(szName);
			TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::iterator itPulseDef = pStimulusConfig->m_pulses.find(uHash);
			if (itPulseDef != pStimulusConfig->m_pulses.end())
			{
				AIWarning("CTargetTrackManager::LoadConfigPulses() Warning: Hash clash with pulse \'%s\' in stimulus configuration \'%s\'... please use a different name!",
				          szName.c_str(), pStimulusConfig->m_sStimulus.c_str());
			}
			/*else if (fValue <= FLT_EPSILON || fDuration <= FLT_EPSILON)
			   {
			   AIWarning("CTargetTrackManager::LoadConfigPulses() Warning: Invalid values used for pulse \'%s\' in configuration \'%s\'",
			    szName.c_str(), pStimulusConfig->m_sStimulus.c_str());
			   }*/
			else
			{
				TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::value_type pulsePair(uHash, TargetTrackHelpers::STargetTrackPulseConfig(szName, fValue, fDuration));
				pStimulusConfig->m_pulses.insert(pulsePair);
				bSuccess = true;
			}
		}

		bResult &= bSuccess;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::LoadConfigReleaseThreatLevels(TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig, XmlNodeRef& pReleaseThreatLevelsElement)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pStimulusConfig && pReleaseThreatLevelsElement);

	bool bResult = true;

	const int iChildCount = (pReleaseThreatLevelsElement ? pReleaseThreatLevelsElement->getChildCount() : 0);
	for (int iChild = 0; iChild < iChildCount; ++iChild)
	{
		XmlNodeRef pThreatLevelElement = pReleaseThreatLevelsElement->getChild(iChild);
		if (!pThreatLevelElement)
			continue;

		bool bSuccess = false;

		const char* szThreatLevelTag = pThreatLevelElement->getTag();
		EAITargetThreat threatLevel = AITHREAT_NONE;

		if (!stricmp(szThreatLevelTag, "Aggressive"))
		{
			threatLevel = AITHREAT_AGGRESSIVE;
		}
		else if (!stricmp(szThreatLevelTag, "Threatening"))
		{
			threatLevel = AITHREAT_THREATENING;
		}
		else if (!stricmp(szThreatLevelTag, "Interesting"))
		{
			threatLevel = AITHREAT_INTERESTING;
		}
		else if (!stricmp(szThreatLevelTag, "Suspect"))
		{
			threatLevel = AITHREAT_SUSPECT;
		}

		if (threatLevel > AITHREAT_NONE)
		{
			float fRatio = 0.0f;
			pThreatLevelElement->getAttr("ratio", fRatio);

			TargetTrackHelpers::STargetTrackStimulusConfig::TThreatLevelContainer::value_type threatPair(threatLevel, fRatio);
			pStimulusConfig->m_threatLevels.insert(threatPair);

			bSuccess = true;
		}

		if (!bSuccess)
		{
			AIWarning("CTargetTrackManager::LoadConfigReleaseThreatLevels() Warning: No matching threat level with the name \'%s\' in stimulus configuration \'%s\'",
			          szThreatLevelTag, pStimulusConfig->m_sStimulus.c_str());
		}

		bResult &= bSuccess;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::ApplyStimulusTemplates()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = true;

	bool bKeepProcessing = true;
	while (bKeepProcessing)
	{
		bKeepProcessing = false;

		TConfigContainer::iterator itConfig = m_Configs.begin();
		TConfigContainer::iterator itConfigEnd = m_Configs.end();
		for (; itConfig != itConfigEnd; ++itConfig)
		{
			TargetTrackHelpers::STargetTrackConfig* pConfig = itConfig->second;
			assert(pConfig);
			if (!pConfig || pConfig->m_bTemplateApplied)
				continue;

			// Find template parent and see if it is ready
			const uint32 uTemplateHash = GetConfigNameHash(pConfig->m_sTemplate.c_str());
			TConfigContainer::const_iterator itParent = m_Configs.find(uTemplateHash);
			if (itParent == m_Configs.end())
			{
				AIWarning("CTargetTrackManager::ApplyConfigTemplates() Warning: Configuration \'%s\' is defined to use template \'%s\' which doesn't exist!", pConfig->m_sName.c_str(), pConfig->m_sTemplate.c_str());
				pConfig->m_bTemplateApplied = true;
				continue;
			}

			const TargetTrackHelpers::STargetTrackConfig* pParent = itParent->second;
			assert(pParent);
			if (pParent && !pParent->m_bTemplateApplied)
			{
				// Parent is not ready, so we need another pass
				bKeepProcessing = true;
				continue;
			}

			const bool bApplied = ApplyStimulusTemplate(pConfig, pParent);
			if (!bApplied)
			{
				AIWarning("CTargetTrackManager::ApplyConfigTemplates() Warning: Configuration \'%s\' failed to inherit from \'%s\'", pConfig->m_sName.c_str(), pConfig->m_sTemplate.c_str());
			}

			bResult &= bApplied;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrackManager::ApplyStimulusTemplate(TargetTrackHelpers::STargetTrackConfig* pConfig, const TargetTrackHelpers::STargetTrackConfig* pParent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	if (pConfig && pParent)
	{
		assert(pConfig->m_sTemplate.compare(pParent->m_sName) == 0);

		// 1. Use values from parent's stimulus definition if config did not define it
		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::iterator itConfigStimulus = pConfig->m_stimuli.begin();
		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::iterator itConfigStimulusEnd = pConfig->m_stimuli.end();
		for (; itConfigStimulus != itConfigStimulusEnd; ++itConfigStimulus)
		{
			TargetTrackHelpers::STargetTrackStimulusConfig& configStimulus = itConfigStimulus->second;

			TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itParentStimulus_Copy = pParent->m_stimuli.find(GetStimulusNameHash(configStimulus.m_sStimulus));
			if (itParentStimulus_Copy != pParent->m_stimuli.end())
			{
				const TargetTrackHelpers::STargetTrackStimulusConfig& parentStimulus = itParentStimulus_Copy->second;

				if (configStimulus.m_fPeak == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE)
				{
					configStimulus.m_fPeak = parentStimulus.m_fPeak;
					configStimulus.m_ucInheritanceMask |= TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Peak;
				}
				if (configStimulus.m_fAttack == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE)
				{
					configStimulus.m_fAttack = parentStimulus.m_fAttack;
					configStimulus.m_ucInheritanceMask |= TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Attack;
				}
				if (configStimulus.m_fDecay == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE)
				{
					configStimulus.m_fDecay = parentStimulus.m_fDecay;
					configStimulus.m_ucInheritanceMask |= TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Decay;
				}
				if (configStimulus.m_fSustainRatio == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE)
				{
					configStimulus.m_fSustainRatio = parentStimulus.m_fSustainRatio;
					configStimulus.m_ucInheritanceMask |= TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Sustain;
				}
				if (configStimulus.m_fRelease == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE)
				{
					configStimulus.m_fRelease = parentStimulus.m_fRelease;
					configStimulus.m_ucInheritanceMask |= TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Release;
				}
				if (configStimulus.m_fIgnore == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE)
				{
					configStimulus.m_fIgnore = parentStimulus.m_fIgnore;
					configStimulus.m_ucInheritanceMask |= TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Ignore;
				}

				// Inherit any undefined modifiers from the parent
				TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::const_iterator itParentMod = parentStimulus.m_modifiers.begin();
				TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::const_iterator itParentModEnd = parentStimulus.m_modifiers.end();
				for (; itParentMod != itParentModEnd; ++itParentMod)
				{
					const TargetTrackHelpers::STargetTrackModifierConfig& parentMod = itParentMod->second;

					TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::const_iterator itConfigMod_Copy = configStimulus.m_modifiers.find(parentMod.m_uId);
					if (itConfigMod_Copy == configStimulus.m_modifiers.end())
					{
						// Duplicate it
						TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::value_type modPair(parentMod.m_uId, TargetTrackHelpers::STargetTrackModifierConfig(parentMod, true));
						configStimulus.m_modifiers.insert(modPair);
					}
				}

				// Inherit any undefined pulses from the parent
				TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::const_iterator itParentPulse = parentStimulus.m_pulses.begin();
				TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::const_iterator itParentPulseEnd = parentStimulus.m_pulses.end();
				for (; itParentPulse != itParentPulseEnd; ++itParentPulse)
				{
					const TargetTrackHelpers::STargetTrackPulseConfig& parentPulse = itParentPulse->second;

					const uint32 uPulseHash = GetPulseNameHash(parentPulse.m_sPulse);
					TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::const_iterator itConfigPulse_Copy = configStimulus.m_pulses.find(uPulseHash);
					if (itConfigPulse_Copy == configStimulus.m_pulses.end())
					{
						// Duplicate it
						TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::value_type pulsePair(uPulseHash, TargetTrackHelpers::STargetTrackPulseConfig(parentPulse, true));
						configStimulus.m_pulses.insert(pulsePair);
					}
				}

				// Inherit any undefined threat levels from the parent
				TargetTrackHelpers::STargetTrackStimulusConfig::TThreatLevelContainer::const_iterator itParentThreatLevel = parentStimulus.m_threatLevels.begin();
				TargetTrackHelpers::STargetTrackStimulusConfig::TThreatLevelContainer::const_iterator itParentThreatLevelEnd = parentStimulus.m_threatLevels.end();
				for (; itParentThreatLevel != itParentThreatLevelEnd; ++itParentThreatLevel)
				{
					TargetTrackHelpers::STargetTrackStimulusConfig::TThreatLevelContainer::const_iterator itThreatLevel_Copy = configStimulus.m_threatLevels.find(itParentThreatLevel->first);

					if (itThreatLevel_Copy == configStimulus.m_threatLevels.end())
					{
						// Duplicate it
						TargetTrackHelpers::STargetTrackStimulusConfig::TThreatLevelContainer::value_type threatPair(itParentThreatLevel->first, itParentThreatLevel->second);
						configStimulus.m_threatLevels.insert(threatPair);
					}
				}
			}
		}

		// 2. Copy stimulus from the parent if the client did not define them
		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itParentStimulus = pParent->m_stimuli.begin();
		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itParentStimulusEnd = pParent->m_stimuli.end();
		for (; itParentStimulus != itParentStimulusEnd; ++itParentStimulus)
		{
			const TargetTrackHelpers::STargetTrackStimulusConfig& parentStimulus = itParentStimulus->second;

			const uint32 uStimulusHash = GetStimulusNameHash(parentStimulus.m_sStimulus);
			TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itConfigStimulus_New = pConfig->m_stimuli.find(uStimulusHash);
			if (itConfigStimulus_New == pConfig->m_stimuli.end())
			{
				// Add to container using hash
				TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::value_type stimulusPair(uStimulusHash, TargetTrackHelpers::STargetTrackStimulusConfig(parentStimulus, true));
				pConfig->m_stimuli.insert(stimulusPair);
			}
		}

		pConfig->m_bTemplateApplied = true;
		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
uint32 CTargetTrackManager::GetConfigNameHash(const char* sName)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	return CryStringUtils::CalculateHashLowerCase(sName);
}

//////////////////////////////////////////////////////////////////////////
uint32 CTargetTrackManager::GetStimulusNameHash(const char* sStimulusName)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	return CryStringUtils::CalculateHashLowerCase(sStimulusName);
}

//////////////////////////////////////////////////////////////////////////
uint32 CTargetTrackManager::GetPulseNameHash(const char* sPulseName)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	return CryStringUtils::CalculateHashLowerCase(sPulseName);
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::DebugDraw()
{
#ifdef TARGET_TRACK_DEBUG
	const int nConfigMode = gAIEnv.CVars.TargetTracks_ConfigDebugDraw;
	const int nTargetMode = gAIEnv.CVars.TargetTracks_TargetDebugDraw;
	const char* szAgentName = gAIEnv.CVars.TargetTracks_AgentDebugDraw;

	if (szAgentName && szAgentName[0] && (
	      stricmp(szAgentName, "0") == 0 ||
	      stricmp(szAgentName, "none") == 0
	      ))
	{
		szAgentName = 0;
	}

	DebugDrawConfig(nConfigMode);
	DebugDrawTargets(nTargetMode, szAgentName);
	DebugDrawAgent(szAgentName);
#endif //TARGET_TRACK_DEBUG
}

#ifdef TARGET_TRACK_DEBUG
//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::DebugDrawConfig(int nMode)
{
	if (nMode <= 0)
		return;

	CDebugDrawContext dc;
	float fColumnX = 1.0f;
	float fColumnY = 11.0f;

	const ColorB textCol(255, 255, 255, 255);
	const ColorB inheritCol(255, 0, 0, 255);
	const ColorB unusedCol(128, 128, 128, 255);

	dc->Draw2dLabel(fColumnX, fColumnY, 1.5f, textCol, false, "Target Track Configs: (%" PRISIZE_T ")", m_Configs.size());
	fColumnY += 20.0f;

	const string sFilterName = gAIEnv.CVars.TargetTracks_ConfigDebugFilter;

	TConfigContainer::const_iterator itConfig = m_Configs.begin();
	TConfigContainer::const_iterator itConfigEnd = m_Configs.end();
	for (int i = 1; itConfig != itConfigEnd; ++itConfig, ++i)
	{
		const TargetTrackHelpers::STargetTrackConfig* pConfig = itConfig->second;
		assert(pConfig);

		// Use filter
		if (!sFilterName.empty() && sFilterName != "none" && pConfig->m_sName.find(sFilterName) == string::npos)
			continue;

		// Draw name
		dc->Draw2dLabel(fColumnX, fColumnY, 1.5f, textCol, false, "%d. %s", i, pConfig->m_sName.c_str());
		fColumnY += 15.0f;

		// Draw inheritance
		if (!pConfig->m_sTemplate.empty())
		{
			string sInheritance;
			const TargetTrackHelpers::STargetTrackConfig* pParent = pConfig;
			while (pParent && !pParent->m_sTemplate.empty())
			{
				TConfigContainer::const_iterator itParent = m_Configs.find(GetConfigNameHash(pParent->m_sTemplate));
				pParent = itParent->second;
				if (pParent)
				{
					if (!sInheritance.empty())
					{
						sInheritance += " <- ";
					}
					sInheritance += pParent->m_sName;
				}
			}

			dc->Draw2dLabel(fColumnX + 5.0f, fColumnY, 1.2f, inheritCol, false, "Inheritance: %s", sInheritance.c_str());
			fColumnY += 15.0f;
		}

		// Draw stimulus
		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itStimulus = pConfig->m_stimuli.begin();
		TargetTrackHelpers::STargetTrackConfig::TStimulusContainer::const_iterator itStimulusEnd = pConfig->m_stimuli.end();
		for (; itStimulus != itStimulusEnd; ++itStimulus)
		{
			const TargetTrackHelpers::STargetTrackStimulusConfig& stimulus = itStimulus->second;
			dc->Draw2dLabel(fColumnX + 5.0f, fColumnY, 1.2f, (stimulus.m_ucInheritanceMask > 0 ? inheritCol : textCol), false,
			                "Stimulus: %s", stimulus.m_sStimulus.c_str());
			fColumnY += 15.0f;
			if (nMode > 1)
			{
				// Draw the envelope properties of the stimulus
				dc->Draw2dLabel(fColumnX + 10.0f, fColumnY, 1.0f, stimulus.m_ucInheritanceMask & TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Peak ? inheritCol : textCol,
				                false, "Peak: %.3f", stimulus.m_fPeak);
				dc->Draw2dLabel(fColumnX + 80.0f, fColumnY, 1.0f, stimulus.m_fAttack == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE ? unusedCol : (stimulus.m_ucInheritanceMask & TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Attack ? inheritCol : textCol),
				                false, "Attack: %.3f", stimulus.m_fAttack);
				dc->Draw2dLabel(fColumnX + 150.0f, fColumnY, 1.0f, stimulus.m_fDecay == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE ? unusedCol : (stimulus.m_ucInheritanceMask & TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Decay ? inheritCol : textCol),
				                false, "Decay: %.3f", stimulus.m_fDecay);
				dc->Draw2dLabel(fColumnX + 220.0f, fColumnY, 1.0f, stimulus.m_fSustainRatio == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE ? unusedCol : (stimulus.m_ucInheritanceMask & TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Sustain ? inheritCol : textCol),
				                false, "Sustain: %.3f", stimulus.m_fSustainRatio);
				dc->Draw2dLabel(fColumnX + 290.0f, fColumnY, 1.0f, stimulus.m_fRelease == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE ? unusedCol : (stimulus.m_ucInheritanceMask & TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Release ? inheritCol : textCol),
				                false, "Release: %.3f", stimulus.m_fRelease);
				dc->Draw2dLabel(fColumnX + 360.0f, fColumnY, 1.0f, stimulus.m_fIgnore == TargetTrackHelpers::STargetTrackStimulusConfig::INVALID_VALUE ? unusedCol : (stimulus.m_ucInheritanceMask & TargetTrackHelpers::STargetTrackStimulusConfig::eIM_Ignore ? inheritCol : textCol),
				                false, "Release: %.3f", stimulus.m_fIgnore);
				fColumnY += 15.0f;
			}
			if (nMode == 3 || nMode > 4) // i.e., on '3' or '5'+
			{
				TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::const_iterator itPulse = stimulus.m_pulses.begin();
				TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::const_iterator itPulseEnd = stimulus.m_pulses.end();
				for (; itPulse != itPulseEnd; ++itPulse)
				{
					const TargetTrackHelpers::STargetTrackPulseConfig& pulse = itPulse->second;

					// Draw the pulses of the stimulus
					dc->Draw2dLabel(fColumnX + 10.0f, fColumnY, 1.0f, pulse.m_bInherited ? inheritCol : textCol, false, "Pulse: %s", pulse.m_sPulse.c_str());
					dc->Draw2dLabel(fColumnX + 150.0f, fColumnY, 1.0f, pulse.m_bInherited ? inheritCol : textCol, false, "Value: %.3f", pulse.m_fValue);
					dc->Draw2dLabel(fColumnX + 220.0f, fColumnY, 1.0f, pulse.m_bInherited ? inheritCol : textCol, false, "Duration: %.3f", pulse.m_fDuration);
					fColumnY += 15.0f;
				}
			}
			if (nMode >= 4)
			{
				TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::const_iterator itMod = stimulus.m_modifiers.begin();
				TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::const_iterator itModEnd = stimulus.m_modifiers.end();
				for (; itMod != itModEnd; ++itMod)
				{
					const TargetTrackHelpers::STargetTrackModifierConfig& modifier = itMod->second;
					const ITargetTrackModifier* pModifier = GetTargetTrackModifier(modifier.m_uId);
					assert(pModifier);

					// Draw the modifiers of the stimulus
					dc->Draw2dLabel(fColumnX + 10.0f, fColumnY, 1.0f, modifier.m_bInherited ? inheritCol : textCol, false, "Modifier: %s", pModifier->GetTag());
					dc->Draw2dLabel(fColumnX + 150.0f, fColumnY, 1.0f, modifier.m_bInherited ? inheritCol : textCol, false, "Value: %.3f", modifier.m_fValue);
					dc->Draw2dLabel(fColumnX + 220.0f, fColumnY, 1.0f, modifier.m_bInherited ? inheritCol : textCol, false, "Limit: %.3f", modifier.m_fLimit);
					fColumnY += 15.0f;
				}
			}
		}

		fColumnY += 10.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::DebugDrawTargets(int nMode, char const* szAgentName)
{
	if (nMode <= 0)
		return;

	if (szAgentName && szAgentName[0])
	{
		// Get group for this agent
		CAIObject* pAgent = gAIEnv.pAIObjectManager->GetAIObjectByName(szAgentName);
		if (pAgent)
		{
			const tAIObjectID aiObjectId = pAgent->GetAIObjectID();
			TAgentContainer::iterator itAgent = m_Agents.find(aiObjectId);
			if (itAgent != m_Agents.end())
			{
				CTargetTrackGroup* pGroup = itAgent->second;
				assert(pGroup);

				const int nTargetedCount = (nMode == 1
				                            ? GetDesiredTargetCount(aiObjectId)
				                            : GetPotentialTargetCount(aiObjectId)
				                            );

				pGroup->DebugDrawTargets(nMode, nTargetedCount, true);
			}
		}
	}
	else
	{
		TAgentContainer::iterator itAgent = m_Agents.begin();
		TAgentContainer::iterator itAgentEnd = m_Agents.end();
		for (; itAgent != itAgentEnd; ++itAgent)
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);

			tAIObjectID aiObjectId = pGroup->GetAIObjectID();

			const int nTargetedCount = (nMode == 1
			                            ? GetDesiredTargetCount(aiObjectId)
			                            : GetPotentialTargetCount(aiObjectId)
			                            );

			pGroup->DebugDrawTargets(nMode, nTargetedCount);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackManager::DebugDrawAgent(char const* szAgentName)
{
	const tAIObjectID uLastDebugAgent = m_uLastDebugAgent;
	m_uLastDebugAgent = 0;

	if (szAgentName && szAgentName[0])
	{
		// Get group for this agent
		CAIObject* pAgent = gAIEnv.pAIObjectManager->GetAIObjectByName(szAgentName);
		if (pAgent)
		{
			const tAIObjectID uAgentObjectId = pAgent->GetAIObjectID();
			TAgentContainer::iterator itAgent = m_Agents.find(uAgentObjectId);
			if (itAgent != m_Agents.end())
			{
				CTargetTrackGroup* pGroup = itAgent->second;
				assert(pGroup);

				pGroup->DebugDrawTracks(m_pTrackConfigProxy, false);
				m_uLastDebugAgent = uAgentObjectId;
			}
		}
	}

	if (m_uLastDebugAgent != uLastDebugAgent && uLastDebugAgent > 0)
	{
		TAgentContainer::iterator itAgent = m_Agents.find(uLastDebugAgent);
		if (itAgent != m_Agents.end())
		{
			CTargetTrackGroup* pGroup = itAgent->second;
			assert(pGroup);

			pGroup->DebugDrawTracks(m_pTrackConfigProxy, true);
		}
	}
}
#endif //TARGET_TRACK_DEBUG
