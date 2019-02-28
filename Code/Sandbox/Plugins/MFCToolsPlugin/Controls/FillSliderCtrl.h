// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CFillSliderCtrl_h__
#define __CFillSliderCtrl_h__
#pragma once

#include "SliderCtrlEx.h"

// This notification (Sent with WM_COMMAND) sent when slider changes position.
#include "UserMessageDefines.h"

/////////////////////////////////////////////////////////////////////////////
// CFillSliderCtrl window
class PLUGIN_API CFillSliderCtrl : public CSliderCtrlEx
{
public:
	enum EFillStyle
	{
		// Fill vertically instead of horizontally.
		eFillStyle_Vertical = 1 << 0,

		// Fill with a gradient instead of a solid.
		eFillStyle_Gradient = 1 << 1,

		// Fill the background instead of the slider.
		eFillStyle_Background = 1 << 2,

		// Fill with color hue gradient instead of a solid.
		eFillStyle_ColorHueGradient = 1 << 3,
	};

private:
	DECLARE_DYNCREATE(CFillSliderCtrl);
	// Construction
public:
	typedef Functor1<CFillSliderCtrl*> UpdateCallback;

	CFillSliderCtrl();
	~CFillSliderCtrl();

	void SetFilledLook(bool bFilled);
	void SetFillStyle(uint32 style)                  { m_fillStyle = style; }
	void SetFillColors(COLORREF start, COLORREF end) { m_fillColorStart = start; m_fillColorEnd = end; }

	// Operations
public:
	//! Set current value.
	virtual void SetValue(float val);

	// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	void         NotifyUpdate(bool tracking);
	void         ChangeValue(int sliderPos, bool bTracking);

private:
	void DrawFill(CDC& dc, CRect& rect);

protected:
	bool     m_bFilled;
	uint32   m_fillStyle;
	COLORREF m_fillColorStart;
	COLORREF m_fillColorEnd;
	CPoint   m_mousePos;
};

#endif //__CFillSliderCtrl_h__

