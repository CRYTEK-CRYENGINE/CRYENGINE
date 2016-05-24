// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioSystemImpl_NULL.h"

using namespace CryAudio::Impl;

///////////////////////////////////////////////////////////////////////////
CAudioImpl_null::CAudioImpl_null()
{
}

///////////////////////////////////////////////////////////////////////////
CAudioImpl_null::~CAudioImpl_null()
{
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::Update(float const deltaTime)
{
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::Init()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::ShutDown()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::Release()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::OnLoseFocus()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::OnGetFocus()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::MuteAll()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::UnmuteAll()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::StopAllSounds()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::RegisterAudioObject(IAudioObject* const pObjectData, char const* const szAudioObjectName)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::RegisterAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::UnregisterAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::ResetAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::UpdateAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::PlayFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::StopFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::PrepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::UnprepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::PrepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger, IAudioEvent* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::UnprepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger, IAudioEvent* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::ActivateTrigger(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger, IAudioEvent* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::StopEvent(IAudioObject* const pAudioObject, IAudioEvent const* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::StopAllEvents(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::Set3DAttributes(IAudioObject* const pAudioObject, SAudioObject3DAttributes const& attributes)
{
	return eAudioRequestStatus_Success;
}
///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::SetEnvironment(IAudioObject* const pAudioObjectData, IAudioEnvironment const* const pEnvironmentImplData, float const amount)
{
	return eAudioRequestStatus_Success;
}
///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::SetRtpc(IAudioObject* const pAudioObjectData, IAudioRtpc const* const pRtpcData, float const value)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::SetSwitchState(IAudioObject* const pAudioObject, IAudioSwitchState const* const pState)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::SetObstructionOcclusion(IAudioObject* const pAudioObjectData, float const obstruction, float const occlusion)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::SetListener3DAttributes(IAudioListener* const pAudioListener, SAudioObject3DAttributes const& attributes)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::RegisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::UnregisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_null::ParseAudioFileEntry(XmlNodeRef const pAudioFileEntryNode, SAudioFileEntryInfo* const pFileEntryInfo)
{
	pFileEntryInfo->memoryBlockAlignment = 0;
	pFileEntryInfo->size = 0;
	pFileEntryInfo->bLocalized = false;
	pFileEntryInfo->pFileData = nullptr;
	pFileEntryInfo->pImplData = nullptr;
	pFileEntryInfo->szFileName = nullptr;
	return eAudioRequestStatus_Failure; // This is the correct behavior: the NULL implementation does not recognize any EntryNodes.
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry)
{
}

//////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl_null::GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl_null::NewGlobalAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(IAudioObject, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl_null::NewAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(IAudioObject, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioObject(IAudioObject const* const pOldObject)
{
	POOL_FREE_CONST(pOldObject);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioListener(IAudioListener* const pOldListener)
{
	POOL_FREE(pOldListener);
}

///////////////////////////////////////////////////////////////////////////
IAudioListener* CAudioImpl_null::NewDefaultAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(IAudioListener, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioListener* CAudioImpl_null::NewAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(IAudioListener, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioEvent* CAudioImpl_null::NewAudioEvent(AudioEventId const audioEventID)
{
	POOL_NEW_CREATE(IAudioEvent, pNewEvent)();
	return pNewEvent;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioEvent(IAudioEvent const* const pOldEventImpl)
{
	POOL_FREE_CONST(pOldEventImpl);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::ResetAudioEvent(IAudioEvent* const pEventData)
{
}

//////////////////////////////////////////////////////////////////////////
IAudioStandaloneFile* CAudioImpl_null::NewAudioStandaloneFile()
{
	POOL_NEW_CREATE(IAudioStandaloneFile, pNewAudioStandaloneFile);
	return pNewAudioStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioStandaloneFile(IAudioStandaloneFile const* const _pOldAudioStandaloneFileData)
{
	POOL_FREE_CONST(_pOldAudioStandaloneFileData);
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::ResetAudioStandaloneFile(IAudioStandaloneFile* const _pAudioStandaloneFileData)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioTrigger const* CAudioImpl_null::NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioTrigger(IAudioTrigger const* const pOldTriggerImpl)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioRtpc const* CAudioImpl_null::NewAudioRtpc(XmlNodeRef const pAudioRtpcNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioRtpc(IAudioRtpc const* const pOldRtpcImpl)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioSwitchState const* CAudioImpl_null::NewAudioSwitchState(XmlNodeRef const pAudioSwitchNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchStateImplData)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioEnvironment const* CAudioImpl_null::NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::DeleteAudioEnvironment(IAudioEnvironment const* const pOldEnvironmentImpl)
{
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const
{
	memoryInfo.primaryPoolSize = 0;
	memoryInfo.primaryPoolUsedSize = 0;
	memoryInfo.primaryPoolAllocations = 0;
	memoryInfo.secondaryPoolSize = 0;
	memoryInfo.secondaryPoolUsedSize = 0;
	memoryInfo.secondaryPoolAllocations = 0;
	memoryInfo.bucketUsedSize = 0;
	memoryInfo.bucketAllocations = 0;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::OnAudioSystemRefresh()
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::SetLanguage(char const* const szLanguage)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_null::GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const
{
}
