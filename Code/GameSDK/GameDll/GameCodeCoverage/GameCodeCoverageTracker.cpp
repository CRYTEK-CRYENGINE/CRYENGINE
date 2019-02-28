// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameCodeCoverageTracker.cpp
//  Created:     18/06/2008 by Matthew
//  Description: Defines code coverage check points
//               and a central class to track their registration
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "GameCodeCoverage/GameCodeCoverageManager.h"

#if ENABLE_GAME_CODE_COVERAGE

CGameCodeCoverageCheckPoint::CGameCodeCoverageCheckPoint( const char * label ) : m_nCount(0), m_psLabel(label)
{
	assert(label);
	CGameCodeCoverageManager::GetInstance()->Register(this);
}

void CGameCodeCoverageCheckPoint::Touch()
{
	++ m_nCount;
	CGameCodeCoverageManager::GetInstance()->Hit(this);
}

#endif