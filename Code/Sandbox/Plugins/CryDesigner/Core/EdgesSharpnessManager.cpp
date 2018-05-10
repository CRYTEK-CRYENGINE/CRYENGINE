// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EdgesSharpnessManager.h"
#include "Core/Model.h"
#include "Core/Polygon.h"
#include "Util/ElementSet.h"

namespace Designer
{
void EdgeSharpnessManager::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo)
{
	if (bLoading)
	{
		int nEdgeSharpnessCount = xmlNode->getChildCount();
		for (int i = 0; i < nEdgeSharpnessCount; ++i)
		{
			XmlNodeRef pSemiSharpCreaseNode = xmlNode->getChild(i);

			EdgeSharpness semiSharpCrease;
			const char* name = NULL;
			pSemiSharpCreaseNode->getAttr("name", &name);
			semiSharpCrease.name = name;
			pSemiSharpCreaseNode->getAttr("sharpness", semiSharpCrease.sharpness);
			pSemiSharpCreaseNode->getAttr("guid", semiSharpCrease.guid);

			int nEdgeCount = pSemiSharpCreaseNode->getChildCount();
			for (int k = 0; k < nEdgeCount; ++k)
			{
				XmlNodeRef pEdgeNode = pSemiSharpCreaseNode->getChild(k);
				BrushEdge3D e;
				pEdgeNode->getAttr("v0", e.m_v[0]);
				pEdgeNode->getAttr("v1", e.m_v[1]);
				semiSharpCrease.edges.push_back(e);
			}

			m_EdgeSharpnessList.push_back(semiSharpCrease);
		}
	}
	else
	{
		auto ii = m_EdgeSharpnessList.begin();
		for (; ii != m_EdgeSharpnessList.end(); ++ii)
		{
			const EdgeSharpness& semiSharpCrease = *ii;
			if (semiSharpCrease.edges.empty())
				continue;

			XmlNodeRef pSemiSharpCreaseNode(xmlNode->newChild("SemiSharpCrease"));

			pSemiSharpCreaseNode->setAttr("name", semiSharpCrease.name);
			pSemiSharpCreaseNode->setAttr("sharpness", semiSharpCrease.sharpness);
			pSemiSharpCreaseNode->setAttr("guid", semiSharpCrease.guid);

			for (int i = 0, iEdgeCount(semiSharpCrease.edges.size()); i < iEdgeCount; ++i)
			{
				XmlNodeRef pEdgeNode = pSemiSharpCreaseNode->newChild("edge");
				pEdgeNode->setAttr("v0", semiSharpCrease.edges[i].m_v[0]);
				pEdgeNode->setAttr("v1", semiSharpCrease.edges[i].m_v[1]);
			}
		}
	}
}

EdgeSharpnessManager& EdgeSharpnessManager::operator=(const EdgeSharpnessManager& edgeSharpnessMgr)
{
	m_EdgeSharpnessList = edgeSharpnessMgr.m_EdgeSharpnessList;
	return *this;
}

bool EdgeSharpnessManager::AddEdges(const char* name, ElementSet* pElements, float sharpness)
{
	std::vector<BrushEdge3D> edges;
	for (int i = 0, iCount(pElements->GetCount()); i < iCount; ++i)
	{
		const Element& element = pElements->Get(i);
		if (element.IsEdge())
		{
			edges.push_back(element.GetEdge());
		}
		if (element.IsPolygon() && element.m_pPolygon)
		{
			for (int k = 0, iEdgeCount(element.m_pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
				edges.push_back(element.m_pPolygon->GetEdge(k));
		}
	}
	return AddEdges(name, edges, sharpness);
}

bool EdgeSharpnessManager::AddEdges(const char* name, const std::vector<BrushEdge3D>& edges, float sharpness)
{
	if (edges.empty() || HasName(name))
		return false;

	int iEdgeCount(edges.size());
	for (int i = 0, iEdgeCount(edges.size()); i < iEdgeCount; ++i)
		DeleteEdge(GetEdgeInfo(edges[i]));

	EdgeSharpness edgeSharpness;
	edgeSharpness.name = name;
	edgeSharpness.edges = edges;
	edgeSharpness.sharpness = sharpness;
	edgeSharpness.guid = CryGUID::Create();
	m_EdgeSharpnessList.push_back(edgeSharpness);

	return true;
}

void EdgeSharpnessManager::RemoveEdgeSharpness(const char* name)
{
	auto ii = m_EdgeSharpnessList.begin();
	for (; ii != m_EdgeSharpnessList.end(); ++ii)
	{
		if (!stricmp(name, ii->name.c_str()))
		{
			m_EdgeSharpnessList.erase(ii);
			break;
		}
	}
}

void EdgeSharpnessManager::RemoveEdgeSharpness(const BrushEdge3D& edge)
{
	BrushEdge3D invEdge = edge.GetInverted();
	auto ii = m_EdgeSharpnessList.begin();
	for (; ii != m_EdgeSharpnessList.end(); )
	{
		for (int i = 0, iEdgeCount(ii->edges.size()); i < iEdgeCount; ++i)
		{
			if (ii->edges[i].IsEquivalent(edge) || ii->edges[i].IsEquivalent(invEdge))
			{
				ii->edges.erase(ii->edges.begin() + i);
				break;
			}
		}
		if (ii->edges.empty())
			ii = m_EdgeSharpnessList.erase(ii);
		else
			++ii;
	}
}

void EdgeSharpnessManager::SetSharpness(const char* name, float sharpness)
{
	EdgeSharpness* pEdgeSharpness = FindEdgeSharpness(name);
	if (pEdgeSharpness == NULL)
		return;
	pEdgeSharpness->sharpness = sharpness;
}

void EdgeSharpnessManager::Rename(const char* oldName, const char* newName)
{
	EdgeSharpness* pEdgeSharpness = FindEdgeSharpness(oldName);
	if (pEdgeSharpness == NULL)
		return;
	pEdgeSharpness->name = newName;
}

EdgeSharpness* EdgeSharpnessManager::FindEdgeSharpness(const char* name)
{
	if (name)
	{
		for (int i = 0, iCount(m_EdgeSharpnessList.size()); i < iCount; ++i)
		{
			if (m_EdgeSharpnessList[i].name == name)
				return &m_EdgeSharpnessList[i];
		}
	}
	return NULL;
}

float EdgeSharpnessManager::FindSharpness(const BrushEdge3D& edge) const
{
	BrushEdge3D invEdge = edge.GetInverted();
	for (int i = 0, iCount(m_EdgeSharpnessList.size()); i < iCount; ++i)
	{
		const std::vector<BrushEdge3D>& edges = m_EdgeSharpnessList[i].edges;
		for (int k = 0, iEdgeCount(edges.size()); k < iEdgeCount; ++k)
		{
			if (edges[k].IsEquivalent(edge) || edges[k].IsEquivalent(invEdge))
				return m_EdgeSharpnessList[i].sharpness;
		}
	}
	return 0;
}

bool EdgeSharpnessManager::HasName(const char* name) const
{
	for (int i = 0, iCount(m_EdgeSharpnessList.size()); i < iCount; ++i)
	{
		if (!stricmp(m_EdgeSharpnessList[i].name.c_str(), name))
			return true;
	}
	return false;
}

SharpEdgeInfo EdgeSharpnessManager::GetEdgeInfo(const BrushEdge3D& edge)
{
	SharpEdgeInfo sei;
	BrushEdge3D invertedEdge = edge.GetInverted();

	for (int i = 0, iCount(m_EdgeSharpnessList.size()); i < iCount; ++i)
	{
		EdgeSharpness& edgeSharpness = m_EdgeSharpnessList[i];
		for (int k = 0, iEdgeCount(edgeSharpness.edges.size()); k < iEdgeCount; ++k)
		{
			if (edgeSharpness.edges[k].IsEquivalent(edge) || edgeSharpness.edges[k].IsEquivalent(invertedEdge))
			{
				sei.sharpnessindex = i;
				sei.edgeindex = k;
				break;
			}
		}
		if (sei.edgeindex != -1)
			break;
	}

	return sei;
}

void EdgeSharpnessManager::DeleteEdge(const SharpEdgeInfo& edgeInfo)
{
	if (edgeInfo.sharpnessindex == -1 || edgeInfo.sharpnessindex >= m_EdgeSharpnessList.size())
		return;

	EdgeSharpness& sharpness = m_EdgeSharpnessList[edgeInfo.sharpnessindex];

	if (edgeInfo.edgeindex >= sharpness.edges.size())
		return;

	sharpness.edges.erase(sharpness.edges.begin() + edgeInfo.edgeindex);
}

string EdgeSharpnessManager::GenerateValidName(const char* baseName) const
{
	string validName(baseName);
	char numStr[10];
	int i = 0;
	while (i < 1000000)
	{
		if (!HasName(validName))
			break;
		validName = baseName;
		cry_sprintf(numStr, "%d", ++i);
		validName += numStr;
	}
	return validName;
}
}

