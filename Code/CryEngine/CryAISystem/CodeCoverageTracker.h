// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   CodeCoverageTracker.h
//  Created:     18/06/2008 by Matthew
//  Description: Defines code coverage check points
//               and a central class to track their registration
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

/**
 * Design notes:
 *   See the CodeCoverageManager class that acts as the high-level API to the system
 *   A (separate) tracking GUI is crucial to this system - an efficient interface to service this is not trivial
 *
 * Technical notes:
 *   The manager would appear to lend itself to a map of names to checkpoint pointers for quick lookup
 *   However I really want to keep overhead to a minimum even in registering and I guess we might have 1000 CCCPoints
 *   A vector, with some sorting or heapifying might be better when the code matures
 */

#ifndef __CODE_COVERAGE_TRACKER_H_
#define __CODE_COVERAGE_TRACKER_H_

#pragma once

#include <CrySystem/ICodeCheckpointMgr.h>

#ifdef CODECHECKPOINT_ENABLED

// Hijack the old CCCPOINT definition (and add a semi-colon to allow compilation)
	#define CCCPOINT(x) CODECHECKPOINT(x);

#else   // Use old CCCPOINT system

// Code coverage works on console too, but for performance sake lets keep it to PC for now
	#if CRY_PLATFORM_WINDOWS && !defined(_RELEASE)
		#define CCCPOINT_ENABLE
	#endif

/**
 * The key macro
 * If this is ever used by other systems, just break out the "AI" part into new macros
 * Clever people will think of using method names automatically rather than having to type them
 * Please don't do this - if code is moved, renamed, copy/pasted, it's more useful to preserve the label
 */
	#ifdef CCCPOINT_ENABLE
		#define CCCPOINT(x) static CCodeCoverageCheckPoint autoReg ## x("AI_" # x); autoReg ## x.Touch();
	#else
		#define CCCPOINT(x) ;
	#endif

#endif

#if !defined(_RELEASE)

// Forward declarations

/**
 * A check point
 * As above, static creation occurs once and then Touching happens often
 * I suspect it will be better to keep an explicit flag for registration, to allow clean reset at runtime
 */
class CCodeCoverageCheckPoint
{
	friend class CCodeCoverageTracker;

public:

	CCodeCoverageCheckPoint(const char* label);

	inline void Touch()          { m_nCount++; }

	void        Reset()          { m_nCount = 0; }

	int         GetCount() const { return m_nCount; }

	const char* GetLabel() const { return m_psLabel; }

protected:

	int         m_nCount;
	const char* m_psLabel;
};

/**
 * The check point manager
 * Collects pointers to all checkpoints that have registered
 * If a checkpoint is never hit, it will never be registered
 */
class CCodeCoverageTracker
{
	typedef std::vector<CCodeCoverageCheckPoint*> CheckPointVector;

public:
	CCodeCoverageTracker(void);

	void Register(CCodeCoverageCheckPoint* pPoint);

	// Zero all counters
	void Clear();

	// Zero all counters and discard all pointers
	// This might well never be needed
	void Reset();

	// Get number of checkpoints registered so far
	// Important for monitoring coverage progress
	int GetTotalRegistered();

	// Retrieves the last 3 encountered checkpoints in the current frame.
	// It also resets the buffer for storing the last 3 checkpoints.
	// This is used by the CCodeCoverageGUI class to display the last few checkpoints encountered.
	void GetMostRecentAndReset(const char* pRet[3]);

	// Get pointer to checkpoint of this label
	CCodeCoverageCheckPoint* GetCheckPoint(const char* sLabel) const;

	const CheckPointVector&  GetRecentCheckpoints() const
	{
		return m_vecCheckPoints;
	}

	void ResetRecentCheckpoints()
	{
		m_vecCheckPoints.clear();
	}

	void AddCheckpoint(CCodeCoverageCheckPoint* pPoint)
	{
		m_mCheckPoints.insert(std::make_pair(pPoint->GetLabel(), pPoint));
	}

protected:
	// String comparison for map
	struct cmp_str : public std::binary_function<const char*, const char*, bool>
	{
		bool operator()(char const* a, char const* b) const
		{
			return strcmp(a, b) < 0;
		}
	};

	// Map that keeps all the points that have been hit
	typedef std::map<const char*, CCodeCoverageCheckPoint*, cmp_str> CheckPointMap;
	CheckPointMap m_mCheckPoints;

	// Temporary list to keep track of points
	// before the CodeCoverageManager has been initialized
	// After the CodeCoverageManager has been initialized this becomes a list checkpoints per frame
	CheckPointVector m_vecCheckPoints;

	const char*      m_pMostRecent[3];
	int              m_nLastEntry; // Last entry in m_pMostRecent
};

#endif //_RELEASE

#endif // __CODE_COVERAGE_TRACKER_H_
