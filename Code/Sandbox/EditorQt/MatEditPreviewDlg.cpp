// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MatEditPreviewDlg.h"
#include "Material/MaterialManager.h"

/////////////////////////////////////////////////////////////////////////////
// CMatEditPreviewDlg dialog

CMatEditPreviewDlg::CMatEditPreviewDlg(const char* title, CWnd* pParent)
	: CToolbarDialog(CMatEditPreviewDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMatEditPreviewDlg)
	//}}AFX_DATA_INIT

	GetIEditorImpl()->GetMaterialManager()->AddListener(this);

	BOOL b = Create(IDD_MATEDITPREVIEWDLG, pParent);

	OnPreviewSphere();
}

CMatEditPreviewDlg::~CMatEditPreviewDlg()
{
	GetIEditorImpl()->GetMaterialManager()->RemoveListener(this);
}

/*
   void CMatEditPreviewDlg::DoDataExchange(CDataExchange* pDX)
   {
   __super::DoDataExchange(pDX);
   //{{AFX_DATA_MAP(CMatEditPreviewDlg)
   //}}AFX_DATA_MAP
   }
 */

BEGIN_MESSAGE_MAP(CMatEditPreviewDlg, CToolbarDialog)
ON_COMMAND(ID_MATPR_SPHERE, OnPreviewSphere)
ON_COMMAND(ID_MATPR_BOX, OnPreviewBox)
ON_COMMAND(ID_MATPR_TEAPOT, OnPreviewTeapot)
ON_COMMAND(ID_MATPR_CUSTOM, OnPreviewCustom)
ON_COMMAND(ID_MATPR_UPDATEALWAYS, OnUpdateAlways)
ON_UPDATE_COMMAND_UI(ID_MATPR_UPDATEALWAYS, OnUpdateAlwaysUpdateUI)
ON_WM_CLOSE()
ON_WM_SIZE()
ON_WM_DESTROY()
ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::OnPreviewSphere()
{
	m_previewCtrl.LoadFile("%EDITOR%/Objects/MtlSphere.cgf");
}

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::OnPreviewBox()
{
	m_previewCtrl.LoadFile("%EDITOR%/Objects/MtlBox.cgf");
}

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::OnPreviewTeapot()
{
	m_previewCtrl.LoadFile("%EDITOR%/Objects/MtlTeapot.cgf");
}

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::OnPreviewPlane()
{
	m_previewCtrl.LoadFile("%EDITOR%/Objects/MtlPlane.cgf");
}

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::OnPreviewCustom()
{
	string fullFileName = "";
	if (CFileUtil::SelectFile("Objects (*.cgf)|*.cgf|All files (*.*)|*.*", "", fullFileName))
	{
		m_previewCtrl.LoadFile(fullFileName);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::OnUpdateAlways()
{
	m_previewCtrl.EnableUpdate(!m_previewCtrl.IsUpdateEnabled());
}

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::OnUpdateAlwaysUpdateUI(CCmdUI* pCmdUI)
{
	if (m_previewCtrl.IsUpdateEnabled())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CMatEditPreviewDlg::PostNcDestroy()
{
	delete this;
}

/////////////////////////////////////////////////////////////////////////////
// CMatEditPreviewDlg message handlers

BOOL CMatEditPreviewDlg::OnInitDialog()
{
	__super::OnInitDialog();

	CRect rc;
	GetClientRect(rc);

	m_previewCtrl.Create(this, rc, WS_CHILD | WS_VISIBLE);
	m_previewCtrl.SetGrid(true);
	m_previewCtrl.EnableUpdate(true);

	RecalcLayout();

	OnPreviewPlane();
	m_previewCtrl.SetMaterial(GetIEditorImpl()->GetMaterialManager()->GetCurrentMaterial());
	m_previewCtrl.Update();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CMatEditPreviewDlg::OnClose()
{
	__super::OnClose();
	DestroyWindow();
}

void CMatEditPreviewDlg::OnDestroy()
{
	__super::OnDestroy();
}

void CMatEditPreviewDlg::OnSize(UINT nType, int cx, int cy)
{
	CRect rc;
	if (m_previewCtrl.m_hWnd)
	{
		GetClientRect(rc);
		m_previewCtrl.MoveWindow(rc, FALSE);
		m_previewCtrl.Invalidate();
	}

	//RecalcLayout();

	/*
	   if (m_previewCtrl.m_hWnd)
	   {
	   GetClientRect(rc);
	   m_previewCtrl.MoveWindow(&rc, FALSE);
	   }
	 */
	CToolbarDialog::OnSize(nType, cx, cy);
}

void CMatEditPreviewDlg::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	CMaterial* pMtl = (CMaterial*)pItem;
	switch (event)
	{
	case EDB_ITEM_EVENT_SELECTED:
	case EDB_ITEM_EVENT_ADD:
	case EDB_ITEM_EVENT_CHANGED:
		m_previewCtrl.SetMaterial(GetIEditorImpl()->GetMaterialManager()->GetCurrentMaterial());
		m_previewCtrl.Update();
		break;
	case EDB_ITEM_EVENT_DELETE:
		m_previewCtrl.SetMaterial(0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT CMatEditPreviewDlg::OnKickIdle(WPARAM wParam, LPARAM)
{
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, 1);
	return 0;
}

