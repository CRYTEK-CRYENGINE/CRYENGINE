// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Store stream ids rather than names in order to optimize filtering.
// #SchematycTODO : Display separate report when opening editor rather than copying recorded messages to log?
// #SchematycTODO : Should we allow log widgets to output messages when tabbed (i.e. not visible but not closed)?

#pragma once

#include <QTextBrowser>
#include <QWidget>
#include <CrySchematyc2/Serialization/SerializationUtils.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>

class QBoxLayout;
class QLineEdit;
class QParentWndWidget;
class QPlainTextEdit;
class QPropertyTree;
class QPushButton;
class QSplitter;

namespace Schematyc2
{
	typedef std::vector<SLogStreamName> LogStreams;

	class CLogSettingsWidget : public QWidget
	{
		Q_OBJECT

	public:

		CLogSettingsWidget(QWidget* pParent);

		void InitLayout();
		void Serialize(Serialization::IArchive& archive);
		bool FilterMessage(const LogStreamId& streamId, const EntityId entityId, const ELogMessageType messageType) const;
		void FormatMessage(QString& output, const ELogMessageType messageType, const CLogMessageMetaInfo& metaInfo, const char* szMessage) const;

	public slots:

		void OnAttachToSelectedEntityButtonClicked();

	private:

		string         m_entity;
		LogStreams     m_streams;
		bool           m_bShowComments;
		bool           m_bShowWarnings;
		bool           m_bShowErrors;
		bool           m_bShowTime;
		bool           m_bShowOrigin;
		bool           m_bShowEntity;
		QBoxLayout*    m_pLayout;
		QPushButton*   m_pAttachToSelectedEntityButton;
		QPropertyTree* m_pPropertyTree;
	};

	class CLogWidget : public QWidget
	{
		Q_OBJECT

	public:

		CLogWidget(QWidget* pParent);

		void InitLayout();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);

	public slots:

		void OnShowHideSettingsButtonClicked();
		void OnClearOutputButtonClicked();
		void OnLinkClicked(const QUrl& link);

	protected:

		void timerEvent(QTimerEvent* pTimerEvent);

	private:

		typedef std::vector<QString> PendingMessages;

		EVisitStatus VisitRecordedLogMessage(const SLogMessageData& logMessageData);
		void OnLogMessage(const SLogMessageData& logMessageData);
		void WriteLogMessageToOutput(const SLogMessageData& logMessageData);

		QBoxLayout*					m_pMainLayout;
		QSplitter*					m_pSplitter;
		CLogSettingsWidget* m_pSettings;
		QTextBrowser*       m_pOutput;
		QBoxLayout*         m_pControlLayout;
		QPushButton*        m_pShowHideSettingsButton;
		QPushButton*        m_pClearOutputButton;

		PendingMessages                 m_pendingMessages;
		TemplateUtils::CConnectionScope m_connectionScope;
	};

	class CLogWnd : public CWnd
	{
		DECLARE_MESSAGE_MAP()

	public:

		CLogWnd();

		~CLogWnd();

		void Init();
		void InitLayout();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);

	private:

		afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
		afx_msg void OnSize(UINT nType, int cx, int cy);

		QParentWndWidget*	m_pParentWndWidget;
		CLogWidget*				m_pLogWidget;
		QBoxLayout*				m_pLayout;
	};
}
