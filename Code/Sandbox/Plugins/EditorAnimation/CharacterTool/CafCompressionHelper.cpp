// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform.h>
#include "CafCompressionHelper.h"
#include "Shared/AnimSettings.h"

#include <FilePathUtil.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/File/ICryPak.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <QtCore/QDir>

static const char* ANIMATION_COMPRESSION_TEMP_ROOT = "Editor/Tmp/AnimationCompression/";

struct RCLogger : IResourceCompilerListener
{
	virtual void OnRCMessage(MessageSeverity severity, const char* text)
	{
		if (severity == MessageSeverity_Error)
		{
			gEnv->pLog->LogError("RC: %s", text);
		}
		else if (severity == MessageSeverity_Warning)
		{
			gEnv->pLog->LogWarning("RC: %s", text);
		}
		else
		{
			OutputDebugString("RC: ");
			OutputDebugString(text);
			OutputDebugString("\n");
		}
	}
};

//////////////////////////////////////////////////////////////////////////
bool CafCompressionHelper::CompressAnimation(const string& animationPath, string& outErrorMessage, bool* failReported)
{
	const string inputFilePath = PathUtil::ReplaceExtension(animationPath, "i_caf");

	if (gEnv->pCryPak->IsFileExist(inputFilePath, ICryPak::eFileLocation_OnDisk) == false)
	{
		outErrorMessage.Format("Uncompressed animation '%s' doesn't exist on disk.", inputFilePath.c_str());
		if (failReported)
			*failReported = true;
		return false;
	}

	const string gameFolderPath = PathUtil::AddSlash(PathUtil::GetGameProjectAssetsPath());
	string additionalSettings;
	additionalSettings += " /animConfigFolder=Animations";
	additionalSettings += " /SkipDba=1";
	additionalSettings += " /sourceroot=\"" + gameFolderPath + "\"";
	ICVar* resourceCacheFolderCVar = gEnv->pSystem->GetIConsole()->GetCVar("sys_resource_cache_folder");
	if (resourceCacheFolderCVar)
		additionalSettings += " /targetroot=\"" + string(resourceCacheFolderCVar->GetString()) + "\"";
	additionalSettings += " /debugcompression";
	additionalSettings += " /refresh=1";

	RCLogger rcLogger;
	const bool mayShowWindow = true;

	const bool silent = false;
	const bool noUserDialog = false;

	OutputDebugString("RC Call: ");
	OutputDebugString(inputFilePath.c_str());
	OutputDebugString(additionalSettings.c_str());
	OutputDebugString("\n");

	const CResourceCompilerHelper::ERcCallResult rcCallResult = CResourceCompilerHelper::CallResourceCompiler(
	  inputFilePath,
	  additionalSettings,
	  &rcLogger,
	  mayShowWindow,
	  CResourceCompilerHelper::eRcExePath_editor,
	  silent,
	  noUserDialog,
	  L".");

	if (rcCallResult != CResourceCompilerHelper::eRcCallResult_success)
	{
		outErrorMessage.Format("Compression of '%s' by RC failed. (%s)", inputFilePath.c_str(), CResourceCompilerHelper::GetCallResultDescription(rcCallResult));
		// If rcCallResult == CResourceCompilerHelper::eRcCallResult_error,
		// the RC likely already logged the error, so tell the caller not to report
		// it again. In the other cases the RC wasn't able to run yet, so we do need
		// to report it.
		if (failReported && (rcCallResult == CResourceCompilerHelper::eRcCallResult_error))
			*failReported = true;
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CafCompressionHelper::CompressAnimationForPreview(string* outputCafPath, string* outErrorMessage, const string& animationPath, const SAnimSettings& animSettings, bool ignorePresets, int sessionIndex)
{
	if (!outErrorMessage)
		return false;

	const string inputFilePath = PathUtil::ReplaceExtension(animationPath, "i_caf"); //AnimSettingsFileHelper::GetIntermediateFilename(animationPath);

	if (animSettings.build.skeletonAlias.empty())
	{
		outErrorMessage->Format("Skeleton alias is not specified.");
		return false;
	}

	if (gEnv->pCryPak->IsFileExist(inputFilePath, ICryPak::eFileLocation_OnDisk) == false)
	{
		outErrorMessage->Format("Uncompressed animation '%s' doesn't exist on disk.", inputFilePath.c_str());
		return false;
	}

	string outputFilename;
	outputFilename.Format("%s%d/", ANIMATION_COMPRESSION_TEMP_ROOT, sessionIndex);
	if (!QDir().mkpath(QString::fromLocal8Bit(outputFilename.c_str())))
	{
		outErrorMessage->Format("Failed to create directory: \"%s\"", outputFilename.c_str());
		return false;
	}
	const string settingsFilename = outputFilename + "override.animsettings";

	if (!animSettings.SaveOutsideBuild(settingsFilename.c_str()))
	{
		outErrorMessage->Format("Failed to save %s", settingsFilename.c_str());
		QFile::remove(settingsFilename.c_str());
		return false;
	}

	const string outputDirectory = PathUtil::GetPathWithoutFilename(outputFilename.c_str());
	outputFilename += animationPath;
	*outputCafPath = outputFilename;

	const string gameFolderPath = PathUtil::AddSlash(PathUtil::GetGameProjectAssetsPath());
	string additionalSettings;
	additionalSettings += " /animConfigFolder=Animations";
	additionalSettings += " /SkipDba=1";
	additionalSettings += " /sourceroot=\"" + gameFolderPath + "\"";
	additionalSettings += " /targetroot=\"" + outputDirectory + "\"";
	if (ignorePresets)
		additionalSettings += " /ignorepresets=1";
	additionalSettings += " /debugcompression";
	additionalSettings += " /refresh";
	additionalSettings += " /animSettingsFile=\"" + settingsFilename + "\"";

	RCLogger rcLogger;
	const bool mayShowWindow = true;

	const bool silent = false;
	const bool noUserDialog = false;

	OutputDebugString("RC Call: ");
	OutputDebugString(inputFilePath.c_str());
	OutputDebugString(additionalSettings.c_str());
	OutputDebugString("\n");

	const CResourceCompilerHelper::ERcCallResult rcCallResult = CResourceCompilerHelper::CallResourceCompiler(
	  inputFilePath,
	  additionalSettings,
	  &rcLogger,
	  mayShowWindow,
	  CResourceCompilerHelper::eRcExePath_editor,
	  silent,
	  noUserDialog,
	  L".");

	bool succeed = rcCallResult == CResourceCompilerHelper::eRcCallResult_success;
	QFile::remove(settingsFilename.c_str());
	string tempAnimSettingsFilename = PathUtil::ReplaceExtension(outputFilename, "$animsettings");
	QFile::remove(QString::fromLocal8Bit(tempAnimSettingsFilename.c_str()));

	if (succeed)
	{
		return true;
	}
	else
	{
		outErrorMessage->Format("Compression of '%s' by RC failed. (%s)", PathUtil::GetFile(inputFilePath.c_str()), CResourceCompilerHelper::GetCallResultDescription(rcCallResult));
		return false;
	}
}

static bool GetFileTime(__time64_t* filetime, const char* filename)
{
	_finddata_t FindFileData;
	const intptr_t hFind = gEnv->pCryPak->FindFirst(filename, &FindFileData, 0);

	if (hFind < 0)
		return false;

	gEnv->pCryPak->FindClose(hFind);

	if (filetime)
		*filetime = FindFileData.time_write;
	return true;
}

static bool ModificationTimeEqual(const char* filename1, const char* filename2)
{
	__time64_t time1;
	if (!GetFileTime(&time1, filename1))
		return false;

	__time64_t time2;
	if (!GetFileTime(&time2, filename2))
		return false;

	return time1 == time2;
}

static string AnimationNameToPath(const char* animationName)
{
	return string(PathUtil::GetGameFolder()) + "/" + animationName;
}

bool CafCompressionHelper::CheckIfNeedUpdate(const char* animationName)
{
	string intermediatePath = PathUtil::ReplaceExtension(animationName, "i_caf");
	if (!gEnv->pCryPak->IsFileExist(intermediatePath, ICryPak::eFileLocation_OnDisk))
		return false;

	string settingsPath = PathUtil::ReplaceExtension(animationName, "animsettings");
	string settingsDollarPath = PathUtil::ReplaceExtension(animationName, "$animsettings");
	if (!gEnv->pCryPak->IsFileExist(settingsPath, ICryPak::eFileLocation_OnDisk))
		return false;
	if (!gEnv->pCryPak->IsFileExist(settingsDollarPath, ICryPak::eFileLocation_OnDisk))
		return true;

	string animationPath = PathUtil::ReplaceExtension(intermediatePath, "caf");
	if (!gEnv->pCryPak->IsFileExist(animationPath, ICryPak::eFileLocation_OnDisk))
		return true;

	if (!ModificationTimeEqual(intermediatePath.c_str(), animationPath.c_str()))
		return true;

	if (!ModificationTimeEqual(settingsPath.c_str(), settingsDollarPath.c_str()))
		return true;

	return false;
}

bool CafCompressionHelper::MoveCompressionResult(string* outErrorMessage, const char* createdFile, const char* destination)
{
	if (createdFile[0] == '\0')
		return false;
	if (!QFile::exists(createdFile))
	{
		outErrorMessage->Format("Compiled animation was not created by RC: \"%s\"", createdFile);
		return false;
	}

	const string gameFolderPath = PathUtil::AddSlash(PathUtil::GetGameFolder());
	const string compressionPreviewFile = gameFolderPath + destination;
	string path;
	string filename;
	PathUtil::Split(compressionPreviewFile, path, filename);
	const bool createPathSuccess = QDir().mkpath(path.c_str());
	if (!createPathSuccess)
	{
		outErrorMessage->Format("Cannot create the path of '%s' for the compression preview file.", path.c_str());
		return false;
	}

	string dest = path + "/" + filename;
	if (QFile::exists(dest.c_str()) && !QFile::remove(dest.c_str()))
	{
		outErrorMessage->Format("Failed to remove existing file: %s", dest.c_str());
		return false;
	}
	if (!QFile::rename(createdFile, dest.c_str()))
	{
		outErrorMessage->Format("Unable to move compiled animation %s to %s", createdFile, path.c_str());
		return false;
	}

	return true;
}

void CafCompressionHelper::CleanUpCompressionResult(const char* createdFile)
{
	string createdFolder;
	string filename;
	PathUtil::Split(createdFile, createdFolder, filename);
	if (createdFile[0] != '\0')
		QFile::remove(createdFile);

	// remove all empty folders inside ANIMATION_COMPRESSION_TEMP_ROOT
	string path = createdFolder;
	while (!path.empty() && stricmp(path.c_str(), ANIMATION_COMPRESSION_TEMP_ROOT) != 0)
	{
		path.TrimRight("\\/");
		if (!QDir().rmdir(path.c_str()))
			break;
		PathUtil::Split(path, createdFolder, filename);
		path = createdFolder;
	}
}

