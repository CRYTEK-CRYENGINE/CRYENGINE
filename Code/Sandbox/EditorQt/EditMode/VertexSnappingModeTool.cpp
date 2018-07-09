// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VertexSnappingModeTool.h"
#include "Viewport.h"
#include "SurfaceInfoPicker.h"
#include "Material/Material.h"
#include "Util/KDTree.h"
#include "Objects/Group.h"
#include "Objects/DisplayContext.h"

#include <EditorFramework/Editor.h>

IMPLEMENT_DYNCREATE(CVertexSnappingModeTool, CEditTool)

SVertexSnappingPreferences gVertexSnappingPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SVertexSnappingPreferences, &gVertexSnappingPreferences)

struct VertexSnappingCVars
{
	static void Register()
	{
		static bool initialized = false;
		if (!initialized)
		{
			REGISTER_CVAR(ed_vert_snapping_show_spatial_partition, 0, 0, "Show spatial partitioning for vertex snapping");
			initialized = true;
		}
	}

	static int ed_vert_snapping_show_spatial_partition;
};

int VertexSnappingCVars::ed_vert_snapping_show_spatial_partition = 0;

class CVertexSnappingModeToolClassDesc : public IClassDesc
{
	ESystemClassID SystemClassID()   { return ESYSTEM_CLASS_EDITTOOL; }
	const char*    ClassName()       { return "EditTool.VertexSnappingMode"; };
	const char*    Category()        { return "Select"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVertexSnappingModeTool); }
};

REGISTER_CLASS_DESC(CVertexSnappingModeToolClassDesc);

bool FindNearestVertex(CViewport* pViewport, CBaseObject* pObject, CKDTree* pTree, const Vec3& vWorldRaySrc, const Vec3& vWorldRayDir, Vec3& outPos, Vec3& vOutHitPosOnCube)
{
	Matrix34 worldInvTM = pObject->GetWorldTM().GetInverted();
	Vec3 vRaySrc = worldInvTM.TransformPoint(vWorldRaySrc);
	Vec3 vRayDir = worldInvTM.TransformVector(vWorldRayDir);
	Vec3 vLocalCameraPos = worldInvTM.TransformPoint(pViewport->GetViewTM().GetTranslation());
	Vec3 vPos;
	Vec3 vHitPosOnCube;

	if (pTree->FindNearestVertex(vRaySrc, vRayDir, gVertexSnappingPreferences.vertexCubeSize(), vLocalCameraPos, vPos, vHitPosOnCube))
	{
		outPos = pObject->GetWorldTM().TransformPoint(vPos);
		vOutHitPosOnCube = pObject->GetWorldTM().TransformPoint(vHitPosOnCube);
		return true;
	}

	return false;
}

CVertexSnappingModeTool::CVertexSnappingModeTool()
{
	VertexSnappingCVars::Register();

	m_modeStatus = eVSS_SelectFirstVertex;
	m_bHit = false;
}

CVertexSnappingModeTool::~CVertexSnappingModeTool()
{
	std::map<CBaseObjectPtr, CKDTree*>::iterator ii = m_ObjectKdTreeMap.begin();
	for (; ii != m_ObjectKdTreeMap.end(); ++ii)
		delete ii->second;
}

bool CVertexSnappingModeTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	CBaseObjectPtr pExcludedObject = NULL;
	if (m_modeStatus == eVSS_MoveSelectVertexToAnotherVertex)
		pExcludedObject = m_SelectionInfo.m_pObject;

	m_bHit = HitTest(view, point, pExcludedObject, &m_vHitVertex, m_pHitObject, m_Objects);

	if (event == eMouseLDown && m_bHit && m_pHitObject && m_modeStatus == eVSS_SelectFirstVertex)
	{
		m_modeStatus = eVSS_MoveSelectVertexToAnotherVertex;
		m_SelectionInfo.m_pObject = m_pHitObject;
		m_SelectionInfo.m_vPos = m_vHitVertex;

		GetIEditorImpl()->GetIUndoManager()->Begin();
		m_pHitObject->StoreUndo("Vertex Snapping", true);
	}

	if (m_modeStatus == eVSS_MoveSelectVertexToAnotherVertex)
	{
		if (event == eMouseLUp)
		{
			m_modeStatus = eVSS_SelectFirstVertex;

			GetIEditorImpl()->GetIUndoManager()->Accept("Vertex Snapping");
		}
		else if ((flags & MK_LBUTTON) && event == eMouseMove)
		{
			Vec3 vOffset = m_SelectionInfo.m_pObject->GetWorldPos() - m_SelectionInfo.m_vPos;
			CGroup* pGroup = (CGroup*)m_SelectionInfo.m_pObject->GetGroup();
			if (pGroup)
			{
				Vec3 vOffsetFromChild2Group = pGroup->GetWorldPos() - m_SelectionInfo.m_pObject->GetWorldPos();
				Vec3 vChildWorldPosAfter = m_vHitVertex + vOffset;
				pGroup->SetWorldPos(vChildWorldPosAfter + vOffsetFromChild2Group);
			}
			else
			{
				m_SelectionInfo.m_pObject->SetWorldPos(m_vHitVertex + vOffset);
			}
			m_SelectionInfo.m_vPos = m_SelectionInfo.m_pObject->GetWorldPos() - vOffset;
		}
	}

	return true;
}

bool CVertexSnappingModeTool::HitTest(CViewport* view, const CPoint& point, CBaseObject* pExcludedObj, Vec3* outHitPos, CBaseObjectPtr& pHitObject, std::vector<CBaseObjectPtr>& outObjects)
{
	if (VertexSnappingCVars::ed_vert_snapping_show_spatial_partition)
		m_DebugBoxes.clear();

	CSurfaceInfoPicker picker;
	CSurfaceInfoPicker::CExcludedObjects excludedObjects;
	if (pExcludedObj)
		excludedObjects.Add(pExcludedObj);

	int nPickFlag = CSurfaceInfoPicker::ePOG_BrushObject |
	                CSurfaceInfoPicker::ePOG_Prefabs |
	                CSurfaceInfoPicker::ePOG_Solid |
	                CSurfaceInfoPicker::ePOG_DesignerObject |
	                CSurfaceInfoPicker::ePOG_Entity;

	pHitObject = NULL;
	outObjects.clear();

	std::vector<CBaseObjectPtr> penetratedObjects;
	if (!picker.PickByAABB(point, nPickFlag, view, &excludedObjects, &penetratedObjects))
		return false;

	for (int i = 0, iCount(penetratedObjects.size()); i < iCount; ++i)
	{
		if (!penetratedObjects[i]->GetIStatObj())
			continue;
		CMaterial* pMaterial = (CMaterial*)penetratedObjects[i]->GetMaterial();
		if (pMaterial)
		{
			string matName = pMaterial->GetName();
			if (!stricmp(matName.GetBuffer(), "Objects/sky/forest_sky_dome"))
				continue;
		}
		outObjects.push_back(penetratedObjects[i]);
	}

	Vec3 vWorldRaySrc, vWorldRayDir;
	view->ViewToWorldRay(point, vWorldRaySrc, vWorldRayDir);

	std::vector<CBaseObjectPtr>::iterator ii = outObjects.begin();
	float fNearestDist = 3e10f;
	Vec3 vNearestPos;
	CBaseObjectPtr pNearestObject = NULL;
	for (ii = outObjects.begin(); ii != outObjects.end(); ++ii)
	{
		if (VertexSnappingCVars::ed_vert_snapping_show_spatial_partition)
		{
			Matrix34 invWorldTM = (*ii)->GetWorldTM().GetInverted();
			int nIndex = m_DebugBoxes.size();

			Vec3 vLocalRaySrc = invWorldTM.TransformPoint(vWorldRaySrc);
			Vec3 vLocalRayDir = invWorldTM.TransformVector(vWorldRayDir);
			GetKDTree(*ii)->GetPenetratedBoxes(vLocalRaySrc, vLocalRayDir, m_DebugBoxes);
			for (int i = nIndex; i < m_DebugBoxes.size(); ++i)
				m_DebugBoxes[i].SetTransformedAABB((*ii)->GetWorldTM(), m_DebugBoxes[i]);
		}

		Vec3 vPos, vHitPosOnCube;
		if (FindNearestVertex(view, *ii, GetKDTree(*ii), vWorldRaySrc, vWorldRayDir, vPos, vHitPosOnCube))
		{
			float fDistance = vHitPosOnCube.GetDistance(vWorldRaySrc);
			if (fDistance < fNearestDist)
			{
				fNearestDist = fDistance;
				vNearestPos = vPos;
				pNearestObject = *ii;
			}
		}
	}

	if (fNearestDist < 3e10f)
	{
		if (outHitPos)
			*outHitPos = vNearestPos;
		pHitObject = pNearestObject;
	}

	bool bBlockedFromObject = false;
	for (ii = outObjects.begin(); ii != outObjects.end(); ++ii)
	{
		CSurfaceInfoPicker picker;
		SRayHitInfo hitInfo;
		if (picker.PickObject(point, hitInfo, *ii))
		{
			AABB localBoundbox;
			(*ii)->GetLocalBounds(localBoundbox);
			float fOffset = localBoundbox.max.GetDistance(localBoundbox.min) * 0.1f;

			if ((*ii) != pHitObject && hitInfo.fDistance < fNearestDist || (*ii) == pHitObject && hitInfo.fDistance + fOffset < fNearestDist)
			{
				fNearestDist = hitInfo.fDistance;
				pHitObject = *ii;
				bBlockedFromObject = true;
			}
		}
	}

	if (pHitObject)
	{
		Vec3 vPivotPos = pHitObject->GetWorldPos();
		Vec3 vPivotBox = GetCubeSize(view, pHitObject->GetWorldPos());
		AABB pivotAABB(vPivotPos - vPivotBox, vPivotPos + vPivotBox);
		Vec3 vPosOnPivotCube;
		if (Intersect::Ray_AABB(vWorldRaySrc, vWorldRayDir, pivotAABB, vPosOnPivotCube))
		{
			if (outHitPos)
				*outHitPos = vPivotPos;
			return true;
		}
	}

	return pHitObject && pHitObject == pNearestObject && !bBlockedFromObject;
}

Vec3 CVertexSnappingModeTool::GetCubeSize(IDisplayViewport* pView, const Vec3& pos) const
{
	if (!pView)
		return Vec3(0, 0, 0);
	float fScreenFactor = pView->GetScreenScaleFactor(pos);

	return gVertexSnappingPreferences.vertexCubeSize() * Vec3(fScreenFactor, fScreenFactor, fScreenFactor);
}

void CVertexSnappingModeTool::Display(struct DisplayContext& dc)
{
	const ColorB SnappedColor(0xFF00FF00);
	const ColorB PivotColor(0xFF2020FF);
	const ColorB VertexColor(0xFFFFAAAA);

	dc.SetColor(VertexColor);
	for (int i = 0, iCount(m_Objects.size()); i < iCount; ++i)
	{
		AABB worldAABB;
		m_Objects[i]->GetBoundBox(worldAABB);
		if (!dc.view->IsBoundsVisible(worldAABB))
			continue;
		DrawVertexCubes(dc, m_Objects[i]->GetWorldTM(), m_Objects[i]->GetIStatObj());
	}

	if (m_modeStatus == eVSS_MoveSelectVertexToAnotherVertex && m_SelectionInfo.m_pObject)
	{
		dc.SetColor(0xFFAAAAAA);
		DrawVertexCubes(dc, m_SelectionInfo.m_pObject->GetWorldTM(), m_SelectionInfo.m_pObject->GetIStatObj());
	}

	dc.SetColor(PivotColor);
	dc.DepthTestOff();
	if (m_pHitObject && (!m_bHit || m_bHit && !m_pHitObject->GetWorldPos().IsEquivalent(m_vHitVertex, 0.001f)))
	{
		Vec3 vBoxSize = GetCubeSize(dc.view, m_pHitObject->GetWorldPos()) * 1.2f;
		AABB vertexBox(m_pHitObject->GetWorldPos() - vBoxSize, m_pHitObject->GetWorldPos() + vBoxSize);
		dc.DrawBall((vertexBox.min + vertexBox.max) * 0.5f, (vertexBox.max.x - vertexBox.min.x) * 0.5f);
	}
	dc.DepthTestOn();

	if (m_bHit)
	{
		dc.DepthTestOff();
		dc.SetColor(SnappedColor);
		Vec3 vBoxSize = GetCubeSize(dc.view, m_vHitVertex);
		if (m_vHitVertex.IsEquivalent(m_pHitObject->GetWorldPos(), 0.001f))
			dc.DrawBall(m_vHitVertex, vBoxSize.x * 1.2f);
		else
			dc.DrawSolidBox(m_vHitVertex - vBoxSize, m_vHitVertex + vBoxSize);
		dc.DepthTestOn();
	}

	if (m_pHitObject && m_pHitObject->GetIStatObj())
	{
		SGeometryDebugDrawInfo dd;
		dd.tm = m_pHitObject->GetWorldTM();
		dd.color = ColorB(250, 0, 250, 30);
		dd.lineColor = ColorB(255, 255, 0, 160);
		dd.bDrawInFront = true;
		m_pHitObject->GetIStatObj()->DebugDraw(dd);
	}

	if (VertexSnappingCVars::ed_vert_snapping_show_spatial_partition)
	{
		ColorB boxColor(40, 40, 40);
		for (int i = 0, iCount(m_DebugBoxes.size()); i < iCount; ++i)
		{
			dc.SetColor(boxColor);
			boxColor += ColorB(25, 25, 25);
			dc.DrawWireBox(m_DebugBoxes[i].min, m_DebugBoxes[i].max);
		}
	}
}

void CVertexSnappingModeTool::DrawVertexCubes(DisplayContext& dc, const Matrix34& tm, IStatObj* pStatObj)
{
	if (!pStatObj)
		return;

	IIndexedMesh* pIndexedMesh = pStatObj->GetIndexedMesh();
	if (pIndexedMesh)
	{
		IIndexedMesh::SMeshDescription md;
		pIndexedMesh->GetMeshDescription(md);
		for (int k = 0; k < md.m_nVertCount; ++k)
		{
			Vec3 vPos(0, 0, 0);
			if (md.m_pVerts)
				vPos = md.m_pVerts[k];
			else if (md.m_pVertsF16)
				vPos = md.m_pVertsF16[k].ToVec3();
			else
				continue;
			vPos = tm.TransformPoint(vPos);
			Vec3 vBoxSize = GetCubeSize(dc.view, vPos);
			if (!m_bHit || !m_vHitVertex.IsEquivalent(vPos, 0.001f))
				dc.DrawSolidBox(vPos - vBoxSize, vPos + vBoxSize);
		}
	}

	for (int i = 0, iSubStatObjNum(pStatObj->GetSubObjectCount()); i < iSubStatObjNum; ++i)
	{
		IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(i);
		if (pSubObj)
			DrawVertexCubes(dc, tm * pSubObj->localTM, pSubObj->pStatObj);
	}
}

CKDTree* CVertexSnappingModeTool::GetKDTree(CBaseObject* pObject)
{
	if (m_ObjectKdTreeMap.find(pObject) != m_ObjectKdTreeMap.end())
		return m_ObjectKdTreeMap[pObject];

	CKDTree* pTree = new CKDTree;
	pTree->Build(pObject->GetIStatObj());
	m_ObjectKdTreeMap[pObject] = pTree;

	return pTree;
}

