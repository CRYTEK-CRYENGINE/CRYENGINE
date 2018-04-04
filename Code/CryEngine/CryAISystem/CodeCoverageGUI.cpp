// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/********************************************************************
   -------------------------------------------------------------------------
   File name:   CodeCoverageGUI.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 2 Mar 2009	: Evgeny Adamenkov: Removed parameter of type IRenderer from DebugDraw

 *********************************************************************/
#include "StdAfx.h"

#if !defined(_RELEASE)

	#include "CodeCoverageGUI.h"
	#include <CryGame/IGameFramework.h>
	#include "CodeCoverageManager.h"
	#include "DebugDrawContext.h"

// CCodeCoverageGUI construction & destruction
CCodeCoverageGUI::CCodeCoverageGUI(void) :
	m_pTex(NULL), m_pTexHit(NULL), m_pTexUnexpected(NULL), m_fPercentageDone(0.0f), m_fNewHit(0.0f), m_fUnexpectedHit(0.0f), m_nListUnexpectedSize(0)
{
}

CCodeCoverageGUI::~CCodeCoverageGUI(void)
{
	SAFE_RELEASE(m_pTex);
	SAFE_RELEASE(m_pTexHit);
	SAFE_RELEASE(m_pTexUnexpected);
}

// CCodeCoverageGUI operations
void CCodeCoverageGUI::Reset(IAISystem::EResetReason reason)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Code coverage GUI");

	for (int i = 0; i < CCLAST_ELEM; ++i)
		m_arrLast3[i] = SStrAndTime();

	stl::free_container(m_vecRemaining);

	if ((reason == IAISystem::RESET_ENTER_GAME) || (reason == IAISystem::RESET_INTERNAL_LOAD))
	{
		m_fPercentageDone = 0.f;
		m_fNewHit = 0.f;
		m_fUnexpectedHit = 0.f;
		m_nListUnexpectedSize = 0;

		SAFE_RELEASE(m_pTex);
		CDebugDrawContext dc;
		m_pTex = dc->LoadTexture("%ENGINE%/EngineAssets/CodeCoverage/pbar.dds", 0);

		SAFE_RELEASE(m_pTexHit);
		m_pTexHit = dc->LoadTexture("%ENGINE%/EngineAssets/CodeCoverage/hit.dds", 0);

		SAFE_RELEASE(m_pTexUnexpected);
		m_pTexUnexpected = dc->LoadTexture("%ENGINE%/EngineAssets/CodeCoverage/unexpected.dds", 0);
	}
	else if ((reason == IAISystem::RESET_EXIT_GAME) || (reason == IAISystem::RESET_UNLOAD_LEVEL))
	{
		/*
		   // Dump the list (TEMP)
		   {
		   typedef std::list<CCodeCoverageCheckPoint *> CheckPointList;
		   FILE *stream = freopen("ListOnExitGame.txt", "wt", stdout);
		   CheckPointList &lst = gAIEnv.pCodeCoverageTracker->GetTempList();
		   for (CheckPointList::iterator it = lst.begin(); it != lst.end(); ++it)
		   {
		    fprintf(stream, "%s\r\n", (*it)->GetLabel());
		   }
		   fclose(stream);
		   }*/

		SAFE_RELEASE(m_pTex);
		SAFE_RELEASE(m_pTexHit);
		SAFE_RELEASE(m_pTexUnexpected);
	}
}

void CCodeCoverageGUI::Update(CTimeValue frameStartTime, float frameDeltaTime)
{
	if (!gAIEnv.pCodeCoverageManager || !gAIEnv.pCodeCoverageManager->IsContextValid())
		return;

	int iTotal = gAIEnv.pCodeCoverageManager->GetTotal();
	int iTotalReg = gAIEnv.pCodeCoverageManager->GetTotalRegistered();
	m_fPercentageDone = (iTotal ? iTotalReg / (float)iTotal : -0.0f);

	// Check for unexpected hits
	int nSize = gAIEnv.pCodeCoverageManager->GetUnexpectedCheckpoints().size();
	if (nSize != m_nListUnexpectedSize)
	{
		m_nListUnexpectedSize = nSize;
		m_fUnexpectedHit = 1.f;
	}
	else
	{
		if (m_fUnexpectedHit > 0.f)
			m_fUnexpectedHit -= frameDeltaTime * .2f;
	}

	// Update the list
	for (int i = 0; i < CCLAST_ELEM; ++i)
	{
		if (m_arrLast3[i].fTime > 0.f)
			m_arrLast3[i].fTime -= frameDeltaTime;
	}

	if (m_fNewHit > 0.f)
		m_fNewHit -= frameDeltaTime * .33f;

	const char* pNewHit[3];
	gAIEnv.pCodeCoverageTracker->GetMostRecentAndReset(pNewHit);

	for (int i = 0; i < 3; ++i)
		if (pNewHit[i])
		{
			for (int j = 1; j < CCLAST_ELEM; ++j)
				m_arrLast3[j - 1] = m_arrLast3[j];
			m_arrLast3[CCLAST_ELEM - 1] = SStrAndTime(pNewHit[i], 3.f);
			m_fNewHit = 1.f;
		}

	// If there are only 10 left
	int nPointsLeft = gAIEnv.pCodeCoverageManager->GetTotal() - gAIEnv.pCodeCoverageManager->GetTotalRegistered();
	if ((nPointsLeft <= 10) && (nPointsLeft > 0))
		gAIEnv.pCodeCoverageManager->GetRemainingCheckpointLabels(m_vecRemaining);
	else if ((nPointsLeft == 0) && !m_vecRemaining.empty())
		m_vecRemaining.clear();
}

void CCodeCoverageGUI::DebugDraw(int nMode)
{
	if (!gAIEnv.pCodeCoverageManager || !gAIEnv.pCodeCoverageManager->IsContextValid())
		return;

	if (m_pTex && m_pTexHit)
	{
		if ((nMode == 3) || (nMode == 1 && m_fPercentageDone > .7f))
		{
			// For 3D drawing we have to assume 800x600. An old convention I guess.
			int w = 800;
			int h = 600;

			CDebugDrawContext dc;
			dc->Init2DMode();
			dc->SetAlphaBlended(false);
			dc->SetDepthTest(false);

			// Render the progress bar
			float xBar = w * .05f;
			float yBar = h * .05f;
			float wBar = w * .035f;
			float hBar = h * .9f;
			dc->Draw2dImage(xBar, yBar + hBar * (1.0f - m_fPercentageDone), wBar, hBar * m_fPercentageDone, m_pTex->GetTextureID(), 0.f, 1.f - m_fPercentageDone, 1, 1, 0, 1.f, 1.f, 1.f, .5f);

			// Render the NewHit quad
			if (m_fNewHit > 0.f)
			{
				float xNewHit = w * .05f;
				float yNewHit = h * .04f;
				float wNewHit = wBar;
				float hNewHit = h * (.062f - .04f);
				dc->Draw2dImage(xNewHit, yNewHit, wNewHit, hNewHit, m_pTexHit->GetTextureID(), 0, 1, 1, 0, 0, 1.f, 1.f, 1.f, m_fNewHit);
			}

			// Render UnexpectedHit quad
			if ((m_fUnexpectedHit > 0.f) && m_pTexUnexpected)
			{
				float xUnexpHit = w * .6f;
				float yUnexpHit = h * .75f;
				dc->Draw2dImage(xUnexpHit, yUnexpHit, 150, 75, m_pTexUnexpected->GetTextureID(), 0, 1, 1, 0, 0, 1.f, 1.f, 1.f, m_fUnexpectedHit);
			}

			// Render the 3 last hit
			float fLeft = .15f * w, fTop = .02f * h;
			for (int i = 0; i < CCLAST_ELEM; ++i)
				if (m_arrLast3[i].fTime > 0.f)
				{
					ColorB color(128, 255, 255, uint8(255 * m_arrLast3[i].fTime / 3.f));
					dc->Draw2dLabel(fLeft, fTop, 1.7f, color, false, "%s", m_arrLast3[i].pStr);
					fTop += 20.f;
				}

			// Render percentage
			ColorB colorPercentage(255, 255, 255);
			dc->Draw2dLabel(xBar + wBar * 0.5f, yBar + hBar * 0.5f, 1.5f, colorPercentage, false, "%0.0f%%", m_fPercentageDone * 100.f);

			// Render the remaining 10
			if (!m_vecRemaining.empty())
			{
				fTop = .14f * h;
				ColorB color(255, 102, 102);
				for (std::vector<const char*>::iterator it = m_vecRemaining.begin(); it != m_vecRemaining.end(); ++it)
				{
					dc->Draw2dLabel(fLeft, fTop, 1.2f, color, false, "%s", *it);
					fTop += 14.f;
				}
			}
		}
	}
}

#endif //_RELEASE
