%include "CryEngine.swig"

%import "CryCommon.i"

%{
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IListener.h>
#include <CryAudio/IObject.h>
%}

%ignore CryAudio::ISystemModule;
%ignore CryAudio::IImplModule;

%typemap(csbase) CryAudio::ERequestFlags "uint"
%typemap(csbase) CryAudio::ERequestStatus "uint"
%typemap(csbase) CryAudio::ERequestResult "uint"
%typemap(csbase) CryAudio::ESystemEvents "uint"
%typemap(csbase) CryAudio::EDataScope "uint"
%typemap(csbase) CryAudio::EOcclusionType "uint"

%ignore CryAudio::SRequestInfo::pStandaloneFile;
%ignore CryAudio::SRequestInfo::pAudioEvent;
%ignore CryAudio::SRequestInfo::pOwner;
%ignore CryAudio::SRequestInfo::pUserData;
%ignore CryAudio::SRequestInfo::pUserDataOwner;
%ignore CryAudio::SRequestInfo::SRequestInfo(ERequestResult const, void* const, void* const, void* const,EnumFlagsType const, ControlId const, IObject* const, CATLStandaloneFile*, CATLEvent* );

%ignore CryAudio::SRequestUserData::pOwner;
%ignore CryAudio::SRequestUserData::pUserData;
%ignore CryAudio::SRequestUserData::pUserDataOwner;
%ignore CryAudio::SRequestUserData::SRequestUserData;

%include "../../../../CryEngine/CryCommon/CryAudio/IAudioInterfacesCommonData.h"
%feature("director") IAudioListener;

//suppress IListener 
%ignore CryAudio::IAudioSystem::CreateListener;
%ignore CryAudio::IAudioSystem::ReleaseListener(IListener* const pIListener);

//suppress audio functions that are wrapped through extension below
%ignore CryAudio::IAudioSystem::GetAudioTriggerId(char const* const szAudioTriggerName, ControlId& audioTriggerId) const;
%ignore CryAudio::IAudioSystem::GetAudioParameterId(char const* const szParameterName, ControlId& parameterId) const;
%ignore CryAudio::IAudioSystem::GetAudioSwitchId(char const* const szAudioSwitchName, ControlId& audioSwitchId) const;
%ignore CryAudio::IAudioSystem::GetAudioSwitchStateId(ControlId const audioSwitchId, char const* const szSwitchStateName, SwitchStateId& audioSwitchStateId) const ;
%ignore CryAudio::IAudioSystem::GetAudioPreloadRequestId(char const* const szAudioPreloadRequestName, PreloadRequestId& audioPreloadRequestId) const;
%ignore CryAudio::IAudioSystem::GetAudioEnvironmentId(char const* const szAudioEnvironmentName, EnvironmentId& audioEnvironmentId) const;

//suppress add/remove request listeners
%ignore CryAudio::IAudioSystem::AddRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo, EnumFlagsType const eventMask);
%ignore CryAudio::IAudioSystem::RemoveRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo);
%ignore CryAudio::IAudioSystem::SetImpl;

%ignore CryAudio::IAudioSystem::ReportStartedFile;
%ignore CryAudio::IAudioSystem::ReportStoppedFile;
%ignore CryAudio::IAudioSystem::ReportFinishedEvent;
%ignore CryAudio::CATLStandaloneFile;
%ignore CryAudio::CATLEvent;

%include "../../../../CryEngine/CryCommon/CryAudio/IAudioSystem.h"
%include "../../../../CryEngine/CryCommon/CryAudio/IObject.h"

//test test