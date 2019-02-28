// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem/Internal/FileSystem_Internal_UpdateSequence.h"

#include <QObject>

#include <memory>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

/**
 * \brief simple wrapper interface that creates a normal QObject for the monitor threads
 */
class CMonitor : public QObject
{
	Q_OBJECT
public:
	explicit CMonitor(QObject* parent = 0);
	~CMonitor();

	/// \param path is an absolute filesystem path
	bool AddPath(const SAbsolutePath& absolutePath);

	/// \brief top monitoring the path represented by the key
	bool RemovePath(const QString& absoluteKeyPath);

signals:
	void LostTrack(const QString& fullPath);
	void Updates(const FileSystem::Internal::CUpdateSequence&);

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

