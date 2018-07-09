// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequencerDopeSheetToolbar.h"

#include "Controls/SharedFonts.h"

CSequencerDopeSheetToolbar::CSequencerDopeSheetToolbar()
	: CDlgToolBar()
{
	m_lastTime = -1;
}

CSequencerDopeSheetToolbar::~CSequencerDopeSheetToolbar()
{
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetToolbar::InitToolbar()
{
	// Set up time display
	CRect rc(0, 0, 0, 0);
	int index = CommandToIndex(ID_TV_CURSORPOS);
	if (index >= 0)
	{
		SetButtonInfo(index, ID_TV_CURSORPOS, TBBS_SEPARATOR, 100);
		GetItemRect(index, &rc);
	}
	++rc.top;
	m_timeWindow.Create("0.000", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE | SS_SUNKEN, rc, this, IDC_STATIC);
	m_timeWindow.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFontBold));
	m_timeWindow.SetParent(this);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheetToolbar::SetTime(float fTime, float fFps)
{
	if (fTime == m_lastTime)
		return;

	m_lastTime = fTime;

	int nMins = (int)(fTime / 60.0f);
	fTime -= (float)(nMins * 60);
	int nSecs = (int)fTime;
	fTime -= (float)nSecs;
	int nMillis = fTime * 100.0f;
	int nFrames = (int)(fTime / (1.0f / CLAMP(fFps, FLT_EPSILON, FLT_MAX)));

	CString sText;
	sText.Format("%02d:%02d:%02d (%02d)", nMins, nSecs, nMillis, nFrames);
	m_timeWindow.SetWindowText(sText);
}

