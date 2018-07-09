// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Testing/FeatureTester.h"
#include "Utility/CryWatch.h"
#include <CryString/StringUtils.h>
#include "Utility/DesignerWarning.h"
#include "Player.h"
#include "IPlayerInput.h"
#include "GameCodeCoverage/GameCodeCoverageManager.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRules.h"
#include "Testing/AutoTester.h"
#include "IWorldQuery.h"
#include "Weapon.h"

#if ENABLE_FEATURE_TESTER

#if defined(_RELEASE) && !defined(PERFORMANCE_BUILD)
#error "Attempted to compile feature tester into release code - this won't work as it relies on other non-release functionality"
#endif

//===================================================================================================
// Macros (including auto-enum defines)
//===================================================================================================

#define kMaximumFrameTime  0.5f

#define CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_XML    460

// After reading the file, the actual amount of memory required will be allocated and the buffer will be disposed of... it's just for during the loading procedure
#define BUFFER_SIZE_FOR_READING_INSTRUCTIONS                       3500

#define GET_PLAYER_SELECTION_TEXT(bits) AutoEnum_GetStringFromBitfield(bits, s_featureTestPlayerSelectionNames, FeatureTestPlayerSelectionList_numBits).c_str()

#define FeatureTestDataTypeDetails(f)                                                                                                          \
	f(kFTVT_Command,  EFeatureTestCommand, (val >= 0 && val < kFTC_Num),           "%s",              s_featureTestCommandNames[val]           ) \
	f(kFTVT_Float,    float,               true,                                   "%f",              val                                      ) \
	f(kFTVT_Int,      int,                 true,                                   "%i",              val                                      ) \
	f(kFTVT_Bool,     bool,                (val == true || val == false),          "%s",              val ? "TRUE" : "FALSE"                   ) \
	f(kFTVT_Text,     const char *,        (val != NULL),                          "\"%s\"",          val                                      ) \
	f(kFTVT_HitResponse, EFTCheckpointHitResponse, (val >= 0 && val < kFTCHR_num), "%s",              s_featureTestHitResponseNames[val]       ) \
	f(kFTVT_EntityFlags, uint32,                   true,                           "entity flags %u", val                                      ) \
	f(kFTVT_Who,      TBitfield,           (val < BIT(FeatureTestPlayerSelectionList_numBits)), "%s", GET_PLAYER_SELECTION_TEXT(val)           ) \
	f(kFTVT_AimAt,    EFTAimCondition,     (val >= 0 && val < kFTAC_num),          "%s",              s_featureTestAimConditionNames[val]      ) \

#define FeatureTesterLog(...)       CryLog("$3[FEATURETESTER]$1 %s%s", CFeatureTester::GetContextString().c_str(), string().Format(__VA_ARGS__).c_str())
#define FeatureTesterWarning(...)   CryLog("$3[FEATURETESTER] <WARN> %s%s", CFeatureTester::GetContextString().c_str(), string().Format(__VA_ARGS__).c_str())

#if defined(USER_timf) || defined(USER_martinsh)
#define FeatureTesterSpam(...)      FeatureTesterLog("SPAM! " __VA_ARGS__)
#else
#define FeatureTesterSpam(...)      (void)(0)
#endif

#define MAKE_UNION_CONTENTS(enumVal, theType, ...)        theType m_ ## enumVal;
#define MAKE_INSTRUCTION_FUNC_POINTER(instructionName)    &CFeatureTester::Instruction_ ## instructionName,

#define MAKE_CASE_STRING_OUTPUT(enumVal, theType, isValid, strFormat, ...)                 \
	case enumVal:                                                                            \
	{                                                                                        \
		theType val = m_data.m_ ## enumVal;                                                    \
		if (isValid)                                                                           \
		{                                                                                      \
			strOutput.Format(strFormat, __VA_ARGS__);                                            \
		}                                                                                      \
		else                                                                                   \
		{                                                                                      \
			strOutput.Format("<invalid %s>", s_featureTestValTypeNames[enumVal]);                \
		}                                                                                      \
	}                                                                                        \
	break;

#define MAKE_FUNCTIONS(enumVal, theType, isValid, strFormat, ...)                          \
	ILINE bool AddData_ ## enumVal (theType val)                                             \
	{                                                                                        \
	  m_type = enumVal;                                                                      \
		m_data.m_ ## enumVal = val;                                                            \
		return isValid;                                                                        \
	}                                                                                        \
	ILINE theType GetData_ ## enumVal () const                                               \
	{                                                                                        \
		theType val = m_data.m_ ## enumVal;                                                    \
		if (m_type != enumVal)		     																	                       \
		{																									                                     \
		  string contents;                                                                     \
			CRY_ASSERT_TRACE(0, ("Expected next item to be a '%s' but it's a '%s' value=%s",     \
				s_featureTestValTypeNames[enumVal], s_featureTestValTypeNames[m_type],             \
				GetContentsAsString(contents)));                                                   \
		}                                                                                      \
		else if (! (isValid))                                                                  \
		{																									                                     \
			CRY_ASSERT_TRACE(0, ("Next item is right type (%s) but value is invalid!",           \
				s_featureTestValTypeNames[enumVal]));                                              \
		}                                                                                      \
		else                                                                                   \
		{																							                                         \
			FeatureTesterSpam("Getting " #enumVal " (" #theType ") " strFormat, __VA_ARGS__);    \
		}                                                                                      \
		return val;                                                                            \
	}

#define MAKE_WORKSPACE_FUNCTIONS(enumVal, theType, isValid, strFormat, ...)               \
	ILINE bool AddData_ ## enumVal (theType val)                                            \
	{                                                                                       \
		if (m_count < BUFFER_SIZE_FOR_READING_INSTRUCTIONS)                                   \
		{                                                                                     \
			if (! m_instructionBuffer[m_count ++].AddData_ ## enumVal(val))                     \
			{                                                                                   \
				FeatureTesterWarning("Can't load feature test \"%s\" - it contains an invalid "   \
				                     # enumVal " value", m_nameOfTestBeingLoaded);                \
				SetAbortReadingTest(true);                                                        \
			}                                                                                   \
			return true;                                                                        \
		}                                                                                     \
		FeatureTesterWarning("Out of memory reading feature tester data - please increase "   \
		                     "BUFFER_SIZE_FOR_READING_INSTRUCTIONS (currently %d)",           \
		                     BUFFER_SIZE_FOR_READING_INSTRUCTIONS);                           \
		return false;                                                                         \
	}

// TODO: Enum for the type of failure... did the test fail to run (error in data) or did the test run OK and find a problem with the game?
#define CheckFeatureTestFailure(condition, ...)        ((condition) || FeatureTestFailureFunc(#condition, string().Format(__VA_ARGS__)))
#define FeatureTestFailure(...)                        FeatureTestFailureFunc("No condition specified; please see message", string().Format(__VA_ARGS__))

//===================================================================================================
// Data types, structures and variable initialization
//===================================================================================================

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EFeatureTestValType, FeatureTestDataTypeDetails, kFTVT_Invalid, kFTVT_Num);

static AUTOENUM_BUILDNAMEARRAY(s_featureTestCommandNames, FeatureTestCommandList);
static AUTOENUM_BUILDNAMEARRAY(s_featureTestValTypeNames, FeatureTestDataTypeDetails);
static AUTOENUM_BUILDNAMEARRAY(s_featureTestPauseReasonNames, FeatureTestPauseReasonList);
static AUTOENUM_BUILDNAMEARRAY(s_featureTestRequirementNames, FeatureTestRequirementList);
static AUTOENUM_BUILDNAMEARRAY(s_featureTestHitResponseNames, FeatureTestCheckpointHitResponseList);
static AUTOENUM_BUILDNAMEARRAY(s_featureTestPlayerSelectionNames, FeatureTestPlayerSelectionList);
static AUTOENUM_BUILDNAMEARRAY(s_featureTestAimConditionNames, FeatureTestAimConditionList);

CFeatureTester::InstructionFunc CFeatureTester::s_instructionFunctions[] =
{
	FeatureTestCommandList(MAKE_INSTRUCTION_FUNC_POINTER)
};

struct SFeatureTestInstructionOrParam
{
	EFeatureTestValType m_type;

	union
	{
		FeatureTestDataTypeDetails(MAKE_UNION_CONTENTS)
	} m_data;

	FeatureTestDataTypeDetails(MAKE_FUNCTIONS)

	const char * GetContentsAsString(string & strOutput) const
	{
		switch (m_type)
		{
			FeatureTestDataTypeDetails(MAKE_CASE_STRING_OUTPUT)

			default:
			strOutput.Format("<BAD DATA>");
			break;
		}

		return strOutput.c_str();
	}

	SFeatureTestInstructionOrParam() : m_type(kFTVT_Invalid) {}
};

struct SFeatureTestDataLoadWorkspace
{
	private:
	SFeatureTestInstructionOrParam m_instructionBuffer[BUFFER_SIZE_FOR_READING_INSTRUCTIONS];
	const char * m_nameOfTestBeingLoaded;
	int m_count;
	bool m_abortReadingTest;

	public:
	SFeatureTestDataLoadWorkspace()
	{
		m_nameOfTestBeingLoaded = NULL;
		m_count = 0;
		m_abortReadingTest = false;
	}

	ILINE int GetNumInstructions()                                             { return m_count;             }
	ILINE const SFeatureTestInstructionOrParam * GetInstructionBuffer() const  { return m_instructionBuffer; }
	ILINE SFeatureTestInstructionOrParam * GetInstructionBufferModifiable()    { return m_instructionBuffer; }
	ILINE bool GetAbortReadingTest() const                                     { return m_abortReadingTest;  }

	ILINE void RewindNumInstructions(int num)
	{
		assert (num >= 0);
		assert (num <= m_count);
		assert (m_abortReadingTest);
		m_count = num;
	}

	ILINE void SetAbortReadingTest(bool abort)
	{
		m_abortReadingTest = abort;
	}

	void SetNameOfTestBeingLoaded(const char * newName)
	{
		assert ((newName == NULL) == (m_nameOfTestBeingLoaded != NULL));
		m_nameOfTestBeingLoaded = newName;
	}

	FeatureTestDataTypeDetails(MAKE_WORKSPACE_FUNCTIONS)
};

class CFeatureTestArgumentAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const { return CFeatureTester::s_instance->m_numTests; }
	virtual const char* GetValue( int nIndex ) const { return CFeatureTester::s_instance->m_featureTestArray[nIndex].m_testName; }
};

class CFeatureTestFilenameAutoComplete : public IConsoleArgumentAutoComplete
{
	public:
	CFeatureTestFilenameAutoComplete()
	{
		m_lastFrameUsed = -1;
		m_nonConstThis = this;
	}

	private:
	int m_lastFrameUsed;
	std::vector<string> m_filenames;
	CFeatureTestFilenameAutoComplete * m_nonConstThis;

	virtual int GetCount() const
	{
		int curFrame = gEnv->pRenderer->GetFrameID(false);
		if (m_lastFrameUsed != curFrame)
		{
			m_nonConstThis->FindAllFiles(curFrame);
		}

		return m_filenames.size();
	}
	virtual const char* GetValue( int nIndex ) const { return m_filenames[nIndex].c_str(); }

	void FindAllFiles(int curFrame)
	{
		m_lastFrameUsed = curFrame;
		m_filenames.clear();

		ICryPak *pPak = gEnv->pCryPak;
		_finddata_t fd;
		intptr_t handle = pPak->FindFirst("Scripts/FeatureTests/*.xml", &fd);

		if (handle > -1)
		{
			do
			{
				char buffer[64];
				size_t charsCopied = cry_copyStringUntilFindChar(buffer, fd.name, sizeof(buffer), '.');
				if (charsCopied && 0 == stricmp(fd.name + charsCopied, "xml"))
				{
					string addThis = buffer;
					m_filenames.push_back(addThis);
				}
			}
			while (pPak->FindNext(handle, &fd) >= 0);

			pPak->FindClose(handle);
		}
	}
};

CFeatureTester * CFeatureTester::s_instance = NULL;
static CFeatureTestArgumentAutoComplete s_featureTestArgumentAutoComplete;
static CFeatureTestFilenameAutoComplete s_featureTestFilenameAutoComplete;

static float s_colour_testName_Active[]      = {1.0f, 1.0f, 1.0f, 1.0f};
static float s_colour_testName_CannotStart[] = {1.0f, 1.0f, 0.1f, 1.0f};
static float s_colour_testDescription[]      = {1.0f, 1.0f, 1.0f, 1.0f};

struct IFeatureTestTextHandler
{
	virtual ~IFeatureTestTextHandler(){}
	virtual void HandleText(const char * txt) = 0;
};

class StTextHandler_FeatureTesterLog : public IFeatureTestTextHandler
{
	private:
	virtual void HandleText(const char * txt)
	{
		FeatureTesterLog("%s", txt);
	}
};

class StTextHandler_BuildString : public IFeatureTestTextHandler
{
	public:
	ILINE const char * GetText()
	{
		return m_string.c_str();
	}

	private:
	virtual void HandleText(const char * txt)
	{
		m_string.append("\n").append(txt);
	}

	string m_string;
};

//===================================================================================================
// Functions
//===================================================================================================

//-------------------------------------------------------------------------------
CFeatureTester::CFeatureTester() :
	REGISTER_GAME_MECHANISM(CFeatureTester),
	m_featureTestArray(NULL)
{
	FeatureTesterLog ("Creating feature tester instance");
	INDENT_LOG_DURING_SCOPE();

	m_informAutoTesterOfResults = NULL;
	m_currentTest = NULL;
	m_haveJustWrittenMessageAboutUnfinishedTest = NULL;
	m_currentTestNextInstruction = NULL;
	m_singleBufferContainingAllInstructions = NULL;
	m_numWatchedCheckpoints = 0;
	m_waitUntilCCCPointHit_numStillToHit = 0;
	m_runFeatureTestStack.m_count = 0;
	m_listEntityClassesWhenFail.m_num = 0;
	m_checkpointsWhichAlwaysFailATest.m_num = 0;
	m_numOverriddenInputs = 0;
	m_numTests = 0;
	m_numFeatureTestsLeftToAutoRun = 0;
	m_abortUntilNextFrame = false;
	m_pause_enableCountdown = false;
	m_timeSinceCurrentTestBegan = 0.f;
	m_saveScreenshotWhenFail = 0;

	m_pFeatureTestMgr = new CFeatureTestMgr;

	memset(& m_iterateOverParams, 0, sizeof(m_iterateOverParams));
	memset(& m_nextIteration, 0, sizeof(m_nextIteration));

	ClearLocalPlayerAutoAim();

	assert (s_instance == NULL);
	s_instance = this;

	IConsole * console = GetISystem()->GetIConsole();
	assert (console);

	REGISTER_COMMAND("ft_startTest", CmdStartTest, VF_NULL, "FEATURE TESTER: Start a feature test");
	REGISTER_COMMAND("ft_reload", CmdReload, VF_NULL, "FEATURE TESTER: Reload current feature tester data file");
	REGISTER_COMMAND("ft_load", CmdLoad, VF_NULL, "FEATURE TESTER: Load a feature tester data file");
	REGISTER_COMMAND("ft_runAll", CmdRunAll, VF_NULL, "FEATURE TESTER: Run all enabled feature tests");
	console->RegisterAutoComplete("ft_startTest", & s_featureTestArgumentAutoComplete);
	console->RegisterAutoComplete("ft_load", & s_featureTestFilenameAutoComplete);
	REGISTER_CVAR2("ft_saveScreenshotWhenFail", & m_saveScreenshotWhenFail, m_saveScreenshotWhenFail, VF_NULL, "FEATURE TESTER: When non-zero, a screenshot will be saved whenever a feature test fails");
}

//-------------------------------------------------------------------------------
CFeatureTester::~CFeatureTester()
{
	UnloadTestData();

	IConsole * console = GetISystem()->GetIConsole();
	if (console)
	{
		console->RemoveCommand("ft_startTest");
		console->RemoveCommand("ft_reload");
		console->RemoveCommand("ft_load");
		console->RemoveCommand("ft_runAll");
		console->UnregisterVariable("ft_saveScreenshotWhenFail");
	}

	delete m_pFeatureTestMgr;

	assert (s_instance == this);
	s_instance = NULL;
}

//-------------------------------------------------------------------------------
string CFeatureTester::GetContextString()
{
	string reply;
	if (s_instance == NULL)
	{
		reply.Format("(No instance) ");
	}
	else
	{
		if (s_instance->m_timeSinceCurrentTestBegan > 0.f)
		{
			reply.Format("@%.3f ", s_instance->m_timeSinceCurrentTestBegan);
		}

		if (s_instance->m_currentTest)
		{
			reply.append("(Test: ");

			if (s_instance->m_iterateOverParams.m_numParams)
			{
				for (int i = 0; i < s_instance->m_iterateOverParams.m_numParams; ++ i)
				{
					reply.append(i ? ", " : "<");
					reply.append(s_instance->m_iterateOverParams.m_currentParams[i]);
				}
				reply.append("> ");
			}

			reply.append(s_instance->m_currentTest->m_testName);
		
			for (int i = 0; i < s_instance->m_runFeatureTestStack.m_count; ++ i)
			{
				reply.append("=>");
				reply.append(s_instance->m_runFeatureTestStack.m_info[i].m_calledTest->m_testName);
			}

			reply.append(") ");
		}
	}
	return reply;
}

//-------------------------------------------------------------------------------
void CFeatureTester::PreProcessCommandNode(const IItemParamsNode * node)
{
	int numCommandsInNode = node->GetChildCount();
	for (int readCommandNum = 0; readCommandNum < numCommandsInNode; ++ readCommandNum)
	{
		const IItemParamsNode * cmdParams = node->GetChild(readCommandNum);

		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("command"));
		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("checkpointName"));
		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("inputName"));
		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("className"));
		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("testName"));
		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("level"));
		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("bone"));
		m_singleAllocTextBlock.IncreaseSizeNeeded(cmdParams->GetAttribute("customMessage"));

		PreProcessCommandNode(cmdParams);
	}
}

//-------------------------------------------------------------------------------
int CFeatureTester::PreprocessTestSet(const IItemParamsNode *testsListNode)
{
	int numTests = 0;
	int numNodesInThisSet = testsListNode->GetChildCount();
	const char * setName = testsListNode->GetAttribute("setName");

	m_singleAllocTextBlock.IncreaseSizeNeeded(setName);

	// Quickly run through all test data to calculate the amount of memory we need to allocate for strings
	for (int readChildNum = 0; readChildNum < numNodesInThisSet; ++ readChildNum)
	{
		const IItemParamsNode * oneTest = testsListNode->GetChild(readChildNum);

		if (0 == stricmp (oneTest->GetName(), "FeatureTest"))
		{
			++ numTests;

			m_singleAllocTextBlock.IncreaseSizeNeeded(oneTest->GetAttribute("name"));
			m_singleAllocTextBlock.IncreaseSizeNeeded(oneTest->GetAttribute("description"));
			m_singleAllocTextBlock.IncreaseSizeNeeded(oneTest->GetAttribute("iterateOverParams"));
			m_singleAllocTextBlock.IncreaseSizeNeeded(oneTest->GetAttribute("owners"));
			m_singleAllocTextBlock.IncreaseSizeNeeded(oneTest->GetAttribute("prerequisite"));
			PreProcessCommandNode(oneTest);
		}
		else
		{
			FeatureTesterWarning ("Found unexpected tag of type '%s' while reading test set '%s'", oneTest->GetName(), setName);
		}
	}

	return numTests;
}

//-------------------------------------------------------------------------------
void CFeatureTester::ListTestInstructions(const SFeatureTestInstructionOrParam * instruction, IFeatureTestTextHandler & textHandler) const
{
	assert (instruction->m_type == kFTVT_Command);
	bool doCurrentLinePtr = m_currentTestNextInstruction != NULL;

	while(instruction->m_data.m_kFTVT_Command != kFTC_End)
	{
		string padding = "    ";
		string entireLine;
		int paramsDone = 0;

		do 
		{
			string paramText;
			instruction->GetContentsAsString(paramText);

			switch (paramsDone)
			{
				case 0:   entireLine.append(paramText).append(" (");    break;
				case 1:   entireLine.append(paramText);                 break;
				default:  entireLine.append(", ").append(paramText);    break;
			}

			++ paramsDone;
			++ instruction;
		}
		while (instruction->m_type != kFTVT_Command);

		if (doCurrentLinePtr && m_currentTestNextInstruction <= instruction)
		{
			padding = ">   ";
			doCurrentLinePtr = false;
		}

		textHandler.HandleText(padding + entireLine + ")");
	}
}

//-------------------------------------------------------------------------------
bool CFeatureTester::ReadTestSet(const IItemParamsNode *testsListNode, SFeatureTestDataLoadWorkspace * loadWorkspace)
{
	bool bOk = true;

	const char * setName = m_singleAllocTextBlock.StoreText(testsListNode->GetAttribute("setName"));
	int numChildrenInThisSet = testsListNode->GetChildCount();

	for (int readChildNum = 0; bOk && (readChildNum < numChildrenInThisSet); ++ readChildNum)
	{
		const IItemParamsNode * oneTest = testsListNode->GetChild(readChildNum);

		if (0 == stricmp (oneTest->GetName(), "FeatureTest"))
		{
			SFeatureTest & createTest = m_featureTestArray[m_numTests ++];
			createTest.m_offsetIntoInstructionBuffer = loadWorkspace->GetNumInstructions();
			createTest.m_enabled = false;
			createTest.m_autoRunThis = false;
			createTest.m_everPassed = false;
			createTest.m_setName = setName;
			createTest.m_testName = m_singleAllocTextBlock.StoreText(oneTest->GetAttribute("name"));
			createTest.m_owners = m_singleAllocTextBlock.StoreText(oneTest->GetAttribute("owners"));
			createTest.m_prerequisite = m_singleAllocTextBlock.StoreText(oneTest->GetAttribute("prerequisite"));
			createTest.m_testDescription = m_singleAllocTextBlock.StoreText(oneTest->GetAttribute("description"));
			createTest.m_iterateOverParams = m_singleAllocTextBlock.StoreText(oneTest->GetAttribute("iterateOverParams"));
			createTest.m_requirementBitfield = AutoEnum_GetBitfieldFromString(oneTest->GetAttribute("require"), s_featureTestRequirementNames, FeatureTestRequirementList_numBits);
			createTest.m_maxTime = 60.f;
			createTest.m_scaleSpeed = 1.f;

			oneTest->GetAttribute("maxTime", createTest.m_maxTime);
			oneTest->GetAttribute("speed", createTest.m_scaleSpeed);

			if (createTest.m_requirementBitfield & kFTReq_localPlayerExists)     createTest.m_requirementBitfield |= kFTReq_inLevel;
			if (createTest.m_requirementBitfield & kFTReq_remotePlayerExists)    createTest.m_requirementBitfield |= kFTReq_inLevel;

			int readEnabled;
			if (oneTest->GetAttribute("enabled", readEnabled))
			{
				createTest.m_enabled = (readEnabled != 0);
			}

			loadWorkspace->SetNameOfTestBeingLoaded(createTest.m_testName);

			assert (! loadWorkspace->GetAbortReadingTest());

			bOk = LoadCommands(oneTest, createTest, loadWorkspace);

			if (bOk && loadWorkspace->GetAbortReadingTest())
			{
				createTest.m_enabled = false;
				loadWorkspace->RewindNumInstructions(createTest.m_offsetIntoInstructionBuffer);
				FeatureTesterWarning("Failed to load feature test '%s' (in set '%s') but continuing with the rest of the file", createTest.m_testName, setName);
				bOk = bOk && loadWorkspace->AddData_kFTVT_Command(kFTC_Fail);
			}

			bOk = bOk && loadWorkspace->AddData_kFTVT_Command(kFTC_End);

			if (bOk && createTest.m_enabled && createTest.m_owners == NULL)
			{
				FeatureTesterWarning("Test '%s' in set '%s' has no \"owners\" attribute", createTest.m_testName, setName);
			}

			FeatureTesterLog ("Finished reading test '%s' in set '%s': ID = #%04d, size = %d, test enabled = %s, %s, still ok = %s", createTest.m_testName, setName, m_numTests, loadWorkspace->GetNumInstructions() - createTest.m_offsetIntoInstructionBuffer, createTest.m_enabled ? "YES" : "NO", loadWorkspace->GetAbortReadingTest() ? "load aborted" : "loaded OK", bOk ? "YES" : "NO");

			if (bOk)
			{
				StTextHandler_FeatureTesterLog logWriter;

				if (createTest.m_iterateOverParams)
				{
					FeatureTesterLog ("Test '%s' will iterate over these parameters: \"%s\"", createTest.m_testName, createTest.m_iterateOverParams);
				}

				FeatureTesterLog("{");
				ListTestInstructions(loadWorkspace->GetInstructionBuffer() + createTest.m_offsetIntoInstructionBuffer, logWriter);
				FeatureTesterLog("}");
			}

			loadWorkspace->SetAbortReadingTest(false);
			loadWorkspace->SetNameOfTestBeingLoaded(NULL);

#if 0 && defined(USER_timf)
			cry_displayMemInHexAndAscii(string().Format("[FEATURETESTER] Test %d \"%s\": ", readChildNum + 1, createTest.m_testName), loadWorkspace->m_instructionBuffer + createTest.m_offsetIntoInstructionBuffer, (loadWorkspace->m_count - createTest.m_offsetIntoInstructionBuffer) * sizeof(loadWorkspace->m_instructionBuffer[0]), CCryLogOutputHandler(), sizeof(loadWorkspace->m_instructionBuffer[0]));
#endif
		}
	}

	return bOk;
}

//-------------------------------------------------------------------------------
bool CFeatureTester::ConvertSwitchNodeToInstructions(const IItemParamsNode * cmdParams, SFeatureTest & createTest, SFeatureTestDataLoadWorkspace * loadWorkspace)
{
	bool bOk = true;
	int numCasesInSwitch = cmdParams->GetChildCount();
	int numInstructionsAtStartOfSwitch = loadWorkspace->GetNumInstructions();

	SFeatureTestInstructionOrParam * buffer = loadWorkspace->GetInstructionBufferModifiable();

	for (int caseNum = 0; bOk && (caseNum < numCasesInSwitch); ++ caseNum)
	{
		const IItemParamsNode * caseParams = cmdParams->GetChild(caseNum);
		const char * caseType = caseParams->GetName();
		if (0 == stricmp ("Case", caseType))
		{
			// Add a conditional Jump instruction...
			bOk = bOk && loadWorkspace->AddData_kFTVT_Command(kFTC_JumpIfConditionsDoNotMatch);
			int writeJumpDestinationHere = loadWorkspace->GetNumInstructions();
			bOk = bOk && loadWorkspace->AddData_kFTVT_Int(0);
			bOk = bOk && loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(caseParams->GetAttribute("level")));

			// Add the instructions which get jumped if the conditions didn't match...
			bOk = bOk && LoadCommands(caseParams, createTest, loadWorkspace);
			bOk = bOk && loadWorkspace->AddData_kFTVT_Command(kFTC_Jump);
			bOk = bOk && loadWorkspace->AddData_kFTVT_Int(0);

			// Fix the Jump instruction which was added above (make it point to the next instruction after the ones we just added)...
			if (bOk)
			{
				assert (buffer[writeJumpDestinationHere].GetData_kFTVT_Int() == 0);
				buffer[writeJumpDestinationHere].AddData_kFTVT_Int(loadWorkspace->GetNumInstructions());
			}
		}
		else if (0 == stricmp ("Default", caseType))
		{
			bOk = bOk && LoadCommands(caseParams, createTest, loadWorkspace);
		}
		else
		{
			bOk = false;
			FeatureTesterWarning("While reading feature test '%s' set '%s': found a '%s' tag inside a 'Switch' (which is only expected to contain 'Case' and 'Default' tags)", createTest.m_testName, createTest.m_setName, caseType);
		}
	}

	if (bOk)
	{
		for (int fixInstruction = numInstructionsAtStartOfSwitch; fixInstruction < loadWorkspace->GetNumInstructions(); ++ fixInstruction)
		{
			if (buffer[fixInstruction].m_type == kFTVT_Command && buffer[fixInstruction].GetData_kFTVT_Command() == kFTC_Jump && buffer[fixInstruction + 1].GetData_kFTVT_Int() == 0)
			{
				buffer[fixInstruction + 1].AddData_kFTVT_Int(loadWorkspace->GetNumInstructions());
			}
		}
	}

	return bOk;
}

//-------------------------------------------------------------------------------
bool CFeatureTester::LoadCommands(const IItemParamsNode * node, SFeatureTest & createTest, SFeatureTestDataLoadWorkspace * loadWorkspace)
{
	int numCommandsInNode = node->GetChildCount();
	bool bOk = true;

	for (int readCommandNum = 0; bOk && (readCommandNum < numCommandsInNode); ++ readCommandNum)
	{
		const IItemParamsNode * cmdParams = node->GetChild(readCommandNum);
		const char * cmdName = cmdParams->GetName();
		int commandIndex = -1;

		FeatureTesterSpam ("Reading '%s' command", cmdName);

		if (0 == stricmp("Switch", cmdName))
		{
			ConvertSwitchNodeToInstructions(cmdParams, createTest, loadWorkspace);
		}
		else if (AutoEnum_GetEnumValFromString(cmdName, s_featureTestCommandNames, kFTC_Num, & commandIndex))
		{
			EFeatureTestCommand cmdID = (EFeatureTestCommand)commandIndex;
			if (! AddInstructionAndParams(cmdID, cmdParams, loadWorkspace))
			{
				bOk = false;
				FeatureTesterWarning("While reading feature test '%s' set '%s' instruction number %d: error while adding instruction '%s'", createTest.m_testName, createTest.m_setName, 1 + readCommandNum, cmdName);
			}
		}
		else
		{
			bOk = false;
			FeatureTesterWarning("While reading feature test '%s' set '%s' instruction number %d: '%s' isn't a valid instruction name", createTest.m_testName, createTest.m_setName, 1 + readCommandNum, cmdName);
		}
	}

	return bOk;
}

//-------------------------------------------------------------------------------
void CFeatureTester::ReadSettings(const IItemParamsNode * topSettingsNode)
{
	int numSettingsTags = topSettingsNode->GetChildCount();

	for (int i = 0; i < numSettingsTags; ++ i)
	{
		const IItemParamsNode * singleSetting = topSettingsNode->GetChild(i);
		const char * singleSettingType = singleSetting->GetName();

		if (0 == stricmp(singleSettingType, "ListEntitiesWhenFail"))
		{
			const char * className = singleSetting->GetAttribute("className");
			if (className)
			{
				IEntityClass * theClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
				if (theClass)
				{
					if (m_listEntityClassesWhenFail.m_num < m_listEntityClassesWhenFail.k_max)
					{
						FeatureTesterSpam ("Will list all '%s' entities when a test fails (slot %d)", className, m_listEntityClassesWhenFail.m_num);
						m_listEntityClassesWhenFail.m_classPtr[m_listEntityClassesWhenFail.m_num] = theClass;
						++ m_listEntityClassesWhenFail.m_num;
					}
					else
					{
						FeatureTesterWarning("Too many entity classes to list when a test fails; not adding '%s'", className);
				}
			}
				else
				{
					FeatureTesterWarning("While reading settings, failed to find entity class with name '%s'", className);
				}
		}
			else
			{
				FeatureTesterWarning("Setting of type '%s' should have a '%s' attribute", singleSettingType, "className");
			}
		}
		else if (0 == stricmp(singleSettingType, "AlwaysFailWhenHit"))
		{
			const char * checkpointName = singleSetting->GetAttribute("checkpointName");
			if (checkpointName)
			{
				if (m_checkpointsWhichAlwaysFailATest.m_num < m_checkpointsWhichAlwaysFailATest.k_max)
				{
					FeatureTesterSpam ("Will fail any test if the '%s' checkpoint is hit (slot %d)", checkpointName, m_checkpointsWhichAlwaysFailATest.m_num);
					m_checkpointsWhichAlwaysFailATest.m_checkpointName[m_checkpointsWhichAlwaysFailATest.m_num] = checkpointName;
					m_singleAllocTextBlock.IncreaseSizeNeeded(checkpointName);
					++ m_checkpointsWhichAlwaysFailATest.m_num;
				}
				else
				{
					FeatureTesterWarning("Too many checkpoints set up to always fail a test; not adding '%s'", checkpointName);
				}
			}
			else
			{
				FeatureTesterWarning("Setting of type '%s' should have a '%s' attribute", singleSettingType, "checkpointName");
			}
		}
		else
		{
			FeatureTesterWarning("Unexpected setting type '%s'", singleSettingType);
		}
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::LoadTestData(const char * filenameNoPathOrExtension) PREFAST_SUPPRESS_WARNING(6262)
{
	m_currentlyLoadedFileName = filenameNoPathOrExtension;

	string filenameStr;
	filenameStr.Format("Scripts/FeatureTests/%s.xml", filenameNoPathOrExtension);
	const char * filename = filenameStr.c_str();

	FeatureTesterLog ("Loading feature test data from file '%s' i.e. '%s'", filenameNoPathOrExtension, filename);
	INDENT_LOG_DURING_SCOPE();

	assert (m_currentTest == NULL);
	assert (m_currentTestNextInstruction == NULL);
	assert (m_numWatchedCheckpoints == 0);
	assert (m_waitUntilCCCPointHit_numStillToHit == 0);
	assert (m_numOverriddenInputs == 0);
	assert (m_numTests == 0);

	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(filename);
	const char * failureReason = NULL;
	
	if (rootNode && 0 == strcmpi(rootNode->getTag(), "FeatureTester"))
	{
		SFeatureTestDataLoadWorkspace loadWorkspace;

		assert (loadWorkspace.GetNumInstructions() == 0);

		CSingleAllocTextBlock::SReuseDuplicatedStrings duplicatedStringsWorkspace[CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_XML];
		m_singleAllocTextBlock.SetDuplicatedStringWorkspace(duplicatedStringsWorkspace, CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_XML);

		IItemParamsNode *paramNode = g_pGame->GetIGameFramework()->GetIItemSystem()->CreateParams();
		paramNode->ConvertFromXML(rootNode);

		int numChildNodes = paramNode->GetChildCount();
		int numTestsToRead = 0;

		for (int childNodeNum = 0; childNodeNum < numChildNodes; ++ childNodeNum)
		{
			const IItemParamsNode * thisNode = paramNode->GetChild(childNodeNum);
			const char * nodeName = thisNode->GetName();

			if (0 == stricmp (nodeName, "settings"))
			{
				ReadSettings (thisNode);
			}
			else if (0 == stricmp (nodeName, "tests"))
			{
				numTestsToRead += PreprocessTestSet (thisNode);
			}
			else
			{
				FeatureTesterWarning ("Found unexpected tag of type '%s' while reading '%s'", nodeName, filename);
			}
		}

		// Allocate memory
		m_singleAllocTextBlock.Allocate();
		m_featureTestArray = new SFeatureTest[numTestsToRead];

		// Currently the m_checkpointsWhichAlwaysFailATest array contains pointers to text in the XML structure. Nasty!
		// Better fix that, pronto...
		for (int i = 0; i < m_checkpointsWhichAlwaysFailATest.m_num; ++ i)
		{
			const char * oldBuffer = m_checkpointsWhichAlwaysFailATest.m_checkpointName[i];
			const char * newBuffer = m_singleAllocTextBlock.StoreText(oldBuffer);
			CRY_ASSERT_TRACE (m_checkpointsWhichAlwaysFailATest.m_checkpointName[i] && 0 == strcmp(oldBuffer, newBuffer), ("Old string %p '%s' doesn't match new string %p '%s'", oldBuffer, oldBuffer, newBuffer, newBuffer));
			m_checkpointsWhichAlwaysFailATest.m_checkpointName[i] = newBuffer;
		}

		// Run through all test data again and store stuff in the allocated memory
		for (int childNodeNum = 0; failureReason == NULL && (childNodeNum < numChildNodes); ++ childNodeNum)
		{
			const IItemParamsNode *testsListNode = paramNode->GetChild(childNodeNum);
			const char * nodeName = testsListNode->GetName();

			if (0 == stricmp (nodeName, "tests"))
			{
				if (! ReadTestSet(testsListNode, & loadWorkspace))
				{
					failureReason = "error reading a set of tests";
				}
			}
		}

		paramNode->Release();

		if (failureReason == NULL)
		{
			if (numTestsToRead != m_numTests)
			{
				failureReason = numTestsToRead < m_numTests ? "read more feature tests than expected" : "read fewer feature tests than expected";
			}
			else
			{
				// Success!

				assert (m_singleBufferContainingAllInstructions == NULL);
				m_singleBufferContainingAllInstructions = new SFeatureTestInstructionOrParam[loadWorkspace.GetNumInstructions()];

				size_t numBytes = loadWorkspace.GetNumInstructions() * sizeof(SFeatureTestInstructionOrParam);
				FeatureTesterLog ("Allocated %d bytes of memory for storing %d instructions/parameters", numBytes, loadWorkspace.GetNumInstructions());
				memcpy (m_singleBufferContainingAllInstructions, loadWorkspace.GetInstructionBuffer(), numBytes);
			}
		}
	}
	else
	{
		failureReason = "file could not be parsed";
	}

	if (failureReason == NULL)
	{
		m_singleAllocTextBlock.Lock();
		FeatureTesterLog ("Loaded all data for feature tester successfully!");

		// Feature tests are written assuming certain input settings are in place... make sure of these here! [TF]
		g_pGameCVars->cl_invertController = 0;
		g_pGameCVars->cl_invertLandVehicleInput = 0;
		g_pGameCVars->cl_invertAirVehicleInput = 0;
	}
	else
	{
		FeatureTesterWarning ("Failed to load '%s': %s", filename, failureReason);
		UnloadTestData();
	}
}

//-------------------------------------------------------------------------------
TBitfield CFeatureTester::ReadPlayerSelection(const IItemParamsNode * paramsNode)
{
	TBitfield whichPlayers = kFTPS_nobody;
	const char * whoString = paramsNode->GetAttribute("who");

	if (whoString)
	{
		whichPlayers = AutoEnum_GetBitfieldFromString(whoString, s_featureTestPlayerSelectionNames, FeatureTestPlayerSelectionList_numBits);
	}
	else
	{
		int useLocalPlayerInt = 0;
		if (paramsNode->GetAttribute("localPlayer", useLocalPlayerInt))
		{
			whichPlayers = useLocalPlayerInt ? kFTPS_localPlayer : kFTPS_firstRemotePlayer;
		}
	}

	return whichPlayers;
}

//-------------------------------------------------------------------------------
bool CFeatureTester::AddInstructionAndParams(EFeatureTestCommand cmd, const IItemParamsNode * paramsNode, SFeatureTestDataLoadWorkspace * loadWorkspace)
{
	assert (paramsNode);
	assert (loadWorkspace);

	bool bOk = loadWorkspace->AddData_kFTVT_Command(cmd);

	switch (cmd)
	{
		case kFTC_MovePlayerToOtherEntity:
		{
			bOk &= loadWorkspace->AddData_kFTVT_Who(ReadPlayerSelection(paramsNode));
			bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("className")));

			// TODO: Read flags from XML instead of hardcoding!
			uint32 requireFlags = 0;
			uint32 skipWithFlags = 0;
			bOk &= loadWorkspace->AddData_kFTVT_EntityFlags(requireFlags);
			bOk &= loadWorkspace->AddData_kFTVT_EntityFlags(skipWithFlags);

			int onTeamAsInt = 0;
			const char * onTeamText = paramsNode->GetAttribute("onTeam");
			bOk &= AutoEnum_GetEnumValFromString(onTeamText ? onTeamText : "anybody", s_featureTestAimConditionNames, kFTAC_num, & onTeamAsInt);
			bOk &= loadWorkspace->AddData_kFTVT_AimAt((EFTAimCondition) onTeamAsInt);

			// TODO: Read offset pos from XML!
		}
		break;

		case kFTC_TrySpawnPlayer:
		bOk &= loadWorkspace->AddData_kFTVT_Who(ReadPlayerSelection(paramsNode));
		break;

		case kFTC_WaitUntilPlayerIsAlive:
		{
			bOk &= loadWorkspace->AddData_kFTVT_Who(ReadPlayerSelection(paramsNode));
			float timeout = 30.f;
			paramsNode->GetAttribute("timeout", timeout);
			bOk &= loadWorkspace->AddData_kFTVT_Float(timeout);
		}
		break;

		case kFTC_SetLocalPlayerLookAt:
		{
			bOk &= loadWorkspace->AddData_kFTVT_Who(ReadPlayerSelection(paramsNode));
			bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("bone")));
		}
		break;

		case kFTC_WaitUntilPlayerAimingAt:
		{
			float timeout = 30.f;
			int targetTypeAsInt = 0;
			bOk &= loadWorkspace->AddData_kFTVT_Who(ReadPlayerSelection(paramsNode));
			bOk &= AutoEnum_GetEnumValFromString(paramsNode->GetAttribute("targetType"), s_featureTestAimConditionNames, kFTAC_num, & targetTypeAsInt);
			paramsNode->GetAttribute("timeout", timeout);
			bOk &= loadWorkspace->AddData_kFTVT_Float(timeout);
			bOk &= loadWorkspace->AddData_kFTVT_AimAt((EFTAimCondition) targetTypeAsInt);
		}
		break;

		case kFTC_RunFeatureTest:
		bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("testName")));
		break;

		case kFTC_DoConsoleCommand:
		bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("command")));
		break;

		case kFTC_WatchCCCPoint:
		bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("checkpointName")));
		break;

		case kFTC_SetResponseToHittingCCCPoint:
		{
			int responseAsInt;
			bOk &= AutoEnum_GetEnumValFromString(paramsNode->GetAttribute("response"), s_featureTestHitResponseNames, kFTCHR_num, & responseAsInt);
			EFTCheckpointHitResponse response = (EFTCheckpointHitResponse) responseAsInt;
			bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("checkpointName")));
			bOk &= loadWorkspace->AddData_kFTVT_HitResponse((EFTCheckpointHitResponse) responseAsInt);

			if (response == kFTCHR_failTest)
			{
				const char * customMessage = m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("customMessage"));
				bOk &= loadWorkspace->AddData_kFTVT_Text(customMessage ? customMessage : "");
			}
			else if (response == kFTCHR_restartTest)
			{
				float delay = 0.f;
				paramsNode->GetAttribute("restartDelay", delay);
				bOk &= loadWorkspace->AddData_kFTVT_Float(delay);
			}
		}
		break;

		case kFTC_WaitUntilHitAllExpectedCCCPoints:
		{
			float timeout = 10.f;
			bOk &= paramsNode->GetAttribute("timeout", timeout);
			bOk &= loadWorkspace->AddData_kFTVT_Float(timeout);
		}
		break;
		case kFTC_RunFlowGraphFeatureTests:
		{
			int reloadLevel = 1;
			int quickload = 0;
			float waitScheduled = 0.0f;
			float timeout = 10.f;
			paramsNode->GetAttribute("reloadLevel", reloadLevel);				// Optional
			bOk &= loadWorkspace->AddData_kFTVT_Bool(reloadLevel != 0);

			paramsNode->GetAttribute("quickload", quickload);						// Optional
			bOk &= loadWorkspace->AddData_kFTVT_Bool(quickload != 0);

			bOk &= paramsNode->GetAttribute("waitScheduled", waitScheduled);				// Optional
			bOk &= loadWorkspace->AddData_kFTVT_Float(waitScheduled);

			bOk &= paramsNode->GetAttribute("timeout", timeout);				// Required
			bOk &= loadWorkspace->AddData_kFTVT_Float(timeout);
		}
		break;

		case kFTC_SetItem:
		bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("className")));
		break;

		case kFTC_SetAmmo:
			{
				int ammo = 0;
				int clip = 0;
				bOk &= paramsNode->GetAttribute("ammo", ammo);
				bOk &= paramsNode->GetAttribute("clip", clip);
				bOk &= loadWorkspace->AddData_kFTVT_Int(ammo);
				bOk &= loadWorkspace->AddData_kFTVT_Int(clip);
			}
			break;

		case kFTC_OverrideButtonInput_Press:
		case kFTC_OverrideButtonInput_Release:
		bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("inputName")));
		break;

		case kFTC_OverrideAnalogInput:
		{
			float amount;
			bOk &= paramsNode->GetAttribute("value", amount);
			bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("inputName")));
			bOk &= loadWorkspace->AddData_kFTVT_Float(amount);
		}
		break;

		case kFTC_Wait:
		{
			float duration;
			bOk &= paramsNode->GetAttribute("duration", duration);
			bOk &= loadWorkspace->AddData_kFTVT_Float(duration);
		}
		break;

		case kFTC_CheckNumCCCPointHits:
		{
			int expectedNumHits = 0;
			bOk &= paramsNode->GetAttribute("expectedNumHits", expectedNumHits);
			bOk &= loadWorkspace->AddData_kFTVT_Text(m_singleAllocTextBlock.StoreText(paramsNode->GetAttribute("checkpointName")));
			bOk &= loadWorkspace->AddData_kFTVT_Int(expectedNumHits);
		}
		break;
	}

	return bOk;
}

//-------------------------------------------------------------------------------
void CFeatureTester::ClearLocalPlayerAutoAim()
{
	memset (& m_localPlayerAutoAimSettings, 0, sizeof(m_localPlayerAutoAimSettings));
}

//-------------------------------------------------------------------------------
void CFeatureTester::SendInputToLocalPlayer(const char * inputName, EActionActivationMode mode, float value)
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());
	if(pPlayer != NULL && pPlayer->GetPlayerInput())
	{
		pPlayer->GetPlayerInput()->OnAction(CCryName(inputName), mode, value);

		const char * releaseName = inputName;
		bool sendHoldEventEveryFrame = false;

		if (0 == strcmp(inputName, "attack1_xi"))
		{
			sendHoldEventEveryFrame = true;
		}

		// Also remember what inputs we've overridden, so we can reset them to defaults when we stop the test...
		int foundAtIndex = 0;
		while (foundAtIndex < m_numOverriddenInputs && strcmp(releaseName, m_currentlyOverriddenInputs[foundAtIndex].m_inputName))
		{
			++ foundAtIndex;
		}

		if (! ((mode == eAAM_Always && value == 0.f) || (mode == eAAM_OnRelease)))
		{
			// Store it
			if (foundAtIndex == m_numOverriddenInputs)
			{
				if (foundAtIndex < kMaxSimultaneouslyOverriddenInputs)
				{
					m_currentlyOverriddenInputs[foundAtIndex].m_inputName = releaseName;
					m_currentlyOverriddenInputs[foundAtIndex].m_mode = mode;
					m_currentlyOverriddenInputs[foundAtIndex].m_sendHoldEventEveryFrame = sendHoldEventEveryFrame;
					++ m_numOverriddenInputs;
				}
				else
				{
					FeatureTesterWarning ("%d inputs currently overridden, can't remember '%s' too...", m_numOverriddenInputs, releaseName);
				}
			}
			else
			{
				assert (m_currentlyOverriddenInputs[foundAtIndex].m_mode == mode);
			}
		}
		else if (foundAtIndex < m_numOverriddenInputs)
		{
			// Remove it from the list
			m_currentlyOverriddenInputs[foundAtIndex] = m_currentlyOverriddenInputs[-- m_numOverriddenInputs];
		}
	}
}

//-------------------------------------------------------------------------------
const SFeatureTestInstructionOrParam * CFeatureTester::GetNextInstructionOrParam()
{
	assert (m_currentTestNextInstruction);
	return (m_currentTestNextInstruction++);
}

//-------------------------------------------------------------------------------
const char * CFeatureTester::GetTextParam()
{
	const char * param = GetNextInstructionOrParam()->GetData_kFTVT_Text();

	if (param  != NULL&& param[0] == '%' && param[1] >= '1' && param[1] < ('1' + m_iterateOverParams.m_numParams) && param[2] == '\0')
	{
		int paramNum = param[1] - '1';
		assert (paramNum < m_iterateOverParams.m_numParams);
		FeatureTesterSpam ("Found text '%s' so substituting parameter %d i.e. '%s'", param, paramNum, m_iterateOverParams.m_currentParams[paramNum]);
		param = m_iterateOverParams.m_currentParams[paramNum];
		assert(param);
	}

	return param;
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_OverrideAnalogInput()
{
	const char * inputName = GetTextParam();
	float theValue = GetNextInstructionOrParam()->GetData_kFTVT_Float();
	SendInputToLocalPlayer(inputName, eAAM_Always, theValue);
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_End()
{
	CompleteSubroutine();
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_Fail()
{
	FeatureTestFailure("Reached an explicit 'Fail' command in the list of test instructions");
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_JumpIfConditionsDoNotMatch()
{
	int toHere = GetNextInstructionOrParam()->GetData_kFTVT_Int();
	const char * level = GetNextInstructionOrParam()->GetData_kFTVT_Text();
	bool match = true;

	FeatureTesterSpam ("Doing conditional jump...");

	if (level)
	{
		string mapName = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel()->GetName();
		FeatureTesterSpam ("Checking whether level name '%s' is '%s'", mapName.c_str(), level);
		if (0 != stricmp(mapName.c_str(), level))
		{
			match = false;
		}
	}

	if (! match)
	{
		FeatureTesterSpam ("Something didn't match... so jumping from instruction %d to instruction %d", m_currentTestNextInstruction - m_singleBufferContainingAllInstructions, toHere);
		m_currentTestNextInstruction = m_singleBufferContainingAllInstructions + toHere;
	}
	else
	{
		FeatureTesterSpam ("Everything matched, so not jumping from instruction %d to instruction %d", m_currentTestNextInstruction - m_singleBufferContainingAllInstructions, toHere);
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_Jump()
{
	int toHere = GetNextInstructionOrParam()->GetData_kFTVT_Int();
	FeatureTesterSpam ("Jumping from instruction %d to instruction %d", m_currentTestNextInstruction - m_singleBufferContainingAllInstructions, toHere);
	m_currentTestNextInstruction = m_singleBufferContainingAllInstructions + toHere;
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_WaitSingleFrame()
{
	PauseExecution();
}

//-------------------------------------------------------------------------------
void CFeatureTester::SetPauseStateAndTimeout(EFTPauseReason pauseReason, float timeOut)
{
	assert (timeOut >= 0.f);
	assert (pauseReason != kFTPauseReason_none || timeOut == 0.f);

	m_pause_enableCountdown = false;
	m_pause_state = pauseReason;
	m_pause_timeLeft = timeOut;
	m_pause_originalTimeOut = timeOut;	
}

//-------------------------------------------------------------------------------
bool CFeatureTester::IsActorAliveAndPlaying(IActor * iActor)
{
	CActor * actor = (CActor*)iActor;
	return actor && g_pGame->GetGameRules()->IsPlayerActivelyPlaying(actor->GetEntityId(), true) && (actor->IsClient() == false || actor->GetStance() != STANCE_NULL);
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_Wait()
{
	SetPauseStateAndTimeout(kFTPauseReason_untilTimeHasPassed, GetNextInstructionOrParam()->GetData_kFTVT_Float());
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_WaitUntilHitAllExpectedCCCPoints()
{
	float timeOut = GetNextInstructionOrParam()->GetData_kFTVT_Float();

	if (m_waitUntilCCCPointHit_numStillToHit > 0)
	{
		SetPauseStateAndTimeout(kFTPauseReason_untilCCCPointsHit, timeOut);
		FeatureTesterLog ("Waiting until %s %s hit (timeout = %.1f seconds)", GetListOfCheckpointsExpected().c_str(), (m_waitUntilCCCPointHit_numStillToHit == 1) ? "is" : "are", timeOut);
	}
	else
	{
		FeatureTesterLog ("No checkpoints still to be hit by the time we reached the WaitUntilHitAllExpectedCCCPoints instruction; continuing immediately");
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_ResetCCCPointHitCounters()
{
	for (int i = 0; i < m_numWatchedCheckpoints; ++ i)
	{
		m_checkpointCountArray[i].m_timesHit = 0;
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_SetItem()
{
	const char * itemClassName = GetTextParam();
	CActor * pClientActor = (CActor*)gEnv->pGameFramework->GetClientActor();

	if(pClientActor)
	{
		IInventory * pInventory = pClientActor->GetInventory();
		IEntityClass* itemClassPtr = gEnv->pEntitySystem->GetClassRegistry()->FindClass(itemClassName);

		if (CheckFeatureTestFailure(itemClassPtr != NULL, "There's no class called '%s'", itemClassName))
		{
			EntityId itemId = pInventory->GetItemByClass(itemClassPtr);

			// If we fail to switch to the specified item we can try to add a new one to the player's inventory...
			if (itemId == 0)
			{
				gEnv->pConsole->ExecuteString(string().Format("i_giveitem %s", itemClassName));
				itemId = pInventory->GetItemByClass(itemClassPtr);
			}

			if (CheckFeatureTestFailure(itemId != 0, "%s isn't carrying a '%s' (and i_giveitem failed to create one)", pClientActor->GetEntity()->GetName(), itemClassName))
			{
				pClientActor->SelectItem(itemId, false, false);
				SetPauseStateAndTimeout(kFTPauseReason_untilWeaponIsReadyToUse, 10.f);
			}
		}
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_SetAmmo()
{
	CActor * pClientActor = (CActor*)gEnv->pGameFramework->GetClientActor();
	if (!pClientActor)
		return;
	IInventory* pInventory = pClientActor->GetInventory();
	if (!pInventory)
		return;

	CWeapon* pCurrentWeapon = static_cast<CWeapon*>(pClientActor->GetCurrentItem()->GetIWeapon());
	if (!pCurrentWeapon)
		return;
	IFireMode* pCurrentFireMode = pCurrentWeapon->GetFireMode(pCurrentWeapon->GetCurrentFireMode());
	IEntityClass* pAmmoType = pCurrentFireMode ? pCurrentFireMode->GetAmmoType() : 0;
	if (!pAmmoType)
		return;

	int requiredAmmo = GetNextInstructionOrParam()->GetData_kFTVT_Int();
	int requiredClip = GetNextInstructionOrParam()->GetData_kFTVT_Int();
	pInventory->SetAmmoCount(pAmmoType, requiredAmmo);
	pCurrentWeapon->SetAmmoCount(pAmmoType, requiredClip);
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_OverrideButtonInput_Press()
{
	SendInputToLocalPlayer(GetTextParam(), eAAM_OnPress, 1.f);
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_OverrideButtonInput_Release()
{
	SendInputToLocalPlayer(GetTextParam(), eAAM_OnRelease, 0.f);
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_WatchCCCPoint()
{
	WatchCheckpoint(GetTextParam());
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_CheckNumCCCPointHits()
{
	const char * checkpointName = GetTextParam();
	int expectedNumTimesHit = GetNextInstructionOrParam()->GetData_kFTVT_Int();
	int actualNumTimesHit = 0;

	for (int i = 0; i < m_numWatchedCheckpoints; ++ i)
	{
		if (0 == stricmp(checkpointName, m_checkpointCountArray[i].m_checkpointName))
		{
			actualNumTimesHit = m_checkpointCountArray[i].m_timesHit;
			break;
		}
	}

	CheckFeatureTestFailure(actualNumTimesHit == expectedNumTimesHit, "Expected to have hit '%s' %d times but have actually hit it %d times", checkpointName, expectedNumTimesHit, actualNumTimesHit);
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_SetResponseToHittingCCCPoint()
{
	const char * cpName = GetTextParam();
	EFTCheckpointHitResponse response = GetNextInstructionOrParam()->GetData_kFTVT_HitResponse();
	SCheckpointCount * watchedCheckpoint = WatchCheckpoint(cpName);

	if (CheckFeatureTestFailure(watchedCheckpoint, "Feature tester isn't watching a game code coverage checkpoint called '%s'", cpName))
	{
		CRY_ASSERT(watchedCheckpoint);
		PREFAST_ASSUME(watchedCheckpoint);
		SetCheckpointHitResponse(watchedCheckpoint, response);

		if (response == kFTCHR_restartTest)
		{
			watchedCheckpoint->m_restartDelay = GetNextInstructionOrParam()->GetData_kFTVT_Float();
		}
		else if (response == kFTCHR_failTest)
		{
			watchedCheckpoint->m_customMessage = GetTextParam();
		}
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_DoConsoleCommand()
{
	const char * cmd = GetTextParam();
	FeatureTesterLog("Current feature test '%s' is executing console command '%s'", m_currentTest->m_testName, cmd);
	INDENT_LOG_DURING_SCOPE();

	// TODO: Might be a good idea to add a 'defer' bool to XML...
	bool defer = false;
	gEnv->pConsole->ExecuteString(string(cmd), true, defer);
	PauseExecution(defer);
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_RunFlowGraphFeatureTests()
{
	bool reloadLevel = GetNextInstructionOrParam()->GetData_kFTVT_Bool();
	bool quickLoad = GetNextInstructionOrParam()->GetData_kFTVT_Bool();
	float timeoutScheduled = GetNextInstructionOrParam()->GetData_kFTVT_Float();

	FeatureTesterLog("Current feature test '%s' running all registered flow graph feature tests...", m_currentTest->m_testName);
	INDENT_LOG_DURING_SCOPE();

	m_pFeatureTestMgr->SetAutoTester(m_informAutoTesterOfResults);

	m_pFeatureTestMgr->ScheduleRunAll(reloadLevel, quickLoad, timeoutScheduled);
	SetPauseStateAndTimeout(kFTPauseReason_untilFGTestsComplete, GetNextInstructionOrParam()->GetData_kFTVT_Float());
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_RunFeatureTest()
{
	const char * testName = GetTextParam();
	FeatureTesterLog("Current feature test '%s' is triggering another feature test '%s'", m_currentTest->m_testName, testName);
	INDENT_LOG_DURING_SCOPE();

	if (CheckFeatureTestFailure (m_runFeatureTestStack.m_count < m_runFeatureTestStack.k_stackSize, "Out of room on nested-feature-test execution stack (size=%d) when trying to run '%s'", m_runFeatureTestStack.k_stackSize, testName))
	{
		SFeatureTest * test = FindTestByName(testName);

		if (CheckFeatureTestFailure(test, "There's no feature test called '%s'", testName))
		{
			CRY_ASSERT(test);
			PREFAST_ASSUME(test);

			SStackedTestCallInfo * newInfo = & m_runFeatureTestStack.m_info[m_runFeatureTestStack.m_count ++];
			newInfo->m_calledTest = test;
			newInfo->m_returnToHereWhenDone = m_currentTestNextInstruction;
			FeatureTesterSpam ("Subroutine %s (@%u) scaleSpeed=%f", test->m_testName, test->m_offsetIntoInstructionBuffer, test->m_scaleSpeed);
			gEnv->pConsole->ExecuteString(string().Format("t_scale %f", test->m_scaleSpeed), true);
			m_currentTestNextInstruction = m_singleBufferContainingAllInstructions + test->m_offsetIntoInstructionBuffer;
		}
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_MovePlayerToOtherEntity()
{
	TBitfield whichPlayers = GetNextInstructionOrParam()->GetData_kFTVT_Who();
	const char * className = GetTextParam();
	IEntityClass* classPtr = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	uint32 requireFlags = GetNextInstructionOrParam()->GetData_kFTVT_EntityFlags();
	uint32 skipWithFlags = GetNextInstructionOrParam()->GetData_kFTVT_EntityFlags();
	EFTAimCondition onTeam = GetNextInstructionOrParam()->GetData_kFTVT_AimAt();

	if (CheckFeatureTestFailure(classPtr, "There's no class by the name of '%s'", className))
	{
		CGameRules * rules = g_pGame->GetGameRules();
		int i = 0;
		for (TBitfield b = BIT(0); i < FeatureTestPlayerSelectionList_numBits; i ++, b <<= 1)
		{
			if (b & whichPlayers)
			{
				CActor *pPlayer = GetPlayerForSelectionFlag(b);

				if (CheckFeatureTestFailure(pPlayer, "There's no %s player", s_featureTestPlayerSelectionNames[i]))
				{
					CRY_ASSERT(pPlayer);
					PREFAST_ASSUME(pPlayer);

					int playerTeam = rules->GetTeam(pPlayer->GetEntityId());
					IEntity * foundEntity = NULL;
					IEntityItPtr itEntity = gEnv->pEntitySystem->GetEntityIterator();
					if (itEntity)
					{
						itEntity->MoveFirst();
						while (!itEntity ->IsEnd())
						{
							IEntity * pEntity = itEntity->Next();
							if (pEntity != NULL && pEntity->GetClass() == classPtr)
							{
								uint32 entityFlags = pEntity->GetFlags();
								bool hasRequiredFlags = (entityFlags & requireFlags) == requireFlags;
								bool hasAnySkipFlags = (entityFlags & skipWithFlags) != 0;
								bool onCorrectTeam = true;

								switch (onTeam)
								{
									case kFTAC_enemy:   onCorrectTeam = rules->GetTeam(pEntity->GetId()) != playerTeam;   break;
									case kFTAC_friend:  onCorrectTeam = rules->GetTeam(pEntity->GetId()) == playerTeam;   break;
									case kFTAC_nobody:  onCorrectTeam = rules->GetTeam(pEntity->GetId()) == 0;            break;
								}

								FeatureTesterSpam("Entity %u: %s '%s' flags=%u%s%s%s (%.2f %.2f %.2f)%s, %s, %s", pEntity->GetId(), pEntity->GetClass()->GetName(), pEntity->GetName(), entityFlags, pEntity->IsHidden() ? ", HIDDEN" : "", pEntity->IsInvisible() ? ", INVISIBLE" : "", pEntity->IsActive() ? ", ACTIVE" : "", pEntity->GetWorldPos().x, pEntity->GetWorldPos().y, pEntity->GetWorldPos().z, hasAnySkipFlags ? ", skip" : "", hasRequiredFlags ? "suitable" : "not suitable", onCorrectTeam ? "team is OK" : "on wrong team");

								if (onCorrectTeam && hasRequiredFlags && ! hasAnySkipFlags)
								{
									// TODO: Perhaps if foundEntity != NULL then only set to pEntity if it's closer?
									foundEntity = pEntity;
									FeatureTesterLog("Considering moving %s next to %s '%s'", (b == kFTPS_localPlayer) ? "local player" : "all remote players", foundEntity->GetClass()->GetName(), foundEntity->GetName());
								}
							}
						}
					}

					if (foundEntity && CheckFeatureTestFailure(foundEntity, "Didn't find an appropriate entity (was looking for class '%s', with flags %u but without flags %u)", className, requireFlags, skipWithFlags))
					{
						char command[512];
						Vec3 moveToPos = foundEntity->GetWorldPos();

						FeatureTesterLog("Moving %s next to %s '%s' (%.2f %.2f %.2f)", (b == kFTPS_localPlayer) ? "local player" : "all remote players", foundEntity->GetClass()->GetName(), foundEntity->GetName(), moveToPos.x, moveToPos.y, moveToPos.z);
						INDENT_LOG_DURING_SCOPE();

						moveToPos.x += 1.f;
						moveToPos.y += 1.f;
						moveToPos.z += 1.f;
						cry_sprintf(command, "%splayerGoto %.2f %.2f %.2f 0 0 135", (b == kFTPS_localPlayer) ? "" : "sv_sendConsoleCommand ", moveToPos.x, moveToPos.y, moveToPos.z);
						gEnv->pConsole->ExecuteString(command, false, false);
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_TrySpawnPlayer()
{
	TBitfield useWhichPlayer = GetNextInstructionOrParam()->GetData_kFTVT_Who();
	IActor *pPlayer = GetPlayerForSelectionFlag(useWhichPlayer);

	if (CheckFeatureTestFailure(pPlayer, "Failed to find relevant player (%s)", GET_PLAYER_SELECTION_TEXT(useWhichPlayer)))
	{
		if (!IsActorAliveAndPlaying(pPlayer))
		{
			CRY_ASSERT(pPlayer);
			PREFAST_ASSUME(pPlayer);

			IGameRulesSpawningModule *pSpawningModule = g_pGame->GetGameRules() ? g_pGame->GetGameRules()->GetSpawningModule() : NULL;

			if (CheckFeatureTestFailure(pSpawningModule, "No spawning module present"))
			{
				CRY_ASSERT(pSpawningModule);
				PREFAST_ASSUME(pSpawningModule);

				EntityId id = pPlayer->GetEntityId();
				int teamNum = g_pGame->GetGameRules()->GetTeam(id);
				CryFixedStringT<256> strBuffer;
				FeatureTesterLog("Trying to revive %s", GetActorInfoString(pPlayer, strBuffer));
				INDENT_LOG_DURING_SCOPE();

				pSpawningModule->PerformRevive(id, teamNum, 0);
			}
		}
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_WaitUntilPlayerIsAlive()
{
	TBitfield useWhichPlayer = GetNextInstructionOrParam()->GetData_kFTVT_Who();
	float timeout = GetNextInstructionOrParam()->GetData_kFTVT_Float();
	FeatureTesterLog("Waiting until alive (%s) timeout=%.2f", GET_PLAYER_SELECTION_TEXT(useWhichPlayer), timeout);
	INDENT_LOG_DURING_SCOPE();

	SetPauseStateAndTimeout(kFTPauseReason_untilPlayerIsAlive, timeout);
	m_pausedInfo.m_waitUntilPlayerIsAlive_whichPlayers = useWhichPlayer;
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_WaitUntilPlayerAimingAt()
{
	TBitfield useWhichPlayer = GetNextInstructionOrParam()->GetData_kFTVT_Who();
	float timeout = GetNextInstructionOrParam()->GetData_kFTVT_Float();
	EFTAimCondition aimCondition = GetNextInstructionOrParam()->GetData_kFTVT_AimAt();

	FeatureTesterLog("Waiting until %s aiming at %s, timeout=%.2f", GET_PLAYER_SELECTION_TEXT(useWhichPlayer), s_featureTestAimConditionNames[aimCondition], timeout);
	INDENT_LOG_DURING_SCOPE();

	SetPauseStateAndTimeout(kFTPauseReason_untilPlayerAimingAtEnemy, timeout);
	m_pausedInfo.m_waitUntilTargettingAnEnemy_whichPlayers = useWhichPlayer;
	m_pausedInfo.m_waitUntilTargettingAnEnemy_requireTargetOfType = aimCondition;
}

//-------------------------------------------------------------------------------
void CFeatureTester::Instruction_kFTC_SetLocalPlayerLookAt()
{
	TBitfield targetWhichPlayer = GetNextInstructionOrParam()->GetData_kFTVT_Who();
	const char * boneName = GetTextParam();

	CActor * actor = GetPlayerForSelectionFlag(targetWhichPlayer);
	if (actor)
	{
		if (CheckFeatureTestFailure(actor->GetEntity()->GetCharacter(0), "Can't target an actor without a character"))
		{
			int boneNumAsInt;
			m_localPlayerAutoAimSettings.m_entityId = actor->GetEntityId();
			m_localPlayerAutoAimSettings.m_boneId = (EBonesID) (AutoEnum_GetEnumValFromString(boneName, s_BONE_ID_NAME, BONE_ID_NUM, & boneNumAsInt) ? boneNumAsInt : -1);
		}
	}
	else
	{
		ClearLocalPlayerAutoAim();
		SendInputToLocalPlayer("xi_rotatepitch", eAAM_Always, 0.f);
		SendInputToLocalPlayer("xi_rotateyaw", eAAM_Always, 0.f);
	}
}

//-------------------------------------------------------------------------------
CFeatureTester::SCheckpointCount * CFeatureTester::WatchCheckpoint(const char * cpName)
{
	SCheckpointCount * watchedCheckpoint = FindWatchedCheckpointDataByName(cpName, m_runFeatureTestStack.m_count);

	if (watchedCheckpoint == NULL)
	{
		if (m_numWatchedCheckpoints < kMaxWatchedCheckpoints)
		{
			watchedCheckpoint = & m_checkpointCountArray[m_numWatchedCheckpoints];
			++ m_numWatchedCheckpoints;

			memset (watchedCheckpoint, 0, sizeof(SCheckpointCount));
			watchedCheckpoint->m_checkpointName = cpName;
			watchedCheckpoint->m_stackLevelAtWhichAdded = m_runFeatureTestStack.m_count;
			watchedCheckpoint->m_checkpointMgrHandle = gEnv->pCodeCheckpointMgr->GetCheckpointIndex(cpName);
			const CCodeCheckpoint * checkpoint = gEnv->pCodeCheckpointMgr->GetCheckpoint(watchedCheckpoint->m_checkpointMgrHandle);
			watchedCheckpoint->m_checkpointMgrNumHitsSoFar = checkpoint ? checkpoint->HitCount() : 0u;
		}
		else
		{
			FeatureTestFailure("Can't watch checkpoint '%s', already watching %d checkpoints", cpName, kMaxWatchedCheckpoints);
		}
	}

	return watchedCheckpoint;
}

//-------------------------------------------------------------------------------
void CFeatureTester::SetCheckpointHitResponse(SCheckpointCount * checkpoint, EFTCheckpointHitResponse response)
{
	assert(response < kFTCHR_num && checkpoint->m_hitResponse < kFTCHR_num);
	if (response != checkpoint->m_hitResponse)
	{
		FeatureTesterSpam ("Checkpoint %s hit response changing from %s to %s [%u = %s]", checkpoint->m_checkpointName, s_featureTestHitResponseNames[checkpoint->m_hitResponse], s_featureTestHitResponseNames[response], m_waitUntilCCCPointHit_numStillToHit, GetListOfCheckpointsExpected().c_str());

		EFTCheckpointHitResponse oldResponse = checkpoint->m_hitResponse;
		checkpoint->m_hitResponse = kFTCHR_nothing;

		switch (oldResponse)
		{
			case kFTCHR_expectedNext:
			assert (m_waitUntilCCCPointHit_numStillToHit > 0);
			if ((-- m_waitUntilCCCPointHit_numStillToHit) == 0)
			{
				if (m_pause_state == kFTPauseReason_untilCCCPointsHit)
				{
					SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
				}
			}
			else
			{
				FeatureTesterSpam ("Still waiting for %u (%s)", m_waitUntilCCCPointHit_numStillToHit, GetListOfCheckpointsExpected().c_str());
			}
			break;

			case kFTCHR_restartTest:
			checkpoint->m_restartDelay = 0.f;
			break;
		}
	
		checkpoint->m_hitResponse = response;

		switch (checkpoint->m_hitResponse)
		{
			case kFTCHR_expectedNext:
			++ m_waitUntilCCCPointHit_numStillToHit;
			FeatureTesterSpam ("Ah-ha! Upping number of 'expected next' checkpoints to %d: %s", m_waitUntilCCCPointHit_numStillToHit, GetListOfCheckpointsExpected().c_str());
			break;
		}
	}
}

//-------------------------------------------------------------------------------
bool CFeatureTester::FeatureTestFailureFunc(const char * conditionTxt, const char * messageTxt)
{
	string reply = messageTxt;
	const char * testName = m_currentTest ? m_currentTest->m_testName : "N/A";

	if (m_runFeatureTestStack.m_count > 0)
	{
		reply.append(" [in ");
		reply.append(m_runFeatureTestStack.m_info[0].m_calledTest->m_testName);

		for (int i = 1; i < m_runFeatureTestStack.m_count; ++ i)
		{
			reply.append("=>");
			reply.append(m_runFeatureTestStack.m_info[i].m_calledTest->m_testName);
		}
		reply.append("]");
	}

	if (m_informAutoTesterOfResults == NULL)
	{
		DesignerWarningFail(conditionTxt, string().Format("Feature test '%s' failed: %s", testName, reply.c_str()));
	}
	FeatureTesterLog("Aborting feature test '%s' because it failed: %s", testName, messageTxt);
	INDENT_LOG_DURING_SCOPE();

	StopTest(reply.c_str());
	return false;
}

//-------------------------------------------------------------------------------
bool CFeatureTester::CheckPrerequisite(const SFeatureTest * test)
{
	if (test->m_prerequisite)
	{
		const SFeatureTest * prerequisiteTest = FindTestByName(test->m_prerequisite);
		if (prerequisiteTest)
		{
			if (! prerequisiteTest->m_everPassed)
			{
				FeatureTesterLog ("Test '%s' has prerequisite '%s' which was never passed, so not running this test", test->m_testName, test->m_prerequisite);
				return false;
			}
			else
			{
				FeatureTesterSpam ("Test '%s' has prerequisite '%s' which passed earlier, so we're good to go...", test->m_testName, test->m_prerequisite);
			}
		}
		else
		{
			FeatureTesterWarning("Test '%s' has prerequisite '%s' but there's no such test!", test->m_testName, test->m_prerequisite);
			return false;
		}
	}

	return true;
}

//-------------------------------------------------------------------------------
void CFeatureTester::DisplayCaption(const SFeatureTest * test)
{

	if ( IRenderer * renderer = gEnv->pRenderer )
	{
		assert (test);

		string paramString;

		if (s_instance->m_iterateOverParams.m_numParams)
		{
			for (int i = 0; i < s_instance->m_iterateOverParams.m_numParams; ++ i)
			{
				paramString.append(i ? ", " : " <");
				paramString.append(s_instance->m_iterateOverParams.m_currentParams[i]);
			}
			paramString.append(">");
		}

		int height = renderer->GetOverlayHeight();
		IRenderAuxText::Draw2dLabel(30.f, height - 80.f, 3.f, m_currentTest ? s_colour_testName_Active : s_colour_testName_CannotStart, false, "%s%s", test->m_testName, paramString.c_str());
		IRenderAuxText::Draw2dLabel(30.f, height - 45.f, 2.f, s_colour_testDescription, false, "%s", test->m_testDescription);

		ICVar * timeScaleCVar = gEnv->pConsole->GetCVar("t_Scale");
		if (timeScaleCVar)
		{
			IRenderAuxText::Draw2dLabel(30.f, height - 100.f, 2.f, s_colour_testDescription, false, "Speed = %.1f", timeScaleCVar->GetFVal());
		}
		IRenderAuxText::Draw2dLabel(30.f, height - 120.f, 2.f, s_colour_testDescription, false, "%s", m_informAutoTesterOfResults ? m_informAutoTesterOfResults->GetTestName() : "UNKNOWN");
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::Update(float dt)
{
#if 0
	IActorSystem * pActorSystem = gEnv->pGameFramework->GetIActorSystem();
	IActorIteratorPtr pIter = pActorSystem->CreateActorIterator();

	while (IActor * iActor = pIter->Next())
	{
		if (! iActor->GetGameObject()->ShouldUpdate())
		{
			CryWatch ("$4[WARNING]$o %s '%s' is not updating!", iActor->GetEntity()->GetClass()->GetName(), iActor->GetEntity()->GetName());
		}
	}
#endif

	if (gEnv->pGameFramework->StartingGameContext())
		return;

	if (m_pFeatureTestMgr)
		m_pFeatureTestMgr->Update(dt);

	if (m_numOverriddenInputs)
	{
		CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());
		IPlayerInput * playerInput = pPlayer ? pPlayer->GetPlayerInput() : NULL;

		if (playerInput)
		{
			for (int i = 0; i < m_numOverriddenInputs; ++ i)
			{
				SCurrentlyOverriddenInput & overriddenInput = m_currentlyOverriddenInputs[i];
				CryWatch ("$7[FEATURETESTER]$o Overridden input '%s' mode=%d", overriddenInput.m_inputName, overriddenInput.m_mode);

				if (overriddenInput.m_sendHoldEventEveryFrame)
				{
					pPlayer->GetPlayerInput()->OnAction(CCryName(overriddenInput.m_inputName), eAAM_OnHold, 1.f);
				}
			}
		}
	}

	float nonScaledDT = gEnv->pTimer->GetRealFrameTime();

	if (nonScaledDT > kMaximumFrameTime)
	{
		if (GetIsActive())
		{
			FeatureTesterLog ("Last frame took %g seconds; clamping to %g", nonScaledDT, kMaximumFrameTime);
		}
		nonScaledDT = kMaximumFrameTime;
	}

	if (m_currentTestNextInstruction == NULL)
	{
		if (m_nextIteration.m_test)
		{
			AttemptStartTestWithTimeout(m_nextIteration.m_test, "repeat", m_nextIteration.m_charOffset, nonScaledDT);
		}	
		else if (m_numFeatureTestsLeftToAutoRun)
		{
			while (m_numFeatureTestsLeftToAutoRun)
			{
				int i = m_numTests - m_numFeatureTestsLeftToAutoRun;
				SFeatureTest * test = & m_featureTestArray[i];

				FeatureTesterSpam ("Test %d/%d = '%s'... %s", i + 1, m_numTests, test->m_testName, test->m_enabled ? (test->m_autoRunThis ? "needs to be run" : "being skipped") : "disabled");

				if (! (test->m_autoRunThis && CheckPrerequisite(test)))
				{
					-- m_numFeatureTestsLeftToAutoRun;
				}
				else
				{
					AttemptStartTestWithTimeout(test, "auto-start", 0, nonScaledDT);
					break;
				}
			}
		}
	}

	if (m_currentTestNextInstruction)
	{
		assert (m_currentTest);
		DisplayCaption(m_currentTest);

		m_pause_enableCountdown = (m_pause_state != kFTPauseReason_none);
		m_abortUntilNextFrame = false;
		m_timeSinceCurrentTestBegan += nonScaledDT;

		CheckFeatureTestFailure (m_timeSinceCurrentTestBegan <= m_currentTest->m_maxTime, "Test has taken over %.2f seconds, aborting!", m_currentTest->m_maxTime);

		if (m_localPlayerAutoAimSettings.m_entityId)
		{
			IActor * aimAtActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_localPlayerAutoAimSettings.m_entityId);
			IActor * localActor = gEnv->pGameFramework->GetClientActor();
			if (aimAtActor != NULL && localActor != NULL && ! aimAtActor->IsDead() && ! localActor->IsDead())
			{
				Vec3 lookFromPos = localActor->GetGameObject()->GetWorldQuery()->GetPos();
				Vec3 aimAtPos = aimAtActor->GetEntity()->GetWorldPos();
				
				ICVar * timeScaleCVar = gEnv->pConsole->GetCVar("t_Scale");
				float currentGameSpeed = timeScaleCVar ? timeScaleCVar->GetFVal() : 1.f;

				if (m_localPlayerAutoAimSettings.m_boneId >= 0)
				{
					const QuatT & boneTransform = ((CActor*)aimAtActor)->GetBoneTransform(m_localPlayerAutoAimSettings.m_boneId);
					aimAtPos = aimAtActor->GetEntity()->GetWorldTM() * boneTransform.t;
				}

				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(aimAtPos, 0.1f, ColorB(128, 255, 128, 255), true);

				Vec3 currentLookDir = localActor->GetGameObject()->GetWorldQuery()->GetDir();
				Vec3 dirFromPosToTarget = (aimAtPos - lookFromPos);
				dirFromPosToTarget.Normalize();
				float currentRotation = atan2(currentLookDir.x, currentLookDir.y);
				float targetRotation = atan2(dirFromPosToTarget.x, dirFromPosToTarget.y);
				float rotationDiff = targetRotation - currentRotation;
				float rotateAmountYaw = ((rotationDiff < -gf_PI) ? (rotationDiff + gf_PI + gf_PI) : (rotationDiff > gf_PI) ? (rotationDiff - gf_PI - gf_PI) : rotationDiff) * (24.f / currentGameSpeed);
				float rotateAmountPitch = (dirFromPosToTarget.z - currentLookDir.z) * (20.f / currentGameSpeed);
//				CryWatch ("Local player currentAngle=%.3f wantAngle=%.3f diff=%.3f ROTATE=%.3f",
//					currentRotation, targetRotation, rotationDiff, rotateAmountYaw);
				SendInputToLocalPlayer("xi_rotatepitch", eAAM_Always, clamp_tpl(rotateAmountPitch, -0.75f, 0.75f));
				SendInputToLocalPlayer("xi_rotateyaw", eAAM_Always, clamp_tpl(rotateAmountYaw, -0.75f, 0.75f));
			}
			else
			{
				ClearLocalPlayerAutoAim();
				SendInputToLocalPlayer("xi_rotatepitch", eAAM_Always, 0.f);
				SendInputToLocalPlayer("xi_rotateyaw", eAAM_Always, 0.f);
			}
		}

		while (! m_abortUntilNextFrame)
		{
			EFTPauseReason oldPauseState = m_pause_state;

			PollForNewCheckpointHits();

			if (! m_abortUntilNextFrame)
			{
				switch (m_pause_state)
				{
					case kFTPauseReason_none:
					{
						EFeatureTestCommand cmd = GetNextInstructionOrParam()->GetData_kFTVT_Command();

						if (cmd >= 0 && cmd < kFTC_Num)
						{
							(this->*s_instructionFunctions[cmd])();
						}
						else
						{
							FeatureTestFailure("Invalid feature tester command number %u", cmd);
						}
					}
					break;

					case kFTPauseReason_untilWeaponIsReadyToUse:
					{
						CActor * pClientActor = (CActor*)gEnv->pGameFramework->GetClientActor();
						IItem * pCurrentItem = pClientActor ? pClientActor->GetCurrentItem() : NULL;
						IWeapon* pCurrentWep = pCurrentItem ? pCurrentItem->GetIWeapon() : NULL;

						if (pCurrentItem && pCurrentItem->IsBusy() == false && (!pCurrentWep || !pCurrentWep->IsReloading()))
						{
							SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
						}
					}
					break;

					case kFTPauseReason_untilPlayerAimingAtEnemy:
					{
						int i = 0;
						bool allOK = true;

						for (TBitfield b = BIT(0); i < FeatureTestPlayerSelectionList_numBits; i ++, b <<= 1)
						{
							if (b & m_pausedInfo.m_waitUntilTargettingAnEnemy_whichPlayers)
							{
								CActor *pPlayer = GetPlayerForSelectionFlag(b);
								bool iAmOK = IsActorAliveAndPlaying(pPlayer);

								if (iAmOK)
								{
									IEntity * currentTarget = gEnv->pEntitySystem->GetEntity(pPlayer->GetCurrentTargetEntityId());

									switch (m_pausedInfo.m_waitUntilTargettingAnEnemy_requireTargetOfType)
									{
										case kFTAC_anybody: iAmOK = (currentTarget != NULL);                                                break;
										case kFTAC_nobody:  iAmOK = (currentTarget == NULL);                                                break;
										case kFTAC_friend:  iAmOK = (currentTarget && pPlayer->IsFriendlyEntity(currentTarget->GetId()));   break;
										case kFTAC_enemy:   iAmOK = (currentTarget && ! pPlayer->IsFriendlyEntity(currentTarget->GetId())); break;
									}
								}

								CryFixedStringT<256> strBuffer;
								FeatureTesterSpam("Waiting for %s to be targetting %s... %s", GetActorInfoString(pPlayer, strBuffer), s_featureTestAimConditionNames[m_pausedInfo.m_waitUntilTargettingAnEnemy_requireTargetOfType], iAmOK ? "hooray!" : "hasn't happened yet");
								allOK = allOK && iAmOK;
							}
						}

						if (allOK)
						{
							SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
						}
					}
					break;

					case kFTPauseReason_untilPlayerIsAlive:
					{
						int i = 0;
						bool allOK = true;

						for (TBitfield b = BIT(0); (i < FeatureTestPlayerSelectionList_numBits) && m_currentTest; i ++, b <<= 1)
						{
							if (b & m_pausedInfo.m_waitUntilPlayerIsAlive_whichPlayers)
							{
								CActor *pPlayer = GetPlayerForSelectionFlag(b);
								if (CheckFeatureTestFailure(pPlayer, "There's no %s", s_featureTestPlayerSelectionNames[i]))
								{
									const SPlayerStats * stats = (const SPlayerStats *) pPlayer->GetActorStats();
									bool falling = (stats->flyMode == false) && (stats->inAir > 0.0f);
									bool iAmOK = IsActorAliveAndPlaying(pPlayer) && (pPlayer->GetHealth() == pPlayer->GetMaxHealth()) && !falling;
									if (g_pGameCVars->autotest_verbose)
									{
										CryFixedStringT<256> strBuffer;
										FeatureTesterLog("Waiting for %s to be at full health and not falling... %s", GetActorInfoString(pPlayer, strBuffer), iAmOK ? "hooray!" : "hasn't happened yet");
									}
									allOK = allOK && iAmOK;
								}
								else
								{
									allOK = false;
								}
							}
						}

						if (allOK)
						{
							SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
						}
					}
					break;

					case kFTPauseReason_untilFGTestsComplete:
					{
						if (m_pFeatureTestMgr && !m_pFeatureTestMgr->IsRunning())
						{
							// Test complete!
							SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
						}

						PauseExecution();
					}
					break;

					case kFTPauseReason_untilTimeHasPassed:
					case kFTPauseReason_untilCCCPointsHit:
					{
						if (g_pGameCVars->autotest_verbose)
						{
							IActorSystem * pActorSystem = gEnv->pGameFramework->GetIActorSystem();
							IActorIteratorPtr pIter = pActorSystem->CreateActorIterator();

							while (IActor * iActor = pIter->Next())
							{
								CryFixedStringT<256> singleLine;
								GetActorInfoString(iActor, singleLine);
								FeatureTesterLog("%s: %s", s_featureTestPauseReasonNames[m_pause_state], singleLine.c_str());
							}
						}
					}
					break;
				}

				if (m_pause_state != kFTPauseReason_none && oldPauseState != kFTPauseReason_none)
				{
					PauseExecution();
				}

				if (m_pause_enableCountdown)
				{
					if ((m_pause_timeLeft -= dt) <= 0.f)
					{
						switch (m_pause_state)
						{
							case kFTPauseReason_untilTimeHasPassed:
							SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
							break;

							case kFTPauseReason_untilCCCPointsHit:
							FeatureTestFailure("Timed out! Spent over %.1f seconds waiting to hit %s", m_pause_originalTimeOut, GetListOfCheckpointsExpected().c_str());
							break;

							default:
							FeatureTestFailure("Timed out! Spent over %.1f seconds in '%s' state", m_pause_originalTimeOut, s_featureTestPauseReasonNames[m_pause_state]);
							break;
						}
					}
				}
			}
		}

		CryWatch ("$7[FEATURETESTER]$o %sWait=%s Time=%.1f/%.1f", GetContextString().c_str(), s_featureTestPauseReasonNames[m_pause_state], m_pause_timeLeft, m_pause_originalTimeOut);

		for (int i = 0; i < m_numWatchedCheckpoints; ++ i)
		{
			EFTCheckpointHitResponse response = m_checkpointCountArray[i].m_hitResponse;
			CryWatch ("  $%c[%s]$%c %s#%d", response + '2', s_featureTestHitResponseNames[response], (m_checkpointCountArray[i].m_stackLevelAtWhichAdded == m_runFeatureTestStack.m_count) ? '1' : '9', m_checkpointCountArray[i].m_checkpointName, m_checkpointCountArray[i].m_stackLevelAtWhichAdded);
		}
	}
}

//-------------------------------------------------------------------------------
/* static */ void CFeatureTester::CmdStartTest(IConsoleCmdArgs *pArgs)
{
	if (pArgs->GetArgCount() == 2)
	{
		const char * name = pArgs->GetArg(1);

		SFeatureTest * test = s_instance->FindTestByName(name);

		if (test)
		{
			s_instance->StartTest(test, "start", 0.f);
		}
		else
		{
			FeatureTesterLog("No feature test found called \"%s\"", name);
		}
	}
	else
	{
		for (int i = 0; i < s_instance->m_numTests; ++ i)
		{
			char colourChar = s_instance->m_featureTestArray[i].m_enabled ? '5' : '9';
			FeatureTesterLog("$%c%s$o: %s", colourChar, s_instance->m_featureTestArray[i].m_testName, s_instance->m_featureTestArray[i].m_testDescription);
		}
	}
}

//-------------------------------------------------------------------------------
/* static */ void CFeatureTester::CmdReload(IConsoleCmdArgs *pArgs)
{
	if (! s_instance->m_currentlyLoadedFileName.empty())
	{
		s_instance->UnloadTestData();
		s_instance->LoadTestData(s_instance->m_currentlyLoadedFileName.c_str());
	}
	else
	{
		FeatureTesterLog ("Can't reload feature tests - there's nothing currently loaded!");
	}
}

//-------------------------------------------------------------------------------
/* static */ void CFeatureTester::CmdLoad(IConsoleCmdArgs * pArgs)
{
	if (pArgs->GetArgCount() == 2)
	{
		s_instance->UnloadTestData();
		s_instance->LoadTestData(pArgs->GetArg(1));
	}
}

//-------------------------------------------------------------------------------
/* static */ void CFeatureTester::CmdRunAll(IConsoleCmdArgs *pArgs)
{
	if (pArgs->GetArgCount() == 2)
	{
		s_instance->InterruptCurrentTestIfOneIsRunning();

		const char * setNames = pArgs->GetArg(1);
		const char * parseSetNames = setNames;
		char oneSetName[64];
		int total = 0;

		for (int i = 0; i < s_instance->m_numTests; ++ i)
		{
			s_instance->m_featureTestArray[i].m_autoRunThis = false;
		}

		while (parseSetNames)
		{
			size_t skipChars = cry_copyStringUntilFindChar(oneSetName, parseSetNames, sizeof(oneSetName), '+');
			parseSetNames = skipChars ? (parseSetNames + skipChars) : NULL;
			int countInThisSet = 0;

			for (int i = 0; i < s_instance->m_numTests; ++ i)
			{
				SFeatureTest * testData = & s_instance->m_featureTestArray[i];
				if (testData->m_enabled && 0 == stricmp (testData->m_setName, oneSetName))
				{
					testData->m_autoRunThis = true;
					++ countInThisSet;
				}
			}

			FeatureTesterLog ("Number of enabled auto-tests in set '%s' = %d", oneSetName, countInThisSet);
			total += countInThisSet;
		}

		if (total > 0)
		{
			s_instance->m_numFeatureTestsLeftToAutoRun = s_instance->m_numTests;
		}
	}

	if (s_instance->m_numFeatureTestsLeftToAutoRun == 0)
	{
		static const int kMaxSetNamesToList = 32;
		const char * setNamesDisplayed[kMaxSetNamesToList];
		const char * cmdName = pArgs->GetArg(0);
		int numSetNamesDisplayed = 0;
		FeatureTesterLog("Please specify which feature test set(s) to run, e.g.");
		for (int i = 0; i < s_instance->m_numTests; ++ i)
		{
			int j;
			const char * mySetName = s_instance->m_featureTestArray[i].m_setName;

			for (j = 0; j < numSetNamesDisplayed; ++ j)
			{
				if (setNamesDisplayed[j] == mySetName)
				{
					break;
				}
			}

			if (j == numSetNamesDisplayed && numSetNamesDisplayed < kMaxSetNamesToList)
			{
				setNamesDisplayed[j] = mySetName;
				FeatureTesterLog ("  %s %s", cmdName, mySetName);
				++ numSetNamesDisplayed;
			}
		}
		if (numSetNamesDisplayed >= 2)
		{
			FeatureTesterLog ("  %s %s+%s", cmdName, setNamesDisplayed[0], setNamesDisplayed[1]);
		}
	}
}

//-------------------------------------------------------------------------------
CFeatureTester::SFeatureTest * CFeatureTester::FindTestByName(const char * name)
{
	for (int i = 0; i < m_numTests; ++ i)
	{
		if (0 == stricmp(name, m_featureTestArray[i].m_testName))
		{
			return & m_featureTestArray[i];
		}
	}

	return NULL;
}

//-------------------------------------------------------------------------------
void CFeatureTester::UnloadTestData()
{
	if (m_informAutoTesterOfResults && m_currentTest)
	{
		FeatureTestFailure("Did not finish");
	}

	InterruptCurrentTestIfOneIsRunning();

	FeatureTesterLog ("Unloading all feature test data");
	INDENT_LOG_DURING_SCOPE();

	if (m_informAutoTesterOfResults && m_singleBufferContainingAllInstructions)
	{
		while (m_numFeatureTestsLeftToAutoRun)
		{
			SFeatureTest * test = & m_featureTestArray[m_numTests - m_numFeatureTestsLeftToAutoRun];

			if (test->m_autoRunThis)
			{
#if 0
				SubmitResultToAutoTester(test, -1.f, "Did not start");
#endif
				test->m_autoRunThis = false;
			}

			-- m_numFeatureTestsLeftToAutoRun;
		}

		m_informAutoTesterOfResults->Stop();
		m_informAutoTesterOfResults = NULL;
	}

	SAFE_DELETE_ARRAY(m_featureTestArray);
	SAFE_DELETE_ARRAY(m_singleBufferContainingAllInstructions);

	m_numTests = 0;
	m_numFeatureTestsLeftToAutoRun = 0;
	m_timeSinceCurrentTestBegan = 0.f;
	memset (& m_nextIteration, 0, sizeof(m_nextIteration));
	m_singleAllocTextBlock.Reset();
	m_haveJustWrittenMessageAboutUnfinishedTest = NULL;

	m_listEntityClassesWhenFail.m_num = 0;
	m_checkpointsWhichAlwaysFailATest.m_num = 0;
}

//-------------------------------------------------------------------------------
void CFeatureTester::RemoveWatchedCheckpointsAddedAtCurrentStackLevel(const char * reason)
{
	FeatureTesterSpam ("%s %s... removing all checkpoints of level %d [%u = %s]", m_runFeatureTestStack.m_count ? "Subroutine" : "Entire test", reason, m_runFeatureTestStack.m_count, m_waitUntilCCCPointHit_numStillToHit, GetListOfCheckpointsExpected().c_str());

	while (m_numWatchedCheckpoints > 0 && m_checkpointCountArray[m_numWatchedCheckpoints - 1].m_stackLevelAtWhichAdded == m_runFeatureTestStack.m_count)
	{
		SetCheckpointHitResponse(& m_checkpointCountArray[m_numWatchedCheckpoints - 1], kFTCHR_nothing);
		-- m_numWatchedCheckpoints;
		FeatureTesterSpam ("%s %s... no longer watching array[%d] %s#%d", m_runFeatureTestStack.m_count ? "Subroutine" : "Entire test", reason, m_numWatchedCheckpoints, m_checkpointCountArray[m_numWatchedCheckpoints].m_checkpointName, m_runFeatureTestStack.m_count);
	}
}

//-------------------------------------------------------------------------------
bool CFeatureTester::CompleteSubroutine()
{
	RemoveWatchedCheckpointsAddedAtCurrentStackLevel("completed");

	if (m_runFeatureTestStack.m_count)
	{
		--m_runFeatureTestStack.m_count;
		
		SStackedTestCallInfo * info = & m_runFeatureTestStack.m_info[m_runFeatureTestStack.m_count];
		
		const SFeatureTest * backToThisTest = m_currentTest;
		
		if (m_runFeatureTestStack.m_count > 0 && m_runFeatureTestStack.m_count < SStack::k_stackSize)
		{
			backToThisTest = m_runFeatureTestStack.m_info[m_runFeatureTestStack.m_count - 1].m_calledTest;
		}

		m_currentTestNextInstruction = info->m_returnToHereWhenDone;
		FeatureTesterSpam ("Finished using feature test '%s' as a subroutine (wait state was %s), continuing with '%s' scaleSpeed=%f", info->m_calledTest->m_testName, s_featureTestPauseReasonNames[m_pause_state], backToThisTest->m_testName, backToThisTest->m_scaleSpeed);
		gEnv->pConsole->ExecuteString(string().Format("t_scale %f", backToThisTest->m_scaleSpeed), true);
		SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
		return true;
	}

	StopTest("");
	return false;
}

//-------------------------------------------------------------------------------
const char * CFeatureTester::GetActorInfoString(IActor * iActor, CryFixedStringT<256> & reply)
{
	if (iActor)
	{
		CActor * pActor = (CActor*) iActor;
		IEntity * pEntity = pActor->GetEntity();
		Vec3 pos = pEntity->GetWorldPos();
		const Vec3 & dir = pEntity->GetForwardDir();
		CGameRules * gameRules = g_pGame->GetGameRules();
		int teamNum = gameRules->GetTeam(pEntity->GetId());
		IItem * item = pActor->GetCurrentItem();
		SActorStats * stats = pActor->GetActorStats();
		IGameObject * gameObject = pActor->GetGameObject();

		CryFixedStringT<128> itemInfo;
		if (item)
		{
			IWeapon * weapon = item->GetIWeapon();
			if (weapon)
			{
				int currentFireModeNum = weapon->GetCurrentFireMode();
				const char * zoomedText = weapon->IsZoomingInOrOut() ? (weapon->IsZoomed() ? ", zooming out" : ", zooming in") : (weapon->IsZoomed() ? ", zoomed in" : "");

				IFireMode * currentFireModePtr = weapon->GetFireMode(currentFireModeNum);
				if (currentFireModePtr)
				{
					IEntityClass * ammoType = currentFireModePtr->GetAmmoType();
					if (ammoType)
					{
						itemInfo.Format(", %s:%s %d/%d+%d %s%s%s", item->GetEntity()->GetClass()->GetName(), currentFireModePtr->GetName(), currentFireModePtr->GetAmmoCount(), currentFireModePtr->GetClipSize(), weapon->GetInventoryAmmoCount(ammoType), ammoType->GetName(), zoomedText, item->IsBusy() ? " BUSY" : "");
					}
					else
					{
						itemInfo.Format(", %s:%s %d/%d+%d NULL AMMO TYPE %s", item->GetEntity()->GetClass()->GetName(), currentFireModePtr->GetName(), currentFireModePtr->GetAmmoCount(), currentFireModePtr->GetClipSize(), zoomedText, item->IsBusy() ? " BUSY" : "");
					}
				}
				else
				{
					itemInfo.Format(", %s%s%s", item->GetEntity()->GetClass()->GetName(), zoomedText, item->IsBusy() ? " BUSY" : "");
				}
			}
			else
			{
				itemInfo.Format(", currentItem=%s%s", item->GetEntity()->GetClass()->GetName(), item->IsBusy() ? " BUSY" : "");
			}
		}

		CryFixedStringT<128> targetInfo;
		IEntity * target = gEnv->pEntitySystem->GetEntity(pActor->GetCurrentTargetEntityId());
		if (target)
		{
			bool friendly = pActor->IsFriendlyEntity(target->GetId()) != 0;
			targetInfo.Format(", targeting %s %s '%s'", friendly ? "friendly" : "enemy", target->GetClass()->GetName(), target->GetName());
		}

		reply.Format("%s '%s' (%steam=%d '%s', %s%s, health=%g/%g, stance=%s, inAir=%.2f%s%s%s%s%s%s%s%s%s) pos=<%.2f %.2f %.2f> dir=<%.2f %.2f %.2f>",
			pEntity->GetClass()->GetName(), pEntity->GetName(),
			pActor->IsPlayer() ? "" : "NPC, ",
			teamNum, teamNum ? gameRules->GetTeamName(teamNum) : "none",
			pActor->IsClient() ? "client, " : "", pActor->IsDead() ? "dead" : "alive", pActor->GetHealth(), pActor->GetMaxHealth(),
			pActor->GetStanceInfo(pActor->GetStance())->name, stats->inAir.Value(),
			(pActor->IsPlayer() && ((SPlayerStats *)(stats))->flyMode) ? ", FLYMODE" : "",
			stats->isRagDoll ? ", RAGDOLL" : "",
			pEntity->GetPhysics() ? "" : ", NO PHYSICS",
			gameObject->ShouldUpdate() ? "" : ", NOT UPDATING",
			pEntity->IsHidden() ? ", hidden" : "",
			pEntity->IsActivatedForUpdates() ? "" : ", inactive",
			pEntity->IsInvisible() ? ", invisible" : "",
			itemInfo.c_str(),
			targetInfo.c_str(),
			pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);

		CActor::EActorSpectatorMode spectatorMode = (CActor::EActorSpectatorMode)pActor->GetSpectatorMode();

		if (spectatorMode != CActor::eASM_None)
		{
			CActor::EActorSpectatorState spectatorState = pActor->GetSpectatorState();
			reply.append(string().Format(" [SPECTATING state=%d mode=%u]", spectatorState, spectatorMode).c_str());
		}
	}
	else
	{
		reply = "NULL actor";
	}

	return reply.c_str();
}

//-------------------------------------------------------------------------------
void CFeatureTester::SubmitTestStartedMessageToAutoTester(const SFeatureTest * test)
{
	if (m_informAutoTesterOfResults && test != m_haveJustWrittenMessageAboutUnfinishedTest)
	{
		string testName(test->m_setName);
		testName.append(": ");
		testName.append(test->m_testName);

		if (test == m_currentTest && m_iterateOverParams.m_numParams)
		{
			for (int i = 0; i < m_iterateOverParams.m_numParams; ++ i)
			{
				testName.append(i ? ", " : " <");
				testName.append(m_iterateOverParams.m_currentParams[i]);
			}
			testName.append(">");
		}

		testName.append(" - ");
		testName.append(test->m_testDescription);

		XmlNodeRef testCase = GetISystem()->CreateXmlNode();
		XmlNodeRef failedCase = GetISystem()->CreateXmlNode();

		failedCase->setTag("failure");
		failedCase->setAttr("type", "TestCaseFailed");
		failedCase->setAttr("message", "Test started but did not finish; this is possibly due to a crash, a hang or an unexpected process shut-down");

		testCase->setTag("testcase");
		testCase->setAttr("name", testName);
		testCase->setAttr("owners", test->m_owners ? test->m_owners : "NULL");
		testCase->addChild(failedCase);

		string testSuiteName;
		testSuiteName.Format("%s: %s", m_informAutoTesterOfResults->GetTestName(), m_currentlyLoadedFileName.c_str());
		
		m_informAutoTesterOfResults->WriteResults(m_informAutoTesterOfResults->kWriteResultsFlag_unfinished, & testSuiteName, & testCase);

		m_haveJustWrittenMessageAboutUnfinishedTest = test;
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::SubmitResultToAutoTester(const SFeatureTest * test, float timeTaken, const char * failureMessage)
{
	CryFixedStringT<1024> actuallySendFailureMessage;
	CGameRules * gameRules = g_pGame->GetGameRules();

	if (failureMessage && failureMessage[0])
	{
		actuallySendFailureMessage = failureMessage;

		float frameRate = gEnv->pTimer->GetFrameRate();
		char fpsMessage[32];
		cry_sprintf(fpsMessage, "\nFPS = %.3f", frameRate);
		actuallySendFailureMessage.append(fpsMessage);

		if (gameRules)
		{
			IActorSystem * pActorSystem = gEnv->pGameFramework->GetIActorSystem();
			IActorIteratorPtr pIter = pActorSystem->CreateActorIterator();

			ILevelInfo* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
			if (level)
			{
				actuallySendFailureMessage.append(", level = '").append(level->GetName()).append("' (").append(gameRules->GetEntity()->GetClass()->GetName()).append(")");
				actuallySendFailureMessage.append(gameRules->IsLevelLoaded() ? " loaded," : " still loading,");
				actuallySendFailureMessage.append(gameRules->IsGameInProgress() ? " in progress," : " not in progress,");
				actuallySendFailureMessage.append(string().Format(" server time=%.1f", gameRules->GetServerTime() / 1000.f).c_str());
			}

			while (IActor * iActor = pIter->Next())
			{
				CryFixedStringT<256> singleLine;
				GetActorInfoString(iActor, singleLine);
				FeatureTesterLog("FYI: %s", singleLine.c_str());
				actuallySendFailureMessage.append("\n").append(singleLine);
			}

			for (int i = 0; i < m_listEntityClassesWhenFail.m_num; ++ i)
			{
				IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();
				while ( !it->IsEnd() )
				{
					IEntity* pEntity = it->Next();
					if(pEntity->GetClass() == m_listEntityClassesWhenFail.m_classPtr[i])
					{
						string info;
						int teamNum = gameRules->GetTeam(pEntity->GetId());
						Vec3 pos = pEntity->GetWorldPos();
						const Vec3 & dir = pEntity->GetForwardDir();

						info.Format("%s '%s' (team=%d '%s'%s%s%s%s%s) pos=<%.2f %.2f %.2f> dir=<%.2f %.2f %.2f>",
							pEntity->GetClass()->GetName(), pEntity->GetName(),
							teamNum, teamNum ? gameRules->GetTeamName(teamNum) : "none",
							pEntity->GetPhysics() ? ", has physics" : ", no physics",							
							pEntity->IsHidden() ? ", hidden" : "",
							pEntity->IsActivatedForUpdates() ? "" : ", inactive",
							pEntity->IsInvisible() ? ", invisible" : "",
							pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);

						FeatureTesterLog("FYI: %s", info.c_str());
						actuallySendFailureMessage.append("\n").append(info.c_str());
					}
				}
			}
		}

		if (test == m_currentTest)
		{
			StTextHandler_BuildString stringBuilder;
			int numInStack = m_runFeatureTestStack.m_count;
			const SFeatureTest * inTest = numInStack ? m_runFeatureTestStack.m_info[numInStack - 1].m_calledTest : test;
			ListTestInstructions(m_singleBufferContainingAllInstructions + inTest->m_offsetIntoInstructionBuffer, stringBuilder);
			actuallySendFailureMessage.append(stringBuilder.GetText());
		}

		for (int i = 0; i < m_numWatchedCheckpoints; ++ i)
		{
			CryFixedStringT<32> msg;
			switch (m_checkpointCountArray[i].m_timesHit)
			{
				case 0:  msg = "never been hit"; break;
				case 1:  msg = "been hit once"; break;
				case 2:  msg = "been hit twice"; break;
				default: msg.Format("been hit %d times", m_checkpointCountArray[i].m_timesHit); break;
			}
			int stackLevel = m_checkpointCountArray[i].m_stackLevelAtWhichAdded;
			assert (stackLevel <= m_runFeatureTestStack.m_count);
			const SFeatureTest * stackTest = stackLevel ? m_runFeatureTestStack.m_info[stackLevel - 1].m_calledTest : m_currentTest;
			assert (stackTest);
			actuallySendFailureMessage.append("\n").append(stackTest->m_testName).append(": '").append(m_checkpointCountArray[i].m_checkpointName).append("' has ").append(msg);
		}

		if (m_saveScreenshotWhenFail && gEnv->pRenderer && m_informAutoTesterOfResults)
		{
			CryFixedStringT<256> screenShotFileName;
			screenShotFileName.Format("featureTestFailureImages/%s_%s/%s_%s", m_currentlyLoadedFileName.c_str(), m_informAutoTesterOfResults->GetTestName(), test->m_setName, test->m_testName);
			if (gEnv->pRenderer->ScreenShot(screenShotFileName.c_str()))
			{
				actuallySendFailureMessage.append("\nScreenshot saved to: ").append(screenShotFileName);
			}
		}

		failureMessage = actuallySendFailureMessage.c_str();
	}

	if (m_informAutoTesterOfResults)
	{
		string testName(test->m_setName);
		testName.append(": ");
		testName.append(test->m_testName);

		if (test == m_currentTest && m_iterateOverParams.m_numParams)
		{
			for (int i = 0; i < m_iterateOverParams.m_numParams; ++ i)
			{
				testName.append(i ? ", " : " <");
				testName.append(m_iterateOverParams.m_currentParams[i]);
			}
			testName.append(">");
		}

		testName.append(" - ");
		testName.append(test->m_testDescription);

		m_informAutoTesterOfResults->AddSimpleTestCase((string().Format("%s", m_currentlyLoadedFileName.c_str())).c_str(), testName.c_str(), timeTaken, failureMessage, test->m_owners);
		m_informAutoTesterOfResults->WriteResults(m_informAutoTesterOfResults->kWriteResultsFlag_unfinished);
	}

	m_haveJustWrittenMessageAboutUnfinishedTest = NULL;
}

//-------------------------------------------------------------------------------
void CFeatureTester::DoneWithTest(const SFeatureTest * doneWithThisTest, const char * actionName)
{
	if (m_numFeatureTestsLeftToAutoRun)
	{
		if (doneWithThisTest == & m_featureTestArray[m_numTests - m_numFeatureTestsLeftToAutoRun])
		{
			if (doneWithThisTest->m_autoRunThis == false)
			{
				FeatureTesterWarning ("Expected %s to be marked as running automatically and it's not!", doneWithThisTest->m_testName);
			}

			FeatureTesterLog ("%s %s so reducing 'num tests left to run' value from %d to %d", actionName, doneWithThisTest->m_testName, m_numFeatureTestsLeftToAutoRun, m_numFeatureTestsLeftToAutoRun - 1);
			m_featureTestArray[m_numTests - m_numFeatureTestsLeftToAutoRun].m_autoRunThis = false;
			-- m_numFeatureTestsLeftToAutoRun;
		}
		else
		{
			FeatureTesterWarning ("Apparently we're done with %s even though the current auto-run test is %s", doneWithThisTest->m_testName, m_featureTestArray[m_numTests - m_numFeatureTestsLeftToAutoRun].m_testName);
		}
	}

	memset(& m_nextIteration, 0, sizeof(m_nextIteration));
}

//-------------------------------------------------------------------------------
void CFeatureTester::StopTest(const char * failureMessage)
{
	CRY_ASSERT_TRACE (m_currentTest, ("Should only call StopTest function when there's a test running!"));
	CRY_ASSERT_TRACE (m_currentTestNextInstruction, ("Should only call StopTest function when there's a test running!"));

	if (failureMessage)
	{
		SubmitResultToAutoTester(m_currentTest, m_timeSinceCurrentTestBegan, failureMessage);

		if (failureMessage[0] == '\0')
		{
			FeatureTesterLog("Test '%s' complete!", m_currentTest->m_testName);
			SFeatureTest * nonConstTestPtr = const_cast<SFeatureTest*>(m_currentTest);
			nonConstTestPtr->m_everPassed = true;
		}

		if (m_iterateOverParams.m_nextIterationCharOffset)
		{
			FeatureTesterLog ("Test '%s' is finished (%s) but has further parameters so will restart soon!", m_currentTest->m_testName, failureMessage[0] ? failureMessage : "OK");
			m_nextIteration.m_test = m_currentTest;
			m_nextIteration.m_charOffset = m_iterateOverParams.m_nextIterationCharOffset;
		}
		else
		{
			DoneWithTest(m_currentTest, failureMessage[0] ? "Failed" : "Completed");
		}

		m_timeSinceCurrentTestBegan = 0.f;
	}

	gEnv->pConsole->ExecuteString("t_scale 1", true);
	m_currentTest = NULL;
	m_currentTestNextInstruction = NULL;
	m_numWatchedCheckpoints = 0;
	m_waitUntilCCCPointHit_numStillToHit = 0;
	m_runFeatureTestStack.m_count = 0;
	ClearLocalPlayerAutoAim();
	PauseExecution();

	for (int i = 0; i < m_iterateOverParams.m_numParams; ++ i)
	{
		assert (m_iterateOverParams.m_currentParams[i]);
		SAFE_DELETE_ARRAY (m_iterateOverParams.m_currentParams[i]);
	}
	memset(& m_iterateOverParams, 0, sizeof(m_iterateOverParams));

#if ENABLE_GAME_CODE_COVERAGE
	CGameCodeCoverageManager::GetInstance()->DisableListener(this);
#endif

	// Revert all inputs which are currently overridden by this feature test...
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());
	if(pPlayer != NULL && pPlayer->GetPlayerInput())
	{
		for (int i = 0; i < m_numOverriddenInputs; ++ i)
		{
			SCurrentlyOverriddenInput & overriddenInput = m_currentlyOverriddenInputs[i];

			EActionActivationMode currentMode = overriddenInput.m_mode;
			FeatureTesterSpam("Test finished while '%s' is still being overridden (mode 0x%x) - reverting it now!", overriddenInput.m_inputName, currentMode);
			CRY_ASSERT_TRACE(currentMode == eAAM_OnPress || currentMode == eAAM_Always, ("Unexpected mode 0x%x", currentMode));

			EActionActivationMode stopMode = (currentMode == eAAM_OnPress) ? eAAM_OnRelease : eAAM_Always;
			const char * releaseName = overriddenInput.m_inputName;
			pPlayer->GetPlayerInput()->OnAction(CCryName(releaseName), stopMode, 0.f);
		}
	}

	m_numOverriddenInputs = 0;
}

//-------------------------------------------------------------------------------
void CFeatureTester::InterruptCurrentTestIfOneIsRunning()
{
	if (m_currentTest)
	{
		FeatureTesterLog("Test '%s' interrupted!", m_currentTest->m_testName);
		INDENT_LOG_DURING_SCOPE();
		StopTest(NULL);
		assert (m_currentTestNextInstruction == NULL);
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::SendTextMessageToRemoteClients(const char * msg)
{
	if (gEnv->bServer && g_pGame->GetGameRules())
	{
		g_pGame->GetGameRules()->SendTextMessage(eTextMessageServer, msg, eRMI_ToRemoteClients);
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::AttemptStartTestWithTimeout(const SFeatureTest * test, const char * actionName, int offsetIntoIterationList, float dt)
{
	const char * failReason = StartTest(test, actionName, 0.f, offsetIntoIterationList);

	if (failReason)
	{
		DisplayCaption(test);
		m_timeSinceCurrentTestBegan += dt;
		string listOfFlags = AutoEnum_GetStringFromBitfield(test->m_requirementBitfield, s_featureTestRequirementNames, FeatureTestRequirementList_numBits);
		if (m_timeSinceCurrentTestBegan > test->m_maxTime)
		{
			SubmitResultToAutoTester(test, m_timeSinceCurrentTestBegan, string().Format("Did not start - %s (failed to meet requirements \"%s\" for over %.3f seconds)", failReason, listOfFlags.c_str(), test->m_maxTime));
			m_timeSinceCurrentTestBegan = 0.f;
			string message = "Timed-out trying to ";
			message.append(actionName);
			DoneWithTest(test, message.c_str());
		}
		else
		{
			CryWatch("$7[FEATURETESTER]$o Waiting for \"%s\"", listOfFlags.c_str());
		}
	}
}

//-------------------------------------------------------------------------------
const char * CFeatureTester::StartTest(const SFeatureTest * test, const char * actionName, float delay, int offsetIntoIterationList)
{
	InterruptCurrentTestIfOneIsRunning();

	assert (test);
	assert (m_numWatchedCheckpoints == 0);

	const char * failReason = NULL;
	TBitfield requirements = test->m_requirementBitfield;

	if (requirements & kFTReq_inLevel)
	{
		CGameRules * rules = g_pGame->GetGameRules();
		failReason = rules ? NULL : "there are no game rules set";

		if (failReason == NULL && (requirements & kFTReq_localPlayerExists))
		{
			IActor * pActor = gEnv->pGameFramework->GetClientActor();
			CPlayer * pPlayer = (pActor != NULL && pActor->IsPlayer()) ? (static_cast<CPlayer *>(pActor)) : NULL;
			IPlayerInput * inputs = pPlayer ? pPlayer->GetPlayerInput() : NULL;

			failReason = (pActor == NULL)   ? "there's no local actor" :
			             (pPlayer == NULL)  ? "local actor isn't a player" :
			             (inputs == NULL)   ? "local player has no player input class" :
									 NULL;
		}

		if (failReason == NULL && (requirements & kFTReq_remotePlayerExists))
		{
			IActor * remotePlayer = GetNthNonLocalActor(0);
			failReason = remotePlayer ? NULL : "there's no remote player";
		}
	}
	else if ((requirements & kFTReq_noLevelLoaded) && g_pGame->GetGameRules())
	{
		failReason = "a level is loaded";
	}

	if (failReason == NULL)
	{
		gEnv->pConsole->ExecuteString(string().Format("t_scale %f", test->m_scaleSpeed), true);
		m_currentTest = test;
		PauseExecution();
		assert (m_singleBufferContainingAllInstructions != NULL);
		m_currentTestNextInstruction = m_singleBufferContainingAllInstructions + test->m_offsetIntoInstructionBuffer;

		if (delay > 0.f)
		{
			SetPauseStateAndTimeout(kFTPauseReason_untilTimeHasPassed, delay);
			FeatureTesterLog("Test '%s' will %s in %.1f seconds!", test->m_testName, actionName, delay);
		}
		else
		{
			SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
			FeatureTesterLog("Test '%s' %sed!", test->m_testName, actionName);
		}

#if ENABLE_GAME_CODE_COVERAGE
		CGameCodeCoverageManager::GetInstance()->EnableListener(this);
#endif

		assert (m_iterateOverParams.m_numParams == 0);
		assert (m_iterateOverParams.m_nextIterationCharOffset == 0);

		if (test->m_iterateOverParams)
		{
			assert (offsetIntoIterationList < (int)strlen(test->m_iterateOverParams));
			const char * paramsStartHere = test->m_iterateOverParams + offsetIntoIterationList;
			char curParams[256];
			size_t readChars = cry_copyStringUntilFindChar(curParams, paramsStartHere, sizeof(curParams), ';');
			FeatureTesterLog("Test '%s' has parameter list '%s', this time using '%s'", test->m_testName, test->m_iterateOverParams, curParams);
			// TODO: split on ',' and support multiple parameters
			{
				size_t paramLength = strlen(curParams) + 1;

				m_iterateOverParams.m_currentParams[m_iterateOverParams.m_numParams] = new char[paramLength];
				cry_strcpy(m_iterateOverParams.m_currentParams[m_iterateOverParams.m_numParams], paramLength, curParams);
				++ m_iterateOverParams.m_numParams;
			}

			if (readChars)
			{
				m_iterateOverParams.m_nextIterationCharOffset = offsetIntoIterationList + readChars;
			}

			SendTextMessageToRemoteClients(string().Format("Feature tester %sing test '%s' with param '%s'", actionName, test->m_testName, curParams));
		}
		else
		{
			SendTextMessageToRemoteClients(string().Format("Feature tester %sing test '%s'", actionName, test->m_testName));
		}

		SubmitTestStartedMessageToAutoTester(test);

		for (int i = 0; i < m_checkpointsWhichAlwaysFailATest.m_num; ++ i)
		{
			const char * cpName = m_checkpointsWhichAlwaysFailATest.m_checkpointName[i];
			SCheckpointCount * watchedCheckpoint = WatchCheckpoint(cpName);
			assert (watchedCheckpoint);
			SetCheckpointHitResponse(watchedCheckpoint, kFTCHR_failTest);
			watchedCheckpoint->m_customMessage = "Shouldn't have hit this checkpoint at ANY TIME during this entire suite of tests!";
		}
	}
	else
	{
		FeatureTesterLog("Test '%s' didn't %s because %s", test->m_testName, actionName, failReason);
		unsigned int blinker = (unsigned int)(m_timeSinceCurrentTestBegan * 3.f);
		CryWatch ("$4%s can't %s: %s $%c(timeout=%.1f/%.1f)", test->m_testName, actionName, failReason, (blinker & 1) ? '9' : 'o', m_timeSinceCurrentTestBegan, test->m_maxTime);
	}

	return failReason;
}

//-------------------------------------------------------------------------------
CFeatureTester::SCheckpointCount * CFeatureTester::FindWatchedCheckpointDataByName(const char * name, int stackLevel)
{
	for (int i = 0; i < m_numWatchedCheckpoints; ++ i)
	{
		if ((stackLevel == m_checkpointCountArray[i].m_stackLevelAtWhichAdded) && (0 == stricmp(name, m_checkpointCountArray[i].m_checkpointName)))
		{
			return & m_checkpointCountArray[i];
		}
	}

	return NULL;
}

//-------------------------------------------------------------------------------
string CFeatureTester::GetListOfCheckpointsExpected()
{
	string reply;
	int countFound = 0;

	for (int i = 0; i < m_numWatchedCheckpoints; ++ i)
	{
		if (m_checkpointCountArray[i].m_hitResponse == kFTCHR_expectedNext)
		{
			if (countFound == 0)
			{
				reply = m_checkpointCountArray[i].m_checkpointName;
			}
			else if (countFound == m_waitUntilCCCPointHit_numStillToHit - 1)
			{
				reply.append(" and ");
				reply.append(m_checkpointCountArray[i].m_checkpointName);
			}
			else
			{
				reply.append(", ");
				reply.append(m_checkpointCountArray[i].m_checkpointName);
			}
			++ countFound;
		}
	}

	assert (countFound == m_waitUntilCCCPointHit_numStillToHit);

	return reply.empty() ? "none" : reply;
}

#if ENABLE_GAME_CODE_COVERAGE
//-------------------------------------------------------------------------------
void CFeatureTester::InformCodeCoverageCheckpointHit(CGameCodeCoverageCheckPoint * cp)
{
	InformCodeCoverageLabelHit(cp->GetLabel());
}
#endif

//-------------------------------------------------------------------------------
void CFeatureTester::InformCodeCoverageLabelHit(const char * cpLabel)
{
	int countNumDealtWith = 0;

	for (int i = 0; i <= m_runFeatureTestStack.m_count; ++ i)
	{
		SCheckpointCount * watchedCheckpoint = FindWatchedCheckpointDataByName(cpLabel, i);

		if (watchedCheckpoint)
		{
			++ countNumDealtWith;
			InformWatchedCheckpointHit(watchedCheckpoint);
		}
	}

	if (countNumDealtWith == 0)
	{
		FeatureTesterLog ("Unwatched checkpoint '%s' has been hit while state=%s", cpLabel, s_featureTestPauseReasonNames[m_pause_state]);
	}
}

//-------------------------------------------------------------------------------
void CFeatureTester::InformWatchedCheckpointHit(SCheckpointCount * watchedCheckpoint)
{
	FeatureTesterLog ("Watched checkpoint '%s#%d' has been hit (response is \"%s\") while state=%s", watchedCheckpoint->m_checkpointName, watchedCheckpoint->m_stackLevelAtWhichAdded, s_featureTestHitResponseNames[watchedCheckpoint->m_hitResponse], s_featureTestPauseReasonNames[m_pause_state]);
		++ watchedCheckpoint->m_timesHit;

		float oldTimeLeft = m_pause_timeLeft;
		float oldTotalTime = m_pause_originalTimeOut;
		EFTPauseReason oldPauseReason = m_pause_state;
		EFTCheckpointHitResponse response = watchedCheckpoint->m_hitResponse;
		SetCheckpointHitResponse(watchedCheckpoint, kFTCHR_nothing);

		if (m_pause_state == kFTPauseReason_none && oldPauseReason == kFTPauseReason_untilCCCPointsHit)
		{
			FeatureTesterLog ("Hit all checkpoints with %.1f/%.1f seconds left on the clock", oldTimeLeft, oldTotalTime);
		}

		switch (response)
		{
			case kFTCHR_restartTest:
			if (StartTest(m_currentTest, "restart", watchedCheckpoint->m_restartDelay))
			{
				FeatureTesterLog ("Failed to restart test as start conditions are no longer met...");
			}
			break;

			case kFTCHR_restartSubroutine:
			if (watchedCheckpoint->m_stackLevelAtWhichAdded == m_runFeatureTestStack.m_count)
			{
				RemoveWatchedCheckpointsAddedAtCurrentStackLevel("restarting");

				assert(m_runFeatureTestStack.m_count >= 0 && m_runFeatureTestStack.m_count < SStack::k_stackSize);
				const SFeatureTest * runningSubroutine = m_runFeatureTestStack.m_count ? m_runFeatureTestStack.m_info[m_runFeatureTestStack.m_count - 1].m_calledTest : m_currentTest;
				FeatureTesterSpam ("Running subroutine '%s' (@%u) again from the start...", runningSubroutine->m_testName, runningSubroutine->m_offsetIntoInstructionBuffer);
				m_currentTestNextInstruction = m_singleBufferContainingAllInstructions + runningSubroutine->m_offsetIntoInstructionBuffer;
				SetPauseStateAndTimeout(kFTPauseReason_none, 0.f);
				PauseExecution();
			}
			else
			{
				FeatureTesterWarning("There's currently no support for restarting any subroutine other than the one currently being executed");
			}
			break;

			case kFTCHR_completeTest:
			StopTest("");
			break;

			case kFTCHR_completeSubroutine:
			if (watchedCheckpoint->m_stackLevelAtWhichAdded == m_runFeatureTestStack.m_count)
			{
				CompleteSubroutine();
			}
			else
			{
				FeatureTesterWarning("There's currently no support for completing any subroutine other than the one currently being executed");
			}
			break;

			case kFTCHR_failTest:
			CRY_ASSERT_TRACE (watchedCheckpoint->m_customMessage != NULL, ("Custom failure message for hitting checkpoint '%s' is NULL", watchedCheckpoint->m_checkpointName));
			if (watchedCheckpoint->m_customMessage[0] == '\0')
			{
				FeatureTestFailure ("Checkpoint '%s' was hit while it was flagged as a reason for failing the test", watchedCheckpoint->m_checkpointName);
			}
			else
			{
				FeatureTestFailure ("Checkpoint '%s' was hit: %s", watchedCheckpoint->m_checkpointName, watchedCheckpoint->m_customMessage);
			}
			break;

			case kFTCHR_expectedNext:
			break;

			default:
			if (watchedCheckpoint->m_stackLevelAtWhichAdded == m_runFeatureTestStack.m_count)
			{
				CheckFeatureTestFailure (m_pause_state != kFTPauseReason_untilCCCPointsHit, "Checkpoint %s was hit while waiting to hit %s!", watchedCheckpoint->m_checkpointName, GetListOfCheckpointsExpected().c_str());
			}
			break;
		}
}

//-------------------------------------------------------------------------------
CActor * CFeatureTester::GetPlayerForSelectionFlag(TBitfield flag)
{
	switch (flag)
	{
		case kFTPS_localPlayer:
		return (CActor*)gEnv->pGameFramework->GetClientActor();
		break;

		case kFTPS_firstRemotePlayer:
		return (CActor*)GetNthNonLocalActor(0);
		break;

		case kFTPS_secondRemotePlayer:
		return (CActor*)GetNthNonLocalActor(1);
		break;
	}

	return NULL;
}

//-------------------------------------------------------------------------------
IActor * CFeatureTester::GetNthNonLocalActor(int skipThisManyBeforeReturning)
{
	IActorSystem * pActorSystem = gEnv->pGameFramework->GetIActorSystem();
	IActor * localPlayerActor = gEnv->pGameFramework->GetClientActor();
	IActorIteratorPtr pIter = pActorSystem->CreateActorIterator();

	while (IActor * pActor = pIter->Next())
	{
		if (pActor != localPlayerActor && pActor->IsPlayer())
		{
			if (-- skipThisManyBeforeReturning < 0)
			{
				return pActor;
			}
		}
	}

	return NULL;
}

//-------------------------------------------------------------------------------
void CFeatureTester::PollForNewCheckpointHits()
{
	ICodeCheckpointMgr * checkpointMgr = gEnv->pCodeCheckpointMgr;

	for (int i = 0; i < m_numWatchedCheckpoints; ++ i)
	{
		const CCodeCheckpoint * checkpoint = checkpointMgr->GetCheckpoint(m_checkpointCountArray[i].m_checkpointMgrHandle);
		uint32 numHits = checkpoint ? checkpoint->HitCount() : 0u;

		assert (numHits >= m_checkpointCountArray[i].m_checkpointMgrNumHitsSoFar);

		while (numHits > m_checkpointCountArray[i].m_checkpointMgrNumHitsSoFar)
		{
			assert (checkpoint);
			FeatureTesterSpam ("Checkpoint '%s' has been hit!", checkpoint->Name());
			++ m_checkpointCountArray[i].m_checkpointMgrNumHitsSoFar;
			InformWatchedCheckpointHit(& m_checkpointCountArray[i]);

			if (m_currentTest == NULL)
			{
				FeatureTesterSpam ("Hitting that checkpoint stopped the test! Let's not process any other checkpoint hits");
				return;
			}
		}
	}
}

#endif // ENABLE_FEATURE_TESTER
