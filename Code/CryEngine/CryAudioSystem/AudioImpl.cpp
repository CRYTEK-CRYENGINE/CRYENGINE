// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"

using namespace CryAudio::Impl;
using namespace CryAudio::Impl::Null;

///////////////////////////////////////////////////////////////////////////
CAudioImpl::CAudioImpl()
{
}

///////////////////////////////////////////////////////////////////////////
CAudioImpl::~CAudioImpl()
{
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::Update(float const deltaTime)
{
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Init()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ShutDown()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Release()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::OnLoseFocus()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::OnGetFocus()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::MuteAll()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnmuteAll()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopAllSounds()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterAudioObject(IAudioObject* const pObjectData, char const* const szAudioObjectName)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnregisterAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ResetAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UpdateAudioObject(IAudioObject* const pObject)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PlayFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PrepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnprepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PrepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger, IAudioEvent* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnprepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger, IAudioEvent* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ActivateTrigger(IAudioObject* const pAudioObject, IAudioTrigger const* const pTrigger, IAudioEvent* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopEvent(IAudioObject* const pAudioObject, IAudioEvent const* const pEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopAllEvents(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Set3DAttributes(IAudioObject* const pAudioObject, SAudioObject3DAttributes const& attributes)
{
	return eAudioRequestStatus_Success;
}
///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetEnvironment(IAudioObject* const pAudioObjectData, IAudioEnvironment const* const pEnvironmentImplData, float const amount)
{
	return eAudioRequestStatus_Success;
}
///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetRtpc(IAudioObject* const pAudioObjectData, IAudioRtpc const* const pRtpcData, float const value)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetSwitchState(IAudioObject* const pAudioObject, IAudioSwitchState const* const pState)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetObstructionOcclusion(IAudioObject* const pAudioObjectData, float const obstruction, float const occlusion)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetListener3DAttributes(IAudioListener* const pAudioListener, SAudioObject3DAttributes const& attributes)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnregisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ParseAudioFileEntry(XmlNodeRef const pAudioFileEntryNode, SAudioFileEntryInfo* const pFileEntryInfo)
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
void CAudioImpl::DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry)
{
}

//////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl::GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl::NewGlobalAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(IAudioObject, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl::NewAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(IAudioObject, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioObject(IAudioObject const* const pOldObject)
{
	POOL_FREE_CONST(pOldObject);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioListener(IAudioListener* const pOldListener)
{
	POOL_FREE(pOldListener);
}

///////////////////////////////////////////////////////////////////////////
IAudioListener* CAudioImpl::NewDefaultAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(IAudioListener, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioListener* CAudioImpl::NewAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(IAudioListener, pNewObject)();
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioEvent* CAudioImpl::NewAudioEvent(AudioEventId const audioEventID)
{
	POOL_NEW_CREATE(IAudioEvent, pNewEvent)();
	return pNewEvent;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioEvent(IAudioEvent const* const pOldEventImpl)
{
	POOL_FREE_CONST(pOldEventImpl);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::ResetAudioEvent(IAudioEvent* const pEventData)
{
}

//////////////////////////////////////////////////////////////////////////
IAudioStandaloneFile* CAudioImpl::NewAudioStandaloneFile()
{
	POOL_NEW_CREATE(IAudioStandaloneFile, pNewAudioStandaloneFile);
	return pNewAudioStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioStandaloneFile(IAudioStandaloneFile const* const _pOldAudioStandaloneFileData)
{
	POOL_FREE_CONST(_pOldAudioStandaloneFileData);
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::ResetAudioStandaloneFile(IAudioStandaloneFile* const _pAudioStandaloneFileData)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioTrigger const* CAudioImpl::NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioTrigger(IAudioTrigger const* const pOldTriggerImpl)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioRtpc const* CAudioImpl::NewAudioRtpc(XmlNodeRef const pAudioRtpcNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioRtpc(IAudioRtpc const* const pOldRtpcImpl)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioSwitchState const* CAudioImpl::NewAudioSwitchState(XmlNodeRef const pAudioSwitchNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchStateImplData)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioEnvironment const* CAudioImpl::NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioEnvironment(IAudioEnvironment const* const pOldEnvironmentImpl)
{
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const
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
void CAudioImpl::OnAudioSystemRefresh()
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::SetLanguage(char const* const szLanguage)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const
{
}
