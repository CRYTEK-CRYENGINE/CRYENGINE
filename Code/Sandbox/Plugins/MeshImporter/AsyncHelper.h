// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Helper for async worker thread interacting with Qt UI
#pragma once

class QObject;
class QThread;
class QString;
class QWidget;
class QHBoxLayout;
class QMainWindow;
class QProgressBar;
class CAsyncTaskBase;

class CProgressNotification;
namespace MeshImporter {
class CBaseDialog;
}

struct STaskNotifyEvent;

class CProgressNotificationStack
{
public:
	struct SNotification
	{
		explicit SNotification(CProgressNotificationStack* pOwner);
		virtual ~SNotification();

		void    SetMessage(const QString& message);

		QString GetMessage() const;
	private:
		CProgressNotificationStack* const m_pOwner;
		QString                           m_message;
	};

	std::unique_ptr<SNotification> CreateNotification(const QString& message);
	void                           Hide();

	virtual QString                GetTitle() const = 0;
private:
	void                           Remove(SNotification* pNotification);

	void                           UpdateSystemNotification();

	std::vector<SNotification*>            m_notifications;
	std::unique_ptr<CProgressNotification> m_pSysNotification;
};

struct ITaskHost
{
	virtual ~ITaskHost() {}

	virtual bool RunTask(CAsyncTaskBase* pTask) = 0;

	// Called by tasks.
	virtual void SetProgress(CAsyncTaskBase* pTask, const QString& what, int progress) = 0;
	virtual void ShowWarning(const CAsyncTaskBase* pTask, const QString& what) const = 0;
	virtual void ShowError(const CAsyncTaskBase* pTask, const QString& what) const = 0;
};

// Abstract task host.
// Task host is the object on the Qt main thread.
class CAsyncTaskHost : public ITaskHost
{
protected:
	CAsyncTaskHost(QObject* pParent)
		: m_pParent(pParent), m_pFilter(nullptr)
	{
		Init();
	}

	virtual ~CAsyncTaskHost() {}

	void Finalize();

	// Called when task is launched or is finished.
	// This could block/unblock the UI or some other function.
	virtual void OnTaskLaunched(CAsyncTaskBase* pTask) = 0;
	virtual void OnTaskFinished(CAsyncTaskBase* pTask, bool bResult) = 0;
	virtual void OnTaskProgress(CAsyncTaskBase* pTask, const QString& what, int progress) = 0;
	virtual void OnTaskNotify(const STaskNotifyEvent* pEvent) = 0;

public:
	// Progress message helpers, can be called from main or worker thread.
	// The provided information is marshaled to the main thread automatically for display.
	virtual void SetProgress(CAsyncTaskBase* pTask, const QString& what, int progress) override;
	virtual void ShowWarning(const CAsyncTaskBase* pTask, const QString& what) const override;
	virtual void ShowError(const CAsyncTaskBase* pTask, const QString& what) const override;

	// Launches the given task on this host.
	// Must be called from the main thread.
	virtual bool RunTask(CAsyncTaskBase* pTask) override;

	// Tests if any tasks are currently running.
	// Must be called from the main thread.
	bool IsIdle() const
	{
		return m_tasks.empty();
	}

private:
	void Init();
	void JoinTask(CAsyncTaskBase* pTask, bool bResult);
	void FinishTask(CAsyncTaskBase* pTask, bool bResult);

	friend struct SProgressEventFilter;
	QObject* const m_pParent;
	QObject*       m_pFilter;
	std::vector<std::pair<CAsyncTaskBase*, QThread*>> m_tasks;
};

// Base class for an async task.
// Tasks can derive this class so they can be run inside Qt.
class CAsyncTaskBase
{
public:
	CAsyncTaskBase()
		: m_pHost(nullptr) {}

	virtual ~CAsyncTaskBase() {}

	// Initializes the task (called on main thread)
	// If the function fails, PerformTask() is never called.
	virtual bool InitializeTask() { return true; }

	// Performs the task (called on worker thread)
	// Usually should not touch the UI state.
	virtual bool PerformTask() = 0;

	// Finishes the task (called on main thread)
	// The parameter passed in is the return value of PerformTask
	// Note: It's specifically allowed for the implementation to end with "delete this;"
	virtual void FinishTask(bool bResult) = 0;

	// Tests if this task blocks (disabled) the UI while it's running.
	// This is used by CAsyncTaskBaseHostForMainWindow to determine if the main window gets greyed out.
	virtual bool BlocksUI() const { return false; }

	// Updates the progress of the task (can be called from either thread).
	//
	// Progress is either quantized or non-quantized. A progress is quantized if there is a number
	// in the range [0, 100] indicating the percentage of completion. If it is not possible (or feasible)
	// to show such a number, progress is said to be non-quantized.
	// Non-quantized progress is used to merely indicate that some action is currently being
	// executed.
	//
	// A value of 'progress' in the range [0, 100] means quantized progress. Otherwise the progress
	// is non-quantized and the concrete value of 'progress' is irrelevant.
	void        UpdateProgress(const QString& what, int progress);

	void        ShowWarning(const QString& what) const;
	void        ShowError(const QString& what) const;

	static void CallBlocking(CAsyncTaskBase* pTask)
	{
		bool bSucceeded = false;
		if (pTask->InitializeTask())
		{
			bSucceeded = pTask->PerformTask();
		}
		pTask->FinishTask(bSucceeded);
	}
private:
	friend class CAsyncTaskHost;
	friend class CSyncTaskHost;
	ITaskHost* m_pHost;
};

class CAsyncTaskHostForDialog : public CAsyncTaskHost, private CProgressNotificationStack
{
public:
	explicit CAsyncTaskHostForDialog(MeshImporter::CBaseDialog* pDialog);
	~CAsyncTaskHostForDialog();

	virtual void    OnTaskLaunched(CAsyncTaskBase* pTask) override;
	virtual void    OnTaskFinished(CAsyncTaskBase* pTask, bool bResult) override;
	virtual void    OnTaskProgress(CAsyncTaskBase* pTask, const QString& message, int progress) override;
	virtual void    OnTaskNotify(const STaskNotifyEvent* pEvent) override;
private:
	virtual QString GetTitle() const;

	MeshImporter::CBaseDialog* m_pDialog;
	std::map<CAsyncTaskBase*, std::unique_ptr<SNotification>> m_notifications;
};

class CSyncTaskHost : public ITaskHost
{
public:
	CSyncTaskHost();

	void SetTitle(const QString& title);

	// ITaskHost implementation.
	virtual bool RunTask(CAsyncTaskBase* pTask) override;
	virtual void SetProgress(CAsyncTaskBase* pTask, const QString& what, int progress) override {}
	virtual void ShowWarning(const CAsyncTaskBase* pTask, const QString& what) const override;
	virtual void ShowError(const CAsyncTaskBase* pTask, const QString& what) const override;
private:
	QString m_title;
};

