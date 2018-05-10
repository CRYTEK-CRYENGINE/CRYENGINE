// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Helper.h"
#include "Objects/DesignerObject.h"
#include "Objects/PrefabObject.h"
#include "Model.h"
#include "Polygon.h"
#include "BSPTree2D.h"
#include "Objects/AreaSolidObject.h"
#include "Objects/ClipVolumeObject.h"
#include "DesignerEditor.h"
#include "Viewport.h"
#include "Core/PolygonDecomposer.h"
#include "Util/ElementSet.h"
#include "Core/UVIslandManager.h"
#include "Core/SmoothingGroupManager.h"
#include "Material/Material.h"
#include "DesignerSession.h"
#include "DesignerEditor.h"
#include "Tools/AreaSolidTool.h"
#include "Tools/ClipVolumeTool.h"

namespace Designer
{

	bool CanDesignerEditObject(CBaseObject* pObj)
	{
		if (pObj == NULL)
			return false;

		if (pObj->IsKindOf(RUNTIME_CLASS(DesignerObject))
		|| pObj->IsKindOf(RUNTIME_CLASS(AreaSolidObject))
		|| pObj->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
		{
			return true;
		}

		return false;
	}

bool UpdateStatObjWithoutBackFaces(CBaseObject* pObj)
{
	Model* pModel = NULL;
	ModelCompiler* pCompiler = NULL;

	if (GetModel(pObj, pModel) && GetCompiler(pObj, pCompiler))
	{
		bool bDisplayBackFace = pModel->CheckFlag(eModelFlag_DisplayBackFace);
		if (bDisplayBackFace)
		{
			pModel->SetFlag(pModel->GetFlag() & (~eModelFlag_DisplayBackFace));
			pCompiler->Compile(pObj, pModel);
			pModel->SetFlag(pModel->GetFlag() | eModelFlag_DisplayBackFace);
			return true;
		}
	}
	return false;
}

bool UpdateStatObj(CBaseObject* pObj)
{
	Model* pModel = NULL;
	ModelCompiler* pCompiler = NULL;

	if (GetModel(pObj, pModel) && GetCompiler(pObj, pCompiler))
	{
		pCompiler->Compile(pObj, pModel);
		return true;
	}
	return false;
}

bool UpdateGameResource(CBaseObject* pObj)
{
	if (pObj == NULL)
		return false;

	if (pObj->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
	{
		AreaSolidObject* pArea = (AreaSolidObject*)pObj;
		pArea->InvalidateTM(0);
		return true;
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
	{
		ClipVolumeObject* pVolume = (ClipVolumeObject*)pObj;
		pVolume->InvalidateTM(0);
		return true;
	}

	return false;
}

bool GetIStatObj(CBaseObject* pObj, _smart_ptr<IStatObj>* pOutStatObj)
{
	if (pObj == NULL)
		return false;

	ModelCompiler* pCompiler = NULL;
	if (GetCompiler(pObj, pCompiler))
	{
		pCompiler->GetIStatObj(pOutStatObj);
		return true;
	}

	return false;
}

bool GenerateGameFilename(CBaseObject* pObj, string& outFileName)
{
	if (pObj == NULL)
		return false;

	if (pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
	{
		DesignerObject* pModel = (DesignerObject*)pObj;
		pModel->GenerateGameFilename(outFileName);
		return true;
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
	{
		AreaSolidObject* pArea = (AreaSolidObject*)pObj;
		pArea->GenerateGameFilename(outFileName);
		return true;
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
	{
		ClipVolumeObject* pVolume = (ClipVolumeObject*)pObj;
		outFileName = pVolume->GenerateGameFilename();
		return true;
	}

	return false;
}

bool GetRenderFlag(CBaseObject* pObj, int& outRenderFlag)
{
	if (pObj == NULL)
		return false;

	ModelCompiler* pCompiler = NULL;
	if (GetCompiler(pObj, pCompiler))
	{
		outRenderFlag = pCompiler->GetRenderFlags();
		return true;
	}

	return false;
}

bool GetCompiler(CBaseObject* pObj, ModelCompiler*& pCompiler)
{
	if (pObj == NULL)
		return false;

	pCompiler = NULL;

	if (pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
	{
		DesignerObject* pModel = (DesignerObject*)pObj;
		pCompiler = pModel->GetCompiler();
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
	{
		AreaSolidObject* pArea = (AreaSolidObject*)pObj;
		pCompiler = pArea->GetCompiler();
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
	{
		ClipVolumeObject* pVolume = (ClipVolumeObject*)pObj;
		pCompiler = pVolume->GetCompiler();
	}

	return pCompiler ? true : false;
}

bool GetModel(CBaseObject* pObj, Model*& pModel)
{
	if (pObj == NULL)
		return false;

	pModel = NULL;

	if (pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
	{
		DesignerObject* pModelObj = (DesignerObject*)pObj;
		pModel = pModelObj->GetModel();
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
	{
		AreaSolidObject* pArea = (AreaSolidObject*)pObj;
		pModel = pArea->GetModel();
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
	{
		ClipVolumeObject* pVolume = (ClipVolumeObject*)pObj;
		pModel = pVolume->GetModel();
	}

	return pModel != NULL;
}

void RemovePolygonWithSpecificFlagsFromList(std::vector<PolygonPtr>& polygonList, int nFlags)
{
	auto ii = polygonList.begin();
	for (; ii != polygonList.end(); )
	{
		if ((*ii)->CheckFlags(nFlags))
			ii = polygonList.erase(ii);
		else
			++ii;
	}
}

void RemovePolygonWithoutSpecificFlagsFromList(std::vector<PolygonPtr>& polygonList, int nFlags)
{
	auto ii = polygonList.begin();
	for (; ii != polygonList.end(); )
	{
		if (!(*ii)->CheckFlags(nFlags))
			ii = polygonList.erase(ii);
		else
			++ii;
	}
}

void SwitchToDesignerToolForObject(CBaseObject * pObj)
{
	if(pObj)
	{
		CEditTool* pEditor = GetIEditor()->GetEditTool();

		if (pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
		{
			if (!pEditor || pEditor->GetRuntimeClass() != RUNTIME_CLASS(DesignerEditor))
				GetIEditor()->SetEditTool("EditTool.DesignerEditor", false);
		}
		else if (pObj->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
		{
			if (!pEditor || pEditor->GetRuntimeClass() != RUNTIME_CLASS(AreaSolidTool))
				GetIEditor()->SetEditTool("EditTool.AreaSolidTool", false);
		}
		else if (pObj->IsKindOf(RUNTIME_CLASS(ClipVolumeObject)))
		{
			if (!pEditor || pEditor->GetRuntimeClass() != RUNTIME_CLASS(ClipVolumeTool))
				GetIEditor()->SetEditTool("EditTool.ClipVolumeTool", false);
		}
	}
	else
	{
		GetIEditor()->SetEditTool("EditTool.DesignerEditor", false);
	}
}

namespace MirrorUtil
{
void UpdateMirroredPartWithPlane(Model* pModel, const BrushPlane& plane)
{
	if (!pModel->CheckFlag(eModelFlag_Mirror))
		return;

	const BrushPlane& mirroredPlane = pModel->GetMirrorPlane().MirrorPlane(plane);
	pModel->RemovePolygonsWithSpecificFlagsAndPlane(ePolyFlag_Mirrored, &mirroredPlane);

	std::vector<PolygonPtr> polygons;
	if (!pModel->QueryPolygons(plane, polygons))
		return;

	MirrorUtil::RemoveMirroredPolygons(polygons);

	for (int i = 0, iPolygonSize(polygons.size()); i < iPolygonSize; ++i)
		AddMirroredPolygon(pModel, polygons[i]);
}

void EraseMirroredEdge(Model* pModel, const BrushEdge3D& edge)
{
	if (!pModel->CheckFlag(eModelFlag_Mirror))
		return;

	const BrushPlane& mirrorPlane = pModel->GetMirrorPlane();
	BrushEdge3D mirroredEdge(mirrorPlane.MirrorVertex(edge.m_v[0]), mirrorPlane.MirrorVertex(edge.m_v[1]));

	pModel->EraseEdge(mirroredEdge);
}

void AddMirroredPolygon(Model* pModel, PolygonPtr pPolygon)
{
	if (!pModel->CheckFlag(eModelFlag_Mirror))
		return;
	PolygonPtr pMirroredPolygon = pPolygon->Clone()->Mirror(pModel->GetMirrorPlane());
	pModel->AddPolygon(pMirroredPolygon, false);
}

void RemoveMirroredPolygon(Model* pModel, PolygonPtr pPolygon, bool bLeaveFrame)
{
	if (!pModel->CheckFlag(eModelFlag_Mirror))
		return;

	PolygonPtr pMirroredPolygon = pPolygon->Clone()->Mirror(pModel->GetMirrorPlane());
	if (pMirroredPolygon == NULL)
		return;

	pModel->RemovePolygon(pModel->QueryEquivalentPolygon(pMirroredPolygon), bLeaveFrame);
}

void RemoveMirroredPolygons(std::vector<PolygonPtr>& polygonList)
{
	RemovePolygonWithSpecificFlagsFromList(polygonList, ePolyFlag_Mirrored);
}

void RemoveNonMirroredPolygons(std::vector<PolygonPtr>& polygonList)
{
	RemovePolygonWithoutSpecificFlagsFromList(polygonList, ePolyFlag_Mirrored);
}
}

bool MakeListConsistingOfArc(const BrushVec2& vOutsideVertex, const BrushVec2& vBaseVertex0, const BrushVec2& vBaseVertex1, int nSegmentCount, std::vector<BrushVec2>& outVertexList)
{
	BrushFloat distToCrossPoint;
	BrushEdge::GetSquaredDistance(BrushEdge(vBaseVertex0, vBaseVertex1), vOutsideVertex, distToCrossPoint);
	if (distToCrossPoint == 0)
	{
		DESIGNER_ASSERT(0);
		return false;
	}

	BrushFloat circumCircleRadius;
	BrushVec2 circumCenter;
	if (!ComputeCircumradiusAndCircumcenter(vBaseVertex0, vBaseVertex1, vOutsideVertex, &circumCircleRadius, &circumCenter))
	{
		DESIGNER_ASSERT(0);
		return false;
	}

	const BrushVec2 vXAxis(1.0f, 0.0f);

	BrushVec2 circumCenterToFirstPoint = (vBaseVertex0 - circumCenter).GetNormalized();
	BrushVec2 circumCenterToLastPoint = (vBaseVertex1 - circumCenter).GetNormalized();

	BrushFloat startRadian = acosf(vXAxis.Dot(circumCenterToFirstPoint));
	BrushFloat endRadian = acosf(vXAxis.Dot(circumCenterToLastPoint));

	BrushLine vXAxisLine(circumCenter, circumCenter + vXAxis);

	bool bFirstPointBetween180To360 = vXAxisLine.Distance(vBaseVertex0) < 0;
	if (bFirstPointBetween180To360)
		startRadian = -startRadian;

	bool bSecondPointBetween180To360 = vXAxisLine.Distance(vBaseVertex1) < 0;
	if (bSecondPointBetween180To360)
		endRadian = -endRadian;

	BrushFloat diffRadian = endRadian - startRadian;
	BrushFloat diffOppositeRadian = diffRadian > 0 ? -(PI2 - diffRadian) : -(-PI2 - diffRadian);

	BrushFloat diffTryRadian[2] = { diffRadian, diffOppositeRadian };
	std::vector<BrushVec2> arcTryVertexList[2];
	const int nTryEdgeCount = 3;
	BrushFloat fNearestDistance = 3e10f;
	int nNearerSide(0);

	for (int k = 0; k < 2; ++k)
	{
		MakeSectorOfCircle(circumCircleRadius, circumCenter, startRadian, diffTryRadian[k], nTryEdgeCount, arcTryVertexList[k]);

		for (int i = 0, iArcVertexSize(arcTryVertexList[k].size()); i < iArcVertexSize; ++i)
		{
			const BrushVec2& v0 = arcTryVertexList[k][i];
			const BrushVec2& v1 = arcTryVertexList[k][(i + 1) % iArcVertexSize];

			BrushFloat fDistance = 3e10f;
			BrushEdge::GetSquaredDistance(BrushEdge(v0, v1), vOutsideVertex, fDistance);
			fDistance = std::abs(fDistance);
			if (fDistance < fNearestDistance)
			{
				nNearerSide = k;
				fNearestDistance = fDistance;
			}
		}
	}

	MakeSectorOfCircle(circumCircleRadius, circumCenter, startRadian, diffTryRadian[nNearerSide], nSegmentCount, outVertexList);

	for (int i = 0, iCount(outVertexList.size()); i < iCount; ++i)
	{
		if (!outVertexList[i].IsValid())
		{
			DESIGNER_ASSERT(0);
			return false;
		}
	}

	return true;
}

template<class T>
bool ComputeCircumradiusAndCircumcenter(const T& v0, const T& v1, const T& v2, BrushFloat* outCircumradius, T* outCircumcenter)
{
	BrushFloat dca = (v2 - v0).Dot(v1 - v0);
	BrushFloat dba = (v2 - v1).Dot(v0 - v1);
	BrushFloat dcb = (v0 - v2).Dot(v1 - v2);

	BrushFloat n1 = dba * dcb;
	BrushFloat n2 = dcb * dca;
	BrushFloat n3 = dca * dba;

	BrushFloat n123 = n1 + n2 + n3;

	if (n123 == 0)
		return false;

	if (outCircumradius)
		*outCircumradius = sqrt(((dca + dba) * (dba + dcb) * (dcb + dca)) / n123) * 0.5f;

	if (outCircumcenter)
		*outCircumcenter = (v0 * (n2 + n3) + v1 * (n3 + n1) + v2 * (n1 + n2)) / (2 * n123);

	return true;
}

BrushVec3 GetWorldBottomCenter(CBaseObject* pObject)
{
	AABB boundbox;
	pObject->GetBoundBox(boundbox);
	return BrushVec3(boundbox.GetCenter().x, boundbox.GetCenter().y, boundbox.min.z);
}

BrushVec3 GetOffsetToAnother(CBaseObject* pObject, const BrushVec3& vTargetPos)
{
	BrushVec3 vPos = pObject->GetWorldPos();
	return pObject->GetWorldTM().GetInverted().TransformVector(vPos - vTargetPos);
}

void PivotToCenter(MainContext& mc)
{
	BrushVec3 vBottomCenter = GetWorldBottomCenter(mc.pObject);
	BrushVec3 vWorldPos = mc.pObject->GetWorldPos();
	BrushVec3 vDiff = GetOffsetToAnother(mc.pObject, vBottomCenter);
	mc.pModel->Translate(vDiff);
	mc.pObject->SetWorldPos(vBottomCenter);
}

void PivotToPos(CBaseObject* pObject, Model* pModel, const BrushVec3& vPivot)
{
	BrushVec3 vWorldNewPivot = pObject->GetWorldTM().TransformPoint(vPivot);
	BrushVec3 vWorldPos = pObject->GetWorldPos();
	BrushVec3 vDiff = GetOffsetToAnother(pObject, vWorldNewPivot);
	pModel->Translate(vDiff);
	pObject->SetWorldPos(vWorldPos - pObject->GetWorldTM().TransformVector(vDiff));
}

bool DoesEdgeIntersectRect(const CRect& rect, CViewport* pView, const Matrix34& worldTM, const BrushEdge3D& edge)
{
	const CCamera& camera = GetDesigner()->GetCamera();
	const Plane* pNearPlane = camera.GetFrustumPlane(FR_PLANE_NEAR);
	BrushPlane nearPlane(pNearPlane->n, pNearPlane->d);

	BrushEdge3D clippedEdge(edge);
	clippedEdge.m_v[0] = ToBrushVec3(worldTM.TransformPoint(ToVec3(clippedEdge.m_v[0])));
	clippedEdge.m_v[1] = ToBrushVec3(worldTM.TransformPoint(ToVec3(clippedEdge.m_v[1])));

	BrushVec3 intersectedVertex;
	BrushFloat fDist = 0;
	if (nearPlane.HitTest(BrushRay(edge.m_v[0], edge.m_v[1] - edge.m_v[0]), &fDist, &intersectedVertex) && fDist >= 0 && fDist <= edge.GetLength())
	{
		if (nearPlane.Distance(edge.m_v[0]) < 0)
			clippedEdge.m_v[1] = intersectedVertex;
		else if (nearPlane.Distance(edge.m_v[1]) < 0)
			clippedEdge.m_v[0] = intersectedVertex;
	}

	POINT p0 = pView->WorldToView(ToVec3(clippedEdge.m_v[0]));
	POINT p1 = pView->WorldToView(ToVec3(clippedEdge.m_v[1]));
	BrushEdge3D clippedEdgeOnView(BrushVec3(p0.x, p0.y, 0), BrushVec3(p1.x, p1.y, 0));

	PolygonPtr pRectPolygon = MakePolygonFromRectangle(rect);
	return pRectPolygon->GetBSPTree()->HasIntersection(clippedEdgeOnView);
}

bool DoesSelectionBoxIntersectRect(CViewport* view, const Matrix34& worldTM, const CRect& rect, PolygonPtr pPolygon)
{
	BrushVec3 vBoxSize = GetElementBoxSize(view, view->GetType() != ET_ViewportCamera, worldTM.GetTranslation());
	BrushVec3 vRepresentativePos = pPolygon->GetRepresentativePosition();
	AABB aabb(ToVec3(vRepresentativePos - vBoxSize), ToVec3(vRepresentativePos + vBoxSize));
	CPoint center2D = view->WorldToView(worldTM.TransformPoint(aabb.GetCenter()));
	return rect.PtInRect(center2D) && !pPolygon->CheckFlags(ePolyFlag_Hidden);
}

PolygonPtr MakePolygonFromRectangle(const CRect& rectangle)
{
	std::vector<BrushVec3> vRectangleList;
	vRectangleList.push_back(BrushVec3(rectangle.right, rectangle.top, 0));
	vRectangleList.push_back(BrushVec3(rectangle.right, rectangle.bottom, 0));
	vRectangleList.push_back(BrushVec3(rectangle.left, rectangle.bottom, 0));
	vRectangleList.push_back(BrushVec3(rectangle.left, rectangle.top, 0));
	return new Polygon(vRectangleList);
}

void CopyPolygons(std::vector<PolygonPtr>& sourcePolygons, std::vector<PolygonPtr>& destPolygons)
{
	for (int i = 0, iSourcePolygonCount(sourcePolygons.size()); i < iSourcePolygonCount; ++i)
		destPolygons.push_back(sourcePolygons[i]->Clone());
}

PolygonPtr Convert2ViewPolygon(IDisplayViewport* pView, const BrushMatrix34& worldTM, PolygonPtr pPolygon, PolygonPtr* pOutPolygon)
{
	BrushMatrix34 vInvMatrix = worldTM.GetInverted();

	const BrushVec3 vCameraPos = vInvMatrix.TransformPoint(ToBrushVec3(pView->GetViewTM().GetTranslation()));
	const BrushVec3 vDir = vInvMatrix.TransformVector(ToBrushVec3(pView->GetViewTM().GetColumn1()));

	std::set<int> verticesBehindCamera;
	for (int i = 0, iVertexCount(pPolygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		if (vDir.Dot((pPolygon->GetPos(i) - vCameraPos).GetNormalized()) <= 0)
			verticesBehindCamera.insert(i);
	}

	if (verticesBehindCamera.size() == pPolygon->GetVertexCount())
		return NULL;

	PolygonPtr pViewPolygon = new Polygon(*pPolygon);
	pViewPolygon->Transform(worldTM);

	if (!verticesBehindCamera.empty())
	{
		const CCamera& camera = GetDesigner()->GetCamera();
		const Plane* pNearPlane = camera.GetFrustumPlane(FR_PLANE_NEAR);
		BrushPlane nearPlane(pNearPlane->n, pNearPlane->d);

		std::vector<PolygonPtr> frontPolygons;
		std::vector<PolygonPtr> backPolygons;
		pViewPolygon->ClipByPlane(nearPlane, frontPolygons, backPolygons);

		int nBackPolygonCount = backPolygons.size();
		if (nBackPolygonCount == 1)
		{
			pViewPolygon = backPolygons[0];
		}
		else if (nBackPolygonCount > 1)
		{
			pViewPolygon->Clear();
			for (int i = 0; i < nBackPolygonCount; ++i)
				pViewPolygon->Union(backPolygons[i]);
		}

		for (int i = 0, iVertexCount(pViewPolygon->GetVertexCount()); i < iVertexCount; ++i)
		{
			const BrushVec3& v = pViewPolygon->GetPos(i);
			BrushFloat fDistance = nearPlane.Distance(v);
			if (fDistance > -kDesignerEpsilon)
				pViewPolygon->SetPos(i, v - nearPlane.Normal() * (BrushFloat)0.001);
		}

		if (pOutPolygon)
		{
			*pOutPolygon = pViewPolygon->Clone();
			(*pOutPolygon)->Transform(worldTM.GetInverted());
		}
	}
	else
	{
		if (pOutPolygon)
			*pOutPolygon = NULL;
	}

	for (int i = 0, iVertexCount(pViewPolygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		CPoint pt = pView->WorldToView(pViewPolygon->GetPos(i));
		pViewPolygon->SetPos(i, BrushVec3((BrushFloat)pt.x, (BrushFloat)pt.y, 0));
	}

	BrushPlane plane(BrushVec3(0, 0, 1), 0);
	pViewPolygon->GetComputedPlane(plane);
	pViewPolygon->SetPlane(plane);

	return pViewPolygon;
}

bool IsFrameLeftInRemovingPolygon(CBaseObject* pObject)
{
	if (pObject == NULL)
		return false;

	return pObject->GetType() != OBJTYPE_SOLID ? true : false;
}

bool BinarySearchForScale(BrushFloat fValidScale, BrushFloat fInvalidScale, int nCount, Polygon& polygon, BrushFloat& fOutScale)
{
	static const int kMaxCount(10);
	static const int kMaxCountToAvoidInfiniteLoop(100);

	BrushFloat fMiddleScale = (fValidScale + fInvalidScale) * 0.5f;

	Polygon tempPolygon(polygon);
	if (tempPolygon.Scale(fMiddleScale, true))
	{
		if (nCount >= kMaxCount || std::abs(fValidScale - fMiddleScale) <= (BrushFloat)0.001)
		{
			fOutScale = fMiddleScale;
			return true;
		}
		return BinarySearchForScale(fMiddleScale, fInvalidScale, nCount + 1, polygon, fOutScale);
	}

	if (nCount >= kMaxCountToAvoidInfiniteLoop)
		return false;

	return BinarySearchForScale(fValidScale, fMiddleScale, nCount + 1, polygon, fOutScale);
}

BrushFloat ApplyScaleCheckingBoundary(PolygonPtr pPolygon, BrushFloat fScale)
{
	if (!pPolygon)
		return fScale;

	if (!pPolygon->Scale(fScale, true))
	{
		BrushFloat fChangedScale = fScale;
		if (BinarySearchForScale(0, fScale, 0, *pPolygon, fChangedScale))
		{
			if (pPolygon->Scale(fChangedScale, true))
				return fChangedScale;
		}
	}

	return fScale;
}

void ResetXForm(MainContext& mc, int nResetFlag)
{
	if (mc.pModel == NULL || mc.pObject == NULL)
		return;

	Matrix34 worldTM;
	worldTM.SetIdentity();

	if ((nResetFlag & eResetXForm_Rotation) && (nResetFlag & eResetXForm_Scale))
	{
		Matrix34 brushTM = mc.pObject->GetWorldTM();
		brushTM.SetTranslation(BrushVec3(0, 0, 0));
		mc.pModel->Transform(ToBrushMatrix34(brushTM));
	}
	else if (nResetFlag & eResetXForm_Rotation)
	{
		Matrix34 rotationTM = Matrix34(mc.pObject->GetRotation());
		worldTM = Matrix34::CreateScale(mc.pObject->GetScale());
		mc.pModel->Transform(ToBrushMatrix34(rotationTM));
	}
	else if (nResetFlag & eResetXForm_Scale)
	{
		Matrix34 scaleTM = Matrix34::CreateScale(mc.pObject->GetScale());
		worldTM = Matrix34(mc.pObject->GetRotation());
		mc.pModel->Transform(ToBrushMatrix34(scaleTM));
	}

	if ((nResetFlag & eResetXForm_Rotation) || (nResetFlag & eResetXForm_Scale))
	{
		worldTM.SetTranslation(mc.pObject->GetWorldPos());
		mc.pObject->SetWorldTM(worldTM);
	}

	if (nResetFlag & eResetXForm_Position)
		PivotToCenter(mc);
}

void AddPolygonWithSubMatID(PolygonPtr pPolygon, bool bUnion)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	Model* pModel = pSession->GetModel();
	pPolygon->SetSubMatID(pSession->GetCurrentSubMatID());

	if (bUnion)
		pModel->AddUnionPolygon(pPolygon);
	else
		pModel->AddPolygon(pPolygon);
}

void CreateMeshFacesFromPolygon(PolygonPtr pPolygon, FlexibleMesh& mesh, bool bCreateBackFaces)
{
	PolygonDecomposer decomposer;
	decomposer.TriangulatePolygon(pPolygon, mesh);

	if (bCreateBackFaces)
	{
		FlexibleMesh meshForBackfaces(mesh);
		meshForBackfaces.Invert();
		mesh.Join(meshForBackfaces);
	}

	TransformUVs(pPolygon->GetTexInfo(), mesh);

	int nMatID = pPolygon->GetSubMatID();
	int nSubsetNumber = mesh.AddMatID(nMatID);
	for (int k = 0, iFaceSize(mesh.faceList.size()); k < iFaceSize; ++k)
		mesh.faceList[k].nSubset = nSubsetNumber;
}

void TransformUVs(const TexInfo& texInfo, FlexibleMesh& mesh)
{
	AABB uvBoundBox;
	uvBoundBox.Reset();

	for (int i = 0, iCount(mesh.vertexList.size()); i < iCount; ++i)
		uvBoundBox.Add(mesh.vertexList[i].uv);

	Vec3 uvCenter = uvBoundBox.GetCenter();

	Matrix33 m = Matrix33::CreateRotationZ(DEG2RAD(texInfo.rotate)) * Matrix33::CreateScale(Vec3(1.0f / texInfo.scale[0], 1.0f / texInfo.scale[1], 0));

	for (int i = 0, iCount(mesh.vertexList.size()); i < iCount; ++i)
	{
		Vec3 localUV(mesh.vertexList[i].uv.x - uvCenter.x, mesh.vertexList[i].uv.y - uvCenter.y, 1);

		m.SetColumn(2, Vec3(uvCenter.x + texInfo.shift[0], uvCenter.y + texInfo.shift[1], 1));

		Vec3 transformedUV = m.TransformVector(localUV);

		mesh.vertexList[i].uv.x = transformedUV.x;
		mesh.vertexList[i].uv.y = transformedUV.y;
	}
}

bool HitTest(const BrushRay& ray, MainContext& mc, HitContext& hc)
{
	if (!mc.pModel || !mc.pObject)
		return false;

	MODEL_SHELF_RECONSTRUCTOR(mc.pModel);
	mc.pModel->SetShelf(eShelf_Base);

	BrushVec3 outPosition;
	const Matrix34& worldTM(mc.pObject->GetWorldTM());
	if (mc.pModel->QueryPosition(ray, outPosition))
	{
		BrushVec3 worldRaySrc = worldTM.TransformPoint(ray.origin);
		BrushVec3 worldHitPos = worldTM.TransformPoint(outPosition);
		hc.dist = (float)worldRaySrc.GetDistance(worldHitPos);
		return true;
	}

	return true;
}

void AssignMatIDToSelectedPolygons(int matID)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelect = pSession->GetSelectedElements();
	Model* pModel = pSession->GetModel();

	for (int i = 0, iCount(pSelect->GetCount()); i < iCount; ++i)
	{
		if (!(*pSelect)[i].m_pPolygon)
			continue;
		(*pSelect)[i].m_pPolygon->SetSubMatID(matID);

		// Probably checking for pModel here is redundant
		if (!pModel || !pModel->CheckFlag(eModelFlag_Mirror))
			continue;
		MirrorUtil::RemoveMirroredPolygon(pModel, (*pSelect)[i].m_pPolygon);
		MirrorUtil::AddMirroredPolygon(pModel, (*pSelect)[i].m_pPolygon);
	}
}

std::vector<DesignerObject*> GetSelectedDesignerObjects()
{
	std::vector<DesignerObject*> designerObjectSet;
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	for (int i = 0, iSelectionCount(pSelection->GetCount()); i < iSelectionCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		if (!pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
			continue;
		designerObjectSet.push_back((DesignerObject*)pObj);
	}
	return designerObjectSet;
}

bool IsDesignerRelated(CBaseObject* pObject)
{
	return pObject->IsKindOf(RUNTIME_CLASS(DesignerObject)) || pObject->IsKindOf(RUNTIME_CLASS(AreaSolidObject)) || pObject->IsKindOf(RUNTIME_CLASS(ClipVolumeObject));
}

void RemoveAllEmptyDesignerObjects()
{
	DynArray<CBaseObject*> objects;
	GetIEditor()->GetObjectManager()->GetObjects(objects);
	for (int i = 0, iCount(objects.size()); i < iCount; ++i)
	{
		if (!objects[i]->IsKindOf(RUNTIME_CLASS(DesignerObject)))
			continue;
		DesignerObject* pDesignerObj = (DesignerObject*)objects[i];
		if (pDesignerObj->IsEmpty())
			GetIEditor()->GetObjectManager()->DeleteObject(pDesignerObj);
	}
}

bool CheckIfHasValidModel(CBaseObject* pObject)
{
	if (pObject == NULL)
		return false;

	Model* pModel = NULL;
	if (!GetModel(pObject, pModel))
		return false;

	if (pModel->IsEmpty(eShelf_Base))
		return false;

	bool bValidModel = false;
	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = pModel->GetPolygon(i);
		if (!polygon->IsOpen())
		{
			bValidModel = true;
			break;
		}
	}

	return bValidModel;
}

void MakeRectangle(const BrushPlane& plane, const BrushVec2& startPos, const BrushVec2& endPos, std::vector<BrushVec3>& outVertices)
{
	outVertices.reserve(4);
	outVertices.push_back(plane.P2W(BrushVec2(startPos.x, startPos.y)));
	outVertices.push_back(plane.P2W(BrushVec2(startPos.x, endPos.y)));
	outVertices.push_back(plane.P2W(BrushVec2(endPos.x, endPos.y)));
	outVertices.push_back(plane.P2W(BrushVec2(endPos.x, startPos.y)));
}

void ConvertMeshFacesToFaces(const std::vector<Vertex>& vertices, std::vector<SMeshFace>& meshFaces, std::vector<std::vector<Vec3>>& outFaces)
{
	for (int i = 0, iFaceCount(meshFaces.size()); i < iFaceCount; ++i)
	{
		std::vector<Vec3> f;
		f.push_back(vertices[meshFaces[i].v[0]].pos);
		f.push_back(vertices[meshFaces[i].v[1]].pos);
		f.push_back(vertices[meshFaces[i].v[2]].pos);
		outFaces.push_back(f);
	}
}

void MergeTwoObjects(const MainContext& mc0, const MainContext& mc1)
{
	if (!mc0.IsValid() || !mc1.IsValid())
		return;

	MODEL_SHELF_RECONSTRUCTOR(mc0.pModel);
	MODEL_SHELF_RECONSTRUCTOR_POSTFIX(mc1.pModel, 1);

	BrushVec3 offset = mc1.pObject->GetPos() - mc0.pObject->GetWorldPos();
	Matrix34 offsetTM(mc1.pObject->GetWorldTM());
	offsetTM.SetTranslation(offset);

	mc0.pModel->RecordUndo("Designer : Merge", mc0.pObject);
	mc1.pModel->RecordUndo("Designer : Merge", mc1.pObject);

	int nSubMatCount = mc0.pObject->GetMaterial()->GetSubMaterialCount();

	for (int shelfID = eShelf_Base; shelfID < cShelfMax; ++shelfID)
	{
		mc0.pModel->SetShelf(static_cast<ShelfID>(shelfID));
		mc1.pModel->SetShelf(static_cast<ShelfID>(shelfID));
		for (int i = 0, nPolygonSize(mc1.pModel->GetPolygonCount()); i < nPolygonSize; ++i)
		{
			PolygonPtr pTargetPolygon = mc1.pModel->GetPolygon(i);
			if (pTargetPolygon->GetSubMatID() >= nSubMatCount)
				pTargetPolygon->SetSubMatID(0);
			pTargetPolygon->Transform(offsetTM);
			mc0.pModel->AddPolygon(pTargetPolygon, false);
		}
		mc1.pModel->Clear();
	}

	mc0.pModel->InvalidateAABB();
	mc0.pModel->GetUVIslandMgr()->Join(mc1.pModel->GetUVIslandMgr());
	mc1.pModel->GetUVIslandMgr()->Clear();
}

void AddPolygonToListCheckingDuplication(std::vector<PolygonPtr>& polygonList, PolygonPtr polygon)
{
	for (auto ii = polygonList.begin(); ii != polygonList.end(); ++ii)
	{
		if ((*ii) == polygon)
			return;
	}
	polygonList.push_back(polygon);
}

int AddVertex(std::vector<Vertex>& vertices, const Vertex& newVertex)
{
	for (int i = 0, vSize(vertices.size()); i < vSize; ++i)
	{
		if (Comparison::IsEquivalent(newVertex.pos, vertices[i].pos) &&
		    newVertex.id == vertices[i].id)
		{
			return i;
		}
	}
	vertices.push_back(newVertex);
	return vertices.size() - 1;
}
}

