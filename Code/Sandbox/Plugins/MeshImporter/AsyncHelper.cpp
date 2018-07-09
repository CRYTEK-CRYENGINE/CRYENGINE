// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Helper for async worker thread
#include "StdAfx.h"
#include "AsyncHelper.h"
#include "QtUtil.h"
#include "Expected.h"
#include <QObject>
#include <QThread>
#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QProgressBar>

#include "Notifications/NotificationCenter.h"
#include "DialogCommon.h"

static int s_idTaskProgressEvent = 0;
static int s_idTaskNotifyEvent = 0;
static int s_idTaskFinishedEvent = 0;

CProgressNotificationStack::SNotification::SNotification(CProgressNotificationStack* pOwner)
	: m_pOwner(pOwner)
{
	CRY_ASSERT(m_pOwner);
}

CProgressNotificationStack::SNotification::~SNotification()
{
	m_pOwner->Remove(this);
}

void CProgressNotificationStack::SNotification::SetMessage(const QString& message)
{
	m_message = message;
}

QString CProgressNotificationStack::SNotification::GetMessage() const
{
	return m_message;
}

std::unique_ptr<CProgressNotificationStack::SNotification> CProgressNotificationStack::CreateNotification(const QString& message)
{
	std::unique_ptr<SNotification> pNotification(new SNotification(this));
	pNotification->SetMessage(message);
	m_notifications.push_back(pNotification.get());
	UpdateSystemNotification();
	return std::move(pNotification);
}

void CProgressNotificationStack::Hide()
{
	m_pSysNotification.reset();
}

void CProgressNotificationStack::Remove(SNotification* pNotification)
{
	auto it = std::find(m_notifications.begin(), m_notifications.end(), pNotification);
	CRY_ASSERT(it != m_notifications.end());
	m_notifications.erase(it);
	UpdateSystemNotification();
}

void CProgressNotificationStack::UpdateSystemNotification()
{
	if (m_notifications.empty())
	{
		Hide();
		return;
	}

	const QString message = m_notifications.front()->GetMessage();

	if (!m_pSysNotification)
	{
		m_pSysNotification.reset(new CProgressNotification(GetTitle(), message, false));
	}
	else
	{
		m_pSysNotification->SetMessage(message);
	}
}

struct STaskProgressEvent : public QEvent
{
	STaskProgressEvent() : QEvent((Type)s_idTaskProgressEvent) {}

	CAsyncTaskBase* pTask;
	QString         what;
	int             progress;
};

struct STaskNotifyEvent : public QEvent
{
	enum ENotificationType
	{
		eNotificationType_Warning = 0,
		eNotificationType_Error
	};

	const CAsyncTaskBase* pTask;
	const QString         what;
	const int             type;

	STaskNotifyEvent(const CAsyncTaskBase* pTask, const QString& what, int type)
		: QEvent((Type)s_idTaskNotifyEvent)
		, pTask(pTask)
		, what(what)
		, type(type)
	{}
};

struct STaskFinishedEvent : public QEvent
{
	STaskFinishedEvent() : QEvent((Type)s_idTaskFinishedEvent) {}

	CAsyncTaskBase* pTask;
	bool            bResult;
};

struct SProgressEventFilter : public QObject
{
	SProgressEventFilter(CAsyncTaskHost* pHost)
		: pHost(pHost) {}

	bool eventFilter(QObject* pTarget, QEvent* pBaseEvent) override
	{
		if (pBaseEvent->type() == s_idTaskProgressEvent)
		{
			const STaskProgressEvent* const pEvent = (STaskProgressEvent*)pBaseEvent;
			pHost->OnTaskProgress(pEvent->pTask, pEvent->what, pEvent->progress);
			return true;
		}
		else if (pBaseEvent->type() == s_idTaskNotifyEvent)
		{
			const STaskNotifyEvent* const pEvent = (STaskNotifyEvent*)pBaseEvent;
			pHost->OnTaskNotify(pEvent);
			return true;
		}
		else if (pBaseEvent->type() == s_idTaskFinishedEvent)
		{
			const STaskFinishedEvent* const pEvent = (STaskFinishedEvent*)pBaseEvent;
			pHost->FinishTask(pEvent->pTask, pEvent->bResult);
			return true;
		}
		return false;
	}

	CAsyncTaskHost* const pHost;
};

struct SWorkerThread : public QThread
{
	SWorkerThread(QObject* pNotify, CAsyncTaskBase* pTask)
		: pNotify(pNotify), pTask(pTask) {}

	virtual ~SWorkerThread() {}

	virtual void run() override
	{
		const bool bResult = pTask->PerformTask();

		STaskFinishedEvent* pEvent = new STaskFinishedEvent();
		pEvent->pTask = pTask;
		pEvent->bResult = bResult;
		QCoreApplication::postEvent(pNotify, pEvent);
	}

	QObject* const        pNotify;
	CAsyncTaskBase* const pTask;
};

void CAsyncTaskHost::Init()
{
	if (m_pParent)
	{
		if (s_idTaskProgressEvent == 0)
		{
			s_idTaskProgressEvent = QEvent::registerEventType();
		}
		if (s_idTaskNotifyEvent == 0)
		{
			s_idTaskNotifyEvent = QEvent::registerEventType();
		}
		if (s_idTaskFinishedEvent == 0)
		{
			s_idTaskFinishedEvent = QEvent::registerEventType();
		}

		m_pFilter = new SProgressEventFilter(this);
		m_pParent->installEventFilter(m_pFilter);
	}
}

void CAsyncTaskHost::Finalize()
{
	// Finalize all pending tasks.
	// Failure to do so may cause access violation.
	while (!m_tasks.empty())
	{
		CAsyncTaskBase* pTask = m_tasks.back().first;
		JoinTask(pTask, false);
	}

	if (m_pFilter)
	{
		m_pParent->removeEventFilter(m_pFilter);
		delete m_pFilter;
	}
}

void CAsyncTaskHost::SetProgress(CAsyncTaskBase* pTask, const QString& what, int progress)
{
	if (m_pParent)
	{
		STaskProgressEvent* pEvent = new STaskProgressEvent();
		pEvent->pTask = pTask;
		pEvent->what = what;
		pEvent->progress = progress;
		QCoreApplication::postEvent(m_pParent, pEvent);
	}
}

void CAsyncTaskHost::ShowWarning(const CAsyncTaskBase* pTask, const QString& what) const
{
	if (m_pParent)
	{
		STaskNotifyEvent* pEvent = new STaskNotifyEvent(pTask, what, STaskNotifyEvent::eNotificationType_Warning);
		QCoreApplication::postEvent(m_pParent, pEvent);
	}
}

void CAsyncTaskHost::ShowError(const CAsyncTaskBase* pTask, const QString& what) const
{
	if (m_pParent)
	{
		STaskNotifyEvent* pEvent = new STaskNotifyEvent(pTask, what, STaskNotifyEvent::eNotificationType_Warning);
		QCoreApplication::postEvent(m_pParent, pEvent);
	}
}

bool CAsyncTaskHost::RunTask(CAsyncTaskBase* pTask)
{
	if (m_pParent && pTask && pTask->InitializeTask())
	{
		pTask->m_pHost = this;
		OnTaskLaunched(pTask);

		QThread* const pThread = new SWorkerThread(m_pParent, pTask);
		m_tasks.emplace_back(pTask, pThread);
		pThread->start();
		return true;
	}
	else if (pTask)
	{
		pTask->FinishTask(false);
	}
	return false;
}

void CAsyncTaskHost::JoinTask(CAsyncTaskBase* pTask, bool bResult)
{
	for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it)
	{
		if (it->first == pTask)
		{
			EXPECTED(it->second->wait());
			delete it->second;
			m_tasks.erase(it);
			return;
		}
	}
	assert(0); // The task should always be in the collection
}

void CAsyncTaskHost::FinishTask(CAsyncTaskBase* pTask, bool bResult)
{
	for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it)
	{
		if (it->first == pTask)
		{
			EXPECTED(it->second->wait());
			delete it->second;
			m_tasks.erase(it);

			OnTaskFinished(pTask, bResult);
			pTask->FinishTask(bResult);

			return;
		}
	}
	assert(0); // The task should always be in the collection
}

void CAsyncTaskBase::UpdateProgress(const QString& what, int progress)
{
	assert(m_pHost);
	m_pHost->SetProgress(this, what, progress);
}

void CAsyncTaskBase::ShowWarning(const QString& what) const
{
	assert(m_pHost);
	m_pHost->ShowWarning(this, what);
}

void CAsyncTaskBase::ShowError(const QString& what) const
{
	assert(m_pHost);
	m_pHost->ShowError(this, what);
}

CAsyncTaskHostForDialog::CAsyncTaskHostForDialog(MeshImporter::CBaseDialog* pDialog)
	: CAsyncTaskHost(pDialog)
	, m_pDialog(pDialog)
{}

CAsyncTaskHostForDialog::~CAsyncTaskHostForDialog()
{
	Finalize();
}

void CAsyncTaskHostForDialog::OnTaskLaunched(CAsyncTaskBase* pTask) {}

void CAsyncTaskHostForDialog::OnTaskFinished(CAsyncTaskBase* pTask, bool bResult)
{
	auto it = m_notifications.find(pTask);
	if (it != m_notifications.end())
	{
		it->second.reset();
		m_notifications.erase(it);
	}
}

void CAsyncTaskHostForDialog::OnTaskProgress(CAsyncTaskBase* pTask, const QString& message, int progress)
{
	if (m_notifications.find(pTask) != m_notifications.end())
	{
		return;
	}
	else
	{
		m_notifications[pTask] = CreateNotification(message);
	}
}

void CAsyncTaskHostForDialog::OnTaskNotify(const STaskNotifyEvent* pEvent)
{
	if (pEvent->type == STaskNotifyEvent::eNotificationType_Warning)
	{
		GetIEditor()->GetNotificationCenter()->ShowWarning(GetTitle(), pEvent->what);
	}
	else if (pEvent->type == STaskNotifyEvent::eNotificationType_Error)
	{
		GetIEditor()->GetNotificationCenter()->ShowWarning(GetTitle(), pEvent->what);
	}
}

QString CAsyncTaskHostForDialog::GetTitle() const
{
	return m_pDialog->GetDialogName();
}

CSyncTaskHost::CSyncTaskHost()
	: m_title("Mesh Importer")
{}

void CSyncTaskHost::SetTitle(const QString& title)
{
	m_title = title;
}

bool CSyncTaskHost::RunTask(CAsyncTaskBase* pTask)
{
	if (!pTask || !pTask->InitializeTask())
	{
		return false;
	}

	pTask->m_pHost = this;

	pTask->FinishTask(pTask->PerformTask());

	return true;
}

void CSyncTaskHost::ShowWarning(const CAsyncTaskBase* pTask, const QString& what) const
{
	GetIEditor()->GetNotificationCenter()->ShowWarning(m_title, what);
}

void CSyncTaskHost::ShowError(const CAsyncTaskBase* pTask, const QString& what) const
{
	GetIEditor()->GetNotificationCenter()->ShowCritical(m_title, what);
}

