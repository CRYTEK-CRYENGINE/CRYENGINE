// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkeletonManager.h"

#include "../../../CryXML/IXMLSerializer.h"
#include "../../../CryXML/ICryXML.h"
#include "FileUtil.h"
#include "PathHelpers.h"
#include "PakSystem.h"
#include "PakXmlFileBufferSource.h"


//////////////////////////////////////////////////////////////////////////
SkeletonManager::SkeletonManager(IPakSystem* pPakSystem, ICryXML* pXmlParser, IResourceCompiler* pRc)
: m_pPakSystem(pPakSystem)
, m_pXmlParser(pXmlParser)
{
	assert(m_pPakSystem);
	assert(m_pXmlParser);

	assert(pRc != 0);
	m_tmpPath = pRc->GetTmpPath();
}


//////////////////////////////////////////////////////////////////////////
bool SkeletonManager::LoadSkeletonList(const string& filename, const string& rootPath, const std::set<string>& usedSkeletons)
{
	m_rootPath = rootPath;

	RCLog("Loading skeleton list '%s'.", filename.c_str());
	const bool bFileExists = FileUtil::FileExists(filename.c_str());
	if (!bFileExists)
	{
		RCLog("Failed to find '%s' on disk, searching in the paks...", filename.c_str());
		PakSystemFile* pFile = m_pPakSystem->Open(filename.c_str(), "rb");
		const bool bFileExistsInPak = (pFile != 0);
		if (!bFileExistsInPak)
		{
			RCLogError("Can't load skeleton list: Failed to find '%s' on disk or in pak files.", filename.c_str());
			return false;
		}
		m_pPakSystem->Close(pFile);
	}

	XmlNodeRef pXmlRoot;
	{
		const int errorBufferSize = 4096;
		char errorBuffer[errorBufferSize];
		IXMLSerializer* pXmlSerializer = m_pXmlParser->GetXMLSerializer();
		PakXmlFileBufferSource xmlFileBufferSource(m_pPakSystem, filename.c_str());
		const bool bRemoveNonessentialSpacesFromContent = true;
		pXmlRoot = pXmlSerializer->Read(xmlFileBufferSource, bRemoveNonessentialSpacesFromContent, errorBufferSize, errorBuffer);
		if (!pXmlRoot)
		{
			RCLogError("Can't load skeleton list: Failed to read '%s'. '%s'\n", filename.c_str(), errorBuffer);
			return false;
		}
	}

	const int childCount = pXmlRoot->getChildCount();
	for (int i = 0; i < childCount; ++i)
	{
		XmlNodeRef pXmlEntry = pXmlRoot->getChild(i);

		const string xmlAttrName = pXmlEntry->getAttr("name");
		const bool bHasNameAttribute = !xmlAttrName.empty();

		const string xmlAttrFile = pXmlEntry->getAttr("file");
		const bool bHasFileAttribute = !xmlAttrFile.empty();

		if (!bHasNameAttribute)
		{
			RCLogError("While parsing '%s' failed to find 'name' attribute for element '%s' at line '%d'. Skipping this entry.", filename.c_str(), pXmlEntry->getTag(), pXmlEntry->getLine());
		}
		else if (!bHasFileAttribute)
		{
			RCLogError("While parsing '%s' failed to find 'file' attribute for element '%s' at line '%d'. Skipping this entry.", filename.c_str(), pXmlEntry->getTag(), pXmlEntry->getLine());
		}
		else
		{
			const bool bIsDuplicateEntry = (m_nameToFile.find(xmlAttrName) != m_nameToFile.end());
			if (bIsDuplicateEntry)
			{
				RCLogError("While parsing '%s' found duplicate name '%s' in element '%s' at line '%d'. Skipping this entry.", filename.c_str(), xmlAttrName.c_str(), pXmlEntry->getTag(), pXmlEntry->getLine());
			}
			else
			{
				//RCLog("Adding entry with name '%s' pointing to file '%s'", xmlAttrName.c_str(), xmlAttrFile.c_str());
				m_nameToFile[xmlAttrName] = xmlAttrFile;
			}
		}
	}

	RCLog("Starting preloading of skeletons.");
	for (TNameToFileMap::const_iterator cit = m_nameToFile.begin(); cit != m_nameToFile.end(); ++cit)
	{
		const string& name = cit->first;
		if (usedSkeletons.find(name) != usedSkeletons.end())
		{
			const string& file = cit->second;
			LoadSkeletonInfo(name, file);
		}
	}
	RCLog("Finished preloading of skeletons.");

	RCLog("Finished loading skeleton list.");
	return true;
}


//////////////////////////////////////////////////////////////////////////
const CSkeletonInfo* SkeletonManager::LoadSkeletonInfo(const string& name, const string& file)
{
	const bool bIsAlreadyLoaded = (m_nameToSkeletonInfo.find(name) != m_nameToSkeletonInfo.end());
	if (bIsAlreadyLoaded)
	{
		assert(false);
		return 0;
	}

	string fullFile = PathUtil::Make(m_rootPath, file);
	RCLog("Loading skeleton with alias '%s' from file '%s'.", name.c_str(), fullFile.c_str());

	SkeletonLoader& skeletonInfo = m_nameToSkeletonInfo[name];
	skeletonInfo.Load(fullFile.c_str(), m_pPakSystem, m_pXmlParser, m_tmpPath);
	const bool bLoadSuccess = skeletonInfo.IsLoaded();
	if (!bLoadSuccess)
	{
		RCLogError("Failed to load skeleton '%s'. Alias '%s' not loaded.", fullFile.c_str(), name.c_str());
		return 0;
	}

	return &skeletonInfo.Skeleton();
}


//////////////////////////////////////////////////////////////////////////
const CSkeletonInfo* SkeletonManager::FindSkeleton(const string& name) const
{
	if (name.empty())
	{
		return 0;
	}

	TNameToSkeletonMap::const_iterator cit = m_nameToSkeletonInfo.find(name);
	if (cit != m_nameToSkeletonInfo.end())
	{
		const SkeletonLoader& skeletonInfo = cit->second;
		const bool bIsLoaded = skeletonInfo.IsLoaded();
		if (!bIsLoaded)
		{
			return 0;
		}

		return &skeletonInfo.Skeleton();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
const CSkeletonInfo* SkeletonManager::LoadSkeleton(const string& name)
{
	TNameToFileMap::iterator it = m_nameToFile.find(name);
	if (it == m_nameToFile.end())
	{
		return 0;
	}

	const CSkeletonInfo* skeleton = FindSkeleton(name);
	if (skeleton)
	{
		return skeleton;
	}

	return LoadSkeletonInfo(name, it->second);
}
