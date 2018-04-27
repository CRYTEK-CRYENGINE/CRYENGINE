// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Converter.h"
#include "Objects/DesignerObject.h"
#include "Core/ModelCompiler.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "Core/BSPTree2D.h"
#include "Objects/BrushObject.h"
#include "Geometry/EdGeometry.h"
#include "Objects/EntityObject.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include "Material/MaterialManager.h"
#include "Tools/ToolCommon.h"
#include "Core/UVIslandManager.h"

namespace Designer
{
bool Converter::CreateNewDesignerObject()
{
	std::vector<SelectedMesh> pSelectedMeshes;
	GetSelectedObjects(pSelectedMeshes);

	if (pSelectedMeshes.empty())
		return false;

	CUndo undo("Create Designer Objects");
	GetIEditor()->ClearSelection();

	std::vector<DesignerObject*> pDesignerObjects;
	ConvertToDesignerObjects(pSelectedMeshes, pDesignerObjects);

	int iSizeOfDesignerObjects(pDesignerObjects.size());

	if (iSizeOfDesignerObjects <= 0)
		return false;

	const float kMarginBetweenOldAndNew = 1.2f;

	for (int i = 0; i < iSizeOfDesignerObjects; ++i)
	{
		AABB aabb;
		pDesignerObjects[i]->GetBoundBox(aabb);
		float gapX = (aabb.max.x - aabb.min.x) * kMarginBetweenOldAndNew;
		Matrix34 newTM(pDesignerObjects[i]->GetWorldTM());
		newTM.SetTranslation(newTM.GetTranslation() + Vec3(gapX, 0, 0));
		pDesignerObjects[i]->SetWorldTM(newTM);
		GetIEditor()->SelectObject(pDesignerObjects[i]);
	}

	return true;
}

bool Converter::ConvertToDesignerObject()
{
	std::vector<SelectedMesh> pSelectedMeshes;
	GetSelectedObjects(pSelectedMeshes);

	if (pSelectedMeshes.empty())
		return false;

	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	if (pSelection == NULL)
		return false;

	CUndo undo("Convert to Designer Objects");

	GetIEditor()->GetObjectManager()->ClearSelection();

	std::vector<DesignerObject*> pDesignerObjects;
	ConvertToDesignerObjects(pSelectedMeshes, pDesignerObjects);

	int iSizeOfDesignerObjects(pDesignerObjects.size());
	if (iSizeOfDesignerObjects <= 0)
		return false;

	for (int i = 0; i < iSizeOfDesignerObjects; ++i)
		GetIEditor()->SelectObject(pDesignerObjects[i]);

	for (int i = 0, iSizeOfSelectedObjects(pSelectedMeshes.size()); i < iSizeOfSelectedObjects; ++i)
		GetIEditor()->DeleteObject(pSelectedMeshes[i].m_pOriginalObject);

	return true;
}

void Converter::ConvertToDesignerObjects(std::vector<SelectedMesh>& pSelectedMeshes, std::vector<DesignerObject*>& pOutDesignerObjects)
{
	for (int i = 0, iSizeOfSelectedObjs(pSelectedMeshes.size()); i < iSizeOfSelectedObjs; ++i)
	{
		SelectedMesh& obj = pSelectedMeshes[i];

		if (obj.m_pIndexedMesh == NULL)
			continue;

		DesignerObject* pDesignerObj = ConvertToDesignerObject(obj.m_pIndexedMesh);

		if (obj.m_bLoadedIndexedMeshFromFile)
			delete obj.m_pIndexedMesh;

		if (pDesignerObj == NULL)
			continue;

		if (obj.m_pMaterial)
			pDesignerObj->SetMaterial(obj.m_pMaterial);

		pDesignerObj->SetWorldTM(obj.m_worldTM);
		pOutDesignerObjects.push_back(pDesignerObj);
	}
}

void Converter::GetSelectedObjects(std::vector<SelectedMesh>& pObjects) const
{
	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	if (pSelection == NULL)
		return;

	for (int i = 0, iCount(pSelection->GetCount()); i < iCount; ++i)
	{
		SelectedMesh s;
		CBaseObject* pObj = pSelection->GetObject(i);

		if (pObj->GetMaterial())
			s.m_pMaterial = (CMaterial*)pObj->GetMaterial();

		if (pObj->IsKindOf(RUNTIME_CLASS(CBrushObject)))
		{
			CBrushObject* pBrushObj = (CBrushObject*)pObj;
			if (pBrushObj->GetGeometry())
			{
				s.m_pIndexedMesh = pBrushObj->GetGeometry()->GetIndexedMesh();
				IStatObj* pStatObj = pBrushObj->GetGeometry()->GetIStatObj();
				if (pStatObj)
					s.m_pMaterial = GetIEditor()->GetMaterialManager()->FromIMaterial(pStatObj->GetMaterial());
			}
		}
		else if (pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntityObject = (CEntityObject*)pObj;
			IEntity* pEntity = pEntityObject->GetIEntity();
			if (pEntity)
			{
				int nSlotCount = pEntity->GetSlotCount();
				int nCurSlot = 0;
				while (nCurSlot < nSlotCount)
				{
					SEntitySlotInfo slotInfo;
					if (pEntity->GetSlotInfo(nCurSlot++, slotInfo) == false)
						continue;
					if (!slotInfo.pStatObj)
						continue;
					s.m_pIndexedMesh = slotInfo.pStatObj->GetIndexedMesh();
					s.m_pMaterial = GetIEditor()->GetMaterialManager()->FromIMaterial(slotInfo.pStatObj->GetMaterial());
					if (s.m_pIndexedMesh == NULL)
					{
						string sFilename(slotInfo.pStatObj->GetFilePath());
						CContentCGF cgf(sFilename);
						if (!GetIEditor()->Get3DEngine()->LoadChunkFileContent(&cgf, sFilename))
							continue;
						for (int i = 0; i < cgf.GetNodeCount(); i++)
						{
							if (cgf.GetNode(i)->type != CNodeCGF::NODE_MESH)
								continue;
							CMesh* pMesh = cgf.GetNode(i)->pMesh;
							if (!pMesh)
								continue;
							s.m_pIndexedMesh = GetIEditor()->Get3DEngine()->CreateIndexedMesh();
							s.m_pIndexedMesh->SetMesh(*pMesh);
							s.m_bLoadedIndexedMeshFromFile = true;
							break;
						}
					}
				}
			}
		}
		if (s.m_pIndexedMesh == NULL)
			continue;
		s.m_worldTM = pObj->GetWorldTM();
		s.m_pOriginalObject = pObj;
		pObjects.push_back(s);
	}
}

DesignerObject* Converter::ConvertToDesignerObject(IIndexedMesh* pMesh)
{
	if (pMesh == NULL)
		return NULL;

	DesignerObject* pDesignerObj = (DesignerObject*)GetIEditor()->NewObject("Designer", "");

	if (!ConvertMeshToDesignerObject(pDesignerObj, pMesh))
	{
		GetIEditor()->DeleteObject(pDesignerObj);
		return NULL;
	}

	return pDesignerObj;
}

bool Converter::ConvertMeshToDesignerObject(DesignerObject* pDesignerObject, IIndexedMesh* pMesh)
{
	if (ConvertMeshToBrushDesigner(pMesh, pDesignerObject->GetModel()))
	{
		ApplyPostProcess(pDesignerObject->GetMainContext(), ePostProcess_Mesh | ePostProcess_DataBase);
		return true;
	}

	return false;
}

bool Converter::ConvertMeshToBrushDesigner(IIndexedMesh* pMesh, Model* pModel)
{
	if (pMesh == NULL || pModel == NULL)
		return false;

	int numVerts = pMesh->GetVertexCount();
	int numFaces = pMesh->GetIndexCount() / 3;

	IIndexedMesh::SMeshDescription md;
	pMesh->GetMeshDescription(md);

	Vec3* const positions = pMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::POSITIONS);
	Vec3f16* const positionsf16 = pMesh->GetMesh()->GetStreamPtr<Vec3f16>(CMesh::POSITIONSF16);
	SMeshTexCoord* const texCoords = pMesh->GetMesh()->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);
	vtx_idx* const indices = pMesh->GetMesh()->GetStreamPtr<vtx_idx>(CMesh::INDICES);

	if ((positions == NULL && positionsf16 == NULL) || indices == NULL)
		return false;

	int nSubsetCount = pMesh->GetMesh()->m_subsets.size();
	for (int i = 0; i < nSubsetCount; ++i)
	{
		const SMeshSubset& subset = pMesh->GetMesh()->m_subsets[i];
		for (int k = 0; k < subset.nNumIndices; k += 3)
		{
			int i0 = indices[k + 0 + subset.nFirstIndexId];
			int i1 = indices[k + 1 + subset.nFirstIndexId];
			int i2 = indices[k + 2 + subset.nFirstIndexId];

			if (i0 >= numVerts || i0 < 0)
				continue;
			if (i1 >= numVerts || i1 < 0)
				continue;
			if (i2 >= numVerts || i2 < 0)
				continue;

			std::vector<Vertex> vertices(3);

			if (positions)
			{
				vertices[0].pos = ToBrushVec3(positions[i0]);
				vertices[1].pos = ToBrushVec3(positions[i1]);
				vertices[2].pos = ToBrushVec3(positions[i2]);
			}
			else if (positionsf16)
			{
				vertices[0].pos = ToBrushVec3(positionsf16[i0].ToVec3());
				vertices[1].pos = ToBrushVec3(positionsf16[i1].ToVec3());
				vertices[2].pos = ToBrushVec3(positionsf16[i2].ToVec3());
			}

			// To avoid registering degenerated triangles
			BrushVec3 d0 = (vertices[2].pos - vertices[1].pos).GetNormalized();
			BrushVec3 d1 = (vertices[0].pos - vertices[1].pos).GetNormalized();
			if (std::abs(d0.Dot(d1)) >= 1 - kDesignerEpsilon)
				continue;

			BrushPlane plane(vertices[0].pos, vertices[1].pos, vertices[2].pos);

			if (texCoords)
			{
				vertices[0].uv = texCoords[i0].GetUV();
				vertices[1].uv = texCoords[i1].GetUV();
				vertices[2].uv = texCoords[i2].GetUV();
			}

			PolygonPtr pPolygon = new Polygon(vertices, plane, subset.nMatID, NULL, true);
			pModel->AddPolygon(pPolygon, false);
		}
	}

	GenerateUVIslands(pModel);

	return true;
}

bool Converter::ConvertSolidXMLToDesignerObject(XmlNodeRef pSolidNode, DesignerObject* pDesignerObject)
{
	if (pSolidNode->getChildCount() == 0)
	{
		DESIGNER_ASSERT(0);
		return false;
	}

	const char* tag = pSolidNode->getChild(0)->getTag();

	if (!stricmp(tag, "Face"))
	{
		int numFaces = pSolidNode->getChildCount();
		std::vector<SolidPolygon> polygonlist;
		std::vector<BrushVec3> vertexlist;
		for (int i = 0; i < numFaces; ++i)
		{
			XmlNodeRef faceNode = pSolidNode->getChild(i);
			if (faceNode == NULL)
				continue;
			if (faceNode->haveAttr("NumberOfPoints"))
			{
				SolidPolygon polygon;
				LoadPolygon(&polygon, faceNode);
				polygonlist.push_back(polygon);
				if (i == 0)
					LoadVertexList(vertexlist, faceNode);
			}
			else
			{
				DESIGNER_ASSERT(!"Can't convert an old type of solid");
				return true;
			}
		}
		AddPolygonsToDesigner(polygonlist, vertexlist, pDesignerObject);
	}
	else if (!stricmp(tag, "Polygon"))
	{
		std::vector<SolidPolygon> polylist;
		int numberOfChildren = pSolidNode->getChildCount();
		for (int i = 0; i < numberOfChildren; ++i)
		{
			XmlNodeRef polygonNode = pSolidNode->getChild(i);
			if (polygonNode && !strcmp(polygonNode->getTag(), "Polygon"))
			{
				SolidPolygon polygon;
				LoadPolygon(&polygon, polygonNode);
				polylist.push_back(polygon);
			}
		}
		std::vector<BrushVec3> vertexlist;
		LoadVertexList(vertexlist, pSolidNode);

		AddPolygonsToDesigner(polylist, vertexlist, pDesignerObject);
	}

	return true;
}

void Converter::AddPolygonsToDesigner(const std::vector<SolidPolygon>& polygonList, const std::vector<BrushVec3>& vList, DesignerObject* pDesignerObject)
{
	for (int i = 0, iPolySize(polygonList.size()); i < iPolySize; ++i)
	{
		const SolidPolygon& solidPolygon = polygonList[i];
		std::vector<BrushVec3> reorderedVertexList;
		for (int k = 0, iVIndexCount(solidPolygon.vIndexList.size()); k < iVIndexCount; ++k)
			reorderedVertexList.push_back(vList[solidPolygon.vIndexList[k]]);
		if (reorderedVertexList.size() < 3)
			continue;
		BrushPlane plane(reorderedVertexList[0], reorderedVertexList[1], reorderedVertexList[2]);
		PolygonPtr pPolygon = new Polygon(reorderedVertexList, plane, solidPolygon.matID, &solidPolygon.texinfo, true);

		if (!pPolygon->IsOpen())
		{
			if (pPolygon->GetBSPTree() && !pPolygon->GetBSPTree()->HasNegativeNode())
				pPolygon->SetPlane(pPolygon->GetPlane().GetInverted());
			pDesignerObject->GetModel()->AddPolygon(pPolygon);
		}
	}
}

void Converter::LoadTexInfo(TexInfo* texinfo, const XmlNodeRef& node)
{
	Vec3 texScale(1, 1, 1);
	Vec3 texShift(0, 0, 0);
	node->getAttr("TexScale", texScale);
	node->getAttr("TexShift", texShift);
	node->getAttr("TexRotate", texinfo->rotate);
	texinfo->scale[0] = texScale.x;
	texinfo->scale[1] = texScale.y;
	texinfo->shift[0] = texShift.x;
	texinfo->shift[1] = texShift.y;
}

void Converter::LoadPolygon(SolidPolygon* polygon, const XmlNodeRef& polygonNode)
{
	int numberOfPoints = 0;
	polygonNode->getAttr("NumberOfPoints", numberOfPoints);

	int nCount = 0;
	while (1)
	{
		string attribute;
		attribute.Format("v%d", nCount++);
		int16 vertexindex;
		bool ok = polygonNode->getAttr(attribute, vertexindex);
		if (ok == false)
			break;
		polygon->vIndexList.push_back(vertexindex);
	}

	int nMatId;
	polygonNode->getAttr("MatId", nMatId);
	polygon->matID = nMatId;

	LoadTexInfo(&polygon->texinfo, polygonNode);
}

void Converter::LoadVertexList(std::vector<BrushVec3>& vertexlist, const XmlNodeRef& node)
{
	XmlNodeRef vertexNode = node->findChild("Vertex");
	int numberOfVertices;
	vertexNode->getAttr("NumberOfVertices", numberOfVertices);
	int nCount = 0;
	while (1)
	{
		Vec3 position;
		string attribute;
		attribute.Format("p%d", nCount++);
		bool ok = vertexNode->getAttr(attribute, position);
		if (ok == false)
			break;
		vertexlist.push_back(ToBrushVec3(position));
	}
}

bool IsAdjacent(std::vector<PolygonPtr>& polygons, PolygonPtr polygon)
{
	auto ii = polygons.begin();
	for (; ii != polygons.end(); ++ii)
	{
		if ((*ii)->GetSubMatID() != polygon->GetSubMatID())
			continue;
		for (int i = 0, iEdgeCount0(polygon->GetEdgeCount()); i < iEdgeCount0; ++i)
		{
			const IndexPair& e0 = polygon->GetEdgeIndexPair(i);
			const Vec2& uv00 = polygon->GetUV(e0.m_i[0]);
			const Vec2& uv01 = polygon->GetUV(e0.m_i[1]);
			for (int k = 0, iEdgeCount1((*ii)->GetEdgeCount()); k < iEdgeCount1; ++k)
			{
				const IndexPair& e1 = (*ii)->GetEdgeIndexPair(k);
				const Vec2& uv10 = (*ii)->GetUV(e1.m_i[0]);
				const Vec2& uv11 = (*ii)->GetUV(e1.m_i[1]);
				if (Comparison::IsEquivalent(uv00, uv11) && Comparison::IsEquivalent(uv01, uv10))
					return true;
			}
		}
	}
	return false;
}

void Converter::GenerateUVIslands(Model* pModel)
{
	std::set<PolygonPtr> usedPolygons;
	std::vector<PolygonPtr> groupPolygons;

	while (usedPolygons.size() < pModel->GetPolygonCount())
	{
		bool bAdded = false;
		for (int i = 0, iCount(pModel->GetPolygonCount()); i < iCount; ++i)
		{
			PolygonPtr polygon = pModel->GetPolygon(i);
			if (usedPolygons.find(polygon) != usedPolygons.end())
				continue;

			if (groupPolygons.empty() || IsAdjacent(groupPolygons, polygon))
			{
				groupPolygons.push_back(polygon);
				usedPolygons.insert(polygon);
				bAdded = true;
			}
		}

		if (!bAdded && !groupPolygons.empty())
		{
			UVIslandPtr pUVIsland = new UVIsland();
			for (int i = 0, iCount(groupPolygons.size()); i < iCount; ++i)
				pUVIsland->AddPolygon(groupPolygons[i]);
			pModel->GetUVIslandMgr()->AddUVIsland(pUVIsland);
			groupPolygons.clear();
		}
	}
}
};

