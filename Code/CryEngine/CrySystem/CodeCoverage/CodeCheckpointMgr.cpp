// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     08/04/2010 by Will W (based on work by Matthew J)
//  Description: A central class to track code checkpoint registration
// -------------------------------------------------------------------------
//  History: Created by Will Wilson based on work by Matthew Jack.
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "CodeCheckpointMgr.h"

CCodeCheckpointMgr::CCodeCheckpointMgr()
{
	// Start with a reasonable reserve size to avoid excessive early reallocations
	m_checkpoints.reserve(40);
}

CCodeCheckpointMgr::~CCodeCheckpointMgr()
{
	// Ensure any remaining heap allocated names are deleted
	for (TCheckpointVector::const_iterator iter(m_checkpoints.begin()), endIter(m_checkpoints.end()); iter != endIter; ++iter)
	{
		const CheckpointRecord& rec = *iter;

		// If the checkpoint was never registered, the name is heap allocated
		if (!rec.m_pCheckpoint)
			delete[] rec.m_name;
	}
}

/// Used by code checkpoints to register themselves with the manager.
void CCodeCheckpointMgr::RegisterCheckpoint(CCodeCheckpoint* pCheckpoint)
{
	CRY_ASSERT(pCheckpoint);
	CRY_ASSERT(pCheckpoint->Name() != NULL);
	CRY_ASSERT(pCheckpoint->HitCount() == 0);

	CryAutoCriticalSection lock(m_critSection);

	// Check to see if the record for this CP exists already
	const size_t existingRecordIndex = FindRecordByName(pCheckpoint->Name());

	// If no existing record was found
	if (existingRecordIndex == ~0)
	{
		CheckpointRecord newRec;
		newRec.m_pCheckpoint = pCheckpoint;

		// Use the static name from the checkpoint
		newRec.m_name = pCheckpoint->Name();

		// Create a new record
		m_checkpoints.push_back(newRec);
	}
	else  // Checkpoint name already present
	{
		CheckpointRecord& oldRec = m_checkpoints[existingRecordIndex];

		// If this is a new checkpoint instance (not a duplicate)
		if (oldRec.m_pCheckpoint == NULL)
		{
			/// Delete the old heap allocated name (created by GetCheckpointIndex())
			delete[] oldRec.m_name;

			oldRec.m_pCheckpoint = pCheckpoint;

			// Use the static name from the checkpoint
			oldRec.m_name = pCheckpoint->Name();
		}
		else  // Error: This is a duplicate of a preexisting CP
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Duplicate CODECHECKPOINT(\"%s\") found. Ignoring.", pCheckpoint->Name());
		}

		// Ensure duplicate code checkpoints are renamed
		CRY_ASSERT_TRACE(oldRec.m_pCheckpoint == NULL, ("Duplicate CODECHECKPOINT(\"%s\") found. Please rename!", pCheckpoint->Name()));
	}
}

void CCodeCheckpointMgr::UnRegisterCheckpoint(const char* szName)
{
	CryAutoCriticalSection lock(m_critSection);

	for (TCheckpointVector::iterator iter(m_checkpoints.begin()), endIter(m_checkpoints.end()); iter != endIter; ++iter)
	{
		const CheckpointRecord& rec = *iter;
		if (strcmp(rec.m_name, szName) == 0)
		{
			m_checkpoints.erase(iter);
			break;
		}
	}
}

/// Performs a (possibly) expensive lookup by name for a given checkpoint index. Should never fail.
size_t CCodeCheckpointMgr::GetCheckpointIndex(const char* name)
{
	// Ensure name is valid
	CRY_ASSERT(name);

	CryAutoCriticalSection lock(m_critSection);

	size_t recordIndex = FindRecordByName(name);

	// If no index was found
	if (recordIndex == ~0)
	{
		// The checkpoint has not (yet) registered. So create an empty record for it.
		size_t nameLength = strlen(name);

		CheckpointRecord newRec;
		newRec.m_pCheckpoint = NULL;

		// Create a dynamic string buffer for a NULL terminated copy of the name
		char* nameCopy = new char[nameLength + 1];

		strcpy(nameCopy, name);

		newRec.m_name = nameCopy;

		recordIndex = m_checkpoints.size();
		m_checkpoints.push_back(newRec);
	}

	// Should always succeed!
	CRY_ASSERT(recordIndex != ~0);

	return recordIndex;
}

/// Performs a cheap lookup by index, will return NULL if checkpoint has not yet been registered.
const CCodeCheckpoint* CCodeCheckpointMgr::GetCheckpoint(size_t checkpointIdx) const
{
	CCodeCheckpoint* pCheckpoint = NULL;

	CryAutoCriticalSection lock(m_critSection);

	// Ensure checkpoint index is legal
	CRY_ASSERT(checkpointIdx < m_checkpoints.size());
	if (checkpointIdx < m_checkpoints.size())
	{
		const CheckpointRecord& rec = m_checkpoints[checkpointIdx];

		pCheckpoint = rec.m_pCheckpoint;
	}

	return pCheckpoint;
}

const char* CCodeCheckpointMgr::GetCheckPointName(size_t checkpointIdx) const
{
	CryAutoCriticalSection lock(m_critSection);

	// Ensure checkpoint index is legal
	CRY_ASSERT(checkpointIdx < m_checkpoints.size());
	if (checkpointIdx < m_checkpoints.size())
	{
		const CheckpointRecord& rec = m_checkpoints[checkpointIdx];

		return rec.m_name;
	}

	return "";
}

/// Returns the total number of checkpoints
size_t CCodeCheckpointMgr::GetTotalCount() const
{
	CryAutoCriticalSection lock(m_critSection);
	return m_checkpoints.size();
}

/// Returns the total number or registered checkpoints
size_t CCodeCheckpointMgr::GetTotalEncountered() const
{
	size_t regCount = 0;

	CryAutoCriticalSection lock(m_critSection);

	for (TCheckpointVector::const_iterator iter(m_checkpoints.begin()); iter != m_checkpoints.end(); ++iter)
	{
		const CheckpointRecord& rec = *iter;

		// Is there a registered checkpoint for this record?
		if (rec.m_pCheckpoint)
			++regCount;
	}

	return regCount;
}

/// Frees this instance from memory
void CCodeCheckpointMgr::Release()
{
	delete this;
}

/// Returns index for record matching name or ~0 if not found.
size_t CCodeCheckpointMgr::FindRecordByName(const char* name) const
{
	// Ensure the caller has exclusive access
	CRY_ASSERT(m_critSection.IsLocked());

	size_t index = ~0;

	// Check to see if the record for this CP exists already
	for (TCheckpointVector::const_iterator iter(m_checkpoints.begin()), endIter(m_checkpoints.end()); iter != endIter; ++iter)
	{
		const CheckpointRecord& rec = *iter;

		if (strcmp(rec.m_name, name) == 0)
		{
			index = iter - m_checkpoints.begin();
			break;
		}
	}

	return index;
}
