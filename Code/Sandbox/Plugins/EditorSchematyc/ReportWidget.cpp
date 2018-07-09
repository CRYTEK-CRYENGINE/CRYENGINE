// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ReportWidget.h"

#include <QBoxLayout>
#include <QLineEdit>

namespace Schematyc
{
	CReportWidget::CReportWidget(QWidget* pParent)
		: QWidget(pParent)
	{
		m_pMainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pOutput     = new QTextBrowser(this);

		m_pOutput->setContextMenuPolicy(Qt::NoContextMenu);
		m_pOutput->setUndoRedoEnabled(false);
		m_pOutput->setOpenLinks(false);

		QWidget::setLayout(m_pMainLayout);

		m_pMainLayout->setSpacing(1);
		m_pMainLayout->setMargin(0);
		m_pMainLayout->addWidget(m_pOutput, 1);

		connect(m_pOutput, SIGNAL(anchorClicked(const QUrl&)), this, SLOT(OnLinkClicked(const QUrl&)));
	}

	void CReportWidget::Write(const char* szText)
	{
		m_pOutput->setUpdatesEnabled(false);
		m_pOutput->append(szText);
		m_pOutput->setUpdatesEnabled(true);
	}

	void CReportWidget::WriteUri(const char* szUri, const char* szText)
	{
		CStackString html;
		html.append("<a href=\"");
		html.append(szUri);
		html.append("\" style=\"color:");
		html.append("blue");
		html.append("\">");
		html.append(szText);
		html.append("</a>");
		Write(html.c_str());
	}

	void CReportWidget::OnLinkClicked(const QUrl& link)
	{
		QString                  qString = link.toDisplayString();
		QByteArray               qBytes = qString.toLocal8Bit();
		CryLinkService::CCryLink cryLink = qBytes.constData();
		const char*              szCmd = cryLink.GetQuery("cmd1");
		if(szCmd && szCmd[0])
		{
			gEnv->pConsole->ExecuteString(szCmd);
		}
	}
}

