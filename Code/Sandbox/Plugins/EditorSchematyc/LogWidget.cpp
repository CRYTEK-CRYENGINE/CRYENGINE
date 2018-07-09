// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LogWidget.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ICryLink.h>
#include <CrySerialization/IArchiveHost.h>
#include <QAdvancedPropertyTree.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Services/ILog.h>
#include <CrySchematyc/Services/ILogRecorder.h>
#include <CrySchematyc/Services/LogStreamName.h>
#include <CrySchematyc/Utils/StackString.h>

#include "PluginUtils.h"

namespace Schematyc {
namespace LogWidgetUtils {

inline bool FilterMessage(const SLogSettings& settings, LogStreamId streamId, const EntityId entityId, const ELogMessageType messageType)
{
	switch (messageType)
	{
	case ELogMessageType::Comment:
		{
			if (!settings.bShowComments)
			{
				return false;
			}
			break;
		}
	case ELogMessageType::Warning:
		{
			if (!settings.bShowWarnings)
			{
				return false;
			}
			break;
		}
	case ELogMessageType::Error:
	case ELogMessageType::CriticalError:
	case ELogMessageType::FatalError:
		{
			if (!settings.bShowErrors)
			{
				return false;
			}
			break;
		}
	}

	if (!settings.entity.empty())
	{
		const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if (!pEntity || (settings.entity != pEntity->GetName()))
		{
			return false;
		}
	}

	const SLogStreamName streamName(gEnv->pSchematyc->GetLog().GetStreamName(streamId));
	auto compareStreamName = [&streamName](const SLogStreamName& rhs)
	{
		return streamName.value == rhs.value;
	};
	return std::find_if(settings.streams.begin(), settings.streams.end(), compareStreamName) != settings.streams.end();
}

void FormatMessage(const SLogSettings& settings, QString& output, const CLogMetaData* pMetaData, ELogMessageType messageType, const char* szMessage)
{
	CStackString color = "lightgray";
	switch (messageType)
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

	CStackString text;

	if (settings.bShowOrigin && pMetaData)
	{
		CStackString function;
		pMetaData->Get(ELogMetaField::Function, function);
		if (!function.empty())
		{
			text.append("[");
			text.append(function.c_str());
			text.append("] ");
		}
	}

	if (settings.bShowEntity && pMetaData)
	{
		EntityId entityId = INVALID_ENTITYID;
		pMetaData->Get(ELogMetaField::EntityId, entityId);
		if (entityId != INVALID_ENTITYID)
		{
			const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
			if (pEntity)
			{
				text.append(pEntity->GetName());
				text.append(" : ");
			}
		}
	}

	text.append(szMessage);

	text.replace("\n", "<br>");

	CStackString uri;
	if (pMetaData)
	{
		pMetaData->CreateUri(uri);
	}
	if (!uri.empty())
	{
		output.append("<a href=\"");
		output.append(uri.c_str());
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
} // LogWidgetUtils

SLogSettings::SLogSettings()
	: bShowComments(true)
	, bShowWarnings(true)
	, bShowErrors(true)
	, bShowEntity(false)
	, bShowOrigin(false)
{
	ILog& log = gEnv->pSchematyc->GetLog();

	const char* szStreamName = log.GetStreamName(LogStreamId::Default);
	if (szStreamName && szStreamName != "")
		streams.emplace_back(SLogStreamName(szStreamName));

	szStreamName = log.GetStreamName(LogStreamId::Core);
	if (szStreamName && szStreamName != "")
		streams.emplace_back(SLogStreamName(szStreamName));

	szStreamName = log.GetStreamName(LogStreamId::Compiler);
	if (szStreamName && szStreamName != "")
		streams.emplace_back(SLogStreamName(szStreamName));

	szStreamName = log.GetStreamName(LogStreamId::Editor);
	if (szStreamName && szStreamName != "")
		streams.emplace_back(SLogStreamName(szStreamName));

	szStreamName = log.GetStreamName(LogStreamId::Env);
	if (szStreamName && szStreamName != "")
		streams.emplace_back(SLogStreamName(szStreamName));
}

void SLogSettings::Serialize(Serialization::IArchive& archive)
{
	//archive(entity, "entity", "Entity");
	archive(streams, "streams", "Streams");
	archive(bShowComments, "bShowComments", "Show Comments");
	archive(bShowWarnings, "bShowWarnings", "Show Warnings");
	archive(bShowErrors, "bShowErrors", "Show Errors");
	archive(bShowEntity, "bShowEntity", "Show Entity");
	archive(bShowOrigin, "bShowOrigin", "Show Origin");
}

CLogSettingsWidget::CLogSettingsWidget(SLogSettings& settings)
	: m_settings(settings)
{
	QVBoxLayout* pLayout = new QVBoxLayout(this);

	m_pPropertyTree = new QAdvancedPropertyTree("LogSettings");
	m_pPropertyTree->setSizeHint(QSize(250, 250));
	m_pPropertyTree->setExpandLevels(1);
	m_pPropertyTree->setSliderUpdateDelay(5);
	m_pPropertyTree->setValueColumnWidth(0.6f);
	m_pPropertyTree->attach(Serialization::SStruct(m_settings));

	pLayout->addWidget(m_pPropertyTree);
}

void CLogSettingsWidget::showEvent(QShowEvent* pEvent)
{
	QWidget::showEvent(pEvent);

	if (m_pPropertyTree)
		m_pPropertyTree->setSizeToContent(true);
}

const SLogSettings& CLogSettingsWidget::GetSettings() const
{
	return m_settings;
}

//void CLogSettingsWidget::OnAttachToSelectedEntityButtonClicked()
//{
//	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(CrySchematycEditor::Utils::GetSelectedEntityId());
//	m_settings.entity = pEntity ? pEntity->GetName() : "";
//	m_pPropertyTree->revert();
//}

CLogWidget::CLogWidget(const SLogSettings& settings)
	: m_settings(settings)
{
	m_pMainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	m_pOutput = new QTextBrowser(this);

	m_pOutput->setContextMenuPolicy(Qt::NoContextMenu);
	m_pOutput->setUndoRedoEnabled(false);
	m_pOutput->setOpenLinks(false);

	QObject::connect(m_pOutput, SIGNAL(anchorClicked(const QUrl &)), this, SLOT(OnLinkClicked(const QUrl &)));

	gEnv->pSchematyc->GetLogRecorder().VisitMessages(SCHEMATYC_MEMBER_DELEGATE(&CLogWidget::VisitRecordedLogMessage, *this));
	gEnv->pSchematyc->GetLog().GetMessageSignalSlots().Connect(SCHEMATYC_MEMBER_DELEGATE(&CLogWidget::OnLogMessage, *this), m_connectionScope);

	QWidget::startTimer(80);
}

void CLogWidget::InitLayout()
{
	QWidget::setLayout(m_pMainLayout);
	m_pMainLayout->setSpacing(1);
	m_pMainLayout->setMargin(0);
	m_pMainLayout->addWidget(m_pOutput);
}

void CLogWidget::Clear()
{
	m_pOutput->clear();
}

void CLogWidget::OnLinkClicked(const QUrl& link)
{
	QString qString = link.toDisplayString();
	QByteArray qBytes = qString.toLocal8Bit();
	CryLinkUtils::ExecuteUri(qBytes.constData());
}

void CLogWidget::timerEvent(QTimerEvent* pTimerEvent)
{
	m_pOutput->setUpdatesEnabled(false);
	for (const QString& pendingMessage : m_pendingMessages)
	{
		m_pOutput->append(pendingMessage);
	}
	m_pendingMessages.clear();
	m_pOutput->setUpdatesEnabled(true);
}

EVisitStatus CLogWidget::VisitRecordedLogMessage(const SLogMessageData& logMessageData)
{
	WriteLogMessageToOutput(logMessageData);
	return EVisitStatus::Continue;
}

void CLogWidget::OnLogMessage(const SLogMessageData& logMessageData)
{
	if (QWidget::isVisible())
	{
		EntityId entityId = INVALID_ENTITYID;
		if (logMessageData.pMetaData)
		{
			logMessageData.pMetaData->Get(ELogMetaField::EntityId, entityId);
		}

		if (LogWidgetUtils::FilterMessage(m_settings, logMessageData.streamId, entityId, logMessageData.messageType))
		{
			WriteLogMessageToOutput(logMessageData);
		}
	}
}

void CLogWidget::WriteLogMessageToOutput(const SLogMessageData& logMessageData)
{
	QString message;
	LogWidgetUtils::FormatMessage(m_settings, message, logMessageData.pMetaData, logMessageData.messageType, logMessageData.szMessage);
	m_pendingMessages.push_back(message);
}

} // Schematyc

