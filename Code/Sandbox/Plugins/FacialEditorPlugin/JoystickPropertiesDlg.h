// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/NumberCtrl.h"

class CJoystickPropertiesDlg : public CDialog
{
	DECLARE_DYNAMIC(CJoystickPropertiesDlg)

public:
	CJoystickPropertiesDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CJoystickPropertiesDlg();

	void  SetChannelName(int axis, const string& channelName);
	void  SetChannelFlipped(int axis, bool flipped);
	bool  GetChannelFlipped(int axis) const;
	void  SetVideoScale(int axis, float offset);
	float GetVideoScale(int axis) const;
	void  SetVideoOffset(int axis, float offset);
	float GetVideoOffset(int axis) const;

	void  SetChannelEnabled(int axis, bool enabled);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	BOOL         OnInitDialog();
	virtual void OnOK();

	CEdit       m_nameEdits[2];
	CNumberCtrl m_scaleEdits[2];
	CButton     m_flippedButtons[2];
	CNumberCtrl m_offsetEdits[2];

	string      m_names[2];
	float       m_scales[2];
	bool        m_flippeds[2];
	float       m_offsets[2];
	bool        m_enableds[2];
};
