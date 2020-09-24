// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "Common/IListener.h"
#include "Common/IObject.h"
#include "Common/FileInfo.h"

namespace CryAudio
{
namespace Impl
{
namespace Null
{
struct SListener final : IListener
{
	virtual void                   Update(float const deltaTime) override                            {}
	virtual void                   SetName(char const* const szName) override                        {}
	virtual void                   SetTransformation(CTransformation const& transformation) override {}
	virtual CTransformation const& GetTransformation() const override                                { return CTransformation::GetEmptyObject(); }
};

struct SObject final : IObject
{
	virtual void                   Update(float const deltaTime) override                                                                        {}
	virtual void                   SetTransformation(CTransformation const& transformation) override                                             {}
	virtual CTransformation const& GetTransformation() const override                                                                            { return CTransformation::GetEmptyObject(); }
	virtual void                   SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners) override  {}
	virtual void                   SetOcclusionType(EOcclusionType const occlusionType) override                                                 {}
	virtual void                   StopAllTriggers() override                                                                                    {}
	virtual void                   SetName(char const* const szName) override                                                                    {}
	virtual void                   AddListener(IListener* const pIListener) override                                                             {}
	virtual void                   RemoveListener(IListener* const pIListener) override                                                          {}
	virtual void                   ToggleFunctionality(EObjectFunctionality const type, bool const enable) override                              {}
	virtual void                   DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter) override {}
};

///////////////////////////////////////////////////////////////////////////
void CImpl::Update()
{
}

///////////////////////////////////////////////////////////////////////////
bool CImpl::Init(uint16 const objectPoolSize)
{
	return true;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ShutDown()
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeRelease()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Release()
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLibraryData(XmlNodeRef const& node, ContextId const contextId)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeLibraryDataChanged()
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAfterLibraryDataChanged(int const poolAllocationMode)
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnLoseFocus()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnGetFocus()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::MuteAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::UnmuteAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::PauseAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ResumeAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::StopAllSounds()
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::ConstructFile(XmlNodeRef const& rootNode, SFileInfo* const pFileInfo)
{
	return false; // This is the correct behavior: the NULL implementation does not recognize any file nodes.
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
}

///////////////////////////////////////////////////////////////////////////
char const* CImpl::GetFileLocation(IFile* const pIFile) const
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetInfo(SImplInfo& implInfo) const
{
	cry_fixed_size_strcpy(implInfo.name, "null-impl");
	implInfo.folderName[0] = '\0';
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(CTransformation const& transformation, IListeners const& listeners, char const* const szName /*= nullptr*/)
{
	MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::Impl::Null::SObject");
	return static_cast<IObject*>(new SObject());
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
IListener* CImpl::ConstructListener(CTransformation const& transformation, char const* const szName)
{
	MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::Impl::Null::SListener");
	return static_cast<IListener*>(new SListener());
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
ITriggerConnection* CImpl::ConstructTriggerConnection(XmlNodeRef const& rootNode, float& radius)
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTriggerConnection(ITriggerConnection* const pITriggerConnection)
{
}

///////////////////////////////////////////////////////////////////////////
IParameterConnection* CImpl::ConstructParameterConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameterConnection(IParameterConnection const* const pIParameterConnection)
{
}

///////////////////////////////////////////////////////////////////////////
ISwitchStateConnection* CImpl::ConstructSwitchStateConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchStateConnection(ISwitchStateConnection const* const pISwitchStateConnection)
{
}

///////////////////////////////////////////////////////////////////////////
IEnvironmentConnection* CImpl::ConstructEnvironmentConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironmentConnection(IEnvironmentConnection const* const pIEnvironmentConnection)
{
}

//////////////////////////////////////////////////////////////////////////
ISettingConnection* CImpl::ConstructSettingConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructSettingConnection(ISettingConnection const* const pISettingConnection)
{
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
void CImpl::DrawDebugMemoryInfo(IRenderAuxGeom& auxGeom, float posX, float& posY, bool const drawDetailedInfo)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugInfoList(IRenderAuxGeom& auxGeom, float& posX, float posY, Vec3 const& camPos, float const debugDistance, char const* const szTextFilter) const
{
}
} // namespace Null
} // namespace Impl
} // namespace CryAudio
