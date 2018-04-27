// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   convertcontext.h
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __convertcontext_h__
#define __convertcontext_h__
#pragma once

#include "PathHelpers.h"
#include "string.h"
#include "IMultiplatformConfig.h"
#include "IResCompiler.h"
#include <CryString/CryPath.h>

class IConfig;

// IConvertContext is a description of what and how should be processed by compiler
struct IConvertContext
{
	virtual void SetConverterExtension(const char* converterExtension) = 0;

	virtual void SetSourceFolder(const char* sourceFolder) = 0;
	virtual void SetSourceFileNameOnly(const char* sourceFileNameOnly) = 0;
	virtual void SetOutputFolder(const char* sOutputFolder) = 0;

	virtual void SetRC(IResourceCompiler* pRC) = 0;
	virtual void SetMultiplatformConfig(IMultiplatformConfig* pMultiConfig) = 0;
	virtual void SetThreads(int threads) = 0;
	virtual void SetForceRecompiling(bool bForceRecompiling) = 0;
};

struct ConvertContext
	: public IConvertContext
{
	//////////////////////////////////////////////////////////////////////////
	// Interface IConvertContext

	virtual void SetConverterExtension(const char* converterExtension)
	{
		this->converterExtension = converterExtension;
	}

	virtual void SetSourceFolder(const char* sourceFolder)
	{
		this->sourceFolder = sourceFolder;
	}
	virtual void SetSourceFileNameOnly(const char* sourceFileNameOnly)
	{
		this->sourceFileNameOnly = sourceFileNameOnly;
	}
	virtual void SetOutputFolder(const char* sOutputFolder)
	{
		this->outputFolder = sOutputFolder;
	}

	virtual void SetRC(IResourceCompiler* pRC)
	{
		this->pRC = pRC;
	}
	virtual void SetMultiplatformConfig(IMultiplatformConfig* pMultiConfig)
	{
		this->multiConfig = pMultiConfig;
		this->config = &pMultiConfig->getConfig();
		this->platform = pMultiConfig->getActivePlatform();
	}
	virtual void SetThreads(int threads)
	{
		this->threads = threads;
	}
	virtual void SetForceRecompiling(bool bForceRecompiling)
	{
		this->bForceRecompiling = bForceRecompiling;
	}
	//////////////////////////////////////////////////////////////////////////

	ConvertContext()
		: pRC(0)
		, multiConfig(0)
		, platform(-1)
		, config(0)
		, threads(0)
		, bForceRecompiling(false)
	{
	}

	const string GetSourcePath() const
	{
		return PathUtil::Make(sourceFolder, sourceFileNameOnly);
	}

	const string& GetOutputFolder() const
	{
		return outputFolder;
	}

public:
	// Converter will assume that the source file has content matching this extension
	// (the sourceFileNameOnly can have a different extension, say 'tmp').
	string converterExtension;

	// Source file's folder.
	string sourceFolder;
	// Source file that needs to be converted, for example "test.tif".
	// Contains filename only, the folder is stored in sourceFolder.
	string sourceFileNameOnly;

	// Pointer to resource compiler interface.
	IResourceCompiler* pRC;

	// Configuration settings.
	IMultiplatformConfig* multiConfig;
	// Platform to which file must be processed.
	int platform;
	// Platform's config.
	const IConfig* config;

	// Number of threads.
	int threads;    

	// true if compiler is requested to skip up-to-date checks
	bool bForceRecompiling;

private:
	// Output folder.
	string outputFolder;
};

#endif // __convertcontext_h__
