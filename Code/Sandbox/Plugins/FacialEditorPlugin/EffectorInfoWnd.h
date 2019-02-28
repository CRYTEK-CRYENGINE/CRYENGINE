// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EffectorInfoWnd_h__
#define __EffectorInfoWnd_h__
#pragma once

#include "Dialogs/ToolbarDialog.h"
#include "FacialEdContext.h"
#include "Controls/SliderCtrlEx.h"
#include "Controls/PropertyCtrlEx.h"
#include "Controls/ListCtrlEx.h"

class CSplineCtrl;
//////////////////////////////////////////////////////////////////////////
// Window that holds effector info.
//////////////////////////////////////////////////////////////////////////
class CEffectorInfoWnd : public CToolbarDialog, public IFacialEdListener, public IEditorNotifyListener
{
public:
	CEffectorInfoWnd();
	~CEffectorInfoWnd();
	void Create(CWnd* pParent);
	void SetContext(CFacialEdContext* pContext);
	void ResetWeight() { SetWeight(1.0f, 0); };

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSplineRClick(UINT nID, NMHDR* pNMHDR, LRESULT* lpResult);
	afx_msg void OnBeforeSplineChange(UINT nID, NMHDR* pNMHDR, LRESULT* lpResult);
	afx_msg void OnSplineChange(UINT nID, NMHDR* pNMHDR, LRESULT* lpResult);

	afx_msg void OnPlayAnim();
	afx_msg void OnUpdatePlayAnim(CCmdUI* pCmdUI);
	afx_msg void OnPlayAnimFrom0();
	afx_msg void OnUpdatePlayAnimFrom0(CCmdUI* pCmdUI);
	afx_msg void OnGotoMinus1();
	afx_msg void OnGoto0();
	afx_msg void OnGoto1();
	afx_msg void OnChangeCurrentTime();
	afx_msg void OnMeasureItemSplines(LPMEASUREITEMSTRUCT pMeasureItem);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);

	void         ReloadCtrls();
	void         SetWeight(float fWeight, float fBalance = 0.0f);
	void         ClearCtrls();
	void         RecalcLayout();
	void         OnSelectEffector(IFacialEffector* pEffector);

	//////////////////////////////////////////////////////////////////////////
	// IfacialEdListener
	//////////////////////////////////////////////////////////////////////////
	virtual void OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	//////////////////////////////////////////////////////////////////////////

private:
	CXTPToolBar           m_wndToolBar;
	//CXTPToolBar m_wndToolBar;
	CXTPTaskPanel         m_wndTaskPanel;
	CImageList            m_imageList;

	CSliderCtrlCustomDraw m_weightSlider;
	CSliderCtrlCustomDraw m_balanceSlider;
	CStatic               m_textMinus100, m_textPlus100, m_text0;
	bool                  m_bAnimation;
	bool                  m_bPlayFrom0;
	bool                  m_bIgnoreScroll;
	float                 m_fCurrWeight;
	float                 m_fCurrBalance;
	int                   m_nSplineHeight;

	CXTPControlLabel*     m_pCurrPosCtrl;
	CXTPControlLabel*     m_pExpName;

	CFacialEdContext*     m_pContext;
	struct ControllerInfo
	{
		IFacialEffCtrl* pCtrl;
		CSplineCtrl*    pSplineCtrl;
		CToolbarDialog* pDlg;
	};
	std::vector<ControllerInfo> m_controllers;

	//CControlsListBox m_wndSplines;
	CPropertyCtrlEx                    m_wndProperties;
	class CFacialAttachmentEffectorUI* m_pAttachEffectorUI;
};

#endif // __EffectorInfoWnd_h__

