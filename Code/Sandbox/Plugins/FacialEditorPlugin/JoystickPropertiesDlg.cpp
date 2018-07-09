// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "JoystickPropertiesDlg.h"
#include "Resource.h"

// CJoystickPropertiesDlg dialog

IMPLEMENT_DYNAMIC(CJoystickPropertiesDlg, CDialog)

CJoystickPropertiesDlg::CJoystickPropertiesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_FACEED_JOYSTICK_PROPERTIES, pParent)
{
	m_names[0] = m_names[1] = "<UNSET>";
	m_scales[0] = m_scales[1] = 1.0f;
	m_flippeds[0] = m_flippeds[1] = false;
	m_offsets[0] = m_offsets[1] = 0.0f;
	m_enableds[0] = m_enableds[1] = true;
}

CJoystickPropertiesDlg::~CJoystickPropertiesDlg()
{
}

void CJoystickPropertiesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_CHANNEL_X, m_nameEdits[0]);
	DDX_Control(pDX, IDC_FLIPPED_X, m_flippedButtons[0]);

	DDX_Control(pDX, IDC_CHANNEL_Y, m_nameEdits[1]);
	DDX_Control(pDX, IDC_FLIPPED_Y, m_flippedButtons[1]);
}

BEGIN_MESSAGE_MAP(CJoystickPropertiesDlg, CDialog)
END_MESSAGE_MAP()

// CJoystickPropertiesDlg message handlers

BOOL CJoystickPropertiesDlg::OnInitDialog()
{
	BOOL bReturnValue = __super::OnInitDialog();

	m_scaleEdits[0].Create(this, IDC_VIDEO_SCALE_X);
	m_scaleEdits[0].SetRange(-(1e+10), (1e+10));
	m_offsetEdits[0].Create(this, IDC_VIDEO_OFFSET_X);
	m_offsetEdits[0].SetRange(-(1e+10), (1e+10));

	m_scaleEdits[1].Create(this, IDC_VIDEO_SCALE_Y);
	m_scaleEdits[1].SetRange(-(1e+10), (1e+10));
	m_offsetEdits[1].Create(this, IDC_VIDEO_OFFSET_Y);
	m_offsetEdits[1].SetRange(-(1e+10), (1e+10));

	for (int axis = 0; axis < 2; ++axis)
	{
		m_nameEdits[axis].SetWindowText(m_names[axis].c_str());
		m_nameEdits[axis].EnableWindow(m_enableds[axis]);
		m_flippedButtons[axis].SetCheck(m_flippeds[axis]);
		m_flippedButtons[axis].EnableWindow(m_enableds[axis]);
		m_scaleEdits[axis].SetValue(m_scales[axis]);
		m_scaleEdits[axis].EnableWindow(m_enableds[axis]);
		m_offsetEdits[axis].SetValue(m_offsets[axis]);
		m_offsetEdits[axis].EnableWindow(m_enableds[axis]);
	}

	return bReturnValue;
}

void CJoystickPropertiesDlg::OnOK()
{
	for (int axis = 0; axis < 2; ++axis)
	{
		CString text;
		m_nameEdits[axis].GetWindowText(text);
		m_names[axis] = text.GetString();
		m_flippeds[axis] = m_flippedButtons[axis].GetCheck();
		m_scales[axis] = m_scaleEdits[axis].GetValue();
		m_offsets[axis] = m_offsetEdits[axis].GetValue();
	}

	CDialog::OnOK();
}

void CJoystickPropertiesDlg::SetChannelName(int axis, const string& channelName)
{
	m_names[axis] = channelName;
}

void CJoystickPropertiesDlg::SetChannelFlipped(int axis, bool flipped)
{
	m_flippeds[axis] = flipped;
}

bool CJoystickPropertiesDlg::GetChannelFlipped(int axis) const
{
	return m_flippeds[axis];
}

void CJoystickPropertiesDlg::SetVideoScale(int axis, float offset)
{
	m_scales[axis] = offset;
}

float CJoystickPropertiesDlg::GetVideoScale(int axis) const
{
	return m_scales[axis];
}

void CJoystickPropertiesDlg::SetVideoOffset(int axis, float offset)
{
	m_offsets[axis] = offset;
}

float CJoystickPropertiesDlg::GetVideoOffset(int axis) const
{
	return m_offsets[axis];
}

void CJoystickPropertiesDlg::SetChannelEnabled(int axis, bool enabled)
{
	m_enableds[axis] = enabled;
}

