// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Display.h"
#include "ElementSet.h"
#include "Core/Model.h"
#include "Core/HalfEdgeMesh.h"
#include "Util/ElementSet.h"
#include "Util/ExcludedEdgeManager.h"
#include "DesignerEditor.h"
#include "Objects/DisplayContext.h"
#include "Util/MFCUtil.h"

namespace Designer {
namespace Display
{
void DisplayHighlightedElements(DisplayContext& dc, MainContext& mc, int nPickFlag, ExcludedEdgeManager* pExcludedEdgeMgr)
{
	if (Deprecated::CheckVirtualKey(VK_SPACE))
		dc.DepthTestOff();

	if (nPickFlag & ePF_Vertex)
		DisplayHighlightedVertices(dc, mc);

	if (nPickFlag & ePF_Edge)
		DisplayHighlightedEdges(dc, mc, pExcludedEdgeMgr);

	if (nPickFlag & ePF_Polygon)
		DisplayHighlightedPolygons(dc, mc);

	dc.DepthTestOn();
}

void DisplayHighlightedVertices(DisplayContext& dc, MainContext& mc, bool bExcludeVerticesFromSecondInOpenPolygon)
{
	if (GetDesigner() == NULL)
		return;

	ElementSet* pElements = DesignerSession::GetInstance()->GetSelectedElements();

	dc.PopMatrix();

	MODEL_SHELF_RECONSTRUCTOR(mc.pModel);
	for (int k = eShelf_Base; k < cShelfMax; ++k)
	{
		mc.pModel->SetShelf(static_cast<ShelfID>(k));
		for (int i = 0, iPolygonCount(mc.pModel->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr pPolygon = mc.pModel->GetPolygon(i);
			if (pPolygon->CheckFlags(ePolyFlag_Hidden | ePolyFlag_Mirrored))
				continue;
			for (int a = 0, iVertexCount(pPolygon->GetVertexCount()); a < iVertexCount; ++a)
			{
				const BrushVec3& v = pPolygon->GetPos(a);
				if (pElements->HasVertex(v))
					continue;

				if (bExcludeVerticesFromSecondInOpenPolygon)
				{
					BrushVec3 prevVertex;
					if (pPolygon->IsOpen() && pPolygon->GetPrevVertex(a, prevVertex))
						continue;
				}

				BrushVec3 vWorldVertexPos = mc.pObject->GetWorldTM().TransformPoint(v);
				BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, vWorldVertexPos);
				dc.SetColor(kElementBoxColor);
				dc.DrawSolidBox(ToVec3(vWorldVertexPos - vBoxSize), ToVec3(vWorldVertexPos + vBoxSize));
			}
		}
	}

	dc.PushMatrix(mc.pObject->GetWorldTM());
}

void DisplayHighlightedEdges(DisplayContext& dc, MainContext& mc, ExcludedEdgeManager* pExcludedEdgeMgr)
{
	DisplayModel(dc, mc.pModel, pExcludedEdgeMgr, eShelf_Any, kElementEdgeThickness, kElementBoxColor);
}

void DisplayHighlightedPolygons(DisplayContext& dc, MainContext& mc)
{
	if (GetDesigner() == NULL)
		return;

	ElementSet* pElements = DesignerSession::GetInstance()->GetSelectedElements();

	dc.PopMatrix();

	MODEL_SHELF_RECONSTRUCTOR(mc.pModel);
	for (int k = eShelf_Base; k < cShelfMax; ++k)
	{
		mc.pModel->SetShelf(static_cast<ShelfID>(k));
		for (int i = 0, iPolygonCount(mc.pModel->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr pPolygon = mc.pModel->GetPolygon(i);
			if (!pPolygon->IsValid() || pPolygon->CheckFlags(ePolyFlag_Hidden | ePolyFlag_Mirrored))
				continue;
			if (pElements->HasPolygonSelected(pPolygon))
				dc.SetColor(kSelectedColor);
			else
				dc.SetColor(kElementBoxColor);
			BrushVec3 pos = mc.pObject->GetWorldTM().TransformPoint(pPolygon->GetRepresentativePosition());
			BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, pos);
			dc.DrawSolidBox(ToVec3(pos - vBoxSize), ToVec3(pos + vBoxSize));
		}
	}

	dc.PushMatrix(mc.pObject->GetWorldTM());
}

void DisplayModel(DisplayContext& dc, Model* pModel, ExcludedEdgeManager* pExcludedEdgeMgr, ShelfID nShelf, const int nLineThickness, const ColorB& lineColor)
{
	MODEL_SHELF_RECONSTRUCTOR(pModel);
	for (int i = eShelf_Base; i < cShelfMax; ++i)
	{
		if (nShelf != eShelf_Any && nShelf != i)
			continue;
		pModel->SetShelf(static_cast<ShelfID>(i));
		std::vector<PolygonPtr> polygonList;
		pModel->GetPolygonList(polygonList, true);
		DisplayPolygons(dc, polygonList, pExcludedEdgeMgr, nLineThickness, lineColor);
	}
	DisplaySubdividedMesh(dc, pModel);
}

void DisplaySubdividedMesh(DisplayContext& dc, Model* pModel)
{
	HalfEdgeMesh* pSubdivisionResult = pModel->GetSubdivisionResult();
	if (!pSubdivisionResult || gDesignerSettings.bDisplayTriangulation || !gDesignerSettings.bDisplaySubdividedResult)
		return;

	dc.SetColor(ColorB(150, 150, 150));
	dc.SetLineWidth(2);

	for (int i = 0, iFaceCount(pSubdivisionResult->GetFaceCount()); i < iFaceCount; ++i)
	{
		const HE_Face& f = pSubdivisionResult->GetFace(i);
		std::vector<BrushVec3> posList;
		pSubdivisionResult->GetFacePositions(f, posList);
		for (int k = 0, iPosListCount(posList.size()); k < iPosListCount; ++k)
		{
			const BrushVec3& v0 = posList[k];
			const BrushVec3& v1 = posList[(k + 1) % iPosListCount];
			dc.DrawLine(ToVec3(v0), ToVec3(v1));
		}
	}
}

void DisplayPolygons(DisplayContext& dc, const std::vector<PolygonPtr>& polygons, ExcludedEdgeManager* pExcludedEdgeMgr, const int nLineThickness, const ColorB& lineColor)
{
	int oldThickness = dc.GetLineWidth();
	dc.SetLineWidth(nLineThickness);
	dc.SetColor(lineColor);

	int iPolygonSize = polygons.size();

	for (int i = 0; i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon = polygons[i];

		if (!pPolygon->IsValid() || pPolygon->CheckFlags(ePolyFlag_Hidden | ePolyFlag_Mirrored))
			continue;

		for (int k = 0, iEdgeSize(pPolygon->GetEdgeCount()); k < iEdgeSize; ++k)
		{
			const IndexPair& rEdge = pPolygon->GetEdgeIndexPair(k);
			BrushEdge3D edge(pPolygon->GetPos(rEdge.m_i[0]), pPolygon->GetPos(rEdge.m_i[1]));
			std::vector<BrushEdge3D> visibleParts;
			if (pExcludedEdgeMgr && !pExcludedEdgeMgr->GetVisibleEdge(edge, pPolygon->GetPlane(), visibleParts))
				continue;
			if (!visibleParts.empty())
			{
				for (int i = 0, iVisibleSize(visibleParts.size()); i < iVisibleSize; ++i)
					dc.DrawLine(visibleParts[i].m_v[0], visibleParts[i].m_v[1]);
			}
			else
			{
				dc.DrawLine(edge.m_v[0], edge.m_v[1]);
			}
		}
	}
	dc.SetLineWidth(oldThickness);
}

void DisplayPolygon(DisplayContext& dc, PolygonPtr polygon)
{
	if (!polygon || !polygon->IsValid())
		return;

	for (int i = 0, iEdgeCount(polygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		BrushEdge3D e = polygon->GetEdge(i);
		dc.DrawLine(e.m_v[0], e.m_v[1]);
	}
}

void DisplayBottomPolygon(DisplayContext& dc, PolygonPtr polygon, const ColorB& lineColor)
{
	if (!polygon)
		return;

	int oldThickness = dc.GetLineWidth();
	dc.SetLineWidth(kLineThickness);
	dc.SetColor(lineColor);
	Display::DisplayPolygon(dc, polygon);
	dc.SetLineWidth(oldThickness);
}

void DisplayVertexNormals(DisplayContext& dc, MainContext& mc)
{
	_smart_ptr<IStatObj> pStatObj = NULL;
	if (!mc.pCompiler->GetIStatObj(&pStatObj))
		return;

	const float normal_length = 1.0f * 0.025f * dc.view->GetScreenScaleFactor(mc.pObject->GetWorldPos());
	dc.SetColor(ColorB(255, 255, 255, 255));
	dc.SetLineWidth(1);

	IIndexedMesh* pMesh = pStatObj->GetIndexedMesh();
	IIndexedMesh::SMeshDescription md;
	pMesh->GetMeshDescription(md);
	for (int i = 0; i < md.m_nVertCount; ++i)
	{
		Vec3 normal = md.m_pNorms[i].GetN();
		Vec3 pos = md.m_pVerts[i];
		dc.DrawLine(pos, pos + normal * normal_length);
	}
}

void DisplayPolygonNormals(DisplayContext& dc, MainContext& mc)
{
	const float normal_length = 1.0f * 0.025f * dc.view->GetScreenScaleFactor(mc.pObject->GetWorldPos());
	dc.SetColor(ColorB(255, 255, 255, 255));
	dc.SetLineWidth(1);

	for (int i = 0, iPolygonCount(mc.pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = mc.pModel->GetPolygon(i);
		Vec3 pos = ToVec3(polygon->GetRepresentativePosition());
		Vec3 normal = ToVec3(polygon->GetPlane().Normal());
		dc.DrawLine(pos, pos + normal * normal_length);
	}
}

void DisplayTriangulation(DisplayContext& dc, MainContext& mc)
{
	MODEL_SHELF_RECONSTRUCTOR(mc.pModel);

	dc.SetColor(Vec3(0.5f, 0.5f, 0.5f));
	dc.SetLineWidth(1.0f);

	for (int shelfID = eShelf_Base; shelfID < cShelfMax; ++shelfID)
	{
		mc.pModel->SetShelf(static_cast<ShelfID>(shelfID));

		if (!mc.pModel->HasClosedPolygon(static_cast<ShelfID>(shelfID)))
			continue;

		for (int i = 0, iPolygonCount(mc.pModel->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr polygon = mc.pModel->GetPolygon(i);
			FlexibleMesh* pTriangles = polygon->GetTriangles();

			for (int k = 0, iFaceCount(pTriangles->faceList.size()); k < iFaceCount; ++k)
			{
				Vec3 v[3] = {
					ToVec3(pTriangles->vertexList[pTriangles->faceList[k].v[0]].pos),
					ToVec3(pTriangles->vertexList[pTriangles->faceList[k].v[1]].pos),
					ToVec3(pTriangles->vertexList[pTriangles->faceList[k].v[2]].pos)
				};

				dc.DrawPolyLine(v, 3);
			}
		}
	}
}
}
}

