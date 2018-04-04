// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   CodeCoverageTracker.cpp
//  Created:     18/06/2008 by Matthew
//  Description: Defines code coverage check points
//               and a central class to track their registration
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if !defined(_RELEASE)

	#include "CodeCoverageTracker.h"
	#include "CodeCoverageManager.h"

// CCodeCoverageCheckPoint constructor
CCodeCoverageCheckPoint::CCodeCoverageCheckPoint(const char* label) : m_nCount(0), m_psLabel(label)
{
	assert(label);
	// Register self with central manager
	if (gAIEnv.pCodeCoverageTracker)
		gAIEnv.pCodeCoverageTracker->Register(this);
}

// CCodeCoverageTracker constructor
CCodeCoverageTracker::CCodeCoverageTracker(void)
{
	for (int i = 0; i < 3; ++i)
		m_pMostRecent[i] = NULL;
	m_nLastEntry = 0;
}

// CCodeCoverageTracker operations
CCodeCoverageCheckPoint* CCodeCoverageTracker::GetCheckPoint(const char* sLabel) const
{
	assert(sLabel);
	CheckPointMap::const_iterator it(m_mCheckPoints.find(sLabel));
	return (it != m_mCheckPoints.end() ? it->second : NULL);
}

void CCodeCoverageTracker::Register(CCodeCoverageCheckPoint* pPoint)
{
	assert(pPoint);

	m_vecCheckPoints.push_back(pPoint);

	/*
	   if (!gAIEnv.pCodeCoverageManager || !gAIEnv.pCodeCoverageManager->IsContextValid())
	   {
	   // At this point the CodeCoverageManager is not in a valid state.
	   // We keep track of points encountered in a temporary list for now
	   m_vecCheckPoints.push_back(pPoint);
	   }
	   else
	   {
	   // Add it to the map
	   // And, ensure that no other checkpoint has the same name
	   CheckPointMap::iterator it = m_mCheckPoints.find(pPoint->GetLabel());
	   assert (it == m_mCheckPoints.end());
	   if (gAIEnv.pCodeCoverageManager->IsExpected(pPoint->GetLabel()))
	    m_mCheckPoints.insert(std::make_pair(pPoint->GetLabel(), pPoint));
	   else
	    m_vecCheckPoints.push_back(pPoint);

	   // Store this label so it can be retrieved by the renderer for the code coverage
	   m_pMostRecent[m_nLastEntry++] = pPoint->GetLabel();
	   if (3 == m_nLastEntry)
	    m_nLastEntry = 0;

	   //gEnv->pLog->Log("Registered checkpoint: %s", pPoint->m_psLabel);
	   }
	 */
	m_pMostRecent[m_nLastEntry++] = pPoint->GetLabel();
	if (3 == m_nLastEntry)
		m_nLastEntry = 0;
}

void CCodeCoverageTracker::Clear()
{
	for (CheckPointMap::iterator it = m_mCheckPoints.begin(); it != m_mCheckPoints.end(); ++it)
		it->second->Reset();
}

void CCodeCoverageTracker::Reset()
{
	Clear();
	m_mCheckPoints.clear();
}

int CCodeCoverageTracker::GetTotalRegistered()
{
	return (int)m_mCheckPoints.size();
}

void CCodeCoverageTracker::GetMostRecentAndReset(const char* pRet[3])
{
	for (int i = 0; i < 3; ++i)
	{
		pRet[i] = m_pMostRecent[i];
		m_pMostRecent[i] = NULL;
	}
}

#endif //_RELEASE
