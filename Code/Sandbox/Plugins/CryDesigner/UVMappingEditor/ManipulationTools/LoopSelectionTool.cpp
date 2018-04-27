// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LoopSelectionTool.h"
#include "../UVUndo.h"
#include "../UVMappingEditor.h"

namespace Designer {
namespace UVMapping
{
void LoopSelectionTool::Enter()
{
	UVElementSetPtr pUVElements = GetUVEditor()->GetElementSet();
	if (!pUVElements->IsEmpty())
	{
		CUndo undo("UVMapping Editor : Loop Selection");
		CUndo::Record(new UVSelectionUndo);

		UVElementSetPtr pInitUVElement = pUVElements->Clone();

		BorderSelection(pInitUVElement);
		LoopSelection(pInitUVElement);

		SyncSelection();
		GetUVEditor()->UpdateGizmoPos();
	}
	else
	{
		MessageBox("Warning", "At least an edge needs to be selected to use this tool.");
	}
	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
}

bool LoopSelectionTool::BorderSelection(UVElementSetPtr pUVElements)
{
	if (pUVElements == NULL)
		pUVElements = GetUVEditor()->GetElementSet();

	if (pUVElements->IsEmpty())
		return false;

	int initialElementCount = pUVElements->GetCount();

	std::vector<UVElement> elements;
	for (int i = 0, iCount(pUVElements->GetCount()); i < iCount; ++i)
	{
		const UVElement& element = pUVElements->GetElement(i);
		if (!element.IsEdge())
			continue;
		elements.push_back(element);
	}

	while (!elements.empty())
		elements = BorderSelection(elements);

	return pUVElements->GetCount() > initialElementCount;
}

std::vector<UVElement> LoopSelectionTool::BorderSelection(const std::vector<UVElement>& input)
{
	UVElementSetPtr pUVElements = GetUVEditor()->GetElementSet();
	std::vector<UVElement> addedElements;

	for (int i = 0, iCount(input.size()); i < iCount; ++i)
	{
		const UVElement& uvElement = input[i];
		if (!uvElement.IsEdge())
			continue;

		UVIslandPtr pUVIslandPtr = uvElement.m_pUVIsland;

		std::vector<UVEdge> prevConnectedUVEdges;
		std::vector<UVEdge> nextConnectedUVEdges;
		if (!SearchUtil::FindConnectedEdges(uvElement.m_UVVertices[0].GetUV(), uvElement.m_UVVertices[1].GetUV(), pUVIslandPtr, prevConnectedUVEdges, nextConnectedUVEdges))
			continue;

		int prev = FindBestBorderUVEdge(uvElement.m_UVVertices[0].GetUV(), uvElement.m_UVVertices[1].GetUV(), pUVIslandPtr, prevConnectedUVEdges);
		if (prev != -1)
		{
			UVElement element(prevConnectedUVEdges[prev]);
			if (pUVElements->AddElement(element))
				addedElements.push_back(element);
		}

		int next = FindBestBorderUVEdge(uvElement.m_UVVertices[0].GetUV(), uvElement.m_UVVertices[1].GetUV(), pUVIslandPtr, nextConnectedUVEdges);
		if (next != -1)
		{
			UVElement element(nextConnectedUVEdges[next]);
			if (pUVElements->AddElement(element))
				addedElements.push_back(element);
		}
	}

	return addedElements;
}

int LoopSelectionTool::FindBestBorderUVEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIslandPtr, const std::vector<UVEdge>& candidateUVEdges)
{
	const float kThresholdAngle = 80.0f;

	Vec3 vUVEdgeDir = (uv1 - uv0).GetNormalized();
	for (int i = 0, iEdgeCount(candidateUVEdges.size()); i < iEdgeCount; ++i)
	{
		if (!SearchUtil::FindPolygonContainingEdge(
		      candidateUVEdges[i].uv1.GetUV(),
		      candidateUVEdges[i].uv0.GetUV(),
		      pUVIslandPtr))
		{
			Vec3 uvCandidateEdgeDir = (candidateUVEdges[i].uv1.GetUV() - candidateUVEdges[i].uv0.GetUV()).GetNormalized();
			float angle = RAD2DEG(std::acos(vUVEdgeDir.Dot(uvCandidateEdgeDir)));
			if (angle < kThresholdAngle)
				return i;
		}
	}

	return -1;
}

bool LoopSelectionTool::LoopSelection(UVElementSetPtr pUVElements)
{
	if (pUVElements == NULL)
		pUVElements = GetUVEditor()->GetElementSet();

	if (pUVElements->IsEmpty())
		return false;

	int initialElementCount = pUVElements->GetCount();

	std::vector<UVElement> elements;
	for (int i = 0, iCount(pUVElements->GetCount()); i < iCount; ++i)
	{
		const UVElement& element = pUVElements->GetElement(i);
		if (!element.IsEdge())
			continue;
		elements.push_back(element);
	}

	while (!elements.empty())
		elements = LoopSelection(elements);

	return pUVElements->GetCount() > initialElementCount;
}

std::vector<UVElement> LoopSelectionTool::LoopSelection(const std::vector<UVElement>& input)
{
	UVElementSetPtr pUVElements = GetUVEditor()->GetElementSet();
	std::vector<UVElement> addedElements;

	for (int i = 0, iCount(input.size()); i < iCount; ++i)
	{
		const UVElement& uvElement = input[i];
		if (!uvElement.IsEdge())
			continue;

		UVIslandPtr pUVIslandPtr = uvElement.m_pUVIsland;

		std::vector<UVEdge> prevConnectedUVEdges;
		std::vector<UVEdge> nextConnectedUVEdges;
		if (!SearchUtil::FindConnectedEdges(uvElement.m_UVVertices[0].GetUV(), uvElement.m_UVVertices[1].GetUV(), pUVIslandPtr, prevConnectedUVEdges, nextConnectedUVEdges))
			continue;

		int prev = FindBestLoopUVEdge(uvElement.m_UVVertices[0].GetUV(), uvElement.m_UVVertices[1].GetUV(), pUVIslandPtr, prevConnectedUVEdges);
		if (prev != -1)
		{
			UVElement element(prevConnectedUVEdges[prev]);
			if (pUVElements->AddElement(element))
				addedElements.push_back(element);
		}

		int next = FindBestLoopUVEdge(uvElement.m_UVVertices[0].GetUV(), uvElement.m_UVVertices[1].GetUV(), pUVIslandPtr, nextConnectedUVEdges);
		if (next != -1)
		{
			UVElement element(nextConnectedUVEdges[next]);
			if (pUVElements->AddElement(element))
				addedElements.push_back(element);
		}
	}

	return addedElements;
}

int LoopSelectionTool::FindBestLoopUVEdge(const Vec2& uv0, const Vec2& uv1, UVIslandPtr pUVIslandPtr, const std::vector<UVEdge>& candidateUVEdges)
{
	if (candidateUVEdges.size() != 4)
		return -1;

	const float kMinimum = 0.4f;
	Vec3 vUVEdgeDir = (uv1 - uv0).GetNormalized();
	float bestDot = -1;
	int nBestUVIndex = -1;
	for (int i = 0, iEdgeCount(candidateUVEdges.size()); i < iEdgeCount; ++i)
	{
		Vec3 uvCandidateEdgeDir = (candidateUVEdges[i].uv1.GetUV() - candidateUVEdges[i].uv0.GetUV()).GetNormalized();
		float dot = vUVEdgeDir.Dot(uvCandidateEdgeDir);
		if (dot > bestDot)
		{
			bestDot = dot;
			nBestUVIndex = i;

		}
	}
	return nBestUVIndex;
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_LoopSelect, eUVMappingToolGroup_Manipulation, "Loop Selection", LoopSelectionTool,
                                    loopselection, "runs loop selection tool", "uvmapping.loopselection")

