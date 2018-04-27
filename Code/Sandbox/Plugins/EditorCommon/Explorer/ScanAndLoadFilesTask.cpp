// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ScanAndLoadFilesTask.h"
#include <CrySystem/File/ICryPak.h>
#include <CryString/StringUtils.h>
#include "ExplorerFileList.h"

namespace Explorer
{

SScanAndLoadFilesTask::SScanAndLoadFilesTask(const SLookupRule& rule, const char* description)
	: m_rule(rule)
	, m_description(description)
{
}

ETaskResult SScanAndLoadFilesTask::Work()
{
	const SLookupRule& rule = m_rule;
	vector<string> masks = rule.masks;

	vector<string> filenames;

	for (size_t j = 0; j < masks.size(); ++j)
	{
		filenames.clear();
		const string& mask = masks[j];
		SDirectoryEnumeratorHelper dirHelper;
		dirHelper.ScanDirectoryRecursive("", "", mask.c_str(), filenames);

		m_loadedFiles.reserve(m_loadedFiles.size() + filenames.size());
		for (int k = 0; k < filenames.size(); ++k)
		{
			if (!CryStringUtils::MatchWildcard(filenames[k].c_str(), mask.c_str()))
				continue;

			ScanLoadedFile file;
			file.scannedFile = filenames[k].c_str();
			file.pakState = ExplorerFileList::GetFilePakState(file.scannedFile.c_str());
			m_loadedFiles.push_back(file);
		}
	}

	return eTaskResult_Completed;
}

void SScanAndLoadFilesTask::Finalize()
{
	for (size_t i = 0; i < m_loadedFiles.size(); ++i)
	{
		const ScanLoadedFile& loadedFile = m_loadedFiles[i];

		SignalFileLoaded(loadedFile);
	}

	SignalLoadingFinished();
}

}

