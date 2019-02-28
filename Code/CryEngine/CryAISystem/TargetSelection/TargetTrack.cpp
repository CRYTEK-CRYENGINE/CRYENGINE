// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Maintains an interest level of an agent's perception on a
        target, used for determining the agent's target

   -------------------------------------------------------------------------
   History:
   - 02:01:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "TargetTrack.h"
#include "TargetTrackModifiers.h"
#include "PipeUser.h"
#include "ObjectContainer.h"
#include "AIVehicle.h"

#ifdef TARGET_TRACK_DEBUG
	#include "DebugDrawContext.h"
#endif //TARGET_TRACK_DEBUG

// Error ratio beyond the agent's update rate to consider the stimulus invocation to still be running
// A higher value means we lessen the impact of thrashing via the invocation turning on/off rapidly,
//	but it has the impact of introducing latency
static const float TARGET_TRACK_RUNNING_THRESHOLD = 0.01f;

//////////////////////////////////////////////////////////////////////////
bool CTargetTrack::SStimulusInvocation::IsRunning(float fUpdateInterval) const
{
	bool bResult = m_bMustRun;
	if (!bResult)
	{
		CAISystem* pAISystem = GetAISystem();
		assert(pAISystem);

		const float fCurrTime = pAISystem->GetFrameStartTimeSeconds();
		bResult = (fCurrTime - m_envelopeData.m_fLastInvokeTime - fUpdateInterval * 2.0f <= TARGET_TRACK_RUNNING_THRESHOLD);
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::SStimulusInvocation::SPulseTrigger::Serialize(TSerialize ser)
{
	ser.Value("uPulseNameHash", uPulseNameHash);
	ser.Value("fTriggerTime", fTriggerTime);
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::SStimulusInvocation::Serialize(TSerialize ser)
{
	ser.Value("m_vLastPos", m_vLastPos);
	ser.Value("m_vLastDir", m_vLastDir);
	ser.Value("m_fCurrentValue", m_envelopeData.m_fCurrentValue);
	ser.Value("m_fStartTime", m_envelopeData.m_fStartTime);
	ser.Value("m_fLastInvokeTime", m_envelopeData.m_fLastInvokeTime);
	ser.Value("m_fLastRunningValue", m_envelopeData.m_fLastRunningValue);
	ser.Value("m_fLastReleasingValue", m_envelopeData.m_fLastReleasingValue);
	ser.Value("m_pulseTriggers", m_pulseTriggers);
	ser.EnumValue("m_eTargetThreat", m_eTargetThreat, AITHREAT_NONE, AITHREAT_LAST);
	ser.EnumValue("m_eTargetContextType", m_eTargetContextType, AITARGET_CONTEXT_UNKNOWN, AITARGET_CONTEXT_LAST);

	uint8 stimulusType = (int8)m_eStimulusType;
	ser.Value("m_eStimulusType", stimulusType);
	m_eStimulusType = (TargetTrackHelpers::EAIEventStimulusType)stimulusType;
}

//////////////////////////////////////////////////////////////////////////
CTargetTrack::CTargetTrack()
	: m_vTargetPos(ZERO)
	, m_vTargetDir(ZERO)
	, m_eTargetType(AITARGET_NONE)
	, m_eTargetContextType(AITARGET_CONTEXT_UNKNOWN)
	, m_eTargetThreat(AITHREAT_NONE)
	, m_uConfigHash(0)
	, m_iLastUpdateFrame(0)
	, m_fTrackValue(0.0f)
	, m_fFirstVisualTime(0.0f)
	, m_fLastVisualTime(0.0f)
	, m_fThreatRatio(0.0f)
{
#ifdef TARGET_TRACK_DEBUG
	m_fLastDebugDrawTime = 0.0f;
	m_uDebugGraphIndex = 0;
#endif //TARGET_TRACK_DEBUG
}

//////////////////////////////////////////////////////////////////////////
CTargetTrack::~CTargetTrack()
{

}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::Init(tAIObjectID aiGroupOwnerId, tAIObjectID aiObjectId, uint32 uConfigHash)
{
	assert(aiGroupOwnerId > 0);
	assert(uConfigHash > 0);

	m_groupOwner = gAIEnv.pObjectContainer->GetWeakRef(aiGroupOwnerId);
	m_object = gAIEnv.pObjectContainer->GetWeakRef(aiObjectId);
	m_uConfigHash = uConfigHash;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::ResetForPool()
{
	m_vTargetPos.zero();
	m_vTargetDir.zero();
	m_eTargetType = AITARGET_NONE;
	m_eTargetContextType = AITARGET_CONTEXT_UNKNOWN;
	m_eTargetThreat = AITHREAT_NONE;

	m_groupOwner.Reset();
	m_object.Reset();
	m_uConfigHash = 0;

	m_fLastVisualTime = 0.0f;

	m_iLastUpdateFrame = 0;
	m_fTrackValue = 0.0f;
	m_fFirstVisualTime = 0.0f;
	m_fThreatRatio = 0.0f;
	m_StimuliInvocations.clear();
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::Serialize(TSerialize ser)
{
	ser.Value("m_fTrackValue", m_fTrackValue);
	ser.Value("m_fFirstVisualTime", m_fFirstVisualTime);
	ser.Value("m_fThreatRatio", m_fThreatRatio);
	ser.Value("m_StimuliInvocations", m_StimuliInvocations);

	if (ser.IsReading())
		m_iLastUpdateFrame = 0;
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrack::GetHighestEnvelopeValue() const
{
	float fHighestEnvelopeValue = 0.0f;

	TStimuliInvocationContainer::const_iterator itStimulusInvoke = m_StimuliInvocations.begin();
	TStimuliInvocationContainer::const_iterator itStimulusInvokeEnd = m_StimuliInvocations.end();
	for (; itStimulusInvoke != itStimulusInvokeEnd; ++itStimulusInvoke)
	{
		const SStimulusInvocation& invoke = itStimulusInvoke->second;
		fHighestEnvelopeValue = max(fHighestEnvelopeValue, invoke.m_envelopeData.m_fCurrentValue);
	}

	return fHighestEnvelopeValue;
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrack::GetUpdateInterval() const
{
	CPipeUser* pPipeUser = CastToCPipeUserSafe(m_groupOwner.GetAIObject());
	if (pPipeUser)
	{
		return pPipeUser->GetLastUpdateInterval();
	}

	return GetAISystem()->GetUpdateInterval();
}

//////////////////////////////////////////////////////////////////////////
CWeakRef<CAIObject> CTargetTrack::GetAITarget() const
{
	// Check if has vehicle attached to it and use it if possible
	CAIActor* pAIActor = CastToCAIActorSafe(m_object.GetAIObject());
	IAIActorProxy* pAIProxy = (pAIActor ? pAIActor->GetProxy() : NULL);
	if (pAIProxy)
	{
		EntityId vehicleId = pAIProxy->GetLinkedVehicleEntityId();
		IEntity* pVehicle = (vehicleId > 0 ? gEnv->pEntitySystem->GetEntity(vehicleId) : NULL);

		if (pVehicle)
		{
			CWeakRef<CAIObject> refVehicle = gAIEnv.pObjectContainer->GetWeakRef(pVehicle->GetAIObjectID());

			CAIObject* vehicleAIObject = refVehicle.GetAIObject();
			CAIVehicle* aiVehicle = vehicleAIObject ? vehicleAIObject->CastToCAIVehicle() : 0;
			CAIActor* driver = aiVehicle ? aiVehicle->GetDriver() : 0;

			if (driver == pAIActor)
			{
				assert(refVehicle.IsValid());
				return refVehicle;
			}
		}
	}

	return m_object;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrack::Update(float fCurrTime, TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CAISystem* pAISystem = GetAISystem();
	assert(pAISystem);

	bool bHasTarget = true;
	bool containsNonVisualStimulus = false;

	const int iFrameId = pAISystem->GetAITickCount();
	if (iFrameId != m_iLastUpdateFrame)
	{
		bHasTarget = false;
		m_iLastUpdateFrame = iFrameId;

		float fLastInvokeTime = 0.0f;
		float fLastStartInvoke = 0.0f;
		m_fTrackValue = 0.0f;
		EAITargetType eNewTargetType = AITARGET_NONE;
		EAITargetContextType eNewTargetSubType = AITARGET_CONTEXT_UNKNOWN;
		EAITargetThreat eNewTargetThreat = AITHREAT_NONE;

		// For a brief moment after the AI looses a visual target, the memory
		// position will be kept up to date. This is awesome for two reasons:
		// 1. The player can't quickly run closely by the AI and hide
		//    behind without the AI knowing where the player is. Win!
		// 2. When the player runs behind an object, the AI has an idea
		//    where the player is going to pop out. Cool!
		const float timeSinceVisual = fCurrTime - m_fLastVisualTime;
		const float intuitionTime = 1.0f;
		const bool intuition = (m_eTargetType == AITARGET_MEMORY) && (timeSinceVisual < intuitionTime);
		CAIObject* target = m_object.GetAIObject();

		TStimuliInvocationContainer::iterator itStimulusInvoke = m_StimuliInvocations.begin();
		TStimuliInvocationContainer::iterator itStimulusInvokeEnd = m_StimuliInvocations.end();
		for (; itStimulusInvoke != itStimulusInvokeEnd; ++itStimulusInvoke)
		{
			const uint32 uStimulusHash = itStimulusInvoke->first;
			SStimulusInvocation& invoke = itStimulusInvoke->second;

			const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig = NULL;
			if (pConfigProxy->GetTargetTrackStimulusConfig(m_uConfigHash, uStimulusHash, pStimulusConfig))
			{
				SStimData stimData;
				const float fStimulusValue = UpdateStimulusValue(fCurrTime, invoke, pStimulusConfig, pConfigProxy, stimData);

				if (intuition && target)
				{
					invoke.m_vLastPos = target->GetPos();
					invoke.m_vLastDir = target->GetEntityDir();
				}

				// Update position to most recent invocation's info
				if (invoke.m_envelopeData.m_fLastInvokeTime > fLastInvokeTime)
				{
					fLastInvokeTime = invoke.m_envelopeData.m_fLastInvokeTime;

					m_vTargetPos = invoke.m_vLastPos;
					m_vTargetDir = invoke.m_vLastDir;
				}

				//Context uses the most recent
				if (invoke.m_envelopeData.m_fStartTime > fLastStartInvoke)
				{
					fLastStartInvoke = invoke.m_envelopeData.m_fStartTime;
					eNewTargetSubType = invoke.m_eTargetContextType;
				}

				//Note if at least one active invocation is not visual
				if (fStimulusValue > 0.0f && invoke.m_eStimulusType != TargetTrackHelpers::eEST_Visual)
				{
					containsNonVisualStimulus = true;
				}

				// Track value and threat uses the highest
				m_fTrackValue = max(fStimulusValue, m_fTrackValue);

#ifdef TARGET_TRACK_DOTARGETTHREAT
				eNewTargetThreat = max(eNewTargetThreat, invoke.m_eTargetThreat);

				const float currentRatio = (fabs(pStimulusConfig->m_fPeak) > FLT_EPSILON) ? stimData.envelopeValue / pStimulusConfig->m_fPeak : 1.0f; // Prevent div by 0

				TargetTrackHelpers::STargetTrackStimulusConfig::TThreatLevelContainer::const_iterator threatLevel = pStimulusConfig->m_threatLevels.begin();
				TargetTrackHelpers::STargetTrackStimulusConfig::TThreatLevelContainer::const_iterator threatLevelEnd = pStimulusConfig->m_threatLevels.end();

				for (; threatLevel != threatLevelEnd; ++threatLevel)
				{
					const float threatLevelRatio = threatLevel->second;
					if (currentRatio <= threatLevelRatio)
					{
						invoke.m_eTargetThreat = threatLevel->first;
						break;
					}
				}
#endif  //TARGET_TRACK_DOTARGETTHREAT

#ifdef TARGET_TRACK_DOTARGETTYPE
				bHasTarget = UpdateTargetType(eNewTargetType, eNewTargetThreat, invoke);

#endif  //TARGET_TRACK_DOTARGETTYPE
			}

			SStimulusInvocation::TPulseTriggersContainer::iterator itNewEnd = std::remove_if(invoke.m_pulseTriggers.begin(), invoke.m_pulseTriggers.end(), SStimulusInvocation::SPulseTrigger::IsObsolete);
			invoke.m_pulseTriggers.erase(itNewEnd, invoke.m_pulseTriggers.end());
		}

		m_eTargetThreat = eNewTargetThreat;
		m_eTargetType = eNewTargetType;
		m_eTargetContextType = eNewTargetSubType;
	}

	if (!bHasTarget || m_eTargetType != AITARGET_VISUAL)
	{
		m_fFirstVisualTime = 0.0f;
	}
	else if (m_fFirstVisualTime <= 0.0f)  //  && (m_eTargetType == AITARGET_VISUAL)
	{
		m_fFirstVisualTime = fCurrTime;
	}

	if (m_eTargetType == AITARGET_VISUAL)
	{
		m_fLastVisualTime = fCurrTime;
	}

#ifdef TARGET_TRACK_DOTARGETTHREAT
	if (bHasTarget && pConfigProxy)
	{
		ModifyTargetThreat(pConfigProxy);

		//If this memory has decayed to no threat, then remove it from the target track if there is something else stronger
		if (m_eTargetType == AITARGET_MEMORY && m_fThreatRatio < FLT_EPSILON && containsNonVisualStimulus)
		{
			//Kill ALL visual stimullii on this track. Allows AI which has not gone aggresive to target, to lose weak memories. - Morgan 01/12/2011
			TStimuliInvocationContainer::iterator itStimulusInvoke = m_StimuliInvocations.begin();
			for (; itStimulusInvoke != m_StimuliInvocations.end(); ++itStimulusInvoke)
			{
				SStimulusInvocation& invoke = itStimulusInvoke->second;
				if (invoke.m_eStimulusType == TargetTrackHelpers::eEST_Visual)
				{
					invoke.m_envelopeData.m_fCurrentValue = 0.0f;
					invoke.m_envelopeData.m_fLastRunningValue = 0.0f;
				}
			}
		}
	}
	else
	{
		m_fThreatRatio = 0.0f;
	}
#endif //TARGET_TRACK_DOTARGETTHREAT

	return bHasTarget;
}

//////////////////////////////////////////////////////////////////////////
#ifdef TARGET_TRACK_DOTARGETTHREAT
void CTargetTrack::ModifyTargetThreat(TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy)
{
	assert(pConfigProxy);

	CAIObject* pOwner = m_groupOwner.GetAIObject();
	CAIObject* pTarget = m_object.GetAIObject();

	const bool modifyTargetTrack = (pOwner != NULL) && (pTarget != NULL);
	if (modifyTargetTrack)
	{
		pConfigProxy->ModifyTargetThreat(*pOwner, *pTarget, *this, m_fThreatRatio, m_eTargetThreat);
	}
}
#endif //TARGET_TRACK_DOTARGETTHREAT

//////////////////////////////////////////////////////////////////////////
#ifdef TARGET_TRACK_DOTARGETTYPE
bool CTargetTrack::UpdateTargetType(EAITargetType& outTargetType, EAITargetThreat eTargetThreat, const SStimulusInvocation& invoke)
{
	bool bResult = false;

	// Update the target type
	switch (invoke.m_eStimulusType)
	{
	case TargetTrackHelpers::eEST_Visual:
		{
			if (outTargetType < AITARGET_VISUAL)
			{
				outTargetType = (invoke.IsRunning(GetUpdateInterval()) && invoke.m_eTargetThreat == AITHREAT_AGGRESSIVE ? AITARGET_VISUAL : AITARGET_MEMORY);
			}
		}
		break;

	case TargetTrackHelpers::eEST_Sound:
	case TargetTrackHelpers::eEST_BulletRain:
	case TargetTrackHelpers::eEST_Generic:
		{
			if (outTargetType < AITARGET_SOUND)
				outTargetType = AITARGET_SOUND;
		}
		break;
	}

	bResult = (outTargetType != AITARGET_NONE);

	return bResult;
}
#endif //TARGET_TRACK_DOTARGETTYPE

//////////////////////////////////////////////////////////////////////////
bool CTargetTrack::InvokeStimulus(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (uStimulusNameHash > 0)
	{
		{
			TStimuliInvocationContainer::iterator itInvoke = m_StimuliInvocations.find(uStimulusNameHash);

			if (itInvoke != m_StimuliInvocations.end())
			{
				SStimulusInvocation& invoke = itInvoke->second;
				UpdateStimulusInvoke(invoke, stimulusEvent);

				return true;
			}
		}

		SStimulusInvocation invoke;
		UpdateStimulusInvoke(invoke, stimulusEvent);
		m_StimuliInvocations[uStimulusNameHash] = invoke;

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTargetTrack::TriggerPulse(uint32 uStimulusNameHash, uint32 uPulseNameHash)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (uStimulusNameHash > 0 && uPulseNameHash > 0)
	{
		{
			TStimuliInvocationContainer::iterator itInvoke = m_StimuliInvocations.find(uStimulusNameHash);

			if (itInvoke != m_StimuliInvocations.end())
			{
				SStimulusInvocation& invoke = itInvoke->second;
				UpdateStimulusPulse(invoke, uPulseNameHash);

				return true;
			}
		}

		SStimulusInvocation invoke;
		UpdateStimulusPulse(invoke, uPulseNameHash);
		m_StimuliInvocations[uStimulusNameHash] = invoke;

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::UpdateStimulusInvoke(SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent) const
{
	CAISystem* pAISystem = GetAISystem();
	assert(pAISystem);

	const float fCurrTime = pAISystem->GetFrameStartTimeSeconds();

	if (!invoke.IsRunning(GetUpdateInterval()))
	{
		//If the stimulus had a previous non-zero running value, then this stimululs was reinvoked.
		if (invoke.m_envelopeData.m_fLastRunningValue > 0.0f)
		{
			invoke.m_envelopeData.m_bReinvoked = true;
		}
		//Otherwsie make sure to clear the reinvoke flag
		else
		{
			invoke.m_envelopeData.m_bReinvoked = false;
		}
		invoke.m_envelopeData.m_fStartTime = fCurrTime;
	}

	CWeakRef<CAIObject> refTarget = gAIEnv.pObjectContainer->GetWeakRef(stimulusEvent.m_targetId);
	CAIObject* pTarget = refTarget.GetAIObject();

	if (!stimulusEvent.m_vTargetPos.IsZero())
	{
		invoke.m_vLastPos = stimulusEvent.m_vTargetPos;
	}
	else
	{
		if (pTarget)
			invoke.m_vLastPos = pTarget->GetPos();
		else
			CRY_ASSERT_MESSAGE(0, "No position could be set from invoked stimulus event!");
	}

	if (pTarget)
		invoke.m_vLastDir = pTarget->GetEntityDir();

	invoke.m_envelopeData.m_fLastInvokeTime = fCurrTime;
	invoke.m_eTargetThreat = stimulusEvent.m_eTargetThreat;
	invoke.m_eStimulusType = stimulusEvent.m_eStimulusType;

	if (stimulusEvent.m_sStimulusName == "SoundWeapon")
		invoke.m_eTargetContextType = AITARGET_CONTEXT_GUNFIRE;

	invoke.m_bMustRun = true;
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::UpdateStimulusPulse(SStimulusInvocation& invoke, uint32 uPulseNameHash) const
{
	assert(uPulseNameHash > 0);

	SStimulusInvocation::TPulseTriggersContainer::iterator itPulse = invoke.m_pulseTriggers.begin();
	SStimulusInvocation::TPulseTriggersContainer::iterator itPulseEnd = invoke.m_pulseTriggers.end();
	for (; itPulse != itPulseEnd; ++itPulse)
	{
		SStimulusInvocation::SPulseTrigger& pulseTrigger = *itPulse;
		if (pulseTrigger.uPulseNameHash == uPulseNameHash)
		{
			UpdatePulseValue(pulseTrigger);
			return;
		}
	}

	// Add new entry
	SStimulusInvocation::SPulseTrigger pulseTrigger(uPulseNameHash);
	UpdatePulseValue(pulseTrigger);
	invoke.m_pulseTriggers.push_back(pulseTrigger);
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrack::UpdatePulseValue(SStimulusInvocation::SPulseTrigger& pulseTrigger) const
{
	CAISystem* pAISystem = GetAISystem();
	assert(pAISystem);

	const float fCurrTime = pAISystem->GetFrameStartTimeSeconds();

	pulseTrigger.fTriggerTime = fCurrTime;
	pulseTrigger.bObsolete = false;
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrack::UpdateStimulusValue(float fCurrTime, SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig,
                                        TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy, SStimData& stimData)
{
	assert(pStimulusConfig);
	assert(pConfigProxy);

	const float fEnvelopeValue = GetStimulusEnvelopeValue(fCurrTime, invoke, pStimulusConfig);
	const float fPulseValue = GetStimulusPulseValue(fCurrTime, invoke, pStimulusConfig);
	const float fModValue = GetStimulusModifierValue(invoke, pConfigProxy, pStimulusConfig);

	stimData.envelopeValue = fEnvelopeValue;

	invoke.m_envelopeData.m_fCurrentValue = fEnvelopeValue;

	if (invoke.IsRunning(GetUpdateInterval()))
		invoke.m_envelopeData.m_fLastRunningValue = fEnvelopeValue;
	else
		invoke.m_envelopeData.m_fLastReleasingValue = fEnvelopeValue;

	invoke.m_bMustRun = false;

	return GetStimulusTotalValue(fCurrTime, fEnvelopeValue, fPulseValue, fModValue);
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrack::GetStimulusTotalValue(float fCurrTime, float fEnvelopeValue, float fPulseValue, float fModValue) const
{
	return (fEnvelopeValue + fPulseValue) * fModValue;
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrack::GetStimulusEnvelopeValue(float fCurrTime, const SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pStimulusConfig);

	if (invoke.IsRunning(GetUpdateInterval()))
	{
		// Check if this stimulus should be ignored for a brief startup time when freshly invoked (new invoke or invocation from zero Stimulus)
		const bool bUseIgnoreTime = (invoke.m_envelopeData.m_fLastReleasingValue < FLT_EPSILON && !invoke.m_envelopeData.m_bReinvoked);
		const float fIgnoreEnd = invoke.m_envelopeData.m_fStartTime + (bUseIgnoreTime ? pStimulusConfig->m_fIgnore : 0.0f);
		if (bUseIgnoreTime && fCurrTime < fIgnoreEnd)
		{
			return 0.0f;
		}

		//Attack
		const float fAttackEnd = fIgnoreEnd + pStimulusConfig->m_fAttack;
		//If the stimulus defines an attack value and we are in the attack period
		if (pStimulusConfig->m_fAttack && fCurrTime <= fAttackEnd)
		{
			const float fDuration = fAttackEnd - fIgnoreEnd;
			const float fAttackRatio = (fCurrTime - fIgnoreEnd) / (fDuration > FLT_EPSILON ? fDuration : 1.0f);
			return invoke.m_envelopeData.m_fLastReleasingValue + (pStimulusConfig->m_fPeak - invoke.m_envelopeData.m_fLastReleasingValue) * fAttackRatio;
		}

		// Decay
		const float fDecayEnd = fAttackEnd + pStimulusConfig->m_fDecay;
		//If the stimulus defines a decay value and we are in the decay period
		if (pStimulusConfig->m_fDecay && fCurrTime <= fDecayEnd)
		{
			const float fDuration = fDecayEnd - fAttackEnd;
			const float fDecayRatio = (fCurrTime - fAttackEnd) / (fDuration > FLT_EPSILON ? fDuration : 1.0f);
			return (pStimulusConfig->m_fPeak - fDecayRatio * (pStimulusConfig->m_fPeak - pStimulusConfig->m_fPeak * pStimulusConfig->m_fSustainRatio));
		}

		// Sustain
		return pStimulusConfig->m_fPeak * pStimulusConfig->m_fSustainRatio;
	}

	// Release
	const float fReleaseStart = invoke.m_envelopeData.m_fLastInvokeTime;
	const float fReleaseEnd = fReleaseStart + pStimulusConfig->m_fRelease;
	const float fReleaseDuration = fReleaseEnd - fReleaseStart;
	const float fDeltaTime = fCurrTime - fReleaseStart;
	const float fReleaseRatio = (fReleaseDuration > FLT_EPSILON) ? (fDeltaTime / fReleaseDuration) : fDeltaTime;
	return max(0.0f, invoke.m_envelopeData.m_fLastRunningValue - fReleaseRatio * invoke.m_envelopeData.m_fLastRunningValue);
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrack::GetStimulusPulseValue(float fCurrTime, const SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pStimulusConfig);

	// Apply combined pulse value
	float fPulseValue = 0.0f;
	SStimulusInvocation::TPulseTriggersContainer::const_iterator itPulse = invoke.m_pulseTriggers.begin();
	SStimulusInvocation::TPulseTriggersContainer::const_iterator itPulseEnd = invoke.m_pulseTriggers.end();
	for (; itPulse != itPulseEnd; ++itPulse)
	{
		const SStimulusInvocation::SPulseTrigger& pulseTrigger = *itPulse;

		TargetTrackHelpers::STargetTrackStimulusConfig::TPulseContainer::const_iterator itPulseDef = pStimulusConfig->m_pulses.find(pulseTrigger.uPulseNameHash);
		if (itPulseDef != pStimulusConfig->m_pulses.end())
		{
			const TargetTrackHelpers::STargetTrackPulseConfig& pulseDef = itPulseDef->second;

			const float fDT = fCurrTime - pulseTrigger.fTriggerTime;
			const float fRatio = clamp_tpl((pulseDef.m_fDuration > FLT_EPSILON ? 1.0f - fDT / pulseDef.m_fDuration : 0.0f), 0.0f, 1.0f);
			fPulseValue += pulseDef.m_fValue * fRatio;

			if (fRatio <= 0.0f)
			{
				pulseTrigger.bObsolete = true;
			}
		}
	}

	return fPulseValue;
}

//////////////////////////////////////////////////////////////////////////
float CTargetTrack::GetStimulusModifierValue(const SStimulusInvocation& invoke, TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pStimulusConfig);
	assert(pConfigProxy);

	// Value is used as a product
	float fModValue = 1.0f;

	TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::const_iterator itMod = pStimulusConfig->m_modifiers.begin();
	TargetTrackHelpers::STargetTrackStimulusConfig::TModifierContainer::const_iterator itModEnd = pStimulusConfig->m_modifiers.end();
	for (; itMod != itModEnd; ++itMod)
	{
		const TargetTrackHelpers::STargetTrackModifierConfig& modifierInfo = itMod->second;
		const ITargetTrackModifier* pModifier = pConfigProxy->GetTargetTrackModifier(modifierInfo.m_uId);

		if (pModifier)
		{
			fModValue *= pModifier->GetModValue(this, invoke.m_eStimulusType, invoke.m_vLastPos,
			                                    invoke.m_envelopeData, modifierInfo);
		}
	}

	return fModValue;
}

#ifdef TARGET_TRACK_DEBUG
//////////////////////////////////////////////////////////////////////////
void CTargetTrack::DebugDraw(CDebugDrawContext& dc, int iIndex, float& fColumnX, float& fColumnY, TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy) const
{
	CAIObject* pObject = m_object.GetAIObject();
	CAISystem* pAISystem = GetAISystem();
	assert(pAISystem);

	const float fCurrTime = pAISystem->GetFrameStartTimeSeconds();

	const ColorB textCol(255, 255, 255, 255);
	const ColorB textActiveCol(0, 128, 0, 255);

	dc->Draw2dLabel(fColumnX, fColumnY, 1.5f, iIndex > 0 ? textCol : textActiveCol, false, "Track \'%s\' [%.3f - %d]", pObject ? pObject->GetName() : "(Null)", m_fTrackValue, iIndex + 1);

	TStimuliInvocationContainer::const_iterator itStimulusInvoke = m_StimuliInvocations.begin();
	TStimuliInvocationContainer::const_iterator itStimulusInvokeEnd = m_StimuliInvocations.end();
	for (; itStimulusInvoke != itStimulusInvokeEnd; ++itStimulusInvoke)
	{
		fColumnY += 15.0f;

		const uint32 uStimulusHash = itStimulusInvoke->first;
		const SStimulusInvocation& invoke = itStimulusInvoke->second;
		string sStimulusName = "Unknown";
		float fStimulusValue = 0.0f;
		float fEnvelopeValue = 0.0f;
		float fPulseValue = 0.0f;
		float fModValue = 1.0f;

		const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig = NULL;
		if (pConfigProxy->GetTargetTrackStimulusConfig(m_uConfigHash, uStimulusHash, pStimulusConfig))
		{
			sStimulusName = pStimulusConfig->m_sStimulus;
			fEnvelopeValue = GetStimulusEnvelopeValue(fCurrTime, invoke, pStimulusConfig);
			fPulseValue = GetStimulusPulseValue(fCurrTime, invoke, pStimulusConfig);
			fModValue = GetStimulusModifierValue(invoke, pConfigProxy, pStimulusConfig);
			fStimulusValue = GetStimulusTotalValue(fCurrTime, fEnvelopeValue, fPulseValue, fModValue);
		}

		dc->Draw2dLabel(fColumnX + 5.0f, fColumnY, 1.2f, textCol, false, "Stimulus \'%s\' @ [%.3f : E = %.3f : P = %.3f : M = %.3f]", sStimulusName.c_str(), fStimulusValue, fEnvelopeValue, fPulseValue, fModValue);
		fColumnY += 15.0f;

		dc->Draw2dLabel(fColumnX + 10.0f, fColumnY, 1.0f, textCol, false, "Running: %d (P = %" PRISIZE_T ")", invoke.IsRunning(GetUpdateInterval()) ? 1 : 0, invoke.m_pulseTriggers.size());
		dc->Draw2dLabel(fColumnX + 150.0f, fColumnY, 1.0f, textCol, false, "Start: %.3f", invoke.m_envelopeData.m_fStartTime);
		dc->Draw2dLabel(fColumnX + 220.0f, fColumnY, 1.0f, textCol, false, "Last: %.3f", invoke.m_envelopeData.m_fLastInvokeTime);
	}
}
#endif //TARGET_TRACK_DEBUG
