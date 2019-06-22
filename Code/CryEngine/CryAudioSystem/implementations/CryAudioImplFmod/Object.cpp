// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Object.h"
#include "CVars.h"
#include "Impl.h"
#include "Event.h"
#include "EventInstance.h"
#include "Parameter.h"
#include "ParameterState.h"
#include "ParameterEnvironment.h"
#include "Return.h"
#include "Listener.h"

#include <CryAudio/IAudioSystem.h>

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	#include <DebugStyle.h>
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
CObject::CObject(CTransformation const& transformation, int const listenerMask, Listeners const& listeners)
	: m_listenerMask(listenerMask)
	, m_flags(EObjectFlags::None)
	, m_transformation(transformation)
	, m_occlusion(0.0f)
	, m_absoluteVelocity(0.0f)
	, m_previousAbsoluteVelocity(0.0f)
	, m_lowestOcclusionPerListener(1.0f)
	, m_position(transformation.GetPosition())
	, m_previousPosition(transformation.GetPosition())
	, m_velocity(ZERO)
	, m_listeners(listeners)
{
	m_eventInstances.reserve(2);

	ZeroStruct(m_attributes);
	m_attributes.forward.z = 1.0f;
	m_attributes.up.y = 1.0f;

	Fill3DAttributeTransformation(transformation, m_attributes);
	Set3DAttributes();

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	UpdateListenerNames();
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
CObject::~CObject()
{
	if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
	{
		CRY_ASSERT_MESSAGE(g_numObjectsWithDoppler > 0, "g_numObjectsWithDoppler is 0 but an object with doppler tracking still exists during %s", __FUNCTION__);
		g_numObjectsWithDoppler--;
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::Update(float const deltaTime)
{
	EObjectFlags const previousFlags = m_flags;
	bool removedEvent = false;

	if (!m_eventInstances.empty())
	{
		m_flags |= EObjectFlags::IsVirtual;
	}

	auto iter(m_eventInstances.begin());
	auto iterEnd(m_eventInstances.end());

	while (iter != iterEnd)
	{
		CEventInstance* const pEventInstance = *iter;

		if (pEventInstance->IsToBeRemoved())
		{
			ETriggerResult const result = (pEventInstance->GetState() == EEventState::Pending) ? ETriggerResult::Pending : ETriggerResult::Playing;
			gEnv->pAudioSystem->ReportFinishedTriggerConnectionInstance(pEventInstance->GetTriggerInstanceId(), result);
			g_pImpl->DestructEventInstance(pEventInstance);
			removedEvent = true;

			if (iter != (iterEnd - 1))
			{
				(*iter) = m_eventInstances.back();
			}

			m_eventInstances.pop_back();
			iter = m_eventInstances.begin();
			iterEnd = m_eventInstances.end();
		}
		else if ((pEventInstance->GetState() == EEventState::Pending))
		{
			if (SetEventInstance(pEventInstance))
			{
				ETriggerResult const result = (pEventInstance->GetState() == EEventState::Playing) ? ETriggerResult::Playing : ETriggerResult::Virtual;
				gEnv->pAudioSystem->ReportStartedTriggerConnectionInstance(pEventInstance->GetTriggerInstanceId(), result);

				UpdateVirtualFlag(pEventInstance);
			}

			++iter;
		}
		else
		{
			UpdateVirtualFlag(pEventInstance);

			++iter;
		}
	}

	if ((previousFlags != m_flags) && !m_eventInstances.empty())
	{
		if (((previousFlags& EObjectFlags::IsVirtual) != 0) && ((m_flags& EObjectFlags::IsVirtual) == 0))
		{
			gEnv->pAudioSystem->ReportPhysicalizedObject(this);
		}
		else if (((previousFlags& EObjectFlags::IsVirtual) == 0) && ((m_flags& EObjectFlags::IsVirtual) != 0))
		{
			gEnv->pAudioSystem->ReportVirtualizedObject(this);
		}
	}

	if (removedEvent)
	{
		UpdateVelocityTracking();
	}

	if (((m_flags& EObjectFlags::MovingOrDecaying) != 0) && (deltaTime > 0.0f))
	{
		UpdateVelocities(deltaTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetTransformation(CTransformation const& transformation)
{
	m_position = transformation.GetPosition();

	if (((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0) || ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0))
	{
		m_flags |= EObjectFlags::MovingOrDecaying;
	}
	else
	{
		m_previousPosition = m_position;
	}

	float const threshold = GetDistanceToListener() * g_cvars.m_positionUpdateThresholdMultiplier;

	if (!m_transformation.IsEquivalent(transformation, threshold))
	{
		m_transformation = transformation;
		Fill3DAttributeTransformation(m_transformation, m_attributes);

		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
		{
			Fill3DAttributeVelocity(m_velocity, m_attributes);
		}

		Set3DAttributes();
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners)
{
	// Lowest occlusion value of all listeners is used.
	m_lowestOcclusionPerListener = std::min(m_lowestOcclusionPerListener, occlusion);

	if (numRemainingListeners == 1)
	{
		SetParameter(g_occlusionParameterInfo, m_lowestOcclusionPerListener);

		for (auto const pEventInstance : m_eventInstances)
		{
			pEventInstance->SetOcclusion(m_lowestOcclusionPerListener);
		}

		m_occlusion = m_lowestOcclusionPerListener;
		m_lowestOcclusionPerListener = 1.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetOcclusionType(EOcclusionType const occlusionType)
{
	// For disabling ray casts of the propagation processor if an object is virtual.
	if ((occlusionType != EOcclusionType::None) && (occlusionType != EOcclusionType::Ignore))
	{
		m_flags |= EObjectFlags::UpdateVirtualStates;
	}
	else
	{
		m_flags &= ~EObjectFlags::UpdateVirtualStates;
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::StopAllTriggers()
{
	for (auto const pEventInstance : m_eventInstances)
	{
		pEventInstance->StopImmediate();
	}
}
//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetName(char const* const szName)
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
void CObject::AddListener(IListener* const pIListener)
{
	auto const pNewListener = static_cast<CListener*>(pIListener);
	int const newListenerId = pNewListener->GetId();
	bool hasListener = false;

	for (auto const pListener : m_listeners)
	{
		if (pListener->GetId() == newListenerId)
		{
			hasListener = true;
			break;
		}
	}

	if (!hasListener)
	{
		m_listenerMask |= BIT(newListenerId);
		m_listeners.push_back(pNewListener);

		for (auto const pEventInstance : m_eventInstances)
		{
			pEventInstance->SetListenermask(m_listenerMask);
		}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		UpdateListenerNames();
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::RemoveListener(IListener* const pIListener)
{
	auto const pListenerToRemove = static_cast<CListener*>(pIListener);
	bool wasRemoved = false;

	auto iter(m_listeners.begin());
	auto const iterEnd(m_listeners.cend());

	for (; iter != iterEnd; ++iter)
	{
		CListener* const pListener = *iter;

		if (pListener == pListenerToRemove)
		{
			m_listenerMask &= ~BIT(pListenerToRemove->GetId());

			if (iter != (iterEnd - 1))
			{
				(*iter) = m_listeners.back();
			}

			m_listeners.pop_back();
			wasRemoved = true;
			break;
		}
	}

	if (wasRemoved)
	{
		for (auto const pEventInstance : m_eventInstances)
		{
			pEventInstance->SetListenermask(m_listenerMask);
		}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		UpdateListenerNames();
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::ToggleFunctionality(EObjectFunctionality const type, bool const enable)
{
	switch (type)
	{
	case EObjectFunctionality::TrackRelativeVelocity:
		{
			if (enable)
			{
				if ((m_flags& EObjectFlags::TrackVelocityForDoppler) == 0)
				{
					m_flags |= EObjectFlags::TrackVelocityForDoppler;
					g_numObjectsWithDoppler++;
				}
			}
			else
			{
				if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
				{
					m_flags &= ~EObjectFlags::TrackVelocityForDoppler;

					Vec3 const zeroVelocity{ 0.0f, 0.0f, 0.0f };
					Fill3DAttributeVelocity(zeroVelocity, m_attributes);
					Set3DAttributes();

					CRY_ASSERT_MESSAGE(g_numObjectsWithDoppler > 0, "g_numObjectsWithDoppler is 0 but an object with doppler tracking still exists during %s", __FUNCTION__);
					g_numObjectsWithDoppler--;
				}
			}

			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter)
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)

	if (((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0) || ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0))
	{
		bool isVirtual = true;

		for (auto const pEventInstance : m_eventInstances)
		{
			if (pEventInstance->GetState() != EEventState::Virtual)
			{
				isVirtual = false;
				break;
			}
		}

		if ((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0)
		{
			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_objectFontSize,
				isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorParameter,
				false,
				"[Fmod] %s: %2.2f m/s\n",
				g_szAbsoluteVelocityParameterName,
				m_absoluteVelocity);

			posY += Debug::g_objectLineHeight;
		}

		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
		{
			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_objectFontSize,
				isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorActive,
				false,
				"[Fmod] Doppler calculation enabled\n");
		}
	}

#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
bool CObject::SetEventInstance(CEventInstance* const pEventInstance)
{
	bool bSuccess = false;

	// Update the event with all parameter and environment values
	// that are currently set on the object before starting it.
	if (pEventInstance->PrepareForOcclusion())
	{
		FMOD::Studio::EventInstance* const pFModEventInstance = pEventInstance->GetFmodEventInstance();
		CRY_ASSERT_MESSAGE(pFModEventInstance != nullptr, "Fmod event instance doesn't exist during %s", __FUNCTION__);

		for (auto const& parameterPair : m_parameters)
		{
			pFModEventInstance->setParameterByID(parameterPair.first.GetId(), parameterPair.second);
		}

		for (auto const& returnPair : m_returns)
		{
			pEventInstance->SetReturnSend(returnPair.first, returnPair.second);
		}

		UpdateVelocityTracking();
		pEventInstance->SetOcclusion(m_occlusion);

		FMOD_RESULT const fmodResult = pFModEventInstance->start();
		CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

		pEventInstance->UpdateVirtualState();
		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CObject::AddEventInstance(CEventInstance* const pEventInstance)
{
	m_eventInstances.push_back(pEventInstance);
}

//////////////////////////////////////////////////////////////////////////
void CObject::StopEventInstance(uint32 const id)
{
	for (auto const pEventInstance : m_eventInstances)
	{
		if (pEventInstance->GetEvent().GetId() == id)
		{
			if (pEventInstance->IsPaused() || g_masterBusPaused)
			{
				pEventInstance->StopImmediate();
			}
			else
			{
				pEventInstance->StopAllowFadeOut();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetParameter(CParameterInfo& parameterInfo, float const value)
{
	if (parameterInfo.HasValidId())
	{
		for (auto const pEventInstance : m_eventInstances)
		{
			pEventInstance->GetFmodEventInstance()->setParameterByID(parameterInfo.GetId(), value);
		}

		m_parameters[parameterInfo] = value;
	}
	else
	{
		for (auto const pEventInstance : m_eventInstances)
		{
			FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;

			if (pEventInstance->GetEvent().GetEventDescription()->getParameterDescriptionByName(parameterInfo.GetName(), &parameterDescription) == FMOD_OK)
			{
				FMOD_STUDIO_PARAMETER_ID const id = parameterDescription.id;
				parameterInfo.SetId(id);

				for (auto const pEventInstance : m_eventInstances)
				{
					pEventInstance->GetFmodEventInstance()->setParameterByID(id, value);
				}

				m_parameters[parameterInfo] = value;

				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::RemoveParameter(CParameterInfo const& parameterInfo)
{
	m_parameters.erase(parameterInfo);
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetReturn(CReturn const* const pReturn, float const amount)
{
	auto const iter(m_returns.find(pReturn));

	if (iter != m_returns.end())
	{
		if (fabs(iter->second - amount) > 0.001f)
		{
			iter->second = amount;

			for (auto const pEventInstance : m_eventInstances)
			{
				pEventInstance->SetReturnSend(pReturn, amount);
			}
		}
	}
	else
	{
		m_returns.emplace(pReturn, amount);

		for (auto const pEventInstance : m_eventInstances)
		{
			pEventInstance->SetReturnSend(pReturn, amount);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::RemoveReturn(CReturn const* const pReturn)
{
	m_returns.erase(pReturn);
}

//////////////////////////////////////////////////////////////////////////
void CObject::Set3DAttributes()
{
	for (auto const pEventInstance : m_eventInstances)
	{
		FMOD_RESULT const fmodResult = pEventInstance->GetFmodEventInstance()->set3DAttributes(&m_attributes);
		CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::UpdateVirtualFlag(CEventInstance* const pEventInstance)
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	// Always update in production code for debug draw.
	pEventInstance->UpdateVirtualState();

	if (pEventInstance->GetState() != EEventState::Virtual)
	{
		m_flags &= ~EObjectFlags::IsVirtual;
	}
#else
	if (((m_flags& EObjectFlags::IsVirtual) != 0) && ((m_flags& EObjectFlags::UpdateVirtualStates) != 0))
	{
		pEventInstance->UpdateVirtualState();

		if (pEventInstance->GetState() != EEventState::Virtual)
		{
			m_flags &= ~EObjectFlags::IsVirtual;
		}
	}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CObject::UpdateVelocityTracking()
{
	bool trackVelocity = false;

	for (auto const pEventInstance : m_eventInstances)
	{
		if ((pEventInstance->GetEvent().GetFlags() & EEventFlags::HasAbsoluteVelocityParameter) != 0)
		{
			trackVelocity = true;
			break;
		}
	}

	trackVelocity ? (m_flags |= EObjectFlags::TrackAbsoluteVelocity) : (m_flags &= ~EObjectFlags::TrackAbsoluteVelocity);
}

///////////////////////////////////////////////////////////////////////////
void CObject::UpdateVelocities(float const deltaTime)
{
	Vec3 const deltaPos(m_position - m_previousPosition);

	if (!deltaPos.IsZero())
	{
		m_velocity = deltaPos / deltaTime;
		m_previousPosition = m_position;
	}
	else if (!m_velocity.IsZero())
	{
		// We did not move last frame, begin exponential decay towards zero.
		float const decay = std::max(1.0f - deltaTime / 0.05f, 0.0f);
		m_velocity *= decay;

		if (m_velocity.GetLengthSquared() < FloatEpsilon)
		{
			m_velocity = ZERO;
			m_flags &= ~EObjectFlags::MovingOrDecaying;
		}

		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
		{
			Fill3DAttributeVelocity(m_velocity, m_attributes);
			Set3DAttributes();
		}
	}

	if ((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0)
	{
		float const absoluteVelocity = m_velocity.GetLength();

		if (absoluteVelocity == 0.0f || fabs(absoluteVelocity - m_previousAbsoluteVelocity) > g_cvars.m_velocityTrackingThreshold)
		{
			m_previousAbsoluteVelocity = absoluteVelocity;
			SetParameter(g_absoluteVelocityParameterInfo, absoluteVelocity);
			m_absoluteVelocity = absoluteVelocity;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
float CObject::GetDistanceToListener()
{
	float shortestDistanceToListener = std::numeric_limits<float>::max();

	for (auto const pListener : m_listeners)
	{
		float const distance = m_position.GetDistance(pListener->GetPosition());
		shortestDistanceToListener = std::min(shortestDistanceToListener, distance);
	}

	return shortestDistanceToListener;
}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CObject::UpdateListenerNames()
{
	m_listenerNames.clear();
	size_t const numListeners = m_listeners.size();

	if (numListeners != 0)
	{
		for (size_t i = 0; i < numListeners; ++i)
		{
			m_listenerNames += m_listeners[i]->GetName();

			if (i != (numListeners - 1))
			{
				m_listenerNames += ", ";
			}
		}
	}
	else
	{
		m_listenerNames = "No Listener!";
	}
}
#endif // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}      // namespace Fmod
}      // namespace Impl
}      // namespace CryAudio