// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Sandbox plugin wrapper.
#include "StdAfx.h"
#include "SandboxPlugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include <CryCore/Platform/CryLibrary.h>
#include "QtViewPane.h"
#include "IEditor.h"
#include <CrySystem/ISystem.h>
#include <EditorFramework/PersonalizationManager.h>
#include "SubstanceCommon.h"
#include "EditorSubstanceManager.h"
#include "FilePathUtil.h"

#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.inl>
#include <CryCore/ToolsHelpers/SettingsManagerHelpers.inl>
#include <CryCore/ToolsHelpers/EngineSettingsManager.inl>

// Plugin versioning
static const char* g_szPluginName = "EditorSubstance";
static const char* g_szPluginDesc = "Support for Alegorythmic Substance";
static DWORD g_pluginVersion = 1;

// Plugin instance
static CSubstancePlugin* g_pInstance = nullptr;

// Global lock instance
CryCriticalSection Detail::g_lock;

REGISTER_PLUGIN(CSubstancePlugin);


class CFileHandler : public IFileManipulator
{
	virtual bool ReadFile(const string& filePath, std::vector<char>& buffer, size_t& readSize, const string& mode) override
	{
		ICryPak* const pPak = GetISystem()->GetIPak();
		FILE* pFile = pPak->FOpen(filePath, mode);
		if (!pFile)
		{
			return false;
		}
		buffer.resize(pPak->FGetSize(pFile));
		const uint64 timestamp = pPak->GetModificationTime(pFile);
		readSize = pPak->FReadRawAll(buffer.data(), buffer.size(), pFile);
		pPak->FClose(pFile);
		return true;		
	}

	virtual string GetAbsolutePath(const string& filename) const override
	{
		//char buffer[ICryPak::g_nMaxPath];
		//const char* absfilename = gEnv->pCryPak->AdjustFileName(filename, buffer, ICryPak::FLAGS_FOR_WRITING);
		return PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), filename);
	}

};

CSubstancePlugin::CSubstancePlugin()
{
	CScopedGlobalLock lock;

	assert(g_pInstance == nullptr);
	g_pInstance = this;
	CFileHandler* fileHandler = new CFileHandler();

	string iniPath;
	{
		std::vector<char> buffer;
		buffer.resize(MAX_PATH);
		auto getPath = [](std::vector<char>& buffer)
		{
			return CResourceCompilerHelper::GetResourceCompilerConfigPath(&buffer[0], buffer.size(), CResourceCompilerHelper::eRcExePath_editor);
		};
		const int len = getPath(buffer);
		if (len >= buffer.size())
		{
			buffer.resize(len + 1);
			getPath(buffer);
		}
		iniPath.assign(&buffer[0]);
	}

	InitCrySubstanceLib(fileHandler);
	EditorSubstance::CManager::Instance()->Init();
}

CSubstancePlugin::~CSubstancePlugin()
{
	CScopedGlobalLock lock;
	assert(g_pInstance == this);
	g_pInstance = nullptr;
}


CSubstancePlugin* CSubstancePlugin::GetInstance()
{
	return g_pInstance;
}

const char* CSubstancePlugin::GetPluginName()
{
	return g_szPluginName;
}


int32 CSubstancePlugin::GetPluginVersion()
{
	return g_pluginVersion;
}

const char* CSubstancePlugin::GetPluginDescription()
{
	return g_szPluginDesc;
}

static void LogVPrintf(const char* szFormat, va_list args)
{
	string format;
	format.Format("[%s] %s", g_szPluginName, szFormat);
	gEnv->pLog->LogV(IMiniLog::eMessage, format.c_str(), args);
}

void LogPrintf(const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	LogVPrintf(szFormat, args);
	va_end(args);
}

namespace EditorSubstance
{
namespace Detail
{

void Log(const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	LogVPrintf(szFormat, args);
	va_end(args);
}

} // namespace Detail
} // namespace EditorSubstance



