// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FloatEditView.h"

void FloatEditView::OnTinyDocumentChanged(TinyDocument<float>* pDocument)
{
	this->UpdateText();
}

FloatEditView::FloatEditView()
:	pDocument(0)
{
}

FloatEditView::~FloatEditView()
{
	this->ClearDocument();
}

void FloatEditView::SetDocument(TinyDocument<float>* pDocument)
{
	this->ClearDocument();

	if (pDocument != 0)
	{
		this->pDocument = pDocument;
		this->pDocument->AddListener(this);
		this->UpdateText();
	}
}

void FloatEditView::ClearDocument()
{
	if (this->pDocument != 0)
	{
		this->pDocument->RemoveListener(this);
		this->pDocument = 0;
	}
}

LRESULT FloatEditView::OnKillFocus(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (this->pDocument != 0)
	{
		char szText[1024];
		this->GetWindowText(szText, sizeof(szText));

		float fValue = 0;
		if (sscanf(szText,"%f",&fValue) == 1)
		{
			if (fValue < this->pDocument->GetMin())
				fValue = this->pDocument->GetMin();
			if (fValue > this->pDocument->GetMax())
				fValue = this->pDocument->GetMax();
			this->pDocument->SetValue(fValue);
		}
		bHandled = TRUE;
	}

	return 0;
}

LRESULT FloatEditView::OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	// Check whether we are interested in this key.
	switch (wParam)
	{
	case VK_TAB:
	case VK_RETURN:
		this->GetTopLevelParent().SetFocus();
		bHandled = TRUE;
		break;
	default:
		bHandled = FALSE;
		break;
	}
	return 0;
}

BOOL FloatEditView::SubclassWindow(HWND hWnd)
{
	// Call the base class implementation.
	BOOL bSubclassSuccessful = CWindowImpl<FloatEditView>::SubclassWindow(hWnd);

	if (bSubclassSuccessful)
	{
		// Set the text.
		this->UpdateText();
	}

	return bSubclassSuccessful;
}

void FloatEditView::UpdateText()
{
	if (this->m_hWnd != 0 && this->pDocument != 0)
	{
		// Update the text of the window.
		char str[128];
		cry_sprintf( str,"%g",this->pDocument->GetValue() );
		this->SetWindowText( str );
	}
}
