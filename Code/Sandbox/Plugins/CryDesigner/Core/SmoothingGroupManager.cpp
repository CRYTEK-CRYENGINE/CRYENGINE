// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SmoothingGroupManager.h"
#include "Core/Model.h"

namespace Designer
{
bool SmoothingGroupManager::AddSmoothingGroup(const char* id_name, SmoothingGroupPtr pSmoothingGroup)
{
	for (int i = 0, iPolygonCount(pSmoothingGroup->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = pSmoothingGroup->GetPolygon(i);
		auto ii = m_MapPolygon2GroupId.find(pPolygon);
		if (ii == m_MapPolygon2GroupId.end())
			continue;
		m_SmoothingGroups[ii->second]->RemovePolygon(pPolygon);
		if (m_SmoothingGroups[ii->second]->GetPolygonCount() == 0)
			m_SmoothingGroups.erase(ii->second);
	}

	auto iSmoothingGroup = m_SmoothingGroups.find(id_name);
	if (iSmoothingGroup != m_SmoothingGroups.end())
	{
		for (int i = 0, iCount(pSmoothingGroup->GetPolygonCount()); i < iCount; ++i)
			iSmoothingGroup->second->AddPolygon(pSmoothingGroup->GetPolygon(i));
	}
	else
	{
		m_SmoothingGroups[id_name] = pSmoothingGroup;
	}

	for (int i = 0, iPolygonCount(pSmoothingGroup->GetPolygonCount()); i < iPolygonCount; ++i)
		m_MapPolygon2GroupId[pSmoothingGroup->GetPolygon(i)] = id_name;

	return true;
}

bool SmoothingGroupManager::AddPolygon(const char* id_name, PolygonPtr pPolygon)
{
	SmoothingGroupPtr pSmoothingGroup = GetSmoothingGroup(id_name);
	if (pSmoothingGroup == NULL)
	{
		std::vector<PolygonPtr> polygons;
		polygons.push_back(pPolygon);
		return AddSmoothingGroup(id_name, new SmoothingGroup(polygons));
	}

	auto ii = m_MapPolygon2GroupId.find(pPolygon);
	if (ii != m_MapPolygon2GroupId.end())
	{
		if (ii->second != id_name)
		{
			m_SmoothingGroups[ii->second]->RemovePolygon(pPolygon);
			if (m_SmoothingGroups[ii->second]->GetPolygonCount() == 0)
				m_SmoothingGroups.erase(ii->second);
			ii->second = id_name;
		}
	}
	else
	{
		m_MapPolygon2GroupId[pPolygon] = id_name;
	}
	pSmoothingGroup->AddPolygon(pPolygon);
	return true;
}

void SmoothingGroupManager::RemoveSmoothingGroup(const char* id_name)
{
	auto iter = m_SmoothingGroups.find(id_name);
	if (iter == m_SmoothingGroups.end())
		return;
	for (int i = 0, iCount(iter->second->GetPolygonCount()); i < iCount; ++i)
	{
		PolygonPtr pPolygon = iter->second->GetPolygon(i);
		m_MapPolygon2GroupId.erase(pPolygon);
	}
	m_SmoothingGroups.erase(iter);
}

SmoothingGroupPtr SmoothingGroupManager::GetSmoothingGroup(const char* id_name) const
{
	auto iter = m_SmoothingGroups.find(id_name);
	if (iter == m_SmoothingGroups.end())
		return NULL;
	return iter->second;
}

SmoothingGroupPtr SmoothingGroupManager::FindSmoothingGroup(PolygonPtr pPolygon) const
{
	const char* id = GetSmoothingGroupID(pPolygon);
	if (id == NULL)
		return NULL;
	return GetSmoothingGroup(id);
}

const char* SmoothingGroupManager::GetSmoothingGroupID(PolygonPtr pPolygon) const
{
	auto ii = m_MapPolygon2GroupId.find(pPolygon);
	if (ii == m_MapPolygon2GroupId.end())
		return NULL;
	return ii->second;
}

const char* SmoothingGroupManager::GetSmoothingGroupID(SmoothingGroupPtr pSmoothingGroup) const
{
	auto ii = m_SmoothingGroups.begin();

	for (; ii != m_SmoothingGroups.end(); ++ii)
	{
		if (pSmoothingGroup == ii->second)
			return ii->first;
	}

	return NULL;
}

void SmoothingGroupManager::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo, Model* pModel)
{
	if (bLoading)
	{
		m_SmoothingGroups.clear();
		m_MapPolygon2GroupId.clear();

		int nCount = xmlNode->getChildCount();
		for (int i = 0; i < nCount; ++i)
		{
			XmlNodeRef pSmoothingGroupNode = xmlNode->getChild(i);
			string id_name;
			int nID = 0;
			if (pSmoothingGroupNode->getAttr("ID", nID))
			{
				char buffer[1024];
				cry_sprintf(buffer, "%d", nID);
				id_name = buffer;
			}
			else
			{
				const char* pBuf = NULL;
				if (pSmoothingGroupNode->getAttr("GroupName", &pBuf))
					id_name = pBuf;
			}

			if (!id_name.empty())
			{
				SmoothingGroupPtr pSmoothingGroupPtr = new SmoothingGroup();
				pSmoothingGroupPtr->Serialize(pSmoothingGroupNode, bLoading, bUndo, pModel);
				AddSmoothingGroup(id_name, pSmoothingGroupPtr);
			}
		}
	}
	else
	{
		auto ii = m_SmoothingGroups.begin();
		for (; ii != m_SmoothingGroups.end(); ++ii)
		{
			SmoothingGroupPtr pSmoothingGroupPtr = ii->second;
			if (pSmoothingGroupPtr->GetPolygonCount() == 0)
				continue;
			XmlNodeRef pSmoothingGroupNode(xmlNode->newChild("SmoothingGroup"));
			pSmoothingGroupNode->setAttr("GroupName", ii->first);
			pSmoothingGroupPtr->Serialize(pSmoothingGroupNode, bLoading, bUndo, pModel);
		}
	}
}

void SmoothingGroupManager::Clear()
{
	m_SmoothingGroups.clear();
	m_MapPolygon2GroupId.clear();
}

void SmoothingGroupManager::Clean(Model* pModel)
{
	auto ii = m_MapPolygon2GroupId.begin();
	for (; ii != m_MapPolygon2GroupId.end(); )
	{
		if (!pModel->HasPolygon(ii->first))
		{
			m_SmoothingGroups[ii->second]->RemovePolygon(ii->first);
			if (m_SmoothingGroups[ii->second]->GetPolygonCount() == 0)
				m_SmoothingGroups.erase(ii->second);
			ii = m_MapPolygon2GroupId.erase(ii);
		}
		else
		{
			++ii;
		}
	}
}

std::vector<std::pair<string, SmoothingGroupPtr>> SmoothingGroupManager::GetSmoothingGroupList() const
{
	std::vector<std::pair<string, SmoothingGroupPtr>> smoothingGroupList;
	auto ii = m_SmoothingGroups.begin();
	for (; ii != m_SmoothingGroups.end(); ++ii)
		smoothingGroupList.push_back(*ii);
	return smoothingGroupList;
}

void SmoothingGroupManager::RemovePolygon(PolygonPtr pPolygon)
{
	auto ii = m_MapPolygon2GroupId.find(pPolygon);
	if (ii == m_MapPolygon2GroupId.end())
		return;

	string id_name = ii->second;
	m_MapPolygon2GroupId.erase(ii);

	auto iGroup = m_SmoothingGroups.find(id_name);
	if (iGroup == m_SmoothingGroups.end())
	{
		DESIGNER_ASSERT(0);
		return;
	}

	SmoothingGroupPtr pSmoothingGroup = iGroup->second;
	pSmoothingGroup->RemovePolygon(pPolygon);

	if (pSmoothingGroup->GetPolygonCount() == 0)
		RemoveSmoothingGroup(iGroup->first);
}

void SmoothingGroupManager::CopyFromModel(Model* pModel, const Model* pSourceModel)
{
	Clear();

	MODEL_SHELF_RECONSTRUCTOR_POSTFIX((Model*)pSourceModel, 0);
	MODEL_SHELF_RECONSTRUCTOR_POSTFIX(pModel, 1);

	SmoothingGroupManager* pSourceSmoothingGroupMgr = pSourceModel->GetSmoothingGroupMgr();
	std::vector<std::pair<string, SmoothingGroupPtr>> sourceSmoothingGroupList = pSourceSmoothingGroupMgr->GetSmoothingGroupList();

	for (int i = 0, iSmoothingGroupCount(sourceSmoothingGroupList.size()); i < iSmoothingGroupCount; ++i)
	{
		const char* groupName = sourceSmoothingGroupList[i].first;
		std::vector<PolygonPtr> polygons;
		for (int k = 0, iPolygonCount(sourceSmoothingGroupList[i].second->GetPolygonCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr pPolygon = sourceSmoothingGroupList[i].second->GetPolygon(k);
			int nPolygonIndex = -1;
			ShelfID nShelfID = eShelf_Any;
			for (int a = eShelf_Base; a < cShelfMax; ++a)
			{
				pSourceModel->SetShelf(static_cast<ShelfID>(a));
				nPolygonIndex = pSourceModel->GetPolygonIndex(pPolygon);
				if (nPolygonIndex != -1)
				{
					nShelfID = static_cast<ShelfID>(a);
					break;
				}
			}
			if (nShelfID == eShelf_Any || nPolygonIndex == -1)
				continue;
			pModel->SetShelf(nShelfID);
			polygons.push_back(pModel->GetPolygon(nPolygonIndex));
		}
		AddSmoothingGroup(groupName, new SmoothingGroup(polygons));
	}
}

void SmoothingGroupManager::InvalidateAll()
{
	auto ii = m_SmoothingGroups.begin();
	for (; ii != m_SmoothingGroups.end(); ++ii)
		ii->second->Invalidate();
}

string SmoothingGroupManager::GetEmptyGroupID() const
{
	string basicName = "SG";
	int count = 0;
	do
	{
		char buff[1024];
		cry_sprintf(buff, "%d", count);
		string combinedName = basicName + string(buff);
		if (m_SmoothingGroups.find(combinedName) == m_SmoothingGroups.end())
			return combinedName;
	}
	while (++count < 100000);
	return "";
}

void SmoothingGroupManager::InvalidateSmoothingGroup(PolygonPtr pPolygon)
{
	const char* groupName = GetSmoothingGroupID(pPolygon);
	if (groupName)
	{
		SmoothingGroupPtr pSmoothingGroupPtr = GetSmoothingGroup(groupName);
		pSmoothingGroupPtr->Invalidate();
	}
}
}

