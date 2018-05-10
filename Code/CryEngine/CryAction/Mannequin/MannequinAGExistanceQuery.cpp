// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MannequinAGExistanceQuery.h"

// ============================================================================
// ============================================================================

namespace MannequinAG
{

// ============================================================================
// ============================================================================

//////////////////////////////////////////////////////////////////////////
CMannequinAGExistanceQuery::CMannequinAGExistanceQuery(IAnimationGraphState* pAnimationGraphState)
	: m_pAnimationGraphState(pAnimationGraphState)
{
	CRY_ASSERT(m_pAnimationGraphState != NULL);
}

//////////////////////////////////////////////////////////////////////////
CMannequinAGExistanceQuery::~CMannequinAGExistanceQuery()
{
}

//////////////////////////////////////////////////////////////////////////
IAnimationGraphState* CMannequinAGExistanceQuery::GetState()
{
	return m_pAnimationGraphState;
}

//////////////////////////////////////////////////////////////////////////
void CMannequinAGExistanceQuery::SetInput(InputID, float)
{
}

//////////////////////////////////////////////////////////////////////////
void CMannequinAGExistanceQuery::SetInput(InputID, int)
{
}

//////////////////////////////////////////////////////////////////////////
void CMannequinAGExistanceQuery::SetInput(InputID, const char*)
{
}

//////////////////////////////////////////////////////////////////////////
bool CMannequinAGExistanceQuery::Complete()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
CTimeValue CMannequinAGExistanceQuery::GetAnimationLength() const
{
	return CTimeValue();
}

//////////////////////////////////////////////////////////////////////////
void CMannequinAGExistanceQuery::Reset()
{
}

// ============================================================================
// ============================================================================

} // MannequinAG
