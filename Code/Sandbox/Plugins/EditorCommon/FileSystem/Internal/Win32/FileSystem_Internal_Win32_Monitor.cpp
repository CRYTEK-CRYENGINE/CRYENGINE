// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Win32_Monitor.h"

#include "FileSystem_Internal_Win32_ActionMonitor.h"
#include "FileSystem_Internal_Win32_MonitorAdapter.h"

#include <QSharedPointer>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

struct CMonitor::Implementation
{
	Implementation(CMonitor* api)
		: m_api(api)
		, m_actionMonitor(new CActionMonitor)
		, m_adapter(new CMonitorAdapter)
		, m_handleCount(0)
	{
		qRegisterMetaType<SActionSequence>();

		QObject::connect(m_actionMonitor.data(), &CActionMonitor::SendChanges, m_adapter.data(), &CMonitorAdapter::Enqueue, Qt::QueuedConnection);
		QObject::connect(m_actionMonitor.data(), &CActionMonitor::SendLostTrack, m_api, &CMonitor::LostTrack, Qt::QueuedConnection);
		QObject::connect(m_adapter.data(), &CMonitorAdapter::SendUpdates, m_api, &CMonitor::Updates, Qt::QueuedConnection);
	}

	bool AddPath(const SAbsolutePath& absolutePath)
	{
		if (m_pathHandle.contains(absolutePath.key))
		{
			return false; // not added
		}
		auto handle = ++m_handleCount;
		m_pathHandle[absolutePath.key] = handle;
		m_actionMonitor->AddPath(handle, absolutePath.full);
		return true;
	}

	bool RemovePath(const QString& absoluteKeyPath)
	{
		auto it = m_pathHandle.find(absoluteKeyPath);
		if (it == m_pathHandle.end())
		{
			return false; // not removed
		}
		int handle = it.value();
		m_actionMonitor->Remove(handle);
		m_pathHandle.erase(it);
		return true;
	}

	CMonitor*                       m_api;
	QSharedPointer<CActionMonitor>  m_actionMonitor;
	QSharedPointer<CMonitorAdapter> m_adapter;

	QHash<QString, int>             m_pathHandle;
	int                             m_handleCount;
};

CMonitor::CMonitor(QObject* parent)
	: QObject(parent)
	, p(new Implementation(this))
{}

CMonitor::~CMonitor()
{}

bool CMonitor::AddPath(const SAbsolutePath& absolutePath)
{
	return p->AddPath(absolutePath);
}

bool CMonitor::RemovePath(const QString& absoluteKeyPath)
{
	return p->RemovePath(absoluteKeyPath);
}

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

