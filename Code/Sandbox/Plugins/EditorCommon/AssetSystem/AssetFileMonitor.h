// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AssetManagerHelpers.h"
#include <CrySystem/File/IFileChangeMonitor.h>

namespace AssetManagerHelpers
{

//! Monitors changes of asset meta-data (.cryasset) files on disk and merges them.
class CAssetFileMonitor : public IFileChangeListener
{
public:
	static void RegisterFileListener();

	// IFileChangeListener implementation.
	virtual void OnFileChange(const char* szFilename, EChangeType changeType) override;

private:
	static void RemoveAsset(const string& assetPath);
	static void LoadAsset(const string& assetPath);
	CAssetFileMonitor();
private:
	CProcessingQueue m_fileQueue;
};

};
