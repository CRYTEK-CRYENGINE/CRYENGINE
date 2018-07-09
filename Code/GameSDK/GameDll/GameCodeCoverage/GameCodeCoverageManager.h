// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameCodeCoverageManager.h
//  Created:     18/06/2008 by Matthew
//  Description: High-level manager class for code coverage system
//               including file handing of checkpoint lisst
//               Tracks whether key points of code have been hit in testing
// -------------------------------------------------------------------------
//  History:     Tim Furnish, 11/11/2009:
//               Moved into game DLL from AI system
//               Wrapped contents in ENABLE_GAME_CODE_COVERAGE
//
//               Tim Furnish, 19/11/2009:
//               Reads context from XML file instead of raw text
//               Support for checkpoints which are only valid for certain game modes
//               Support for checkpoints which are only valid in certain states (server, hasLocalPlayer, multiplayer etc.)
//
////////////////////////////////////////////////////////////////////////////

/**
* Design notes:
*   For now, reading contexts is done in one shot. I'm tempted to work through the file slowly to avoid a spike.
*   A (separate) tracking GUI is crucial to this system - an efficient interface to service this is not trivial
*   Quite a basic principle to underlie this code is that we're only interested in checkpoints we haven't yet hit
*/

#ifndef __GAME_CODE_COVERAGE_MANAGER_H_
#define __GAME_CODE_COVERAGE_MANAGER_H_

#pragma once

#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Utility/SingleAllocTextBlock.h"
#include "AutoEnum.h"
#include "GameMechanismManager/GameMechanismBase.h"

#if ENABLE_GAME_CODE_COVERAGE

#define GameCodeCoverageFlagList(f)  \
	f(kGCCFlag_server)                 \
	f(kGCCFlag_nonServer)              \
	f(kGCCFlag_hasLocalPlayer)         \
	f(kGCCFlag_multiplayer)            \
	f(kGCCFlag_singleplayer)           \
	f(kGCCFlag_noLevelLoaded)          \

AUTOENUM_BUILDFLAGS_WITHZERO(GameCodeCoverageFlagList, kGCCFlag_none);

typedef uint16  TGameCodeCoverageCustomListBitfield;
typedef uint8   TGameCodeCoverageGroupNum;

#define GAMECODECOVERAGE_STOREHITTHISFRAME  5

struct SCustomCheckpointList
{
	const char * m_name;
	unsigned int m_numInList;
};

struct SLabelInfoFromFile
{
	const char * m_gameModeNames;
	const char * m_labelName;
	SLabelInfoFromFile * m_nextInAutoNamedGroup;

	TBitfield m_flags;
	TGameCodeCoverageCustomListBitfield m_customListBitfield;
	TGameCodeCoverageGroupNum m_groupNum;

	bool m_validForCurrentGameState, m_hasBeenHit, m_ignoreMe;
};

struct SMatchThisPattern;
class IGameCodeCoverageOutput;
class IGameCodeCoverageListener;

class CNamedCheckpointGroup
{
	const char * m_name;
	SLabelInfoFromFile * m_firstInGroup;
	int m_numInGroup, m_numValid, m_numHit;

	public:
	CNamedCheckpointGroup()
	{
		m_name = NULL;
		m_firstInGroup = NULL;
		m_numInGroup = 0;
		m_numValid = 0;
		m_numHit = 0;
	}

	ILINE void SetName(const char * namePtr)              { m_name = namePtr;                                 }
	ILINE const char * GetName() const                    { return m_name;                                    }
	ILINE int GetNumInGroup() const                       { return m_numInGroup;                              }
	ILINE int GetNumValidForCurrentGameState() const      { return m_numValid;                                }
	ILINE int GetNumHit() const                           { return m_numHit;                                  }
	ILINE void IncreaseNumHit()                           { m_numHit ++; assert (m_numHit <= m_numValid);     }
	const SLabelInfoFromFile * GetFirstUnhitValidCheckpoint() const;
	void AddCheckpoint(SLabelInfoFromFile * addThis);
	int FixNumValidForCurrentGameState();
};

/**
* The code coverage manager
*/
class CGameCodeCoverageManager : public CGameMechanismBase
{
	// String comparison for set and map
	struct cmp_str : public std::binary_function<const char *, const char *, bool>
	{
		bool operator()(char const * a, char const * b) const
		{
			return strcmp(a, b) < 0;
		}
	};

	typedef std::vector<CGameCodeCoverageCheckPoint *> CheckPointVector;
	typedef std::map < const char *, CGameCodeCoverageCheckPoint*, cmp_str > CheckPointMap;
	typedef std::set < const char *, cmp_str > CheckPointSet;

public:
	CGameCodeCoverageManager(const char * filename);
	~CGameCodeCoverageManager();

	struct SRecentlyHitList
	{
		struct SStrAndTime
		{
			const char	*pStr;
			float				fTime;
			bool				bExpected;
		};

		SStrAndTime m_array[GAMECODECOVERAGE_STOREHITTHISFRAME];
		int					m_count;
	};

	ILINE static CGameCodeCoverageManager * GetInstance()         { return s_instance;                                                  }
	ILINE bool IsContextValid() const                             { return m_bContextValid;                                             }
	ILINE int GetTotalCheckpointsReadFromFileAndValid() const     { return m_nTotalCheckpointsReadFromFileAndValid;                     }
	ILINE const CheckPointSet &GetUnexpectedCheckpoints() const	  { return m_setUnexpectedCheckPoints;                                  }
	ILINE const SRecentlyHitList & GetRecentlyHitList() const     {	return m_recentlyHit;                                               }
	ILINE int GetTotalValidCheckpointsHit() const                 { return m_totalValidCheckpointsHit;                                  }
	ILINE int GetNumAutoNamedGroups() const                       { return m_numAutoNamedCheckpointGroups;                              }
	ILINE int GetTotalCustomListsReadFromFile() const             { return m_nTotalCustomListsReadFromFile;                             }
	ILINE const char * GetCustomListName(int n) const             { return m_pCustomCheckpointLists_Array[n].m_name;                    }

	const CNamedCheckpointGroup * GetAutoNamedCheckpointGroup(int i) const;
	void GetRemainingCheckpointLabels( std::vector < const char * > &vLabels ) const;
	void Register( CGameCodeCoverageCheckPoint * pPoint );
	void Hit( CGameCodeCoverageCheckPoint * pPoint );
	void Reset();
	void UploadHitCheckpointsToServer();
	void EnableListener(IGameCodeCoverageListener * listener);
	void DisableListener(IGameCodeCoverageListener * listener);

protected:

	virtual void Update(float frameTime);
	virtual void Inform(EGameMechanismEvent gmEvent, const SGameMechanismEventData * data);

	void CheckWhichCheckpointsAreValidForGameState();
	SLabelInfoFromFile * FindWatchedCheckpointStructByName(const char *szCheckPoint) const;
	bool ReadCodeCoverageContext( IItemParamsNode *paramNode );
	void ParseCustomList(const char * name, const char * list, int numListsParsedSoFar);
	void AddCheckpointToCustomList(SCustomCheckpointList * checkpointList, int checkpointNum, TGameCodeCoverageCustomListBitfield bit);
	void RemoveFirstEntryFromRecentlyHitList();
	void AddToRecentlyHitList(const char * which, bool expected);
	void IgnoreAllCheckpointsWhichMatchPattern(bool onOff, const char * argument);
	bool DoOutput(IGameCodeCoverageOutput & outputClass) const;
	bool GetShouldIgnoreCheckpoint(const SLabelInfoFromFile * cp) const;
	void CacheCurrentTime();
	void ClearMem();

	static bool GetMatchThisPatternData(const char * patternTxt, SMatchThisPattern * pattOut);
	static bool MatchesPattern(const char * checkpointName, const SMatchThisPattern * patt);
	static void CmdDumpCodeCoverage(IConsoleCmdArgs *pArgs);
	static void CmdUploadHitCheckpoints(IConsoleCmdArgs *pArgs);
	static void CmdGCCIgnore(IConsoleCmdArgs *pArgs);
	static void CmdGCCUseList(IConsoleCmdArgs *pArgs);

	static CGameCodeCoverageManager * s_instance;

	CheckPointSet             m_setUnexpectedCheckPoints;
	CheckPointMap             m_mCheckPoints;
	CheckPointVector          m_vecCheckPoints;
	SRecentlyHitList          m_recentlyHit;
	CSingleAllocTextBlock     m_singleAllocTextBlock;

	// Pointers to memory controlled elsewhere - DON'T free these up when destroying the class
	const char *                  m_filepath;
	IGameCodeCoverageListener *   m_onlyListener;

	// Pointers to memory allocated within this class - DO free these up when destroying the class
	SLabelInfoFromFile *      m_pExpectedCheckpoints_Array;
	CNamedCheckpointGroup *   m_pAutoNamedCheckpointGroups_Array;
	SCustomCheckpointList *   m_pCustomCheckpointLists_Array;

	string                    m_outputDirectory;
	string                    m_timeWhenStarted;

	int 											m_bCodeCoverageEnabled;
	int 											m_bCodeCoverageDisplay;
	int 											m_bCodeCoverageLogAll;
	int                       m_nTotalCustomListsReadFromFile;
	int                       m_nTotalCheckpointsReadFromFile;
	int                       m_nTotalCheckpointsReadFromFileAndValid;
	int                       m_numAutoNamedCheckpointGroups;
	int                       m_totalValidCheckpointsHit;

	TGameCodeCoverageCustomListBitfield   m_listsBeingWatched_Bitfield;

	// TODO: Combine these into a 'state' enum... there's no way both will be true at the same time
	bool											m_bContextValid;
	bool											m_bCodeCoverageFailed;
};

#endif // ENABLE_GAME_CODE_COVERAGE

#endif // __GAME_CODE_COVERAGE_MANAGER_H_