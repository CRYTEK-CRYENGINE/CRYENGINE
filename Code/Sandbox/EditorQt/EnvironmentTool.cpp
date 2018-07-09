// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EnvironmentTool.h"
#include "EnvironmentPanel.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEnvironmentTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CEnvironmentTool::CEnvironmentTool()
{
	SetStatusText(_T("Click Apply to accept changes"));
	m_panelId = 0;
	m_panel = 0;
}

//////////////////////////////////////////////////////////////////////////
CEnvironmentTool::~CEnvironmentTool()
{
}

//////////////////////////////////////////////////////////////////////////
void CEnvironmentTool::BeginEditParams(IEditor* ie, int flags)
{
	m_ie = ie;
	if (!m_panelId)
	{
		m_panel = new CEnvironmentPanel(AfxGetMainWnd());
		m_panelId = m_ie->AddRollUpPage(ROLLUP_TERRAIN, "Environment", m_panel);
		AfxGetMainWnd()->SetFocus();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEnvironmentTool::EndEditParams()
{
	if (m_panelId)
	{
		m_ie->RemoveRollUpPage(ROLLUP_TERRAIN, m_panelId);
		m_panel = 0;
	}
}

