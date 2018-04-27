// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TextureManager.h"
#include "TextureHelpers.h"
#include "ImporterUtil.h"
#include "DialogCommon.h" // SceneManager

#include <QDir>
#include <QFileInfo>

bool CTextureManager::AddTexture(const MeshImporter::IImportFile& importFile, const string& sourceFilePath, const string& rcSettings)
{
	auto texIt = std::find_if(m_textures.begin(), m_textures.end(), [sourceFilePath](const std::unique_ptr<STexture>& pTex)
	{
		return !pTex->sourceFilePath.compareNoCase(sourceFilePath);
	});
	if (texIt != m_textures.end())
	{
		// Filename already exists.
		return false;
	}

	std::unique_ptr<STexture> texture(new STexture());

	texture->targetFilePath = TranslateFilePath(importFile, sourceFilePath);
	texture->sourceFilePath = sourceFilePath;
	texture->rcSettings = rcSettings;

	m_textures.push_back(std::move(texture));

	return true;
}

void CTextureManager::SetRcSettings(TextureHandle tex, const string& rcSettings)
{
	tex->rcSettings = rcSettings;
}

int CTextureManager::GetTextureCount() const
{
	return m_textures.size();
}

CTextureManager::TextureHandle CTextureManager::GetTextureFromIndex(int index)
{
	CRY_ASSERT(index >= 0 && index < m_textures.size());
	return m_textures[index].get();
}

CTextureManager::TextureHandle CTextureManager::GetTextureFromSourcePath(const string& sourceFilePath)
{
	auto texIt = std::find_if(m_textures.begin(), m_textures.end(), [sourceFilePath](const std::unique_ptr<STexture>& pTex)
	{
		return !pTex->sourceFilePath.compareNoCase(sourceFilePath);
	});

	if (texIt == m_textures.end())
	{
		return nullptr;
	}

	return texIt->get();
}

string CTextureManager::GetTargetPath(TextureHandle tex) const
{
	return tex->targetFilePath;
}

string CTextureManager::GetSourcePath(TextureHandle tex) const
{
	return tex->sourceFilePath;
}

bool CTextureManager::CopyTexture(TextureHandle tex, const string& dir) const
{
	string loadPath;
	if (tex->targetFilePath == tex->sourceFilePath)
	{
		// Texture path is relative to source file.
		loadPath = m_sourceDirectory + "/" + tex->sourceFilePath;
	}
	else
	{
		// Texture path is absolute.
		loadPath = tex->sourceFilePath;
	}

	if (!FileExists(loadPath))
	{
		// Try to find texture in directory of source file.
		loadPath = PathUtil::GetFile(tex->sourceFilePath);
		if (!FileExists(loadPath))
		{
			return false;
		}
	}

	const string targetPath = dir + "/" + tex->targetFilePath;

	const string subPath = PathUtil::RemoveSlash(PathUtil::GetPathWithoutFilename(tex->targetFilePath));
	if (!subPath.empty() && !QDir(QtUtil::ToQString(dir)).mkpath(QtUtil::ToQString(subPath)))
	{
		return false;
	}
	return QFile::copy(QtUtil::ToQString(loadPath), QtUtil::ToQString(targetPath));
}

bool CTextureManager::CopyTextures(const string& dir)
{
	for (int i = 0; i < GetTextureCount(); ++i)
	{
		CTextureManager::TextureHandle tex = GetTextureFromIndex(i);
		CopyTexture(tex, dir);
	}
	return true;
}

string CTextureManager::GetRcSettings(TextureHandle tex) const
{
	return tex->rcSettings;
}

CTextureManager::STexture::STexture()
{}

string CTextureManager::TranslateFilePath(const MeshImporter::IImportFile& importFile, const string& sourceTexturePath) const
{
	return TextureHelpers::TranslateFilePath(QtUtil::ToString(importFile.GetOriginalFilePath()), QtUtil::ToString(importFile.GetFilePath()), sourceTexturePath);
}

static const char* GetPresetFromChannelType(FbxTool::EMaterialChannelType channelType)
{
	// TODO: The preset names are loaded from rc.ini. Since this file might be modified by
	// a user, the preset names should not be hard coded. Possibly use preset aliases for this.

	switch (channelType)
	{
	case FbxTool::eMaterialChannelType_Diffuse:
		return "Albedo";
	case FbxTool::eMaterialChannelType_Bump:
		return "Normals";
	case FbxTool::eMaterialChannelType_Specular:
		return "Reflectance";
	default:
		return nullptr;
	}
}

void CTextureManager::Init(const MeshImporter::CSceneManager& sceneManager)
{
	const FbxTool::CScene* const pScene = sceneManager.GetScene();

	m_sourceDirectory = QtUtil::ToString(QFileInfo(sceneManager.GetImportFile()->GetOriginalFilePath()).dir().path());
	m_textures.clear();

	for (int i = 0; i < pScene->GetMaterialCount(); ++i)
	{
		const FbxTool::SMaterial* const pMaterial = pScene->GetMaterialByIndex(i);

		for (int j = 0; j < FbxTool::eMaterialChannelType_COUNT; ++j)
		{
			const FbxTool::SMaterialChannel& channel = pMaterial->m_materialChannels[j];
			if (channel.m_bHasTexture)
			{
				string rcArguments;
				const char* szPreset = GetPresetFromChannelType(FbxTool::EMaterialChannelType(j));
				if (szPreset)
				{
					rcArguments = string("/preset=") + szPreset;
				}

				AddTexture(*sceneManager.GetImportFile(), channel.m_textureFilename, rcArguments);
			}
		}
	}
}

