// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UIMIPControlPanel.h"    // CUIMIPControlPanel
#include "resource.h"
#include "ImageProperties.h"      // CBumpProperties
#include <Commdlg.h>              // OPENFILENAME
#include <commctrl.h>             // TBM_SETRANGEMAX, TBM_SETRANGEMIN

#include <windowsx.h>
#undef SubclassWindow

const int CUIMIPControlPanel::sliderIDs[NUM_CONTROLLED_MIP_MAPS] = {
	IDC_SLIDER_MIPALPHA0,
	IDC_SLIDER_MIPALPHA1,
	IDC_SLIDER_MIPALPHA2,
	IDC_SLIDER_MIPALPHA3,
	IDC_SLIDER_MIPALPHA4,
	IDC_SLIDER_MIPALPHA5
};

const int CUIMIPControlPanel::editIDs[NUM_CONTROLLED_MIP_MAPS] = {
	IDC_EDIT_MIPALPHA0,
	IDC_EDIT_MIPALPHA1,
	IDC_EDIT_MIPALPHA2,
	IDC_EDIT_MIPALPHA3,
	IDC_EDIT_MIPALPHA4,
	IDC_EDIT_MIPALPHA5
};

CUIMIPControlPanel::CUIMIPControlPanel()
	: m_hTab(0)
{
	memset(&m_GenIndexToControl, 0, sizeof(m_GenIndexToControl));
	memset(&m_GenControlToIndex, 0, sizeof(m_GenControlToIndex));
}

void CUIMIPControlPanel::InitDialog( HWND hWndParent, const bool bShowBumpmapFileName )
{
	// Initialise the values.
	for (int i = 0; i < NUM_CONTROLLED_MIP_MAPS; ++i)
	{
		m_MIPAlphaControl[i].SetRange(0.0f, 100.0f);
		m_MIPAlphaControl[i].SetValue(50.0f);
	}

	m_MIPBlurringControl.SetRange(-16.0f, 16.0f);
	m_MIPBlurringControl.SetValue(0.0f);

	const HWND hwnd = GetDlgItem(hWndParent, IDC_PROPTAB);
	assert(hwnd);

	RECT rect = { 0, 0, 1, 1 };
	TabCtrl_AdjustRect(hwnd, TRUE, &rect);

	m_hTab = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_MIPMAPCONTROL), hwnd, WndProc, reinterpret_cast<LPARAM>(this));
	assert(m_hTab);
	SetWindowPos(m_hTab, HWND_TOP, -rect.left, -rect.top, 0, 0, SWP_NOSIZE);

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_VALUESPARENT); 
		assert(hwnd);
		m_valuesDlg.Create(hwnd);
		RECT rect;
		GetClientRect(hwnd, &rect);
		m_valuesDlg.MoveWindow(&rect);
	}

	// Subclass the sliders and edit controls.
	for (int i = 0; i < NUM_CONTROLLED_MIP_MAPS; ++i)
	{
		const HWND hwndEdit = m_valuesDlg.GetDlgItem(CUIMIPControlPanel::editIDs[i]);
		assert(hwndEdit);
		const HWND hwndSlider = m_valuesDlg.GetDlgItem(CUIMIPControlPanel::sliderIDs[i]);
		assert(hwndSlider);
		m_MIPAlphaControl[i].SubclassWindows(hwndSlider, hwndEdit);
	}

	{
		const HWND hwndEdit = m_valuesDlg.GetDlgItem(IDC_EDIT_MIPBLUR);
		assert(hwndEdit);
		const HWND hwndSlider = m_valuesDlg.GetDlgItem(IDC_SLIDER_MIPBLUR);
		assert(hwndSlider);
		m_MIPBlurringControl.SubclassWindows(hwndSlider, hwndEdit);
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPMAPS);
		assert(hwnd);
		SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)"none (0)");
		SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)"max, tiled (1)");
	//SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)"max, mirrored (2)");
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPMETHOD);
		assert(hwnd);
		for (int i = 0, c = 0; i < CImageProperties::GetMipGenerationMethodCount(); ++i)
		{
			const char* genStr = (i == 0 ? NULL : CImageProperties::GetMipGenerationMethodName(i));
			if (genStr && genStr[0])
			{
				SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)genStr);

				m_GenIndexToControl[i] = c;
				m_GenControlToIndex[c] = i;

				++c;
			}
		}
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPOP);
		assert(hwnd);
		for (int i = 0; i < CImageProperties::GetMipGenerationEvalCount(); ++i)
		{
			const char* genStr = CImageProperties::GetMipGenerationEvalName(i);
			if (genStr && genStr[0])
			{
				SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)genStr);
			}
		}
	}
}

HWND CUIMIPControlPanel::GetHWND() const
{
	return m_hTab;
}

void CUIMIPControlPanel::GetDataFromDialog(CImageProperties& rProp)
{
	for (int i = 0; i < NUM_CONTROLLED_MIP_MAPS; ++i)
	{
		rProp.SetMIPAlpha(i, (int)(this->m_MIPAlphaControl[i].GetValue() + 0.5f));
	}

	rProp.SetMipBlurring(this->m_MIPBlurringControl.GetValue());

	{
		const HWND hwnd = GetDlgItem(m_hTab,IDC_MAINTAINALPHACOVERAGE);
		assert(hwnd);
		rProp.SetMaintainAlphaCoverage(Button_GetCheck(hwnd) != 0);
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPMAPS);
		assert(hwnd);
		rProp.SetMipMaps((ComboBox_GetCurSel(hwnd) > 0) ? 1 : 0);
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPMETHOD);
		assert(hwnd);
		rProp.SetMipGenerationMethodIndex(m_GenControlToIndex[ComboBox_GetCurSel(hwnd)]);
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPOP);
		assert(hwnd);
		rProp.SetMipGenerationEvalIndex(ComboBox_GetCurSel(hwnd));
	}
}

void CUIMIPControlPanel::SetDataToDialog(const IConfig* pConfig, const CImageProperties &rProp, const bool bInitalUpdate)
{
	int iMIPAlpha[NUM_CONTROLLED_MIP_MAPS];

	rProp.GetMIPAlphaArray(iMIPAlpha);

	for (int i = 0; i < NUM_CONTROLLED_MIP_MAPS; ++i)
	{
		this->m_MIPAlphaControl[i].SetValue((float)iMIPAlpha[i]);
	}

	this->m_MIPBlurringControl.SetValue(rProp.GetMipBlurring());

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MAINTAINALPHACOVERAGE);
		assert(hwnd);
		Button_SetCheck(hwnd,rProp.GetMaintainAlphaCoverage()?1:0);		
		EnableWindow(hwnd, (pConfig->GetAsInt("mc", -12345, -12345, eCP_PriorityAll & (~eCP_PriorityFile)) == -12345));
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPMAPS);
		assert(hwnd);
		const int iSel = rProp.GetMipMaps() ? 1 : 0;
		SendMessage(hwnd, CB_SETCURSEL, iSel, 0);
		EnableWindow(hwnd, (pConfig->GetAsInt("mipmaps", -12345, -12345, eCP_PriorityAll & (~eCP_PriorityFile)) == -12345));
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPMETHOD);
		assert(hwnd);
		const int iSel = m_GenIndexToControl[rProp.GetMipGenerationMethodIndex()];
		SendMessage(hwnd, CB_SETCURSEL, iSel, 0);
		EnableWindow(hwnd, stricmp(pConfig->GetAsString("mipgentype", "<>", "<>", eCP_PriorityAll & (~eCP_PriorityFile)).c_str(), "<>")  == 0);
	}

	{
		const HWND hwnd = GetDlgItem(m_hTab, IDC_MIPOP);
		assert(hwnd);
		const int iSel = rProp.GetMipGenerationEvalIndex();
		SendMessage(hwnd, CB_SETCURSEL, iSel, 0);
		EnableWindow(hwnd, stricmp(pConfig->GetAsString("mipgeneval", "<>", "<>", eCP_PriorityAll & (~eCP_PriorityFile)).c_str(), "<>")  == 0);
	}
}

INT_PTR CALLBACK CUIMIPControlPanel::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const HWND hParent = GetParent(hWnd);
	if (hParent)
	{
		return SendMessage(hParent,uMsg,wParam,lParam) != 0 ? TRUE : FALSE;
	}

	//	return DefWindowProc( hWnd, uMsg, wParam, lParam );
	return FALSE;
}
