// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/File/IFileChangeMonitor.h>
#include <functional>
#include <deque>

namespace AssetManagerHelpers
{

//! Monitors creation of new pak files, creates cryassets for asset data files located in paks.
class CPakImporter : public IFileChangeListener
{
public:
	static void RegisterFileListener();
	virtual void OnFileChange(const char* szFilename, EChangeType changeType) override;

private:
	void ProcessQueue();
	bool TryToProcessPak(const string& pak);
	bool IsPakOpened(const string& pakPath);
	static void ImportAssets(const string& pak);
	static string GetTemporaryDirectoryPath();
	static void Unpak(const char* szArchivePath, const char* szDestPath, std::function<void(float)> progress);
	static void GenerateCryassets(const char* szPath, std::function<void(float)> progress);
	CPakImporter();

private:
	std::deque<string> m_queue;
};

};
