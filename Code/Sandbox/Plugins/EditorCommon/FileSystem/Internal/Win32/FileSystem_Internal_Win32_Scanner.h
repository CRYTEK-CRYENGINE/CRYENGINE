// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem/Internal/FileSystem_Internal_Data.h"

#include <QObject>

#include <memory>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

/**
 * \brief scanner implementation for Win32-API
 * \note instances are moved to the internal thread
 */
class CScanner
	: public QObject
{
	Q_OBJECT
public:
	CScanner();
	~CScanner();

public slots:
	/// will start to scan this directory in the background
	void ScanDirectoryRecursiveInBackground(const QString& fullPath);

	/// scans this directory with priority (use if user is wating)
	void ScanDirectoryPreferred(const QString& fullPath);

signals:
	void ScanResults(const FileSystem::Internal::SScanResult& scanResult);
	void IsActiveChanged(bool);

protected:
	void timerEvent(QTimerEvent* event) override;

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

