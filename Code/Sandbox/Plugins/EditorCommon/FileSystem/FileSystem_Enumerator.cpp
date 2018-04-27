// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Enumerator.h"

#include "FileSystem/Internal/FileSystem_Internal_WorkerApi.h"

#include "FileSystem/FileSystem_DirectoryFilter.h"
#include "FileSystem/FileSystem_FileFilter.h"

#include <QSharedPointer>

#include <atomic>

#include "FilePathUtil.h"

namespace FileSystem
{

struct CEnumerator::Implementation
{
	std::atomic<int>                     m_monitorHandle;
	QString                              m_projectBasePath;
	QSharedPointer<Internal::CWorkerApi> m_workerApi;

	Implementation(const QString& absoluteProjectPath)
		: m_monitorHandle(0)
		, m_projectBasePath(absoluteProjectPath)
		, m_workerApi(new Internal::CWorkerApi(absoluteProjectPath))
	{}
};

CEnumerator::CEnumerator(const QString& absoluteProjectPath)
	: p(new Implementation(PathUtil::ToUnixPath(absoluteProjectPath)))
{
	connect(p->m_workerApi.data(), &Internal::CWorkerApi::IsScannerActiveChanged, this, &CEnumerator::IsScannerActiveChanged);
}

CEnumerator::~CEnumerator()
{}

const QString& CEnumerator::GetProjectRootPath() const
{
	return p->m_projectBasePath;
}

const SnapshotPtr& CEnumerator::GetCurrentSnapshot() const
{
	return p->m_workerApi->GetCurrentSnapshot();
}

bool CEnumerator::IsScannerActive() const
{
	return p->m_workerApi->IsScannerActive();
}

CEnumerator::MonitorHandle CEnumerator::StartFileMonitor(const SFileFilter& filter, const FileMonitorPtr& monitor)
{
	auto handle = ++p->m_monitorHandle;
	auto keyFilter = filter;
	keyFilter.MakeInputValid();
	p->m_workerApi->StartFileMonitor(handle, keyFilter, monitor);
	return handle;
}

void CEnumerator::StopFileMonitor(MonitorHandle handle)
{
	p->m_workerApi->StopFileMonitor(handle);
}

CEnumerator::MonitorHandle CEnumerator::StartSubTreeMonitor(const SFileFilter& filter, const SubTreeMonitorPtr& monitor)
{
	auto handle = ++p->m_monitorHandle;
	auto keyFilter = filter;
	keyFilter.MakeInputValid();
	p->m_workerApi->StartSubTreeMonitor(handle, keyFilter, monitor);
	return handle;
}

void CEnumerator::StopSubTreeMonitor(MonitorHandle handle)
{
	p->m_workerApi->StopSubTreeMonitor(handle);
}

void CEnumerator::RegisterFileTypes(const QVector<const SFileType*>& fileTypes)
{
	p->m_workerApi->RegisterFileTypes(fileTypes);
}

} // namespace FileSystem

