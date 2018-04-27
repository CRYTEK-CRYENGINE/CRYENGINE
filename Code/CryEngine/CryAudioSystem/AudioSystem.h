// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATL.h"
#include <CrySystem/TimeValue.h>
#include <CryThreading/IThreadManager.h>
#include <concqueue/concqueue.hpp>

namespace CryAudio
{
// Forward declarations.
class CSystem;

class CMainThread final : public ::IThread
{
public:

	CMainThread() = default;
	CMainThread(CMainThread const&) = delete;
	CMainThread(CMainThread&&) = delete;
	CMainThread& operator=(CMainThread const&) = delete;
	CMainThread& operator=(CMainThread&&) = delete;

	void         Init(CSystem* const pSystem);

	// IThread
	virtual void ThreadEntry() override;
	// ~IThread

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork();

	bool IsActive();
	void Activate();
	void Deactivate();

private:

	CSystem*      m_pSystem = nullptr;
	volatile bool m_bQuit = false;
};

class CSystem final : public IAudioSystem, ::ISystemEventListener
{
public:

	CSystem();
	virtual ~CSystem() override;

	CSystem(CSystem const&) = delete;
	CSystem(CSystem&&) = delete;
	CSystem& operator=(CSystem const&) = delete;
	CSystem& operator=(CSystem&&) = delete;

	// CryAudio::IAudioSystem
	virtual void        Release() override;
	virtual void        SetImpl(Impl::IImpl* const pIImpl, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        LoadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        UnloadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        ExecuteTriggerEx(SExecuteTriggerData const& triggerData, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        StopFile(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        ReportStartedFile(CATLStandaloneFile& standaloneFile, bool const bSuccessfullyStarted, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        ReportStoppedFile(CATLStandaloneFile& standaloneFile, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        ReportFinishedEvent(CATLEvent& event, bool const bSuccess, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        StopAllSounds(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        Refresh(char const* const szLevelName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        PreloadSingleRequest(PreloadRequestId const id, bool const bAutoLoadOnly, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        UnloadSingleRequest(PreloadRequestId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        ReloadControlsData(char const* const szFolderPath, char const* const szLevelName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        AddRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo, ESystemEvents const eventMask) override;
	virtual void        RemoveRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo) override;
	virtual void        ExternalUpdate() override;
	virtual char const* GetConfigPath() const override;
	virtual IListener*  CreateListener(char const* const szname = nullptr) override;
	virtual void        ReleaseListener(IListener* const pIListener) override;
	virtual IObject*    CreateObject(SCreateObjectData const& objectData = SCreateObjectData::GetEmptyObject(), SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void        ReleaseObject(IObject* const pIObject) override;
	virtual void        GetFileData(char const* const szName, SFileData& fileData) override;
	virtual void        GetTriggerData(ControlId const triggerId, STriggerData& triggerData) override;
	virtual void        OnLoadLevel(char const* const szLevelName) override;
	virtual void        OnUnloadLevel() override;
	virtual void        OnLanguageChanged() override;
	virtual void        GetImplInfo(SImplInfo& implInfo) override;
	virtual void        Log(ELogType const type, char const* const szFormat, ...) override;
	// ~CryAudio::IAudioSystem

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	bool Initialize();
	void PushRequest(CAudioRequest const& request);
	void ParseControlsData(char const* const szFolderPath, EDataScope const dataScope, SRequestUserData const& userData = SRequestUserData::GetEmptyObject());
	void ParsePreloadsData(char const* const szFolderPath, EDataScope const dataScope, SRequestUserData const& userData = SRequestUserData::GetEmptyObject());
	void RetriggerAudioControls(SRequestUserData const& userData = SRequestUserData::GetEmptyObject());

	void InternalUpdate();

private:

	using AudioRequests = ConcQueue<UnboundMPSC, CAudioRequest>;
	using AudioRequestsSyncCallbacks = ConcQueue<UnboundSPSC, CAudioRequest>;

	void        ProcessRequests(AudioRequests& requestQueue);
	static void OnCallback(SRequestInfo const* const pRequestInfo);

	bool                       m_isInitialized;
	bool                       m_didThreadWait;
	volatile float             m_accumulatedFrameTime;
	std::atomic<uint32>        m_externalThreadFrameId;
	uint32                     m_lastExternalThreadFrameId;
	CMainThread                m_mainThread;

	CAudioTranslationLayer     m_atl;
	AudioRequests              m_requestQueue;
	AudioRequestsSyncCallbacks m_syncCallbacks;
	CAudioRequest              m_syncRequest;
	CAudioRequest              m_request;
	CryEvent                   m_mainEvent;
	CryEvent                   m_audioThreadWakeupEvent;

#if defined(ENABLE_AUDIO_LOGGING)
	int m_loggingOptions;
#endif // ENABLE_AUDIO_LOGGING

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:
	void ScheduleIRenderAuxGeomForRendering(IRenderAuxGeom* pRenderAuxGeom);

private:
	void SubmitLastIRenderAuxGeomForRendering();
	void DrawAudioDebugData();

	std::atomic<IRenderAuxGeom*> m_currentRenderAuxGeom;
	std::atomic<IRenderAuxGeom*> m_lastRenderAuxGeom;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
