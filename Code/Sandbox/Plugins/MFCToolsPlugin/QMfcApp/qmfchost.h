// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "qwinhost.h"

class PLUGIN_API QMfcHost : public QWinHost
{
public:
	class QMfcHost(QWidget* parent, CWnd* pWnd, bool bNoScale = false, bool deleteWnd = true) : QWinHost(parent)
	{
		m_wnd = pWnd;
		CRect rc;
		pWnd->GetClientRect(rc);
		m_width = rc.Width();
		m_height = rc.Height();
		if (bNoScale)
			setMinimumSize(m_width, m_height);

		setWindow(pWnd->GetSafeHwnd(), !deleteWnd);
	}

	~QMfcHost()
	{
		if (!own_hwnd)
			delete m_wnd;
	}

	virtual QSize sizeHint() const { return QSize(m_width, m_height); }

private:
	CWnd* m_wnd;
	int   m_width, m_height;
};

