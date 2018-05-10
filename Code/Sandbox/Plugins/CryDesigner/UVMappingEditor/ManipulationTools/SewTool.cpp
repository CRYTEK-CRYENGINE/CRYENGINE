// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../UVMappingEditor.h"
#include "Core/UVIslandManager.h"
#include "SewTool.h"
#include "SelectSharedTool.h"
#include "LoopSelectionTool.h"
#include "../UVUndo.h"

namespace Designer {
namespace UVMapping
{
void SewTool::Enter()
{
	UVElementSetPtr pElementSet = GetUVEditor()->GetElementSet();

	CUndo undo("UVMapping Editor : Sew");

	if (pElementSet->AllOnlyVertices() || pElementSet->AllOnlyEdges())
	{
		CUndo::Record(new UVSelectionUndo);
		CUndo::Record(new UVIslandUndo);
	}

	if (pElementSet->AllOnlyVertices())
		SewVertices();
	else if (pElementSet->AllOnlyEdges())
		SewEdges();

	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
}

void SewTool::SewEdges()
{
	SortedEdges se0, se1;
	if (!SortEdges(se0, se1))
		return;

	SewEdges(se0, se1);

	GetUVEditor()->GetUVIslandMgr()->RemoveEmptyUVIslands();
	GetUVEditor()->ApplyPostProcess();
}

void SewTool::SewEdges(SortedEdges& se0, SortedEdges& se1)
{
	Matrix33 offsetTM = Matrix33::CreateIdentity();

	UVElementSetPtr pVertexElementSet = new UVElementSet;
	UVCluster cluster;
	cluster.SetElementSetPtr(pVertexElementSet);
	for (int i = 0, iCount(se0.uvs.size()); i < iCount; ++i)
	{
		cluster.Invalidate();
		pVertexElementSet->Clear();
		pVertexElementSet->AddElement(UVElement(se0.uvs[i]));
		offsetTM.SetColumn(2, se1.uvs[iCount - i - 1].GetUV() - se0.uvs[i].GetUV());
		cluster.Transform(offsetTM);
	}

	if (se0.pUVIsland != se1.pUVIsland)
		se1.pUVIsland->Join(se0.pUVIsland);
}

bool SewTool::SortEdges(SortedEdges& outSortedEdges0, SortedEdges& outSortedEdges1)
{
	UVElementSetPtr pElementSet = GetUVEditor()->GetElementSet();
	std::vector<UVEdge> uvEdges;
	for (int i = 0, iCount(pElementSet->GetCount()); i < iCount; ++i)
	{
		UVElement& uvElement = pElementSet->GetElement(i);
		if (!uvElement.IsEdge())
			return false;
		uvEdges.push_back(UVEdge(uvElement.m_pUVIsland, uvElement.m_UVVertices[0], uvElement.m_UVVertices[1]));
	}

	std::vector<std::vector<UVEdge>> islolatedEdges;
	if (!FindSerialEdges(uvEdges, islolatedEdges))
		return false;

	if (islolatedEdges.size() != 2)
		return false;

	if (islolatedEdges[0].size() != islolatedEdges[1].size())
		return false;

	bool bReverse[] = { false, false };
	for (int i = 0; i < 2; ++i)
	{
		if (IsUVCCW(islolatedEdges[i][0].uv0.pPolygon))
			continue;
		std::reverse(islolatedEdges[i].begin(), islolatedEdges[i].end());
		for (int k = 0, iCount(islolatedEdges[i].size()); k < iCount; ++k)
			std::swap(islolatedEdges[i][k].uv0, islolatedEdges[i][k].uv1);
		bReverse[i] = true;
	}

	outSortedEdges0.AssignUVs(islolatedEdges[0]);
	outSortedEdges0.pUVIsland = islolatedEdges[0][0].pUVIsland;
	outSortedEdges0.UpdateAABB();
	outSortedEdges0.bReverse = bReverse[0];

	outSortedEdges1.AssignUVs(islolatedEdges[1]);
	outSortedEdges1.pUVIsland = islolatedEdges[1][0].pUVIsland;
	outSortedEdges1.UpdateAABB();
	outSortedEdges1.bReverse = bReverse[1];

	if (pElementSet->GetElement(0).m_pUVIsland != outSortedEdges0.pUVIsland)
		std::swap(outSortedEdges0, outSortedEdges1);

	return true;
}

bool SewTool::FindSerialEdges(const std::vector<UVEdge>& inputEdges, std::vector<std::vector<UVEdge>>& outEdges) const
{
	std::vector<UVEdge> sortedEdges;
	std::set<int> usedEdges;

	while (usedEdges.size() < inputEdges.size())
	{
		bool bInserted = false;

		if (sortedEdges.empty())
		{
			for (int i = 0, iCount(inputEdges.size()); i < iCount; ++i)
			{
				if (usedEdges.find(i) == usedEdges.end())
				{
					sortedEdges.push_back(inputEdges[i]);
					usedEdges.insert(i);
					break;
				}
			}
		}

		if (sortedEdges.empty())
			break;

		for (int i = 0, iCount(inputEdges.size()); i < iCount; ++i)
		{
			if (usedEdges.find(i) != usedEdges.end())
				continue;

			auto iEdge = sortedEdges.begin();
			for (; iEdge != sortedEdges.end(); ++iEdge)
			{
				if (inputEdges[i].uv0.IsEquivalent((*iEdge).uv1))
				{
					sortedEdges.insert(iEdge + 1, inputEdges[i]);
					usedEdges.insert(i);
					bInserted = true;
					break;
				}
				else if (inputEdges[i].uv1.IsEquivalent((*iEdge).uv0))
				{
					sortedEdges.insert(iEdge, inputEdges[i]);
					usedEdges.insert(i);
					bInserted = true;
					break;
				}
			}
			if (bInserted)
				break;
		}

		if (!bInserted || usedEdges.size() == inputEdges.size())
		{
			outEdges.push_back(sortedEdges);
			sortedEdges.clear();
		}
	}

	return true;
}

void SewTool::SewVertices()
{
	UVElementSetPtr pElementSet = GetUVEditor()->GetElementSet();

	if (pElementSet->GetCount() != 2)
		return;

	UVElement& e0 = pElementSet->GetElement(0);
	UVElement& e1 = pElementSet->GetElement(1);

	UVElementSetPtr pVertexElementSet = new UVElementSet;
	pVertexElementSet->AddElement(e0);
	UVCluster uvCluster;
	uvCluster.SetElementSetPtr(pVertexElementSet);

	Matrix33 tm = Matrix33::CreateIdentity();
	tm.SetColumn(2, e1.m_UVVertices[0].GetUV() - e0.m_UVVertices[0].GetUV());
	uvCluster.Transform(tm);

	if (e0.m_pUVIsland != e1.m_pUVIsland)
	{
		e1.m_pUVIsland->Join(e0.m_pUVIsland);
		GetUVEditor()->GetUVIslandMgr()->RemoveEmptyUVIslands();
	}

	GetUVEditor()->ApplyPostProcess();
}

void MoveAndSewTool::Enter()
{
	UVElementSetPtr pElementSet = GetUVEditor()->GetElementSet();

	if (pElementSet->AllOnlyEdges())
	{
		CUndo undo("UVMapping Editor : Move And Sew");
		CUndo::Record(new UVSelectionUndo);
		CUndo::Record(new UVIslandUndo);
		MoveAndSew();
	}

	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
}

void MoveAndSewTool::MoveAndSew()
{
	SortedEdges se0, se1;
	if (!SortEdges(se0, se1))
		return;

	if (se0.pUVIsland == se1.pUVIsland)
		return;

	if (se0.bReverse != se1.bReverse)
	{
		UVElementSetPtr pUVElements = new UVElementSet();
		pUVElements->AddElement(UVElement(se0.pUVIsland));
		Vec3 center = se0.pUVIsland->GetUVBoundBox().GetCenter();

		UVCluster cluster;
		cluster.SetElementSetPtr(pUVElements);
		cluster.Flip(Vec2(center.x, center.y), Vec2(1.0f, 0));

		se0.Reset();
		se1.Reset();
		if (!SortEdges(se0, se1) || se0.bReverse != se1.bReverse)
			return;
	}

	MoveUVIsland(se0, se1);
	ScaleUVIsland(se0, se1);
	SewEdges(se0, se1);

	GetUVEditor()->GetUVIslandMgr()->RemoveEmptyUVIslands();
	GetUVEditor()->ApplyPostProcess();
}

void MoveAndSewTool::MoveUVIsland(SortedEdges& se0, SortedEdges& se1)
{
	SBrushLine<float> line0(se0.uvs[0].GetUV(), se0.uvs[se0.uvs.size() - 1].GetUV());
	SBrushLine<float> line1(se1.uvs[0].GetUV(), se1.uvs[se1.uvs.size() - 1].GetUV());
	float deltaRadian = std::acos(line0.m_Normal.Dot(-line1.m_Normal));

	Vec3 v0(0, 0, 0);
	Vec3 v1(line0.m_Normal);
	Vec3 v2(-line1.m_Normal);
	Vec3 v01 = (v1 - v0).GetNormalized();
	Vec3 v02 = (v2 - v0).GetNormalized();
	if (v01.Cross(v02).z < 0)
		deltaRadian = -deltaRadian;

	if (std::abs(deltaRadian) > kDesignerEpsilon)
	{
		Vec3 pivot = se0.pUVIsland->GetUVBoundBox().GetCenter();
		Matrix33 pivotTM = Matrix33::CreateIdentity();
		pivotTM.SetColumn(2, Vec3(-pivot.x, -pivot.y, 0));
		Matrix33 tm = Matrix33::CreateIdentity();
		tm.SetRotationZ(deltaRadian);

		se0.pUVIsland->Transform(pivotTM);
		se0.pUVIsland->Transform(tm);
		pivotTM.SetColumn(2, Vec3(pivot.x, pivot.y, 0));
		se0.pUVIsland->Transform(pivotTM);
	}

	se0.UpdateAABB();
	se1.UpdateAABB();
	Matrix33 offsetTM = Matrix33::CreateIdentity();
	offsetTM.SetColumn(2, se1.aabb.GetCenter() - se0.aabb.GetCenter());
	se0.pUVIsland->Transform(offsetTM);
}

void MoveAndSewTool::ScaleUVIsland(SortedEdges& se0, SortedEdges& se1)
{
	SBrushEdge<float> e0(se0.uvs[0].GetUV(), se0.uvs[se0.uvs.size() - 1].GetUV());
	SBrushEdge<float> e1(se1.uvs[0].GetUV(), se1.uvs[se1.uvs.size() - 1].GetUV());

	float length0 = e0.GetLength();
	float length1 = e1.GetLength();

	float ratio = length1 / length0;

	std::set<UVVertex> allUVs;
	GetAllUVsFromUVIsland(se0.pUVIsland, allUVs);

	UVElementSetPtr pUVElementSet = new UVElementSet;
	for (auto iUV = allUVs.begin(); iUV != allUVs.end(); ++iUV)
	{
		if (se0.HasUV((*iUV).GetUV()))
			continue;
		pUVElementSet->AddElement(UVElement(*iUV));
	}

	UVCluster cluster;
	cluster.SetElementSetPtr(pUVElementSet);

	Vec2 pivot = (e0.m_v[0] + e0.m_v[1]) * 0.5f;

	Matrix33 pivotTM = Matrix33::CreateIdentity();
	pivotTM(0, 2) = -pivot.x;
	pivotTM(1, 2) = -pivot.y;
	cluster.Transform(pivotTM);

	Matrix33 scaleTM = Matrix33::CreateScale(Vec3(ratio, ratio, 1));
	cluster.Transform(scaleTM);

	pivotTM(0, 2) = pivot.x;
	pivotTM(1, 2) = pivot.y;
	cluster.Transform(pivotTM);
}

void SmartSewTool::Enter()
{
	UVElementSetPtr uvSelectedSet = GetUVEditor()->GetElementSet();

	if (uvSelectedSet->IsEmpty())
	{
		//Early return here without setting back to previous tool will lead to stack overflow
		//if then changing to another tool that does also switch back to previous tool
		//This design is very fragile and this should not be a tool but rather a function
		//Anyway just make sure you restore previous tool in every tool that wants this behavior no matter
		//which conditions happen, early exits should not be allowed.
		GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
		return;
	}

	CUndo undo("UVMapping Editor : SmartSew Selection");
	CUndo::Record(new UVSelectionUndo);
	CUndo::Record(new UVIslandUndo);

	if (uvSelectedSet->GetCount() == 1)
		LoopSelectionTool::BorderSelection();

	GetUVEditor()->UpdateSharedSelection();

	if (GetUVEditor()->GetElementSet()->GetCount() > GetUVEditor()->GetSharedElementSet()->GetCount())
	{
		*GetUVEditor()->GetElementSet() = *GetUVEditor()->GetSharedElementSet();
		GetUVEditor()->GetSharedElementSet()->Clear();
		GetUVEditor()->UpdateSharedSelection();
		*GetUVEditor()->GetElementSet() = *GetUVEditor()->GetSharedElementSet();
		GetUVEditor()->UpdateSharedSelection();
	}

	SelectSharedTool::SelectShared();
	MoveAndSew();

	SyncSelection();
	GetUVEditor()->UpdateGizmoPos();
	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Sew, eUVMappingToolGroup_Manipulation, "Sew", SewTool,
                                    sew, "runs sew tool", "uvmapping.sew")
REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_MoveAndSew, eUVMappingToolGroup_Manipulation, "Move and Sew", MoveAndSewTool,
                                    move_and_sew, "runs move and sew tool", "uv_mapping.move_and_sew")
REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_SmartSew, eUVMappingToolGroup_Manipulation, "Smart Sew", SmartSewTool,
                                    smart_sew, "runs smart sew tool", "uvmapping.smart_sew")

