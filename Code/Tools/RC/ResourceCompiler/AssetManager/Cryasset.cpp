// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Cryasset.h"
#include "IConfig.h"
#include "IResCompiler.h"
#include "IAssetManager.h"
#include "StringHelpers.h"
#include "FileUtil.h"
#include "CryString/CryPath.h"
#include "CryGUIDHelper.h"
#include "IRCLog.h"

namespace Private_Cryasset
{

static string GetLastErrorString()
{
	const char szMsgBuf[1024]{};

	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		0,
		(LPSTR)&szMsgBuf,
		1024,
		NULL);

	return string(szMsgBuf);
}

}

namespace AssetManager
{

CAsset::CAsset(IResourceCompiler* pRC) : m_pRC(pRC) 
{
}

bool CAsset::New()
{
	m_xml = m_pRC->CreateXml(AssetManager::GetMetadataTag());
	return m_xml != nullptr;
}

bool CAsset::Read(const string& filename)
{
	m_xml = IsXml(filename) ? m_pRC->LoadXml(filename) : nullptr;
	return m_xml != nullptr;
}

bool CAsset::Save(const string& filename)
{
	const string tmpFilename = filename + ".$tmp$";

	// Force remove of the read only flag.
	SetFileAttributes(filename.c_str(), FILE_ATTRIBUTE_ARCHIVE);

	if (!m_xml->saveToFile(tmpFilename))
	{
		return false;
	}

	if (!MoveFileExA(tmpFilename.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{

		RCLogError("Can't rename '%s' to '%s'. Error code: %s.", tmpFilename.c_str(), filename.c_str(), Private_Cryasset::GetLastErrorString().c_str());
		remove(tmpFilename.c_str());
		return false;
	}
	return true;
}

//! Returns true if successful, otherwise returns false.
bool CAsset::SetMetadata(const SAssetMetadata& metadata)
{
	return AssetManager::WriteMetadata(m_xml, metadata);
}

//! Returns false if metadata does not exist or an error occurs, otherwise returns true.
bool CAsset::GetMetadata(SAssetMetadata& metadata) const
{
	return AssetManager::ReadMetadata(m_xml, metadata);
}

XmlNodeRef CAsset::GetMetadataRoot()
{
	return AssetManager::GetMetadataNode(m_xml);
}

//! Returns instance of CAsset if successful or metadata file does not exist, otherwise returns nullptr.
std::unique_ptr<CAsset> CAsset::Create(IResourceCompiler* pRc, const string& metadataFilename)
{
	std::unique_ptr<CAsset> pAsset(new CAsset(pRc));

	if (!FileUtil::FileExists(metadataFilename))
	{
		pAsset->New();
	}
	else if (!pAsset->Read(metadataFilename))
	{
		RCLogError("Can't read asset '%s'", metadataFilename.c_str());
		return std::unique_ptr<CAsset>();
	}

	return std::move(pAsset);
}

bool CAsset::IsXml(const string& filename)
{
	typedef std::unique_ptr<FILE, int(*)(FILE*)> file_ptr;

	file_ptr file(fopen(filename.c_str(), "rb"), fclose);

	if (!file.get())
		return false;

	static const size_t bufferSizeElements = 2048;
	char buffer[bufferSizeElements];
	while (size_t elementsRead = fread(buffer, sizeof(char), bufferSizeElements, file.get()))
	{
		for (size_t i = 0; i < elementsRead; ++i)
		{
			const int c = buffer[i];
			switch (buffer[i])
			{
			case '<':
				return true;
			case ' ':
				continue;
			case '\r':
				continue;
			case '\n':
				continue;
			case '\t':
				continue;
			default:
				return false;
			}
		}
	}
	return false;
}

}
