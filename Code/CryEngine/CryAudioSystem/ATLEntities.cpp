// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "ATLEntities.h"
#include "AudioSystem.h"
#include "ATLAudioObject.h"
#include "Common/IAudioImpl.h"

namespace CryAudio
{
#if CRY_PLATFORM_WINDOWS
char const* const SATLXMLTags::szPlatform = "pc";
#elif CRY_PLATFORM_DURANGO
char const* const SATLXMLTags::szPlatform = "xboxone";
#elif CRY_PLATFORM_ORBIS
char const* const SATLXMLTags::szPlatform = "ps4";
#elif CRY_PLATFORM_MAC
char const* const SATLXMLTags::szPlatform = "mac";
#elif CRY_PLATFORM_LINUX
char const* const SATLXMLTags::szPlatform = "linux";
#elif CRY_PLATFORM_IOS
char const* const SATLXMLTags::szPlatform = "ios";
#elif CRY_PLATFORM_ANDROID
char const* const SATLXMLTags::szPlatform = "linux";
#else
	#error "Undefined platform."
#endif

char const* const SATLXMLTags::szRootNodeTag = "ATLConfig";
char const* const SATLXMLTags::szEditorDataTag = "EditorData";
char const* const SATLXMLTags::szTriggersNodeTag = "AudioTriggers";
char const* const SATLXMLTags::szParametersNodeTag = "AudioRtpcs";
char const* const SATLXMLTags::szSwitchesNodeTag = "AudioSwitches";
char const* const SATLXMLTags::szPreloadsNodeTag = "AudioPreloads";
char const* const SATLXMLTags::szEnvironmentsNodeTag = "AudioEnvironments";

char const* const SATLXMLTags::szATLTriggerTag = "ATLTrigger";
char const* const SATLXMLTags::szATLSwitchTag = "ATLSwitch";
char const* const SATLXMLTags::szATLParametersTag = "ATLRtpc";
char const* const SATLXMLTags::szATLSwitchStateTag = "ATLSwitchState";
char const* const SATLXMLTags::szATLEnvironmentTag = "ATLEnvironment";
char const* const SATLXMLTags::szATLPlatformsTag = "ATLPlatforms";
char const* const SATLXMLTags::szATLConfigGroupTag = "ATLConfigGroup";

char const* const SATLXMLTags::szATLTriggerRequestTag = "ATLTriggerRequest";
char const* const SATLXMLTags::szATLSwitchRequestTag = "ATLSwitchRequest";
char const* const SATLXMLTags::szATLValueTag = "ATLValue";
char const* const SATLXMLTags::szATLParametersRequestTag = "ATLRtpcRequest";
char const* const SATLXMLTags::szATLPreloadRequestTag = "ATLPreloadRequest";
char const* const SATLXMLTags::szATLEnvironmentRequestTag = "ATLEnvironmentRequest";

char const* const SATLXMLTags::szATLNameAttribute = "atl_name";
char const* const SATLXMLTags::szATLVersionAttribute = "atl_version";
char const* const SATLXMLTags::szATLInternalNameAttribute = "atl_internal_name";
char const* const SATLXMLTags::szATLTypeAttribute = "atl_type";
char const* const SATLXMLTags::szATLConfigGroupAttribute = "atl_config_group_name";
char const* const SATLXMLTags::szATLRadiusAttribute = "atl_radius";
char const* const SATLXMLTags::szATLOcclusionFadeOutDistanceAttribute = "atl_occlusion_fadeout_distance";

char const* const SATLXMLTags::szATLDataLoadType = "AutoLoad";

Impl::IImpl* CATLControlImpl::s_pIImpl = nullptr;

//////////////////////////////////////////////////////////////////////////
void CATLListener::SetTransformation(CObjectTransformation const& transformation, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioListenerRequestData<EAudioListenerRequestType::SetTransformation> requestData(transformation, this);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	CATLAudioObject::s_pAudioSystem->PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CATLListener::Update()
{
	// Exponential decay towards zero.
	if (m_attributes.velocity.GetLengthSquared() > 0.0f)
	{
		float const deltaTime = (g_lastMainThreadFrameStartTime - m_previousTime).GetSeconds();
		float const decay = std::max(1.0f - deltaTime / 0.125f, 0.0f);
		m_attributes.velocity *= decay;
	}
	else if (m_bNeedsFinalSetPosition)
	{
		m_attributes.velocity = ZERO;
		m_pImplData->Set3DAttributes(m_attributes);
		m_bNeedsFinalSetPosition = false;
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CATLListener::HandleSetTransformation(CObjectTransformation const& transformation)
{
	float const deltaTime = (g_lastMainThreadFrameStartTime - m_previousTime).GetSeconds();

	if (deltaTime > 0.0f)
	{
		m_attributes.transformation = transformation;
		m_attributes.velocity = (m_attributes.transformation.GetPosition() - m_previousAttributes.transformation.GetPosition()) / deltaTime;
		m_previousTime = g_lastMainThreadFrameStartTime;
		m_previousAttributes = m_attributes;
		m_bNeedsFinalSetPosition = m_attributes.velocity.GetLengthSquared() > 0.0f;
	}
	else if (deltaTime < 0.0f) //to handle time resets (e.g. loading a save-game might revert the game-time to a previous value)
	{
		m_attributes.transformation = transformation;
		m_previousTime = g_lastMainThreadFrameStartTime;
		m_previousAttributes = m_attributes;
		m_bNeedsFinalSetPosition = m_attributes.velocity.GetLengthSquared() > 0.0f;
	}

	return m_pImplData->Set3DAttributes(m_attributes);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CATLEvent::Stop()
{
	return m_pImplData->Stop();
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CATLEvent::Reset()
{
	ERequestStatus const result = Stop();
	m_pTrigger = nullptr;
	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CParameterImpl::Set(CATLAudioObject& audioObject, float const value) const
{
	return audioObject.GetImplDataPtr()->SetParameter(m_pImplData, value);
}

//////////////////////////////////////////////////////////////////////////
CParameterImpl::~CParameterImpl()
{
	CRY_ASSERT(s_pIImpl != nullptr);
	s_pIImpl->DestructParameter(m_pImplData);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CExternalAudioSwitchStateImpl::Set(CATLAudioObject& audioObject) const
{
	return audioObject.GetImplDataPtr()->SetSwitchState(m_pImplData);
}

//////////////////////////////////////////////////////////////////////////
CExternalAudioSwitchStateImpl::~CExternalAudioSwitchStateImpl()
{
	CRY_ASSERT(s_pIImpl != nullptr);
	s_pIImpl->DestructSwitchState(m_pImplData);
}

//////////////////////////////////////////////////////////////////////////
CATLTriggerImpl::~CATLTriggerImpl()
{
	CRY_ASSERT(s_pIImpl != nullptr);
	s_pIImpl->DestructTrigger(m_pImplData);
}

//////////////////////////////////////////////////////////////////////////
CATLEnvironmentImpl::~CATLEnvironmentImpl()
{
	CRY_ASSERT(s_pIImpl != nullptr);
	s_pIImpl->DestructEnvironment(m_pImplData);
}
} // namespace CryAudio
