// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertiesPanel.h"

#include "Controls/PropertyCtrl.h"

#define HEIGHT_OFFSET 4
#define HEIGHT_ADD    4

/////////////////////////////////////////////////////////////////////////////
// CPropertiesPanel dialog

CPropertiesPanel::CPropertiesPanel(CWnd* pParent)
	: CDialog(IDD_PANEL_PROPERTIES, pParent)
	, m_multiSelect(false)
	, m_titleAdjust(0)
{
	m_pWndProps.reset(new CPropertyCtrl());

	Create(IDD_PANEL_PROPERTIES, pParent);
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPropertiesPanel)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPropertiesPanel, CDialog)
//{{AFX_MSG_MAP(CPropertiesPanel)
ON_WM_DESTROY()
ON_WM_KILLFOCUS()
ON_WM_SIZE()
//}}AFX_MSG_MAP
ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPropertiesPanel message handlers

BOOL CPropertiesPanel::OnInitDialog()
{
	CDialog::OnInitDialog();

	ModifyStyleEx(0, WS_EX_CLIENTEDGE);

	CRect rc;
	GetClientRect(rc);

	CStatic* pStatic = (CStatic*)GetDlgItem(IDC_CHILD_CONTROL);
	if (pStatic != NULL)
	{
		pStatic->GetClientRect(rc);
		m_pWndProps->Create(WS_CHILD | WS_VISIBLE, rc, pStatic);
		m_pWndProps->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPropertiesPanel::SetMultiSelect(bool bEnable)
{
	if (bEnable == m_multiSelect)
		return;

	m_multiSelect = bEnable;
	if (m_pWndProps->m_hWnd)
	{
		m_pWndProps->SetDisplayOnlyModified(bEnable);
		m_pWndProps->ReloadValues();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::ResizeToFitProperties()
{
	CRect rc;
	GetClientRect(rc);
	int h = m_pWndProps->GetVisibleHeight() + HEIGHT_ADD;
	if (h > 400)
		h = 400;
	SetWindowPos(NULL, 0, 0, rc.right, h + HEIGHT_OFFSET * 2 + 4, SWP_NOMOVE);
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::DeleteVars()
{
	if (!m_pWndProps->m_hWnd)
		return;
	m_pWndProps->DeleteAllItems();
	m_updateCallbacks.clear();
	m_varBlock = 0;
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::SetVarBlock(class CVarBlock* vb, const UpdateCallback& updCallback, const bool resizeToFit)
{
	assert(vb);

	if (!m_pWndProps->m_hWnd)
		return;

	m_varBlock = vb;

	// Must first clear any selection in properties window.
	m_pWndProps->ClearSelection();
	m_pWndProps->DeleteAllItems();
	m_varBlock = vb;
	m_pWndProps->AddVarBlock(m_varBlock);

	m_pWndProps->SetUpdateCallback(functor(*this, &CPropertiesPanel::OnPropertyChanged));
	m_pWndProps->ExpandAll();

	if (resizeToFit)
		ResizeToFitProperties();

	m_multiSelect = false;
	m_pWndProps->SetDisplayOnlyModified(false);

	// When new object set all previous callbacks freed.
	m_updateCallbacks.clear();
	if (updCallback)
		stl::push_back_unique(m_updateCallbacks, updCallback);
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::AddVars(CVarBlock* vb, const UpdateCallback& updCallback)
{
	assert(vb);

	if (!m_pWndProps->m_hWnd)
		return;
	//m_pWndProps->DeleteAllItems();

	bool bNewBlock = false;
	// Make a clone of properties.
	if (!m_varBlock)
	{
		// Must first clear any selection in properties window.
		m_pWndProps->ClearSelection();
		m_pWndProps->DeleteAllItems();
		m_varBlock = vb->Clone(true);
		m_pWndProps->AddVarBlock(m_varBlock);
		bNewBlock = true;
	}
	m_varBlock->Wire(vb);
	//CVarBlock *propVB = m_varBlock->Clone(true);
	//propVB->Wire( m_varBlock );
	//m_pWndProps->AddVarBlock( propVB );

	if (bNewBlock)
	{
		m_pWndProps->SetUpdateCallback(functor(*this, &CPropertiesPanel::OnPropertyChanged));
		m_pWndProps->ExpandAll();

		ResizeToFitProperties();

		m_multiSelect = false;
		m_pWndProps->SetDisplayOnlyModified(false);

		// When new object set all previous callbacks freed.
		m_updateCallbacks.clear();
	}
	else
	{
		m_multiSelect = true;
		m_pWndProps->SetDisplayOnlyModified(true);
	}

	if (updCallback)
		stl::push_back_unique(m_updateCallbacks, updCallback);
}

void CPropertiesPanel::ReloadValues()
{
	if (m_pWndProps->m_hWnd)
	{
		m_pWndProps->ReloadValues();
	}
}

void CPropertiesPanel::OnDestroy()
{
	CDialog::OnDestroy();
}

void CPropertiesPanel::OnKillFocus(CWnd* pNewWnd)
{
	CDialog::OnKillFocus(pNewWnd);
}

void CPropertiesPanel::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	CStatic* pStatic = (CStatic*)GetDlgItem(IDC_CHILD_CONTROL);
	if (pStatic != NULL)
	{
		pStatic->SetWindowPos(this, 0, 0, cx, cy, SWP_NOZORDER);
	}

	if (m_pWndProps->m_hWnd)
	{
		m_pWndProps->SetWindowPos(this, 0, m_titleAdjust, cx, cy - m_titleAdjust, SWP_NOZORDER);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::OnPropertyChanged(IVariable* pVar)
{
	std::list<UpdateCallback>::iterator iter;
	for (iter = m_updateCallbacks.begin(); iter != m_updateCallbacks.end(); ++iter)
	{
		(*iter)(pVar);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::UpdateVarBlock(CVarBlock* pVars)
{
	m_pWndProps->UpdateVarBlock(pVars);
}

//////////////////////////////////////////////////////////////////////////
void CPropertiesPanel::SetTitle(const char* title)
{
	CStatic* pStatic = (CStatic*)GetDlgItem(IDC_CHILD_CONTROL);
	if (pStatic != NULL)
	{
		string paddedTitle(title);
		paddedTitle = " " + paddedTitle;

		if (title == NULL)
		{
			m_titleAdjust = 0;
		}
		else
		{
			CClientDC dc(pStatic);
			CRect rc;
			dc.DrawText(paddedTitle.GetString(), &rc, DT_CALCRECT);
			m_titleAdjust = rc.bottom;
		}

		pStatic->SetWindowText(paddedTitle.GetString());
	}
}

