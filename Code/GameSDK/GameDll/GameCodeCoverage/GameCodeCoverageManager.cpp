// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameCodeCoverageManager.cpp
//  Created:     18/06/2008 by Matthew
//  Description: High-level manager class for code coverage system
//               including file handing of checkpoint lists
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

/*
* File format specification:
*   The "Code Coverage Context" file format is an XML file defined as follows:

<GameCodeCoverage>

		// Optional!
		<ListDefinitions>
				<List name="CheckpointsStartingWithTheWordExample" contents="Example_*"  />
				<List name="AListCalledAFoo" contents="Foo*"                             />
				<List name="FooAndCapture" contents="AListCalledFoo CaptureTheFlag_*"    />    // Lists can contain the names of other (previously defined) lists
		</ListDefinitions>

		// Required!
		<Checkpoints>
				<Example_SomeCheckpoint                               />
				<Example_SomeOtherCheckpoint                          />
				<Foobar_Enter                                         />
				<Foobar_SvTick        requiredState="server"          />     // Optional 'requiredState' attribute defines whether each checkpoint is expected in
				<Foobar_RenderOnHUD   requiredState="hasLocalPlayer"  />     // the current game state (see GameCodeCoverageFlagList in GameCodeCoverageManager.h)
				<Foobar_Serialize                                     />
				<Foobar_Exit                                          />
				<CaptureTheFlag_Something     gameModes="CTF"         />     // Can also specify for which game mode(s) each checkpoint is valid
		</Checkpoints>

</GameCodeCoverage>

* Design notes:
*   We're only interested in checkpoints that we haven't hit yet
*   Hence we could wait until we have 90% of checkpoints and then load the context, keeping only new labels.
*   Could be worth file specifying total buffer size required.
*/

#include "StdAfx.h"

#include "GameCodeCoverage/GameCodeCoverageManager.h"
#include "GameCodeCoverage/GameCodeCoverageGUI.h"
#include "GameCodeCoverage/IGameCodeCoverageListener.h"
#include <CryString/StringUtils.h>
#include "IGameRulesSystem.h"
#include "IItemSystem.h"
#include "ITelemetryCollector.h"
#include "StatsRecordingMgr.h"

#if ENABLE_GAME_CODE_COVERAGE

#define kMaxNumCCGroups 64
#define kMaxLenGroupName 32

#define CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_GCC_XML 16

#define ENABLE_UPLOAD_OF_GCC_OUTPUT_TO_TELEMETRY_SERVER		1

CGameCodeCoverageManager * CGameCodeCoverageManager::s_instance = NULL;

static AUTOENUM_BUILDNAMEARRAY(s_requiredEnvFlagNames, GameCodeCoverageFlagList);

//---------------------------------------------------------------------------------------
struct SMatchThisPattern
{
	const char * text;
	size_t numChars;
	enum
	{
		kMatchType_wildcard,
		kMatchType_exact
	} matchType;
};

//---------------------------------------------------------------------------------------
class IGameCodeCoverageOutput
{
	public:
	virtual bool GetOK() { return true; }
	virtual void PrintComment(const char * comment) = 0;
	virtual void PrintLine(const char * heading, const char * xmlTag, const char * colour, const char * checkpointName) = 0;
};

//---------------------------------------------------------------------------------------
class CGameCodeCoverageOutput_Log : public IGameCodeCoverageOutput
{
	private:
	void PrintComment(const char * comment)
	{
		CryLogAlways ("[CODECOVERAGE] %s", comment);
	}

	void PrintLine(const char * heading, const char * xmlTag, const char * colour, const char * checkpointName)
	{
		int len = strlen(heading);
		int padding = 21 - len;
		CryLogAlways ("[CODECOVERAGE] %s:%s%s%s", heading, string(' ', max(0, padding)).c_str(), colour, checkpointName);
	}
};

//---------------------------------------------------------------------------------------
class CGameCodeCoverageOutput_File : public IGameCodeCoverageOutput
{
	public:
	CGameCodeCoverageOutput_File(const char * filename)
	{
		{
			SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();
			stream = gEnv->pCryPak->FOpen(filename, "w");
		}
		
		CRY_ASSERT_MESSAGE (stream, string().Format("Failed to open '%s' for output of game code coverage checkpoints!"));
		if (stream)
		{
			gEnv->pCryPak->FPrintf(stream, "<GameCodeCoverageOutput>\n");
		}
	}

	~CGameCodeCoverageOutput_File()
	{
		if (stream)
		{
			gEnv->pCryPak->FPrintf(stream, "</GameCodeCoverageOutput>\n");
			gEnv->pCryPak->FClose(stream);
			stream = NULL;
		}
	}

	private:
	bool GetOK()
	{
		return stream != NULL;
	}

	void PrintComment(const char * comment)
	{
		gEnv->pCryPak->FPrintf(stream, "  <!-- %s -->\n", comment);
	}

	void PrintLine(const char * heading, const char * xmlTag, const char * colour, const char * checkpointName)
	{
		if (xmlTag)
		{
			gEnv->pCryPak->FPrintf(stream, "  <%s name=\"%s\" />\n", xmlTag, checkpointName);
		}
	}

	FILE * stream;
};

//---------------------------------------------------------------------------------------
static struct SGameCodeCoverageListAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const { return CGameCodeCoverageManager::GetInstance()->GetTotalCustomListsReadFromFile(); }
	virtual const char* GetValue( int nIndex ) const { return CGameCodeCoverageManager::GetInstance()->GetCustomListName(nIndex); }
} s_gameCodeCoverageListAutoComplete;

//---------------------------------------------------------------------------------------
CGameCodeCoverageManager::CGameCodeCoverageManager(const char * filename):
  m_bContextValid(false)
, m_pAutoNamedCheckpointGroups_Array(NULL)
, m_pCustomCheckpointLists_Array(NULL)
, m_pExpectedCheckpoints_Array(NULL)
, m_nTotalCheckpointsReadFromFile(0)
, m_filepath(filename)
, REGISTER_GAME_MECHANISM(CGameCodeCoverageManager)
{
	assert (s_instance == NULL);
	s_instance = this;

	// Having a pointer to a single active listener is clearly an implementation which needs replacing if/when we have multiple
	// bits of code wanting to listen to code coverage checkpoints being hit! But it'll do for now... because who knows if/when
	// that'll happen. If one is all we need, we don't need anything more complex (linked list, stretchy vector, whatever). [TF]
	m_onlyListener = NULL;

	m_totalValidCheckpointsHit = 0;
	m_recentlyHit.m_count = 0;
	m_bCodeCoverageFailed = false;
	m_bCodeCoverageEnabled = false;
	m_bCodeCoverageDisplay = true;
	m_bCodeCoverageLogAll = true;

	IConsole * console = GetISystem()->GetIConsole();

	// Commands
	REGISTER_COMMAND("GCC_dump", CmdDumpCodeCoverage, VF_NULL, "Dump code coverage info to console/log");
	REGISTER_COMMAND("GCC_ignore", CmdGCCIgnore, VF_NULL, "Ignore a game code coverage checkpoint or group of checkpoints");
	REGISTER_COMMAND("GCC_uselist", CmdGCCUseList, VF_NULL, "View/change which of the lists (defined in the XML file) the code coverage system checks");
	REGISTER_COMMAND("GCC_upload", CmdUploadHitCheckpoints, VF_NULL, "Upload list of hit checkpoints to server");
	
	// Variables
	REGISTER_CVAR2("GCC_enabled", & m_bCodeCoverageEnabled, m_bCodeCoverageEnabled, VF_NULL, "Enable code coverage system");
	REGISTER_CVAR2("GCC_display", & m_bCodeCoverageDisplay, m_bCodeCoverageDisplay, VF_NULL, "Display code coverage stuff on the screen using CryWatch");
  REGISTER_CVAR2("GCC_logAll",  & m_bCodeCoverageLogAll,  m_bCodeCoverageLogAll,  VF_NULL, "Log all first or unexpected code coverage checkpoint hits to console and log file");

	// Auto-complete goodness
	console->RegisterAutoComplete("GCC_uselist", & s_gameCodeCoverageListAutoComplete);

	m_outputDirectory.Format("%%USER%%/CodeCoverage");
	gEnv->pCryPak->MakeDir(m_outputDirectory.c_str());

	CacheCurrentTime();
}

//---------------------------------------------------------------------------------------
CGameCodeCoverageManager::~CGameCodeCoverageManager()
{
	assert (s_instance == this);
	s_instance = NULL;

	ClearMem();

	//GetISystem()->GetIConsole()->RemoveCommand("GCC_dump");
	//GetISystem()->GetIConsole()->RemoveCommand("GCC_ignore");
	//GetISystem()->GetIConsole()->RemoveCommand("GCC_upload");
	//GetISystem()->GetIConsole()->UnregisterVariable("GCC_enabled");
	//GetISystem()->GetIConsole()->UnregisterVariable("GCC_display");
	//GetISystem()->GetIConsole()->UnregisterVariable("GCC_logAll");
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::ClearMem()
{
	SAFE_DELETE_ARRAY(m_pAutoNamedCheckpointGroups_Array);
	SAFE_DELETE_ARRAY(m_pCustomCheckpointLists_Array);
	SAFE_DELETE_ARRAY(m_pExpectedCheckpoints_Array);

	m_singleAllocTextBlock.Reset();

	m_bContextValid = false;
	m_nTotalCheckpointsReadFromFile = 0;
	m_nTotalCheckpointsReadFromFileAndValid = 0;
	m_nTotalCustomListsReadFromFile = 0;
	m_numAutoNamedCheckpointGroups = 0;
	m_listsBeingWatched_Bitfield = 0;
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::CacheCurrentTime()
{
	CryFixedStringT<128> timeStr;
	time_t ltime;

	time( &ltime );
	tm *today = localtime( &ltime );
	strftime(timeStr.m_str, timeStr.MAX_SIZE, "%y-%m-%d_%H%M%S", today);

	m_timeWhenStarted = timeStr.c_str();

	CryLogAlways("[CODECOVERAGE] From now on output filenames will contain '%s'", m_timeWhenStarted.c_str());
}

//---------------------------------------------------------------------------------------
struct STempGroupInfo
{
	char groupName[kMaxLenGroupName];
};

//---------------------------------------------------------------------------------------
int GetGroupNum(const char * txt, STempGroupInfo info[], int num)
{
	for (int i = 0; i < num; ++ i)
	{
		if (0 == stricmp (txt, info[i].groupName))
		{
			return i;
		}
	}

	return -1;
}

//---------------------------------------------------------------------------------------
bool CGameCodeCoverageManager::ReadCodeCoverageContext( IItemParamsNode *paramNode )
{
	bool bOk = true;

	assert (paramNode);

	ClearMem();

	STempGroupInfo tempGroupInfo[kMaxNumCCGroups];
	int tempGroupsFound = 0;

	const IItemParamsNode * checkpointsNode = paramNode->GetChild("Checkpoints");
	const IItemParamsNode * listDefinitions = paramNode->GetChild("ListDefinitions");

	// Read number of custom lists defined in the file
	const int maxCustomLists = sizeof(TGameCodeCoverageCustomListBitfield) * 8;	// Number of bits in type == max number of lists we can handle
	const int numListDefinitionsInFile = listDefinitions ? listDefinitions->GetChildCount() : 0;
	const int numListDefinitions = min(numListDefinitionsInFile, maxCustomLists);
	CryLog ("[CODECOVERAGE] Supports up to %d custom lists, XML file specifies %d", maxCustomLists, numListDefinitionsInFile);
	CRY_ASSERT_MESSAGE (numListDefinitions, string().Format("Code coverage system can only handle a maximum of %d custom lists, but %d are specified in the file. Please remove some lists from XML (or change the TGameCodeCoverageCustomListBitfield definition in GameCodeCoverageManager.h to be a larger type)", maxCustomLists, numListDefinitionsInFile));

	// Read number of checkpoints
	m_nTotalCheckpointsReadFromFile = checkpointsNode ? checkpointsNode->GetChildCount() : 0;

	bOk = checkpointsNode != NULL;

	if (bOk)
	{
		CSingleAllocTextBlock::SReuseDuplicatedStrings duplicatedStringsWorkspace[CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_GCC_XML];
		m_singleAllocTextBlock.SetDuplicatedStringWorkspace(duplicatedStringsWorkspace, CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_GCC_XML);

		// Pre-process the XML data to calculate how much memory is needed etc.
		for (int listNum = 0; listNum < numListDefinitions && bOk; ++ listNum)
		{
			const IItemParamsNode * listNode = listDefinitions->GetChild(listNum);
			if (0 == stricmp("List", listNode->GetName()))
			{
				const char * listName = listNode->GetAttribute("name");
				m_singleAllocTextBlock.IncreaseSizeNeeded(listName, false);
			}
			else
			{
				bOk = false;
			}
		}

		for (int i = 0; i < m_nTotalCheckpointsReadFromFile && bOk; ++ i)
		{
			const IItemParamsNode * eachCheckpointNode = checkpointsNode->GetChild(i);
			const char * name = eachCheckpointNode->GetName();
			const char * gameModes = eachCheckpointNode->GetAttribute("gameModes");
			m_singleAllocTextBlock.IncreaseSizeNeeded(name, false);
			m_singleAllocTextBlock.IncreaseSizeNeeded(gameModes, true);

			char groupName[kMaxLenGroupName];
			if (cry_copyStringUntilFindChar(groupName, name, kMaxLenGroupName, '_'))
			{
				int existingGroupNum = GetGroupNum(groupName, tempGroupInfo, tempGroupsFound);

				if (existingGroupNum < 0)
				{
					if (tempGroupsFound < kMaxNumCCGroups)
					{
						existingGroupNum = tempGroupsFound;
						++ tempGroupsFound;

						memcpy (tempGroupInfo[existingGroupNum].groupName, groupName, kMaxLenGroupName);
						m_singleAllocTextBlock.IncreaseSizeNeeded(tempGroupInfo[existingGroupNum].groupName, false);
					}
					else
					{
						bOk = false;
					}
				}
			}
		}

		if (bOk)
		{
			m_singleAllocTextBlock.Allocate();
			m_pExpectedCheckpoints_Array = new SLabelInfoFromFile[m_nTotalCheckpointsReadFromFile];
			m_pAutoNamedCheckpointGroups_Array = new CNamedCheckpointGroup[tempGroupsFound];
			m_pCustomCheckpointLists_Array = new SCustomCheckpointList[numListDefinitions];

			m_numAutoNamedCheckpointGroups = tempGroupsFound;

			for (int n = 0; n < tempGroupsFound; ++ n)
			{
				CryLog ("[CODECOVERAGE]     Group %d = '%s'", n, tempGroupInfo[n].groupName);
				m_pAutoNamedCheckpointGroups_Array[n].SetName(m_singleAllocTextBlock.StoreText(tempGroupInfo[n].groupName, false));
			}

			for (int nCheckpointsRead = 0; nCheckpointsRead < m_nTotalCheckpointsReadFromFile; ++ nCheckpointsRead)
			{
				const IItemParamsNode * eachCheckpointNode = checkpointsNode->GetChild(nCheckpointsRead);
				const char * name = eachCheckpointNode->GetName();
				const char * requiredState = eachCheckpointNode->GetAttribute("requiredState");
				
				SLabelInfoFromFile & cp = m_pExpectedCheckpoints_Array[nCheckpointsRead];

				cp.m_labelName = m_singleAllocTextBlock.StoreText(name, false);
				cp.m_gameModeNames = m_singleAllocTextBlock.StoreText(eachCheckpointNode->GetAttribute("gameModes"), true);
				cp.m_flags = AutoEnum_GetBitfieldFromString(requiredState, s_requiredEnvFlagNames, GameCodeCoverageFlagList_numBits);
				cp.m_validForCurrentGameState = false;
				cp.m_ignoreMe = false;
				cp.m_hasBeenHit = false;
				cp.m_customListBitfield = 0;

				char groupName[kMaxLenGroupName];
				size_t numChars = cry_copyStringUntilFindChar(groupName, name, kMaxLenGroupName, '_');
				assert (numChars);

				cp.m_groupNum = (TGameCodeCoverageGroupNum) GetGroupNum(groupName, tempGroupInfo, tempGroupsFound);
				assert (cp.m_groupNum < tempGroupsFound);
				m_pAutoNamedCheckpointGroups_Array[cp.m_groupNum].AddCheckpoint(& cp);
			}

			for (int listNum = 0; listNum < numListDefinitions; ++ listNum)
			{
				const IItemParamsNode * listNode = listDefinitions->GetChild(listNum);
				const char * listName = listNode->GetAttribute("name");
				ParseCustomList(m_singleAllocTextBlock.StoreText(listName, false), listNode->GetAttribute("contents"), listNum);
			}

	#if defined(USER_timf)
			cry_displayMemInHexAndAscii("[CODECOVERAGE] <str> ", m_singleAllocTextBlock.GetMem(), m_singleAllocTextBlock.GetSizeNeeded(), CCryLogAlwaysOutputHandler());
			cry_displayMemInHexAndAscii("[CODECOVERAGE] <grp> ", m_pAutoNamedCheckpointGroups_Array, sizeof(CNamedCheckpointGroup) * tempGroupsFound, CCryLogAlwaysOutputHandler(), sizeof(CNamedCheckpointGroup));
			cry_displayMemInHexAndAscii("[CODECOVERAGE] <lst> ", m_pCustomCheckpointLists_Array, sizeof(SCustomCheckpointList) * numListDefinitions, CCryLogAlwaysOutputHandler(), sizeof(SCustomCheckpointList));
			cry_displayMemInHexAndAscii("[CODECOVERAGE] <hit> ", m_pExpectedCheckpoints_Array, sizeof(SLabelInfoFromFile) * m_nTotalCheckpointsReadFromFile, CCryLogAlwaysOutputHandler(), sizeof(SLabelInfoFromFile));
	#endif
		}
	}

	CRY_ASSERT_MESSAGE(bOk, string().Format("GameCodeCoverageManager: Failed! Found file but failed after reading %d code coverage checkpoints", m_nTotalCheckpointsReadFromFile));

	if (bOk)
	{
		m_singleAllocTextBlock.Lock();

		m_bContextValid = true;
		m_nTotalCustomListsReadFromFile = numListDefinitions;

		CryLog("[CODECOVERAGE] Read all %d code coverage checkpoints (and %d checkpoint lists) successfully", m_nTotalCheckpointsReadFromFile, m_nTotalCustomListsReadFromFile);
		CheckWhichCheckpointsAreValidForGameState();
	}
	else
	{
		ClearMem();
	}

	return bOk;
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::ParseCustomList(const char * name, const char * listString, int numListsParsedSoFar)
{
	const char * moveThroughList = listString;
	size_t nowSkipAhead = 0;
	TGameCodeCoverageCustomListBitfield bit = (TGameCodeCoverageCustomListBitfield) BIT(numListsParsedSoFar);
	SCustomCheckpointList * checkpointList = & m_pCustomCheckpointLists_Array[numListsParsedSoFar];
	checkpointList->m_name = name;
	checkpointList->m_numInList = 0;

	do
	{
		char snippet[64];
		SMatchThisPattern matchThisPattern;

		nowSkipAhead = cry_copyStringUntilFindChar(snippet, moveThroughList, sizeof(snippet), ' ');

		if (GetMatchThisPatternData(snippet, & matchThisPattern))
		{
			bool doneSomething = false;

			if (matchThisPattern.matchType == SMatchThisPattern::kMatchType_exact)
			{
				for (int checkOtherList = 0; checkOtherList < numListsParsedSoFar; ++ checkOtherList)
				{
					if (MatchesPattern(m_pCustomCheckpointLists_Array[checkOtherList].m_name, & matchThisPattern))
					{
						TGameCodeCoverageCustomListBitfield checkThisBit = (TGameCodeCoverageCustomListBitfield) BIT(checkOtherList);

						for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
						{
							if (m_pExpectedCheckpoints_Array[i].m_customListBitfield & checkThisBit)
							{
								AddCheckpointToCustomList(checkpointList, i, bit);
							}
						}

						doneSomething = true;
					}
				}
			}

			if (! doneSomething)
			{
				for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
				{
					if (MatchesPattern(m_pExpectedCheckpoints_Array[i].m_labelName, & matchThisPattern))
					{
						AddCheckpointToCustomList(checkpointList, i, bit);
						doneSomething = true;
					}
				}
			}

			CRY_ASSERT_MESSAGE (doneSomething, string().Format("While reading list '%s', pattern '%s' didn't match any code coverage checkpoint names (or name of any other list)", name, snippet));
		}

		moveThroughList += nowSkipAhead;
	}
	while (nowSkipAhead);

	CryLog ("[CODECOVERAGE] List '%s' contains %u checkpoints", name, checkpointList->m_numInList);
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::AddCheckpointToCustomList(SCustomCheckpointList * checkpointList, int checkpointNum, TGameCodeCoverageCustomListBitfield bit)
{
	m_pExpectedCheckpoints_Array[checkpointNum].m_customListBitfield |= bit;
	++ checkpointList->m_numInList;
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::GetRemainingCheckpointLabels( std::vector < const char * > &vLabels ) const
{
	int reserveNum = m_nTotalCheckpointsReadFromFileAndValid - GetTotalValidCheckpointsHit(); 
	vLabels.clear();
	vLabels.reserve(reserveNum);

	for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
	{
		const SLabelInfoFromFile & cp = m_pExpectedCheckpoints_Array[i];

		if (cp.m_validForCurrentGameState && ! cp.m_hasBeenHit)
		{
			vLabels.push_back(cp.m_labelName);
		}
	}

	CRY_ASSERT_MESSAGE(vLabels.size() == reserveNum, string().Format("Reserved room for %d elements, but vector size wound up being %d", reserveNum, vLabels.size()));
}
	
//---------------------------------------------------------------------------------------
SLabelInfoFromFile * CGameCodeCoverageManager::FindWatchedCheckpointStructByName(const char *szCheckPoint) const
{
	for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
	{
		SLabelInfoFromFile & cp = m_pExpectedCheckpoints_Array[i];

		if (0 == strcmp (cp.m_labelName, szCheckPoint))
		{
			return & cp;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------------------
bool CGameCodeCoverageManager::GetShouldIgnoreCheckpoint(const SLabelInfoFromFile * cp) const
{
	return cp && (cp->m_ignoreMe == false) && (m_listsBeingWatched_Bitfield && 0 == (cp->m_customListBitfield & m_listsBeingWatched_Bitfield));
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::Update(float frameTime)
{
	// First, try to load context if required
	if (m_bCodeCoverageEnabled && !m_bCodeCoverageFailed && !IsContextValid())
	{
		XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(m_filepath);
		bool bOk = (rootNode && 0 == strcmpi(rootNode->getTag(), "GameCodeCoverage"));

		if (bOk)
		{
			IItemParamsNode *paramNode = g_pGame->GetIGameFramework()->GetIItemSystem()->CreateParams();
			paramNode->ConvertFromXML(rootNode);
			bOk = ReadCodeCoverageContext(paramNode);
			paramNode->Release();

			CryLogAlways("[CODECOVERAGE] %s code coverage context file \"%s\"", bOk ? "Successfully loaded" : "Failed during read of", m_filepath);
		}
		else
		{
			CryLogAlways("[CODECOVERAGE] Failed to open code coverage context file \"%s\"", m_filepath);
		}

		m_bCodeCoverageFailed = !bOk;
	}

	if (IsContextValid())
	{
		for (CheckPointVector::const_iterator it = m_vecCheckPoints.begin(); it != m_vecCheckPoints.end(); ++it)
		{
			const char * name = (*it)->GetLabel();
			SLabelInfoFromFile * watchedCheckpointStruct = FindWatchedCheckpointStructByName(name);
			bool ignoreMe = GetShouldIgnoreCheckpoint(watchedCheckpointStruct);

			// Always remember the checkpoint has been hit, even if it's invalid for current game state or is being ignored...
			if (watchedCheckpointStruct)
			{
				assert (watchedCheckpointStruct->m_hasBeenHit == false);
				if (watchedCheckpointStruct->m_validForCurrentGameState)
				{
					m_pAutoNamedCheckpointGroups_Array[watchedCheckpointStruct->m_groupNum].IncreaseNumHit();
					m_totalValidCheckpointsHit ++;
				}
				watchedCheckpointStruct->m_hasBeenHit = true;
			}

			// Feedback to user: don't do anything for ignored checkpoints, otherwise say whether we expected to hit the checkpoint
			if ((! ignoreMe) && watchedCheckpointStruct && watchedCheckpointStruct->m_validForCurrentGameState)
			{
				AddToRecentlyHitList(name, true);
			}

			// Maintain list of all checkpoints which have been hit - only used in Reset function
			m_mCheckPoints.insert(std::make_pair(name, *it));
		}

		m_vecCheckPoints.clear();
	}

	if (m_bCodeCoverageEnabled)
	{
		while (m_recentlyHit.m_count && m_recentlyHit.m_array[0].fTime <= 0.f)
		{
			RemoveFirstEntryFromRecentlyHitList();
		}

		for (int i = 0; i < m_recentlyHit.m_count; ++i)
		{
			m_recentlyHit.m_array[i].fTime -= frameTime;
		}
	}

	if ((m_bCodeCoverageEnabled != 0) && m_bCodeCoverageDisplay)
	{
		CGameCodeCoverageGUI::GetInstance()->Draw();
	}
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::Inform(EGameMechanismEvent gmEvent, const SGameMechanismEventData * data)
{
	switch (gmEvent)
	{
		case kGMEvent_GameRulesInit:
		case kGMEvent_GameRulesDestroyed:
		Reset();
		break;
	}
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::CmdGCCIgnore(IConsoleCmdArgs *pArgs)
{
	int numArgs = pArgs->GetArgCount();
	assert (numArgs >= 1);

	CGameCodeCoverageManager * mgr = GetInstance();

	if (mgr->IsContextValid())
	{
		for (int i = 1; i < numArgs; ++ i)
		{
			const char * argument = pArgs->GetArg(i);
			mgr->IgnoreAllCheckpointsWhichMatchPattern(true, argument);
		}

		mgr->CheckWhichCheckpointsAreValidForGameState();
	}
	else
	{
		CryLogAlways("[CODECOVERAGE] Can't use %s command until we've loaded a code coverage context", pArgs->GetArg(0));
	}
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::CmdGCCUseList(IConsoleCmdArgs *pArgs)
{
	int numArgs = pArgs->GetArgCount();
	assert (numArgs >= 1);

	CGameCodeCoverageManager * mgr = GetInstance();

	// If given any params, we actually want to change the m_listsBeingWatched_Bitfield value...
	if (numArgs > 1)
	{
		mgr->m_listsBeingWatched_Bitfield = 0;

		if (numArgs == 2 && 0 == stricmp ("All", pArgs->GetArg(1)))
		{
			// Special case: specifying the command and then the single argument "All" leaves the value set to 0 without a warning
		}
		else
		{
			for (int i = 1; i < numArgs; ++ i)
			{
				const char * argument = pArgs->GetArg(i);
				int findIt;
				TGameCodeCoverageCustomListBitfield bit = 1;

				for (findIt = 0; findIt < mgr->m_nTotalCustomListsReadFromFile; ++ findIt, bit <<= 1)
				{
					if (0 == stricmp (argument, mgr->m_pCustomCheckpointLists_Array[findIt].m_name))
					{
						mgr->m_listsBeingWatched_Bitfield |= bit;
						break;
					}
				}

				if (findIt == mgr->m_nTotalCustomListsReadFromFile)
				{
					CryLogAlways("[CODECOVERAGE] WARNING: There's no custom checkpoint list called '%s'", argument);
				}
			}
		}

		mgr->CheckWhichCheckpointsAreValidForGameState();
	}

	// Whether or not we changed the member variable, display which lists are being watched
	int count = 0;
	TGameCodeCoverageCustomListBitfield bit = 1;

	for (int displayNum = 0; displayNum < mgr->m_nTotalCustomListsReadFromFile; ++ displayNum, bit <<= 1)
	{
		SCustomCheckpointList * listData = & mgr->m_pCustomCheckpointLists_Array[displayNum];
		int on = (mgr->m_listsBeingWatched_Bitfield & bit) ? 1 : 0;
		CryLogAlways ("[CODECOVERAGE] $9List %d/%d = '%s' (containing %d checkpoints)%s", displayNum + 1, mgr->m_nTotalCustomListsReadFromFile, listData->m_name, listData->m_numInList, mgr->m_listsBeingWatched_Bitfield ? (on ? " $3ON" : " $4OFF") : "");
		count += on;
	}

	if (mgr->m_listsBeingWatched_Bitfield)
	{
		CryLogAlways ("[CODECOVERAGE] Watching %d of %d lists", count, mgr->m_nTotalCustomListsReadFromFile);
	}
	else
	{
		CryLogAlways ("[CODECOVERAGE] Watching all checkpoints, whether or not featured in any of the %d lists", mgr->m_nTotalCustomListsReadFromFile);
	}
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::IgnoreAllCheckpointsWhichMatchPattern(bool onOff, const char * argument)
{
	SMatchThisPattern patt;

	if (GetMatchThisPatternData(argument, & patt))
	{
		CryLogAlways ("[CODECOVERAGE] Ignoring anything matching the first %d characters of '%s'", patt.numChars, patt.text);
		for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
		{
			SLabelInfoFromFile & cp = m_pExpectedCheckpoints_Array[i];

			if (MatchesPattern(cp.m_labelName, & patt))
			{
				CryLogAlways ("[CODECOVERAGE] %s is %s being %s", cp.m_labelName, (cp.m_ignoreMe == onOff) ? "already" : "now", onOff ? "ignored" : "checked");
				cp.m_ignoreMe = onOff;
			}
		}
	}
	else
	{
		CryLogAlways ("[CODECOVERAGE] Invalid pattern to match '%s'", argument);
	}
}

//---------------------------------------------------------------------------------------
// Turns char * patternTxt into a SMatchThisPattern structure
// Returns whether patternTxt is a valid pattern; if returns false, don't use structure!
bool CGameCodeCoverageManager::GetMatchThisPatternData(const char * patternTxt, SMatchThisPattern * pattOut)
{
	const char * pWildcard = strchr(patternTxt, '*');
	pattOut->text = patternTxt;
	
	if (pWildcard)
	{
		pattOut->numChars = pWildcard - patternTxt;
		pattOut->matchType = SMatchThisPattern::kMatchType_wildcard;

		// Pattern is only valid if the '*' is at the end of the string (i.e. next char is terminator)
		return (pWildcard[1] == '\0');
	}

	// No wildcard character means we need to match the whole pattern plus the null terminator...
	pattOut->numChars = strlen (patternTxt) + 1;
	pattOut->matchType = SMatchThisPattern::kMatchType_exact;
	return true;
}

//---------------------------------------------------------------------------------------
bool CGameCodeCoverageManager::MatchesPattern(const char * checkpointName, const SMatchThisPattern * patt)
{
	return 0 == strncmp(checkpointName, patt->text, patt->numChars);
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::CmdDumpCodeCoverage(IConsoleCmdArgs *pArgs)
{
	CGameCodeCoverageOutput_Log outputStuff;
	GetInstance()->DoOutput(outputStuff);
}

//---------------------------------------------------------------------------------------
bool CGameCodeCoverageManager::DoOutput(IGameCodeCoverageOutput & outputClass) const
{
	if (outputClass.GetOK())
	{
		if (IsContextValid())
		{
			for (CheckPointSet::const_iterator it = m_setUnexpectedCheckPoints.begin(); it != m_setUnexpectedCheckPoints.end(); ++it)
			{
				outputClass.PrintLine("Unexpectedly hit", "CheckpointHit type=\"unexpected\"", "$4", *it);
			}

			for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
			{
				if (m_pExpectedCheckpoints_Array[i].m_hasBeenHit)
				{
					outputClass.PrintLine("Hit when expected", "CheckpointHit", "$5", m_pExpectedCheckpoints_Array[i].m_labelName);
				}
			}

			for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
			{
				if (m_pExpectedCheckpoints_Array[i].m_validForCurrentGameState && ! m_pExpectedCheckpoints_Array[i].m_hasBeenHit)
				{
					outputClass.PrintLine("Still need to hit", NULL, "$6", m_pExpectedCheckpoints_Array[i].m_labelName);
				}
			}
		}
		else
		{
			outputClass.PrintComment("No context loaded! Listing all checkpoints which have been hit!");
			for (CheckPointVector::const_iterator it = m_vecCheckPoints.begin(); it != m_vecCheckPoints.end(); ++it)
			{
				outputClass.PrintLine("Checkpoint hit", "CheckpointHit", "$8", (*it)->GetLabel());
			}
		}

		return true;
	}

	return false;
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::CheckWhichCheckpointsAreValidForGameState()
{
	if (IsContextValid())
	{
		IGameRulesSystem * gameRulesSystem = gEnv->pGameFramework->GetIGameRulesSystem();
		IGameRules * rules = gameRulesSystem->GetCurrentGameRules();
		const char * rulesName = rules ? rules->GetEntity()->GetClass()->GetName() : NULL;

		TBitfield curState = kGCCFlag_none;

		if (rules)
		{
			curState |= (gEnv->bServer ? kGCCFlag_server : kGCCFlag_nonServer);
			curState |= (gEnv->IsClient() ? kGCCFlag_hasLocalPlayer : kGCCFlag_none);
			curState |= (gEnv->bMultiplayer ? kGCCFlag_multiplayer : kGCCFlag_singleplayer);
		}
		else
		{
			curState |= kGCCFlag_noLevelLoaded;
		}

		m_nTotalCheckpointsReadFromFileAndValid = 0;

		for (int i = 0; i < m_nTotalCheckpointsReadFromFile; ++ i)
		{
			SLabelInfoFromFile & cp = m_pExpectedCheckpoints_Array[i];
			bool useIt = false;

			if (GetShouldIgnoreCheckpoint(& cp) == false && (curState & cp.m_flags) == cp.m_flags)
			{
				// All required tags are there... now check game rules

				const char * reader = cp.m_gameModeNames;

				// Sensible default...
				reader = reader ? reader : "!FRONTEND";

				useIt = reader[0] == '\0';

				if (! useIt)
				{
					size_t sizeRead;
					bool gameRulesAnyMatch = false;
					bool invert = false;

					if (reader[0] == '!')
					{
						++ reader;
						invert = true;
					}

					do
					{
						char eachOne[16];
						bool match = false;

						sizeRead = cry_copyStringUntilFindChar(eachOne, reader, sizeof(eachOne), '|');

						if (0 == strcmp("FRONTEND", eachOne))
						{
							match = (rulesName == NULL);
						}
						else if (rulesName)
						{
							const char * expanded = gameRulesSystem->GetGameRulesName(eachOne);
							CRY_ASSERT_MESSAGE (expanded, string().Format("%s is not a game mode alias!", eachOne));
							match = (expanded && 0 == strcmp(rulesName, expanded));
						}

						reader += sizeRead;
						gameRulesAnyMatch = gameRulesAnyMatch || match;
					}
					while (sizeRead);

					useIt = (gameRulesAnyMatch != invert);
				}
			}

			cp.m_validForCurrentGameState = useIt;
			m_nTotalCheckpointsReadFromFileAndValid += (useIt) ? 1 : 0;
			//CryLog("[CODECOVERAGE] Fixing validity of '%s' which should be set for '%s' flags=0x%x... %s", cp.m_labelName, cp.m_gameModeNames, cp.m_flags, useIt ? "VALID" : "SKIP");
		}

		CryLog ("[CODECOVERAGE] %d checkpoints are valid for current game state (rules=%s, multiplayer=%d, server=%d, client=%d, flags=0x%x)", m_nTotalCheckpointsReadFromFileAndValid, rulesName, gEnv->bMultiplayer, gEnv->bServer, gEnv->IsClient(), curState);

		m_totalValidCheckpointsHit = 0;
		for (int n = 0; n < m_numAutoNamedCheckpointGroups; ++ n)
		{
			m_totalValidCheckpointsHit += m_pAutoNamedCheckpointGroups_Array[n].FixNumValidForCurrentGameState();
		}
	}
}

//---------------------------------------------------------------------------------------
const CNamedCheckpointGroup * CGameCodeCoverageManager::GetAutoNamedCheckpointGroup(int i) const
{
	assert (i >= 0 && i < m_numAutoNamedCheckpointGroups);
	return & m_pAutoNamedCheckpointGroups_Array[i];
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::Register( CGameCodeCoverageCheckPoint * pPoint )
{
	assert(pPoint);

	m_vecCheckPoints.push_back(pPoint);
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::Hit( CGameCodeCoverageCheckPoint * pPoint )
{
	// Will need to become a loop if we support multiple listeners one day. [TF]
	if (m_onlyListener)
	{
		m_onlyListener->InformCodeCoverageCheckpointHit(pPoint);
	}

	SLabelInfoFromFile * watchedCheckpointStruct = FindWatchedCheckpointStructByName(pPoint->GetLabel());
	
	if (watchedCheckpointStruct)
	{
		if (! watchedCheckpointStruct->m_validForCurrentGameState && ! GetShouldIgnoreCheckpoint(watchedCheckpointStruct))
		{
			m_setUnexpectedCheckPoints.insert(pPoint->GetLabel());
			AddToRecentlyHitList(pPoint->GetLabel(), false);
		}
	}
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::AddToRecentlyHitList(const char * which, bool expected)
{
	if (m_recentlyHit.m_count >= GAMECODECOVERAGE_STOREHITTHISFRAME)
	{
		assert (m_recentlyHit.m_count == GAMECODECOVERAGE_STOREHITTHISFRAME);
		RemoveFirstEntryFromRecentlyHitList();
	}

	assert (m_recentlyHit.m_count < GAMECODECOVERAGE_STOREHITTHISFRAME);

	if (m_bCodeCoverageLogAll)
	{
		CryLog ("[CODECOVERAGE] %s hit of checkpoint '%s'", expected ? "First" : "Unexpected", which);
	}

	m_recentlyHit.m_array[m_recentlyHit.m_count].pStr = which;
	m_recentlyHit.m_array[m_recentlyHit.m_count].fTime = 5.f;
	m_recentlyHit.m_array[m_recentlyHit.m_count].bExpected = expected;
	++ m_recentlyHit.m_count;
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::Reset()
{
	m_mCheckPoints.clear();

	CheckWhichCheckpointsAreValidForGameState();

	CacheCurrentTime();
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::RemoveFirstEntryFromRecentlyHitList()
{
	-- m_recentlyHit.m_count;

//CryLog ("[CODECOVERAGE] Removing '%s' from recent hit list (time left = %.2f, list now contains %d entries)", m_recentlyHit.m_array[0].pStr, m_recentlyHit.m_array[0].fTime, m_recentlyHit.m_count);
	memmove (m_recentlyHit.m_array, m_recentlyHit.m_array + 1, sizeof(m_recentlyHit.m_array[0]) * m_recentlyHit.m_count);
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::CmdUploadHitCheckpoints(IConsoleCmdArgs *pArgs)
{
	GetInstance()->UploadHitCheckpointsToServer();
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::UploadHitCheckpointsToServer()
{
	string filePath;
	bool ok = false;
	filePath.Format("%s/%s.xml", m_outputDirectory.c_str(), m_timeWhenStarted.c_str());

	// Print a list of the checkpoints we have/haven't hit
	{
		CGameCodeCoverageOutput_File outputStuff(filePath); // THIS DOES THE fclose IN THE DESTRUCTOR, MUST HAVE HAPPENED BEFORE SubmitFile below
		ok = DoOutput(outputStuff);
		CryLogAlways ("[CODECOVERAGE] %s to '%s'", ok ? "Wrote" : "Failed to write", filePath.c_str());
	}

#if ENABLE_UPLOAD_OF_GCC_OUTPUT_TO_TELEMETRY_SERVER
	if (ok)
	{
		// Upload file to telemetry-collector server
		ITelemetryCollector	* tc = g_pGame->GetITelemetryCollector();
		if (tc)
		{
			tc->SubmitFile(filePath.c_str(), filePath.c_str());
		}
	}
#endif
}

//---------------------------------------------------------------------------------------
void CNamedCheckpointGroup::AddCheckpoint(SLabelInfoFromFile * addThis)
{
	m_numInGroup ++;

	addThis->m_nextInAutoNamedGroup = m_firstInGroup;
	m_firstInGroup = addThis;
}

//---------------------------------------------------------------------------------------
const SLabelInfoFromFile * CNamedCheckpointGroup::GetFirstUnhitValidCheckpoint() const
{
	for (const SLabelInfoFromFile * eachOne = m_firstInGroup; eachOne; eachOne = eachOne->m_nextInAutoNamedGroup)
	{
		if (eachOne->m_validForCurrentGameState == true && eachOne->m_hasBeenHit == false)
		{
			return eachOne;
		}
	}

	CRY_ASSERT_TRACE(false, ("No unhit valid checkpoints found in group '%s'", m_name));
	return NULL;
}

//---------------------------------------------------------------------------------------
int CNamedCheckpointGroup::FixNumValidForCurrentGameState()
{
	m_numValid = 0;
	m_numHit = 0;

	for (SLabelInfoFromFile * eachOne = m_firstInGroup; eachOne; eachOne = eachOne->m_nextInAutoNamedGroup)
	{
		m_numValid += (eachOne->m_validForCurrentGameState) ? 1 : 0;
		m_numHit += (eachOne->m_hasBeenHit && eachOne->m_validForCurrentGameState) ? 1 : 0;
	}

	CryLog ("[CODECOVERAGE] Group '%s' %d/%d checkpoints are valid for current game state", m_name, m_numValid, m_numInGroup);

	return m_numHit;
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::EnableListener(IGameCodeCoverageListener * listener)
{
	assert (m_onlyListener == NULL);
	m_onlyListener = listener;
}

//---------------------------------------------------------------------------------------
void CGameCodeCoverageManager::DisableListener(IGameCodeCoverageListener * listener)
{
	assert (m_onlyListener == listener);
	m_onlyListener = NULL;
}

#endif
