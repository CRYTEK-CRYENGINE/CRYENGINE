// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GenerateCharacter.h"
#include "AsyncHelper.h"
#include "AnimationHelpers/AnimationHelpers.h"
#include "DialogCommon.h"
#include "MaterialSettings.h"
#include "MaterialHelpers.h"
#include "MaterialGenerator/MaterialGenerator.h"
#include "TextureManager.h"
#include "RcLoader.h"
#include "RcCaller.h"
#include "ImporterUtil.h"

#include <Cry3DEngine/I3DEngine.h>

// EditorCommon
#include <FilePathUtil.h>
#include <FileDialogs/EngineFileDialog.h>
#include <Notifications/NotificationCenter.h>

// Editor
#include <Material/Material.h>

// Qt
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

namespace MeshImporter
{

QString CreateCHPARAMS(const QString& path);

} // namespace MeshImporter

namespace Private_GenerateCharacter
{

static const QString GetRcOptions()
{
	return QtUtil::ToQString(CRcCaller::OptionOverwriteExtension("fbx"));
}

struct SRcPayload
{
	std::unique_ptr<QTemporaryFile> m_pMetaDataFile;
	bool                            m_bCompiled;
	QString                         m_compiledFilename;
	QString                         m_extension;
};

struct SCharacterGenerator : ICharacterGenerator
{
	SCharacterGenerator()
	{
		m_sceneManager.AddAssignSceneCallback([this](const MeshImporter::SImportScenePayload* pUserData)
			{
				AssignScene(pUserData);
		  });

		m_pTextureManager.reset(new CTextureManager());
		m_pMaterialGenerator.reset(new CMaterialGenerator());
		m_pMaterialGenerator->SetSceneManager(&m_sceneManager);
		m_pMaterialGenerator->SetTaskHost(&m_taskHost);
		m_pMaterialGenerator->SetTextureManager(m_pTextureManager);
		m_pMaterialGenerator->sigMaterialGenerated.Connect(std::function<void(CMaterial*, void*)>([this](CMaterial* pMat, void*)
		{
			if (pMat)
			{
				m_materialName = pMat->GetName();
				m_outputFiles.push_back(PathUtil::ReplaceExtension(m_materialName, "mtl"));
			}
		}));

		m_pRcCaller.reset(new CRcInOrderCaller());
		m_pRcCaller->SetAssignCallback(std::bind(&SCharacterGenerator::RcCaller_Assign, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		m_pRcCaller->SetDiscardCallback(&DeletePayload<SRcPayload> );

		m_taskHost.SetTitle("Character tool");
	}

	typedef void (SCharacterGenerator::* UpdateFunc)();

	void AssignScene(const MeshImporter::SImportScenePayload* pUserData)
	{
		m_targetDirPath = QFileInfo(GetSceneManager().GetImportFile()->GetFilePath()).dir().path();
		LogPrintf("%s: Target directory is \n", m_targetDirPath.toLocal8Bit().constData());

		AutoAssignSubMaterialIDs(std::vector<std::pair<int, QString>>(), m_sceneManager.GetScene());

		m_pTextureManager->Init(m_sceneManager);
	}

	bool Import(const string& filePath)
	{
		m_outputFiles.clear();

		CFileImporter fileImporter;
		if (!fileImporter.Import(filePath))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, fileImporter.GetError().c_str());
			return false;
		}
		const string absSourceFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), fileImporter.GetOutputFilePath());

		m_outputFilePath = fileImporter.GetOutputFilePath();

		LogPrintf("%s: source file path is '%s'\n", __FUNCTION__, absSourceFilePath.c_str());

		m_sceneManager.ImportFile(QtUtil::ToQString(absSourceFilePath), nullptr, &m_taskHost);

		return true;
	}

	template<typename T>
	void RefreshResource(const T& createMetaData)
	{
		FbxMetaData::SMetaData metaData;
		createMetaData(metaData);

		auto pMetaDataFile = WriteTemporaryFile(m_targetDirPath, metaData.ToJson());
		CRY_ASSERT(pMetaDataFile);

		std::unique_ptr<SRcPayload> pPayload(new SRcPayload());
		pPayload->m_pMetaDataFile = std::move(pMetaDataFile);
		pPayload->m_bCompiled = false;
		pPayload->m_compiledFilename = GetSceneManager().GetImportFile()->GetFilename();
		pPayload->m_extension = metaData.outputFileExt;

		const QString metaDataFilename = pPayload->m_pMetaDataFile->fileName();
		m_pRcCaller->CallRc(metaDataFilename, pPayload.release(), GetTaskHost(), GetRcOptions());
	}

	void CreateMaterial()
	{
		const string mtlFilePath = QtUtil::ToString(PathUtil::ReplaceExtension(GetSceneManager().GetImportFile()->GetFilePath(), "mtl"));
		m_pMaterialGenerator->GenerateMaterial(mtlFilePath);
	}

	void CreateSkeleton()
	{
		RefreshResource([this](FbxMetaData::SMetaData& metaData)
			{
				CreateMetaDataChr(metaData);
		  });
	}

	void CreateSkin()
	{
		RefreshResource([this](FbxMetaData::SMetaData& metaData)
			{
				CreateMetaDataSkin(metaData);
		  });
	}

	void CreateAllAnimations()
	{
		std::vector<string> aliases;
		const FbxTool::CScene* const pScene = GetSceneManager().GetScene();
		aliases.reserve(pScene->GetAnimationTakeCount());
		for (int i = 0; i < pScene->GetAnimationTakeCount(); ++i)
		{
			const FbxTool::SAnimationTake* const pAnimTake = pScene->GetAnimationTakeByIndex(i);
			const string alias = UpdateAnim(pAnimTake->name, pAnimTake->startFrame, pAnimTake->endFrame);
			aliases.push_back(alias);
		}

		for (auto& s : aliases)
		{
			const string assetPath = PathUtil::Make(m_outputFilePath, s);
			const string absPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), s);
			if (PathUtil::FileExists(PathUtil::ReplaceExtension(absPath, "caf")))
			{
				m_outputFiles.push_back(s);
			}
		}
	}

	string UpdateAnim(const string& takeName, int startFrame, int endFrame)
	{
		FbxMetaData::SMetaData metaData;
		CreateMetaDataCaf(metaData, takeName, startFrame, endFrame);

		string alias = takeName.empty() ? "Default take" : MakeAlphaNum(takeName);

		auto pMetaDataFile = WriteTemporaryFile(m_targetDirPath, metaData.ToJson(), QtUtil::ToQString(alias));
		CRY_ASSERT(pMetaDataFile);

		std::unique_ptr<SRcPayload> pPayload(new SRcPayload());
		pPayload->m_pMetaDataFile = std::move(pMetaDataFile);
		pPayload->m_bCompiled = true;
		pPayload->m_extension = metaData.outputFileExt;

		const QString metaDataFilename = pPayload->m_pMetaDataFile->fileName();
		m_pRcCaller->CallRc(metaDataFilename, pPayload.release(), GetTaskHost(), GetRcOptions());

		return alias;
	}

	void CreateCharacterParameters()
	{
		using namespace MeshImporter;

		const QString filePath = GetSceneManager().GetImportFile()->GetFilePath();
		const QString gameDirPath = PathUtil::ToGamePath(m_targetDirPath);
		const QString chrparams = CreateCHPARAMS(gameDirPath);
		if (chrparams.isEmpty())
		{
			return;
		}

		WriteToFile(PathUtil::ReplaceExtension(filePath, "chrparams"), chrparams);
	}

	void CreateCharacterDefinition()
	{
		using namespace MeshImporter;

		const QString filePath = GetSceneManager().GetImportFile()->GetFilePath();
		const QString gameFilePath = PathUtil::ToGamePath(filePath);

		const QString cdf = CreateCDF(
		  PathUtil::ReplaceExtension(gameFilePath, "chr"),
		  PathUtil::ReplaceExtension(gameFilePath, "skin"),
		  PathUtil::ReplaceExtension(gameFilePath, "mtl"));
		if (cdf.isEmpty())
		{
			return;
		}

		WriteToFile(PathUtil::ReplaceExtension(filePath, "cdf"), cdf);
	}

	const FbxTool::CScene* GetScene() const
	{
		return m_sceneManager.GetScene();
	}

	FbxTool::CScene* GetScene()
	{
		return m_sceneManager.GetScene();
	}

	void CreateMetaDataCommon(FbxMetaData::SMetaData& metaData) const
	{
		metaData.Clear();
		metaData.sourceFilename = PathUtil::GetFile(QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath()));
	}

	void CreateMetaDataChr(FbxMetaData::SMetaData& metaData) const
	{
		CreateMetaDataCommon(metaData);
		metaData.outputFileExt = "chr";
	}

	void CreateMetaDataSkin(FbxMetaData::SMetaData& metaData) const
	{
		CreateMetaDataCommon(metaData);
		metaData.outputFileExt = "skin";

		WriteMaterialMetaData(GetSceneManager().GetScene(), metaData.materialData);

		metaData.materialFilename = m_materialName;
	}

	void CreateMetaDataCaf(FbxMetaData::SMetaData& metaData, const string& takeName, int startFrame, int endFrame) const
	{
		CreateMetaDataCommon(metaData);
		metaData.animationClip.takeName = takeName;
		metaData.animationClip.startFrame = startFrame;
		metaData.animationClip.endFrame = endFrame;

		// Writes i_caf with caf extension. Temporary solution.
		metaData.outputFileExt = "caf";
	}

	void RcCaller_Assign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData)
	{
		using namespace MeshImporter;

		std::unique_ptr<SRcPayload> pPayload((SRcPayload*)pUserData);

		const QString ext = pPayload->m_extension;

		const QString outputFilePath = PathUtil::ReplaceExtension(filePath, ext.toLocal8Bit().constData());

		if (pPayload->m_bCompiled)
		{
			if (!pPayload->m_compiledFilename.isEmpty())
			{
				const QString newFilePath = QFileInfo(filePath).dir().path() + "/" + PathUtil::ReplaceExtension(pPayload->m_compiledFilename, ext.toLocal8Bit().constData());
				QFile file(outputFilePath);
				RenameAllowOverwrite(file, newFilePath);
			}
		}
		else
		{
			pPayload->m_bCompiled = true;
			pCaller->CallRc(outputFilePath, pPayload.release(), GetTaskHost());
		}
	}

	const MeshImporter::CSceneManager& GetSceneManager() const
	{
		return m_sceneManager;
	}

	MeshImporter::CSceneManager& GetSceneManager()
	{
		return m_sceneManager;
	}

	ITaskHost* GetTaskHost()
	{
		return &m_taskHost;
	}

	std::vector<string> GetOutputFiles() const
	{
		return m_outputFiles;
	}

	CSyncTaskHost                     m_taskHost;
	MeshImporter::CSceneManager       m_sceneManager;
	QString                           m_targetDirPath;
	std::shared_ptr<CTextureManager> m_pTextureManager;
	std::unique_ptr<CMaterialGenerator>   m_pMaterialGenerator;
	string m_materialName;
	std::unique_ptr<CRcInOrderCaller> m_pRcCaller;
	std::vector<string> m_outputFiles; //!< Relative to asset directory.
	string m_outputFilePath; //!< Output directory path, relative to asset directory.
	int m_genFlags;
};

} // Private_GenerateCharacter

string GenerateCharacter(const QString& filePath)
{
	using namespace Private_GenerateCharacter;

	typedef void(ICharacterGenerator::*Func)();
	std::vector<Func> funcs =
	{
		&ICharacterGenerator::CreateMaterial,
		&ICharacterGenerator::CreateSkeleton,
		&ICharacterGenerator::CreateSkin,
		&ICharacterGenerator::CreateAllAnimations,
		&ICharacterGenerator::CreateCharacterParameters,
		&ICharacterGenerator::CreateCharacterDefinition
	};

	CProgressNotification notif("Character tool", "Creating character definition", true);
	const float progressStep = 1.0f / funcs.size();
	float progress = 0.0f;

	SCharacterGenerator charGen;
	charGen.Import(QtUtil::ToString(filePath));

	for (auto& f : funcs)
	{
		(charGen.*f)();
		progress += progressStep;
		notif.SetProgress(progress);
	}

	return string(); // Unused.
}

std::unique_ptr<ICharacterGenerator> CreateCharacterGenerator(const string& inputFilePath)
{
	using namespace Private_GenerateCharacter;
	std::unique_ptr<SCharacterGenerator> pCharGen(new SCharacterGenerator());
	pCharGen->Import(inputFilePath);
	return pCharGen;
}

std::vector<string> GenerateMaterialAndTextures(const string& inputFilePath)
{
	using namespace Private_GenerateCharacter;

	SCharacterGenerator charGen;
	charGen.Import(inputFilePath);

	charGen.CreateMaterial();

	return charGen.GetOutputFiles();
}

std::vector<string> GenerateAllAnimations(const string& inputFilePath)
{
	using namespace Private_GenerateCharacter;

	SCharacterGenerator charGen;
	charGen.Import(inputFilePath);

	charGen.CreateAllAnimations();

	return charGen.GetOutputFiles();
}

