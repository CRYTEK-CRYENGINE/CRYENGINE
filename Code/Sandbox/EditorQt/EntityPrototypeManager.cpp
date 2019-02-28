// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityPrototypeManager.h"

#include "EntityPrototype.h"
#include "EntityPrototypeLibrary.h"
#include "Util/CryMemFile.h"
#include "Util/PakFile.h"
#include "Util/IndexedFiles.h"

#define ENTITY_LIBS_PATH "Libs/EntityArchetypes/"

//////////////////////////////////////////////////////////////////////////
// CEntityPrototypeManager implementation.
//////////////////////////////////////////////////////////////////////////
CEntityPrototypeManager::CEntityPrototypeManager()
{
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
CEntityPrototypeManager::~CEntityPrototypeManager()
{
}

//////////////////////////////////////////////////////////////////////////
void CEntityPrototypeManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
CEntityPrototype* CEntityPrototypeManager::LoadPrototype(CEntityPrototypeLibrary* pLibrary, XmlNodeRef& node)
{
	assert(pLibrary);
	assert(node != NULL);

	CBaseLibraryItem::SerializeContext ctx(node, true);
	ctx.bCopyPaste = true;
	ctx.bUniqName = true;

	CEntityPrototype* prototype = new CEntityPrototype;
	pLibrary->AddItem(prototype);
	prototype->Serialize(ctx);
	return prototype;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CEntityPrototypeManager::MakeNewItem()
{
	return new CEntityPrototype;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CEntityPrototypeManager::MakeNewLibrary()
{
	return new CEntityPrototypeLibrary(this);
}

//////////////////////////////////////////////////////////////////////////
string CEntityPrototypeManager::GetRootNodeName()
{
	return "EntityPrototypesLibs";
}

//////////////////////////////////////////////////////////////////////////
string CEntityPrototypeManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = ENTITY_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPrototypeManager::ExportPrototypes(const string& path, const string& levelName, CPakFile& levelPakFile)
{
	if (!m_pLevelLibrary)
		return;

	XmlNodeRef node = XmlHelpers::CreateXmlNode("EntityPrototypeLibrary");
	m_pLevelLibrary->Serialize(node, false);
	XmlString xmlData = node->getXML();

	CCryMemFile file;
	file.Write(xmlData.c_str(), xmlData.length());
	string filename = PathUtil::Make(path, "LevelPrototypes.xml");
	levelPakFile.UpdateFile(filename, file);
}

CEntityPrototype* CEntityPrototypeManager::FindPrototypeFromXMLFiles(CryGUID guid)
{
	CFileUtil::FileArray files;
	string libPath = GetLibsPath();
	std::vector<string> tags = CFileUtil::PickTagsFromPath(libPath);
	CIndexedFiles::GetDB().GetFilesWithTags(files, tags);

	auto ii = files.begin();
	for (; ii != files.end(); ++ii)
	{
		XmlNodeRef root = XmlHelpers::LoadXmlFromFile(ii->filename);
		if (!root)
		{
			continue;
		}

		for (int i = 0, iChildCount(root->getChildCount()); i < iChildCount; ++i)
		{
			XmlNodeRef child = root->getChild(i);
			CryGUID candidateGuid;
			if (!child->getAttr("Id", candidateGuid))
			{
				continue;
			}
			if (candidateGuid == guid)
			{
				LoadLibrary(ii->filename);
				return (CEntityPrototype*)FindItem(guid);
			}
		}
	}

	return NULL;
}

