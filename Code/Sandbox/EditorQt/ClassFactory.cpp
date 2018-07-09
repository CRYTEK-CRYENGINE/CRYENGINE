// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ClassFactory.h"

CClassFactory::CClassFactory()
{
	m_classes.reserve(100);
}

CClassFactory::~CClassFactory()
{
}

CClassFactory* CClassFactory::Instance()
{
	static CClassFactory instance;
	return &instance;
}

void CClassFactory::RegisterClass(IClassDesc* pClassDesc)
{
	assert(pClassDesc);
	m_classes.push_back(pClassDesc);
	m_nameToClass[pClassDesc->ClassName()] = pClassDesc;
}

IClassDesc* CClassFactory::FindClass(const char* pClassName) const
{
	IClassDesc* pClassDesc = stl::find_in_map(m_nameToClass, pClassName, (IClassDesc*)nullptr);

	if (pClassDesc)
	{
		return pClassDesc;
	}

	const char* pSubClassName = strstr(pClassName, "::");

	if (!pSubClassName)
	{
		return NULL;
	}

	string name;

	name.Append(pClassName, pSubClassName - pClassName);

	return stl::find_in_map(m_nameToClass, name, (IClassDesc*)nullptr);
}

void CClassFactory::UnregisterClass(IClassDesc* pClassDesc)
{
	if (!pClassDesc)
		return;

	stl::find_and_erase(m_classes, pClassDesc);
	stl::member_find_and_erase(m_nameToClass, pClassDesc->ClassName());
}

bool ClassDescNameCompare(IClassDesc* pArg1, IClassDesc* pArg2)
{
	return (stricmp(pArg1->ClassName(), pArg2->ClassName()) < 0);
}

void CClassFactory::GetClassesBySystemID(ESystemClassID aSystemClassID, std::vector<IClassDesc*>& rOutClasses)
{
	rOutClasses.clear();

	for (size_t i = 0; i < m_classes.size(); ++i)
	{
		if (m_classes[i]->SystemClassID() == aSystemClassID)
		{
			rOutClasses.push_back(m_classes[i]);
		}
	}

	std::sort(rOutClasses.begin(), rOutClasses.end(), ClassDescNameCompare);
}

void CClassFactory::GetClassesByCategory(const char* pCategory, std::vector<IClassDesc*>& rOutClasses)
{
	rOutClasses.clear();

	for (size_t i = 0; i < m_classes.size(); ++i)
	{
		if (!stricmp(pCategory, m_classes[i]->Category()))
		{
			rOutClasses.push_back(m_classes[i]);
		}
	}

	std::sort(rOutClasses.begin(), rOutClasses.end(), ClassDescNameCompare);
}

