// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IObject.h>

namespace CryAudio
{
namespace Null
{
class CObject final : public IObject
{
public:

	CObject() = default;
	virtual ~CObject() override = default;

	virtual void     ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                             {}
	virtual void     StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                             {}
	virtual void     SetTransformation(CObjectTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                        {}
	virtual void     SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                          {}
	virtual void     SetSwitchState(ControlId const audioSwitchId, SwitchStateId const audioSwitchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override {}
	virtual void     SetEnvironment(EnvironmentId const audioEnvironmentId, float const amount, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override            {}
	virtual void     SetCurrentEnvironments(EntityId const entityToIgnore = 0, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                             {}
	virtual void     ResetEnvironments(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                     {}
	virtual void     SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                  {}
	virtual void     PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                           {}
	virtual void     StopFile(char const* const szFile, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                    {}
	virtual void     SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                     {}
	virtual EntityId GetEntityId() const override                                                                                                                                          { return INVALID_ENTITYID; }
};

class CSystem final : public IAudioSystem
{
public:

	CSystem() = default;
	CSystem(CSystem const&) = delete;
	CSystem(CSystem&&) = delete;
	CSystem&            operator=(CSystem const&) = delete;
	CSystem&            operator=(CSystem&&) = delete;

	virtual void        Release() override                                                                                                                                                      {}
	virtual void        SetImpl(Impl::IImpl* const pIImpl, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                      {}
	virtual void        LoadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                  {}
	virtual void        UnloadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                {}
	virtual void        ExecuteTriggerEx(SExecuteTriggerData const& triggerData, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                {}
	virtual void        ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                               {}
	virtual void        StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                               {}
	virtual void        SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                            {}
	virtual void        SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override             {}
	virtual void        PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                             {}
	virtual void        StopFile(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                      {}
	virtual void        ReportStartedFile(CATLStandaloneFile& standaloneFile, bool const bSuccessfullyStarted, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override  {}
	virtual void        ReportStoppedFile(CATLStandaloneFile& standaloneFile, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                   {}
	virtual void        ReportFinishedEvent(CATLEvent& event, bool const bSuccess, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                              {}
	virtual void        StopAllSounds(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                           {}
	virtual void        Refresh(char const* const szLevelName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                  {}
	virtual void        PreloadSingleRequest(PreloadRequestId const id, bool const bAutoLoadOnly, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override               {}
	virtual void        UnloadSingleRequest(PreloadRequestId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                          {}
	virtual void        ReloadControlsData(char const* const szFolderPath, char const* const szLevelName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override       {}
	virtual void        AddRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo, ESystemEvents const eventMask) override                                     {}
	virtual void        RemoveRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo) override                                                                 {}
	virtual void        ExternalUpdate() override                                                                                                                                               {}
	virtual char const* GetConfigPath() const override                                                                                                                                          { return ""; }
	virtual IListener*  CreateListener(char const* const szName = nullptr) override                                                                                                             { return nullptr; }
	virtual void        ReleaseListener(IListener* const pIListener) override                                                                                                                   {}
	virtual IObject*    CreateObject(SCreateObjectData const& objectData = SCreateObjectData::GetEmptyObject(), SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override { return static_cast<IObject*>(&m_object); }
	virtual void        ReleaseObject(IObject* const pIObject) override                                                                                                                         {}
	virtual void        GetFileData(char const* const szName, SFileData& fileData) override                                                                                                     {}
	virtual void        GetTriggerData(ControlId const triggerId, STriggerData& triggerData) override                                                                                           {}
	virtual void        OnLoadLevel(char const* const szLevelName) override                                                                                                                     {}
	virtual void        OnUnloadLevel() override                                                                                                                                                {}
	virtual void        OnLanguageChanged() override                                                                                                                                            {}
	virtual void        GetImplInfo(SImplInfo& implInfo) override                                                                                                                               {}
	virtual void        Log(ELogType const type, char const* const szFormat, ...) override                                                                                                      {}

private:

	CObject m_object;
};
} // namespace Null
} // namespace CryAudio
