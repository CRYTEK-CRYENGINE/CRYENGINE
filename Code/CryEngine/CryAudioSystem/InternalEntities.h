// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"

namespace CryAudio
{
class CAudioListenerManager;
class CATLAudioObject;

struct SInternalControls
{
	using SwitchState = std::pair<ControlId const, SwitchStateId const>;

	std::map<SwitchState, IAudioSwitchStateImpl const*> m_switchStates;
	std::map<ControlId, IParameterImpl*>                m_parameters;
};

class COcclusionObstructionState final : public IAudioSwitchStateImpl
{
public:
	explicit COcclusionObstructionState(SwitchStateId const stateId, CAudioListenerManager const& audioListenerManager, CATLAudioObject const& globalAudioObject);
	virtual ERequestStatus Set(CATLAudioObject& audioObject) const override;

	COcclusionObstructionState(COcclusionObstructionState const&) = delete;
	COcclusionObstructionState(COcclusionObstructionState&&) = delete;
	COcclusionObstructionState& operator=(COcclusionObstructionState const&) = delete;
	COcclusionObstructionState& operator=(COcclusionObstructionState&&) = delete;

private:
	SwitchStateId const          m_stateId;
	CAudioListenerManager const& m_audioListenerManager;
	CATLAudioObject const&       m_globalAudioObject;
};

class CDopplerTrackingState final : public IAudioSwitchStateImpl
{
public:
	explicit CDopplerTrackingState(SwitchStateId const stateId, CATLAudioObject const& globalAudioObject);
	virtual ERequestStatus Set(CATLAudioObject& audioObject) const override;

	CDopplerTrackingState(CDopplerTrackingState const&) = delete;
	CDopplerTrackingState(CDopplerTrackingState&&) = delete;
	CDopplerTrackingState& operator=(CDopplerTrackingState const&) = delete;
	CDopplerTrackingState& operator=(CDopplerTrackingState&&) = delete;

private:
	SwitchStateId const    m_stateId;
	CATLAudioObject const& m_globalAudioObject;
};

class CVelocityTrackingState final : public IAudioSwitchStateImpl
{
public:
	explicit CVelocityTrackingState(SwitchStateId const stateId, CATLAudioObject const& globalAudioObject);
	virtual ERequestStatus Set(CATLAudioObject& audioObject) const override;

	CVelocityTrackingState(CVelocityTrackingState const&) = delete;
	CVelocityTrackingState(CVelocityTrackingState&&) = delete;
	CVelocityTrackingState& operator=(CVelocityTrackingState const&) = delete;
	CVelocityTrackingState& operator=(CVelocityTrackingState&&) = delete;

private:
	SwitchStateId const    m_stateId;
	CATLAudioObject const& m_globalAudioObject;
};
} // namespace CryAudio
