// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "InternalEntities.h"
#include "AudioListenerManager.h"
#include "ATLAudioObject.h"

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
COcclusionObstructionState::COcclusionObstructionState(SwitchStateId const stateId, CAudioListenerManager const& audioListenerManager, CATLAudioObject const& globalAudioObject)
	: m_stateId(stateId)
	, m_audioListenerManager(audioListenerManager)
	, m_globalAudioObject(globalAudioObject)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus COcclusionObstructionState::Set(CATLAudioObject& audioObject) const
{
	if (&audioObject != &m_globalAudioObject)
	{
		Vec3 const& audioListenerPosition = m_audioListenerManager.GetActiveListenerAttributes().transformation.GetPosition();
		if (m_stateId == IgnoreStateId)
		{
			audioObject.HandleSetOcclusionType(EOcclusionType::Ignore, audioListenerPosition);
			audioObject.SetObstructionOcclusion(0.0f, 0.0f);
		}
		else if (m_stateId == AdaptiveStateId)
		{
			audioObject.HandleSetOcclusionType(EOcclusionType::Adaptive, audioListenerPosition);
		}
		else if (m_stateId == LowStateId)
		{
			audioObject.HandleSetOcclusionType(EOcclusionType::Low, audioListenerPosition);
		}
		else if (m_stateId == MediumStateId)
		{
			audioObject.HandleSetOcclusionType(EOcclusionType::Medium, audioListenerPosition);
		}
		else if (m_stateId == HighStateId)
		{
			audioObject.HandleSetOcclusionType(EOcclusionType::High, audioListenerPosition);
		}
		else
		{
			audioObject.HandleSetOcclusionType(EOcclusionType::Ignore, audioListenerPosition);
			audioObject.SetObstructionOcclusion(0.0f, 0.0f);
		}
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
CDopplerTrackingState::CDopplerTrackingState(SwitchStateId const stateId, CATLAudioObject const& globalAudioObject)
	: m_stateId(stateId)
	, m_globalAudioObject(globalAudioObject)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CDopplerTrackingState::Set(CATLAudioObject& audioObject) const
{
	if (&audioObject != &m_globalAudioObject)
	{
		if (m_stateId == OnStateId)
		{
			audioObject.SetDopplerTracking(true);
		}
		else if (m_stateId == OffStateId)
		{
			audioObject.SetDopplerTracking(false);
		}
		else
		{
			CRY_ASSERT(false);
		}
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
CVelocityTrackingState::CVelocityTrackingState(SwitchStateId const stateId, CATLAudioObject const& globalAudioObject)
	: m_stateId(stateId)
	, m_globalAudioObject(globalAudioObject)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CVelocityTrackingState::Set(CATLAudioObject& audioObject) const
{
	if (&audioObject != &m_globalAudioObject)
	{
		if (m_stateId == OnStateId)
		{
			audioObject.SetVelocityTracking(true);
		}
		else if (m_stateId == OffStateId)
		{
			audioObject.SetVelocityTracking(false);
		}
		else
		{
			CRY_ASSERT(false);
		}
	}

	return ERequestStatus::Success;
}
} // namespace CryAudio
