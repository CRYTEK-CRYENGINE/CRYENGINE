// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SliderView.h"
#include "FloatEditView.h"
#include "CheckBoxView.h"
#include "ImageProperties.h"
#include <set>

class CBumpProperties;


class CUIBumpmapPanel
{
public:

	// constructor
	CUIBumpmapPanel();

	void InitDialog(HWND hWndParent, const bool bShowBumpmapFileName);

	void GetDataFromDialog(CBumpProperties& rBumpProp);

	void SetDataToDialog(const CBumpProperties& rBumpProp, const bool bInitalUpdate);

	HWND GetHWND() const;

	void ChooseAddBump(HWND hParentWnd);

	class CImageUserDialog* m_pDlg;

	TinyDocument<float> m_bumpStrengthValue;
	TinyDocument<float> m_bumpBlurValue;
	TinyDocument<bool>  m_bumpInvertValue;

private: // ---------------------------------------------------------------------------------------

	HWND          m_hTab_Normalmapgen;      // window handle
	SliderView    m_bumpStrengthSlider;
	FloatEditView m_bumpStrengthEdit;
	SliderView    m_bumpBlurSlider;
	FloatEditView m_bumpBlurEdit;
	CheckBoxView  m_bumpInvertCheckBox;

	int m_GenIndexToControl[eWindowFunction_Num];
	int m_GenControlToIndex[eWindowFunction_Num];

	void UpdateBumpStrength(const CBumpProperties& rBumpProp);
	void UpdateBumpBlur(const CBumpProperties& rBumpProp);
	void UpdateBumpInvert(const CBumpProperties& rBumpProp);

	static INT_PTR CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT ReflectNotifications(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled, const std::set<int>& reflectIDs);
	std::set<int> reflectIDs;
};
