// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef LUA_COMPILER
#define LUA_COMPILER

#include "SingleThreadedConverter.h"

struct ConvertContext;

class LuaCompiler : public CSingleThreadedCompiler
{
public:
	LuaCompiler();
	~LuaCompiler();

	// IConverter methods.
	virtual const char* GetExt(int index) const { return (index == 0) ? "lua" : 0; }

	// ICompiler methods.
	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }
	virtual bool Process();

public:
	bool IsDumping() const { return m_bIsDumping; }
	bool IsStripping() const { return m_bIsStripping; }
	bool IsBigEndian() const { return m_bIsBigEndian; }
	const char* GetInFilename() const { return m_sInFilename.c_str(); }
	const char* GetOutFilename() const { return m_sOutFilename.c_str(); }

private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;

private:
	bool m_bIsDumping;			/* dump bytecodes? */
	bool m_bIsStripping;			/* strip debug information? */
	bool m_bIsBigEndian;
	string m_sInFilename;
	string m_sOutFilename;
};

#endif
