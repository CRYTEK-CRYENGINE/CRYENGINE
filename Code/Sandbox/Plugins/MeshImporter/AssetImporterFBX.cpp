// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetImporterFBX.h"
#include "SandboxPlugin.h"
#include "ImporterUtil.h"
#include "FbxScene.h"
#include "FbxMetaData.h"
#include "RcCaller.h"
#include "TextureHelpers.h"
#include "MaterialHelpers.h"
#include "AnimationHelpers/AnimationHelpers.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetImportContext.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/AssetType.h>
#include <FilePathUtil.h>
#include <ThreadingUtils.h>

#include <Cry3DEngine/I3DEngine.h>
#include <Material/Material.h>
#include <Material/MaterialManager.h>
#include <Material/MaterialHelpers.h>

#include <Util/FileUtil.h> // #TODO Remove dependency to MFCToolPlugin

#include <QDirIterator>
#include <QFile>
#include <QTemporaryFile>
#include <QTemporaryDir>

REGISTER_ASSET_IMPORTER(CAssetImporterFBX)

namespace MeshImporter
{

QString CreateCHPARAMS(const QString& path);

} // namespace MeshImporter

bool ReadMetaDataFromFile(const QString& filePath, FbxMetaData::SMetaData& metaData);

namespace Private_AssetImporterFBX
{

class CRcListener : public CRcCaller::IListener
{
public:
	static CRcListener& GetInstance()
	{
		static CRcListener theInstance;
		return theInstance;
	}

	virtual void ShowWarning(const string& message) override
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING, "%s", message.c_str());
	}

	virtual void ShowError(const string& message) override {}
};

string ImportFbxFile(const string& inputFilePath, const string& outputDirectoryPath)
{
	if (inputFilePath.empty() || outputDirectoryPath.empty())
	{
		return string();
	}

	string outputFilePath = PathUtil::Make(outputDirectoryPath, PathUtil::GetFile(inputFilePath));
	if (CopyNoOverwrite(inputFilePath, outputFilePath))
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_ERROR, "Importing FBX file '%s' failed.", inputFilePath.c_str());
		return string();
	}

	return outputFilePath;
}

std::vector<const FbxTool::SNode*> FindAllSkinNodes(const FbxTool::CScene* pFbxScene)
{
	std::vector<const FbxTool::SNode*> skinNodes;
	for (int i = 0, N = pFbxScene->GetNodeCount(); i < N; ++i)
	{
		const FbxTool::SNode* const pFbxNode = pFbxScene->GetNodeByIndex(i);
		bool bHasSkin = false;
		for (int j = 0; !bHasSkin && j < pFbxNode->numMeshes; ++j)
		{
			bHasSkin = bHasSkin || pFbxNode->ppMeshes[j]->bHasSkin;
		}
		if (bHasSkin)
		{
			skinNodes.push_back(pFbxNode);
		}
	}
	return skinNodes;
}

// Returns a list of named nodes. The names are unique among all nodes and can be used as filenames.
static std::vector<std::pair<const FbxTool::SNode*, string>> GetNamedNodes(const FbxTool::CScene* pFbxScene, const std::vector<const FbxTool::SNode*>& nodes)
{
	std::vector<std::pair<const FbxTool::SNode*, string>> out;
	for (const FbxTool::SNode* pNode : nodes)
	{
		out.emplace_back(pNode, string(pNode->szName));
	}
	return out;
}

static std::vector<const FbxTool::SAnimationTake*> GetAnimationTakes(const FbxTool::CScene* pScene)
{
	std::vector<const FbxTool::SAnimationTake*> takes;
	takes.reserve(pScene->GetAnimationTakeCount());
	for (int i = 0, N = pScene->GetAnimationTakeCount(); i < N; ++i)
	{
		takes.push_back(pScene->GetAnimationTakeByIndex(i));
	}
	return takes;
}

// Returns a list of named takes. The names are unique among all takes and can be used as filenames.
static std::vector<std::pair<const FbxTool::SAnimationTake*, string>> GetNamedTakes(const std::vector<const FbxTool::SAnimationTake*>& takes)
{
	typedef std::pair<const FbxTool::SAnimationTake*, string> NamedTake;

	std::vector<NamedTake> namedTakes;
	namedTakes.reserve(takes.size());
	std::transform(takes.begin(), takes.end(), std::back_inserter(namedTakes), [](const FbxTool::SAnimationTake* pTake)
	{
		return std::make_pair(pTake, MakeAlphaNum(pTake->name));
	});
	std::sort(namedTakes.begin(), namedTakes.end(), [](const NamedTake& lhp, const NamedTake& rhp)
	{
		return lhp.second < rhp.second;
	});
	std::vector<int> count(namedTakes.size(), 0);
	for (size_t i = 1; i < namedTakes.size(); ++i)
	{
		if (namedTakes[i].second == namedTakes[i - 1].second)
		{
			count[i] = count[i - 1] + 1;
		}
	}
	for (size_t i = 1; i < namedTakes.size(); ++i)
	{
		if (count[i] > 0)
		{
			namedTakes[i].second = string().Format("%s%d", namedTakes[i].second, count[i]);
		}
	}
	return namedTakes;
}

static bool WriteAssetMetaData(const FbxMetaData::SMetaData& metaData, const string& outputDirectory, const string& filename)
{
	using namespace Private_AssetImporterFBX;

	// Write meta-data to file.
	std::unique_ptr<QTemporaryFile> pTempFile = WriteTemporaryFile(outputDirectory, metaData.ToJson(), filename);
	if (!pTempFile)
	{
		return false;
	}

	// Call RC.
	CRcCaller rcCaller;
	rcCaller.SetEcho(false);
	rcCaller.SetListener(&CRcListener::GetInstance());
	rcCaller.OverwriteExtension("fbx");
	rcCaller.SetAdditionalOptions(CRcCaller::OptionOverwriteFilename(PathUtil::ReplaceExtension(filename, metaData.outputFileExt)));
	return rcCaller.Call(QtUtil::ToString(pTempFile->fileName()));
}

static std::vector<string> FindAssetMetaDataPaths(const string& dir)
{
	const size_t dirLen = PathUtil::AddSlash(dir).size();
	std::vector<string> assetPaths;
	QDirIterator iterator(QtUtil::ToQString(dir), QStringList() << "*.cryasset", QDir::Files, QDirIterator::Subdirectories);
	while (iterator.hasNext())
	{
		const string filePath = QtUtil::ToString(iterator.next()).substr(dirLen); // Remove leading path to search directory.
		assetPaths.push_back(filePath);
	}
	return assetPaths;
}

static std::vector<string> FindAllTextures(const FbxTool::CScene* pFbxScene)
{
	std::vector<string> texs;
	for (int i = 0; i < pFbxScene->GetMaterialCount(); ++i)
	{
		const FbxTool::SMaterial* const pMaterial = pFbxScene->GetMaterialByIndex(i);

		for (int j = 0; j < FbxTool::eMaterialChannelType_COUNT; ++j)
		{
			const FbxTool::SMaterialChannel& channel = pMaterial->m_materialChannels[j];
			if (channel.m_bHasTexture)
			{
				texs.push_back(channel.m_textureFilename);
			}
		}
	}
	return texs;
}

static std::vector<std::pair<string, string>> TranslateTexturePaths(const std::vector<string>& texs, const string& sourceFilePath)
{
	std::vector<std::pair<string, string>> relTexs;
	relTexs.reserve(texs.size());
	for (const string& tex : texs)
	{
		relTexs.emplace_back(tex, TextureHelpers::TranslateFilePath(sourceFilePath, sourceFilePath, tex));
	}
	return relTexs;
}

static void InitScene(FbxTool::CScene* pFbxScene)
{
	for (int i = 0, N = pFbxScene->GetMaterialCount(); i < N; ++i)
	{
		const FbxTool::SMaterial* const pFbxMaterial = pFbxScene->GetMaterialByIndex(i);
		pFbxScene->SetMaterialSubMaterialIndex(pFbxMaterial, i);
		pFbxScene->SetMaterialPhysicalizeSetting(pFbxMaterial, FbxTool::eMaterialPhysicalizeSetting_None);
	}
}

static void InitializeMaterial(CMaterial* pEditorMaterial, const FbxTool::SMaterial& fbxMaterial, const std::vector<std::pair<string, string>>& relTexs, const string& outDir)
{
	SInputShaderResources& inputRes = pEditorMaterial->GetShaderResources();

	// Lighting settings of fabric (non-metal).
	// Setting specular from file might mess up rendering if it does not respect PBS conventions.
	inputRes.m_LMaterial.m_Diffuse = ColorGammaToLinear(ColorF(255, 255, 255));
	inputRes.m_LMaterial.m_Specular = ColorGammaToLinear(ColorF(61, 61, 61));
	inputRes.m_LMaterial.m_Smoothness = 255.0f;

	constexpr char* defaultTexture[FbxTool::eMaterialChannelType_COUNT]
	{
		"%ENGINE%/EngineAssets/Textures/white.dds",
		"%ENGINE%/EngineAssets/Textures/white_ddn.dds",
		""
	};

	for (int i = 0; i < FbxTool::eMaterialChannelType_COUNT; ++i)
	{
		const char* szSemantic = GetTextureSemanticFromChannelType(FbxTool::EMaterialChannelType(i));
		if (!szSemantic)
		{
			continue;
		}

		const EEfResTextures texId = MaterialHelpers::FindTexSlot(szSemantic);

		if (fbxMaterial.m_materialChannels[i].m_bHasTexture)
		{
			const string tex = fbxMaterial.m_materialChannels[i].m_textureFilename;
			auto it = std::find_if(relTexs.begin(), relTexs.end(), [&tex](const std::pair<string, string>& relTex)
			{
				return relTex.first == tex;
			});
			if (it != relTexs.end())
			{
				const string relTif = PathUtil::ReplaceExtension(it->second, "tif");
				inputRes.m_Textures[texId].m_Name = PathUtil::Make(outDir, relTif);
			}
		}
		else
		{
			inputRes.m_Textures[texId].m_Name = defaultTexture[i];
		}
	}
}

static void WriteCHRPARAMS(CAsset* pSkeletonAsset, CAssetImportContext& ctx)
{
	if (!pSkeletonAsset->GetFilesCount())
	{
		return;
	}

	const string skeletonFilePath = pSkeletonAsset->GetFile(0);
	bool bWroteChrParams = false;
	const string chrParamsAssetPath = PathUtil::ReplaceExtension(skeletonFilePath, "chrparams");
	const string chrParamsAbsFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), chrParamsAssetPath);
	const QString chrparams = MeshImporter::CreateCHPARAMS(QtUtil::ToQString(ctx.GetOutputDirectoryPath()));
	if (!chrparams.isEmpty())
	{
		if (WriteToFile(QtUtil::ToQString(chrParamsAbsFilePath), chrparams))
		{
			CEditableAsset editableSkeletonAsset = ctx.CreateEditableAsset(*pSkeletonAsset);
			editableSkeletonAsset.AddFile(chrParamsAssetPath);
			editableSkeletonAsset.WriteToFile();
			bWroteChrParams = true;
		}
	}
	if (!bWroteChrParams)
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_WARNING, string().Format("Unable to write .chrparams file '%s'", PathUtil::ToUnixPath(chrParamsAbsFilePath).c_str()));
	}
}

//! Returns a list of all files that make up an asset which cannot be automatically regenerated.
//! This list includes, for example, the meta-data (cryasset) file, source file, and all data files.
//! The list does not include thumbnails.
//! The list of independent files is used when moving an asset from a temporary folder to the game project folder.
static std::vector<string> GetIndependentFiles(const CAsset* pAsset)
{
	std::vector<string> files;

	files.push_back(pAsset->GetMetadataFile());
	files.push_back(pAsset->GetSourceFile());
	for (size_t i = 0, N = pAsset->GetFilesCount(); i < N; ++i)
	{
		files.push_back(pAsset->GetFile(i));
	}

	return files;
}

//! Convenience function that imports a single asset type to an output directory and passes along the parent importing context.
std::vector<CAsset*> ImportDependency(const string& sourceFilePath, const string& outputDirectory, const char* assetType, CAssetImportContext& parentCtx)
{
	string ext(PathUtil::GetExt(sourceFilePath.c_str()));
	ext.MakeLower();
	CAssetImporter* pAssetImporter = CAssetManager::GetInstance()->GetAssetImporter(ext, assetType);
	if (!pAssetImporter)
	{
		return {};
	}
	CAssetImportContext ctx;
	ctx.SetInputFilePath(sourceFilePath);
	ctx.SetOutputDirectoryPath(outputDirectory);
	ctx.SetOverwriteAll(parentCtx.IsOverwriteAll());
	return pAssetImporter->Import({ assetType }, ctx);
}

} // namespace Private_AssetImporterFBX

std::vector<string> CAssetImporterFBX::GetFileExtensions() const
{
	return { "fbx" }; // TODO: Return all extensions supported by the FBX SDK.
}

std::vector<string> CAssetImporterFBX::GetAssetTypes() const
{
	return
	{
		"Mesh",
		"Skeleton",
		"SkinnedMesh",
		"Animation"
	};
}

bool CAssetImporterFBX::Init(const string& inputFilePath)
{
	using namespace Private_AssetImporterFBX;

	m_fbxFilePath.clear();
	m_pFbxScene.release();
	m_pTempDir.release();
	m_bCreateMaterial = false;

	m_pTempDir = CreateTemporaryDirectory();
	const string importedFilePath = ImportFbxFile(inputFilePath, GetTempDirPath());
	if (importedFilePath.empty())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Importing file '%s' failed.", inputFilePath.c_str());
		return false;
	}
	m_fbxFilePath = importedFilePath;
	return true;
}

FbxTool::CScene* CAssetImporterFBX::GetScene()
{
	using namespace Private_AssetImporterFBX;

	// Lazy scene loading.
	if (!m_pFbxScene)
	{
		m_pFbxScene = FbxTool::CScene::ImportFile(m_fbxFilePath.c_str());
		if (!m_pFbxScene)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Importing scene from failed.");
			return nullptr;
		}
		InitScene(m_pFbxScene.get());
	}
	return m_pFbxScene.get();
}

string CAssetImporterFBX::GetTempDirPath() const
{
	return QtUtil::ToString(m_pTempDir->path());
}

void CAssetImporterFBX::Import(const std::vector<string>& assetTypes, CAssetImportContext& ctx)
{
	for (const string& assetType : assetTypes)
	{
		if (assetType == "Mesh")
		{
			m_bCreateMaterial = true;
			ImportMesh(ctx);
		}
		else if (assetType == "Skeleton")
		{
			ImportSkeleton(ctx);
		}
		else if (assetType == "SkinnedMesh")
		{
			m_bCreateMaterial = true;
			ImportAllSkins(ctx);
		}
		else if (assetType == "Animation")
		{
			ImportAllAnimations(ctx);
		}
	}
}

std::vector<string> CAssetImporterFBX::GetAssetNames(const std::vector<string>& assetTypes, CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	if (!Init(ctx.GetInputFilePath()))
	{
		return std::vector<string>();
	}

	Import(assetTypes, ctx);

	std::vector<string> assetMetaDataPaths = FindAssetMetaDataPaths(GetTempDirPath());

	// Prepend paths that are local to temporary directory with actual output directory.
	for (string& path : assetMetaDataPaths)
	{
		path = PathUtil::Make(ctx.GetOutputDirectoryPath(), path);
	}

	return assetMetaDataPaths;
}

std::vector<CAsset*> CAssetImporterFBX::ImportAssets(const std::vector<string>& assetPaths, CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	std::vector<CAsset*> importedAssets;
	importedAssets.reserve(assetPaths.size());
	for (const string& assetPath : assetPaths)
	{
		MoveAsset(ctx, assetPath);

		CAsset* const pAsset = ctx.LoadAsset(assetPath);
		if (pAsset)
		{
			importedAssets.push_back(pAsset);
		}
	}

	if (m_bCreateMaterial)
	{
		const std::vector<string>& texs = FindAllTextures(GetScene());
		const std::vector<std::pair<string, string>>& relTexs = TranslateTexturePaths(texs, m_fbxFilePath);
		ImportAllTextures(relTexs, ctx.GetOutputDirectoryPath(), ctx);
		// Importing a material uses the material manager and renderer, so it must run on the main thread.
		const string materialAssetName = ThreadingUtils::PostOnMainThread([this, &ctx, &relTexs] { return ImportMaterial(ctx, relTexs); }).get();
		if (!materialAssetName.empty())
		{
			CAsset* const pMaterialAsset = ctx.LoadAsset(materialAssetName);
			if (pMaterialAsset)
			{
				importedAssets.push_back(pMaterialAsset);
			}
		}
	}

	ImportCharacterDefinition(ctx, importedAssets);

	return importedAssets; // TODO: Return complete list of assets!
}

void CAssetImporterFBX::ImportMesh(CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	const FbxTool::CScene* const pFbxScene = GetScene();
	if (!pFbxScene)
	{
		return;
	}

	const string filename = PathUtil::GetFile(ctx.GetInputFilePath());
	const string basename = ctx.GetAssetName();
	const string absOutputDirectoryPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), ctx.GetOutputDirectoryPath());

	// Init meta-data.
	FbxMetaData::SMetaData metaData;
	metaData.sourceFilename = filename;
	metaData.materialFilename = basename;
	metaData.outputFileExt = "cgf";
	WriteMaterialMetaData(pFbxScene, metaData.materialData);

	WriteAssetMetaData(metaData, GetTempDirPath(), basename);
}

void CAssetImporterFBX::ImportSkeleton(CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	const string filename = PathUtil::GetFile(ctx.GetInputFilePath());
	const string basename = ctx.GetAssetName();
	const string absOutputDirectoryPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), ctx.GetOutputDirectoryPath());

	// Init meta-data.
	FbxMetaData::SMetaData metaData;
	metaData.sourceFilename = filename;
	metaData.outputFileExt = "chr";

	WriteAssetMetaData(metaData, GetTempDirPath(), basename);
}

void CAssetImporterFBX::ImportAllSkins(CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	const FbxTool::CScene* const pFbxScene = GetScene();
	if (!pFbxScene)
	{
		return;
	}

	typedef std::pair<const FbxTool::SNode*, string> NamedNode;

	const std::vector<const FbxTool::SNode*>& skinNodes = FindAllSkinNodes(pFbxScene);
	const std::vector<NamedNode>& namedSkinNodes = GetNamedNodes(pFbxScene, skinNodes);

	for (const NamedNode& namedNode : namedSkinNodes)
	{
		ImportSkin(namedNode.first, namedNode.second, ctx);
	}
}

void CAssetImporterFBX::ImportSkin(const FbxTool::SNode* pFbxNode, const string& name, CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	const string filename = PathUtil::GetFile(ctx.GetInputFilePath());
	const string basename = ctx.GetAssetName();

	// Init meta-data.
	FbxMetaData::SMetaData metaData;
	metaData.sourceFilename = filename;
	metaData.materialFilename = basename;
	metaData.outputFileExt = "skin";
	WriteMaterialMetaData(pFbxNode->pScene, metaData.materialData);

	// Find the node for this skin and write its path to meta-data.
	FbxMetaData::SNodeMeta nodeMeta;
	nodeMeta.name = pFbxNode->szName;
	nodeMeta.nodePath = FbxTool::GetPath(pFbxNode);
	metaData.nodeData.push_back(nodeMeta);

	WriteAssetMetaData(metaData, GetTempDirPath(), basename + "_" + name);
}

void CAssetImporterFBX::ImportAllAnimations(CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	const FbxTool::CScene* const pFbxScene = GetScene();
	if (!pFbxScene)
	{
		return;
	}

	typedef std::pair<const FbxTool::SAnimationTake*, string> NamedTake;

	const std::vector<const FbxTool::SAnimationTake*>& takes = GetAnimationTakes(pFbxScene);
	const std::vector<NamedTake>& namedTakes = GetNamedTakes(takes); 

	for (const NamedTake& namedTake : namedTakes)
	{
		ImportAnimation(namedTake.first, namedTake.second, ctx);
	}
}

void CAssetImporterFBX::ImportAnimation(const FbxTool::SAnimationTake* pTake, const string& name, CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	const string filename = PathUtil::GetFile(ctx.GetInputFilePath());
	const string basename = ctx.GetAssetName();

	// Init meta-data.
	FbxMetaData::SMetaData metaData;
	metaData.sourceFilename = filename;
	metaData.outputFileExt = "caf";
	metaData.animationClip.takeName = pTake->name;
	metaData.animationClip.startFrame = pTake->startFrame;
	metaData.animationClip.endFrame = pTake->endFrame;

	WriteAssetMetaData(metaData, GetTempDirPath(), basename + "_" + name);
}

void CAssetImporterFBX::ImportAllTextures(const std::vector<std::pair<string, string>>& relTexs, const string& outputDirectory, CAssetImportContext& ctx)
{
	using namespace Private_AssetImporterFBX;

	CAssetType* texType = CAssetManager::GetInstance()->FindAssetType("Texture");
	if (!texType)
	{
		return;
	}

	for (const std::pair<string, string>& relTex : relTexs)
	{
		const string relDir = PathUtil::GetPathWithoutFilename(relTex.second);
		ImportDependency(relTex.first, PathUtil::Make(outputDirectory, relDir), "Texture", ctx);
	}
}

std::vector<std::pair<string, string>> CAssetImporterFBX::GetAbsOutputFilePaths(const CAssetImportContext& ctx, const std::vector<string>& absTempFilePaths) const
{	
	std::vector<std::pair<string, string>> result;
	result.reserve(absTempFilePaths.size());

	const string fromDir = GetTempDirPath();
	const string toDir = ctx.GetOutputDirectoryPath();

	const string absToDir = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), toDir);
	for (string file : absTempFilePaths)
	{
		file = file.substr(PathUtil::AddSlash(fromDir).size());
		result.emplace_back(PathUtil::Make(fromDir, file), PathUtil::Make(absToDir, file));
	};

	return result;
}

void CAssetImporterFBX::MoveAsset(CAssetImportContext& ctx, const string& assetPath)
{
	using namespace Private_AssetImporterFBX;

	const string fromDir = GetTempDirPath();
	const string toDir = ctx.GetOutputDirectoryPath();

	// Assets are moved from the temporary directory to the output directory.
	const string fromAssetPath = PathUtil::Make(fromDir, assetPath.substr(PathUtil::AddSlash(toDir).size()));

	std::unique_ptr<CAsset> pAsset(ctx.LoadAsset(fromAssetPath));

	const std::vector<std::pair<string, string>> movedFilePaths = GetAbsOutputFilePaths(ctx, GetIndependentFiles(pAsset.get()));

	for (const auto& movedFilePath : movedFilePaths)
	{
		const string& fromName = movedFilePath.first;
		const string& toName = movedFilePath.second;

		if (FileExists(toName) && !IsFileWritable(toName) && !CFileUtil::CompareFiles(toName.c_str(), fromName.c_str()))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot create asset '%s', because file '%s' is not writable.",
				assetPath.c_str(), toName.c_str());
			return; // If any of the target files is not writable, we cancel the move operation to prevent incomplete assets.
		}
	}

	const string absToDir = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), toDir);
	QDir().mkpath(QtUtil::ToQString(absToDir));

	for (const auto& movedFilePath : movedFilePaths)
	{
		const string& fromName = movedFilePath.first;
		const string& toName = movedFilePath.second;

		// It is possible that the source fromName has been moved already by a preceding MoveAsset.
		// In particular, only one MoveAsset will successfully move the source FBX.
		if (FileExists(fromName))
		{
			CopyAllowOverwrite(fromName, toName);
		}
	}
}

string CAssetImporterFBX::ImportMaterial(CAssetImportContext& ctx, const std::vector<std::pair<string, string>>& relTexs)
{
	using namespace Private_AssetImporterFBX;

	FbxTool::CScene* const pFbxScene = GetScene();
	if (!pFbxScene)
	{
		return string();
	}

	const int materialCount = pFbxScene->GetMaterialCount();

	const int mtlFlags = materialCount > 1 ? MTL_FLAG_MULTI_SUBMTL : 0;

	const string materialName = ctx.GetOutputFilePath("");
	_smart_ptr<CMaterial> pMaterial = GetIEditor()->GetMaterialManager()->CreateMaterial(materialName, XmlNodeRef(), mtlFlags);
	if (!pMaterial)
	{
		return string();
	}
	const string outDir = ctx.GetOutputDirectoryPath();
	CreateMaterial(pMaterial, pFbxScene, [&relTexs, &outDir](CMaterial* pEditorMaterial, const FbxTool::SMaterial& fbxMaterial)
	{
		InitializeMaterial(pEditorMaterial, fbxMaterial, relTexs, outDir);
	});

	// Call RC to create the .mtl.cryasset for the .mtl file.
	const string absMaterialFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), materialName + ".mtl");
	CRcCaller rcCaller;
	rcCaller.SetEcho(false);
	rcCaller.SetListener(&CRcListener::GetInstance());
	rcCaller.OverwriteExtension("cryasset");
	rcCaller.Call(absMaterialFilePath);

	return materialName + ".mtl.cryasset";
}

void CAssetImporterFBX::ImportCharacterDefinition(CAssetImportContext& ctx, const std::vector<CAsset*> assets)
{
	using namespace Private_AssetImporterFBX;

	const bool bHasAnimations = std::any_of(assets.begin(), assets.end(), [](CAsset* pAsset)
	{
		return !strcmp(pAsset->GetType()->GetTypeName(), "Animation");
	});

	string skeletonFilePath;
	string skinFilePath;

	if (bHasAnimations)
	{
		for (CAsset* pAsset : assets)
		{
			if (!strcmp(pAsset->GetType()->GetTypeName(), "Skeleton"))
			{
				// If there are animations, we create a CHRPARAMS file for the skeleton that references them.
				WriteCHRPARAMS(pAsset, ctx);

				skeletonFilePath = pAsset->GetFile(0);
			}
			else if (!strcmp(pAsset->GetType()->GetTypeName(), "SkinnedMesh"))
			{
				skinFilePath = pAsset->GetFile(0);
			}
		}
	}

	// Write CDF file.
	const QString cdf = CreateCDF(
		QtUtil::ToQString(skeletonFilePath),
		QtUtil::ToQString(skinFilePath),
		QString());
	if (cdf.isEmpty())
	{
		return;
	}

	const string absOutputFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::ReplaceExtension(skeletonFilePath, "cdf"));

	// Asset manager file monitor will create the corresponding cryasset.
	// TODO: move the asset creation to the asset type.
	if (!WriteToFile(QtUtil::ToQString(absOutputFilePath), cdf))
	{
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_ERROR, string().Format("Writing character definition file '%s' failed."),
			PathUtil::ToUnixPath(absOutputFilePath).c_str());
		return;
	}
}

void CAssetImporterFBX::ReimportAsset(CAsset* pAsset)
{
	using namespace Private_AssetImporterFBX;

	const string absSourcePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetSourceFile());
	if (!Init(absSourcePath))
	{
		return;
	}

	// Try to read FBX meta-data from data file.
	FbxMetaData::SMetaData metaData;
	const string absChunkFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetFile(0));
	if (!ReadMetaDataFromFile(QtUtil::ToQString(absChunkFilePath), metaData))
	{
		return;
	}

	const string filename = PathUtil::GetFile(absSourcePath);
	const string basename = PathUtil::RemoveExtension(filename);

	WriteAssetMetaData(metaData, GetTempDirPath(), basename);

	const string outputDir = PathUtil::GetPathWithoutFilename(pAsset->GetMetadataFile());
	CAssetImportContext ctx;
	ctx.SetOutputDirectoryPath(outputDir);
	for (const string& assetPath : FindAssetMetaDataPaths(GetTempDirPath()))
	{
		MoveAsset(ctx, PathUtil::Make(outputDir, assetPath));
	}
}

