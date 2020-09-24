// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "System.h"
#include "Impl.h"
#include "PoolObject_impl.h"
#include "Managers.h"
#include "FileCacheManager.h"
#include "ListenerManager.h"
#include "EventListenerManager.h"
#include "XMLProcessor.h"
#include "SystemRequestData.h"
#include "ObjectRequestData.h"
#include "ListenerRequestData.h"
#include "CallbackRequestData.h"
#include "CVars.h"
#include "File.h"
#include "Listener.h"
#include "Object.h"
#include "LoseFocusTrigger.h"
#include "GetFocusTrigger.h"
#include "MuteAllTrigger.h"
#include "UnmuteAllTrigger.h"
#include "PauseAllTrigger.h"
#include "ResumeAllTrigger.h"
#include "Environment.h"
#include "Parameter.h"
#include "PreloadRequest.h"
#include "Setting.h"
#include "Switch.h"
#include "SwitchState.h"
#include "Trigger.h"
#include "TriggerInstance.h"
#include "Common/IObject.h"
#include <CrySystem/ITimer.h>
#include <CryString/CryPath.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryMath/Cry_Camera.h>

#if defined(CRY_AUDIO_USE_OCCLUSION)
	#include "PropagationProcessor.h"
#endif // CRY_AUDIO_USE_OCCLUSION

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "PreviewTrigger.h"
	#include "Debug.h"
	#include "Common/Logger.h"
	#include "Common/DebugStyle.h"
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
ContextId g_currentLevelContextId = InvalidContextId;

constexpr uint16 g_systemExecuteTriggerPoolSize = 4;
constexpr uint16 g_systemExecuteTriggerExPoolSize = 16;
constexpr uint16 g_systemExecuteTriggerWithCallbacksPoolSize = 16;
constexpr uint16 g_systemStopTriggerPoolSize = 4;
constexpr uint16 g_systemRegisterObjectPoolSize = 16;
constexpr uint16 g_systemReleaseObjectPoolSize = 16;
constexpr uint16 g_systemSetParameterPoolSize = 4;
constexpr uint16 g_systemSetSwitchStatePoolSize = 4;

constexpr uint16 g_objectExecuteTriggerPoolSize = 64;
constexpr uint16 g_objectExecuteTriggerWithCallbacksPoolSize = 16;
constexpr uint16 g_objectStopTriggerPoolSize = 128;
constexpr uint16 g_objectSetTransformationPoolSize = 128;
constexpr uint16 g_objectSetParameterPoolSize = 128;
constexpr uint16 g_objectSetSwitchStatePoolSize = 64;
constexpr uint16 g_objectSetCurrentEnvironmentsPoolSize = 16;
constexpr uint16 g_objectSetEnvironmentPoolSize = 64;
constexpr uint16 g_objectProcessPhysicsRayPoolSize = 128;

constexpr uint16 g_listenerSetTransformationPoolSize = 2;

constexpr uint16 g_callbackReportStartedTriggerConnectionInstancePoolSize = 64;
constexpr uint16 g_callbackReportFinishedTriggerConnectionInstancePoolSize = 128;
constexpr uint16 g_callbackReportFinishedTriggerInstancePoolSize = 128;
constexpr uint16 g_callbackReportTriggerConnectionInstanceCallbackPoolSize = 16;
constexpr uint16 g_callbackSendTriggerInstanceCallbackPoolSize = 16;
constexpr uint16 g_callbackReportPhysicalizedObjectPoolSize = 64;
constexpr uint16 g_callbackReportVirtualizedObjectPoolSize = 64;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
enum class ELoggingOptions : EnumFlagsType
{
	None,
	Errors   = BIT(6), // a
	Warnings = BIT(7), // b
	Comments = BIT(8), // c
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ELoggingOptions);

enum class EDebugUpdateFilter : EnumFlagsType
{
	None             = 0,
	FileCacheManager = BIT(0),
	Contexts         = BIT(1),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EDebugUpdateFilter);

struct SRequestCount final
{
	uint16 requests = 0;

	uint16 systemExecuteTrigger = 0;
	uint16 systemExecuteTriggerEx = 0;
	uint16 systemExecuteTriggerWithCallbacks = 0;
	uint16 systemStopTrigger = 0;
	uint16 systemRegisterObject = 0;
	uint16 systemReleaseObject = 0;
	uint16 systemSetParameter = 0;
	uint16 systemSetSwitchState = 0;

	uint16 objectExecuteTrigger = 0;
	uint16 objectExecuteTriggerWithCallbacks = 0;
	uint16 objectStopTrigger = 0;
	uint16 objectSetTransformation = 0;
	uint16 objectSetParameter = 0;
	uint16 objectSetSwitchState = 0;
	uint16 objectSetCurrentEnvironments = 0;
	uint16 objectSetEnvironment = 0;
	uint16 objectProcessPhysicsRay = 0;

	uint16 listenerSetTransformation = 0;

	uint16 callbackReportStartedriggerConnectionInstance = 0;
	uint16 callbackReportFinishedTriggerConnectionInstance = 0;
	uint16 callbackReportFinishedTriggerInstance = 0;
	uint16 callbackReportTriggerConnectionInstanceCallback = 0;
	uint16 callbackSendTriggerInstanceCallback = 0;
	uint16 callbackReportPhysicalizedObject = 0;
	uint16 callbackReportVirtualizedObject = 0;
};

SRequestCount g_requestsPerUpdate;
SRequestCount g_requestPeaks;
Debug::StateDrawInfo g_stateDrawInfo;

//////////////////////////////////////////////////////////////////////////
void CountRequestPerUpdate(CRequest const& request)
{
	auto const pRequestData = request.GetData();

	if (pRequestData != nullptr)
	{
		g_requestsPerUpdate.requests++;

		switch (request.GetData()->requestType)
		{
		case ERequestType::SystemRequest:
			{
				auto const pBase = static_cast<SSystemRequestDataBase const*>(pRequestData);

				switch (pBase->systemRequestType)
				{
				case ESystemRequestType::RegisterObject:
					{
						g_requestsPerUpdate.systemRegisterObject++;

						break;
					}
				case ESystemRequestType::ReleaseObject:
					{
						g_requestsPerUpdate.systemReleaseObject++;

						break;
					}
				case ESystemRequestType::ExecuteTrigger:
					{
						g_requestsPerUpdate.systemExecuteTrigger++;

						break;
					}
				case ESystemRequestType::ExecuteTriggerEx:
					{
						g_requestsPerUpdate.systemExecuteTriggerEx++;

						break;
					}
				case ESystemRequestType::ExecuteTriggerWithCallbacks:
					{
						g_requestsPerUpdate.systemExecuteTriggerWithCallbacks++;

						break;
					}
				case ESystemRequestType::StopTrigger:
					{
						g_requestsPerUpdate.systemStopTrigger++;

						break;
					}
				case ESystemRequestType::SetParameter:
					{
						g_requestsPerUpdate.systemSetParameter++;

						break;
					}
				case ESystemRequestType::SetSwitchState:
					{
						g_requestsPerUpdate.systemSetSwitchState++;

						break;
					}
				default:
					{
						break;
					}
				}

				break;
			}
		case ERequestType::ObjectRequest:
			{
				auto const pBase = static_cast<SObjectRequestDataBase const*>(pRequestData);

				switch (pBase->objectRequestType)
				{
				case EObjectRequestType::ExecuteTrigger:
					{
						g_requestsPerUpdate.objectExecuteTrigger++;

						break;
					}
				case EObjectRequestType::ExecuteTriggerWithCallbacks:
					{
						g_requestsPerUpdate.objectExecuteTriggerWithCallbacks++;

						break;
					}
				case EObjectRequestType::StopTrigger:
					{
						g_requestsPerUpdate.objectStopTrigger++;

						break;
					}
				case EObjectRequestType::SetTransformation:
					{
						g_requestsPerUpdate.objectSetTransformation++;

						break;
					}
				case EObjectRequestType::SetParameter:
					{
						g_requestsPerUpdate.objectSetParameter++;

						break;
					}
				case EObjectRequestType::SetSwitchState:
					{
						g_requestsPerUpdate.objectSetSwitchState++;

						break;
					}
				case EObjectRequestType::SetCurrentEnvironments:
					{
						g_requestsPerUpdate.objectSetCurrentEnvironments++;

						break;
					}
				case EObjectRequestType::SetEnvironment:
					{
						g_requestsPerUpdate.objectSetEnvironment++;

						break;
					}
				case EObjectRequestType::ProcessPhysicsRay:
					{
						g_requestsPerUpdate.objectProcessPhysicsRay++;

						break;
					}
				default:
					{
						break;
					}
				}

				break;
			}
		case ERequestType::ListenerRequest:
			{
				auto const pBase = static_cast<SListenerRequestDataBase const*>(pRequestData);

				if (pBase->listenerRequestType == EListenerRequestType::SetTransformation)
				{
					g_requestsPerUpdate.listenerSetTransformation++;
				}

				break;
			}
		case ERequestType::CallbackRequest:
			{
				auto const pBase = static_cast<SCallbackRequestDataBase const*>(pRequestData);

				switch (pBase->callbackRequestType)
				{
				case ECallbackRequestType::ReportStartedTriggerConnectionInstance:
					{
						g_requestsPerUpdate.callbackReportStartedriggerConnectionInstance++;

						break;
					}
				case ECallbackRequestType::ReportFinishedTriggerConnectionInstance:
					{
						g_requestsPerUpdate.callbackReportFinishedTriggerConnectionInstance++;

						break;
					}
				case ECallbackRequestType::ReportFinishedTriggerInstance:
					{
						g_requestsPerUpdate.callbackReportFinishedTriggerInstance++;

						break;
					}
				case ECallbackRequestType::ReportTriggerConnectionInstanceCallback:
					{
						g_requestsPerUpdate.callbackReportTriggerConnectionInstanceCallback++;

						break;
					}
				case ECallbackRequestType::SendTriggerInstanceCallback:
					{
						g_requestsPerUpdate.callbackSendTriggerInstanceCallback++;

						break;
					}
				case ECallbackRequestType::ReportPhysicalizedObject:
					{
						g_requestsPerUpdate.callbackReportPhysicalizedObject++;

						break;
					}
				case ECallbackRequestType::ReportVirtualizedObject:
					{
						g_requestsPerUpdate.callbackReportVirtualizedObject++;

						break;
					}
				default:
					{
						break;
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
}

//////////////////////////////////////////////////////////////////////////
void SetRequestCountPeak()
{
	g_requestPeaks.requests = std::max(g_requestPeaks.requests, g_requestsPerUpdate.requests);

	g_requestPeaks.systemRegisterObject = std::max(g_requestPeaks.systemRegisterObject, g_requestsPerUpdate.systemRegisterObject);
	g_requestPeaks.systemReleaseObject = std::max(g_requestPeaks.systemReleaseObject, g_requestsPerUpdate.systemReleaseObject);
	g_requestPeaks.systemExecuteTrigger = std::max(g_requestPeaks.systemExecuteTrigger, g_requestsPerUpdate.systemExecuteTrigger);
	g_requestPeaks.systemExecuteTriggerEx = std::max(g_requestPeaks.systemExecuteTriggerEx, g_requestsPerUpdate.systemExecuteTriggerEx);
	g_requestPeaks.systemExecuteTriggerWithCallbacks = std::max(g_requestPeaks.systemExecuteTriggerWithCallbacks, g_requestsPerUpdate.systemExecuteTriggerWithCallbacks);
	g_requestPeaks.systemStopTrigger = std::max(g_requestPeaks.systemStopTrigger, g_requestsPerUpdate.systemStopTrigger);
	g_requestPeaks.systemSetParameter = std::max(g_requestPeaks.systemSetParameter, g_requestsPerUpdate.systemSetParameter);
	g_requestPeaks.systemSetSwitchState = std::max(g_requestPeaks.systemSetSwitchState, g_requestsPerUpdate.systemSetSwitchState);

	g_requestPeaks.objectExecuteTrigger = std::max(g_requestPeaks.objectExecuteTrigger, g_requestsPerUpdate.objectExecuteTrigger);
	g_requestPeaks.objectExecuteTriggerWithCallbacks = std::max(g_requestPeaks.objectExecuteTriggerWithCallbacks, g_requestsPerUpdate.objectExecuteTriggerWithCallbacks);
	g_requestPeaks.objectStopTrigger = std::max(g_requestPeaks.objectStopTrigger, g_requestsPerUpdate.objectStopTrigger);
	g_requestPeaks.objectSetTransformation = std::max(g_requestPeaks.objectSetTransformation, g_requestsPerUpdate.objectSetTransformation);
	g_requestPeaks.objectSetParameter = std::max(g_requestPeaks.objectSetParameter, g_requestsPerUpdate.objectSetParameter);
	g_requestPeaks.objectSetSwitchState = std::max(g_requestPeaks.objectSetSwitchState, g_requestsPerUpdate.objectSetSwitchState);
	g_requestPeaks.objectSetCurrentEnvironments = std::max(g_requestPeaks.objectSetCurrentEnvironments, g_requestsPerUpdate.objectSetCurrentEnvironments);
	g_requestPeaks.objectSetEnvironment = std::max(g_requestPeaks.objectSetEnvironment, g_requestsPerUpdate.objectSetEnvironment);
	g_requestPeaks.objectProcessPhysicsRay = std::max(g_requestPeaks.objectProcessPhysicsRay, g_requestsPerUpdate.objectProcessPhysicsRay);

	g_requestPeaks.listenerSetTransformation = std::max(g_requestPeaks.listenerSetTransformation, g_requestsPerUpdate.listenerSetTransformation);

	g_requestPeaks.callbackReportStartedriggerConnectionInstance = std::max(g_requestPeaks.callbackReportStartedriggerConnectionInstance, g_requestsPerUpdate.callbackReportStartedriggerConnectionInstance);
	g_requestPeaks.callbackReportFinishedTriggerConnectionInstance = std::max(g_requestPeaks.callbackReportFinishedTriggerConnectionInstance, g_requestsPerUpdate.callbackReportFinishedTriggerConnectionInstance);
	g_requestPeaks.callbackReportFinishedTriggerInstance = std::max(g_requestPeaks.callbackReportFinishedTriggerInstance, g_requestsPerUpdate.callbackReportFinishedTriggerInstance);
	g_requestPeaks.callbackReportTriggerConnectionInstanceCallback = std::max(g_requestPeaks.callbackReportTriggerConnectionInstanceCallback, g_requestsPerUpdate.callbackReportTriggerConnectionInstanceCallback);
	g_requestPeaks.callbackSendTriggerInstanceCallback = std::max(g_requestPeaks.callbackSendTriggerInstanceCallback, g_requestsPerUpdate.callbackSendTriggerInstanceCallback);
	g_requestPeaks.callbackReportPhysicalizedObject = std::max(g_requestPeaks.callbackReportPhysicalizedObject, g_requestsPerUpdate.callbackReportPhysicalizedObject);
	g_requestPeaks.callbackReportVirtualizedObject = std::max(g_requestPeaks.callbackReportVirtualizedObject, g_requestsPerUpdate.callbackReportVirtualizedObject);

	ZeroStruct(g_requestsPerUpdate);
}

//////////////////////////////////////////////////////////////////////////
bool ExecuteDefaultTrigger(ControlId const id)
{
	bool wasSuccess = true;

	switch (id)
	{
	case g_loseFocusTriggerId:
		{
			g_loseFocusTrigger.Execute();
			break;
		}
	case g_getFocusTriggerId:
		{
			g_getFocusTrigger.Execute();
			break;
		}
	case g_muteAllTriggerId:
		{
			g_muteAllTrigger.Execute();
			break;
		}
	case g_unmuteAllTriggerId:
		{
			g_unmuteAllTrigger.Execute();
			break;
		}
	case g_pauseAllTriggerId:
		{
			g_pauseAllTrigger.Execute();
			break;
		}
	case g_resumeAllTriggerId:
		{
			g_resumeAllTrigger.Execute();
			break;
		}
	default:
		{
			wasSuccess = false;
			break;
		}
	}

	return wasSuccess;
}

///////////////////////////////////////////////////////////////////////////
void ForceGlobalDataImplRefresh()
{
	// Parameters
	for (auto const& parameterPair : g_parameters)
	{
		CParameter const* const pParameter = stl::find_in_map(g_parameterLookup, parameterPair.first, nullptr);

		if (pParameter != nullptr)
		{
			pParameter->Set(parameterPair.second);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Parameter \"%u\" does not exist!", parameterPair.first);
		}
	}

	for (auto const& parameterPair : g_parametersGlobally)
	{
		CParameter const* const pParameter = stl::find_in_map(g_parameterLookup, parameterPair.first, nullptr);

		if (pParameter != nullptr)
		{
			pParameter->SetGlobally(parameterPair.second);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Parameter \"%u\" does not exist!", parameterPair.first);
		}
	}

	// Switches
	for (auto const& switchPair : g_switchStates)
	{
		CSwitch const* const pSwitch = stl::find_in_map(g_switchLookup, switchPair.first, nullptr);

		if (pSwitch != nullptr)
		{
			CSwitchState const* const pState = stl::find_in_map(pSwitch->GetStates(), switchPair.second, nullptr);

			if (pState != nullptr)
			{
				pState->Set();
			}
		}
	}

	for (auto const& switchPair : g_switchStatesGlobally)
	{
		CSwitch const* const pSwitch = stl::find_in_map(g_switchLookup, switchPair.first, nullptr);

		if (pSwitch != nullptr)
		{
			CSwitchState const* const pState = stl::find_in_map(pSwitch->GetStates(), switchPair.second, nullptr);

			if (pState != nullptr)
			{
				pState->SetGlobally();
			}
		}
	}

	uint16 triggerCounter = 0;
	// Last re-execute its active triggers.
	for (auto const& triggerInstancePair : g_triggerInstances)
	{
		CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, triggerInstancePair.second->GetTriggerId(), nullptr);

		if (pTrigger != nullptr)
		{
			pTrigger->Execute(triggerInstancePair.first, triggerInstancePair.second, triggerCounter);
			++triggerCounter;
		}
		else if (!ExecuteDefaultTrigger(triggerInstancePair.second->GetTriggerId()))
		{
			Cry::Audio::Log(ELogType::Warning, "Trigger \"%u\" does not exist!", triggerInstancePair.second->GetTriggerId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void UpdateContextDebugInfo(char const* const szDebugFilter)
{
	g_contextDebugInfo.clear();

	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(szDebugFilter);
	lowerCaseSearchString.MakeLower();

	for (auto const& contextPair : g_contextInfo)
	{
		if (contextPair.second.isRegistered)
		{
			char const* const szContextName = contextPair.first.c_str();
			CryFixedStringT<MaxControlNameLength> lowerCaseContextName(szContextName);
			lowerCaseContextName.MakeLower();

			if ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseContextName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos))
			{
				g_contextDebugInfo.emplace(std::piecewise_construct, std::forward_as_tuple(szContextName), std::forward_as_tuple(contextPair.second.isActive));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void HandleUpdateDebugInfo(EDebugUpdateFilter const filter)
{
	char const* const szDebugFilter = g_cvars.m_pDebugFilter->GetString();

	if ((filter& EDebugUpdateFilter::FileCacheManager) != EDebugUpdateFilter::None)
	{
		g_fileCacheManager.UpdateDebugInfo(szDebugFilter);
	}

	if ((filter& EDebugUpdateFilter::Contexts) != EDebugUpdateFilter::None)
	{
		UpdateContextDebugInfo(szDebugFilter);
	}
}
#endif // CRY_AUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void ReleaseGlobalData()
{
	for (auto& triggerInstancePair : g_triggerInstances)
	{
		triggerInstancePair.second->Release();
	}

	g_pIObject = nullptr;
}

///////////////////////////////////////////////////////////////////////////
void ReportStartedGlobalTriggerInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result)
{
	TriggerInstances::iterator const iter(g_triggerInstances.find(triggerInstanceId));

	if (iter != g_triggerInstances.end())
	{
		iter->second->SetPendingToPlaying();
	}
}

///////////////////////////////////////////////////////////////////////////
void ReportFinishedTriggerInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result)
{
	TriggerInstances::iterator const iter(g_triggerInstances.find(triggerInstanceId));

	if (iter != g_triggerInstances.end())
	{
		CTriggerInstance* const pTriggerInstance = iter->second;

		if (result != ETriggerResult::Pending)
		{
			if (pTriggerInstance->IsPlayingInstanceFinished())
			{
				g_triggerInstanceIds.erase(std::remove(g_triggerInstanceIds.begin(), g_triggerInstanceIds.end(), triggerInstanceId), g_triggerInstanceIds.end());
				pTriggerInstance->SendFinishedRequest();

				g_triggerInstances.erase(iter);
				delete pTriggerInstance;
			}
		}
		else
		{
			if (pTriggerInstance->IsPendingInstanceFinished())
			{
				g_triggerInstanceIds.erase(std::remove(g_triggerInstanceIds.begin(), g_triggerInstanceIds.end(), triggerInstanceId), g_triggerInstanceIds.end());
				pTriggerInstance->SendFinishedRequest();

				g_triggerInstances.erase(iter);
				delete pTriggerInstance;
			}
		}
	}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown trigger instance id %u during %s", triggerInstanceId, __FUNCTION__);
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void ReportTriggerInstanceCallback(TriggerInstanceId const triggerInstanceId, ESystemEvents const events)
{
	TriggerInstances::iterator const iter(g_triggerInstances.find(triggerInstanceId));

	if (iter != g_triggerInstances.end())
	{
		iter->second->SendCallbackRequest(events);
	}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown trigger instance id %u during %s", triggerInstanceId, __FUNCTION__);
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void UpdateGlobalData(float const deltaTime)
{
	if (!g_triggerInstances.empty())
	{
		g_pIObject->Update(deltaTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void AllocateMemoryPools()
{
	// Controls
	CTrigger::CreateAllocator(g_poolSizes.triggers);
	CParameter::CreateAllocator(g_poolSizes.parameters);
	CSwitch::CreateAllocator(g_poolSizes.switches);
	CSwitchState::CreateAllocator(g_poolSizes.states);
	CEnvironment::CreateAllocator(g_poolSizes.environments);
	CPreloadRequest::CreateAllocator(g_poolSizes.preloads);
	CSetting::CreateAllocator(g_poolSizes.settings);

	// Files
	CFile::CreateAllocator(g_poolSizes.files);

	// System requests
	SSystemRequestData<ESystemRequestType::ExecuteTrigger>::CreateAllocator(g_systemExecuteTriggerPoolSize);
	SSystemRequestData<ESystemRequestType::ExecuteTriggerEx>::CreateAllocator(g_systemExecuteTriggerExPoolSize);
	SSystemRequestData<ESystemRequestType::ExecuteTriggerWithCallbacks>::CreateAllocator(g_systemExecuteTriggerWithCallbacksPoolSize);
	SSystemRequestData<ESystemRequestType::StopTrigger>::CreateAllocator(g_systemStopTriggerPoolSize);
	SSystemRequestData<ESystemRequestType::RegisterObject>::CreateAllocator(g_systemRegisterObjectPoolSize);
	SSystemRequestData<ESystemRequestType::ReleaseObject>::CreateAllocator(g_systemReleaseObjectPoolSize);
	SSystemRequestData<ESystemRequestType::SetParameter>::CreateAllocator(g_systemSetParameterPoolSize);
	SSystemRequestData<ESystemRequestType::SetSwitchState>::CreateAllocator(g_systemSetSwitchStatePoolSize);

	// Object requests
	SObjectRequestData<EObjectRequestType::ExecuteTrigger>::CreateAllocator(g_objectExecuteTriggerPoolSize);
	SObjectRequestData<EObjectRequestType::ExecuteTriggerWithCallbacks>::CreateAllocator(g_objectExecuteTriggerWithCallbacksPoolSize);
	SObjectRequestData<EObjectRequestType::StopTrigger>::CreateAllocator(g_objectStopTriggerPoolSize);
	SObjectRequestData<EObjectRequestType::SetTransformation>::CreateAllocator(g_objectSetTransformationPoolSize);
	SObjectRequestData<EObjectRequestType::SetParameter>::CreateAllocator(g_objectSetParameterPoolSize);
	SObjectRequestData<EObjectRequestType::SetSwitchState>::CreateAllocator(g_objectSetSwitchStatePoolSize);
	SObjectRequestData<EObjectRequestType::SetCurrentEnvironments>::CreateAllocator(g_objectSetCurrentEnvironmentsPoolSize);
	SObjectRequestData<EObjectRequestType::SetEnvironment>::CreateAllocator(g_objectSetEnvironmentPoolSize);
	SObjectRequestData<EObjectRequestType::ProcessPhysicsRay>::CreateAllocator(g_objectProcessPhysicsRayPoolSize);

	// Listener requests
	SListenerRequestData<EListenerRequestType::SetTransformation>::CreateAllocator(g_listenerSetTransformationPoolSize);

	// Callback requests
	SCallbackRequestData<ECallbackRequestType::ReportStartedTriggerConnectionInstance>::CreateAllocator(g_callbackReportStartedTriggerConnectionInstancePoolSize);
	SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerConnectionInstance>::CreateAllocator(g_callbackReportFinishedTriggerConnectionInstancePoolSize);
	SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerInstance>::CreateAllocator(g_callbackReportFinishedTriggerInstancePoolSize);
	SCallbackRequestData<ECallbackRequestType::ReportTriggerConnectionInstanceCallback>::CreateAllocator(g_callbackReportTriggerConnectionInstanceCallbackPoolSize);
	SCallbackRequestData<ECallbackRequestType::SendTriggerInstanceCallback>::CreateAllocator(g_callbackSendTriggerInstanceCallbackPoolSize);
	SCallbackRequestData<ECallbackRequestType::ReportPhysicalizedObject>::CreateAllocator(g_callbackReportPhysicalizedObjectPoolSize);
	SCallbackRequestData<ECallbackRequestType::ReportVirtualizedObject>::CreateAllocator(g_callbackReportVirtualizedObjectPoolSize);
}

//////////////////////////////////////////////////////////////////////////
void FreeMemoryPools()
{
	// Controls
	CTrigger::FreeMemoryPool();
	CParameter::FreeMemoryPool();
	CSwitch::FreeMemoryPool();
	CSwitchState::FreeMemoryPool();
	CEnvironment::FreeMemoryPool();
	CPreloadRequest::FreeMemoryPool();
	CSetting::FreeMemoryPool();

	// Files
	CFile::FreeMemoryPool();

	// System requests
	SSystemRequestData<ESystemRequestType::ExecuteTrigger>::FreeMemoryPool();
	SSystemRequestData<ESystemRequestType::ExecuteTriggerEx>::FreeMemoryPool();
	SSystemRequestData<ESystemRequestType::ExecuteTriggerWithCallbacks>::FreeMemoryPool();
	SSystemRequestData<ESystemRequestType::StopTrigger>::FreeMemoryPool();
	SSystemRequestData<ESystemRequestType::RegisterObject>::FreeMemoryPool();
	SSystemRequestData<ESystemRequestType::ReleaseObject>::FreeMemoryPool();
	SSystemRequestData<ESystemRequestType::SetParameter>::FreeMemoryPool();
	SSystemRequestData<ESystemRequestType::SetSwitchState>::FreeMemoryPool();

	// Object requests
	SObjectRequestData<EObjectRequestType::ExecuteTrigger>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::ExecuteTriggerWithCallbacks>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::StopTrigger>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::SetTransformation>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::SetParameter>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::SetSwitchState>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::SetCurrentEnvironments>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::SetEnvironment>::FreeMemoryPool();
	SObjectRequestData<EObjectRequestType::ProcessPhysicsRay>::FreeMemoryPool();

	// Listener requests
	SListenerRequestData<EListenerRequestType::SetTransformation>::FreeMemoryPool();

	// Callback requests
	SCallbackRequestData<ECallbackRequestType::ReportStartedTriggerConnectionInstance>::FreeMemoryPool();
	SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerConnectionInstance>::FreeMemoryPool();
	SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerInstance>::FreeMemoryPool();
	SCallbackRequestData<ECallbackRequestType::ReportTriggerConnectionInstanceCallback>::FreeMemoryPool();
	SCallbackRequestData<ECallbackRequestType::SendTriggerInstanceCallback>::FreeMemoryPool();
	SCallbackRequestData<ECallbackRequestType::ReportPhysicalizedObject>::FreeMemoryPool();
	SCallbackRequestData<ECallbackRequestType::ReportVirtualizedObject>::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void UpdateActiveObjects(float const deltaTime)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE) && defined(CRY_AUDIO_USE_OCCLUSION)
	CPropagationProcessor::s_totalAsyncPhysRays = 0;
	CPropagationProcessor::s_totalSyncPhysRays = 0;
#endif // CRY_AUDIO_USE_DEBUG_CODE && CRY_AUDIO_USE_OCCLUSION

	if (deltaTime > 0.0f)
	{
		UpdateGlobalData(deltaTime);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		if (g_previewObject.IsActive())
		{
			g_previewObject.Update(deltaTime);
		}
#endif // CRY_AUDIO_USE_DEBUG_CODE

		auto iter = g_activeObjects.begin();
		auto iterEnd = g_activeObjects.end();

		while (iter != iterEnd)
		{
			CObject* const pObject = *iter;

			if (pObject->IsActive())
			{
				pObject->Update(deltaTime);
			}
			else if (!pObject->HasPendingCallbacks())
			{
				pObject->RemoveFlag(EObjectFlags::Active);

				if ((pObject->GetFlags() & EObjectFlags::InUse) == EObjectFlags::None)
				{
					pObject->Destruct();
				}

				if (iter != (iterEnd - 1))
				{
					(*iter) = g_activeObjects.back();
				}

				g_activeObjects.pop_back();
				iter = g_activeObjects.begin();
				iterEnd = g_activeObjects.end();
				continue;
			}

			++iter;
		}
	}
	else
	{
		UpdateGlobalData(deltaTime);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		if (g_previewObject.IsActive())
		{
			g_previewObject.Update(deltaTime);
		}
#endif // CRY_AUDIO_USE_DEBUG_CODE

		for (auto const pObject : g_activeObjects)
		{
			pObject->GetImplData()->Update(deltaTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CSystem::~CSystem()
{
	CRY_ASSERT_MESSAGE(g_pIImpl == nullptr, "<Audio> The implementation must get destroyed before the audio system is destructed during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(g_activeObjects.empty(), "There are still active objects during %s", __FUNCTION__);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CRY_ASSERT_MESSAGE(g_constructedObjects.empty(), "There are still objects during %s", __FUNCTION__);
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::AddRequestListener(void (*func)(SRequestInfo const* const), void* const pObjectToListenTo, ESystemEvents const eventMask)
{
	SSystemRequestData<ESystemRequestType::AddRequestListener> const requestData(pObjectToListenTo, func, eventMask);
	CRequest const request(&requestData, ERequestFlags::ExecuteBlocking, pObjectToListenTo); // This makes sure that the listener is notified.
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RemoveRequestListener(void (*func)(SRequestInfo const* const), void* const pObjectToListenTo)
{
	SSystemRequestData<ESystemRequestType::RemoveRequestListener> const requestData(pObjectToListenTo, func);
	CRequest const request(&requestData, ERequestFlags::ExecuteBlocking, pObjectToListenTo); // This makes sure that the listener is notified.
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExternalUpdate()
{
	CRY_PROFILE_SECTION(PROFILE_AUDIO, "Audio: External Update");

	CRY_ASSERT(gEnv->mMainThreadId == CryGetCurrentThreadId());
	CRequest request;

	while (m_syncCallbacks.dequeue(request))
	{
		NotifyListener(request);

		if (request.GetData()->requestType == ERequestType::ObjectRequest)
		{
			auto const pBase = static_cast<SObjectRequestDataBase const*>(request.GetData());
			pBase->pObject->DecrementSyncCallbackCounter();
			// No sync callback counting for default object, because it gets released on unloading of the audio system dll.
		}
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	DrawDebug();
#endif // CRY_AUDIO_USE_DEBUG_CODE

	m_accumulatedFrameTime += gEnv->pTimer->GetFrameTime();
	++m_externalThreadFrameId;

	// If sleeping, wake up the audio thread to start processing requests again.
	m_audioThreadWakeupEvent.Set();
}

//////////////////////////////////////////////////////////////////////////
void CSystem::PushRequest(CRequest const& request)
{
	CRY_PROFILE_FUNCTION(PROFILE_AUDIO);

	if ((g_systemStates& ESystemStates::ImplShuttingDown) == ESystemStates::None)
	{
		m_requestQueue.enqueue(request);

		if ((request.flags & ERequestFlags::ExecuteBlocking) != ERequestFlags::None)
		{
			// If sleeping, wake up the audio thread to start processing requests again.
			m_audioThreadWakeupEvent.Set();

			m_mainEvent.Wait();
			m_mainEvent.Reset();

			if ((request.flags & ERequestFlags::CallbackOnExternalOrCallingThread) != ERequestFlags::None)
			{
				NotifyListener(m_syncRequest);
			}
		}
	}
	else
	{
		Log(ELogType::Warning, "Discarded PushRequest due to Audio System not allowing for new ones!");
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		{
			// This event is issued in Editor and Game mode.
			string const contextName = PathUtil::GetFileName(reinterpret_cast<const char*>(wparam));

			if (!contextName.empty() && (contextName.compareNoCase("Untitled") != 0))
			{
				g_currentLevelContextId = StringToId(contextName.c_str());

				CryFixedStringT<MaxFilePathLength> contextPath(g_configPath);
				contextPath += g_szContextsFolderName;
				contextPath += "/";
				contextPath += contextName;

				SSystemRequestData<ESystemRequestType::ParseControlsData> const requestData1(contextPath.c_str(), contextName.c_str(), g_currentLevelContextId);
				CRequest const request1(&requestData1);
				PushRequest(request1);

				SSystemRequestData<ESystemRequestType::ParsePreloadsData> const requestData2(contextPath.c_str(), g_currentLevelContextId);
				CRequest const request2(&requestData2);
				PushRequest(request2);

				auto const preloadRequestId = static_cast<PreloadRequestId>(g_currentLevelContextId);
				SSystemRequestData<ESystemRequestType::PreloadSingleRequest> const requestData3(preloadRequestId, true);
				CRequest const request3(&requestData3);
				PushRequest(request3);

				SSystemRequestData<ESystemRequestType::AutoLoadSetting> const requestData4(g_currentLevelContextId);
				CRequest const request4(&requestData4);
				PushRequest(request4);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				ResetRequestCount();
#endif    // CRY_AUDIO_USE_DEBUG_CODE
			}

			break;
		}
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			// This event is issued in Editor and Game mode.
#if defined(CRY_AUDIO_USE_OCCLUSION)
			CPropagationProcessor::s_bCanIssueRWIs = false;
#endif    // CRY_AUDIO_USE_OCCLUSION

			SSystemRequestData<ESystemRequestType::ReleasePendingRays> const requestData1;
			CRequest const request1(&requestData1);
			PushRequest(request1);

			SSystemRequestData<ESystemRequestType::UnloadAFCMDataByContext> const requestData2(g_currentLevelContextId);
			CRequest const request2(&requestData2);
			PushRequest(request2);

			SSystemRequestData<ESystemRequestType::ClearControlsData> const requestData3(g_currentLevelContextId);
			CRequest const request3(&requestData3);
			PushRequest(request3);

			SSystemRequestData<ESystemRequestType::ClearPreloadsData> const requestData4(g_currentLevelContextId);
			CRequest const request4(&requestData4);
			PushRequest(request4);

			break;
		}
#if defined(CRY_AUDIO_USE_OCCLUSION)
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			// This event is issued in Editor and Game mode.
			CPropagationProcessor::s_bCanIssueRWIs = true;

			break;
		}
#endif    // CRY_AUDIO_USE_OCCLUSION
	case ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE:
		{
			if (gEnv->pInput != nullptr)
			{
				gEnv->pInput->AddConsoleEventListener(this);
			}

			break;
		}
	case ESYSTEM_EVENT_FULL_SHUTDOWN: // Intentional fall-through.
	case ESYSTEM_EVENT_FAST_SHUTDOWN:
		{
			if (gEnv->pInput != nullptr)
			{
				gEnv->pInput->RemoveConsoleEventListener(this);
			}

			break;
		}
	case ESYSTEM_EVENT_ACTIVATE:
		{
			// When Alt+Tabbing out of the application while it's in full-screen mode
			// ESYSTEM_EVENT_ACTIVATE is sent instead of ESYSTEM_EVENT_CHANGE_FOCUS.
			if (g_cvars.m_ignoreWindowFocus == 0)
			{
				// wparam != 0 is active, wparam == 0 is inactive
				// lparam != 0 is minimized, lparam == 0 is not minimized
				if (wparam == 0 || lparam != 0)
				{
					// lost focus
					ExecuteDefaultTrigger(EDefaultTriggerType::LoseFocus);
				}
				else
				{
					// got focus
					ExecuteDefaultTrigger(EDefaultTriggerType::GetFocus);
				}
			}

			break;
		}
	case ESYSTEM_EVENT_CHANGE_FOCUS:
		{
			if (g_cvars.m_ignoreWindowFocus == 0)
			{
				// wparam != 0 is focused, wparam == 0 is not focused
				if (wparam == 0)
				{
					// lost focus
					ExecuteDefaultTrigger(EDefaultTriggerType::LoseFocus);
				}
				else
				{
					// got focus
					ExecuteDefaultTrigger(EDefaultTriggerType::GetFocus);
				}
			}

			break;
		}
	case ESYSTEM_EVENT_AUDIO_MUTE:
		{
			ExecuteDefaultTrigger(EDefaultTriggerType::MuteAll);

			break;
		}
	case ESYSTEM_EVENT_AUDIO_UNMUTE:
		{
			ExecuteDefaultTrigger(EDefaultTriggerType::UnmuteAll);

			break;
		}
	case ESYSTEM_EVENT_AUDIO_PAUSE:
		{
			ExecuteDefaultTrigger(EDefaultTriggerType::PauseAll);

			break;
		}
	case ESYSTEM_EVENT_AUDIO_RESUME:
		{
			ExecuteDefaultTrigger(EDefaultTriggerType::ResumeAll);

			break;
		}
	case ESYSTEM_EVENT_AUDIO_LANGUAGE_CHANGED:
		{
			OnLanguageChanged();

			break;
		}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	case ESYSTEM_EVENT_AUDIO_REFRESH:
		{
			SSystemRequestData<ESystemRequestType::RefreshSystem> const requestData;
			CRequest const request(&requestData, ERequestFlags::ExecuteBlocking);
			PushRequest(request);

			break;
		}
	case ESYSTEM_EVENT_AUDIO_RELOAD_CONTROLS_DATA:
		{
			SSystemRequestData<ESystemRequestType::ReloadControlsData> const requestData;
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
#endif    // CRY_AUDIO_USE_DEBUG_CODE
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::OnInputEvent(SInputEvent const& event)
{
	if (event.state == eIS_Changed && event.deviceType == eIDT_Gamepad)
	{
		if (event.keyId == eKI_SYS_ConnectDevice)
		{
			g_pIImpl->GamepadConnected(event.deviceUniqueID);
		}
		else if (event.keyId == eKI_SYS_DisconnectDevice)
		{
			g_pIImpl->GamepadDisconnected(event.deviceUniqueID);
		}
	}

	// Do not consume event
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InternalUpdate()
{
	CRY_PROFILE_SECTION(PROFILE_AUDIO, "Audio: Internal Update");

	if (m_lastExternalThreadFrameId != m_externalThreadFrameId)
	{
		if (g_pIImpl != nullptr)
		{
			g_listenerManager.Update(m_accumulatedFrameTime);
			UpdateActiveObjects(m_accumulatedFrameTime);
			g_pIImpl->Update();
		}

		ProcessRequests(m_requestQueue);
		m_lastExternalThreadFrameId = m_externalThreadFrameId;
		m_accumulatedFrameTime = 0.0f;
		m_didThreadWait = false;
	}
	else if (m_didThreadWait)
	{
		// Effectively no time has passed for the external thread as it didn't progress.
		// Consequently 0.0f is passed for deltaTime.
		if (g_pIImpl != nullptr)
		{
			g_listenerManager.Update(0.0f);
			UpdateActiveObjects(0.0f);
			g_pIImpl->Update();
		}

		ProcessRequests(m_requestQueue);
		m_didThreadWait = false;
	}
	else
	{
		// If we're faster than the external thread let's wait to make room for other threads.
		CRY_PROFILE_SECTION_WAITING(PROFILE_AUDIO, "Wait - Audio Update");

		// The external thread will wake the audio thread up effectively syncing it to itself.
		// If not however, the audio thread will execute at a minimum of roughly 30 fps.
		if (m_audioThreadWakeupEvent.Wait(30))
		{
			// Only reset if the event was signaled, not timed-out!
			m_audioThreadWakeupEvent.Reset();
		}

		m_didThreadWait = true;
	}
}

///////////////////////////////////////////////////////////////////////////
bool CSystem::Initialize()
{
	if (!m_isInitialized)
	{
		g_cvars.RegisterVariables();

		if (g_cvars.m_objectPoolSize < 1)
		{
			g_cvars.m_objectPoolSize = 1;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Warning, R"(Audio Object pool size must be at least 1. Forcing the cvar "s_AudioObjectPoolSize" to 1!)");
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}

		CObject::CreateAllocator(static_cast<uint16>(g_cvars.m_objectPoolSize));

#if defined(CRY_AUDIO_USE_OCCLUSION)
		SOcclusionInfo::CreateAllocator(static_cast<uint16>(g_cvars.m_objectPoolSize));
#endif    // CRY_AUDIO_USE_OCCLUSION

		if (g_cvars.m_triggerInstancePoolSize < 1)
		{
			g_cvars.m_triggerInstancePoolSize = 1;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Warning, R"(Trigger instance pool size must be at least 1. Forcing the cvar "s_TriggerInstancePoolSize" to 1!)");
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}

		CTriggerInstance::CreateAllocator(static_cast<uint16>(g_cvars.m_triggerInstancePoolSize));

#if defined(CRY_AUDIO_USE_OCCLUSION)
		// Add the callback for the obstruction calculation.
		gEnv->pPhysicalWorld->AddEventClient(
			EventPhysRWIResult::id,
			&CPropagationProcessor::OnObstructionTest,
			1);

		CPropagationProcessor::UpdateOcclusionRayFlags();
		CPropagationProcessor::UpdateOcclusionPlanes();
#endif    // CRY_AUDIO_USE_OCCLUSION

		m_objectPoolSize = static_cast<uint16>(g_cvars.m_objectPoolSize);

		g_activeObjects.reserve(static_cast<size_t>(m_objectPoolSize));

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		g_constructedObjects.reserve(static_cast<size_t>(m_objectPoolSize));

		g_contextInfo.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(g_szGlobalContextName),
			std::forward_as_tuple(SContextInfo(GlobalContextId, true, true)));
#endif // CRY_AUDIO_USE_DEBUG_CODE

		g_listenerManager.Initialize();
		g_fileCacheManager.Initialize();

		CRY_ASSERT_MESSAGE(!m_mainThread.IsActive(), "AudioSystem thread active before initialization during %s", __FUNCTION__);
		m_mainThread.Activate();
		AddRequestListener(&CSystem::OnCallback, nullptr, ESystemEvents::TriggerExecuted | ESystemEvents::TriggerFinished);
		m_isInitialized = true;
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "AudioSystem has already been initialized during %s", __FUNCTION__);
	}

	return m_isInitialized;
}

///////////////////////////////////////////////////////////////////////////
void CSystem::Release()
{
	if (m_isInitialized)
	{
		RemoveRequestListener(&CSystem::OnCallback, nullptr);

		SSystemRequestData<ESystemRequestType::ReleaseImpl> const requestData;
		CRequest const request(&requestData, ERequestFlags::ExecuteBlocking);
		PushRequest(request);

		m_mainThread.Deactivate();

#if defined(CRY_AUDIO_USE_OCCLUSION)
		if (gEnv->pPhysicalWorld != nullptr)
		{
			// remove the callback for the obstruction calculation
			gEnv->pPhysicalWorld->RemoveEventClient(
				EventPhysRWIResult::id,
				&CPropagationProcessor::OnObstructionTest,
				1);
		}
#endif    // CRY_AUDIO_USE_OCCLUSION

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		for (auto const pObject : g_constructedObjects)
		{
			CRY_ASSERT_MESSAGE(pObject->GetImplData() == nullptr, "An object cannot have valid impl data during %s", __FUNCTION__);
			delete pObject;
		}

		g_constructedObjects.clear();
#endif // CRY_AUDIO_USE_DEBUG_CODE

		g_listenerManager.Terminate();
		g_cvars.UnregisterVariables();

		CObject::FreeMemoryPool();
		CTriggerInstance::FreeMemoryPool();

#if defined(CRY_AUDIO_USE_OCCLUSION)
		SOcclusionInfo::FreeMemoryPool();
#endif    // CRY_AUDIO_USE_OCCLUSION

		m_isInitialized = false;
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "AudioSystem has already been released or was never properly initialized during %s", __FUNCTION__);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetImpl(Impl::IImpl* const pIImpl, SRequestUserData const& userData /*= SAudioRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::SetImpl> const requestData(pIImpl);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecuteTriggerEx(SExecuteTriggerData const& triggerData, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::ExecuteTriggerEx> const requestData(triggerData);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecuteDefaultTrigger(EDefaultTriggerType const type, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const requestData(type);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::SetParameter> const requestData(parameterId, value);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetParameterGlobally(ControlId const parameterId, float const value, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::SetParameterGlobally> const requestData(parameterId, value);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::SetSwitchState> const requestData(switchId, switchStateId);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetSwitchStateGlobally(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::SetSwitchStateGlobally> const requestData(switchId, switchStateId);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::ExecuteTrigger> const requestData(triggerId);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecuteTriggerWithCallbacks(STriggerCallbackData const& callbackData, SRequestUserData const& userData /* = SRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::ExecuteTriggerWithCallbacks> const requestData(callbackData);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::StopTrigger(ControlId const triggerId /* = CryAudio::InvalidControlId */, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	if (triggerId != InvalidControlId)
	{
		SSystemRequestData<ESystemRequestType::StopTrigger> const requestData(triggerId);
		CRequest const request(&requestData, userData);
		PushRequest(request);
	}
	else
	{
		SSystemRequestData<ESystemRequestType::StopAllTriggers> const requestData;
		CRequest const request(&requestData, userData);
		PushRequest(request);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecutePreviewTrigger(ControlId const triggerId)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	switch (triggerId)
	{
	case g_loseFocusTriggerId:
		{
			SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const requestData(EDefaultTriggerType::LoseFocus);
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
	case g_getFocusTriggerId:
		{
			SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const requestData(EDefaultTriggerType::GetFocus);
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
	case g_muteAllTriggerId:
		{
			SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const requestData(EDefaultTriggerType::MuteAll);
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
	case g_unmuteAllTriggerId:
		{
			SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const requestData(EDefaultTriggerType::UnmuteAll);
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
	case g_pauseAllTriggerId:
		{
			SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const requestData(EDefaultTriggerType::PauseAll);
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
	case g_resumeAllTriggerId:
		{
			SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const requestData(EDefaultTriggerType::ResumeAll);
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
	default:
		{
			SSystemRequestData<ESystemRequestType::ExecutePreviewTrigger> const requestData(triggerId);
			CRequest const request(&requestData);
			PushRequest(request);

			break;
		}
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecutePreviewTriggerEx(Impl::ITriggerInfo const& triggerInfo)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerEx> const requestData(triggerInfo);
	CRequest const request(&requestData);
	PushRequest(request);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecutePreviewTriggerEx(XmlNodeRef const& node)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerExNode> const requestData(node);
	CRequest const request(&requestData);
	PushRequest(request);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::StopPreviewTrigger()
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	SSystemRequestData<ESystemRequestType::StopPreviewTrigger> const requestData;
	CRequest const request(&requestData);
	PushRequest(request);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RefreshObject(Impl::IObject* const pIObject)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	SSystemRequestData<ESystemRequestType::RefreshObject> const requestData(pIObject);
	CRequest const request(&requestData);
	PushRequest(request);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CSystem::ResetRequestCount()
{
	SSystemRequestData<ESystemRequestType::ResetRequestCount> const requestData;
	CRequest const request(&requestData);
	PushRequest(request);
}
#endif  // CRY_AUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportStartedTriggerConnectionInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SCallbackRequestData<ECallbackRequestType::ReportStartedTriggerConnectionInstance> const requestData(triggerInstanceId, result);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportFinishedTriggerConnectionInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerConnectionInstance> const requestData(triggerInstanceId, result);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportTriggerConnectionInstanceCallback(TriggerInstanceId const triggerInstanceId, ESystemEvents const systemEvent, SRequestUserData const& userData /* = SRequestUserData::GetEmptyObject() */)
{
	SCallbackRequestData<ECallbackRequestType::ReportTriggerConnectionInstanceCallback> const requestData(triggerInstanceId, systemEvent);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportPhysicalizedObject(Impl::IObject* const pIObject, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SCallbackRequestData<ECallbackRequestType::ReportPhysicalizedObject> const requestData(pIObject);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportVirtualizedObject(Impl::IObject* const pIObject, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SCallbackRequestData<ECallbackRequestType::ReportVirtualizedObject> const requestData(pIObject);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::StopAllSounds(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::StopAllSounds> const requestData;
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ParseControlsData(
	char const* const szFolderPath,
	char const* const szContextName,
	ContextId const contextId,
	SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::ParseControlsData> const requestData(szFolderPath, szContextName, contextId);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ParsePreloadsData(
	char const* const szFolderPath,
	ContextId const contextId,
	SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::ParsePreloadsData> const requestData(szFolderPath, contextId);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::PreloadSingleRequest(PreloadRequestId const id, bool const bAutoLoadOnly, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::PreloadSingleRequest> const requestData(id, bAutoLoadOnly);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UnloadSingleRequest(PreloadRequestId const id, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::UnloadSingleRequest> const requestData(id);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ActivateContext(ContextId const contextId)
{
	if ((contextId != InvalidContextId) && (contextId != GlobalContextId))
	{
		SSystemRequestData<ESystemRequestType::ActivateContext> const requestData(contextId);
		CRequest const request(&requestData);
		PushRequest(request);
	}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	else
	{
		if (contextId == InvalidContextId)
		{
			Cry::Audio::Log(ELogType::Warning, "Invalid context id passed in %s", __FUNCTION__);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "The global context cannot get activated manually.");
		}
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::DeactivateContext(ContextId const contextId)
{
	if ((contextId != InvalidContextId) && (contextId != GlobalContextId))
	{
		SSystemRequestData<ESystemRequestType::DeactivateContext> const requestData(contextId);
		CRequest const request(&requestData);
		PushRequest(request);
	}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	else
	{
		if (contextId == InvalidContextId)
		{
			Cry::Audio::Log(ELogType::Warning, "Invalid context id passed in %s", __FUNCTION__);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "The global context cannot get deactivated manually.");
		}
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::AutoLoadSetting(ContextId const contextId, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::AutoLoadSetting> const requestData(contextId);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::LoadSetting(ControlId const id, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::LoadSetting> const requestData(id);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UnloadSetting(ControlId const id, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SSystemRequestData<ESystemRequestType::UnloadSetting> const requestData(id);
	CRequest const request(&requestData, userData);
	PushRequest(request);
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CSystem::RetriggerControls(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SSystemRequestData<ESystemRequestType::RetriggerControls> const requestData;
	CRequest const request(&requestData, userData);
	PushRequest(request);
}
#endif  // CRY_AUDIO_USE_DEBUG_CODE

///////////////////////////////////////////////////////////////////////////
char const* CSystem::GetConfigPath() const
{
	return g_configPath.c_str();
}

///////////////////////////////////////////////////////////////////////////
CryAudio::IListener* CSystem::CreateListener(CTransformation const& transformation, char const* const szName)
{
	CListener* pListener = nullptr;
	SSystemRequestData<ESystemRequestType::RegisterListener> const requestData(&pListener, transformation, szName);
	CRequest const request(&requestData, ERequestFlags::ExecuteBlocking);
	PushRequest(request);

	return static_cast<CryAudio::IListener*>(pListener);
}

///////////////////////////////////////////////////////////////////////////
void CSystem::ReleaseListener(CryAudio::IListener* const pIListener)
{
	if (static_cast<CListener*>(pIListener)->IsUserCreated())
	{
		SSystemRequestData<ESystemRequestType::ReleaseListener> const requestData(static_cast<CListener*>(pIListener));
		CRequest const request(&requestData);
		PushRequest(request);
	}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Error, "Non-user-created listener cannot get released during %s", __FUNCTION__);
	}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
IListener* CSystem::GetListener(ListenerId const id /*= DefaultListenerId*/)
{
	CListener* pListener = &g_defaultListener;

	if ((id != DefaultListenerId) && (id != InvalidListenerId))
	{
		SSystemRequestData<ESystemRequestType::GetListener> const requestData(&pListener, id);
		CRequest const request(&requestData, ERequestFlags::ExecuteBlocking);
		PushRequest(request);
	}

	return static_cast<CryAudio::IListener*>(pListener);
}

//////////////////////////////////////////////////////////////////////////
CryAudio::IObject* CSystem::CreateObject(SCreateObjectData const& objectData /*= SCreateObjectData::GetEmptyObject()*/)
{
	CObject* pObject = nullptr;
	SSystemRequestData<ESystemRequestType::RegisterObject> const requestData(&pObject, objectData);
	CRequest const request(&requestData, ERequestFlags::ExecuteBlocking);
	PushRequest(request);

	return static_cast<CryAudio::IObject*>(pObject);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReleaseObject(CryAudio::IObject* const pIObject)
{
	CRY_ASSERT_MESSAGE(pIObject != nullptr, "pIObject is nullptr during %s", __FUNCTION__);
	SSystemRequestData<ESystemRequestType::ReleaseObject> const requestData(static_cast<CObject*>(pIObject));
	CRequest const request(&requestData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetTriggerData(ControlId const triggerId, STriggerData& triggerData)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, triggerId, nullptr);

	if (pTrigger != nullptr)
	{
		triggerData.radius = pTrigger->GetRadius();
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReleaseImpl()
{
	// Reject new requests during shutdown.
	g_systemStates |= ESystemStates::ImplShuttingDown;

	g_pIImpl->OnBeforeRelease();

	// Release middleware specific data before its shutdown.
	g_listenerManager.ReleaseImplData();

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	// Don't delete objects here as we need them to survive a middleware switch!
	for (auto const pObject : g_constructedObjects)
	{
		g_pIImpl->DestructObject(pObject->GetImplData());
		pObject->Release();
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	g_pIImpl->DestructObject(g_pIObject);
	ReleaseGlobalData();

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	g_pIImpl->DestructObject(g_previewObject.GetImplData());
	g_previewObject.Release();
	ResetRequestCount();
#endif // CRY_AUDIO_USE_DEBUG_CODE

	g_xmlProcessor.ClearPreloadsData(GlobalContextId, true);
	g_xmlProcessor.ClearControlsData(GlobalContextId, true);

	ReportContextDeactivated(GlobalContextId);

	g_pIImpl->ShutDown();
	g_pIImpl->Release();
	g_pIImpl = nullptr;

	FreeMemoryPools();

	g_systemStates &= ~ESystemStates::ImplShuttingDown;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnLanguageChanged()
{
	SSystemRequestData<ESystemRequestType::ChangeLanguage> const requestData;
	CRequest const request(&requestData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Log(ELogType const type, char const* const szFormat, ...)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (szFormat != nullptr && szFormat[0] != '\0' && gEnv->pLog->GetVerbosityLevel() != -1)
	{
		CRY_PROFILE_SECTION(PROFILE_AUDIO, "CSystem::Log");

		char buffer[256];
		va_list ArgList;
		va_start(ArgList, szFormat);
		cry_vsprintf(buffer, szFormat, ArgList);
		va_end(ArgList);

		auto const loggingOptions = static_cast<ELoggingOptions>(g_cvars.m_loggingOptions);

		switch (type)
		{
		case ELogType::Warning:
			{
				if ((loggingOptions& ELoggingOptions::Warnings) != ELoggingOptions::None)
				{
					gEnv->pSystem->Warning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, nullptr, "<Audio>: %s", buffer);
				}

				break;
			}
		case ELogType::Error:
			{
				if ((loggingOptions& ELoggingOptions::Errors) != ELoggingOptions::None)
				{
					gEnv->pSystem->Warning(VALIDATOR_MODULE_AUDIO, VALIDATOR_ERROR, VALIDATOR_FLAG_AUDIO, nullptr, "<Audio>: %s", buffer);
				}

				break;
			}
		case ELogType::Comment:
			{
				if ((gEnv->pLog != nullptr) && (gEnv->pLog->GetVerbosityLevel() >= 4) && ((loggingOptions& ELoggingOptions::Comments) != ELoggingOptions::None))
				{
					CryLogAlways("<Audio>: %s", buffer);
				}

				break;
			}
		case ELogType::Always:
			{
				CryLogAlways("<Audio>: %s", buffer);

				break;
			}
		default:
			{
				CRY_ASSERT(false);

				break;
			}
		}
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ProcessRequests(Requests& requestQueue)
{
	CRequest request;

	while (requestQueue.dequeue(request))
	{
		if (request.status == ERequestStatus::None)
		{
			request.status = ERequestStatus::Pending;
			ProcessRequest(request);
		}
		else
		{
			// TODO: handle pending requests!
		}

		if (request.status != ERequestStatus::Pending)
		{
			if ((request.flags & ERequestFlags::CallbackOnAudioThread) != ERequestFlags::None)
			{
				NotifyListener(request);

				if ((request.flags & ERequestFlags::ExecuteBlocking) != ERequestFlags::None)
				{
					m_mainEvent.Set();
				}
			}
			else if ((request.flags & ERequestFlags::CallbackOnExternalOrCallingThread) != ERequestFlags::None)
			{
				if ((request.flags & ERequestFlags::ExecuteBlocking) != ERequestFlags::None)
				{
					m_syncRequest = request;
					m_mainEvent.Set();
				}
				else
				{
					if (request.GetData()->requestType == ERequestType::ObjectRequest)
					{
						auto const pBase = static_cast<SObjectRequestDataBase const*>(request.GetData());
						pBase->pObject->IncrementSyncCallbackCounter();
						// No sync callback counting for default object, because it gets released on unloading of the audio system dll.
					}

					m_syncCallbacks.enqueue(request);
				}
			}
			else if ((request.flags & ERequestFlags::ExecuteBlocking) != ERequestFlags::None)
			{
				m_mainEvent.Set();
			}
		}
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	SetRequestCountPeak();
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ProcessRequest(CRequest& request)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CountRequestPerUpdate(request);
#endif // CRY_AUDIO_USE_DEBUG_CODE

	ERequestStatus result = ERequestStatus::None;

	if (request.GetData())
	{
		switch (request.GetData()->requestType)
		{
		case ERequestType::ObjectRequest:
			{
				result = ProcessObjectRequest(request);

				break;
			}
		case ERequestType::ListenerRequest:
			{
				result = ProcessListenerRequest(request.GetData());

				break;
			}
		case ERequestType::CallbackRequest:
			{
				result = ProcessCallbackRequest(request);

				break;
			}
		case ERequestType::SystemRequest:
			{
				result = ProcessSystemRequest(request);

				break;
			}
		default:
			{
				CRY_ASSERT_MESSAGE(false, "Unknown request type: %u during %s", request.GetData()->requestType, __FUNCTION__);

				break;
			}
		}
	}

	request.status = result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CSystem::ProcessSystemRequest(CRequest const& request)
{
	ERequestStatus result = ERequestStatus::Failure;
	auto const pBase = static_cast<SSystemRequestDataBase const*>(request.GetData());

	switch (pBase->systemRequestType)
	{
	case ESystemRequestType::AddRequestListener:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::AddRequestListener> const*>(request.GetData());
			g_eventListenerManager.AddRequestListener(pRequestData);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::RemoveRequestListener:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::RemoveRequestListener> const*>(request.GetData());
			g_eventListenerManager.RemoveRequestListener(pRequestData->func, pRequestData->pObjectToListenTo);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::SetImpl:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::SetImpl> const*>(request.GetData());
			result = HandleSetImpl(pRequestData->pIImpl) ? ERequestStatus::Success : ERequestStatus::Failure;

			break;
		}
	case ESystemRequestType::ExecuteTrigger:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecuteTrigger> const* const>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				pTrigger->Execute(request.pOwner, request.pUserData, request.pUserDataOwner, request.flags);
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::ExecuteTriggerWithCallbacks:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecuteTriggerWithCallbacks> const* const>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->data.triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				pTrigger->ExecuteWithCallbacks(pRequestData->data, request.pOwner, request.pUserData, request.pUserDataOwner, request.flags);
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::StopTrigger:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::StopTrigger> const* const>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				pTrigger->Stop(g_pIObject);
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::StopAllTriggers:
		{
			g_pIObject->StopAllTriggers();
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ExecuteTriggerEx:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecuteTriggerEx> const*>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				Listeners listeners;

				for (auto const id : pRequestData->listenerIds)
				{
					listeners.push_back(g_listenerManager.GetListener(id));
				}

				Impl::IListeners implListeners;

				for (auto const pListener : listeners)
				{
					implListeners.push_back(pListener->GetImplData());
				}

				MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CObject");

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				auto const pNewObject = new CObject(pRequestData->transformation, pRequestData->name.c_str());
				pNewObject->Init(g_pIImpl->ConstructObject(pRequestData->transformation, implListeners, pNewObject->GetName()), listeners);
				g_constructedObjects.push_back(pNewObject);
#else
				auto const pNewObject = new CObject(pRequestData->transformation);
				pNewObject->Init(g_pIImpl->ConstructObject(pRequestData->transformation, implListeners), listeners);
#endif      // CRY_AUDIO_USE_DEBUG_CODE

				if (pRequestData->setCurrentEnvironments)
				{
					SetCurrentEnvironmentsOnObject(pNewObject, INVALID_ENTITYID);
				}

#if defined(CRY_AUDIO_USE_OCCLUSION)
				SetOcclusionType(*pNewObject, pRequestData->occlusionType);
#endif    // CRY_AUDIO_USE_OCCLUSION
				pTrigger->Execute(*pNewObject, request.pOwner, request.pUserData, request.pUserDataOwner, request.flags, pRequestData->entityId);
				pNewObject->RemoveFlag(EObjectFlags::InUse);

				if ((pNewObject->GetFlags() & EObjectFlags::Active) == EObjectFlags::None)
				{
					pNewObject->Destruct();
				}

				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::ExecuteDefaultTrigger:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const*>(request.GetData());

			switch (pRequestData->triggerType)
			{
			case EDefaultTriggerType::LoseFocus:
				{
					if ((g_systemStates& ESystemStates::IsMuted) == ESystemStates::None)
					{
						g_loseFocusTrigger.Execute();
					}

					result = ERequestStatus::Success;

					break;
				}
			case EDefaultTriggerType::GetFocus:
				{
					if ((g_systemStates& ESystemStates::IsMuted) == ESystemStates::None)
					{
						g_getFocusTrigger.Execute();
					}

					result = ERequestStatus::Success;

					break;
				}
			case EDefaultTriggerType::MuteAll:
				{
					g_muteAllTrigger.Execute();
					result = ERequestStatus::Success;
					g_systemStates |= ESystemStates::IsMuted;

					break;
				}
			case EDefaultTriggerType::UnmuteAll:
				{
					g_unmuteAllTrigger.Execute();
					result = ERequestStatus::Success;
					g_systemStates &= ~ESystemStates::IsMuted;

					break;
				}
			case EDefaultTriggerType::PauseAll:
				{
					g_pauseAllTrigger.Execute();
					result = ERequestStatus::Success;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					g_systemStates |= ESystemStates::IsPaused;
#endif      // CRY_AUDIO_USE_DEBUG_CODE

					break;
				}
			case EDefaultTriggerType::ResumeAll:
				{
					g_resumeAllTrigger.Execute();
					result = ERequestStatus::Success;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					g_systemStates &= ~ESystemStates::IsPaused;
#endif      // CRY_AUDIO_USE_DEBUG_CODE

					break;
				}
			default:
				{
					break;
				}
			}

			break;
		}
	case ESystemRequestType::StopAllSounds:
		{
			g_pIImpl->StopAllSounds();
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ParseControlsData:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ParseControlsData> const*>(request.GetData());
			g_xmlProcessor.ParseControlsData(pRequestData->folderPath.c_str(), pRequestData->contextId, pRequestData->contextName.c_str());

			if (pRequestData->contextId != GlobalContextId)
			{
				ReportContextActivated(pRequestData->contextId);
			}

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ParsePreloadsData:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ParsePreloadsData> const*>(request.GetData());
			g_xmlProcessor.ParsePreloadsData(pRequestData->folderPath.c_str(), pRequestData->contextId);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager | EDebugUpdateFilter::Contexts);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ClearControlsData:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ClearControlsData> const*>(request.GetData());
			g_xmlProcessor.ClearControlsData(pRequestData->contextId, false);

			ContextId const id = pRequestData->contextId;

			if ((id != GlobalContextId) && (id != InvalidContextId))
			{
				ReportContextDeactivated(id);
			}

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ClearPreloadsData:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ClearPreloadsData> const*>(request.GetData());
			g_xmlProcessor.ClearPreloadsData(pRequestData->contextId, false);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager | EDebugUpdateFilter::Contexts);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::PreloadSingleRequest:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::PreloadSingleRequest> const*>(request.GetData());
			result = g_fileCacheManager.TryLoadRequest(
				pRequestData->preloadRequestId,
				((request.flags & ERequestFlags::ExecuteBlocking) != ERequestFlags::None),
				pRequestData->bAutoLoadOnly,
				request.flags,
				request.pOwner,
				request.pUserData,
				request.pUserDataOwner);

			break;
		}
	case ESystemRequestType::UnloadSingleRequest:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::UnloadSingleRequest> const*>(request.GetData());
			result = g_fileCacheManager.TryUnloadRequest(pRequestData->preloadRequestId);

			break;
		}
	case ESystemRequestType::ActivateContext:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ActivateContext> const*>(request.GetData());

			HandleActivateContext(pRequestData->contextId);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::DeactivateContext:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::DeactivateContext> const*>(request.GetData());

			HandleDeactivateContext(pRequestData->contextId);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::SetParameter:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::SetParameter> const*>(request.GetData());

			CParameter const* const pParameter = stl::find_in_map(g_parameterLookup, pRequestData->parameterId, nullptr);

			if (pParameter != nullptr)
			{
				pParameter->Set(pRequestData->value);
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::SetParameterGlobally:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::SetParameterGlobally> const*>(request.GetData());

			CParameter const* const pParameter = stl::find_in_map(g_parameterLookup, pRequestData->parameterId, nullptr);

			if (pParameter != nullptr)
			{
				pParameter->SetGlobally(pRequestData->value);
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::SetSwitchState:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::SetSwitchState> const*>(request.GetData());

			CSwitch const* const pSwitch = stl::find_in_map(g_switchLookup, pRequestData->switchId, nullptr);

			if (pSwitch != nullptr)
			{
				CSwitchState const* const pState = stl::find_in_map(pSwitch->GetStates(), pRequestData->switchStateId, nullptr);

				if (pState != nullptr)
				{
					pState->Set();
					result = ERequestStatus::Success;
				}
			}

			break;
		}
	case ESystemRequestType::SetSwitchStateGlobally:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::SetSwitchStateGlobally> const*>(request.GetData());

			CSwitch const* const pSwitch = stl::find_in_map(g_switchLookup, pRequestData->switchId, nullptr);

			if (pSwitch != nullptr)
			{
				CSwitchState const* const pState = stl::find_in_map(pSwitch->GetStates(), pRequestData->switchStateId, nullptr);

				if (pState != nullptr)
				{
					pState->SetGlobally();
					result = ERequestStatus::Success;
				}
			}

			break;
		}
	case ESystemRequestType::AutoLoadSetting:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::AutoLoadSetting> const*>(request.GetData());

			for (auto const& settingPair : g_settingLookup)
			{
				CSetting const* const pSetting = settingPair.second;

				if (pSetting->IsAutoLoad() && (pSetting->GetContextId() == pRequestData->contextId))
				{
					pSetting->Load();
					break;
				}
			}

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::LoadSetting:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::LoadSetting> const*>(request.GetData());

			CSetting const* const pSetting = stl::find_in_map(g_settingLookup, pRequestData->id, nullptr);

			if (pSetting != nullptr)
			{
				pSetting->Load();
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::UnloadSetting:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::UnloadSetting> const*>(request.GetData());

			CSetting const* const pSetting = stl::find_in_map(g_settingLookup, pRequestData->id, nullptr);

			if (pSetting != nullptr)
			{
				pSetting->Unload();
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::UnloadAFCMDataByContext:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::UnloadAFCMDataByContext> const*>(request.GetData());
			g_fileCacheManager.UnloadDataByContext(pRequestData->contextId);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ReleaseImpl:
		{
			ReleaseImpl();
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ChangeLanguage:
		{
			SetImplLanguage();

			g_fileCacheManager.UpdateLocalizedFileCacheEntries();

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			result = ERequestStatus::Success;

			break;
		}
#if defined(CRY_AUDIO_USE_OCCLUSION)
	case ESystemRequestType::ReleasePendingRays:
		{
			for (auto const pObject : g_activeObjects)
			{
				pObject->ReleasePendingRays();
			}

			result = ERequestStatus::Success;

			break;
		}
#endif // CRY_AUDIO_USE_OCCLUSION
	case ESystemRequestType::GetImplInfo:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::GetImplInfo> const*>(request.GetData());
			g_pIImpl->GetInfo(pRequestData->implInfo);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::RegisterListener:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::RegisterListener> const*>(request.GetData());
			*pRequestData->ppListener = g_listenerManager.CreateListener(pRequestData->transformation, pRequestData->name.c_str(), true);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ReleaseListener:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ReleaseListener> const*>(request.GetData());

			CRY_ASSERT(pRequestData->pListener != nullptr);

			if (pRequestData->pListener != nullptr)
			{
				g_listenerManager.ReleaseListener(pRequestData->pListener);
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::GetListener:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::GetListener> const*>(request.GetData());
			*pRequestData->ppListener = g_listenerManager.GetListener(pRequestData->id);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::RegisterObject:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::RegisterObject> const*>(request.GetData());

			Listeners listeners;

			for (auto const id : pRequestData->listenerIds)
			{
				listeners.push_back(g_listenerManager.GetListener(id));
			}

			Impl::IListeners implListeners;
			implListeners.reserve(listeners.size());

			for (auto const pListener : listeners)
			{
				implListeners.push_back(pListener->GetImplData());
			}

			MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CObject");

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			auto const pNewObject = new CObject(pRequestData->transformation, pRequestData->name.c_str());
			pNewObject->Init(g_pIImpl->ConstructObject(pRequestData->transformation, implListeners, pNewObject->GetName()), listeners);
			g_constructedObjects.push_back(pNewObject);
#else
			auto const pNewObject = new CObject(pRequestData->transformation);
			pNewObject->Init(g_pIImpl->ConstructObject(pRequestData->transformation, implListeners), listeners);
#endif    // CRY_AUDIO_USE_DEBUG_CODE

			if (pRequestData->setCurrentEnvironments)
			{
				SetCurrentEnvironmentsOnObject(pNewObject, INVALID_ENTITYID);
			}

#if defined(CRY_AUDIO_USE_OCCLUSION)
			SetOcclusionType(*pNewObject, pRequestData->occlusionType);
#endif  // CRY_AUDIO_USE_OCCLUSION
			*pRequestData->ppObject = pNewObject;
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ReleaseObject:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ReleaseObject> const*>(request.GetData());

			pRequestData->pObject->RemoveFlag(EObjectFlags::InUse);

			if ((pRequestData->pObject->GetFlags() & EObjectFlags::Active) == EObjectFlags::None)
			{
				pRequestData->pObject->Destruct();
			}

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::None:
		{
			result = ERequestStatus::Success;

			break;
		}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	case ESystemRequestType::RefreshSystem:
		{
			HandleRefresh();
			HandleRetriggerControls();
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ExecutePreviewTrigger:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecutePreviewTrigger> const*>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				pTrigger->Execute(g_previewObject, request.pOwner, request.pUserData, request.pUserDataOwner, request.flags);
				result = ERequestStatus::Success;
			}

			break;
		}
	case ESystemRequestType::ExecutePreviewTriggerEx:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerEx> const*>(request.GetData());

			g_previewTrigger.Execute(pRequestData->triggerInfo);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ExecutePreviewTriggerExNode:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerExNode> const*>(request.GetData());

			g_previewTrigger.Execute(pRequestData->node);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::StopPreviewTrigger:
		{
			g_previewObject.StopAllTriggers();
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::RefreshObject:
		{
			auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::RefreshObject> const*>(request.GetData());

			for (auto const pObject : g_constructedObjects)
			{
				if (pObject->GetImplData() == pRequestData->pIObject)
				{
					pObject->ForceImplementationRefresh();
					break;
				}
			}

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ResetRequestCount:
		{
			ZeroStruct(g_requestsPerUpdate);
			ZeroStruct(g_requestPeaks);
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::RetriggerControls:
		{
			HandleRetriggerControls();
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::ReloadControlsData:
		{
			for (auto const pObject : g_activeObjects)
			{
				pObject->StopAllTriggers();
			}

			g_pIObject->StopAllTriggers();
			g_previewObject.StopAllTriggers();

			// Store active contexts for reloading after they have been unloaded which empties the map.
			ContextInfo const tempContextInfo = g_contextInfo;

			g_xmlProcessor.ClearControlsData(GlobalContextId, true);
			ReportContextDeactivated(GlobalContextId);

			g_xmlProcessor.ParseSystemData();
			g_xmlProcessor.ParseControlsData(g_configPath.c_str(), GlobalContextId, g_szGlobalContextName);

			for (auto const& contextPair : tempContextInfo)
			{
				ContextId const contextId = contextPair.second.contextId;

				if (contextPair.second.isRegistered && contextPair.second.isActive && (contextId != GlobalContextId))
				{
					char const* const szContextName = contextPair.first.c_str();

					CryFixedStringT<MaxFilePathLength> contextPath = g_configPath;
					contextPath += g_szContextsFolderName;
					contextPath += "/";
					contextPath += szContextName;

					g_xmlProcessor.ParseControlsData(contextPath.c_str(), contextId, szContextName);

					ReportContextActivated(contextId);
				}
			}

			HandleRetriggerControls();

			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::DrawDebugInfo:
		{
			HandleDrawDebug();
			m_canDraw = true;
			result = ERequestStatus::Success;

			break;
		}
	case ESystemRequestType::UpdateDebugInfo:
		{
			HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager | EDebugUpdateFilter::Contexts);
			result = ERequestStatus::Success;

			break;
		}
#endif  // CRY_AUDIO_USE_DEBUG_CODE
	default:
		{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Warning, "Unknown manager request type: %u", pBase->systemRequestType);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			break;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CSystem::ProcessCallbackRequest(CRequest& request)
{
	ERequestStatus result = ERequestStatus::Failure;
	auto const pBase = static_cast<SCallbackRequestDataBase const*>(request.GetData());

	switch (pBase->callbackRequestType)
	{
	case ECallbackRequestType::ReportStartedTriggerConnectionInstance:
		{
			auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportStartedTriggerConnectionInstance> const*>(request.GetData());

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			bool objectFound = false;
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			CObject* const pObject = stl::find_in_map(g_triggerInstanceIdLookup, pRequestData->triggerInstanceId, nullptr);

			if (pObject != nullptr)
			{
				pObject->ReportStartedTriggerInstance(pRequestData->triggerInstanceId, pRequestData->result);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				objectFound = true;
#endif    // CRY_AUDIO_USE_DEBUG_CODE
			}
			else if (std::find(g_triggerInstanceIds.begin(), g_triggerInstanceIds.end(), pRequestData->triggerInstanceId) != g_triggerInstanceIds.end())
			{
				ReportStartedGlobalTriggerInstance(pRequestData->triggerInstanceId, pRequestData->result);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				objectFound = true;
#endif      // CRY_AUDIO_USE_DEBUG_CODE
			}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			CRY_ASSERT_MESSAGE(objectFound, "TriggerInstanceId %u is not mapped to an object during %s", pRequestData->triggerInstanceId, __FUNCTION__);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			result = ERequestStatus::Success;

			break;
		}
	case ECallbackRequestType::ReportFinishedTriggerConnectionInstance:
		{
			auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerConnectionInstance> const*>(request.GetData());

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			bool objectFound = false;
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			CObject* const pObject = stl::find_in_map(g_triggerInstanceIdLookup, pRequestData->triggerInstanceId, nullptr);

			if (pObject != nullptr)
			{
				pObject->ReportFinishedTriggerInstance(pRequestData->triggerInstanceId, pRequestData->result);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				objectFound = true;
#endif    // CRY_AUDIO_USE_DEBUG_CODE

			}
			else if (std::find(g_triggerInstanceIds.begin(), g_triggerInstanceIds.end(), pRequestData->triggerInstanceId) != g_triggerInstanceIds.end())
			{
				ReportFinishedTriggerInstance(pRequestData->triggerInstanceId, pRequestData->result);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				objectFound = true;
#endif      // CRY_AUDIO_USE_DEBUG_CODE
			}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			CRY_ASSERT_MESSAGE(objectFound, "TriggerInstanceId %u is not mapped to an object during %s", pRequestData->triggerInstanceId, __FUNCTION__);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			result = ERequestStatus::Success;

			break;
		}
	case ECallbackRequestType::ReportTriggerConnectionInstanceCallback:
		{
			auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportTriggerConnectionInstanceCallback> const*>(request.GetData());

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			bool objectFound = false;
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			CObject* const pObject = stl::find_in_map(g_triggerInstanceIdLookup, pRequestData->triggerInstanceId, nullptr);

			if (pObject != nullptr)
			{
				pObject->ReportTriggerInstanceCallback(pRequestData->triggerInstanceId, pRequestData->events);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				objectFound = true;
#endif    // CRY_AUDIO_USE_DEBUG_CODE

			}
			else if (std::find(g_triggerInstanceIds.begin(), g_triggerInstanceIds.end(), pRequestData->triggerInstanceId) != g_triggerInstanceIds.end())
			{
				ReportTriggerInstanceCallback(pRequestData->triggerInstanceId, pRequestData->events);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				objectFound = true;
#endif      // CRY_AUDIO_USE_DEBUG_CODE
			}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			CRY_ASSERT_MESSAGE(objectFound, "TriggerInstanceId %u is not mapped to an object during %s", pRequestData->triggerInstanceId, __FUNCTION__);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			result = ERequestStatus::Success;

			break;
		}
	case ECallbackRequestType::ReportPhysicalizedObject:
		{
			auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportPhysicalizedObject> const*>(request.GetData());

			for (auto const pObject : g_activeObjects)
			{
				if (pObject->GetImplData() == pRequestData->pIObject)
				{
					pObject->RemoveFlag(EObjectFlags::Virtual);

#if defined(CRY_AUDIO_USE_DEBUG_CODE) && defined(CRY_AUDIO_USE_OCCLUSION)
					pObject->ResetObstructionRays();
#endif      // CRY_AUDIO_USE_DEBUG_CODE && CRY_AUDIO_USE_OCCLUSION

					break;
				}
			}

			result = ERequestStatus::Success;

			break;
		}
	case ECallbackRequestType::ReportVirtualizedObject:
		{
			auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportVirtualizedObject> const*>(request.GetData());

			for (auto const pObject : g_activeObjects)
			{
				if (pObject->GetImplData() == pRequestData->pIObject)
				{
					pObject->SetFlag(EObjectFlags::Virtual);
					break;
				}
			}

			result = ERequestStatus::Success;

			break;
		}
	case ECallbackRequestType::ReportFinishedTriggerInstance: // Intentional fall-through.
	case ECallbackRequestType::ReportContextActivated:        // Intentional fall-through.
	case ECallbackRequestType::ReportContextDeactivated:      // Intentional fall-through.
	case ECallbackRequestType::ReportFinishedPreload:         // Intentional fall-through.
	case ECallbackRequestType::None:
		{
			result = ERequestStatus::Success;

			break;
		}
	default:
		{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Warning, "Unknown callback manager request type: %u", pBase->callbackRequestType);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			break;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CSystem::ProcessObjectRequest(CRequest const& request)
{
	ERequestStatus result = ERequestStatus::Failure;

	auto const pBase = static_cast<SObjectRequestDataBase const*>(request.GetData());
	CObject* const pObject = pBase->pObject;

	switch (pBase->objectRequestType)
	{
	case EObjectRequestType::ExecuteTrigger:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::ExecuteTrigger> const*>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				pTrigger->Execute(*pObject, request.pOwner, request.pUserData, request.pUserDataOwner, request.flags, pRequestData->entityId);
				result = ERequestStatus::Success;
			}

			break;
		}
	case EObjectRequestType::ExecuteTriggerWithCallbacks:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::ExecuteTriggerWithCallbacks> const*>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->data.triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				pTrigger->ExecuteWithCallbacks(*pObject, pRequestData->data, request.pOwner, request.pUserData, request.pUserDataOwner, request.flags, pRequestData->entityId);
				result = ERequestStatus::Success;
			}

			break;
		}
	case EObjectRequestType::StopTrigger:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::StopTrigger> const*>(request.GetData());

			CTrigger const* const pTrigger = stl::find_in_map(g_triggerLookup, pRequestData->triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				pTrigger->Stop(pObject->GetImplData());
				result = ERequestStatus::Success;
			}

			break;
		}
	case EObjectRequestType::StopAllTriggers:
		{
			pObject->StopAllTriggers();
			result = ERequestStatus::Success;

			break;
		}
	case EObjectRequestType::SetTransformation:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetTransformation> const*>(request.GetData());

			pObject->HandleSetTransformation(pRequestData->transformation);
			result = ERequestStatus::Success;

			break;
		}
	case EObjectRequestType::SetParameter:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetParameter> const*>(request.GetData());

			CParameter const* const pParameter = stl::find_in_map(g_parameterLookup, pRequestData->parameterId, nullptr);

			if (pParameter != nullptr)
			{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				pParameter->Set(*pObject, pRequestData->value);
#else
				pParameter->Set(pObject->GetImplData(), pRequestData->value);
#endif    // CRY_AUDIO_USE_DEBUG_CODE

				result = ERequestStatus::Success;
			}

			break;
		}
	case EObjectRequestType::SetSwitchState:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetSwitchState> const*>(request.GetData());

			CSwitch const* const pSwitch = stl::find_in_map(g_switchLookup, pRequestData->switchId, nullptr);

			if (pSwitch != nullptr)
			{
				CSwitchState const* const pState = stl::find_in_map(pSwitch->GetStates(), pRequestData->switchStateId, nullptr);

				if (pState != nullptr)
				{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					pState->Set(*pObject);
#else
					pState->Set(pObject->GetImplData());
#endif      // CRY_AUDIO_USE_DEBUG_CODE

					result = ERequestStatus::Success;
				}
			}

			break;
		}
#if defined(CRY_AUDIO_USE_OCCLUSION)
	case EObjectRequestType::SetOcclusionType:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetOcclusionType> const*>(request.GetData());

			SetOcclusionType(*pObject, pRequestData->occlusionType);
			result = ERequestStatus::Success;

			break;
		}
	case EObjectRequestType::SetOcclusionRayOffset:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetOcclusionRayOffset> const*>(request.GetData());

			pObject->HandleSetOcclusionRayOffset(pRequestData->occlusionRayOffset);
			result = ERequestStatus::Success;

			break;
		}
#endif // CRY_AUDIO_USE_OCCLUSION
	case EObjectRequestType::SetCurrentEnvironments:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetCurrentEnvironments> const*>(request.GetData());

			SetCurrentEnvironmentsOnObject(pObject, pRequestData->entityToIgnore);

			break;
		}
	case EObjectRequestType::SetEnvironment:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetEnvironment> const*>(request.GetData());

			CEnvironment const* const pEnvironment = stl::find_in_map(g_environmentLookup, pRequestData->environmentId, nullptr);

			if (pEnvironment != nullptr)
			{
				pEnvironment->Set(*pObject, pRequestData->amount);
				result = ERequestStatus::Success;
			}

			break;
		}
#if defined(CRY_AUDIO_USE_OCCLUSION)
	case EObjectRequestType::ProcessPhysicsRay:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::ProcessPhysicsRay> const*>(request.GetData());

			pObject->ProcessPhysicsRay(pRequestData->rayInfo);
			result = ERequestStatus::Success;

			break;
		}
#endif // CRY_AUDIO_USE_OCCLUSION
	case EObjectRequestType::AddListener:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::AddListener> const*>(request.GetData());

			pObject->HandleAddListener(pRequestData->listenerId);
			result = ERequestStatus::Success;

			break;
		}
	case EObjectRequestType::RemoveListener:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::RemoveListener> const*>(request.GetData());

			pObject->HandleRemoveListener(pRequestData->listenerId);
			result = ERequestStatus::Success;

			break;
		}
	case EObjectRequestType::ToggleAbsoluteVelocityTracking:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::ToggleAbsoluteVelocityTracking> const*>(request.GetData());

			if (pRequestData->isEnabled)
			{
				pObject->GetImplData()->ToggleFunctionality(EObjectFunctionality::TrackAbsoluteVelocity, true);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				pObject->SetFlag(EObjectFlags::TrackAbsoluteVelocity);
#endif    // CRY_AUDIO_USE_DEBUG_CODE
			}
			else
			{
				pObject->GetImplData()->ToggleFunctionality(EObjectFunctionality::TrackAbsoluteVelocity, false);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				pObject->RemoveFlag(EObjectFlags::TrackAbsoluteVelocity);
#endif    // CRY_AUDIO_USE_DEBUG_CODE
			}

			result = ERequestStatus::Success;
			break;
		}
	case EObjectRequestType::ToggleRelativeVelocityTracking:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::ToggleRelativeVelocityTracking> const*>(request.GetData());

			if (pRequestData->isEnabled)
			{
				pObject->GetImplData()->ToggleFunctionality(EObjectFunctionality::TrackRelativeVelocity, true);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				pObject->SetFlag(EObjectFlags::TrackRelativeVelocity);
#endif    // CRY_AUDIO_USE_DEBUG_CODE
			}
			else
			{
				pObject->GetImplData()->ToggleFunctionality(EObjectFunctionality::TrackRelativeVelocity, false);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				pObject->RemoveFlag(EObjectFlags::TrackRelativeVelocity);
#endif    // CRY_AUDIO_USE_DEBUG_CODE
			}

			result = ERequestStatus::Success;
			break;
		}
	case EObjectRequestType::None:
		{
			result = ERequestStatus::Success;
			break;
		}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	case EObjectRequestType::SetName:
		{
			auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::SetName> const*>(request.GetData());

			pObject->HandleSetName(pRequestData->name.c_str());
			result = ERequestStatus::Success;

			break;
		}
#endif // CRY_AUDIO_USE_DEBUG_CODE
	default:
		{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Warning, "Unknown object request type: %u", pBase->objectRequestType);
#endif    // CRY_AUDIO_USE_DEBUG_CODE

			break;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CSystem::ProcessListenerRequest(SRequestData const* const pPassedRequestData)
{
	ERequestStatus result = ERequestStatus::Failure;
	auto const pBase = static_cast<SListenerRequestDataBase const*>(pPassedRequestData);

	switch (pBase->listenerRequestType)
	{
	case EListenerRequestType::SetTransformation:
		{
			auto const pRequestData = static_cast<SListenerRequestData<EListenerRequestType::SetTransformation> const* const>(pPassedRequestData);

			CRY_ASSERT(pRequestData->pListener != nullptr);

			if (pRequestData->pListener != nullptr)
			{
				pRequestData->pListener->HandleSetTransformation(pRequestData->transformation);
			}

			result = ERequestStatus::Success;

			break;
		}

	case EListenerRequestType::None:
		{
			result = ERequestStatus::Success;

			break;
		}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	case EListenerRequestType::SetName:
		{
			auto const pRequestData = static_cast<SListenerRequestData<EListenerRequestType::SetName> const*>(pPassedRequestData);

			pRequestData->pListener->HandleSetName(pRequestData->name.c_str());
			result = ERequestStatus::Success;

			break;
		}
#endif // CRY_AUDIO_USE_DEBUG_CODE
	default:
		{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Warning, "Unknown listener request type: %u", pBase->listenerRequestType);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			break;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::NotifyListener(CRequest const& request)
{
	ESystemEvents systemEvent = ESystemEvents::None;
	ControlId controlID = InvalidControlId;
	EntityId entityId = INVALID_ENTITYID;

	switch (request.GetData()->requestType)
	{
	case ERequestType::SystemRequest:
		{
			auto const pBase = static_cast<SSystemRequestDataBase const*>(request.GetData());

			switch (pBase->systemRequestType)
			{
			case ESystemRequestType::SetImpl:
				{
					systemEvent = ESystemEvents::ImplSet;

					break;
				}
			case ESystemRequestType::ExecuteTrigger:
				{
					auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecuteTrigger> const*>(pBase);
					controlID = pRequestData->triggerId;
					systemEvent = ESystemEvents::TriggerExecuted;

					break;
				}
			case ESystemRequestType::ExecuteTriggerWithCallbacks:
				{
					auto const pRequestData = static_cast<SSystemRequestData<ESystemRequestType::ExecuteTriggerWithCallbacks> const*>(pBase);
					controlID = pRequestData->data.triggerId;
					systemEvent = ESystemEvents::TriggerExecuted;

					break;
				}
			default:
				{
					break;
				}
			}

			break;
		}
	case ERequestType::CallbackRequest:
		{
			auto const pBase = static_cast<SCallbackRequestDataBase const*>(request.GetData());

			switch (pBase->callbackRequestType)
			{
			case ECallbackRequestType::ReportFinishedTriggerInstance:
				{
					auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerInstance> const*>(pBase);
					controlID = pRequestData->triggerId;
					entityId = pRequestData->entityId;
					systemEvent = ESystemEvents::TriggerFinished;

					break;
				}
			case ECallbackRequestType::ReportTriggerConnectionInstanceCallback:
				{
					auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportTriggerConnectionInstanceCallback> const*>(pBase);
					systemEvent = pRequestData->events;

					break;
				}
			case ECallbackRequestType::SendTriggerInstanceCallback:
				{
					auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::SendTriggerInstanceCallback> const*>(pBase);
					controlID = pRequestData->triggerId;
					entityId = pRequestData->entityId;
					systemEvent = pRequestData->events;

					break;
				}
			case ECallbackRequestType::ReportContextActivated:
				{
					systemEvent = ESystemEvents::ContextActivated;

					break;
				}
			case ECallbackRequestType::ReportContextDeactivated:
				{
					systemEvent = ESystemEvents::ContextDeactivated;

					break;
				}
			case ECallbackRequestType::ReportFinishedPreload:
				{
					auto const pRequestData = static_cast<SCallbackRequestData<ECallbackRequestType::ReportFinishedPreload> const*>(pBase);
					controlID = static_cast<ControlId>(pRequestData->preloadRequestId);
					systemEvent = pRequestData->isFullSuccess ? ESystemEvents::PreloadFinishedSuccess : ESystemEvents::PreloadFinishedFailure;

					break;
				}
			default:
				{
					break;
				}
			}

			break;
		}
	case ERequestType::ObjectRequest:
		{
			auto const pBase = static_cast<SObjectRequestDataBase const*>(request.GetData());

			switch (pBase->objectRequestType)
			{
			case EObjectRequestType::ExecuteTrigger:
				{
					auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::ExecuteTrigger> const*>(pBase);
					controlID = pRequestData->triggerId;
					entityId = pRequestData->entityId;
					systemEvent = ESystemEvents::TriggerExecuted;

					break;
				}
			case EObjectRequestType::ExecuteTriggerWithCallbacks:
				{
					auto const pRequestData = static_cast<SObjectRequestData<EObjectRequestType::ExecuteTriggerWithCallbacks> const*>(pBase);
					controlID = pRequestData->data.triggerId;
					entityId = pRequestData->entityId;
					systemEvent = ESystemEvents::TriggerExecuted;

					break;
				}
			default:
				{
					break;
				}
			}

			break;
		}
	case ERequestType::ListenerRequest:
		{
			// Nothing to do currently for this type of request.

			break;
		}
	default:
		{
			CryFatalError("Unknown request type during %s!", __FUNCTION__);

			break;
		}
	}

	ERequestResult result = ERequestResult::Failure;

	switch (request.status)
	{
	case ERequestStatus::Success:
		{
			result = ERequestResult::Success;
			break;
		}
	case ERequestStatus::Failure:
		{
			result = ERequestResult::Failure;
			break;
		}
	default:
		{
			CRY_ASSERT_MESSAGE(false, "Invalid request status '%u'. Cannot be converted to a request result during %s", request.status, __FUNCTION__);
			result = ERequestResult::Failure;
			break;
		}
	}

	SRequestInfo const requestInfo(
		result,
		request.pOwner,
		request.pUserData,
		request.pUserDataOwner,
		systemEvent,
		controlID,
		entityId);

	g_eventListenerManager.NotifyListener(&requestInfo);
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::HandleSetImpl(Impl::IImpl* const pIImpl)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	ContextInfo const tempContextInfo = g_contextInfo;
#endif // CRY_AUDIO_USE_DEBUG_CODE

	if ((g_pIImpl != nullptr) && (pIImpl != g_pIImpl))
	{
		ReleaseImpl();
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	g_contextInfo.clear();
	g_contextInfo.insert(tempContextInfo.begin(), tempContextInfo.end());
#endif // CRY_AUDIO_USE_DEBUG_CODE

	g_pIImpl = pIImpl;

	if (g_pIImpl == nullptr)
	{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, "nullptr passed to SetImpl, will run with the null implementation");
#endif // CRY_AUDIO_USE_DEBUG_CODE

		MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::Impl::Null::CImpl");
		auto const pImpl = new Impl::Null::CImpl();
		CRY_ASSERT(pImpl != nullptr);
		g_pIImpl = static_cast<Impl::IImpl*>(pImpl);
	}

	g_xmlProcessor.ParseSystemData();

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if ((g_systemStates& ESystemStates::PoolsAllocated) == ESystemStates::None)
	{
		// Don't allocate again after impl switch.
		AllocateMemoryPools();
		g_systemStates |= ESystemStates::PoolsAllocated;
	}
#else
	AllocateMemoryPools();
#endif // CRY_AUDIO_USE_DEBUG_CODE

	bool const isInitialized = g_pIImpl->Init(m_objectPoolSize);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	// Get impl info again (was done in ParseSystemData) to set the impl name, because
	// it's not guaranteed that it already existed in the impl constructor.
	g_pIImpl->GetInfo(g_implInfo);
#endif // CRY_AUDIO_USE_DEBUG_CODE

	if (!isInitialized)
	{
		// The impl failed to initialize, allow it to shut down and release then fall back to the null impl.

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Error, "Failed to set the AudioImpl %s. Will run with the null implementation.", g_implInfo.name);
#endif // CRY_AUDIO_USE_DEBUG_CODE

		// There's no need to call Shutdown when the initialization failed as
		// we expect the implementation to clean-up itself if it couldn't be initialized

		g_pIImpl->Release(); // Release the engine specific data.

		MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::Impl::Null::CImpl");
		auto const pImpl = new Impl::Null::CImpl();
		CRY_ASSERT(pImpl != nullptr);
		g_pIImpl = static_cast<Impl::IImpl*>(pImpl);
	}

	CRY_ASSERT_MESSAGE(g_defaultListener.GetImplData() == nullptr, "<Audio> The default listeners's impl-data must be nullptr during %s", __FUNCTION__);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	g_defaultListener.SetImplData(g_pIImpl->ConstructListener(g_defaultListener.GetDebugTransformation(), g_szDefaultListenerName));
#else
	g_defaultListener.SetImplData(g_pIImpl->ConstructListener(CTransformation::GetEmptyObject(), g_szDefaultListenerName));
#endif // CRY_AUDIO_USE_DEBUG_CODE

	CRY_ASSERT_MESSAGE(g_pIObject == nullptr, "<Audio> g_pIObject must be nullptr during %s", __FUNCTION__);
	Impl::IListeners const defaultImplListener{ g_defaultListener.GetImplData() };

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	g_pIObject = g_pIImpl->ConstructObject(CTransformation::GetEmptyObject(), defaultImplListener, g_szGlobalName);
#else
	g_pIObject = g_pIImpl->ConstructObject(CTransformation::GetEmptyObject(), defaultImplListener);
#endif // CRY_AUDIO_USE_DEBUG_CODE

	string const listenerNames = g_cvars.m_pListeners->GetString();

	if (!listenerNames.empty())
	{
		int curPos = 0;
		string listenerName = listenerNames.Tokenize(",", curPos);

		while (!listenerName.empty())
		{
			listenerName.Trim();

			if (g_listenerManager.GetListener(StringToId(listenerName.c_str())) == &g_defaultListener)
			{
				g_listenerManager.CreateListener(CTransformation::GetEmptyObject(), listenerName.c_str(), false);
			}

			listenerName = listenerNames.Tokenize(",", curPos);
		}
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CRY_ASSERT_MESSAGE(g_previewListener.GetImplData() == nullptr, "<Audio> The preview listeners's impl-data must be nullptr during %s", __FUNCTION__);
	g_previewListener.SetImplData(g_pIImpl->ConstructListener(CTransformation::GetEmptyObject(), g_szPreviewListenerName));

	CRY_ASSERT_MESSAGE(g_previewObject.GetImplData() == nullptr, "<Audio> The preview object's impl-data must be nullptr during %s", __FUNCTION__);
	Impl::IListeners const previewImplListener{ g_previewListener.GetImplData() };
	g_previewObject.SetImplData(g_pIImpl->ConstructObject(CTransformation::GetEmptyObject(), previewImplListener, g_previewObject.GetName()));
	g_previewObject.SetFlag(EObjectFlags::IgnoreDrawDebugInfo);

	g_listenerManager.ReconstructImplData();

	for (auto const pObject : g_constructedObjects)
	{
		CRY_ASSERT_MESSAGE(pObject->GetImplData() == nullptr, "<Audio> The object's impl-data must be nullptr during %s", __FUNCTION__);

		Listeners const& listeners = pObject->GetListeners();
		Impl::IListeners implListeners;

		for (auto const pListener : listeners)
		{
			implListeners.push_back(pListener->GetImplData());
		}

		pObject->SetImplData(g_pIImpl->ConstructObject(pObject->GetTransformation(), implListeners, pObject->GetName()));
	}

	for (auto const& contextPair : g_contextInfo)
	{
		ContextId const contextId = contextPair.second.contextId;

		if (contextPair.second.isRegistered && contextPair.second.isActive && (contextId != GlobalContextId))
		{
			char const* const szContextName = contextPair.first.c_str();

			CryFixedStringT<MaxFilePathLength> contextPath = g_configPath;
			contextPath += g_szContextsFolderName;
			contextPath += "/";
			contextPath += szContextName;

			g_xmlProcessor.ParseControlsData(contextPath.c_str(), contextId, szContextName);
			g_xmlProcessor.ParsePreloadsData(contextPath.c_str(), contextId);

			auto const preloadRequestId = static_cast<PreloadRequestId>(contextId);

			if (g_fileCacheManager.TryLoadRequest(preloadRequestId, true, true) != ERequestStatus::Success)
			{
				Cry::Audio::Log(ELogType::Warning, R"(No preload request found for context - "%s"!)", szContextName);
			}

			AutoLoadSetting(contextId);
			ReportContextActivated(contextId);
		}
	}

	HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager | EDebugUpdateFilter::Contexts);
#endif // CRY_AUDIO_USE_DEBUG_CODE

	SetImplLanguage();

	return isInitialized;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetImplLanguage()
{
	if (ICVar* pCVar = gEnv->pConsole->GetCVar("g_languageAudio"))
	{
		g_pIImpl->SetLanguage(pCVar->GetString());
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::HandleActivateContext(ContextId const contextId)
{
	CryFixedStringT<MaxFileNameLength> const contextName = stl::find_in_map(g_contextLookup, contextId, "");

	if (!contextName.empty())
	{
		CryFixedStringT<MaxFilePathLength> contextPath = g_configPath;
		contextPath += g_szContextsFolderName;
		contextPath += "/";
		contextPath += contextName.c_str();

		if (g_xmlProcessor.ParseControlsData(contextPath.c_str(), contextId, contextName.c_str()))
		{
			g_xmlProcessor.ParsePreloadsData(contextPath.c_str(), contextId);

			for (auto const& preloadPair : g_preloadRequests)
			{
				if (preloadPair.second->GetContextId() == contextId)
				{
					g_fileCacheManager.TryLoadRequest(preloadPair.second->GetId(), true, true);
				}
			}

			AutoLoadSetting(contextId);
			ReportContextActivated(contextId);
		}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager | EDebugUpdateFilter::Contexts);
#endif // CRY_AUDIO_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::HandleDeactivateContext(ContextId const contextId)
{
	g_xmlProcessor.ClearControlsData(contextId, false);
	g_xmlProcessor.ClearPreloadsData(contextId, false);

	for (auto const& preloadPair : g_preloadRequests)
	{
		if (preloadPair.second->GetContextId() == contextId)
		{
			g_fileCacheManager.TryUnloadRequest(preloadPair.second->GetId());
		}
	}

	ReportContextDeactivated(contextId);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager | EDebugUpdateFilter::Contexts);
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportContextActivated(ContextId const id)
{
	SCallbackRequestData<ECallbackRequestType::ReportContextActivated> const requestData;
	CRequest const request(
		&requestData,
		ERequestFlags::CallbackOnExternalOrCallingThread,
		nullptr,
		reinterpret_cast<void*>(static_cast<uintptr_t>(id)));
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportContextDeactivated(ContextId const id)
{
	// Use GlobalContextId when all contexts have been deactivated.

	SCallbackRequestData<ECallbackRequestType::ReportContextDeactivated> const requestData;
	CRequest const request(
		&requestData,
		ERequestFlags::CallbackOnExternalOrCallingThread,
		nullptr,
		reinterpret_cast<void*>(static_cast<uintptr_t>(id)));
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetCurrentEnvironmentsOnObject(CObject* const pObject, EntityId const entityToIgnore)
{
	IAreaManager* const pIAreaManager = gEnv->pEntitySystem->GetAreaManager();
	size_t numAreas = 0;
	static size_t const s_maxAreas = 10;
	static SAudioAreaInfo s_areaInfos[s_maxAreas];

	if (pIAreaManager->QueryAudioAreas(pObject->GetTransformation().GetPosition(), s_areaInfos, s_maxAreas, numAreas))
	{
		for (size_t i = 0; i < numAreas; ++i)
		{
			SAudioAreaInfo const& areaInfo = s_areaInfos[i];

			if (entityToIgnore == INVALID_ENTITYID || entityToIgnore != areaInfo.envProvidingEntityId)
			{
				CEnvironment const* const pEnvironment = stl::find_in_map(g_environmentLookup, areaInfo.audioEnvironmentId, nullptr);

				if (pEnvironment != nullptr)
				{
					pEnvironment->Set(*pObject, areaInfo.amount);
				}
			}
		}
	}
}

#if defined(CRY_AUDIO_USE_OCCLUSION)
//////////////////////////////////////////////////////////////////////////
void CSystem::SetOcclusionType(CObject& object, EOcclusionType const occlusionType) const
{
	switch (occlusionType)
	{
	case EOcclusionType::Ignore:
		{
			object.HandleSetOcclusionType(EOcclusionType::Ignore);
			object.SetOcclusion(0.0f);

			break;
		}
	case EOcclusionType::Adaptive:
		{
			object.HandleSetOcclusionType(EOcclusionType::Adaptive);

			break;
		}
	case EOcclusionType::Low:
		{
			object.HandleSetOcclusionType(EOcclusionType::Low);

			break;
		}
	case EOcclusionType::Medium:
		{
			object.HandleSetOcclusionType(EOcclusionType::Medium);

			break;
		}
	case EOcclusionType::High:
		{
			object.HandleSetOcclusionType(EOcclusionType::High);

			break;
		}
	default:
		{
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Warning, "Unknown occlusion type during %s: %u", __FUNCTION__, occlusionType);
	#endif // CRY_AUDIO_USE_DEBUG_CODE

			break;
		}
	}
}
#endif // CRY_AUDIO_USE_OCCLUSION

//////////////////////////////////////////////////////////////////////////
void CSystem::OnCallback(SRequestInfo const* const pRequestInfo)
{
	if ((gEnv->mMainThreadId == CryGetCurrentThreadId()) && (pRequestInfo->entityId != INVALID_ENTITYID))
	{
		IEntity* const pIEntity = gEnv->pEntitySystem->GetEntity(pRequestInfo->entityId);

		if (pIEntity != nullptr)
		{
			SEntityEvent eventData;
			eventData.nParam[0] = reinterpret_cast<intptr_t>(pRequestInfo);

			if (pRequestInfo->systemEvent == ESystemEvents::TriggerExecuted)
			{
				eventData.event = ENTITY_EVENT_AUDIO_TRIGGER_STARTED;
				pIEntity->SendEvent(eventData);
			}

			if ((pRequestInfo->systemEvent == ESystemEvents::TriggerFinished) ||
			    ((pRequestInfo->systemEvent == ESystemEvents::TriggerExecuted) && (pRequestInfo->requestResult != ERequestResult::Success)))
			{
				eventData.event = ENTITY_EVENT_AUDIO_TRIGGER_ENDED;
				pIEntity->SendEvent(eventData);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetImplInfo(SImplInfo& implInfo)
{
	SSystemRequestData<ESystemRequestType::GetImplInfo> const requestData(implInfo);
	CRequest const request(&requestData, ERequestFlags::ExecuteBlocking);
	PushRequest(request);
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CSystem::HandleRefresh()
{
	Cry::Audio::Log(ELogType::Warning, "Beginning to refresh the AudioSystem!");

	g_pIImpl->StopAllSounds();

	for (auto const& contextPair : g_contextInfo)
	{
		if (contextPair.second.isRegistered && contextPair.second.isActive)
		{
			g_fileCacheManager.UnloadDataByContext(contextPair.second.contextId);
		}
	}

	g_fileCacheManager.UnloadDataByContext(GlobalContextId);

	// Store active contexts for reloading after they have been unloaded which empties the map.
	ContextInfo const tempContextInfo = g_contextInfo;

	g_xmlProcessor.ClearPreloadsData(GlobalContextId, true);
	g_xmlProcessor.ClearControlsData(GlobalContextId, true);
	ReportContextDeactivated(GlobalContextId);
	ResetRequestCount();

	g_pIImpl->OnRefresh();

	g_xmlProcessor.ParseSystemData();
	g_xmlProcessor.ParseControlsData(g_configPath.c_str(), GlobalContextId, g_szGlobalContextName);
	g_xmlProcessor.ParsePreloadsData(g_configPath.c_str(), GlobalContextId);

	// The global preload might not exist if no preloads have been created, for that reason we don't check the result of this call
	g_fileCacheManager.TryLoadRequest(GlobalPreloadRequestId, true, true);

	AutoLoadSetting(GlobalContextId);

	for (auto const& contextPair : tempContextInfo)
	{
		ContextId const contextId = contextPair.second.contextId;

		if (contextPair.second.isRegistered && contextPair.second.isActive && (contextId != GlobalContextId))
		{
			char const* const szContextName = contextPair.first.c_str();

			CryFixedStringT<MaxFilePathLength> contextPath = g_configPath;
			contextPath += g_szContextsFolderName;
			contextPath += "/";
			contextPath += szContextName;

			g_xmlProcessor.ParseControlsData(contextPath.c_str(), contextId, szContextName);
			g_xmlProcessor.ParsePreloadsData(contextPath.c_str(), contextId);

			auto const preloadRequestId = static_cast<PreloadRequestId>(contextId);

			if (g_fileCacheManager.TryLoadRequest(preloadRequestId, true, true) != ERequestStatus::Success)
			{
				Cry::Audio::Log(ELogType::Warning, R"(No preload request found for context - "%s"!)", szContextName);
			}

			AutoLoadSetting(contextId);
			ReportContextActivated(contextId);
		}
	}

	HandleUpdateDebugInfo(EDebugUpdateFilter::FileCacheManager | EDebugUpdateFilter::Contexts);

	Cry::Audio::Log(ELogType::Warning, "Done refreshing the AudioSystem!");
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ScheduleIRenderAuxGeomForRendering(IRenderAuxGeom* pRenderAuxGeom)
{
	auto oldRenderAuxGeom = m_currentRenderAuxGeom.exchange(pRenderAuxGeom);
	CRY_ASSERT(oldRenderAuxGeom != pRenderAuxGeom);

	// Kill FIFO entries beyond 1, only the head survives in m_currentRenderAuxGeom
	// Throw away all older entries
	if (oldRenderAuxGeom && oldRenderAuxGeom != pRenderAuxGeom)
	{
		gEnv->pRenderer->DeleteAuxGeom(oldRenderAuxGeom);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SubmitLastIRenderAuxGeomForRendering()
{
	if (m_lastDebugRenderSubmitExternalFrame != gEnv->nMainFrameID)
	{
		// get auxGeom from "storage"
		auto pCurrentRenderAuxGeom = m_currentRenderAuxGeom.exchange(nullptr);
		if (pCurrentRenderAuxGeom != nullptr)
		{
			gEnv->pRenderer->SubmitAuxGeom(pCurrentRenderAuxGeom, true);
			// if another auxGeom was stored in the meantime, we can throw away the current one
			// otherwise we keep it around for re-use, to avoid flickering
			IRenderAuxGeom* pExpected = nullptr;
			if (!m_currentRenderAuxGeom.compare_exchange_strong(pExpected, pCurrentRenderAuxGeom))
			{
				gEnv->pRenderer->DeleteAuxGeom(pCurrentRenderAuxGeom);
			}
			m_lastDebugRenderSubmitExternalFrame = gEnv->nMainFrameID;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::DrawDebug()
{
	if (g_cvars.m_drawDebug > 0)
	{
		SubmitLastIRenderAuxGeomForRendering();

		if (m_canDraw)
		{
			m_canDraw = false;

			SSystemRequestData<ESystemRequestType::DrawDebugInfo> const requestData;
			CRequest const request(&requestData);
			PushRequest(request);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UpdateDebugInfo()
{
	SSystemRequestData<ESystemRequestType::UpdateDebugInfo> const requestData;
	CRequest const request(&requestData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void DrawRequestCategoryInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, char const* const szType)
{
	auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextSecondary, false, "%s Request Peak:", szType);
	posY += Debug::g_systemLineHeight;
}

//////////////////////////////////////////////////////////////////////////
void DrawRequestPeakInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, char const* const szType, uint16 const peak, uint16 poolSize)
{
	bool const poolSizeExceeded = (peak > poolSize) && (poolSize != 0);
	CryFixedStringT<MaxInfoStringLength> debugText;

	if (poolSizeExceeded)
	{
		debugText.Format("%s: %u (Pool Size: %u)", szType, peak, poolSize);
	}
	else
	{
		debugText.Format("%s: %u", szType, peak);
	}

	auxGeom.Draw2dLabel(
		posX,
		posY,
		Debug::g_systemFontSize,
		poolSizeExceeded ? Debug::s_globalColorError : Debug::s_systemColorTextPrimary,
		false,
		"%s", debugText.c_str());
	posY += Debug::g_systemLineHeight;
}

//////////////////////////////////////////////////////////////////////////
void DrawRequestDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY)
{
	auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Audio Requests");
	posY += Debug::g_listHeaderLineHeight;

	DrawRequestPeakInfo(auxGeom, posX, posY, "Total", g_requestPeaks.requests, 0);

	DrawRequestCategoryInfo(auxGeom, posX, posY, "System");
	DrawRequestPeakInfo(auxGeom, posX, posY, "ExecuteTrigger", g_requestPeaks.systemExecuteTrigger, g_systemExecuteTriggerPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ExecuteTriggerEx", g_requestPeaks.systemExecuteTriggerEx, g_systemExecuteTriggerExPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ExecuteTriggerWithCallbacks", g_requestPeaks.systemExecuteTriggerWithCallbacks, g_systemExecuteTriggerWithCallbacksPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "StopTrigger", g_requestPeaks.systemStopTrigger, g_systemStopTriggerPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "RegisterObject", g_requestPeaks.systemRegisterObject, g_systemRegisterObjectPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ReleaseObject", g_requestPeaks.systemReleaseObject, g_systemReleaseObjectPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetParameter", g_requestPeaks.systemSetParameter, g_systemSetParameterPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetSwitchState", g_requestPeaks.systemSetSwitchState, g_systemSetSwitchStatePoolSize);

	DrawRequestCategoryInfo(auxGeom, posX, posY, "Object");
	DrawRequestPeakInfo(auxGeom, posX, posY, "ExecuteTrigger", g_requestPeaks.objectExecuteTrigger, g_objectExecuteTriggerPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ExecuteTriggerWithCallbacks", g_requestPeaks.objectExecuteTriggerWithCallbacks, g_objectExecuteTriggerWithCallbacksPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "StopTrigger", g_requestPeaks.objectStopTrigger, g_objectStopTriggerPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetTransformation", g_requestPeaks.objectSetTransformation, g_objectSetTransformationPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetParameter", g_requestPeaks.objectSetParameter, g_objectSetParameterPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetSwitchState", g_requestPeaks.objectSetSwitchState, g_objectSetSwitchStatePoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetCurrentEnvironments", g_requestPeaks.objectSetCurrentEnvironments, g_objectSetCurrentEnvironmentsPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetEnvironment", g_requestPeaks.objectSetEnvironment, g_objectSetEnvironmentPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ProcessPhysicsRay", g_requestPeaks.objectProcessPhysicsRay, g_objectProcessPhysicsRayPoolSize);

	DrawRequestCategoryInfo(auxGeom, posX, posY, "Listener");
	DrawRequestPeakInfo(auxGeom, posX, posY, "SetTransformation", g_requestPeaks.listenerSetTransformation, g_listenerSetTransformationPoolSize);

	DrawRequestCategoryInfo(auxGeom, posX, posY, "Callback");
	DrawRequestPeakInfo(auxGeom, posX, posY, "ReportStartedTriggerConnectionInstance", g_requestPeaks.callbackReportStartedriggerConnectionInstance, g_callbackReportStartedTriggerConnectionInstancePoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ReportFinishedTriggerConnectionInstance", g_requestPeaks.callbackReportFinishedTriggerConnectionInstance, g_callbackReportFinishedTriggerConnectionInstancePoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ReportFinishedTriggerInstance", g_requestPeaks.callbackReportFinishedTriggerInstance, g_callbackReportFinishedTriggerInstancePoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ReportTriggerConnectionInstanceCallback", g_requestPeaks.callbackReportTriggerConnectionInstanceCallback, g_callbackReportTriggerConnectionInstanceCallbackPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "SendTriggerInstanceCallback", g_requestPeaks.callbackSendTriggerInstanceCallback, g_callbackSendTriggerInstanceCallbackPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ReportPhysicalizedObject", g_requestPeaks.callbackReportPhysicalizedObject, g_callbackReportPhysicalizedObjectPoolSize);
	DrawRequestPeakInfo(auxGeom, posX, posY, "ReportVirtualizedObject", g_requestPeaks.callbackReportVirtualizedObject, g_callbackReportVirtualizedObjectPoolSize);
}

//////////////////////////////////////////////////////////////////////////
void DrawObjectInfo(
	IRenderAuxGeom& auxGeom,
	float const posX,
	float& posY,
	Vec3 const& camPos,
	CObject const& object,
	CryFixedStringT<MaxControlNameLength> const& lowerCaseSearchString,
	size_t& numObjects)
{
	Vec3 const& position = object.GetTransformation().GetPosition();
	float const distance = position.GetDistance(camPos);

	if ((g_cvars.m_debugDistance <= 0.0f) || ((g_cvars.m_debugDistance > 0.0f) && (distance < g_cvars.m_debugDistance)))
	{
		char const* const szObjectName = object.GetName();
		CryFixedStringT<MaxControlNameLength> lowerCaseObjectName(szObjectName);
		lowerCaseObjectName.MakeLower();
		bool const hasActiveData = (object.GetFlags() & EObjectFlags::Active) != EObjectFlags::None;
		bool const isVirtual = (object.GetFlags() & EObjectFlags::Virtual) != EObjectFlags::None;
		bool const stringFound = (lowerCaseSearchString.empty() || (lowerCaseSearchString.compareNoCase("0") == 0)) || (lowerCaseObjectName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos);
		bool const draw = stringFound && ((g_cvars.m_hideInactiveObjects == 0) || ((g_cvars.m_hideInactiveObjects != 0) && hasActiveData && !isVirtual));

		if (draw)
		{
			CryFixedStringT<MaxMiscStringLength> debugText;

			if ((object.GetFlags() & EObjectFlags::InUse) != EObjectFlags::None)
			{
				debugText.Format(szObjectName);
			}
			else
			{
				debugText.Format("%s [To Be Released]", szObjectName);
			}

			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_listFontSize,
				!hasActiveData ? Debug::s_globalColorInactive : (isVirtual ? Debug::s_globalColorVirtual : Debug::s_listColorItemActive),
				false,
				"%s", debugText.c_str());

			posY += Debug::g_listLineHeight;
			++numObjects;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawObjectDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY)
{
	size_t numObjects = 0;
	float const headerPosY = posY;
	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
	lowerCaseSearchString.MakeLower();

	posY += Debug::g_listHeaderLineHeight;
	Vec3 const& camPos = GetISystem()->GetViewCamera().GetPosition();

	DrawObjectInfo(auxGeom, posX, posY, camPos, g_previewObject, lowerCaseSearchString, numObjects);

	for (auto const pObject : g_constructedObjects)
	{
		float const distance = pObject->GetTransformation().GetPosition().GetDistance(camPos);

		if ((g_cvars.m_debugDistance <= 0.0f) || ((g_cvars.m_debugDistance > 0.0f) && (distance <= g_cvars.m_debugDistance)))
		{
			DrawObjectInfo(auxGeom, posX, posY, camPos, *pObject, lowerCaseSearchString, numObjects);
		}
	}

	auxGeom.Draw2dLabel(posX, headerPosY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Audio Objects [%" PRISIZE_T "]", numObjects);
}

//////////////////////////////////////////////////////////////////////////
void DrawPerActiveObjectDebugInfo(IRenderAuxGeom& auxGeom)
{
	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
	lowerCaseSearchString.MakeLower();
	bool const isTextFilterDisabled = (lowerCaseSearchString.empty() || (lowerCaseSearchString.compareNoCase("0") == 0));

	for (auto const pObject : g_activeObjects)
	{
		if ((pObject->GetFlags() & EObjectFlags::IgnoreDrawDebugInfo) == EObjectFlags::None)
		{
			pObject->DrawDebugInfo(auxGeom, isTextFilterDisabled, lowerCaseSearchString);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawContextDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY)
{
	auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Contexts [%" PRISIZE_T "]", g_contextLookup.size() + 1);
	posY += Debug::g_listHeaderLineHeight;

	for (auto const& contextPair : g_contextDebugInfo)
	{
		auxGeom.Draw2dLabel(
			posX,
			posY,
			Debug::g_listFontSize,
			contextPair.second ? Debug::s_listColorItemActive : Debug::s_globalColorInactive,
			false,
			"%s", contextPair.first.c_str());

		posY += Debug::g_listLineHeight;
	}
}

///////////////////////////////////////////////////////////////////////////
void DrawGlobalDataDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY)
{
	auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, g_szGlobalName);
	posY += Debug::g_listHeaderLineHeight;

	// Check if text filter is enabled.
	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
	lowerCaseSearchString.MakeLower();
	bool const isTextFilterDisabled = (lowerCaseSearchString.empty() || (lowerCaseSearchString.compareNoCase("0") == 0));
	bool const filterAllObjectInfo = (g_cvars.m_drawDebug & Debug::EDrawFilter::FilterAllObjectInfo) != 0;

	// Check if any trigger matches text filter.
	bool doesTriggerMatchFilter = false;
	std::vector<CryFixedStringT<MaxMiscStringLength>> triggerInfo;

	if (!g_triggerInstances.empty() || filterAllObjectInfo)
	{
		Debug::TriggerCounts triggerCounts;

		for (auto const& triggerInstancePair : g_triggerInstances)
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

	if (!g_switchStates.empty() || filterAllObjectInfo)
	{
		for (auto const& switchStatePair : g_switchStates)
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

	if (!g_parameters.empty() || filterAllObjectInfo)
	{
		for (auto const& parameterPair : g_parameters)
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

	if (isTextFilterDisabled || doesTriggerMatchFilter)
	{
		for (auto const& debugText : triggerInfo)
		{
			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_objectFontSize,
				Debug::s_objectColorTrigger,
				false,
				"%s", debugText.c_str());

			posY += Debug::g_objectLineHeight;
		}
	}

	if (isTextFilterDisabled || doesStateSwitchMatchFilter)
	{
		for (auto const& switchStatePair : switchStateInfo)
		{
			auto const pSwitch = switchStatePair.first;
			auto const pSwitchState = switchStatePair.second;

			Debug::CStateDrawData& drawData = g_stateDrawInfo.emplace(std::piecewise_construct, std::forward_as_tuple(pSwitch->GetId()), std::forward_as_tuple(pSwitchState->GetId())).first->second;
			drawData.Update(pSwitchState->GetId());
			ColorF const switchTextColor = { 0.8f, drawData.m_currentSwitchColor, 0.6f };

			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_objectFontSize,
				switchTextColor,
				false,
				"%s: %s\n",
				pSwitch->GetName(),
				pSwitchState->GetName());

			posY += Debug::g_objectLineHeight;
		}
	}

	if (isTextFilterDisabled || doesParameterMatchFilter)
	{
		for (auto const& parameterPair : parameterInfo)
		{
			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_objectFontSize,
				Debug::s_objectColorParameter,
				false,
				"%s: %2.2f\n",
				parameterPair.first,
				parameterPair.second);

			posY += Debug::g_objectLineHeight;
		}
	}

	if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectImplInfo) != 0)
	{
		g_pIObject->DrawDebugInfo(auxGeom, posX, posY, (isTextFilterDisabled ? nullptr : lowerCaseSearchString.c_str()));
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::HandleDrawDebug()
{
	CRY_PROFILE_FUNCTION(PROFILE_AUDIO);
	IRenderAuxGeom* const pAuxGeom = gEnv->pRenderer ? gEnv->pRenderer->GetOrCreateIRenderAuxGeom() : nullptr;

	if (pAuxGeom != nullptr)
	{
		if ((g_cvars.m_drawDebug & Debug::objectMask) != 0)
		{
			// Needs to be called first so that the rest of the labels are printed on top.
			// (Draw2dLabel doesn't provide a way to set which labels are printed on top)
			DrawPerActiveObjectDebugInfo(*pAuxGeom);
		}

		float posX = 8.0f;
		float posY = 4.0f;
		float const headerPosY = posY;

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::HideMemoryInfo) == 0)
		{
			posY += Debug::g_systemHeaderLineSpacerHeight;

			size_t totalPoolSize = 0;
			bool const drawDetailedMemInfo = (g_cvars.m_drawDebug & Debug::EDrawFilter::DetailedMemoryInfo) != 0;

			{
				auto& allocator = CObject::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Objects", m_objectPoolSize);
				}
			}

	#if defined(CRY_AUDIO_USE_OCCLUSION)
			{
				auto& allocator = SOcclusionInfo::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Occlusion Infos", m_objectPoolSize);
				}
			}
	#endif // CRY_AUDIO_USE_OCCLUSION

			{
				auto& allocator = CTriggerInstance::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Trigger Instances", static_cast<uint16>(g_cvars.m_triggerInstancePoolSize));
				}
			}

			if (g_debugPoolSizes.triggers > 0)
			{
				auto& allocator = CTrigger::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Triggers", g_poolSizes.triggers);
				}
			}

			if (g_debugPoolSizes.parameters > 0)
			{
				auto& allocator = CParameter::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Parameters", g_poolSizes.parameters);
				}
			}

			if (g_debugPoolSizes.switches > 0)
			{
				auto& allocator = CSwitch::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Switches", g_poolSizes.switches);
				}
			}

			if (g_debugPoolSizes.states > 0)
			{
				auto& allocator = CSwitchState::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "SwitchStates", g_poolSizes.states);
				}
			}

			if (g_debugPoolSizes.environments > 0)
			{
				auto& allocator = CEnvironment::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Environments", g_poolSizes.environments);
				}
			}

			if (g_debugPoolSizes.preloads > 0)
			{
				auto& allocator = CPreloadRequest::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Preloads", g_poolSizes.preloads);
				}
			}

			if (g_debugPoolSizes.settings > 0)
			{
				auto& allocator = CSetting::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Settings", g_poolSizes.settings);
				}
			}

			if (g_debugPoolSizes.files > 0)
			{
				auto& allocator = CFile::GetAllocator();
				size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
				totalPoolSize += memAlloc;

				if (drawDetailedMemInfo)
				{
					Debug::DrawMemoryPoolInfo(*pAuxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Files", g_poolSizes.files);
				}
			}

			CryModuleMemoryInfo memInfo;
			ZeroStruct(memInfo);

	#if !defined(_LIB)
			// This works differently in monolithic builds and therefore doesn't cater to our needs.
			CryGetMemoryInfoForModule(&memInfo);
	#endif // _LIB

			CryFixedStringT<Debug::MaxMemInfoStringLength> memAllocSizeString;
			auto const memAllocSize = static_cast<size_t>(memInfo.allocated - memInfo.freed);
			Debug::FormatMemoryString(memAllocSizeString, memAllocSize);

			CryFixedStringT<Debug::MaxMemInfoStringLength> totalPoolSizeString;
			Debug::FormatMemoryString(totalPoolSizeString, totalPoolSize);

			size_t const totalFileSize = g_fileCacheManager.GetTotalCachedFileSize();

			CryFixedStringT<Debug::MaxMemInfoStringLength> totalMemSizeString;
			size_t const totalMemSize = memAllocSize + totalPoolSize + totalFileSize;
			Debug::FormatMemoryString(totalMemSizeString, totalMemSize);

			char const* const szMuted = ((g_systemStates& ESystemStates::IsMuted) != ESystemStates::None) ? " - Muted" : "";
			char const* const szPaused = ((g_systemStates& ESystemStates::IsPaused) != ESystemStates::None) ? " - Paused" : "";

			if (totalFileSize > 0)
			{
				CryFixedStringT<Debug::MaxMemInfoStringLength> totalFileSizeString;
				Debug::FormatMemoryString(totalFileSizeString, totalFileSize);

				pAuxGeom->Draw2dLabel(posX, headerPosY, Debug::g_systemHeaderFontSize, Debug::s_globalColorHeader, false,
				                      "Audio (System: %s | Pools: %s | Assets: %s | Total: %s)%s%s",
				                      memAllocSizeString.c_str(), totalPoolSizeString.c_str(), totalFileSizeString.c_str(), totalMemSizeString.c_str(), szMuted, szPaused);
			}
			else
			{
				pAuxGeom->Draw2dLabel(posX, headerPosY, Debug::g_systemHeaderFontSize, Debug::s_globalColorHeader, false,
				                      "Audio (System: %s | Pools: %s | Total: %s)%s%s",
				                      memAllocSizeString.c_str(), totalPoolSizeString.c_str(), totalMemSizeString.c_str(), szMuted, szPaused);
			}

			size_t const numObjects = g_constructedObjects.size();
			size_t const numActiveObjects = g_activeObjects.size();
			size_t const numListeners = g_listenerManager.GetNumListeners();
			size_t const numEventListeners = g_eventListenerManager.GetNumEventListeners();

	#if defined(CRY_AUDIO_USE_OCCLUSION)
			static float const SMOOTHING_ALPHA = 0.2f;
			static float syncRays = 0;
			static float asyncRays = 0;
			syncRays += (CPropagationProcessor::s_totalSyncPhysRays - syncRays) * SMOOTHING_ALPHA;
			asyncRays += (CPropagationProcessor::s_totalAsyncPhysRays - asyncRays) * SMOOTHING_ALPHA * 0.1f;

			posY += Debug::g_systemLineHeight;
			pAuxGeom->Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextSecondary, false,
			                      "Objects: %3" PRISIZE_T "/%3" PRISIZE_T " | EventListeners %3" PRISIZE_T " | Listeners: %" PRISIZE_T " | SyncRays: %3.1f AsyncRays: %3.1f",
			                      numActiveObjects, numObjects, numEventListeners, numListeners, syncRays, asyncRays);
	#else
			posY += Debug::g_systemLineHeight;
			pAuxGeom->Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextSecondary, false,
			                      "Objects: %3" PRISIZE_T "/%3" PRISIZE_T " | EventListeners %3" PRISIZE_T " | Listeners: %" PRISIZE_T,
			                      numActiveObjects, numObjects, numEventListeners, numListeners);
	#endif  // CRY_AUDIO_USE_OCCLUSION

			if (g_pIImpl != nullptr)
			{
				posY += Debug::g_systemHeaderLineHeight;
				g_pIImpl->DrawDebugMemoryInfo(*pAuxGeom, posX, posY, (g_cvars.m_drawDebug & Debug::EDrawFilter::DetailedMemoryInfo) != 0);
			}

			posY += Debug::g_systemHeaderLineHeight;
		}

		string debugFilter = g_cvars.m_pDebugFilter->GetString();

		if (debugFilter.IsEmpty() || debugFilter == "0")
		{
			debugFilter = "<none>";
		}

		string debugDistance = ToString(g_cvars.m_debugDistance) + " m";

		if (g_cvars.m_debugDistance <= 0)
		{
			debugDistance = "<infinite>";
		}

		string debugDraw = "";

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::Spheres) != 0)
		{
			debugDraw += "Spheres, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectLabel) != 0)
		{
			debugDraw += "Labels, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectTriggers) != 0)
		{
			debugDraw += "Triggers, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectStates) != 0)
		{
			debugDraw += "States, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectParameters) != 0)
		{
			debugDraw += "Parameters, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectEnvironments) != 0)
		{
			debugDraw += "Environments, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectDistance) != 0)
		{
			debugDraw += "Distances, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionRayLabels) != 0)
		{
			debugDraw += "Occlusion Ray Labels, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionRays) != 0)
		{
			debugDraw += "Occlusion Rays, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionRayOffset) != 0)
		{
			debugDraw += "Occlusion Ray Offset, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionListenerPlane) != 0)
		{
			debugDraw += "Occlusion Listener Plane, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionCollisionSpheres) != 0)
		{
			debugDraw += "Occlusion Collision Spheres, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::GlobalPlaybackInfo) != 0)
		{
			debugDraw += "Default Object Info, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ObjectImplInfo) != 0)
		{
			debugDraw += "Object Middleware Info, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::Contexts) != 0)
		{
			debugDraw += "Active Contexts, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ImplList) != 0)
		{
			debugDraw += "Implementation List, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ActiveObjects) != 0)
		{
			debugDraw += "Active Objects, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::FileCacheManagerInfo) != 0)
		{
			debugDraw += "File Cache Manager, ";
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::RequestInfo) != 0)
		{
			debugDraw += "Requests, ";
		}

		if (!debugDraw.IsEmpty())
		{
			debugDraw.erase(debugDraw.length() - 2, 2);
			pAuxGeom->Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextPrimary, false, "Debug Draw: %s", debugDraw.c_str());
			posY += Debug::g_systemLineHeight;
			pAuxGeom->Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextPrimary, false, "Debug Filter: %s | Debug Distance: %s", debugFilter.c_str(), debugDistance.c_str());

			posY += Debug::g_systemHeaderLineHeight;
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::FileCacheManagerInfo) != 0)
		{
			g_fileCacheManager.DrawDebugInfo(*pAuxGeom, posX, posY);
			posX += 600.0f;
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::GlobalPlaybackInfo) != 0)
		{
			DrawGlobalDataDebugInfo(*pAuxGeom, posX, posY);
			posX += 400.0f;
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::Contexts) != 0)
		{
			DrawContextDebugInfo(*pAuxGeom, posX, posY);
			posX += 200.0f;
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ActiveObjects) != 0)
		{
			DrawObjectDebugInfo(*pAuxGeom, posX, posY);
			posX += 300.0f;
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::ImplList) != 0)
		{
			if (g_pIImpl != nullptr)
			{
				Vec3 const& camPos = GetISystem()->GetViewCamera().GetPosition();

				g_pIImpl->DrawDebugInfoList(*pAuxGeom, posX, posY, camPos, g_cvars.m_debugDistance, g_cvars.m_pDebugFilter->GetString());
				// The impl is responsible for increasing posX.
			}
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::RequestInfo) != 0)
		{
			DrawRequestDebugInfo(*pAuxGeom, posX, posY);
			posX += 600.0f;
		}
	}

	g_system.ScheduleIRenderAuxGeomForRendering(pAuxGeom);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::HandleRetriggerControls()
{
	for (auto const pObject : g_constructedObjects)
	{
		pObject->ForceImplementationRefresh();
	}

	ForceGlobalDataImplRefresh();
	g_previewObject.ForceImplementationRefresh();

	if ((g_systemStates& ESystemStates::IsMuted) != ESystemStates::None)
	{
		ExecuteDefaultTrigger(EDefaultTriggerType::MuteAll);
	}

	if ((g_systemStates& ESystemStates::IsPaused) != ESystemStates::None)
	{
		ExecuteDefaultTrigger(EDefaultTriggerType::PauseAll);
	}
}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}      // namespace CryAudio
