// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TagDefinitionXml.h"

#include "ICryMannequin.h"

//////////////////////////////////////////////////////////////////////////
#ifndef _RELEASE
	#define RECURSIVE_IMPORT_CHECK (1)
#else
	#define RECURSIVE_IMPORT_CHECK (0)
#endif

#if RECURSIVE_IMPORT_CHECK
typedef std::vector<string> TRecursiveGuardList;
#endif
typedef std::vector<string> TImportsList;

//////////////////////////////////////////////////////////////////////////
namespace mannequin
{
int GetTagDefinitionVersion(XmlNodeRef pXmlNode);
bool LoadTagDefinitionImpl(const char* const filename, CTagDefinition & tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
                           TRecursiveGuardList & recursiveGuardListOut,
#endif
                           STagDefinitionImportsInfo & importsInfo);

XmlNodeRef SaveTagDefinitionImpl(const CTagDefinition& tagDefinition, const TImportsList& importsList);

namespace impl
{
std::map<uint32, STagDefinitionImportsInfo> g_defaultImportInfo;

bool LoadTagDefinitionImplXml(XmlNodeRef pXmlNode, CTagDefinition & tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
                              TRecursiveGuardList & recursiveGuardListOut,
#endif
                              STagDefinitionImportsInfo & importsInfo);
}
}

//////////////////////////////////////////////////////////////////////////
STagDefinitionImportsInfo& mannequin::GetDefaultImportsInfo(const char* const filename)
{
	assert(filename);
	assert(filename[0]);

	const uint32 crc = CCrc32::ComputeLowercase(filename);
	STagDefinitionImportsInfo& importsInfo = impl::g_defaultImportInfo[crc];
	importsInfo.SetFilename(filename);
	return importsInfo;
}

//////////////////////////////////////////////////////////////////////////
void mannequin::OnDatabaseManagerUnload()
{
	impl::g_defaultImportInfo.clear();
}

//////////////////////////////////////////////////////////////////////////
template<int version>
struct STagDefinitionXml
{
};

//////////////////////////////////////////////////////////////////////////
template<>
struct STagDefinitionXml<1>
{
	static bool Load(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
	                 TRecursiveGuardList&,
#endif
	                 STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);
		const int childCount = pXmlNode->getChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef pXmlChildNode = pXmlNode->getChild(i);
			LoadTagOrGroupNode(pXmlChildNode, tagDefinitionOut, importsInfo);
		}
		return true;
	}

	static void Save(XmlNodeRef pXmlNode, const CTagDefinition& tagDefinition)
	{
		assert(pXmlNode != 0);
		pXmlNode->setAttr("version", 1);

		std::vector<XmlNodeRef> groupXmlNodes;

		const TagGroupID groupCount = tagDefinition.GetNumGroups();
		groupXmlNodes.resize(groupCount);
		for (TagGroupID i = 0; i < groupCount; ++i)
		{
			const char* const groupName = tagDefinition.GetGroupName(i);
			XmlNodeRef pXmlTagGroup = pXmlNode->createNode(groupName);
			pXmlNode->addChild(pXmlTagGroup);

			groupXmlNodes[i] = pXmlTagGroup;
		}

		const TagID tagCount = tagDefinition.GetNum();
		for (TagID i = 0; i < tagCount; ++i)
		{
			const char* const tagName = tagDefinition.GetTagName(i);
			XmlNodeRef pXmlTag = pXmlNode->createNode(tagName);

			const uint32 tagPriority = tagDefinition.GetPriority(i);
			if (tagPriority != 0)
			{
				pXmlTag->setAttr("priority", tagPriority);
			}

			const TagGroupID groupId = tagDefinition.GetGroupID(i);
			if (groupId == GROUP_ID_NONE)
			{
				pXmlNode->addChild(pXmlTag);
			}
			else
			{
				XmlNodeRef pXmlTagGroup = groupXmlNodes[groupId];
				pXmlTagGroup->addChild(pXmlTag);
			}
		}
	}

private:
	static void LoadTagOrGroupNode(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut, STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);
		const int childCount = pXmlNode->getChildCount();
		const bool isGroupNode = (childCount != 0);
		if (isGroupNode)
		{
			LoadGroupNode(pXmlNode, childCount, tagDefinitionOut, importsInfo);
		}
		else
		{
			LoadTagNode(pXmlNode, NULL, tagDefinitionOut, importsInfo);
		}
	}

	static void LoadTagNode(XmlNodeRef pXmlNode, const char* const groupName, CTagDefinition& tagDefinitionOut, STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);

		uint32 priority = 0;
		pXmlNode->getAttr("priority", priority);
		const char* const tagName = pXmlNode->getTag();

		const TagID tagId = tagDefinitionOut.AddTag(tagName, groupName, priority);
		if (tagId == TAG_ID_INVALID)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Duplicate tag '%s'", tagName);
			// We will continue loading
		}

		importsInfo.AddTag(tagId);
	}

	static void LoadGroupNode(XmlNodeRef pXmlNode, const int childCount, CTagDefinition& tagDefinitionOut, STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);
		assert(pXmlNode->getChildCount() == childCount);

		const char* const groupName = pXmlNode->getTag();
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef pXmlChildNode = pXmlNode->getChild(i);
			LoadTagNode(pXmlChildNode, groupName, tagDefinitionOut, importsInfo);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
template<>
struct STagDefinitionXml<2>
{
	static bool Load(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
	                 TRecursiveGuardList& recursiveGuardListOut,
#endif
	                 STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);

		XmlNodeRef pXmlImportsNode = pXmlNode->findChild("Imports");
		const bool importsLoadSuccess = LoadImportsNode(pXmlImportsNode, tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
		                                                recursiveGuardListOut,
#endif
		                                                importsInfo);
		if (!importsLoadSuccess)
		{
			return false;
		}

		XmlNodeRef pXmlTagsNode = pXmlNode->findChild("Tags");
		const bool loadTagsSuccess = LoadTagsNode(pXmlTagsNode, tagDefinitionOut, importsInfo);
		return loadTagsSuccess;
	}

	static void Save(XmlNodeRef pXmlNode, const CTagDefinition& tagDefinition, const TImportsList& importsList)
	{
		pXmlNode->setAttr("version", 2);

		SaveImportsNode(pXmlNode, importsList);
		SaveTagsNode(pXmlNode, tagDefinition);
	}

private:
	static bool LoadImportsNode(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
	                            TRecursiveGuardList& recursiveGuardListOut,
#endif
	                            STagDefinitionImportsInfo& importsInfo)
	{
		if (!pXmlNode)
		{
			return true;
		}

		const int childCount = pXmlNode->getChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef pXmlChildNode = pXmlNode->getChild(i);
			const bool loadSuccess = LoadImportNode(pXmlChildNode, tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
			                                        recursiveGuardListOut,
#endif
			                                        importsInfo);
			if (!loadSuccess)
			{
				return false;
			}
		}
		return true;
	}

	static bool LoadImportNode(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
	                           TRecursiveGuardList& recursiveGuardListOut,
#endif
	                           STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);

		const char* const filename = pXmlNode->getAttr("filename");
		if (!filename)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Tag definition failed to find 'filename' attribute in an 'Import' node.");
			return false;
		}

		STagDefinitionImportsInfo& subImportsInfo = importsInfo.AddImport(filename);
		const bool loadImportedTagDefinitionSuccess = mannequin::LoadTagDefinitionImpl(filename, tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
		                                                                               recursiveGuardListOut,
#endif
		                                                                               subImportsInfo);
		return loadImportedTagDefinitionSuccess;
	}

	static bool LoadTagsNode(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut, STagDefinitionImportsInfo& importsInfo)
	{
		if (!pXmlNode)
		{
			return true;
		}
		const int childCount = pXmlNode->getChildCount();

		bool loadSuccess = true;
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef pXmlChildNode = pXmlNode->getChild(i);
			loadSuccess &= LoadTagOrGroupNode(pXmlChildNode, tagDefinitionOut, importsInfo);
		}

		return loadSuccess;
	}

	static bool LoadTagOrGroupNode(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut, STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);
		const int childCount = pXmlNode->getChildCount();
		const bool isGroupNode = (childCount != 0);
		if (isGroupNode)
		{
			const bool loadGroupSuccess = LoadGroupNode(pXmlNode, childCount, tagDefinitionOut, importsInfo);
			return loadGroupSuccess;
		}
		else
		{
			const bool loadTagSuccess = LoadTagNode(pXmlNode, NULL, tagDefinitionOut, importsInfo);
			return loadTagSuccess;
		}
	}

	static bool LoadTagNode(XmlNodeRef pXmlNode, const char* const groupName, CTagDefinition& tagDefinitionOut, STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);

		uint32 priority = 0;
		pXmlNode->getAttr("priority", priority);

		const char* const tagName = pXmlNode->getAttr("name");
		if (!tagName)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to find a 'name' attribute in a 'tag' node.");
			return false;
		}

		const TagID tagId = tagDefinitionOut.AddTag(tagName, groupName, priority);
		if (tagId == TAG_ID_INVALID)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Duplicate tag '%s'", tagName);
			// We will continue loading
		}
		else
		{
			const char* const pSubTagsFilename = pXmlNode->getAttr("subTagDef");
			if (pSubTagsFilename && pSubTagsFilename[0])
			{
				IAnimationDatabaseManager& animationDatabaseManager = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
				const CTagDefinition* pTagDef = animationDatabaseManager.LoadTagDefs(pSubTagsFilename, true);
				if (pTagDef)
				{
					tagDefinitionOut.SetSubTagDefinition(tagId, pTagDef);
				}
			}
		}

		importsInfo.AddTag(tagId);

		return true;
	}

	static bool LoadGroupNode(XmlNodeRef pXmlNode, const int childCount, CTagDefinition& tagDefinitionOut, STagDefinitionImportsInfo& importsInfo)
	{
		assert(pXmlNode != 0);
		assert(pXmlNode->getChildCount() == childCount);

		const char* const groupName = pXmlNode->getAttr("name");
		if (!groupName)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to find a 'name' attribute in a 'group' node.");
			return false;
		}
		else if (tagDefinitionOut.FindGroup(groupName) != GROUP_ID_NONE)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Group '%s' overrides existing group.", groupName);
			return false;
		}

		bool loadSuccess = true;
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef pXmlChildNode = pXmlNode->getChild(i);
			loadSuccess &= LoadTagNode(pXmlChildNode, groupName, tagDefinitionOut, importsInfo);
		}
		return loadSuccess;
	}

	static XmlNodeRef CreateChildNode(XmlNodeRef pXmlNode, const char* const childName)
	{
		assert(pXmlNode != 0);
		assert(childName);
		assert(childName[0]);

		XmlNodeRef pXmlChildNode = pXmlNode->createNode(childName);
		pXmlNode->addChild(pXmlChildNode);
		return pXmlChildNode;
	}

	static void SaveImportsNode(XmlNodeRef pXmlNode, const TImportsList& importsList)
	{
		assert(pXmlNode != 0);

		const size_t importsCount = importsList.size();
		if (importsCount == 0)
		{
			return;
		}

		XmlNodeRef pXmlImportsNode = CreateChildNode(pXmlNode, "Imports");

		for (size_t i = 0; i < importsCount; ++i)
		{
			const string& filename = importsList[i];
			SaveImportNode(pXmlImportsNode, filename);
		}
	}

	static void SaveImportNode(XmlNodeRef pXmlNode, const string& filename)
	{
		assert(pXmlNode != 0);

		XmlNodeRef pXmlImportNode = CreateChildNode(pXmlNode, "Import");
		pXmlImportNode->setAttr("filename", filename.c_str());
	}

	static void SaveTagsNode(XmlNodeRef pXmlNode, const CTagDefinition& tagDefinition)
	{
		const TagID tagCount = tagDefinition.GetNum();
		if (tagCount == 0)
		{
			return;
		}

		XmlNodeRef pXmlTagsNode = CreateChildNode(pXmlNode, "Tags");

		std::vector<XmlNodeRef> groupXmlNodes;

		const TagGroupID groupCount = tagDefinition.GetNumGroups();
		groupXmlNodes.resize(groupCount);
		for (TagGroupID i = 0; i < groupCount; ++i)
		{
			const char* const groupName = tagDefinition.GetGroupName(i);
			XmlNodeRef pXmlTagGroup = CreateChildNode(pXmlTagsNode, "Group");
			pXmlTagGroup->setAttr("name", groupName);

			groupXmlNodes[i] = pXmlTagGroup;
		}

		for (TagID i = 0; i < tagCount; ++i)
		{
			const char* const tagName = tagDefinition.GetTagName(i);
			XmlNodeRef pXmlTagNode = pXmlTagsNode->createNode("Tag");
			pXmlTagNode->setAttr("name", tagName);

			const uint32 tagPriority = tagDefinition.GetPriority(i);
			if (tagPriority != 0)
			{
				pXmlTagNode->setAttr("priority", tagPriority);
			}

			const CTagDefinition* pTagDef = tagDefinition.GetSubTagDefinition(i);
			if (pTagDef)
			{
				pXmlTagNode->setAttr("subTagDef", pTagDef->GetFilename());
			}

			const TagGroupID groupId = tagDefinition.GetGroupID(i);
			if (groupId == GROUP_ID_NONE)
			{
				pXmlTagsNode->addChild(pXmlTagNode);
			}
			else
			{
				XmlNodeRef pXmlTagGroup = groupXmlNodes[groupId];
				pXmlTagGroup->addChild(pXmlTagNode);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
int mannequin::GetTagDefinitionVersion(XmlNodeRef pXmlNode)
{
	if (stricmp(pXmlNode->getTag(), "TagDefinition") != 0)
	{
		return -1;
	}

	int version = 1;
	pXmlNode->getAttr("version", version);
	return version;
}

//////////////////////////////////////////////////////////////////////////
bool mannequin::LoadTagDefinitionImpl(const char* const filename, CTagDefinition& tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
                                      TRecursiveGuardList& recursiveGuardListOut,
#endif
                                      STagDefinitionImportsInfo& importsInfo)
{
	assert(filename);
	assert(filename[0]);

#if RECURSIVE_IMPORT_CHECK
	for (int i = 0; i < recursiveGuardListOut.size(); ++i)
	{
		const char* const alreadyLoadedFilename = recursiveGuardListOut[i];
		if (stricmp(filename, alreadyLoadedFilename) == 0)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load tag definition file '%s' due to a cyclic dependency.", filename);
			return false;
		}
	}

	recursiveGuardListOut.push_back(filename);
#endif

	XmlNodeRef pXmlNode = gEnv->pSystem->LoadXmlFromFile(filename);
	if (!pXmlNode)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load tag definition file '%s'", filename);
		return false;
	}

	CRY_DEFINE_ASSET_SCOPE("TagDefinition", filename);
	const bool loadSuccess = impl::LoadTagDefinitionImplXml(pXmlNode, tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
	                                                        recursiveGuardListOut,
#endif
	                                                        importsInfo);
	return loadSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool mannequin::impl::LoadTagDefinitionImplXml(XmlNodeRef pXmlNode, CTagDefinition& tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
                                               TRecursiveGuardList& recursiveGuardListOut,
#endif
                                               STagDefinitionImportsInfo& importsInfo)
{
	assert(pXmlNode != 0);

	bool loadSuccess = false;

	const int version = GetTagDefinitionVersion(pXmlNode);
	switch (version)
	{
	case 2:
		loadSuccess = STagDefinitionXml<2>::Load(pXmlNode, tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
		                                         recursiveGuardListOut,
#endif
		                                         importsInfo);
		break;
	case 1:
		loadSuccess = STagDefinitionXml<1>::Load(pXmlNode, tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
		                                         recursiveGuardListOut,
#endif
		                                         importsInfo);
		break;
	default:
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load tag definition: Unsupported version '%d'", version);
		break;
	}

	return loadSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool mannequin::LoadTagDefinition(const char* const filename, CTagDefinition& tagDefinitionOut, bool isTags)
{
	assert(filename);
	assert(filename[0]);

	tagDefinitionOut.Clear();
	tagDefinitionOut.SetFilename(filename);

#if RECURSIVE_IMPORT_CHECK
	TRecursiveGuardList recursiveGuardListOut;
#endif

	STagDefinitionImportsInfo& importsInfo = GetDefaultImportsInfo(filename);
	importsInfo.Clear();
	const bool loadSuccess = LoadTagDefinitionImpl(filename, tagDefinitionOut,
#if RECURSIVE_IMPORT_CHECK
	                                               recursiveGuardListOut,
#endif
	                                               importsInfo);

	if (isTags)
	{
		tagDefinitionOut.AssignBits();
	}

	return loadSuccess;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef mannequin::SaveTagDefinitionImpl(const CTagDefinition& tagDefinition, const TImportsList& importList)
{
	XmlNodeRef pXmlNode = GetISystem()->CreateXmlNode("TagDefinition");
	STagDefinitionXml<2>::Save(pXmlNode, tagDefinition, importList);
	return pXmlNode;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef mannequin::SaveTagDefinition(const CTagDefinition& tagDefinition)
{
	const TImportsList emptyImportList;
	return SaveTagDefinitionImpl(tagDefinition, emptyImportList);
}

//////////////////////////////////////////////////////////////////////////
namespace
{
size_t FindImportInfoIndex(const std::vector<const STagDefinitionImportsInfo*>& importsInfoList, const STagDefinitionImportsInfo& importsInfo)
{
	const size_t totalImportsInfoCount = importsInfoList.size();
	for (size_t i = 0; i < totalImportsInfoCount; ++i)
	{
		const STagDefinitionImportsInfo* pCurrentImportsInfo = importsInfoList[i];
		if (pCurrentImportsInfo == &importsInfo)
		{
			return i;
		}
	}
	assert(false);
	return size_t(~0);
}
}

//////////////////////////////////////////////////////////////////////////
void mannequin::SaveTagDefinition(const CTagDefinition& tagDefinition, TTagDefinitionSaveDataList& saveDataListOut)
{
	const char* const mainFilename = tagDefinition.GetFilename();
	const STagDefinitionImportsInfo& importsInfo = GetDefaultImportsInfo(mainFilename);

	std::vector<const STagDefinitionImportsInfo*> importsInfoList;
	importsInfo.FlattenImportsInfo(importsInfoList);
	const size_t totalImportsInfoCount = importsInfoList.size();

	std::vector<CTagDefinition> saveTagDefinitionList;
	saveTagDefinitionList.resize(totalImportsInfoCount);

	for (size_t i = 0; i < totalImportsInfoCount; ++i)
	{
		const STagDefinitionImportsInfo* pImportsInfo = importsInfoList[i];
		assert(pImportsInfo);
		const char* const filename = pImportsInfo->GetFilename();

		CTagDefinition& tagDefinitionSave = saveTagDefinitionList[i];
		tagDefinitionSave.SetFilename(filename);
	}

	const TagID tagCount = tagDefinition.GetNum();
	for (TagID tagId = 0; tagId < tagCount; ++tagId)
	{
		const STagDefinitionImportsInfo& importInfoForTag = importsInfo.Find(tagId);

		const size_t importInfoId = FindImportInfoIndex(importsInfoList, importInfoForTag);
		assert(importInfoId < importsInfoList.size());

		const char* const tagName = tagDefinition.GetTagName(tagId);
		const TagGroupID tagGroupId = tagDefinition.GetGroupID(tagId);
		const char* const groupName = (tagGroupId == GROUP_ID_NONE) ? NULL : tagDefinition.GetGroupName(tagGroupId);
		const int tagPriority = tagDefinition.GetPriority(tagId);

		CTagDefinition& tagDefinitionSave = saveTagDefinitionList[importInfoId];
		TagID newTagID = tagDefinitionSave.AddTag(tagName, groupName, tagPriority);
		tagDefinitionSave.SetSubTagDefinition(newTagID, tagDefinition.GetSubTagDefinition(tagId));
	}

	for (size_t i = 0; i < totalImportsInfoCount; ++i)
	{
		const STagDefinitionImportsInfo* pImportsInfo = importsInfoList[i];
		const CTagDefinition& tagDefinitionSave = saveTagDefinitionList[i];

		TImportsList importList;
		const size_t subImportsCount = pImportsInfo->GetImportCount();
		for (size_t j = 0; j < subImportsCount; ++j)
		{
			const STagDefinitionImportsInfo& subImportInfo = pImportsInfo->GetImport(j);
			const char* const filename = subImportInfo.GetFilename();
			// prevent duplicate import entries
			if (std::find(importList.begin(), importList.end(), filename) == importList.end())
			{
				importList.push_back(filename);
			}
		}

		STagDefinitionSaveData saveInfo;
		saveInfo.pXmlNode = SaveTagDefinitionImpl(tagDefinitionSave, importList);
		saveInfo.filename = tagDefinitionSave.GetFilename();

		saveDataListOut.push_back(saveInfo);
	}
}
