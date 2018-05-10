// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PlayerProfileImplConsole.cpp
//  Created:     21/12/2009 by Alex Weighell.
//  Description: Player profile implementation for consoles which manage
//               the profile data via the OS and not via a file system
//               which may not be present.
//
//               Note:
//               Currently only the user login code is implemented though.
//               The save data is still (incorrectly) using the file system.
//
//               TODO:
//               Change the save data to use the platforms OS save data API.
//               Probably best to do this through the IPLatformOS interface.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySystem/XML/IXml.h>
#include <CryString/StringUtils.h>
#include <CryCore/Platform/IPlatformOS.h>
#include "PlayerProfileImplConsole.h"
#include "PlayerProfile.h"
#include "PlayerProfileImplRSFHelper.h"
#include "CryActionCVars.h"

using namespace PlayerProfileImpl;

void CPlayerProfileImplConsole::Release()
{
	delete this;
}

// Profile Implementation which stores most of the stuff ActionMaps/Attributes in separate files

//------------------------------------------------------------------------
CPlayerProfileImplConsole::CPlayerProfileImplConsole() : m_pMgr(0)
{
}

//------------------------------------------------------------------------
CPlayerProfileImplConsole::~CPlayerProfileImplConsole()
{
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::Initialize(CPlayerProfileManager* pMgr)
{
	m_pMgr = pMgr;
	return true;
}

//------------------------------------------------------------------------
void CPlayerProfileImplConsole::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
}

//------------------------------------------------------------------------
void CPlayerProfileImplConsole::InternalMakeFSPath(SUserEntry* pEntry, const char* profileName, string& outPath)
{
	if (pEntry)
	{
		outPath = ("%USER%/Profiles/");
	}
	else
	{
		// default profile always in game folder
		outPath = PathUtil::GetGameFolder();
		outPath.append("/Libs/Config/Profiles/");
	}
	if (profileName && *profileName)
	{
		// [Ian:11/10/10] We currently need to pass the profile name through the filename so we can figure out which user in PlatformOS.
		//if(!pEntry) // only do this for the default profile
		{
			outPath.append(profileName);
			outPath.append("/");
		}
	}
}

//------------------------------------------------------------------------
void CPlayerProfileImplConsole::InternalMakeFSSaveGamePath(SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder)
{
	assert(pEntry != 0);
	assert(profileName != 0);

	if (m_pMgr->IsSaveGameFolderShared())
	{
		outPath = m_pMgr->GetSharedSaveGameFolder();
		outPath.append("/");
		if (!bNeedFolder)
		{
			// [Ian:20/10/10] We currently need to pass the profile name through the filename so we can figure out which user in PlatformOS.
			outPath.append(profileName);
			outPath.append("_");
		}
	}
	else
	{
		outPath = "%USER%/Profiles/";
		// [Ian:20/10/10] We currently need to pass the profile name through the filename so we can figure out which user in PlatformOS.
		outPath.append(profileName);
		outPath.append("/SaveGames/");
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name, bool initialSave, int /*reason = ePR_All*/)
{
	IPlatformOS* os = gEnv->pSystem->GetPlatformOS();

	// save the profile into a specific location

	// check if it's a valid filename
	if (IsValidFilename(name) == false)
		return false;

	string path;
	InternalMakeFSPath(pEntry, name, path);

	XmlNodeRef rootNode = GetISystem()->CreateXmlNode(PROFILE_ROOT_TAG);
	rootNode->setAttr(PROFILE_NAME_TAG, name);
	CSerializerXML serializer(rootNode, false);
	pProfile->SerializeXML(&serializer);

	XmlNodeRef attributes = serializer.GetSection(CPlayerProfileManager::ePPS_Attribute);
	XmlNodeRef actionMap = serializer.GetSection(CPlayerProfileManager::ePPS_Actionmap);

	if (!rootNode->findChild("Attributes"))
		rootNode->addChild(attributes);
	if (!rootNode->findChild("ActionMaps"))
		rootNode->addChild(actionMap);

	return SaveXMLFile(path + "profile.xml", rootNode);
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name)
{
	// load the profile from a specific location

	// XML for now
	string path;
	InternalMakeFSPath(pEntry, name, path);

	XmlNodeRef rootNode = GetISystem()->CreateXmlNode(PROFILE_ROOT_TAG);
	CSerializerXML serializer(rootNode, true);

	XmlNodeRef profile = LoadXMLFile(path + "profile.xml");

	bool ok = false;
	if (profile)
	{
		XmlNodeRef attrNode = profile->findChild("Attributes");
		XmlNodeRef actionNode = profile->findChild("ActionMaps");
		if (!(attrNode && actionNode)) //default (PC) profile?
		{
			attrNode = LoadXMLFile(path + "attributes.xml");
			actionNode = LoadXMLFile(path + "actionmaps.xml");
		}

		if (attrNode && actionNode)
		{
			serializer.SetSection(CPlayerProfileManager::ePPS_Attribute, attrNode);
			serializer.SetSection(CPlayerProfileManager::ePPS_Actionmap, actionNode);

			ok = pProfile->SerializeXML(&serializer);
		}
	}

	return ok;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::LoginUser(SUserEntry* pEntry)
{
	// lookup stored profiles of the user (pEntry->userId) and fill in the pEntry->profileDesc
	// vector
	pEntry->profileDesc.clear();

	IPlatformOS* os = gEnv->pSystem->GetPlatformOS();

	unsigned int userIndex;
	bool signedIn = os->UserIsSignedIn(pEntry->userId.c_str(), userIndex);
	CryLogAlways("LoginUser::UserIsSignedIn %d\n", signedIn);

	if (signedIn)
	{
		pEntry->profileDesc.push_back(SLocalProfileInfo(pEntry->userId));

		// Check the profile data exists - if not create it
		string path;
		InternalMakeFSPath(pEntry, pEntry->userId, path);
		IPlatformOS::IFileFinderPtr fileFinder = os->GetFileFinder(userIndex);
		//this assumes there is a profile if a directory exists
		if (!fileFinder->FileExists(path))
		{
			// Create new profile based on the defaults
			CPlayerProfile* profile = m_pMgr->GetDefaultCPlayerProfile();
			string name = profile->GetName();
			profile->SetName(pEntry->userId);
			m_pMgr->LoadGamerProfileDefaults(profile);
#if !CRY_PLATFORM_DURANGO
			// Durango: no reason to overwrite user profile with default profile, mainly for that we're always online.
			SaveProfile(pEntry, profile, pEntry->userId.c_str(), true);
#endif
			profile->SetName(name);
		}
	}
	else
	{
		printf("OS No User signed in\n");
	}

	return signedIn;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::DeleteProfile(SUserEntry* pEntry, const char* name)
{
	string path;
	InternalMakeFSPath(pEntry, name, path);  // no profile name -> only path
	bool bOK = gEnv->pCryPak->RemoveDir(path.c_str(), true);
	// in case the savegame folder is shared, we have to delete the savegames from the shared folder
	if (bOK && m_pMgr->IsSaveGameFolderShared())
	{
		CPlayerProfileManager::TSaveGameInfoVec saveGameInfoVec;
		if (GetSaveGames(pEntry, saveGameInfoVec, name)) // provide alternate profileName, because pEntry->pCurrentProfile points to the active profile
		{
			CPlayerProfileManager::TSaveGameInfoVec::iterator iter = saveGameInfoVec.begin();
			CPlayerProfileManager::TSaveGameInfoVec::iterator iterEnd = saveGameInfoVec.end();
			while (iter != iterEnd)
			{
				DeleteSaveGame(pEntry, iter->name);
				++iter;
			}
		}
	}
	return bOK;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::RenameProfile(SUserEntry* pEntry, const char* newName)
{
	// TODO: only rename in the filesystem. for now save new and delete old
	const char* oldName = pEntry->pCurrentProfile->GetName();
	bool ok = SaveProfile(pEntry, pEntry->pCurrentProfile, newName);
	if (!ok)
		return false;

	// move the save games
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		sgHelper.MoveSaveGames(pEntry, oldName, newName);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		sgHelper.MoveSaveGames(pEntry, oldName, newName);
	}

	DeleteProfile(pEntry, oldName);
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::LogoutUser(SUserEntry* pEntry)
{
	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::GetSaveGames(SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGames(pEntry, outVec, altProfileName);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGames(pEntry, outVec, altProfileName);
	}
}

//------------------------------------------------------------------------
ISaveGame* CPlayerProfileImplConsole::CreateSaveGame(SUserEntry* pEntry)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.CreateSaveGame(pEntry);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.CreateSaveGame(pEntry);
	}
}

//------------------------------------------------------------------------
ILoadGame* CPlayerProfileImplConsole::CreateLoadGame(SUserEntry* pEntry)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.CreateLoadGame(pEntry);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.CreateLoadGame(pEntry);
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::DeleteSaveGame(SUserEntry* pEntry, const char* name)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.DeleteSaveGame(pEntry, name);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.DeleteSaveGame(pEntry, name);
	}
}

//------------------------------------------------------------------------
bool CPlayerProfileImplConsole::GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail)
{
	if (CPlayerProfileManager::sUseRichSaveGames)
	{
		CRichSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGameThumbnail(pEntry, saveGameName, thumbnail);
	}
	else
	{
		CCommonSaveGameHelper sgHelper(this);
		return sgHelper.GetSaveGameThumbnail(pEntry, saveGameName, thumbnail);
	}
}

ILevelRotationFile* CPlayerProfileImplConsole::GetLevelRotationFile(SUserEntry* pEntry, const char* name)
{
	CCommonSaveGameHelper sgHelper(this);
	return sgHelper.GetLevelRotationFile(pEntry, name);
}
