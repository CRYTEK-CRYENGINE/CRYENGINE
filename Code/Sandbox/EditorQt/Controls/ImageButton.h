// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Handle Windows XP theming in a backwords compatible way
typedef HTHEME (WINAPI *  PFNOpenThemeData)(HWND, LPCWSTR);
typedef HRESULT (WINAPI * PFNDrawThemeBackground)(HTHEME, HDC, int, int, const RECT*, const RECT*);
typedef HRESULT (WINAPI * PFNDrawThemeText)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, DWORD, const RECT*);
typedef HRESULT (WINAPI * PFNCloseThemeData)(HTHEME);

/////////////////////////////////////////////////////////////////////////////
// CImageButton window

class CImageButton : public CButton
{
	DECLARE_DYNAMIC(CImageButton)

protected:
	HMODULE                m_hUxThemeLib;
	PFNOpenThemeData       OpenThemeData;
	PFNDrawThemeBackground DrawThemeBackground;
	PFNDrawThemeText       DrawThemeText;
	PFNCloseThemeData      CloseThemeData;

	// Construction
public:
	enum eType
	{
		eT_NONE,
		eT_IMAGELIST
	};
	enum eImageAlignment
	{
		eIA_LEFT,
		eIA_RIGHT
	};
	CImageButton();

	// Attributes
public:

	// Operations
public:
	void SetImage(CImageList& imageList, uint32 imageIndex, eImageAlignment alignment = eIA_LEFT);

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImageButton)
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	//}}AFX_VIRTUAL

	afx_msg void OnNMThemeChanged(NMHDR* pNMHDR, LRESULT* pResult);
	LRESULT      OnMouseLeave(WPARAM wParam, LPARAM lParam);

protected:

	// Implementation
public:
	virtual ~CImageButton();

	// Generated message map functions
protected:
	//{{AFX_MSG(CImageButton)
	// NOTE - the ClassWizard will add and remove member functions here.
	afx_msg void    OnKillFocus(CWnd* pNewWnd);
	afx_msg void    OnSetFocus(CWnd* pOldWnd);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	//}}AFX_MSG

	bool            m_hasFocus;
	bool            m_mouseOver;

	eImageAlignment m_eAlignment;

	int             m_iWidth;
	int             m_iHeight;

	eType           m_imageType;
	CImageList*     m_pImageList;
	uint32          m_imageListIndex;

	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

