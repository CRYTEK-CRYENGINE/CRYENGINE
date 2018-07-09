// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "InternalEntities.h"
#include "AudioListenerManager.h"
#include "ATLAudioObject.h"
#include "AudioCVars.h"
#include <IAudioImpl.h>

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
CRelativeVelocityTrackingState::CRelativeVelocityTrackingState(SwitchStateId const stateId, CATLAudioObject const& globalAudioObject)
	: m_stateId(stateId)
	, m_globalAudioObject(globalAudioObject)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CRelativeVelocityTrackingState::Set(CATLAudioObject& audioObject) const
{
	if (&audioObject != &m_globalAudioObject)
	{
		if (m_stateId == OnStateId)
		{
			audioObject.SetFlag(EObjectFlags::TrackRelativeVelocity);
		}
		else if (m_stateId == OffStateId)
		{
			audioObject.RemoveFlag(EObjectFlags::TrackRelativeVelocity);
		}
		else
		{
			CRY_ASSERT(false);
		}
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
CAbsoluteVelocityTrackingState::CAbsoluteVelocityTrackingState(SwitchStateId const stateId, CATLAudioObject const& globalAudioObject)
	: m_stateId(stateId)
	, m_globalAudioObject(globalAudioObject)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CAbsoluteVelocityTrackingState::Set(CATLAudioObject& audioObject) const
{
	if (&audioObject != &m_globalAudioObject)
	{
		if (m_stateId == OnStateId)
		{
			audioObject.SetFlag(EObjectFlags::TrackAbsoluteVelocity);
		}
		else if (m_stateId == OffStateId)
		{
			audioObject.RemoveFlag(EObjectFlags::TrackAbsoluteVelocity);
		}
		else
		{
			CRY_ASSERT(false);
		}
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
CDoNothingTrigger::CDoNothingTrigger(TriggerImplId const id)
	: CATLTriggerImpl(id)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CDoNothingTrigger::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	return ERequestStatus::SuccessDoNotTrack;
}

//////////////////////////////////////////////////////////////////////////
CLoseFocusTrigger::CLoseFocusTrigger(TriggerImplId const id, Impl::IImpl& iimpl)
	: CATLTriggerImpl(id)
	, m_iimpl(iimpl)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CLoseFocusTrigger::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	ERequestStatus const result = m_iimpl.OnLoseFocus();
	return result == ERequestStatus::Success ? ERequestStatus::SuccessDoNotTrack : result;
}

//////////////////////////////////////////////////////////////////////////
CGetFocusTrigger::CGetFocusTrigger(TriggerImplId const id, Impl::IImpl& iimpl)
	: CATLTriggerImpl(id)
	, m_iimpl(iimpl)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CGetFocusTrigger::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	ERequestStatus const result = m_iimpl.OnGetFocus();
	return result == ERequestStatus::Success ? ERequestStatus::SuccessDoNotTrack : result;
}

//////////////////////////////////////////////////////////////////////////
CMuteAllTrigger::CMuteAllTrigger(TriggerImplId const id, Impl::IImpl& iimpl)
	: CATLTriggerImpl(id)
	, m_iimpl(iimpl)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CMuteAllTrigger::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	ERequestStatus const result = m_iimpl.MuteAll();
	return result == ERequestStatus::Success ? ERequestStatus::SuccessDoNotTrack : result;
}

//////////////////////////////////////////////////////////////////////////
CUnmuteAllTrigger::CUnmuteAllTrigger(TriggerImplId const id, Impl::IImpl& iimpl)
	: CATLTriggerImpl(id)
	, m_iimpl(iimpl)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CUnmuteAllTrigger::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	ERequestStatus const result = m_iimpl.UnmuteAll();
	return result == ERequestStatus::Success ? ERequestStatus::SuccessDoNotTrack : result;
}

//////////////////////////////////////////////////////////////////////////
CPauseAllTrigger::CPauseAllTrigger(TriggerImplId const id, Impl::IImpl& iimpl)
	: CATLTriggerImpl(id)
	, m_iimpl(iimpl)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CPauseAllTrigger::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	ERequestStatus const result = m_iimpl.PauseAll();
	return result == ERequestStatus::Success ? ERequestStatus::SuccessDoNotTrack : result;
}

//////////////////////////////////////////////////////////////////////////
CResumeAllTrigger::CResumeAllTrigger(TriggerImplId const id, Impl::IImpl& iimpl)
	: CATLTriggerImpl(id)
	, m_iimpl(iimpl)
{
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CResumeAllTrigger::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	ERequestStatus const result = m_iimpl.ResumeAll();
	return result == ERequestStatus::Success ? ERequestStatus::SuccessDoNotTrack : result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CAbsoluteVelocityParameter::Set(CATLAudioObject& audioObject, float const value) const
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CRelativeVelocityParameter::Set(CATLAudioObject& audioObject, float const value) const
{
	return ERequestStatus::Success;
}
} // namespace CryAudio
