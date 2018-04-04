// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __bootprofilersystem_h__
#define __bootprofilersystem_h__
#pragma once

#if defined(ENABLE_LOADING_PROFILER)

#include <CryThreading/IThreadManager.h>

class CBootProfilerRecord;
class CBootProfilerSession;

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
	virtual void          ThreadEntry() override;
	// ~IThread

	static CBootProfiler& GetInstance();

	void                  Init(ISystem* pSystem);
	void                  RegisterCVars();

	void                  StartSession(const char* sessionName);
	void                  StopSession();

	void                  StopSaveSessionsThread();
	void                  QueueSessionToDelete(CBootProfilerSession*pSession);
	void                  QueueSessionToSave(float functionMinTimeThreshold, CBootProfilerSession* pSession);

	CBootProfilerRecord*  StartBlock(const char* name, const char* args);
	void                  StopBlock(CBootProfilerRecord* record);

	void                  StartFrame(const char* name);
	void                  StopFrame();

protected:
	// ISystemEventListener
	virtual void          OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

private:
	CBootProfilerSession* m_pCurrentSession;

	bool                  m_quitSaveThread;
	CryEvent              m_saveThreadWakeUpEvent;
	TSessions             m_sessionsToDelete;
	TSessionsToSave       m_sessionsToSave;

	static int            CV_sys_bp_frames_worker_thread;
	static int            CV_sys_bp_frames;
	static float          CV_sys_bp_frames_threshold;
	static float          CV_sys_bp_time_threshold;
	CBootProfilerRecord*  m_pMainThreadFrameRecord;

	int                   m_levelLoadAdditionalFrames;
};

#else //ENABLE_LOADING_PROFILER
#endif

#endif
