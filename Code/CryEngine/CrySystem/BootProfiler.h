// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(ENABLE_LOADING_PROFILER)

#include <CryThreading/IThreadManager.h>
#include <CrySystem/Profilers/ILegacyProfiler.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryThreading/MultiThread_Containers.h>

struct ICVar;
class CBootProfilerRecord;
class CBootProfilerSession;

enum class EBootProfilerFormat : int
{
	XML,
	ChromeTraceJSON
};

//////////////////////////////////////////////////////////////////////////

class CBootProfiler : public ISystemEventListener, public IThread
{
	friend class CBootProfilerSession;

public:
	struct SSaveSessionInfo
	{
		float                 functionMinTimeThreshold;
		CBootProfilerSession* pSession;

		SSaveSessionInfo(float functionMinTimeThreshold, CBootProfilerSession* pSession)
			: functionMinTimeThreshold(functionMinTimeThreshold)
			, pSession(pSession)
		{}
	};

public:
	CBootProfiler();
	~CBootProfiler();

	// IThread
	virtual void               ThreadEntry() override;
	// ~IThread

	// these will be called from CCryProfilingSystem
	void OnFrameStart();
	void OnSectionStart(const SProfilingSection&);
	void OnSectionEnd(const SProfilingSection&, const SProfilingSectionEnd&);
	void OnMarker(int64 timeStamp, const SProfilingMarker&);
	void OnFrameEnd();

	static CBootProfiler& GetInstance();

	void                  Init(ISystem* pSystem, const char* cmdLine);
	void                  RegisterCVars();

	void                  StartSession(const char* sessionName);
	void                  StopSession();

private:
	void                  StopSaveSessionsThread();
	void                  QueueSessionToDelete(CBootProfilerSession*pSession);
	void                  QueueSessionToSave(CBootProfilerSession* pSession);

protected:
	// ISystemEventListener
	virtual void          OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

private:
	typedef CryMT::vector<SSaveSessionInfo> TSessionsToSave;

	static void                StartFrameProfilingCmd(IConsoleCmdArgs* pArgs);

	static void                SaveProfileSessionToDisk(const float funcMinTimeThreshold, CBootProfilerSession* pSession);
		
	CBootProfilerSession*      m_pCurrentSession;

	bool                       m_quitSaveThread;
	bool                       m_initialized;
	CryEvent                   m_saveThreadWakeUpEvent;
	TSessionsToSave            m_sessionsToSave;

	static EBootProfilerFormat CV_sys_bp_output_formats;
	static int                 CV_sys_bp_enabled;
	static int                 CV_sys_bp_level_load;
	static int                 CV_sys_bp_level_load_include_first_frame;
	static int                 CV_sys_bp_frames_worker_thread;
	static int                 CV_sys_bp_frames_sample_period;
	static int                 CV_sys_bp_frames_sample_period_rnd;
	static float               CV_sys_bp_frames_threshold;
	static float               CV_sys_bp_time_threshold;
	static ICVar*              CV_sys_bp_frames_required_label;

	CBootProfilerRecord*       m_pMainThreadFrameRecord;
	int                        m_numFramesToLog;
	int                        m_levelLoadAdditionalFrames;
	int                        m_countdownToNextSaveSesssion;
};

#else //ENABLE_LOADING_PROFILER

#endif //ENABLE_LOADING_PROFILER
