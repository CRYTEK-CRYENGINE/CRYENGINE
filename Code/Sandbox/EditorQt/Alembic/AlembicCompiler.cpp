// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AlembicCompiler.h"
#include "AlembicCompileDialog.h"
#include "Dialogs/ResourceCompilerDialog.h"
#include "FilePathUtil.h"

namespace AlembicCompiler_Private
{

// RAII handler of temporary asset data. Based on RenderDll::TextureCompiler::CTemporaryAsset. 
// TODO : Perhaps we need a generalized implementation to be used for all use cases.
class CTemporaryAsset
{
	struct SAsset
	{
		SAsset(string dst, string tmp) : dst(dst), tmp(tmp) {}
		string dst;   // Destination file.
		string tmp;   // A temporary file for the RC processing.
	};

public:
	CTemporaryAsset(const string& dstFile)
		: m_assets(CreateTemp(dstFile))
	{
	}

	~CTemporaryAsset()
	{
		const string tempPath = GetTmpPath();
		if (!tempPath.empty())
		{
			DeleteTemp(tempPath);
		}
	}

	const string& GetTmpPath() const
	{
		const static string empty;
		return !m_assets.empty() ? m_assets.front().tmp : empty;
	}

	// Moves asset(s) from the temporary to the destination location.
	bool CreateDestinationAssets()
	{
		if (m_assets.empty())
		{
			return true;
		}

		for (const auto& asset : m_assets)
		{
			if (!MoveAsset(asset.tmp.c_str(), asset.dst.c_str()))
			{
				return false;
			}
		}

		DeleteTemp(GetTmpPath());
		return true;
	}

private:
	static string GetTemporaryDirectoryPath()
	{
		char path[ICryPak::g_nMaxPath] = {};
		return GetISystem()->GetIPak()->AdjustFileName("%USER%/temp/AlembicCompiler", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_ADD_TRAILING_SLASH);
	}

	// Create a temp directory to store asset files during processing.
	static std::vector<SAsset> CreateTemp(const string& dstFile)
	{
		// Temporary files are created outside the asset directory primarily to avoid interference with the editor asset system.
		// Right now, when creating a temporary folder, we copy the folder hierarchy of the asset folder to avoid naming conflicts.
		// In the future, this should be replaced by some central system that creates temporary folders.
		static const string tempPrefix = GetTemporaryDirectoryPath();

		string drive;
		string dir;
		string filename;
		string ext;
		PathUtil::Split(dstFile, dir, filename, ext);

		// The destination should be a child of the game dir.
		const string relPath = PathUtil::AbsolutePathToGamePath(dir);
		CRY_ASSERT(!GetISystem()->GetIPak()->IsAbsPath(relPath));

		string tempDir = PathUtil::ToUnixPath(PathUtil::Make(PathUtil::Make(tempPrefix, relPath), filename));
		if (!GetISystem()->GetIPak()->MakeDir(tempDir.c_str()))
		{
			return {};
		}

		// Several assets may be created by RC from a single source file depends on the source file options.
		std::vector<SAsset> assets;
		std::vector<string> suffixes = { "" };
		const bool bCubemap = strstr(dstFile.c_str(), "_cm.") != nullptr;
		if (bCubemap)
		{
			suffixes.push_back("_diff");
		}
		assets.reserve(suffixes.size());

		for (const string& suffix : suffixes)
		{
			const string asset = filename + suffix;
			const string dstPath = PathUtil::Make(dir, asset, ext);
			const string tmpPath = PathUtil::Make(tempDir, asset, ext);
			assets.emplace_back(dstPath, tmpPath);

			// Try to update existing ".cryasset" instead of creating a new one to keep the asset GUID.
			const string dstCryasset = dstPath + ".cryasset";
			const string tmpCryasset = tmpPath + ".cryasset";
			CopyFileA(dstCryasset.c_str(), tmpCryasset.c_str(), false);
		}

		return assets;
	}

	// Delete the temporary asset directory.
	static bool DeleteTemp(const string& filepath)
	{
		const string tmpFolder = PathUtil::GetPathWithoutFilename(filepath);
		return GetISystem()->GetIPak()->RemoveDir(tmpFolder.c_str(), true);
	}

	// Returns list of asset filenames without paths.
	static std::vector<string> GetAssetFiles(const string& srcFile)
	{
		// Try to read the file list form .cryasset
		const string cryasset = srcFile + ".cryasset";

		const XmlNodeRef pXml = (GetFileAttributes(cryasset) != INVALID_FILE_ATTRIBUTES) ? GetISystem()->LoadXmlFromFile(cryasset) : nullptr;
		if (!pXml)
		{
			return { PathUtil::GetFile(srcFile) };
		}

		std::vector<string> files;
		const XmlNodeRef pMetadata = pXml->isTag("AssetMetadata") ? pXml : pXml->findChild("AssetMetadata");
		if (pMetadata)
		{
			const XmlNodeRef pFiles = pMetadata->findChild("Files");
			if (pFiles)
			{
				files.reserve(pFiles->getChildCount());
				for (int i = 0, n = pFiles->getChildCount(); i < n; ++i)
				{
					const XmlNodeRef pFile = pFiles->getChild(i);
					files.emplace_back(pFile->getAttr("path"));
				}
			}
		}

		// A fallback solution, should never happen.
		if (files.empty())
		{
			CRY_ASSERT(0);
			files.push_back(PathUtil::GetFile(srcFile));
		}

		files.emplace_back(PathUtil::GetFile(cryasset));
		return files;
	}

	// Moves asset files.
	static bool MoveAsset(const string& srcFile, const string& dstFile)
	{
		// Does not support rename.
		CRY_ASSERT(stricmp(PathUtil::GetFile(srcFile), PathUtil::GetFile(dstFile)) == 0);

		const string srcPath = PathUtil::GetPathWithoutFilename(srcFile);
		const string dstPath = PathUtil::GetPathWithoutFilename(dstFile);
		if (!GetISystem()->GetIPak()->MakeDir(dstPath.c_str()))
		{
			return false;
		}

		const auto files = GetAssetFiles(srcFile);
		for (const string& file : files)
		{
			const string srcFile = PathUtil::Make(srcPath, file);
			const string dstFile = PathUtil::Make(dstPath, file);
			if (CFileUtil::MoveFile(srcFile.c_str(), dstFile.c_str()) != CFileUtil::ETREECOPYOK)
			{
				return false;
			}
		}
		return true;
	}

private:
	std::vector<SAsset> m_assets;
};

}

bool CAlembicCompiler::CompileAlembic(string& fileName, const string& fullPath, bool showDialog)
{
	const CString rcFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), fullPath.GetString());

	const string configPath = PathUtil::ReplaceExtension(rcFilePath.GetString(), "cbc");
	XmlNodeRef config = PathUtil::FileExists(configPath) ? XmlHelpers::LoadXmlFromFile(configPath) : XmlNodeRef();
	CAlembicCompileDialog dialog(config);

	if (!showDialog || dialog.DoModal() == IDOK)
	{
		const CString upAxis = dialog.GetUpAxis();
		const CString playbackFromMemory = dialog.GetPlaybackFromMemory();
		const CString blockCompressionFormat = dialog.GetBlockCompressionFormat();
		const CString meshPrediction = dialog.GetMeshPrediction();
		const CString useBFrames = dialog.GetUseBFrames();
		const CString faceSetNumberingBase = dialog.GetFaceSetNumberingBase();
		const uint indexFrameDistance = dialog.GetIndexFrameDistance();
		const double positionPrecision = dialog.GetPositionPrecision();
		const CString vertexIndexFormat = (sizeof(vtx_idx) == sizeof(uint16)) ? "u16" : "u32";

		string targetPath = PathUtil::ReplaceExtension(rcFilePath.GetString(), CRY_GEOM_CACHE_FILE_EXT);
		AlembicCompiler_Private::CTemporaryAsset tmpAsset(targetPath);
		const string tmpDir = PathUtil::GetPathWithoutFilename(tmpAsset.GetTmpPath());

		CString additionalSettings;
		additionalSettings.Format("/refresh /threads=\"processors\" /targetroot=\"%s\" /upAxis=\"%s\" /playbackFromMemory=\"%s\""
		                          " /blockCompressionFormat=\"%s\" /meshPrediction=\"%s\" /useBFrames=\"%s\" /indexFrameDistance=\"%d\" /positionPrecision=\"%g\""
		                          " /vertexIndexFormat=\"%s\" /faceSetNumberingBase=\"%s\"",
		                          tmpDir, upAxis, playbackFromMemory, blockCompressionFormat, meshPrediction, useBFrames, indexFrameDistance, positionPrecision, vertexIndexFormat, faceSetNumberingBase);

		bool bResult = false;

		CResourceCompilerDialog rcDialog(rcFilePath, additionalSettings, [&](const CResourceCompilerHelper::ERcCallResult result)
		{
			if (result != CResourceCompilerHelper::eRcCallResult_success || !tmpAsset.CreateDestinationAssets())
			{
			  return;
			}

			// Return name of generated file
			fileName = PathUtil::ReplaceExtension(fileName.GetString(), CRY_GEOM_CACHE_FILE_EXT);
			bResult = true;
		});

		rcDialog.DoModal();
		return bResult;
	}

	return false;
}

