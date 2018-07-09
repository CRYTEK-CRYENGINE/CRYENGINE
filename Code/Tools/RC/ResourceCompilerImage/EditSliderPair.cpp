// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EditSliderPair.h"

void CEditSliderPair::SubclassWindows(HWND hwndSlider, HWND hwndEdit)
{
	m_slider.SetDocument(&m_value);
	m_slider.SubclassWindow(hwndSlider);
	m_slider.UpdatePosition();
	m_edit.SetDocument(&m_value);
	m_edit.SubclassWindow(hwndEdit);
	m_edit.UpdateText();
}

void CEditSliderPair::SetValue(float fValue)
{
	m_value.SetValue(fValue);
}

float CEditSliderPair::GetValue() const
{
	return m_value.GetValue();
}

void CEditSliderPair::SetRange(float fMin, float fMax)
{
	m_value.SetMin(fMin);
	m_value.SetMax(fMax);
}

TinyDocument<float>& CEditSliderPair::GetDocument()
{
	return m_value;
}
