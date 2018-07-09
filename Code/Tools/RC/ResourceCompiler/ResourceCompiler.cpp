// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ResourceCompiler.cpp: Defines the entry point for the console application.

#include "stdafx.h"

// Must be included only once in DLL module.
#include <CryCore/Assert/CryAssert_impl.h>
#include <platform_implRC.inl>

#include <time.h>

#include <CryCore/Platform/CryWindows.h>   // needed for DbgHelp.h
#include <DbgHelp.h>

#include <io.h>
#include "ResourceCompiler.h"
#include "Config.h"
#include "CfgFile.h"
#include "FileUtil.h"
#include "IConverter.h"
#include "AssetManager/AssetManager.h"
#include "CrashHandler.h"
#include "CpuInfo.h"
#include <CryString/CryPath.h>
#include "Mailer.h"
#include "StringHelpers.h"
#include "ListFile.h"
#include "Util.h"
#include "ICryXML.h"
#include "IXmlSerializer.h"
#include "MathHelpers.h"
#include "NameConverter.h"
#include <CryCore/CryCrc32.h>
#include "PropertyVars.h"
#include <CryCore/StlUtils.h>
#include "TextFileReader.h"
#include "ZipEncryptor.h"
#include "FileStorage.h"
#include "ScopeTimer.h"
#include "Digest.h"
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

#include <psapi.h> // GetProcessMemoryInfo()
#include <cctype> // tolower
#include <Shellapi.h>

#pragma comment( lib, "Version.lib" )

const char* const ResourceCompiler::m_filenameRcExe            = "rc.exe";
const char* const ResourceCompiler::m_filenameRcIni            = "rc.ini";
const char* const ResourceCompiler::m_filenameOptions          = "rc_options.txt";
const char* const ResourceCompiler::m_filenameLog              = "rc_log.log";
const char* const ResourceCompiler::m_filenameLogWarnings      = "rc_log_warnings.log";
const char* const ResourceCompiler::m_filenameLogErrors        = "rc_log_errors.log";
const char* const ResourceCompiler::m_filenameCrashDump        = "rc_crash.dmp";
const char* const ResourceCompiler::m_filenameOutputFileList   = "rc_outputfiles.txt";
const char* const ResourceCompiler::m_filenameDeletedFileList  = "rc_deletedfiles.txt";
const char* const ResourceCompiler::m_filenameCreatedFileList  = "rc_createdfiles.txt";

namespace
{
	typedef CScopeTimer<1> CCompilerProcessTimer;
	typedef CScopeTimer<2> CCacheAddFilesTimer;
	typedef CScopeTimer<3> CCacheGetFilesTimer;
	typedef CScopeTimer<4> CComputeDigestTimer;
}

//////////////////////////////////////////////////////////////////////////
// ResourceCompiler implementation.
//////////////////////////////////////////////////////////////////////////
ResourceCompiler::ResourceCompiler()
{
	m_bQuiet = false;
	m_platformCount = 0;
	m_memorySizePeakMb = 0;
	m_startTime = 0;
	m_bTimeLogging = false;
	m_progressLastPercent = -1;
	m_verbosityLevel = 0;
	m_bWarningHeaderLine = false;
	m_bErrorHeaderLine = false;
	m_bWarningsAsErrors = false;
	m_systemCpuCoreCount = 0;
	m_processCpuCoreCount = 0;
	m_systemLogicalProcessorCount = 0;
	m_processLogicalProcessorCount = 0;
	m_maxThreads = 0;
	m_pPakManager = 0;
	m_numWarnings = 0;
	m_numErrors = 0;
	m_tlsIndex_pThreadData = 0;
	m_pAssetManager.reset(new CAssetManager(this));

	InitializeThreadIds();
}

ResourceCompiler::~ResourceCompiler()
{
	delete m_pPakManager;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::StartProgress()
{
	m_progressLastPercent = -1;
}

void ResourceCompiler::ShowProgress(const char* pMessage, size_t progressValue, size_t maxProgressValue)
{
	float percent = (progressValue * 100.0f) / maxProgressValue;
	if ((percent <= 100.0f) && (percent != m_progressLastPercent))
	{
		m_progressLastPercent = percent;
		char str[1024];
		cry_sprintf(str, "Progress: %d.%d%% %s", int(percent), int(percent * 10.0f) % 10, pMessage);

		SetConsoleTitle(str);
	}
}

void ResourceCompiler::FinishProgress()
{
	SetConsoleTitle("Progress: 100%");
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::RegisterConverter(const char* name, IConverter* conv)
{
	m_extensionManager.RegisterConverter(name, conv, this);
}

//////////////////////////////////////////////////////////////////////////
IPakSystem* ResourceCompiler::GetPakSystem()
{
	return (m_pPakManager ? m_pPakManager->GetPakSystem() : 0);
}

//////////////////////////////////////////////////////////////////////////
const ICfgFile* ResourceCompiler::GetIniFile() const
{
	return &m_iniFile;
}

//////////////////////////////////////////////////////////////////////////
int ResourceCompiler::GetPlatformCount() const
{
	return m_platformCount;
}

//////////////////////////////////////////////////////////////////////////
const PlatformInfo* ResourceCompiler::GetPlatformInfo(int index) const
{
	if (index < 0 || index > m_platformCount)
	{
		assert(0);
		return 0;
	}
	return &m_platforms[index];
}

//////////////////////////////////////////////////////////////////////////
int ResourceCompiler::FindPlatform(const char* name) const
{
	for (int i = 0; i < m_platformCount; ++i)
	{
		if (m_platforms[i].HasName(name))
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
bool ResourceCompiler::AddPlatform(const string& names, bool bBigEndian, int pointerSize)
{
	if (m_platformCount >= kMaxPlatformCount)
	{
		return false;
	}

	if (pointerSize != 4 && pointerSize != 8)
	{
		return false;
	}

	std::vector<string> arrNames;
	StringHelpers::SplitByAnyOf(names, ",; ", false, arrNames);

	if (arrNames.empty() || arrNames.size() > PlatformInfo::kMaxPlatformNames)
	{
		return false;
	}

	PlatformInfo& p = m_platforms[m_platformCount];

	p.Clear();

	for (size_t i = 0; i < arrNames.size(); ++i)
	{
		if (!p.SetName(i, arrNames[i].c_str()))
		{
			return false;
		}
	}

	p.index = m_platformCount++;
	p.bBigEndian = bBigEndian;
	p.pointerSize = pointerSize;

	return true;
}

//////////////////////////////////////////////////////////////////////////
const char* ResourceCompiler::GetLogPrefix() const
{
	return m_logPrefix.c_str();
}

void ResourceCompiler::RemoveOutputFiles()
{
	DeleteFile(FormLogFileName(m_filenameDeletedFileList));
	DeleteFile(FormLogFileName(m_filenameCreatedFileList));
}

class FilesToConvert
{
public:
	std::vector<RcFile> m_allFiles;
	std::vector<RcFile> m_inputFiles;
	std::vector<RcFile> m_outOfMemoryFiles;
	std::vector<RcFile> m_failedFiles;
	std::vector<RcFile> m_convertedFiles;

private:
	ThreadUtils::CriticalSection m_lock;

public:
	void lock()
	{
		m_lock.Lock();
	}

	void unlock()
	{
		m_lock.Unlock();
	}
};

struct RcThreadData
{
	ResourceCompiler* rc;
	FilesToConvert* pFilesToConvert;
	unsigned long tlsIndex_pThreadData;	  // copy of private rc->m_tlsIndex_pThreadData
	int threadId;
	IConverter* converter;
	ICompiler* compiler;
	const IConfig* config;

	bool bLogMemory;
	bool bWarningHeaderLine;
	bool bErrorHeaderLine;
	string logHeaderLine;
};

unsigned int WINAPI ThreadFunc(void* threadDataMemory);
	
static void CompileFilesMultiThreaded(
	ResourceCompiler* pRC,
	unsigned long a_tlsIndex_pThreadData,
	FilesToConvert& a_files,
	int threadCount,
	IConverter* converter,
	const IConfig* config)
{
	assert(pRC);

	if (threadCount <= 0)
	{
		return;
	}

	bool bLogMemory = false;

	while (!a_files.m_inputFiles.empty())
	{
		// Never create more threads than needed
		if (threadCount > a_files.m_inputFiles.size())
		{
			threadCount = a_files.m_inputFiles.size();
		}

		RCLog("Spawning %d thread%s", threadCount, ((threadCount > 1) ? "s" : ""));
		RCLog("");

		// Initialize the converter
		{
			ConverterInitContext initContext;
			initContext.config = config;
			initContext.inputFiles = a_files.m_inputFiles.empty() ? 0 : &a_files.m_inputFiles[0];
			initContext.inputFileCount = a_files.m_inputFiles.size();

			converter->Init(initContext);
		}

		// Initialize the thread data for each thread.
		std::vector<RcThreadData> threadData(threadCount);
		for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
		{
			threadData[threadIndex].rc = pRC;
			threadData[threadIndex].converter = converter;
			threadData[threadIndex].compiler = converter->CreateCompiler();
			threadData[threadIndex].config = config;
			threadData[threadIndex].tlsIndex_pThreadData = a_tlsIndex_pThreadData;
			threadData[threadIndex].threadId = threadIndex + 1;	 // our "thread ids" are 1-based indices
			threadData[threadIndex].pFilesToConvert = &a_files;
			threadData[threadIndex].bLogMemory = bLogMemory;
			threadData[threadIndex].bWarningHeaderLine = false;
			threadData[threadIndex].bErrorHeaderLine = false;
			threadData[threadIndex].logHeaderLine = "";
		}

		// Spawn the threads.
		std::vector<HANDLE> threads(threadCount);
		for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
		{
			threads[threadIndex] = (HANDLE)_beginthreadex( 
				0,                               //void *security,
				0,                               //unsigned stack_size,
				ThreadFunc,                      //unsigned ( *start_address )( void * ),
				&threadData[threadIndex],        //void *arglist,
				0,                               //unsigned initflag,
				0);                              //unsigned *thrdaddr 
		}

		// Wait until all the threads have exited
		while (WaitForMultipleObjects(threads.size(), &threads[0], TRUE, 1500) == WAIT_TIMEOUT)
		{
			// Show progress

			a_files.lock();

			if (!a_files.m_inputFiles.empty())
			{
				const int processedFileCount = a_files.m_outOfMemoryFiles.size() + a_files.m_failedFiles.size() + a_files.m_convertedFiles.size();

				pRC->ShowProgress(a_files.m_inputFiles.back().m_sourceInnerPathAndName.c_str(), processedFileCount, a_files.m_allFiles.size());
			}

			a_files.unlock();
		}

		pRC->FinishProgress();

		assert(a_files.m_inputFiles.empty());

		// Release all the compiler objects.
		for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
		{
			threadData[threadIndex].compiler->Release();
		}

		// Clean up the converter.
		converter->DeInit();

		if (!a_files.m_outOfMemoryFiles.empty())
		{
			pRC->LogMemoryUsage(false);

			if (threadCount > 1)
			{
				// If we ran out of memory while processing files, we should try 
				// to process the files again in single thread (since we may
				// have run out of memory just because we had multiple threads).
				RCLogWarning("Run out of memory while processing %i file(s):", (int)a_files.m_outOfMemoryFiles.size());
				for (int i = 0; i < (int)a_files.m_outOfMemoryFiles.size(); ++i)
				{
					const RcFile& rf = a_files.m_outOfMemoryFiles[i];
					RCLogWarning(" \"%s\" \"%s\"", rf.m_sourceLeftPath.c_str(), rf.m_sourceInnerPathAndName.c_str());
				}
				RCLogWarning("Switching to single-thread mode and trying to convert the files again");
				a_files.m_inputFiles.insert(a_files.m_inputFiles.end(), a_files.m_outOfMemoryFiles.begin(), a_files.m_outOfMemoryFiles.end());
				threadCount = 1; 
				bLogMemory = true;
			}
			else
			{
				RCLogError("Run out of memory while processing %i file(s):", (int)a_files.m_outOfMemoryFiles.size());
				for (int i = 0; i < (int)a_files.m_outOfMemoryFiles.size(); ++i)
				{
					const RcFile& rf = a_files.m_outOfMemoryFiles[i];
					RCLogError(" \"%s\" \"%s\"", rf.m_sourceLeftPath.c_str(), rf.m_sourceInnerPathAndName.c_str());
				}

				a_files.m_failedFiles.insert(a_files.m_failedFiles.end(), a_files.m_outOfMemoryFiles.begin(), a_files.m_outOfMemoryFiles.end());
			}

			a_files.m_outOfMemoryFiles.resize(0);
		}
	}
}

static void CompileFilesSingleThreaded(
	ResourceCompiler* pRC,
	unsigned long a_tlsIndex_pThreadData,
	FilesToConvert& a_files,
	int threadCount /* unused */,
	IConverter* converter,
	const IConfig* config)
{
	assert(pRC);

	// Initialize the converter
	{
		ConverterInitContext initContext;
		initContext.config = config;
		initContext.inputFiles = a_files.m_inputFiles.empty() ? 0 : &a_files.m_inputFiles[0];
		initContext.inputFileCount = a_files.m_inputFiles.size();

		converter->Init(initContext);
	}

	int threadId = 0;  // Main thread.

	RcThreadData* const pSaveThreadData = static_cast<RcThreadData*>(TlsGetValue(a_tlsIndex_pThreadData));
	if (pSaveThreadData)
	{
		threadId = pSaveThreadData->threadId;
	}


	RcThreadData threadData;
	threadData.rc = pRC;
	threadData.converter = converter;
	threadData.compiler = converter->CreateCompiler();
	threadData.config = config;
	threadData.tlsIndex_pThreadData = a_tlsIndex_pThreadData;
	threadData.threadId = threadId;
	threadData.pFilesToConvert = &a_files;
	threadData.bLogMemory = true;
	threadData.bWarningHeaderLine = false;
	threadData.bErrorHeaderLine = false;
	threadData.logHeaderLine = "";

	ThreadFunc(&threadData);

	TlsSetValue(a_tlsIndex_pThreadData, pSaveThreadData);

	assert(a_files.m_inputFiles.empty());

	threadData.compiler->Release();

	// Clean up the converter.
	converter->DeInit();

	if (!a_files.m_outOfMemoryFiles.empty())
	{
		pRC->LogMemoryUsage(false);

		RCLogError("Run out of memory while processing %i file(s):", (int)a_files.m_outOfMemoryFiles.size());
		for (int i = 0; i < (int)a_files.m_outOfMemoryFiles.size(); ++i)
		{
			const RcFile& rf = a_files.m_outOfMemoryFiles[i];
			RCLogError(" \"%s\" \"%s\"", rf.m_sourceLeftPath.c_str(), rf.m_sourceInnerPathAndName.c_str());
		}

		a_files.m_failedFiles.insert(a_files.m_failedFiles.end(), a_files.m_outOfMemoryFiles.begin(), a_files.m_outOfMemoryFiles.end());

		a_files.m_outOfMemoryFiles.resize(0);
	}
}

//////////////////////////////////////////////////////////////////////////
typedef std::set<string, stl::less_stricmp<string> > TStringSet;
// Adds file into vector<RcFile> ensuring that only one sourceInnerPathAndName exists.
static void AddRcFile(
	std::vector<RcFile>& files,
	TStringSet& addedFilenames,
	const std::vector<string>& sourceRootsReversed,
	const string& sourceLeftPath,
	const string& sourceInnerPathAndName,
	const string& targetLeftPath)
{
	if (sourceRootsReversed.size() == 1)
	{
		files.push_back(RcFile(sourceLeftPath, sourceInnerPathAndName, targetLeftPath));
	}
	else
	{
		if (addedFilenames.find(sourceInnerPathAndName) == addedFilenames.end())
		{
			files.push_back(RcFile(sourceLeftPath, sourceInnerPathAndName, targetLeftPath));
			addedFilenames.insert(sourceInnerPathAndName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::GetSourceRootsReversed(const IConfig* config, std::vector<string>& sourceRootsReversed)
{
	const int verbosityLevel = config->GetAsInt("verbose", 0, 1);
	const string sourceRootsStr = config->GetAsString("sourceroot", "", "");

	std::vector<string> sourceRoots;
	StringHelpers::Split(sourceRootsStr, ";", true, sourceRoots);

	const size_t sourceRootCount = sourceRoots.size();
	sourceRootsReversed.resize(sourceRootCount);

	if (verbosityLevel >= 2)
	{
		RCLog("Source Roots (%i):", (int)sourceRootCount);
	}

	for (size_t i = 0; i < sourceRootCount; ++i)
	{
		sourceRootsReversed[i] = PathHelpers::GetAbsolutePath(sourceRoots[sourceRootCount - 1 - i]);

		if (verbosityLevel >= 3)
		{
			RCLog("  [%i]: '%s' (%s)", (int)i, sourceRootsReversed[i].c_str(), sourceRoots[sourceRootCount - 1 - i].c_str());
		}
		else if (verbosityLevel >= 2)
		{
			RCLog("  [%i]: '%s'", (int)i, sourceRootsReversed[i].c_str());
		}
	}

	if (sourceRootsReversed.empty())
	{
		sourceRootsReversed.push_back(string());
	}
}

//////////////////////////////////////////////////////////////////////////
bool ResourceCompiler::CollectFilesToCompile(const string& filespec, std::vector<RcFile>& files)
{
	const IConfig* const config = &m_multiConfig.getConfig();

	files.clear();

	const bool bVerbose = GetVerbosityLevel() > 0;
	const bool bRecursive = config->GetAsBool("recursive", true, true);
	const bool bSkipMissing = config->GetAsBool("skipmissing", false, true);

	// RC expects source filenames in command line in form 
	// "<source left path><mask for recursion>" or "<source left path><non-masked name>".
	// After recursive subdirectory scan we will have a list with source filenames in 
	// form "<source left path><source inner path><name>".
	// The target filename can be written as "<target left path><source inner path><name>".

	// Determine the target output path (may be a different directory structure).
	// If none is specified, the target path is the same as the <source left path>.
	const string targetLeftPath = PathHelpers::CanonicalizePath(config->GetAsString("targetroot", "", ""));

	std::vector<string> sourceRootsReversed;
	GetSourceRootsReversed(config, sourceRootsReversed);

	const string listFile = config->GetAsString("listfile", "", "");

	TStringSet addedFiles;
	if (!listFile.empty())
	{
		const string listFormat = config->GetAsString("listformat", "", "");

		for (size_t i = 0; i < sourceRootsReversed.size(); ++i)
		{
			const string& sourceRoot = sourceRootsReversed[i];
			{
				std::vector< std::pair<string, string> > filenames;

				CListFile(this).Process(listFile, listFormat, filespec, sourceRoot, filenames);

				for (size_t i = 0; i < filenames.size(); ++i)
				{
					AddRcFile(files, addedFiles, sourceRootsReversed, filenames[i].first, filenames[i].second, targetLeftPath);
				}
			}
		}

		if (files.empty())
		{
			if (!bSkipMissing)
			{
				RCLogError("No files to convert found in list file \"%s\" (filter is \"%s\")", listFile.c_str(), filespec.c_str());
			}
			return bSkipMissing;
		}

		if (bVerbose)
		{
			RCLog("Contents of the list file \"%s\" (filter is \"%s\"):", listFile.c_str(), filespec.c_str());
			for (size_t i = 0; i < files.size(); ++i)
			{
				RCLog(" %3d: \"%s\" \"%s\"", i, files[i].m_sourceLeftPath.c_str(), files[i].m_sourceInnerPathAndName.c_str());
			}
		}
	}
	else
	{
		if (filespec.find_first_of("*?") != string::npos)
		{
			// It's a mask (path\*.mask). Scan directory and accumulate matching filenames in the list.
			// Multiple masks allowed, for example path\*.xml;*.dlg;path2\*.mtl

			std::vector<string> tokens;
			StringHelpers::Split(filespec, ";", false, tokens);

			for (size_t i = 0; i < sourceRootsReversed.size(); ++i)
			{
				const string& sourceRoot = sourceRootsReversed[i];
				for (size_t t = 0; t < tokens.size(); ++t)
				{
					// Scan directory and accumulate matching filenames in the list.
					const string path = PathUtil::Make(sourceRoot, PathHelpers::GetDirectory(tokens[t]));
					const string pattern = PathUtil::GetFile(tokens[t]);
					RCLog("Scanning directory '%s' for '%s'...", path.c_str(), pattern.c_str());
					std::vector<string> filenames;
					FileUtil::ScanDirectory(path, pattern, filenames, bRecursive, targetLeftPath);
					for (size_t i = 0; i < filenames.size(); ++i)
					{
						string sourceLeftPath;
						string sourceInnerPathAndName;
						if (sourceRoot.empty())
						{
							sourceLeftPath = PathHelpers::GetDirectory(tokens[t]);
							sourceInnerPathAndName = filenames[i];
						}
						else
						{
							sourceLeftPath = sourceRoot;
							sourceInnerPathAndName = PathUtil::Make(PathHelpers::GetDirectory(tokens[t]), filenames[i]);
						}
						AddRcFile(files, addedFiles, sourceRootsReversed, sourceLeftPath, sourceInnerPathAndName, targetLeftPath);
					}
				}
			}

			if (files.empty())
			{
				// We failed to find any file matching the mask specified by user.
				// Using mask (say, *.cgf) usually means that user doesn't know if
				// the file exists or not, so it's better to return "success" code.
				RCLog("RC can't find files matching '%s', 0 files converted", filespec.c_str());
				return true;
			}
		}
		else
		{
			for (size_t i = 0; i < sourceRootsReversed.size(); ++i)
			{
				const string& sourceRoot = sourceRootsReversed[i];
				const DWORD dwFileSpecAttr = GetFileAttributes(PathUtil::Make(sourceRoot, filespec));

				if (dwFileSpecAttr == INVALID_FILE_ATTRIBUTES)
				{
					// There's no such file
					RCLog("RC can't find file '%s'. Will try to find the file in .pak files.", filespec.c_str());
					if (sourceRoot.empty())
					{
						AddRcFile(files, addedFiles, sourceRootsReversed, PathHelpers::GetDirectory(filespec), PathUtil::GetFile(filespec), targetLeftPath);
					}
					else
					{
						AddRcFile(files, addedFiles, sourceRootsReversed, sourceRoot, filespec, targetLeftPath);
					}
				}
				else
				{
					// The file exists

					if (dwFileSpecAttr & FILE_ATTRIBUTE_DIRECTORY)
					{
						// We found a file, but it's a directory, not a regular file.
						// Let's assume that the user wants to export every file in the 
						// directory (with subdirectories if bRecursive == true) or
						// that he wants to export a file specified in /file option.
						const string path = PathUtil::Make(sourceRoot, filespec);
						const string pattern = config->GetAsString("file", "*.*", "*.*");
						RCLog("Scanning directory '%s' for '%s'...", path.c_str(), pattern.c_str());
						std::vector<string> filenames;
						FileUtil::ScanDirectory(path, pattern, filenames, bRecursive, targetLeftPath);
						for (size_t i = 0; i < filenames.size(); ++i)
						{
							string sourceLeftPath;
							string sourceInnerPathAndName;
							if (sourceRoot.empty())
							{
								sourceLeftPath = filespec;
								sourceInnerPathAndName = filenames[i];
							}
							else
							{
								sourceLeftPath = sourceRoot;
								sourceInnerPathAndName = PathUtil::Make(filespec, filenames[i]);
							}
							AddRcFile(files, addedFiles, sourceRootsReversed, sourceLeftPath, sourceInnerPathAndName, targetLeftPath);
						}
					}
					else
					{	
						// we found a regular file
						if (sourceRoot.empty())
						{
							AddRcFile(files, addedFiles, sourceRootsReversed, PathHelpers::GetDirectory(filespec), PathUtil::GetFile(filespec), targetLeftPath);
						}
						else
						{
							AddRcFile(files, addedFiles, sourceRootsReversed, sourceRoot, filespec, targetLeftPath);
						}
					}
				}
			}
		}

		if (files.empty())
		{
			if (!bSkipMissing)
			{
				RCLogError("No files found to convert.");
			}
			return bSkipMissing;
		}
	}
	
	// Remove excluded files from the list of files to process.
	FilterExcludedFiles(files);

	if (files.empty())
	{
		if (!bSkipMissing)
		{
			RCLogError("No files to convert (all files were excluded by /exclude command).");
		}
		return bSkipMissing;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Returns true if successfully converted at least one file
bool ResourceCompiler::CompileFilesBySingleProcess(const std::vector<RcFile>& files)
{
	const IConfig* const config = &m_multiConfig.getConfig();
	return CompileFiles(files, config, CompileFilesMultiThreaded);
}

void ParseCommandLine(const std::vector<string>& args, IConfig* config, string& fileSpec);

bool ResourceCompiler::CompileSingleFileNested(const string& fileSpec, const std::vector<string>& args)
{

	MultiplatformConfig config = m_multiConfig;

	std::vector<string> extArgs;
	extArgs.reserve(args.size() + 1);
	extArgs.push_back("");  // Add dummy string in first place as executable name.
	extArgs.insert(extArgs.end(), args.begin(), args.end());

	string unusedFileSpec;
	ParseCommandLine(extArgs, &config.getConfig(), unusedFileSpec);
	std::vector<RcFile> list;
	// TODO we need to figure out how to not pass sourceroot for nested calls
	// for now we just don't use it together with target root
	//list.push_back(RcFile(PathHelpers::CanonicalizePath(config.getConfig().GetAsString("sourceroot", "", "")), fileSpec, PathHelpers::CanonicalizePath(config.getConfig().GetAsString("targetroot", "", ""))));
	list.push_back(RcFile("", fileSpec, ""));

	return CompileFiles(list, &config.getConfig(), CompileFilesSingleThreaded);
}

//////////////////////////////////////////////////////////////////////////
// Returns true if successfully converted at least one file
bool ResourceCompiler::CompileFiles(const std::vector<RcFile>& files, const IConfig* config, CompileFilesFunction compileFiles)
{
	const clock_t compilationStartTime = clock();

	FilesToConvert filesToConvert;
	filesToConvert.m_allFiles = files;

	if (config->GetAsBool("copyonly", false, true))
	{
		const string targetroot = PathHelpers::CanonicalizePath(config->GetAsString("targetroot", "", ""));
		if (targetroot.empty())
		{
			RCLogError("/copyonly: you must specify /targetroot.");
			return false;
		}

		CopyFiles(filesToConvert.m_allFiles);
		return true;
	}

	const PakManager::ECallResult eResult = 
		m_pPakManager->CompileFilesIntoPaks(config, filesToConvert.m_allFiles);

	if (eResult != PakManager::eCallResult_Skipped)
	{
		return eResult == PakManager::eCallResult_Succeeded;
	}

	//////////////////////////////////////////////////////////////////////////

	// Split up the files based on the converter they are to use.
	typedef std::map<IConverter*, std::vector<RcFile> > FileConverterMap;
	FileConverterMap fileConverterMap;
	for (size_t i = 0; i < filesToConvert.m_allFiles.size(); ++i)
	{
		string filenameForConverterSearch;
		{
			const string sOverWriteExtension = config->GetAsString("overwriteextension", "", "");
			if (!sOverWriteExtension.empty())
			{
				filenameForConverterSearch = string("filename.") + sOverWriteExtension;
			}
			else
			{
				filenameForConverterSearch = filesToConvert.m_allFiles[i].m_sourceInnerPathAndName;
			}
		}

		IConverter* const converter = m_extensionManager.FindConverter(filenameForConverterSearch.c_str());

		if (!converter)
		{
			RCLogWarning("Cannot find converter for %s", filenameForConverterSearch);
			filesToConvert.m_allFiles.erase(filesToConvert.m_allFiles.begin() + i);
			--i;
			continue;
		}

		FileConverterMap::iterator converterIt = fileConverterMap.find(converter);
		if (converterIt == fileConverterMap.end())
		{
			converterIt = fileConverterMap.insert(std::make_pair(converter, std::vector<RcFile>())).first;
		}
		(*converterIt).second.push_back(filesToConvert.m_allFiles[i]);
	}

	if (GetVerbosityLevel() > 0)
	{
		RCLog("%d file%s to convert:", filesToConvert.m_allFiles.size(), ((filesToConvert.m_allFiles.size() != 1) ? "s" : ""));
		for (size_t i = 0; i < filesToConvert.m_allFiles.size(); ++i)
		{
			RCLog("  \"%s\"  \"%s\" -> \"%s\"",
				filesToConvert.m_allFiles[i].m_sourceLeftPath.c_str(), 
				filesToConvert.m_allFiles[i].m_sourceInnerPathAndName.c_str(), 
				filesToConvert.m_allFiles[i].m_targetLeftPath.c_str());
		}
		RCLog("");
	}

	// Loop through all the converters that we need to invoke.
	for (FileConverterMap::iterator converterIt = fileConverterMap.begin(); converterIt != fileConverterMap.end(); ++converterIt)
	{
		assert(filesToConvert.m_inputFiles.empty());
		assert(filesToConvert.m_outOfMemoryFiles.empty());

		IConverter* const converter = (*converterIt).first;
		assert(converter);

		// Check whether this converter is thread-safe.
		assert(m_maxThreads >= 1);
		int threadCount = m_maxThreads;
		if ((threadCount > 1) && (!converter->SupportsMultithreading()))
		{
			RCLog("/threads specified, but converter does not support multi-threading. Falling back to single-threading.");
			threadCount = 1;
		}

		const std::vector<RcFile>& converterFiles = (*converterIt).second;
		assert(converterFiles.size()>0);

		// implementation note: we insert filenames starting from last, because converting function will take filenames one by one from the end(!) of the array
		for(int i = converterFiles.size() - 1; i >= 0; --i)
		{
			filesToConvert.m_inputFiles.push_back(converterFiles[i]);
		}

		LogMemoryUsage(false);

		if (!m_cacheFolder.empty() && converter->IsCacheable())
		{
			TryToGetFilesFromCache(filesToConvert, converter);
		}

		compileFiles(this, m_tlsIndex_pThreadData, filesToConvert, threadCount, converter, config);

		assert(filesToConvert.m_inputFiles.empty());
		assert(filesToConvert.m_outOfMemoryFiles.empty());
	}

	const int numFilesConverted = filesToConvert.m_convertedFiles.size();
	const int numFilesFailed = filesToConvert.m_failedFiles.size();
	assert(numFilesConverted+numFilesFailed == filesToConvert.m_allFiles.size());

	const bool savedTimeLogging = GetTimeLogging();
	SetTimeLogging(false);

	char szTimeMsg[128];
	const float secondsElapsed = float(clock() - compilationStartTime) / CLOCKS_PER_SEC;
	cry_sprintf(szTimeMsg, " in %.1f sec", secondsElapsed);

	RCLog("");

	if (numFilesFailed <= 0)
	{
		RCLog("%d file%s processed%s.", numFilesConverted, (numFilesConverted > 1? "s" : ""), szTimeMsg);
		RCLog("");
	}
	else
	{
		RCLog("");
		RCLog(
			"%d of %d file%s were converted%s. Couldn't convert the following file%s:", 
			numFilesConverted, 
			numFilesConverted + numFilesFailed, 
			(numFilesConverted + numFilesFailed > 1 ? "s" : ""), 
			szTimeMsg, 
			(numFilesFailed > 1 ? "s" : ""));
		RCLog("");
		for (size_t i = 0; i < numFilesFailed; ++i)
		{
			const string failedFilename = PathUtil::Make(filesToConvert.m_failedFiles[i].m_sourceLeftPath, filesToConvert.m_failedFiles[i].m_sourceInnerPathAndName);
			RCLog("  %s", failedFilename.c_str());
		}
		RCLog("");
	}

	SetTimeLogging(savedTimeLogging);

	if (!m_cacheFolder.empty())
	{
		AddFilesToTheCache(filesToConvert);
	}

	return numFilesConverted > 0;
}

bool ResourceCompiler::CompileSingleFileBySingleProcess(const char* filename)
{
	std::vector<RcFile> list;
	list.push_back(RcFile("", filename, ""));
	return CompileFilesBySingleProcess(list);
}

unsigned int WINAPI ThreadFunc(void* threadDataMemory)
{
	MathHelpers::EnableFloatingPointExceptions(~(_EM_INEXACT | _EM_UNDERFLOW));

	RcThreadData* const data = static_cast<RcThreadData*>(threadDataMemory);

	// Initialize the thread local storage, so the log can prepend the thread id to each line.
	TlsSetValue(data->tlsIndex_pThreadData, data);

	data->compiler->BeginProcessing(data->config);

	for(;;)
	{
		data->pFilesToConvert->lock();

		if (data->pFilesToConvert->m_inputFiles.empty())
		{
			data->pFilesToConvert->unlock();
			break;
		}

		const RcFile fileToConvert = data->pFilesToConvert->m_inputFiles.back();
		data->pFilesToConvert->m_inputFiles.pop_back();

		data->pFilesToConvert->unlock();

		const string sourceInnerPath = PathHelpers::GetDirectory(fileToConvert.m_sourceInnerPathAndName);
		const string sourceFullFileName = PathUtil::Make(fileToConvert.m_sourceLeftPath, fileToConvert.m_sourceInnerPathAndName);

		enum EResult
		{
			eResult_Ok,
			eResult_Error,
			eResult_OutOfMemory,
			eResult_Exception
		};
		EResult eResult;

		try
		{
			if (data->rc->CompileFile(sourceFullFileName.c_str(), fileToConvert.m_targetLeftPath.c_str(), sourceInnerPath.c_str(), data->compiler))
			{
				eResult = eResult_Ok;
			}
			else
			{
				eResult = eResult_Error;
			}
		}
		catch (std::bad_alloc&)
		{
			eResult = eResult_OutOfMemory;
		}
		catch (...)
		{
			eResult = eResult_Exception;
		}

		data->pFilesToConvert->lock();
		switch (eResult)
		{
		case eResult_Ok:
			data->pFilesToConvert->m_convertedFiles.push_back(fileToConvert);
			break;
		case eResult_Error:
			data->pFilesToConvert->m_failedFiles.push_back(fileToConvert);
			break;
		case eResult_OutOfMemory:
			data->rc->LogMemoryUsage(false);
			RCLogWarning("Run out of memory: \"%s\" \"%s\"", fileToConvert.m_sourceLeftPath.c_str(), fileToConvert.m_sourceInnerPathAndName.c_str());
			data->pFilesToConvert->m_outOfMemoryFiles.push_back(fileToConvert);
			break;
		case eResult_Exception:
			data->rc->LogMemoryUsage(false);
			RCLogError("Unknown failure: \"%s\" \"%s\"", fileToConvert.m_sourceLeftPath.c_str(), fileToConvert.m_sourceInnerPathAndName.c_str());
			data->pFilesToConvert->m_failedFiles.push_back(fileToConvert);
			break;
		default:
			assert(0);
			break;
		}
		data->pFilesToConvert->unlock();
	}

	data->compiler->EndProcessing();

	return 0;
}


const char* ResourceCompiler::GetExePath() const
{
	return m_exePath.c_str();
}

const char* ResourceCompiler::GetTmpPath() const
{
	return m_tempPath.c_str();
}

const char* ResourceCompiler::GetInitialCurrentDir() const
{
	return m_initialCurrentDir.c_str();
}

//////////////////////////////////////////////////////////////////////////
bool ResourceCompiler::CompileFile(
	const char* const sourceFullFileName,
	const char* const targetLeftPath,
	const char* const sourceInnerPath,
	ICompiler* compiler)
{
	RcThreadData* const pThreadData = static_cast<RcThreadData*>(TlsGetValue(m_tlsIndex_pThreadData));
	if (!pThreadData)
	{
		RCLogError("Unexpected threading failure");
		exit(eRcExitCode_FatalError);
	}
	const bool bMemoryReportProblemsOnly = !pThreadData->bLogMemory;
	LogMemoryUsage(bMemoryReportProblemsOnly);

	MultiplatformConfig localMultiConfig = m_multiConfig;
	const IConfig* const config = pThreadData->config;
	localMultiConfig.getConfig().AddConfig(pThreadData->config);
	const string targetPath = PathUtil::Make(targetLeftPath, sourceInnerPath);

	if (GetVerbosityLevel() >= 2)
	{
		RCLog("CompileFile():");
		RCLog("  sourceFullFileName: '%s'", sourceFullFileName);
		RCLog("  targetLeftPath: '%s'", targetLeftPath);
		RCLog("  sourceInnerPath: '%s'", sourceInnerPath);
		RCLog("targetPath: '%s'", targetPath);
	}

	// Setup conversion context.
	IConvertContext* const pCC = compiler->GetConvertContext();

	pCC->SetMultiplatformConfig(&localMultiConfig);
	pCC->SetRC(this);
	pCC->SetThreads(m_maxThreads);

	{
		bool bRefresh = config->GetAsBool("refresh", false, true);

		// Force "refresh" to be true if user asked for dialog - it helps a lot
		// when a command line used, because users very often forget to specify /refresh
		// in such cases. Also, some tools, including CryTif exporter, call RC without
		// providing /refresh
		if (config->GetAsBool("userdialog", false, true))
		{
			bRefresh = true;
		}
		
		pCC->SetForceRecompiling(bRefresh);
	}

	{
		const string sourceExtension = PathUtil::GetExt(sourceFullFileName);
		const string converterExtension = config->GetAsString("overwriteextension", sourceExtension, sourceExtension);
		pCC->SetConverterExtension(converterExtension);
	}

	const string sourceFileName = PathUtil::GetFile(sourceFullFileName);

	pCC->SetSourceFileNameOnly(sourceFileName);
	pCC->SetSourceFolder(PathHelpers::GetDirectory(PathHelpers::GetAbsolutePath(sourceFullFileName)));

	const string outputFolder = PathHelpers::GetAbsolutePath(targetPath);
	pCC->SetOutputFolder(outputFolder);

	if (!FileUtil::EnsureDirectoryExists(outputFolder.c_str()))
	{
		RCLogError("Creating directory failed: %s", outputFolder.c_str());
		return false;
	}

	if (GetVerbosityLevel() >= 0)
	{
		RCLog("---------------------------------");
	}

	if (GetVerbosityLevel() >= 2)
	{
		RCLog("sourceFullFileName: '%s'", sourceFullFileName);
		RCLog("outputFolder: '%s'", outputFolder);
		RCLog("Path='%s'", PathHelpers::CanonicalizePath(sourceInnerPath).c_str());
		RCLog("File='%s'", sourceFileName.c_str());
	}
	else if (GetVerbosityLevel() > 0)
	{
		RCLog("Path='%s'", PathHelpers::CanonicalizePath(sourceInnerPath).c_str());
		RCLog("File='%s'", sourceFileName.c_str());
	}
	else if (GetVerbosityLevel() == 0)
	{
		const string sourceInnerPathAndName = PathUtil::AddSlash(sourceInnerPath) + sourceFileName;
		RCLog("File='%s'", sourceInnerPathAndName.c_str());
	}

	// file name changed - print new header for warnings and errors
	{
		RcThreadData* const pThreadData = static_cast<RcThreadData*>(TlsGetValue(m_tlsIndex_pThreadData));
		if (!pThreadData)
		{
			RCLogError("Unexpected threading failure");
			exit(eRcExitCode_FatalError);
		}
		pThreadData->bWarningHeaderLine = false;
		pThreadData->bErrorHeaderLine = false;
		pThreadData->logHeaderLine = sourceFullFileName;
	}

	auto compile = [compiler]()
	{
		CCompilerProcessTimer accumulateDuration;
		return compiler->Process();
	};

	if (!compile())
	{
		RCLogError("Failed to convert file %s", sourceFullFileName);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::AddInputOutputFilePair(const char* inputFilename, const char* outputFilename)
{
	assert(outputFilename && outputFilename[0]);
	assert(inputFilename && inputFilename[0]); 

	ThreadUtils::AutoLock lock(m_inputOutputFilesLock);

	m_inputOutputFileList.Add(inputFilename, outputFilename);
}

void ResourceCompiler::MarkOutputFileForRemoval(const char* outputFilename)
{
	assert(outputFilename && outputFilename[0]);

	ThreadUtils::AutoLock lock(m_inputOutputFilesLock);

	// using empty input file name will force CleanTargetFolder(false) to delete the output file
	m_inputOutputFileList.Add("", outputFilename);

	if (GetVerbosityLevel() > 0)
	{
		RCLog("Marking file for removal: %s", outputFilename);
	}
}

void ResourceCompiler::InitializeThreadIds()
{
	m_tlsIndex_pThreadData = TlsAlloc();
	if (m_tlsIndex_pThreadData == TLS_OUT_OF_INDEXES)
	{
		printf("RC Initialization error");
		exit(eRcExitCode_FatalError);
	}
	TlsSetValue(m_tlsIndex_pThreadData, 0);
}

void ResourceCompiler::LogLine(const IRCLog::EType eType, const char* szText)
{
	ThreadUtils::AutoLock lock(m_logLock);

	if (eType == IRCLog::eType_Warning)
	{
		++m_numWarnings;
	}
	else if (eType == IRCLog::eType_Error)
	{
		++m_numErrors;
	}

	FILE* fLog = 0;
	if (!m_mainLogFileName.empty())
	{
		fopen_s(&fLog, m_mainLogFileName.c_str(), "a+t");
	}

	if (m_bQuiet)
	{
		if (fLog)
		{
			fprintf(fLog,"%s\n", szText);
			fclose(fLog);
		}
		return;
	}
				
	RcThreadData* const pThreadData = static_cast<RcThreadData*>(TlsGetValue(m_tlsIndex_pThreadData));
	// if pThreadData is 0, then probably RC is just not running threads yet

	char threadString[10];
	threadString[0] = 0;
	if (pThreadData) 
	{
		cry_sprintf(threadString, "%d> ", pThreadData->threadId);
	}

	char timeString[20];
	timeString[0] = 0;
	if (m_bTimeLogging)
	{
		const int seconds = int(float((clock() - m_startTime) / CLOCKS_PER_SEC) + 0.5f);
		const int minutes = seconds / 60;
		cry_sprintf(timeString, "%2d:%02d ", minutes, seconds - (minutes * 60));
	}

	const char* prefix = "";
	const char* additionalLogFileName = 0;
	bool* pbAdditionalLogHeaderLine = 0;

	switch(eType)
	{
	case IRCLog::eType_Info:
		prefix = "   ";
		break;

	case IRCLog::eType_Warning:
		prefix = "W: ";
		if (!m_warningLogFileName.empty())
		{
			additionalLogFileName = m_warningLogFileName.c_str();
			if (pThreadData) 
			{
				pbAdditionalLogHeaderLine = &pThreadData->bWarningHeaderLine;
			}
		}
		break;

	case IRCLog::eType_Error:
		prefix = "E: ";
		if (!m_errorLogFileName.empty())
		{
			additionalLogFileName = m_errorLogFileName.c_str();
			if (pThreadData) 
			{
				pbAdditionalLogHeaderLine = &pThreadData->bErrorHeaderLine;
			}
		}
		break;

	default:
		assert(0);
		break;
	}

	FILE* fAdditionalLog = 0;
	if (additionalLogFileName)
	{
		fopen_s(&fAdditionalLog, additionalLogFileName, "a+t");
	}

	if (fAdditionalLog)
	{
		if (pbAdditionalLogHeaderLine && (*pbAdditionalLogHeaderLine == false))
		{
			fprintf(fAdditionalLog, "------------------------------------\n");
			fprintf(fAdditionalLog, "%s%s%s%s\n", prefix, threadString, timeString, pThreadData->logHeaderLine.c_str());
			*pbAdditionalLogHeaderLine = true;
		}
	}

	const char* line = szText;
	for (;;)
	{
		size_t lineLen = 0;
		while (line[lineLen] && line[lineLen] != '\n')
		{
			++lineLen;
		}

		if (fAdditionalLog)
		{
			fprintf(fAdditionalLog, "%s%s%s%.*s\n", prefix, threadString, timeString, int(lineLen), line);
		}

		if (fLog)
		{
			fprintf(fLog, "%s%s%s%.*s\n", prefix, threadString, timeString, int(lineLen), line);
		}

		printf("%s%s%s%.*s\n", prefix, threadString, timeString, int(lineLen), line);
		fflush(stdout);

		line += lineLen;
		if (*line == '\0')
		{
			break;
		}
		++line;
	}

	if (fAdditionalLog)
	{
		fclose(fAdditionalLog);
		fAdditionalLog = 0;
	}

	if (fLog)
	{
		fclose(fLog);
		fLog = 0;
	}

	if (m_bWarningsAsErrors && (eType == IRCLog::eType_Warning || eType == IRCLog::eType_Error))
	{
		::MessageBoxA(NULL, szText, "RC Compilation Error", MB_OK | MB_ICONERROR);
		this->NotifyExitObservers();
		exit(eRcExitCode_Error);
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::LogMultiLine( const char *szText )
{
	const char *p=szText;
	char szLine[80],*pLine=szLine;

	for(;;)
	{
		if(*p=='\n' || *p==0 || (pLine-szLine)>=sizeof(szLine)-(5+2+1)) // 5 spaces +2 (W: or E:) +1 to avoid nextline jump
		{
			*pLine=0;                                                     // zero termination
			RCLog("     %s",szLine);                                      // 5 spaces
			pLine=szLine;
			
			if(*p=='\n')
			{
				++p;
				continue;
			}
		}

		if(*p==0)
			return;

		*pLine++=*p++;

	} while(*p);
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::ShowHelp(const bool bDetailed)
{
	RCLog( "" );
	RCLog( "Usage: RC filespec /p=<platform> [/Key1=Value1] [/Key2=Value2] etc..." );

	if (bDetailed)
	{
		RCLog( "" );

		std::map<string,string>::const_iterator it, end = m_KeyHelp.end();

		for (it=m_KeyHelp.begin(); it!=end; ++it)
		{
			const string &rKey = it->first;
			const string &rHelp = it->second;

			// hide internal keys (keys starting from '_')
			if (!rKey.empty() && rKey[0] != '_')
			{
				RCLog("/%s",rKey.c_str());
				LogMultiLine(rHelp.c_str());
				RCLog( "" );
			}
		}
	}
	else
	{
		RCLog( "       RC /help             // will list all usable keys with description" );
		RCLog( "       RC /help >file.txt   // output help text to file.txt" );
		RCLog( "" );
	}
}

//////////////////////////////////////////////////////////////////////////
bool ResourceCompiler::RegisterConverters()
{
	const string strDir = GetExePath();
	const string strMask = strDir + "ResourceCompiler*.dll";

	RCLog("Searching for %s", strMask.c_str());

	wstring widePath, wstrDir;
	Unicode::Convert(widePath, strMask.c_str());
	Unicode::Convert(wstrDir, strDir.c_str());

	_wfinddata64_t fd;
	intptr_t hSearch = _wfindfirst64(widePath, &fd);
	if (hSearch == -1)
	{
		RCLog("Cannot find any %s", strMask.c_str());
		return false;
	}

	do
	{
		const wstring pluginFilename = wstrDir + fd.name;
		RCLog("Loading \"%s\"", pluginFilename.c_str());
		HMODULE hPlugin = LoadLibraryW(pluginFilename.c_str());
		if (!hPlugin)
		{
			const DWORD errCode = GetLastError();
			char messageBuffer[1024] = { '?', 0  };
			FormatMessageA(
				FORMAT_MESSAGE_FROM_SYSTEM, 
				NULL, 
				errCode, 
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				messageBuffer, 
				sizeof(messageBuffer) - 1, 
				NULL);
			RCLogError("Couldn't load plug-in module \"%s\"", pluginFilename.c_str());
			RCLogError("Error code: 0x%x (%s)", errCode, messageBuffer);
			return false;
		}
		
		FnRegisterConverters fnRegister = 
			hPlugin 
			? (FnRegisterConverters)GetProcAddress(hPlugin, "RegisterConverters") 
			: NULL;
		if (!fnRegister)
		{
			RCLog("Error: plug-in module \"%s\" doesn't have RegisterConverters function", pluginFilename.c_str());
			return false;
		}

		RCLog("  Loaded \"%s\"", fd.name);

		const int oldErrorCount = GetNumErrors();
		fnRegister(this);
		const int newErrorCount = GetNumErrors();
		if (newErrorCount > oldErrorCount)
		{
			return false;
		}

		if (m_pDigest)
		{
			CComputeDigestTimer accumulateDuration;

			string pluginName;
			Unicode::Convert(pluginName, pluginFilename);

			m_pDigest->UpdateFromFile(pluginName);
		}
	}
	while (_wfindnext64(hSearch, &fd) != -1);

	RCLog("");

	_findclose(hSearch);

	if (m_pDigest)
	{
		// Each RC configuration has an unique cache folder name.
		m_cacheFolder = PathUtil::ToDosPath(PathUtil::Make(m_cacheFolder, m_pDigest->Clone().Final()));

		RCLog("Cache folder: %s.", m_cacheFolder.c_str());
	}
	return true;
}

void ResourceCompiler::InitPakManager()
{
	delete m_pPakManager;

	m_pPakManager = new PakManager(this);
	m_pPakManager->RegisterKeys(this);
}


static string GetResourceCompilerGenericInfo(const ResourceCompiler& rc, const string& newline)
{
	string s;
	const SFileVersion& v = rc.GetFileVersion();

#if defined(_WIN64)
	s += "ResourceCompiler  64-bit";
#else
	s += "ResourceCompiler  32-bit";
#endif
#if defined(_DEBUG)
	s += "  DEBUG";
#endif
	s += newline;

	s += "Platform support: PC";
#if defined(TOOLS_SUPPORT_DURANGO)
	s += ", XboxOne";
#endif
#if defined(TOOLS_SUPPORT_ORBIS)
	s += ", PS4";
#endif
#if defined(TOOLS_SUPPORT_POWERVR)
	s += ", PowerVR";
#endif
	s += newline;

	s += StringHelpers::Format("Version %d.%d.%d.%d  %s %s", v.v[3], v.v[2], v.v[1], v.v[0], __DATE__, __TIME__);
	s += newline;

	s += newline;

	s += "Copyright (c) 2001-2016 Crytek GmbH. All rights reserved.";
	s += newline;

	s += newline;

	s += "Exe directory:";
	s += newline;
	s += StringHelpers::Format("  \"%s\"", rc.GetExePath());
	s += newline;
	s += "Temp directory:";
	s += newline;
	s += StringHelpers::Format("  \"%s\"", rc.GetTmpPath());
	s += newline;
	s += "Current directory:";
	s += newline;
	s += StringHelpers::Format("  \"%s\"", rc.GetInitialCurrentDir());
	s += newline;

	return s;
}


const void ResourceCompiler::GetGenericInfo(
	char* buffer, 
	size_t bufferSize, 
	const char* rowSeparator) const
{
	if (bufferSize <= 0 || buffer == 0)
	{
		return;
	}
	const string s = GetResourceCompilerGenericInfo(*this, string(rowSeparator));
	cry_strcpy(buffer, bufferSize, s.c_str());
}


static void CopyStringToClipboard(const string& s)
{
	if (OpenClipboard(NULL))
	{
		const HGLOBAL hAllocResult = GlobalAlloc(GMEM_MOVEABLE, s.length() + 1);
		if (hAllocResult)
		{
			const LPTSTR lptstrCopy = (LPTSTR)GlobalLock(hAllocResult); 
			memcpy(lptstrCopy, s.c_str(), s.length() + 1); 
			GlobalUnlock(hAllocResult); 

			EmptyClipboard();

			if (::SetClipboardData(CF_TEXT, hAllocResult) == NULL)
			{
				GlobalFree(hAllocResult);
			}
		}
		CloseClipboard();
	}
}


static void ShowAboutDialog(const ResourceCompiler& rc)
{
	const string newline("\r\n");

	const string s = GetResourceCompilerGenericInfo(rc, newline);

	string sSuffix;
	sSuffix += newline;
	sSuffix += newline;
	sSuffix += newline;
	sSuffix += "Use \"RC /help\" to list all available command-line options.";
	sSuffix += newline;
	sSuffix += newline;
	sSuffix += "Press [OK] to copy the info above to clipboard.";

	if (::MessageBoxA(NULL, (s + sSuffix).c_str(), "About", MB_OKCANCEL | MB_APPLMODAL | MB_SETFOREGROUND) == IDOK)
	{
		CopyStringToClipboard(s.c_str());
	}
}


static void ShowWaitDialog(const ResourceCompiler& rc, const string& action, const std::vector<string>& args, int originalArgc)
{
	const string newline("\r\n");

	const string title = string("RC is about to ") + action;

	string sPrefix; 
	sPrefix += title;
	sPrefix += " (/wait was specified).";
	sPrefix += newline;
	sPrefix += newline;
	sPrefix += newline;

	string s;
	s += GetResourceCompilerGenericInfo(rc, newline);
	s += "Command line:";
	s += newline;
	for (size_t i = 0; i < args.size(); ++i)
	{
		s += "  \"";
		s += args[i];
		s += "\"";
		if ((int)i >= originalArgc)
		{
			s += "  (from ";
			s += rc.m_filenameOptions;
			s += ")";
		}
		s += newline;
	}

	string sSuffix;
	sSuffix += newline;
	sSuffix += "Do you want to copy the info above to clipboard?";

	if (::MessageBoxA(NULL, (sPrefix + s + sSuffix).c_str(), title.c_str(), MB_YESNO | MB_ICONINFORMATION | MB_APPLMODAL | MB_SETFOREGROUND) == IDYES)
	{
		CopyStringToClipboard(s);
	}
}


static string GetTimeAsString(const time_t tm)
{
	char buffer[40] = { 0 };
	ctime_s(buffer, sizeof(buffer), &tm);

	string str = buffer;
	while (StringHelpers::EndsWith(str, "\n"))
	{
		str = str.substr(0, str.length() - 1);
	}

	return str;
}


static void ShowResourceCompilerVersionInfo(const ResourceCompiler& rc)
{
	const string newline("\n");
	const string info = GetResourceCompilerGenericInfo(rc, newline);
	std::vector<string> rows;	
	StringHelpers::Split(info, newline, true, rows);
	for (size_t i = 0; i < rows.size(); ++i)
	{
		RCLog("%s", rows[i]);
	}
}


static void ShowResourceCompilerLaunchInfo(const std::vector<string>& args, int originalArgc, const ResourceCompiler& rc)
{
	ShowResourceCompilerVersionInfo(rc);

	RCLog("");
	RCLog("Command line:");
	for (size_t i = 0; i < args.size(); ++i)
	{
		if ((int)i < originalArgc)
		{
			RCLog("  \"%s\"", args[i].c_str());
		}
		else
		{
			RCLog("  \"%s\"  (from %s)", args[i].c_str(), rc.m_filenameOptions);
		}
	}
	RCLog("");

	RCLog("Platforms specified in %s:", rc.m_filenameRcIni);
	for (int i = 0; i < rc.GetPlatformCount(); ++i)
	{
		const PlatformInfo* const p = rc.GetPlatformInfo(i);
		RCLog("  %s (%s)",
			p->GetCommaSeparatedNames().c_str(), 
			(p->bBigEndian ? "big-endian" : "little-endian"));
	}
	RCLog("");

	RCLog("Started at: %s", GetTimeAsString(time(0)).c_str());
}


static bool CheckCommandLineOptions(const IConfig& config, const std::vector<string>* keysToIgnore)
{
	std::vector<string> unknownKeys;
	config.GetUnknownKeys(unknownKeys);

	if (keysToIgnore)
	{
		for (size_t i = 0; i < unknownKeys.size(); ++i)
		{
			if (std::find(keysToIgnore->begin(), keysToIgnore->end(), StringHelpers::MakeLowerCase(unknownKeys[i])) != keysToIgnore->end())
			{
				unknownKeys.erase(unknownKeys.begin() + i);
				--i;
			}
		}
	}

	if (!unknownKeys.empty())
	{
		RCLogWarning("Unknown command-line options (use \"RC /help\"):");

		for (size_t i = 0; i < unknownKeys.size(); ++i)
		{
			RCLogWarning("    /%s", unknownKeys[i].c_str());
		}

		if (config.GetAsBool("failonwarnings", false, true))
		{
			return false;
		}
	}

	return true;
}


static void EnableCrtMemoryChecks()
{
	uint32 tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmp &= ~_CRTDBG_CHECK_ALWAYS_DF;
	tmp |= _CRTDBG_ALLOC_MEM_DF;
	tmp |= _CRTDBG_CHECK_CRT_DF;
	tmp |= _CRTDBG_DELAY_FREE_MEM_DF;
	tmp |= _CRTDBG_LEAK_CHECK_DF;    // used on exit
	// Set desired check frequency
	const uint32 eachX = 1;
	tmp = (tmp & 0xFFFF) | (eachX << 16);
	_CrtSetDbgFlag(tmp);
	// Check heap every 
	//_CrtSetBreakAlloc(2031);
}


static void GetCommandLineArguments(std::vector<string>& resArgs)
{
	LPWSTR *argv;
	int argc;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	resArgs.clear();
	resArgs.reserve(argc);
	for (int i = 0; i < argc; ++i)
	{
		resArgs.push_back(CryStringUtils::WStrToUTF8(argv[i]));
	}
}

static void AddCommandLineArgumentsFromFile(std::vector<string>& args, const char* const pFilename)
{
	FILE* const f = fopen(pFilename, "rt");
	if (!f)
	{
		return;
	}

	char line[1024];
	while (fgets(line, sizeof(line), f) != 0)
	{
		if (line[0] == 0)
		{
			continue;
		}

		string sLine = line;
		sLine.Trim();
		if (sLine.empty())
		{
			continue;
		}

		args.push_back(sLine);
	}

	fclose(f);
}


static void ParseCommandLine(const std::vector<string>& args, IConfig* config, string& fileSpec)
{
	assert(config);

	fileSpec.clear();

	for (int i = 1; i < (int)args.size(); ++i)
	{
		const char* const parameter = args[i].c_str();
		if (parameter[0] != '-' && parameter[0] != '/')
		{
			if (fileSpec.empty())
			{
				fileSpec = parameter;
			}
		}
		else
		{
			// Split on key/value pair
			const string p = parameter + 1;
			const size_t splitterPos = p.find('=');
			if (splitterPos != string::npos)
			{
				const string key = p.substr(0, splitterPos);
				const string value = p.substr(splitterPos + 1);
				if (!key.empty())
				{
					config->SetKeyValue(eCP_PriorityCmdline, key.c_str(), value.c_str());
				}
			}
			else
			{
				config->SetKeyValue(eCP_PriorityCmdline, p.c_str(), "");
			}
		}
	}
}


static void GetFileSizeAndCrc32(int64& size, uint32& crc32, const char* pFilename)
{
	size = FileUtil::GetFileSize(pFilename);
	if (size <= 0)
	{
		return;
	}

	FILE* const f = fopen(pFilename, "rb");
	if (!f)
	{
		size = -1;
		return;
	}

	int64 sizeLeft = size;
	char buffer[1024];

	CCrc32 c;

	while (sizeLeft > 0)
	{
		const int len = int((sizeLeft <= sizeof(buffer)) ? sizeLeft : sizeof(buffer));
		sizeLeft -= len;
		if (fread(buffer, 1, len, f) != len)
		{
			fclose(f);
			size = -1;
			return;
		}
		c.Add(buffer, len);
	}

	fclose(f);

	crc32 = c.Get();
}


static CrashHandler s_crashHandler;

//////////////////////////////////////////////////////////////////////////
int __cdecl main(int argCount, char** argv)
{
#if 0
	EnableCrtMemoryChecks();
#endif

	MathHelpers::EnableFloatingPointExceptions(~(_EM_INEXACT | _EM_UNDERFLOW));

	ResourceCompiler rc;

	rc.QueryVersionInfo();
	rc.InitPaths();

	if (argCount <= 1)
	{
		ShowAboutDialog(rc);
		return eRcExitCode_Success;
	}

	std::vector<string> args;
	{
		GetCommandLineArguments(args);

		const string filename = string(rc.GetExePath()) + rc.m_filenameOptions;
		AddCommandLineArgumentsFromFile(args, filename.c_str());
	}

	rc.RegisterKey("_debug", "");   // hidden key for debug-related activities. parsing is module-dependent and subject to change without prior notice.

	rc.RegisterKey("wait",
		"wait for an user action on start and/or finish of RC:\n"
		"0-don't wait (default),\n"
		"1-wait for a key pressed on finish,\n"
		"2-pop up a message box and wait for the button pressed on finish,\n"
		"3-pop up a message box and wait for the button pressed on start,\n"
		"4-pop up a message box and wait for the button pressed on start and on finish");
	rc.RegisterKey("wx","pause and display message box in case of warning or error");
	rc.RegisterKey("recursive","traverse input directory with sub folders");
	rc.RegisterKey("refresh","force recompilation of resources with up to date timestamp");
	rc.RegisterKey("p","to specify platform (for supported names see [_platform] sections in rc.ini)");
	rc.RegisterKey("statistics","log statistics to rc_stats_* files");
	rc.RegisterKey("dependencies",
		"Use it to specify a file with dependencies to be written.\n"
		"Each line in the file will contain an input filename\n"
		"and an output filename for every file written by RC.");
	rc.RegisterKey("clean_targetroot","When 'targetroot' switch specified will clean up this folder after rc runs, to delete all files that were not processed");
	rc.RegisterKey("verbose","to control amount of information in logs: 0-default, 1-detailed, 2-very detailed, etc");
	rc.RegisterKey("quiet","to suppress all printouts");
	rc.RegisterKey("skipmissing","do not produce warnings about missing input files");
	rc.RegisterKey("logfiles","to suppress generating log file rc_log.log");
	rc.RegisterKey("logprefix","prepends this prefix to every log file name used (by default the prefix is the rc.exe's folder).");
	rc.RegisterKey("logtime","logs time passed: 0=off, 1=on (default)");
	rc.RegisterKey("sourceroot","list of source folders separated by semicolon");
	rc.RegisterKey("targetroot","to define the destination folder. note: this folder and its subtrees will be excluded from the source files scanning process");
	rc.RegisterKey("targetnameformat",
		"Use it to specify format of the output filenames.\n"
		"syntax is /targetnameformat=\"<pair[;pair[;pair[...]]]>\" where\n"
		"<pair> is <mask>,<resultingname>.\n"
		"<mask> is a name consisting of normal and wildcard chars.\n"
		"<resultingname> is a name consisting of normal chars and special strings:\n"
		"{0} filename of a file matching the mask,\n"
		"{1} part of the filename matching the first wildcard of the mask,\n"
		"{2} part of the filename matching the second wildcard of the mask,\n"
		"and so on.\n"
		"A filename will be processed by first pair that has matching mask.\n"
		"If no any match for a filename found, then the filename stays\n"
		"unmodified.\n"
		"Example: /targetnameformat=\"*alfa*.txt,{1}beta{2}.txt\"");
	rc.RegisterKey("filesperprocess",
		"to specify number of files converted by one process in one step\n"
		"default is 100. this option is unused if /processes is 0.");
	rc.RegisterKey("threads",
		"use multiple threads. syntax is /threads=<expression>.\n"
		"<expression> is an arithmetical expression consisting of numbers,\n"
		"'cores', 'processors', '+' and '-'. 'cores' is the number of CPU\n"
		"cores; 'processors' is the number of logical processors.\n"
		"if expression is omitted, then /threads=cores is assumed.\n"
		"example: /threads=cores+2");
	rc.RegisterKey("failonwarnings","return error code if warnings are encountered");
	
	rc.RegisterKey("help","lists all usable keys of the ResourceCompiler with description");
	rc.RegisterKey("version","shows version and exits");
	rc.RegisterKey("overwriteextension","ignore existing file extension and use specified converter");
	rc.RegisterKey("overwritefilename","use the filename for output file (folder part is not affected)");
	
	rc.RegisterKey("listfile","Specify List file, List file can contain file lists from zip files like: @Levels\\Test\\level.pak|resourcelist.txt");
	rc.RegisterKey("listformat",
		"Specify format of the file name read from the list file. You may use special strings:\n"
		"{0} the file name from the file list,\n"
		"{1} text matching first wildcard from the input file mask,\n"
		"{2} text matching second wildcard from the input file mask,\n"
		"and so on.\n"
		"Also, you can use multiple format strings, separated by semicolons.\n"
		"In this case multiple filenames will be generated, one for\n"
		"each format string.");
	rc.RegisterKey("copyonly","copy source files to target root without processing");
	rc.RegisterKey("name_as_crc32","When creating Pak File outputs target filename as the CRC32 code without the extension");
	rc.RegisterKey("exclude","List of file exclusions for the command, separated by semicolon, may contain wildcard characters");
	rc.RegisterKey("exclude_listfile","Specify a file which contains a list of files to be excluded from command input");

	rc.RegisterKey("validate","When specified RC is running in a resource validation mode");
	rc.RegisterKey("MailServer","SMTP Mail server used when RC needs to send an e-mail");
	rc.RegisterKey("MailErrors","0=off 1=on When enabled sends an email to the user who checked in asset that failed validation");
	rc.RegisterKey("cc_email","When sending mail this address will be added to CC, semicolon separates multiple addresses");
	rc.RegisterKey("job","Process a job xml file");
	rc.RegisterKey("jobtarget","Run only a job with specific name instead of whole job-file. Used only with /job option");

	rc.RegisterKey("cachefolder", "When specified RC might cache compiled files, to use them instead of re-compiling them again every time");

	string fileSpec;

	// Initialization, showing startup info, loading configs
	{
		Config mainConfig;
		mainConfig.SetConfigKeyRegistry(&rc);

		ParseCommandLine(args, &mainConfig, fileSpec);

		// initialize rc (also initializes logs)
		rc.Init(mainConfig);

		s_crashHandler.SetFiles(
			rc.GetMainLogFileName(),
			rc.GetErrorLogFileName(),
			rc.FormLogFileName(ResourceCompiler::m_filenameCrashDump));

		if (mainConfig.GetAsBool("version", false, true))
		{
			ShowResourceCompilerVersionInfo(rc);
			return eRcExitCode_Success;
		}

		switch (mainConfig.GetAsInt("wait", 0, 1))
		{
		case 3:
		case 4:
			ShowWaitDialog(rc, "start", args, argCount);
			break;
		default:
			break;
		}

		ShowResourceCompilerLaunchInfo(args, argCount, rc);

		rc.SetTimeLogging(mainConfig.GetAsBool("logtime", true, true));

		rc.LogMemoryUsage(false);

		RCLog("");

		if (!rc.LoadIniFile())
		{
			return eRcExitCode_FatalError;
		}

		// Make sure that rc.ini doesn't have obsolete settings
		for (int i = 0; ; ++i)
		{
			const char* const pName = rc.GetIniFile()->GetSectionName(i);
			if (!pName)
			{
				break;
			}

			Config cfg;
			rc.GetIniFile()->CopySectionKeysToConfig(eCP_PriorityRcIni, i, 0, &cfg);

			if (cfg.HasKeyMatchingWildcards("srgb") || cfg.HasKeyMatchingWildcards("srgb:*"))
			{
				RCLogError("Obsolete setting 'srgb' found in %s", rc.m_filenameRcIni);
				RCLog(
					"\n"
					"Please replace all occurences of 'srgb' by corresponding\n"
					"'colorspace' settings. Use the following table as the reference:\n"
					"  srgb=0 -> colorspace=linear,linear\n"
					"  srgb=1 -> colorspace=sRGB,auto\n"
					"  srgb=2 -> colorspace=sRGB,sRGB\n"
					"  srgb=3 -> colorspace=linear,sRGB\n"
					"  srgb=4 -> colorspace=sRGB,linear");
				return eRcExitCode_FatalError;
			}
		}
		
		// Load list of platforms
		{
			for (int i = 0; ; ++i)
			{
				const char* const pName = rc.GetIniFile()->GetSectionName(i);
				if (!pName)
				{
					break;
				}
				if (!StringHelpers::Equals(pName, "_platform"))
				{
					continue;
				}

				Config cfg;
				rc.GetIniFile()->CopySectionKeysToConfig(eCP_PriorityRcIni, i, "", &cfg);

				const string names = StringHelpers::MakeLowerCase(cfg.GetAsString("name", "", ""));
				const bool bBigEndian = cfg.GetAsBool("bigendian", false, true);
				const int pointerSize = cfg.GetAsInt("pointersize", 4, 0);

				if (!rc.AddPlatform(names, bBigEndian, pointerSize))
				{
					RCLogError("Bad platform data in %s", rc.m_filenameRcIni);
					return eRcExitCode_FatalError;
				}
			}

			if (rc.GetPlatformCount() <= 0)
			{
				RCLogError("Missing [_platform] in %s", rc.m_filenameRcIni);
				return eRcExitCode_FatalError;
			}
		}

		// Obtain target platform
		int platform;
		{
			string platformStr = mainConfig.GetAsString("p", "", "");
			if (platformStr.empty())
			{
				if (!mainConfig.GetAsBool("version", false, true))
				{
					RCLog("Platform (/p) not specified, defaulting to 'pc'.");
					RCLog("");
				}
				platformStr = "pc";
				mainConfig.SetKeyValue(eCP_PriorityCmdline, "p", platformStr.c_str());
			}

			platform = rc.FindPlatform(platformStr.c_str());
			if (platform < 0)
			{
				RCLogError("Unknown platform specified: '%s'", platformStr.c_str());
				return eRcExitCode_FatalError;
			}

			// Error if the platform specified is not supported by RC
			const char* const unsupportedPlatforms[] =
			{
#if !defined(TOOLS_SUPPORT_DURANGO)
				"XOne", "XboxOne", "Durango",
#endif
#if !defined(TOOLS_SUPPORT_ORBIS)
				"PS4", "Orbis",
#endif
#if !defined(TOOLS_SUPPORT_POWERVR)
				"es3",
#endif
				""
			};
			for (int i = 0; i < sizeof(unsupportedPlatforms) / sizeof(unsupportedPlatforms[0]); ++i)
			{
				if (rc.GetPlatformInfo(platform)->HasName(unsupportedPlatforms[i]))
				{
					RCLogError("Platform specified ('%s') is not supported by this RC", platformStr.c_str());
					return eRcExitCode_FatalError;
				}
			}
		}

		// Load configs for every platform
		rc.GetMultiplatformConfig().init(rc.GetPlatformCount(), platform, &rc);
		for (int i = 0; i < rc.GetPlatformCount(); ++i)
		{
			IConfig& cfg = rc.GetMultiplatformConfig().getConfig(i);
			rc.GetIniFile()->CopySectionKeysToConfig(eCP_PriorityRcIni, 0, rc.GetPlatformInfo(i)->GetCommaSeparatedNames().c_str(), &cfg);
			cfg.AddConfig(&mainConfig);
		}
	}

	const IConfig& config = rc.GetMultiplatformConfig().getConfig();

	{
		RCLog("Initializing pak management");
		rc.InitPakManager();
		RCLog("");

		RCLog("Loading compiler plug-ins (ResourceCompiler*.dll)");
		if (!rc.RegisterConverters())
		{
			RCLogError("A fatal error occurred when loading plug-ins (see error message(s) above). RC cannot continue.");
			rc.UnregisterConverters();
			return eRcExitCode_FatalError;
		}
		RCLog("");

		RCLog("Loading zip & pak compiler module");
		rc.RegisterConverter("zip & pak compiler", new ZipEncryptor(&rc));
		RCLog("");

		rc.LogMemoryUsage(false);
	}

	const bool bJobMode = config.HasKey("job");
	if (!bJobMode && !CheckCommandLineOptions(config, 0))
	{
		rc.UnregisterConverters();
		return eRcExitCode_Error;
	}

	bool bExitCodeIsReady = false;
	int exitCode = eRcExitCode_Success;

	bool bShowUsage = false;
	if (bJobMode)
	{
		const int tmpResult = rc.ProcessJobFile();
		if (tmpResult)
		{
			exitCode = tmpResult;
			bExitCodeIsReady = true;
		}
		rc.PostBuild();      // e.g. writing statistics files
	}
	else if (!fileSpec.empty())
	{
		rc.RemoveOutputFiles();
		std::vector<RcFile> files;
		if (rc.CollectFilesToCompile(fileSpec, files) && !files.empty())
		{
			rc.SetupMaxThreads();
			rc.CompileFilesBySingleProcess(files);
		}
		rc.PostBuild();      // e.g. writing statistics files
	}
	else
	{
		bShowUsage = true;
	}

	rc.UnregisterConverters();

	rc.SetTimeLogging(false);

	if (bShowUsage && !rc.m_bQuiet)
	{
		rc.ShowHelp(false);
	}

	if (config.GetAsBool("help", false, true))
	{
		rc.ShowHelp(true);
	}

	rc.LogMemoryUsage(false);

	RCLog("");
	RCLog("Finished at: %s", GetTimeAsString(time(0)).c_str());

	if (rc.GetNumErrors() || rc.GetNumWarnings())
	{
		RCLog("");
		RCLog("%d errors, %d warnings.", rc.GetNumErrors(), rc.GetNumWarnings());
	}

	if (!bExitCodeIsReady)
	{
		const bool bFail = rc.GetNumErrors() || (rc.GetNumWarnings() && config.GetAsBool("failonwarnings", false, true));
		exitCode = (bFail ? eRcExitCode_Error : eRcExitCode_Success);
	}

	switch (config.GetAsInt("wait", 0, 1))
	{
	case 1:
		RCLog("");
		RCLog("    Press <RETURN> (/wait was specified)");
		getchar();
		break;
	case 2:
	case 4:
		ShowWaitDialog(rc, "finish", args, argCount);
		break;
	default:
		break;
	}

	return exitCode;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::PostBuild()
{
	const IConfig* const config = &m_multiConfig.getConfig();

	// Save list of created files.
	{
		m_inputOutputFileList.RemoveDuplicates();
		m_inputOutputFileList.SaveOutputOnly(FormLogFileName(m_filenameCreatedFileList));
	}

	if (!m_cacheFolder.empty() && GetVerbosityLevel() >= 0)
	{
		RCLog("Timing:");
		RCLog("");
		RCLog("Compiling files: %f sec.", CCompilerProcessTimer::Total());
		RCLog("Computing digest: %f sec.", CComputeDigestTimer::Total());
		RCLog("Getting files from the cachefolder: %f sec.", CCacheGetFilesTimer::Total());
		RCLog("Adding files to the cachefolder: %f sec.", CCacheAddFilesTimer::Total());
		RCLog("");
	}

	switch (config->GetAsInt("clean_targetroot", 0, 1))
	{
	case 1:
		CleanTargetFolder(false);
		break;
	case 2:
		CleanTargetFolder(true);
		break;
	default:
		break;
	}

	const string dependenciesFilename = m_multiConfig.getConfig().GetAsString("dependencies", "", "");
	if (!dependenciesFilename.empty())
	{
		m_inputOutputFileList.RemoveDuplicates();
		m_inputOutputFileList.Save(dependenciesFilename.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::QueryVersionInfo()
{
	wchar_t moduleNameW[MAX_PATH];
	{
		const int bufferCharCount = sizeof(moduleNameW) / sizeof(moduleNameW[0]);
		const int charCount = GetModuleFileNameW(NULL, moduleNameW, bufferCharCount);
		if (charCount <= 0 || charCount >= bufferCharCount)
		{
			printf("RC QueryVersionInfo(): fatal error");
			exit(eRcExitCode_FatalError);
		}
		moduleNameW[charCount] = 0;
	}

	m_exePath = PathHelpers::GetAbsolutePath(moduleNameW);
	if (m_exePath.empty())
	{
		printf("RC module name: fatal error");
		exit(eRcExitCode_FatalError);
	}
	m_exePath = PathUtil::AddSlash(PathHelpers::GetDirectory(m_exePath));

	DWORD handle;
	char ver[1024*8];
	const int verSize = GetFileVersionInfoSizeW(moduleNameW, &handle);
	if (verSize > 0 && verSize <= sizeof(ver))
	{
		GetFileVersionInfoW(moduleNameW, 0, sizeof(ver), ver);
		VS_FIXEDFILEINFO *vinfo;
		UINT len;
		VerQueryValue(ver, "\\", (void**)&vinfo, &len);

		m_fileVersion.v[0] = vinfo->dwFileVersionLS & 0xFFFF;
		m_fileVersion.v[1] = vinfo->dwFileVersionLS >> 16;
		m_fileVersion.v[2] = vinfo->dwFileVersionMS & 0xFFFF;
		m_fileVersion.v[3] = vinfo->dwFileVersionMS >> 16;
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::InitPaths()
{
	if (m_exePath.empty())
	{
		printf("RC InitPaths(): internal error");
		exit(eRcExitCode_FatalError);
	}

	// Get wide-character temp path from OS
	{
		static const DWORD bufferSize = MAX_PATH;
		wchar_t bufferWchars[bufferSize];

		DWORD resultLength = GetTempPathW(bufferSize, bufferWchars);
		if ((resultLength >= bufferSize) || (resultLength <= 0))
		{
			resultLength = 0;
		}
		bufferWchars[resultLength] = 0;

		m_tempPath = PathHelpers::GetAbsolutePath(bufferWchars);
		if (m_tempPath.empty())
		{
			m_tempPath = m_exePath;
		}
		m_tempPath = PathUtil::AddSlash(m_tempPath);
	}

	m_initialCurrentDir = PathHelpers::GetAbsolutePath(".");
	if (m_initialCurrentDir.empty())
	{
		printf("RC InitPaths(): internal error");
		exit(eRcExitCode_FatalError);
	}
	m_initialCurrentDir = PathUtil::AddSlash(m_initialCurrentDir);
}

//////////////////////////////////////////////////////////////////////////
bool ResourceCompiler::LoadIniFile()
{
	const string filename = PathUtil::ToDosPath(m_exePath) + m_filenameRcIni;

	RCLog("Loading \"%s\"", filename.c_str());

	if (!FileUtil::FileExists(filename.c_str()))
	{
		RCLogError("Resource compiler .ini file (%s) is missing.", filename.c_str());
		return false;
	}

	if (!m_iniFile.Load(filename))
	{
		RCLogError("Failed to load resource compiler .ini file (%s).", filename.c_str());
		return false;
	}

	RCLog("  Loaded \"%s\"", filename.c_str());
	RCLog("");

	if (m_pDigest)
	{
		CComputeDigestTimer accumulateDuration;
		m_pDigest->UpdateFromFile(filename);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::InitFileCache(Config & config)
{
	string path = config.GetAsString("cachefolder", "", "");
	if (path.empty())
		return;

	m_cacheFolder = path;
	m_pDigest.reset(new CDigest);

	const string filename = PathUtil::ToDosPath(m_exePath) + m_filenameRcExe;

	{
		CComputeDigestTimer accumulateDuration;
		m_pDigest->UpdateFromFile(filename);
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::TryToGetFilesFromCache(FilesToConvert & files, IConverter * converter)
{
	assert(m_pDigest);
	assert(!m_cacheFolder.empty());

	auto computeFileDigest = [this](const string& path)
	{
		CComputeDigestTimer accumulateDuration;

		CDigest digest = m_pDigest.get()->Clone();
		digest.UpdateFromFile(path);
		return digest.Final();
	};

	CFileStorage fileStorage(m_cacheFolder);

	std::vector<RcFile> inputFiles(files.m_inputFiles);
	files.m_inputFiles.clear();

	const string progressString("Getting files from the cache");
	RCLog("%s %s.", progressString, m_cacheFolder);

	StartProgress();

	const size_t numFiles = inputFiles.size();
	for (size_t i = 0; i < numFiles; ++i)
	{
		ShowProgress(progressString.c_str(), i, numFiles);

		RcFile& file = inputFiles[i];

		const string sourceFullFileName = PathUtil::Make(file.m_sourceLeftPath, file.m_sourceInnerPathAndName);
		const string sourceFileName = PathUtil::GetFile(file.m_sourceInnerPathAndName);
		const string sourceInnerPath = PathHelpers::GetDirectory(file.m_sourceInnerPathAndName);
		const string targetPath = PathUtil::Make(file.m_targetLeftPath, sourceInnerPath);
		const string outputFolder = PathHelpers::GetAbsolutePath(targetPath);

		FileUtil::EnsureDirectoryExists(outputFolder);

		const string key = string().Format("%s-%s", sourceFileName, computeFileDigest(sourceFullFileName).c_str()).replace(".", "-");

		std::vector<string> cachedFiles;

		{
			CCacheGetFilesTimer accumulateDuration;
			fileStorage.CopyFiles(key, outputFolder, cachedFiles);
		}

		if (cachedFiles.empty())
		{
			file.m_sourceDigest = key;
			files.m_inputFiles.push_back(file);
			continue;
		}

		for (const string& cachedFile : cachedFiles)
		{
			AddInputOutputFilePair(sourceFullFileName, cachedFile);
		}

		files.m_convertedFiles.push_back(file);

		if (GetVerbosityLevel() >= 1)
		{
			RCLog("file='%s'", file.m_sourceInnerPathAndName.c_str());
		}
	}

	FinishProgress();

	size_t numFilesRestored = numFiles - files.m_inputFiles.size();
	RCLog("%d file%s restored from the cache.", numFilesRestored, (numFilesRestored > 1 ? "s" : ""));
	RCLog("");
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::AddFilesToTheCache(const FilesToConvert & files)
{
	assert(!m_cacheFolder.empty());

	const string progressString("Adding files to the cache");

	RCLog("%s %s.", progressString, m_cacheFolder);

	StartProgress();

	CCacheAddFilesTimer accumulateDuration;

	CFileStorage fileStorage(m_cacheFolder);

	std::vector<string> outputFiles;

	const size_t numFiles = files.m_convertedFiles.size();
	size_t numSourceFilesCached = 0;
	size_t numOutputFilesAdded = 0;
	for (size_t i = 0; i < numFiles; ++i)
	{
		ShowProgress(progressString.c_str(), i, numFiles);

		const RcFile& file = files.m_convertedFiles[i];

		if (file.m_sourceDigest.empty())
		{
			continue;
		}

		outputFiles.clear();

		const string sourceFullFileName = PathUtil::Make(file.m_sourceLeftPath, file.m_sourceInnerPathAndName);

		m_inputOutputFileList.GetOutputFiles(sourceFullFileName, outputFiles);

		fileStorage.SetFiles(file.m_sourceDigest, outputFiles);

		if (GetVerbosityLevel() >= 1)
		{
			RCLog("file='%s'", file.m_sourceInnerPathAndName);
		}
		if (GetVerbosityLevel() >= 1)
		{
			for (const string& outputFile : outputFiles)
			{
				RCLog("\t""output file='%s'", outputFile);
			}
		}

		++numSourceFilesCached;
		numOutputFilesAdded += outputFiles.size();
	}

	FinishProgress();

	RCLog("%d source file%s cached, %d compiled file%s added to the cache.", 
		numSourceFilesCached, 
		(numSourceFilesCached > 1 ? "s" : ""),
		numOutputFilesAdded,
		(numOutputFilesAdded > 1 ? "s" : ""));

	RCLog("");
}

IAssetManager* ResourceCompiler::GetAssetManager() const
{
	assert(m_pAssetManager);
	return m_pAssetManager.get();
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::Init(Config& config)
{
	m_bQuiet = config.GetAsBool("quiet", false, true);
	m_verbosityLevel = config.GetAsInt("verbose", 0, 1);

	m_bWarningsAsErrors = config.GetAsBool("wx", false, true);

	m_startTime = clock();
	m_bTimeLogging = false;

	InitLogs(config);
	SetRCLog(this);

	InitFileCache(config);
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::UnregisterConverters()
{
	m_extensionManager.UnregisterAll();
}

//////////////////////////////////////////////////////////////////////////
MultiplatformConfig& ResourceCompiler::GetMultiplatformConfig()
{
	return m_multiConfig;
}

//////////////////////////////////////////////////////////////////////////
static int ComputeExpression(
	const char* a_optionName, 
	int a_processCpuCoreCount, 
	int a_processLogicalProcessorCount, 
	const string& a_expression, 
	string& a_message)
{
	string expression = a_expression;
	a_message.clear();
	int expressionResult = 0;
	char op = 0;
	for (;;)
	{
		expression = StringHelpers::TrimLeft(expression);
		if (expression.empty())
		{
			break;
		}

		if ((expression[0] == '-') || (expression[0] == '+'))
		{
			if ((op == '+') || (op == '-'))
			{
				a_message.Format("/%s syntax error: '%s'.", a_optionName, a_expression.c_str());
				break;					
			}

			op = expression[0];
			expression = expression.substr(1, expression.npos);
		}
		else
		{
			if (op == '?')
			{
				a_message.Format("/%s syntax error: '%s'.", a_optionName, a_expression.c_str());
				break;					
			}

			const size_t pos = expression.find_first_of(" +-");
			const string token = expression.substr(0, pos);
			expression = (pos == expression.npos) ? "" : expression.substr(pos, expression.npos);

			int tokenInt = 0;
			if (StringHelpers::Equals(token, "cores"))
			{
				tokenInt = a_processCpuCoreCount;
			}
			else if (StringHelpers::Equals(token, "processors"))
			{
				tokenInt = a_processLogicalProcessorCount;
			}
			else if (token.find_first_not_of("0123456789") == token.npos)
			{
				expressionResult  += ((op == '-') ? -1 : 1) * atol(token.c_str());
			}
			else
			{
				a_message.Format("/%s contains unknown parameter '%s': '%s'.", a_optionName, token.c_str(), a_expression.c_str());
				break;
			}
			expressionResult += ((op == '-') ? -1 : 1) * tokenInt;
			op = '?';
		}
	}

	return expressionResult;
}

//////////////////////////////////////////////////////////////////////////
static int GetAParallelOption(
	const IConfig* a_config,
	const char* a_optionName, 
	int a_processCpuCoreCount, 
	int a_processLogicalProcessorCount,
	int a_valueIfNotSpecified,
	int a_valueIfEmpty,
	int a_minValue,
	int a_maxValue,
	string& message)
{
	int result = -1;

	const string optionValue = a_config->GetAsString(a_optionName, "?", "");

	if (optionValue.empty())
	{
		message.Format("/%s specified without value.", a_optionName);
		result = a_valueIfEmpty;
	}
	else if (optionValue[0] == '?')
	{
		message.Format("/%s was not specified.", a_optionName);
		result = a_valueIfNotSpecified;
	}
	else
	{
		const int expressionResult = ComputeExpression(a_optionName, a_processCpuCoreCount, a_processLogicalProcessorCount, optionValue, message);		
		if (!message.empty())
		{
			result = a_valueIfNotSpecified;
		}
		else if (expressionResult < a_minValue)
		{
			message.Format("/%s specified with too small value %d: '%s'.", a_optionName, expressionResult, optionValue.c_str());
			result = a_minValue;
		}
		else if (expressionResult > a_maxValue)
		{
			message.Format("/%s specified with too big value %d: '%s'.", a_optionName, expressionResult, optionValue.c_str());
			result = a_maxValue;
		}
		else
		{
			message.Format("/%s specified with value %d: '%s'.", a_optionName, expressionResult, optionValue.c_str());
			result = expressionResult;
		}
	}

	if ((result != 1) && a_config->GetAsBool("validate", false, true))
	{
		message.Format("/%s forced to 1 because RC is in resource validation mode (see /validate).", a_optionName);
		result = 1;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
static unsigned int CountBitsSetTo1(const DWORD_PTR val)
{
	unsigned int result = 0;
	const size_t bitCount = sizeof(val) * 8;
	for (size_t i = 0; i < bitCount; ++i)
	{
		result += (val & (((DWORD_PTR)1) << i)) ? 1 : 0;
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::SetupMaxThreads()
{
	const IConfig* const config = &m_multiConfig.getConfig();

	{
		unsigned int numCoresInSystem = 0;
		unsigned int numCoresAvailableToProcess = 0;
		GetNumCPUCores(numCoresInSystem, numCoresAvailableToProcess);
		if (numCoresAvailableToProcess <= 0)
		{
			numCoresAvailableToProcess = 1;
		}

		unsigned int numLogicalProcessorsAvailableToProcess = 0;
		{
			DWORD_PTR processAffinity = 0;
			DWORD_PTR systemAffinity = 0;
			GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity);

			numLogicalProcessorsAvailableToProcess = CountBitsSetTo1(processAffinity);

			if (numLogicalProcessorsAvailableToProcess < numCoresAvailableToProcess)
			{
				numLogicalProcessorsAvailableToProcess = numCoresAvailableToProcess;
			}
		}

		unsigned int numLogicalProcessorsInSystem = 0;
		{
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			numLogicalProcessorsInSystem = si.dwNumberOfProcessors;
		}

		m_systemCpuCoreCount = numCoresInSystem;
		m_processCpuCoreCount = numCoresAvailableToProcess;
		m_systemLogicalProcessorCount = numLogicalProcessorsInSystem;
		m_processLogicalProcessorCount = numLogicalProcessorsAvailableToProcess;
	}

	RCLog("");
	RCLog("CPU cores: %d available (%d in system).",
		m_processCpuCoreCount, 
		m_systemCpuCoreCount);				
	RCLog("Logical processors: %d available (%d in system).",
		m_processLogicalProcessorCount, 
		m_systemLogicalProcessorCount);

	string message;

	m_maxThreads = GetAParallelOption(
		config,
		"threads", 
		m_processCpuCoreCount, 
		m_processLogicalProcessorCount,
		1,
		m_processCpuCoreCount,
		1,
		m_processLogicalProcessorCount,
		message);
	RCLog("%s Using up to %d thread%s.", message.c_str(), m_maxThreads, ((m_maxThreads > 1) ? "s" : ""));

	RCLog("");

	assert(m_maxThreads >= 1);

	m_pPakManager->SetMaxThreads(m_maxThreads);
}

//////////////////////////////////////////////////////////////////////////
int ResourceCompiler::GetMaxThreads() const
{
	return m_maxThreads;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::VerifyKeyRegistration( const char *szKey ) const
{
	assert(szKey);
	const string sKey = StringHelpers::MakeLowerCase(string(szKey));

	if (m_KeyHelp.count(sKey) == 0)
	{
		RCLogWarning("Key '%s' was not registered, call RegisterKey() before using the key", szKey);
	}
}

//////////////////////////////////////////////////////////////////////////
bool ResourceCompiler::HasKeyRegistered( const char *szKey ) const
{
	assert(szKey);
	const string sKey = StringHelpers::MakeLowerCase(string(szKey));

	return (m_KeyHelp.count(sKey) != 0);
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::RegisterKey( const char *key, const char *helptxt )
{
	const string sKey = StringHelpers::MakeLowerCase(string(key));

	assert(m_KeyHelp.count(sKey)==0);		// registered twice

	m_KeyHelp[sKey] = helptxt;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::InitLogs(Config& config)
{
	m_logPrefix.clear();
	m_logPrefix = config.GetAsString("logprefix", "", "");
	if (m_logPrefix.empty())
	{
		m_logPrefix = m_exePath;
	}

	{
		const string logDir = PathHelpers::GetDirectory(m_logPrefix + "unused.name");
		if (!FileUtil::EnsureDirectoryExists(logDir.c_str()))
		{
			RCLogError("Creating directory failed: %s", logDir.c_str());
		}
	}

	m_mainLogFileName    = FormLogFileName(m_filenameLog);
	m_warningLogFileName = FormLogFileName(m_filenameLogWarnings);
	m_errorLogFileName   = FormLogFileName(m_filenameLogErrors);

	DeleteFile(m_mainLogFileName);
	DeleteFile(m_warningLogFileName);
	DeleteFile(m_errorLogFileName);

	if (config.GetAsBool("logfiles", false, true))
	{
		m_mainLogFileName = "";
	}
}

string ResourceCompiler::FormLogFileName(const char* suffix) const
{
	return (suffix && suffix[0]) ? m_logPrefix + suffix : string();
}

const string& ResourceCompiler::GetMainLogFileName() const
{
	return m_mainLogFileName;
}

const string& ResourceCompiler::GetErrorLogFileName() const
{
	return m_errorLogFileName;
}

//////////////////////////////////////////////////////////////////////////
clock_t ResourceCompiler::GetStartTime() const
{
	return m_startTime;
}

bool ResourceCompiler::GetTimeLogging() const
{
	return m_bTimeLogging;
}

void ResourceCompiler::SetTimeLogging(bool enable)
{
	m_bTimeLogging = enable;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::AddExitObserver(IResourceCompiler::IExitObserver* p)
{
	if (p == 0)
	{
		return;
	}

	ThreadUtils::AutoLock lock(m_exitObserversLock);

	m_exitObservers.push_back(p);
}

void ResourceCompiler::RemoveExitObserver(IResourceCompiler::IExitObserver* p)
{
	if (p == 0)
	{
		return;
	}

	ThreadUtils::AutoLock lock(m_exitObserversLock);

	for (size_t i = 0; i < m_exitObservers.size(); ++i)
	{
		if (m_exitObservers[i] == p)
		{
			m_exitObservers.erase(m_exitObservers.begin() + i);
			return;
		}
	}
}

void ResourceCompiler::NotifyExitObservers()
{
	ThreadUtils::AutoLock lock(m_exitObserversLock);

	for (size_t i = 0; i < m_exitObservers.size(); ++i)
	{
		m_exitObservers[i]->OnExit();
	}

	m_exitObservers.clear();
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::LogV(const IRCLog::EType eType, const char* szFormat, va_list args)
{
	char str[4 * 1024];

	cry_vsprintf(str, szFormat, args);

	// remove non-printable characters except newlines and tabs
	char* p = str;
	while (*p)
	{
		if ((*p < ' ') && (*p != '\n') && (*p != '\t'))
		{
			*p = ' ';
		}
		++p;
	}

	LogLine(eType, str);
}

namespace
{
struct CheckEmptySourceInnerPathAndName
{
	bool operator()(const RcFile& file) const
	{
		return file.m_sourceInnerPathAndName.empty();
	}
};
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::FilterExcludedFiles(std::vector<RcFile>& files)
{
	const IConfig* const config = &m_multiConfig.getConfig();

	const bool bVerbose = GetVerbosityLevel() > 0;

	std::vector<string> excludes;
	{
		string excludeStr = config->GetAsString("exclude", "", "", eCP_PriorityAll & ~eCP_PriorityJob);
		if (!excludeStr.empty())
		{
			excludeStr.replace('/', '\\');
			StringHelpers::Split(excludeStr, ";", false, excludes);
		}

		excludeStr = config->GetAsString("exclude", "", "", eCP_PriorityJob);
		if (!excludeStr.empty())
		{
			excludeStr.replace('/', '\\');
			StringHelpers::Split(excludeStr, ";", false, excludes);
		}

		if (bVerbose)
		{
			excludeStr.clear();
			for (size_t i = 0; i < excludes.size(); ++i)
			{
				if (i > 0)
				{
					excludeStr += ";";
				}
				excludeStr += excludes[i];
			}
			RCLog( "   Exclude: %s", excludeStr.c_str());
		}
	}

	std::set<const char*, stl::less_stricmp<const char*> > excludedFiles;
	std::vector<std::pair<string, string> > excludedStrings;
	const string excludeListFile = config->GetAsString("exclude_listfile", "", "");
	if (!excludeListFile.empty())
	{
		const string listFormat = config->GetAsString("listformat", "" ,"");
		CListFile(this).Process(excludeListFile, listFormat, "*", string(), excludedStrings);
		const size_t filenameCount = excludedStrings.size();
		for (size_t i = 0; i < filenameCount; ++i)
		{
			string& name = excludedStrings[i].second;
			name.replace('/', '\\');
			excludedFiles.insert(name.c_str());
		}
	}

	if (excludes.empty() && excludedFiles.empty())
	{
		return;
	}

	string name;
	for (size_t nFile = 0; nFile < files.size(); ++nFile)
	{
		name = files[nFile].m_sourceInnerPathAndName;
		name.replace('/', '\\');

		if (excludedFiles.find(name.c_str()) != excludedFiles.end())
		{
			if (bVerbose)
			{
				RCLog( "    Excluding file %s by %s", name.c_str(), excludeListFile.c_str());
			}
			files[nFile].m_sourceInnerPathAndName.clear();
			continue;
		}

		for (size_t i = 0; i < excludes.size(); ++i)
		{
			if (StringHelpers::MatchesWildcardsIgnoreCase(name, excludes[i]))
			{
				if (bVerbose)
				{
					RCLog( "    Excluding file %s by %s", name.c_str(), excludes[i].c_str());
				}
				files[nFile].m_sourceInnerPathAndName.clear();
				break;
			}
		}
	}

	const size_t sizeBefore = files.size();
	files.erase(std::remove_if(files.begin(), files.end(), CheckEmptySourceInnerPathAndName()), files.end());

	RCLog("Files excluded: %i", sizeBefore - files.size());
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::CopyFiles(const std::vector<RcFile>& files)
{
	const IConfig* const config = &m_multiConfig.getConfig();

	const bool bSkipMissing = config->GetAsBool("skipmissing", false, true);
	const int srcMaxSize = config->GetAsInt("sourcemaxsize", -1, -1);

	const size_t numFiles = files.size();
	size_t numFilesCopied = 0;
	size_t numFilesUpToDate = 0;
	size_t numFilesSkipped = 0;
	size_t numFilesMissing = 0;
	size_t numFilesFailed = 0;

	RCLog("Starting copying %" PRISIZE_T" files", numFiles);

	string progressString;
	progressString.Format("Copying %" PRISIZE_T" files", numFiles);
	StartProgress();

	const bool bRefresh = config->GetAsBool("refresh", false, true);

	NameConverter nc;
	if (!nc.SetRules(config->GetAsString("targetnameformat", "", "")))
	{
		return;
	}

	for (size_t i = 0; i < numFiles; ++i)
	{
		ShowProgress(progressString.c_str(), i, numFiles);

		const string srcFilename = PathUtil::Make(files[i].m_sourceLeftPath, files[i].m_sourceInnerPathAndName);
		string trgFilename = PathUtil::Make(files[i].m_targetLeftPath, files[i].m_sourceInnerPathAndName);
		if (nc.HasRules()) 
		{
			const string oldFilename = PathUtil::GetFile(trgFilename);
			const string newFilename = nc.GetConvertedName(oldFilename);
			if (newFilename.empty())
			{
				return;
			}
			if (!StringHelpers::EqualsIgnoreCase(oldFilename, newFilename))
			{
				if (GetVerbosityLevel() >= 2)
				{
					RCLog("Target file name changed: %s -> %s", oldFilename.c_str(), newFilename.c_str());
				}
				trgFilename = PathUtil::Make(PathHelpers::GetDirectory(trgFilename), newFilename);
			}
		}

		if (GetVerbosityLevel() >= 1)
		{
			RCLog("Copying %s to %s", srcFilename.c_str(), trgFilename.c_str());
		}

		const bool bSourceFileExists = FileUtil::FileExists(srcFilename.c_str());

		if (!bSourceFileExists)
		{
			++numFilesMissing;
			if (!bSkipMissing)
			{
				RCLog("Source file %s does not exist", srcFilename.c_str());
			}
			continue;
		}
		else
		{
			if (!FileUtil::EnsureDirectoryExists(PathHelpers::GetDirectory(trgFilename).c_str()))
			{
				RCLog("Failed creating directory for %s", trgFilename.c_str());
				++numFilesFailed;
				RCLog("Failed to copy %s to %s", srcFilename.c_str(), trgFilename.c_str());
				continue;
			}

			//////////////////////////////////////////////////////////////////////////
			// Compare source and target files modify timestamps.
			FILETIME ftSource = FileUtil::GetLastWriteFileTime(srcFilename);
			FILETIME ftTarget = FileUtil::GetLastWriteFileTime(trgFilename);

			if (ftSource.dwHighDateTime == ftTarget.dwHighDateTime &&
				ftSource.dwLowDateTime == ftTarget.dwLowDateTime &&
				!bRefresh)
			{
				// Up to date file already exists in target folder
				++numFilesUpToDate;
				AddInputOutputFilePair(srcFilename, trgFilename);
				continue;
			}
			//////////////////////////////////////////////////////////////////////////

			if (srcMaxSize >= 0)
			{
				const __int64 fileSize = FileUtil::GetFileSize(srcFilename);
				if (fileSize > srcMaxSize)
				{
					++numFilesSkipped;
					RCLog("Source file %s is bigger than %d bytes (size is %" PRId64" ). Skipped.", srcFilename.c_str(), srcMaxSize, fileSize);
					AddInputOutputFilePair(srcFilename, trgFilename);
					continue;
				}
			}

			SetFileAttributes(trgFilename,FILE_ATTRIBUTE_ARCHIVE);
			const bool bCopied = (::CopyFile( srcFilename,trgFilename,FALSE ) != 0);

			if (bCopied)
			{
				++numFilesCopied;
				SetFileAttributes(trgFilename,FILE_ATTRIBUTE_ARCHIVE);
				FileUtil::SetFileTimes(trgFilename,ftSource);
			}
			else
			{
				++numFilesFailed;
				RCLog("Failed to copy %s to %s", srcFilename.c_str(), trgFilename.c_str());
			}
		}

		AddInputOutputFilePair(srcFilename, trgFilename);
	}

	RCLog("Finished copying %" PRISIZE_T " files: "
		"%" PRISIZE_T" copied, "
		"%" PRISIZE_T" up-to-date, "
		"%" PRISIZE_T" skipped, "
		"%" PRISIZE_T" missing, "
		"%" PRISIZE_T" failed",
		numFiles, numFilesCopied, numFilesUpToDate, numFilesSkipped, numFilesMissing, numFilesFailed);
}

//////////////////////////////////////////////////////////////////////////
string ResourceCompiler::FindSuitableSourceRoot(const std::vector<string>& sourceRootsReversed, const string& fileName)
{
	if (sourceRootsReversed.empty())
	{
		return string();
	}
	if (sourceRootsReversed.size() > 1)
	{
		for (size_t i = 0; i < sourceRootsReversed.size(); ++i)
		{
			const string& sourceRoot = sourceRootsReversed[i];
			const string fullPath = PathUtil::Make(sourceRoot, fileName);
			const DWORD fileAttribs = GetFileAttributes(fullPath);
			if (fileAttribs == INVALID_FILE_ATTRIBUTES)
			{
				continue;
			}
			if ((fileAttribs & FILE_ATTRIBUTE_NORMAL) != 0)
			{
				return sourceRoot;
			}
		}
	}

	return sourceRootsReversed[0];
}

//////////////////////////////////////////////////////////////////////////
typedef std::set<const char*, stl::less_strcmp<const char*> > References;
static bool FindReference(const References& references, const char* filename)
{
	// FIXME: for .mtl we have paths with gamefolder included
	// that's why we first check with gamefolder in path...
	if (references.find(filename) != references.end())
	{
		return true;
	}

	// ...then trim gamefolder and check again
	filename = strchr(filename, '/');
	if (!filename)
	{
		return false;
	}

	// Trim slash
	++filename;

	return references.find(filename) != references.end();
}

//////////////////////////////////////////////////////////////////////////
// Transforms *.$dds -> *.dds, *.$caf -> *.caf, etc.
static string FixAssetPath(const char* path)
{
	const char* const ext = PathUtil::GetExt(path);
	if (ext[0] == '$')
	{
		return PathUtil::ReplaceExtension(path, ext + 1);
	}
	else
	{
		return path;
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::ScanForAssetReferences( std::vector<string>& outReferences, const string& refsRoot )
{
	const IConfig* const config = &m_multiConfig.getConfig();

	const string targetroot = config->GetAsString("targetroot", "", "");
	if (targetroot.empty())
	{
		return;
	}

	const char* const scanRoot = ".";

	RCLog("Scanning for asset references in \"%s\"", scanRoot);

	int numSources = 0;

	References references;

	std::list<string> strings; // storage for all strings
	std::vector<char*> lines;

	TextFileReader reader;

	std::vector<string> resourceListFiles;
	FileUtil::ScanDirectory(scanRoot, "auto_resource*.txt", resourceListFiles, true, string());
	FileUtil::ScanDirectory(scanRoot, "resourcelist.txt", resourceListFiles, true, string());
	for (size_t i = 0; i < resourceListFiles.size(); ++i)
	{
		const string& resourceListFile = resourceListFiles[i];
		if (reader.Load(resourceListFile.c_str(), lines))
		{
			for (size_t j = 0; j < lines.size(); ++j)
			{
				const char* const line = lines[j];
				if (references.find(line) == references.end())
				{
					strings.push_back(line);
					references.insert(strings.back().c_str());
				}
			}
			++numSources;
		}
	}

	std::vector<string> levelPaks;
	FileUtil::ScanDirectory(scanRoot, "level.pak", levelPaks, true, string());	
	for (size_t i = 0; i < levelPaks.size(); ++i)
	{
		const string path = PathHelpers::GetDirectory(levelPaks[i].c_str()) + "\\resourcelist.txt";
		if (reader.LoadFromPak(GetPakSystem(), path, lines))
		{
			for (size_t j = 0; j < lines.size(); ++j)
			{
				const char* const line = lines[j];
				if (references.find(line) == references.end())
				{
					strings.push_back(line);
					references.insert(strings.back().c_str());
				}
			}
			++numSources;
		}
	}

	RCLog("Found %i unique references in %i sources", references.size(), numSources);
	RCLog("");

	if (refsRoot.empty())
	{
		outReferences.insert(outReferences.end(), references.begin(), references.end());
	}
	else
	{
		// include mipmaps together with referenced dds-textures
		const string ddsExt("dds");
		const string cgfExt("cgf");
		const string chrExt("chr");
		const string skinExt("skin");
		char extSuffix[16];
		
		for (References::iterator it = references.begin(); it != references.end(); ++it)
		{
			const string ext = PathUtil::GetExt(*it);
			const string dosPath = PathUtil::ToDosPath(*it);

			if (StringHelpers::EqualsIgnoreCase(ext, ddsExt))
			{
				string fullPath;
				string mipPath;
				for (int mip = 0;; ++mip)
				{
					cry_sprintf(extSuffix, ".%i", mip);
					fullPath = PathUtil::Make(refsRoot, dosPath + extSuffix);
					if (!FileUtil::FileExists(fullPath.c_str()))
					{
						break;
					}
					outReferences.push_back(string(*it) + extSuffix);
				}
				for (int mip = 0;; ++mip)
				{
					cry_sprintf(extSuffix, ".%ia", mip);
					fullPath = PathUtil::Make(refsRoot, dosPath + extSuffix);
					if (!FileUtil::FileExists(fullPath.c_str()))
					{
						break;
					}
					outReferences.push_back(string(*it) + extSuffix);
				}
			}
			else if (StringHelpers::EqualsIgnoreCase(ext, cgfExt) || StringHelpers::EqualsIgnoreCase(ext, chrExt) || StringHelpers::EqualsIgnoreCase(ext, skinExt))
			{
				static const char* const cgfmext = "m";

				string fullPath;
				fullPath = PathUtil::Make(refsRoot, dosPath + cgfmext);
				if (FileUtil::FileExists(fullPath.c_str()))
				{
					outReferences.push_back(string(*it) + cgfmext);
				}
			}

			// we are interested only in existing files
			const string fullPath = PathUtil::Make(refsRoot, dosPath);
			if (FileUtil::FileExists(fullPath))
			{
				outReferences.push_back(*it);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
static bool MatchesWildcardsSet(const string& str, const std::vector<string>& masks)
{
	const size_t numMasks = masks.size();

	for (size_t i = 0; i < numMasks; ++i)
	{
		if(StringHelpers::MatchesWildcardsIgnoreCase(str, masks[i]))
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
static string GetCommonRelativePath(const CDependencyList::SFile& file)
{
	const string path1 = PathUtil::ReplaceExtension(file.inputFile, PathUtil::GetExt(file.outputFile));
	const string& path2 = file.outputFile;

	string::const_iterator first1 = path1.begin();
	string::const_iterator first2 = path2.begin();

	string::const_iterator last1 = path1.begin() + path1.length() - 1;
	string::const_iterator last2 = path2.begin() + path2.length() - 1;

	while (first1 <= last1 && first2 <= last2 && std::tolower(*last1) == std::tolower(*last2)) 
	{
		--last1;
		--last2;
	}
	++last2;

	if (*last2 == '/' || *last2 == '\\')
	{
		++last2;
	}

	return path2.substr(last2 - first2);
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::SaveAssetReferences( const std::vector<string>& references, const string& filename, const string& includeMasksStr, const string& excludeMasksStr)
{
	std::vector<string> includeMasks;
	StringHelpers::Split(includeMasksStr, ";", false, includeMasks);

	std::vector<string> excludeMasks;
	StringHelpers::Split(excludeMasksStr, ";", false, excludeMasks);

	FILE* const f = fopen(filename.c_str(), "wt");
	if (!f)
	{
		RCLogError("Unable to open %s for writing", filename.c_str());
		return;
	}

	for (std::vector<string>::const_iterator it = references.begin(); it != references.end(); ++it)
	{
		if (MatchesWildcardsSet(*it, excludeMasks))
		{
			continue;
		}

		if (!includeMasks.empty() && !MatchesWildcardsSet(*it, includeMasks))
		{
			continue;
		}
		fprintf(f, "%s\n", it->c_str());
	}
	fclose(f);
}
//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::CleanTargetFolder(bool bUseOnlyInputFiles)
{
	const IConfig* const config = &m_multiConfig.getConfig();

	{
		const string targetroot = config->GetAsString("targetroot", "", "");
		if (targetroot.empty())
		{
			return;
		}
		RCLog("Cleaning target folder %s", targetroot.c_str());
	}

	CDependencyList inputOutputFileList(m_inputOutputFileList);

	// Look at the list of processed files.
	{
		inputOutputFileList.RemoveDuplicates();

		const string filename = FormLogFileName(m_filenameOutputFileList);

		if (FileUtil::FileExists(filename.c_str()))
		{
			const size_t oldCount = inputOutputFileList.GetCount();
			inputOutputFileList.Load(filename);
			const size_t loadedCount = inputOutputFileList.GetCount() - oldCount;
			inputOutputFileList.RemoveDuplicates();
			const size_t addedCount = inputOutputFileList.GetCount() - oldCount;
			RCLog("%u entries (%u unique) found in list of processed files '%s'", (uint)loadedCount, (uint)addedCount, filename.c_str());
		}
		else
		{
			RCLog("List of processed files '%s' is not found", filename.c_str());
		}

		RCLog("%u entries in list of processed files", (uint)inputOutputFileList.GetCount());
	}										  

	// Prepare lists of files that were deleted.
	std::vector<string> deletedSourceFiles;
	std::vector<string> deletedTargetFiles;
	std::vector<string> deletedTargetFilesPak;

	if (bUseOnlyInputFiles)
	{
		for (size_t nInput = 0; nInput < m_inputFilesDeleted.size(); ++nInput)
		{
			const string& deletedInputFilename = m_inputFilesDeleted[nInput].m_sourceInnerPathAndName;
			deletedSourceFiles.push_back(deletedInputFilename);
			for (size_t i = 0; i < inputOutputFileList.GetCount(); ++i)
			{
				const CDependencyList::SFile &of = inputOutputFileList.GetElement(i);
				if (deletedInputFilename == of.inputFile)
				{
					deletedTargetFiles.push_back(of.outputFile);
					deletedTargetFilesPak.push_back(GetCommonRelativePath(of));
				}
			}
		}
	}
	else
	{
		string lastInputFile;
		bool bSrcFileExists = false;
		for (size_t i = 0; i < inputOutputFileList.GetCount(); ++i)
		{
			// Check if input file exists
			const CDependencyList::SFile &of = inputOutputFileList.GetElement(i);
			if (of.inputFile != lastInputFile)
			{
				lastInputFile = of.inputFile;
				if (FileUtil::FileExists(of.inputFile.c_str()))
				{
					bSrcFileExists = true;
				}
				else
				{
					RCLog("Source file deleted: \"%s\"", of.inputFile.c_str());
					deletedSourceFiles.push_back(of.inputFile);
					bSrcFileExists = false;
				}
			}
			if (!bSrcFileExists)
			{
				deletedTargetFiles.push_back(of.outputFile);
				deletedTargetFilesPak.push_back(GetCommonRelativePath(of));
			}
		}
	}

	std::sort(deletedSourceFiles.begin(), deletedSourceFiles.end());
	std::sort(deletedTargetFiles.begin(), deletedTargetFiles.end());
	std::sort(deletedTargetFilesPak.begin(), deletedTargetFilesPak.end());
	
	//////////////////////////////////////////////////////////////////////////
	// Remove records with matching input filenames
	inputOutputFileList.RemoveInputFiles(deletedSourceFiles);

	//////////////////////////////////////////////////////////////////////////
	// Delete target files from disk
	for (size_t i = 0; i < deletedTargetFiles.size(); ++i)
	{
		const string& filename = deletedTargetFiles[i];
		if (i <= 0 || filename != deletedTargetFiles[i - 1])
		{
			RCLog("Deleting file \"%s\"", filename.c_str());
			::DeleteFile(filename.c_str());
		}
	}

	StartProgress();

	m_pPakManager->DeleteFilesFromPaks(config, deletedTargetFilesPak);

	{
		const string filename = FormLogFileName(m_filenameOutputFileList);
		RCLog("Saving %s", filename.c_str());
		inputOutputFileList.RemoveDuplicates();
		inputOutputFileList.Save(filename.c_str());
	}

	// store deleted files list.
	{
		const string filename = FormLogFileName(m_filenameDeletedFileList);
		RCLog("Saving %s", filename.c_str());

		FILE* const f = fopen(filename.c_str(), "wt");
		if (f)
		{
			for (size_t i = 0; i < deletedTargetFiles.size(); ++i)
			{
				const string filename = CDependencyList::NormalizeFilename(deletedTargetFiles[i].c_str());
				fprintf(f, "%s\n", filename.c_str());
			}
			fclose(f);
		}
		else
		{
			RCLogError("Failed to write %s", filename.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
static ICryXML* LoadICryXML()
{
	HMODULE hXMLLibrary = LoadLibrary("CryXML.dll");
	if (NULL == hXMLLibrary)
	{
		RCLogError("Unable to load xml library (CryXML.dll)");
		return 0;
	}
	FnGetICryXML pfnGetICryXML = (FnGetICryXML)GetProcAddress(hXMLLibrary, "GetICryXML");
	if (pfnGetICryXML == 0)
	{
		RCLogError("Unable to load xml library (CryXML.dll) - cannot find exported function GetICryXML().");
		return 0;
	}
	return pfnGetICryXML();
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef ResourceCompiler::LoadXml(const char* filename)
{
	ICryXML* pCryXML = LoadICryXML();
	if (!pCryXML)
	{
		return 0;
	}

	// Get the xml serializer.
	IXMLSerializer* pSerializer = pCryXML->GetXMLSerializer();

	// Read in the input file.
	XmlNodeRef root;
	{
		const bool bRemoveNonessentialSpacesFromContent = false;
		char szErrorBuffer[1024];

		wstring widePath;
		Unicode::Convert(widePath, filename);

		root = pSerializer->Read(FileXmlBufferSource(widePath), bRemoveNonessentialSpacesFromContent, sizeof(szErrorBuffer), szErrorBuffer);
		if (!root)
		{
			RCLogError("Failed to load XML file '%s': %s", filename, szErrorBuffer);
			return 0;
		}
	}
	return root;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef ResourceCompiler::CreateXml(const char* tag)
{
	ICryXML* pCryXML = LoadICryXML();
	if (!pCryXML)
	{
		return 0;
	}

	// Get the xml serializer.
	IXMLSerializer* pSerializer = pCryXML->GetXMLSerializer();

	XmlNodeRef root;
	{
		root = pSerializer->CreateNode(tag);
		if (!root)
		{
			RCLogError("Cannot create new XML node '%s'\n", tag);
			return 0;
		}
	}
	return root;
}

//////////////////////////////////////////////////////////////////////////
int ResourceCompiler::ProcessJobFile()
{
	const MultiplatformConfig savedConfig = m_multiConfig;
	const IConfig* const config = &m_multiConfig.getConfig();

	// Job file is an XML with multiple jobs for the RC
	const string jobFile = config->GetAsString("job", "", "");
	if (jobFile.empty())
	{
		RCLogError( "No job file specified" );
		m_multiConfig = savedConfig;
		return eRcExitCode_Error;
	}

	const string runJob = config->GetAsString("jobtarget", "", "");
	const bool runJobFromCommandLine = !runJob.empty();

	CPropertyVars properties(this);

	properties.SetProperty("_rc_exe_folder", GetExePath());
	properties.SetProperty("_rc_tmp_folder", GetTmpPath());

	if (GetVerbosityLevel() >= 0)
	{
		RCLog("Pre-defined job properties:");
		properties.PrintProperties();
	}
	
	config->CopyToPropertyVars(properties);

	XmlNodeRef root = LoadXml( jobFile.c_str() );
	if (!root)
	{
		RCLogError( "Failed to load job XML file %s",jobFile.c_str() );
		m_multiConfig = savedConfig;
		return eRcExitCode_Error;
	}

	// Check command line with respect to known DefaultProperties
	{
		std::vector<string> defaultProperties;

		for (int i = 0; i < root->getChildCount(); ++i)
		{
			ExtractJobDefaultProperties(defaultProperties, root->getChild(i));
		}

		if (!CheckCommandLineOptions(*config, &defaultProperties))
		{
			return eRcExitCode_Error;
		}
	}

	int result = eRcExitCode_Success;

	for (int i = 0; i < root->getChildCount(); ++i)
	{
		XmlNodeRef jobNode = root->getChild(i);
		const bool runNodes = !runJobFromCommandLine;
		const int tmpResult = EvaluateJobXmlNode( properties,jobNode,runNodes );
		if (result == eRcExitCode_Success)
		{
			result = tmpResult;
		}
	}

	if (runJobFromCommandLine)
	{
		const int tmpResult = RunJobByName( properties,root,runJob );
		if (result == eRcExitCode_Success)
		{
			result = tmpResult;
		}
	}

	m_multiConfig = savedConfig;
	return result;
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::ExtractJobDefaultProperties( std::vector<string> &properties, XmlNodeRef &jobNode )
{
	if (jobNode->isTag("DefaultProperties"))
	{
		// Attributes are config modifiers.
		for (int attr = 0; attr < jobNode->getNumAttributes(); ++attr)
		{
			const char *key = "";
			const char *value = "";
			jobNode->getAttributeByIndex(attr,&key,&value);
			if (std::find(properties.begin(), properties.end(), key) == properties.end())
			{
				properties.push_back(StringHelpers::MakeLowerCase(key));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int ResourceCompiler::EvaluateJobXmlNode( CPropertyVars &properties,XmlNodeRef &jobNode, bool runJobs )
{
	IConfig* const config = &m_multiConfig.getConfig();

	if (jobNode->isTag("Properties"))
	{
		// Attributes are config modifiers.
		for (int attr = 0; attr < jobNode->getNumAttributes(); attr++)
		{
			const char *key = "";
			const char *value = "";
			jobNode->getAttributeByIndex(attr,&key,&value);
			string strValue = value;
			properties.ExpandProperties(strValue);
			properties.SetProperty( key,strValue );
		}
		return eRcExitCode_Success;
	}
	
	// Default properties don't overwrite existing ones (e.g. specified from command line)
	if (jobNode->isTag("DefaultProperties"))
	{
		// Attributes are config modifiers.
		for (int attr = 0; attr < jobNode->getNumAttributes(); attr++)
		{
			const char *key = "";
			const char *value = "";
			jobNode->getAttributeByIndex(attr,&key,&value);
			if(!properties.HasProperty(key))
			{
				string strValue = value;
				properties.ExpandProperties(strValue);
				properties.SetProperty( key,strValue );
			}
		}
		return eRcExitCode_Success;
	}

	if (jobNode->isTag("Run"))
	{
		if (!runJobs)
			return eRcExitCode_Success;

		const char *jobListName = jobNode->getAttr("Job");
		if (strlen(jobListName) == 0)
			return eRcExitCode_Success;

		// Attributes are config modifiers.
		const CPropertyVars previousProperties = properties;
		for (int attr = 0; attr < jobNode->getNumAttributes(); attr++)
		{
			const char *key = "";
			const char *value = "";
			jobNode->getAttributeByIndex(attr,&key,&value);
			string strValue = value;
			properties.ExpandProperties(strValue);
			properties.SetProperty( key,strValue );
		}
		
		int result = RunJobByName( properties,jobNode,jobListName );
		properties = previousProperties;
		return result;
	}

	if (jobNode->isTag("Include"))
	{
		const char *includeFile = jobNode->getAttr("file");
		if (strlen(includeFile) == 0)
			return eRcExitCode_Success;
		
		const string jobFile = config->GetAsString("job", "", "");
		const string includePath = PathUtil::AddSlash(PathHelpers::GetDirectory(jobFile)) + includeFile;

		XmlNodeRef root = LoadXml(includePath);
		if (!root)
		{
			RCLogError("Failed to load included job XML file '%s'", includePath.c_str());
			return eRcExitCode_Error;
		}

		// Add include sub-nodes
		XmlNodeRef parent = jobNode->getParent();
		while (root->getChildCount() != 0)
		{
			XmlNodeRef subJobNode = root->getChild(0);
			root->removeChild(subJobNode);
			parent->addChild(subJobNode);
		}
		return eRcExitCode_Success;
	}

	// Condition node.
	if (jobNode->isTag("if") || jobNode->isTag("ifnot"))
	{
		bool bIf = false;
		for (int attr = 0; attr < jobNode->getNumAttributes(); attr++)
		{
			const char *key = "";
			const char *value = "";
			jobNode->getAttributeByIndex(attr,&key,&value);

			string propValue;
			properties.GetProperty(key,propValue);
			if (stricmp(value,propValue.c_str()) == 0)
			{
				// match.
				bIf = true;
			}
		}
		if (jobNode->isTag("ifnot"))
		{
			bIf = !bIf; // Invert
		}
		int result = eRcExitCode_Success;
		if (bIf)
		{
			// Exec sub-nodes
			for (int i = 0; i < jobNode->getChildCount(); i++)
			{
				XmlNodeRef subJobNode = jobNode->getChild(i);
				const int tmpResult = EvaluateJobXmlNode( properties,subJobNode,true );
				if (result == eRcExitCode_Success)
				{
					result = tmpResult;
				}
			}
		}
		return result;
	}

	if (jobNode->isTag("Job"))
	{
		RCLog( "-------------------------------------------------------------------" );
		string jobLog = "Job: ";
		// Delete all config entries from previous job.
		config->ClearPriorityUsage(eCP_PriorityJob);

		bool bCleanTargetRoot = false;
		string refsSaveFilename;
		string refsSaveInclude;
		string refsSaveExclude;
		string refsRoot;

		// Attributes are config modifiers.
		for (int attr = 0; attr < jobNode->getNumAttributes(); attr++)
		{
			const char *key = "";
			const char *value = "";
			jobNode->getAttributeByIndex(attr,&key,&value);
			
			string valueStr = value;
			properties.ExpandProperties(valueStr);

			if (stricmp(key,"input") == 0)
			{
				jobLog += string("/") + key + "=" + valueStr + " ";
				continue;
			}
			else if (stricmp(key, "clean_targetroot") == 0)
			{
				bCleanTargetRoot = true;
				continue;
			}
			else if (stricmp(key, "refs_scan") == 0)
			{
				RCLogError("refs_scan is not supported anymore");
				return eRcExitCode_Error;
			}
			else if (stricmp(key, "refs_save") == 0)
			{
				if (valueStr.empty())
				{
					RCLogError("Missing filename in refs_save option");
					return eRcExitCode_Error;
				}
				refsSaveFilename = valueStr;
				continue;
			}
			else if (stricmp(key, "refs_root") == 0)
			{
				refsRoot = valueStr;
				continue;
			}
			else if(stricmp(key, "refs_save_include") == 0)
			{
				refsSaveInclude = valueStr;
				continue;
			}
			else if(stricmp(key, "refs_save_exclude") == 0)
			{
				refsSaveExclude = valueStr;
				continue;
			}

			config->SetKeyValue(eCP_PriorityJob, key, valueStr);
			jobLog += string("/") + key + "=" + valueStr + " (attribute) ";
		}

		// Apply properties from RCJob to config
		properties.Enumerate([config, &jobLog](const string &propName, const string &propVal)
		{
			if (config->HasKeyRegistered(propName) && !StringHelpers::EqualsIgnoreCase(propName, "job"))
			{
				config->SetKeyValue(eCP_PriorityProperty, propName, propVal);
				jobLog += string("/") + propName + "=" + propVal + " (property) ";
			}
		});

		// Check current platform property against start-up platform setting
		// Reason: This setting cannot be modified after start-up
		const char *pCurrentPlatform = NULL;
		if (config->GetKeyValue("p", pCurrentPlatform) && pCurrentPlatform)
		{
			int currentPlatformIndex = FindPlatform(pCurrentPlatform);
			if (GetMultiplatformConfig().getActivePlatform() != currentPlatformIndex)
			{
				RCLogWarning("The platform property '/p=%s' is ignored because it can only be specified on the command-line", pCurrentPlatform);
			}
		}

		string fileSpec = jobNode->getAttr( "input" );
		properties.ExpandProperties(fileSpec);
		if (!fileSpec.empty())
		{
			RCLog(jobLog);
			RemoveOutputFiles();
			std::vector<RcFile> files;
			if (CollectFilesToCompile(fileSpec, files) && !files.empty())
			{
				SetupMaxThreads();
				CompileFilesBySingleProcess(files);
			}
		}
		else
		{
			if (!refsSaveFilename.empty())
			{
				properties.ExpandProperties(refsSaveFilename);
				// cppcheck-suppress oppositeInnerCondition
				if (refsSaveFilename.empty())
				{
					RCLogError("Empty filename specified in refs_save option");
					return eRcExitCode_Error;
				}

				properties.ExpandProperties(refsRoot);

				std::vector<string> references;
				ScanForAssetReferences(references, refsRoot);
				SaveAssetReferences(references, refsSaveFilename, refsSaveInclude, refsSaveExclude);				
			}
			if (bCleanTargetRoot)
			{
				CleanTargetFolder(false);
			}
		}
		// Delete all config entries from our job.
		config->ClearPriorityUsage(eCP_PriorityJob | eCP_PriorityProperty);
		return eRcExitCode_Success;
	}

	return eRcExitCode_Success;
}

//////////////////////////////////////////////////////////////////////////
int ResourceCompiler::RunJobByName( CPropertyVars& properties,XmlNodeRef &anyNode,const char* name )
{
	// Find root
	XmlNodeRef root = anyNode;
	while (root->getParent())
	{
		root = root->getParent();
	}

	// Find JobList.
	XmlNodeRef jobListNode = root->findChild( name );
	if (!jobListNode)
	{
		RCLogError( "Unable to find job \"%s\"",name );
		return eRcExitCode_Error;
	}
	{
		// Execute Job sub nodes.
		int result = eRcExitCode_Success;
		for (int i = 0; i < jobListNode->getChildCount(); i++)
		{
			XmlNodeRef subJobNode = jobListNode->getChild(i);
			const int tmpResult = EvaluateJobXmlNode( properties,subJobNode,true );
			if (result == eRcExitCode_Success)
			{
				result = tmpResult;
			}
		}
		return result;
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceCompiler::LogMemoryUsage(bool bReportProblemsOnly)
{
	PROCESS_MEMORY_COUNTERS p;
	ZeroMemory(&p, sizeof(p));
	p.cb = sizeof(p);

	if (!GetProcessMemoryInfo(GetCurrentProcess(), &p, sizeof(p)))
	{
		RCLogError("Cannot obtain memory info");
		return;
	}

	static const float megabyte = 1024 * 1024;
	const float peakSizeMb = p.PeakWorkingSetSize / megabyte;
#if CRY_PLATFORM_64BIT
	static const float warningMemorySizePeakMb =  7500.0f;
	static const float errorMemorySizePeakMb   = 15500.0f;
#else
	static const float warningMemorySizePeakMb =  3100.0f;
	static const float errorMemorySizePeakMb   =  3600.0f;
#endif

	bool bReportProblem = false;
	{
		ThreadUtils::AutoLock lock(m_memorySizeLock);
		if (peakSizeMb > m_memorySizePeakMb)
		{
			m_memorySizePeakMb = peakSizeMb;
			bReportProblem = (peakSizeMb >= warningMemorySizePeakMb);
		}
	}

	if (bReportProblem || !bReportProblemsOnly)
	{
		if (peakSizeMb >= warningMemorySizePeakMb)
		{
			((peakSizeMb >= errorMemorySizePeakMb) ? RCLogError : RCLogWarning)(
				"Memory: working set %.1fMb (peak %.1fMb - DANGER!), pagefile %.1fMb (peak %.1fMb)",
				p.WorkingSetSize / megabyte, p.PeakWorkingSetSize / megabyte,
				p.PagefileUsage / megabyte, p.PeakPagefileUsage / megabyte);
		}
		else
		{
			RCLog(
				"Memory: working set %.1fMb (peak %.1fMb), pagefile %.1fMb (peak %.1fMb)",
				p.WorkingSetSize / megabyte, p.PeakWorkingSetSize / megabyte,
				p.PagefileUsage / megabyte, p.PeakPagefileUsage / megabyte);
		}
	}
}
