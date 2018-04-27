// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"

namespace CryAudio
{
namespace Impl
{
namespace Null
{
struct SEvent final : IEvent
{
	virtual ERequestStatus Stop() override { return ERequestStatus::Success; }
};

struct SListener final : IListener
{
	virtual ERequestStatus Set3DAttributes(SObject3DAttributes const& attributes) override { return ERequestStatus::Success; }
};

struct STrigger final : ITrigger
{
	virtual ERequestStatus Load() const override                             { return ERequestStatus::Success; }
	virtual ERequestStatus Unload() const override                           { return ERequestStatus::Success; }
	virtual ERequestStatus LoadAsync(IEvent* const pIEvent) const override   { return ERequestStatus::Success; }
	virtual ERequestStatus UnloadAsync(IEvent* const pIEvent) const override { return ERequestStatus::Success; }
};

struct SObject final : IObject
{
	virtual ERequestStatus Update() override                                                                    { return ERequestStatus::Success; }
	virtual ERequestStatus Set3DAttributes(SObject3DAttributes const& attributes) override                      { return ERequestStatus::Success; }
	virtual ERequestStatus SetEnvironment(IEnvironment const* const pIEnvironment, float const amount) override { return ERequestStatus::Success; }
	virtual ERequestStatus SetParameter(IParameter const* const pIParameter, float const value) override        { return ERequestStatus::Success; }
	virtual ERequestStatus SetSwitchState(ISwitchState const* const pISwitchState) override                     { return ERequestStatus::Success; }
	virtual ERequestStatus SetObstructionOcclusion(float const obstruction, float const occlusion) override     { return ERequestStatus::Success; }
	virtual ERequestStatus ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent) override      { return ERequestStatus::Success; }
	virtual ERequestStatus StopAllTriggers() override                                                           { return ERequestStatus::Success; }
	virtual ERequestStatus PlayFile(IStandaloneFile* const pIStandaloneFile) override                           { return ERequestStatus::Success; }
	virtual ERequestStatus StopFile(IStandaloneFile* const pIStandaloneFile) override                           { return ERequestStatus::Success; }
	virtual ERequestStatus SetName(char const* const szName) override                                           { return ERequestStatus::Success; }
};

///////////////////////////////////////////////////////////////////////////
void CImpl::Update()
{
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Init(uint32 const objectPoolSize, uint32 const eventPoolSize)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnBeforeShutDown()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ShutDown()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Release()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnLoseFocus()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnGetFocus()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::MuteAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnmuteAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::PauseAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ResumeAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::StopAllSounds()
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo)
{
	pFileInfo->memoryBlockAlignment = 0;
	pFileInfo->size = 0;
	pFileInfo->bLocalized = false;
	pFileInfo->pFileData = nullptr;
	pFileInfo->pImplData = nullptr;
	pFileInfo->szFileName = nullptr;
	return ERequestStatus::Failure; // This is the correct behavior: the NULL implementation does not recognize any file nodes.
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
}

//////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetFileLocation(SFileInfo* const pFileInfo)
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetInfo(SImplInfo& implInfo) const
{
	implInfo.name = "null-impl";
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructGlobalObject()
{
	return static_cast<IObject*>(new SObject());
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(char const* const szName /*= nullptr*/)
{
	return new SObject();
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	delete pIObject;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	delete pIListener;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(char const* const szName /*= nullptr*/)
{
	return static_cast<IListener*>(new SListener());
}

///////////////////////////////////////////////////////////////////////////
IEvent* CImpl::ConstructEvent(CATLEvent& event)
{
	return static_cast<IEvent*>(new SEvent());
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEvent(IEvent const* const pIEvent)
{
	delete pIEvent;
}

//////////////////////////////////////////////////////////////////////////
IStandaloneFile* CImpl::ConstructStandaloneFile(CATLStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITrigger const* pITrigger /*= nullptr*/)
{
	return new IStandaloneFile();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructStandaloneFile(IStandaloneFile const* const pIStandaloneFile)
{
	delete pIStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadConnected(DeviceId const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadDisconnected(DeviceId const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
ITrigger const* CImpl::ConstructTrigger(XmlNodeRef const pRootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTrigger(ITrigger const* const pITrigger)
{
}

///////////////////////////////////////////////////////////////////////////
IParameter const* CImpl::ConstructParameter(XmlNodeRef const pRootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameter(IParameter const* const pIParameter)
{
}

///////////////////////////////////////////////////////////////////////////
ISwitchState const* CImpl::ConstructSwitchState(XmlNodeRef const pRootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchState(ISwitchState const* const pISwitchState)
{
}

///////////////////////////////////////////////////////////////////////////
IEnvironment const* CImpl::ConstructEnvironment(XmlNodeRef const pRootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironment(IEnvironment const* const pIEnvironment)
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::GetMemoryInfo(SMemoryInfo& memoryInfo) const
{
	ZeroStruct(memoryInfo);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetFileData(char const* const szName, SFileData& fileData) const
{
}
} // namespace Null
} // namespace Impl
} // namespace CryAudio
