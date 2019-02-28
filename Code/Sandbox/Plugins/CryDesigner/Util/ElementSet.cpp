// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ElementSet.h"
#include "Core/Model.h"
#include "Viewport.h"
#include "Core/Helper.h"
#include "DesignerEditor.h"
#include "Tools/ToolCommon.h"
#include "Util/MFCUtil.h"

namespace Designer
{
Element Element::GetMirroredElement(const BrushPlane& mirrorPlane) const
{
	DESIGNER_ASSERT(!m_Vertices.empty());

	Element mirroredElement(*this);

	for (int i = 0, iSize(m_Vertices.size()); i < iSize; ++i)
		mirroredElement.m_Vertices[i] = mirrorPlane.MirrorVertex(m_Vertices[i]);

	return mirroredElement;
}

bool Element::operator==(const Element& info)
{
	if (m_Vertices.size() != info.m_Vertices.size())
		return false;

	if (IsEdge())
	{
		DESIGNER_ASSERT(m_Vertices.size() == 2 && info.m_Vertices.size() == 2);
		BrushEdge3D edge(m_Vertices[0], m_Vertices[1]);
		BrushEdge3D infoEdge(info.m_Vertices[0], info.m_Vertices[1]);
		if (edge.IsEquivalent(infoEdge) || edge.IsEquivalent(infoEdge.GetInverted()))
			return true;
		else
			return false;
	}

	if (IsPolygon() || IsVertex())
	{
		if (m_Vertices.size() != info.m_Vertices.size() || m_pPolygon != info.m_pPolygon)
			return false;
		for (int i = 0, iSize(m_Vertices.size()); i < iSize; ++i)
		{
			bool bSameExist = false;
			for (int k = 0; k < iSize; ++k)
			{
				int nIndex = (i + k) % iSize;
				if (Comparison::IsEquivalent(m_Vertices[nIndex], info.m_Vertices[nIndex]))
				{
					bSameExist = true;
					break;
				}
			}
			if (!bSameExist)
				return false;
		}
		return true;
	}

	return false;
}

void Element::Invalidate()
{
	m_Vertices.clear();
	m_pPolygon = NULL;
	m_pObject = NULL;
}

bool Element::IsEquivalent(const Element& elementInfo) const
{
	if (m_Vertices.size() != elementInfo.m_Vertices.size())
		return false;

	if (IsPolygon())
	{
		if (m_pPolygon != NULL && m_pPolygon == elementInfo.m_pPolygon)
			return true;
		return false;
	}

	for (int i = 0, iVertexSize(m_Vertices.size()); i < iVertexSize; ++i)
	{
		bool bFoundEquivalent = false;
		for (int k = 0; k < iVertexSize; ++k)
		{
			if (Comparison::IsEquivalent(m_Vertices[i], elementInfo.m_Vertices[(k + i) % iVertexSize]))
			{
				bFoundEquivalent = true;
				break;
			}
		}
		if (!bFoundEquivalent)
			return false;
	}

	return true;
}

bool ElementSet::Add(ElementSet& elements)
{
	if (elements.IsEmpty())
		return false;
	for (int i = 0, iElementSize(elements.GetCount()); i < iElementSize; ++i)
	{
		bool bEquivalentExist = false;
		for (int k = 0, iSelectedElementSize(m_Elements.size()); k < iSelectedElementSize; ++k)
		{
			if (m_Elements[k].IsEquivalent(elements[i]))
			{
				bEquivalentExist = true;
				break;
			}
		}
		if (!bEquivalentExist)
			m_Elements.push_back(elements[i]);
	}
	return true;
}

bool ElementSet::Add(const Element& element)
{
	for (int k = 0, iSelectedElementSize(m_Elements.size()); k < iSelectedElementSize; ++k)
	{
		if (m_Elements[k].IsEquivalent(element))
			return false;
	}
	m_Elements.push_back(element);
	return true;
}

bool ElementSet::PickFromModel(CBaseObject* pObject, Model* pModel, CViewport* viewport, CPoint point, int nFlag, bool bOnlyIncludeCube, BrushVec3* pOutPickedPos)
{
	BrushVec3 outNearestPos;
	if (nFlag & ePF_Vertex)
	{
		if (QueryNearestVertex(pObject, pModel, viewport, point, GetDesigner()->GetRay(), false, outNearestPos))
		{
			Clear();
			Element element;
			element.m_Vertices.push_back(outNearestPos);
			element.m_pObject = pObject;
			element.m_pPolygon = NULL;
			if (pOutPickedPos)
				*pOutPickedPos = outNearestPos;
			Add(element);
			return true;
		}
	}

	std::vector<QueryEdgeResult> queryResults;
	if (nFlag & ePF_Edge)
	{
		std::vector<std::pair<BrushEdge3D, BrushVec3>> edges;
		CRect selectionRect(CPoint(point.x - 5, point.y - 5), CPoint(point.x + 5, point.y + 5));
		PolygonPtr pRectPolygon = MakePolygonFromRectangle(selectionRect);
		pModel->QueryIntersectionEdgesWith2DRect(viewport, pObject->GetWorldTM(), pRectPolygon, false, edges);
		if (!edges.empty())
		{
			Clear();
			std::vector<BrushEdge3D> nearestEdges = FindNearestEdges(viewport, pObject->GetWorldTM(), edges);
			int nShortEdgeIndex = FindShortestEdge(nearestEdges);
			BrushEdge3D shortestEdge = nearestEdges[nShortEdgeIndex];
			Element element;
			element.m_Vertices.push_back(shortestEdge.m_v[0]);
			element.m_Vertices.push_back(shortestEdge.m_v[1]);
			element.m_pObject = pObject;
			element.m_pPolygon = NULL;
			if (pOutPickedPos)
				*pOutPickedPos = shortestEdge.GetCenter();
			Add(element);
			return true;
		}
	}

	if (nFlag & ePF_Polygon)
	{
		int nPickedPolygon(-1);
		BrushVec3 hitPos;
		if (!bOnlyIncludeCube && pModel->QueryPolygon(GetDesigner()->GetRay(), nPickedPolygon))
		{
			pModel->GetPolygon(nPickedPolygon)->GetPlane().HitTest(GetDesigner()->GetRay(), NULL, &hitPos);
			if (pOutPickedPos)
				*pOutPickedPos = hitPos;
		}

		BrushVec3 vPickedPosFromBox;
		PolygonPtr pPickedPolygonFromBox = PickPolygonFromRepresentativeBox(pObject, pModel, viewport, point, vPickedPosFromBox);

		if (nPickedPolygon != -1)
		{
			if (pPickedPolygonFromBox)
			{
				if (pPickedPolygonFromBox != pModel->GetPolygon(nPickedPolygon))
				{
					BrushFloat distToBox = vPickedPosFromBox.GetDistance(GetDesigner()->GetRay().origin);
					BrushFloat distToFace = hitPos.GetDistance(GetDesigner()->GetRay().origin);
					if (distToFace < distToBox)
					{
						pPickedPolygonFromBox = pModel->GetPolygon(nPickedPolygon);
					}
					else
					{
						if (pOutPickedPos)
							*pOutPickedPos = vPickedPosFromBox;
					}
				}
			}
			else
			{
				pPickedPolygonFromBox = pModel->GetPolygon(nPickedPolygon);
			}
		}

		if (pPickedPolygonFromBox)
		{
			Clear();
			Element element;
			element.SetPolygon(pObject, pPickedPolygonFromBox);
			Add(element);
			return true;
		}
	}

	return false;
}

void ElementSet::Display(CBaseObject* pObject, DisplayContext& dc) const
{
	int nOldLineWidth = dc.GetLineWidth();
	dc.SetColor(kSelectedColor);
	dc.SetLineWidth(kChosenLineThickness);

	dc.PopMatrix();

	if (Deprecated::CheckVirtualKey(VK_SPACE))
		dc.DepthTestOff();

	for (int i = 0, iElementCount(m_Elements.size()); i < iElementCount; ++i)
	{
		if (m_Elements[i].IsVertex())
		{
			BrushVec3 worldVertexPos = pObject->GetWorldTM().TransformPoint(m_Elements[i].m_Vertices[0]);
			BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, worldVertexPos);
			dc.DrawSolidBox(ToVec3(worldVertexPos - vBoxSize), ToVec3(worldVertexPos + vBoxSize));
		}
	}
	dc.PushMatrix(pObject->GetWorldTM());

	for (int i = 0, iElementCount(m_Elements.size()); i < iElementCount; ++i)
	{
		if (m_Elements[i].IsEdge())
		{
			dc.DrawLine(m_Elements[i].m_Vertices[0], m_Elements[i].m_Vertices[1]);
		}
		else if (m_Elements[i].IsPolygon() && m_Elements[i].m_pPolygon->IsOpen())
		{
			for (int k = 0, iEdgeCount(m_Elements[i].m_pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
			{
				BrushEdge3D e = m_Elements[i].m_pPolygon->GetEdge(k);
				dc.DrawLine(e.m_v[0], e.m_v[1]);
			}
		}
	}

	dc.SetLineWidth(nOldLineWidth);
	dc.DepthTestOn();
}

void ElementSet::FindElementsInRect(const CRect& rect, CViewport* pView, const Matrix34& worldTM, bool bOnlyUseSelectionCube, std::vector<Element>& elements) const
{
	for (int i = 0, iCount(m_Elements.size()); i < iCount; ++i)
	{
		const Element& element = m_Elements[i];

		if (element.IsVertex())
		{
			CPoint pt = pView->WorldToView(worldTM.TransformPoint(element.GetPos()));
			if (rect.PtInRect(pt))
				elements.push_back(element);
		}

		if (element.IsEdge())
		{
			if (DoesEdgeIntersectRect(rect, pView, worldTM, element.GetEdge()))
				elements.push_back(element);
		}

		if (element.IsPolygon() && element.m_pPolygon)
		{
			bool bAdded = false;
			PolygonPtr pRectPolygon = MakePolygonFromRectangle(rect);
			if (!bOnlyUseSelectionCube && element.m_pPolygon->IsIntersectedBy2DRect(pView, worldTM, pRectPolygon, false))
			{
				bAdded = true;
				elements.push_back(element);
			}
			if (!bAdded && gDesignerSettings.bHighlightElements)
			{
				if (DoesSelectionBoxIntersectRect(pView, worldTM, rect, element.m_pPolygon))
					elements.push_back(element);
			}
		}
	}
}

BrushVec3 ElementSet::GetNormal(Model* pModel) const
{
	if (m_Elements.empty())
		return BrushVec3(0, 0, 1);

	BrushVec3 normal(0, 0, 0);
	for (int i = 0, iSize(m_Elements.size()); i < iSize; ++i)
	{
		if (m_Elements[i].IsPolygon())
			normal += m_Elements[i].m_pPolygon->GetPlane().Normal();
	}

	if (!normal.IsZero())
		return normal.GetNormalized();

	AABB aabb;
	aabb.Reset();
	ModelDB::QueryResult queryResult = QueryFromElements(pModel);
	for (int i = 0, iQuerySize(queryResult.size()); i < iQuerySize; ++i)
	{
		for (int k = 0, iMarkListSize(queryResult[i].m_MarkList.size()); k < iMarkListSize; ++k)
		{
			PolygonPtr pPolygon = queryResult[i].m_MarkList[k].m_pPolygon;
			if (pPolygon == NULL)
				continue;
			normal += pPolygon->GetPlane().Normal();
			aabb.Add(queryResult[i].m_Pos);
		}
	}

	if (normal.IsZero(kDesignerEpsilon))
	{
		float dx = aabb.max.x - aabb.min.x;
		float dy = aabb.max.y - aabb.min.y;
		float dz = aabb.max.z - aabb.min.z;
		if (dx < dy && dx < dz)
			normal = BrushVec3(1, 0, 0);
		else if (dy < dx && dy < dz)
			normal = BrushVec3(0, 1, 0);
		else
			normal = BrushVec3(0, 0, 1);
	}

	return normal.GetNormalized();
}

Matrix33 ElementSet::GetOrts(Model* pModel) const
{
	BrushVec3 normal = GetNormal(pModel);
	if (m_Elements.size() == 1)
	{
		if (m_Elements.front().IsEdge())
		{
			const BrushVec3 dir = m_Elements.front().GetEdge().GetDirection();

			// For some reason, CreateOrientation uses the left handed coordinate system to compute x, which changes the direction of the normal to the opposite.
			return Matrix33::CreateOrientation(dir, -normal, 0);
		}
		else if (m_Elements.front().IsVertex())
		{
			// Try to use the adjacent polygon as a helper to build the orts.
			ModelDB::QueryResult queryResult;
			if (pModel->GetDB()->QueryAsVertex(m_Elements.front().GetPos(), queryResult) && queryResult.front().m_MarkList.size())
			{
				const auto i = std::find_if(queryResult.front().m_MarkList.begin(), queryResult.front().m_MarkList.end(), [](const auto& x)
				{
					return x.m_pPolygon != nullptr;
				});

				if (i != queryResult.front().m_MarkList.end())
				{
					// Use the poligon plane normal as an 'up', since it might be adjusted to create orthogonal base. 
					const BrushVec3 up = i->m_pPolygon->GetPlane().Normal();
					const Matrix33 t = Matrix33::CreateOrientation(normal, -up, 0);

					// Swizzle orts to have 'z' as the normal ort and 'y' as a dir ort lying in the polygon plane.
					return Matrix33::CreateFromVectors(t.GetColumn2(), t.GetColumn0(), t.GetColumn1());
				}
			}
		}
	}

	return 	Matrix33::CreateOrthogonalBase(normal);
}

void ElementSet::Erase(int nElementFlags)
{
	auto ii = m_Elements.begin();
	for (; ii != m_Elements.end(); )
	{
		if ((nElementFlags & ePF_Vertex) && (*ii).IsVertex() || (nElementFlags & ePF_Edge) && (*ii).IsEdge() || (nElementFlags & ePF_Polygon) && (*ii).IsPolygon())
			ii = m_Elements.erase(ii);
		else
			++ii;
	}
}

bool ElementSet::Erase(ElementSet& elements)
{
	auto ii = m_Elements.begin();
	bool bErasedAtLeastOne = false;
	for (; ii != m_Elements.end(); )
	{
		bool bErased = false;
		for (int i = 0, iElementCount(elements.GetCount()); i < iElementCount; ++i)
		{
			if ((*ii) == elements[i])
			{
				ii = m_Elements.erase(ii);
				bErasedAtLeastOne = bErased = true;
				break;
			}
		}
		if (!bErased)
			++ii;
	}
	return bErasedAtLeastOne;
}

void ElementSet::Erase(const Element& element)
{
	auto ii = m_Elements.begin();
	for (; ii != m_Elements.end(); )
	{
		if ((*ii) == element)
			ii = m_Elements.erase(ii);
		else
			++ii;
	}
}

void ElementSet::EraseUnusedPolygonElements(Model* pModel)
{
	for (auto iElement = m_Elements.begin(); iElement != m_Elements.end(); )
	{
		if (!(*iElement).IsPolygon() || (*iElement).m_pPolygon == NULL)
			continue;

		if (!pModel->QueryEquivalentPolygon((*iElement).m_pPolygon))
			iElement = m_Elements.erase(iElement);
		else
			++iElement;
	}
}

bool ElementSet::Has(const Element& elementInfo) const
{
	if (m_Elements.empty())
		return false;

	for (int i = 0, iElementSize(m_Elements.size()); i < iElementSize; ++i)
	{
		if (m_Elements[i].IsEquivalent(elementInfo))
			return true;
	}

	return false;
}

bool ElementSet::HasVertex(const BrushVec3& vertex) const
{
	for (int i = 0, iElementCount(m_Elements.size()); i < iElementCount; ++i)
	{
		if (!m_Elements[i].IsVertex())
			continue;

		if (Comparison::IsEquivalent(m_Elements[i].GetPos(), vertex))
			return true;
	}

	return false;
}

string ElementSet::GetElementsInfoText()
{
	int nVertexNum = 0;
	int nEdgeNum = 0;
	int nPolygonNum = 0;
	for (int i = 0, iCount(m_Elements.size()); i < iCount; ++i)
	{
		if (m_Elements[i].IsVertex())
			++nVertexNum;
		else if (m_Elements[i].IsEdge())
			++nEdgeNum;
		else if (m_Elements[i].IsPolygon())
			++nPolygonNum;
	}

	string str;
	if (nVertexNum > 0)
	{
		string substr;
		substr.Format("%d Vertex(s)", nVertexNum);
		str += substr;
	}
	if (nEdgeNum > 0)
	{
		if (!str.IsEmpty())
			str += ",";
		string substr;
		substr.Format("%d Edge(s)", nEdgeNum);
		str += substr;
	}
	if (nPolygonNum > 0)
	{
		if (!str.IsEmpty())
			str += ",";
		string substr;
		substr.Format("%d Polygon(s)", nPolygonNum);
		str += substr;
	}

	return str;
}

void ElementSet::RemoveInvalidElements()
{
	auto ii = m_Elements.begin();
	for (; ii != m_Elements.end(); )
	{
		if ((*ii).IsPolygon() && ((*ii).m_pPolygon == NULL || !(*ii).m_pPolygon->IsValid()))
			ii = m_Elements.erase(ii);
		else
			++ii;
	}
}

ModelDB::QueryResult ElementSet::QueryFromElements(Model* pModel) const
{
	MODEL_SHELF_RECONSTRUCTOR(pModel);
	ModelDB::QueryResult queryResult;

	for (int i = 0, iSelectedSize(m_Elements.size()); i < iSelectedSize; ++i)
	{
		int iVertexSize(m_Elements[i].m_Vertices.size());
		for (int k = 0; k < iVertexSize; ++k)
			pModel->GetDB()->QueryAsVertex(m_Elements[i].m_Vertices[k], queryResult);
	}

	if (!pModel->CheckFlag(eModelFlag_Mirror))
		return queryResult;

	auto iQuery = queryResult.begin();
	for (; iQuery != queryResult.end(); )
	{
		auto iMark = (*iQuery).m_MarkList.begin();
		for (; iMark != (*iQuery).m_MarkList.end(); )
		{
			PolygonPtr pPolygon = iMark->m_pPolygon;
			if (pPolygon == NULL || pPolygon->CheckFlags(ePolyFlag_Mirrored))
				iMark = (*iQuery).m_MarkList.erase(iMark);
			else
				++iMark;
		}

		if ((*iQuery).m_MarkList.empty())
			iQuery = queryResult.erase(iQuery);
		else
			++iQuery;
	}

	return queryResult;
}

PolygonPtr ElementSet::QueryNearestVertex(
  CBaseObject* pObject,
  Model* pModel,
  CViewport* pView,
  CPoint point,
  const BrushRay& ray,
  bool bOnlyPickFirstVertexInOpenPolygon,
  BrushVec3& outPos,
  BrushVec3* pOutNormal) const
{
	Vec3 worldRaySrc, worldRayDir;
	pView->ViewToWorldRay(point, worldRaySrc, worldRayDir);

	BrushFloat fLeastDist = (BrushFloat)3e10;
	PolygonPtr polygon = NULL;

	for (int a = 0, iPolygonCount(pModel->GetPolygonCount()); a < iPolygonCount; ++a)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(a);
		if (pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;
		for (int i = 0, iVertexCount(pPolygon->GetVertexCount()); i < iVertexCount; ++i)
		{
			const BrushVec3& v = pPolygon->GetPos(i);
			BrushVec3 prevVertex;
			if (bOnlyPickFirstVertexInOpenPolygon && pPolygon->IsOpen() && pPolygon->GetPrevVertex(i, prevVertex))
				continue;

			BrushFloat t = 0;
			BrushVec3 vWorldPos = pObject->GetWorldTM().TransformPoint(v);
			BrushVec3 vBoxSize = GetElementBoxSize(pView, pView->GetType() != ET_ViewportCamera, vWorldPos);
			if (!GetIntersectionOfRayAndAABB(ToBrushVec3(worldRaySrc), ToBrushVec3(worldRayDir), AABB(ToVec3(vWorldPos - vBoxSize), ToVec3(vWorldPos + vBoxSize)), &t))
				continue;
			if (t > 0 && t < fLeastDist)
			{
				fLeastDist = t;
				outPos = v;
				if (pOutNormal)
				{
					if (pPolygon->IsOpen())
					{
						int nPolygonIndex = -1;
						if (pModel->QueryPolygon(ray, nPolygonIndex))
						{
							PolygonPtr pClosedPolygon = pModel->GetPolygon(nPolygonIndex);
							*pOutNormal = pClosedPolygon->GetPlane().Normal();
						}
						else
						{
							*pOutNormal = BrushVec3(0, 0, 1);
						}
					}
					else
					{
						*pOutNormal = pPolygon->GetPlane().Normal();
					}
				}
				polygon = pPolygon;
			}
		}
	}

	return polygon;
}

bool ElementSet::HasPolygonSelected(PolygonPtr pPolygon) const
{
	for (int i = 0, iCount(m_Elements.size()); i < iCount; ++i)
	{
		if (m_Elements[i].IsPolygon() && m_Elements[i].m_pPolygon == pPolygon)
			return true;
	}
	return false;
}

PolygonPtr ElementSet::PickPolygonFromRepresentativeBox(CBaseObject* pObject, Model* pModel, CViewport* pView, CPoint point, BrushVec3& outPickedPos) const
{
	if (!gDesignerSettings.bHighlightElements)
		return NULL;

	Vec3 worldRaySrc, worlsRayDir;
	pView->ViewToWorldRay(point, worldRaySrc, worlsRayDir);

	MODEL_SHELF_RECONSTRUCTOR(pModel);

	BrushFloat fLeastDist = (BrushFloat)3e10;
	PolygonPtr pPickedPolygon = NULL;

	BrushMatrix34 matInvWorld = pObject->GetWorldTM().GetInverted();

	for (int i = eShelf_Base; i < cShelfMax; ++i)
	{
		pModel->SetShelf(static_cast<ShelfID>(i));
		for (int k = 0, iPolygonCount(pModel->GetPolygonCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr pPolygon = pModel->GetPolygon(k);
			if (!pPolygon->IsValid() || pPolygon->CheckFlags(ePolyFlag_Hidden))
				continue;
			BrushVec3 v = pPolygon->GetRepresentativePosition();
			BrushFloat t = 0;
			BrushVec3 vWorldPos = pObject->GetWorldTM().TransformPoint(v);
			BrushVec3 vBoxSize = GetElementBoxSize(pView, pView->GetType() != ET_ViewportCamera, vWorldPos);
			if (GetIntersectionOfRayAndAABB(ToBrushVec3(worldRaySrc), ToBrushVec3(worlsRayDir), AABB(ToVec3(vWorldPos - vBoxSize), ToVec3(vWorldPos + vBoxSize)), &t))
			{
				if (t > 0 && t < fLeastDist)
				{
					fLeastDist = t;
					outPickedPos = matInvWorld.TransformPoint(worldRaySrc + worlsRayDir * t);
					pPickedPolygon = pPolygon;
				}
			}
		}
	}

	return pPickedPolygon;
}

int ElementSet::GetPolygonCount() const
{
	int nFaceCount = 0;
	for (int i = 0, iElementCount(m_Elements.size()); i < iElementCount; ++i)
	{
		if (m_Elements[i].IsPolygon())
			++nFaceCount;
	}
	return nFaceCount;
}

int ElementSet::GetEdgeCount() const
{
	int nEdgeCount = 0;
	for (int i = 0, iElementCount(m_Elements.size()); i < iElementCount; ++i)
	{
		if (m_Elements[i].IsEdge())
			++nEdgeCount;
	}
	return nEdgeCount;
}

int ElementSet::GetVertexCount() const
{
	int nVertexCount = 0;
	for (int i = 0, iElementCount(m_Elements.size()); i < iElementCount; ++i)
	{
		if (m_Elements[i].IsVertex())
			++nVertexCount;
	}
	return nVertexCount;
}

void ElementSet::ApplyOffset(const BrushVec3& vOffset)
{
	for (int i = 0, iElementCount(m_Elements.size()); i < iElementCount; ++i)
	{
		for (int k = 0, iVertexCount(m_Elements[i].m_Vertices.size()); k < iVertexCount; ++k)
			m_Elements[i].m_Vertices[k] += vOffset;
	}
}

std::vector<PolygonPtr> ElementSet::GetAllPolygons() const
{
	std::vector<PolygonPtr> polygons;
	for (int i = 0, iCount(m_Elements.size()); i < iCount; ++i)
	{
		if (m_Elements[i].IsPolygon() && m_Elements[i].m_pPolygon)
			polygons.push_back(m_Elements[i].m_pPolygon);
	}
	return polygons;
}

void ElementSet::Transform(const BrushMatrix34& tm)
{
	for (int i = 0, iCount(GetCount()); i < iCount; ++i)
	{
		Element& element = m_Elements[i];
		for (int k = 0, iVertexCount(element.m_Vertices.size()); k < iVertexCount; ++k)
			element.m_Vertices[k] = tm.TransformPoint(element.m_Vertices[k]);
	}
}

bool ElementSet::IsIsolated() const
{
	auto ii = m_Elements.begin();
	for (; ii != m_Elements.end(); ++ii)
	{
		if (!ii->m_bIsolated)
			return false;
	}
	return true;
}

_smart_ptr<Model> ElementSet::CreateModel()
{
	_smart_ptr<Model> pModel = new Model;
	auto ii = m_Elements.begin();
	for (; ii != m_Elements.end(); ++ii)
	{
		if (ii->IsPolygon() && (*ii).m_pPolygon)
			pModel->AddPolygon((*ii).m_pPolygon, false);
	}
	if (pModel->GetPolygonCount() == 0)
		return NULL;
	return pModel;
}
}

