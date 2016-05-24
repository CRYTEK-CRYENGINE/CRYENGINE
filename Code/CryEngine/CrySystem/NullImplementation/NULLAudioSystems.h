// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>
#include <CrySystem/TimeValue.h>

struct ICVar;

class CNULLAudioProxy : public IAudioProxy
{
public:

	CNULLAudioProxy() {}
	virtual ~CNULLAudioProxy() {}

	virtual void          Initialize(char const* const szObjectName, bool const bInitAsync = true) override                                                           {}
	virtual void          Release() override                                                                                                                          {}
	virtual void          Reset() override                                                                                                                            {}
	virtual void          PlayFile(SAudioPlayFileInfo const& playbackInfo, SAudioCallBackInfo const& callBackInfo = SAudioCallBackInfo::GetEmptyObject()) override    {}
	virtual void          StopFile(char const* const szFile) override                                                                                                 {}
	virtual void          ExecuteTrigger(AudioControlId const audioTriggerId, SAudioCallBackInfo const& callBackInfo = SAudioCallBackInfo::GetEmptyObject()) override {}
	virtual void          StopTrigger(AudioControlId const audioTriggerId) override                                                                                   {}
	virtual void          SetSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioStateId) override                                          {}
	virtual void          SetRtpcValue(AudioControlId const audioRtpcId, float const value) override                                                                  {}
	virtual void          SetOcclusionType(EAudioOcclusionType const occlusionType) override                                                                          {}
	virtual void          SetTransformation(Matrix34 const& transformation) override                                                                                  {}
	virtual void          SetPosition(Vec3 const& position) override                                                                                                  {}
	virtual void          SetEnvironmentAmount(AudioEnvironmentId const audioEnvironmentId, float const amount) override                                              {}
	virtual void          SetCurrentEnvironments(EntityId const entityToIgnore = 0) override                                                                          {}
	virtual AudioObjectId GetAudioObjectId() const override                                                                                                           { return INVALID_AUDIO_OBJECT_ID; }
};

class CNULLAudioSystem : public IAudioSystem
{
public:

	CNULLAudioSystem() {}
	~CNULLAudioSystem() {}

	virtual bool         Initialize() override                                                                                                                                                                                                                                                     { return true; }
	virtual void         Release() override                                                                                                                                                                                                                                                        { delete this; }
	virtual void         PushRequest(SAudioRequest const& audioRequest) override                                                                                                                                                                                                                   {}
	virtual void         AddRequestListener(void (* func)(SAudioRequestInfo const* const), void* const pObjectToListenTo, EAudioRequestType const requestType = eAudioRequestType_AudioAllRequests, AudioEnumFlagsType const specificRequestMask = ALL_AUDIO_REQUEST_SPECIFIC_TYPE_FLAGS) override {}
	virtual void         RemoveRequestListener(void (* func)(SAudioRequestInfo const* const), void* const pObjectToListenTo) override                                                                                                                                                              {}
	virtual void         ExternalUpdate() override                                                                                                                                                                                                                                                 {}
	virtual bool         GetAudioTriggerId(char const* const szAudioTriggerName, AudioControlId& audioTriggerId) const override                                                                                                                                                                    { return true; }
	virtual bool         GetAudioRtpcId(char const* const szAudioRtpcName, AudioControlId& audioRtpcId) const override                                                                                                                                                                             { return true; }
	virtual bool         GetAudioSwitchId(char const* const szAudioSwitchName, AudioControlId& audioSwitchId) const override                                                                                                                                                                       { return true; }
	virtual bool         GetAudioSwitchStateId(AudioControlId const audioSwitchId, char const* const szSwitchStateName, AudioSwitchStateId& audioSwitchStateId) const override                                                                                                                     { return true; }
	virtual bool         GetAudioPreloadRequestId(char const* const szAudioPreloadRequestName, AudioPreloadRequestId& audioPreloadRequestId) const override                                                                                                                                        { audioPreloadRequestId = INVALID_AUDIO_PRELOAD_REQUEST_ID; return true; }
	virtual bool         GetAudioEnvironmentId(char const* const szAudioEnvironmentName, AudioEnvironmentId& audioEnvironmentId) const override                                                                                                                                                    { audioEnvironmentId = INVALID_AUDIO_ENVIRONMENT_ID; return true; }
	virtual bool         ReserveAudioListenerId(AudioObjectId& audioObjectId) override                                                                                                                                                                                                             { return true; }
	virtual bool         ReleaseAudioListenerId(AudioObjectId const audioObjectId) override                                                                                                                                                                                                        { return true; }
	virtual void         OnCVarChanged(ICVar* const pCvar) override                                                                                                                                                                                                                                {}
	virtual char const*  GetConfigPath() const override                                                                                                                                                                                                                                            { return ""; }

	virtual IAudioProxy* GetFreeAudioProxy() override                                                                                                                                                                                                                                              { return static_cast<IAudioProxy*>(&m_nullAudioProxy); }

	virtual char const*  GetAudioControlName(EAudioControlType const audioControlType, AudioIdType const audioControlId) override                                                                                                                                                                  { return nullptr; }
	virtual char const*  GetAudioControlName(EAudioControlType const audioControlType, AudioIdType const audioControlId1, AudioIdType const audioControlId2) override                                                                                                                              { return nullptr; }
	virtual void         GetAudioDebugData(SAudioDebugData& audioDebugData) const override                                                                                                                                                                                                         {}
	virtual void         GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) override                                                                                                                                                                                    {}

private:

	CNULLAudioProxy m_nullAudioProxy;
};
