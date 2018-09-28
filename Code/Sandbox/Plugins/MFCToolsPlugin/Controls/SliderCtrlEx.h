// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SliderCtrlEx_h__
#define __SliderCtrlEx_h__
#pragma once

#include <CryCore/functor.h>

//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CSliderCtrlEx : public CSliderCtrl
{
public:
	typedef Functor1<CSliderCtrlEx*> UpdateCallback;

	DECLARE_DYNAMIC(CSliderCtrlEx)
	CSliderCtrlEx();

	//////////////////////////////////////////////////////////////////////////
	virtual void    EnableUndo(const CString& undoText);
	virtual void    SetUpdateCallback(const UpdateCallback& cb) { m_updateCallback = cb; };

	virtual void    SetRangeInternal(const mpfloat& min, const mpfloat& max, const mpfloat& step = 0);
	virtual void    SetValue(const mpfloat& val);
	virtual const mpfloat& GetValue() const;
	virtual CString GetValueAsString() const;

protected:
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);

	DECLARE_MESSAGE_MAP()

	bool SetThumb(const CPoint& pt);
	void PostMessageToParent(const int nTBCode);

protected:
	bool           m_bDragging;
	bool           m_bDragChanged;

	mpfloat        m_min, m_max;
	mutable mpfloat m_value;
	mpfloat        m_lastUpdateValue;
	CPoint         m_mousePos;

	bool           m_noNotify;
	bool           m_integer;

	bool           m_bUndoEnabled;
	bool           m_bUndoStarted;
	bool           m_bDragged;
	CString        m_undoText;
	bool           m_bLocked;
	bool           m_bInNotifyCallback;

	UpdateCallback m_updateCallback;
};

//////////////////////////////////////////////////////////////////////////
class PLUGIN_API CSliderCtrlCustomDraw : public CSliderCtrlEx
{
public:
	DECLARE_DYNAMIC(CSliderCtrlCustomDraw)

	CSliderCtrlCustomDraw() { m_tickFreq = 1; m_selMin = m_selMax = 0; }
	void SetTicFreq(int nFreq) { m_tickFreq = nFreq; };
	void SetSelection(int nMin, int nMax);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);

	void         DrawTicks(CDC* pDC);
	void         DrawTick(CDC* pDC, int x, bool bMajor = false);
	int          ValueToPos(int n);

private:
	int   m_selMin, m_selMax;
	int   m_tickFreq;
	CRect m_channelRc;
};

#endif // __SliderCtrlEx_h__

