// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>

#include "EditorCommonAPI.h"

//! On creation, spawns a progress notification widget. the lifetime of the widget is tied to the lifetime
//! of this notification. Example usage:
//! class WorkerThread : public QThread
//! {
//! 	virtual void run() override
//! 	{
//! 		CProgressNotification notification("My Title", "My Message", true);
//! 		for (int i = 0; i < 101; ++i)
//! 		{
//! 			float progress = static_cast<float>(i) * 0.01f;
//! 			notification.SetProgress(progress);
//! 			msleep(20);
//! 		}
//! 	}
//! };
//! 
//! WorkerThread* pWorker = new WorkerThread();
//! pWorker->start();

class EDITOR_COMMON_API CProgressNotification : public QObject
{
public:
	CProgressNotification(const QString& title, const QString& message, bool bShowProgress = false);
	~CProgressNotification();

	// Set progress [0.0, 1.0]. Anything greater or equal to 1.0 will mark the task as complete
	void SetProgress(float value);

	void SetMessage(const QString& message);
public:
	int m_id;
};

//////////////////////////////////////////////////////////////////////////

//! NotificationCenter will log any warnings and errors when using CryWarning.
//! It will also log any asserts (even if asserts are ignored)
//! User can also show any additional info with the provided functions

class EDITOR_COMMON_API INotificationCenter
{
public:
	virtual void Init() = 0;

	virtual int  ShowInfo(const QString& title, const QString& message) = 0;
	virtual int  ShowWarning(const QString& title, const QString& message) = 0;
	virtual int  ShowCritical(const QString& title, const QString& message) = 0;
	virtual int  ShowProgress(const QString& title, const QString& message, bool bShowProgress = false) = 0;
};

