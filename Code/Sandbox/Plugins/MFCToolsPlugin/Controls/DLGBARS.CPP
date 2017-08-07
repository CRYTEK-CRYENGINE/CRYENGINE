/*****************************************************************************
  DLGBARS.CPP

  Purpose: 
  	Interface for CDlgToolBar, a special type of CToolBar which does not
  	expect a parent frame window to be available, and CDlgStatusBar, which
  	does the same for CStatusBars.  This allows the control bars
  	to be used in applications where the main window is a dialog bar.

  Functions:
    CDlgToolBar::CDlgToolBar()          -- constructor
    CDlgToolBar::~CDlgToolBar()         -- destructor
    CDlgToolBar::OnIdleUpdateCmdUI()    -- WM_IDLEUPDATECMDUI handler
    
    CDlgStatusBar::CDlgStatusBar()      -- constructor
    CDlgStatusBar::~CDlgStatusBar()     -- destructor
    CDlgStatusBar::OnIdleUpdateCmdUI()	-- WM_IDLEUPDATECMDUI handler

  Development Team:
	Mary Kirtland
  Ported to 32-bit by:
    Mike Hedley	
  Written by Microsoft Product Support Services, Premier ISV Support
  Copyright (c) 1996 Microsoft Corporation. All rights reserved.
\****************************************************************************/

#include "stdafx.h" 
#include "dlgbars.h"

#define TOOLBAR_TRANSPARENT_COLOR RGB(0xC0, 0xC0, 0xC0)

/////////////////////////////////////////////////////////////////////////////
// CDlgToolBar

BEGIN_MESSAGE_MAP(CDlgToolBar, CToolBar)
	//{{AFX_MSG_MAP(CDlgToolBar)
	ON_MESSAGE(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
                 
/////////////////////////////////////////////////////////////////////////////
// CDlgToolBar Construction/Destruction

CDlgToolBar::CDlgToolBar()
{
}

CDlgToolBar::~CDlgToolBar()
{
} 

/////////////////////////////////////////////////////////////////////////////
// CDlgToolBar::OnIdleUpdateCmdUI
//		OnIdleUpdateCmdUI handles the WM_IDLEUPDATECMDUI message, which is 
//      used to update the status of user-interface elements within the MFC 
//		framework.
//
// 		We have to get a little tricky here: CToolBar::OnUpdateCmdUI 
//      expects a CFrameWnd pointer as its first parameter.  However, it
//      doesn't do anything but pass the parameter on to another function
//      which only requires a CCmdTarget pointer.  We can get a CWnd pointer
//      to the parent window, which is a CCmdTarget, but may not be a 
//   	CFrameWnd.  So, to make CToolBar::OnUpdateCmdUI happy, we will call
//      our CWnd pointer a CFrameWnd pointer temporarily.  	

LRESULT CDlgToolBar::OnIdleUpdateCmdUI(WPARAM wParam, LPARAM) 
{
	if (IsWindowVisible()) 
	{
		CFrameWnd *pParent = (CFrameWnd *)GetParent();
		if (pParent)
			OnUpdateCmdUI(pParent, (BOOL)wParam);
	}
	return 0L;
}

//////////////////////////////////////////////////////////////////////////
BOOL CDlgToolBar::LoadToolBar24( UINT nIDResource,int nBtnWidth )
{
	return LoadToolBarDepth(nIDResource,nBtnWidth, ILC_COLOR24);
}
//////////////////////////////////////////////////////////////////////////
BOOL CDlgToolBar::LoadToolBarDepth( UINT nIDResource,int nBtnWidth, UINT colorDepth )
{
	CBitmap toolbarBitmap;
	CImageList toolbarImageList;

	// This will potentially assert in MFC in debug mode, because LoadToolBar can't 
	// load 24 bit images. We deal with this below, so suppress the assert.
	const int oldMode = _CrtSetReportMode(_CRT_ASSERT, 0);
	BOOL bRes = LoadToolBar(nIDResource);
	VERIFY(bRes);
	_CrtSetReportMode(_CRT_ASSERT, oldMode);

	/*
	//////////////////////////////////////////////////////////////////////////
	// Use 24Bit toolbars.
	//////////////////////////////////////////////////////////////////////////
	toolbarBitmap.LoadBitmap(IDR_MAINFRAME);
	toolbarImageList.Create(16, 15, ILC_COLORDDB|ILC_MASK, 1, 1);
	toolbarImageList.Add(&toolbarBitmap,TOOLBAR_TRANSPARENT_COLOR);
	SendMessage(TB_SETIMAGELIST, 0, (LPARAM)toolbarImageList.m_hImageList);
	*/

	CImageList imageList;
	CBitmap bitmap;
	BITMAP bmBitmap;

	if (!bitmap.Attach(LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(nIDResource),IMAGE_BITMAP, 0, 0,LR_DEFAULTSIZE|LR_CREATEDIBSECTION)))
		return FALSE;

	if (!bitmap.GetBitmap(&bmBitmap))
		return FALSE;

	CSize		cSize(bmBitmap.bmWidth, bmBitmap.bmHeight); 
	RGBTRIPLE*	rgb		= (RGBTRIPLE*)(bmBitmap.bmBits);

	int	nCount	= cSize.cx/nBtnWidth;
	if (!imageList.Create(nBtnWidth, cSize.cy, colorDepth|ILC_MASK, nCount, 0))
		return FALSE;

	if (imageList.Add(&bitmap,TOOLBAR_TRANSPARENT_COLOR) == -1)
		return FALSE;

	SendMessage(TB_SETIMAGELIST, 0, (LPARAM)imageList.m_hImageList);
	imageList.Detach(); 
	bitmap.Detach();
	return bRes;
}

/////////////////////////////////////////////////////////////////////////////
// CDlgStatusBar

BEGIN_MESSAGE_MAP(CDlgStatusBar, CStatusBar)
	//{{AFX_MSG_MAP(CDlgStatusBar)
	ON_MESSAGE(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
                 
/////////////////////////////////////////////////////////////////////////////
// CDlgStatusBar Construction/Destruction

CDlgStatusBar::CDlgStatusBar()
{
}

CDlgStatusBar::~CDlgStatusBar()
{
} 

/////////////////////////////////////////////////////////////////////////////
// CDlgStatusBar::OnIdleUpdateCmdUI
//		OnIdleUpdateCmdUI handles the WM_IDLEUPDATECMDUI message, which is 
//      used to update the status of user-interface elements within the MFC 
//		framework.
//
// 		We have to get a little tricky here: CStatusBar::OnUpdateCmdUI 
//      expects a CFrameWnd pointer as its first parameter.  However, it
//      doesn't do anything but pass the parameter on to another function
//      which only requires a CCmdTarget pointer.  We can get a CWnd pointer
//      to the parent window, which is a CCmdTarget, but may not be a 
//   	CFrameWnd.  So, to make CStatusBar::OnUpdateCmdUI happy, we will call
//      our CWnd pointer a CFrameWnd pointer temporarily.  	

LRESULT CDlgStatusBar::OnIdleUpdateCmdUI(WPARAM wParam, LPARAM) 
{
	if (IsWindowVisible()) 
	{
		CFrameWnd *pParent = (CFrameWnd *)GetParent();
		if (pParent)
			OnUpdateCmdUI(pParent, (BOOL)wParam);
	}
	return 0L;
}
