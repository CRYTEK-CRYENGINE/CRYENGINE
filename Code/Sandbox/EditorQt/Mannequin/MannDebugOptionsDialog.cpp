// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannDebugOptionsDialog.h"

#include "Controls/PropertiesPanel.h"
#include "MannequinModelViewport.h"

BEGIN_MESSAGE_MAP(CMannequinDebugOptionsDialog, CDialog)
END_MESSAGE_MAP()

CMannequinDebugOptionsDialog::CMannequinDebugOptionsDialog(CMannequinModelViewport* pModelViewport, CWnd* pParent)
	: CXTResizeDialog(IDD_MANN_DEBUG_OPTIONS, pParent),
	m_pModelViewport(pModelViewport)
{
}

BOOL CMannequinDebugOptionsDialog::OnInitDialog()
{
	const int IDC_PROPERTIES_CONTROL = 1;
	__super::OnInitDialog();

	m_pPanel.reset(new CPropertiesPanel(this));
	m_pPanel->SetOwner(this);
	m_pPanel->SetDlgCtrlID(IDC_PROPERTIES_CONTROL);
	m_pPanel->AddVars(m_pModelViewport->GetVarObject());
	m_pPanel->ShowWindow(SW_SHOW);

	CRect rc;
	GetClientRect(&rc);
	rc.DeflateRect(8, 8);
	m_pPanel->MoveWindow(&rc);

	SetResize(IDC_PROPERTIES_CONTROL, SZ_TOP_LEFT, SZ_BOTTOM_RIGHT);

	return TRUE;
}

