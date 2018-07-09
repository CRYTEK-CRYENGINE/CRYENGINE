// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FacialSlidersCtrl_h__
#define __FacialSlidersCtrl_h__
#pragma once

#include "FacialEdContext.h"
#include "Controls/ListCtrlEx.h"
#include "Controls/SliderCtrlEx.h"
#include "Controls/NumberCtrl.h"

class CFacialEdContext;

struct IFacialEffector;

/////////////////////////////////////////////////////////////////////////////
// CFillSliderCtrl window
class CFacialSlidersCtrl : public CListCtrlEx, public IFacialEdListener
{
	DECLARE_DYNCREATE(CFacialSlidersCtrl);
	// Construction
public:
	CFacialSlidersCtrl();
	~CFacialSlidersCtrl();

	void SetContext(CFacialEdContext* pContext);
	void SetShowExpressions(bool bShowExpressions);
	void OnClearAll();

	void SetMorphWeight(IFacialEffector* pEffector, float fWeight);

	// Operations
public:
	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID = -1);

	// Overrite from XT
	virtual void SetRowColor(int iRow, COLORREF crText, COLORREF crBack);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	afx_msg void OnRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRClickSlider(UINT nID, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);

	void         CreateSliders();
	void         ClearSliders();
	void         RecalcLayout();
	void         OnSliderChanged(int nSlider);
	void         UpdateValuesFromSelected();
	void         UpdateSliderUI(int nSlider);
	void         CreateSlider(int nIndex, IFacialEffector* pEffector);

	virtual void OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);

	void         OnBalanceChanged(CNumberCtrl* numberCtrl);

private:
	CBrush m_bkBrush;
	int    m_nSliderWidth;
	int    m_nSliderHeight;
	int    m_nTextWidth;

	bool   m_bCreated;
	bool   m_bIgnoreSliderChange;

	bool   m_bShowExpressions;

	struct SSliderInfo
	{
		bool                   bEnabled;
		COLORREF               rowColor;
		CRect                  rc;
		CSliderCtrlCustomDraw* pSlider;
		CNumberCtrl*           pBalance;
		//CWnd *pNumberCtrl;
		IFacialEffector*       pEffector;

		SSliderInfo() { pSlider = 0; pEffector = 0; bEnabled = false; rowColor = 0; }
	};
	typedef std::vector<SSliderInfo> SliderContainer;
	SliderContainer   m_sliders;
	CFacialEdContext* m_pContext;
	CImageList        m_imageList;

	int               m_nDraggedItem;
	CImageList*       m_pDragImage;
	bool              m_bLDragging;
};

#endif // __FacialSlidersCtrl_h__

