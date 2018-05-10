// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
-------------------------------------------------------------------------
File name:   GameCodeCoverageGUI.h
$Id$
Description: 

-------------------------------------------------------------------------
//  History:     Tim Furnish, 11/11/2009:
//               Moved into game DLL from AI system
//               Wrapped contents in ENABLE_GAME_CODE_COVERAGE
*********************************************************************/

#ifndef __GAME_CODE_COVERAGE_GUI_H_
#define __GAME_CODE_COVERAGE_GUI_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include "GameCodeCoverage/GameCodeCoverageEnabled.h"
#include "GameMechanismManager/GameMechanismBase.h"

#if ENABLE_GAME_CODE_COVERAGE

class CGameCodeCoverageGUI : public CGameMechanismBase
{
public:		// Construction & destruction
	CGameCodeCoverageGUI(void);
	~CGameCodeCoverageGUI(void);

	static ILINE CGameCodeCoverageGUI * GetInstance()
	{
		return s_instance;
	}

public:		// Operations
	void Draw();

private:	// Member data
	static CGameCodeCoverageGUI * s_instance;

	virtual void Update(float dt) {}

	int m_showListWhenNumUnhitCheckpointsIs;
};

#endif	// ENABLE_GAME_CODE_COVERAGE

#endif	// __GAME_CODE_COVERAGE_GUI_H_