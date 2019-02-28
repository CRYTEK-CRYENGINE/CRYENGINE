// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CreateMaterialTask.h"
#include "MaterialHelpers.h"
#include "TextureHelpers.h"
#include "TextureManager.h"
#include "DialogCommon.h"
#include "ImporterUtil.h"

#include <CryCore/ToolsHelpers/SettingsManagerHelpers.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Material/Material.h>
#include <Material/MaterialManager.h>
#include <Material/MaterialHelpers.h>

// EditorCommon
#include "FilePathUtil.h"
#include <FileDialogs/EngineFileDialog.h>
#include <Controls/QuestionDialog.h>

#include <QFileInfo>
#include <QDir>

void LogPrintf(const char* szFormat, ...);

namespace Private_CreateMaterialTask
{

// Duplicated code, from Editor/Util/EditorUtils.h
// TODO: Move this to EditorCommon.
// See also:
// CMaterialManager::SelectSaveMaterial
// CMatMan::UnifyName

QString SelectNewMaterial(const QString& startPath)
{
	CEngineFileDialog::RunParams saveParams;
	saveParams.initialDir = PathUtil::ToGamePath(QFileInfo(startPath).dir().path());
	saveParams.title = QObject::tr("Save new material as...");
	saveParams.extensionFilters << CExtensionFilter(QStringLiteral("mtl files"), QStringLiteral("mtl"));

	return CEngineFileDialog::RunGameSave(saveParams, nullptr);
}

static ColorF ColorGammaToLinear(COLORREF col)
{
	float r = (float)GetRValue(col) / 255.0f;
	float g = (float)GetGValue(col) / 255.0f;
	float b = (float)GetBValue(col) / 255.0f;

	return ColorF((float)(r <= 0.04045 ? (r / 12.92) : pow(((double)r + 0.055) / 1.055, 2.4)),
	              (float)(g <= 0.04045 ? (g / 12.92) : pow(((double)g + 0.055) / 1.055, 2.4)),
	              (float)(b <= 0.04045 ? (b / 12.92) : pow(((double)b + 0.055) / 1.055, 2.4)));
}

static void InitializeMaterial(const string& directoryPath, CMaterial* pEditorMaterial, const FbxTool::SMaterial& material, CTextureManager* pTextureManager)
{
	SInputShaderResources& inputRes = pEditorMaterial->GetShaderResources();

	// Lighting settings of fabric (non-metal).
	// Setting specular from file might mess up rendering if it does not respect PBS conventions.
	inputRes.m_LMaterial.m_Diffuse = ColorGammaToLinear(RGB(255, 255, 255));
	inputRes.m_LMaterial.m_Specular = ColorGammaToLinear(RGB(61, 61, 61));
	inputRes.m_LMaterial.m_Smoothness = 255.0f;

	for (int i = 0; i < FbxTool::eMaterialChannelType_COUNT; ++i)
	{
		const char* szSemantic = GetTextureSemanticFromChannelType(FbxTool::EMaterialChannelType(i));
		if (!szSemantic)
		{
			continue;
		}

		const EEfResTextures texId = MaterialHelpers::FindTexSlot(szSemantic);

		CTextureManager::TextureHandle tex = nullptr;
		if (material.m_materialChannels[i].m_bHasTexture)
		{
			tex = pTextureManager->GetTextureFromSourcePath(material.m_materialChannels[i].m_textureFilename);
		}
		const bool bTexExists = tex && FileExists(PathUtil::Make(directoryPath, PathUtil::ReplaceExtension(pTextureManager->GetTargetPath(tex), "tif")));

		// If bump map is missing, lighting breaks in viewport of MeshImporter.
		if (i == FbxTool::eMaterialChannelType_Bump && !bTexExists)
		{
			inputRes.m_Textures[texId].m_Name = "%ENGINE%/EngineAssets/Textures/white_ddn.dds";
			continue;
		}

		// If a diffuse texture is missing, set diffuse color from FBX file.
		// This violates PBS conventions, but gives a result that somewhat matches expectations.
		if (i == FbxTool::eMaterialChannelType_Diffuse && !bTexExists)
		{
			inputRes.m_LMaterial.m_Diffuse = material.m_materialChannels[i].m_color;
			inputRes.m_Textures[texId].m_Name = "%ENGINE%/EngineAssets/Textures/white.dds";
			continue;
		}

		if (!bTexExists)
		{
			continue;
		}

		// Material stores path relative to game directory.
		// Assumption: Texture files are in same directory as material file.

		const string sourceFilepath = material.m_materialChannels[i].m_textureFilename;
		// const string tifFilename = PathUtil::ReplaceExtension(PathUtil::GetFile(sourceFilepath), "tif");
		const string tifFilename = PathUtil::ReplaceExtension(pTextureManager->GetTargetPath(tex), "tif");

		const string tifGamePath = PathUtil::ToGamePath(PathUtil::Make(directoryPath, tifFilename));

		inputRes.m_Textures[texId].m_Name = tifGamePath;
	}
}


} // namespace Private_CreateMaterialTask

CCreateMaterialTask::CCreateMaterialTask()
	: m_bSucceeded(false)
	, m_pUserData(nullptr)
{}

CCreateMaterialTask::~CCreateMaterialTask()
{}

void CCreateMaterialTask::SetCallback(const Callback& callback)
{
	m_callback = callback;
}

void CCreateMaterialTask::SetUserData(void* pUserData)
{
	m_pUserData = pUserData;
}

void CCreateMaterialTask::SetMaterialFilePath(const string& filePath)
{
	m_materialFilePath = filePath;
}

void CCreateMaterialTask::SetDisplayScene(const std::shared_ptr<MeshImporter::SDisplayScene>& pDisplayScene)
{
	m_pDisplayScene = pDisplayScene;
}

void CCreateMaterialTask::SetTextureManager(const std::shared_ptr<CTextureManager>& pTextureManager)
{
	m_pTextureManager = pTextureManager;
}

_smart_ptr<CMaterial> CCreateMaterialTask::GetMaterial() { return m_pMaterial; }

void*                 CCreateMaterialTask::GetUserData() { return m_pUserData; }

bool                  CCreateMaterialTask::InitializeTask()
{
	using namespace Private_CreateMaterialTask;

	FbxTool::CScene* const pScene = m_pDisplayScene->m_pScene.get();
	if (!pScene)
	{
		return false;
	}

	const int materialCount = pScene->GetMaterialCount();

	const QString sourceFilePath = m_pDisplayScene->m_pImportFile->GetFilePath();

	const int mtlFlags = materialCount > 1 ? MTL_FLAG_MULTI_SUBMTL : 0;

	if (m_materialFilePath.empty())
	{
		const QString materialName = SelectNewMaterial(sourceFilePath);
		if (materialName.isEmpty())
		{
			return false;
		}
		m_materialFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), QtUtil::ToString(materialName));

		const string materialItemName = GetMaterialNameFromFilePath(m_materialFilePath);

		CMaterialManager* const pMaterialManager = GetIEditor()->GetMaterialManager(); 
		m_pMaterial = (CMaterial*)pMaterialManager->FindItemByName(materialItemName.c_str());
		if (!m_pMaterial)
		{
			// Create new material.
			m_pMaterial = pMaterialManager->CreateMaterial(materialItemName, XmlNodeRef(), mtlFlags);
			if (!m_pMaterial)
			{
				LogPrintf("%s: Unable to create material %s\n", __FUNCTION__, materialItemName.c_str());
				return false;
			}
			m_pMaterial->Update();
			m_pMaterial->Save();
		}
	}
	else
	{
		string materialName = GetMaterialNameFromFilePath(m_materialFilePath);
		if (materialName.empty())
		{
			return false;
		}

		m_pMaterial = GetIEditor()->GetMaterialManager()->CreateMaterial(materialName, XmlNodeRef(), mtlFlags);
	}

	if (!m_pMaterial)
	{
		return false;
	}

	return true;
}

bool CCreateMaterialTask::PerformTask()
{
	using namespace Private_CreateMaterialTask;

	this->UpdateProgress(QStringLiteral("Creating material..."), -1);

	// Copy textures to target directory.
	const string copyTo = PathUtil::RemoveSlash(PathUtil::GetPathWithoutFilename(m_materialFilePath));
	m_pTextureManager->CopyTextures(copyTo);

	// Convert textures to CryTIF
	for (int i = 0; i < m_pTextureManager->GetTextureCount(); ++i)
	{
		CTextureManager::TextureHandle tex = m_pTextureManager->GetTextureFromIndex(i);
		const string rcSettings = m_pTextureManager->GetRcSettings(tex);
		if (!rcSettings.empty())
		{
			TextureHelpers::CreateCryTif(PathUtil::AddSlash(copyTo) + m_pTextureManager->GetTargetPath(tex), rcSettings);
		}
	}

	return true;
}

void CCreateMaterialTask::FinishTask(bool bTaskSucceeded)
{
	using namespace Private_CreateMaterialTask;

	m_bSucceeded = bTaskSucceeded;

	if (m_bSucceeded)
	{
		FbxTool::CScene* const pScene = m_pDisplayScene->m_pScene.get();

		const string directoryPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetPathWithoutFilename(m_pMaterial->GetFilename()));
		CTextureManager* const pTextureManager = m_pTextureManager.get();

		auto subMaterialInitializer = [&directoryPath, pTextureManager](CMaterial* pEditorMaterial, const FbxTool::SMaterial& material)
		{
			InitializeMaterial(directoryPath, pEditorMaterial, material, pTextureManager);
		};
		CreateMaterial(m_pMaterial, pScene, subMaterialInitializer);

		GetIEditor()->GetMaterialManager()->SetCurrentMaterial(m_pMaterial);
	}

	if (m_callback)
	{
		m_callback(this);
	}

	delete this;
}

