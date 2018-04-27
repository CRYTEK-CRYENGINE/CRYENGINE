// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     06/30/2010 by Morgan K
//  Description: A debug class to track and output code checkpoint state
// -------------------------------------------------------------------------
//  History: Created by Morgan Kita
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CODE_CHECKPOINT_DEBUG_MGR_H_
#define __CODE_CHECKPOINT_DEBUG_MGR_H_

#pragma once

#include <CrySystem/ICodeCheckpointMgr.h>

#include "GameMechanismManager/GameMechanismBase.h"

// Not for release
#ifndef _RELEASE

	#define CODECHECKPOINT_DEBUG_ENABLED

#endif

#if defined CODECHECKPOINT_DEBUG_ENABLED
static void CmdCodeCheckPointSearch(IConsoleCmdArgs* pArgs);
#endif

class CCodeCheckpointDebugMgr : public CGameMechanismBase
{
public:

	typedef std::vector<std::pair<string, int>> RecordNameCountPairs;

	struct CheckpointDebugRecord
	{
		CheckpointDebugRecord(const CCodeCheckpoint* checkPoint, const char* name, int checkpointIdx)
			: m_pCheckpoint(checkPoint), m_name(name), m_queried(false), m_lastHitTime(0.0f),
			m_currHitcount(0), m_prevHitcount(0), m_refCount(0), m_checkPointIdx(checkpointIdx)
		{
			if (m_pCheckpoint)
			{
				m_lastHitTime = gEnv->pTimer->GetCurrTime();
				m_currHitcount = checkPoint->HitCount();
			}
			else
			{
				m_queried = true;
			}
		}

		/// Function to update the reference count, defaults to an increment of 1.
		void UpdateWatched(int count = 1) { m_refCount += count; m_queried = (m_refCount != 0); }

		const CCodeCheckpoint* m_pCheckpoint; /// The checkpoint if registered otherwise NULL
		string                 m_name;        /// The name of the checkpoint (owned if m_pCheckpoint is NULL)

		float                  m_lastHitTime;  /// Last clock time this checkpoint was hit as determined by the snapshots
		uint32                 m_prevHitcount; /// Hit count for this node as of the last snapshot
		uint32                 m_currHitcount; /// Hit count for this node as of this snapshot

		uint32                 m_checkPointIdx; /// Index into checkpoint manager

		bool                   m_queried;  /// Flag to indicate this checkpoint was specifically requested for at load time
		int                    m_refCount; /// Count of references that requested this point for observation
	};

	///Predicate functions on records
	static bool RecordHasHits(CheckpointDebugRecord& rec) { return rec.m_currHitcount > 0; }

	///Predicate functors on records

	//Checks for a named record and updates its watched information if found
	class RecordMatchAndUpdate
	{
	public:
		RecordMatchAndUpdate(const string& name, int count = 1)
			: m_name(name), m_count(count)
		{}

		bool operator()(CheckpointDebugRecord& record) const
		{
			if (record.m_name == m_name)
			{
				record.UpdateWatched(m_count);
				return true;
			}
			return false;
		}

	private:
		string m_name;
		int    m_count;
	};

	class SearchRecordForString
	{

	public:

		SearchRecordForString(RecordNameCountPairs& namePairs, const string& substr = "") : m_namePairs(namePairs), m_searchStr(substr){}
		void operator()(const CheckpointDebugRecord& rec)
		{
			// Is there a registered checkpoint for this record?
			if (rec.m_pCheckpoint)
			{
				//If the search string is empty, or the checkpoint contains the substring anywhere inside of it
				if (m_searchStr == "" || rec.m_name.find(m_searchStr) != string::npos)
					m_namePairs.push_back(std::make_pair(rec.m_name, rec.m_pCheckpoint->HitCount()));
			}
		}

	private:
		RecordNameCountPairs& m_namePairs;
		string                m_searchStr;
	};

	CCodeCheckpointDebugMgr();
	virtual ~CCodeCheckpointDebugMgr();

	//Static function to retrieve singleton code checkpoint debug manager
	static CCodeCheckpointDebugMgr* RetrieveCodeCheckpointDebugMgr();

	///Update function will update the internal snapshot and do any debug drawing required
	virtual void Update(float dt);

	///Register name to be marked as a watch point
	void RegisterWatchPoint(const string& name);

	///Unregister name to be marked as a watch point
	void UnregisterWatchPoint(const string& name);

	void SearchCheckpoints(RecordNameCountPairs& outputList, string& searchStr) const;

	void ReadFile(const char* fileName);

	int  GetLine(char* pBuff, FILE* fp);

	static int   ft_debug_ccoverage;
	static float ft_debug_ccoverage_rate;
	static int   ft_debug_ccoverage_maxlines;
	static int   ft_debug_ccoverage_filter_maxcount;
	static int   ft_debug_ccoverage_filter_mincount;

private:

	static CCodeCheckpointDebugMgr* s_pCodeCheckPointDebugManager;

	///CVar variables
	int   m_debug_ccoverage;
	float m_debug_ccoverage_rate;
	int   m_debug_ccoverage_maxlines;
	int   m_debug_ccoverage_filter_maxcount;
	int   m_debug_ccoverage_filter_mincount;

	void UpdateRecord(const CCodeCheckpoint* pCheckpoint, CheckpointDebugRecord& record);
	void UpdateRecords();

	void DrawDebugInfo();

	typedef std::list<CheckpointDebugRecord>   TCheckpointDebugList;
	typedef std::vector<CheckpointDebugRecord> TCheckpointDebugVector;

	typedef std::pair<string, int>             TBookmarkPair;
	typedef std::map<string, int>              TBookmarkMap;
	TBookmarkMap m_bookmarkedNames;     /// List of points to be marked as watched with their current ref counts.

	///Keep watched points and unwatched points separate. This way when watched points must be queried/sorted, it can be done more quickly.
	TCheckpointDebugList m_watchedPoints;   /// Current snapshot of checkpoints that have been requested for monitoring
	TCheckpointDebugList m_unwatchedPoints; /// Current snapshot of checkpoints that are not watched with all required debug information

	float                m_timeSinceLastRun;
};

#endif
