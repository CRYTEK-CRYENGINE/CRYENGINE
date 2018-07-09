// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LogWidget.h"

#include <QBoxLayout>
#include <QLineEdit>
#include <Util/QParentWndWidget.h>
#include <QPlainTextEdit>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <QPushButton>
#include <QSplitter>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ICryLink.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>
#include <CrySchematyc2/Services/ILog.h>

#include "PluginUtils.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CLogSettingsWidget::CLogSettingsWidget(QWidget* pParent)
		: QWidget(pParent)
		, m_bShowComments(true)
		, m_bShowWarnings(true)
		, m_bShowErrors(true)
		, m_bShowTime(false)
		, m_bShowOrigin(false)
		, m_bShowEntity(false)
	{
		ILog& log = GetSchematyc()->GetLog();
		m_streams.push_back(log.GetStreamName(LOG_STREAM_DEFAULT));
		m_streams.push_back(log.GetStreamName(LOG_STREAM_SYSTEM));
		m_streams.push_back(log.GetStreamName(LOG_STREAM_COMPILER));
		m_streams.push_back(log.GetStreamName(LOG_STREAM_GAME));

		m_pLayout                       = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pAttachToSelectedEntityButton = new QPushButton("Attach To Selected Entity", this);
		m_pPropertyTree                 = new QPropertyTree(this);

		connect(m_pAttachToSelectedEntityButton, SIGNAL(clicked()), this, SLOT(OnAttachToSelectedEntityButtonClicked()));
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogSettingsWidget::InitLayout()
	{
		QWidget::setLayout(m_pLayout);
		m_pLayout->setSpacing(2);
		m_pLayout->setMargin(4);
		m_pLayout->addWidget(m_pAttachToSelectedEntityButton, 1);
		m_pLayout->addWidget(m_pPropertyTree, 1);

		m_pPropertyTree->setSizeHint(QSize(250, 250));
		m_pPropertyTree->setExpandLevels(1);
		m_pPropertyTree->setSliderUpdateDelay(5);
		m_pPropertyTree->setValueColumnWidth(0.6f);
		m_pPropertyTree->attach(Serialization::SStruct(*this));
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogSettingsWidget::Serialize(Serialization::IArchive& archive)
	{
		archive(m_entity, "entity", "Entity");
		archive(m_streams, "streams", "Streams");
		archive(m_bShowComments, "bShowComments", "Show Comments");
		archive(m_bShowWarnings, "bShowWarnings", "Show Warnings");
		archive(m_bShowErrors, "bShowErrors", "Show Errors");
		archive(m_bShowTime, "bShowTime", "Show Time");
		archive(m_bShowOrigin, "bShowOrigin", "Show Origin");
		archive(m_bShowEntity, "bShowEntity", "Show Entity");
	}

	//////////////////////////////////////////////////////////////////////////
	bool CLogSettingsWidget::FilterMessage(const LogStreamId& streamId, const EntityId entityId, const ELogMessageType messageType) const
	{
		switch(messageType)
		{
		case ELogMessageType::Comment:
			{
				if(!m_bShowComments)
				{
					return false;
				}
				break;
			}
		case ELogMessageType::Warning:
			{
				if(!m_bShowWarnings)
				{
					return false;
				}
				break;
			}
		case ELogMessageType::Error:
		case ELogMessageType::CriticalError:
		case ELogMessageType::FatalError:
			{
				if(!m_bShowErrors)
				{
					return false;
				}
				break;
			}
		}

		if(!m_entity.empty())
		{
			const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
			if(!pEntity || (m_entity != pEntity->GetName()))
			{
				return false;
			}
		}
		
		const SLogStreamName streamName(GetSchematyc()->GetLog().GetStreamName(streamId));
		auto compareStreamName = [&streamName] (const SLogStreamName& rhs)
		{
			return streamName.value == rhs.value;
		};
		return std::find_if(m_streams.begin(), m_streams.end(), compareStreamName) != m_streams.end();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogSettingsWidget::FormatMessage(QString& output, const ELogMessageType messageType, const CLogMessageMetaInfo& metaInfo, const char* szMessage) const
	{
		stack_string color = "lightgray";
		switch(messageType)
		{
		case ELogMessageType::Warning:
			{
				color = "yellow";
				break;
			}
		case ELogMessageType::Error:
		case ELogMessageType::CriticalError:
		case ELogMessageType::FatalError:
			{
				color = "red";
				break;
			}
		}

		stack_string text;

		if(m_bShowTime)
		{
			LogUtils::TimeStringBuffer stringBuffer = "";
			text.append("[");
			text.append(LogUtils::FormatTime(stringBuffer));
			text.append("] ");
		}

		if(m_bShowOrigin)
		{
			const char* szFunction = metaInfo.GetFunction();
			if(szFunction && szFunction[0])
			{
				text.append("[");
				text.append(szFunction);
				text.append("] ");
			}
		}

		if(m_bShowEntity && (metaInfo.GetEntityId() != INVALID_ENTITYID))
		{
			const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(metaInfo.GetEntityId());
			if(pEntity)
			{
				text.append(pEntity->GetName());
				text.append(" - ");
			}
		}

		text.append(szMessage);

		const string uri = metaInfo.GetUri();
		if(!uri.empty())
		{
			output.append("<a href=\"");
			output.append(uri);
			output.append("\" style=\"color:");
			output.append(color.c_str());
			output.append("\">");
			output.append(text.c_str());
			output.append("</a>");
		}
		else
		{
			output.append("<font color=\"");
			output.append(color.c_str());
			output.append("\">");
			output.append(text.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogSettingsWidget::OnAttachToSelectedEntityButtonClicked()
	{
		const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(PluginUtils::GetSelectedEntityId());
		m_entity = pEntity ? pEntity->GetName() : "";
		m_pPropertyTree->revert();
	}

	//////////////////////////////////////////////////////////////////////////
	CLogWidget::CLogWidget(QWidget* pParent)
		: QWidget(pParent)
	{
		m_pMainLayout             = new QBoxLayout(QBoxLayout::TopToBottom);
		m_pSplitter               = new QSplitter(Qt::Horizontal, this);
		m_pSettings               = new CLogSettingsWidget(this);
		m_pOutput                 = new QTextBrowser(this);
		m_pControlLayout          = new QBoxLayout(QBoxLayout::LeftToRight);
		m_pShowHideSettingsButton = new QPushButton(">> Show Settings", this);
		m_pClearOutputButton      = new QPushButton("Clear Output", this);

		m_pSettings->setVisible(false);

		m_pOutput->setContextMenuPolicy(Qt::NoContextMenu);
		m_pOutput->setUndoRedoEnabled(false);
		m_pOutput->setOpenLinks(false);

		connect(m_pShowHideSettingsButton, SIGNAL(clicked()), this, SLOT(OnShowHideSettingsButtonClicked()));
		connect(m_pClearOutputButton, SIGNAL(clicked()), this, SLOT(OnClearOutputButtonClicked()));
		connect(m_pOutput, SIGNAL(anchorClicked(const QUrl&)), this, SLOT(OnLinkClicked(const QUrl&)));

		GetSchematyc()->GetLogRecorder().VisitMessages(LogMessageVisitor::FromMemberFunction<CLogWidget, &CLogWidget::VisitRecordedLogMessage>(*this));
		GetSchematyc()->GetLog().Signals().message.Connect(LogMessageSignal::Delegate::FromMemberFunction<CLogWidget, &CLogWidget::OnLogMessage>(*this), m_connectionScope);

		QWidget::startTimer(80);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::InitLayout()
	{
		QWidget::setLayout(m_pMainLayout);
		m_pMainLayout->setSpacing(1);
		m_pMainLayout->setMargin(0);
		m_pMainLayout->addWidget(m_pSplitter, 1);
		m_pMainLayout->addLayout(m_pControlLayout, 0);

		m_pSplitter->setStretchFactor(0, 1);
		m_pSplitter->setStretchFactor(1, 0);
		m_pSplitter->addWidget(m_pSettings);
		m_pSplitter->addWidget(m_pOutput);
		
		QList<int> splitterSizes;
		splitterSizes.push_back(60);
		splitterSizes.push_back(200);
		m_pSplitter->setSizes(splitterSizes);

		m_pSettings->InitLayout();

		m_pControlLayout->setSpacing(2);
		m_pControlLayout->setMargin(4);
		m_pControlLayout->addWidget(m_pShowHideSettingsButton, 1);
		m_pControlLayout->addWidget(m_pClearOutputButton, 1);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::LoadSettings(const XmlNodeRef& xml)
	{
		Serialization::LoadXmlNode(*m_pSettings, xml);
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CLogWidget::SaveSettings(const char* szName)
	{
		return Serialization::SaveXmlNode(*m_pSettings, szName);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::OnShowHideSettingsButtonClicked()
	{
		const bool bShowSettings = !m_pSettings->isVisible();
		m_pShowHideSettingsButton->setText(bShowSettings ? "<< Hide Settings" : ">> Show Settings");
		m_pSettings->setVisible(bShowSettings);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::OnClearOutputButtonClicked()
	{
		m_pOutput->clear();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::OnLinkClicked(const QUrl& link)
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

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::timerEvent(QTimerEvent* pTimerEvent)
	{
		m_pOutput->setUpdatesEnabled(false);
		for(const QString& pendingMessage : m_pendingMessages)
		{
			m_pOutput->append(pendingMessage);
		}
		m_pendingMessages.clear();
		m_pOutput->setUpdatesEnabled(true);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CLogWidget::VisitRecordedLogMessage(const SLogMessageData& logMessageData)
	{
		WriteLogMessageToOutput(logMessageData);
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::OnLogMessage(const SLogMessageData& logMessageData)
	{
		if(QWidget::isVisible() && m_pSettings->FilterMessage(logMessageData.streamId, logMessageData.metaInfo.GetEntityId(), logMessageData.messageType))
		{
			WriteLogMessageToOutput(logMessageData);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWidget::WriteLogMessageToOutput(const SLogMessageData& logMessageData)
	{
		QString	message;
		m_pSettings->FormatMessage(message, logMessageData.messageType, logMessageData.metaInfo, logMessageData.szMessage);
		m_pendingMessages.push_back(message);
	}

	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CLogWnd, CWnd)
		ON_WM_SHOWWINDOW()
		ON_WM_SIZE()
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CLogWnd::CLogWnd()
		: m_pParentWndWidget(nullptr)
		, m_pLogWidget(nullptr)
		, m_pLayout(nullptr)
	{}

	//////////////////////////////////////////////////////////////////////////
	CLogWnd::~CLogWnd()
	{
		SAFE_DELETE(m_pLayout);
		SAFE_DELETE(m_pLogWidget);
		SAFE_DELETE(m_pParentWndWidget);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWnd::Init()
	{
		LOADING_TIME_PROFILE_SECTION;
		m_pParentWndWidget = new QParentWndWidget(GetSafeHwnd());
		m_pLogWidget       = new CLogWidget(m_pParentWndWidget);
		m_pLayout          = new QBoxLayout(QBoxLayout::TopToBottom);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWnd::InitLayout()
	{
		LOADING_TIME_PROFILE_SECTION;
		m_pLayout->setContentsMargins(0, 0, 0, 0);
		m_pLayout->addWidget(m_pLogWidget, 1);
		m_pParentWndWidget->setLayout(m_pLayout);
		m_pParentWndWidget->show();
		m_pLogWidget->InitLayout();
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWnd::LoadSettings(const XmlNodeRef& xml)
	{
		m_pLogWidget->LoadSettings(xml);
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CLogWnd::SaveSettings(const char* szName)
	{
		return m_pLogWidget->SaveSettings(szName);
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWnd::OnShowWindow(BOOL bShow, UINT nStatus)
	{
		if(m_pLogWidget)
		{
			m_pLogWidget->setVisible(bShow);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CLogWnd::OnSize(UINT nType, int cx, int cy)
	{
		if(m_pParentWndWidget)
		{
			CRect rect; 
			CWnd::GetClientRect(&rect);
			m_pParentWndWidget->setGeometry(0, 0, rect.Width(), rect.Height());
		}
	}
}
