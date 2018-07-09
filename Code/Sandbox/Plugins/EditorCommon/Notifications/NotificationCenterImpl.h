// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

// Qt
#include <QObject>
#include <QMutex>

// CryCommon
#include <CrySystem/ILog.h>
#include "EditorCommonAPI.h"

#include "Internal/Notifications_Internal.h"
#include "EditorFramework/Preferences.h"
#include "NotificationCenter.h"

// Preferences
struct SNotificationPreferences : public SPreferencePage
{
	SNotificationPreferences()
		: SPreferencePage("Notifications", "Notifications/General")
		, m_maxNotificationCount(25)
		, m_combineNotifications(true)
		, m_allowPopUps(true)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override
	{
		ar.openBlock("general", "General");
		ar(m_allowPopUps, "allowPopUps", "Allow Pop-Ups");
		ar(m_combineNotifications, "combineNotifications", "Combine Notifications");
		ar(m_maxNotificationCount, "maxNotificationCount", m_combineNotifications ? "Max Notification Count" : "!Max Notification Count");
		ar.closeBlock();
		return true;
	}

	ADD_PREFERENCE_PAGE_PROPERTY(int, maxNotificationCount, setMaxNotificationCount)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, allowPopUps, setAllowPopUps)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, combineNotifications, setCombineNotifications)
};

extern SNotificationPreferences gNotificationPreferences;

// Using Qt's signals and slots instead of Crysignal because connections are, by default, executed on the thread
// where the receiver lives. This is done because we can only create UI in the main thread. For more info:
// http://doc.qt.io/qt-5/qt.html#ConnectionType-enum and http://doc.qt.io/qt-5/thread-basics.html

class EDITOR_COMMON_API CNotificationCenter : public QObject, public INotificationCenter, public IEditorNotifyListener, public ILogCallback
{
	Q_OBJECT
public:
	CNotificationCenter(QObject* pParent = nullptr);
	~CNotificationCenter();

	// Required to hook into the log after EditorCommon has been initialized
	virtual void                           Init() override;

	virtual void                           OnEditorNotifyEvent(EEditorNotifyEvent ev) override;

	virtual int                            ShowInfo(const QString& title, const QString& message) override;
	virtual int                            ShowWarning(const QString& title, const QString& message) override;
	virtual int                            ShowCritical(const QString& title, const QString& message) override;
	virtual int                            ShowProgress(const QString& title, const QString& message, bool bShowProgress = false) override;

	virtual void                           OnWriteToConsole(const char* szMessage, bool bNewLine) override;
	virtual void                           OnWriteToFile(const char* szMessage, bool bNewLine) override;

	std::vector<Internal::CNotification*>& GetNotifications()            { return m_notifications; }
	size_t                                 GetNotificationCount() const  { return m_notifications.size(); }
	Internal::CNotification*               GetNotification(int id) const { return id >= 0 && id < m_notifications.size() ? m_notifications[id] : nullptr; }

signals:
	void NotificationAdded(int id);

protected:
	int AddNotification(Internal::CNotification* notification);
	int AddNotification(Internal::CNotification::Type type, const QString& title, const QString& message);

protected:
	QMutex notificationsLock;
	std::vector<Internal::CNotification*> m_notifications;
};

