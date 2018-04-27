// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   converter.h
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __converter_h__
#define __converter_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "ConvertContext.h"


// A Converter does no actual work, it merely describes the work that a Compiler will do,
// and can create Compiler instances to do the actual processing.
//
// For converters that support multithreading (see SupportsMultithreading()) it is possible
// to create *more than one* compiler and run them in multiple threads.

// Compiler interface, all compilers must implement this interface.
struct ICompiler
{
	virtual ~ICompiler() {}

	// Release memory of interface.
	virtual void Release() = 0;

	// This function is called by RC before starting processing files.
	virtual void BeginProcessing(const IConfig* config) = 0;

	// This function is called by RC after finishing processing files.
	virtual void EndProcessing() = 0;

	// Return a convert context object.
	// RC will fill with compilation parameters right before calling Process().
	virtual IConvertContext* GetConvertContext() = 0;

	// Process a file.
	//
	// The file and processing parameters are provided by RC
	// by calling appropriate functions of a convert context
	// object returned by GetConvertContext().
	// 
	// Returns true if succeeded.
	virtual bool Process() = 0;
};

class RcFile;
struct ConverterInitContext
{
	const IConfig* config;
	size_t inputFileCount;
	const RcFile* inputFiles;
};

// Converter interface, all converters must implement this interface.
struct IConverter
{
	virtual ~IConverter() {}

	// Release memory of interface.
	virtual void Release() = 0;

	virtual void Init(const ConverterInitContext& context) {}
	virtual void DeInit() {}

	// Return an object that will do actual processing.
	// Called only once if SupportsMultithreading() returns false.
	// Otherwise can be called multiple times and run from separate threads.
	virtual ICompiler* CreateCompiler() = 0;

	// Check whether the converter supports multithreading.
	// See CreateCompiler() comments for details.
	virtual bool SupportsMultithreading() const = 0;

	// Get supported extension by zero-based index.
	// If index is < 0  or >= number of supported extensions,
	// then the function *must* return 0.
	virtual const char* GetExt(int index) const = 0;

	// if returns true RC may try to reuse converted data.
	virtual bool IsCacheable() const { return false; }
};

#endif // __converter_h__
