// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EULADIALOG_H__
#define __EULADIALOG_H__

#include "FrameWindow.h"
#include "EditControl.h"
#include "Spacer.h"
#include "Layout.h"
#include "PushButton.h"

class EULADialog
{
public:
	enum UserResponse
	{
		UserResponseNone,
		UserResponseCancel,
		UserResponseAccept
	};

	static UserResponse Show(int width, int height, TCHAR* resourceID);

private:
	EULADialog();

	UserResponse Run(int width, int height, TCHAR* resourceID);

	void CancelPressed();
	void AcceptPressed();

	FrameWindow m_frameWindow;
	PushButton m_cancelButton;
	Spacer m_buttonSpacer;
	PushButton m_acceptButton;
	Layout m_buttonLayout;
	EditControl m_edit;

	UserResponse m_userResponse;
};

#endif //__EULADIALOG_H__
