// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVIslandManager.h"

namespace Designer
{

void UVIslandManager::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo, Model* pModel)
{
	if (bLoading)
	{
		Clear();
		int childCount = xmlNode->getChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			XmlNodeRef pUVIsland = xmlNode->getChild(i);
			UVIsland* pIsland = new UVIsland();
			pIsland->Serialize(pUVIsland, bLoading, bUndo, pModel);
			m_UVIslands.push_back(pIsland);
		}
	}
	else
	{
		for (int i = 0, iCount(m_UVIslands.size()); i < iCount; ++i)
		{
			XmlNodeRef pUVIsland = xmlNode->newChild("UVIsland");
			m_UVIslands[i]->Serialize(pUVIsland, bLoading, bUndo, pModel);
		}
	}
}

UVIslandManager& UVIslandManager::operator=(const UVIslandManager& uvIslandMgr)
{
	Clear();

	for (int i = 0, iCount(uvIslandMgr.GetCount()); i < iCount; ++i)
		AddUVIsland(new UVIsland(*uvIslandMgr.GetUVIsland(i)));

	return *this;
}

UVIslandManager* UVIslandManager::Clone() const
{
	UVIslandManager* pClone = new UVIslandManager;
	*pClone = *this;
	return pClone;
}

void UVIslandManager::AddUVIsland(UVIslandPtr pUVIsland)
{
	for (int i = 0, iCount(m_UVIslands.size()); i < iCount; ++i)
	{
		if (m_UVIslands[i] == pUVIsland)
			return;
	}

	for (int i = 0, iPolygonCount(pUVIsland->GetCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = pUVIsland->GetPolygon(i);
		std::vector<UVIslandPtr> owners;

		if (FindUVIslandsHavingPolygon(polygon, owners))
		{
			for (int k = 0, iIslandCount(owners.size()); k < iIslandCount; ++k)
				owners[k]->DeletePolygon(polygon);
		}
	}
	RemoveEmptyUVIslands();
	m_UVIslands.push_back(pUVIsland);
}

bool UVIslandManager::FindUVIslandsHavingPolygon(PolygonPtr polygon, std::vector<UVIslandPtr>& OutUVIslands)
{
	bool bFound = false;
	for (int i = 0, iCount(m_UVIslands.size()); i < iCount; ++i)
	{
		if (m_UVIslands[i]->HasPolygon(polygon))
		{
			bFound = true;
			OutUVIslands.push_back(m_UVIslands[i]);
		}
	}
	return bFound;
}

void UVIslandManager::RemoveEmptyUVIslands()
{
	auto ii = m_UVIslands.begin();
	for (; ii != m_UVIslands.end(); )
	{
		if ((*ii)->GetCount() == 0)
			ii = m_UVIslands.erase(ii);
		else
			++ii;
	}
}

void UVIslandManager::Clean(Model* pModel)
{
	for (int i = 0, iCount(GetCount()); i < iCount; ++i)
		GetUVIsland(i)->Clean(pModel);
	RemoveEmptyUVIslands();
}

void UVIslandManager::Invalidate()
{
	auto iter = m_UVIslands.begin();
	for (; iter != m_UVIslands.end(); ++iter)
		(*iter)->Invalidate();
}

bool UVIslandManager::HasSubMatID(int nSubMatID) const
{
	auto ii = m_UVIslands.begin();
	for (; ii != m_UVIslands.end(); ++ii)
	{
		if ((*ii)->HasSubMatID(nSubMatID))
			return true;
	}
	return false;
}

UVIslandPtr UVIslandManager::FindUVIsland(const CryGUID& guid) const
{
	auto ii = m_UVIslands.begin();
	for (; ii != m_UVIslands.end(); ++ii)
	{
		if (guid == (*ii)->GetGUID())
			return *ii;
	}
	return NULL;
}

void UVIslandManager::Reset(Model* pModel)
{
	auto ii = m_UVIslands.begin();
	for (; ii != m_UVIslands.end(); ++ii)
		(*ii)->ResetPolygonsInUVIslands(pModel);
}

void UVIslandManager::Join(UVIslandManager* pUVIslandManager)
{
	for (int i = 0, iCount(pUVIslandManager->GetCount()); i < iCount; ++i)
		AddUVIsland(pUVIslandManager->GetUVIsland(i));
}

void UVIslandManager::ConvertIsolatedAreasToIslands()
{
	std::vector<UVIslandPtr> newUVIslands;

	auto ii = m_UVIslands.begin();
	for (; ii != m_UVIslands.end(); ++ii)
	{
		if ((*ii)->IsEmpty())
			continue;
		std::vector<UVIslandPtr> isolatedAreas;
		if ((*ii)->FindIsolatedAreas(isolatedAreas))
			newUVIslands.insert(newUVIslands.end(), isolatedAreas.begin(), isolatedAreas.end());
		else
			newUVIslands.push_back(*ii);
	}

	if (newUVIslands.size() > m_UVIslands.size())
		m_UVIslands = newUVIslands;
}

}

