// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_TERRAINFORMULADLG_H__03882873_87CD_4F37_B976_27099322837A__INCLUDED_)
#define AFX_TERRAINFORMULADLG_H__03882873_87CD_4F37_B976_27099322837A__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000
// TerrainFormulaDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTerrainFormulaDlg dialog

class CTerrainFormulaDlg : public CDialog
{
	// Construction
public:
	CTerrainFormulaDlg(CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	//{{AFX_DATA(CTerrainFormulaDlg)
	enum { IDD = IDD_TERRAIN_FORMULA };
	double m_dParam1;
	double m_dParam2;
	double m_dParam3;
	//}}AFX_DATA

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTerrainFormulaDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTerrainFormulaDlg)
	// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TERRAINFORMULADLG_H__03882873_87CD_4F37_B976_27099322837A__INCLUDED_)

