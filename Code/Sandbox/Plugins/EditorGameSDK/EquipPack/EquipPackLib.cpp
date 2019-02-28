// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EquipPackLib.h"
#include "EquipPack.h"
#include "GameEngine.h"
#include "Util/FileUtil.h"

#include <CrySandbox/IEditorGame.h>

#include "UIEnumsDatabase.h"

#define EQUIPMENTPACKS_PATH "/Libs/EquipmentPacks/"

CEquipPackLib CEquipPackLib::s_rootEquips;

CEquipPackLib::CEquipPackLib()
{
}

CEquipPackLib::~CEquipPackLib()
{
	Reset();
}

CEquipPack* CEquipPackLib::CreateEquipPack(const string& name)
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter != m_equipmentPacks.end())
		return 0;

	CEquipPack* pPack = new CEquipPack(this);
	if (!pPack)
		return 0;
	pPack->SetName(name);
	m_equipmentPacks.insert(TEquipPackMap::value_type(name, pPack));

	UpdateEnumDatabase();

	return pPack;
}

// currently we ignore the bDeleteFromDisk.
// will have to be manually removed via Source control
bool CEquipPackLib::RemoveEquipPack(const string& name, bool /* bDeleteFromDisk */)
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter == m_equipmentPacks.end())
		return false;
	delete iter->second;
	m_equipmentPacks.erase(iter);

#if 0
	if (bDeleteFromDisk)
	{
		string path = PathUtil::GetGameFolder() + EQUIPMENTPACKS_PATH;
		path += name;
		path += ".xml";
		bool bOK = CFileUtil::OverwriteFile(path);
		if (bOK)
		{
			::DeleteFile(path);
		}
	}
#endif

	UpdateEnumDatabase();

	return true;
}

CEquipPack* CEquipPackLib::FindEquipPack(const string& name)
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter == m_equipmentPacks.end())
		return 0;
	return iter->second;
}

bool CEquipPackLib::RenameEquipPack(const string& name, const string& newName)
{
	TEquipPackMap::iterator iter = m_equipmentPacks.find(name);
	if (iter == m_equipmentPacks.end())
		return false;

	CEquipPack* pPack = iter->second;
	pPack->SetName(newName);
	m_equipmentPacks.erase(iter);
	m_equipmentPacks.insert(TEquipPackMap::value_type(newName, pPack));

	UpdateEnumDatabase();

	return true;
}

bool CEquipPackLib::LoadLibs(bool bExportToGame)
{
	IEquipmentSystemInterface* pESI = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface();
	if (bExportToGame && pESI)
	{
		pESI->DeleteAllEquipmentPacks();
	}

	string path = PathUtil::GetGameFolder() + EQUIPMENTPACKS_PATH;
	// std::vector<string> files;
	// CFileEnum::ScanDirectory(path, "*.xml", files);

	std::vector<CFileUtil::FileDesc> files;
	CFileUtil::ScanDirectory(path, "*.xml", files, false);

	for (int iFile = 0; iFile < files.size(); ++iFile)
	{
		string filename = path;
		filename += files[iFile].filename;
		// CryLogAlways("Filename '%s'", filename);

		XmlNodeRef node = XmlHelpers::LoadXmlFromFile(filename);
		if (node == 0)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CEquipPackLib:LoadLibs: Cannot load pack from file '%s'", filename.GetString());
			continue;
		}

		if (node->isTag("EquipPack") == false)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CEquipPackLib:LoadLibs: File '%s' is not an equipment pack. Skipped.", filename.GetString());
			continue;
		}

		string packName;
		if (!node->getAttr("name", packName))
			packName = "Unnamed";

		CEquipPack* pPack = CreateEquipPack(packName);
		if (pPack == 0)
			continue;
		pPack->Load(node);
		pPack->SetModified(false);

		if (bExportToGame && pESI)
		{
			pESI->LoadEquipmentPack(node);
		}
	}

	return true;
}

bool CEquipPackLib::SaveLibs(bool bExportToGame)
{
	IEquipmentSystemInterface* pESI = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface();
	if (bExportToGame && pESI)
	{
		pESI->DeleteAllEquipmentPacks();
	}

	string libsPath = PathUtil::GetGameFolder() + EQUIPMENTPACKS_PATH;
	if (!libsPath.IsEmpty())
		CFileUtil::CreateDirectory(libsPath);

	for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
	{
		CEquipPack* pPack = iter->second;
		XmlNodeRef packNode = XmlHelpers::CreateXmlNode("EquipPack");
		pPack->Save(packNode);
		if (pPack->IsModified())
		{
			string path(libsPath);
			path += iter->second->GetName();
			path += ".xml";
			bool bOK = XmlHelpers::SaveXmlNode(packNode, path.GetString());
			if (bOK)
				pPack->SetModified(false);
		}
		if (bExportToGame && pESI)
		{
			pESI->LoadEquipmentPack(packNode);
		}
	}
	return true;
}

void CEquipPackLib::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bResetWhenLoad)
{
	if (!xmlNode)
		return;
	if (bLoading)
	{
		if (bResetWhenLoad)
			Reset();
		XmlNodeRef equipXMLNode = xmlNode->findChild("EquipPacks");
		if (equipXMLNode)
		{
			for (int i = 0; i < equipXMLNode->getChildCount(); ++i)
			{
				XmlNodeRef node = equipXMLNode->getChild(i);
				if (node->isTag("EquipPack") == false)
					continue;
				string packName;
				if (!node->getAttr("name", packName))
				{
					CryLog("Warning: Unnamed EquipPack found !");
					packName = "Unnamed";
					node->setAttr("name", packName);
				}
				CEquipPack* pCurPack = CreateEquipPack(packName);
				if (!pCurPack)
				{
					CryLog("Warning: Unable to create EquipPack %s !", packName.GetString());
					continue;
				}
				pCurPack->Load(node);
				pCurPack->SetModified(false);
				GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface()->LoadEquipmentPack(node);
			}
		}
	}
	else
	{
		XmlNodeRef equipXMLNode = xmlNode->newChild("EquipPacks");
		for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
		{
			XmlNodeRef packNode = equipXMLNode->newChild("EquipPack");
			iter->second->Save(packNode);
		}
	}
}

void CEquipPackLib::Reset()
{
	for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
	{
		delete iter->second;
	}
	m_equipmentPacks.clear();
}

void CEquipPackLib::UpdateEnumDatabase()
{
	CUIEnumsDatabase* enumDB = GetIEditor()->GetUIEnumsDatabase();

	std::vector<string> equipmentItems;

	for (auto iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
	{
		equipmentItems.push_back(iter->first);
	}

	enumDB->SetEnumStrings("SandboxEquipmentDatabase", equipmentItems);
}

void CEquipPackLib::ExportToGame()
{
	IEquipmentSystemInterface* pESI = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface();
	if (pESI)
	{
		pESI->DeleteAllEquipmentPacks();
		for (TEquipPackMap::iterator iter = m_equipmentPacks.begin(); iter != m_equipmentPacks.end(); ++iter)
		{
			XmlNodeRef packNode = XmlHelpers::CreateXmlNode("EquipPack");
			iter->second->Save(packNode);
			pESI->LoadEquipmentPack(packNode);
		}
	}
}

