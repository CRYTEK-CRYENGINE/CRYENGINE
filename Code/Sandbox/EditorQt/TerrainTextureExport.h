// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_TerrainTextureExport_H__4CAA5295_2647_42FD_8334_359F55EBA777__INCLUDED_)
#define AFX_TerrainTextureExport_H__4CAA5295_2647_42FD_8334_359F55EBA777__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000
// TerrainTextureExport.h : header file
//

#include "Controls/ColorCheckBox.h"

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureExport dialog

class CTerrainTextureExport : public CDialog
{
	// Construction
public:
	CTerrainTextureExport(CWnd* pParent = NULL);   // standard constructor
	~CTerrainTextureExport();

	// Dialog Data
	//{{AFX_DATA(CTerrainTextureExport)
	enum { IDD = IDD_TERRAIN_TEXTURE_EXPORT };
	//}}AFX_DATA

	void ImportExport(bool bIsImport, bool bIsClipboard = false);

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTerrainTextureExport)

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	string BrowseTerrainTexture(bool bIsSave);
		//}}AFX_VIRTUAL

	void DrawPreview(CDC& dc);
	void PreviewToTile(uint32& outX, uint32& outY, uint32 x, uint32 y);

	// Implementation
protected:
	CBitmap               m_bmpLightmap;
	CDC                   m_dcLightmap;
	class CTerrainTexGen* m_pTexGen;
	CImageEx              m_terrain;

	CCustomButton         m_importBtn;
	CCustomButton         m_exportBtn;
	CCustomButton         m_closeBtn;
	CCustomButton         m_changeResolutionBtn;

	int                   m_cx;
	int                   m_cy;

	bool                  m_bSelectMode;
	RECT                  rcSel;

	// Generated message map functions
	//{{AFX_MSG(CTerrainTextureExport)
	afx_msg void OnPaint();
	afx_msg void OnGenerate();
	virtual BOOL OnInitDialog();
	afx_msg void OnExport();
	afx_msg void OnImport();
	afx_msg void OnChangeResolutionBtn();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TerrainTextureExport_H__4CAA5295_2647_42FD_8334_359F55EBA777__INCLUDED_)

