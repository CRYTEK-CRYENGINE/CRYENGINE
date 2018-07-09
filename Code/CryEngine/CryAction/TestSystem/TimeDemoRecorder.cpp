// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   timedemorecorder.cpp
//  Version:     v1.00
//  Created:     2/8/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//  Pascal Kross, 18.05.2015, Added HMD support with TIMEDEMO_FILE_VERSION_7
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TimeDemoRecorder.h"
#include <CrySystem/File/CryFile.h>
#include <IActorSystem.h>
#include <CryAISystem/IAgent.h>
#include <ILevelSystem.h>
#include <CrySystem/ITestSystem.h>
#include <CryMovie/IMovieSystem.h>
#include "IMovementController.h"
#include <CrySystem/Profilers/IStatoscope.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <ITimeDemoRecorder.h>
#include <CrySystem/VR/IHMDManager.h>
#include <CrySystem/VR/IHMDDevice.h>
#include <CryCore/Platform/CryWindows.h>

#include <array>

//////////////////////////////////////////////////////////////////////////
// Brush Export structures.
//////////////////////////////////////////////////////////////////////////
#define TIMEDEMO_FILE_SIGNATURE         "CRY "
#define TIMEDEMO_FILE_TYPE              150
#define TIMEDEMO_FILE_VERSION_1         1
#define TIMEDEMO_FILE_VERSION_2         2
#define TIMEDEMO_FILE_VERSION_3         4 // ?
#define TIMEDEMO_FILE_VERSION_4         6
#define TIMEDEMO_FILE_VERSION_7         7
#define TIMEDEMO_FILE_VERSION           TIMEDEMO_FILE_VERSION_7

#define TIMEDEMO_MAX_INPUT_EVENTS       16
#define TIMEDEMO_MAX_GAME_EVENTS        1 // For now...
#define TIMEDEMO_MAX_DESCRIPTION_LENGTH 64

#define FIXED_TIME_STEP                 (30) // Assume running at 30fps.

enum ETimeDemoFileFlags
{
	eTimeDemoCompressed = 0x0001,
};

#pragma pack(push,1)
struct STimeDemoHeader
{
	char signature[4];  // File signature.
	int  filetype;      // File type.
	int  version;       // File version.
	int  nDataOffset;   // Offset where frame data starts.

	//////////////////////////////////////////////////////////////////////////
	int    numFrames;   // Number of frames.
	int    nFrameSize;  // Size of the per frame data in bytes.
	float  totalTime;
	char   levelname[128];
	// @see ETimeDemoFileFlags
	uint32 nDemoFlags;
	uint32 nCompressedDataSize;
	uint32 nUncompressedDataSze;
	char   reserved[116];

	//////////////////////////////////////////////////////////////////////////
	void SwapEndianThis()
	{
		SwapEndian(filetype);
		SwapEndian(version);
		SwapEndian(nDataOffset);
		SwapEndian(numFrames);
		SwapEndian(nFrameSize);
		SwapEndian(totalTime);
		SwapEndian(nDemoFlags);
		SwapEndian(nCompressedDataSize);
		SwapEndian(nUncompressedDataSze);
	}
};

//////////////////////////////////////////////////////////////////////////
struct STimeDemoHeader_4 : public STimeDemoHeader
{
	uint16 fixedTimeStep;

	void SwapEndianThis()
	{
		STimeDemoHeader::SwapEndianThis();
		SwapEndian(fixedTimeStep);
	}
};

//////////////////////////////////////////////////////////////////////////
struct STimeDemoFrame_1
{
	Vec3         curPlayerPosition;
	Ang3         angles;
	float        frametime;

	unsigned int nActionFlags[2];
	float        fLeaning;
	int          nPolygonsPerFrame;
	char         reserved[28];
};

//////////////////////////////////////////////////////////////////////////
struct STimeDemoFrameEvent_2
{
#ifndef NEED_ENDIAN_SWAP
	struct
	{
		uint16 deviceType : 4;
		uint16 state      : 4;
		uint16 modifiers  : 4;
		uint16 reserved   : 4;
	};
#else
	struct
	{
		uint16 state      : 4;
		uint16 deviceType : 4;
		uint16 reserved   : 4;
		uint16 modifiers  : 4;
	};
#endif
	uint16 keyId;
	float  value;

	void   SwapEndianThis()
	{
		SwapEndian(keyId); // this points to the first uint32 in structure
		SwapEndian(value);
	}
};
struct STimeDemoFrame_2
{
	Vec3                  curPlayerPosition;
	Ang3                  curCameraAngles;
	Quat                  curViewRotation;
	float                 frametime;

	unsigned int          nActionFlags[2];
	float                 fLeaning;
	int                   nPolygonsPerFrame;
	uint8                 numInputEvents;

	STimeDemoFrameEvent_2 inputEvents[TIMEDEMO_MAX_INPUT_EVENTS];
	char                  reserved[32];

	//////////////////////////////////////////////////////////////////////////
	void SwapEndianThis()
	{
		SwapEndian(curPlayerPosition);
		SwapEndian(curCameraAngles);
		SwapEndian(curViewRotation);
		SwapEndian(frametime);
		SwapEndian(nActionFlags[0]);
		SwapEndian(nActionFlags[1]);
		SwapEndian(fLeaning);
		SwapEndian(nPolygonsPerFrame);
		SwapEndian(numInputEvents);
		for (int i = 0; i < TIMEDEMO_MAX_INPUT_EVENTS; i++)
			inputEvents[i].SwapEndianThis();
	}
};

struct SRecordedGameEvent
{
	uint32 gameEventType;
	char   entityName[TIMEDEMO_MAX_DESCRIPTION_LENGTH];
	char   description[TIMEDEMO_MAX_DESCRIPTION_LENGTH];
	char   description2[TIMEDEMO_MAX_DESCRIPTION_LENGTH];
	float  value;
	int32  extra;

	void operator=(const STimeDemoGameEvent& event)
	{
		gameEventType = event.gameEventType;
		value = event.value;
		extra = event.extra;

		cry_strcpy(entityName, event.entityName.c_str());
		cry_strcpy(description, event.description.c_str());
		cry_strcpy(description2, event.description2.c_str());
	}

	void SwapEndianThis()
	{
		SwapEndian(extra);
		SwapEndian(value);
	}
};

STimeDemoGameEvent::STimeDemoGameEvent(const SRecordedGameEvent& event)
	: entityName(event.entityName)
	, gameEventType(event.gameEventType)
	, description(event.description)
	, description2(event.description2)
	, value(event.value)
	, extra(event.extra)
{}

struct STimeDemoFrame_3
{
	int                   nFrameDataSize; // Size of this frame in bytes.

	Vec3                  curPlayerPosition;
	Ang3                  curCameraAngles;
	Quat                  curViewRotation;
	float                 frametime;

	unsigned int          nActionFlags[2];
	float                 fLeaning;
	int                   nPolygonsPerFrame;
	int                   numInputEvents;
	STimeDemoFrameEvent_2 inputEvents[TIMEDEMO_MAX_INPUT_EVENTS];

	char                  reserved[32];

	//char data[]; // Special frame data.
	//////////////////////////////////////////////////////////////////////////
	void SwapEndianThis()
	{
		SwapEndian(curPlayerPosition);
		SwapEndian(curCameraAngles);
		SwapEndian(curViewRotation);
		SwapEndian(frametime);
		SwapEndian(nActionFlags[0]);
		SwapEndian(nActionFlags[1]);
		SwapEndian(fLeaning);
		SwapEndian(nPolygonsPerFrame);
		SwapEndian(numInputEvents);
		for (int i = 0; i < TIMEDEMO_MAX_INPUT_EVENTS; i++)
			inputEvents[i].SwapEndianThis();
	}
};

struct STimeDemoFrame_4
{
	int                   nFrameDataSize; // Size of this frame in bytes.

	Vec3                  curPlayerPosition;
	Ang3                  curCameraAngles;
	Quat                  curViewRotation;
	float                 frametime;

	unsigned int          nActionFlags[2];
	float                 fLeaning;
	int                   nPolygonsPerFrame;
	int                   numInputEvents;
	STimeDemoFrameEvent_2 inputEvents[TIMEDEMO_MAX_INPUT_EVENTS];
	int                   numGameEvents;
	SRecordedGameEvent    gameEvents[TIMEDEMO_MAX_GAME_EVENTS];

	uint32                bFollow; // if true, data from the next timedemo frame will be collected in this frame

	char                  reserved[32];

	//char data[]; // Special frame data.
	//////////////////////////////////////////////////////////////////////////
	void SwapEndianThis()
	{
		SwapEndian(curPlayerPosition);
		SwapEndian(curCameraAngles);
		SwapEndian(curViewRotation);
		SwapEndian(frametime);
		SwapEndian(nActionFlags[0]);
		SwapEndian(nActionFlags[1]);
		SwapEndian(fLeaning);
		SwapEndian(nPolygonsPerFrame);
		SwapEndian(numInputEvents);
		SwapEndian(numGameEvents);
		for (int i = 0; i < TIMEDEMO_MAX_INPUT_EVENTS; i++)
			inputEvents[i].SwapEndianThis();
		for (int i = 0; i < TIMEDEMO_MAX_GAME_EVENTS; i++)
			gameEvents[i].SwapEndianThis();
	}
};

struct STimeDemoFrame_7
{
	int                   nFrameDataSize;

	Vec3                  curPlayerPosition;
	Quat                  curPlayerRotation;
	Quat                  curViewRotation;
	Vec3                  curHmdPositionOffset;
	Quat                  curHmdViewRotation;
	float                 frametime;

	unsigned int          nActionFlags[2];
	float                 fLeaning;
	int                   nPolygonsPerFrame;
	int                   numInputEvents;
	STimeDemoFrameEvent_2 inputEvents[TIMEDEMO_MAX_INPUT_EVENTS];
	int                   numGameEvents;
	SRecordedGameEvent    gameEvents[TIMEDEMO_MAX_GAME_EVENTS];

	uint32                bFollow;

	char                  reserved[32];

	//////////////////////////////////////////////////////////////////////////
	void SwapEndianThis()
	{
		SwapEndian(curPlayerPosition);
		SwapEndian(curPlayerRotation);
		SwapEndian(curViewRotation);
		SwapEndian(curHmdPositionOffset);
		SwapEndian(curHmdViewRotation);
		SwapEndian(frametime);
		SwapEndian(nActionFlags[0]);
		SwapEndian(nActionFlags[1]);
		SwapEndian(fLeaning);
		SwapEndian(nPolygonsPerFrame);
		SwapEndian(numInputEvents);
		SwapEndian(numGameEvents);
		for (int i = 0; i < TIMEDEMO_MAX_INPUT_EVENTS; ++i)
		{
			inputEvents[i].SwapEndianThis();
		}
		for (int i = 0; i < TIMEDEMO_MAX_GAME_EVENTS; ++i)
		{
			gameEvents[i].SwapEndianThis();
		}
	}
};

#pragma pack(pop)

//////////////////////////////////////////////////////////////////////////

CTimeDemoRecorder* CTimeDemoRecorder::s_pTimeDemoRecorder = 0;
ICVar* CTimeDemoRecorder::s_timedemo_file = 0;

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StartRecordingTimeDemo(IConsoleCmdArgs* pArgs)
{
	if (s_pTimeDemoRecorder)
	{
		if (s_pTimeDemoRecorder->IsRecording())
			return;

		if (pArgs->GetArgCount() > 1)
		{
			s_timedemo_file->Set(pArgs->GetArg(1));
		}
		s_pTimeDemoRecorder->Record(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_Play(IConsoleCmdArgs* pArgs)
{
	if (s_pTimeDemoRecorder)
	{
		if (pArgs->GetArgCount() > 1)
		{
			s_timedemo_file->Set(pArgs->GetArg(1));
		}
		s_pTimeDemoRecorder->StartDemoDelayed();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_Stop(IConsoleCmdArgs* pArgs)
{
	if (s_pTimeDemoRecorder)
		s_pTimeDemoRecorder->Record(false);
	if (s_pTimeDemoRecorder)
		s_pTimeDemoRecorder->Play(false);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StartDemoChain(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 1)
	{
		const char* sLevelsFile = pArgs->GetArg(1);
		s_pTimeDemoRecorder->StartChainDemo(sLevelsFile, false);
	}
	else
	{
		s_pTimeDemoRecorder->StartChainDemo("test_chainlevels.txt", true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::cmd_StartDemoLevel(IConsoleCmdArgs* pArgs)
{
	const int argCount = pArgs->GetArgCount();
	if (argCount > 1)
	{
		const int levelCount = argCount - 1;

		std::vector<const char*> levels(levelCount);

		for (int i = 0; i < levelCount; ++i)
			levels[i] = pArgs->GetArg(i + 1);

		s_pTimeDemoRecorder->StartDemoLevel(&(levels.front()), levelCount);
	}
	else
	{
		CryLogAlways("Expect level name(s)");
	}
}

static void OnChange_demo_num_orientations(ICVar* pCVar)
{
	if (pCVar->GetIVal() < 1)
		pCVar->Set(1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CTimeDemoRecorder::CTimeDemoRecorder()
	: m_listeners(1)
	, m_bRecording(false)
	, m_bPlaying(false)
	, m_bPaused(false)
	, m_bDemoFinished(false)
	, m_bDemoEnded(false)
	, m_bChainloadingDemo(false)
	, m_currentFrame(0)
	, m_nTotalPolysRecorded(0)
	, m_nTotalPolysPlayed(0)
	, m_lastPlayedTotalTime(0)
	, m_lastAveFrameRate(0)
	, m_sumFPS(0)
	, m_minFPS(0)
	, m_maxFPS(0)
	, m_currFPS(0)
	, m_minFPSCounter(0)
	, m_minFPS_Frame(0)
	, m_maxFPS_Frame(0)
	, m_nCurrPolys(0)
	, m_nMaxPolys(0)
	, m_nMinPolys(0)
	, m_nPolysPerSec(0)
	, m_nPolysCounter(0)
	, m_fpsCounter(0)
	, m_fileVersion(TIMEDEMO_FILE_VERSION)
	, m_bEnabledProfiling(false)
	, m_bVisibleProfiling(false)
	, m_oldPeakTolerance(0.0f)
	, m_fixedTimeStep(0)
	, m_pTimeDemoInfo(nullptr)
	, m_numLoops(0)
	, m_bAIEnabled(false)
	, m_bDelayedPlayFlag(false)
	, m_prevGodMode(0)
	, m_nCurrentDemoLevel(0)
	, m_lastChainDemoTime(0.0f)
{
	s_pTimeDemoRecorder = this;

	//gEnv->pGameFramework->GetIGameplayRecorder()->EnableGameStateRecorder(false, this, false);

	CRY_ASSERT(GetISystem());

	// Register demo variables.
	s_timedemo_file = REGISTER_STRING("demo_file", "timedemo", 0, "Time Demo Filename");
	REGISTER_CVAR2("demo_game_state", &m_demo_gameState, 0, 0, "enable/disable the game state recording");
	REGISTER_CVAR2("demo_profile", &m_demo_profile, 1, 0, "Enable demo profiling");
	REGISTER_CVAR2("demo_noinfo", &m_demo_noinfo, 0, 0, "Disable info display during demo playback");

	REGISTER_COMMAND("record", &CTimeDemoRecorder::cmd_StartRecordingTimeDemo, 0,
	                 "Starts recording of a time demo.\n"
	                 "Usage: record demoname\n"
	                 "File 'demoname.tmd' will be created.");

	REGISTER_COMMAND("stoprecording", &CTimeDemoRecorder::cmd_Stop, 0,
	                 "Stops recording of a time demo.\n"
	                 "Usage: stoprecording\n"
	                 "File 'demoname.?' will be saved.");

	REGISTER_COMMAND("demo", &CTimeDemoRecorder::cmd_Play, 0,
	                 "Plays a time demo from file.\n"
	                 "Usage: demo demoname\n");

	REGISTER_COMMAND("stopdemo", &CTimeDemoRecorder::cmd_Stop, 0,
	                 "Stop playing a time demo.\n");

	REGISTER_COMMAND("demo_StartDemoChain", &CTimeDemoRecorder::cmd_StartDemoChain, 0, "Load's a file at 1st argument with the list of levels and play time demo on each\n");
	REGISTER_COMMAND("demo_StartDemoLevel", &CTimeDemoRecorder::cmd_StartDemoLevel, 0, "Prepares and starts time demos for the specified set of level names\n");

	REGISTER_CVAR2("demo_num_runs", &m_maxLoops, 1, 0, "Number of times to loop timedemo");
	REGISTER_CVAR2("demo_scroll_pause", &m_demo_scroll_pause, 1, 0, "ScrollLock pauses demo play/record");
	REGISTER_CVAR2("demo_quit", &m_demo_quit, 0, 0, "Quit game after demo runs finished");
	REGISTER_CVAR2("demo_finish_memreplay_sizer", &m_finish_replaysizer, 0, 0, "Add a crysizer tree to memreplay when demo is finished");
	REGISTER_CVAR2("demo_finish_memreplay_stop", &m_finish_replaystop, 0, 0, "Stop memreplay when demo is finished");
	REGISTER_CVAR2("demo_screenshot_frame", &m_demo_screenshot_frame, 0, 0, "Make screenshot on specified frame during demo playback, If Negative then do screen shoot every N frame");
	REGISTER_CVAR2("demo_max_frames", &m_demo_max_frames, 100000, 0, "Max number of frames to save");
	REGISTER_CVAR2("demo_savestats", &m_demo_savestats, 0, 0, "Save level stats at the end of the loop");
	REGISTER_CVAR2("demo_ai", &m_demo_ai, 1, 0, "Enable/Disable AI during the demo");

	// Note: Do not restart the level for Hunt, because the timedemo logic uses the regular map command and not mission which is doing other stuff.
	REGISTER_CVAR2("demo_restart_level", &m_demo_restart_level, 0, 0, "Restart level after each loop: 0 = Off; 1 = use quicksave on first playback; 2 = load level start");

	REGISTER_CVAR2("demo_panoramic", &m_demo_panoramic, 0, 0, "Panoramic view when playing back demo");
	REGISTER_CVAR2("demo_fixed_timestep", &m_demo_fixed_timestep, FIXED_TIME_STEP, 0, "number of updates per second");
	REGISTER_CVAR2("demo_vtune", &m_demo_vtune, 0, 0, "Enables VTune profiling when running time demo");
	REGISTER_CVAR2("demo_time_of_day", &m_demo_time_of_day, -1, 0, "Sets the time of day to override in game settings if not negative");
	REGISTER_CVAR2("demo_save_every_frame", &m_demo_save_every_frame, 0, 0, "Save timedemo every frame during recording, in case game crashes timedemo will be reliable");
	REGISTER_CVAR2("demo_use_hmd_rotation", &m_demo_use_hmd_rotation, 0, 0, "Uses alternative entity and view rotation for HMD Devices");

	REGISTER_STRING("demo_finish_cmd", "", 0, "Console command to run when demo is finished");

	REGISTER_CVAR2_CB("demo_num_orientations", &m_numOrientations, 1, 0, "Number of horizontal orientations to play the demo using\n"
	                                                                     "e.g. 3 will play: looking ahead, 120deg left, 120deg right\n"
	                                                                     "default/min: 1",
	                  OnChange_demo_num_orientations);
}

//////////////////////////////////////////////////////////////////////////
CTimeDemoRecorder::~CTimeDemoRecorder()
{
	s_pTimeDemoRecorder = 0;

	gEnv->pConsole->UnregisterVariable("demo_file");
	gEnv->pConsole->UnregisterVariable("demo_game_state");
	gEnv->pConsole->UnregisterVariable("demo_profile");
	gEnv->pConsole->UnregisterVariable("demo_noinfo");
	
	gEnv->pConsole->RemoveCommand("record");
	gEnv->pConsole->RemoveCommand("stoprecording");
	gEnv->pConsole->RemoveCommand("demo");
	gEnv->pConsole->RemoveCommand("stopdemo");
	gEnv->pConsole->RemoveCommand("demo_StartDemoChain");
	gEnv->pConsole->RemoveCommand("demo_StartDemoLevel");

	gEnv->pConsole->UnregisterVariable("demo_num_runs");
	gEnv->pConsole->UnregisterVariable("demo_scroll_pause");
	gEnv->pConsole->UnregisterVariable("demo_quit");
	gEnv->pConsole->UnregisterVariable("demo_finish_memreplay_sizer");
	gEnv->pConsole->UnregisterVariable("demo_finish_memreplay_stop");
	gEnv->pConsole->UnregisterVariable("demo_screenshot_frame");
	gEnv->pConsole->UnregisterVariable("demo_max_frames");
	gEnv->pConsole->UnregisterVariable("demo_savestats");
	gEnv->pConsole->UnregisterVariable("demo_ai");
	gEnv->pConsole->UnregisterVariable("demo_restart_level");
	gEnv->pConsole->UnregisterVariable("demo_panoramic");
	gEnv->pConsole->UnregisterVariable("demo_fixed_timestep");
	gEnv->pConsole->UnregisterVariable("demo_vtune");
	gEnv->pConsole->UnregisterVariable("demo_time_of_day");
	gEnv->pConsole->UnregisterVariable("demo_save_every_frame");
	gEnv->pConsole->UnregisterVariable("demo_use_hmd_rotation");
	gEnv->pConsole->UnregisterVariable("demo_finish_cmd");
	gEnv->pConsole->UnregisterVariable("demo_num_orientations");
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Reset()
{
	m_bRecording = false;
	m_bPlaying = false;
	m_bDemoFinished = false;
	m_bPaused = false;
	m_bChainloadingDemo = false;
	m_bDemoEnded = false;
}

//////////////////////////////////////////////////////////////////////////
const char* CTimeDemoRecorder::GetCurrentLevelPath()
{
	static char buf[_MAX_PATH];
	gEnv->pGameFramework->GetAbsLevelPath(buf, sizeof(buf));
	return buf;
	/*
	   ILevel *pLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel();
	   if (!pLevel)
	    return "";
	   ILevelInfo *pLevelInfo = pLevel->GetLevelInfo();
	   if (!pLevelInfo)
	    return "";
	   return pLevelInfo->GetPath();
	 */
}

std::array<EEntityEvent, 8> g_recordedEntityEvents =
{
	{
		ENTITY_EVENT_XFORM,
		ENTITY_EVENT_HIDE,
		ENTITY_EVENT_UNHIDE,
		ENTITY_EVENT_ATTACH,
		ENTITY_EVENT_DETACH,
		ENTITY_EVENT_DETACH_THIS,
		ENTITY_EVENT_ENABLE_PHYSICS,
		ENTITY_EVENT_ENTER_SCRIPT_STATE
	}
};

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Record(bool bEnable)
{
	if (bEnable == m_bRecording)
		return;

	if (bEnable == m_bRecording)
		return;

	if (gEnv->pMovieSystem)
		gEnv->pMovieSystem->StopAllSequences();

	m_bRecording = bEnable;
	m_bPlaying = false;
	if (m_bRecording)
	{
		SaveAllEntitiesState();

		gEnv->pEntitySystem->AddSink(this, IEntitySystem::OnSpawn);

		IEntityItPtr pEntityIter = gEnv->pEntitySystem->GetEntityIterator();
		while (IEntity* pEntity = pEntityIter->Next())
		{
			for (const EEntityEvent event : g_recordedEntityEvents)
			{
				pEntity->AddEventListener(event, this);
			}
		}

		// Start recording.
		m_records.clear();
		m_records.reserve(1000);

		m_currentFrameInputEvents.clear();
		m_currentFrameEntityEvents.clear();
		// Start listening input events.
		if (gEnv->pInput)
			gEnv->pInput->AddEventListener(this);

		StartSession();

		m_recordStartTime = GetTime();
		m_lastFrameTime = m_recordStartTime;
	}
	else
	{
		// Stop recording.
		m_recordedDemoTime = m_totalDemoTime;
		m_lastFrameTime = GetTime();

		gEnv->pEntitySystem->RemoveSink(this);

		IEntityItPtr pEntityIter = gEnv->pEntitySystem->GetEntityIterator();
		while (IEntity* pEntity = pEntityIter->Next())
		{
			for (const EEntityEvent event : g_recordedEntityEvents)
			{
				pEntity->RemoveEventListener(event, this);
			}
		}

		m_currentFrameInputEvents.clear();
		m_currentFrameEntityEvents.clear();
		// Stop listening tho the input events.
		if (gEnv->pInput)
			gEnv->pInput->RemoveEventListener(this);

		StopSession();

		// Save after stopping.
		string filename = PathUtil::Make(GetCurrentLevelPath(), s_timedemo_file->GetString(), "tmd");
		s_pTimeDemoRecorder->Save(filename.c_str());
	}
	m_currentFrame = 0;
	m_totalDemoTime.SetMilliSeconds(0);

	SignalRecording(bEnable);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Play(bool bEnable)
{
	if (bEnable == m_bPlaying)
		return;

	if (bEnable)
	{
		CRY_ASSERT(strlen(GetCurrentLevelPath()));

		// Try to load demo file.
		string filename = PathUtil::Make(GetCurrentLevelPath(), s_timedemo_file->GetString(), "tmd");

		// Put it back later!
		Load(filename);

		if (m_records.empty())
		{
			m_bDemoFinished = true;
			return;
		}

		if (gEnv->pStatoscope)
			gEnv->pStatoscope->AddUserMarker("TimeDemo", "Start playing");
	}
	else
	{
		if (m_bPlaying)
		{
			// Turning off playback.
			if (m_demo_savestats != 0)
			{
				if (m_nCurrentDemoLevel >= m_demoLevels.size())
				{
					// Save stats after last run only.
					gEnv->pConsole->ExecuteString("SaveLevelStats");
				}
			}

			if (gEnv->pStatoscope)
				gEnv->pStatoscope->AddUserMarker("TimeDemo", "Stop playing");
		}
	}

	m_bPlaying = bEnable;

	IActor* pClActor = gEnv->pGameFramework->GetClientActor();
	if (pClActor)
	{
		IAnimatedCharacter* pAnimChar = pClActor->GetAnimatedCharacter();
		if (pAnimChar)
			pAnimChar->SetNoMovementOverride(m_bPlaying);
	}

	if (m_bPlaying)
	{
		string levelName = GetCurrentLevelName();
		LogInfo("==============================================================");
		LogInfo("TimeDemo Play Started ,Level=%s (Total Frames: %d, Recorded Time: %.2fs)", levelName.c_str(), (int)m_records.size(), m_recordedDemoTime.GetSeconds());

		m_bDemoFinished = false;

		RestoreAllEntitiesState();

		// Start demo playback.
		m_lastPlayedTotalTime = 0;
		StartSession();
	}
	else
	{
		LogInfo("AutoTest Play Ended, (%d Runs Performed)", m_numLoops);
		LogInfo("==============================================================");

		// End demo playback.
		m_lastPlayedTotalTime = m_totalDemoTime.GetSeconds();
		StopSession();
	}
	m_bRecording = false;
	m_currentFrame = 0;
	m_totalDemoTime.SetMilliSeconds(0);
	m_lastFpsTimeRecorded = GetTime();

	m_numLoops = 0;
	m_fpsCounter = 0;
	m_lastFpsTimeRecorded = GetTime();

	m_currFPS = 0;
	m_sumFPS = 0;
	m_minFPS = 10000;
	m_maxFPS = -10000;
	m_nMaxPolys = INT_MIN;
	m_nMinPolys = INT_MAX;

	SignalPlayback(bEnable);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::Save(const char* filename)
{
	// Not save empty file.
	if (m_records.empty())
		return;

	m_file = filename;

	// Save Time demo file.
	// #if TIMEDEMO_FILE_VERSION== TIMEDEMO_FILE_VERSION_4
	//  STimeDemoHeader_4 hdr;
	// #else
	STimeDemoHeader hdr;
	//#endif

	memset(&hdr, 0, sizeof(hdr));

	cry_strcpy(hdr.levelname, GetCurrentLevelPath());
	hdr.nDataOffset = sizeof(hdr);
	//hdr.nFrameSize = sizeof(STimeDemoFrame_3);

	memcpy(hdr.signature, TIMEDEMO_FILE_SIGNATURE, 4);
	hdr.filetype = TIMEDEMO_FILE_TYPE;
	hdr.version = TIMEDEMO_FILE_VERSION;

#if TIMEDEMO_FILE_VERSION == TIMEDEMO_FILE_VERSION_3

	hdr.nFrameSize = sizeof(STimeDemoFrame_3);
	hdr.nDemoFlags = 0;
	hdr.numFrames = m_records.size();
	hdr.totalTime = m_recordedDemoTime.GetSeconds();

	hdr.nUncompressedDataSze = hdr.nFrameSize * hdr.numFrames;

	std::vector<STimeDemoFrame_3> file_records;
	file_records.resize(hdr.numFrames);

	for (int i = 0; i < hdr.numFrames; i++)
	{
		FrameRecord& rec = m_records[i];
		STimeDemoFrame_3& frame = file_records[i];
		ZeroStruct(frame);
		ZeroStruct(frame.inputEvents);
		frame.nFrameDataSize = sizeof(frame);
		frame.curViewRotation = rec.playerViewRotation;
		frame.curCameraAngles = rec.cameraAngles;
		frame.curPlayerPosition = rec.playerPosition;
		frame.frametime = rec.frameTime;
		*frame.nActionFlags = *rec.nActionFlags;
		frame.fLeaning = rec.fLeaning;
		frame.nPolygonsPerFrame = rec.nPolygons;
		frame.numInputEvents = 0;
		for (InputEventsList::const_iterator it = rec.inputEventsList.begin();
		     it != rec.inputEventsList.end() && frame.numInputEvents < TIMEDEMO_MAX_INPUT_EVENTS; ++it)
		{
			const SInputEvent& inputEvent = *it;
			frame.inputEvents[frame.numInputEvents].deviceType = inputEvent.deviceType;
			frame.inputEvents[frame.numInputEvents].modifiers = inputEvent.modifiers;
			frame.inputEvents[frame.numInputEvents].state = inputEvent.state;
			frame.inputEvents[frame.numInputEvents].keyId = inputEvent.keyId;
			frame.inputEvents[frame.numInputEvents].value = inputEvent.value;
			frame.numInputEvents++;
		}
	}

#endif
#if TIMEDEMO_FILE_VERSION == TIMEDEMO_FILE_VERSION_7

	hdr.nFrameSize = sizeof(STimeDemoFrame_7);
	hdr.nDemoFlags = 0;
	hdr.numFrames = m_records.size();

	float* pHdrFixedTimeStep = (float*)hdr.reserved;
	*pHdrFixedTimeStep = (float)m_demo_fixed_timestep;
	// check possible extra frames for carrying extra game events

	int n = hdr.numFrames;
	for (int i = 0; i < n; i++)
	{
		FrameRecord& rec = m_records[i];
		int gesize = rec.gameEvents.size();
		if (gesize > TIMEDEMO_MAX_GAME_EVENTS)
			hdr.numFrames += (gesize - 1) / TIMEDEMO_MAX_GAME_EVENTS;
	}

	hdr.totalTime = m_recordedDemoTime.GetSeconds();
	hdr.nUncompressedDataSze = hdr.nFrameSize * hdr.numFrames;

	std::vector<STimeDemoFrame_7> file_records;
	file_records.resize(hdr.numFrames);

	for (int i = 0, fr = 0; fr < hdr.numFrames; ++i, ++fr)
	{
		FrameRecord& rec = m_records[i];

		STimeDemoFrame_7& frame = file_records[fr];

		frame.curPlayerPosition = rec.playerPosition;
		frame.curPlayerRotation = rec.playerRotation;
		frame.curViewRotation = rec.playerViewRotation;
		frame.curHmdPositionOffset = rec.hmdPositionOffset;
		frame.curHmdViewRotation = rec.hmdViewRotation;
		frame.frametime = rec.frameTime;
		*frame.nActionFlags = *rec.nActionFlags;
		frame.fLeaning = rec.fLeaning;
		frame.nPolygonsPerFrame = rec.nPolygons;

		//input events
		frame.numInputEvents = 0;
		frame.bFollow = 0;

		ZeroStruct(frame.inputEvents);
		ZeroStruct(frame.gameEvents);
		for (InputEventsList::const_iterator it = rec.inputEventsList.begin();
		     it != rec.inputEventsList.end() && frame.numInputEvents < TIMEDEMO_MAX_INPUT_EVENTS; ++it)
		{
			const SInputEvent& inputEvent = *it;
			frame.inputEvents[frame.numInputEvents].deviceType = inputEvent.deviceType;
			frame.inputEvents[frame.numInputEvents].modifiers = inputEvent.modifiers;
			frame.inputEvents[frame.numInputEvents].state = inputEvent.state;
			frame.inputEvents[frame.numInputEvents].keyId = inputEvent.keyId;
			frame.inputEvents[frame.numInputEvents].value = inputEvent.value;
			frame.numInputEvents++;
		}

		// game events
		// LEAVE THE GAME EVENTS FOR LAST (extended frames)
		frame.numGameEvents = rec.gameEvents.size();

		int remainingEvents = frame.numGameEvents - TIMEDEMO_MAX_GAME_EVENTS;
		if (frame.numGameEvents > TIMEDEMO_MAX_GAME_EVENTS)
			frame.numGameEvents = TIMEDEMO_MAX_GAME_EVENTS;

		int fc = 0;
		for (int j = 0; j < frame.numGameEvents; j++)
			frame.gameEvents[j] = rec.gameEvents[fc++];

		bool bExtended = false;

		STimeDemoFrame_7* pAddedFrame = &frame;
		if (remainingEvents > 0)
			bExtended = true;
		while (remainingEvents > 0)
		{
			//	GameWarning("Timedemo: Exceeding number of game events in frame %i. Those game events will not be recorded.",i);
			pAddedFrame->bFollow = 1;
			fr++;
			pAddedFrame = &file_records[fr];
			pAddedFrame->numGameEvents = min(remainingEvents, TIMEDEMO_MAX_GAME_EVENTS);
			remainingEvents -= TIMEDEMO_MAX_GAME_EVENTS;
			CRY_ASSERT(pAddedFrame->numGameEvents < TIMEDEMO_MAX_GAME_EVENTS);
			for (int j = 0; j < pAddedFrame->numGameEvents; j++)
			{
				pAddedFrame->gameEvents[j] = rec.gameEvents[fc++];
			}
		}

		if (bExtended)
			pAddedFrame->bFollow = 0;
	}

#endif

	//////////////////////////////////////////////////////////////////////////
	// Save to file.
	//////////////////////////////////////////////////////////////////////////

	CCryFile file;
	if (!file.Open(filename, "wb"))
	{
		GetISystem()->Warning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, 0, filename, "Cannot open time demo file %s", filename);
		return;
	}

	size_t nCompressedSize = hdr.nUncompressedDataSze * 2 + 1024;
	char* pCompressedData = new char[nCompressedSize];
	if (GetISystem()->CompressDataBlock(&file_records[0], hdr.nUncompressedDataSze, pCompressedData, nCompressedSize))
	{
		// Save compressed.
		hdr.nCompressedDataSize = nCompressedSize;
		hdr.nDemoFlags |= eTimeDemoCompressed;

		file.Write(&hdr, sizeof(hdr));
		file.Write(pCompressedData, hdr.nCompressedDataSize);
	}
	else
	{
		// Save uncompressed.
		file.Write(&hdr, sizeof(hdr));
		file.Write(&file_records[0], hdr.nUncompressedDataSze);
	}

	delete[]pCompressedData;

	/*
	   XmlNodeRef root = GetISystem()->CreateXmlNode( "TimeDemo" );
	   root->setAttr( "TotalTime",m_recordedDemoTime );
	   for (FrameRecords::iterator it = m_records.begin(); it != m_records.end(); ++it)
	   {
	    FrameRecord &rec = *it;
	    XmlNodeRef xmlRecord = root->newChild( "Frame" );
	    xmlRecord->setAttr( "Pos",rec.playerPos );
	    xmlRecord->setAttr( "Ang",rec.playerRotation );
	    xmlRecord->setAttr( "Time",rec.frameTime );
	   }
	   root->saveToFile( filename );
	 */
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::AddFrameRecord(const FrameRecord& rec)
{
	m_records.push_back(rec);
}

//////////////////////////////////////////////////////////////////////////
bool CTimeDemoRecorder::Load(const char* filename)
{
	// ignore invalid file access for time demo playback
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	stl::free_container(m_records);
	m_recordedDemoTime.SetMilliSeconds(0);
	m_totalDemoTime.SetMilliSeconds(0);

	IInput* pIInput = GetISystem()->GetIInput(); // Cache IInput pointer.

	CCryFile file;
	if (!file.Open(filename, "rb", ICryPak::FOPEN_ONDISK))
	{
		char str[256];
		CryGetCurrentDirectory(sizeof(str), str);

		GetISystem()->Warning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, 0, filename, "Cannot open time demo file %s (%s)", filename, str);
		return false;
	}

	// Load Time demo file.
	// #if TIMEDEMO_FILE_VERSION== TIMEDEMO_FILE_VERSION_4
	//  STimeDemoHeader_4 hdr;
	// #else
	STimeDemoHeader hdr;
	//#endif

	file.ReadRaw(&hdr, sizeof(hdr));

	if (hdr.signature[0] == 'C' && hdr.signature[1] == 'R' && hdr.signature[2] == 'Y' && (uint8)hdr.signature[3] == 0x96)
	{
		// This is old header.
		file.Seek(0, SEEK_SET);
		file.ReadRaw(((char*)&hdr) + 1, sizeof(hdr) - 1); // Old header was 1 byte shorter at the signature.
	}

	// Swap endian if needed.
	hdr.SwapEndianThis();

	m_recordedDemoTime = hdr.totalTime;
	m_totalDemoTime = m_recordedDemoTime;

	m_file = filename;
	m_records.reserve(hdr.numFrames);

	m_fileVersion = hdr.version;

	const float fixedTimeStepMin = 0.0f;
	const float fixedTimeStepMax = 1000.f;

	switch (m_fileVersion)
	{
	case TIMEDEMO_FILE_VERSION_1:
		{
			for (int i = 0; i < hdr.numFrames && !file.IsEof(); i++)
			{
				STimeDemoFrame_1 frame;
				FrameRecord rec;
				file.ReadRaw(&frame, sizeof(frame));

				Quat rot;
				rot.SetRotationXYZ(Ang3(DEG2RAD(frame.angles)));
				rot = rot * Quat::CreateRotationZ(gf_PI); // to fix some game to camera rotation issues.
				rec.playerViewRotation = rot;
				rec.cameraAngles = frame.angles;
				rec.playerPosition = frame.curPlayerPosition;
				rec.frameTime = frame.frametime;
				*rec.nActionFlags = *frame.nActionFlags;
				rec.fLeaning = frame.fLeaning;
				rec.nPolygons = frame.nPolygonsPerFrame;
				AddFrameRecord(rec);
			}
		}
		break;

	case TIMEDEMO_FILE_VERSION_2:
		{
			char* pFrameData = new char[hdr.nUncompressedDataSze];
			if (hdr.nDemoFlags & eTimeDemoCompressed)
			{
				char* pCompressedData = new char[hdr.nCompressedDataSize];
				// Read Compressed.
				file.ReadRaw(pCompressedData, hdr.nCompressedDataSize);

				// Uncompress data.
				size_t uncompressedSize = hdr.nUncompressedDataSze;
				GetISystem()->DecompressDataBlock(pCompressedData, hdr.nCompressedDataSize, pFrameData, uncompressedSize);
				CRY_ASSERT(uncompressedSize == hdr.nUncompressedDataSze);
				if (uncompressedSize != hdr.nUncompressedDataSze)
				{
					GameWarning("Corrupted compressed time demo file %s", filename);
					delete[]pCompressedData;
					return false;
				}
				delete[]pCompressedData;
			}
			else
			{
				// Read Uncompressed.
				if (file.ReadRaw(pFrameData, hdr.nUncompressedDataSze) != hdr.nUncompressedDataSze)
				{
					GameWarning("Corrupted time demo file %s", filename);
					return false;
				}
			}
			CRY_ASSERT(sizeof(STimeDemoFrame_2) * hdr.numFrames == hdr.nUncompressedDataSze);
			if (sizeof(STimeDemoFrame_2) * hdr.numFrames != hdr.nUncompressedDataSze)
			{
				GameWarning("Corrupted time demo file %s", filename);
				return false;
			}
			STimeDemoFrame_2* pFileFrame = (STimeDemoFrame_2*)pFrameData;
			for (int i = 0; i < hdr.numFrames; i++)
			{
				STimeDemoFrame_2& frame = *pFileFrame++;
				frame.SwapEndianThis(); // Swap endian if needed

				FrameRecord rec;

				rec.playerViewRotation = frame.curViewRotation;
				rec.cameraAngles = frame.curCameraAngles;
				rec.playerPosition = frame.curPlayerPosition;
				rec.frameTime = frame.frametime;
				*rec.nActionFlags = *frame.nActionFlags;
				rec.fLeaning = frame.fLeaning;
				rec.nPolygons = frame.nPolygonsPerFrame;
				if (frame.numInputEvents > 0)
				{
					for (int j = 0; j < frame.numInputEvents; j++)
					{
						SInputEvent inputEvent;
						inputEvent.deviceType = (EInputDeviceType)frame.inputEvents[j].deviceType;
						inputEvent.modifiers = frame.inputEvents[j].modifiers;
						inputEvent.state = (EInputState)frame.inputEvents[j].state;
						inputEvent.keyId = (EKeyId)frame.inputEvents[j].keyId;
						inputEvent.value = frame.inputEvents[j].value;
						SInputSymbol* pInputSymbol = pIInput ? pIInput->LookupSymbol(inputEvent.deviceType, 0, inputEvent.keyId) : 0;
						if (pInputSymbol)
							inputEvent.keyName = pInputSymbol->name;
						rec.inputEventsList.push_back(inputEvent);
					}
				}
				AddFrameRecord(rec);
			}

			delete[]pFrameData;
		}
		break;

	case TIMEDEMO_FILE_VERSION_3:
		{
			char* pFrameData = new char[hdr.nUncompressedDataSze];
			if (hdr.nDemoFlags & eTimeDemoCompressed)
			{
				char* pCompressedData = new char[hdr.nCompressedDataSize];
				// Read Compressed.
				file.ReadRaw(pCompressedData, hdr.nCompressedDataSize);

				// Uncompress data.
				size_t uncompressedSize = hdr.nUncompressedDataSze;
				GetISystem()->DecompressDataBlock(pCompressedData, hdr.nCompressedDataSize, pFrameData, uncompressedSize);
				CRY_ASSERT(uncompressedSize == hdr.nUncompressedDataSze);
				if (uncompressedSize != hdr.nUncompressedDataSze)
				{
					GameWarning("Corrupted compressed time demo file %s", filename);
					delete[]pCompressedData;
					return false;
				}
				delete[]pCompressedData;
			}
			else
			{
				// Read Uncompressed.
				if (file.ReadRaw(pFrameData, hdr.nUncompressedDataSze) != hdr.nUncompressedDataSze)
				{
					GameWarning("Corrupted time demo file %s", filename);
					return false;
				}
			}

			CRY_ASSERT(sizeof(STimeDemoFrame_3) * hdr.numFrames == hdr.nUncompressedDataSze);
			if (sizeof(STimeDemoFrame_3) * hdr.numFrames != hdr.nUncompressedDataSze)
			{
				GameWarning("Corrupted time demo file %s", filename);
				return false;
			}

			STimeDemoFrame_3* pFileFrame = (STimeDemoFrame_3*)pFrameData;
			for (int i = 0; i < hdr.numFrames; i++)
			{
				STimeDemoFrame_3& frame = *pFileFrame++;
				frame.SwapEndianThis(); // Swap endian if needed
				FrameRecord rec;

				rec.playerViewRotation = frame.curViewRotation;
				rec.cameraAngles = frame.curCameraAngles;
				rec.playerPosition = frame.curPlayerPosition;
				rec.frameTime = frame.frametime;
				*rec.nActionFlags = *frame.nActionFlags;
				rec.fLeaning = frame.fLeaning;
				rec.nPolygons = frame.nPolygonsPerFrame;

				if (frame.numInputEvents > 0)
				{
					for (int j = 0; j < frame.numInputEvents; j++)
					{
						SInputEvent inputEvent;
						inputEvent.deviceType = (EInputDeviceType)frame.inputEvents[j].deviceType;
						inputEvent.modifiers = frame.inputEvents[j].modifiers;
						inputEvent.state = (EInputState)frame.inputEvents[j].state;
						inputEvent.keyId = (EKeyId)frame.inputEvents[j].keyId;
						inputEvent.value = frame.inputEvents[j].value;
						SInputSymbol* pInputSymbol = pIInput ? pIInput->LookupSymbol(inputEvent.deviceType, 0, inputEvent.keyId) : 0;
						if (pInputSymbol)
							inputEvent.keyName = pInputSymbol->name;
						rec.inputEventsList.push_back(inputEvent);
					}
				}

				AddFrameRecord(rec);
			}

			delete[]pFrameData;
		}
		break;

	case TIMEDEMO_FILE_VERSION_4:
		{
			float recFixedTimeStep = *(float*)(hdr.reserved);
			if (recFixedTimeStep > 0 && recFixedTimeStep < 1000)
				m_demo_fixed_timestep = (uint16)recFixedTimeStep;

			char* pFrameData = new char[hdr.nUncompressedDataSze];
			if (hdr.nDemoFlags & eTimeDemoCompressed)
			{
				char* pCompressedData = new char[hdr.nCompressedDataSize];
				// Read Compressed.
				file.ReadRaw(pCompressedData, hdr.nCompressedDataSize);

				// Uncompress data.
				size_t uncompressedSize = hdr.nUncompressedDataSze;
				GetISystem()->DecompressDataBlock(pCompressedData, hdr.nCompressedDataSize, pFrameData, uncompressedSize);
				CRY_ASSERT(uncompressedSize == hdr.nUncompressedDataSze);
				if (uncompressedSize != hdr.nUncompressedDataSze)
				{
					GameWarning("Corrupted compressed time demo file %s", filename);
					delete[]pCompressedData;
					return false;
				}
				delete[]pCompressedData;
			}
			else
			{
				// Read Uncompressed.
				if (file.ReadRaw(pFrameData, hdr.nUncompressedDataSze) != hdr.nUncompressedDataSze)
				{
					GameWarning("Corrupted time demo file %s", filename);
					return false;
				}
			}

			CRY_ASSERT(sizeof(STimeDemoFrame_4) * hdr.numFrames == hdr.nUncompressedDataSze);
			if (sizeof(STimeDemoFrame_4) * hdr.numFrames != hdr.nUncompressedDataSze)
			{
				GameWarning("Corrupted time demo file %s", filename);
				return false;
			}

			STimeDemoFrame_4* pFileFrame = (STimeDemoFrame_4*)pFrameData;
			for (int i = 0; i < hdr.numFrames; i++)
			{
				STimeDemoFrame_4& frame = *pFileFrame++;
				frame.SwapEndianThis(); // Swap endian if needed
				FrameRecord rec;

				rec.playerViewRotation = frame.curViewRotation;
				rec.cameraAngles = frame.curCameraAngles;
				rec.playerPosition = frame.curPlayerPosition;
				rec.frameTime = frame.frametime;
				*rec.nActionFlags = *frame.nActionFlags;
				rec.fLeaning = frame.fLeaning;
				rec.nPolygons = frame.nPolygonsPerFrame;

				if (frame.numInputEvents > 0)
				{
					for (int j = 0; j < frame.numInputEvents; j++)
					{
						SInputEvent inputEvent;
						inputEvent.deviceType = (EInputDeviceType)frame.inputEvents[j].deviceType;
						inputEvent.modifiers = frame.inputEvents[j].modifiers;
						inputEvent.state = (EInputState)frame.inputEvents[j].state;
						inputEvent.keyId = (EKeyId)frame.inputEvents[j].keyId;
						inputEvent.value = frame.inputEvents[j].value;
						SInputSymbol* pInputSymbol = pIInput ? pIInput->LookupSymbol(inputEvent.deviceType, 0, inputEvent.keyId) : 0;
						if (pInputSymbol)
							inputEvent.keyName = pInputSymbol->name;
						rec.inputEventsList.push_back(inputEvent);
					}
				}

				if (frame.numGameEvents > 0)
				{
					for (int j = 0; j < frame.numGameEvents; j++)
						rec.gameEvents.push_back(frame.gameEvents[j]);
				}

				STimeDemoFrame_4* pAddFrame = &frame;
				while (pAddFrame->bFollow && i < hdr.numFrames - 1)
				{
					++i;
					pAddFrame = pFileFrame++;
					for (int j = 0; j < pAddFrame->numGameEvents; j++)
						rec.gameEvents.push_back(pAddFrame->gameEvents[j]);
				}

				AddFrameRecord(rec);
			}

			delete[]pFrameData;
		}
		break;

	case TIMEDEMO_FILE_VERSION_7:
		{
			float recFixedTimeStep = *(float*)(hdr.reserved);

			if (recFixedTimeStep > fixedTimeStepMin && recFixedTimeStep < fixedTimeStepMax)
			{
				m_demo_fixed_timestep = (uint16)recFixedTimeStep;
			}

			std::vector<char> frameData;
			frameData.resize(hdr.nUncompressedDataSze);

			if (hdr.nDemoFlags & eTimeDemoCompressed)
			{
				std::vector<char> compressedData;
				compressedData.resize(hdr.nCompressedDataSize);

				// Read Compressed.
				file.ReadRaw(compressedData.data(), compressedData.size());

				// Uncompress data.
				size_t uncompressedSize = frameData.size();
				GetISystem()->DecompressDataBlock(compressedData.data(), compressedData.size(), frameData.data(), uncompressedSize);
				CRY_ASSERT(uncompressedSize == frameData.size());

				if (uncompressedSize != frameData.size())
				{
					GameWarning("Corrupted compressed time demo file %s", filename);
					return false;
				}
			}
			else
			{
				// Read Uncompressed.
				if (file.ReadRaw(frameData.data(), frameData.size()) != frameData.size())
				{
					GameWarning("Corrupted time demo file %s", filename);
					return false;
				}
			}

			CRY_ASSERT(sizeof(STimeDemoFrame_7) * hdr.numFrames == hdr.nUncompressedDataSze);
			if (sizeof(STimeDemoFrame_7) * hdr.numFrames != hdr.nUncompressedDataSze)
			{
				GameWarning("Corrupted time demo file %s", filename);
				return false;
			}

			STimeDemoFrame_7* pFileFrame = (STimeDemoFrame_7*)(frameData.data());

			for (int i = 0; i < hdr.numFrames; ++i)
			{
				STimeDemoFrame_7& frame = *pFileFrame++;
				frame.SwapEndianThis(); // Swap endian if needed

				FrameRecord rec;
				rec.playerPosition = frame.curPlayerPosition;
				rec.playerRotation = frame.curPlayerRotation;
				rec.playerViewRotation = frame.curViewRotation;
				rec.hmdPositionOffset = frame.curHmdPositionOffset;
				rec.hmdViewRotation = frame.curHmdViewRotation;
				rec.frameTime = frame.frametime;
				*rec.nActionFlags = *frame.nActionFlags;
				rec.fLeaning = frame.fLeaning;
				rec.nPolygons = frame.nPolygonsPerFrame;

				if (frame.numInputEvents > 0)
				{
					rec.inputEventsList.reserve(frame.numInputEvents);

					for (int j = 0; j < frame.numInputEvents; ++j)
					{
						SInputEvent inputEvent;
						inputEvent.deviceType = (EInputDeviceType)frame.inputEvents[j].deviceType;
						inputEvent.modifiers = frame.inputEvents[j].modifiers;
						inputEvent.state = (EInputState)frame.inputEvents[j].state;
						inputEvent.keyId = (EKeyId)frame.inputEvents[j].keyId;
						inputEvent.value = frame.inputEvents[j].value;
						SInputSymbol* pInputSymbol = pIInput ? pIInput->LookupSymbol(inputEvent.deviceType, 0, inputEvent.keyId) : 0;
						if (pInputSymbol)
						{
							inputEvent.keyName = pInputSymbol->name;
						}
						rec.inputEventsList.push_back(inputEvent);
					}
				}

				if (frame.numGameEvents > 0)
				{
					rec.gameEvents.reserve(frame.numGameEvents);

					for (int j = 0; j < frame.numGameEvents; ++j)
					{
						rec.gameEvents.push_back(frame.gameEvents[j]);
					}
				}

				STimeDemoFrame_7* pAddFrame = &frame;
				while (pAddFrame->bFollow && i < hdr.numFrames - 1)
				{
					++i;
					pAddFrame = pFileFrame++;
					rec.gameEvents.reserve(pAddFrame->numGameEvents);

					for (int j = 0; j < pAddFrame->numGameEvents; ++j)
					{
						rec.gameEvents.push_back(pAddFrame->gameEvents[j]);
					}
				}

				AddFrameRecord(rec);
			}
		}
		break;

	default:
		{
			GameWarning("Timedemo: Unknown file version");
		}
		break;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::PreUpdate()
{
	if (m_bPlaying && !m_bPaused && !gEnv->pConsole->GetStatus()) // Only when console closed.
	{
		bool bPlaying = PlayFrame();
		if (!bPlaying) // No more frames
		{
			// Stop playing if max runs reached.
			LogEndOfLoop();
			m_numLoops++;

			if (m_numLoops >= m_maxLoops * m_numOrientations)
			{
				Play(false);
				m_bDemoFinished = true;
			}
			else
			{
				ResetSessionLoop();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::PostUpdate()
{
	if (gEnv->pSystem->IsQuitting())
	{
		return;
	}

	if (!m_bDelayedPlayFlag && !m_bPlaying && m_bDemoFinished)
	{
		if (!m_demoLevels.empty())
		{
			StartNextChainedLevel();
			return;
		}
		ICVar* pFinishCmd = gEnv->pConsole->GetCVar("demo_finish_cmd");
		if (pFinishCmd && !m_bDemoEnded)
		{
			const char* const szFinishCmd = pFinishCmd->GetString();
			if (szFinishCmd && szFinishCmd[0] != '\0')
			{
				gEnv->pConsole->ExecuteString(szFinishCmd);
			}
		}
		if (m_demo_quit)
		{
			QuitGame();
		}
		else if (!m_bDemoEnded)
		{
			EndDemo();
		}
	}

	if (m_bDelayedPlayFlag)
	{
		// to avoid playing demo before game is initialized (when running autotest)
		if (strlen(GetCurrentLevelPath()))
		{
			m_bDelayedPlayFlag = false;
			Play(true);
		}
	}

	ProcessKeysInput();

	if (!m_bPaused && m_bRecording && !gEnv->pConsole->GetStatus()) // Only when console closed.
	{
		// Reset random number generators seed.
		if (!RecordFrame())
		{
			Record(false);
		}
	}

	if (gEnv->pRenderer && (m_bPlaying || m_bRecording) && m_demo_noinfo <= 0)
	{
		RenderInfo(1);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTimeDemoRecorder::RecordFrame()
{
	CTimeValue time = GetTime();

	if (m_bPaused)
	{
		m_lastFrameTime = time;
		return true;
	}

	GetISystem()->GetRandomGenerator().Seed(0);

	FrameRecord rec;

	rec.frameTime = (time - m_lastFrameTime).GetSeconds();

	IEntity* pPlayerEntity = NULL;

	IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
	if (pClientActor)
	{
		pPlayerEntity = pClientActor->GetEntity();
	}

	if (pPlayerEntity)
	{
		rec.playerPosition = pPlayerEntity->GetPos();
		rec.playerRotation = pPlayerEntity->GetRotation();
		rec.playerViewRotation = pClientActor == NULL ? pPlayerEntity->GetRotation() : pClientActor->GetViewRotation();
	}

	// Legacy
	Ang3 cameraAngles = Ang3(GetISystem()->GetViewCamera().GetMatrix());
	rec.cameraAngles = RAD2DEG(cameraAngles);

	if (IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager())
	{
		if (IHmdDevice* pHmdDevice = pHmdManager->GetHmdDevice())
		{
			const HmdTrackingState& trackingState = pHmdDevice->GetLocalTrackingState();

			if (trackingState.CheckStatusFlags(eHmdStatus_IsUsable))
			{
				rec.hmdViewRotation = trackingState.pose.orientation;
				rec.hmdPositionOffset = trackingState.pose.position;
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
	// Record input events.
	//////////////////////////////////////////////////////////////////////////
	rec.inputEventsList = m_currentFrameInputEvents;
	rec.entityEvents = m_currentFrameEntityEvents;
	rec.gameEvents = m_currentFrameGameEvents;

	m_currentFrameInputEvents.clear();
	m_currentFrameEntityEvents.clear();
	m_currentFrameGameEvents.clear();

	//////////////////////////////////////////////////////////////////////////

	m_totalDemoTime += rec.frameTime;

	int nPolygons, nShadowVolPolys;
	gEnv->pRenderer->GetPolyCount(nPolygons, nShadowVolPolys);
	rec.nPolygons = nPolygons;

	m_nTotalPolysRecorded += nPolygons;

	AddFrameRecord(rec);

	FrameRecords::iterator it = m_records.begin();
	FrameRecords::iterator it1 = it;
	++it1;

	m_lastFrameTime = GetTime();

	if (m_demo_save_every_frame)
	{
		// Save after stopping.
		string filename = PathUtil::Make(GetCurrentLevelPath(), s_timedemo_file->GetString(), "tmd");
		s_pTimeDemoRecorder->Save(filename.c_str());
	}

	m_currentFrame++;
	if (m_currentFrame >= m_demo_max_frames)
	{
		Record(false);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CTimeDemoRecorder::PlayFrame()
{
	if (m_records.empty()) // can't playback empty records.
		return false;

	CTimeValue time = GetTime();
	CTimeValue deltaFrameTime = (time - m_lastFrameTime);
	float frameTime = deltaFrameTime.GetSeconds();

	if (m_bPaused)
	{
		m_lastFrameTime = time;
		return true;
	}

	FrameRecord& rec = m_records[m_currentFrame];
	m_nTotalPolysRecorded += rec.nPolygons;

	//////////////////////////////////////////////////////////////////////////
	// Play input events.
	//////////////////////////////////////////////////////////////////////////
	if (!rec.inputEventsList.empty())
	{
		//string str;
		for (InputEventsList::const_iterator it = rec.inputEventsList.begin(), end = rec.inputEventsList.end(); it != end; ++it)
		{
			const SInputEvent& inputEvent = *it;
			if (gEnv->pInput)
				gEnv->pInput->PostInputEvent(inputEvent);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Play back entity events.
	//////////////////////////////////////////////////////////////////////////
	{
		for (int i = 0; i < rec.entityEvents.size(); i++)
		{
			PlayBackEntityEvent(rec.entityEvents[i]);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	IEntity* pPlayer = NULL;
	IActor* pClActor = gEnv->pGameFramework->GetClientActor();
	if (pClActor)
	{
		pPlayer = pClActor->GetEntity();
	}

	if (pPlayer)
	{
		if (pPlayer->GetParent() == 0)
		{
			// Only if player is not linked to anything.
			pPlayer->SetPos(rec.playerPosition, ENTITY_XFORM_TIMEDEMO);

			int orientationIndex = m_numLoops % m_numOrientations;
			float zAngle = ((float)orientationIndex / m_numOrientations) * gf_PI2;
			const Quat& rotation = m_demo_use_hmd_rotation == 0 ? rec.playerViewRotation : rec.playerRotation;

			Quat adjustedPlayerRotation = Quat::CreateRotationZ(zAngle) * rotation;
			CRY_ASSERT(adjustedPlayerRotation.IsValid());

			pPlayer->SetRotation(adjustedPlayerRotation, ENTITY_XFORM_TIMEDEMO);
		}
	}

	m_totalDemoTime += deltaFrameTime;

	int nPolygons = ComputePolyCount();
	m_nTotalPolysPlayed += nPolygons;

	//////////////////////////////////////////////////////////////////////////
	// Calculate Frame Rates.
	//////////////////////////////////////////////////////////////////////////
	// Skip some frames before calculating frame rates.
	float timeElapsed = (time - m_lastFpsTimeRecorded).GetSeconds();
	if (timeElapsed > 1 && m_fpsCounter > 0)
	{
		// Skip some frames before recording frame rates.
		//if (m_currentFrame > 60)
		m_nPolysPerSec = (int)(float(m_nPolysCounter) / timeElapsed);
		m_nPolysCounter = 0;

		m_fpsCounter = 0;
		m_lastFpsTimeRecorded = time;
	}
	else
	{
		m_fpsCounter++;
	}

	if (m_currentFrame > m_minFPSCounter)
	{
		//m_currFPS = (float)m_fpsCounter / timeElapsed;

		m_currFPS = (float)(1.0 / deltaFrameTime.GetSeconds());
		m_sumFPS += m_currFPS;

		if (m_currFPS > m_maxFPS)
		{
			m_maxFPS_Frame = m_currentFrame;
			m_maxFPS = m_currFPS;
		}
		if (m_currFPS < m_minFPS)
		{
			m_minFPS_Frame = m_currentFrame;
			m_minFPS = m_currFPS;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Fill Time Demo Info structure.
	//////////////////////////////////////////////////////////////////////////
	if (m_pTimeDemoInfo)
	{
		m_pTimeDemoInfo->frames[m_currentFrame].fFrameRate = (float)(1.0 / deltaFrameTime.GetSeconds());
		m_pTimeDemoInfo->frames[m_currentFrame].nPolysRendered = nPolygons;
		m_pTimeDemoInfo->frames[m_currentFrame].nDrawCalls = gEnv->pRenderer ? gEnv->pRenderer->GetCurrentNumberOfDrawCalls() : 0;
	}
	//////////////////////////////////////////////////////////////////////////
	m_lastFrameTime = GetTime();

	//////////////////////////////////////////////////////////////////////////
	//override game time of day
	//////////////////////////////////////////////////////////////////////////
	if (0 <= m_demo_time_of_day)
	{
		//force update if time is significantly different from current time
		float fTime = gEnv->p3DEngine->GetTimeOfDay()->GetTime();
		const float cfSmallDeltaTime = 0.1f;

		if ((fTime > m_demo_time_of_day + cfSmallDeltaTime) ||
		    (fTime < m_demo_time_of_day - cfSmallDeltaTime))
		{
			gEnv->p3DEngine->GetTimeOfDay()->SetTime((float)m_demo_time_of_day, true);
			SetConsoleVar("e_TimeOfDaySpeed", 0);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	if ((m_numLoops == 0) &&
	    ((m_demo_screenshot_frame && m_currentFrame == m_demo_screenshot_frame) ||
	     (m_demo_screenshot_frame < 0) && (m_currentFrame % abs(m_demo_screenshot_frame) == 0)))
	{
		gEnv->pRenderer->ScreenShot();
	}
	//////////////////////////////////////////////////////////////////////////

	ReplayGameState(rec);

	m_currentFrame++;
	// Play looped.
	if (m_currentFrame >= GetNumberOfFrames() || m_currentFrame > m_demo_max_frames)
	{
		return false;
	}

	SignalPlayFrame();
	return true;
}

//////////////////////////////////////////////////////////////////////////
CTimeValue CTimeDemoRecorder::GetTime()
{
	// Must be asynchronius time, used for profiling.
	return gEnv->pTimer->GetAsyncTime();
}

//////////////////////////////////////////////////////////////////////////
int CTimeDemoRecorder::GetNumFrames() const
{
	return m_records.size();
}

//////////////////////////////////////////////////////////////////////////
float CTimeDemoRecorder::GetAverageFrameRate() const
{
	if (m_currentFrame)
	{
		float aveFrameTime = m_totalDemoTime.GetSeconds() / m_currentFrame;
		float aveFrameRate = 1.0f / aveFrameTime;
		return aveFrameRate;
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
float CTimeDemoRecorder::RenderInfo(float y)
{
	float retY = 0;

	if (m_demo_noinfo != 0)
		return retY;

	const char* sInfo = m_bPaused ? " (Paused)" : "";

	if (m_bRecording)
	{
		// TO DO
		float fColor[4] = { 0.7f, 0, 0, 1 };
		IRenderAuxText::Draw2dLabel(1, y, 1.3f, fColor, false, "Recording AutoTest%s", sInfo);
	}
	else if (m_bPlaying)
	{
		float fColor[4] = { 0, 0.7f, 0, 1 };
		IRenderAuxText::Draw2dLabel(1, y, 1.3f, fColor, false, "Playing AutoTest%s - Loop %d of %d, Orientation %d of %d", sInfo, (m_numLoops / m_numOrientations) + 1, m_maxLoops, (m_numLoops % m_numOrientations) + 1, m_numOrientations);
	}

	y += 15;

	if (m_bRecording)
	{
		float fColor[4] = { 1, 0, 0, 1 };
		IRenderAuxText::Draw2dLabel(1, y + retY, 1.3f, fColor, false, "TimeDemo%s, Frames: %d", sInfo, m_currentFrame);
		retY += 15;
	}
	else if (m_bPlaying)
	{
		int numFrames = GetNumFrames();
		float fColor[4] = { 0, 1, 0, 1 };
		IRenderAuxText::Draw2dLabel(1, y + retY, 1.3f, fColor, false, "TimeDemo%s, Frame %d of %d", sInfo, m_currentFrame, numFrames);
		retY += 15;

		float aveFrameRate = GetAverageFrameRate();
		//float aveFrameRate = m_currentFrame > m_minFPSCounter ? m_sumFPS/(m_currentFrame - m_minFPSCounter) : 0;
		//float aveFrameRate = m_sumFPS/(m_currentFrame +1);
		float polyRatio = m_nTotalPolysPlayed ? (float)m_nTotalPolysRecorded / m_nTotalPolysPlayed : 0.0f;
		//int numFrames = GetNumFrames();

		IRenderAuxText::Draw2dLabel(1, y + retY, 1.3f, fColor, false, "TestProfiler%s, Frame %d", sInfo, m_currentFrame);
		retY += 15;
		IRenderAuxText::Draw2dLabel(1, y + retY, 1.3f, fColor, false, " Last Played Length: %.2fs, FPS: %.2f", m_lastPlayedTotalTime, m_lastAveFrameRate);
		retY += 15;
		IRenderAuxText::Draw2dLabel(1, y + retY, 1.3f, fColor, false, " Average FPS: %.2f, FPS: %.2f, Polys/Frame: %d", aveFrameRate, m_currFPS, m_nCurrPolys);
		retY += 15;

		IRenderAuxText::Draw2dLabel(1, y + retY, 1.3f, fColor, false, " Polys Rec/Play Ratio: %.2f", polyRatio);
		retY += 15;
	}

	if (!m_bPaused && m_demo_gameState && m_fileVersion >= TIMEDEMO_FILE_VERSION_4)
	{
		IGameStateRecorder* pGameStateRecorder = gEnv->pGameFramework->GetIGameplayRecorder()->GetIGameStateRecorder();
		if (pGameStateRecorder)
			retY += pGameStateRecorder->RenderInfo(y + retY, m_bRecording);
	}

	return retY;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SetConsoleVar(const char* sVarName, float value)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(sVarName);
	if (pVar)
		pVar->Set(value);
}

//////////////////////////////////////////////////////////////////////////
float CTimeDemoRecorder::GetConsoleVar(const char* sVarName)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(sVarName);
	if (pVar)
		return pVar->GetFVal();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StartSession()
{
	m_bDemoEnded = false;
	m_bDemoFinished = false;

	Pause(false);

	bool bCurrentlyRecording = m_bRecording;
	m_bRecording = false;
	if (m_demo_restart_level == 1 && bCurrentlyRecording)
	{
		//QS does not work when playing/recording a timedemo, so pretend we're not...
		// Quick save at the begining of the recording, so we restore initial state as good as possible.
		CCryAction::GetCryAction()->SaveGame(GetInitSaveFileName(), true, true, eSGR_QuickSave, true);
	}

	ResetSessionLoop();
	m_bRecording = bCurrentlyRecording;

	m_nTotalPolysPlayed = 0;
	m_nTotalPolysRecorded = 0;

	//////////////////////////////////////////////////////////////////////////
	if (m_bRecording && m_bAIEnabled)
	{
		SaveAllEntitiesState();
	}
	//////////////////////////////////////////////////////////////////////////

	if (gEnv->pMovieSystem)
		gEnv->pMovieSystem->StopAllCutScenes();

	if (m_demo_ai == 0)
	{
		SetConsoleVar("ai_SystemUpdate", 0);
		SetConsoleVar("ai_IgnorePlayer", 1);
		SetConsoleVar("ai_SoundPerception", 0);
	}
	else
	{
		SetConsoleVar("ai_UseCalculationStopperCounter", 1);  // To make AI async time independent.
	}

	// No wait for key-press on level load.
	SetConsoleVar("hud_startPaused", 0);
	// Not cut-scenes
	SetConsoleVar("mov_NoCutscenes", 1);

	// cache console vars - changing them runtime has no effect
	m_bAIEnabled = m_demo_ai > 0;

	m_prevGodMode = (int)GetConsoleVar("g_godMode");

	if (m_demo_gameState && m_fileVersion >= TIMEDEMO_FILE_VERSION_4)
		gEnv->pGameFramework->GetIGameplayRecorder()->EnableGameStateRecorder(true, this, m_bRecording);

	if (m_bPlaying)
	{
		IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
		if (pClientActor)
		{
			pClientActor->EnableTimeDemo(true);
		}
	}

	if (!m_pTimeDemoInfo)
	{
		m_pTimeDemoInfo = new STimeDemoInfo();
	}

	int size = GetNumberOfFrames();
	if (m_pTimeDemoInfo && m_pTimeDemoInfo->frames.size() != size)
	{
		m_pTimeDemoInfo->frames.resize(size);
	}

	//////////////////////////////////////////////////////////////////////////
	// Force player out of the vehicle on start.
	//////////////////////////////////////////////////////////////////////////
	// Luc TO DO: remove this
	gEnv->pConsole->ExecuteString("v_exit_player");

	m_lastAveFrameRate = 0;
	m_lastPlayedTotalTime = 0;
	m_totalDemoTime.SetMilliSeconds(0);

	// Register to frame profiler.

	// remember old profiling settings
	m_bEnabledProfiling = gEnv->pFrameProfileSystem->IsEnabled();
	m_bVisibleProfiling = gEnv->pFrameProfileSystem->IsVisible();

	if (m_demo_profile)
	{
		gEnv->pFrameProfileSystem->Enable(true, gEnv->pFrameProfileSystem->IsVisible());
	}
	gEnv->pFrameProfileSystem->AddPeaksListener(this);

	// Profile
	m_oldPeakTolerance = GetConsoleVar("profile_peak");
	SetConsoleVar("profile_peak", 50);

	m_fixedTimeStep = GetConsoleVar("t_FixedStep");
	if (m_demo_fixed_timestep > 0)
		SetConsoleVar("t_FixedStep", 1.0f / (float)m_demo_fixed_timestep);

	if (m_demo_vtune)
		GetISystem()->GetIProfilingSystem()->VTuneResume();

	m_lastFrameTime = GetTime();
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StopSession()
{
	if (m_demo_vtune)
	{
		GetISystem()->GetIProfilingSystem()->VTunePause();
	}

	// Set old time step.
	SetConsoleVar("t_FixedStep", m_fixedTimeStep);

	if (m_demo_ai == 0)
	{
		SetConsoleVar("ai_SystemUpdate", 1);
		SetConsoleVar("ai_IgnorePlayer", 0);
		SetConsoleVar("ai_SoundPerception", 1);
	}
	else
	{
		SetConsoleVar("ai_UseCalculationStopperCounter", 0);  // To make AI async time independent.
	}
	SetConsoleVar("mov_NoCutscenes", 0);

	IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
	if (pClientActor)
		pClientActor->EnableTimeDemo(false);

	gEnv->pGameFramework->GetIGameplayRecorder()->EnableGameStateRecorder(false, this, false);

	// Profile.
	SetConsoleVar("profile_peak", m_oldPeakTolerance);
	gEnv->pFrameProfileSystem->RemovePeaksListener(this);

	if (m_demo_profile)
	{
		gEnv->pFrameProfileSystem->Enable(m_bEnabledProfiling, m_bVisibleProfiling);
	}
	m_lastPlayedTotalTime = m_totalDemoTime.GetSeconds();
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::ResetSessionLoop()
{
	if (m_demo_restart_level != 0 && m_bPlaying)
	{
		switch (m_demo_restart_level)
		{
		case 1:
			// Quick load at the begining of the playback, so we restore initial state as good as possible.
			if (gEnv->pGameFramework)
				gEnv->pGameFramework->LoadGame(GetInitSaveFileName());//, true, true);

			break;

		case 2:
		default:
			//load save made at start of level
			CCryAction::GetCryAction()->LoadGame(CCryAction::GetCryAction()->GetStartLevelSaveGameName());//, true);
			break;
		}

	}
	// Reset random number generators seed.
	GetISystem()->GetRandomGenerator().Seed(0);

	m_totalDemoTime.SetMilliSeconds(0);
	m_currentFrame = 0;

	if (gEnv->pStatoscope)
	{
		CryStackStringT<char, 64> loopStr;
		loopStr.Format("Loop %d Orientation %d", m_numLoops / m_numOrientations, m_numLoops % m_numOrientations);
		gEnv->pStatoscope->AddUserMarker("TimeDemo", loopStr.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::EraseLogFile()
{
	string filename = PathUtil::Make("%USER%/TestResults", PathUtil::ReplaceExtension(CTimeDemoRecorder::s_timedemo_file->GetString(), "log"));
	gEnv->pCryPak->RemoveFile(filename);
	gEnv->pCryPak->RemoveFile("./AutoTestFinished.log");
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::LogInfo(const char* format, ...)
{
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	va_list ArgList;
	char szBuffer[1024];

	va_start(ArgList, format);
	cry_vsprintf(szBuffer, format, ArgList);
	va_end(ArgList);

	gEnv->pLog->Log("%s", szBuffer);

	string filename = PathUtil::Make("%USER%/TestResults", PathUtil::ReplaceExtension(CTimeDemoRecorder::s_timedemo_file->GetString(), "log"));
	FILE* hFile = fxopen(filename.c_str(), "at");
	if (hFile)
	{
		// Write the string to the file and close it
		fprintf(hFile, "%s\n", szBuffer);
		fclose(hFile);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTimeDemoRecorder::OnInputEvent(const SInputEvent& event)
{
	if ((event.deviceType == eIDT_Keyboard) && event.keyId == eKI_Tilde)
	{
		return false;
	}
	if (event.deviceType != eIDT_Unknown)
	{
		m_currentFrameInputEvents.push_back(event);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::LogEndOfLoop()
{
	m_lastPlayedTotalTime = m_totalDemoTime.GetSeconds();
	m_lastAveFrameRate = GetAverageFrameRate();

	if (m_pTimeDemoInfo)
	{
		STimeDemoInfo* pTD = m_pTimeDemoInfo;
		pTD->lastPlayedTotalTime = m_lastPlayedTotalTime;
		pTD->lastAveFrameRate = m_lastAveFrameRate;
		pTD->minFPS = m_minFPS;
		pTD->maxFPS = m_maxFPS;
		pTD->minFPS_Frame = m_minFPS_Frame;
		pTD->maxFPS_Frame = m_maxFPS_Frame;
		pTD->nTotalPolysRecorded = m_nTotalPolysRecorded;
		pTD->nTotalPolysPlayed = m_nTotalPolysPlayed;
		GetISystem()->GetITestSystem()->SetTimeDemoInfo(m_pTimeDemoInfo);
	}

	int numFrames = GetNumberOfFrames();//m_records.size();
	LogInfo(" Run Finished.");
	LogInfo("    Play Time: %.2fs, Average FPS: %.2f", m_lastPlayedTotalTime, m_lastAveFrameRate);
	LogInfo("    Min FPS: %.2f at frame %d, Max FPS: %.2f at frame %d", m_minFPS, m_minFPS_Frame, m_maxFPS, m_maxFPS_Frame);
	if (m_lastPlayedTotalTime * (float)numFrames > 0.f)
		LogInfo("    Average Tri/Sec: %d, Tri/Frame: %d", (int)(m_nTotalPolysPlayed / m_lastPlayedTotalTime), m_nTotalPolysPlayed / numFrames);
	if (m_nTotalPolysPlayed)
		LogInfo("    Recorded/Played Tris ratio: %.2f", (float)m_nTotalPolysRecorded / m_nTotalPolysPlayed);

	IMemoryManager::SProcessMemInfo meminfo;
	if (GetISystem()->GetIMemoryManager()->GetProcessMemInfo(meminfo))
	{
		int MB = 1024 * 1024;
		LogInfo("    Memory Usage: WorkingSet=%" PRIu64 "Mb, PageFile=%" PRIu64 "Mb, PageFaults=%" PRIu64, meminfo.WorkingSetSize / MB, meminfo.PagefileUsage / MB, meminfo.PageFaultCount);
	}
}

void CTimeDemoRecorder::GetMemoryStatistics(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "TimeDemoRecorder");
	s->Add(*this);
	s->AddObject(m_records);
	s->AddObject(m_currentFrameInputEvents);
	s->AddObject(m_currentFrameEntityEvents);
	s->AddObject(m_currentFrameGameEvents);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
{
	pEntity->AddEventListener(ENTITY_EVENT_XFORM, this);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	CRY_ASSERT(m_bRecording);

	// Record entity event for this frame.
	EntityGUID guid = pEntity->GetGuid();
	if (guid.IsNull())
		return;

	// Record entity event for this frame.
	switch (event.event)
	{
	// Events to save.
	case ENTITY_EVENT_XFORM:
	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_UNHIDE:
	case ENTITY_EVENT_ATTACH:
	case ENTITY_EVENT_DETACH:
	case ENTITY_EVENT_DETACH_THIS:
	case ENTITY_EVENT_ENABLE_PHYSICS:
	case ENTITY_EVENT_ENTER_SCRIPT_STATE:
		{
			EntityEventRecord rec;
			memset(&rec, 0, sizeof(rec));
			rec.entityId = pEntity->GetId();
			rec.guid = guid;
			rec.eventType = event.event;
			rec.nParam[0] = event.nParam[0];
			rec.nParam[1] = event.nParam[1];
			rec.nParam[2] = event.nParam[2];
			rec.nParam[3] = event.nParam[3];
			rec.pos = pEntity->GetPos();
			rec.q = pEntity->GetRotation();
			m_currentFrameEntityEvents.push_back(rec);
		}
		break;

	// Skip all other events.
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::PlayBackEntityEvent(const EntityEventRecord& rec)
{
	EntityId entityId = gEnv->pEntitySystem->FindEntityByGuid(rec.guid);
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if (!pEntity)
		return;

	switch (rec.eventType)
	{
	// Events to save.
	case ENTITY_EVENT_XFORM:
		pEntity->SetPosRotScale(rec.pos, rec.q, pEntity->GetScale(), ENTITY_XFORM_TIMEDEMO);
		break;
	case ENTITY_EVENT_HIDE:
		pEntity->Hide(true);
		break;
	case ENTITY_EVENT_UNHIDE:
		pEntity->Hide(false);
		break;
	case ENTITY_EVENT_ATTACH:
		{
			IEntity* pChild = (IEntity*)gEnv->pEntitySystem->GetEntity((EntityId)rec.nParam[0]); // Get Child entity.
			if (pChild)
				pEntity->AttachChild(pChild);
		}
		break;
	case ENTITY_EVENT_DETACH:
		break;
	case ENTITY_EVENT_DETACH_THIS:
		pEntity->DetachThis(IEntity::EAttachmentFlags(0), ENTITY_XFORM_TIMEDEMO);
		break;
	case ENTITY_EVENT_ENABLE_PHYSICS:
		if (rec.nParam[0] == 0)
			pEntity->EnablePhysics(false);
		else
			pEntity->EnablePhysics(true);
		break;
	case ENTITY_EVENT_ENTER_SCRIPT_STATE:
		{
			IEntityScriptComponent* pScriptProxy = (IEntityScriptComponent*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
			if (pScriptProxy)
			{
				pScriptProxy->GotoStateId((int)rec.nParam[0]);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SaveAllEntitiesState()
{
	m_firstFrameEntityState.clear();
	// Record all objects positions.
	IEntity* pEntity;
	IEntityItPtr pEntityIter = gEnv->pEntitySystem->GetEntityIterator();
	while (pEntity = pEntityIter->Next())
	{
		EntityGUID guid = pEntity->GetGuid();
		if (!guid.IsNull())
		{
			EntityEventRecord rec;
			memset(&rec, 0, sizeof(rec));
			rec.entityId = pEntity->GetId();
			rec.guid = guid;
			rec.eventType = ENTITY_EVENT_RESET;
			rec.pos = pEntity->GetPos();
			rec.q = pEntity->GetRotation();
			rec.flags |= (pEntity->IsHidden()) ? EntityEventRecord::HIDDEN : 0;
			m_firstFrameEntityState.push_back(rec);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::RestoreAllEntitiesState()
{
	for (int i = 0; i < m_firstFrameEntityState.size(); i++)
	{
		EntityEventRecord& rec = m_firstFrameEntityState[i];
		if (rec.eventType == ENTITY_EVENT_RESET)
		{
			EntityId entityId = gEnv->pEntitySystem->FindEntityByGuid(rec.guid);
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
			if (!pEntity)
				continue;

			pEntity->Hide((rec.flags & EntityEventRecord::HIDDEN) != 0);
			pEntity->SetPosRotScale(rec.pos, rec.q, pEntity->GetScale(), ENTITY_XFORM_TIMEDEMO);
		}
	}

}

///////////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::ParseParams(XmlNodeRef baseNode)
{

}

///////////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SetVariable(const char* name, const char* szValue)
{

}

///////////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SetVariable(const char* name, float value)
{

}

///////////////////////////////////////////////////////////////////////////////
int CTimeDemoRecorder::GetNumberOfFrames()
{
	return m_records.size();
}

///////////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::OnGameplayEvent(IEntity* pEntity, const GameplayEvent& event)
{
	if (IsRecording())
	{
		STimeDemoGameEvent ge(pEntity, event);
		m_currentFrameGameEvents.push_back(ge);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::OnFrameProfilerPeak(CFrameProfiler* pProfiler, float fPeakTime)
{
	if (m_bPlaying && !m_bPaused)
	{
		LogInfo("    -Peak at Frame %d, %.2fms : %s (count: %d)", m_currentFrame, fPeakTime, pProfiler->m_name, pProfiler->m_count);
	}
}

//////////////////////////////////////////////////////////////////////////
int CTimeDemoRecorder::ComputePolyCount()
{
	if (!gEnv->pRenderer)
	{
		return 0;
	}
	int nPolygons, nShadowVolPolys;
	gEnv->pRenderer->GetPolyCount(nPolygons, nShadowVolPolys);
	m_nPolysCounter += nPolygons;
	m_nCurrPolys = nPolygons;
	if (nPolygons > m_nMaxPolys)
		m_nMaxPolys = nPolygons;
	if (nPolygons < m_nMinPolys)
		m_nMinPolys = nPolygons;
	return nPolygons;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StartChainDemo(const char* levelsListFilename, bool bAutoLoadChainConfig)
{
	if (bAutoLoadChainConfig)
	{
		gEnv->pConsole->ExecuteString("exec AutoTestChain.cfg");
	}
	m_bChainloadingDemo = true;
	EraseLogFile();
	m_demoLevels.clear();
	if (levelsListFilename && *levelsListFilename)
	{
		// Open file with list of levels for autotest.
		// This is used only for development so doesn`t need to use CryPak!
		FILE* f = fxopen(levelsListFilename, "rt");
		if (f)
		{
			while (!feof(f))
			{
				char str[512];
				if (fgets(str, sizeof(str), f))
				{
					string level = str;
					level.Trim();
					if (level.size())
					{
						SChainDemoLevel lvl;
						lvl.level = level;
						lvl.time = 0;
						lvl.bSuccess = false;
						lvl.bRun = false;
						m_demoLevels.push_back(lvl);
					}
				}
			}
			fclose(f);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to find chainload file: %s", levelsListFilename);
		}
	}

	if (!m_demoLevels.empty())
	{
		if (IsRecording())
			Record(false);
		else if (IsPlaying())
			Play(false);

		m_nCurrentDemoLevel = 0;
		StartNextChainedLevel();
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "No levels found for chainload in: %s", levelsListFilename);
	}
}

void CTimeDemoRecorder::StartDemoLevel(const char** levelNames, int levelCount)
{
	m_bChainloadingDemo = true;
	EraseLogFile();
	m_demoLevels.clear();

	if (levelNames && levelCount > 0)
	{
		for (int i = 0; i < levelCount; ++i)
		{
			SChainDemoLevel lvl;
			lvl.level = levelNames[i];
			lvl.time = 0;
			lvl.bSuccess = false;
			lvl.bRun = false;
			m_demoLevels.push_back(lvl);
		}
	}

	if (!m_demoLevels.empty())
	{
		if (IsRecording())
			Record(false);
		else if (IsPlaying())
			Play(false);

		m_nCurrentDemoLevel = 0;
		StartNextChainedLevel();
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "demo_StartDemoLevel: No level(s) specified.");
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StartNextChainedLevel()
{
	if (!m_demoLevels.empty())
	{
		float tcurr = GetISystem()->GetITimer()->GetAsyncTime().GetSeconds();
		if (m_nCurrentDemoLevel - 1 >= 0 && m_nCurrentDemoLevel - 1 < m_demoLevels.size())
		{
			m_demoLevels[m_nCurrentDemoLevel - 1].bSuccess = true;
			m_demoLevels[m_nCurrentDemoLevel - 1].bRun = true;
			m_demoLevels[m_nCurrentDemoLevel - 1].time = tcurr - m_lastChainDemoTime;
		}
		m_lastChainDemoTime = tcurr;

		SaveChainloadingJUnitResults();

		// This support loading level/playing time demo, then loading next level, etc...
		if (m_nCurrentDemoLevel < m_demoLevels.size())
		{
			CryStackStringT<char, 256> mapCmd("map ");
			mapCmd += m_demoLevels[m_nCurrentDemoLevel].level;
			gEnv->pConsole->ExecuteString(mapCmd);
			StartDemoDelayed();
			m_nCurrentDemoLevel++;
			return;
		}
	}
	ICVar* pFinishCmd = gEnv->pConsole->GetCVar("demo_finish_cmd");
	if (pFinishCmd)
	{
		const char* const szFinishCmd = pFinishCmd->GetString();
		if (szFinishCmd && szFinishCmd[0] != '\0')
		{
			gEnv->pConsole->ExecuteString(szFinishCmd);
		}
	}
	if (m_demo_quit)
	{
		// If No more chained levels. quit.
		QuitGame();
	}
	else if (!m_bDemoEnded)
	{
		EndDemo();
	}
}

//////////////////////////////////////////////////////////////////////////
string CTimeDemoRecorder::GetCurrentLevelName()
{
	return gEnv->pGameFramework->GetLevelName();
}

//////////////////////////////////////////////////////////////////////////
string CTimeDemoRecorder::GetInitSaveFileName()
{
	string level = GetCurrentLevelName();
	string str = "autotest_init_";
	str += level;
	str += CRY_SAVEGAME_FILE_EXT;
	return str;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SaveChainloadingJUnitResults()
{
	XmlNodeRef testsuit = GetISystem()->CreateXmlNode("testsuite");
	testsuit->setAttr("name", "ChainLoading");
	for (int i = 0; i < (int)m_demoLevels.size(); i++)
	{
		int nSeconds = static_cast<int>(m_demoLevels[i].time);
		XmlNodeRef testcase = testsuit->newChild("testcase");
		testcase->setAttr("name", m_demoLevels[i].level.c_str());
		testcase->setAttr("time", nSeconds);
		if (!m_demoLevels[i].bSuccess)
		{
			XmlNodeRef failure = testcase->newChild("failure");
			if (!m_demoLevels[i].bRun)
				failure->setAttr("type", "Not Run");
			else
				failure->setAttr("type", "Failed");
		}
	}

	gEnv->pCryPak->MakeDir("%USER%/TestResults");
	testsuit->saveToFile("%USER%/TestResults/ChainLoadingJUnit.xml");
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::EndDemo()
{
	m_bDemoFinished = true;
	m_bDemoEnded = true;

	if (!gEnv->IsEditor())
	{
		CCryAction::GetCryAction()->EndGameContext();
	}

#if CAPTURE_REPLAY_LOG
	if (m_finish_replaysizer)
		CryGetIMemReplay()->AddSizerTree("TimeDemoSizers");
	if (m_finish_replaystop)
		CryGetIMemReplay()->Stop();
#endif

	CryLogAlways("Testing Successfully Finished, Quiting...");
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::QuitGame()
{
	CryLogAlways("Testing Successfully Finished, Quiting...");

	m_bDemoFinished = true;
	GetISystem()->Quit();
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::ProcessKeysInput()
{
#if CRY_PLATFORM_WINDOWS
	if (!gEnv->IsDedicated() && gEnv->pSystem->IsDevMode())
	{
		// Check if special development keys where pressed.
		bool bAlt = ((CryGetAsyncKeyState(VK_LMENU) & (1 << 15)) != 0) || (CryGetAsyncKeyState(VK_RMENU) & (1 << 15)) != 0;
		bool bCtrl = (CryGetAsyncKeyState(VK_CONTROL) & (1 << 15)) != 0;
		bool bShift = (CryGetAsyncKeyState(VK_SHIFT) & (1 << 15)) != 0;

		bool bCancel = CryGetAsyncKeyState(VK_CANCEL) & 1;
		bool bTimeDemoKey = CryGetAsyncKeyState(VK_SNAPSHOT) & 1;

		if (bCancel)
		{
			if (IsRecording())
			{
				// stop all test modules
				Record(false);
				return;
			}
			// Toggle start/stop of demo recording.
			if (IsPlaying())
			{
				// Stop playing.
				Play(false);
				return;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Time demo on/off
		//////////////////////////////////////////////////////////////////////////
		if ((bCtrl) && bTimeDemoKey)
		{
			if (!IsRecording())
			{
				// Start record.
				Record(true);
			}
		}
		if (bShift && bTimeDemoKey)
		{
			if (!IsPlaying())
			{
				// Load and start playing.
				Play(true);
			}
		}
	}
#endif

	bool bPaused = false;
	if (m_bRecording || m_bPlaying)
	{
		bPaused = gEnv->pConsole->IsOpened();

		if (!bPaused && m_demo_scroll_pause != 0 && gEnv->pInput)
		{
			static TKeyName scrollKey("scrolllock");
			bPaused = gEnv->pInput->InputState(scrollKey, eIS_Down);
		}

		if (bPaused != m_bPaused)
			Pause(bPaused);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::ReplayGameState(FrameRecord& rec)
{
	//////////////////////////////////////////////////////////////////////////
	// Game state
	IGameStateRecorder* pGameStateRecorder = gEnv->pGameFramework->GetIGameplayRecorder()->GetIGameStateRecorder();
	if (pGameStateRecorder && m_fileVersion >= TIMEDEMO_FILE_VERSION_4 && m_demo_gameState)
	{
		int n = rec.gameEvents.size();
		for (int i = 0; i < n; i++)
		{
			STimeDemoGameEvent& gameEvent = rec.gameEvents[i];
			//IEntity * pEntity = gEnv->pEntitySystem->GetEntity(gameEvent.id);
			IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(gameEvent.entityName);
			if (pEntity)
			{
				GameplayEvent event;
				event.event = gameEvent.gameEventType;
				event.value = gameEvent.value;
				event.extra = (void*)(EXPAND_PTR)(gameEvent.extra);

				if (gameEvent.description.size())
					event.description = gameEvent.description.c_str();

				if (gameEvent.description2.size())
					event.extra = (void*)gameEvent.description2.c_str();

				pGameStateRecorder->OnRecordedGameplayEvent(pEntity, event, m_currentFrame);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::StartDemoDelayed()
{
	CRY_ASSERT(!m_bDelayedPlayFlag);
	EraseLogFile();
	m_bDelayedPlayFlag = true;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::RegisterListener(ITimeDemoListener* pListener)
{
	m_listeners.Add(pListener);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::UnregisterListener(ITimeDemoListener* pListener)
{
	m_listeners.Remove(pListener);
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::GetCurrentFrameRecord(STimeDemoFrameRecord& externalRecord) const
{
	const FrameRecord& record = m_records[m_currentFrame];

	externalRecord.playerPosition = record.playerPosition;
	externalRecord.playerRotation = record.playerRotation;
	externalRecord.playerViewRotation = record.playerViewRotation;
	externalRecord.hmdPositionOffset = record.hmdPositionOffset;
	externalRecord.hmdViewRotation = record.hmdViewRotation;
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SignalPlayback(bool bEnable)
{
	for (TTimeDemoListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnPlayback(bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SignalRecording(bool bEnable)
{
	for (TTimeDemoListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnRecord(bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeDemoRecorder::SignalPlayFrame()
{
	for (TTimeDemoListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnPlayFrame();
	}
}
