// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>
#include <IAudioSystemImplementation.h>
#include <CryString/CryName.h>

enum EAudioRequestInfoFlags : AudioEnumFlagsType
{
	eAudioRequestInfoFlags_None              = 0,
	eAudioRequestInfoFlags_WaitingForRemoval = BIT(0),
};

enum EAudioStandaloneFileState : AudioEnumFlagsType
{
	eAudioStandaloneFileState_None,
	eAudioStandaloneFileState_Playing,
	eAudioStandaloneFileState_Stopping,
	eAudioStandaloneFileState_Loading,
};

//////////////////////////////////////////////////////////////////////////
struct SAudioEventListener
{
	SAudioEventListener()
		: pObjectToListenTo(nullptr)
		, OnEvent(nullptr)
		, requestType(eAudioRequestType_AudioAllRequests)
		, specificRequestMask(ALL_AUDIO_REQUEST_SPECIFIC_TYPE_FLAGS)
	{}

	void const*        pObjectToListenTo;
	void               (* OnEvent)(SAudioRequestInfo const* const);

	EAudioRequestType  requestType;
	AudioEnumFlagsType specificRequestMask;
};

//////////////////////////////////////////////////////////////////////////
struct SAudioRequestDataInternal : public _i_multithread_reference_target_t
{
	explicit SAudioRequestDataInternal(EAudioRequestType const _type)
		: type(_type)
	{}

	virtual ~SAudioRequestDataInternal() {}

	virtual void Release();

	EAudioRequestType const type;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
struct SAudioManagerRequestDataInternalBase : public SAudioRequestDataInternal
{
	explicit SAudioManagerRequestDataInternalBase(EAudioManagerRequestType const _type)
		: SAudioRequestDataInternal(eAudioRequestType_AudioManagerRequest)
		, type(_type)
	{}

	virtual ~SAudioManagerRequestDataInternalBase() {}

	EAudioManagerRequestType const type;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternalBase);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternalBase);
};

//////////////////////////////////////////////////////////////////////////
template<EAudioManagerRequestType T>
struct SAudioManagerRequestDataInternal : public SAudioManagerRequestDataInternalBase
{
	SAudioManagerRequestDataInternal()
		: SAudioManagerRequestDataInternalBase(T)
	{}

	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestDataInternal<T> const* const pAMRDataInternal)
		: SAudioManagerRequestDataInternalBase(T)
	{}

	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<T> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(T)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_SetAudioImpl> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_SetAudioImpl> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_SetAudioImpl)
		, pImpl(pAMRData->pImpl)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	CryAudio::Impl::IAudioImpl* const pImpl;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_ReserveAudioObjectId> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_ReserveAudioObjectId> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_ReserveAudioObjectId)
		, pAudioObjectId(pAMRData->pAudioObjectId)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		, audioObjectName(pAMRData->szAudioObjectName)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	AudioObjectId* const pAudioObjectId;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> audioObjectName;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_AddRequestListener> : public SAudioManagerRequestDataInternalBase
{
	// Used when constructed internally.
	explicit SAudioManagerRequestDataInternal(
	  void const* const _pObjectToListenTo,
	  void(*_func)(SAudioRequestInfo const* const),
	  EAudioRequestType const _requestType,
	  AudioEnumFlagsType const _specificRequestMask)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_AddRequestListener)
		, pObjectToListenTo(_pObjectToListenTo)
		, func(_func)
		, requestType(_requestType)
		, specificRequestMask(_specificRequestMask)
	{}

	// Used when allocated internally.
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestDataInternal<eAudioManagerRequestType_AddRequestListener> const* const pAMRDataInternal)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_AddRequestListener)
		, pObjectToListenTo(pAMRDataInternal->pObjectToListenTo)
		, func(pAMRDataInternal->func)
		, requestType(pAMRDataInternal->requestType)
		, specificRequestMask(pAMRDataInternal->specificRequestMask)
	{}

	// Used when copied from external to internal request.
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_AddRequestListener> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_AddRequestListener)
		, pObjectToListenTo(pAMRData->pObjectToListenTo)
		, func(pAMRData->func)
		, requestType(pAMRData->type)
		, specificRequestMask(pAMRData->specificRequestMask)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	void const* const        pObjectToListenTo;
	void                     (* func)(SAudioRequestInfo const* const);
	EAudioRequestType const  requestType;
	AudioEnumFlagsType const specificRequestMask;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_RemoveRequestListener> : public SAudioManagerRequestDataInternalBase
{
	// Used when constructed internally.
	explicit SAudioManagerRequestDataInternal(void const* const _pObjectToListenTo, void(*_func)(SAudioRequestInfo const* const))
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_RemoveRequestListener)
		, pObjectToListenTo(_pObjectToListenTo)
		, func(_func)
	{}

	// Used when allocated internally.
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestDataInternal<eAudioManagerRequestType_RemoveRequestListener> const* const pAMRDataInternal)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_RemoveRequestListener)
		, pObjectToListenTo(pAMRDataInternal->pObjectToListenTo)
		, func(pAMRDataInternal->func)
	{}

	// Used when copied from external to internal request.
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_RemoveRequestListener> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_RemoveRequestListener)
		, pObjectToListenTo(pAMRData->pObjectToListenTo)
		, func(pAMRData->func)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	void const* const pObjectToListenTo;
	void              (* func)(SAudioRequestInfo const* const);

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_ParseControlsData> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_ParseControlsData> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_ParseControlsData)
		, folderPath(pAMRData->szFolderPath)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const folderPath;
	EAudioDataScope const                             dataScope;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_ParsePreloadsData> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_ParsePreloadsData> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_ParsePreloadsData)
		, folderPath(pAMRData->szFolderPath)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const folderPath;
	EAudioDataScope const                             dataScope;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_ClearControlsData> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_ClearControlsData> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_ClearControlsData)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	EAudioDataScope const dataScope;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_ClearPreloadsData> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_ClearPreloadsData> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_ClearPreloadsData)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	EAudioDataScope const dataScope;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_PreloadSingleRequest> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_PreloadSingleRequest> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_PreloadSingleRequest)
		, audioPreloadRequestId(pAMRData->audioPreloadRequestId)
		, bAutoLoadOnly(pAMRData->bAutoLoadOnly)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	AudioPreloadRequestId const audioPreloadRequestId;
	bool const                  bAutoLoadOnly;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_UnloadSingleRequest> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_UnloadSingleRequest> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_UnloadSingleRequest)
		, audioPreloadRequestId(pAMRData->audioPreloadRequestId)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	AudioPreloadRequestId const audioPreloadRequestId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_UnloadAFCMDataByScope> : public SAudioManagerRequestDataInternalBase
{
	// Used when constructed internally.
	explicit SAudioManagerRequestDataInternal(EAudioDataScope const _dataScope)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_UnloadAFCMDataByScope)
		, dataScope(_dataScope)
	{}

	// Used when allocated internally.
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestDataInternal<eAudioManagerRequestType_UnloadAFCMDataByScope> const* const pAMRDataInternal)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_UnloadAFCMDataByScope)
		, dataScope(pAMRDataInternal->dataScope)
	{}

	// Used when copied from external to internal request.
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_UnloadAFCMDataByScope> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_UnloadAFCMDataByScope)
		, dataScope(pAMRData->dataScope)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	EAudioDataScope const dataScope;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_RefreshAudioSystem> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_RefreshAudioSystem> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_RefreshAudioSystem)
		, levelName(pAMRData->szLevelName)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	CryFixedStringT<MAX_AUDIO_FILE_NAME_LENGTH> const levelName;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_ReloadControlsData> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestData<eAudioManagerRequestType_ReloadControlsData> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_ReloadControlsData)
		, folderPath(pAMRData->szFolderPath)
		, levelName(pAMRData->szLevelName)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const folderPath;
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const levelName;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioManagerRequestDataInternal<eAudioManagerRequestType_GetAudioFileData> : public SAudioManagerRequestDataInternalBase
{
	explicit SAudioManagerRequestDataInternal(SAudioManagerRequestDataInternal<eAudioManagerRequestType_GetAudioFileData> const* const pAMRData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_GetAudioFileData)
		, szFilename(pAMRData->szFilename)
		, audioFileData(pAMRData->audioFileData)
	{}

	explicit SAudioManagerRequestDataInternal(char const* const _szFilename, SAudioFileData& _audioFileData)
		: SAudioManagerRequestDataInternalBase(eAudioManagerRequestType_GetAudioFileData)
		, szFilename(_szFilename)
		, audioFileData(_audioFileData)
	{}

	virtual ~SAudioManagerRequestDataInternal() {}

	char const* const szFilename;
	SAudioFileData&   audioFileData;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
struct SAudioCallbackManagerRequestDataInternalBase : public SAudioRequestDataInternal
{
	explicit SAudioCallbackManagerRequestDataInternalBase(EAudioCallbackManagerRequestType const _type)
		: SAudioRequestDataInternal(eAudioRequestType_AudioCallbackManagerRequest)
		, type(_type)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternalBase() {}

	EAudioCallbackManagerRequestType const type;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternalBase);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternalBase);
};

//////////////////////////////////////////////////////////////////////////
template<EAudioCallbackManagerRequestType T>
struct SAudioCallbackManagerRequestDataInternal : public SAudioCallbackManagerRequestDataInternalBase
{
	SAudioCallbackManagerRequestDataInternal()
		: SAudioCallbackManagerRequestDataInternalBase(T)
	{}

	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<T> const* const pAMRData)
		: SAudioCallbackManagerRequestDataInternalBase(T)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedEvent> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStartedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportStartedEvent)
		, audioEventId(pACMRData->audioEventId)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioEventId const audioEventId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportFinishedEvent> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportFinishedEvent)
		, audioEventId(pACMRData->audioEventId)
		, bSuccess(pACMRData->bSuccess)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioEventId const audioEventId;
	bool const         bSuccess;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportVirtualizedEvent> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportVirtualizedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportVirtualizedEvent)
		, audioEventId(pACMRData->audioEventId)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioEventId const audioEventId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportPhysicalizedEvent> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportPhysicalizedEvent> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportPhysicalizedEvent)
		, audioEventId(pACMRData->audioEventId)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioEventId const audioEventId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance)
		, audioTriggerId(pACMRData->audioTriggerId)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioControlId const audioTriggerId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedFile> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStartedFile> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportStartedFile)
		, audioStandaloneFileInstanceId(pACMRData->audioStandaloneFileInstanceId)
		, file(pACMRData->szFile)
		, bSuccess(pACMRData->bSuccess)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioStandaloneFileId const                       audioStandaloneFileInstanceId;
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const file;
	bool const bSuccess;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStoppedFile> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStoppedFile> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportStoppedFile)
		, audioStandaloneFileInstanceId(pACMRData->audioStandaloneFileInstanceId)
		, file(pACMRData->szFile)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioStandaloneFileId const                       audioStandaloneFileInstanceId;
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const file;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportProcessedObstructionRay> : public SAudioCallbackManagerRequestDataInternalBase
{
	explicit SAudioCallbackManagerRequestDataInternal(SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportProcessedObstructionRay> const* const pACMRData)
		: SAudioCallbackManagerRequestDataInternalBase(eAudioCallbackManagerRequestType_ReportProcessedObstructionRay)
		, rayId(pACMRData->rayId)
		, audioObjectId(pACMRData->audioObjectId)
	{}

	virtual ~SAudioCallbackManagerRequestDataInternal() {}

	AudioObjectId const audioObjectId;
	size_t const        rayId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioCallbackManagerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioCallbackManagerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
struct SAudioObjectRequestDataInternalBase : public SAudioRequestDataInternal
{
	explicit SAudioObjectRequestDataInternalBase(EAudioObjectRequestType const _type)
		: SAudioRequestDataInternal(eAudioRequestType_AudioObjectRequest)
		, type(_type)
	{}

	virtual ~SAudioObjectRequestDataInternalBase() {}

	EAudioObjectRequestType const type;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternalBase);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternalBase);
};

//////////////////////////////////////////////////////////////////////////
template<EAudioObjectRequestType T>
struct SAudioObjectRequestDataInternal : public SAudioObjectRequestDataInternalBase
{
	SAudioObjectRequestDataInternal()
		: SAudioObjectRequestDataInternalBase(T)
	{}

	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<T> const* const pAMRData)
		: SAudioObjectRequestDataInternalBase(T)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_PrepareTrigger> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_PrepareTrigger> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_PrepareTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	AudioControlId const audioTriggerId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_UnprepareTrigger> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_UnprepareTrigger> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_UnprepareTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	AudioControlId const audioTriggerId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_PlayFile> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_PlayFile> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_PlayFile)
		, file(pAORData->szFile)
		, usedAudioTriggerId(pAORData->usedAudioTriggerId)
		, bLocalized(pAORData->bLocalized)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const file;
	AudioControlId const                              usedAudioTriggerId;
	bool const bLocalized;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_StopFile> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_StopFile> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_StopFile)
		, file(pAORData->szFile)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const file;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_ExecuteTrigger> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_ExecuteTrigger> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_ExecuteTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
		, timeUntilRemovalInMS(pAORData->timeUntilRemovalInMS)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	AudioControlId const audioTriggerId;
	float const          timeUntilRemovalInMS;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_StopTrigger> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_StopTrigger> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_StopTrigger)
		, audioTriggerId(pAORData->audioTriggerId)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	AudioControlId const audioTriggerId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetTransformation> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_SetTransformation> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_SetTransformation)
		, transformation(pAORData->transformation)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	CAudioObjectTransformation const transformation;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetRtpcValue> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_SetRtpcValue)
		, audioRtpcId(pAORData->audioRtpcId)
		, value(pAORData->value)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	AudioControlId const audioRtpcId;
	float const          value;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetSwitchState> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_SetSwitchState> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_SetSwitchState)
		, audioSwitchId(pAORData->audioSwitchId)
		, audioSwitchStateId(pAORData->audioSwitchStateId)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	AudioControlId const     audioSwitchId;
	AudioSwitchStateId const audioSwitchStateId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetVolume> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_SetVolume> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_SetVolume)
		, volume(pAORData->volume)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	float const volume;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetEnvironmentAmount> : public SAudioObjectRequestDataInternalBase
{
	explicit SAudioObjectRequestDataInternal(SAudioObjectRequestData<eAudioObjectRequestType_SetEnvironmentAmount> const* const pAORData)
		: SAudioObjectRequestDataInternalBase(eAudioObjectRequestType_SetEnvironmentAmount)
		, audioEnvironmentId(pAORData->audioEnvironmentId)
		, amount(pAORData->amount)
	{}

	virtual ~SAudioObjectRequestDataInternal() {}

	AudioEnvironmentId const audioEnvironmentId;
	float const              amount;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObjectRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioObjectRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
struct SAudioListenerRequestDataInternalBase : public SAudioRequestDataInternal
{
	explicit SAudioListenerRequestDataInternalBase(EAudioListenerRequestType const _type)
		: SAudioRequestDataInternal(eAudioRequestType_AudioListenerRequest)
		, type(_type)
	{}

	virtual ~SAudioListenerRequestDataInternalBase() {}

	EAudioListenerRequestType const type;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioListenerRequestDataInternalBase);
	PREVENT_OBJECT_COPY(SAudioListenerRequestDataInternalBase);
};

//////////////////////////////////////////////////////////////////////////
template<EAudioListenerRequestType T>
struct SAudioListenerRequestDataInternal : public SAudioListenerRequestDataInternalBase
{
	SAudioListenerRequestDataInternal()
		: SAudioListenerRequestDataInternalBase(T)
	{}

	explicit SAudioListenerRequestDataInternal(SAudioListenerRequestData<T> const* const pAMRData)
		: SAudioListenerRequestDataInternalBase(T)
	{}

	virtual ~SAudioListenerRequestDataInternal() {}

	PREVENT_OBJECT_COPY(SAudioListenerRequestDataInternal);
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SAudioListenerRequestDataInternal<eAudioListenerRequestType_SetTransformation> : public SAudioListenerRequestDataInternalBase
{
	explicit SAudioListenerRequestDataInternal(SAudioListenerRequestData<eAudioListenerRequestType_SetTransformation> const* const pALRData)
		: SAudioListenerRequestDataInternalBase(eAudioListenerRequestType_SetTransformation)
		, transformation(pALRData->transformation)
	{}

	virtual ~SAudioListenerRequestDataInternal() {}

	CAudioObjectTransformation const transformation;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioListenerRequestDataInternal);
	PREVENT_OBJECT_COPY(SAudioListenerRequestDataInternal);
};

SAudioRequestDataInternal* ConvertToInternal(SAudioRequestDataBase const* const pExternalData);
SAudioRequestDataInternal* AllocateForInternal(SAudioRequestDataInternal const* const pRequestDataInternal);

class CAudioRequestInternal
{
public:

	CAudioRequestInternal()
		: flags(eAudioRequestFlags_None)
		, audioObjectId(INVALID_AUDIO_OBJECT_ID)
		, pOwner(nullptr)
		, pUserData(nullptr)
		, pUserDataOwner(nullptr)
		, status(eAudioRequestStatus_None)
		, internalInfoFlags(eAudioRequestInfoFlags_None)
		, pData(nullptr)
	{}

	explicit CAudioRequestInternal(SAudioRequest const& externalRequest)
		: flags(externalRequest.flags)
		, audioObjectId(externalRequest.audioObjectId)
		, pOwner(externalRequest.pOwner)
		, pUserData(externalRequest.pUserData)
		, pUserDataOwner(externalRequest.pUserDataOwner)
		, status(eAudioRequestStatus_None)
		, internalInfoFlags(eAudioRequestInfoFlags_None)
		, pData(ConvertToInternal(externalRequest.pData))
	{}

	explicit CAudioRequestInternal(SAudioRequestDataInternal const* const pRequestDataInternal)
		: flags(eAudioRequestFlags_None)
		, audioObjectId(INVALID_AUDIO_OBJECT_ID)
		, pOwner(nullptr)
		, pUserData(nullptr)
		, pUserDataOwner(nullptr)
		, status(eAudioRequestStatus_None)
		, internalInfoFlags(eAudioRequestInfoFlags_None)
		, pData(AllocateForInternal(pRequestDataInternal))
	{}

	~CAudioRequestInternal() {}

	AudioEnumFlagsType                    flags;
	AudioObjectId                         audioObjectId;
	void*                                 pOwner;
	void*                                 pUserData;
	void*                                 pUserDataOwner;
	EAudioRequestStatus                   status;
	AudioEnumFlagsType                    internalInfoFlags;
	_smart_ptr<SAudioRequestDataInternal> pData;
};

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
// Filter for drawing debug info to the screen
enum EAudioDebugDrawFilter : AudioEnumFlagsType
{
	eADDF_ALL                          = 0,
	eADDF_DRAW_SPHERES                 = BIT(6),  // a
	eADDF_SHOW_OBJECT_LABEL            = BIT(7),  // b
	eADDF_SHOW_OBJECT_TRIGGERS         = BIT(8),  // c
	eADDF_SHOW_OBJECT_STATES           = BIT(9),  // d
	eADDF_SHOW_OBJECT_RTPCS            = BIT(10), // e
	eADDF_SHOW_OBJECT_ENVIRONMENTS     = BIT(11), // f
	eADDF_DRAW_OBSTRUCTION_RAYS        = BIT(12), // g
	eADDF_SHOW_OBSTRUCTION_RAY_LABELS  = BIT(13), // h
	eADDF_DRAW_OBJECT_STANDALONE_FILES = BIT(14), // i

	eADDF_SHOW_STANDALONE_FILES        = BIT(26), // u
	eADDF_SHOW_ACTIVE_EVENTS           = BIT(27), // v
	eADDF_SHOW_ACTIVE_OBJECTS          = BIT(28), // w
	eADDF_SHOW_FILECACHE_MANAGER_INFO  = BIT(29), // x
};
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
