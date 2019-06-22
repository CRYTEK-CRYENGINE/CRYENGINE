// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RequestData.h"
#include "Common/PoolObject.h"
#include <CryAudio/IAudioSystem.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include <CrySystem/XML/IXml.h>
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
class CListener;
class CObject;

enum class ESystemRequestType : EnumFlagsType
{
	None,
	SetImpl,
	ReleaseImpl,
	StopAllSounds,
	ParseControlsData,
	ParsePreloadsData,
	ClearControlsData,
	ClearPreloadsData,
	PreloadSingleRequest,
	UnloadSingleRequest,
	ActivateContext,
	DeactivateContext,
	SetParameter,
	SetParameterGlobally,
	SetSwitchState,
	SetSwitchStateGlobally,
	AutoLoadSetting,
	LoadSetting,
	UnloadSetting,
	UnloadAFCMDataByContext,
	AddRequestListener,
	RemoveRequestListener,
	ChangeLanguage,
	ReleasePendingRays,
	GetImplInfo,
	RegisterListener,
	GetListener,
	ReleaseListener,
	RegisterObject,
	ReleaseObject,
	ExecuteTrigger,
	ExecuteTriggerEx,
	ExecuteDefaultTrigger,
	StopTrigger,
	StopAllTriggers,
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	RefreshSystem,
	ReloadControlsData,
	RetriggerControls,
	DrawDebugInfo,
	UpdateDebugInfo,
	ExecutePreviewTrigger,
	ExecutePreviewTriggerEx,
	ExecutePreviewTriggerExNode,
	StopPreviewTrigger,
	ResetRequestCount,
#endif // CRY_AUDIO_USE_DEBUG_CODE
};

//////////////////////////////////////////////////////////////////////////
struct SSystemRequestDataBase : public SRequestData
{
	explicit SSystemRequestDataBase(ESystemRequestType const systemRequestType_)
		: SRequestData(ERequestType::SystemRequest)
		, systemRequestType(systemRequestType_)
	{}

	virtual ~SSystemRequestDataBase() override = default;

	ESystemRequestType const systemRequestType;
};

//////////////////////////////////////////////////////////////////////////
template<ESystemRequestType T>
struct SSystemRequestData final : public SSystemRequestDataBase
{
	SSystemRequestData()
		: SSystemRequestDataBase(T)
	{}

	explicit SSystemRequestData(SSystemRequestData<T> const* const pSRData)
		: SSystemRequestDataBase(T)
	{}

	virtual ~SSystemRequestData() override = default;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::SetImpl> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(Impl::IImpl* const pIImpl_)
		: SSystemRequestDataBase(ESystemRequestType::SetImpl)
		, pIImpl(pIImpl_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::SetImpl> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::SetImpl)
		, pIImpl(pSRData->pIImpl)
	{}

	virtual ~SSystemRequestData() override = default;

	Impl::IImpl* const pIImpl;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::AddRequestListener> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(
		void const* const pObjectToListenTo_,
		void(*func_)(SRequestInfo const* const),
		ESystemEvents const eventMask_)
		: SSystemRequestDataBase(ESystemRequestType::AddRequestListener)
		, pObjectToListenTo(pObjectToListenTo_)
		, func(func_)
		, eventMask(eventMask_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::AddRequestListener> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::AddRequestListener)
		, pObjectToListenTo(pSRData->pObjectToListenTo)
		, func(pSRData->func)
		, eventMask(pSRData->eventMask)
	{}

	virtual ~SSystemRequestData() override = default;

	void const* const   pObjectToListenTo;
	void                (* func)(SRequestInfo const* const);
	ESystemEvents const eventMask;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::RemoveRequestListener> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(void const* const pObjectToListenTo_, void(*func_)(SRequestInfo const* const))
		: SSystemRequestDataBase(ESystemRequestType::RemoveRequestListener)
		, pObjectToListenTo(pObjectToListenTo_)
		, func(func_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::RemoveRequestListener> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::RemoveRequestListener)
		, pObjectToListenTo(pSRData->pObjectToListenTo)
		, func(pSRData->func)
	{}

	virtual ~SSystemRequestData() override = default;

	void const* const pObjectToListenTo;
	void              (* func)(SRequestInfo const* const);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ParseControlsData> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(char const* const szFolderPath, char const* const szContextName, ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::ParseControlsData)
		, folderPath(szFolderPath)
		, contextName(szContextName)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ParseControlsData> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ParseControlsData)
		, folderPath(pSRData->folderPath)
		, contextName(pSRData->contextName)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	CryFixedStringT<MaxFilePathLength> const folderPath;
	CryFixedStringT<MaxFilePathLength> const contextName;
	ContextId const                          contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ParsePreloadsData> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(char const* const szFolderPath, ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::ParsePreloadsData)
		, folderPath(szFolderPath)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ParsePreloadsData> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ParsePreloadsData)
		, folderPath(pSRData->folderPath)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	CryFixedStringT<MaxFilePathLength> const folderPath;
	ContextId const                          contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ClearControlsData> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::ClearControlsData)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ClearControlsData> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ClearControlsData)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	ContextId const contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ClearPreloadsData> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::ClearPreloadsData)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ClearPreloadsData> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ClearPreloadsData)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	ContextId const contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::PreloadSingleRequest> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(PreloadRequestId const preloadRequestId_, bool const bAutoLoadOnly_)
		: SSystemRequestDataBase(ESystemRequestType::PreloadSingleRequest)
		, preloadRequestId(preloadRequestId_)
		, bAutoLoadOnly(bAutoLoadOnly_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::PreloadSingleRequest> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::PreloadSingleRequest)
		, preloadRequestId(pSRData->preloadRequestId)
		, bAutoLoadOnly(pSRData->bAutoLoadOnly)
	{}

	virtual ~SSystemRequestData() override = default;

	PreloadRequestId const preloadRequestId;
	bool const             bAutoLoadOnly;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::UnloadSingleRequest> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(PreloadRequestId const preloadRequestId_)
		: SSystemRequestDataBase(ESystemRequestType::UnloadSingleRequest)
		, preloadRequestId(preloadRequestId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::UnloadSingleRequest> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::UnloadSingleRequest)
		, preloadRequestId(pSRData->preloadRequestId)
	{}

	virtual ~SSystemRequestData() override = default;

	PreloadRequestId const preloadRequestId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ActivateContext> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::ActivateContext)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ActivateContext> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ActivateContext)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	ContextId const contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::DeactivateContext> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::DeactivateContext)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::DeactivateContext> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::DeactivateContext)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	ContextId const contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::SetParameter> final : public SSystemRequestDataBase, public CPoolObject<SSystemRequestData<ESystemRequestType::SetParameter>, stl::PSyncMultiThread>
{
	explicit SSystemRequestData(ControlId const parameterId_, float const value_)
		: SSystemRequestDataBase(ESystemRequestType::SetParameter)
		, parameterId(parameterId_)
		, value(value_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::SetParameter> const* const pASRData)
		: SSystemRequestDataBase(ESystemRequestType::SetParameter)
		, parameterId(pASRData->parameterId)
		, value(pASRData->value)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const parameterId;
	float const     value;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::SetParameterGlobally> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ControlId const parameterId_, float const value_)
		: SSystemRequestDataBase(ESystemRequestType::SetParameterGlobally)
		, parameterId(parameterId_)
		, value(value_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::SetParameterGlobally> const* const pASRData)
		: SSystemRequestDataBase(ESystemRequestType::SetParameterGlobally)
		, parameterId(pASRData->parameterId)
		, value(pASRData->value)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const parameterId;
	float const     value;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::SetSwitchState> final : public SSystemRequestDataBase, public CPoolObject<SSystemRequestData<ESystemRequestType::SetSwitchState>, stl::PSyncMultiThread>
{
	explicit SSystemRequestData(ControlId const switchId_, SwitchStateId const switchStateId_)
		: SSystemRequestDataBase(ESystemRequestType::SetSwitchState)
		, switchId(switchId_)
		, switchStateId(switchStateId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::SetSwitchState> const* const pASRData)
		: SSystemRequestDataBase(ESystemRequestType::SetSwitchState)
		, switchId(pASRData->switchId)
		, switchStateId(pASRData->switchStateId)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const     switchId;
	SwitchStateId const switchStateId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::SetSwitchStateGlobally> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ControlId const switchId_, SwitchStateId const switchStateId_)
		: SSystemRequestDataBase(ESystemRequestType::SetSwitchStateGlobally)
		, switchId(switchId_)
		, switchStateId(switchStateId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::SetSwitchStateGlobally> const* const pASRData)
		: SSystemRequestDataBase(ESystemRequestType::SetSwitchStateGlobally)
		, switchId(pASRData->switchId)
		, switchStateId(pASRData->switchStateId)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const     switchId;
	SwitchStateId const switchStateId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::AutoLoadSetting> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::AutoLoadSetting)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::AutoLoadSetting> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::AutoLoadSetting)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	ContextId const contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::LoadSetting> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ControlId const id_)
		: SSystemRequestDataBase(ESystemRequestType::LoadSetting)
		, id(id_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::LoadSetting> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::LoadSetting)
		, id(pSRData->id)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const id;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::UnloadSetting> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ControlId const id_)
		: SSystemRequestDataBase(ESystemRequestType::UnloadSetting)
		, id(id_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::UnloadSetting> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::UnloadSetting)
		, id(pSRData->id)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const id;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::UnloadAFCMDataByContext> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ContextId const contextId_)
		: SSystemRequestDataBase(ESystemRequestType::UnloadAFCMDataByContext)
		, contextId(contextId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::UnloadAFCMDataByContext> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::UnloadAFCMDataByContext)
		, contextId(pSRData->contextId)
	{}

	virtual ~SSystemRequestData() override = default;

	ContextId const contextId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::GetImplInfo> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(SImplInfo& implInfo_)
		: SSystemRequestDataBase(ESystemRequestType::GetImplInfo)
		, implInfo(implInfo_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::GetImplInfo> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::GetImplInfo)
		, implInfo(pSRData->implInfo)
	{}

	virtual ~SSystemRequestData() override = default;

	SImplInfo& implInfo;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::RegisterListener> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(CListener** const ppListener_, CTransformation const& transformation_, char const* const szName)
		: SSystemRequestDataBase(ESystemRequestType::RegisterListener)
		, ppListener(ppListener_)
		, transformation(transformation_)
		, name(szName)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::RegisterListener> const* const pALRData)
		: SSystemRequestDataBase(ESystemRequestType::RegisterListener)
		, ppListener(pALRData->ppListener)
		, name(pALRData->name)
	{}

	virtual ~SSystemRequestData() override = default;

	CListener** const                          ppListener;
	CTransformation const                      transformation;
	CryFixedStringT<MaxObjectNameLength> const name;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::GetListener> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(CListener** const ppListener_, ListenerId const id_)
		: SSystemRequestDataBase(ESystemRequestType::GetListener)
		, ppListener(ppListener_)
		, id(id_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::GetListener> const* const pALRData)
		: SSystemRequestDataBase(ESystemRequestType::GetListener)
		, ppListener(pALRData->ppListener)
		, id(pALRData->id)
	{}

	virtual ~SSystemRequestData() override = default;

	CListener** const ppListener;
	ListenerId const  id;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ReleaseListener> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(CListener* const pListener_)
		: SSystemRequestDataBase(ESystemRequestType::ReleaseListener)
		, pListener(pListener_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ReleaseListener> const* const pALRData)
		: SSystemRequestDataBase(ESystemRequestType::ReleaseListener)
		, pListener(pALRData->pListener)
	{}

	virtual ~SSystemRequestData() override = default;

	CListener* const pListener;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::RegisterObject> final : public SSystemRequestDataBase, public CPoolObject<SSystemRequestData<ESystemRequestType::RegisterObject>, stl::PSyncMultiThread>
{
	explicit SSystemRequestData(CObject** const ppObject_, SCreateObjectData const& data)
		: SSystemRequestDataBase(ESystemRequestType::RegisterObject)
		, ppObject(ppObject_)
		, name(data.szName)
		, occlusionType(data.occlusionType)
		, transformation(data.transformation)
		, setCurrentEnvironments(data.setCurrentEnvironments)
		, listenerIds(data.listenerIds)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::RegisterObject> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::RegisterObject)
		, ppObject(pSRData->ppObject)
		, name(pSRData->name)
		, occlusionType(pSRData->occlusionType)
		, transformation(pSRData->transformation)
		, setCurrentEnvironments(pSRData->setCurrentEnvironments)
		, listenerIds(pSRData->listenerIds)
	{}

	virtual ~SSystemRequestData() override = default;

	CObject** const                            ppObject;
	CryFixedStringT<MaxObjectNameLength> const name;
	EOcclusionType const                       occlusionType;
	CTransformation const                      transformation;
	bool const setCurrentEnvironments;
	ListenerIds const                          listenerIds;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ReleaseObject> final : public SSystemRequestDataBase, public CPoolObject<SSystemRequestData<ESystemRequestType::ReleaseObject>, stl::PSyncMultiThread>
{
	explicit SSystemRequestData(CObject* const pObject_)
		: SSystemRequestDataBase(ESystemRequestType::ReleaseObject)
		, pObject(pObject_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ReleaseObject> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ReleaseObject)
		, pObject(pSRData->pObject)
	{}

	virtual ~SSystemRequestData() override = default;

	CObject* const pObject;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ExecuteTrigger> final : public SSystemRequestDataBase, public CPoolObject<SSystemRequestData<ESystemRequestType::ExecuteTrigger>, stl::PSyncMultiThread>
{
	explicit SSystemRequestData(ControlId const triggerId_)
		: SSystemRequestDataBase(ESystemRequestType::ExecuteTrigger)
		, triggerId(triggerId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ExecuteTrigger> const* const pASRData)
		: SSystemRequestDataBase(ESystemRequestType::ExecuteTrigger)
		, triggerId(pASRData->triggerId)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const triggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ExecuteTriggerEx> final : public SSystemRequestDataBase, public CPoolObject<SSystemRequestData<ESystemRequestType::ExecuteTriggerEx>, stl::PSyncMultiThread>
{
	explicit SSystemRequestData(SExecuteTriggerData const& data)
		: SSystemRequestDataBase(ESystemRequestType::ExecuteTriggerEx)
		, name(data.szName)
		, occlusionType(data.occlusionType)
		, transformation(data.transformation)
		, entityId(data.entityId)
		, setCurrentEnvironments(data.setCurrentEnvironments)
		, triggerId(data.triggerId)
		, listenerIds(data.listenerIds)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ExecuteTriggerEx> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ExecuteTriggerEx)
		, name(pSRData->name)
		, occlusionType(pSRData->occlusionType)
		, transformation(pSRData->transformation)
		, entityId(pSRData->entityId)
		, setCurrentEnvironments(pSRData->setCurrentEnvironments)
		, triggerId(pSRData->triggerId)
		, listenerIds(pSRData->listenerIds)
	{}

	virtual ~SSystemRequestData() override = default;

	CryFixedStringT<MaxObjectNameLength> const name;
	EOcclusionType const                       occlusionType;
	CTransformation const                      transformation;
	EntityId const                             entityId;
	bool const setCurrentEnvironments;
	ControlId const                            triggerId;
	ListenerIds const                          listenerIds;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(EDefaultTriggerType const triggerType_)
		: SSystemRequestDataBase(ESystemRequestType::ExecuteDefaultTrigger)
		, triggerType(triggerType_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ExecuteDefaultTrigger> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ExecuteDefaultTrigger)
		, triggerType(pSRData->triggerType)
	{}

	virtual ~SSystemRequestData() override = default;

	EDefaultTriggerType const triggerType;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::StopTrigger> final : public SSystemRequestDataBase, public CPoolObject<SSystemRequestData<ESystemRequestType::StopTrigger>, stl::PSyncMultiThread>
{
	explicit SSystemRequestData(ControlId const triggerId_)
		: SSystemRequestDataBase(ESystemRequestType::StopTrigger)
		, triggerId(triggerId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::StopTrigger> const* const pASRData)
		: SSystemRequestDataBase(ESystemRequestType::StopTrigger)
		, triggerId(pASRData->triggerId)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const triggerId;
};

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ExecutePreviewTrigger> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(ControlId const triggerId_)
		: SSystemRequestDataBase(ESystemRequestType::ExecutePreviewTrigger)
		, triggerId(triggerId_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ExecutePreviewTrigger> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ExecutePreviewTrigger)
		, triggerId(pSRData->triggerId)
	{}

	virtual ~SSystemRequestData() override = default;

	ControlId const triggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerEx> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(Impl::ITriggerInfo const& triggerInfo_)
		: SSystemRequestDataBase(ESystemRequestType::ExecutePreviewTriggerEx)
		, triggerInfo(triggerInfo_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerEx> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ExecutePreviewTriggerEx)
		, triggerInfo(pSRData->triggerInfo)
	{}

	virtual ~SSystemRequestData() override = default;

	Impl::ITriggerInfo const& triggerInfo;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerExNode> final : public SSystemRequestDataBase
{
	explicit SSystemRequestData(XmlNodeRef const& node_)
		: SSystemRequestDataBase(ESystemRequestType::ExecutePreviewTriggerExNode)
		, node(node_)
	{}

	explicit SSystemRequestData(SSystemRequestData<ESystemRequestType::ExecutePreviewTriggerExNode> const* const pSRData)
		: SSystemRequestDataBase(ESystemRequestType::ExecutePreviewTriggerExNode)
		, node(pSRData->node)
	{}

	virtual ~SSystemRequestData() override = default;

	XmlNodeRef const node;
};
#endif // CRY_AUDIO_USE_DEBUG_CODE
}      // namespace CryAudio
