// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ChunkCompiler_h__
#define __ChunkCompiler_h__

#include "SingleThreadedConverter.h"

struct ConvertContext;

class CChunkCompiler : public CSingleThreadedCompiler
{
public:
	class Error
	{
	public:
		explicit Error(int nCode);
		Error(const char* szFormat, ...);
		const char* c_str() const 
		{ 
			return m_strReason.c_str(); 
		}
	protected:
		string m_strReason;
	};

	CChunkCompiler();
	~CChunkCompiler();

	// ICompiler methods.
	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }
	virtual bool Process();

	// IConverter methods.
	virtual const char* GetExt(int index) const;

private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;
};

#endif