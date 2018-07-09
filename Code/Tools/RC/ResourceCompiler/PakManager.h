// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PAKMANAGER_H__
#define __PAKMANAGER_H__

#include "ZipDir/ZipDir.h"
#include "PakSystem.h"
#include "PakHelpers.h"
#include "RcFile.h"
#include "IProgress.h"

class PakManager
{
public:
	explicit PakManager(IProgress* pProgress);
	~PakManager();

	void RegisterKeys(IResourceCompiler* pRC);
	IPakSystem* GetPakSystem();
	bool HasPakFiles() const;

	void SetMaxThreads(int maxThreads);
	int GetMaxThreads() const;

	// -----------------------------------------------
	enum ECallResult
	{
		eCallResult_Skipped,    // functionality didn't apply and has been skipped
		eCallResult_Succeeded,  // call has been successfull
		eCallResult_Erroneous,  // call has run and ended, but with minor errors (duplicate CRC etc.)
		eCallResult_Failed,     // call has failed - pak files are in inconsistent state
		eCallResult_BadArgs,    // arguments are illformed - pak files have not been touched or changed
	};

	ECallResult CompileFilesIntoPaks(
		const IConfig* config,
		const std::vector<RcFile>& m_allFiles);

	// It's not allowed to pass an absolute path in filesToDelete.
	ECallResult DeleteFilesFromPaks(
		const IConfig* config,
		const std::vector<string>& filesToDelete);

private:
	ECallResult SplitListFileToPaks(
		const IConfig* config, 
		const std::vector<string>& sourceRootsReversed,
		const std::vector<RcFile>& files, 
		const string& pakFilePath);

	ECallResult CreatePakFile(
		const IConfig* config,
		const std::vector<RcFile>& sourceFiles,
		const string& folderInPak,
		const string& requestedPakFilename,
		bool bUpdate);

	ECallResult SynchronizePaks(
		const IConfig* config,
		const std::vector<string>& filesToDelete);

private:
	// All output zip files.
	std::vector<string> m_zipFiles;
	int m_maxThreads;

	PakSystem m_pPakSystem;
	IProgress* m_pProgress;
};

#endif
