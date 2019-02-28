// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem/FileSystem_SubTreeMonitor.h"
#include "FileSystem/FileSystem_FileMonitor.h"

#include <QObject>
#include <QThread>

namespace FileSystem
{

class CEnumerator;

namespace Internal
{

class CWorker;

/// \brief interface from any thread to worker thread
class CWorkerApi
	: public QObject
{
	Q_OBJECT

public:
	CWorkerApi(const QString& absoluteEnginePath, QObject* parent = nullptr);
	~CWorkerApi();

signals:
	void StartFileMonitor(int handle, const FileSystem::SFileFilter& filter, const FileSystem::FileMonitorPtr&);
	void StopFileMonitor(int handle);

	void StartSubTreeMonitor(int handle, const FileSystem::SFileFilter& filter, const FileSystem::SubTreeMonitorPtr&);
	void StopSubTreeMonitor(int handle);

	void RegisterFileTypes(const QVector<const SFileType*>& fileTypes);
	void IsScannerActiveChanged(bool);

private slots:
	void SetCurrentSnapshot(const SnapshotPtr&);
	void SetIsScannerActive(bool);

	void TriggerFileMonitorActivated(const FileMonitorPtr&, const SnapshotPtr&);
	void TriggerFileMonitorUpdate(const FileMonitorPtr&, const SFileMonitorUpdate&);

	void TriggerSubTreeMonitorActivated(const SubTreeMonitorPtr&, const SnapshotPtr&);
	void TriggerSubTreeMonitorUpdate(const SubTreeMonitorPtr&, const SSubTreeMonitorUpdate&);

private:
	friend class FileSystem::CEnumerator;
	const SnapshotPtr& GetCurrentSnapshot() const { return m_currentSnapshot; }
	bool               IsScannerActive() const;

private:
	bool        m_isScannerActive;
	SnapshotPtr m_currentSnapshot;
	QThread     m_thread;
	CWorker*    m_worker;
};

} // namespace Internal
} // namespace FileSystem

