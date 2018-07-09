// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "QuestionDialog.h"

//! This class allows to set a timeout for auto-closing the dialog in addition to the functionality of CQuestionDialog.
class EDITOR_COMMON_API CQuestionTimeoutDialog : public CQuestionDialog
{
	Q_OBJECT
public:
	CQuestionTimeoutDialog();
	~CQuestionTimeoutDialog();

	//! Displays modal dialog.
	//! \param timeoutInSeconds The dialog is closed automatically after a specified amount of time. 
	//! \return User selection or StandardButton::NoButton if the timeout expires.
	QDialogButtonBox::StandardButton Execute(int timeoutInSeconds);

protected:
	void timerEvent(QTimerEvent* pEvent);

private:
	void UpdateText();

private:
	QString m_infoLabelText;
	int m_timeInSeconds;
	int m_timerId;
};

