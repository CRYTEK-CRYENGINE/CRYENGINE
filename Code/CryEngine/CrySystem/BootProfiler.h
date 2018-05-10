// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(ENABLE_LOADING_PROFILER)

#include <CryThreading/IThreadManager.h>

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
	friend class CBootProfileBLock;
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

	typedef CryMT::vector<CBootProfilerSession*> TSessions;
	typedef CryMT::vector<SSaveSessionInfo> TSessionsToSave;

public:
	CBootProfiler();
	~CBootProfiler();

	// IThread
	virtual void               ThreadEntry() override;
	// ~IThread

	static CBootProfiler&      GetInstance();

	void                       Init(ISystem* pSystem);
	void                       RegisterCVars();

	void                       StartSession(const char* sessionName);
	void                       StopSession();

	void                       StopSaveSessionsThread();
	void                       QueueSessionToDelete(CBootProfilerSession*pSession);
	void                       QueueSessionToSave(float functionMinTimeThreshold, CBootProfilerSession* pSession);

	CBootProfilerRecord*       StartBlock(const char* name, const char* args,EProfileDescription type);
	void                       StopBlock(CBootProfilerRecord* record);

	void                       StartFrame(const char* name);
	void                       StopFrame();

protected:
	// ISystemEventListener
	virtual void               OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

private:
	CBootProfilerSession*      m_pCurrentSession;

	bool                       m_quitSaveThread;
	CryEvent                   m_saveThreadWakeUpEvent;
	TSessions                  m_sessionsToDelete;
	TSessionsToSave            m_sessionsToSave;

	static EBootProfilerFormat CV_sys_bp_output_formats;
	static int                 CV_sys_bp_enabled;
	static int                 CV_sys_bp_level_load;
	static int                 CV_sys_bp_frames_worker_thread;
	static int                 CV_sys_bp_frames;
	static int                 CV_sys_bp_frames_sample_period;
	static int                 CV_sys_bp_frames_sample_period_rnd;
	static float               CV_sys_bp_frames_threshold;
	static float               CV_sys_bp_time_threshold;
	CBootProfilerRecord*       m_pMainThreadFrameRecord;

	int                        m_levelLoadAdditionalFrames;
	int                        m_countdownToNextSaveSesssion;
};

#else //ENABLE_LOADING_PROFILER

#endif ////ENABLE_LOADING_PROFILER
