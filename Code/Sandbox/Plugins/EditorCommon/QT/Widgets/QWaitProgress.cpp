// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QWaitProgress.h"

#include <QApplication>
#include <QStatusBar>

bool CWaitProgress::s_bInProgressNow = false;

CWaitProgress::CWaitProgress(const char* text, bool bStart)
	: m_strText(text)
	, m_bStarted(false)
{
	m_bIgnore = false;

	if (bStart)
	{
		Start();
	}
}

CWaitProgress::~CWaitProgress()
{
	if (m_bStarted)
	{
		Stop();
	}
}

void CWaitProgress::Start()
{
	if (m_bStarted)
	{
		Stop();
	}

	if (s_bInProgressNow)
	{
		// Do not affect previously started progress bar.
		m_bIgnore = true;
		m_bStarted = false;
		return;
	}

	GetIEditor()->AddWaitProgress(this);

	// switch on wait cursor
	QApplication::setOverrideCursor(Qt::WaitCursor);

	m_bStarted = true;
	m_percent = 0;
}

//////////////////////////////////////////////////////////////////////////
void CWaitProgress::Stop()
{
	if (!m_bStarted)
	{
		return;
	}

	GetIEditor()->RemoveWaitProgress(this);

	// switch off wait cursor
	QApplication::restoreOverrideCursor();

	s_bInProgressNow = false;
	m_bStarted = false;
}

bool CWaitProgress::Step(int nPercentage, bool bProcessEvents)
{
	if (m_bIgnore)
		return true;

	if (!m_bStarted)
		Start();

	if (m_percent == nPercentage)
		return true;

	m_percent = nPercentage;

	if (bProcessEvents)
	{
		qApp->processEvents();
	}

	return true;
}

void CWaitProgress::SetText(LPCTSTR lpszText)
{
	if (m_bIgnore)
		return;

	m_strText = lpszText;
}

