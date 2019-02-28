// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UIBumpmapPanel.h"       // CUIBumpmapPanel
#include "resource.h"
#include "ImageProperties.h"      // CBumpProperties
#include <Commdlg.h>              // OPENFILENAME
#include <commctrl.h>             // TBM_SETRANGEMAX, TBM_SETRANGEMIN
#include "ImageUserDialog.h"

#include <windowsx.h>
#undef SubclassWindow

CUIBumpmapPanel::CUIBumpmapPanel() :m_hTab_Normalmapgen(0)
{
	m_pDlg = NULL;
	// Reflect slider bump notifications back to the control.
	this->reflectIDs.insert(IDC_NM_FILTERTYPE);
	this->reflectIDs.insert(IDC_NM_SLIDERBUMPBLUR);
	this->reflectIDs.insert(IDC_EDIT_BUMPBLUR);
	this->reflectIDs.insert(IDC_NM_SLIDERBUMPSTRENGTH);
	this->reflectIDs.insert(IDC_EDIT_BUMPSTRENGTH);
	this->reflectIDs.insert(IDC_NM_BUMPINVERT);

	memset(&m_GenIndexToControl, 0, sizeof(m_GenIndexToControl));
	memset(&m_GenControlToIndex, 0, sizeof(m_GenControlToIndex));
}

void CUIBumpmapPanel::InitDialog( HWND hWndParent, const bool bShowBumpmapFileName )
{
	// Initialise the values.
	m_bumpBlurValue.SetValue(0.0f);
	m_bumpBlurValue.SetMin(0.0f);
	m_bumpBlurValue.SetMax(10.0f);
	m_bumpStrengthValue.SetValue(0.0f);
	m_bumpStrengthValue.SetMin(0.0f);
	m_bumpStrengthValue.SetMax(15.0f);
	m_bumpInvertValue.SetValue(false);

	HWND hwnd = GetDlgItem(hWndParent, IDC_PROPTAB);
	assert(hwnd);
	
	RECT rect = { 0, 0, 1, 1 };
	TabCtrl_AdjustRect(hwnd, TRUE, &rect);

	m_hTab_Normalmapgen = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_NORMALMAPGEN), hwnd, WndProc, reinterpret_cast<LPARAM>(this));
	assert(m_hTab_Normalmapgen);
	SetWindowPos(m_hTab_Normalmapgen, HWND_TOP, -rect.left, -rect.top, 0, 0, SWP_NOSIZE);

	{
		int nCmdShow = bShowBumpmapFileName ? SW_SHOW : SW_HIDE;

		HWND hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_SRFO_ADDBUMP_TEXT0);assert(hwnd);

		ShowWindow(hwnd,nCmdShow);

		hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_SRFO_ADDBUMP_NAME);assert(hwnd);

		ShowWindow(hwnd,nCmdShow);

		hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_SRFO_ADDBUMP_CHOOSE);assert(hwnd);

		ShowWindow(hwnd,nCmdShow);
	}

	{
		hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_NM_SLIDERBUMPSTRENGTH);assert(hwnd);
		m_bumpStrengthSlider.SetDocument(&m_bumpStrengthValue);
		m_bumpStrengthSlider.SubclassWindow(hwnd);
	}

	{
		hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_NM_SLIDERBUMPBLUR);assert(hwnd);
		m_bumpBlurSlider.SetDocument(&m_bumpBlurValue);
		m_bumpBlurSlider.SubclassWindow(hwnd);
	}

	{
		hwnd = GetDlgItem(m_hTab_Normalmapgen,IDC_EDIT_BUMPBLUR);assert(hwnd);
		m_bumpBlurEdit.SetDocument(&m_bumpBlurValue);
		m_bumpBlurEdit.SubclassWindow(hwnd);
	}

	{
		hwnd = GetDlgItem(m_hTab_Normalmapgen,IDC_EDIT_BUMPSTRENGTH);assert(hwnd);
		m_bumpStrengthEdit.SetDocument(&m_bumpStrengthValue);
		m_bumpStrengthEdit.SubclassWindow(hwnd);
	}

	{
		hwnd = GetDlgItem(m_hTab_Normalmapgen,IDC_NM_BUMPINVERT);assert(hwnd);
		m_bumpInvertCheckBox.SetDocument(&m_bumpInvertValue);
		m_bumpInvertCheckBox.SubclassWindow(hwnd);
	}

	// bump to normal map conversion
	{
		const HWND hwnd = GetDlgItem(m_hTab_Normalmapgen, IDC_NM_FILTERTYPE);
		assert(hwnd);
		for (int i = 0, c = 0; i < CBumpProperties::GetBumpToNormalFilterCount(); ++i)
		{
			const char* const genStr = (i == 0 ? "None (off)" : CBumpProperties::GetBumpToNormalFilterName(i));
			if (genStr && genStr[0])
			{
				SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)genStr);

				m_GenIndexToControl[i] = c;
				m_GenControlToIndex[c] = i;

				++c;
			}
		}
	}
}

HWND CUIBumpmapPanel::GetHWND() const
{
	return m_hTab_Normalmapgen;
}


void CUIBumpmapPanel::UpdateBumpStrength( const CBumpProperties &rBumpProp )
{
	const float fValue = rBumpProp.GetBumpStrengthAmount();

	this->m_bumpStrengthValue.SetValue(abs(fValue));
}


void CUIBumpmapPanel::UpdateBumpBlur( const CBumpProperties &rBumpProp )
{
	this->m_bumpBlurValue.SetValue(rBumpProp.GetBumpBlurAmount());
}


void CUIBumpmapPanel::UpdateBumpInvert( const CBumpProperties &rBumpProp )
{
	const float fValue = rBumpProp.GetBumpStrengthAmount();

	this->m_bumpInvertValue.SetValue(fValue<0);
}


void CUIBumpmapPanel::GetDataFromDialog( CBumpProperties &rBumpProp )
{
	{
		const bool bInverted = this->m_bumpInvertValue.GetValue();
		const float fValue = abs(this->m_bumpStrengthValue.GetValue());

		rBumpProp.SetBumpStrengthAmount( bInverted ? -fValue : fValue );
	}

	rBumpProp.SetBumpBlurAmount(this->m_bumpBlurValue.GetValue());


	{
		const HWND hwnd = GetDlgItem(m_hTab_Normalmapgen, IDC_NM_FILTERTYPE);
		assert(hwnd);
		rBumpProp.SetBumpToNormalFilterIndex(m_GenControlToIndex[ComboBox_GetCurSel(hwnd)]);
	}

	{
		char str[2048];

		HWND dlgitem=::GetDlgItem(m_hTab_Normalmapgen,IDC_SRFO_ADDBUMP_NAME);

		GetWindowText(dlgitem,str,2048);

		rBumpProp.SetBumpmapName(str);
	}
}


void CUIBumpmapPanel::ChooseAddBump( HWND hParentWnd )
{
	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	char bumpmapname[1024]="";

	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner=hParentWnd;
	ofn.hInstance=(HINSTANCE)GetModuleHandle(NULL);
	ofn.lpstrFilter="TIF Files (*.TIF)\0*.TIF\0";
	ofn.lpstrDefExt="tif";
	ofn.lpstrFile=bumpmapname;
	ofn.nMaxFile=1024;
	ofn.lpstrTitle="Open optional Bumpmap (.TIF)";
	ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;

	if(!GetOpenFileName(&ofn))		// filename: bumpmapname
		bumpmapname[0]=0;

	HWND dlgitem=::GetDlgItem(m_hTab_Normalmapgen,IDC_SRFO_ADDBUMP_NAME);

	SetWindowText(dlgitem,bumpmapname);
}

void CUIBumpmapPanel::SetDataToDialog( const CBumpProperties &rBumpProp, const bool bInitalUpdate )
{
	// sub window normalmapgen
	HWND hwnd = GetDlgItem(m_hTab_Normalmapgen, IDC_NM_FILTERTYPE);
	assert(hwnd);
	const int iSel = m_GenIndexToControl[rBumpProp.GetBumpToNormalFilterIndex()];
	SendMessage(hwnd, CB_SETCURSEL, iSel, 0);
	rBumpProp.EnableWindowBumpToNormalFilter(hwnd);

	if(bInitalUpdate)
	{
		UpdateBumpStrength(rBumpProp);
		UpdateBumpBlur(rBumpProp);
		UpdateBumpInvert(rBumpProp);
	}

	hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_EDIT_BUMPSTRENGTH);assert(hwnd);
	rBumpProp.EnableWindowBumpStrengthAmount(hwnd);
	hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_NM_SLIDERBUMPSTRENGTH);assert(hwnd);
	rBumpProp.EnableWindowBumpStrengthAmount(hwnd);
	hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_NM_BUMPINVERT);assert(hwnd);
	rBumpProp.EnableWindowBumpStrengthAmount(hwnd);

	hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_NM_SLIDERBUMPBLUR);assert(hwnd);
	rBumpProp.EnableWindowBumpBlurAmount(hwnd);
	hwnd=GetDlgItem(m_hTab_Normalmapgen,IDC_EDIT_BUMPBLUR);assert(hwnd);
	rBumpProp.EnableWindowBumpBlurAmount(hwnd);


	{
		HWND dlgitem=::GetDlgItem(m_hTab_Normalmapgen,IDC_SRFO_ADDBUMP_NAME);

		SetWindowText(dlgitem,rBumpProp.GetBumpmapName());
	}
}

INT_PTR CALLBACK CUIBumpmapPanel::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Get the instance pointer.
	CUIBumpmapPanel* pThis;
	if (uMsg == WM_INITDIALOG)
	{
		SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
		pThis = reinterpret_cast<CUIBumpmapPanel*>(static_cast<LONG_PTR>(lParam));
	}
	else
	{
		pThis = reinterpret_cast<CUIBumpmapPanel*>(static_cast<LONG_PTR>(GetWindowLongPtr(hWnd, GWLP_USERDATA)));
	}

	// Reflect notifications back to (some of) the controls.
	BOOL bHandled = TRUE;
	LRESULT lResult = pThis->ReflectNotifications(hWnd, uMsg, wParam, lParam, bHandled, pThis->reflectIDs);
	if (bHandled)
		return lResult;

	//HWND hParent=GetParent(hWnd);
	//if(hParent)
	//	return SendMessage(hParent,uMsg,wParam,lParam)!=0?TRUE:FALSE;

	//	return DefWindowProc( hWnd, uMsg, wParam, lParam );
	return FALSE;
}

// Based on code from AtlWin.h
LRESULT CUIBumpmapPanel::ReflectNotifications(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled, const std::set<int>& reflectIDs)
{
	HWND hWndChild = NULL;

	switch(uMsg)
	{
	case WM_COMMAND:
	{
		if(lParam != NULL)	// not from a menu
			hWndChild = (HWND)lParam;

		UINT id = GetDlgCtrlID(hWndChild);
		if (id == IDC_NM_FILTERTYPE)
		{
			// prevent refresh when opening the drop-out
			if(HIWORD(wParam)==CBN_SELCHANGE)
			{
				m_pDlg->TriggerUpdatePreview(true);
			}
		}

		break;
	}
	case WM_NOTIFY:
		hWndChild = ((LPNMHDR)lParam)->hwndFrom;
		break;
	case WM_PARENTNOTIFY:
		switch(LOWORD(wParam))
		{
		case WM_CREATE:
		case WM_DESTROY:
			hWndChild = (HWND)lParam;
			break;
		default:
			hWndChild = GetDlgItem(hWnd, HIWORD(wParam));
			break;
		}
		break;
	case WM_DRAWITEM:
		if(wParam)	// not from a menu
			hWndChild = ((LPDRAWITEMSTRUCT)lParam)->hwndItem;
		break;
	case WM_MEASUREITEM:
		if(wParam)	// not from a menu
			hWndChild = GetDlgItem(hWnd, ((LPMEASUREITEMSTRUCT)lParam)->CtlID);
		break;
	case WM_COMPAREITEM:
		if(wParam)	// not from a menu
			hWndChild = GetDlgItem(hWnd, ((LPCOMPAREITEMSTRUCT)lParam)->CtlID);
		break;
	case WM_DELETEITEM:
		if(wParam)	// not from a menu
			hWndChild = GetDlgItem(hWnd, ((LPDELETEITEMSTRUCT)lParam)->CtlID);
		break;
	case WM_VKEYTOITEM:
	case WM_CHARTOITEM:
	case WM_HSCROLL:
	case WM_VSCROLL:
		hWndChild = (HWND)lParam;
		break;
	case WM_CTLCOLORBTN:
	case WM_CTLCOLORDLG:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORMSGBOX:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORSTATIC:
		hWndChild = (HWND)lParam;
		break;
	default:
		break;
	}

	if(hWndChild == NULL || reflectIDs.find(GetDlgCtrlID(hWndChild)) == reflectIDs.end())
	{
		bHandled = FALSE;
		return 1;
	}

	ATLASSERT(::IsWindow(hWndChild));
	return ::SendMessage(hWndChild, OCM__BASE + uMsg, wParam, lParam);
}
