// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QTextBrowser>
#include <QWidget>

class QBoxLayout;

namespace Schematyc
{
	class CReportWidget : public QWidget
	{
		Q_OBJECT

	public:

		CReportWidget(QWidget* pParent);

		void Write(const char* szText);
		void WriteUri(const char* szUri, const char* szText);

	public slots:

		void OnLinkClicked(const QUrl& link);

	private:

		QBoxLayout*   m_pMainLayout;
		QTextBrowser* m_pOutput;
	};
}

