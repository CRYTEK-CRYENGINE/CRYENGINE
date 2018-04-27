// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>
#include "EditorCommonAPI.h"
#include <CrySystem/ISystem.h>
#include <IBackgroundTaskManager.h>
#include <QObject>
#include <vector>

namespace Explorer
{

using std::vector;

struct SLookupRule
{
	vector<string> masks;

	SLookupRule()
	{
	}
};

struct ScanLoadedFile
{
	string       scannedFile;
	string       loadedFile;
	vector<char> content;
	int          pakState;  //  One or more values from ICryPak::EFileSearchLocation

	ScanLoadedFile() : pakState(0) {}
};

struct SScanAndLoadFilesTask : QObject, IBackgroundTask
{
	Q_OBJECT
public:
	SLookupRule m_rule;
	vector<ScanLoadedFile> m_loadedFiles;
	string                 m_description;

	SScanAndLoadFilesTask(const SLookupRule& rule, const char* description);
	ETaskResult Work() override;
	void        Finalize() override;
	const char* Description() const { return m_description; }
	void        Delete() override   { delete this; }

signals:
	void SignalFileLoaded(const ScanLoadedFile& loadedFile);
	void SignalLoadingFinished();
};

}

