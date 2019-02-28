// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>

#include <memory>
#include <unordered_map>
#include <vector>

class CAsset;
struct IAssetDropImporter;

//! CAssetDropHandler delegates import requests to a set of registered asset drop handlers.
class CAssetDropHandler
{
private:
	struct SStringHasher
	{
		uint32 operator()(const string& str) const
		{
			return CryStringUtils::CalculateHashLowerCase(str.c_str());
		}
	};

	struct SImportParamsEx;

public:
	struct SImportParams
	{
		string outputDirectory;
		bool bHideDialog = false;
	};

public:
	CAssetDropHandler();

	static bool CanImportAny(const std::vector<string>& filePaths);
	static bool CanImportAny(const QStringList& filePaths);

	static std::vector<CAsset*> Import(const std::vector<string>& filePaths, const SImportParams& importParams);
	static std::vector<CAsset*> Import(const QStringList& filePaths, const SImportParams& importParams);

private:
	static std::vector<CAsset*> ImportExt(const string& ext, const std::vector<string>& filePaths, const SImportParamsEx& importParams);
};

