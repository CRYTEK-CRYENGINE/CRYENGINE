// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   CodeCoverageManager.cpp
//  Created:     18/06/2008 by Matthew
//  Description: High-level manager class for code coverage system
//               including file handing of checkpoint lists
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

/*
 * File format specification:
 *   The "Code Coverage Context" file format is a text file defined as follows:
 *   - The first line is a numeric string giving the number of checkpoints listed in this file
 *   - All subsequent lines are a single alphanumeric string (no character restrictions so far)
 *   - Each line is delimited by a DOS or Unix newline and is limited to 64 character excluding the delimiter
 *
 * Design notes:
 *   We're only interested in checkpoints that we haven't hit yet
 *   Hence we could wait until we have 90% of checkpoints and then load the context, keeping only new labels.
 *   Could be worth file specifying total buffer size required.
 */

#include "StdAfx.h"

#if !defined(_RELEASE)

	#include "CodeCoverageManager.h"

const static int LABEL_LENGTH = 64;
const static int BUFF_SIZE = LABEL_LENGTH + 3;

//---------------------------------------------------------------------------------------//

CCodeCoverageManager::CCodeCoverageManager() :
	m_bContextValid(false)
	, m_pLabelBlock(NULL)
	, m_nTotalCheckpoints(0)
{
	assert(GetTracker());
}

//---------------------------------------------------------------------------------------//

bool CCodeCoverageManager::ReadCodeCoverageContext(FILE* fp)
{
	assert(fp);

	// Temporary buffer
	char cBuffer[BUFF_SIZE];

	// Clear any packed string block
	SAFE_DELETE(m_pLabelBlock);

	char* pNextLabelInBlock = NULL;

	// Clear all state in manager

	bool bOk = true;

	// Allow a comment to start file?

	// Read number of checkpoints
	m_nTotalCheckpoints = 0;
	bOk = bOk && (GetLine(cBuffer, fp) > 0);
	if (bOk)
	{
		// Grab as an integer and sanity check
		m_nTotalCheckpoints = atoi(cBuffer);
		bOk = (m_nTotalCheckpoints > 0 && m_nTotalCheckpoints < 1000000);
		assert(bOk);
	}

	if (bOk)
	{
		// Allocate packed string block (assuming worst-case of max length labels + \0)
		m_pLabelBlock = new char[m_nTotalCheckpoints * (BUFF_SIZE + 1)];
		pNextLabelInBlock = m_pLabelBlock;
	}

	for (int i = 0; i < m_nTotalCheckpoints && bOk; ++i)
	{
		// Get next checkpoint label
		int nCharsRead = GetLine(pNextLabelInBlock, fp);
		bOk = nCharsRead > 0;

		if (bOk)
		{
			// Do a little check for practicality's sake
			if (strncmp(pNextLabelInBlock, "AI_", 3) != 0)
				AIWarning("CodeCoverageManager: So far only the AI system uses code coverage - and this label doesn't start AI_. \"%s\"", pNextLabelInBlock);

			// Add to set
			m_setCheckPoints.insert(pNextLabelInBlock);

			// Advance pointer
			pNextLabelInBlock += nCharsRead;
		}
	}

	m_bContextValid = bOk;

	if (bOk)
	{
		AILogComment("CodeCoverageManager: Read all %d code coverage checkpoints successfully", m_nTotalCheckpoints);
		// Does this finish initialisation?
		Update();
	}
	else
	{
		AIWarning("CodeCoverageManager: Failed! Found file but failed after reading %d code coverage checkpoints", m_nTotalCheckpoints);
	}

	return bOk;
}

//---------------------------------------------------------------------------------------//

void CCodeCoverageManager::GetRemainingCheckpointLabels(std::vector<const char*>& vLabels) const
{
	vLabels.clear();
	vLabels.reserve(m_nTotalCheckpoints - GetTotalRegistered());

	for (CheckPointSet::const_iterator it = m_setCheckPoints.begin(); it != m_setCheckPoints.end(); ++it)
	{
		CCodeCoverageCheckPoint* pCP = GetTracker()->GetCheckPoint(*it);
		if (!pCP)
			vLabels.push_back(*it);
	}
}

//---------------------------------------------------------------------------------------//

int CCodeCoverageManager::GetLine(char* pBuff, FILE* fp)
{
	assert(pBuff && fp);

	// Wraps fgets to remove newlines and use correct buffer/string lengths

	// Must use CryPak FGets on CryPak files
	if (!gEnv->pCryPak->FGets(pBuff, BUFF_SIZE, fp))
		return 0;

	// Convert newlines to \0 and discover length
	int i = 0;
	int nLimit = LABEL_LENGTH + 1;
	for (; i < nLimit; i++)
	{
		char c = pBuff[i];
		if (c == '\n' || c == '\r' || c == '\0')
			break;
	}

	// If i == LABEL_LENGTH, the string was of maximum length
	// If i == LABEL_LENGTH + 1, the string was too long
	if (i == nLimit)
	{
		// Malformed - fail
		pBuff[--i] = '\0';
		AIWarningID("<CCodeCoverageManager>", "Checkpoint label %s... in code coverage context file is too long", pBuff);
		return 0;
	}

	// Overwrite the last character (usually line terminator) with string delimiter
	pBuff[i] = '\0';

	if (i == 0)
	{
		// Malformed - fail
		AIWarningID("<CCodeCoverageManager>", "Empty line in code coverage context file");
		return 0;
	}

	return i + 1;
}

//---------------------------------------------------------------------------------------//

void CCodeCoverageManager::Update(void)
{
	const CheckPointVector& lst = GetTracker()->GetRecentCheckpoints();
	for (CheckPointVector::const_iterator it = lst.begin(); it != lst.end(); ++it)
	{
		if (m_setCheckPoints.find((*it)->GetLabel()) != m_setCheckPoints.end())
		{
			// add to map
			GetTracker()->AddCheckpoint(*it);
		}
		else
		{
			m_setUnexpectedCheckPoints.insert((*it)->GetLabel());
		}
	}

	GetTracker()->ResetRecentCheckpoints();
}

void CCodeCoverageManager::DumpCheckpoints(bool bToFile) const
{
	if (bToFile)
	{
		const char* sDumpFile = "DumpedPoints.txt";
		// Should really be in the level folder, when process is mature
		string filePath = PathUtil::Make(PathUtil::GetGameFolder(), sDumpFile);
		FILE* stream = gEnv->pCryPak->FOpen(filePath.c_str(), "w");

		// Print the unexpected checkpoints we've hit
		gEnv->pCryPak->FPrintf(stream, "Unexpected Checkpoints Hit: %" PRISIZE_T "\n", m_setUnexpectedCheckPoints.size());
		for (CheckPointSet::const_iterator it = m_setUnexpectedCheckPoints.begin(); it != m_setUnexpectedCheckPoints.end(); ++it)
			gEnv->pCryPak->FPrintf(stream, "%s\n", *it);

		// If we've hit more than 80% of the expected checkpoints, then print the remaining ones
		if (m_setCheckPoints.size() * .8f < (float)GetTracker()->GetTotalRegistered())
		{
			gEnv->pCryPak->FPrintf(stream, "Remaining Checkpoints: %" PRISIZE_T "\n", m_setUnexpectedCheckPoints.size());

			std::vector<const char*> vec;
			GetRemainingCheckpointLabels(vec);

			for (std::vector<const char*>::iterator it = vec.begin(); it != vec.end(); ++it)
				gEnv->pCryPak->FPrintf(stream, "%s\n", *it);
		}
		gEnv->pCryPak->FClose(stream);
		AILogAlways("Dumped checkpoints to file \"%s\"", filePath.c_str());
	}
	else
	{
		// Print the unexpected checkpoints we've hit
		gEnv->pLog->Log("CC:Unexpected Checkpoints Hit:");
		for (CheckPointSet::const_iterator it = m_setUnexpectedCheckPoints.begin(); it != m_setUnexpectedCheckPoints.end(); ++it)
			gEnv->pLog->Log("  CC:%s", *it);

		// If we've hit more than 30% of the expected checkpoints, then print the remaining ones
		// If not, probably this dump was done too early
		if (m_setCheckPoints.size() * .3f < (float)GetTracker()->GetTotalRegistered())
		{
			gEnv->pLog->Log("CC:Remaining Checkpoints:");

			for (CheckPointSet::const_iterator it = m_setCheckPoints.begin(); it != m_setCheckPoints.end(); ++it)
			{
				CCodeCoverageCheckPoint* pCP = GetTracker()->GetCheckPoint(*it);
				if (!pCP)
				{
					gEnv->pLog->Log("  CC:%s", *it);
				}
			}

			gEnv->pLog->Log("CC:Checkpoints Hit:");

			for (CheckPointSet::const_iterator it = m_setCheckPoints.begin(); it != m_setCheckPoints.end(); ++it)
			{
				CCodeCoverageCheckPoint* pCP = GetTracker()->GetCheckPoint(*it);
				if (pCP)
				{
					gEnv->pLog->Log("  CC:%s", *it);
				}
			}

		}
		AILogAlways("Dumped checkpoints to log");
	}
}

#endif //_RELEASE
