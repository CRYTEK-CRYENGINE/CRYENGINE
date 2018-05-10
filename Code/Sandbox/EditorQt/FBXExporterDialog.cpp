// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FBXExporterDialog.h"
#include "Controls/QuestionDialog.h"

namespace
{
const uint kDefaultFPS = 30.0f;
}

CFBXExporterDialog::CFBXExporterDialog(bool bDisplayOnlyFPSSetting, CWnd* pParent)
	: CDialog(CFBXExporterDialog::IDD, pParent)
{
	m_bExportLocalCoords = false;
	m_bExportLocalCoordsCheckBoxEnable = false;
	m_bExportOnlyMasterCamera = false;
	m_fps = kDefaultFPS;
	m_bDisplayOnlyFPSSetting = bDisplayOnlyFPSSetting;
}

void CFBXExporterDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FBX_EXPORT_EXPORT_LOCAL_COORDINATES, m_exportLocalCoordsCheckbox);
	DDX_Control(pDX, IDC_FBX_EXPORT_EXPORT_ONLY_MASTER_CAMERA, m_exportOnlyMasterCameraCheckBox);
	DDX_Control(pDX, IDC_FBX_EXPORT_CAMSHAKEFPSCOMBO, m_fpsCombo);
}

BEGIN_MESSAGE_MAP(CFBXExporterDialog, CDialog)
ON_CBN_SELENDOK(IDC_FBX_EXPORT_CAMSHAKEFPSCOMBO, OnFPSChange)
ON_CBN_SELCHANGE(IDC_FBX_EXPORT_CAMSHAKEFPSCOMBO, OnFPSChange)
ON_BN_CLICKED(IDC_FBX_EXPORT_EXPORT_LOCAL_COORDINATES, SetExportLocalToTheSelectedObjectCheckBox)
ON_BN_CLICKED(IDC_FBX_EXPORT_EXPORT_ONLY_MASTER_CAMERA, SetExportOnlyMasterCameraCheckBox)
END_MESSAGE_MAP()

void CFBXExporterDialog::SetExportLocalToTheSelectedObjectCheckBox()
{
	m_bExportLocalCoords = m_exportLocalCoordsCheckbox.GetCheck();
}

void CFBXExporterDialog::SetExportOnlyMasterCameraCheckBox()
{
	m_bExportOnlyMasterCamera = m_exportOnlyMasterCameraCheckBox.GetCheck();
}

void CFBXExporterDialog::OnOK()
{
	m_bExportLocalCoords = m_exportLocalCoordsCheckbox.GetCheck();

	CString fpsStr;
	int selNumber = m_fpsCombo.GetCurSel();

	if (selNumber >= 0)
	{
		m_fpsCombo.GetLBText(m_fpsCombo.GetCurSel(), fpsStr);
	}
	else
	{
		m_fpsCombo.GetWindowText(fpsStr);
	}

	sscanf(fpsStr, "%f", &m_fps);

	if (m_fps <= 0 && !fpsStr.IsEmpty() || fpsStr.IsEmpty())
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Please enter a correct FPS value"));
		m_fps = 30;
		m_fpsCombo.SetCurSel(2);
		return;
	}

	CDialog::OnOK();
}

void CFBXExporterDialog::OnFPSChange()
{
	CString fpsStr;
	int selNumber = m_fpsCombo.GetCurSel();

	if (selNumber >= 0)
	{
		m_fpsCombo.GetLBText(m_fpsCombo.GetCurSel(), fpsStr);
	}
	else
	{
		m_fpsCombo.GetWindowText(fpsStr);
	}

	if (fpsStr.MakeLower().Find("custom") != -1)
	{
		m_fpsCombo.SetCurSel(-1);
	}
}

BOOL CFBXExporterDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	if (m_bDisplayOnlyFPSSetting)
	{
		m_exportLocalCoordsCheckbox.EnableWindow(false);
		m_exportOnlyMasterCameraCheckBox.EnableWindow(false);
	}
	else
	{
		m_exportLocalCoordsCheckbox.EnableWindow(m_bExportLocalCoordsCheckBoxEnable);
	}

	m_fpsCombo.AddString("24");
	m_fpsCombo.AddString("25");
	m_fpsCombo.AddString("30");
	m_fpsCombo.AddString("48");
	m_fpsCombo.AddString("60");
	m_fpsCombo.AddString("Custom");
	m_fpsCombo.SetCurSel(2);

	return TRUE;
}

