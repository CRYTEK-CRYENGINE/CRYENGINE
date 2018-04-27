// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ZipEncryptor_h__
#define __ZipEncryptor_h__
#pragma once

#include "IConverter.h"

struct ConvertContext;

class ZipEncryptor 
	: public ICompiler
	, public IConverter
{
public:
	explicit ZipEncryptor(IResourceCompiler* pRC);
	~ZipEncryptor();

	static bool ParseKey(uint32 outputKey[4], const char* inputString);

	// ICompiler + IConverter methods.
	virtual void Release();

	// ICompiler methods.
	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }
	virtual IConvertContext* GetConvertContext() { return &m_CC; }
	virtual bool Process();

	// IConverter methods.
	virtual ICompiler* CreateCompiler();
	virtual bool SupportsMultithreading() const { return false; }
	virtual const char* GetExt(int index) const;

private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;

private:
	ConvertContext m_CC;
};

#endif
