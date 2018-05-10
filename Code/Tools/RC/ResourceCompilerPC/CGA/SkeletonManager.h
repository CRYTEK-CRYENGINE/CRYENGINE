// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SKELETON_MANAGER__H__
#define __SKELETON_MANAGER__H__

#include <set>
#include "SkeletonLoader.h"

struct IPakSystem;
class ICryXML;
struct IResourceCompiler;

class SkeletonManager
{
public:
	SkeletonManager(IPakSystem* pPakSystem, ICryXML* pXMLParser, IResourceCompiler* pRc);

	bool LoadSkeletonList(const string& filename, const string& rootPath, const std::set<string>& skeletons);
	const CSkeletonInfo* FindSkeleton(const string& name) const;
	const CSkeletonInfo* LoadSkeleton(const string& name);

private:
	const CSkeletonInfo* LoadSkeletonInfo(const string& name, const string& file);

private:
	IPakSystem* m_pPakSystem;
	ICryXML* m_pXmlParser;

	string m_rootPath;
	string m_tmpPath;

	typedef std::map<string, string> TNameToFileMap;
	TNameToFileMap m_nameToFile;

	typedef std::map<string, SkeletonLoader> TNameToSkeletonMap;
	TNameToSkeletonMap m_nameToSkeletonInfo;
};

#endif
