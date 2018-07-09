// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "AnimationDatabaseManager.h"
#include <CryCore/TypeInfo_impl.h>
#include "Xml/TagDefinitionXml.h"
#include "ProceduralClipConversion.h"
#include "MannequinUtils.h"

#include "Mannequin/Serialization.h"
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/File/IResourceManager.h>

const string MANNEQUIN_FOLDER = "Animations/Mannequin/ADB/";

CAnimationDatabaseManager* CAnimationDatabaseManager::s_Instance = NULL;

static void NormalizeFilename(char (&outFilename)[DEF_PATH_LENGTH], const char* inFilename)
{
	cry_strcpy(outFilename, DEF_PATH_LENGTH, inFilename);
	for (size_t i = 0; outFilename[i]; ++i)
	{
		if (outFilename[i] == '\\')
			outFilename[i] = '/';
	}
}

static bool IsValidNameIdentifier(const char* const name)
{
	const char* cit = name;
	if (!cit)
	{
		return false;
	}

	if (!isalpha(static_cast<unsigned char>(*cit)) && !(*cit == '_'))
	{
		return false;
	}
	cit++;

	while (*cit)
	{
		const char c = *cit;
		if (!isalnum(static_cast<unsigned char>(c)) && !(c == '_'))
		{
			return false;
		}
		cit++;
	}

	return true;
}

CAnimationDatabaseLibrary::CAnimationDatabaseLibrary()
{
	const char* ANIM_FLAGS[] =
	{
		"ManualUpdate",
		"Loop",
		"RepeatLast",
		"TimeWarping",
		"StartAtKeyTime",
		"StartAfter",
		"Idle2Move",
		"Move2Idle",
		"AllowRestart",
		"Sample30Hz",
		"DisableMultilayer",
		"ForceSkelUpdate",
		"TrackView",
		"RemoveFromFIFO",
		"FullRootPriority",
		"ForceTransitionToAnim"
	};
	const int NUM_ANIM_FLAGS = CRY_ARRAY_COUNT(ANIM_FLAGS);

	for (int i = 0; i < NUM_ANIM_FLAGS; i++)
	{
		m_animFlags.AddTag(ANIM_FLAGS[i]);
	}

	m_animFlags.AssignBits();

	const char* TRANSITION_FLAGS[] =
	{
		"Cyclic",
		"CycleLocked",
		"Outro"
	};
	const int NUM_TRANSITION_FLAGS = CRY_ARRAY_COUNT(TRANSITION_FLAGS);
	for (int i = 0; i < NUM_TRANSITION_FLAGS; i++)
	{
		m_transitionFlags.AddTag(TRANSITION_FLAGS[i]);
	}

	m_transitionFlags.AssignBits();
}

CAnimationDatabaseLibrary::~CAnimationDatabaseLibrary()
{
}

void CAnimationDatabaseLibrary::Insert(uint32 nCrc, CAnimationDatabase* pDb)
{
	m_databases.insert(std::make_pair(nCrc, pDb));
}

CAnimationDatabaseLibrary::THandle CAnimationDatabaseLibrary::LoadResource(const char* name, uint32 nFlags)
{
	THandle ret = TryOpenResource(name, nFlags);

	if (!ret)
	{
		CreateResource(ret, name, nFlags);
		if (ret)
			PublishResource(ret);
	}

	return ret;
}

CAnimationDatabaseLibrary::THandle CAnimationDatabaseLibrary::TryOpenResource(const char* name, uint32 nFlags)
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, name);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	TAnimDatabaseList::const_iterator iter = m_databases.find(crc32);

	THandle res = NULL;
	if (iter != m_databases.end())
		res = iter->second;

	return res;
}

void CAnimationDatabaseLibrary::CreateResource(THandle& hdlOut, const char* name, uint32 nFlags)
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, name);

	CAnimationDatabase* pDb = new CAnimationDatabase(normalizedFilename);
	XmlNodeRef xml = gEnv->pSystem->GetXmlUtils()->LoadXmlFromFile(normalizedFilename);
	if (!xml || !LoadDatabase(xml, *pDb, true))
	{
		delete pDb;
		pDb = NULL;
	}

	hdlOut = pDb;
}

void CAnimationDatabaseLibrary::PublishResource(THandle& hdl)
{
#ifndef _RELEASE
	if (!hdl)
		__debugbreak();
#endif

	uint32 crc = CCrc32::ComputeLowercase(hdl->GetFilename());
	Insert(crc, const_cast<CAnimationDatabase*>(static_cast<const CAnimationDatabase*>(hdl)));
}

CAnimationTagDefLibrary::CAnimationTagDefLibrary()
{
}

CAnimationTagDefLibrary::~CAnimationTagDefLibrary()
{
}

void CAnimationTagDefLibrary::Insert(uint32 nCrc, CTagDefinition* pDef)
{
	m_tagDefs.insert(std::make_pair(nCrc, pDef));
}

CAnimationTagDefLibrary::THandle CAnimationTagDefLibrary::LoadResource(const char* name, uint32 nFlags)
{
	THandle ret = TryOpenResource(name, nFlags);

	if (!ret)
	{
		CreateResource(ret, name, nFlags);
		if (ret)
			PublishResource(ret);
	}

	return ret;
}

CAnimationTagDefLibrary::THandle CAnimationTagDefLibrary::TryOpenResource(const char* name, uint32 nFlags)
{
	THandle res = NULL;

	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, name);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	TTagDefList::const_iterator iter = m_tagDefs.find(crc32);
	if (iter != m_tagDefs.end())
		res = iter->second;

	return res;
}

void CAnimationTagDefLibrary::CreateResource(THandle& hdlOut, const char* name, uint32 nFlags)
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, name);

	CTagDefinition* pDef = new CTagDefinition(normalizedFilename);

	if (!mannequin::LoadTagDefinition(normalizedFilename, *pDef, (nFlags & eTDF_Tags) != 0))
	{
		delete pDef;
		pDef = NULL;
	}

	hdlOut = pDef;
}

void CAnimationTagDefLibrary::PublishResource(THandle& hdlOut)
{
#ifndef _RELEASE
	if (!hdlOut)
		__debugbreak();
#endif

	uint32 crc32 = CCrc32::ComputeLowercase(hdlOut->GetFilename());
	Insert(crc32, const_cast<CTagDefinition*>(hdlOut));
}

CAnimationControllerDefLibrary::CAnimationControllerDefLibrary()
{
}

CAnimationControllerDefLibrary::~CAnimationControllerDefLibrary()
{
}

void CAnimationControllerDefLibrary::Insert(uint32 crc32, SControllerDef* pDef)
{
	m_controllerDefs.insert(std::make_pair(crc32, pDef));
}

CAnimationControllerDefLibrary::THandle CAnimationControllerDefLibrary::LoadResource(const char* name, uint32 nFlags)
{
	THandle ret = TryOpenResource(name, nFlags);

	if (!ret)
	{
		CreateResource(ret, name, nFlags);
		if (ret)
			PublishResource(ret);
	}

	return ret;
}

CAnimationControllerDefLibrary::THandle CAnimationControllerDefLibrary::TryOpenResource(const char* name, uint32 nFlags)
{
	THandle res = NULL;

	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, name);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	TControllerDefList::const_iterator iter = m_controllerDefs.find(crc32);

	if (iter != m_controllerDefs.end())
		res = iter->second;

	return res;
}

void CAnimationControllerDefLibrary::CreateResource(THandle& hdlOut, const char* name, uint32 nFlags)
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, name);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	SControllerDef* pDef = NULL;
	XmlNodeRef xml = gEnv->pSystem->GetXmlUtils()->LoadXmlFromFile(normalizedFilename);
	if (xml)
	{
		pDef = LoadControllerDef(xml, normalizedFilename);
		if (pDef)
			pDef->m_filename.SetByString(normalizedFilename);
	}

	hdlOut = pDef;
}

void CAnimationControllerDefLibrary::PublishResource(THandle& hdl)
{
#ifndef _RELEASE
	if (!hdl)
		__debugbreak();
#endif

	uint32 crc32 = hdl->m_filename.crc;
	Insert(crc32, const_cast<SControllerDef*>(hdl));
}

CAnimationDatabaseManager::CAnimationDatabaseManager()
	: m_editorListenerSet(1)
{
	const char* FRAGMENT_FLAGS[] =
	{
		"Persistent",
		"AutoReinstall"
	};
	const int NUM_FRAGMENT_FLAGS = CRY_ARRAY_COUNT(FRAGMENT_FLAGS);
	for (int i = 0; i < NUM_FRAGMENT_FLAGS; i++)
	{
		m_fragmentFlags.AddTag(FRAGMENT_FLAGS[i]);
	}

	m_fragmentFlags.AssignBits();
}

CAnimationDatabaseManager::~CAnimationDatabaseManager()
{
	UnloadAll();
}

const IAnimationDatabase* CAnimationDatabaseManager::FindDatabase(uint32 crcFilename) const
{
	return stl::find_in_map(m_databases, crcFilename, NULL);
}

const IAnimationDatabase* CAnimationDatabaseManager::Load(const char* databaseName)
{
	//Don't load if there is no database
	if (databaseName == nullptr || strlen(databaseName) == 0)
	{
		return nullptr;
	}

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Mannequin, 0, "Load ADB: %s", databaseName);

	const IAnimationDatabase* pDb = CAnimationDatabaseLibrary::LoadResource(databaseName, 0);
	return pDb;
}

IAnimationDatabase* CAnimationDatabaseManager::Create(const char* filename, const char* defFilename)
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, filename);

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Mannequin, 0, "Create ADB: %s", normalizedFilename);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	TAnimDatabaseList::const_iterator iter = m_databases.find(crc32);

	if (iter != m_databases.end())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "!Attempting to create ADB file %s which already exists", normalizedFilename);
		return NULL;
	}
	else
	{
		char normalizedDefFilename[DEF_PATH_LENGTH];
		NormalizeFilename(normalizedDefFilename, defFilename);

		const SControllerDef* def = LoadControllerDef(normalizedDefFilename);

		if (def)
		{
			CAnimationDatabase* animDB = new CAnimationDatabase();
			m_databases.insert(std::make_pair(crc32, animDB));

			const uint32 numActions = def->m_fragmentIDs.GetNum();

			animDB->m_filename = normalizedFilename;
			animDB->m_pFragDef = &def->m_fragmentIDs;
			animDB->m_pTagDef = &def->m_tags;
			animDB->m_fragmentList.resize(numActions);
			for (uint32 i = 0; i < numActions; ++i)
			{
				animDB->m_fragmentList[i] = new CAnimationDatabase::SFragmentEntry();
			}

			Save(*animDB, normalizedFilename);

			return animDB;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "!Failed to load def file %s whilst creating ADB file %s", normalizedDefFilename, normalizedFilename);

			return NULL;
		}
	}
}

CTagDefinition* CAnimationDatabaseManager::CreateTagDefinition(const char* filename)
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, filename);

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Mannequin, 0, "Create TagDefinition: %s", normalizedFilename);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	CTagDefinition* tagDef = new CTagDefinition(normalizedFilename);

	mannequin::SaveTagDefinition(*tagDef);

	m_tagDefs.insert(std::make_pair(crc32, tagDef));

	return tagDef;
}

const SControllerDef* CAnimationDatabaseManager::LoadControllerDef(const char* filename)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Mannequin, 0, "Load ControllerDef: %s", filename);

	const SControllerDef* const pDef = CAnimationControllerDefLibrary::LoadResource(filename, 0);
	return pDef;
}

const SControllerDef* CAnimationDatabaseManager::FindControllerDef(const uint32 crcFilename) const
{
	return stl::find_in_map(m_controllerDefs, crcFilename, NULL);
}

const SControllerDef* CAnimationDatabaseManager::FindControllerDef(const char* filename) const
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, filename);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	return FindControllerDef(crc32);
}

const CTagDefinition* CAnimationDatabaseManager::LoadTagDefs(const char* filename, bool isTags)
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, filename);

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Mannequin, 0, "Load TagDefs: %s", normalizedFilename);

	const CTagDefinition* pTagDef = CAnimationTagDefLibrary::LoadResource(filename, isTags ? eTDF_Tags : 0);
	return pTagDef;
}

const CTagDefinition* CAnimationDatabaseManager::FindTagDef(const uint32 crcFilename) const
{
	return stl::find_in_map(m_tagDefs, crcFilename, NULL);
}

const CTagDefinition* CAnimationDatabaseManager::FindTagDef(const char* filename) const
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, filename);

	uint32 crc32 = CCrc32::ComputeLowercase(normalizedFilename);

	return FindTagDef(crc32);
}

void CAnimationDatabaseManager::RemoveDataFromParent(CAnimationDatabase* parentADB, const CAnimationDatabase::SSubADB* subADB)
{
	// Clear out fragments that came from subAnimDB
	FragmentID fragID = 0;
	for (CAnimationDatabase::TFragmentList::const_iterator it = parentADB->m_fragmentList.begin(), itEnd = parentADB->m_fragmentList.end(); it != itEnd; ++it, ++fragID)
	{
		if (CanSaveFragmentID(fragID, *parentADB, subADB))
		{
			if (CAnimationDatabase::SFragmentEntry* entry = *it)
			{
				for (int32 k = entry->tagSetList.Size() - 1; k >= 0; k--)
				{
					bool bDummy;
					if (ShouldSaveFragment(fragID, entry->tagSetList.m_keys[k].globalTags, *parentADB, subADB, bDummy))
					{
						entry->tagSetList.Erase(k);
					}
				}
			}
		}
	}

	// Clear out blends that came from subAnimDB
	for (CAnimationDatabase::TFragmentBlendDatabase::iterator iter = parentADB->m_fragmentBlendDB.begin(), iterEnd = parentADB->m_fragmentBlendDB.end(); iter != iterEnd; ++iter)
	{
		if (CanSaveFragmentID(iter->first.fragFrom, *parentADB, subADB) && CanSaveFragmentID(iter->first.fragTo, *parentADB, subADB))
		{
			for (int32 v = iter->second.variantList.size() - 1; v >= 0; v--)
			{
				CAnimationDatabase::SFragmentBlendVariant& variant = iter->second.variantList[v];
				bool bDummy;
				if (ShouldSaveTransition(iter->first.fragFrom, iter->first.fragTo, variant.tagsFrom.globalTags, variant.tagsTo.globalTags, *parentADB, subADB, bDummy))
				{
					for (CAnimationDatabase::TFragmentBlendList::iterator fragIt = variant.blendList.begin(), fragItEnd = variant.blendList.end(); fragIt != fragItEnd; ++fragIt)
					{
						delete fragIt->pFragment;
					}
					iter->second.variantList.erase(iter->second.variantList.begin() + v);
				}
			}
		}
	}
}

void CAnimationDatabaseManager::RevertSubADB(const char* szFilename, CAnimationDatabase* animDB, const CAnimationDatabase::SSubADB& subADB)
{
	// Check the subADBs of animDB
	if (stricmp(szFilename, subADB.filename) == 0)
	{
		RemoveDataFromParent(animDB, &subADB);

		XmlNodeRef xmlData = GetISystem()->LoadXmlFromFile(subADB.filename);
		LoadDatabaseData(*animDB, xmlData, animDB->GetFragmentDefs(), animDB->GetTagDefs(), subADB.tags, false);
	}
	for (CAnimationDatabase::TSubADBList::const_iterator adbIt = subADB.subADBs.begin(); adbIt != subADB.subADBs.end(); ++adbIt)
		RevertSubADB(szFilename, animDB, *adbIt);
}

void CAnimationDatabaseManager::RevertDatabase(const char* szFilename)
{
	for (TAnimDatabaseList::const_iterator it = m_databases.begin(), itEnd = m_databases.end(); it != itEnd; ++it)
	{
		CAnimationDatabase* animDB = it->second;

		animDB->SetAutoSort(false);

		for (CAnimationDatabase::TSubADBList::const_iterator adbIt = animDB->m_subADBs.begin(); adbIt != animDB->m_subADBs.end(); ++adbIt)
		{
			const CAnimationDatabase::SSubADB& subADB = *adbIt;
			RevertSubADB(szFilename, animDB, subADB);
		}

		// Are we reverting animDB itself?
		if (stricmp(szFilename, animDB->GetFilename()) == 0)
		{
			RemoveDataFromParent(animDB, NULL);

			XmlNodeRef xmlData = GetISystem()->LoadXmlFromFile(animDB->GetFilename());
			LoadDatabaseData(*animDB, xmlData,
			                 animDB->GetFragmentDefs(), animDB->GetTagDefs(),
			                 TAG_STATE_EMPTY, false);
		}

		animDB->Sort();
		animDB->SetAutoSort(true);
	}
}

void CAnimationDatabaseManager::ClearDatabase(CAnimationDatabase* pDatabase)
{
	// Clear out the fragment list
	std::for_each(pDatabase->m_fragmentList.begin(), pDatabase->m_fragmentList.end(), stl::container_object_deleter());
	pDatabase->m_fragmentList.clear();

	// Clear out the blends
	for (CAnimationDatabase::TFragmentBlendDatabase::iterator blDbIt = pDatabase->m_fragmentBlendDB.begin(), blDbItEnd = pDatabase->m_fragmentBlendDB.end(); blDbIt != blDbItEnd; ++blDbIt)
	{
		CAnimationDatabase::SFragmentBlendEntry& entry = blDbIt->second;
		for (CAnimationDatabase::TFragmentVariantList::iterator vrIt = entry.variantList.begin(), vrItEnd = entry.variantList.end(); vrIt != vrItEnd; ++vrIt)
		{
			CAnimationDatabase::SFragmentBlendVariant& variant = *vrIt;
			for (CAnimationDatabase::TFragmentBlendList::iterator fragIt = variant.blendList.begin(), fragItEnd = variant.blendList.end(); fragIt != fragItEnd; ++fragIt)
			{
				delete fragIt->pFragment;
			}
		}
	}
	pDatabase->m_fragmentBlendDB.clear();
}

void CAnimationDatabaseManager::ReloadDatabase(IAnimationDatabase* pDatabase)
{
	CAnimationDatabase* animDB = (CAnimationDatabase*)pDatabase;

	ClearDatabase(animDB);

	// Clear out the subADBs
	stl::free_container(animDB->m_subADBs);

	// Reload the database and all subDB data
	if (XmlNodeRef xmlData = GetISystem()->LoadXmlFromFile(animDB->m_filename.c_str()))
	{
		LoadDatabase(xmlData, *animDB, true);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not load xml file '%s'", animDB->m_filename.c_str());
	}
}

void CAnimationDatabaseManager::RevertControllerDef(const char* szFilename)
{
	if (SControllerDef* controllerDef = const_cast<SControllerDef*>(FindControllerDef(szFilename)))
	{
		ReloadControllerDef(controllerDef);
	}
}

void CAnimationDatabaseManager::ReloadControllerDef(SControllerDef* pControllerDef)
{
	TDefPathString defFilename = pControllerDef->m_filename;

	if (XmlNodeRef xmlData = GetISystem()->LoadXmlFromFile(defFilename.c_str()))
	{
		if (SControllerDef* newControllerDef = CAnimationControllerDefLibrary::LoadControllerDef(xmlData, defFilename.c_str()))
		{
			new(pControllerDef) SControllerDef(*newControllerDef);
			pControllerDef->m_filename = defFilename;
			delete newControllerDef;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Error in reloading xml file %s", defFilename.c_str());
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not load xml file %s", defFilename.c_str());
	}
}

void CAnimationDatabaseManager::RevertTagDef(const char* szFilename)
{
	if (CTagDefinition* tagDef = const_cast<CTagDefinition*>(FindTagDef(szFilename)))
	{
		ReloadTagDefinition(tagDef);
	}
}

void CAnimationDatabaseManager::ReloadTagDefinition(CTagDefinition* pTagDefinition)
{
	assert(pTagDefinition);
	pTagDefinition->Clear();

	const char* const filename = pTagDefinition->GetFilename();
	mannequin::LoadTagDefinition(filename, *pTagDefinition, pTagDefinition->HasMasks());

	NotifyListenersTagDefinitionChanged(*pTagDefinition);
}

void CAnimationDatabaseManager::ReloadAll()
{
	for (TTagDefList::const_iterator iter = m_tagDefs.begin(), iterEnd = m_tagDefs.end(); iter != iterEnd; ++iter)
	{
		CTagDefinition* tagDef = iter->second;
		ReloadTagDefinition(tagDef);
	}

	for (TControllerDefList::iterator iter = m_controllerDefs.begin(), iterEnd = m_controllerDefs.end(); iter != iterEnd; ++iter)
	{
		SControllerDef* controllerDef = iter->second;
		ReloadControllerDef(controllerDef);
	}

	for (TAnimDatabaseList::const_iterator iter = m_databases.begin(), iterEnd = m_databases.end(); iter != iterEnd; ++iter)
	{
		CAnimationDatabase* animDB = iter->second;
		ReloadDatabase(animDB);
	}
}

void CAnimationDatabaseManager::UnloadAll()
{
	for (TAnimDatabaseList::iterator it = m_databases.begin(), itEnd = m_databases.end(); it != itEnd; ++it)
		delete it->second;
	for (TControllerDefList::iterator it = m_controllerDefs.begin(), itEnd = m_controllerDefs.end(); it != itEnd; ++it)
		delete it->second;
	for (TTagDefList::iterator it = m_tagDefs.begin(), itEnd = m_tagDefs.end(); it != itEnd; ++it)
		delete it->second;

	m_databases.clear();
	m_controllerDefs.clear();
	m_tagDefs.clear();

	mannequin::OnDatabaseManagerUnload();
}

void CAnimationDatabaseManager::GetAffectedFragmentsString(const CTagDefinition* pQueryTagDef, TagID tagID, char* buffer, int bufferSize)
{
	string filename;
	string temp;
	string matchingFragments;
	uint pos = 0;

	for (TAnimDatabaseList::const_iterator iter = m_databases.begin(); iter != m_databases.end(); ++iter)
	{
		CAnimationDatabase* pCurrentDatabase = iter->second;
		const CTagDefinition& fragmentDefs = pCurrentDatabase->GetFragmentDefs();
		bool isGlobalTag = (&pCurrentDatabase->GetTagDefs() == pQueryTagDef);
		bool filenameAdded = false;

		filename = pCurrentDatabase->GetFilename();
		filename = filename.substr(MANNEQUIN_FOLDER.length());

		const uint32 numFragmentDefs = fragmentDefs.GetNum();
		for (uint32 fragIndex = 0; fragIndex < numFragmentDefs; ++fragIndex)
		{
			if (isGlobalTag || (fragmentDefs.GetSubTagDefinition(fragIndex) == pQueryTagDef))
			{
				const uint32 numTagSets = pCurrentDatabase->GetTotalTagSets(fragIndex);
				for (uint32 tagSetIndex = 0; tagSetIndex < numTagSets; ++tagSetIndex)
				{
					SFragTagState tagState;
					uint32 numOptions = pCurrentDatabase->GetTagSetInfo(fragIndex, tagSetIndex, tagState);
					if (isGlobalTag)
					{
						if (pQueryTagDef->IsSet(tagState.globalTags, tagID))
						{
							if (!filenameAdded)
							{
								matchingFragments.TrimRight(',');
								temp.Format("\n[%s]:", filename.c_str());
								matchingFragments += temp;
								filenameAdded = true;
							}
							CryStackStringT<char, 128> sGlobalTags;
							pCurrentDatabase->GetTagDefs().FlagsToTagList(tagState.globalTags, sGlobalTags);
							temp.Format(" %s (%s),", fragmentDefs.GetTagName(fragIndex), sGlobalTags.c_str());
							matchingFragments += temp;
						}
					}
					else
					{
						if (pQueryTagDef->IsSet(tagState.fragmentTags, tagID))
						{
							if (!filenameAdded)
							{
								matchingFragments.TrimRight(',');
								temp.Format("\n[%s]:", filename.c_str());
								matchingFragments += temp;
								filenameAdded = true;
							}
							CryStackStringT<char, 128> sFragmentTags;
							fragmentDefs.GetSubTagDefinition(fragIndex)->FlagsToTagList(tagState.fragmentTags, sFragmentTags);
							temp.Format(" %s (%s),", fragmentDefs.GetTagName(fragIndex), sFragmentTags.c_str());
							matchingFragments += temp;
						}
					}
				}
			}

			if (pos != matchingFragments.length())
			{
				// Log the fragments so there's a record
				CryLog("[TAGDEF]: affected fragments: %s", matchingFragments.Mid(pos, matchingFragments.length() - 1).c_str());
				pos = matchingFragments.length();
			}
		}
	}

	if (!matchingFragments.empty())
	{
		matchingFragments.TrimRight(',');
	}
	else
	{
		matchingFragments = "\nno fragments";
	}

	cry_strcpy(buffer, bufferSize, matchingFragments.c_str());
	if (matchingFragments.length() + 1 > bufferSize)
	{
		// In case the string is truncated
		memcpy(buffer + bufferSize - 4, "...", 4);
	}
}

void CAnimationDatabaseManager::ApplyTagDefChanges(const CTagDefinition* pOriginal, CTagDefinition* pModified)
{
	string filename;

	for (TAnimDatabaseList::const_iterator iter = m_databases.begin(); iter != m_databases.end(); ++iter)
	{
		CAnimationDatabase* pCurrentDatabase = iter->second;
		bool isGlobalTag = (&pCurrentDatabase->GetTagDefs() == pOriginal);

		if (isGlobalTag)
		{
			const int32 numSubADBs = pCurrentDatabase->m_subADBs.size();
			TagState modified;

			for (int subADBIndex = numSubADBs - 1; subADBIndex >= 0; --subADBIndex)
			{
				CAnimationDatabase::SSubADB& subADB = pCurrentDatabase->m_subADBs[subADBIndex];
				if (pOriginal->MapTagState(subADB.tags, modified, *pModified))
				{
					subADB.tags = modified;
					subADB.comparisonMask = pModified->GenerateMask(modified);

					if (subADB.pTagDef == pOriginal)
					{
						subADB.knownTags = TAG_STATE_FULL;
					}
					else
					{
						subADB.knownTags = pModified->GetSharedTags(*subADB.pTagDef);
					}
				}
				else
				{
					RemoveDataFromParent(pCurrentDatabase, &pCurrentDatabase->m_subADBs[subADBIndex]);
					pCurrentDatabase->m_subADBs.erase(pCurrentDatabase->m_subADBs.begin() + subADBIndex);
				}
			}
		}
	}

	STagDefinitionImportsInfo& originalImports = mannequin::GetDefaultImportsInfo(pOriginal->GetFilename());
	originalImports.MapTags(*pOriginal, *pModified);

	// Do the save
	*(const_cast<CTagDefinition*>(pOriginal)) = *pModified;
}

void CAnimationDatabaseManager::RenameTag(const CTagDefinition* pOriginal, int32 tagCRC, const char* newName)
{
	STagDefinitionImportsInfo& originalImports = mannequin::GetDefaultImportsInfo(pOriginal->GetFilename());

	// Determine which import (if any) contains the tag to rename
	TagID id = pOriginal->FindGroup(tagCRC);
	const STagDefinitionImportsInfo& root = originalImports.Find(id);
	//CryLog("[TAGDEF]: root is %s", root.GetFilename());

	// For each tagdef we know about...
	for (TTagDefList::iterator it = m_tagDefs.begin(); it != m_tagDefs.end(); ++it)
	{
		std::vector<const STagDefinitionImportsInfo*> tagDefImportsInfo;
		STagDefinitionImportsInfo& tagDefImports = mannequin::GetDefaultImportsInfo(it->second->GetFilename());
		tagDefImports.FlattenImportsInfo(tagDefImportsInfo);

		// ...check if it imports the root...
		bool importsRoot = false;
		for (uint32 index = 0, size = tagDefImportsInfo.size(); index < size; ++index)
		{
			if (stricmp(root.GetFilename(), tagDefImportsInfo[index]->GetFilename()) == 0)
			{
				importsRoot = true;
				break;
			}
		}

		// ...and if it does, rename all occurrences of the tag in the import "chain"
		if (importsRoot)
		{
			for (uint32 index = 0, size = tagDefImportsInfo.size(); index < size; ++index)
			{
				const STagDefinitionImportsInfo* pImportsInfo = tagDefImportsInfo[index];
				CTagDefinition* pTagDef = stl::find_in_map(m_tagDefs, CCrc32::ComputeLowercase(pImportsInfo->GetFilename()), NULL);
				id = TAG_ID_INVALID;
				if (pTagDef != NULL)
				{
					id = pTagDef->Find(tagCRC);
				}
				if (id != TAG_ID_INVALID)
				{
					//CryLog("[TAGDEF]: tag %p [%s]:[%s]->[%s]", pTagDef, pImportsInfo->GetFilename(), pTagDef->GetTagName(id), newName);
					pTagDef->SetTagName(id, newName);
				}
			}
		}
	}
};

void CAnimationDatabaseManager::RenameTagGroup(const CTagDefinition* pOriginal, int32 tagGroupCRC, const char* newName)
{
	STagDefinitionImportsInfo& originalImports = mannequin::GetDefaultImportsInfo(pOriginal->GetFilename());

	// Determine which import (if any) contains the group to rename
	TagGroupID id = pOriginal->FindGroup(tagGroupCRC);
	const STagDefinitionImportsInfo& root = originalImports.Find(id);
	//CryLog("[TAGDEF]: root is %s", root.GetFilename());

	// For each tagdef we know about...
	for (TTagDefList::iterator it = m_tagDefs.begin(); it != m_tagDefs.end(); ++it)
	{
		std::vector<const STagDefinitionImportsInfo*> tagDefImportsInfo;
		STagDefinitionImportsInfo& tagDefImports = mannequin::GetDefaultImportsInfo(it->second->GetFilename());
		tagDefImports.FlattenImportsInfo(tagDefImportsInfo);

		// ...check if it imports the root...
		bool importsRoot = false;
		for (uint32 index = 0, size = tagDefImportsInfo.size(); index < size; ++index)
		{
			if (stricmp(root.GetFilename(), tagDefImportsInfo[index]->GetFilename()) == 0)
			{
				importsRoot = true;
				break;
			}
		}

		// ...and if it does, rename all occurrences of the group in the import "chain"
		if (importsRoot)
		{
			for (uint32 index = 0, size = tagDefImportsInfo.size(); index < size; ++index)
			{
				const STagDefinitionImportsInfo* pImportsInfo = tagDefImportsInfo[index];
				CTagDefinition* pTagDef = stl::find_in_map(m_tagDefs, CCrc32::ComputeLowercase(pImportsInfo->GetFilename()), NULL);
				id = GROUP_ID_NONE;
				if (pTagDef != NULL)
				{
					id = pTagDef->FindGroup(tagGroupCRC);
				}
				if (id != GROUP_ID_NONE)
				{
					//CryLog("[TAGDEF]: group %p [%s]:[%s]->[%s]", pTagDef, pImportsInfo->GetFilename(), pTagDef->GetGroupName(id), newName);
					pTagDef->SetGroupName(id, newName);
				}
			}
		}
	}
};

void CAnimationDatabaseManager::GetIncludedTagDefs(const CTagDefinition* pQueriedTagDef, DynArray<CTagDefinition*>& outputTagDefs) const
{
	outputTagDefs.clear();

	if (!pQueriedTagDef)
	{
		return;
	}

	const STagDefinitionImportsInfo& queriedImportsInfo = mannequin::GetDefaultImportsInfo(pQueriedTagDef->GetFilename());
	std::vector<const STagDefinitionImportsInfo*> flatImportsInfoList;
	queriedImportsInfo.FlattenImportsInfo(flatImportsInfoList);

	for (uint32 importIndex = 0, size = flatImportsInfoList.size(); importIndex < size; ++importIndex)
	{
		const STagDefinitionImportsInfo* pImportsInfo = flatImportsInfoList[importIndex];
		assert(pImportsInfo);
		CTagDefinition* pFoundTagDef = stl::find_in_map(m_tagDefs, CCrc32::ComputeLowercase(pImportsInfo->GetFilename()), NULL);

		if (pFoundTagDef && (pFoundTagDef != pQueriedTagDef))
		{
			outputTagDefs.push_back(pFoundTagDef);
		}
	}
}

void CAnimationDatabaseManager::SaveSubADB(const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB& subAnimDB, const TFragmentSaveList& vFragSaveList, const TFragmentBlendSaveDatabase& mBlendSaveDatabase) const
{
	XmlNodeRef xmlSubNode = SaveDatabase(animDB, &subAnimDB, vFragSaveList, mBlendSaveDatabase);
	xmlSubNode->saveToFile(subAnimDB.filename.c_str());

	for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = subAnimDB.subADBs.begin(); itSubADB != subAnimDB.subADBs.end(); ++itSubADB)
		SaveSubADB(animDB, *itSubADB, vFragSaveList, mBlendSaveDatabase);
}

void CAnimationDatabaseManager::Save(const IAnimationDatabase& iAnimDB, const char* databaseName) const
{
	CAnimationDatabase& animDB = (CAnimationDatabase&)iAnimDB;
	TFragmentSaveList vFragSaveList;
	TFragmentBlendSaveDatabase mBlendSaveDatabase;

	XmlNodeRef xmlNode = SaveDatabase(animDB, NULL, vFragSaveList, mBlendSaveDatabase);
	xmlNode->saveToFile(databaseName);

	for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = animDB.m_subADBs.begin(); itSubADB != animDB.m_subADBs.end(); ++itSubADB)
		SaveSubADB(animDB, *itSubADB, vFragSaveList, mBlendSaveDatabase);
}

//--- Helper functions

void CAnimationDatabaseLibrary::AnimEntryToXML(XmlNodeRef outXmlAnimEntry, const SAnimationEntry& animEntry, const char* name) const
{
	outXmlAnimEntry->setAttr("name", animEntry.animRef.c_str());
	if (animEntry.flags)
	{
		CryStackStringT<char, 1024> sFlags;
		m_animFlags.IntegerFlagsToTagList(animEntry.flags, sFlags);
		outXmlAnimEntry->setAttr("flags", sFlags.c_str());
	}
	if (animEntry.playbackSpeed != 1.0f)
	{
		outXmlAnimEntry->setAttr("speed", animEntry.playbackSpeed);
	}
	if (animEntry.playbackWeight != 1.0f)
	{
		outXmlAnimEntry->setAttr("weight", animEntry.playbackWeight);
	}
	if (animEntry.weightList != 0)
	{
		outXmlAnimEntry->setAttr("weightList", animEntry.weightList);
	}
	char channelName[10];
	cry_strcpy(channelName, "channel0");
	for (uint32 i = 0; i < MANN_NUMBER_BLEND_CHANNELS; i++)
	{
		if (animEntry.blendChannels[i] != 0.0f)
		{
			channelName[7] = '0' + i;
			outXmlAnimEntry->setAttr(channelName, animEntry.blendChannels[i]);
		}
	}
}

bool CAnimationDatabaseLibrary::XMLToAnimEntry(SAnimationEntry& animEntry, XmlNodeRef root) const
{
	bool success = true;
	animEntry.animRef.SetByString(root->getAttr("name"));
	success = success && m_animFlags.TagListToIntegerFlags(root->getAttr("flags"), animEntry.flags);
	animEntry.playbackSpeed = 1.0f;
	root->getAttr("speed", animEntry.playbackSpeed);
	animEntry.playbackWeight = 1.0f;
	root->getAttr("weight", animEntry.playbackWeight);
	root->getAttr("weightList", animEntry.weightList);

	char channelName[10];
	cry_strcpy(channelName, "channel0");
	for (uint32 i = 0; i < MANN_NUMBER_BLEND_CHANNELS; i++)
	{
		channelName[7] = '0' + i;
		root->getAttr(channelName, animEntry.blendChannels[i]);
	}
	return success;
}

void CAnimationDatabaseLibrary::BlendToXML(XmlNodeRef outXmlBlend, const SAnimBlend& animBlend, const char* name) const
{
	outXmlBlend->setAttr("ExitTime", animBlend.exitTime);
	outXmlBlend->setAttr("StartTime", animBlend.startTime);
	outXmlBlend->setAttr("Duration", animBlend.duration);

	if (animBlend.flags)
	{
		CryStackStringT<char, 1024> sFlags;
		m_animFlags.IntegerFlagsToTagList(animBlend.flags, sFlags);
		outXmlBlend->setAttr("flags", sFlags.c_str());
	}

	if (animBlend.terminal)
	{
		outXmlBlend->setAttr("terminal", true);
	}
}

void CAnimationDatabaseLibrary::XMLToBlend(SAnimBlend& animBlend, XmlNodeRef xmlBlend) const
{
	xmlBlend->getAttr("ExitTime", animBlend.exitTime);
	xmlBlend->getAttr("StartTime", animBlend.startTime);
	xmlBlend->getAttr("Duration", animBlend.duration);
	m_animFlags.TagListToIntegerFlags(xmlBlend->getAttr("flags"), animBlend.flags);
	xmlBlend->getAttr("terminal", animBlend.terminal);
}

void CAnimationDatabaseLibrary::ProceduralToXml(XmlNodeRef pOutNode, const SProceduralEntry& proceduralEntry) const
{
	const bool isNoneType = proceduralEntry.IsNoneType();
	const char* const typeName = isNoneType ? "" : mannequin::FindProcClipTypeName(proceduralEntry.typeNameHash);

	if (!typeName)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Unable to create xml node for procedural entry: Type '%u' is unknown!", proceduralEntry.typeNameHash.ToUInt32());
		return;
	}

	pOutNode->setAttr("type", typeName);

	if (proceduralEntry.pProceduralParams)
	{
		XmlNodeRef pXmlProceduralParams = Serialization::SaveXmlNode(*proceduralEntry.pProceduralParams, "ProceduralParams");
		if (pXmlProceduralParams)
		{
			pOutNode->addChild(pXmlProceduralParams);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Unable to create 'ProceduralParams' node for procedural entry parameters for clip with type name '%s'", typeName);
		}
	}
}

bool CAnimationDatabaseLibrary::XmlToProcedural(XmlNodeRef pInNode, SProceduralEntry& outProceduralEntry) const
{
	CRY_ASSERT(gEnv && gEnv->pGameFramework);
	const IProceduralClipFactory& proceduralClipFactory = gEnv->pGameFramework->GetMannequinInterface().GetProceduralClipFactory();

	const char* const typeName = pInNode->getAttr("type");
	const char* const safeTypeName = typeName ? typeName : "";

	const IProceduralClipFactory::THash typeNameHash(safeTypeName);

	outProceduralEntry.typeNameHash = IProceduralClipFactory::THash();
	const bool isNoneType = typeNameHash.IsEmpty();
	if (!isNoneType)
	{
		outProceduralEntry.pProceduralParams = proceduralClipFactory.CreateProceduralClipParams(typeNameHash);
		if (!outProceduralEntry.pProceduralParams)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Could not create procedural clip parameters for procedural clip with type name '%s' and hash '%u' at line '%d'.", safeTypeName, typeNameHash.ToUInt32(), pInNode->getLine());
			return false;
		}

		XmlNodeRef pXmlProceduralParams = pInNode->findChild("ProceduralParams");

		{
			// Old procedural clip format conversion. Remove this whole block when support for old format is not desired anymore.
			if (!pXmlProceduralParams)
			{
				CProcClipConversionHelper helper;
				pXmlProceduralParams = helper.Convert(pInNode);
				if (pXmlProceduralParams)
				{
					CryLogAlways("Converted old format procedural clip with type name '%s' at line '%d'.", safeTypeName, pInNode->getLine());
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Failed to convert old format procedural clip with type name '%s' at line '%d'.", safeTypeName, pInNode->getLine());
					return false;
				}
			}
		}

		if (!pXmlProceduralParams)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Failed loading procedural parameters for clip with type name '%s' at line '%d': Unable to find 'ProceduralParams' xml node.", safeTypeName, pInNode->getLine());
			return false;
		}
		if (!Serialization::LoadXmlNode(*outProceduralEntry.pProceduralParams, pXmlProceduralParams))
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed loading procedural parameters for clip with type name '%s' at line '%d': Unable to create Xml Input Archive for serialization.", safeTypeName, pInNode->getLine());
			return false;
		}
	}

	outProceduralEntry.typeNameHash = typeNameHash;
	return true;
}

void CAnimationDatabaseLibrary::FragmentToXML(XmlNodeRef outXmlFrag, const CFragment* fragment, bool transition) const
{
	const uint32 numLayers = fragment->m_animLayers.size();
	for (uint32 i = 0; i < numLayers; ++i)
	{
		const TAnimClipSequence& sequence = fragment->m_animLayers[i];

		XmlNodeRef xmlAnimLayer = outXmlFrag->createNode("AnimLayer");
		outXmlFrag->addChild(xmlAnimLayer);

		const uint32 numClips = sequence.size();
		for (uint32 c = 0; c < numClips; c++)
		{
			const SAnimClip& animClip = sequence[c];

			XmlNodeRef xmlBlend = xmlAnimLayer->createNode("Blend");
			BlendToXML(xmlBlend, animClip.blend, "Blend");
			xmlAnimLayer->addChild(xmlBlend);

			const bool blendOnly = transition && (c == numClips - 1);
			if (!blendOnly)
			{
				XmlNodeRef xmlClip = xmlAnimLayer->createNode("Animation");
				AnimEntryToXML(xmlClip, animClip.animation, "Animation");
				xmlAnimLayer->addChild(xmlClip);
			}
		}
	}
	const uint32 numProcLayers = fragment->m_procLayers.size();
	for (uint32 i = 0; i < numProcLayers; ++i)
	{
		const TProcClipSequence& sequence = fragment->m_procLayers[i];

		XmlNodeRef xmlProcLayer = outXmlFrag->createNode("ProcLayer");
		outXmlFrag->addChild(xmlProcLayer);

		const uint32 numClips = sequence.size();
		for (uint32 c = 0; c < numClips; c++)
		{
			const SProceduralEntry& procClip = sequence[c];

			XmlNodeRef xmlBlend = xmlProcLayer->createNode("Blend");
			BlendToXML(xmlBlend, procClip.blend, "Blend");
			xmlProcLayer->addChild(xmlBlend);

			const bool blendOnly = transition && (c == numClips - 1);
			if (!blendOnly)
			{
				XmlNodeRef xmlClip = xmlProcLayer->createNode("Procedural");
				ProceduralToXml(xmlClip, procClip);
				xmlProcLayer->addChild(xmlClip);
			}
		}
	}

	outXmlFrag->setAttr("BlendOutDuration", fragment->m_blendOutDuration);
}

bool CAnimationDatabaseLibrary::XMLToFragment(CFragment& fragment, XmlNodeRef root, bool transition) const
{
	const uint32 numChildren = root->getChildCount();
	uint32 numAnimLayers = 0;
	uint32 numProcLayers = 0;

	root->getAttr("BlendOutDuration", fragment.m_blendOutDuration);

	for (uint32 i = 0; i < numChildren; ++i)
	{
		XmlNodeRef xmlLayer = root->getChild(i);

		if (strcmp(xmlLayer->getTag(), "AnimLayer") == 0)
		{
			++numAnimLayers;
		}
		else if (strcmp(xmlLayer->getTag(), "ProcLayer") == 0)
		{
			++numProcLayers;
		}
		else
		{
			return false;
		}
	}

	fragment.m_animLayers.resize(numAnimLayers);
	fragment.m_procLayers.resize(numProcLayers);

	uint32 animLayer = 0;
	uint32 procLayer = 0;
	for (uint32 i = 0; i < numChildren; ++i)
	{
		XmlNodeRef xmlLayer = root->getChild(i);

		if (strcmp(xmlLayer->getTag(), "AnimLayer") == 0)
		{
			const uint32 numEntries = xmlLayer->getChildCount();
			const uint32 transitionInc = transition ? 1 : 0;
			const uint32 numClips = (numEntries / 2) + transitionInc;

			TAnimClipSequence& sequence = fragment.m_animLayers[animLayer];
			sequence.resize(numClips);

			uint32 clipNumber = 0;
			for (uint32 e = 0; e < numEntries; ++e)
			{
				XmlNodeRef xmlChild = xmlLayer->getChild(e);
				SAnimClip& animClip = sequence[clipNumber];
				if (strcmp(xmlChild->getTag(), "Blend") == 0)
				{
					XMLToBlend(animClip.blend, xmlChild);
				}
				else if (strcmp(xmlChild->getTag(), "Animation") == 0)
				{
					XMLToAnimEntry(animClip.animation, xmlChild);
					++clipNumber;
				}
			}

			++animLayer;
		}
		else if (strcmp(xmlLayer->getTag(), "ProcLayer") == 0)
		{
			const uint32 numEntries = xmlLayer->getChildCount();
			const uint32 transitionInc = transition ? 1 : 0;
			const uint32 numClips = (numEntries / 2) + transitionInc;

			TProcClipSequence& sequence = fragment.m_procLayers[procLayer];
			sequence.resize(numClips);

			uint32 clipNumber = 0;
			for (uint32 e = 0; e < numEntries; ++e)
			{
				XmlNodeRef xmlChild = xmlLayer->getChild(e);
				SProceduralEntry& proceduralEntry = sequence[clipNumber];
				if (strcmp(xmlChild->getTag(), "Procedural") == 0)
				{
					XmlToProcedural(xmlChild, proceduralEntry);
					clipNumber++;
				}
				else if (strcmp(xmlChild->getTag(), "Blend") == 0)
				{
					XMLToBlend(proceduralEntry.blend, xmlChild);
				}
			}

			++procLayer;
		}
	}

	return true;
}

void CAnimationDatabaseLibrary::LoadDatabaseData
(
  CAnimationDatabase& animDB,
  const XmlNodeRef root,
  const CTagDefinition& fragIDs,
  const CTagDefinition& tagIDs,
  const TagState& adbFilter,
  bool recursive,
  CAnimationDatabase::SSubADB* subAnimDB
)
{
	if (recursive)
	{
		XmlNodeRef subADBs = root->findChild("SubADBs");
		const uint32 numSubADBs = subADBs ? subADBs->getChildCount() : 0;
		for (uint32 i = 0; i < numSubADBs; ++i)
		{
			XmlNodeRef adbEntry = subADBs->getChild(i);
			const char* tags = adbEntry->getAttr("Tags");

			TagState subTags(TAG_STATE_EMPTY);
			bool tagsMatched = false;
			bool hasTags = (tags && strlen(tags) > 0);
			if (hasTags)
				tagsMatched = tagIDs.TagListToFlags(tags, subTags);

			char normalizedFilename[DEF_PATH_LENGTH];
			NormalizeFilename(normalizedFilename, adbEntry->getAttr("File"));

			CAnimationDatabase::SSubADB newSubADB;
			newSubADB.tags = tagIDs.GetUnion(adbFilter, subTags);
			newSubADB.comparisonMask = tagIDs.GenerateMask(newSubADB.tags);
			newSubADB.filename = normalizedFilename;

			CryLog("Loading subADB %s", normalizedFilename);

			XmlNodeRef xmlData = gEnv->pSystem->GetXmlUtils()->LoadXmlFromFile(normalizedFilename);
			if (xmlData && LoadDatabaseDefinitions(xmlData, animDB.m_filename.c_str(), &newSubADB.pFragDef, &newSubADB.pTagDef))
			{
				//--- Create a mask of valid tags
				if (newSubADB.pTagDef == &tagIDs)
				{
					newSubADB.knownTags = TAG_STATE_FULL;
				}
				else
				{
					newSubADB.knownTags = tagIDs.GetSharedTags(*newSubADB.pTagDef);
				}

				int numFragIDs = adbEntry->getChildCount();
				for (int j = 0; j < numFragIDs; ++j)
				{
					XmlNodeRef FragIDEntry = adbEntry->getChild(j);
					const char* szFragmentName = FragIDEntry->getAttr("Name");
					if (szFragmentName && strlen(szFragmentName) > 0)
					{
						int fragmentID = newSubADB.pFragDef->Find(szFragmentName);
						if (fragmentID != TAG_ID_INVALID)
							newSubADB.vFragIDs.push_back(fragmentID);
					}
				}
				if (!tagsMatched && hasTags)
				{
					CryLog("[CAnimationDatabaseManager::LoadDatabase] Unknown tags %s for subADB %s", tags, adbEntry->getAttr("File"));
					continue;
				}

				LoadDatabaseData(animDB, xmlData, fragIDs, tagIDs, newSubADB.tags, recursive, &newSubADB);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "!Could not load animation database (ADB) file '%s' (check the log for XML reader warnings)", normalizedFilename);
				continue;
			}

			if (!subAnimDB)
				animDB.m_subADBs.push_back(newSubADB);
			else
				subAnimDB->subADBs.push_back(newSubADB);

			SLICE_AND_SLEEP();
		}
	}

	if (root)
	{
		CFragment newFragment;
		XmlNodeRef fragmentList = root->findChild("FragmentList");
		uint32 numChildren = fragmentList ? fragmentList->getChildCount() : 0;
		for (uint32 i = 0; i < numChildren; ++i)
		{
			XmlNodeRef fragmentEntry = fragmentList->getChild(i);
			const char* szFragmentName = fragmentEntry->getTag();
			int fragmentID = fragIDs.Find(szFragmentName);

			if (fragmentID >= 0)
			{
				const uint32 numOptions = fragmentEntry->getChildCount();

				for (uint32 k = 0; k < numOptions; ++k)
				{
					XmlNodeRef fragment = fragmentEntry->getChild(k);

					const char* globalTags = fragment->getAttr("Tags");
					const char* fragTags = fragment->getAttr("FragTags");
					SFragTagState tagState;
					bool tagsMatched = tagIDs.TagListToFlags(globalTags, tagState.globalTags);
					const CTagDefinition* pFragTagDef = fragIDs.GetSubTagDefinition(fragmentID);
					if (fragTags && pFragTagDef)
					{
						tagsMatched = tagsMatched && pFragTagDef->TagListToFlags(fragTags, tagState.fragmentTags);
					}

					if (tagsMatched)
					{
						newFragment.m_animLayers.clear();
						newFragment.m_procLayers.clear();
						if (!XMLToFragment(newFragment, fragment, false))
						{
							CryLog("[CAnimationDatabaseManager::LoadDatabase] Broken fragment entry for fragmentID %s tag %s fragTags %s", szFragmentName, globalTags, fragTags ? fragTags : "None");
						}
						else
						{
							animDB.AddEntry(fragmentID, tagState, newFragment);
						}
					}
					else
					{
						CryLog("[CAnimationDatabaseManager::LoadDatabase] Unknown tags for fragmentID %s tag %s fragTags %s", szFragmentName, globalTags, fragTags ? fragTags : "None");
					}
				}
			}
			else
			{
				CryLog("[CAnimationDatabaseManager::LoadDatabase] Warning missing fragmentID %s", szFragmentName);
			}
		}

		XmlNodeRef fragmentBlendList = root->findChild("FragmentBlendList");
		const uint32 numFragmentBlends = fragmentBlendList ? fragmentBlendList->getChildCount() : 0;
		for (uint32 i = 0; i < numFragmentBlends; ++i)
		{
			XmlNodeRef xmlBlendEntry = fragmentBlendList->getChild(i);
			const char* fragFrom = xmlBlendEntry->getAttr("from");
			const char* fragTo = xmlBlendEntry->getAttr("to");
			int fragIDFrom = fragIDs.Find(fragFrom);
			int fragIDTo = fragIDs.Find(fragTo);

			const bool fragFromValid = (fragIDFrom != FRAGMENT_ID_INVALID) || (fragFrom[0] == '\0');
			const bool fragToValid = (fragIDTo != FRAGMENT_ID_INVALID) || (fragTo[0] == '\0');

			if (fragFromValid && fragToValid)
			{
				const uint32 numVariants = xmlBlendEntry->getChildCount();
				for (uint32 v = 0; v < numVariants; ++v)
				{
					XmlNodeRef xmlVariant = xmlBlendEntry->getChild(v);
					const char* tagFrom = xmlVariant->getAttr("from");
					const char* tagTo = xmlVariant->getAttr("to");
					SFragTagState tsFrom, tsTo;
					bool tagStateMatched = tagIDs.TagListToFlags(tagFrom, tsFrom.globalTags);
					tagStateMatched = tagStateMatched && tagIDs.TagListToFlags(tagTo, tsTo.globalTags);

					const CTagDefinition* fragFromDef = (fragIDFrom != FRAGMENT_ID_INVALID) ? fragIDs.GetSubTagDefinition(fragIDFrom) : NULL;
					const CTagDefinition* fragToDef = (fragIDTo != FRAGMENT_ID_INVALID) ? fragIDs.GetSubTagDefinition(fragIDTo) : NULL;
					const char* fragTagFrom = xmlVariant->getAttr("fromFrag");
					const char* fragTagTo = xmlVariant->getAttr("toFrag");

					if (fragFromDef && fragTagFrom)
					{
						tagStateMatched = tagStateMatched && fragFromDef->TagListToFlags(fragTagFrom, tsFrom.fragmentTags);
					}
					if (fragToDef && fragTagTo)
					{
						tagStateMatched = tagStateMatched && fragToDef->TagListToFlags(fragTagTo, tsTo.fragmentTags);
					}

					if (tagStateMatched)
					{
						const uint32 numBlends = xmlVariant->getChildCount();
						for (uint32 b = 0; b < numBlends; ++b)
						{
							TagState tempTags;
							XmlNodeRef xmlBlend = xmlVariant->getChild(b);
							SFragmentBlend fragBlend;
							xmlBlend->getAttr("selectTime", fragBlend.selectTime);
							xmlBlend->getAttr("startTime", fragBlend.startTime);
							xmlBlend->getAttr("enterTime", fragBlend.enterTime);
							const char* szBlendFlags = xmlBlend->getAttr("flags");

							if (szBlendFlags && szBlendFlags[0])
							{
								m_transitionFlags.TagListToIntegerFlags(szBlendFlags, fragBlend.flags);
							}

							fragBlend.pFragment = new CFragment();
							if ((xmlBlend->getChildCount() != 0) && !XMLToFragment(*fragBlend.pFragment, xmlBlend, true))
							{
								delete fragBlend.pFragment;
								CryLog("[CAnimationDatabaseManager::LoadDatabase] Broken fragment entry for blend");
							}
							else
							{
								animDB.AddBlendInternal(fragIDFrom, fragIDTo, tsFrom, tsTo, fragBlend);
							}
						}
					}
					else
					{
						CryLog("[CAnimationDatabaseManager::LoadDatabase] Unknown tags for blend %s to %s tag %s+%s to %s+%s", fragFrom, fragTo, tagFrom, fragTagFrom ? fragTagFrom : "None", tagTo, fragTagTo ? fragTagTo : "None");
					}
				}
			}
		}
	}
}

bool CAnimationDatabaseLibrary::LoadDatabaseDefinitions(const XmlNodeRef& root, const char* filename, const CTagDefinition** ppFragDefs, const CTagDefinition** ppTagDefs)
{
	const char* szBuffer;
	szBuffer = root->getAttr("Def");
	if (szBuffer && szBuffer[0])
	{
		const SControllerDef* def = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager().LoadControllerDef(szBuffer);
		if (def)
		{
			*ppFragDefs = &def->m_fragmentIDs;
			*ppTagDefs = &def->m_tags;
			return true;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "!Missing def file for database %s - skipping load", filename);
			return false;
		}
	}

	szBuffer = root->getAttr("FragDef");
	if (szBuffer && szBuffer[0])
	{
		*ppFragDefs = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager().LoadTagDefs(szBuffer, false);
	}

	if (*ppFragDefs == NULL)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "!Missing fragment definition file for database %s - skipping load", filename);
		return false;
	}

	szBuffer = root->getAttr("TagDef");
	if (szBuffer && szBuffer[0])
	{
		*ppTagDefs = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager().LoadTagDefs(szBuffer, true);
	}

	if (*ppTagDefs == NULL)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "!Missing tag definition file for database %s - skipping load", filename);
		return false;
	}

	return true;
}

bool CAnimationDatabaseLibrary::LoadDatabase(const XmlNodeRef& root, CAnimationDatabase& animDB, bool recursive)
{
	if (stricmp(root->getTag(), "AnimDB") != 0)
	{
		return false;
	}

	if (LoadDatabaseDefinitions(root, animDB.m_filename.c_str(), &animDB.m_pFragDef, &animDB.m_pTagDef))
	{
		const uint32 numActions = animDB.m_pFragDef->GetNum();

		animDB.SetAutoSort(false);

		animDB.m_fragmentList.resize(numActions);
		for (uint32 i = 0; i < numActions; ++i)
		{
			animDB.m_fragmentList[i] = new CAnimationDatabase::SFragmentEntry();
		}

		LoadDatabaseData(animDB, root, *animDB.m_pFragDef, *animDB.m_pTagDef, TAG_STATE_EMPTY, recursive);

		animDB.Sort();
		animDB.SetAutoSort(true);

		return true;
	}

	return false;
}

bool CAnimationDatabaseManager::CanSaveFragmentID(FragmentID fragID, const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* subAnimDB) const
{
	if (subAnimDB && (subAnimDB->pFragDef != animDB.m_pFragDef) && (fragID != FRAGMENT_ID_INVALID))
		return subAnimDB->pFragDef->Find(animDB.m_pFragDef->GetTagName(fragID)) != FRAGMENT_ID_INVALID;

	return true;
}

bool CAnimationDatabaseManager::FragmentMatches(uint32 fragCRC, const TagState& tagState, const CAnimationDatabase::SSubADB& subAnimDB, bool& bTagMatchFound) const
{
	//--- If this fragmentID is not known by this subADB, then it can't go here
	if ((fragCRC != 0) && (subAnimDB.pFragDef->Find(fragCRC) == FRAGMENT_ID_INVALID))
		return false;

	bTagMatchFound = false;
	bool bFragMatchFound = false;
	bool bMatchFound = false;
	if (!subAnimDB.tags.IsEmpty())
	{
		const TagState tagStateADB = subAnimDB.tags;
		const TagState maskADB = subAnimDB.comparisonMask;
		if (subAnimDB.pTagDef->Contains(tagState, tagStateADB, maskADB) && (subAnimDB.pFragDef->Find(fragCRC) != FRAGMENT_ID_INVALID))
			bTagMatchFound = ((subAnimDB.knownTags & tagState) == tagState);
	}

	if (!subAnimDB.vFragIDs.empty())
		for (CAnimationDatabase::SSubADB::TFragIDList::const_iterator itFID = subAnimDB.vFragIDs.begin(); itFID != subAnimDB.vFragIDs.end(); ++itFID)
			if (subAnimDB.pFragDef->GetTagCRC(*itFID) == fragCRC)
				bFragMatchFound = true;

	if ((subAnimDB.tags.IsEmpty() || bTagMatchFound) && (subAnimDB.vFragIDs.empty() || bFragMatchFound))
		bMatchFound = true;

	return bMatchFound;
}

bool CAnimationDatabaseManager::ShouldSaveFragment(FragmentID fragID, const TagState& tagState, const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* subAnimDB, bool& bTagMatchFound) const
{
	uint32 fragCRC = animDB.m_pFragDef->GetTagCRC(fragID);
	bTagMatchFound = false;
	if (!subAnimDB || FragmentMatches(fragCRC, tagState, *subAnimDB, bTagMatchFound))
	{
		const CAnimationDatabase::TSubADBList& vSubADBs = subAnimDB ? subAnimDB->subADBs : animDB.m_subADBs;

		if (vSubADBs.empty())
		{
			return true;
		}

		bool bDummy;
		for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = vSubADBs.begin(); itSubADB != vSubADBs.end(); ++itSubADB)
			if (FragmentMatches(fragCRC, tagState, *itSubADB, bDummy))
				return false;

		return true;
	}

	return !subAnimDB;
}

bool CAnimationDatabaseManager::TransitionMatches(uint32 fragCRCFrom, uint32 fragCRCTo, const TagState& tagStateFrom, const TagState& tagStateTo, const CTagDefinition& parentTagDefs, const CAnimationDatabase::SSubADB& subAnimDB, bool& bTagMatchFound) const
{
	//--- If these tags are not known by this subADB, then it can't go here
	const TagState totalTags = parentTagDefs.GetUnion(tagStateFrom, tagStateTo);
	if (((subAnimDB.knownTags & totalTags) != totalTags)
	    || ((fragCRCFrom != 0) && (subAnimDB.pFragDef->Find(fragCRCFrom) == FRAGMENT_ID_INVALID))
	    || ((fragCRCTo != 0) && (subAnimDB.pFragDef->Find(fragCRCTo) == FRAGMENT_ID_INVALID)))
	{
		return false;
	}

	const TagState commonTags = parentTagDefs.GetIntersection(tagStateFrom, tagStateTo);
	bool bMatchFound = false;
	bTagMatchFound = false;
	bool bFragMatchFound = false;
	const TagState tagStateADB = subAnimDB.tags;
	const TagState maskADB = subAnimDB.comparisonMask;
	if (tagStateADB.IsEmpty() || parentTagDefs.Contains(commonTags, tagStateADB, maskADB))
	{
		bTagMatchFound = true;
	}

	if (!subAnimDB.vFragIDs.empty() && (fragCRCFrom != 0 || fragCRCTo != 0))
	{
		bool bFragMatchFoundFrom = false;
		bool bFragMatchFoundTo = false;
		for (CAnimationDatabase::SSubADB::TFragIDList::const_iterator itFID = subAnimDB.vFragIDs.begin(); itFID != subAnimDB.vFragIDs.end(); ++itFID)
		{
			if (subAnimDB.pFragDef->GetTagCRC(*itFID) == fragCRCFrom)
				bFragMatchFoundFrom = true;

			if (subAnimDB.pFragDef->GetTagCRC(*itFID) == fragCRCTo)
				bFragMatchFoundTo = true;
		}
		bFragMatchFound = (bFragMatchFoundFrom || !fragCRCFrom) && (bFragMatchFoundTo || !fragCRCTo);
	}

	if (bTagMatchFound && (subAnimDB.vFragIDs.empty() || bFragMatchFound))
		bMatchFound = true;

	return bMatchFound;
}

bool CAnimationDatabaseManager::ShouldSaveTransition
(
  FragmentID fragIDFrom,
  FragmentID fragIDTo,
  const TagState& tagStateFrom,
  const TagState& tagStateTo,
  const CAnimationDatabase& animDB,
  const CAnimationDatabase::SSubADB* subAnimDB,
  bool& bTagMatchFound
) const
{
	//--- Special case rule: if this is an any to any then stick it at the top level, otherwise it'll go in the first match
	if ((fragIDFrom == FRAGMENT_ID_INVALID) && (fragIDTo == FRAGMENT_ID_INVALID))
	{
		return (subAnimDB == NULL);
	}

	uint32 fragCRCFrom = (fragIDFrom == FRAGMENT_ID_INVALID) ? 0 : animDB.m_pFragDef->GetTagCRC(fragIDFrom);
	uint32 fragCRCTo = (fragIDTo == FRAGMENT_ID_INVALID) ? 0 : animDB.m_pFragDef->GetTagCRC(fragIDTo);
	if (!subAnimDB || TransitionMatches(fragCRCFrom, fragCRCTo, tagStateFrom, tagStateTo, *animDB.m_pTagDef, *subAnimDB, bTagMatchFound))
	{
		const CAnimationDatabase::TSubADBList& vSubADBs = subAnimDB ? subAnimDB->subADBs : animDB.m_subADBs;

		if (vSubADBs.empty())
			return true;

		bool bSubTag;
		for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = vSubADBs.begin(); itSubADB != vSubADBs.end(); ++itSubADB)
			if (TransitionMatches(fragCRCFrom, fragCRCTo, tagStateFrom, tagStateTo, *animDB.m_pTagDef, *itSubADB, bSubTag))
				return false;

		return true;
	}

	return !subAnimDB;
}

XmlNodeRef CAnimationDatabaseManager::SaveDatabase
(
  const CAnimationDatabase& animDB,
  const CAnimationDatabase::SSubADB* subAnimDB,
  const TFragmentSaveList& vFragSaveList,
  const TFragmentBlendSaveDatabase& mBlendSaveDatabase,
  bool bFlatten
) const
{
	if (bFlatten && subAnimDB)
		return XmlNodeRef();

	string sMyFileName = subAnimDB ? subAnimDB->filename : animDB.m_filename;

	XmlNodeRef root = GetISystem()->CreateXmlNode("AnimDB");

	if (subAnimDB)
	{
		root->setAttr("FragDef", subAnimDB->pFragDef->GetFilename());
		root->setAttr("TagDef", subAnimDB->pTagDef->GetFilename());
	}
	else
	{
		root->setAttr("FragDef", animDB.m_pFragDef->GetFilename());
		root->setAttr("TagDef", animDB.m_pTagDef->GetFilename());
	}

	const CAnimationDatabase::TSubADBList& vSubADBs = subAnimDB ? subAnimDB->subADBs : animDB.m_subADBs;
	const uint32 numSubADBs = vSubADBs.size();
	if (!vSubADBs.empty())
	{
		XmlNodeRef subADBList;

		for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = vSubADBs.begin(); itSubADB != vSubADBs.end(); ++itSubADB)
		{
			const CAnimationDatabase::SSubADB& subADB = *itSubADB;

			if (!subADBList)
			{
				subADBList = root->createNode("SubADBs");
				root->addChild(subADBList);
			}

			XmlNodeRef adbEntry = subADBList->createNode("SubADB");
			if (!subADB.tags.IsEmpty())
			{
				CryStackStringT<char, 1024> sTags;
				animDB.m_pTagDef->FlagsToTagList(subADB.tags, sTags);
				adbEntry->setAttr("Tags", sTags.c_str());
			}
			adbEntry->setAttr("File", subADB.filename);
			subADBList->addChild(adbEntry);
			for (CAnimationDatabase::SSubADB::TFragIDList::const_iterator itFID = subADB.vFragIDs.begin(); itFID != subADB.vFragIDs.end(); ++itFID)
			{
				XmlNodeRef FragmentIDEntry = adbEntry->createNode("FragmentID");
				FragmentIDEntry->setAttr("Name", subADB.pFragDef->GetTagName(*itFID));
				adbEntry->addChild(FragmentIDEntry);
			}
		}
	}

	XmlNodeRef fragmentList = root->createNode("FragmentList");
	root->addChild(fragmentList);
	for (uint32 i = 0; i < animDB.m_fragmentList.size(); ++i)
	{
		if (CanSaveFragmentID(i, animDB, subAnimDB))
		{
			CAnimationDatabase::SFragmentEntry* entry = animDB.m_fragmentList[i];
			const uint32 numTagSets = entry ? entry->tagSetList.Size() : 0;
			if (numTagSets > 0)
			{
				XmlNodeRef fragmentEntry;

				for (uint32 k = 0; k < numTagSets; k++)
				{
					const string& saveFileName = vFragSaveList[i].vSaveStates[k].m_sFileName;
					if (saveFileName == sMyFileName || bFlatten)
					{
						if (!fragmentEntry)
						{
							fragmentEntry = root->createNode(animDB.m_pFragDef->GetTagName(i));
							fragmentList->addChild(fragmentEntry);
						}

						const SFragTagState& tagState = entry->tagSetList.m_keys[k];
						CAnimationDatabase::TFragmentOptionList& optionList = entry->tagSetList.m_values[k];
						const uint32 numOptions = optionList.size();
						for (uint32 o = 0; o < numOptions; ++o)
						{
							XmlNodeRef fragment = fragmentEntry->createNode("Fragment");
							FragmentToXML(fragment, optionList[o].fragment, false);

							CryStackStringT<char, 1024> sGlobalTags;
							animDB.m_pTagDef->FlagsToTagList(tagState.globalTags, sGlobalTags);
							fragment->setAttr("Tags", sGlobalTags.c_str());

							const CTagDefinition* fragTagDef = animDB.m_pFragDef->GetSubTagDefinition(i);
							if (fragTagDef && (tagState.fragmentTags != TAG_STATE_EMPTY))
							{
								CryStackStringT<char, 1024> sFragmentTags;
								fragTagDef->FlagsToTagList(tagState.fragmentTags, sFragmentTags);
								fragment->setAttr("FragTags", sFragmentTags.c_str());
							}

							fragmentEntry->addChild(fragment);
						}
					}
				}
			}
		}
	}

	if (!animDB.m_fragmentBlendDB.empty())
	{
		XmlNodeRef fragmentBlendList = root->createNode("FragmentBlendList");
		bool hasAnyBlends = false;

		for (CAnimationDatabase::TFragmentBlendDatabase::const_iterator iter = animDB.m_fragmentBlendDB.begin(); iter != animDB.m_fragmentBlendDB.end(); ++iter)
		{
			const FragmentID fragIDFrom = iter->first.fragFrom;
			const FragmentID fragIDTo = iter->first.fragTo;

			if (CanSaveFragmentID(fragIDFrom, animDB, subAnimDB) && CanSaveFragmentID(fragIDTo, animDB, subAnimDB))
			{
				XmlNodeRef pFragment = root->createNode("Blend");
				pFragment->setAttr("from", (fragIDFrom == FRAGMENT_ID_INVALID) ? "" : animDB.m_pFragDef->GetTagName(fragIDFrom));
				pFragment->setAttr("to", (fragIDTo == FRAGMENT_ID_INVALID) ? "" : animDB.m_pFragDef->GetTagName(fragIDTo));

				const CTagDefinition* fragFromDef = (fragIDFrom != FRAGMENT_ID_INVALID) ? animDB.m_pFragDef->GetSubTagDefinition(fragIDFrom) : NULL;
				const CTagDefinition* fragToDef = (fragIDTo != FRAGMENT_ID_INVALID) ? animDB.m_pFragDef->GetSubTagDefinition(fragIDTo) : NULL;

				bool hasAnyEntries = false;

				const uint32 numVars = iter->second.variantList.size();
				for (uint32 v = 0; v < numVars; ++v)
				{
					const CAnimationDatabase::SFragmentBlendVariant& variant = iter->second.variantList[v];

					TFragmentBlendSaveDatabase::const_iterator itSaveEntry = mBlendSaveDatabase.find(iter->first);
					if ((itSaveEntry != mBlendSaveDatabase.end() && itSaveEntry->second.vSaveStates[v].m_sFileName == sMyFileName) || bFlatten)
					{
						hasAnyEntries = true;
						hasAnyBlends = true;
						XmlNodeRef xmlFragmentVariant = root->createNode("Variant");
						pFragment->addChild(xmlFragmentVariant);

						CryStackStringT<char, 1024> sGlobalTagsFrom;
						animDB.m_pTagDef->FlagsToTagList(variant.tagsFrom.globalTags, sGlobalTagsFrom);
						xmlFragmentVariant->setAttr("from", sGlobalTagsFrom.c_str());

						CryStackStringT<char, 1024> sGlobalTagsTo;
						animDB.m_pTagDef->FlagsToTagList(variant.tagsTo.globalTags, sGlobalTagsTo);
						xmlFragmentVariant->setAttr("to", sGlobalTagsTo.c_str());

						if ((variant.tagsFrom.fragmentTags != TAG_STATE_EMPTY) && fragFromDef)
						{
							CryStackStringT<char, 1024> sFragmentTags;
							fragFromDef->FlagsToTagList(variant.tagsFrom.fragmentTags, sFragmentTags);
							xmlFragmentVariant->setAttr("fromFrag", sFragmentTags.c_str());
						}

						if ((variant.tagsTo.fragmentTags != TAG_STATE_EMPTY) && fragToDef)
						{
							CryStackStringT<char, 1024> sFragmentTags;
							fragToDef->FlagsToTagList(variant.tagsTo.fragmentTags, sFragmentTags);
							xmlFragmentVariant->setAttr("toFrag", sFragmentTags.c_str());
						}

						const uint32 numBlends = variant.blendList.size();
						for (uint32 b = 0; b < numBlends; ++b)
						{
							const SFragmentBlend& blend = variant.blendList[b];

							XmlNodeRef xmlFragment = xmlFragmentVariant->createNode("Fragment");
							FragmentToXML(xmlFragment, blend.pFragment, true);
							xmlFragmentVariant->addChild(xmlFragment);
							xmlFragment->setAttr("selectTime", blend.selectTime);
							if (blend.startTime != 0.0f)
							{
								xmlFragment->setAttr("startTime", blend.startTime);
							}
							xmlFragment->setAttr("enterTime", blend.enterTime);

							if (blend.flags != 0)
							{
								CryStackStringT<char, 1024> sFlags;
								m_transitionFlags.IntegerFlagsToTagList(blend.flags, sFlags);
								xmlFragment->setAttr("flags", sFlags.c_str());
							}
						}
					}
				}

				if (hasAnyEntries)
				{
					fragmentBlendList->addChild(pFragment);
				}
			}
		}

		if (hasAnyBlends)
		{
			root->addChild(fragmentBlendList);
		}
	}

	return root;
}

SControllerDef* CAnimationControllerDefLibrary::LoadControllerDef(const XmlNodeRef& root, const char* context)
{
	if (stricmp(root->getTag(), "ControllerDef") != 0)
	{
		return NULL;
	}

	const char* szBuffer;
	const CTagDefinition* tagTD = NULL;
	const CTagDefinition* fragmentTD = NULL;
	CTagDefinition scopeTD;

	XmlNodeRef tagList = root->findChild("Tags");
	szBuffer = tagList ? tagList->getAttr("filename") : NULL;
	if (szBuffer)
	{
		tagTD = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager().LoadTagDefs(szBuffer, true);
	}
	XmlNodeRef fragList = root->findChild("Fragments");
	szBuffer = fragList ? fragList->getAttr("filename") : NULL;
	if (szBuffer)
	{
		fragmentTD = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager().LoadTagDefs(szBuffer, false);
	}

	{
		XmlNodeRef scopeDefList = root->findChild("ScopeDefs");
		if (scopeDefList)
		{
			const int scopeCount = scopeDefList->getChildCount();
			for (int i = 0; i < scopeCount; ++i)
			{
				XmlNodeRef xmlScopeNode = scopeDefList->getChild(i);
				const char* const scopeName = xmlScopeNode->getTag();
				scopeTD.AddTag(scopeName);
			}
			scopeTD.AssignBits();
		}
	}

	if (tagTD && fragmentTD)
	{
		SControllerDef* ret = new SControllerDef(*tagTD, *fragmentTD, scopeTD);

		ret->m_fragmentDef.resize(fragmentTD->GetNum());
		ret->m_scopeDef.resize(scopeTD.GetNum());

		XmlNodeRef subContextList = root->findChild("SubContexts");
		const uint32 numSubContexts = subContextList ? subContextList->getChildCount() : 0;
		for (uint32 i = 0; i < numSubContexts; i++)
		{
			XmlNodeRef xmlSubContextEntry = subContextList->getChild(i);
			ret->m_subContextIDs.AddTag(xmlSubContextEntry->getTag());

			SSubContext subContext;
			tagTD->TagListToFlags(xmlSubContextEntry->getAttr("tags"), subContext.additionalTags);
			scopeTD.TagListToIntegerFlags(xmlSubContextEntry->getAttr("scopes"), subContext.scopeMask);
			ret->m_subContext.push_back(subContext);
		}

		XmlNodeRef fragDefList = root->findChild("FragmentDefs");
		DynArray<bool> loadedFragmentDefs(fragmentTD->GetNum(), false);
		const uint32 numFragDefs = fragDefList ? fragDefList->getChildCount() : 0;
		for (uint32 i = 0; i < numFragDefs; ++i)
		{
			XmlNodeRef xmlFragDefEntry = fragDefList->getChild(i);

			uint32 fragID = fragmentTD->Find(xmlFragDefEntry->getTag());
			if (fragID != FRAGMENT_ID_INVALID)
			{
				if (!loadedFragmentDefs[fragID])
				{
					loadedFragmentDefs[fragID] = true;
					const char* fragDefFlags = xmlFragDefEntry->getAttr("flags");
					ActionScopes scopeMask = 0;
					scopeTD.TagListToIntegerFlags(xmlFragDefEntry->getAttr("scopes"), scopeMask);
					SFragmentDef& fragmentDef = ret->m_fragmentDef[fragID];
					SFragTagState fragTagState;
					fragmentDef.scopeMaskList.Insert(fragTagState, scopeMask);
					if (fragDefFlags && fragDefFlags[0])
					{
						m_fragmentFlags.TagListToIntegerFlags(fragDefFlags, fragmentDef.flags);
					}
					const CTagDefinition* pFragTagDef = fragmentTD->GetSubTagDefinition(fragID);

					const uint32 numTags = xmlFragDefEntry->getChildCount();
					for (uint32 t = 0; t < numTags; ++t)
					{
						XmlNodeRef xmlFragTag = xmlFragDefEntry->getChild(t);

						const char* tags = xmlFragTag->getAttr("tags");
						const char* fragTags = xmlFragTag->getAttr("fragTags");
						const char* scopes = xmlFragTag->getAttr("scopes");

						bool success = tagTD->TagListToFlags(tags, fragTagState.globalTags);
						fragTagState.fragmentTags = TAG_STATE_EMPTY;
						if (pFragTagDef && fragTags)
						{
							success = success && pFragTagDef->TagListToFlags(fragTags, fragTagState.fragmentTags);
						}
						success = success && scopeTD.TagListToIntegerFlags(scopes, scopeMask);

						if (success)
						{
							fragmentDef.scopeMaskList.Insert(fragTagState, scopeMask);
						}
					}

					fragmentDef.scopeMaskList.Sort(*tagTD, pFragTagDef);
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Skipping duplicate fragment '%s' in %s", xmlFragDefEntry->getTag(), context);
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Skipping unknown fragment '%s' in %s", xmlFragDefEntry->getTag(), context);
			}
		}

		//--- Load in explicit scope contexts if they exist
		XmlNodeRef scopeContextDefList = root->findChild("ScopeContextDefs");
		if (scopeContextDefList)
		{
			const uint32 numScopeContextDefs = scopeContextDefList->getChildCount();
			ret->m_scopeContextDef.reserve(numScopeContextDefs);
			for (uint32 i = 0; i < numScopeContextDefs; ++i)
			{
				XmlNodeRef xmlScopeContextDefEntry = scopeContextDefList->getChild(i);
				const char* contextName = xmlScopeContextDefEntry->getTag();
				if (ret->m_scopeContexts.Find(contextName) >= 0)
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Duplicate scope context '%s' referenced in scopecontextdefs in %s", contextName, context);
				}
				else
				{
					TagID contextID = ret->m_scopeContexts.AddTag(contextName);
					ret->m_scopeContextDef.push_back();
					SScopeContextDef& contextDef = ret->m_scopeContextDef[contextID];
					if (xmlScopeContextDefEntry->haveAttr("tags"))
					{
						tagTD->TagListToFlags(xmlScopeContextDefEntry->getAttr("tags"), contextDef.additionalTags);
					}
					if (xmlScopeContextDefEntry->haveAttr("sharedTags"))
					{
						tagTD->TagListToFlags(xmlScopeContextDefEntry->getAttr("sharedTags"), contextDef.sharedTags);
					}
				}
			}
		}

		XmlNodeRef scopeDefList = root->findChild("ScopeDefs");
		DynArray<bool> loadedScopeDefs(scopeTD.GetNum(), false);
		const uint32 numScopeDefs = scopeDefList ? scopeDefList->getChildCount() : 0;

		assert(numScopeDefs <= sizeof(ActionScopes) * 8);

		for (uint32 i = 0; i < numScopeDefs; ++i)
		{
			XmlNodeRef xmlScopeDefEntry = scopeDefList->getChild(i);

			int scopeID = scopeTD.Find(xmlScopeDefEntry->getTag());
			if (scopeID < 0)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Skipping unknown scope '%s' referenced in scopedefs in %s", xmlScopeDefEntry->getTag(), context);
			}
			else
			{
				if (!loadedScopeDefs[scopeID])
				{
					xmlScopeDefEntry->getAttr("layer", ret->m_scopeDef[scopeID].layer);
					xmlScopeDefEntry->getAttr("numLayers", ret->m_scopeDef[scopeID].numLayers);
					const char* szScopeAlias = xmlScopeDefEntry->getAttr("scopeAlias");
					ret->m_scopeDef[scopeID].scopeAlias = (szScopeAlias && szScopeAlias[0]) ? szScopeAlias : scopeTD.GetTagName(scopeID);
					const char* contextName = xmlScopeDefEntry->getAttr("context");
					tagTD->TagListToFlags(xmlScopeDefEntry->getAttr("Tags"), ret->m_scopeDef[scopeID].additionalTags);
					int contextID = ret->m_scopeContexts.Find(contextName);
					if (contextID < 0)
					{
						contextID = ret->m_scopeContexts.AddTag(contextName);
						ret->m_scopeContextDef.push_back();
					}
					ret->m_scopeDef[scopeID].context = contextID;
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Skipping duplicate scope '%s' in %s", xmlScopeDefEntry->getTag(), context);
				}
			}
		}

		return ret;
	}

	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Tags, Fragments or Scopes missing in %s", context);
	return NULL;
}

XmlNodeRef CAnimationDatabaseManager::SaveControllerDef(const SControllerDef& controllerDef) const
{
	XmlNodeRef root = GetISystem()->CreateXmlNode("ControllerDef");

	XmlNodeRef tagList = root->createNode("Tags");
	root->addChild(tagList);
	tagList->setAttr("filename", controllerDef.m_tags.GetFilename());

	XmlNodeRef fragList = root->createNode("Fragments");
	root->addChild(fragList);
	fragList->setAttr("filename", controllerDef.m_fragmentIDs.GetFilename());

	const uint32 numTagContexts = controllerDef.m_subContextIDs.GetNum();
	if (numTagContexts > 0)
	{
		XmlNodeRef subContextList = root->createNode("SubContexts");
		root->addChild(subContextList);

		for (uint32 i = 0; i < numTagContexts; i++)
		{
			const SSubContext& subContext = controllerDef.m_subContext[i];
			XmlNodeRef xmlSubContextEntry = root->createNode(controllerDef.m_subContextIDs.GetTagName(i));
			subContextList->addChild(xmlSubContextEntry);

			if (subContext.scopeMask != ACTION_SCOPES_NONE)
			{
				CryStackStringT<char, 1024> sScopeMask;
				controllerDef.m_scopeIDs.IntegerFlagsToTagList(subContext.scopeMask, sScopeMask);
				xmlSubContextEntry->setAttr("scopes", sScopeMask.c_str());
			}

			if (subContext.additionalTags != TAG_STATE_EMPTY)
			{
				CryStackStringT<char, 1024> sAdditionalTags;
				controllerDef.m_tags.FlagsToTagList(subContext.additionalTags, sAdditionalTags);
				xmlSubContextEntry->setAttr("tags", sAdditionalTags.c_str());
			}
		}
	}

	XmlNodeRef tagDefList = root->createNode("FragmentDefs");
	root->addChild(tagDefList);
	const uint32 numFrags = controllerDef.m_fragmentDef.size();
	for (uint32 i = 0; i < numFrags; ++i)
	{
		const SFragmentDef& fragDef = controllerDef.m_fragmentDef[i];
		const CTagDefinition* pFragTagDef = controllerDef.GetFragmentTagDef(i);
		ActionScopes scopeMask = fragDef.scopeMaskList.GetDefault();

		XmlNodeRef xmlFragDef = tagDefList->createNode(controllerDef.m_fragmentIDs.GetTagName(i));
		tagDefList->addChild(xmlFragDef);

		if (scopeMask != 0)
		{
			CryStackStringT<char, 1024> sScopeMask;
			controllerDef.m_scopeIDs.IntegerFlagsToTagList(scopeMask, sScopeMask);
			xmlFragDef->setAttr("scopes", sScopeMask.c_str());
		}

		//if (fragDef.tagDef)
		//{
		//	xmlFragDef->setAttr("tags", fragDef.tagDef->GetFilename());
		//}
		if (fragDef.flags)
		{
			CryStackStringT<char, 1024> sFlags;
			m_fragmentFlags.IntegerFlagsToTagList(fragDef.flags, sFlags);
			xmlFragDef->setAttr("flags", sFlags.c_str());
		}

		const uint32 numTags = fragDef.scopeMaskList.Size();
		for (uint32 t = 0; t < numTags - 1; ++t)
		{
			const SFragTagState& fragTagState = fragDef.scopeMaskList.m_keys[t];
			const ActionScopes& actionScope = fragDef.scopeMaskList.m_values[t];

			XmlNodeRef xmlOverrideDef = tagDefList->createNode("Override");
			xmlFragDef->addChild(xmlOverrideDef);

			CryStackStringT<char, 1024> sGlobalTags;
			controllerDef.m_tags.FlagsToTagList(fragTagState.globalTags, sGlobalTags);
			xmlOverrideDef->setAttr("tags", sGlobalTags.c_str());

			if (pFragTagDef && (fragTagState.fragmentTags != TAG_STATE_EMPTY))
			{
				CryStackStringT<char, 1024> sFragmentTags;
				pFragTagDef->FlagsToTagList(fragTagState.fragmentTags, sFragmentTags);
				xmlOverrideDef->setAttr("fragTags", sFragmentTags.c_str());
			}

			CryStackStringT<char, 1024> sActionScope;
			controllerDef.m_scopeIDs.IntegerFlagsToTagList(actionScope, sActionScope);
			xmlOverrideDef->setAttr("scopes", sActionScope.c_str());
		}
	}

	XmlNodeRef scopeContextDefList = root->createNode("ScopeContextDefs");
	root->addChild(scopeContextDefList);
	const uint32 numScopeContexts = controllerDef.m_scopeContextDef.size();
	for (uint32 i = 0; i < numScopeContexts; ++i)
	{
		const SScopeContextDef& scopeContextDef = controllerDef.m_scopeContextDef[i];
		XmlNodeRef xmlScopeContextDef = scopeContextDefList->createNode(controllerDef.m_scopeContexts.GetTagName(i));
		scopeContextDefList->addChild(xmlScopeContextDef);
		if (scopeContextDef.additionalTags != TAG_STATE_EMPTY)
		{
			CryStackStringT<char, 1024> sAdditionalTags;
			controllerDef.m_tags.FlagsToTagList(scopeContextDef.additionalTags, sAdditionalTags);
			xmlScopeContextDef->setAttr("tags", sAdditionalTags.c_str());
		}
		if (scopeContextDef.sharedTags != TAG_STATE_FULL)
		{
			CryStackStringT<char, 1024> sSharedTags;
			controllerDef.m_tags.FlagsToTagList(scopeContextDef.sharedTags, sSharedTags);
			xmlScopeContextDef->setAttr("sharedTags", sSharedTags.c_str());
		}
	}

	XmlNodeRef scopeDefList = root->createNode("ScopeDefs");
	root->addChild(scopeDefList);
	const uint32 numScopes = controllerDef.m_scopeDef.size();
	for (uint32 i = 0; i < numScopes; ++i)
	{
		const SScopeDef& scopeDef = controllerDef.m_scopeDef[i];

		XmlNodeRef xmlScopeDef = scopeDefList->createNode(controllerDef.m_scopeIDs.GetTagName(i));
		scopeDefList->addChild(xmlScopeDef);
		xmlScopeDef->setAttr("layer", scopeDef.layer);
		xmlScopeDef->setAttr("numLayers", scopeDef.numLayers);
		if (0 != strcmp(scopeDef.scopeAlias.c_str(), controllerDef.m_scopeIDs.GetTagName(i)))
		{
			xmlScopeDef->setAttr("scopeAlias", scopeDef.scopeAlias.c_str());
		}
		const char* scopeContextName = controllerDef.m_scopeContexts.GetTagName(scopeDef.context);
		xmlScopeDef->setAttr("context", scopeContextName);
		if (scopeDef.additionalTags != TAG_STATE_EMPTY)
		{
			CryStackStringT<char, 1024> sAdditionalTags;
			controllerDef.m_tags.FlagsToTagList(scopeDef.additionalTags, sAdditionalTags);
			xmlScopeDef->setAttr("Tags", sAdditionalTags.c_str());
		}
	}
	return root;
}

EModifyFragmentIdResult CAnimationDatabaseManager::CreateFragmentID(const CTagDefinition& inFragmentIds, const char* szFragmentIdName)
{
	const bool validFragmentIdName = IsValidNameIdentifier(szFragmentIdName);
	if (!validFragmentIdName)
	{
		return eMFIR_InvalidNameIdentifier;
	}

	const char* fragmentDefFilename = inFragmentIds.GetFilename();
	const uint32 fragmentDefFilenameCrc = CCrc32::ComputeLowercase(fragmentDefFilename);

	CTagDefinition* fragmentDefs = stl::find_in_map(m_tagDefs, fragmentDefFilenameCrc, NULL);
	assert(fragmentDefs);
	if (!fragmentDefs)
	{
		return eMFIR_UnknownInputTagDefinition;
	}

	const int fragmentTagId = fragmentDefs->Find(szFragmentIdName);
	const bool fragmentTagIdFound = (fragmentTagId != FRAGMENT_ID_INVALID);
	if (fragmentTagIdFound)
	{
		return eMFIR_DuplicateName;
	}

	const int newFragmentTagId = fragmentDefs->AddTag(szFragmentIdName);
	assert(newFragmentTagId != FRAGMENT_ID_INVALID);

	for (TControllerDefList::const_iterator cit = m_controllerDefs.begin(); cit != m_controllerDefs.end(); ++cit)
	{
		SControllerDef* controllerDef = cit->second;
		const char* controllerFragmentDefFilename = controllerDef->m_fragmentIDs.GetFilename();
		const uint32 controllerFragmentDefFilenameCrc = CCrc32::ComputeLowercase(controllerFragmentDefFilename);
		const bool usingSameFragmentDef = (controllerFragmentDefFilenameCrc == fragmentDefFilenameCrc);
		if (usingSameFragmentDef)
		{
			controllerDef->m_fragmentDef.push_back(SFragmentDef());
		}
	}

	for (TAnimDatabaseList::const_iterator cit = m_databases.begin(); cit != m_databases.end(); ++cit)
	{
		CAnimationDatabase* otherAnimDB = cit->second;
		const char* otherDBFragmentDefFilename = otherAnimDB->GetFragmentDefs().GetFilename();
		const uint32 otherDBFragmentDefFilenameCrc = CCrc32::ComputeLowercase(otherDBFragmentDefFilename);
		const bool usingSameFragmentDef = (otherDBFragmentDefFilenameCrc == fragmentDefFilenameCrc);
		if (usingSameFragmentDef)
		{
			CAnimationDatabase::SFragmentEntry* fragmentEntry = new CAnimationDatabase::SFragmentEntry();
			assert(otherAnimDB->m_fragmentList.size() == newFragmentTagId);
			otherAnimDB->m_fragmentList.push_back(fragmentEntry);
		}
	}

	NotifyListenersTagDefinitionChanged(*fragmentDefs);
	return eMFIR_Success;
}

EModifyFragmentIdResult CAnimationDatabaseManager::RenameFragmentID(const CTagDefinition& fragmentIds, FragmentID fragmentID, const char* szFragmentIdName)
{
	if (fragmentID == FRAGMENT_ID_INVALID)
	{
		return eMFIR_InvalidFragmentId;
	}

	const FragmentID fragmentIDCount = fragmentIds.GetNum();
	if (fragmentIDCount <= fragmentID)
	{
		return eMFIR_InvalidFragmentId;
	}

	const bool validFragmentIdName = IsValidNameIdentifier(szFragmentIdName);
	if (!validFragmentIdName)
	{
		return eMFIR_InvalidNameIdentifier;
	}

	const FragmentID foundFragmentIdWithDesiredName = fragmentIds.Find(szFragmentIdName);
	const bool duplicateFragmentIdName = ((foundFragmentIdWithDesiredName != FRAGMENT_ID_INVALID) && (foundFragmentIdWithDesiredName != fragmentID));
	if (duplicateFragmentIdName)
	{
		return eMFIR_DuplicateName;
	}

	const char* fragmentDefFilename = fragmentIds.GetFilename();
	const uint32 fragmentDefFilenameCrc = CCrc32::ComputeLowercase(fragmentDefFilename);

	CTagDefinition* fragmentDefs = stl::find_in_map(m_tagDefs, fragmentDefFilenameCrc, NULL);
	assert(fragmentDefs);
	assert(fragmentDefs == &fragmentIds);
	if (!fragmentDefs)
	{
		return eMFIR_UnknownInputTagDefinition;
	}

	fragmentDefs->SetTagName(fragmentID, szFragmentIdName);

	NotifyListenersTagDefinitionChanged(*fragmentDefs);
	return eMFIR_Success;
}

EModifyFragmentIdResult CAnimationDatabaseManager::DeleteFragmentID(const CTagDefinition& fragmentIds, FragmentID fragmentID)
{
	if (fragmentID == FRAGMENT_ID_INVALID)
		return eMFIR_InvalidFragmentId;

	const FragmentID fragmentIDCount = fragmentIds.GetNum();
	if (fragmentIDCount <= fragmentID)
		return eMFIR_InvalidFragmentId;

	const char* fragmentDefFilename = fragmentIds.GetFilename();
	const uint32 fragmentDefFilenameCrc = CCrc32::ComputeLowercase(fragmentDefFilename);

	CTagDefinition* fragmentDefs = stl::find_in_map(m_tagDefs, fragmentDefFilenameCrc, NULL);
	assert(fragmentDefs);
	assert(fragmentDefs == &fragmentIds);
	if (!fragmentDefs)
		return eMFIR_UnknownInputTagDefinition;

	fragmentDefs->RemoveTag(fragmentID);

	for (TControllerDefList::const_iterator cit = m_controllerDefs.begin(); cit != m_controllerDefs.end(); ++cit)
	{
		SControllerDef* controllerDef = cit->second;
		const char* controllerFragmentDefFilename = controllerDef->m_fragmentIDs.GetFilename();
		const uint32 controllerFragmentDefFilenameCrc = CCrc32::ComputeLowercase(controllerFragmentDefFilename);
		const bool usingSameFragmentDef = (controllerFragmentDefFilenameCrc == fragmentDefFilenameCrc);
		if (usingSameFragmentDef)
		{
			controllerDef->m_fragmentDef.erase(fragmentID);
		}
	}

	for (TAnimDatabaseList::const_iterator it = m_databases.begin(), itEnd = m_databases.end(); it != itEnd; ++it)
	{
		CAnimationDatabase* database = it->second;
		const char* databaseFragmentDefFilename = database->GetFragmentDefs().GetFilename();
		const uint32 databaseFragmentDefFilenameCrc = CCrc32::ComputeLowercase(databaseFragmentDefFilename);
		const bool usingSameFragmentDef = (databaseFragmentDefFilenameCrc == fragmentDefFilenameCrc);
		if (usingSameFragmentDef)
		{
			database->DeleteFragmentID(fragmentID);
		}
	}

	NotifyListenersTagDefinitionChanged(*fragmentDefs);
	return eMFIR_Success;
}

bool CAnimationDatabaseManager::SetFragmentTagDef(const CTagDefinition& fragmentIds, FragmentID fragmentID, const CTagDefinition* pFragTagDefs)
{
	if (fragmentID == FRAGMENT_ID_INVALID)
		return false;

	const char* fragmentDefFilename = fragmentIds.GetFilename();
	const uint32 fragmentDefFilenameCrc = CCrc32::ComputeLowercase(fragmentDefFilename);

	CTagDefinition* fragmentDefs = stl::find_in_map(m_tagDefs, fragmentDefFilenameCrc, NULL);
	assert(fragmentDefs);
	assert(fragmentDefs == &fragmentIds);
	if (fragmentDefs)
	{
		fragmentDefs->SetSubTagDefinition(fragmentID, pFragTagDefs);
		return true;
	}

	return false;
}

void CAnimationDatabaseManager::SetFragmentDef(const SControllerDef& inControllerDef, FragmentID fragmentID, const SFragmentDef& fragmentDef)
{
	if (fragmentID == FRAGMENT_ID_INVALID)
	{
		return;
	}

	const char* controllerDefFilename = inControllerDef.m_filename.c_str();
	const uint32 controllerDefFilenameCrc = CCrc32::ComputeLowercase(controllerDefFilename);

	SControllerDef* controllerDef = stl::find_in_map(m_controllerDefs, controllerDefFilenameCrc, NULL);
	assert(controllerDef);
	if (!controllerDef)
	{
		return;
	}

	DynArray<SFragmentDef>& fragmentDefs = controllerDef->m_fragmentDef;
	if (fragmentDefs.size() <= fragmentID)
	{
		return;
	}

	fragmentDefs[fragmentID] = fragmentDef;
}

bool CAnimationDatabaseManager::DeleteFragmentEntry(IAnimationDatabase* pDatabaseInterface, FragmentID fragmentID, const SFragTagState& tagState, uint32 optionIdx, bool logWarning)
{
	assert(pDatabaseInterface);
	CAnimationDatabase* pDatabase = static_cast<CAnimationDatabase*>(pDatabaseInterface);

	bool successfullyDeleted = pDatabase->DeleteEntry(fragmentID, tagState, optionIdx);

	if (logWarning && !successfullyDeleted)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "AnimDatabaseManager: Invalid fragment entry: %d passed to %s", fragmentID, pDatabase->GetFilename());
	}

	int fragCRC = pDatabase->m_pFragDef->GetTagCRC(fragmentID);

	CryStackStringT<char, 1024> sTaglist;
	pDatabase->m_pTagDef->FlagsToTagList(tagState.globalTags, sTaglist);
	SFragTagState targetTagState = tagState;

	const std::vector<CAnimationDatabase*>& impactedDatabases = FindImpactedDatabases(pDatabase, fragmentID, tagState.globalTags);
	for (std::vector<CAnimationDatabase*>::const_iterator itDatabases = impactedDatabases.begin(); itDatabases != impactedDatabases.end(); ++itDatabases)
	{
		CAnimationDatabase* pCurrDatabase = *itDatabases;
		if (pCurrDatabase != pDatabase)
		{
			pCurrDatabase->m_pTagDef->TagListToFlags(sTaglist.c_str(), targetTagState.globalTags);
			FragmentID targetFragID = pCurrDatabase->m_pFragDef->Find(fragCRC);

			bool success = pCurrDatabase->DeleteEntry(targetFragID, targetTagState, optionIdx);
			if (logWarning && !success)
			{
				CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "AnimDatabaseManager: Invalid fragment entry: %d passed to %s", fragmentID, pCurrDatabase->GetFilename());
			}
		}
	}

	return successfullyDeleted;
}

uint32 CAnimationDatabaseManager::AddFragmentEntry(IAnimationDatabase* pDatabaseInterface, FragmentID fragmentID, const SFragTagState& tagState, const CFragment& fragment)
{
	assert(pDatabaseInterface);
	CAnimationDatabase* pDatabase = static_cast<CAnimationDatabase*>(pDatabaseInterface);

	uint32 idxRoot = pDatabase->AddEntry(fragmentID, tagState, fragment);

	int fragCRC = pDatabase->m_pFragDef->GetTagCRC(fragmentID);

	CryStackStringT<char, 1024> sTaglist;
	pDatabase->m_pTagDef->FlagsToTagList(tagState.globalTags, sTaglist);
	SFragTagState targetTagState = tagState;

	const std::vector<CAnimationDatabase*>& impactedDatabases = FindImpactedDatabases(pDatabase, fragmentID, tagState.globalTags);
	for (std::vector<CAnimationDatabase*>::const_iterator itDatabases = impactedDatabases.begin(); itDatabases != impactedDatabases.end(); ++itDatabases)
	{
		CAnimationDatabase* pCurrDatabase = *itDatabases;
		if (pCurrDatabase != pDatabase)
		{
			pCurrDatabase->m_pTagDef->TagListToFlags(sTaglist.c_str(), targetTagState.globalTags);
			FragmentID targetFragID = pCurrDatabase->m_pFragDef->Find(fragCRC);

			uint32 currIdx = pCurrDatabase->AddEntry(targetFragID, targetTagState, fragment);
			CRY_ASSERT(currIdx == idxRoot);
		}
	}

	return idxRoot;
}

void CAnimationDatabaseManager::SetFragmentEntry(IAnimationDatabase* pDatabaseInterface, FragmentID fragmentID, const SFragTagState& tagState, uint32 optionIdx, const CFragment& fragment)
{
	assert(pDatabaseInterface);
	CAnimationDatabase* pDatabase = static_cast<CAnimationDatabase*>(pDatabaseInterface);

	int fragCRC = pDatabase->m_pFragDef->GetTagCRC(fragmentID);
	CryStackStringT<char, 1024> sTaglist;
	pDatabase->m_pTagDef->FlagsToTagList(tagState.globalTags, sTaglist);
	SFragTagState targetTagState = tagState;

	const std::vector<CAnimationDatabase*>& impactedDatabases = FindImpactedDatabases(pDatabase, fragmentID, tagState.globalTags);
	for (std::vector<CAnimationDatabase*>::const_iterator itDatabases = impactedDatabases.begin(); itDatabases != impactedDatabases.end(); ++itDatabases)
	{
		CAnimationDatabase* pCurrDatabase = *itDatabases;

		pCurrDatabase->m_pTagDef->TagListToFlags(sTaglist.c_str(), targetTagState.globalTags);
		FragmentID targetFragID = pCurrDatabase->m_pFragDef->Find(fragCRC);

		pCurrDatabase->SetEntry(targetFragID, targetTagState, optionIdx, fragment);
	}
}

std::vector<CAnimationDatabase*> CAnimationDatabaseManager::FindImpactedDatabases(const CAnimationDatabase* pWorkingDatabase, FragmentID fragmentID, TagState globalTags) const
{
	// Find fragment owner database
	string ownerADB;
	FindRootSubADB(*pWorkingDatabase, NULL, fragmentID, globalTags, ownerADB);
	CRY_ASSERT(ownerADB.empty() == false);
	if (ownerADB.empty())
	{
		ownerADB = pWorkingDatabase->GetFilename();
	}

	// Add all databases including the root one
	std::vector<CAnimationDatabase*> databases;
	for (TAnimDatabaseList::const_iterator itDatabases = m_databases.begin(); itDatabases != m_databases.end(); ++itDatabases)
	{
		CAnimationDatabase* pCurrDatabase = itDatabases->second;
		if (HasAncestor(*pCurrDatabase, ownerADB))
		{
			databases.push_back(pCurrDatabase);
		}
	}

	CRY_ASSERT(false == databases.empty());
	IF_UNLIKELY (databases.empty())
	{
		databases.push_back(const_cast<CAnimationDatabase*>(pWorkingDatabase));
	}

	return databases;
}

bool CAnimationDatabaseManager::HasAncestor(const CAnimationDatabase& database, const string& ancestorName) const
{
	if (database.GetFilename() == ancestorName)
		return true;

	for (CAnimationDatabase::TSubADBList::const_iterator itSubADBs = database.m_subADBs.begin(); itSubADBs != database.m_subADBs.end(); ++itSubADBs)
	{
		if (HasAncestor(*itSubADBs, ancestorName))
		{
			return true;
		}
	}
	return false;
}

bool CAnimationDatabaseManager::HasAncestor(const CAnimationDatabase::SSubADB& database, const string& ancestorName) const
{
	if (database.filename == ancestorName)
	{
		return true;
	}

	for (CAnimationDatabase::TSubADBList::const_iterator itSubADBs = database.subADBs.begin(); itSubADBs != database.subADBs.end(); ++itSubADBs)
	{
		if (HasAncestor(*itSubADBs, ancestorName))
		{
			return true;
		}
	}
	return false;
}

void CAnimationDatabaseManager::FindRootSubADB(const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* pSubADB, FragmentID fragID, TagState tagState, string& outRootADB) const
{
	if (false == CanSaveFragmentID(fragID, animDB, pSubADB))
	{
		return;
	}

	bool tagStateMatch = false;
	if (ShouldSaveFragment(fragID, tagState, animDB, pSubADB, tagStateMatch))
	{
		if (tagStateMatch || outRootADB.empty())
		{
			outRootADB = pSubADB ? pSubADB->filename : animDB.m_filename;
		}
	}

	const CAnimationDatabase::TSubADBList& vSubADBs = pSubADB ? pSubADB->subADBs : animDB.m_subADBs;
	for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = vSubADBs.begin(); itSubADB != vSubADBs.end(); ++itSubADB)
	{
		FindRootSubADB(animDB, &*itSubADB, fragID, tagState, outRootADB);
	}
}

void CAnimationDatabaseManager::PrepareSave(const CAnimationDatabase& animDB, const CAnimationDatabase::SSubADB* subAnimDB, TFragmentSaveList& vFragSaveList, TFragmentBlendSaveDatabase& mBlendSaveDatabase) const
{
	vFragSaveList.resize(animDB.m_fragmentList.size());
	for (uint32 i = 0; i < animDB.m_fragmentList.size(); ++i)
	{
		if (CanSaveFragmentID(i, animDB, subAnimDB))
		{
			CAnimationDatabase::SFragmentEntry* entry = animDB.m_fragmentList[i];
			uint32 size = 0;
			if (entry)
				size = entry->tagSetList.Size();
			if (size > 0)
			{
				vFragSaveList[i].vSaveStates.resize(size);
				for (uint32 k = 0; k < size; ++k)
				{
					const SFragTagState& tagState = entry->tagSetList.m_keys[k];
					bool bTagMatchFound = false;
					const bool bAddFragment = ShouldSaveFragment(i, tagState.globalTags, animDB, subAnimDB, bTagMatchFound);
					if (bAddFragment)
					{
						if (vFragSaveList[i].vSaveStates[k].m_sFileName.empty())
						{
							vFragSaveList[i].vSaveStates[k].m_bSavedByTags = bTagMatchFound;
							vFragSaveList[i].vSaveStates[k].m_sFileName = subAnimDB ? subAnimDB->filename : animDB.m_filename;
						}
					}
				}
			}
		}
	}

	if (!animDB.m_fragmentBlendDB.empty())
	{
		for (CAnimationDatabase::TFragmentBlendDatabase::const_iterator iter = animDB.m_fragmentBlendDB.begin(); iter != animDB.m_fragmentBlendDB.end(); ++iter)
		{
			const FragmentID fragIDFrom = iter->first.fragFrom;
			const FragmentID fragIDTo = iter->first.fragTo;

			if (CanSaveFragmentID(fragIDFrom, animDB, subAnimDB) && CanSaveFragmentID(fragIDTo, animDB, subAnimDB))
			{
				const uint32 numVars = iter->second.variantList.size();
				mBlendSaveDatabase[iter->first].vSaveStates.resize(numVars);
				for (uint32 v = 0; v < numVars; ++v)
				{
					const CAnimationDatabase::SFragmentBlendVariant& variant = iter->second.variantList[v];

					bool bTagMatchFound = false;
					bool addBlend = ShouldSaveTransition(fragIDFrom, fragIDTo, variant.tagsFrom.globalTags, variant.tagsTo.globalTags, animDB, subAnimDB, bTagMatchFound);
					if (addBlend)
					{
						if (mBlendSaveDatabase[iter->first].vSaveStates[v].m_sFileName.empty() || bTagMatchFound)
						{
							mBlendSaveDatabase[iter->first].vSaveStates[v].m_bSavedByTags = bTagMatchFound;
							mBlendSaveDatabase[iter->first].vSaveStates[v].m_sFileName = subAnimDB ? subAnimDB->filename : animDB.m_filename;
						}
					}
				}
			}
		}
	}
	const CAnimationDatabase::TSubADBList& vSubADBs = subAnimDB ? subAnimDB->subADBs : animDB.m_subADBs;
	for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = vSubADBs.begin(); itSubADB != vSubADBs.end(); ++itSubADB)
		PrepareSave(animDB, &(*itSubADB), vFragSaveList, mBlendSaveDatabase);

}

void CAnimationDatabaseManager::SaveDatabasesSnapshot(SSnapshotCollection& snapshotCollection) const
{
	snapshotCollection.m_databases.clear();
	snapshotCollection.m_databases.reserve(m_databases.size());
	snapshotCollection.m_controllerDefs.clear();
	snapshotCollection.m_controllerDefs.reserve(m_controllerDefs.size());

	for (TAnimDatabaseList::const_iterator it = m_databases.begin(), itEnd = m_databases.end(); it != itEnd; ++it)
	{
		CAnimationDatabase* animDB = (CAnimationDatabase*)it->second;
		TFragmentSaveList vFragSaveList;
		TFragmentBlendSaveDatabase mBlendSaveDatabase;

		PrepareSave(*animDB, NULL, vFragSaveList, mBlendSaveDatabase);

		SAnimDBSnapshot snapshot;
		snapshot.pDatabase = animDB;
		snapshot.xmlData = SaveDatabase(*animDB, NULL, vFragSaveList, mBlendSaveDatabase, true);

		snapshotCollection.m_databases.push_back(snapshot);
	}

	for (TControllerDefList::const_iterator it = m_controllerDefs.begin(), itEnd = m_controllerDefs.end(); it != itEnd; ++it)
	{
		SControllerDef* pControllerDef = (SControllerDef*)it->second;

		SAnimControllerDefSnapshot snapshot;
		snapshot.pControllerDef = pControllerDef;
		snapshot.xmlData = SaveControllerDef(*pControllerDef);

		snapshotCollection.m_controllerDefs.push_back(snapshot);
	}
}

void CAnimationDatabaseManager::LoadDatabasesSnapshot(const SSnapshotCollection& snapshotCollection)
{
	for (TAnimDBSnapshotCollection::const_iterator it = snapshotCollection.m_databases.begin(), itEnd = snapshotCollection.m_databases.end(); it != itEnd; ++it)
	{
		CAnimationDatabase* animDB = (CAnimationDatabase*)it->pDatabase;
		const XmlNodeRef& xmlData = it->xmlData;

		ClearDatabase(animDB);

		LoadDatabase(xmlData, *animDB, false);
	}

	for (TAnimControllerDefSnapshotCollection::const_iterator it = snapshotCollection.m_controllerDefs.begin(), itEnd = snapshotCollection.m_controllerDefs.end(); it != itEnd; ++it)
	{
		SControllerDef* pControllerDef = (SControllerDef*)it->pControllerDef;
		const XmlNodeRef& xmlData = it->xmlData;

		TDefPathString defFilename = pControllerDef->m_filename;
		SControllerDef* pNewControllerDef = CAnimationControllerDefLibrary::LoadControllerDef(xmlData, "");
		new(pControllerDef) SControllerDef(*pNewControllerDef);
		pControllerDef->m_filename = defFilename;
		delete pNewControllerDef;
	}
}

void CAnimationDatabaseManager::GetLoadedTagDefs(DynArray<const CTagDefinition*>& tagDefs)
{
	tagDefs.clear();
	tagDefs.reserve(m_tagDefs.size());

	for (TTagDefList::const_iterator cit = m_tagDefs.begin(); cit != m_tagDefs.end(); ++cit)
	{
		const CTagDefinition* pTagDefinition = cit->second;
		tagDefs.push_back(pTagDefinition);
	}
}

void CAnimationDatabaseManager::GetLoadedDatabases(DynArray<const IAnimationDatabase*>& animDatabases) const
{
	animDatabases.clear();
	animDatabases.reserve(m_databases.size());

	for (TAnimDatabaseList::const_iterator cit = m_databases.begin(); cit != m_databases.end(); ++cit)
	{
		CAnimationDatabase* pAnimationDatabase = cit->second;
		animDatabases.push_back(pAnimationDatabase);
	}
}

void CAnimationDatabaseManager::GetLoadedControllerDefs(DynArray<const SControllerDef*>& controllerDefs) const
{
	controllerDefs.clear();
	controllerDefs.reserve(m_controllerDefs.size());

	for (TControllerDefList::const_iterator cit = m_controllerDefs.begin(); cit != m_controllerDefs.end(); ++cit)
	{
		const SControllerDef* pControllerDefs = cit->second;
		controllerDefs.push_back(pControllerDefs);
	}
}

void CAnimationDatabaseManager::SaveAll(IMannequinWriter* pWriter) const
{
	if (!pWriter)
	{
		return;
	}

	for (TAnimDatabaseList::const_iterator cit = m_databases.begin(); cit != m_databases.end(); ++cit)
	{
		CAnimationDatabase* pAnimationDatabase = cit->second;
		SaveDatabase(pWriter, pAnimationDatabase);
	}

	for (TControllerDefList::const_iterator cit = m_controllerDefs.begin(); cit != m_controllerDefs.end(); ++cit)
	{
		SControllerDef* pControllerDef = cit->second;
		SaveControllerDef(pWriter, pControllerDef);
	}

	for (TTagDefList::const_iterator cit = m_tagDefs.begin(); cit != m_tagDefs.end(); ++cit)
	{
		CTagDefinition* pTagDefinition = cit->second;
		SaveTagDefinition(pWriter, pTagDefinition);
	}

	pWriter->WriteModifiedFiles();
}

void CAnimationDatabaseManager::SaveSubADB
(
  IMannequinWriter* pWriter,
  const CAnimationDatabase& animationDatabase,
  const CAnimationDatabase::SSubADB& subADB,
  const TFragmentSaveList& vFragSaveList,
  const TFragmentBlendSaveDatabase& mBlendSaveDatabase

) const
{
	XmlNodeRef xmlAnimationDatabase = SaveDatabase(animationDatabase, &subADB, vFragSaveList, mBlendSaveDatabase);
	assert(xmlAnimationDatabase != 0);
	if (!xmlAnimationDatabase)
	{
		return;
	}

	const char* szFilename = subADB.filename;
	pWriter->SaveFile(szFilename, xmlAnimationDatabase, eFET_Database);

	for (CAnimationDatabase::TSubADBList::const_iterator itSubADB = subADB.subADBs.begin(); itSubADB != subADB.subADBs.end(); ++itSubADB)
		SaveSubADB(pWriter, animationDatabase, *itSubADB, vFragSaveList, mBlendSaveDatabase);
}

void CAnimationDatabaseManager::SaveDatabase(IMannequinWriter* pWriter, const IAnimationDatabase* pIAnimationDatabase) const
{
	const CAnimationDatabase* pAnimationDatabase = static_cast<const CAnimationDatabase*>(pIAnimationDatabase);
	assert(pWriter);
	assert(pAnimationDatabase);

	TFragmentSaveList vFragSaveList;
	TFragmentBlendSaveDatabase mBlendSaveDatabase;

	PrepareSave(*pAnimationDatabase, NULL, vFragSaveList, mBlendSaveDatabase);

	uint32 numSubADBs = pAnimationDatabase->m_subADBs.size();
	for (uint32 i = 0; i < numSubADBs; ++i)
		SaveSubADB(pWriter, *pAnimationDatabase, pAnimationDatabase->m_subADBs[i], vFragSaveList, mBlendSaveDatabase);

	XmlNodeRef xmlAnimationDatabase = SaveDatabase(*pAnimationDatabase, NULL, vFragSaveList, mBlendSaveDatabase);
	assert(xmlAnimationDatabase != 0);
	if (!xmlAnimationDatabase)
	{
		return;
	}

	const char* szFilename = pAnimationDatabase->GetFilename();
	pWriter->SaveFile(szFilename, xmlAnimationDatabase, eFET_Database);
}

void CAnimationDatabaseManager::SaveControllerDef(IMannequinWriter* pWriter, const SControllerDef* pControllerDef) const
{
	assert(pWriter);
	assert(pControllerDef);

	XmlNodeRef xmlControllerDef = SaveControllerDef(*pControllerDef);
	assert(xmlControllerDef != 0);
	if (!xmlControllerDef)
	{
		return;
	}

	const char* szFilename = pControllerDef->m_filename.c_str();
	pWriter->SaveFile(szFilename, xmlControllerDef, eFET_ControllerDef);
}

void CAnimationDatabaseManager::SaveTagDefinition(IMannequinWriter* pWriter, const CTagDefinition* pTagDefinition) const
{
	assert(pWriter);
	assert(pTagDefinition);

	mannequin::TTagDefinitionSaveDataList saveList;
	mannequin::SaveTagDefinition(*pTagDefinition, saveList);

	for (size_t i = 0; i < saveList.size(); ++i)
	{
		mannequin::STagDefinitionSaveData& saveData = saveList[i];
		const char* szFilename = saveData.filename.c_str();
		XmlNodeRef xmlTagDefinition = saveData.pXmlNode;

		pWriter->SaveFile(szFilename, xmlTagDefinition, eFET_TagDef);
	}
}

bool IsFileUsedByTagDefinition(const char* normalizedFilename, const CTagDefinition* pTagDefinition)
{
	if (!pTagDefinition)
	{
		return false;
	}

	return (stricmp(normalizedFilename, pTagDefinition->GetFilename()) == 0);
}

bool CAnimationDatabaseManager::IsFileUsedByControllerDef(const SControllerDef& controllerDef, const char* szFilename) const
{
	char normalizedFilename[DEF_PATH_LENGTH];
	NormalizeFilename(normalizedFilename, szFilename);

	if (stricmp(controllerDef.m_filename.c_str(), normalizedFilename) == 0)
	{
		return true;
	}

	if (IsFileUsedByTagDefinition(normalizedFilename, &controllerDef.m_tags))
	{
		return true;
	}

	if (IsFileUsedByTagDefinition(normalizedFilename, &controllerDef.m_fragmentIDs))
	{
		return true;
	}

	if (IsFileUsedByTagDefinition(normalizedFilename, &controllerDef.m_scopeContexts))
	{
		return true;
	}

	if (IsFileUsedByTagDefinition(normalizedFilename, &controllerDef.m_scopeIDs))
	{
		return true;
	}

	for (size_t i = 0; i < controllerDef.m_fragmentDef.size(); ++i)
	{
		const CTagDefinition* pFragTagDef = controllerDef.GetFragmentTagDef(i);
		if (IsFileUsedByTagDefinition(normalizedFilename, pFragTagDef))
		{
			return true;
		}
	}

	//for (TAnimDatabaseList::const_iterator cit = m_databases.begin(); cit != m_databases.end(); ++cit)
	//{
	//	const CAnimationDatabase* pAnimDB = cit->second;
	//	if (pAnimDB->m_def == &controllerDef)
	//	{
	//		if (stricmp(normalizedFilename, pAnimDB->GetFilename()) == 0)
	//		{
	//			return true;
	//		}
	//	}
	//}

	return false;
}

void CAnimationDatabaseManager::RegisterListener(IMannequinEditorListener* pListener)
{
	m_editorListenerSet.Add(pListener);
}

void CAnimationDatabaseManager::UnregisterListener(IMannequinEditorListener* pListener)
{
	m_editorListenerSet.Remove(pListener);
}

void CAnimationDatabaseManager::NotifyListenersTagDefinitionChanged(const CTagDefinition& tagDef)
{
	for (TEditorListenerSet::Notifier notifier(m_editorListenerSet); notifier.IsValid(); notifier.Next())
	{
		notifier->OnMannequinTagDefInvalidated(tagDef);
	}

	CCryAction::GetCryAction()->GetMannequinInterface().GetMannequinUserParamsManager().ReloadAll(*this);
}

void CAnimationDatabaseManager::AddSubADBFragmentFilter(IAnimationDatabase* pDatabaseInterface, const char* szSubADBFilename, FragmentID fragmentID)
{
	CRY_ASSERT(szSubADBFilename);
	CRY_ASSERT(pDatabaseInterface);

	for (TAnimDatabaseList::iterator it = m_databases.begin(); it != m_databases.end(); ++it)
	{
		CAnimationDatabase* const pDatabase = it->second;
		CAnimationDatabase::SSubADB* pSubAdb = pDatabase->FindSubADB(szSubADBFilename, true);
		if (pSubAdb)
		{
			stl::push_back_unique(pSubAdb->vFragIDs, fragmentID);
		}
	}

	ReconcileSubDatabases(static_cast<CAnimationDatabase*>(pDatabaseInterface));
}

void CAnimationDatabaseManager::RemoveSubADBFragmentFilter(IAnimationDatabase* pDatabaseInterface, const char* szSubADBFilename, FragmentID fragmentID)
{
	CRY_ASSERT(szSubADBFilename);
	CRY_ASSERT(pDatabaseInterface);

	for (TAnimDatabaseList::iterator it = m_databases.begin(); it != m_databases.end(); ++it)
	{
		CAnimationDatabase* const pDatabase = it->second;
		CAnimationDatabase::SSubADB* pSubAdb = pDatabase->FindSubADB(szSubADBFilename, true);
		if (pSubAdb)
		{
			stl::find_and_erase(pSubAdb->vFragIDs, fragmentID);
		}
	}

	ReconcileSubDatabases(static_cast<CAnimationDatabase*>(pDatabaseInterface));
}

uint32 CAnimationDatabaseManager::GetSubADBFragmentFilterCount(const IAnimationDatabase* pDatabaseInterface, const char* szSubADBFilename) const
{
	CRY_ASSERT(pDatabaseInterface);
	CRY_ASSERT(szSubADBFilename);

	const CAnimationDatabase* const pDatabase = static_cast<const CAnimationDatabase*>(pDatabaseInterface);
	const CAnimationDatabase::SSubADB* pSubADB = pDatabase->FindSubADB(szSubADBFilename, true);
	if (pSubADB)
	{
		return pSubADB->vFragIDs.size();
	}
	return 0;
}

FragmentID CAnimationDatabaseManager::GetSubADBFragmentFilter(const IAnimationDatabase* pDatabaseInterface, const char* szSubADBFilename, uint32 index) const
{
	CRY_ASSERT(pDatabaseInterface);
	CRY_ASSERT(szSubADBFilename);

	const CAnimationDatabase* const pDatabase = static_cast<const CAnimationDatabase*>(pDatabaseInterface);
	const CAnimationDatabase::SSubADB* pSubADB = pDatabase->FindSubADB(szSubADBFilename, true);
	CRY_ASSERT(pSubADB);
	return pSubADB->vFragIDs[index];
}

void CAnimationDatabaseManager::SetSubADBTagFilter(IAnimationDatabase* pDatabaseInterface, const char* szSubADBFilename, TagState tagState)
{
	CRY_ASSERT(szSubADBFilename);
	CRY_ASSERT(pDatabaseInterface);

	for (TAnimDatabaseList::iterator it = m_databases.begin(); it != m_databases.end(); ++it)
	{
		CAnimationDatabase* const pDatabase = it->second;
		CAnimationDatabase::SSubADB* pSubAdb = pDatabase->FindSubADB(szSubADBFilename, true);
		if (pSubAdb)
		{
			pSubAdb->tags = tagState;
			pSubAdb->comparisonMask = pSubAdb->pTagDef->GenerateMask(tagState);
		}
	}

	ReconcileSubDatabases(static_cast<CAnimationDatabase*>(pDatabaseInterface));
}

TagState CAnimationDatabaseManager::GetSubADBTagFilter(const IAnimationDatabase* pDatabaseInterface, const char* szSubADBFilename) const
{
	CRY_ASSERT(pDatabaseInterface);
	CRY_ASSERT(szSubADBFilename);

	const CAnimationDatabase* const pDatabase = static_cast<const CAnimationDatabase*>(pDatabaseInterface);
	const CAnimationDatabase::SSubADB* pSubADB = pDatabase->FindSubADB(szSubADBFilename, true);
	if (pSubADB)
	{
		return pSubADB->tags;
	}
	return TAG_STATE_EMPTY;
}

void CAnimationDatabaseManager::ReconcileSubDatabases(const CAnimationDatabase* pSourceDatabase)
{
	for (TAnimDatabaseList::iterator it = m_databases.begin(); it != m_databases.end(); ++it)
	{
		CAnimationDatabase* const pDatabase = it->second;

		const char* const databaseFilename = pDatabase->GetFilename();
		const bool isSubDatabaseOfSourceDatabase = (pSourceDatabase->FindSubADB(databaseFilename, true) != NULL);
		if (isSubDatabaseOfSourceDatabase)
		{
			ReconcileSubDatabase(pSourceDatabase, pDatabase);
		}
	}
}

void CAnimationDatabaseManager::ReconcileSubDatabase(const CAnimationDatabase* pSourceDatabase, CAnimationDatabase* pTargetSubDatabase)
{
	CRY_ASSERT(pSourceDatabase);
	CRY_ASSERT(pTargetSubDatabase);

	const char* const subADBFilename = pTargetSubDatabase->GetFilename();
	const CAnimationDatabase::SSubADB* pSubADB = pSourceDatabase->FindSubADB(subADBFilename, true);
	if (!pSubADB)
	{
		return;
	}

	TFragmentSaveList vFragSaveList;
	TFragmentBlendSaveDatabase mBlendSaveDatabase;
	PrepareSave(*pSourceDatabase, NULL, vFragSaveList, mBlendSaveDatabase);
	XmlNodeRef xmlSubNode = SaveDatabase(*pSourceDatabase, pSubADB, vFragSaveList, mBlendSaveDatabase);

	pTargetSubDatabase->m_subADBs.clear();
	for (size_t i = 0; i < pTargetSubDatabase->m_fragmentList.size(); ++i)
	{
		delete pTargetSubDatabase->m_fragmentList[i];
	}
	pTargetSubDatabase->m_fragmentList.clear();
	pTargetSubDatabase->m_fragmentBlendDB.clear();

	LoadDatabase(xmlSubNode, *pTargetSubDatabase, true);
}

void CAnimationDatabaseManager::SetBlend(IAnimationDatabase* pIDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid, const SFragmentBlend& fragBlend)
{
	CRY_ASSERT(pIDatabase);

	CAnimationDatabase* pDatabase = static_cast<CAnimationDatabase*>(pIDatabase);
	pDatabase->SetBlend(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo, blendUid, fragBlend);
}

SFragmentBlendUid CAnimationDatabaseManager::AddBlend(IAnimationDatabase* pIDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, const SFragmentBlend& fragBlend)
{
	CRY_ASSERT(pIDatabase);

	CAnimationDatabase* pDatabase = static_cast<CAnimationDatabase*>(pIDatabase);
	return pDatabase->AddBlend(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo, fragBlend);
}

void CAnimationDatabaseManager::DeleteBlend(IAnimationDatabase* pIDatabase, FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid blendUid)
{
	CRY_ASSERT(pIDatabase);

	CAnimationDatabase* pDatabase = static_cast<CAnimationDatabase*>(pIDatabase);
	pDatabase->DeleteBlend(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo, blendUid);
}

void CAnimationDatabaseManager::GetFragmentBlends(const IAnimationDatabase* pIDatabase, SEditorFragmentBlendID::TEditorFragmentBlendIDArray& outBlendIDs) const
{
	CRY_ASSERT(pIDatabase);

	const CAnimationDatabase* pDatabase = static_cast<const CAnimationDatabase*>(pIDatabase);

	outBlendIDs.reserve(pDatabase->m_fragmentBlendDB.size());
	for (CAnimationDatabase::TFragmentBlendDatabase::const_iterator iter = pDatabase->m_fragmentBlendDB.begin(), end = pDatabase->m_fragmentBlendDB.end(); iter != end; ++iter)
	{
		outBlendIDs.push_back();
		SEditorFragmentBlendID& blend = outBlendIDs.back();
		blend.fragFrom = (*iter).first.fragFrom;
		blend.fragTo = (*iter).first.fragTo;
	}
}

void CAnimationDatabaseManager::GetFragmentBlendVariants(const IAnimationDatabase* pIDatabase, const FragmentID fragmentIDFrom, const FragmentID fragmentIDTo,
                                                         SEditorFragmentBlendVariant::TEditorFragmentBlendVariantArray& outVariants) const
{
	CRY_ASSERT(pIDatabase);

	const CAnimationDatabase* pDatabase = static_cast<const CAnimationDatabase*>(pIDatabase);

	CAnimationDatabase::SFragmentBlendID query;
	query.fragFrom = fragmentIDFrom;
	query.fragTo = fragmentIDTo;
	CAnimationDatabase::TFragmentBlendDatabase::const_iterator iter = pDatabase->m_fragmentBlendDB.find(query);
	if (iter != pDatabase->m_fragmentBlendDB.end())
	{
		outVariants.reserve((*iter).second.variantList.size());
		for (CAnimationDatabase::TFragmentVariantList::const_iterator varIter = (*iter).second.variantList.begin(), end = (*iter).second.variantList.end(); varIter != end; ++varIter)
		{
			outVariants.push_back();
			SEditorFragmentBlendVariant& var = outVariants.back();
			var.tagsFrom = (*varIter).tagsFrom;
			var.tagsTo = (*varIter).tagsTo;
		}
	}
}

void CAnimationDatabaseManager::GetFragmentBlend(const IAnimationDatabase* pIDatabase, const FragmentID fragmentIDFrom, const FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, const SFragmentBlendUid& blendUid, SFragmentBlend& outFragmentBlend) const
{
	CRY_ASSERT(pIDatabase);

	const CAnimationDatabase* pDatabase = static_cast<const CAnimationDatabase*>(pIDatabase);
	const SFragmentBlend* pBlend = pDatabase->GetBlend(fragmentIDFrom, fragmentIDTo, tagFrom, tagTo, blendUid);

	if (pBlend == NULL)
		return;

	outFragmentBlend = *pBlend;
}
