// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CDragNDropListBox : public CListBox
{
#define TID_SCROLLDOWN  100
#define TID_SCROLLUP    101

	DECLARE_DYNAMIC(CDragNDropListBox)

public:
	typedef Functor2<int, int> DragNDropCallback;

	CDragNDropListBox(const DragNDropCallback &cb); // cb can be nullptr
	virtual ~CDragNDropListBox() {}

	void OnDrop(int curIdx, int newIdx);

protected:
	void DrawLine(int idx, bool isActiveLine);
	void UpdateSelection(int hoverIdx);

	void Scroll(CPoint Point, CRect ClientRect);
	void StopScrolling();
	void OnTimer(UINT nIDEvent);

	void DropDraggedItem();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);

protected:
	bool m_isDragging;
	int m_itemPrevIdx;
	int m_itemNextIdx;
	int m_Interval;

	DragNDropCallback m_dropCb;
};

