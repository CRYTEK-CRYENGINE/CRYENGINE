// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SLIDERVIEW_H__
#define __SLIDERVIEW_H__

#include "TinyDocument.h"

class SliderView : public CWindowImpl<SliderView>, public TinyDocument<float>::Listener
{
public:
	DECLARE_WND_SUPERCLASS(NULL, TRACKBAR_CLASS);

	SliderView();
	~SliderView();
	void SetDocument(TinyDocument<float>* pDocument);
	void ClearDocument();
	BOOL SubclassWindow(HWND hWnd);
	void UpdatePosition();

	BEGIN_MSG_MAP(SliderView)
		MESSAGE_HANDLER(OCM_HSCROLL, OnScroll)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

private:
	virtual void OnTinyDocumentChanged(TinyDocument<float>* pDocument);

	LRESULT OnScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	TinyDocument<float>* pDocument;
};

#endif //__SLIDERVIEW_H__
