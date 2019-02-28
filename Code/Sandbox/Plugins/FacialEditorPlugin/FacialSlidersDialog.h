// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FacialSlidersDialog_h__
#define __FacialSlidersDialog_h__
#pragma once

#include "FacialSlidersCtrl.h"

//////////////////////////////////////////////////////////////////////////
class CFacialSlidersDialog : public CDialog
{
	DECLARE_DYNAMIC(CFacialSlidersDialog)
public:
	static void RegisterViewClass();

	CFacialSlidersDialog();
	~CFacialSlidersDialog();

	void SetContext(CFacialEdContext* pContext);

	void SetMorphWeight(IFacialEffector* pEffector, float fWeight);
	void ClearAllMorphs();

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK()     {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult);

	afx_msg void OnClearAllSliders();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	void         RecalcLayout();

private:
	CXTPToolBar        m_wndToolBar;
	CFacialSlidersCtrl m_slidersCtrl[2];
	CTabCtrl           m_tabCtrl;

	CFacialEdContext*  m_pContext;
	HACCEL             m_hAccelerators;
};

#endif // __FacialSlidersDialog_h__

