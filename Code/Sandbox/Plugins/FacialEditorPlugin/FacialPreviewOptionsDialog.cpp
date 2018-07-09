// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FacialPreviewOptionsDialog.h"
#include "FacialPreviewDialog.h"
#include "FacialEdContext.h"

IMPLEMENT_DYNAMIC(CFacialPreviewOptionsDialog, CDialog)

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFacialPreviewOptionsDialog, CDialog)
ON_WM_SIZE()
ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CFacialPreviewOptionsDialog::CFacialPreviewOptionsDialog()
	: m_panel(0),
	m_pModelViewportCE(0),
	m_hAccelerators(0),
	m_pContext(0)
{
}

//////////////////////////////////////////////////////////////////////////
CFacialPreviewOptionsDialog::~CFacialPreviewOptionsDialog()
{
	if (m_panel)
	{
		delete m_panel;
		m_panel = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewOptionsDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_panel->GetSafeHwnd())
	{
		CRect clientRect;
		GetClientRect(&clientRect);
		m_panel->MoveWindow(clientRect);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialPreviewOptionsDialog::OnInitDialog()
{
	BOOL bRes = __super::OnInitDialog();

	m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_FACED_MENU));

	this->m_panel = new CPropertiesPanel(this);
	this->m_panel->AddVars(m_pPreviewDialog->GetVarObject());
	//this->m_panel->AddVars( m_pModelViewportCE->GetVarObject() );
	this->m_panel->ShowWindow(SW_SHOWDEFAULT);

	//this->ShowWindow(SW_SHOWDEFAULT);

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewOptionsDialog::SetViewport(CFacialPreviewDialog* pPreviewDialog)
{
	m_pPreviewDialog = pPreviewDialog;
	m_pModelViewportCE = m_pPreviewDialog->GetViewport();
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialPreviewOptionsDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		return ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);
	return CDialog::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialPreviewOptionsDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CFacialEdContext::MoveToKeyDirection direction = (zDelta < 0 ? CFacialEdContext::MoveToKeyDirectionBackward : CFacialEdContext::MoveToKeyDirectionForward);
	if (m_pContext)
		m_pContext->MoveToFrame(direction);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialPreviewOptionsDialog::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;
}

