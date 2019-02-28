// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QObject>
#include <QDateTime>
#include <IEditor.h>

class CNotificationCenter;

namespace Internal
{

// Using Qt's signals and slots instead of Crysignal because connections are, by default, executed on the thread
// where the receiver lives. This is done because we can only create UI in the main thread. For more info:
// http://doc.qt.io/qt-5/qt.html#ConnectionType-enum and http://doc.qt.io/qt-5/thread-basics.html

class CNotification : public QObject
{
	Q_OBJECT
	friend CNotificationCenter;// only one able to create/set id
public:
	enum Type
	{
		Info,
		Warning,
		Critical,
		Progress
	};

	const QString&   GetTitle() const     { return m_title; }
	const QString&   GetMessage() const   { return m_message; }
	const QDateTime& GetDateTime() const  { return m_date; }
	Type             GetType() const      { return m_type; }
	int              GetId() const        { return m_id; }
	bool             IsLogMessage() const { return m_bIsLogMessage; }
	bool             IsHidden() const     { return m_bHidden; }
	void Hide()
	{
		if (m_bHidden)
			return;

		m_bHidden = true;
		Hidden();
	}

	// Returns message with timestamp and commands that it executes
	QString GetDetailedMessage()
	{
		QString message(m_date.time().toString() + " " + m_message);
		for (const QString& command : m_commands)
		{
			message += command + " ";
		}

		return message;
	}

	void AddCommand(const QString& command) { m_commands.push_back(command); }
	void Execute()
	{
		for (const QString& command : m_commands)
		{
			GetIEditor()->ExecuteCommand(command.toLocal8Bit());
		}
	}

	int GetCommandCount() const { return m_commands.size(); }
	const QVector<QString>& GetCommands() const { return m_commands; }

	void SetMessage(const QString& message)
	{
		if (message != GetMessage())
		{
			m_message = message;
			MessageChanged(message);
		}
	}

protected:
	CNotification(Type type, const QString& title, const QString& message, bool bIsLogMessage = false)
		: m_title(title)
		, m_message(message)
		, m_type(type)
		, m_id(-1)
		, m_bIsLogMessage(bIsLogMessage)
		, m_bHidden(false)
	{
		m_message = m_message.remove(QRegExp("^\n|\n$"));
	}

	void SetId(int id)                      { m_id = id; }
	void SetDateTime(const QDateTime& date) { m_date = date; }

signals:
	void Hidden();
	void MessageChanged(const QString& message);

protected:
	QVector<QString> m_commands;
	QString   m_title;
	QString   m_message;
	QDateTime m_date;
	Type      m_type;
	int       m_id;
	bool      m_bIsLogMessage;
	bool      m_bHidden;
};

class CProgressNotification : public CNotification
{
	Q_OBJECT
	friend CNotificationCenter; // only one able to create/set id
public:
	// Set progress [0.0, 1.0]. Anything above 1.0 will mark the task as complete
	void SetProgress(float progress)
	{
		m_progress = progress;
		if (m_bShowProgress)
			ProgressChanged(m_progress);

		if (IsTaskComplete())
			TaskComplete();
	}

	bool  ShowProgress() const { return m_bShowProgress; }
	float GetProgress() const  { return m_progress; }

	bool IsTaskComplete() { return m_progress >= 1.0f; }

signals:
	void TaskComplete();
	void ProgressChanged(float currProgress);

protected:
	CProgressNotification(const QString& title, const QString& message, bool bShowProgress = false)
		: CNotification(Progress, title, message)
		, m_bShowProgress(bShowProgress)
		, m_progress(0)
	{
	}

protected:
	float m_progress; // 0.0 to 1.0
	bool  m_bShowProgress;
};

} // namespace Internal

