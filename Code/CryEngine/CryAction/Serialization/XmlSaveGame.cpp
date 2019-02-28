// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "XmlSaveGame.h"
#include "XmlSerializeHelper.h"

#include <CryCore/Platform/IPlatformOS.h>

static const int XML_SAVEGAME_MAX_CHUNK_SIZE = 32767 / 2;

struct CXmlSaveGame::Impl
{
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
		pSizer->AddObject(sections);
		pSizer->AddObject(root);
		pSizer->AddObject(metadata);
		pSizer->AddObject(outputFile);
	}

	XmlNodeRef root;
	XmlNodeRef metadata;
	string     outputFile;

	typedef std::vector<_smart_ptr<CXmlSerializeHelper>> TContexts;
	TContexts sections;
};

CXmlSaveGame::CXmlSaveGame() : m_pImpl(new Impl), m_eReason(eSGR_QuickSave)
{
}

CXmlSaveGame::~CXmlSaveGame()
{
}

bool CXmlSaveGame::Init(const char* name)
{
	m_pImpl->outputFile = name;
	m_pImpl->root = GetISystem()->CreateXmlNode("SaveGame", true);
	m_pImpl->metadata = m_pImpl->root->createNode("Metadata");
	m_pImpl->root->addChild(m_pImpl->metadata);
	return true;
}

XmlNodeRef CXmlSaveGame::GetMetadataXmlNode() const
{
	return m_pImpl->metadata;
}

void CXmlSaveGame::AddMetadata(const char* tag, const char* value)
{
	m_pImpl->metadata->setAttr(tag, value);
}

void CXmlSaveGame::AddMetadata(const char* tag, int value)
{
	m_pImpl->metadata->setAttr(tag, value);
}

TSerialize CXmlSaveGame::AddSection(const char* section)
{
	XmlNodeRef node = m_pImpl->root->createNode(section);
	m_pImpl->root->addChild(node);

	_smart_ptr<CXmlSerializeHelper> pSerializer = new CXmlSerializeHelper;
	m_pImpl->sections.push_back(pSerializer);

	return TSerialize(pSerializer->GetWriter(node));
}

uint8* CXmlSaveGame::SetThumbnail(const uint8* imageData, int width, int height, int depth)
{
	return 0;
}

bool CXmlSaveGame::SetThumbnailFromBMP(const char* filename)
{
	return false;
}

bool CXmlSaveGame::Complete(bool successfulSoFar)
{
	if (successfulSoFar)
	{
		successfulSoFar &= Write(m_pImpl->outputFile.c_str(), m_pImpl->root);
	}
	delete this;
	return successfulSoFar;
}

const char* CXmlSaveGame::GetFileName() const
{
	if (m_pImpl.get())
		return m_pImpl.get()->outputFile;
	return NULL;
}

void CXmlSaveGame::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_pImpl);
}

bool CXmlSaveGame::Write(const char* filename, XmlNodeRef data)
{
#if CRY_PLATFORM_WINDOWS
	CrySetFileAttributes(filename, 0x00000080);    // FILE_ATTRIBUTE_NORMAL
#endif

	// Try opening file for creation first, will also create directory.
	FILE* pFile = NULL;

	if (!GetISystem()->GetPlatformOS()->UsePlatformSavingAPI())
	{
		pFile = gEnv->pCryPak->FOpen(filename, "wb");
		if (!pFile)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Failed to create file %s", filename);
			return false;
		}
	}

	const bool bSavedToFile = data->saveToFile(filename, XML_SAVEGAME_MAX_CHUNK_SIZE, pFile);
	if (pFile)
	{
		gEnv->pCryPak->FClose(pFile);
	}
	return bSavedToFile;
}
