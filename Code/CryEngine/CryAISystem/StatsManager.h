// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   StatsManager.h
//  Created:     15/09/2008 by Matthew
//  Description: Stores global AI statistics and provides display/output methods
//  Notes:       Should become a flexible bag of numbers with a strong visualisation
//               Should also support any stats stored locally in the code
// -------------------------------------------------------------------------
//  History:
//
//  Wishlist:    Could categorise as debug or performance
//               Smoothing over frames is very handy - with a visual indication of update?
//               Pause...
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AISTATSMANAGER
#define __AISTATSMANAGER

#pragma once

// Currently available stats
enum EStatistics
{
	eStat_ActiveActors,                                     // Enabled AI Actors
	eStat_FullUpdates,                                      // Full updates performed in a frame
	eStat_SyncTPSQueries,                                   // TPS queries called synchronously this frame
	eStat_AsyncTPSQueries,                                  // TPS queries called _Asynchronously_ this frame
	eStat_Last                                              // Sentinel value
};

// Ideas: //	(TPS) eStat_GenerationTime, eStat_TotalTime,

enum EStatsReset
{
	eStatReset_Frame,                                        // Reset just values accumulated over a frame
	eStatReset_All,                                          // Reset all values
};

class CStatsManager
{
public:
	CStatsManager(void);
	~CStatsManager(void);

	void  Reset(EStatsReset eReset);

	float GetStat(EStatistics eStat)
	{
		assert(eStat < eStat_Last);
		return m_fValues[eStat];
	}

	void SetStat(EStatistics eStat, float fValue)
	{
		assert(eStat < eStat_Last);
		m_fValues[eStat] = fValue;
	}

	// Convenience method - it's very common for these to be integers and few problems converting
	void SetStat(EStatistics eStat, int iValue)
	{ SetStat(eStat, static_cast<float>(iValue)); }

	void Render();

private:
	void InitDescriptions();

	enum EStatFlags
	{
		eSF_FrameReset = 1,                                    // Accumulated over a frame, so 0 on frame-reset
		eSF_Integer    = 1 << 1,                               // Actually an integer - treat it as such
	};

	struct SStatMetadata
	{
		char        description[20];  // Short textual description - bounded at 20 to fit neatly on screen
		int         flags;            // EStatFlags	specifying how to treat the value
		const char* format;           // Optional printf format specifier
	};

	float*               m_fValues;
	const SStatMetadata* m_sMetadata;
};

#endif // __AISTATSMANAGER
