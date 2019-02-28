// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AutoEnum.h"
#include "GameCodeCoverage/IGameCodeCoverageListener.h"
#include "Utility/SingleAllocTextBlock.h"
#include "GameCodeCoverage/GameCodeCoverageEnabled.h"
#include "GameMechanismManager/GameMechanismBase.h"
#include "Actor.h"

#if defined(_RELEASE) && !defined(PERFORMANCE_BUILD)
// Final release - don't enable feature tester!
	#define ENABLE_FEATURE_TESTER 0
#else
// Feel free to turn this on/off or add more conditions (platform, _DEBUG etc.) here...
	#define ENABLE_FEATURE_TESTER 1
#endif

#if ENABLE_FEATURE_TESTER

// Only include if FT enabled
	#include "FeatureTestMgr.h"

struct SFeatureTestInstructionOrParam;
struct SFeatureTestDataLoadWorkspace;
struct IFeatureTestTextHandler;
class CAutoTester;
class CActor;

#define FeatureTestPauseReasonList(f)           \
	f(kFTPauseReason_none)                      \
	f(kFTPauseReason_untilTimeHasPassed)        \
	f(kFTPauseReason_untilCCCPointsHit)         \
	f(kFTPauseReason_untilWeaponIsReadyToUse)   \
	f(kFTPauseReason_untilPlayerIsAlive)        \
	f(kFTPauseReason_untilPlayerAimingAtEnemy)  \
	f(kFTPauseReason_untilFGTestsComplete)		\

#define FeatureTestCommandList(f)            \
	f(kFTC_End)                              \
	f(kFTC_Fail)                             \
	f(kFTC_SetItem)                          \
	f(kFTC_SetAmmo)                          \
	f(kFTC_Wait)                             \
	f(kFTC_WaitSingleFrame)                  \
	f(kFTC_WaitUntilHitAllExpectedCCCPoints) \
	f(kFTC_OverrideButtonInput_Press)        \
	f(kFTC_OverrideButtonInput_Release)      \
	f(kFTC_OverrideAnalogInput)              \
	f(kFTC_WatchCCCPoint)                    \
	f(kFTC_ResetCCCPointHitCounters)         \
	f(kFTC_CheckNumCCCPointHits)             \
	f(kFTC_SetResponseToHittingCCCPoint)     \
	f(kFTC_DoConsoleCommand)                 \
	f(kFTC_RunFeatureTest)                   \
	f(kFTC_TrySpawnPlayer)                   \
	f(kFTC_WaitUntilPlayerIsAlive)           \
	f(kFTC_WaitUntilPlayerAimingAt)          \
	f(kFTC_MovePlayerToOtherEntity)          \
	f(kFTC_JumpIfConditionsDoNotMatch)       \
	f(kFTC_Jump)                             \
	f(kFTC_SetLocalPlayerLookAt)             \
	f(kFTC_RunFlowGraphFeatureTests)         \

#define FeatureTestRequirementList(f)   \
	f(kFTReq_noLevelLoaded)             \
	f(kFTReq_inLevel)                   \
	f(kFTReq_localPlayerExists)         \
	f(kFTReq_remotePlayerExists)        \

#define FeatureTestCheckpointHitResponseList(f)   \
	f(kFTCHR_nothing)                             \
	f(kFTCHR_failTest)                            \
	f(kFTCHR_completeTest)                        \
	f(kFTCHR_completeSubroutine)                  \
	f(kFTCHR_restartTest)                         \
	f(kFTCHR_restartSubroutine)                   \
	f(kFTCHR_expectedNext)                        \

#define FeatureTestPlayerSelectionList(f)   \
	f(kFTPS_localPlayer)                    \
	f(kFTPS_firstRemotePlayer)              \
	f(kFTPS_secondRemotePlayer)             \

#define FeatureTestAimConditionList(f)   \
	f(kFTAC_nobody)                      \
	f(kFTAC_anybody)                     \
	f(kFTAC_friend)                      \
	f(kFTAC_enemy)                       \

AUTOENUM_BUILDENUMWITHTYPE(EFTPauseReason, FeatureTestPauseReasonList);
AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EFeatureTestCommand, FeatureTestCommandList, kFTC_Invalid, kFTC_Num);
AUTOENUM_BUILDFLAGS_WITHZERO(FeatureTestRequirementList, kFTReq_none);
AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(EFTCheckpointHitResponse, FeatureTestCheckpointHitResponseList, kFTCHR_num);
AUTOENUM_BUILDFLAGS_WITHZERO(FeatureTestPlayerSelectionList, kFTPS_nobody);
AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(EFTAimCondition, FeatureTestAimConditionList, kFTAC_num);

	#define DO_COMMAND_PROTOTYPE(name) void Instruction_ ## name();

class CFeatureTester : public CGameMechanismBase
	#if ENABLE_GAME_CODE_COVERAGE
	, public IGameCodeCoverageListener
	#endif
{
	friend class CFeatureTestArgumentAutoComplete;

private:
	static const int kMaxWatchedCheckpoints = 16;
	static const int kMaxSimultaneouslyOverriddenInputs = 8;

	typedef void (CFeatureTester::* InstructionFunc)();

	struct SCheckpointCount
	{
		const char*              m_checkpointName;
		const char*              m_customMessage;
		int                      m_timesHit;
		int                      m_stackLevelAtWhichAdded;
		float                    m_restartDelay;
		EFTCheckpointHitResponse m_hitResponse;

		// For spotting cross-project code checkpoints getting hit...
		size_t m_checkpointMgrHandle;
		uint32 m_checkpointMgrNumHitsSoFar;
	};

	union UPausedInfo
	{
		struct
		{
			TBitfield m_waitUntilPlayerIsAlive_whichPlayers;
		};
		struct
		{
			TBitfield       m_waitUntilTargettingAnEnemy_whichPlayers;
			EFTAimCondition m_waitUntilTargettingAnEnemy_requireTargetOfType;
		};
	};

	struct SFeatureTest
	{
		const char* m_setName;
		const char* m_testName;
		const char* m_owners;
		const char* m_prerequisite;
		const char* m_testDescription;
		const char* m_iterateOverParams;
		float       m_maxTime;
		float       m_scaleSpeed;
		TBitfield   m_requirementBitfield;
		uint16      m_offsetIntoInstructionBuffer;
		bool        m_enabled;
		bool        m_autoRunThis;
		bool        m_everPassed;
	};

	struct SStackedTestCallInfo
	{
		const SFeatureTest*                   m_calledTest;
		const SFeatureTestInstructionOrParam* m_returnToHereWhenDone;
	};

	struct SStack
	{
		static const int     k_stackSize = 3;
		int                  m_count;
		SStackedTestCallInfo m_info[k_stackSize];
	};

	struct SCurrentlyOverriddenInput
	{
		const char*           m_inputName;
		EActionActivationMode m_mode;
		bool                  m_sendHoldEventEveryFrame;
	};

	struct SListOfEntityClasses
	{
		static const int k_max = 4;
		int              m_num;
		IEntityClass*    m_classPtr[k_max];
	};

	struct SListOfCheckpoints
	{
		static const int k_max = 4;
		int              m_num;
		const char*      m_checkpointName[k_max];
	};

	struct SAutoAimPosition
	{
		EntityId m_entityId;
		EBonesID m_boneId;
	};

public:
	CFeatureTester();
	~CFeatureTester();
	static string                GetContextString();

	ILINE static CFeatureTester* GetInstance()
	{
		return s_instance;
	}

	CFeatureTestMgr& GetMapFeatureTestMgr() { return *m_pFeatureTestMgr; }

	ILINE bool       GetIsActive()
	{
		return m_currentTest || m_numFeatureTestsLeftToAutoRun || m_pFeatureTestMgr->IsRunning();
	}

	ILINE void InformAutoTesterOfResults(CAutoTester* autoTester)
	{
		m_informAutoTesterOfResults = autoTester;

		if (m_pFeatureTestMgr)
			m_pFeatureTestMgr->SetAutoTester(m_informAutoTesterOfResults);
	}

	ILINE CAutoTester* GetAutoTesterIfActive()
	{
		return m_informAutoTesterOfResults;
	}

private:
	void                                  Update(float dt);
	void                                  PreProcessCommandNode(const IItemParamsNode* node);
	int                                   PreprocessTestSet(const IItemParamsNode* testsListNode);
	bool                                  ConvertSwitchNodeToInstructions(const IItemParamsNode* cmdParams, SFeatureTest& createTest, SFeatureTestDataLoadWorkspace* loadWorkspace);
	bool                                  LoadCommands(const IItemParamsNode* node, SFeatureTest& createTest, SFeatureTestDataLoadWorkspace* loadWorkspace);
	bool                                  ReadTestSet(const IItemParamsNode* testsListNode, SFeatureTestDataLoadWorkspace* loadWorkspace);
	TBitfield                             ReadPlayerSelection(const IItemParamsNode* paramsNode);
	void                                  ReadSettings(const IItemParamsNode* topSettingsNode);
	void                                  LoadTestData(const char* filename);
	void                                  UnloadTestData();
	bool                                  AddInstructionAndParams(EFeatureTestCommand cmd, const IItemParamsNode* paramsNode, SFeatureTestDataLoadWorkspace* loadWorkspace);
	void                                  AttemptStartTestWithTimeout(const SFeatureTest* test, const char* actionName, int offsetIntoIterationList, float dt);
	const char*                           StartTest(const SFeatureTest* test, const char* actionName, float delay, int offsetIntoIterationList = 0);
	void                                  InterruptCurrentTestIfOneIsRunning();
	void                                  ListTestInstructions(const SFeatureTestInstructionOrParam* instruction, IFeatureTestTextHandler& textHandler) const;
	bool                                  CompleteSubroutine();
	void                                  DoneWithTest(const SFeatureTest* doneWithThisTest, const char* actionName);
	void                                  SendInputToLocalPlayer(const char* inputName, EActionActivationMode mode, float value);
	const SFeatureTestInstructionOrParam* GetNextInstructionOrParam();
	SCheckpointCount*                     WatchCheckpoint(const char* cpName);
	SCheckpointCount*                     FindWatchedCheckpointDataByName(const char* name, int stackLevel);
	SFeatureTest*                         FindTestByName(const char* name);
	bool                                  FeatureTestFailureFunc(const char* conditionTxt, const char* messageTxt);
	CActor*                               GetPlayerForSelectionFlag(TBitfield flag);
	IActor*                               GetNthNonLocalActor(int skipThisManyBeforeReturning);
	void                                  SetCheckpointHitResponse(SCheckpointCount* checkpoint, EFTCheckpointHitResponse response);
	void                                  SetPauseStateAndTimeout(EFTPauseReason pauseReason, float timeOut);
	void                                  RemoveWatchedCheckpointsAddedAtCurrentStackLevel(const char* reason);
	void                                  SubmitResultToAutoTester(const SFeatureTest* test, float timeTaken, const char* failureMessage);
	void                                  SubmitTestStartedMessageToAutoTester(const SFeatureTest* test);
	string                                GetListOfCheckpointsExpected();
	const char*                           GetTextParam();
	bool                                  IsActorAliveAndPlaying(IActor* actor);
	const char*                           GetActorInfoString(IActor* iActor, CryFixedStringT<256>& reply);
	void                                  SendTextMessageToRemoteClients(const char* msg);
	void                                  DisplayCaption(const SFeatureTest* test);
	void                                  InformWatchedCheckpointHit(SCheckpointCount* watchedCheckpoint);
	void                                  InformCodeCoverageLabelHit(const char* cpLabel);
	bool                                  CheckPrerequisite(const SFeatureTest* test);
	void                                  PollForNewCheckpointHits();
	void                                  ClearLocalPlayerAutoAim();

	ILINE void                            PauseExecution(bool pause = true)
	{
		m_abortUntilNextFrame |= (pause == true);
	}

	// StopTest:
	// Pass in NULL to stop quietly, "" to count as a successful run or any other char* ptr for a test failure
	void StopTest(const char* failureMsg);

	// Static functions triggered by console commands...
	static void CmdStartTest(IConsoleCmdArgs* pArgs);
	static void CmdLoad(IConsoleCmdArgs* pArgs);
	static void CmdReload(IConsoleCmdArgs* pArgs);
	static void CmdRunAll(IConsoleCmdArgs* pArgs);

	#if ENABLE_GAME_CODE_COVERAGE
	// IGameCodeCoverageListener
	void InformCodeCoverageCheckpointHit(CGameCodeCoverageCheckPoint* cp);
	#endif

	// Instructions
	FeatureTestCommandList(DO_COMMAND_PROTOTYPE)

	// Static private data
	static CFeatureTester * s_instance;
	static InstructionFunc s_instructionFunctions[];

	// Non-static member vars
	CSingleAllocTextBlock                 m_singleAllocTextBlock;
	SCheckpointCount                      m_checkpointCountArray[kMaxWatchedCheckpoints];
	SCurrentlyOverriddenInput             m_currentlyOverriddenInputs[kMaxSimultaneouslyOverriddenInputs];
	SListOfEntityClasses                  m_listEntityClassesWhenFail;
	SListOfCheckpoints                    m_checkpointsWhichAlwaysFailATest;
	SAutoAimPosition                      m_localPlayerAutoAimSettings;
	UPausedInfo                           m_pausedInfo;
	SStack                                m_runFeatureTestStack;
	string                                m_currentlyLoadedFileName;

	const SFeatureTestInstructionOrParam* m_currentTestNextInstruction;
	SFeatureTest*                         m_featureTestArray;
	const SFeatureTest*                   m_currentTest;
	const SFeatureTest*                   m_haveJustWrittenMessageAboutUnfinishedTest;
	CAutoTester*                          m_informAutoTesterOfResults;
	SFeatureTestInstructionOrParam*       m_singleBufferContainingAllInstructions;
	EFTPauseReason                        m_pause_state;
	float m_pause_timeLeft;
	float m_pause_originalTimeOut;
	float m_timeSinceCurrentTestBegan;
	int   m_numWatchedCheckpoints;
	int   m_numOverriddenInputs;
	int   m_numTests;
	int   m_numFeatureTestsLeftToAutoRun;
	int   m_saveScreenshotWhenFail;
	bool  m_pause_enableCountdown;
	bool  m_abortUntilNextFrame;
	uint8 m_waitUntilCCCPointHit_numStillToHit;

	struct
	{
		int   m_numParams;
		int   m_nextIterationCharOffset;
		char* m_currentParams[9];
	} m_iterateOverParams;

	struct
	{
		const SFeatureTest* m_test;
		int                 m_charOffset;
	} m_nextIteration;

	CFeatureTestMgr* m_pFeatureTestMgr;
};

#endif