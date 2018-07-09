// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
void CATLListener::Update(float const deltaTime)
{
	if (m_isMovingOrDecaying)
	{
		Vec3 const deltaPos(m_attributes.transformation.GetPosition() - m_previousAttributes.transformation.GetPosition());

		if (!deltaPos.IsZero())
		{
			m_attributes.velocity = deltaPos / deltaTime;
			m_previousAttributes.transformation.SetPosition(m_attributes.transformation.GetPosition());
		}
		else if (!m_attributes.velocity.IsZero())
		{
			// We did not move last frame, begin exponential decay towards zero.
			float const decay = std::max(1.0f - deltaTime / 0.05f, 0.0f);
			m_attributes.velocity *= decay;

			if (m_attributes.velocity.GetLengthSquared() < FloatEpsilon)
			{
				m_attributes.velocity = ZERO;
				m_isMovingOrDecaying = false;
			}
		}

		m_pImplData->Set3DAttributes(m_attributes);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLListener::HandleSetTransformation(CObjectTransformation const& transformation)
{
	m_attributes.transformation = transformation;
	m_isMovingOrDecaying = true;

	// Immediately propagate the new transformation down to the middleware calculation of velocity can be safely delayed to next audio frame.
	m_pImplData->Set3DAttributes(m_attributes);
}

//////////////////////////////////////////////////////////////////////////
void CATLListener::SetName(char const* const szName, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SAudioListenerRequestData<EAudioListenerRequestType::SetName> requestData(szName, this);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	CATLAudioObject::s_pAudioSystem->PushRequest(request);
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CATLListener::HandleSetName(char const* const szName)
{
	m_name = szName;
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

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
ERequestStatus CATLTriggerImpl::Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const
{
	return pImplObject->ExecuteTrigger(m_pImplData, pImplEvent);
}

//////////////////////////////////////////////////////////////////////////
CATLEnvironmentImpl::~CATLEnvironmentImpl()
{
	CRY_ASSERT(s_pIImpl != nullptr);
	s_pIImpl->DestructEnvironment(m_pImplData);
}
} // namespace CryAudio
