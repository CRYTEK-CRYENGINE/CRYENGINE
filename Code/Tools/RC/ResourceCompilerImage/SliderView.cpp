// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SliderView.h"

void SliderView::OnTinyDocumentChanged(TinyDocument<float>* pDocument)
{
	this->UpdatePosition();
}

void SliderView::UpdatePosition()
{
	float fValue = pDocument->GetValue();
	fValue -= pDocument->GetMin();
	fValue /= pDocument->GetMax() - pDocument->GetMin();
	this->SendMessage(TBM_SETPOS, TRUE, int(fValue * 1000.0f));
}

LRESULT SliderView::OnScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;

	if (this->pDocument != 0)
	{
		float fValue = float(this->SendMessage(TBM_GETPOS, 0, 0)) / 1000.0f;
		fValue *= this->pDocument->GetMax() - this->pDocument->GetMin();
		fValue += this->pDocument->GetMin();
		this->pDocument->SetValue(fValue);

		bHandled = TRUE;
	}

	// Is this the correct return value?
	return 0;
}

SliderView::SliderView()
:	pDocument(0)
{
}

SliderView::~SliderView()
{
	this->ClearDocument();
}

void SliderView::SetDocument(TinyDocument<float>* pDocument)
{
	this->ClearDocument();

	if (pDocument != 0)
	{
		this->pDocument = pDocument;
		this->pDocument->AddListener(this);
	}
}

void SliderView::ClearDocument()
{
	if (this->pDocument != 0)
	{
		this->pDocument->RemoveListener(this);
		this->pDocument = 0;
	}
}

BOOL SliderView::SubclassWindow(HWND hWnd)
{
	// Call the base class implementation.
	BOOL bSubclassSuccessful = CWindowImpl<SliderView>::SubclassWindow(hWnd);

	if (bSubclassSuccessful)
	{
		// Set the maximum value.
		this->SendMessage(
			TBM_SETRANGE,    // message ID
			(WPARAM) TRUE,          // = (WPARAM) (BOOL) fRedraw
			(LPARAM) MAKELONG(0, 1000)              // = (LPARAM) MAKELONG (lMinimum, lMaximum)
			);
	}

	return bSubclassSuccessful;
}
