// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlayerProfile.h"
#include "PlayerProfileManager.h"
#include "CryAction.h"
#include <iterator>

#include "IActionMapManager.h"
#include <CryCore/Platform/IPlatformOS.h>

const char* CPlayerProfile::ATTRIBUTES_TAG = "Attributes";
const char* CPlayerProfile::ACTIONMAPS_TAG = "ActionMaps";
const char* CPlayerProfile::VERSION_TAG = "Version";

// merge attributes with profile and default profile
class CAttributeEnumerator : public IAttributeEnumerator
{
	struct AttributeComparer
	{
		bool operator()(const CPlayerProfile::TAttributeMap::value_type& lhs, const CPlayerProfile::TAttributeMap::value_type& rhs) const
		{
			return lhs.first.compare(rhs.first) < 0;
		}
	};

public:

	CAttributeEnumerator(CPlayerProfile* pProfile) :
		m_nRefs(0)
	{
		const CPlayerProfile::TAttributeMap& localMap = pProfile->GetAttributeMap();
		const CPlayerProfile::TAttributeMap& parentMap = pProfile->GetDefaultAttributeMap();
		std::merge(localMap.begin(), localMap.end(), parentMap.begin(), parentMap.end(),
		           std::inserter(m_mergedMap, m_mergedMap.begin()), AttributeComparer());

		m_cur = m_mergedMap.begin();
		m_end = m_mergedMap.end();
	}

	bool Next(SAttributeDescription& desc)
	{
		if (m_cur != m_end)
		{
			desc.name = m_cur->first.c_str();
			++m_cur;
			return true;
		}
		desc.name = "";
		return false;
	}

	void AddRef()
	{
		++m_nRefs;
	}

	void Release()
	{
		if (0 == --m_nRefs)
			delete this;
	}

private:
	int m_nRefs;
	CPlayerProfile::TAttributeMap::iterator m_cur;
	CPlayerProfile::TAttributeMap::iterator m_end;
	CPlayerProfile::TAttributeMap           m_mergedMap;
};

//------------------------------------------------------------------------
CPlayerProfile::CPlayerProfile(CPlayerProfileManager* pManager, const char* name, const char* userId, bool bIsPreview)
	: m_pManager(pManager), m_name(name), m_userId(userId), m_bIsPreview(bIsPreview), m_attributesVersion(0)
{
}

//------------------------------------------------------------------------
CPlayerProfile::~CPlayerProfile()
{
}

//------------------------------------------------------------------------
bool CPlayerProfile::Reset()
{
	if (IsDefault())
		return false;

	// for now, try to get the action map from the IActionMapManager
	IActionMapManager* pAM = CCryAction::GetCryAction()->GetIActionMapManager();
	pAM->Reset();
	// well, not very efficient at the moment...
	TAttributeMap::iterator iter = m_attributeMap.begin();
	while (iter != m_attributeMap.end())
	{
		TAttributeMap::iterator next = iter;
		++next;
		ResetAttribute(iter->first);
		iter = next;
	}

	LoadGamerProfileDefaults();
	gEnv->pSystem->AutoDetectSpec(false);

	return true;
}

//------------------------------------------------------------------------
// is this the default profile? it cannot be modified
bool CPlayerProfile::IsDefault() const
{
	return m_pManager->GetDefaultProfile() == this;
}

//------------------------------------------------------------------------
// override values with console player profile defaults
void CPlayerProfile::LoadGamerProfileDefaults()
{
	ICVar* pSysSpec = gEnv->pConsole->GetCVar("sys_spec");
	if (pSysSpec && !(pSysSpec->GetFlags() & VF_WASINCONFIG))
	{
		gEnv->pSystem->AutoDetectSpec(true);
	}

	IPlatformOS::SUserProfileVariant preference;
	IPlatformOS::TUserName userName = GetName();
	unsigned int user;
	if (gEnv->pSystem->GetPlatformOS()->UserIsSignedIn(userName, user) && user != IPlatformOS::Unknown_User)
	{
		if (gEnv->pSystem->GetPlatformOS()->GetUserProfilePreference(user, IPlatformOS::EUPP_CONTROLLER_INVERT_Y, preference))
		{
			TFlowInputData value(preference.GetInt());
			SetAttribute("InvertY", value);
		}
		if (gEnv->pSystem->GetPlatformOS()->GetUserProfilePreference(user, IPlatformOS::EUPP_CONTROLLER_SENSITIVITY, preference))
		{
			TFlowInputData value(preference.GetFloat());
			SetAttribute("Sensitivity", value);
		}
		if (gEnv->pSystem->GetPlatformOS()->GetUserProfilePreference(user, IPlatformOS::EUPP_GAME_DIFFICULTY, preference))
		{
			TFlowInputData value(preference.GetInt());
			SetAttribute("SP/Difficulty", value);
		}
		if (gEnv->pSystem->GetPlatformOS()->GetUserProfilePreference(user, IPlatformOS::EUPP_AIM_ASSIST, preference))
		{
			TFlowInputData value(preference.GetInt());
			SetAttribute("AimAssistance", value);
		}
	}
}

//------------------------------------------------------------------------
// name of the profile
const char* CPlayerProfile::GetName()
{
	return m_name.c_str();
}

//------------------------------------------------------------------------
// Id of the profile user
const char* CPlayerProfile::GetUserId()
{
	return m_userId.c_str();
}

//------------------------------------------------------------------------
// retrieve the action map
IActionMap* CPlayerProfile::GetActionMap(const char* name)
{
	// for now, try to get the action map from the IActionMapManager
	IActionMapManager* pAM = CCryAction::GetCryAction()->GetIActionMapManager();
	return pAM->GetActionMap(name);
}

//------------------------------------------------------------------------
// set the value of an attribute
bool CPlayerProfile::SetAttribute(const char* name, const TFlowInputData& value)
{
	if (IsDefault())
		return false;

	m_attributeMap[name] = value;
	return true;
}

//------------------------------------------------------------------------
// re-set attribute to default value (basically removes it from this profile)
bool CPlayerProfile::ResetAttribute(const char* name)
{
	if (IsDefault())
		return false;

	const TAttributeMap& defaultMap = GetDefaultAttributeMap();
	// resetting means deleting from this profile and using the default value
	// but: if no entry in default map, keep it
	if (defaultMap.find(CONST_TEMP_STRING(name)) != defaultMap.end())
	{
		TAttributeMap::size_type count = m_attributeMap.erase(name);
		return count > 0;
	}
	return false;
}
//------------------------------------------------------------------------
// delete an attribute from attribute map (regardless if has a default)
void CPlayerProfile::DeleteAttribute(const char* name)
{
	m_attributeMap.erase(name);
}

//------------------------------------------------------------------------
// get the value of an attribute. if not specified optionally lookup in default profile
bool CPlayerProfile::GetAttribute(const char* name, TFlowInputData& val, bool bUseDefaultFallback) const
{
	TAttributeMap::const_iterator iter = m_attributeMap.find(CONST_TEMP_STRING(name));
	if (iter != m_attributeMap.end())
	{
		val = iter->second;
		return true;
	}
	if (bUseDefaultFallback && !IsDefault())
	{
		const TAttributeMap& defaultMap = GetDefaultAttributeMap();
		TAttributeMap::const_iterator iter2 = defaultMap.find(CONST_TEMP_STRING(name));
		if (iter2 != defaultMap.end())
		{
			val = iter2->second;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
// get name all attributes available
// all in this profile and inherited from default profile
IAttributeEnumeratorPtr CPlayerProfile::CreateAttributeEnumerator()
{
	return new CAttributeEnumerator(this);
}

// create an enumerator for all save games
ISaveGameEnumeratorPtr CPlayerProfile::CreateSaveGameEnumerator()
{
	return m_pManager->CreateSaveGameEnumerator(GetUserId(), this);
}

//------------------------------------------------------------------------
ISaveGame* CPlayerProfile::CreateSaveGame()
{
	return m_pManager->CreateSaveGame(GetUserId(), this);
}

//------------------------------------------------------------------------
bool CPlayerProfile::DeleteSaveGame(const char* name)
{
	return m_pManager->DeleteSaveGame(GetUserId(), this, name);
}

//------------------------------------------------------------------------
ILoadGame* CPlayerProfile::CreateLoadGame()
{
	return m_pManager->CreateLoadGame(GetUserId(), this);
}

ILevelRotationFile* CPlayerProfile::GetLevelRotationFile(const char* name)
{
	return m_pManager->GetLevelRotationFile(GetUserId(), this, name);
}

//------------------------------------------------------------------------
void CPlayerProfile::SetName(const char* name)
{
	m_name = name;
}

void CPlayerProfile::SetUserId(const char* userId)
{
	m_userId = userId;
}

//------------------------------------------------------------------------
bool CPlayerProfile::SerializeXML(CPlayerProfileManager::IProfileXMLSerializer* pSerializer)
{
	if (pSerializer->IsLoading())
	{
		// serialize attributes
		XmlNodeRef attributesNode = pSerializer->GetSection(CPlayerProfileManager::ePPS_Attribute);
		if (attributesNode)
		{
			CPlayerProfile* pDefaultProfile = static_cast<CPlayerProfile*>(m_pManager->GetDefaultProfile());
			int requiredVersion = 0;
			if (IsDefault() == false && pDefaultProfile)
				requiredVersion = pDefaultProfile->m_attributesVersion;
			bool ok = LoadAttributes(attributesNode, requiredVersion);
		}
		else
		{
			GameWarning("CPlayerProfile::SerializeXML: No attributes tag '%s' found", ATTRIBUTES_TAG);
			return false;
		}

		// preview profiles never load actionmaps!
		if (m_bIsPreview == false)
		{
			// serialize action maps
			XmlNodeRef actionMaps = pSerializer->GetSection(CPlayerProfileManager::ePPS_Actionmap);
			if (actionMaps)
			{
				// the default profile loaded has no associated actionmaps, but
				// rather assumes that all actionmaps which are currently loaded belong to the default
				// profile
				// if it's not the default profile to be loaded, then we load the ActionMap
				// but we do a version check. if the profile's actionmaps are outdated, it's not loaded
				// but the default action map is used instead
				// on saving the profile it's automatically updated (e.g. the current actionmaps [correct version]
				// are saved
				IActionMapManager* pAM = CCryAction::GetCryAction()->GetIActionMapManager();
				if (IsDefault() == false && pAM)
				{
					pAM->Reset();

					pAM->LoadRebindDataFromXML(actionMaps); // check version and don't load if outdated
				}
			}
			else
			{
				GameWarning("CPlayerProfile::SerializeXML: No actionmaps tag '%s' found", ACTIONMAPS_TAG);
				return false;
			}
		}
	}
	else
	{
		if (m_bIsPreview == false)
		{
			// serialize attributes
			XmlNodeRef attributesNode = pSerializer->CreateNewSection(CPlayerProfileManager::ePPS_Attribute, CPlayerProfile::ATTRIBUTES_TAG);
			bool ok = SaveAttributes(attributesNode);
			if (!ok)
				return false;

			// serialize action maps
			IActionMapManager* pAM = CCryAction::GetCryAction()->GetIActionMapManager();
			XmlNodeRef actionMapsNode = pSerializer->CreateNewSection(CPlayerProfileManager::ePPS_Actionmap, CPlayerProfile::ACTIONMAPS_TAG);
			pAM->SaveRebindDataToXML(actionMapsNode);
			return ok;
		}
	}
	return true;
}

//------------------------------------------------------------------------
const CPlayerProfile::TAttributeMap& CPlayerProfile::GetDefaultAttributeMap() const
{
	CPlayerProfile* pDefaultProfile = static_cast<CPlayerProfile*>(m_pManager->GetDefaultProfile());
	assert(pDefaultProfile != 0);
	return pDefaultProfile->GetAttributeMap();
}

//------------------------------------------------------------------------
bool CPlayerProfile::SaveAttributes(const XmlNodeRef& root)
{
	if (m_attributesVersion > 0)
		root->setAttr(VERSION_TAG, m_attributesVersion);

	const TAttributeMap& defaultMap = GetDefaultAttributeMap();
	TAttributeMap::iterator iter = m_attributeMap.begin();
	while (iter != m_attributeMap.end())
	{
		string val;
		iter->second.GetValueWithConversion(val);
		bool bSaveIt = true;
		TAttributeMap::const_iterator defaultIter = defaultMap.find(iter->first);
		if (defaultIter != defaultMap.end())
		{
			string defaultVal;
			defaultIter->second.GetValueWithConversion(defaultVal);
			// check if value is different from default
			bSaveIt = val != defaultVal;
		}
		if (m_pManager->IsOnlineOnlyAttribute(iter->first))
		{
			bSaveIt = false;
		}
		if (bSaveIt)
		{
			// TODO: config. variant saving
			XmlNodeRef child = root->newChild("Attr");
			child->setAttr("name", iter->first);
			child->setAttr("value", val);
		}
		++iter;
	}

	if (m_pManager->HasEnabledOnlineAttributes() && m_pManager->CanProcessOnlineAttributes() && !IsDefault())
	{
		m_pManager->SaveOnlineAttributes(this);
	}

	return true;
}

//------------------------------------------------------------------------
bool CPlayerProfile::LoadAttributes(const XmlNodeRef& root, int requiredVersion)
{
	int version = 0;
	const bool bHaveVersion = root->getAttr(VERSION_TAG, version);

	if (requiredVersion > 0)
	{
		if (bHaveVersion && version < requiredVersion)
		{
			GameWarning("CPlayerProfile::LoadAttributes: Attributes of profile '%s' have different version (%d != %d). Updated.", GetName(), version, requiredVersion);
			return false;
		}
		else if (!bHaveVersion)
		{
			GameWarning("CPlayerProfile::LoadAttributes: Attributes of legacy profile '%s' has no version (req=%d). Loading anyway.", GetName(), requiredVersion);
		}
		m_attributesVersion = requiredVersion;
	}
	else
		// for default profile we set the version we found in the rootNode
		m_attributesVersion = version;

	int nChilds = root->getChildCount();
	for (int i = 0; i < nChilds; ++i)
	{
		XmlNodeRef child = root->getChild(i);
		if (child && strcmp(child->getTag(), "Attr") == 0)
		{
			const char* name = child->getAttr("name");
			const char* value = child->getAttr("value");
			const char* platform = child->getAttr("platform");

			bool platformValid = true;
			if (platform != NULL && platform[0])
			{
#if CRY_PLATFORM_DURANGO
				platformValid = (strstr(platform, "xbox") != 0);
#elif CRY_PLATFORM_ORBIS
				platformValid = (strstr(platform, "ps4") != 0);
#else
				platformValid = (strstr(platform, "pc") != 0);
#endif
			}

			if (name && value && platformValid)
			{
				m_attributeMap[name] = TFlowInputData(string(value));
			}
		}
	}

	if (m_pManager->HasEnabledOnlineAttributes() && m_pManager->CanProcessOnlineAttributes() && !IsDefault())
	{
		m_pManager->LoadOnlineAttributes(this);
	}

	return true;
}

void CPlayerProfile::GetMemoryStatistics(ICrySizer* pSizer)
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_name);
	pSizer->AddObject(m_userId);
	pSizer->AddObject(m_attributeMap);
}
