// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CHECKBOXVIEW_H__
#define __CHECKBOXVIEW_H__

#include "TinyDocument.h"

class CheckBoxView : public CWindowImpl<CheckBoxView>, public TinyDocument<bool>::Listener
{
public:
	DECLARE_WND_SUPERCLASS(NULL, "Button");

	CheckBoxView();
	~CheckBoxView();
	void SetDocument(TinyDocument<bool>* pDocument);
	void ClearDocument();
	BOOL SubclassWindow(HWND hWnd);
	void UpdateView();

	BEGIN_MSG_MAP(SliderView)
		REFLECTED_COMMAND_CODE_HANDLER(BN_CLICKED, OnChange);
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown);
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

private:
	virtual void OnTinyDocumentChanged(TinyDocument<bool>* pDocument);
	LRESULT OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	TinyDocument<bool>* pDocument;
};

#endif //__CHECKBOXVIEW_H__
