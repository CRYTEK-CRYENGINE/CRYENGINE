// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   resourcecompiler.h
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __resourcecompiler_h__
#define __resourcecompiler_h__
#pragma once

#include "IRCLog.h"
#include "IResCompiler.h"
#include "CfgFile.h"
#include "Config.h"
#include "DependencyList.h"
#include "ExtensionManager.h"
#include "MultiplatformConfig.h"
#include "PakSystem.h"
#include "PakManager.h"
#include "RcFile.h"
#include "ThreadUtils.h"
#include <CrySystem/CryVersion.h>
#include "IProgress.h"

#include <map>                 // stl multimap<>
#include <mutex>

class CPropertyVars;
class XmlNodeRef;
class CDigest;
class FilesToConvert;
struct IAssetManager;

/** Implementation of IResCompiler interface. */
class ResourceCompiler :
	public IResourceCompiler,
	public IProgress,
	public IRCLog,
	public IConfigKeyRegistry
{
public:
	ResourceCompiler();
	virtual ~ResourceCompiler();

	//! e.g. print statistics
	void PostBuild();

	int GetNumWarnings() const {return m_numWarnings;}
	int GetNumErrors() const {return m_numErrors;}

	// interface IProgress --------------------------------------------------

	virtual void StartProgress() override;
	virtual void ShowProgress(const char* pMessage, size_t progressValue, size_t maxProgressValue) override;
	virtual void FinishProgress() override;

	// interface IConfigKeyRegistry ------------------------------------------

	virtual void VerifyKeyRegistration( const char *szKey ) const override;
	virtual bool HasKeyRegistered( const char *szKey ) const override;
	// -----------------------------------------------------------------------

	// interface IResourceCompiler -------------------------------------------

	virtual void RegisterKey( const char *key, const char *helptxt ) override;
	
	virtual const char* GetExePath() const override;

	//! Return path to RC's 'temp' directory, where it places processed files before packaging.
	//! The removal of this path results in a clean build, which can take some time.
	virtual const char* GetTmpPath() const override;
	virtual const char* GetInitialCurrentDir() const override;

	virtual void RegisterConverter(const char* name, IConverter* conv) override;

	virtual IPakSystem* GetPakSystem() override;

	virtual const ICfgFile* GetIniFile() const override;

	virtual int GetPlatformCount() const override;
	virtual const PlatformInfo* GetPlatformInfo(int index) const override;
	virtual int FindPlatform(const char* name) const override;

	virtual XmlNodeRef LoadXml(const char* filename) override;
	virtual XmlNodeRef CreateXml(const char* tag) override;

	virtual void AddInputOutputFilePair(const char* inputFilename, const char* outputFilename) override;
	virtual void MarkOutputFileForRemoval(const char* sOutputFilename) override;

	virtual void AddExitObserver( IExitObserver* p ) override;
	virtual void RemoveExitObserver( IExitObserver* p ) override;

	virtual IAssetManager* GetAssetManager() const override;

	virtual IRCLog* GetIRCLog() override
	{
		return this;
	}

	virtual int GetVerbosityLevel() const override
	{
		return m_verbosityLevel;
	}

	virtual const SFileVersion& GetFileVersion() const override
	{ 
		return m_fileVersion; 
	}

	virtual const void GetGenericInfo(char* buffer, size_t bufferSize, const char* rowSeparator) const override;

	virtual bool CompileSingleFileBySingleProcess(const char* filename) override;
	// -----------------------------------------------------------------------

	// interface IRCLog ------------------------------------------------------

	virtual void LogV(const IRCLog::EType eType, const char* szFormat, va_list args) override;
	// -----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// Resource compiler implementation.
	//////////////////////////////////////////////////////////////////////////	

	void Init(Config& config);
	bool LoadIniFile();
	void UnregisterConverters();

	bool AddPlatform(const string& names, bool bBigEndian, int pointerSize);

	MultiplatformConfig& GetMultiplatformConfig();

	void SetupMaxThreads();
	int  GetMaxThreads() const;

	bool CollectFilesToCompile(const string& filespec, std::vector<RcFile>& files);
	bool CompileFilesBySingleProcess(const std::vector<RcFile>& files);
	virtual bool CompileSingleFileNested(const string& fileSpec, const std::vector<string>& args) override;
	int  ProcessJobFile();
	void RemoveOutputFiles();	  // to remove old files for less confusion
	void CleanTargetFolder(bool bUseOnlyInputFiles);

	bool CompileFile(const char* sourceFullFileName, const char* targetLeftPath, const char* sourceInnerPath, ICompiler* compiler);

	const char* GetLogPrefix() const;

	//! call this if user asks for help
	void ShowHelp(bool bDetailed);
	//////////////////////////////////////////////////////////////////////////

	void QueryVersionInfo();

	void InitPaths();

	void NotifyExitObservers();

	void InitLogs(Config& config);
	string FormLogFileName(const char* suffix) const;
	const string& GetMainLogFileName() const;
	const string& GetErrorLogFileName() const;

	clock_t GetStartTime() const;
	bool GetTimeLogging() const;
	void SetTimeLogging(bool enable);

	void LogMemoryUsage(bool bReportProblemsOnly);

	void InitPakManager();

	static string FindSuitableSourceRoot(const std::vector<string>& sourceRootsReversed, const string& fileName);
	static void GetSourceRootsReversed(const IConfig* config, std::vector<string>& sourceRootsReversed);

	bool RegisterConverters();

private:	
	typedef void (* CompileFilesFunction)(
		ResourceCompiler* pRC,
		unsigned long a_tlsIndex_pThreadData,
		FilesToConvert& a_files,
		int threadCount,
		IConverter* converter,
		const IConfig* config);

	bool CompileFiles(const std::vector<RcFile>& files, const IConfig* pConfig, CompileFilesFunction compileFiles);

	void FilterExcludedFiles( std::vector<RcFile> &files );

	void CopyFiles( const std::vector<RcFile> &files );

	void ExtractJobDefaultProperties(std::vector<string>& properties, XmlNodeRef& jobNode);
	int  EvaluateJobXmlNode(CPropertyVars& properties, XmlNodeRef& jobNode, bool runJobs);
	int  RunJobByName(CPropertyVars& properties, XmlNodeRef& anyNode, const char* name);

	void ScanForAssetReferences( std::vector<string>& outReferences, const string& refsRoot );
	void SaveAssetReferences( const std::vector<string>& references, const string& filename, const string& includeMasks, const string& excludeMasks );

	void InitializeThreadIds();

	void LogLine(const IRCLog::EType eType, const char* szText);

	// to log multiple lines (\n separated) with padding before
	void LogMultiLine(const char* szText);

	void InitFileCache(Config& config);
	void TryToGetFilesFromCache(FilesToConvert& files, IConverter* converter);
	void AddFilesToTheCache(const FilesToConvert& files);

	// -----------------------------------------------------------------------
public:
	static const char* const m_filenameRcExe;
	static const char* const m_filenameRcIni;
	static const char* const m_filenameOptions;
	static const char* const m_filenameLog;             
	static const char* const m_filenameLogWarnings;
	static const char* const m_filenameLogErrors;
	static const char* const m_filenameCrashDump;
	static const char* const m_filenameOutputFileList;  //!< list of source=target filenames that rc processed, used for cleaning target folder
	static const char* const m_filenameDeletedFileList;
	static const char* const m_filenameCreatedFileList;

	bool m_bQuiet;                                      //!< true= don't log anything to console

private:
	enum { kMaxPlatformCount = 20 };
	int m_platformCount;
	PlatformInfo m_platforms[kMaxPlatformCount];

	ExtensionManager        m_extensionManager;

	std::recursive_mutex m_inputOutputFilesMutex;
	CDependencyList              m_inputOutputFileList;

	std::vector<IExitObserver*>  m_exitObservers;
	std::recursive_mutex m_exitObserversMutex;

	float                   m_memorySizePeakMb;
	std::recursive_mutex              m_memorySizeMutex;

	// log files
	string                       m_logPrefix;
	string                       m_mainLogFileName;      //!< for all messages, might be empty (no file logging)
	string                       m_warningLogFileName;   //!< for warnings only, might be empty (no file logging)
	string                       m_errorLogFileName;     //!< for errors only, might be empty (no file logging)
	string                       m_logHeaderLine;
	std::recursive_mutex m_logMutex;

	clock_t                 m_startTime;
	bool                    m_bTimeLogging;

	float                   m_progressLastPercent;
	int                     m_verbosityLevel;

	CfgFile                 m_iniFile;
	MultiplatformConfig     m_multiConfig;

	bool                    m_bWarningHeaderLine;   //!< true= header was already printed, false= header needs to be printed
	bool                    m_bErrorHeaderLine;     //!< true= header was already printed, false= header needs to be printed
	bool                    m_bWarningsAsErrors;    //!< true= treat any warning as error.

	int                     m_systemCpuCoreCount;
	int                     m_processCpuCoreCount;
	int                     m_systemLogicalProcessorCount;
	int                     m_processLogicalProcessorCount;
	int                     m_maxThreads;           //!< max number of threads (set by /threads command-line option)

	SFileVersion            m_fileVersion;
	string                  m_exePath;              //!< Absolute path to the RC executable.
	string                  m_tempPath;				//!< Absolute path to the intermediate directory, where processed files are stored before being packaged.
	string                  m_initialCurrentDir;

	std::map<string,string> m_KeyHelp;              // [lower key] = help, created from RegisterKey
	
	// Files to delete
	std::vector<RcFile>     m_inputFilesDeleted;

	PakManager*             m_pPakManager;

	string                  m_cacheFolder;
	std::unique_ptr<CDigest> m_pDigest;
	std::unique_ptr<IAssetManager> m_pAssetManager;

	int                     m_numWarnings;
	int                     m_numErrors;

	unsigned long           m_tlsIndex_pThreadData;
};

#endif // __resourcecompiler_h__
