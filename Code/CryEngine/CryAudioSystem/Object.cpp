// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Object.h"
#include "CVars.h"
#include "Managers.h"
#include "System.h"
#include "ListenerManager.h"
#include "Listener.h"
#include "Request.h"
#include "Environment.h"
#include "Parameter.h"
#include "Switch.h"
#include "SwitchState.h"
#include "Trigger.h"
#include "TriggerInstance.h"
#include "ObjectRequestData.h"
#include "CallbackRequestData.h"
#include "Common/IImpl.h"
#include "Common/IObject.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryMath/Cry_Camera.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "PreviewTrigger.h"
	#include "Common/Logger.h"
	#include "Common/DebugStyle.h"
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
void PushRequest(SRequestData const& requestData, SRequestUserData const& userData)
{
	CRequest const request(&requestData, userData);
	g_system.PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CObject::Destruct()
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	stl::find_and_erase(g_constructedObjects, this);
#endif      // CRY_AUDIO_USE_DEBUG_CODE

	g_pIImpl->DestructObject(m_pIObject);
	m_pIObject = nullptr;

#if defined(CRY_AUDIO_USE_OCCLUSION)
	m_propagationProcessor.Release();
#endif    // CRY_AUDIO_USE_OCCLUSION

	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetActive()
{
#if defined(CRY_AUDIO_USE_OCCLUSION)
	if (((m_flags& EObjectFlags::Virtual) == 0) && m_triggerInstances.empty())
	{
		m_propagationProcessor.UpdateOcclusion();
	}
#endif    // CRY_AUDIO_USE_OCCLUSION

	if (std::find(g_activeObjects.begin(), g_activeObjects.end(), this) == g_activeObjects.end())
	{
		g_activeObjects.push_back(this);
		m_flags |= EObjectFlags::Active;
	}

	g_triggerInstanceIdLookup[g_triggerInstanceIdCounter] = this;
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CObject::ConstructTriggerInstance(
	ControlId const triggerId,
	EntityId const entityId,
	uint16 const numPlayingConnectionInstances,
	uint16 const numPendingConnectionInstances,
	ERequestFlags const flags,
	void* const pOwner,
	void* const pUserData,
	void* const pUserDataOwner,
	float const radius)
{
	SetActive();

	m_triggerInstances.emplace(
		std::piecewise_construct,
		std::forward_as_tuple(g_triggerInstanceIdCounter),
		std::forward_as_tuple(new CTriggerInstance(triggerId, entityId, numPlayingConnectionInstances, numPendingConnectionInstances, flags, pOwner, pUserData, pUserDataOwner, radius)));

	IncrementTriggerInstanceIdCounter();
	UpdateMaxRadius(radius);
}
#else
//////////////////////////////////////////////////////////////////////////
void CObject::ConstructTriggerInstance(
	ControlId const triggerId,
	EntityId const entityId,
	uint16 const numPlayingConnectionInstances,
	uint16 const numPendingConnectionInstances,
	ERequestFlags const flags,
	void* const pOwner,
	void* const pUserData,
	void* const pUserDataOwner)
{
	SetActive();

	m_triggerInstances.emplace(
		std::piecewise_construct,
		std::forward_as_tuple(g_triggerInstanceIdCounter),
		std::forward_as_tuple(new CTriggerInstance(triggerId, entityId, numPlayingConnectionInstances, numPendingConnectionInstances, flags, pOwner, pUserData, pUserDataOwner)));

	IncrementTriggerInstanceIdCounter();
}
#endif // CRY_AUDIO_USE_DEBUG_CODE

///////////////////////////////////////////////////////////////////////////
void CObject::ReportStartedTriggerInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result)
{
	TriggerInstances::iterator const iter(m_triggerInstances.find(triggerInstanceId));

	if (iter != m_triggerInstances.end())
	{
#if defined(CRY_AUDIO_USE_OCCLUSION)
		if ((result == ETriggerResult::Playing) && ((m_flags& EObjectFlags::Virtual) != 0))
		{
			m_flags &= ~EObjectFlags::Virtual;
			m_propagationProcessor.UpdateOcclusion();
		}
#endif    // CRY_AUDIO_USE_OCCLUSION

		iter->second->SetPendingToPlaying();
	}
}

///////////////////////////////////////////////////////////////////////////
void CObject::ReportFinishedTriggerInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result)
{
	TriggerInstances::iterator const iter(m_triggerInstances.find(triggerInstanceId));

	if (iter != m_triggerInstances.end())
	{
		CTriggerInstance* const pTriggerInstance = iter->second;

		if (result != ETriggerResult::Pending)
		{
			if (pTriggerInstance->IsPlayingInstanceFinished())
			{
				g_triggerInstanceIdLookup.erase(triggerInstanceId);
				pTriggerInstance->SendFinishedRequest();

				m_triggerInstances.erase(iter);
				delete pTriggerInstance;
			}
		}
		else
		{
			if (pTriggerInstance->IsPendingInstanceFinished())
			{
				g_triggerInstanceIdLookup.erase(triggerInstanceId);
				pTriggerInstance->SendFinishedRequest();

				m_triggerInstances.erase(iter);
				delete pTriggerInstance;
			}
		}
	}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown trigger instance id %u during %s", triggerInstanceId, __FUNCTION__);
	}

	// Recalculate the max activity radius.
	m_maxRadius = 0.0f;

	for (auto const& triggerState : m_triggerInstances)
	{
		m_maxRadius = std::max(triggerState.second->GetRadius(), m_maxRadius);
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CObject::StopAllTriggers()
{
	m_pIObject->StopAllTriggers();
}

//////////////////////////////////////////////////////////////////////////
bool CObject::IsPlaying() const
{
	return !m_triggerInstances.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CObject::HasPendingCallbacks() const
{
#if defined(CRY_AUDIO_USE_OCCLUSION)
	return m_propagationProcessor.HasPendingRays() || (m_numPendingSyncCallbacks > 0);
#else
	return (m_numPendingSyncCallbacks > 0);
#endif // CRY_AUDIO_USE_OCCLUSION
}

///////////////////////////////////////////////////////////////////////////
void CObject::Update(float const deltaTime)
{
#if defined(CRY_AUDIO_USE_OCCLUSION)
	m_propagationProcessor.Update();

	if (m_propagationProcessor.HasNewOcclusionValues())
	{
		auto numRemainingListeners = static_cast<uint8>(m_listeners.size());

		for (auto const pListener : m_listeners)
		{
			m_pIObject->SetOcclusion(pListener->GetImplData(), m_propagationProcessor.GetOcclusion(pListener), numRemainingListeners);
			--numRemainingListeners;
		}
	}
#endif // CRY_AUDIO_USE_OCCLUSION

	m_pIObject->Update(deltaTime);
}

///////////////////////////////////////////////////////////////////////////
void CObject::HandleSetTransformation(CTransformation const& transformation)
{
	m_transformation = transformation;
	m_pIObject->SetTransformation(transformation);
}

//////////////////////////////////////////////////////////////////////////
void CObject::HandleAddListener(ListenerId const id)
{
	bool hasListener = false;

	for (auto const pListener : m_listeners)
	{
		if (pListener->GetId() == id)
		{
			hasListener = true;
			break;
		}
	}

	if (!hasListener)
	{
		CListener* const pListener = g_listenerManager.GetListener(id);
		m_listeners.push_back(pListener);
		m_pIObject->AddListener(pListener->GetImplData());

#if defined(CRY_AUDIO_USE_OCCLUSION)
		m_propagationProcessor.AddListener(pListener);
#endif // CRY_AUDIO_USE_OCCLUSION
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::HandleRemoveListener(ListenerId const id)
{
	auto iter(m_listeners.begin());
	auto const iterEnd(m_listeners.cend());

	for (; iter != iterEnd; ++iter)
	{
		CListener* const pListener = *iter;

		if (pListener->GetId() == id)
		{
			m_pIObject->RemoveListener(pListener->GetImplData());

#if defined(CRY_AUDIO_USE_OCCLUSION)
			m_propagationProcessor.RemoveListener(pListener);
#endif  // CRY_AUDIO_USE_OCCLUSION

			if (iter != (iterEnd - 1))
			{
				(*iter) = m_listeners.back();
			}

			m_listeners.pop_back();
			break;
		}
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_listeners.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Object "%s" has no listener!)", m_name.c_str());
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

#if defined(CRY_AUDIO_USE_OCCLUSION)
///////////////////////////////////////////////////////////////////////////
void CObject::SetOcclusion(float const occlusion)
{
	auto numRemainingListeners = static_cast<uint8>(m_listeners.size());

	for (auto const pListener : m_listeners)
	{
		m_pIObject->SetOcclusion(pListener->GetImplData(), occlusion, numRemainingListeners);
		--numRemainingListeners;
	}
}

///////////////////////////////////////////////////////////////////////////
void CObject::HandleSetOcclusionType(EOcclusionType const calcType)
{
	CRY_ASSERT_MESSAGE(calcType != EOcclusionType::None, "No valid occlusion type set during %s", __FUNCTION__);
	m_propagationProcessor.SetOcclusionType(calcType);
	m_pIObject->SetOcclusionType(calcType);
}

///////////////////////////////////////////////////////////////////////////
void CObject::ProcessPhysicsRay(CRayInfo& rayInfo)
{
	m_propagationProcessor.ProcessPhysicsRay(rayInfo);
}

//////////////////////////////////////////////////////////////////////////
void CObject::HandleSetOcclusionRayOffset(float const offset)
{
	m_propagationProcessor.SetOcclusionRayOffset(offset);
}

//////////////////////////////////////////////////////////////////////////
void CObject::ReleasePendingRays()
{
	m_propagationProcessor.ReleasePendingRays();
}
#endif // CRY_AUDIO_USE_OCCLUSION

//////////////////////////////////////////////////////////////////////////
void CObject::Init(Impl::IObject* const pIObject, Listeners const& listeners)
{
	m_pIObject = pIObject;
	m_listeners = listeners;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CRY_ASSERT_MESSAGE(m_pIObject != nullptr, "m_pIObject is nullptr on object \"%s\" during %s", m_name.c_str(), __FUNCTION__);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

#if defined(CRY_AUDIO_USE_OCCLUSION)
	m_propagationProcessor.Init();
#endif // CRY_AUDIO_USE_OCCLUSION
}

///////////////////////////////////////////////////////////////////////////
void CObject::SetFlag(EObjectFlags const flag)
{
	m_flags |= flag;
}

///////////////////////////////////////////////////////////////////////////
void CObject::RemoveFlag(EObjectFlags const flag)
{
	m_flags &= ~flag;
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CObject::Release()
{
	// Do not clear the object's name though!

	for (auto& triggerInstancePair : m_triggerInstances)
	{
		triggerInstancePair.second->Release();
	}

	m_pIObject = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetImplData(Impl::IObject* const pIObject)
{
	m_pIObject = pIObject;

	CRY_ASSERT_MESSAGE(m_pIObject != nullptr, "m_pIObject is nullptr on object \"%s\" during %s", m_name.c_str(), __FUNCTION__);
}

///////////////////////////////////////////////////////////////////////////
void CObject::DrawDebugInfo(
	IRenderAuxGeom& auxGeom,
	bool const isTextFilterDisabled,
	CryFixedStringT<MaxControlNameLength> const& lowerCaseSearchString)
{
	Vec3 const& position = m_transformation.GetPosition();
	Vec3 screenPos(ZERO);
	float distance = 0.0f;

	if (IRenderer* const pRenderer = gEnv->pRenderer)
	{
		auto const& camera = GetISystem()->GetViewCamera();
		pRenderer->ProjectToScreen(position.x, position.y, position.z, &screenPos.x, &screenPos.y, &screenPos.z);

		screenPos.x = screenPos.x * 0.01f * camera.GetViewSurfaceX();
		screenPos.y = screenPos.y * 0.01f * camera.GetViewSurfaceZ();

		distance = position.GetDistance(camera.GetPosition());
	}
	else
	{
		screenPos.z = -1.0f;
	}

	if ((screenPos.z >= 0.0f) && (screenPos.z <= 1.0f) && ((g_cvars.m_debugDistance <= 0.0f) || ((g_cvars.m_debugDistance > 0.0f) && (distance <= g_cvars.m_debugDistance))))
	{
		bool const drawSphere = (g_cvars.m_drawDebug & Debug::EDrawFilter::Spheres) != 0;
		bool const drawLabel = (g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectLabel) != 0;
		bool const drawTriggers = (g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectTriggers) != 0;
		bool const drawStates = (g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectStates) != 0;
		bool const drawParameters = (g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectParameters) != 0;
		bool const drawEnvironments = (g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectEnvironments) != 0;
		bool const drawDistance = (g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectDistance) != 0;
	#if defined(CRY_AUDIO_USE_OCCLUSION)
		bool const drawOcclusionRayLabel = (g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionRayLabels) != 0;
		bool const drawOcclusionRayOffset = (g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionRayOffset) != 0;
		bool const drawOcclusionRaysOrSpheres = ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionRays) != 0) ||
		                                        ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionCollisionSpheres) != 0);
		bool const drawOcclusionListenerPlane = (g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionListenerPlane) != 0;
	#endif // CRY_AUDIO_USE_OCCLUSION
		bool const filterAllObjectInfo = (g_cvars.m_drawDebug & Debug::EDrawFilter::FilterAllObjectInfo) != 0;

		// Check if any trigger matches text filter.
		bool doesTriggerMatchFilter = false;
		std::vector<CryFixedStringT<MaxMiscStringLength>> triggerInfo;

		if ((drawTriggers && !m_triggerInstances.empty()) || filterAllObjectInfo)
		{
			Debug::TriggerCounts triggerCounts;

			for (auto const& triggerInstancePair : m_triggerInstances)
			{
				++(triggerCounts[triggerInstancePair.second->GetTriggerId()]);
			}

			for (auto const& triggerCountsPair : triggerCounts)
			{
				CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, triggerCountsPair.first, nullptr);

				if (pTrigger != nullptr)
				{
					char const* const szTriggerName = pTrigger->GetName();

					if (!isTextFilterDisabled)
					{
						CryFixedStringT<MaxControlNameLength> lowerCaseTriggerName(szTriggerName);
						lowerCaseTriggerName.MakeLower();

						if (lowerCaseTriggerName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos)
						{
							doesTriggerMatchFilter = true;
						}
					}

					CryFixedStringT<MaxMiscStringLength> debugText;
					uint8 const numInstances = triggerCountsPair.second;

					if (numInstances == 1)
					{
						debugText.Format("%s\n", szTriggerName);
					}
					else
					{
						debugText.Format("%s: %u\n", szTriggerName, numInstances);
					}

					triggerInfo.emplace_back(debugText);
				}
			}
		}

		// Check if any state or switch matches text filter.
		bool doesStateSwitchMatchFilter = false;
		std::map<CSwitch const* const, CSwitchState const* const> switchStateInfo;

		if ((drawStates && !m_switchStates.empty()) || filterAllObjectInfo)
		{
			for (auto const& switchStatePair : m_switchStates)
			{
				CSwitch const* const pSwitch = stl::find_in_map(g_switchLookup, switchStatePair.first, nullptr);

				if (pSwitch != nullptr)
				{
					CSwitchState const* const pSwitchState = stl::find_in_map(pSwitch->GetStates(), switchStatePair.second, nullptr);

					if (pSwitchState != nullptr)
					{
						if (!isTextFilterDisabled)
						{
							char const* const szSwitchName = pSwitch->GetName();
							CryFixedStringT<MaxControlNameLength> lowerCaseSwitchName(szSwitchName);
							lowerCaseSwitchName.MakeLower();
							char const* const szStateName = pSwitchState->GetName();
							CryFixedStringT<MaxControlNameLength> lowerCaseStateName(szStateName);
							lowerCaseStateName.MakeLower();

							if ((lowerCaseSwitchName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos) ||
							    (lowerCaseStateName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos))
							{
								doesStateSwitchMatchFilter = true;
							}
						}

						switchStateInfo.emplace(pSwitch, pSwitchState);
					}
				}
			}
		}

		// Check if any parameter matches text filter.
		bool doesParameterMatchFilter = false;
		std::map<char const* const, float const> parameterInfo;

		if ((drawParameters && !m_parameters.empty()) || filterAllObjectInfo)
		{
			for (auto const& parameterPair : m_parameters)
			{
				CParameter const* const pParameter = stl::find_in_map(g_parameterLookup, parameterPair.first, nullptr);

				if (pParameter != nullptr)
				{
					char const* const szParameterName = pParameter->GetName();

					if (!isTextFilterDisabled)
					{
						CryFixedStringT<MaxControlNameLength> lowerCaseParameterName(szParameterName);
						lowerCaseParameterName.MakeLower();

						if (lowerCaseParameterName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos)
						{
							doesParameterMatchFilter = true;
						}
					}

					parameterInfo.emplace(szParameterName, parameterPair.second);
				}
			}
		}

		// Check if any environment matches text filter.
		bool doesEnvironmentMatchFilter = false;
		std::map<char const* const, float const> environmentInfo;

		if ((drawEnvironments && !m_environments.empty()) || filterAllObjectInfo)
		{
			for (auto const& environmentPair : m_environments)
			{
				if (environmentPair.second > 0.0f)
				{
					CEnvironment const* const pEnvironment = stl::find_in_map(g_environmentLookup, environmentPair.first, nullptr);

					if (pEnvironment != nullptr)
					{
						char const* const szEnvironmentName = pEnvironment->GetName();

						if (!isTextFilterDisabled)
						{
							CryFixedStringT<MaxControlNameLength> lowerCaseEnvironmentName(szEnvironmentName);
							lowerCaseEnvironmentName.MakeLower();

							if (lowerCaseEnvironmentName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos)
							{
								doesEnvironmentMatchFilter = true;
							}
						}

						environmentInfo.emplace(szEnvironmentName, environmentPair.second);
					}
				}
			}
		}

		// Check if object name matches text filter.
		bool doesObjectNameMatchFilter = false;

	#if defined(CRY_AUDIO_USE_OCCLUSION)
		if (!isTextFilterDisabled &&
		    (drawSphere ||
		     drawLabel ||
		     drawDistance ||
		     drawOcclusionRayLabel ||
		     drawOcclusionRayOffset ||
		     drawOcclusionRaysOrSpheres ||
		     drawOcclusionListenerPlane ||
		     filterAllObjectInfo))
	#else
		if (!isTextFilterDisabled && (drawSphere || drawLabel || drawDistance || filterAllObjectInfo))
	#endif // CRY_AUDIO_USE_OCCLUSION
		{
			CryFixedStringT<MaxControlNameLength> lowerCaseObjectName(m_name.c_str());
			lowerCaseObjectName.MakeLower();
			doesObjectNameMatchFilter = (lowerCaseObjectName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos);
		}

		bool const hasActiveData = (m_flags& EObjectFlags::Active) != 0;
		bool const isVirtual = (m_flags& EObjectFlags::Virtual) != 0;
		bool const canDraw = (g_cvars.m_hideInactiveObjects == 0) || ((g_cvars.m_hideInactiveObjects != 0) && hasActiveData && !isVirtual);

		if (canDraw)
		{
			if (drawSphere && (isTextFilterDisabled || doesObjectNameMatchFilter))
			{
				auxGeom.DrawSphere(
					position,
					Debug::g_objectRadiusPositionSphere,
					isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorPositionSphere);
			}

			if (drawLabel && (isTextFilterDisabled || doesObjectNameMatchFilter))
			{
				auxGeom.Draw2dLabel(
					screenPos.x,
					screenPos.y,
					Debug::g_objectFontSize,
					isVirtual ? Debug::s_globalColorVirtual : (hasActiveData ? Debug::s_objectColorActive : Debug::s_globalColorInactive),
					false,
					"%s", m_name.c_str());

				screenPos.y += Debug::g_objectLineHeight;
			}

			if (drawTriggers && (isTextFilterDisabled || doesTriggerMatchFilter))
			{
				for (auto const& debugText : triggerInfo)
				{
					auxGeom.Draw2dLabel(
						screenPos.x,
						screenPos.y,
						Debug::g_objectFontSize,
						isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorTrigger,
						false,
						"%s", debugText.c_str());

					screenPos.y += Debug::g_objectLineHeight;
				}
			}

			if (drawStates && (isTextFilterDisabled || doesStateSwitchMatchFilter))
			{
				for (auto const& switchStatePair : switchStateInfo)
				{
					auto const pSwitch = switchStatePair.first;
					auto const pSwitchState = switchStatePair.second;

					Debug::CStateDrawData& drawData = m_stateDrawInfo.emplace(std::piecewise_construct, std::forward_as_tuple(pSwitch->GetId()), std::forward_as_tuple(pSwitchState->GetId())).first->second;
					drawData.Update(pSwitchState->GetId());
					ColorF const switchTextColor = { 0.8f, drawData.m_currentSwitchColor, 0.6f };

					auxGeom.Draw2dLabel(
						screenPos.x,
						screenPos.y,
						Debug::g_objectFontSize,
						isVirtual ? Debug::s_globalColorVirtual : switchTextColor,
						false,
						"%s: %s\n",
						pSwitch->GetName(),
						pSwitchState->GetName());

					screenPos.y += Debug::g_objectLineHeight;
				}
			}

			if (drawParameters && (isTextFilterDisabled || doesParameterMatchFilter))
			{
				for (auto const& parameterPair : parameterInfo)
				{
					auxGeom.Draw2dLabel(
						screenPos.x,
						screenPos.y,
						Debug::g_objectFontSize,
						isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorParameter,
						false,
						"%s: %2.2f\n",
						parameterPair.first,
						parameterPair.second);

					screenPos.y += Debug::g_objectLineHeight;
				}
			}

			if (drawEnvironments && (isTextFilterDisabled || doesEnvironmentMatchFilter))
			{
				for (auto const& environmentPair : environmentInfo)
				{
					auxGeom.Draw2dLabel(
						screenPos.x,
						screenPos.y,
						Debug::g_objectFontSize,
						isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorEnvironment,
						false,
						"%s: %.2f\n",
						environmentPair.first,
						environmentPair.second);

					screenPos.y += Debug::g_objectLineHeight;
				}
			}

			if (drawDistance && (isTextFilterDisabled || doesObjectNameMatchFilter))
			{
				CryFixedStringT<MaxMiscStringLength> debugText;

				if (m_maxRadius > 0.0f)
				{
					debugText.Format("Dist: %4.1fm / Max: %.1fm", distance, m_maxRadius);
				}
				else
				{
					debugText.Format("Dist: %4.1fm", distance);
				}

				auxGeom.Draw2dLabel(
					screenPos.x,
					screenPos.y,
					Debug::g_objectFontSize,
					isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorActive,
					false,
					"%s", debugText.c_str());

				screenPos.y += Debug::g_objectLineHeight;
			}

	#if defined(CRY_AUDIO_USE_OCCLUSION)
			if (((m_flags& EObjectFlags::CanRunOcclusion) != 0) && (isTextFilterDisabled || doesObjectNameMatchFilter))
			{
				if (drawOcclusionRayLabel)
				{
					EOcclusionType const occlusionType = m_propagationProcessor.GetOcclusionType();
					OcclusionInfos const& occlusionInfos = m_propagationProcessor.GetOcclusionInfos();

					for (SOcclusionInfo const* const pInfo : occlusionInfos)
					{
						float const occlusion = pInfo->occlusion;

						CryFixedStringT<MaxMiscStringLength> debugText;

						if (distance < g_cvars.m_occlusionMaxDistance)
						{
							if (occlusionType == EOcclusionType::Adaptive)
							{
								debugText.Format(
									"%s(%s)",
									Debug::g_szOcclusionTypes[static_cast<std::underlying_type<EOcclusionType>::type>(occlusionType)],
									Debug::g_szOcclusionTypes[static_cast<std::underlying_type<EOcclusionType>::type>(pInfo->occlusionTypeWhenAdaptive)]);
							}
							else
							{
								debugText.Format("%s", Debug::g_szOcclusionTypes[static_cast<std::underlying_type<EOcclusionType>::type>(occlusionType)]);
							}
						}
						else
						{
							debugText.Format("Ignore (exceeded activity range)");
						}

						ColorF const activeRayLabelColor = { occlusion, 1.0f - occlusion, 0.0f };

						auxGeom.Draw2dLabel(
							screenPos.x,
							screenPos.y,
							Debug::g_objectFontSize,
							isVirtual ? Debug::s_globalColorVirtual : (((occlusionType != EOcclusionType::None) && (occlusionType != EOcclusionType::Ignore)) ? activeRayLabelColor : Debug::s_globalColorInactive),
							false,
							"Occl: %3.2f | Type: %s | %s",
							occlusion,
							debugText.c_str(),
							pInfo->pListener->GetName());

						screenPos.y += Debug::g_objectLineHeight;
					}
				}

				if (drawOcclusionRayOffset && !isVirtual)
				{
					float const occlusionRayOffset = m_propagationProcessor.GetOcclusionRayOffset();

					if (occlusionRayOffset > 0.0f)
					{
						SAuxGeomRenderFlags const previousRenderFlags = auxGeom.GetRenderFlags();
						SAuxGeomRenderFlags newRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
						auxGeom.SetRenderFlags(newRenderFlags);

						auxGeom.DrawSphere(
							position,
							occlusionRayOffset,
							Debug::s_objectColorOcclusionOffsetSphere);

						auxGeom.SetRenderFlags(previousRenderFlags);
					}
				}

				if (drawOcclusionRaysOrSpheres)
				{
					m_propagationProcessor.DrawDebugInfo(auxGeom);
				}

				if (drawOcclusionListenerPlane)
				{
					m_propagationProcessor.DrawListenerPlane(auxGeom);
				}
			}
	#endif // CRY_AUDIO_USE_OCCLUSION

			if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectImplInfo) != 0)
			{
				m_pIObject->DrawDebugInfo(auxGeom, screenPos.x, screenPos.y, (isTextFilterDisabled ? nullptr : lowerCaseSearchString.c_str()));
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CObject::ForceImplementationRefresh()
{
	m_pIObject->SetTransformation(m_transformation);

	m_pIObject->ToggleFunctionality(EObjectFunctionality::TrackAbsoluteVelocity, (m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0);
	m_pIObject->ToggleFunctionality(EObjectFunctionality::TrackRelativeVelocity, (m_flags& EObjectFlags::TrackRelativeVelocity) != 0);

	// Parameters
	for (auto const& parameterPair : m_parameters)
	{
		CParameter const* const pParameter = stl::find_in_map(g_parameterLookup, parameterPair.first, nullptr);

		if (pParameter != nullptr)
		{
			pParameter->Set(*this, parameterPair.second);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Parameter \"%u\" does not exist!", parameterPair.first);
		}
	}

	// Switches
	for (auto const& switchPair : m_switchStates)
	{
		CSwitch const* const pSwitch = stl::find_in_map(g_switchLookup, switchPair.first, nullptr);

		if (pSwitch != nullptr)
		{
			CSwitchState const* const pState = stl::find_in_map(pSwitch->GetStates(), switchPair.second, nullptr);

			if (pState != nullptr)
			{
				pState->Set(*this);
			}
		}
	}

	// Environments
	for (auto const& environmentPair : m_environments)
	{
		CEnvironment const* const pEnvironment = stl::find_in_map(g_environmentLookup, environmentPair.first, nullptr);

		if (pEnvironment != nullptr)
		{
			pEnvironment->Set(*this, environmentPair.second);
		}
	}

	uint16 triggerCounter = 0;
	// Last re-execute its active triggers.
	for (auto const& triggerInstancePair : m_triggerInstances)
	{
		CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, triggerInstancePair.second->GetTriggerId(), nullptr);

		if (pTrigger != nullptr)
		{
			pTrigger->Execute(*this, triggerInstancePair.first, triggerInstancePair.second, triggerCounter);
			++triggerCounter;
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Trigger \"%u\" does not exist!", triggerInstancePair.second->GetTriggerId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::HandleSetName(char const* const szName)
{
	m_name = szName;
	return m_pIObject->SetName(szName);
}

//////////////////////////////////////////////////////////////////////////
void CObject::StoreParameterValue(ControlId const id, float const value)
{
	m_parameters[id] = value;
}

//////////////////////////////////////////////////////////////////////////
void CObject::StoreSwitchValue(ControlId const switchId, SwitchStateId const switchStateId)
{
	m_switchStates[switchId] = switchStateId;
}

//////////////////////////////////////////////////////////////////////////
void CObject::StoreEnvironmentValue(ControlId const id, float const value)
{
	m_environments[id] = value;
}

//////////////////////////////////////////////////////////////////////////
void CObject::UpdateMaxRadius(float const radius)
{
	m_maxRadius = std::max(radius, m_maxRadius);
}
#endif // CRY_AUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CObject::ExecuteTrigger(
	ControlId const triggerId,
	EntityId const entityId /*= INVALID_ENTITYID*/,
	SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::ExecuteTrigger> requestData(this, triggerId, entityId);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::StopTrigger(ControlId const triggerId /* = CryAudio::InvalidControlId */, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	if (triggerId != InvalidControlId)
	{
		SObjectRequestData<EObjectRequestType::StopTrigger> requestData(this, triggerId);
		PushRequest(requestData, userData);
	}
	else
	{
		SObjectRequestData<EObjectRequestType::StopAllTriggers> requestData(this);
		PushRequest(requestData, userData);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetTransformation(CTransformation const& transformation, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::SetTransformation> requestData(this, transformation);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::SetParameter> requestData(this, parameterId, value);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::SetSwitchState> requestData(this, switchId, switchStateId);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetEnvironment(EnvironmentId const environmentId, float const amount, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::SetEnvironment> requestData(this, environmentId, amount);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetCurrentEnvironments(EntityId const entityToIgnore, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::SetCurrentEnvironments> requestData(this, entityToIgnore);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
#if defined(CRY_AUDIO_USE_OCCLUSION)
	if (occlusionType < EOcclusionType::Count)
	{
		SObjectRequestData<EObjectRequestType::SetOcclusionType> requestData(this, occlusionType);
		PushRequest(requestData, userData);
	}
#endif // CRY_AUDIO_USE_OCCLUSION
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetOcclusionRayOffset(float const offset, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
#if defined(CRY_AUDIO_USE_OCCLUSION)
	SObjectRequestData<EObjectRequestType::SetOcclusionRayOffset> requestData(this, offset);
	PushRequest(requestData, userData);
#endif // CRY_AUDIO_USE_OCCLUSION
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetName(char const* const szName, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	SObjectRequestData<EObjectRequestType::SetName> requestData(this, szName);
	PushRequest(requestData, userData);
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CObject::AddListener(ListenerId const id, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SObjectRequestData<EObjectRequestType::AddListener> requestData(this, id);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::RemoveListener(ListenerId const id, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SObjectRequestData<EObjectRequestType::RemoveListener> requestData(this, id);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::ToggleAbsoluteVelocityTracking(bool const enable, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::ToggleAbsoluteVelocityTracking> requestData(this, enable);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CObject::ToggleRelativeVelocityTracking(bool const enable, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SObjectRequestData<EObjectRequestType::ToggleRelativeVelocityTracking> requestData(this, enable);
	PushRequest(requestData, userData);
}
} // namespace CryAudio
