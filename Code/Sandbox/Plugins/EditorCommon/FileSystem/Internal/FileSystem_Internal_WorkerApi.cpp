// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_WorkerApi.h"

#include "FileSystem_Internal_Worker.h"

namespace FileSystem
{
namespace Internal
{

CWorkerApi::CWorkerApi(const QString& absoluteEnginePath, QObject* parent)
	: QObject(parent)
	, m_isScannerActive(false)
	, m_worker(new CWorker(absoluteEnginePath))
{
	m_worker->moveToThread(&m_thread);
	connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
	connect(this, &CWorkerApi::RegisterFileTypes, m_worker, &CWorker::RegisterFileTypes);
	connect(this, &CWorkerApi::StartFileMonitor, m_worker, &CWorker::StartFileMonitor);
	connect(this, &CWorkerApi::StopFileMonitor, m_worker, &CWorker::StopFileMonitor);
	connect(this, &CWorkerApi::StartSubTreeMonitor, m_worker, &CWorker::StartSubTreeMonitor);
	connect(this, &CWorkerApi::StopSubTreeMonitor, m_worker, &CWorker::StopSubTreeMonitor);

	connect(m_worker, &CWorker::TriggerSetSnapshot, this, &CWorkerApi::SetCurrentSnapshot);
	connect(m_worker, &CWorker::TriggerSetIsScannerActive, this, &CWorkerApi::SetIsScannerActive);
	connect(m_worker, &CWorker::TriggerFileMonitorActivated, this, &CWorkerApi::TriggerFileMonitorActivated);
	connect(m_worker, &CWorker::TriggerFileMonitorUpdate, this, &CWorkerApi::TriggerFileMonitorUpdate);
	connect(m_worker, &CWorker::TriggerSubTreeMonitorActivated, this, &CWorkerApi::TriggerSubTreeMonitorActivated);
	connect(m_worker, &CWorker::TriggerSubTreeMonitorUpdate, this, &CWorkerApi::TriggerSubTreeMonitorUpdate);

	m_thread.start();
	m_worker->Start();
}

CWorkerApi::~CWorkerApi()
{
	m_thread.quit();
	m_thread.wait();
}

void CWorkerApi::SetCurrentSnapshot(const SnapshotPtr& snapshot)
{
	m_currentSnapshot = snapshot;
}

void CWorkerApi::SetIsScannerActive(bool active)
{
	if (m_isScannerActive == active)
	{
		return;
	}
	m_isScannerActive = active;
	IsScannerActiveChanged(active);
}

void CWorkerApi::TriggerFileMonitorActivated(const FileMonitorPtr& monitor, const SnapshotPtr& snapshot)
{
	monitor->Activated(snapshot);
}

void CWorkerApi::TriggerFileMonitorUpdate(const FileMonitorPtr& monitor, const SFileMonitorUpdate& update)
{
	monitor->Update(update);
}

void CWorkerApi::TriggerSubTreeMonitorActivated(const SubTreeMonitorPtr& monitor, const SnapshotPtr& snapshot)
{
	monitor->Activated(snapshot);
}

void CWorkerApi::TriggerSubTreeMonitorUpdate(const SubTreeMonitorPtr& monitor, const SSubTreeMonitorUpdate& update)
{
	monitor->Update(update);
}

bool CWorkerApi::IsScannerActive() const
{
	return m_isScannerActive;
}

} // namespace Internal
} // namespace FileSystem

