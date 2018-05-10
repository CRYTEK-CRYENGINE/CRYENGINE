// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   irescompiler.h
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: IResourceCompiler interface.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __irescompiler_h__
#define __irescompiler_h__
#pragma once

struct ConvertContext;
struct CryChunkedFile;
class  ICfgFile;
struct ICompiler;
class  IConfig;
struct IConverter;
struct IPakSystem;
struct IPhysicalWorld;
struct IRCLog;
struct IAssetManager;
class  MultiplatformConfig;
struct SFileVersion;
class XmlNodeRef;

struct PlatformInfo
{
	enum { kMaxNameLength = 15 };
	enum { kMaxPlatformNames = 3 };
	int index;  // unique, starts from 0, increased by one for next platform, persistent for a session
	bool bBigEndian;
	int pointerSize;
	char platformNames[kMaxPlatformNames][kMaxNameLength + 1];  // every name is guaranteed to be zero-terminated, [0] is guaranteed to be non-empty

public:
	PlatformInfo()
		: bBigEndian(false)
	{
		Clear();
	}

	void Clear()
	{
		index = -1;
		pointerSize = 0;
		for (int i = 0; i < kMaxPlatformNames; ++i)
		{
			platformNames[i][0] = 0;
		}
	}

	bool HasName(const char* const pName) const
	{
		for (int i = 0; i < kMaxPlatformNames && platformNames[i][0] != 0; ++i)
		{
			if (stricmp(pName, &platformNames[i][0]) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool SetName(int idx, const char* const pName)
	{
		if (idx < 0 || idx >= kMaxPlatformNames ||
			!pName || !pName[0])
		{
			return false;
		}
		const size_t len = strlen(pName);
		if (len + 1 > sizeof(platformNames[0]))
		{
			return false;
		}
		memcpy(platformNames[idx], pName, len + 1);
		return true;
	}

	const char* GetMainName() const
	{
		return &platformNames[0][0];
	}

	const string GetCommaSeparatedNames() const
	{
		string names;
		for (int i = 0; i < PlatformInfo::kMaxPlatformNames && platformNames[i][0] != 0; ++i)
		{
			if (!names.empty())
			{
				names += ",";
			}
			names += &platformNames[i][0];
		}
		return names;
	}
};


/** Main interface of resource compiler.
*/
struct IResourceCompiler
{
	class IExitObserver
	{
	public:
		virtual ~IExitObserver()
		{
		}

		virtual void OnExit() = 0;
	};

	//! Register new converter.
	virtual void RegisterConverter(const char* name, IConverter* conv) = 0;

	// Get the interface for opening files - handles files stored in ZIP archives.
	virtual IPakSystem* GetPakSystem() = 0;

	virtual const ICfgFile* GetIniFile() const = 0;

	virtual int GetPlatformCount() const = 0;
	virtual const PlatformInfo* GetPlatformInfo(int index) const = 0;
	// Returns index of the platform (or -1 if platform not found)
	virtual int FindPlatform(const char* name) const = 0;

	// One input file can generate multiple output files.
	virtual void AddInputOutputFilePair(const char* inputFilename, const char* outputFilename) = 0;

	// Mark file for removal in clean stage.
	virtual void MarkOutputFileForRemoval(const char* sOutputFilename) = 0;

	// Add pointer to an observer object which will be notified in case of 'unexpected' exit()
	virtual void AddExitObserver(IExitObserver* p) = 0;

	// Remove an observer object which was added previously by AddExitObserver() call
	virtual void RemoveExitObserver(IExitObserver* p) = 0;

	virtual IRCLog* GetIRCLog() = 0;
	virtual int GetVerbosityLevel() const = 0;

	virtual const SFileVersion& GetFileVersion() const = 0;

	virtual const void GetGenericInfo(char* buffer, size_t bufferSize, const char* rowSeparator) const = 0;

	// Arguments:
	//   key - must not be 0
	//   helptxt - must not be 0
	virtual void RegisterKey(const char* key, const char* helptxt) = 0;

	// returns the path of the resource compiler executable's directory (ending with backslash)
	virtual const char* GetExePath() const = 0;

	// returns the path of a directory for temporary files (ending with backslash)
	virtual const char* GetTmpPath() const = 0;

	// returns directory which was current at the moment of RC call (ending with backslash)
	virtual const char* GetInitialCurrentDir() const = 0;

	// returns an xmlnode for the given xml file or null node if the file could not be parsed
	virtual XmlNodeRef LoadXml(const char* filename) = 0;

	// returns an xmlnode with the given tag name
	virtual XmlNodeRef CreateXml(const char* tag) = 0;

	virtual bool CompileSingleFileBySingleProcess(const char* filename) = 0;

	// As compiles single file. args is a list of command-line arguments to overwrite the current configuration.
	// This function is intended to be called by compilers to call the RC recursively.
	// No additional thread is spawned.
	virtual bool CompileSingleFileNested(const string& fileSpec, const std::vector<string>& args) = 0;

	// returns pointer to IAssetManager interface. There is only one instance. Callers should not delete it.
	virtual IAssetManager* GetAssetManager() const = 0;
};


// this is the plugin function that's exported by plugins
// Registers all converters residing in this DLL
extern "C" 
{
	typedef void  (__stdcall* FnRegisterConverters )(IResourceCompiler*pRC);
}

#endif // __irescompiler_h__
