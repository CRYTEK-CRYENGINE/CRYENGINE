// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Audio.h"

#include "MonoRuntime.h"

#include "Wrappers/MonoDomain.h"
#include "Wrappers/MonoClass.h"

#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IObject.h>
#include <CryAudio/IListener.h>

typedef void (* AudioRequestListener)(const CryAudio::SRequestInfo* const);

static void LoadTrigger(uint triggerId)
{
	gEnv->pAudioSystem->LoadTrigger(triggerId);
}

static void UnloadTrigger(uint triggerId)
{
	gEnv->pAudioSystem->UnloadTrigger(triggerId);
}

static void ExecuteTrigger(uint triggerId, bool bExecuteSync)
{
	if (bExecuteSync)
	{
		CryAudio::SRequestUserData const data(CryAudio::ERequestFlags::ExecuteBlocking | CryAudio::ERequestFlags::CallbackOnExternalOrCallingThread | CryAudio::ERequestFlags::DoneCallbackOnExternalThread);
		gEnv->pAudioSystem->ExecuteTrigger(triggerId, data);
	}
	else
	{
		CryAudio::SRequestUserData const data(CryAudio::ERequestFlags::CallbackOnExternalOrCallingThread | CryAudio::ERequestFlags::DoneCallbackOnExternalThread);
		gEnv->pAudioSystem->ExecuteTrigger(triggerId, data);
	}
}

static void StopTrigger(uint triggerId, bool bExecuteSync)
{
	if (bExecuteSync)
	{
		CryAudio::SRequestUserData const data(CryAudio::ERequestFlags::ExecuteBlocking);
		gEnv->pAudioSystem->StopTrigger(triggerId, data);
	}
	else
	{
		gEnv->pAudioSystem->StopTrigger(triggerId);
	}
}

static uint GetAudioTriggerId(MonoInternals::MonoString* pAudioTriggerName)
{
	std::shared_ptr<CMonoString> pAudioTriggerNameObject = CMonoDomain::CreateString(pAudioTriggerName);
	return CryAudio::StringToId(pAudioTriggerNameObject->GetString());
}

static uint GetAudioParameterId(MonoInternals::MonoString* pAudioParameterName)
{
	std::shared_ptr<CMonoString> pAudioParameterNameObject = CMonoDomain::CreateString(pAudioParameterName);
	return CryAudio::StringToId(pAudioParameterNameObject->GetString());
}

static void SetAudioParameter(uint parameterId, float parameterValue)
{
	gEnv->pAudioSystem->SetParameter(parameterId, parameterValue);
}

static uint GetAudioSwitchId(MonoInternals::MonoString* pAudioSwitchName)
{
	std::shared_ptr<CMonoString> pAudioSwitchNameObject = CMonoDomain::CreateString(pAudioSwitchName);
	return CryAudio::StringToId(pAudioSwitchNameObject->GetString());
}

static uint GetAudioSwitchStateId(uint audioSwitchId, MonoInternals::MonoString* pAudioSwitchStateName)
{
	std::shared_ptr<CMonoString> pAudioSwitchStateNameObject = CMonoDomain::CreateString(pAudioSwitchStateName);
	return CryAudio::StringToId(pAudioSwitchStateNameObject->GetString());
}

static void SetAudioSwitchState(uint audioSwitchId, uint switchStateId)
{
	gEnv->pAudioSystem->SetSwitchState(audioSwitchId, switchStateId);
}

static void StopAllSounds()
{
	gEnv->pAudioSystem->StopAllSounds();
}

static MonoInternals::MonoString* GetConfigPath()
{
	std::shared_ptr<CMonoString> pAudioConfigPathObject = gEnv->pMonoRuntime->GetActiveDomain()->CreateString(gEnv->pAudioSystem->GetConfigPath());

	return pAudioConfigPathObject->GetManagedObject();
}

static void PlayFile(CryAudio::SPlayFileInfo* pPlayFileInfo)
{
	gEnv->pAudioSystem->PlayFile(*pPlayFileInfo);
}

static void StopFile(CryAudio::SPlayFileInfo* pPlayFileInfo)
{
	gEnv->pAudioSystem->StopFile(pPlayFileInfo->szFile);
}

static void EnableAllSound(bool bIsEnabled)
{
	if (bIsEnabled)
	{
		gEnv->pAudioSystem->ExecuteTrigger(CryAudio::UnmuteAllTriggerId);
	}
	else
	{
		gEnv->pAudioSystem->ExecuteTrigger(CryAudio::MuteAllTriggerId);
	}
}

// IObject interface
static CryAudio::IObject* CreateAudioObject()
{
	CryAudio::IObject* pAudioObject = gEnv->pAudioSystem->CreateObject();
	return pAudioObject;
}

static CryAudio::CObjectTransformation* CreateAudioTransformation(float m00, float m01, float m02, float m03,
                                                                  float m10, float m11, float m12, float m13,
                                                                  float m20, float m21, float m22, float m23)
{
	Matrix34_tpl<float> m34 = Matrix34_tpl<float>(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23);
	CryAudio::CObjectTransformation* pAudioTransformation = new CryAudio::CObjectTransformation(m34);
	return pAudioTransformation;
}

static void SetAudioTransformation(CryAudio::IObject* pAudioObject, CryAudio::CObjectTransformation* pObjectTransformation)
{
	pAudioObject->SetTransformation(*pObjectTransformation);
}

static void ReleaseAudioTransformation(CryAudio::CObjectTransformation* pObjectTransformation)
{
	delete pObjectTransformation;
}

static void ReleaseAudioObject(CryAudio::IObject* pAudioObject)
{
	gEnv->pAudioSystem->ReleaseObject(pAudioObject);
}

static void ExecuteAudioObjectTrigger(CryAudio::IObject* pAudioObject, uint triggerId, bool bExecuteSync)
{
	if (bExecuteSync)
	{
		const CryAudio::SRequestUserData data(CryAudio::ERequestFlags::ExecuteBlocking | CryAudio::ERequestFlags::CallbackOnExternalOrCallingThread | CryAudio::ERequestFlags::DoneCallbackOnExternalThread);
		pAudioObject->ExecuteTrigger(triggerId, data);
	}
	else
	{
		const CryAudio::SRequestUserData data(CryAudio::ERequestFlags::CallbackOnExternalOrCallingThread | CryAudio::ERequestFlags::DoneCallbackOnExternalThread);
		pAudioObject->ExecuteTrigger(triggerId, data);
	}

}

static void StopAudioObjectTrigger(CryAudio::IObject* pAudioObject, uint triggerId, bool bExecuteSync)
{
	if (bExecuteSync)
	{
		const CryAudio::SRequestUserData data(CryAudio::ERequestFlags::ExecuteBlocking);
		pAudioObject->StopTrigger(triggerId, data);
	}
	else
	{
		pAudioObject->StopTrigger(triggerId);
	}
}

static CryAudio::SPlayFileInfo* CreateSPlayFileInfo(MonoInternals::MonoString* pFilePath)
{
	std::shared_ptr<CMonoString> pFilePathObject = CMonoDomain::CreateString(pFilePath);

	CryAudio::SPlayFileInfo* pPlayFileInfo = new CryAudio::SPlayFileInfo(pFilePathObject->GetString());

	return pPlayFileInfo;
}

static void ReleaseSPlayFileInfo(CryAudio::SPlayFileInfo* pPlayFileInfo)
{
	delete pPlayFileInfo;
}

static CryAudio::SRequestInfo* CreateSRequestInfo(uint eRequestResult, uint audioSystemEvent, uint CtrlId, CryAudio::IObject* pAudioObject)
{
	CryAudio::SRequestInfo* pRequestInfo = new CryAudio::SRequestInfo(static_cast<CryAudio::ERequestResult>(eRequestResult), nullptr, nullptr, nullptr, static_cast<CryAudio::ESystemEvents>(audioSystemEvent), (CryAudio::ControlId)CtrlId, pAudioObject, nullptr, nullptr);
	return pRequestInfo;
}

static void ReleaseSRequestInfo(CryAudio::SRequestInfo* pRequestInfo)
{
	delete pRequestInfo;
}

static uint SRIGetRequestResult(CryAudio::SRequestInfo* pRequestInfo)
{
	return static_cast<uint>(pRequestInfo->requestResult);
}

static uint SRIGetSystemEvent(CryAudio::SRequestInfo* pRequestInfo)
{
	return static_cast<uint>(pRequestInfo->systemEvent);
}

static uint SRIGetControlId(CryAudio::SRequestInfo* pRequestInfo)
{
	return static_cast<uint>(pRequestInfo->audioControlId);
}

static CryAudio::IObject* SRIGetAudioObject(CryAudio::SRequestInfo* pRequestInfo)
{
	return pRequestInfo->pAudioObject;
}

static void AddAudioRequestListener(AudioRequestListener listener)
{
	gEnv->pAudioSystem->AddRequestListener(listener, nullptr, CryAudio::ESystemEvents::TriggerExecuted | CryAudio::ESystemEvents::TriggerFinished);
}

static void RemoveAudioRequestListener(AudioRequestListener listener)
{
	gEnv->pAudioSystem->RemoveRequestListener(listener, nullptr);
}

static CryAudio::IListener* CreateAudioListener()
{
	return gEnv->pAudioSystem->CreateListener();
}

static void SetAudioListenerTransformation(CryAudio::IListener* pAudioListener, CryAudio::CObjectTransformation* pCObjectTransformation)
{
	pAudioListener->SetTransformation(*pCObjectTransformation);
}

void CAudioInterface::RegisterFunctions(std::function<void(const void* pMethod, const char* szMethodName)> func)
{
	// IAudioSystem
	func(LoadTrigger, "LoadTrigger");
	func(UnloadTrigger, "UnloadTrigger");
	func(ExecuteTrigger, "ExecuteTrigger");
	func(StopTrigger, "StopTrigger");
	func(GetAudioTriggerId, "GetAudioTriggerId");
	func(GetAudioParameterId, "GetAudioParameterId");
	func(SetAudioParameter, "SetAudioParameter");
	func(GetAudioSwitchId, "GetAudioSwitchId");
	func(GetAudioSwitchStateId, "GetAudioSwitchStateId");
	func(SetAudioSwitchState, "SetAudioSwitchState");
	func(CreateAudioObject, "CreateAudioObject");
	func(StopAllSounds, "StopAllSounds");
	func(GetConfigPath, "GetConfigPath");
	func(PlayFile, "PlayFile");
	func(StopFile, "StopFile");
	func(EnableAllSound, "EnableAllSound");
	func(AddAudioRequestListener, "AddAudioRequestListener");
	func(RemoveAudioRequestListener, "RemoveAudioRequestListener");
	func(CreateAudioListener, "CreateAudioListener");

	// IObject
	func(CreateAudioObject, "CreateAudioObject");
	func(CreateAudioTransformation, "CreateAudioTransformation");
	func(ReleaseAudioTransformation, "ReleaseAudioTransformation");
	func(SetAudioTransformation, "SetAudioTransformation");
	func(ReleaseAudioObject, "ReleaseAudioObject");
	func(ExecuteAudioObjectTrigger, "ExecuteAudioObjectTrigger");
	func(StopAudioObjectTrigger, "StopAudioObjectTrigger");

	//SPlayFileInfo
	func(CreateSPlayFileInfo, "CreateSPlayFileInfo");
	func(ReleaseSPlayFileInfo, "ReleaseSPlayFileInfo");

	//SRequestInfo
	func(CreateSRequestInfo, "CreateSRequestInfo");
	func(ReleaseSRequestInfo, "ReleaseSRequestInfo");
	func(SRIGetRequestResult, "SRIGetRequestResult");
	func(SRIGetSystemEvent, "SRIGetEnumFlagsType");
	func(SRIGetControlId, "SRIGetControlId");
	func(SRIGetAudioObject, "SRIGetAudioObject");

	//IListener
	func(SetAudioListenerTransformation, "SetAudioListenerTransformation");
}
