// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
-------------------------------------------------------------------------
File name:   GameCodeCoverageGUI.cpp
$Id$
Description: 

-------------------------------------------------------------------------
History:
- ?
- 2 Mar 2009	: Evgeny Adamenkov: Removed parameter of type IRenderer from DebugDraw

*********************************************************************/
#include "StdAfx.h"
#include "GameCodeCoverage/GameCodeCoverageGUI.h"
#include <CryGame/IGameFramework.h>
#include "GameCodeCoverage/GameCodeCoverageManager.h"
#include "Utility/CryWatch.h"

#if ENABLE_GAME_CODE_COVERAGE

CGameCodeCoverageGUI * CGameCodeCoverageGUI::s_instance = NULL;

CGameCodeCoverageGUI::CGameCodeCoverageGUI(void) : REGISTER_GAME_MECHANISM(CGameCodeCoverageGUI)
{
	assert (s_instance == NULL);
	s_instance = this;

	m_showListWhenNumUnhitCheckpointsIs = 10;

	REGISTER_CVAR2("GCC_showListWhenNumUnhitCheckpointsIs", & m_showListWhenNumUnhitCheckpointsIs, m_showListWhenNumUnhitCheckpointsIs, VF_NULL, "Show checkpoint names on-screen when the number of unhit checkpoints gets down to this number");
}

CGameCodeCoverageGUI::~CGameCodeCoverageGUI(void)
{
	assert (s_instance == this);
	s_instance = NULL;

	//GetISystem()->GetIConsole()->UnregisterVariable("GCC_showListWhenNumUnhitCheckpointsIs");
}

void CGameCodeCoverageGUI::Draw()
{
	CGameCodeCoverageManager * mgr = CGameCodeCoverageManager::GetInstance();

	CryWatch ("======================================================================================================");

	if (mgr->IsContextValid())
	{
		int iTotal = mgr->GetTotalCheckpointsReadFromFileAndValid();

		if (iTotal > 0)
		{
			int iTotalReg = mgr->GetTotalValidCheckpointsHit();
			int nPointsLeft = iTotal - iTotalReg;
			float percentageDone = ( iTotal ? iTotalReg / (float)iTotal : -0.0f);

			CryWatch ("Code coverage %d%% complete! (Hit %d out of %d, %d left)", (int)(percentageDone * 100.f), iTotalReg, iTotal, nPointsLeft);

			if ((nPointsLeft <= m_showListWhenNumUnhitCheckpointsIs) && (nPointsLeft > 0))
			{
				std::vector<const char *> vecRemaining;
				mgr->GetRemainingCheckpointLabels(vecRemaining);

				for (std::vector<const char *>::iterator it = vecRemaining.begin(); it != vecRemaining.end(); ++it)
				{
					const char * txt = *it;
					CryWatch ("  Still need to hit: $8%s", txt);
				}
			}
			else
			{
				for (int i = 0; i < mgr->GetNumAutoNamedGroups(); ++ i)
				{
					const CNamedCheckpointGroup * group = mgr->GetAutoNamedCheckpointGroup(i);
					int numHit = group->GetNumHit();
					int numValid = group->GetNumValidForCurrentGameState();
					if (numHit < numValid)
					{
						const SLabelInfoFromFile * finalCheckpoint = (numHit == numValid - 1) ? group->GetFirstUnhitValidCheckpoint() : NULL;
						CryWatch ("  Have hit %s%d/%d checkpoints in $7%s$o group%s%s", numHit ? "$3" : "$4", numHit, numValid, group->GetName(), finalCheckpoint ? " - $8" : "", finalCheckpoint ? finalCheckpoint->m_labelName : "");
					}
				}
			}
		}
		else
		{
			CryWatch ("Not expecting to hit any code coverage checkpoints while the game is in this state!");
		}
	}
	else
	{
		CryWatch ("Code coverage context is invalid! Check game log file for details...");
	}

	const CGameCodeCoverageManager::SRecentlyHitList & recentlyHitList = mgr->GetRecentlyHitList();
	for (int i = 0; i < recentlyHitList.m_count; ++i)
	{
		CryWatch ("  Recently hit: $5%s%s", recentlyHitList.m_array[i].pStr, recentlyHitList.m_array[i].bExpected ? "" : " $4UNEXPECTED!");
	}

	CryWatch ("======================================================================================================");
}

#endif