// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//////////////////////////////////////////////////////////////////////

#ifndef __OPTICSMANAGER_H__
#define __OPTICSMANAGER_H__

#include <CryRenderer/IFlares.h>

class COpticsManager : public Cry3DEngineBase, public IOpticsManager
{
public:

	~COpticsManager(){}

	void                Reset();

	IOpticsElementBase* Create(EFlareType type) const;
	IOpticsElementBase* GetOptics(int nIndex);

	bool                Load(const char* fullFlareName, int& nOutIndex);
	bool                Load(XmlNodeRef& rootNode, int& nOutIndex);
	bool                AddOptics(IOpticsElementBase* pOptics, const char* name, int& nOutNewIndex);
	bool                Rename(const char* fullFlareName, const char* newFullFlareName);

	void                GetMemoryUsage(ICrySizer* pSizer) const;
	void                Invalidate();

private:
	IOpticsElementBase* ParseOpticsRecursively(IOpticsElementBase* pParentOptics, XmlNodeRef& node) const;
	EFlareType          GetFlareType(const char* typeStr) const;
	int                 FindOpticsIndex(const char* fullFlareName) const;

private:
	std::vector<IOpticsElementBasePtr> m_OpticsList;
	std::map<string, int>              m_OpticsMap;
	// All flare list which has already been searched for.
	std::set<string>                   m_SearchedOpticsSet;
};

#endif //__OPTICSMANAGER_H__
