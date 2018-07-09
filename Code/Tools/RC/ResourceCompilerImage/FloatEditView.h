// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLOATEDITVIEW_H__
#define __FLOATEDITVIEW_H__

#include "TinyDocument.h"

class FloatEditView : public CWindowImpl<FloatEditView>, public TinyDocument<float>::Listener
{
public:
	DECLARE_WND_SUPERCLASS(NULL, "EDIT");

	FloatEditView();
	~FloatEditView();
	void SetDocument(TinyDocument<float>* pDocument);
	void ClearDocument();
	BOOL SubclassWindow(HWND hWnd);
	void UpdateText();

	BEGIN_MSG_MAP(SliderView)
		REFLECTED_COMMAND_CODE_HANDLER(EN_KILLFOCUS, OnKillFocus);
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown);
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

private:
	virtual void OnTinyDocumentChanged(TinyDocument<float>* pDocument);
	LRESULT OnKillFocus(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	TinyDocument<float>* pDocument;
};

#endif //__FLOATEDITVIEW_H__
