// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     06/30/2010 by Morgan K
//  Description: A debug class to track and output code checkpoint state
// -------------------------------------------------------------------------
//  History: Created by Morgan Kita
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "GameCVars.h"
#include "CodeCheckpointDebugMgr.h"
#include <CryRenderer/IRenderAuxGeom.h>

const static int LABEL_LENGTH = 100;
const static int BUFF_SIZE = LABEL_LENGTH + 3;

CCodeCheckpointDebugMgr* s_pCodeCheckPointDebugManager = NULL;

bool SortDebugRecord(const CCodeCheckpointDebugMgr::CheckpointDebugRecord& rec1, const CCodeCheckpointDebugMgr::CheckpointDebugRecord& rec2)
{
	return rec1.m_lastHitTime > rec2.m_lastHitTime;
}

void CmdCodeCheckPointSearch(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 2)
		CryLogAlways("Usage: %s <substring>", pArgs->GetArg(0));

	CCodeCheckpointDebugMgr::RecordNameCountPairs foundPointNames;

	string searchString;

	if (pArgs->GetArgCount() == 2)
		searchString = pArgs->GetArg(1);

	CCodeCheckpointDebugMgr::RetrieveCodeCheckpointDebugMgr()->SearchCheckpoints(foundPointNames, searchString);

	CryLogAlways("Search with substring:%s return the following %" PRISIZE_T " hit checkpoints", searchString.c_str(), foundPointNames.size());
	for (CCodeCheckpointDebugMgr::RecordNameCountPairs::iterator fIt = foundPointNames.begin(); fIt != foundPointNames.end(); ++fIt)
		CryLogAlways("Checkpoint:%s Count:%i", fIt->first.c_str(), fIt->second);
}

CCodeCheckpointDebugMgr* CCodeCheckpointDebugMgr::s_pCodeCheckPointDebugManager = NULL;

CCodeCheckpointDebugMgr* CCodeCheckpointDebugMgr::RetrieveCodeCheckpointDebugMgr()
{
	if (!s_pCodeCheckPointDebugManager)
		s_pCodeCheckPointDebugManager = new CCodeCheckpointDebugMgr();

	return s_pCodeCheckPointDebugManager;
}

CCodeCheckpointDebugMgr::CCodeCheckpointDebugMgr()
	: REGISTER_GAME_MECHANISM(CCodeCheckpointDebugMgr), m_timeSinceLastRun(0)
{
	m_debug_ccoverage = 0;
	m_debug_ccoverage_rate = 0.05f;
	m_debug_ccoverage_maxlines = 10;
	m_debug_ccoverage_filter_maxcount = 0;
	m_debug_ccoverage_filter_mincount = 0;

	REGISTER_COMMAND("ft_debug_checkpoint_search", CmdCodeCheckPointSearch, VF_CHEAT, "FEATURE TESTER: Search for code checkpoints that have been encountered by substring");

	REGISTER_CVAR2("ft_debug_ccoverage", &m_debug_ccoverage, m_debug_ccoverage, VF_CHEAT, "FEATURE TESTER: Turn on debug drawing of code checkpoints. 1 = watched checkpoints only. 2 = unwatched only. 3 = both with watched higher priority. 4 = both equal prioirity");
	REGISTER_CVAR2("ft_debug_ccoverage_rate", &m_debug_ccoverage_rate, m_debug_ccoverage_rate, VF_CHEAT, "FEATURE TESTER: Max number of code checkpoints to output");
	REGISTER_CVAR2("ft_debug_ccoverage_maxlines", &m_debug_ccoverage_maxlines, m_debug_ccoverage_maxlines, VF_CHEAT, "FEATURE TESTER: Max number of code checkpoints to output");
	REGISTER_CVAR2("ft_debug_ccoverage_filter_maxcount", &m_debug_ccoverage_filter_maxcount, m_debug_ccoverage_filter_maxcount, VF_CHEAT, "FEATURE TEST: Only print out checkpoints with less than this number of hits");
	REGISTER_CVAR2("ft_debug_ccoverage_filter_mincount", &m_debug_ccoverage_filter_mincount, m_debug_ccoverage_filter_mincount, VF_CHEAT, "FEATURE TEST: Only print out checkpoints with more than this number of hit");

	string filePath = PathUtil::Make("../USER", "CodeCheckpointList.txt");
	ReadFile(filePath.c_str());
}

CCodeCheckpointDebugMgr::~CCodeCheckpointDebugMgr()
{
	s_pCodeCheckPointDebugManager = nullptr;

	gEnv->pConsole->RemoveCommand("ft_debug_checkpoint_search");
	gEnv->pConsole->UnregisterVariable("ft_debug_ccoverage", true);
	gEnv->pConsole->UnregisterVariable("ft_debug_ccoverage_rate", true);
	gEnv->pConsole->UnregisterVariable("ft_debug_ccoverage_maxlines", true);
	gEnv->pConsole->UnregisterVariable("ft_debug_ccoverage_filter_maxcount", true);
	gEnv->pConsole->UnregisterVariable("ft_debug_ccoverage_filter_mincount", true);
}

/// Searches all registered check points for the given substring and returns their counts in the input list
void CCodeCheckpointDebugMgr::SearchCheckpoints(RecordNameCountPairs& outputList, string& searchStr) const
{
	SearchRecordForString searchFunc(outputList, searchStr);

	std::for_each(m_watchedPoints.begin(), m_watchedPoints.end(), searchFunc);
	std::for_each(m_unwatchedPoints.begin(), m_unwatchedPoints.end(), searchFunc);
}

void CCodeCheckpointDebugMgr::ReadFile(const char* fileName)
{
	FILE* cpFile = gEnv->pCryPak->FOpen(fileName, "r");

	if (cpFile)
	{
		// Temporary buffer
		//char cBuffer[BUFF_SIZE];

		char* lineBlock = new char[BUFF_SIZE + 1];
		//char* appendBlock = lineBlock;

		while (int numRead = GetLine(lineBlock, cpFile))
		{
			ICodeCheckpointMgr* pCheckpointManager = gEnv->pCodeCheckpointMgr;
			if (pCheckpointManager)
				pCheckpointManager->GetCheckpointIndex(lineBlock);
			RegisterWatchPoint(lineBlock);
		}

		SAFE_DELETE_ARRAY(lineBlock);

		gEnv->pCryPak->FClose(cpFile);

	}
}

///Getline wrapper to break on newlines as adapted from the old AI Code Coverage Manager
int CCodeCheckpointDebugMgr::GetLine(char* pBuff, FILE* fp)
{
	CRY_ASSERT(pBuff && fp);

	/// Wraps fgets to remove newlines and use correct buffer/string lengths

	/// Must use CryPak FGets on CryPak files
	/// Will retrieve up to the next newline character in the file buffer.
	if (!gEnv->pCryPak->FGets(pBuff, BUFF_SIZE, fp))
		return 0;

	/// Convert newlines to \0 and discover length
	int i = 0;
	int nLimit = LABEL_LENGTH + 1;
	for (; i < nLimit; i++)
	{
		char c = pBuff[i];
		if (c == '\n' || c == '\r' || c == '\0')
			break;
	}

	/// If i == LABEL_LENGTH, the string was of maximum length
	/// If i == LABEL_LENGTH + 1, the string was too long
	if (i == nLimit)
	{
		// Malformed - fail
		pBuff[--i] = '\0';
		return 0;
	}

	/// Overwrite the last character (usually line terminator) with string delimiter
	pBuff[i] = '\0';

	if (i == 0)
		return 0;

	return i + 1;
}

void CCodeCheckpointDebugMgr::Update(float dt)
{
	//Don't do any work if we haven't requested this as active
	if (!m_debug_ccoverage)
		return;

	m_timeSinceLastRun += dt;
	if (m_timeSinceLastRun > m_debug_ccoverage_rate)
	{
		UpdateRecords();
		m_timeSinceLastRun = 0;
	}

	DrawDebugInfo();

}

void CCodeCheckpointDebugMgr::DrawDebugInfo()
{
	int displayLevel = m_debug_ccoverage;

	IRenderer* pRenderer = gEnv->pRenderer;

	int totalHit = (int) std::count_if(m_unwatchedPoints.begin(), m_unwatchedPoints.end(), RecordHasHits);
	int watchedHit = (int) std::count_if(m_watchedPoints.begin(), m_watchedPoints.end(), RecordHasHits);

	totalHit += watchedHit;

	TCheckpointDebugVector outputPoints;
	outputPoints.reserve(m_watchedPoints.size() + m_unwatchedPoints.size());

	//Get the sorted output points. For now assume you want to sort based on time

	//Only show watched
	if (displayLevel == 1)
	{
		outputPoints.assign(m_watchedPoints.begin(), m_watchedPoints.end());
		std::sort(outputPoints.begin(), outputPoints.end(), SortDebugRecord);
	}
	//Only show unwatched
	else if (displayLevel == 2)
	{
		outputPoints.assign(m_unwatchedPoints.begin(), m_unwatchedPoints.end());
		std::sort(outputPoints.begin(), outputPoints.end(), SortDebugRecord);

	}
	//Show both with watched having priority
	else if (displayLevel == 3)
	{
		///Retrieve and sort watched
		TCheckpointDebugVector watchedPts;
		watchedPts.assign(m_watchedPoints.begin(), m_watchedPoints.end());
		std::sort(watchedPts.begin(), watchedPts.end(), SortDebugRecord);

		///Retrieve and sort unwatched
		TCheckpointDebugVector unwatchedPts;
		unwatchedPts.assign(m_unwatchedPoints.begin(), m_unwatchedPoints.end());
		std::sort(unwatchedPts.begin(), unwatchedPts.end(), SortDebugRecord);

		///Combined with watched first
		outputPoints.assign(watchedPts.begin(), watchedPts.end());
		outputPoints.insert(outputPoints.end(), unwatchedPts.begin(), unwatchedPts.end());
	}
	//Show both with equal priority
	else if (displayLevel == 4)
	{
		///Combine sort
		outputPoints.assign(m_unwatchedPoints.begin(), m_unwatchedPoints.end());
		outputPoints.insert(outputPoints.end(), m_watchedPoints.begin(), m_watchedPoints.end());
		std::sort(outputPoints.begin(), outputPoints.end(), SortDebugRecord);
	}

	float percHit = 0.0f;
	if (!m_watchedPoints.empty())
		percHit = (float) watchedHit / m_watchedPoints.size();

	static float statusColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float height = (float) 0.05 * pRenderer->GetOverlayHeight();
	IRenderAuxText::Draw2dLabel(30.f, height, 2.f, statusColor, false,
		"THit: %i | TWatched: %" PRISIZE_T " | WatchedHit: %i | %% WatchedHit: %.2f", 
		totalHit, m_watchedPoints.size(), watchedHit, percHit * 100.0f );

	//Output the interesting lines
	float outputOffset = 0.08f;
	int numberOutput = 0;
	int maxNumberOutput = m_debug_ccoverage_maxlines;
	for (TCheckpointDebugVector::iterator outputIt = outputPoints.begin();
	     outputIt != outputPoints.end() && numberOutput < maxNumberOutput;
	     ++outputIt)
	{
		static float watchedColor[] = { 1.0f, 0.0f, 1.0f, 1.0f };
		static float unwatchedColor[] = { 0.0f, 0.0f, 1.0f, 1.0f };

		//Check filters and skip outputting of ones that don't qualify
		int filterMin = m_debug_ccoverage_filter_mincount, filterMax = m_debug_ccoverage_filter_maxcount;
		if (filterMax && (int)outputIt->m_currHitcount > filterMax)
			continue;
		else if (filterMin && (int)outputIt->m_currHitcount < filterMin)
			continue;

		IRenderAuxText::Draw2dLabel(30.f, outputOffset * pRenderer->GetOverlayHeight(), 2.f, outputIt->m_queried? watchedColor : unwatchedColor, false,
			"CheckPoint: %s Count:%i", outputIt->m_name.c_str(), outputIt->m_currHitcount);

		//Update the display output height
		outputOffset += 0.03f;
		++numberOutput;
	}

}

///Register name to be marked as a watch point
void CCodeCheckpointDebugMgr::RegisterWatchPoint(const string& name)
{
	//Check if its already watched, if so just increment
	TCheckpointDebugList::iterator currentLocation = std::find_if(m_watchedPoints.begin(), m_watchedPoints.end(), RecordMatchAndUpdate(name));

	bool recordFound = currentLocation != m_watchedPoints.end();

	//Check unwatched
	if (currentLocation == m_watchedPoints.end())
	{
		currentLocation = std::find_if(m_unwatchedPoints.begin(), m_unwatchedPoints.end(), RecordMatchAndUpdate(name));

		if (currentLocation != m_unwatchedPoints.end())
		{
			recordFound = true;

			//Move this record to the appropriate watched location
			TCheckpointDebugList::iterator currwIt = m_watchedPoints.begin();
			for (; currwIt != m_watchedPoints.end(); ++currwIt)
			{
				if (currentLocation->m_checkPointIdx < currwIt->m_checkPointIdx)
				{
					currentLocation->UpdateWatched();
					m_watchedPoints.insert(currwIt, *currentLocation);
					break;
				}
			}

			if (currwIt == m_watchedPoints.end())
			{
				currentLocation->UpdateWatched();
				m_watchedPoints.push_back(*currentLocation);
			}

			m_unwatchedPoints.erase(currentLocation);
		}
	}

	//Point hasn't been reached yet so bookmark it
	if (!recordFound)
	{
		//Check if bookmark exists, if so update the count of requests for it
		TBookmarkMap::iterator currBookmark = m_bookmarkedNames.find(name);
		if (currBookmark != m_bookmarkedNames.end())
			++currBookmark->second;
		else
			m_bookmarkedNames.insert(std::make_pair(name, 1));

	}
}

///Unregister name to be marked as a watch point
void CCodeCheckpointDebugMgr::UnregisterWatchPoint(const string& name)
{
	//Check for an existing record in the watched points and if found decrement its refcount of watchers
	TCheckpointDebugList::iterator currentLocation = std::find_if(m_watchedPoints.begin(), m_watchedPoints.end(), RecordMatchAndUpdate(name, -1));

	if (currentLocation != m_watchedPoints.end())
	{
		//If there is no longer a reference to this checkpoint then move it back to the unwatched at the appropriate spot.
		if (currentLocation->m_refCount <= 0)
		{
			TCheckpointDebugList::iterator currwIt = m_unwatchedPoints.begin();
			for (; currwIt != m_unwatchedPoints.end(); ++currwIt)
			{
				if (currentLocation->m_checkPointIdx < currwIt->m_checkPointIdx)
				{
					m_unwatchedPoints.insert(currwIt, *currentLocation);
					break;
				}
			}

			if (currwIt == m_unwatchedPoints.end())
				m_unwatchedPoints.push_back(*currentLocation);

			m_watchedPoints.erase(currentLocation);
		}
	}

	///If we haven't yet reached the checkpoint somehow then check the bookmarks to see if one was waiting
	if (currentLocation == m_watchedPoints.end())
	{
		///Check if bookmark exists, if so update the count of requests for it
		TBookmarkMap::iterator currBookmark = m_bookmarkedNames.find(name);
		if (currBookmark != m_bookmarkedNames.end())
		{
			--currBookmark->second;
			if (currBookmark->second <= 0)
				m_bookmarkedNames.erase(currBookmark);
		}
		else
			///Shouldn't be able to request to unregister a watch point that is not currently registered
			CRY_ASSERT(false);

	}
}

void CCodeCheckpointDebugMgr::UpdateRecord(const CCodeCheckpoint* pCheckpoint, CheckpointDebugRecord& record)
{
	if (!pCheckpoint)
		return;

	//Update the existing record
	if (record.m_prevHitcount < pCheckpoint->HitCount())
	{
		record.m_prevHitcount = record.m_currHitcount;
		record.m_currHitcount = pCheckpoint->HitCount();
		record.m_lastHitTime = gEnv->pTimer->GetCurrTime();
	}
}

void CCodeCheckpointDebugMgr::UpdateRecords()
{
	//Retrieve the latest snapshot
	ICodeCheckpointMgr* pCodeCheckpointMgr = gEnv->pCodeCheckpointMgr;
	if (pCodeCheckpointMgr)
	{
		size_t nextSnapshotCount = pCodeCheckpointMgr->GetTotalCount();

		size_t currIdx = 0;

		TCheckpointDebugList::iterator unwatchedIt = m_unwatchedPoints.begin(), watchedIt = m_watchedPoints.begin();

		///Update existing points with their snapshot
		size_t currSnapShotcount = m_watchedPoints.size() + m_unwatchedPoints.size();
		while (currIdx < currSnapShotcount && currIdx < nextSnapshotCount)
		{
			string name = pCodeCheckpointMgr->GetCheckPointName(currIdx);
			const CCodeCheckpoint* pCheckpoint = pCodeCheckpointMgr->GetCheckpoint(currIdx);

			///Check if we are a watched point
			if (watchedIt != m_watchedPoints.end() && (name == watchedIt->m_name))
			{
				CheckpointDebugRecord& currentRec = *watchedIt;
				UpdateRecord(pCheckpoint, currentRec);
				++watchedIt;
			}
			//Otherwise checked if we are unwatched point
			else if (unwatchedIt != m_unwatchedPoints.end() && (name == unwatchedIt->m_name))
			{
				CheckpointDebugRecord& currentRec = *unwatchedIt;
				UpdateRecord(pCheckpoint, currentRec);
				++unwatchedIt;
			}
			///This should never happen, as each existing point should already have been inserted into either the watched or unwatched vector.
			else
			{
				CRY_ASSERT(false);
			}

			++currIdx;
		}

		///Add entities that are new to this snapshot
		while (currIdx < nextSnapshotCount)
		{
			const char* name = pCodeCheckpointMgr->GetCheckPointName(currIdx);
			const CCodeCheckpoint* pCheckpoint = pCodeCheckpointMgr->GetCheckpoint(currIdx);

			CheckpointDebugRecord newRecord(pCheckpoint, name, currIdx);

			//If we have a current request to watch this point, make sure it goes into watched points
			TBookmarkMap::iterator bookmarkedIt = m_bookmarkedNames.find(name);
			if (bookmarkedIt != m_bookmarkedNames.end())
			{
				newRecord.UpdateWatched(bookmarkedIt->second);
				m_watchedPoints.push_back(newRecord);
				//Clean up the bookmark as its already been hit
				m_bookmarkedNames.erase(bookmarkedIt);
			}
			else
				m_unwatchedPoints.push_back(newRecord);

			++currIdx;
		}
	}
}
