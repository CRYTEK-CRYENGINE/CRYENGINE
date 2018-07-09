// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   timedemorecorder.h
//  Version:     v1.00
//  Created:     2/8/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __timedemorecorder_h__
#define __timedemorecorder_h__
#pragma once

#include <CryCore/Containers/CryListenerSet.h>
#include <ITimeDemoRecorder.h>
#include "ITestModule.h"

struct SRecordedGameEvent;

struct STimeDemoGameEvent
{
	string entityName;
	uint8  gameEventType;
	string description;
	string description2;
	float  value;
	int    extra;

	STimeDemoGameEvent()
		: gameEventType(0)
		, value(0.0f)
		, extra(0)
	{}

	STimeDemoGameEvent(IEntity* pEntity, const GameplayEvent& event)
		: gameEventType(event.event)
		, description(event.description)
		, description2((const char*)(event.extra))
		, value(event.value)
		, extra(0)
	{
		if (pEntity)
		{
			entityName = pEntity->GetName();
		}
	}

	STimeDemoGameEvent(const SRecordedGameEvent& event);

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(entityName);
		pSizer->AddObject(description);
		pSizer->AddObject(description2);
	}
};
typedef std::vector<STimeDemoGameEvent> TGameEventRecords;

class CTimeDemoRecorder 
	: public ITimeDemoRecorder
	, IFrameProfilePeakCallback
	, IInputEventListener
	, IEntitySystemSink
	, IGameplayListener
	, IEntityEventListener
{
public:
	CTimeDemoRecorder();
	virtual ~CTimeDemoRecorder();

	void Reset();

	void PreUpdate();
	void PostUpdate();

	void GetMemoryStatistics(class ICrySizer* pSizer) const;

	bool IsTimeDemoActive() const { return m_bChainloadingDemo || m_bPlaying || m_bRecording; }
	bool IsChainLoading() const   { return m_bChainloadingDemo; }

	//////////////////////////////////////////////////////////////////////////
	// Implements ITimeDemoRecorder interface.
	//////////////////////////////////////////////////////////////////////////
	virtual bool IsRecording() const override { return m_bRecording; };
	virtual bool IsPlaying() const override   { return m_bPlaying; };
	virtual void RegisterListener(ITimeDemoListener* pListener) override;
	virtual void UnregisterListener(ITimeDemoListener* pListener) override;
	virtual void GetCurrentFrameRecord(STimeDemoFrameRecord& externalRecord) const override;
	//////////////////////////////////////////////////////////////////////////

private:
	//////////////////////////////////////////////////////////////////////////
	// Implements IFrameProfilePeakCallback interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnFrameProfilerPeak(CFrameProfiler* pProfiler, float fPeakTime) override;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Implements IInputEventListener interface.
	//////////////////////////////////////////////////////////////////////////
	virtual bool OnInputEvent(const SInputEvent& event) override;
	//////////////////////////////////////////////////////////////////////////
	// Implements IGameplayListener interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnGameplayEvent(IEntity* pEntity, const GameplayEvent& event) override;

	//////////////////////////////////////////////////////////////////////////
	// IEntitySystemSink
	//////////////////////////////////////////////////////////////////////////
	virtual bool OnBeforeSpawn(SEntitySpawnParams& params) override { return true; }
	virtual void OnSpawn(IEntity* pEntity, SEntitySpawnParams& params) override;
	virtual bool OnRemove(IEntity* pEntity) override { return true; }
	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& params) override {}
	//////////////////////////////////////////////////////////////////////////

	// IEntity::IEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) override;
	// ~IEntity::IEventListener

private:
	// Input event list.
	struct EntityEventRecord
	{
		EntityId   entityId;      // What entity performed event.
		EntityGUID guid;          // What entity performed event.

		EEntityEvent eventType;     // What event.
		uint64       nParam[4];     // event params.
		Vec3         pos;
		Quat         q;

		enum Flags
		{
			HIDDEN = BIT(1),
		};

		// Special flags.
		uint32 flags;

		void   GetMemoryUsage(ICrySizer* pSizer) const {}
	};
	typedef std::vector<SInputEvent>       InputEventsList;
	typedef std::vector<EntityEventRecord> EntityEventRecords;

	//! This structure saved for every frame of time demo.
	struct FrameRecord
	{
		FrameRecord()
			: playerPosition(ZERO)
			, playerRotation(IDENTITY)
			, playerViewRotation(IDENTITY)
			, hmdPositionOffset(ZERO)
			, hmdViewRotation(IDENTITY)
			, cameraAngles(ZERO)
			, frameTime(0.0f)
			, fLeaning(0.0f)
			, nPolygons(0)
		{
			nActionFlags[0] = 0;
			nActionFlags[1] = 0;
		}

		Vec3  playerPosition;
		Quat  playerRotation;
		Quat  playerViewRotation;
		Vec3  hmdPositionOffset;
		Quat  hmdViewRotation;
		Ang3  cameraAngles; // Legacy
		float frameTime;    // Immediate frame rate, for this frame.

		// Snapshot of current processing command.
		unsigned int       nActionFlags[2];
		float              fLeaning;
		int                nPolygons; // Polys rendered in this frame.

		InputEventsList    inputEventsList;
		EntityEventRecords entityEvents;
		TGameEventRecords  gameEvents;

		void               GetMemoryUsage(ICrySizer* pSizer) const {}
	};

private:
	void            StartSession();
	void            StopSession();

	float           RenderInfo(float y = 0);
	void            Record(bool bEnable);
	void            Play(bool bEnable);
	ETestModuleType GetType() const { return TM_TIMEDEMO; }
	void            ParseParams(XmlNodeRef node);
	void            SetVariable(const char* name, const char* szValue);
	void            SetVariable(const char* name, float value);

	//! Get number of frames in record.
	int                GetNumFrames() const;
	float              GetAverageFrameRate() const;

	void               Save(const char* filename);
	bool               Load(const char* filename);

	void               StartChainDemo(const char* levelsListFilename, bool bAutoLoadChainConfig);
	void               StartDemoLevel(const char** levelNames, int levelCount);
	void               StartDemoDelayed();

	void               Pause(bool paused) { m_bPaused = paused; }

	bool               RecordFrame();
	bool               PlayFrame();
	int                GetNumberOfFrames();
	int                GetTotalPolysRecorded() { return m_nTotalPolysRecorded; }
	void               LogEndOfLoop();

	//! Gets current loaded level path, "" if not yet finished loading
	static const char* GetCurrentLevelPath();

	CTimeValue         GetTime();
	// Set Value of console variable.
	void               SetConsoleVar(const char* sVarName, float value);
	// Get value of console variable.
	float              GetConsoleVar(const char* sVarName);

	int                ComputePolyCount();

	void               ResetSessionLoop();

	void               EraseLogFile();
	void               LogInfo(const char* format, ...) PRINTF_PARAMS(2, 3);

	void               PlayBackEntityEvent(const EntityEventRecord& rec);
	void               SaveAllEntitiesState();
	void               RestoreAllEntitiesState();

	string             GetCurrentLevelName();
	string             GetInitSaveFileName();

	void               StartNextChainedLevel();
	void               SaveChainloadingJUnitResults();
	void               EndDemo();
	void               QuitGame();
	void               ProcessKeysInput();
	void               ReplayGameState(struct FrameRecord& rec);
	void               SaveDemoFinishedLog();

	void               AddFrameRecord(const FrameRecord& rec);

	void               SignalPlayback(bool bEnable);
	void               SignalRecording(bool bEnable);
	void               SignalPlayFrame();

protected:
	static void cmd_StartRecordingTimeDemo(IConsoleCmdArgs* pArgs);
	static void cmd_Play(IConsoleCmdArgs* pArgs);
	static void cmd_StartDemoChain(IConsoleCmdArgs* pArgs);
	static void cmd_StartDemoLevel(IConsoleCmdArgs* pArgs);
	static void cmd_Stop(IConsoleCmdArgs* pArgs);

private:
	typedef CListenerSet<ITimeDemoListener*> TTimeDemoListeners;
	TTimeDemoListeners m_listeners;

	typedef std::vector<FrameRecord> FrameRecords;
	FrameRecords m_records;

	bool         m_bRecording;
	bool         m_bPlaying;
	bool         m_bPaused;
	bool         m_bDemoFinished;
	bool         m_bDemoEnded;
	bool         m_bChainloadingDemo;

	//! Current play or record frame.
	int                      m_currentFrame;

	std::vector<SInputEvent> m_currentFrameInputEvents;
	EntityEventRecords       m_currentFrameEntityEvents;
	EntityEventRecords       m_firstFrameEntityState;

	TGameEventRecords        m_currentFrameGameEvents;

	//////////////////////////////////////////////////////////////////////////
	// Old values of console vars.
	//////////////////////////////////////////////////////////////////////////

	//! Timings.
	CTimeValue m_recordStartTime;
	CTimeValue m_lastFrameTime;
	CTimeValue m_totalDemoTime;
	CTimeValue m_recordedDemoTime;

	// How many polygons were recorded.
	int   m_nTotalPolysRecorded;
	// How many polygons were played.
	int   m_nTotalPolysPlayed;

	float m_lastPlayedTotalTime;
	float m_lastAveFrameRate;
	float m_sumFPS;
	float m_minFPS;
	float m_maxFPS;
	float m_currFPS;

	int   m_minFPSCounter;
	int   m_minFPS_Frame;
	int   m_maxFPS_Frame;

	int   m_nCurrPolys;
	int   m_nMaxPolys;
	int   m_nMinPolys;
	int   m_nPolysPerSec;
	int   m_nPolysCounter;

	// For calculating current last second fps.
	CTimeValue m_lastFpsTimeRecorded;
	int        m_fpsCounter;

	int        m_fileVersion;

	bool       m_bEnabledProfiling, m_bVisibleProfiling;

	float      m_oldPeakTolerance;
	float      m_fixedTimeStep;

	string     m_file;

	//	IGameStateRecorder* m_pGameStateRecorder;

	struct STimeDemoInfo* m_pTimeDemoInfo;

private:
	static ICVar*             s_timedemo_file;
	static CTimeDemoRecorder* s_pTimeDemoRecorder;

	int                       m_numLoops;
	int                       m_maxLoops;
	int                       m_numOrientations;

	int                       m_demo_scroll_pause;
	int                       m_demo_quit;
	int                       m_finish_replaysizer;
	int                       m_finish_replaystop;
	int                       m_demo_screenshot_frame;
	int                       m_demo_max_frames;
	int                       m_demo_savestats;
	int                       m_demo_ai;
	int                       m_demo_restart_level;
	int                       m_demo_panoramic;
	int                       m_demo_fixed_timestep;
	int                       m_demo_vtune;
	int                       m_demo_time_of_day;
	int                       m_demo_gameState;
	int                       m_demo_profile;
	int                       m_demo_noinfo;
	int                       m_demo_save_every_frame;
	int                       m_demo_use_hmd_rotation;

	bool                      m_bAIEnabled;

	bool                      m_bDelayedPlayFlag;
	int                       m_prevGodMode;

	struct SChainDemoLevel
	{
		string level;    // Level name
		float  time;     // Time of test in seconds
		bool   bSuccess; // If test was successful.
		bool   bRun;     // If test was successful.
	};
	std::vector<SChainDemoLevel> m_demoLevels;
	int                          m_nCurrentDemoLevel;
	float                        m_lastChainDemoTime;
};

#endif // __timedemorecorder_h__
