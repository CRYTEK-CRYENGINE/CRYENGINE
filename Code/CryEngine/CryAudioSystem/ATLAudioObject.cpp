// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ATLAudioObject.h"
#include "SoundCVars.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <Cry3DEngine/ISurfaceType.h>

using namespace CryAudio::Impl;

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::Release()
{
	m_activeEvents.clear();
	m_triggerImplStates.clear();

	for (auto& triggerStatesPair : m_triggerStates)
	{
		triggerStatesPair.second.numLoadingEvents = 0;
		triggerStatesPair.second.numPlayingEvents = 0;
	}

	m_pImplData = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartingTriggerInstance(AudioTriggerInstanceId const audioTriggerInstanceId, AudioControlId const audioTriggerId)
{
	SAudioTriggerInstanceState& audioTriggerInstanceState = m_triggerStates.insert(std::make_pair(audioTriggerInstanceId, SAudioTriggerInstanceState())).first->second;
	audioTriggerInstanceState.audioTriggerId = audioTriggerId;
	audioTriggerInstanceState.flags |= eAudioTriggerStatus_Starting;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartedTriggerInstance(
  AudioTriggerInstanceId const audioTriggerInstanceId,
  void* const pOwnerOverride,
  void* const pUserData,
  void* const pUserDataOwner,
  AudioEnumFlagsType const flags)
{
	ObjectTriggerStates::iterator iter(m_triggerStates.find(audioTriggerInstanceId));

	if (iter != m_triggerStates.end())
	{
		SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;

		if (audioTriggerInstanceState.numPlayingEvents > 0 || audioTriggerInstanceState.numLoadingEvents > 0)
		{
			audioTriggerInstanceState.pOwnerOverride = pOwnerOverride;
			audioTriggerInstanceState.pUserData = pUserData;
			audioTriggerInstanceState.pUserDataOwner = pUserDataOwner;
			audioTriggerInstanceState.flags &= ~eAudioTriggerStatus_Starting;
			audioTriggerInstanceState.flags |= eAudioTriggerStatus_Playing;

			if ((flags & eAudioRequestFlags_SyncFinishedCallback) == 0)
			{
				audioTriggerInstanceState.flags |= eAudioTriggerStatus_CallbackOnAudioThread;
			}
		}
		else
		{
			// All of the events have either finished before we got here or never started.
			// So we report this trigger as finished immediately.
			ReportFinishedTriggerInstance(iter);
		}
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Warning, "Reported a started instance %u that couldn't be found on an object %u",
		                  audioTriggerInstanceId, GetId());
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartedEvent(CATLEvent* const _pEvent)
{
	m_activeEvents.insert(_pEvent);
	m_triggerImplStates.insert(std::make_pair(_pEvent->m_audioTriggerImplId, SAudioTriggerImplState()));

	// Update the radius where events playing in this audio object are audible
	if (_pEvent->m_pTrigger)
	{
		m_maxRadius = std::max(_pEvent->m_pTrigger->m_maxRadius, m_maxRadius);
	}

	ObjectTriggerStates::iterator const iter(m_triggerStates.find(_pEvent->m_audioTriggerInstanceId));

	if (iter != m_triggerStates.end())
	{
		SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;

		switch (_pEvent->m_audioEventState)
		{
		case eAudioEventState_Playing:
			{
				++(audioTriggerInstanceState.numPlayingEvents);

				break;
			}
		case eAudioEventState_PlayingDelayed:
			{
				CRY_ASSERT(audioTriggerInstanceState.numLoadingEvents > 0);
				--(audioTriggerInstanceState.numLoadingEvents);
				++(audioTriggerInstanceState.numPlayingEvents);
				_pEvent->m_audioEventState = eAudioEventState_Playing;

				break;
			}
		case eAudioEventState_Loading:
			{
				++(audioTriggerInstanceState.numLoadingEvents);

				break;
			}
		case eAudioEventState_Unloading:
			{
				// not handled currently
				break;
			}
		default:
			{
				// unknown event state!
				CRY_ASSERT(false);

				break;
			}
		}
	}
	else
	{
		// must exist!
		CRY_ASSERT(false);
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportFinishedEvent(CATLEvent* const _pEvent, bool const _bSuccess)
{
	m_activeEvents.erase(_pEvent);
	m_triggerImplStates.erase(_pEvent->m_audioTriggerImplId);

	// recalculate the max radius of the audio object
	m_maxRadius = 0.0f;
	for (auto const pEvent : m_activeEvents)
	{
		if (pEvent->m_pTrigger)
		{
			m_maxRadius = std::max(pEvent->m_pTrigger->m_maxRadius, m_maxRadius);
		}
	}

	ObjectTriggerStates::iterator iter(m_triggerStates.begin());

	if (FindPlace(m_triggerStates, _pEvent->m_audioTriggerInstanceId, iter))
	{
		switch (_pEvent->m_audioEventState)
		{
		case eAudioEventState_Playing:
			{
				SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;
				CRY_ASSERT(audioTriggerInstanceState.numPlayingEvents > 0);

				if (--(audioTriggerInstanceState.numPlayingEvents) == 0 &&
				    audioTriggerInstanceState.numLoadingEvents == 0 &&
				    (audioTriggerInstanceState.flags & eAudioTriggerStatus_Starting) == 0)
				{
					ReportFinishedTriggerInstance(iter);
				}

				break;
			}
		case eAudioEventState_Loading:
			{
				if (_bSuccess)
				{
					ReportPrepUnprepTriggerImpl(_pEvent->m_audioTriggerImplId, true);
				}

				break;
			}
		case eAudioEventState_Unloading:
			{
				if (_bSuccess)
				{
					ReportPrepUnprepTriggerImpl(_pEvent->m_audioTriggerImplId, false);
				}

				break;
			}
		default:
			{
				// unknown event state!
				CRY_ASSERT(false);

				break;
			}
		}
	}
	else
	{
		if (_pEvent->m_pTrigger)
		{
			g_audioLogger.Log(eAudioLogType_Warning, "Reported finished event %u on an inactive trigger %u", _pEvent->GetId(), _pEvent->m_pTrigger->GetId());
		}
		else
		{
			g_audioLogger.Log(eAudioLogType_Warning, "Reported finished event %u on a trigger that does not exist anymore", _pEvent->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::GetStartedStandaloneFileRequestData(CATLStandaloneFile const* const _pStandaloneFile, CAudioRequestInternal& _request)
{
	ObjectStandaloneFileMap::const_iterator const iter(m_activeStandaloneFiles.find(_pStandaloneFile->GetId()));

	if (iter != m_activeStandaloneFiles.end())
	{
		SAudioStandaloneFileData const& audioStandaloneFileData = iter->second;
		_request.pOwner = audioStandaloneFileData.pOwnerOverride;
		_request.pUserData = audioStandaloneFileData.pUserData;
		_request.pUserDataOwner = audioStandaloneFileData.pUserDataOwner;
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartedStandaloneFile(
  CATLStandaloneFile const* const _pStandaloneFile,
  void* const _pOwner,
  void* const _pUserData,
  void* const _pUserDataOwner)
{
	m_activeStandaloneFiles.insert(
	  std::pair<AudioStandaloneFileId, SAudioStandaloneFileData>(
	    _pStandaloneFile->GetId(),
	    SAudioStandaloneFileData(
	      _pOwner,
	      _pUserData,
	      _pUserDataOwner,
	      _pStandaloneFile->m_id)));
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportFinishedStandaloneFile(CATLStandaloneFile const* const _pStandaloneFile)
{
	m_activeStandaloneFiles.erase(_pStandaloneFile->GetId());
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportPrepUnprepTriggerImpl(AudioTriggerImplId const audioTriggerImplId, bool const bPrepared)
{
	if (bPrepared)
	{
		m_triggerImplStates[audioTriggerImplId].flags |= eAudioTriggerStatus_Prepared;
	}
	else
	{
		m_triggerImplStates[audioTriggerImplId].flags &= ~eAudioTriggerStatus_Prepared;
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioSwitchStateId)
{
	m_switchStates[audioSwitchId] = audioSwitchStateId;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetRtpc(AudioControlId const audioRtpcId, float const value)
{
	m_rtpcs[audioRtpcId] = value;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetEnvironmentAmount(AudioEnvironmentId const audioEnvironmentId, float const amount)
{
	if (amount > 0.0f)
	{
		m_environments[audioEnvironmentId] = amount;
	}
	else
	{
		m_environments.erase(audioEnvironmentId);
	}
}

///////////////////////////////////////////////////////////////////////////
ObjectTriggerImplStates const& CATLAudioObject::GetTriggerImpls() const
{
	return m_triggerImplStates;
}

///////////////////////////////////////////////////////////////////////////
ObjectEnvironmentMap const& CATLAudioObject::GetEnvironments() const
{
	return m_environments;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ClearEnvironments()
{
	m_environments.clear();
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::Clear()
{
	m_activeEvents.clear();
	m_triggerStates.clear();
	m_triggerImplStates.clear();
	m_switchStates.clear();
	m_rtpcs.clear();
	m_environments.clear();
	m_attributes = SAudioObject3DAttributes();
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportFinishedTriggerInstance(ObjectTriggerStates::iterator& iter)
{
	SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;
	SAudioRequest request;
	SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance> requestData(audioTriggerInstanceState.audioTriggerId);
	request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ThreadSafePush | eAudioRequestFlags_SyncCallback;
	request.audioObjectId = GetId();
	request.pData = &requestData;
	request.pOwner = audioTriggerInstanceState.pOwnerOverride;
	request.pUserData = audioTriggerInstanceState.pUserData;
	request.pUserDataOwner = audioTriggerInstanceState.pUserDataOwner;

	if ((audioTriggerInstanceState.flags & eAudioTriggerStatus_CallbackOnAudioThread) > 0)
	{
		request.flags &= ~eAudioRequestFlags_SyncCallback;
	}

	gEnv->pAudioSystem->PushRequest(request);

	if ((audioTriggerInstanceState.flags & eAudioTriggerStatus_Prepared) > 0)
	{
		// if the trigger instance was manually prepared -- keep it
		audioTriggerInstanceState.flags &= ~eAudioTriggerStatus_Playing;
	}
	else
	{
		//if the trigger instance wasn't prepared -- kill it
		m_triggerStates.erase(iter);
	}
}

float const CATLAudioObject::CPropagationProcessor::SRayInfo::s_fSmoothingAlpha = 0.05f;

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::SRayInfo::Reset()
{
	totalSoundOcclusion = 0.0f;
	numHits = 0;
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	startPosition.zero();
	direction.zero();
	randomOffset.zero();
	averageHits = 0.0f;
	distanceToFirstObstacle = FLT_MAX;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

bool CATLAudioObject::CPropagationProcessor::s_bCanIssueRWIs = false;

///////////////////////////////////////////////////////////////////////////
int CATLAudioObject::CPropagationProcessor::OnObstructionTest(EventPhys const* pEvent)
{
	EventPhysRWIResult* const pRWIResult = (EventPhysRWIResult*)pEvent;

	if (pRWIResult->iForeignData == PHYS_FOREIGN_ID_SOUND_OBSTRUCTION)
	{
		CPropagationProcessor::SRayInfo* const pRayInfo = static_cast<CPropagationProcessor::SRayInfo*>(pRWIResult->pForeignData);

		if (pRayInfo != nullptr)
		{
			ProcessObstructionRay(pRWIResult->nHits, pRayInfo);

			SAudioRequest request;
			SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportProcessedObstructionRay> requestData(pRayInfo->audioObjectId, pRayInfo->rayId);
			request.flags = eAudioRequestFlags_ThreadSafePush;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}
		else
		{
			CRY_ASSERT(false);
		}
	}
	else
	{
		CRY_ASSERT(false);
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::ProcessObstructionRay(int const numHits, SRayInfo* const pRayInfo, bool const bReset /*= false*/)
{
	float totalOcclusion = 0.0f;
	int numRealHits = 0;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	float minDistance = FLT_MAX;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (numHits > 0)
	{
		ISurfaceTypeManager* const pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		size_t const count = min(static_cast<size_t>(numHits) + 1, AUDIO_MAX_RAY_HITS);

		for (size_t i = 0; i < count; ++i)
		{
			float const distance = pRayInfo->hits[i].dist;

			if (distance > 0.0f)
			{
				ISurfaceType* const pMat = pSurfaceTypeManager->GetSurfaceType(pRayInfo->hits[i].surface_idx);

				if (pMat != nullptr)
				{
					const ISurfaceType::SPhysicalParams& physParams = pMat->GetPhyscalParams();
					totalOcclusion += physParams.sound_obstruction;// not clamping b/w 0 and 1 for performance reasons
					++numRealHits;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					minDistance = min(minDistance, distance);
#endif    // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
		}
	}

	totalOcclusion = clamp_tpl(totalOcclusion, 0.0f, 1.0f);

	// If the num hits differs too much from the last ray, reduce the change in TotalOcclusion in inverse proportion.
	// This reduces thrashing at the boundaries of occluding entities.
	int const absoluteHitDiff = abs(numRealHits - pRayInfo->numHits);
	float const numHitCorrection = absoluteHitDiff > 1 ? 1.0f / absoluteHitDiff : 1.0f;

	pRayInfo->numHits = numRealHits;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	pRayInfo->distanceToFirstObstacle = minDistance;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (bReset)
	{
		pRayInfo->totalSoundOcclusion = totalOcclusion;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		pRayInfo->averageHits = static_cast<float>(pRayInfo->numHits);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
	else
	{
		pRayInfo->totalSoundOcclusion += numHitCorrection * (totalOcclusion - pRayInfo->totalSoundOcclusion) * SRayInfo::s_fSmoothingAlpha;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		pRayInfo->averageHits += (pRayInfo->numHits - pRayInfo->averageHits) * SRayInfo::s_fSmoothingAlpha;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
}

///////////////////////////////////////////////////////////////////////////
size_t CATLAudioObject::CPropagationProcessor::NumRaysFromCalcType(EAudioOcclusionType const occlusionType)
{
	AudioEnumFlagsType index = static_cast<AudioEnumFlagsType>(occlusionType);
	size_t numRays = 0;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (g_audioCVars.m_audioObjectsRayType > 0)
	{
		index = clamp_tpl<AudioEnumFlagsType>(static_cast<AudioEnumFlagsType>(g_audioCVars.m_audioObjectsRayType), eAudioOcclusionType_Ignore, eAudioOcclusionType_MultiRay);
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	switch (index)
	{
	case eAudioOcclusionType_Ignore:
		numRays = 0;
		break;
	case eAudioOcclusionType_SingleRay:
		numRays = 1;
		break;
	case eAudioOcclusionType_MultiRay:
		numRays = 5;
		break;
	case eAudioOcclusionType_None: // falls through
	default:
		CRY_ASSERT(false); // unknown ObstructionOcclusionCalcType
		break;
	}

	return numRays;
}

#define AUDIO_MAX_OBSTRUCTION_RAYS     static_cast<size_t>(5)
#define AUDIO_MIN_OBSTRUCTION_DISTANCE 0.3f

///////////////////////////////////////////////////////////////////////////
CATLAudioObject::CPropagationProcessor::CPropagationProcessor(
  AudioObjectId const _audioObjectId,
  CAudioObjectTransformation const& _position)
	: m_remainingRays(0)
	, m_totalRays(0)
	, m_obstructionValue(0.2f, 0.0001f)
	, m_occlusionValue(0.2f, 0.0001f)
	, m_position(_position)
	, m_currentListenerDistance(0.0f)
	, m_occlusionType(eAudioOcclusionType_None)

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	, m_rayDebugInfos(AUDIO_MAX_OBSTRUCTION_RAYS)
	, m_timeSinceLastUpdateMS(0.0f)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
{
	m_rayInfos.reserve(AUDIO_MAX_OBSTRUCTION_RAYS);

	for (size_t i = 0; i < AUDIO_MAX_OBSTRUCTION_RAYS; ++i)
	{
		m_rayInfos.push_back(SRayInfo(i, _audioObjectId));
	}
}

//////////////////////////////////////////////////////////////////////////
CATLAudioObject::CPropagationProcessor::~CPropagationProcessor()
{
	stl::free_container(m_rayInfos);
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::Update(float const deltaTime)
{
	m_obstructionValue.Update(deltaTime);
	m_occlusionValue.Update(deltaTime);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_timeSinceLastUpdateMS += deltaTime;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::SetObstructionOcclusionCalcType(EAudioOcclusionType const occlusionType)
{
	m_occlusionType = occlusionType;

	if (occlusionType == eAudioOcclusionType_Ignore)
	{
		// Reset the sound obstruction/occlusion when disabled.
		m_obstructionValue.Reset();
		m_occlusionValue.Reset();
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::GetPropagationData(SATLSoundPropagationData& propagationData) const
{
	propagationData.obstruction = m_obstructionValue.GetCurrent();
	propagationData.occlusion = m_occlusionValue.GetCurrent();
}

static inline float frand_symm()
{
	return cry_random(-1.0f, 1.0f);
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::RunObstructionQuery(
  CAudioObjectTransformation const& listenerPosition,
  bool const bSyncCall,
  bool const bReset /*= false*/)
{
	if (m_remainingRays == 0)
	{
		static Vec3 const up(0.0f, 0.0f, 1.0f);
		static float const minPeripheralRayOffset = 0.3f;
		static float const maxPeripheralRayOffset = 1.0f;
		static float const minRandomOffset = 0.05f;
		static float const maxRandomOffset = 0.5f;
		static float const minOffsetDistance = 1.0f;
		static float const maxOffsetDistance = 20.0f;

		// the previous query has already been processed
		m_totalRays = NumRaysFromCalcType(m_occlusionType);

		if (m_totalRays > 0)
		{
			Vec3 const& listener = listenerPosition.GetPosition();
			Vec3 const& object = m_position.GetPosition();
			Vec3 const diff = object - listener;

			float const distance = diff.GetLength();

			m_currentListenerDistance = distance;

			float const offsetParam = clamp_tpl((distance - minOffsetDistance) / (maxOffsetDistance - minOffsetDistance), 0.0f, 1.0f);
			float const offsetScale = maxPeripheralRayOffset * offsetParam + minPeripheralRayOffset * (1.0f - offsetParam);
			float const randomOffsetScale = maxRandomOffset * offsetParam + minRandomOffset * (1.0f - offsetParam);

			Vec3 const side = diff.GetNormalized() % up;

			// the 0th ray is always shot from the listener to the center of the source
			// the 0th ray only gets 1/2 of the random variation
			CastObstructionRay(
			  listener,
			  (up * frand_symm() + side * frand_symm()) * randomOffsetScale * 0.5f,
			  diff,
			  0,
			  bSyncCall,
			  bReset);

			if (m_totalRays > 1)
			{
				// rays 1 and 2 start below and above the listener and go towards the source
				CastObstructionRay(
				  listener - (up * offsetScale),
				  (up * frand_symm() + side * frand_symm()) * randomOffsetScale,
				  diff,
				  1,
				  bSyncCall,
				  bReset);
				CastObstructionRay(
				  listener + (up * offsetScale),
				  (up * frand_symm() + side * frand_symm()) * randomOffsetScale,
				  diff,
				  2,
				  bSyncCall,
				  bReset);

				// rays 3 and 4 start left and right of the listener and go towards the source
				CastObstructionRay(
				  listener - (side * offsetScale),
				  (up * frand_symm() + side * frand_symm()) * randomOffsetScale,
				  diff,
				  3,
				  bSyncCall,
				  bReset);
				CastObstructionRay(
				  listener + (side * offsetScale),
				  (up * frand_symm() + side * frand_symm()) * randomOffsetScale,
				  diff,
				  4,
				  bSyncCall,
				  bReset);
			}

			if (bSyncCall)
			{
				// If the ObstructionQuery was synchronous, calculate the new obstruction/occlusion values right away.
				// Reset the resulting values without smoothing if bReset is true.
				ProcessObstructionOcclusion(bReset);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::ReportRayProcessed(size_t const rayId)
{
	CRY_ASSERT(m_remainingRays > 0);                    // make sure there are rays remaining
	CRY_ASSERT((0 <= rayId) && (rayId <= m_totalRays)); // check RayID validity

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (m_remainingRays == 0)
	{
		CryFatalError("Negative ref or ray count on audio object %u", m_rayInfos[rayId].audioObjectId);
	}
#endif

	if (--m_remainingRays == 0)
	{
		ProcessObstructionOcclusion();
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::ReleasePendingRays()
{
	if (m_remainingRays > 0)
	{
		m_remainingRays = 0;
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::ProcessObstructionOcclusion(bool const bReset)
{
	if (m_totalRays > 0)
	{
		// the obstruction value always comes from the 0th ray, going directly from the listener to the source
		float obstruction = m_rayInfos[0].totalSoundOcclusion;
		float occlusion = 0.0f;

		if (m_currentListenerDistance > ATL_FLOAT_EPSILON)
		{
			if (m_totalRays > 1)
			{
				// the total occlusion value is the average of the occlusions of all of the peripheral rays
				for (size_t i = 1; i < m_totalRays; ++i)
				{
					occlusion += m_rayInfos[i].totalSoundOcclusion;
				}

				occlusion /= (m_totalRays - 1);
			}
			else
			{
				occlusion = obstruction;
			}

			//the obstruction effect gets less pronounced when the distance between the object and the listener increases
			obstruction *= min(g_audioCVars.m_fullObstructionMaxDistance / m_currentListenerDistance, 1.0f);

			// since the Obstruction filter is applied on top of Occlusion, make sure we only apply what's exceeding the occlusion value
			obstruction = max(obstruction - occlusion, 0.0f);
		}
		else
		{
			// sound is tracking the listener, there is no obstruction
			obstruction = 0.0f;
			occlusion = 0.0f;
		}

		m_obstructionValue.SetNewTarget(obstruction, bReset);
		m_occlusionValue.SetNewTarget(occlusion, bReset);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		if (m_timeSinceLastUpdateMS > 100.0f) // only re-sample the rays about 10 times per second for a smoother debug drawing
		{
			m_timeSinceLastUpdateMS = 0.0f;

			for (size_t i = 0; i < m_totalRays; ++i)
			{
				SRayInfo const& rayInfo = m_rayInfos[i];
				SRayDebugInfo& rayDebugInfo = m_rayDebugInfos[i];

				rayDebugInfo.begin = rayInfo.startPosition + rayInfo.randomOffset;
				rayDebugInfo.end = rayInfo.startPosition + rayInfo.randomOffset + rayInfo.direction;

				if (rayDebugInfo.stableEnd.IsZeroFast())
				{
					// to be moved to the PropagationProcessor Reset method
					rayDebugInfo.stableEnd = rayDebugInfo.end;
				}
				else
				{
					rayDebugInfo.stableEnd += (rayDebugInfo.end - rayDebugInfo.stableEnd) * 0.1f;
				}

				rayDebugInfo.distanceToNearestObstacle = rayInfo.distanceToFirstObstacle;
				rayDebugInfo.numHits = rayInfo.numHits;
				rayDebugInfo.averageHits = rayInfo.averageHits;
				rayDebugInfo.occlusionValue = rayInfo.totalSoundOcclusion;
			}
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::CastObstructionRay(
  Vec3 const& origin,
  Vec3 const& randomOffset,
  Vec3 const& direction,
  size_t const rayIndex,
  bool const bSyncCall,
  bool const bReset /*= false*/)
{
	static int const nPhysicsFlags = ent_water | ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain;
	SRayInfo& rayInfo = m_rayInfos[rayIndex];

	int const numHits = gEnv->pPhysicalWorld->RayWorldIntersection(
	  origin + randomOffset,
	  direction,
	  nPhysicsFlags,
	  bSyncCall ? rwi_pierceability0 : rwi_pierceability0 | rwi_queue,
	  rayInfo.hits,
	  static_cast<int>(AUDIO_MAX_RAY_HITS),
	  nullptr,
	  0,
	  &rayInfo,
	  PHYS_FOREIGN_ID_SOUND_OBSTRUCTION);

	if (bSyncCall)
	{
		ProcessObstructionRay(numHits, &rayInfo, bReset);
	}
	else
	{
		++m_remainingRays;
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	rayInfo.startPosition = origin;
	rayInfo.direction = direction;
	rayInfo.randomOffset = randomOffset;

	if (bSyncCall)
	{
		++s_totalSyncPhysRays;
	}
	else
	{
		++s_totalAsyncPhysRays;
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::Update(float const deltaTime, SAudioObject3DAttributes const& listenerAttributes)
{
	m_propagationProcessor.Update(deltaTime);

	if (CanRunObstructionOcclusion())
	{
		float const distance = (m_attributes.transformation.GetPosition() - listenerAttributes.transformation.GetPosition()).GetLength();

		if ((AUDIO_MIN_OBSTRUCTION_DISTANCE < distance) && (distance < g_audioCVars.m_occlusionMaxDistance))
		{
			// Make the physics ray cast call sync or async depending on the distance to the listener.
			bool const bSyncCall = (distance <= g_audioCVars.m_occlusionMaxSyncDistance);
			m_propagationProcessor.RunObstructionQuery(listenerAttributes.transformation, bSyncCall);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportPhysicsRayProcessed(size_t const rayId)
{
	m_propagationProcessor.ReportRayProcessed(rayId);
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetTransformation(CAudioObjectTransformation const& transformation)
{
	float const deltaTime = (g_lastMainThreadFrameStartTime - m_previousTime).GetSeconds();

	if (deltaTime > 0.0f)
	{
		m_attributes.transformation = transformation;
		m_attributes.velocity = (m_attributes.transformation.GetPosition() - m_previousAttributes.transformation.GetPosition()) / deltaTime;
		m_previousTime = g_lastMainThreadFrameStartTime;
		m_previousAttributes = m_attributes;
		m_flags |= eAudioObjectFlags_NeedsVelocityUpdate;
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetObstructionOcclusionCalc(EAudioOcclusionType const calcType)
{
	CRY_ASSERT(calcType != eAudioOcclusionType_None);
	m_propagationProcessor.SetObstructionOcclusionCalcType(calcType);
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ResetObstructionOcclusion(CAudioObjectTransformation const& listenerTransformation)
{
	// cast the obstruction rays synchronously and reset the obstruction/occlusion values
	// (instead of merely setting them as targets for the SmoothFloats)
	m_propagationProcessor.RunObstructionQuery(listenerTransformation, true, true);
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::GetPropagationData(SATLSoundPropagationData& propagationData)
{
	m_propagationProcessor.GetPropagationData(propagationData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReleasePendingRays()
{
	m_propagationProcessor.ReleasePendingRays();
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetDopplerTracking(bool const bEnable)
{
	if (bEnable)
	{
		m_previousAttributes = m_attributes;
		m_flags |= eAudioObjectFlags_TrackDoppler;
	}
	else
	{
		m_flags &= ~eAudioObjectFlags_TrackDoppler;
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetVelocityTracking(bool const bEnable)
{
	if (bEnable)
	{
		m_previousAttributes = m_attributes;
		m_flags |= eAudioObjectFlags_TrackVelocity;
	}
	else
	{
		m_flags &= ~eAudioObjectFlags_TrackVelocity;
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::UpdateControls(float const deltaTime, SAudioObject3DAttributes const& listenerAttributes)
{
	if ((m_flags & eAudioObjectFlags_TrackDoppler) > 0)
	{
		// Approaching positive, departing negative value.
		if (m_attributes.velocity.GetLengthSquared() > 0.0f || listenerAttributes.velocity.GetLengthSquared() > 0.0f)
		{
			Vec3 const relativeVelocityVec(m_attributes.velocity - listenerAttributes.velocity);
			float const relativeVelocity = -relativeVelocityVec.Dot((m_attributes.transformation.GetPosition() - listenerAttributes.transformation.GetPosition()).GetNormalized());

			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> requestData(SATLInternalControlIDs::objectDopplerRtpcId, relativeVelocity);

			request.audioObjectId = GetId();
			request.flags = eAudioRequestFlags_ThreadSafePush;
			request.pData = &requestData;
			gEnv->pAudioSystem->PushRequest(request);
			m_flags |= eAudioObjectFlags_NeedsDopplerUpdate;
		}
		else if ((m_flags & eAudioObjectFlags_NeedsDopplerUpdate) > 0)
		{
			m_attributes.velocity = ZERO;
			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> requestData(SATLInternalControlIDs::objectDopplerRtpcId, 0.0f);

			request.audioObjectId = GetId();
			request.flags = eAudioRequestFlags_ThreadSafePush;
			request.pData = &requestData;
			gEnv->pAudioSystem->PushRequest(request);
			m_flags &= ~eAudioObjectFlags_NeedsDopplerUpdate;
		}
	}

	if ((m_flags & eAudioObjectFlags_TrackVelocity) > 0)
	{
		if (m_attributes.velocity.GetLengthSquared() > 0.0f)
		{
			float const currentVelocity = m_attributes.velocity.GetLength();

			if (fabs(currentVelocity - m_previousVelocity) > g_audioCVars.m_velocityTrackingThreshold)
			{
				m_previousVelocity = currentVelocity;
				SAudioRequest request;
				SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> requestData(SATLInternalControlIDs::objectVelocityRtpcId, currentVelocity);

				request.audioObjectId = GetId();
				request.flags = eAudioRequestFlags_ThreadSafePush;
				request.pData = &requestData;
				gEnv->pAudioSystem->PushRequest(request);
			}
		}
		else if ((m_flags & eAudioObjectFlags_NeedsVelocityUpdate) > 0)
		{
			m_attributes.velocity = ZERO;
			m_previousVelocity = 0.0f;
			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> requestData(SATLInternalControlIDs::objectVelocityRtpcId, 0.0f);

			request.audioObjectId = GetId();
			request.flags = eAudioRequestFlags_ThreadSafePush;
			request.pData = &requestData;
			gEnv->pAudioSystem->PushRequest(request);
			m_flags &= ~eAudioObjectFlags_NeedsVelocityUpdate;
		}
	}

	if ((m_flags & (eAudioObjectFlags_TrackDoppler | eAudioObjectFlags_TrackVelocity)) > 0)
	{
		// Exponential decay towards zero.
		if (m_attributes.velocity.GetLengthSquared() > 0.0f)
		{
			float const deltaTime = (g_lastMainThreadFrameStartTime - m_previousTime).GetSeconds();
			float const decay = std::max(1.0f - deltaTime / 0.125f, 0.0f);
			m_attributes.velocity *= decay;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
bool CATLAudioObject::CanBeReleased() const
{
	return (m_flags & eAudioObjectFlags_DoNotRelease) == 0 &&
	       m_activeEvents.empty() &&
	       m_activeStandaloneFiles.empty() &&
	       !m_propagationProcessor.HasPendingRays();
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetFlag(EAudioObjectFlags const flag)
{
	m_flags |= flag;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::RemoveFlag(EAudioObjectFlags const flag)
{
	m_flags &= ~flag;
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CheckBeforeRemoval(CATLDebugNameStore const* const pDebugNameStore) const
{
	// warn if there is activity on an object being cleared
	if (!m_activeEvents.empty())
	{
		char const* const szEventString = GetEventIDs("; ").c_str();
		g_audioLogger.Log(eAudioLogType_Warning, "Active events on an object to be released: %u! EventIDs: %s", GetId(), szEventString);
	}

	if (!m_triggerStates.empty())
	{
		char const* const szTriggerString = GetTriggerNames("; ", pDebugNameStore).c_str();
		g_audioLogger.Log(eAudioLogType_Warning, "Active triggers on an object to be released: %u! TriggerNames: %s", GetId(), szTriggerString);
	}

	if (!m_activeStandaloneFiles.empty())
	{
		char const* const szStandaloneFilesString = GetStandaloneFileIDs("; ", pDebugNameStore).c_str();
		g_audioLogger.Log(eAudioLogType_Warning, "Active standalone files on an object to be released: %u! FileNames: %s", GetId(), szStandaloneFilesString);
	}
}

typedef std::map<AudioControlId, size_t, std::less<AudioControlId>,
                 STLSoundAllocator<std::pair<AudioControlId, size_t>>> TTriggerCountMap;

///////////////////////////////////////////////////////////////////////////
CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> CATLAudioObject::GetTriggerNames(char const* const _szSeparator, CATLDebugNameStore const* const _pDebugNameStore) const
{
	CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> triggers;
	CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> temp;
	TTriggerCountMap triggerCounts;

	for (auto const& triggerStatesPair : m_triggerStates)
	{
		++(triggerCounts[triggerStatesPair.second.audioTriggerId]);
	}

	for (auto const& triggerCountsPair : triggerCounts)
	{
		char const* const szName = _pDebugNameStore->LookupAudioTriggerName(triggerCountsPair.first);

		if (szName != nullptr)
		{
			size_t const numInstances = triggerCountsPair.second;

			if (numInstances == 1)
			{
				temp.Format("%s%s", szName, _szSeparator);
			}
			else
			{
				temp.Format("%s(%" PRISIZE_T " inst.)%s", szName, numInstances, _szSeparator);
			}

			triggers += temp;
		}
	}

	return triggers;
}

///////////////////////////////////////////////////////////////////////////
CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> CATLAudioObject::GetEventIDs(char const* const _szSeparator) const
{
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> events;
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> temp;

	for (auto const* const pEvent : m_activeEvents)
	{
		temp.Format("%u%s", pEvent->GetId(), _szSeparator);
		events += temp;
	}

	return events;
}

typedef std::map<AudioStandaloneFileId, size_t, std::less<AudioStandaloneFileId>,
                 STLSoundAllocator<std::pair<AudioStandaloneFileId, size_t>>> TAudioStandaloneFileCountMap;

//////////////////////////////////////////////////////////////////////////
CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> CATLAudioObject::GetStandaloneFileIDs(char const* const _szSeparator, CATLDebugNameStore const* const _pDebugNameStore) const
{
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> audioStandaloneFiles;
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> temp;

	TAudioStandaloneFileCountMap numStandaloneFiles;

	for (auto const& standaloneFilePair : m_activeStandaloneFiles)
	{
		++(numStandaloneFiles[standaloneFilePair.second.fileId]);
	}

	for (auto const& numInstancesPair : numStandaloneFiles)
	{
		char const* const szName = _pDebugNameStore->LookupAudioStandaloneFileName(numInstancesPair.first);

		if (szName != nullptr)
		{
			size_t const numInstances = numInstancesPair.second;

			if (numInstances == 1)
			{
				temp.Format("%s%s", szName, _szSeparator);
			}
			else
			{
				temp.Format("%s(%" PRISIZE_T " inst.)%s", szName, numInstances, _szSeparator);
			}

			audioStandaloneFiles += temp;
		}
	}

	return audioStandaloneFiles;
}

//////////////////////////////////////////////////////////////////////////
float const CATLAudioObject::CStateDebugDrawData::s_minAlpha = 0.5f;
float const CATLAudioObject::CStateDebugDrawData::s_maxAlpha = 1.0f;
int const CATLAudioObject::CStateDebugDrawData::s_maxToMinUpdates = 100;

///////////////////////////////////////////////////////////////////////////
CATLAudioObject::CStateDebugDrawData::CStateDebugDrawData(AudioSwitchStateId const audioSwitchState)
	: m_currentState(audioSwitchState)
	, m_currentAlpha(s_maxAlpha)
{
}

///////////////////////////////////////////////////////////////////////////
CATLAudioObject::CStateDebugDrawData::~CStateDebugDrawData()
{
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CStateDebugDrawData::Update(AudioSwitchStateId const newState)
{
	if ((newState == m_currentState) && (m_currentAlpha > s_minAlpha))
	{
		m_currentAlpha -= (s_maxAlpha - s_minAlpha) / s_maxToMinUpdates;
	}
	else if (newState != m_currentState)
	{
		m_currentState = newState;
		m_currentAlpha = s_maxAlpha;
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, CATLDebugNameStore const* const pDebugNameStore) const
{
	m_propagationProcessor.DrawObstructionRays(auxGeom);

	if (g_audioCVars.m_drawAudioDebug > 0)
	{
		Vec3 const& position = m_attributes.transformation.GetPosition();
		Vec3 screenPos(ZERO);

		if (IRenderer* const pRenderer = gEnv->pRenderer)
		{
			pRenderer->ProjectToScreen(position.x, position.y, position.z, &screenPos.x, &screenPos.y, &screenPos.z);

			screenPos.x = screenPos.x * 0.01f * pRenderer->GetWidth();
			screenPos.y = screenPos.y * 0.01f * pRenderer->GetHeight();
		}
		else
		{
			screenPos.z = -1.0f;
		}

		if ((0.0f <= screenPos.z) && (screenPos.z <= 1.0f))
		{
			float const distance = position.GetDistance(listenerPosition);

			if ((g_audioCVars.m_drawAudioDebug & eADDF_DRAW_SPHERES) > 0)
			{
				SAuxGeomRenderFlags const previousRenderFlags = auxGeom.GetRenderFlags();
				SAuxGeomRenderFlags newRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
				newRenderFlags.SetCullMode(e_CullModeNone);
				auxGeom.SetRenderFlags(newRenderFlags);
				float const radius = 0.15f;
				auxGeom.DrawSphere(position, radius, ColorB(255, 1, 1, 255));
				auxGeom.SetRenderFlags(previousRenderFlags);
			}

			float const fontSize = 1.3f;
			float const lineHeight = 12.0f;

			if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_OBJECT_STATES) > 0 && !m_switchStates.empty())
			{
				Vec3 switchPos(screenPos);

				for (auto const& switchStatePair : m_switchStates)
				{
					AudioControlId const switchId = switchStatePair.first;
					AudioSwitchStateId const stateId = switchStatePair.second;

					char const* const szSwitchName = pDebugNameStore->LookupAudioSwitchName(switchId);
					char const* const szStateName = pDebugNameStore->LookupAudioSwitchStateName(switchId, stateId);

					if ((szSwitchName != nullptr) && (szStateName != nullptr))
					{
						CStateDebugDrawData& drawData = m_stateDrawInfoMap[switchId];
						drawData.Update(stateId);
						float const switchTextColor[4] = { 0.8f, 0.8f, 0.8f, drawData.m_currentAlpha };

						switchPos.y -= lineHeight;
						auxGeom.Draw2dLabel(
						  switchPos.x,
						  switchPos.y,
						  fontSize,
						  switchTextColor,
						  false,
						  "%s: %s\n",
						  szSwitchName,
						  szStateName);
					}
				}
			}

			if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_OBJECT_LABEL) > 0)
			{
				static float const objectTextColor[4] = { 0.90f, 0.90f, 0.90f, 1.0f };
				static float const objectGrayTextColor[4] = { 0.50f, 0.50f, 0.50f, 1.0f };

				size_t const numRays = m_propagationProcessor.GetNumRays();
				SATLSoundPropagationData propagationData;
				m_propagationProcessor.GetPropagationData(propagationData);

				AudioObjectId const audioObjectId = GetId();

				auxGeom.Draw2dLabel(
				  screenPos.x,
				  screenPos.y,
				  fontSize,
				  objectTextColor,
				  false,
				  "%s ID: %u Dist:%4.1fm",
				  pDebugNameStore->LookupAudioObjectName(audioObjectId),
				  audioObjectId,
				  distance);

				auxGeom.Draw2dLabel(
				  screenPos.x,
				  screenPos.y + lineHeight,
				  fontSize,
				  numRays > 0 ? objectTextColor : objectGrayTextColor,
				  false,
				  "Obst: %3.2f Occl: %3.2f #Rays: %1" PRISIZE_T,
				  propagationData.obstruction,
				  propagationData.occlusion,
				  numRays);
			}

			float const textColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };

			if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_OBJECT_RTPCS) > 0 && !m_rtpcs.empty())
			{
				Vec3 rtpcPos(screenPos);

				for (auto const& rtpcPair : m_rtpcs)
				{
					AudioControlId const rtpcId = rtpcPair.first;
					float const rtpcValue = rtpcPair.second;

					char const* const szRtpcName = pDebugNameStore->LookupAudioRtpcName(rtpcId);

					if (szRtpcName != nullptr)
					{
						float const offsetOnX = (strlen(szRtpcName) + 5.6f) * 5.4f * fontSize;

						rtpcPos.y -= lineHeight;
						auxGeom.Draw2dLabel(
						  rtpcPos.x - offsetOnX,
						  rtpcPos.y,
						  fontSize,
						  textColor,
						  false,
						  "%s: %2.2f\n",
						  szRtpcName,
						  rtpcValue);
					}
				}
			}

			if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_OBJECT_ENVIRONMENTS) > 0 && !m_environments.empty())
			{
				Vec3 envPos(screenPos);

				for (auto const& environmentPair : m_environments)
				{
					AudioControlId const envId = environmentPair.first;
					float const envValue = environmentPair.second;

					char const* const szName = pDebugNameStore->LookupAudioEnvironmentName(envId);

					if (szName != nullptr)
					{
						float const offsetOnX = (strlen(szName) + 5.1f) * 5.4f * fontSize;

						envPos.y += lineHeight;
						auxGeom.Draw2dLabel(
						  envPos.x - offsetOnX,
						  envPos.y,
						  fontSize,
						  textColor,
						  false,
						  "%s: %.2f\n",
						  szName,
						  envValue);
					}
				}
			}

			CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> controls;
			CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> temp;

			if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_OBJECT_TRIGGERS) > 0 && !m_triggerStates.empty())
			{
				TTriggerCountMap triggerCounts;

				for (auto const& triggerStatesPair : m_triggerStates)
				{
					++(triggerCounts[triggerStatesPair.second.audioTriggerId]);
				}

				for (auto const& triggerCountsPair : triggerCounts)
				{
					char const* const szName = pDebugNameStore->LookupAudioTriggerName(triggerCountsPair.first);

					if (szName != nullptr)
					{
						size_t const numInstances = triggerCountsPair.second;

						if (numInstances == 1)
						{
							temp.Format("%s\n", szName);
						}
						else
						{
							temp.Format("%s: %" PRISIZE_T "\n", szName, numInstances);
						}

						controls += temp;
						temp.clear();
					}
				}
			}

			if ((g_audioCVars.m_drawAudioDebug & eADDF_DRAW_OBJECT_STANDALONE_FILES) > 0 && !m_activeStandaloneFiles.empty())
			{
				TAudioStandaloneFileCountMap numStandaloneFiles;

				for (auto const& standaloneFilePair : m_activeStandaloneFiles)
				{
					++(numStandaloneFiles[standaloneFilePair.second.fileId]);
				}

				for (auto const& numInstancesPair : numStandaloneFiles)
				{
					char const* const szName = pDebugNameStore->LookupAudioStandaloneFileName(numInstancesPair.first);

					if (szName != nullptr)
					{
						size_t const numInstances = numInstancesPair.second;

						if (numInstances == 1)
						{
							temp.Format("%s\n", szName);
						}
						else
						{
							temp.Format("%s: %" PRISIZE_T "\n", szName, numInstances);
						}

						controls += temp;
						temp.clear();
					}
				}
			}

			if (!controls.empty())
			{
				auxGeom.Draw2dLabel(
				  screenPos.x,
				  screenPos.y + 2.0f * lineHeight,
				  fontSize,
				  textColor,
				  false,
				  "%s",
				  controls.c_str());
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
size_t CATLAudioObject::CPropagationProcessor::s_totalSyncPhysRays = 0;
size_t CATLAudioObject::CPropagationProcessor::s_totalAsyncPhysRays = 0;

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::DrawObstructionRays(IRenderAuxGeom& auxGeom) const
{
	static ColorB const obstructedRayColor(200, 20, 1, 255);
	static ColorB const freeRayColor(20, 200, 1, 255);
	static ColorB const intersectionSphereColor(250, 200, 1, 240);
	static float const obstructedRayLabelColor[4] = { 1.0f, 0.0f, 0.02f, 0.9f };
	static float const freeRayLabelColor[4] = { 0.0f, 1.0f, 0.02f, 0.9f };
	static float const collisionPtSphereRadius = 0.01f;

	if (CanRunObstructionOcclusion())
	{
		SAuxGeomRenderFlags const previousRenderFlags = auxGeom.GetRenderFlags();
		SAuxGeomRenderFlags newRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
		newRenderFlags.SetCullMode(e_CullModeNone);

		for (size_t i = 0; i < m_totalRays; ++i)
		{
			bool const bRayObstructed = (m_rayDebugInfos[i].numHits > 0);
			Vec3 const rayEnd = bRayObstructed ?
			                    m_rayDebugInfos[i].begin + (m_rayDebugInfos[i].end - m_rayDebugInfos[i].begin).GetNormalized() * m_rayDebugInfos[i].distanceToNearestObstacle :
			                    m_rayDebugInfos[i].end; // only draw the ray to the first collision point

			if ((g_audioCVars.m_drawAudioDebug & eADDF_DRAW_OBSTRUCTION_RAYS) > 0)
			{
				ColorB const& rayColor = bRayObstructed ? obstructedRayColor : freeRayColor;

				auxGeom.SetRenderFlags(newRenderFlags);

				if (bRayObstructed)
				{
					// mark the nearest collision with a small sphere
					auxGeom.DrawSphere(rayEnd, collisionPtSphereRadius, intersectionSphereColor);
				}

				auxGeom.DrawLine(m_rayDebugInfos[i].begin, rayColor, rayEnd, rayColor, 1.0f);
				auxGeom.SetRenderFlags(previousRenderFlags);
			}

			if (IRenderer* const pRenderer = (g_audioCVars.m_drawAudioDebug & eADDF_SHOW_OBSTRUCTION_RAY_LABELS) > 0 ? gEnv->pRenderer : nullptr)
			{
				Vec3 screenPos(ZERO);

				pRenderer->ProjectToScreen(m_rayDebugInfos[i].stableEnd.x, m_rayDebugInfos[i].stableEnd.y, m_rayDebugInfos[i].stableEnd.z, &screenPos.x, &screenPos.y, &screenPos.z);

				screenPos.x = screenPos.x * 0.01f * pRenderer->GetWidth();
				screenPos.y = screenPos.y * 0.01f * pRenderer->GetHeight();

				if ((0.0f <= screenPos.z) && (screenPos.z <= 1.0f))
				{
					float const colorLerp = clamp_tpl(m_rayInfos[i].averageHits, 0.0f, 1.0f);
					float const labelColor[4] =
					{
						obstructedRayLabelColor[0] * colorLerp + freeRayLabelColor[0] * (1.0f - colorLerp),
						obstructedRayLabelColor[1] * colorLerp + freeRayLabelColor[1] * (1.0f - colorLerp),
						obstructedRayLabelColor[2] * colorLerp + freeRayLabelColor[2] * (1.0f - colorLerp),
						obstructedRayLabelColor[3] * colorLerp + freeRayLabelColor[3] * (1.0f - colorLerp)
					};

					auxGeom.Draw2dLabel(
					  screenPos.x,
					  screenPos.y - 12.0f,
					  1.2f,
					  labelColor,
					  true,
					  "ObjID:%u\n#Hits:%2.1f\nOccl:%3.2f",
					  m_rayInfos[i].audioObjectId, // a const member, will not be overwritten by a thread filling the obstruction data in
					  m_rayDebugInfos[i].averageHits,
					  m_rayDebugInfos[i].occlusionValue);
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CPropagationProcessor::ResetRayData()
{
	for (size_t i = 0; i < m_totalRays; ++i)
	{
		m_rayInfos[i].Reset();
		m_rayDebugInfos[i] = SRayDebugInfo();
	}
}

#endif // INCLUDE_AUDIO_PRODUCTION_CODE
