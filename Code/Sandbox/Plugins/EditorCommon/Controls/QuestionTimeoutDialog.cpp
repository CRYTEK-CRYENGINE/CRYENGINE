// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "QuestionTimeoutDialog.h"

CQuestionTimeoutDialog::CQuestionTimeoutDialog()
	: m_infoLabelText()
	, m_timeInSeconds(0)
	, m_timerId(0)
{
}

CQuestionTimeoutDialog::~CQuestionTimeoutDialog()
{
}

QDialogButtonBox::StandardButton CQuestionTimeoutDialog::Execute(int timeoutInSeconds)
{
	assert(m_infoLabel);

	if (m_infoLabelText.isNull())
	{
		m_infoLabelText = !m_infoLabel->text().isNull() ? m_infoLabel->text() : "";
	}

	m_timeInSeconds = timeoutInSeconds;

	UpdateText();

	m_timerId = startTimer(1000);

	auto result = CQuestionDialog::Execute();

	killTimer(m_timerId);

	return result;
}

void CQuestionTimeoutDialog::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_timerId)
	{
		CQuestionDialog::timerEvent(pEvent);
		return;
	}

	if (--m_timeInSeconds > 0)
	{
		UpdateText();
	}
	else
	{
		m_buttonPressed = QDialogButtonBox::StandardButton::NoButton;
		close();
	}
}

void CQuestionTimeoutDialog::UpdateText()
{
	const QString countdownText = tr("Waiting for %1 seconds.").arg(m_timeInSeconds);
	const QString infoLabelText = m_infoLabelText.isEmpty() ? countdownText : tr("%1\n%2").arg(m_infoLabelText, countdownText);
	m_infoLabel->setText(infoLabelText);
}

