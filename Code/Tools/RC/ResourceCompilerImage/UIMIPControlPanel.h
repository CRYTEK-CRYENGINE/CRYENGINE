// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditSliderPair.h"
#include "ScrollableDialog.h"
#include "resource.h"
#include "ImageProperties.h"
#include <set>

class CImageProperties;


class CUIMIPControlPanel
{
public:
	enum { NUM_CONTROLLED_MIP_MAPS = CImageProperties::NUM_CONTROLLED_MIP_MAPS };

	CUIMIPControlPanel();

	void InitDialog(HWND hWndParent, const bool bShowBumpmapFileName);

	void GetDataFromDialog(CImageProperties& rProp);

	void SetDataToDialog(const IConfig* pConfig, const CImageProperties& rProp, const bool bInitalUpdate);

	HWND GetHWND() const;

	CEditSliderPair m_MIPAlphaControl[NUM_CONTROLLED_MIP_MAPS];
	CEditSliderPair m_MIPBlurringControl;

	int m_GenIndexToControl[eWindowFunction_Num];
	int m_GenControlToIndex[eWindowFunction_Num];

private: // ---------------------------------------------------------------------------------------

	class CMipMapValuesDialog 
		: public CScrollableDialog<CMipMapValuesDialog>
	{
	public:
		CMipMapValuesDialog()
			: m_pPanel(0) 
		{
		}

		void SetParent(CUIMIPControlPanel* pPanel)
		{
			m_pPanel = pPanel;
		}

		enum { IDD = IDD_PANEL_MIPMAPVALUES };

		BEGIN_MSG_MAP(CMipMapValuesDialog)
			REFLECT_NOTIFICATIONS()
			CHAIN_MSG_MAP(CScrollableDialog)
		END_MSG_MAP()

	private:
		CUIMIPControlPanel* m_pPanel;
	};

	HWND m_hTab;  // window handle
	CMipMapValuesDialog m_valuesDlg;

	static const int sliderIDs[NUM_CONTROLLED_MIP_MAPS];
	static const int editIDs[NUM_CONTROLLED_MIP_MAPS];

	static INT_PTR CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
