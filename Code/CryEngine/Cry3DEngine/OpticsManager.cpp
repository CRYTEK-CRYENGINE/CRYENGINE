// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "OpticsManager.h"

void COpticsManager::Reset()
{
	m_OpticsMap.clear();
	m_SearchedOpticsSet.clear();

	m_OpticsList.clear();

	stl::free_container(m_OpticsList);
}

IOpticsElementBase* COpticsManager::Create(EFlareType type) const
{
	return gEnv->pRenderer->CreateOptics(type);
}

IOpticsElementBase* COpticsManager::ParseOpticsRecursively(IOpticsElementBase* pParentOptics, XmlNodeRef& node) const
{
	const char* type;
	if (!node->getAttr("Type", &type))
		return NULL;

	if (!gEnv->pRenderer)
		return NULL;

	IOpticsElementBase* pOptics = Create(GetFlareType(type));
	if (pOptics == NULL)
		return NULL;

	bool bEnable(false);
	node->getAttr("Enable", bEnable);
	pOptics->SetEnabled(bEnable);

	const char* name;
	if (node->getAttr("Name", &name))
		pOptics->SetName(name);

	if (pParentOptics)
		pParentOptics->AddElement(pOptics);

	for (int i = 0, iChildCount(node->getChildCount()); i < iChildCount; ++i)
	{
		XmlNodeRef pChild = node->getChild(i);
		if (pChild == (IXmlNode*)NULL)
			continue;

		if (!stricmp(pChild->getTag(), "Params"))
		{
			pOptics->Load(pChild);
		}
		else if (!stricmp(pChild->getTag(), "FlareItem"))
		{
			ParseOpticsRecursively(pOptics, pChild);
		}
	}

	return pOptics;
}

bool COpticsManager::Load(const char* fullFlareName, int& nOutIndex)
{
	int nOpticsIndex = FindOpticsIndex(fullFlareName);
	if (nOpticsIndex >= 0)
	{
		nOutIndex = nOpticsIndex;
		return true;
	}

	string strFullFlareName(fullFlareName);
	int nPos = strFullFlareName.find(".");
	if (nPos == -1)
		return false;

	string xmlFileName = strFullFlareName.substr(0, nPos);

	int restLength = strFullFlareName.length() - nPos - 1;
	if (restLength <= 0)
		return false;

	if (m_SearchedOpticsSet.find(fullFlareName) != m_SearchedOpticsSet.end())
		return false;

	string fullPath = gEnv->pCryPak->GetGameFolder() + string("/") + string(FLARE_LIBS_PATH) + xmlFileName + ".xml";
	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(fullPath.c_str());

	if (rootNode == (IXmlNode*)NULL)
		return false;

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "%s", fullFlareName);

	string opticsLibName = strFullFlareName.substr(nPos + 1, restLength);
	m_SearchedOpticsSet.insert(fullFlareName);

	for (int i = 0, iChildCount(rootNode->getChildCount()); i < iChildCount; ++i)
	{
		XmlNodeRef childNode = rootNode->getChild(i);
		if (childNode == (IXmlNode*)NULL)
			continue;
		const char* name;
		if (!childNode->getAttr("Name", &name))
			continue;
		if (stricmp(name, opticsLibName.c_str()))
			continue;
		IOpticsElementBase* pOptics = ParseOpticsRecursively(NULL, childNode);
		if (pOptics == NULL)
			return false;
		return AddOptics(pOptics, fullFlareName, nOutIndex);
	}

	return false;
}

bool COpticsManager::Load(XmlNodeRef& rootNode, int& nOutIndex)
{
	const char* name = NULL;
	if ((rootNode == (IXmlNode*)NULL) || (!rootNode->getAttr("Name", &name)))
		return false;

	const char* libName = NULL;
	if (!rootNode->getAttr("Library", &libName))
		return false;

	IOpticsElementBase* pOptics = ParseOpticsRecursively(NULL, rootNode);
	if (pOptics == NULL)
		return false;

	string fullFlareName = libName + string(".") + name;
	return AddOptics(pOptics, fullFlareName, nOutIndex);
}

EFlareType COpticsManager::GetFlareType(const char* typeStr) const
{
	if (typeStr == NULL)
		return eFT__Base__;

	const FlareInfoArray::Props array = FlareInfoArray::Get();
	for (size_t i = 0; i < array.size; ++i)
	{
		if (!stricmp(array.p[i].name, typeStr))
			return array.p[i].type;
	}

	return eFT__Base__;
}

int COpticsManager::FindOpticsIndex(const char* fullFlareName) const
{
	std::map<string, int>::const_iterator iOptics = m_OpticsMap.find(CONST_TEMP_STRING(fullFlareName));
	if (iOptics != m_OpticsMap.end())
		return iOptics->second;
	return -1;
}

IOpticsElementBase* COpticsManager::GetOptics(int nIndex)
{
	if ((size_t)nIndex >= m_OpticsList.size())
		return NULL;
	return m_OpticsList[nIndex];
}

bool COpticsManager::AddOptics(IOpticsElementBase* pOptics, const char* name, int& nOutNewIndex)
{
	if (name == NULL)
		return false;
	if (m_OpticsMap.find(name) != m_OpticsMap.end())
		return false;
	nOutNewIndex = (int)m_OpticsList.size();
	m_OpticsList.push_back(pOptics);
	m_OpticsMap[name] = nOutNewIndex;
	return true;
}

bool COpticsManager::Rename(const char* fullFlareName, const char* newFullFlareName)
{
	if (m_OpticsMap.find(newFullFlareName) != m_OpticsMap.end())
		return true;

	std::map<string, int>::iterator iOptics = m_OpticsMap.find(fullFlareName);
	if (iOptics == m_OpticsMap.end())
		return false;

	int nOpticsIndex = iOptics->second;
	if (nOpticsIndex < 0 || nOpticsIndex >= (int)m_OpticsList.size())
		return false;

	m_OpticsMap.erase(iOptics);

	IOpticsElementBasePtr pOptics = m_OpticsList[nOpticsIndex];
	pOptics->SetName(newFullFlareName);
	m_OpticsMap[newFullFlareName] = nOpticsIndex;

	return true;
}

void COpticsManager::GetMemoryUsage(ICrySizer* pSizer) const
{
	for (int i = 0, iSize(m_OpticsList.size()); i < iSize; ++i)
		m_OpticsList[i]->GetMemoryUsage(pSizer);
}

void COpticsManager::Invalidate()
{
	for (int i = 0, iSize(m_OpticsList.size()); i < iSize; ++i)
		m_OpticsList[i]->Invalidate();
}
