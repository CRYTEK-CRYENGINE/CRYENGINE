// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TerrainFormulaDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CTerrainFormulaDlg dialog

CTerrainFormulaDlg::CTerrainFormulaDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainFormulaDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainFormulaDlg)
	m_dParam1 = 0.0;
	m_dParam2 = 0.0;
	m_dParam3 = 0.0;
	//}}AFX_DATA_INIT
}

void CTerrainFormulaDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainFormulaDlg)
	DDX_Text(pDX, IDC_EDIT_FORMULA_PARAM1, m_dParam1);
	DDV_MinMaxDouble(pDX, m_dParam1, 0., 10.);
	DDX_Text(pDX, IDC_EDIT_FORMULA_PARAM2, m_dParam2);
	DDV_MinMaxDouble(pDX, m_dParam2, 1., 5000.);
	DDX_Text(pDX, IDC_EDIT_FORMULA_PARAM3, m_dParam3);
	DDV_MinMaxDouble(pDX, m_dParam3, 0., 128.);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTerrainFormulaDlg, CDialog)
//{{AFX_MSG_MAP(CTerrainFormulaDlg)
// NOTE: the ClassWizard will add message map macros here
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainFormulaDlg message handlers

