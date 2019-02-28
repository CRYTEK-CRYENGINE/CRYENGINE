// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>
#include <CryString/CryName.h>

namespace CryAudio
{
class CATLListener;
class CATLAudioObject;
class CAudioRayInfo;

enum class EAudioRequestType : EnumFlagsType
{
	None,
	AudioManagerRequest,
	AudioCallbackManagerRequest,
	AudioObjectRequest,
	AudioListenerRequest,
};

enum class EAudioStandaloneFileState : EnumFlagsType
{
	None,
	Playing,
	Stopping,
	Loading,
};

enum class EAudioManagerRequestType : EnumFlagsType
{
	None,
	SetAudioImpl,
	ReleaseAudioImpl,
	RefreshAudioSystem,
	StopAllSounds,
	ParseControlsData,
	ParsePreloadsData,
	ClearControlsData,
	ClearPreloadsData,
	PreloadSingleRequest,
	UnloadSingleRequest,
	UnloadAFCMDataByScope,
	DrawDebugInfo,
	AddRequestListener,
	RemoveRequestListener,
	ChangeLanguage,
	RetriggerAudioControls,
	ReleasePendingRays,
	ReloadControlsData,
	GetAudioFileData,
	GetImplInfo,
};

enum class EAudioCallbackManagerRequestType : EnumFlagsType
{
	None,
	ReportStartedEvent, //!< Only relevant for delayed playback.
	ReportFinishedEvent,
	ReportFinishedTriggerInstance,
	ReportStartedFile,
	ReportStoppedFile,
	ReportVirtualizedEvent,
	ReportPhysicalizedEvent,
};

enum class EAudioObjectRequestType : EnumFlagsType
{
	None,
	LoadTrigger,
	UnloadTrigger,
	PlayFile,
	StopFile,
	ExecuteTrigger,
	ExecuteTriggerEx,
	StopTrigger,
	StopAllTriggers,
	SetTransformation,
	SetParameter,
	SetSwitchState,
	SetCurrentEnvironments,
	SetEnvironment,
	ResetEnvironments,
	RegisterObject,
	ReleaseObject,
	ProcessPhysicsRay,
	SetName,
};

enum class EAudioListenerRequestType : EnumFlagsType
{
	None,
	SetTransformation,
	RegisterListener,
	ReleaseListener,
	SetName,
};

//////////////////////////////////////////////////////////////////////////
struct SAudioEventListener
{
	SAudioEventListener()
		: pObjectToListenTo(nullptr)
		, OnEvent(nullptr)
		, eventMask(ESystemEvents::None)
	{}

	void const*   pObjectToListenTo;
	void          (* OnEvent)(SRequestInfo const* const);
	ESystemEvents eventMask;
};

//////////////////////////////////////////////////////////////////////////
struct SAudioRequestData : public _i_multithread_reference_target_t
{
	explicit SAudioRequestData(EAudioRequestType const type_)
		: type(type_)
	{}

	virtual ~SAudioRequestData() override = default;

	SAudioRequestData(SAudioRequestData const&) = delete;
	SAudioRequestData(SAudioRequestData&&) = delete;
	SAudioRequestData& operator=(SAudioRequestData const&) = delete;
	SAudioRequestData& operator=(SAudioRequestData&&) = delete;

	EAudioRequestType const type;
};

//////////////////////////////////////////////////////////////////////////
struct SAudioManagerRequestDataBase : public SAudioRequestData
{
	explicit SAudioManagerRequestDataBase(EAudioManagerRequestType const type_)
		: SAudioRequestData(EAudioRequestType::AudioManagerRequest)
		, type(type_)
	{}

	virtual ~SAudioManagerRequestDataBase() override = default;

	EAudioManagerRequestType const type;
};

//////////////////////////////////////////////////////////////////////////
template<EAudioManagerRequestType T>
struct SAudioManagerRequestData final : public SAudioManagerRequestDataBase
{
	SAudioManagerRequestData()
		: SAudioManagerRequestDataBase(T)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<T> const* const pAMRData)
		: SAudioManagerRequestDataBase(T)
	{}

	virtual ~SAudioManagerRequestData() override = default;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::SetAudioImpl> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(Impl::IImpl* const pIImpl_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::SetAudioImpl)
		, pIImpl(pIImpl_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::SetAudioImpl> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::SetAudioImpl)
		, pIImpl(pAMRData->pIImpl)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	Impl::IImpl* const pIImpl;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::AddRequestListener> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(
	  void const* const pObjectToListenTo_,
	  void(*func_)(SRequestInfo const* const),
	  ESystemEvents const eventMask_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::AddRequestListener)
		, pObjectToListenTo(pObjectToListenTo_)
		, func(func_)
		, eventMask(eventMask_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::AddRequestListener> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::AddRequestListener)
		, pObjectToListenTo(pAMRData->pObjectToListenTo)
		, func(pAMRData->func)
		, eventMask(pAMRData->eventMask)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	void const* const   pObjectToListenTo;
	void                (* func)(SRequestInfo const* const);
	ESystemEvents const eventMask;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::RemoveRequestListener> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(void const* const pObjectToListenTo_, void(*func_)(SRequestInfo const* const))
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::RemoveRequestListener)
		, pObjectToListenTo(pObjectToListenTo_)
		, func(func_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::RemoveRequestListener> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::RemoveRequestListener)
		, pObjectToListenTo(pAMRData->pObjectToListenTo)
		, func(pAMRData->func)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	void const* const pObjectToListenTo;
	void              (* func)(SRequestInfo const* const);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::ParseControlsData> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(char const* const szFolderPath_, EDataScope const dataScope_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ParseControlsData)
		, folderPath(szFolderPath_)
		, dataScope(dataScope_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::ParseControlsData> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ParseControlsData)
		, folderPath(pAMRData->folderPath)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	CryFixedStringT<MaxFilePathLength> const folderPath;
	EDataScope const                         dataScope;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::ParsePreloadsData> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(char const* const szFolderPath_, EDataScope const dataScope_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ParsePreloadsData)
		, folderPath(szFolderPath_)
		, dataScope(dataScope_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::ParsePreloadsData> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ParsePreloadsData)
		, folderPath(pAMRData->folderPath)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	CryFixedStringT<MaxFilePathLength> const folderPath;
	EDataScope const                         dataScope;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::ClearControlsData> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(EDataScope const dataScope_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ClearControlsData)
		, dataScope(dataScope_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::ClearControlsData> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ClearControlsData)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	EDataScope const dataScope;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::ClearPreloadsData> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(EDataScope const dataScope_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ClearPreloadsData)
		, dataScope(dataScope_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::ClearPreloadsData> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ClearPreloadsData)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	EDataScope const dataScope;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::PreloadSingleRequest> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(PreloadRequestId const audioPreloadRequestId_, bool const bAutoLoadOnly_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::PreloadSingleRequest)
		, audioPreloadRequestId(audioPreloadRequestId_)
		, bAutoLoadOnly(bAutoLoadOnly_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::PreloadSingleRequest> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::PreloadSingleRequest)
		, audioPreloadRequestId(pAMRData->audioPreloadRequestId)
		, bAutoLoadOnly(pAMRData->bAutoLoadOnly)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	PreloadRequestId const audioPreloadRequestId;
	bool const             bAutoLoadOnly;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::UnloadSingleRequest> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(PreloadRequestId const audioPreloadRequestId_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::UnloadSingleRequest)
		, audioPreloadRequestId(audioPreloadRequestId_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::UnloadSingleRequest> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::UnloadSingleRequest)
		, audioPreloadRequestId(pAMRData->audioPreloadRequestId)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	PreloadRequestId const audioPreloadRequestId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::UnloadAFCMDataByScope> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(EDataScope const dataScope_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::UnloadAFCMDataByScope)
		, dataScope(dataScope_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::UnloadAFCMDataByScope> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::UnloadAFCMDataByScope)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	EDataScope const dataScope;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::RefreshAudioSystem> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(char const* const szLevelName)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::RefreshAudioSystem)
		, levelName(szLevelName)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::RefreshAudioSystem> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::RefreshAudioSystem)
		, levelName(pAMRData->levelName)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	CryFixedStringT<MaxFileNameLength> const levelName;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::ReloadControlsData> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(char const* const szFolderPath, char const* const szLevelName)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ReloadControlsData)
		, folderPath(szFolderPath)
		, levelName(szLevelName)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::ReloadControlsData> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::ReloadControlsData)
		, folderPath(pAMRData->folderPath)
		, levelName(pAMRData->levelName)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	CryFixedStringT<MaxFilePathLength> const folderPath;
	CryFixedStringT<MaxFilePathLength> const levelName;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::GetAudioFileData> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(char const* const szName, SFileData& fileData_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::GetAudioFileData)
		, name(szName)
		, fileData(fileData_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::GetAudioFileData> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::GetAudioFileData)
		, name(pAMRData->name)
		, fileData(pAMRData->fileData)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	CryFixedStringT<MaxFileNameLength> const name;
	SFileData&                               fileData;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestData<EAudioManagerRequestType::GetImplInfo> final : public SAudioManagerRequestDataBase
{
	explicit SAudioManagerRequestData(SImplInfo& implInfo_)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::GetImplInfo)
		, implInfo(implInfo_)
	{}

	explicit SAudioManagerRequestData(SAudioManagerRequestData<EAudioManagerRequestType::GetImplInfo> const* const pAMRData)
		: SAudioManagerRequestDataBase(EAudioManagerRequestType::GetImplInfo)
		, implInfo(pAMRData->implInfo)
	{}

	virtual ~SAudioManagerRequestData() override = default;

	SImplInfo& implInfo;
};

//////////////////////////////////////////////////////////////////////////
struct SAudioCallbackManagerRequestDataBase : public SAudioRequestData
{
	explicit SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType const type_)
		: SAudioRequestData(EAudioRequestType::AudioCallbackManagerRequest)
		, type(type_)
	{}

	virtual ~SAudioCallbackManagerRequestDataBase() override = default;

	EAudioCallbackManagerRequestType const type;
};

//////////////////////////////////////////////////////////////////////////
template<EAudioCallbackManagerRequestType T>
struct SAudioCallbackManagerRequestData final : public SAudioCallbackManagerRequestDataBase
{
	SAudioCallbackManagerRequestData()
		: SAudioCallbackManagerRequestDataBase(T)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<T> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(T)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedEvent> final : public SAudioCallbackManagerRequestDataBase
{
	explicit SAudioCallbackManagerRequestData(CATLEvent& audioEvent_)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportStartedEvent)
		, audioEvent(audioEvent_)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportStartedEvent)
		, audioEvent(pACMRData->audioEvent)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;

	CATLEvent& audioEvent;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedEvent> final : public SAudioCallbackManagerRequestDataBase
{
	explicit SAudioCallbackManagerRequestData(CATLEvent& audioEvent_, bool const bSuccess_)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportFinishedEvent)
		, audioEvent(audioEvent_)
		, bSuccess(bSuccess_)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportFinishedEvent)
		, audioEvent(pACMRData->audioEvent)
		, bSuccess(pACMRData->bSuccess)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;

	CATLEvent& audioEvent;
	bool const bSuccess;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportVirtualizedEvent> final : public SAudioCallbackManagerRequestDataBase
{
	explicit SAudioCallbackManagerRequestData(CATLEvent& audioEvent_)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportVirtualizedEvent)
		, audioEvent(audioEvent_)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportVirtualizedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportVirtualizedEvent)
		, audioEvent(pACMRData->audioEvent)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;

	CATLEvent& audioEvent;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportPhysicalizedEvent> final : public SAudioCallbackManagerRequestDataBase
{
	explicit SAudioCallbackManagerRequestData(CATLEvent& audioEvent_)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportPhysicalizedEvent)
		, audioEvent(audioEvent_)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportPhysicalizedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportPhysicalizedEvent)
		, audioEvent(pACMRData->audioEvent)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;

	CATLEvent& audioEvent;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance> final : public SAudioCallbackManagerRequestDataBase
{
	explicit SAudioCallbackManagerRequestData(ControlId const audioTriggerId_)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance)
		, audioTriggerId(audioTriggerId_)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance)
		, audioTriggerId(pACMRData->audioTriggerId)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;

	ControlId const audioTriggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedFile> final : public SAudioCallbackManagerRequestDataBase
{
	explicit SAudioCallbackManagerRequestData(CATLStandaloneFile& audioStandaloneFile_, bool const bSuccess_)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportStartedFile)
		, audioStandaloneFile(audioStandaloneFile_)
		, bSuccess(bSuccess_)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedFile> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportStartedFile)
		, audioStandaloneFile(pACMRData->audioStandaloneFile)
		, bSuccess(pACMRData->bSuccess)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;

	CATLStandaloneFile& audioStandaloneFile;
	bool const          bSuccess;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStoppedFile> final : public SAudioCallbackManagerRequestDataBase
{
	explicit SAudioCallbackManagerRequestData(CATLStandaloneFile& audioStandaloneFile_)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportStoppedFile)
		, audioStandaloneFile(audioStandaloneFile_)
	{}

	explicit SAudioCallbackManagerRequestData(SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStoppedFile> const* const pACMRData)
		: SAudioCallbackManagerRequestDataBase(EAudioCallbackManagerRequestType::ReportStoppedFile)
		, audioStandaloneFile(pACMRData->audioStandaloneFile)
	{}

	virtual ~SAudioCallbackManagerRequestData() override = default;

	CATLStandaloneFile& audioStandaloneFile;
};

//////////////////////////////////////////////////////////////////////////
struct SAudioObjectRequestDataBase : public SAudioRequestData
{
	explicit SAudioObjectRequestDataBase(EAudioObjectRequestType const type_)
		: SAudioRequestData(EAudioRequestType::AudioObjectRequest)
		, type(type_)
	{}

	virtual ~SAudioObjectRequestDataBase() override = default;

	EAudioObjectRequestType const type;
};

//////////////////////////////////////////////////////////////////////////
template<EAudioObjectRequestType T>
struct SAudioObjectRequestData final : public SAudioObjectRequestDataBase
{
	SAudioObjectRequestData()
		: SAudioObjectRequestDataBase(T)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<T> const* const pAORData)
		: SAudioObjectRequestDataBase(T)
	{}

	virtual ~SAudioObjectRequestData() override = default;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::LoadTrigger> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(ControlId const audioTriggerId_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::LoadTrigger)
		, audioTriggerId(audioTriggerId_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::LoadTrigger> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::LoadTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	ControlId const audioTriggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::UnloadTrigger> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(ControlId const audioTriggerId_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::UnloadTrigger)
		, audioTriggerId(audioTriggerId_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::UnloadTrigger> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::UnloadTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	ControlId const audioTriggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::PlayFile> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(
	  CryFixedStringT<MaxFilePathLength> const& file_,
	  ControlId const usedAudioTriggerId_,
	  bool const bLocalized_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::PlayFile)
		, file(file_)
		, usedAudioTriggerId(usedAudioTriggerId_)
		, bLocalized(bLocalized_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::PlayFile> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::PlayFile)
		, file(pAORData->file)
		, usedAudioTriggerId(pAORData->usedAudioTriggerId)
		, bLocalized(pAORData->bLocalized)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	CryFixedStringT<MaxFilePathLength> const file;
	ControlId const                          usedAudioTriggerId;
	bool const                               bLocalized;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::StopFile> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(CryFixedStringT<MaxFilePathLength> const& file_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::StopFile)
		, file(file_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::StopFile> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::StopFile)
		, file(pAORData->file)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	CryFixedStringT<MaxFilePathLength> const file;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(ControlId const audioTriggerId_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::ExecuteTrigger)
		, audioTriggerId(audioTriggerId_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::ExecuteTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	ControlId const audioTriggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTriggerEx> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(SExecuteTriggerData const& data)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::ExecuteTriggerEx)
		, name(data.szName)
		, occlusionType(data.occlusionType)
		, transformation(data.transformation)
		, entityId(data.entityId)
		, setCurrentEnvironments(data.setCurrentEnvironments)
		, triggerId(data.triggerId)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTriggerEx> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::ExecuteTriggerEx)
		, name(pAORData->name)
		, occlusionType(pAORData->occlusionType)
		, transformation(pAORData->transformation)
		, entityId(pAORData->entityId)
		, setCurrentEnvironments(pAORData->setCurrentEnvironments)
		, triggerId(pAORData->triggerId)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	CryFixedStringT<MaxObjectNameLength> const name;
	EOcclusionType const                       occlusionType;
	CObjectTransformation const                transformation;
	EntityId const                             entityId;
	bool const setCurrentEnvironments;
	ControlId const                            triggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::StopTrigger> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(ControlId const audioTriggerId_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::StopTrigger)
		, audioTriggerId(audioTriggerId_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::StopTrigger> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::StopTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	ControlId const audioTriggerId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::SetTransformation> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(CObjectTransformation const& transformation_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetTransformation)
		, transformation(transformation_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::SetTransformation> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetTransformation)
		, transformation(pAORData->transformation)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	CObjectTransformation const transformation;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(ControlId const parameterId_, float const value_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetParameter)
		, parameterId(parameterId_)
		, value(value_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetParameter)
		, parameterId(pAORData->parameterId)
		, value(pAORData->value)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	ControlId const parameterId;
	float const     value;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::SetSwitchState> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(ControlId const audioSwitchId_, SwitchStateId const audioSwitchStateId_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetSwitchState)
		, audioSwitchId(audioSwitchId_)
		, audioSwitchStateId(audioSwitchStateId_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::SetSwitchState> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetSwitchState)
		, audioSwitchId(pAORData->audioSwitchId)
		, audioSwitchStateId(pAORData->audioSwitchStateId)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	ControlId const     audioSwitchId;
	SwitchStateId const audioSwitchStateId;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::SetCurrentEnvironments> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(EntityId const entityToIgnore_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetCurrentEnvironments)
		, entityToIgnore(entityToIgnore_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::SetCurrentEnvironments> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetCurrentEnvironments)
		, entityToIgnore(pAORData->entityToIgnore)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	EntityId const entityToIgnore;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::SetEnvironment> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(EnvironmentId const audioEnvironmentId_, float const amount_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetEnvironment)
		, audioEnvironmentId(audioEnvironmentId_)
		, amount(amount_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::SetEnvironment> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetEnvironment)
		, audioEnvironmentId(pAORData->audioEnvironmentId)
		, amount(pAORData->amount)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	EnvironmentId const audioEnvironmentId;
	float const         amount;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::RegisterObject> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(SCreateObjectData const& data)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::RegisterObject)
		, name(data.szName)
		, occlusionType(data.occlusionType)
		, transformation(data.transformation)
		, entityId(data.entityId)
		, setCurrentEnvironments(data.setCurrentEnvironments)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::RegisterObject> const* const pAMRData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::RegisterObject)
		, name(pAMRData->name)
		, occlusionType(pAMRData->occlusionType)
		, transformation(pAMRData->transformation)
		, entityId(pAMRData->entityId)
		, setCurrentEnvironments(pAMRData->setCurrentEnvironments)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	CryFixedStringT<MaxObjectNameLength> const name;
	EOcclusionType const                       occlusionType;
	CObjectTransformation const                transformation;
	EntityId const                             entityId;
	bool const setCurrentEnvironments;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::ProcessPhysicsRay> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(CAudioRayInfo* const pAudioRayInfo_)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::ProcessPhysicsRay)
		, pAudioRayInfo(pAudioRayInfo_)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::ProcessPhysicsRay> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::ProcessPhysicsRay)
		, pAudioRayInfo(pAORData->pAudioRayInfo)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	CAudioRayInfo* const pAudioRayInfo;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestData<EAudioObjectRequestType::SetName> final : public SAudioObjectRequestDataBase
{
	explicit SAudioObjectRequestData(char const* const szName)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetName)
		, name(szName)
	{}

	explicit SAudioObjectRequestData(SAudioObjectRequestData<EAudioObjectRequestType::SetName> const* const pAORData)
		: SAudioObjectRequestDataBase(EAudioObjectRequestType::SetName)
		, name(pAORData->name)
	{}

	virtual ~SAudioObjectRequestData() override = default;

	CryFixedStringT<MaxObjectNameLength> const name;
};

//////////////////////////////////////////////////////////////////////////
struct SAudioListenerRequestDataBase : public SAudioRequestData
{
	explicit SAudioListenerRequestDataBase(EAudioListenerRequestType const type_)
		: SAudioRequestData(EAudioRequestType::AudioListenerRequest)
		, type(type_)
	{}

	virtual ~SAudioListenerRequestDataBase() override = default;

	EAudioListenerRequestType const type;
};

//////////////////////////////////////////////////////////////////////////
template<EAudioListenerRequestType T>
struct SAudioListenerRequestData final : public SAudioListenerRequestDataBase
{
	SAudioListenerRequestData()
		: SAudioListenerRequestDataBase(T)
	{}

	explicit SAudioListenerRequestData(SAudioListenerRequestData<T> const* const pALRData)
		: SAudioListenerRequestDataBase(T)
	{}

	virtual ~SAudioListenerRequestData() override = default;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioListenerRequestData<EAudioListenerRequestType::SetTransformation> final : public SAudioListenerRequestDataBase
{
	explicit SAudioListenerRequestData(
	  CObjectTransformation const& transformation_,
	  CATLListener* const pListener_)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::SetTransformation)
		, transformation(transformation_)
		, pListener(pListener_)
	{}

	explicit SAudioListenerRequestData(SAudioListenerRequestData<EAudioListenerRequestType::SetTransformation> const* const pALRData)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::SetTransformation)
		, transformation(pALRData->transformation)
		, pListener(pALRData->pListener)
	{}

	virtual ~SAudioListenerRequestData() override = default;

	CObjectTransformation const transformation;
	CATLListener* const         pListener;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioListenerRequestData<EAudioListenerRequestType::RegisterListener> final : public SAudioListenerRequestDataBase
{
	explicit SAudioListenerRequestData(CATLListener** const ppListener_, char const* const szName)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::RegisterListener)
		, ppListener(ppListener_)
		, name(szName)
	{}

	explicit SAudioListenerRequestData(SAudioListenerRequestData<EAudioListenerRequestType::RegisterListener> const* const pALRData)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::RegisterListener)
		, ppListener(pALRData->ppListener)
		, name(pALRData->name)
	{}

	virtual ~SAudioListenerRequestData() override = default;

	CATLListener** const                       ppListener;
	CryFixedStringT<MaxObjectNameLength> const name;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioListenerRequestData<EAudioListenerRequestType::ReleaseListener> final : public SAudioListenerRequestDataBase
{
	explicit SAudioListenerRequestData(CATLListener* const pListener_)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::ReleaseListener)
		, pListener(pListener_)
	{}

	explicit SAudioListenerRequestData(SAudioListenerRequestData<EAudioListenerRequestType::ReleaseListener> const* const pALRData)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::ReleaseListener)
		, pListener(pALRData->pListener)
	{}

	virtual ~SAudioListenerRequestData() override = default;

	CATLListener* const pListener;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioListenerRequestData<EAudioListenerRequestType::SetName> final : public SAudioListenerRequestDataBase
{
	explicit SAudioListenerRequestData(char const* const szName, CATLListener* const pListener_)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::SetName)
		, pListener(pListener_)
		, name(szName)
	{}

	explicit SAudioListenerRequestData(SAudioListenerRequestData<EAudioListenerRequestType::SetName> const* const pALRData)
		: SAudioListenerRequestDataBase(EAudioListenerRequestType::SetName)
		, pListener(pALRData->pListener)
		, name(pALRData->name)
	{}

	virtual ~SAudioListenerRequestData() override = default;

	CATLListener* const                        pListener;
	CryFixedStringT<MaxObjectNameLength> const name;
};

SAudioRequestData* AllocateRequestData(SAudioRequestData const* const pRequestData);

class CAudioRequest
{
public:

	CAudioRequest() = default;

	explicit CAudioRequest(SAudioRequestData const* const pRequestData)
		: flags(ERequestFlags::None)
		, pObject(nullptr)
		, pOwner(nullptr)
		, pUserData(nullptr)
		, pUserDataOwner(nullptr)
		, status(ERequestStatus::None)
		, pData(AllocateRequestData(pRequestData))
	{}

	explicit CAudioRequest(
	  ERequestFlags const flags_,
	  CATLAudioObject* const pObject_,
	  void* const pOwner_,
	  void* const pUserData_,
	  void* const pUserDataOwner_,
	  SAudioRequestData const* const pRequestData_)
		: flags(flags_)
		, pObject(pObject_)
		, pOwner(pOwner_)
		, pUserData(pUserData_)
		, pUserDataOwner(pUserDataOwner_)
		, status(ERequestStatus::None)
		, pData(AllocateRequestData(pRequestData_))
	{}

	SAudioRequestData* GetData() const { return pData.get(); }

	ERequestFlags    flags = ERequestFlags::None;
	CATLAudioObject* pObject = nullptr;
	void*            pOwner = nullptr;
	void*            pUserData = nullptr;
	void*            pUserDataOwner = nullptr;
	ERequestStatus   status = ERequestStatus::None;

private:

	// Must be private as it needs "AllocateRequestData"!
	_smart_ptr<SAudioRequestData> pData = nullptr;
};

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
// Filter for drawing debug info to the screen
enum class EAudioDebugDrawFilter : EnumFlagsType
{
	All                        = 0,
	ShowSpheres                = BIT(6),  // a
	ShowObjectLabel            = BIT(7),  // b
	ShowObjectTriggers         = BIT(8),  // c
	ShowObjectStates           = BIT(9),  // d
	ShowObjectParameters       = BIT(10), // e
	ShowObjectEnvironments     = BIT(11), // f
	ShowObjectDistance         = BIT(12), // g
	ShowOcclusionRayLabels     = BIT(13), // h
	ShowOcclusionRays          = BIT(14), // i
	DrawListenerOcclusionPlane = BIT(15), // j
	ShowObjectStandaloneFiles  = BIT(16), // k

	HideMemoryInfo             = BIT(18), // m
	FilterAllObjectInfo        = BIT(19), // n

	ShowStandaloneFiles        = BIT(26), // u
	ShowActiveEvents           = BIT(27), // v
	ShowActiveObjects          = BIT(28), // w
	ShowFileCacheManagerInfo   = BIT(29), // x
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EAudioDebugDrawFilter);

static constexpr EAudioDebugDrawFilter objectDebugMask =
  EAudioDebugDrawFilter::ShowSpheres |
  EAudioDebugDrawFilter::ShowObjectLabel |
  EAudioDebugDrawFilter::ShowObjectTriggers |
  EAudioDebugDrawFilter::ShowObjectStates |
  EAudioDebugDrawFilter::ShowObjectParameters |
  EAudioDebugDrawFilter::ShowObjectEnvironments |
  EAudioDebugDrawFilter::ShowObjectDistance |
  EAudioDebugDrawFilter::ShowOcclusionRayLabels |
  EAudioDebugDrawFilter::ShowOcclusionRays |
  EAudioDebugDrawFilter::DrawListenerOcclusionPlane |
  EAudioDebugDrawFilter::ShowObjectStandaloneFiles;

#endif // INCLUDE_AUDIO_PRODUCTION_CODE

} // namespace CryAudio
