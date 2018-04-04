// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   CodeCoverageManager.h
//  Created:     18/06/2008 by Matthew
//  Description: High-level manager class for code coverage system
//               including file handing of checkpoint lisst
//               Tracks whether key points of code have been hit in testing
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

/**
 * Design notes:
 *   For now, reading contexts is done in one shot. I'm tempted to work through the file slowly to avoid a spike.
 *   A (separate) tracking GUI is crucial to this system - an efficient interface to service this is not trivial
 *   Quite a basic principle to underlie this code is that we're only interested in checkpoints we haven't yet hit
 */

#ifndef __CODE_COVERAGE_MANAGER_H_
#define __CODE_COVERAGE_MANAGER_H_

#pragma once

#if !defined(_RELEASE)

// Get rid of this include later - we don't want people including the Manager for this
	#include "CodeCoverageTracker.h"

// Forward declarations

/*
   struct SCodeCoverageStats
   {
   int nTotalRegistered;
   int nTotalInContext;
   };
 */

/**
 * The code coverage manager
 */
class CCodeCoverageManager
{
	typedef std::vector<CCodeCoverageCheckPoint*> CheckPointVector;
	// String comparison for set
	struct cmp_str : public std::binary_function<const char*, const char*, bool>
	{
		bool operator()(char const* a, char const* b) const
		{
			return strcmp(a, b) < 0;
		}
	};

	typedef std::set<const char*, cmp_str> CheckPointSet;

public:
	CCodeCoverageManager();

	~CCodeCoverageManager()
	{
		SAFE_DELETE(m_pLabelBlock);
	}

	// Read a list of checkpoints to look for in this context
	// File format specified in cpp
	// User own the file handle and must close it
	bool ReadCodeCoverageContext(FILE*);

	// Write a list of all the checkpoints currently registered
	// File format specified in cpp
	// User own the file handle and must close it
	bool WriteCodeCoverageContext(FILE*)
	{

		return false;
	}

	// Check whether a valid context is loaded
	// Without a valid context the Manager gives no useful data
	bool IsContextValid() const
	{
		return m_bContextValid;
	}

	// Returns whether the checkpoint is in the file of expected checkpoints
	bool IsExpected(const char* szCheckPoint) const
	{
		return (m_setCheckPoints.find(szCheckPoint) != m_setCheckPoints.end());
	}

	//
	void Clear()
	{
		GetTracker()->Clear();
	}

	// Get number of checkpoints registered so far
	// Important for monitoring coverage progress
	int GetTotalRegistered() const
	{
		return GetTracker()->GetTotalRegistered();
	}

	// Get the total number of checkpoints (count of all in the file)
	int GetTotal() const
	{
		return m_nTotalCheckpoints;
	}

	int GetCodeCoverageStats()
	{
		// Get code coverage statistics
		return 0;
	}

	void RemoveCheckPoint(const char* pName)
	{
		assert(pName);

		m_setCheckPoints.erase(pName);
	}

	// Called every frame
	void                 Update(void);

	const CheckPointSet& GetUnexpectedCheckpoints() const
	{
		return m_setUnexpectedCheckPoints;
	}

	// Fill vector with labels of checkpoints that are still to be hit
	// Labels are given in alphabetical order
	// Any existing contents is wiped
	void GetRemainingCheckpointLabels(std::vector<const char*>& vLabels) const;

	void DumpCheckpoints(bool bLogToFile) const;

protected:
	// Wraps fgets to remove newlines and use correct buffer/string lengths
	// Returns number of characters successfully written, including the null-terminator
	// On error, returns 0
	int                   GetLine(char* pBuff, FILE* fp);

	CCodeCoverageTracker* GetTracker() const { return gAIEnv.pCodeCoverageTracker; }

	bool m_bContextValid;

	// Pointer to a packed block of checkpoint label strings
	// Note that, in fact, all those strings should be somewhere in the binary already
	char* m_pLabelBlock;

	// Set of labels of checkpoints we are still to hit
	CheckPointSet m_setCheckPoints;
	CheckPointSet m_setUnexpectedCheckPoints;

	// The total number of checkpoints (read from the file)
	int m_nTotalCheckpoints;
};

#endif //_RELEASE

#endif // __CODE_COVERAGE_MANAGER_H_
