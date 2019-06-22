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

	virtual void ExecuteTrigger(ControlId const triggerId, EntityId const entityId = INVALID_ENTITYID, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override {}
	virtual void StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                             {}
	virtual void SetTransformation(CTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                              {}
	virtual void SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                          {}
	virtual void SetSwitchState(ControlId const audioSwitchId, SwitchStateId const audioSwitchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override {}
	virtual void SetEnvironment(EnvironmentId const audioEnvironmentId, float const amount, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override            {}
	virtual void SetCurrentEnvironments(EntityId const entityToIgnore = 0, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                             {}
	virtual void SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                  {}
	virtual void SetOcclusionRayOffset(float const occlusionRayOffset, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                 {}
	virtual void SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                     {}
	virtual void AddListener(ListenerId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                      {}
	virtual void RemoveListener(ListenerId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                   {}
	virtual void ToggleAbsoluteVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                     {}
	virtual void ToggleRelativeVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                     {}
};

class CSystem final : public IAudioSystem
{
public:

	CSystem() = default;
	CSystem(CSystem const&) = delete;
	CSystem(CSystem&&) = delete;
	CSystem&            operator=(CSystem const&) = delete;
	CSystem&            operator=(CSystem&&) = delete;

	virtual void        Release() override                                                                                                                                                                              {}
	virtual void        SetImpl(Impl::IImpl* const pIImpl, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                              {}
	virtual void        ExecuteTriggerEx(SExecuteTriggerData const& triggerData, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                        {}
	virtual void        ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                       {}
	virtual void        StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                       {}
	virtual void        SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                    {}
	virtual void        SetParameterGlobally(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                            {}
	virtual void        SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                     {}
	virtual void        SetSwitchStateGlobally(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                             {}
	virtual void        ReportStartedTriggerConnectionInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override  {}
	virtual void        ReportFinishedTriggerConnectionInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override {}
	virtual void        ReportPhysicalizedObject(Impl::IObject* const pIObject, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                         {}
	virtual void        ReportVirtualizedObject(Impl::IObject* const pIObject, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                          {}
	virtual void        StopAllSounds(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                                                   {}
	virtual void        PreloadSingleRequest(PreloadRequestId const id, bool const bAutoLoadOnly, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                       {}
	virtual void        UnloadSingleRequest(PreloadRequestId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                  {}
	virtual void        ActivateContext(ContextId const contextId) override                                                                                                                                             {}
	virtual void        DeactivateContext(ContextId const contextId) override                                                                                                                                           {}
	virtual void        LoadSetting(ControlId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                                 {}
	virtual void        UnloadSetting(ControlId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                               {}
	virtual void        ReloadControlsData(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override                                                                                              {}
	virtual void        AddRequestListener(void (*func)(SRequestInfo const* const), void* const pObjectToListenTo, ESystemEvents const eventMask) override                                                              {}
	virtual void        RemoveRequestListener(void (*func)(SRequestInfo const* const), void* const pObjectToListenTo) override                                                                                          {}
	virtual void        ExternalUpdate() override                                                                                                                                                                       {}
	virtual char const* GetConfigPath() const override                                                                                                                                                                  { return ""; }
	virtual IListener*  CreateListener(CTransformation const& transformation, char const* const szName = nullptr) override                                                                                              { return nullptr; }
	virtual void        ReleaseListener(IListener* const pIListener) override                                                                                                                                           {}
	virtual IListener*  GetListener(ListenerId const id = DefaultListenerId) override                                                                                                                                   { return nullptr; }
	virtual IObject*    CreateObject(SCreateObjectData const& objectData = SCreateObjectData::GetEmptyObject()) override                                                                                                { return static_cast<IObject*>(&m_object); }
	virtual void        ReleaseObject(IObject* const pIObject) override                                                                                                                                                 {}
	virtual void        GetTriggerData(ControlId const triggerId, STriggerData& triggerData) override                                                                                                                   {}
	virtual void        GetImplInfo(SImplInfo& implInfo) override                                                                                                                                                       {}
	virtual void        Log(ELogType const type, char const* const szFormat, ...) override                                                                                                                              {}
	virtual void        ExecutePreviewTrigger(ControlId const triggerId) override                                                                                                                                       {}
	virtual void        ExecutePreviewTriggerEx(Impl::ITriggerInfo const& triggerInfo) override                                                                                                                         {}
	virtual void        ExecutePreviewTriggerEx(XmlNodeRef const& node) override                                                                                                                                        {}
	virtual void        StopPreviewTrigger() override                                                                                                                                                                   {}

private:

	CObject m_object;
};
} // namespace Null
} // namespace CryAudio
