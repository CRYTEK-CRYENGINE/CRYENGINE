// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConvertContext.h"
#include "iconfig.h"
#include "FileUtil.h"
#include "UpToDateFileHelpers.h"

#include "LuaCompiler.h"

extern "C"
{
#include <Lua/src/lua.h>
#include <Lua/src/lauxlib.h>

#include <Lua/src/ldo.h>
#include <Lua/src/lfunc.h>
#include <Lua/src/lmem.h>
#include <Lua/src/lobject.h>
#include <Lua/src/lopcodes.h>
#include <Lua/src/lstring.h>
#include <Lua/src/lundump.h>
}

extern "C"
{
	float script_frand0_1()
	{
		return rand() / static_cast<float>(RAND_MAX);
	}

	void script_randseed(uint seed)
	{
		srand(seed);
	}
}

// Shamelessly stolen from luac.c and modified a bit to support rc usage

#define toproto(L,i) (clvalue(L->top+(i))->l.p)

static const Proto* combine(lua_State* L)
{
	return toproto(L,-1);
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
	UNUSED(L);
	return (fwrite(p,size,1,(FILE*)u)!=1) && (size!=0);
}

static int pmain(lua_State* L)
{
	LuaCompiler* pCompiler = reinterpret_cast<LuaCompiler*>(lua_touserdata(L, 1));
	const Proto* f;
	const char* filename = pCompiler->GetInFilename();
	if (luaL_loadfile(L,filename)!=0)
	{
		RCLogError(lua_tostring(L,-1));
		return 1;
	}

	f = combine(L);
	if (pCompiler->IsDumping())
	{
		const char* outputFilename = pCompiler->GetOutFilename();

		FILE* D = fopen(outputFilename, "wb");
		if (D == NULL)
		{
			RCLogError("Cannot open %s", outputFilename);
			return 1;
		}

		lua_lock(L);
		luaU_dump(L, f, writer, D, (int) pCompiler->IsStripping(), (int) pCompiler->IsBigEndian());
		lua_unlock(L);

		if (ferror(D))
		{
			RCLogError("Cannot write to %s", outputFilename);
			return 1;
		}

		if (fclose(D))
		{
			RCLogError("Cannot close %s", outputFilename);
			return 1;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
LuaCompiler::LuaCompiler()
	: m_bIsDumping(true)
	, m_bIsStripping(true)
	, m_bIsBigEndian(false)
{
}

//////////////////////////////////////////////////////////////////////////
LuaCompiler::~LuaCompiler()
{
}

////////////////////////////////////////////////////////////
string LuaCompiler::GetOutputFileNameOnly() const
{
	return PathUtil::RemoveExtension(m_CC.sourceFileNameOnly) + ".lua";
}

////////////////////////////////////////////////////////////
string LuaCompiler::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}

//////////////////////////////////////////////////////////////////////////
bool LuaCompiler::Process()
{
	string sourceFile = m_CC.GetSourcePath();
	string outputFile = GetOutputPath();
	std::replace(sourceFile.begin(), sourceFile.end(), '/', '\\');
	std::replace(outputFile.begin(), outputFile.end(), '/', '\\');

	if (!m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(GetOutputPath(), m_CC.GetSourcePath()))
	{
		// The file is up-to-date
		m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), GetOutputPath());
		return true;
	}

	bool ok = false;

	const bool isPlatformBigEndian = m_CC.pRC->GetPlatformInfo(m_CC.platform)->bBigEndian;

	m_bIsDumping = true;
	m_bIsStripping = true;
	m_bIsBigEndian = isPlatformBigEndian;
	m_sInFilename = sourceFile;
	m_sOutFilename = outputFile;

	lua_State* L = lua_open();

	if (L)
	{
		if (lua_cpcall(L, pmain, this) == 0)
		{
			ok = true;
		}
		else
		{
			RCLogError(lua_tostring(L,-1));
		}

		lua_close(L);
	}
	else
	{
		RCLogError("Not enough memory for lua state");
	}

	if (ok)
	{
		if (!UpToDateFileHelpers::SetMatchingFileTime(GetOutputPath(), m_CC.GetSourcePath()))
		{
			return false;
		}
		m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), GetOutputPath());
	}

	return ok;
}
