// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjmandraw.cpp
//  Version:     v1.00
//  Created:     18/12/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Visibility areas
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "VisAreas.h"
#include "3dEngine.h"
#include "TimeOfDay.h"

PodArray<CVisArea*> CVisArea::m_lUnavailableAreas;
PodArray<Vec3> CVisArea::s_tmpLstPortVertsClipped;
PodArray<Vec3> CVisArea::s_tmpLstPortVertsSS;
PodArray<Vec3> CVisArea::s_tmpPolygonA;
PodArray<IRenderNode*> CVisArea::s_tmpLstLights;
PodArray<CTerrainNode*> CVisArea::s_tmpLstTerrainNodeResult;
CPolygonClipContext CVisArea::s_tmpClipContext;
PodArray<CCamera> CVisArea::s_tmpCameras;
int CVisArea::s_nGetDistanceThruVisAreasCallCounter = 0;

void CVisArea::Update(const Vec3* pPoints, int nCount, const char* szName, const SVisAreaInfo& info)
{
	assert(m_pVisAreaColdData);

	cry_strcpy(m_pVisAreaColdData->m_sName, szName);
	_strlwr_s(m_pVisAreaColdData->m_sName, sizeof(m_pVisAreaColdData->m_sName));

	m_bThisIsPortal = strstr(m_pVisAreaColdData->m_sName, "portal") != 0;
	m_bIgnoreSky = (strstr(m_pVisAreaColdData->m_sName, "ignoresky") != 0) || info.bIgnoreSkyColor;

	m_fHeight = info.fHeight;
	m_vAmbientColor = info.vAmbientColor;
	m_bAffectedByOutLights = info.bAffectedByOutLights;
	m_bSkyOnly = info.bSkyOnly;
	m_fViewDistRatio = info.fViewDistRatio;
	m_bDoubleSide = info.bDoubleSide;
	m_bUseDeepness = info.bUseDeepness;
	m_bUseInIndoors = info.bUseInIndoors;
	m_bOceanVisible = info.bOceanIsVisible;
	m_bIgnoreGI = info.bIgnoreGI;
	m_bIgnoreOutdoorAO = info.bIgnoreOutdoorAO;
	m_fPortalBlending = info.fPortalBlending;

	m_lstShapePoints.PreAllocate(nCount, nCount);

	if (nCount)
		memcpy(&m_lstShapePoints[0], pPoints, sizeof(Vec3) * nCount);

	// update bbox
	m_boxArea.max = SetMinBB();
	m_boxArea.min = SetMaxBB();

	for (int i = 0; i < nCount; i++)
	{
		m_boxArea.max.CheckMax(pPoints[i]);
		m_boxArea.min.CheckMin(pPoints[i]);

		m_boxArea.max.CheckMax(pPoints[i] + Vec3(0, 0, m_fHeight));
		m_boxArea.min.CheckMin(pPoints[i] + Vec3(0, 0, m_fHeight));
	}

	UpdateGeometryBBox();
	UpdateClipVolume();
}

void CVisArea::StaticReset()
{
	stl::free_container(m_lUnavailableAreas);
	stl::free_container(s_tmpLstPortVertsClipped);
	stl::free_container(s_tmpLstPortVertsSS);
	stl::free_container(s_tmpPolygonA);
	stl::free_container(s_tmpLstLights);
	stl::free_container(s_tmpLstTerrainNodeResult);
	stl::free_container(s_tmpCameras);
	s_tmpClipContext.Reset();
}

void CVisArea::Init()
{
	m_fGetDistanceThruVisAreasMinDistance = 10000.f;
	m_nGetDistanceThruVisAreasLastCallID = -1;
	m_pVisAreaColdData = NULL;
	m_boxStatics.min = m_boxStatics.max = m_boxArea.min = m_boxArea.max = Vec3(0, 0, 0);
	m_nRndFrameId = -1;
	m_bActive = true;
	m_fHeight = 0;
	m_vAmbientColor(0, 0, 0);
	m_vConnNormals[0] = m_vConnNormals[1] = Vec3(0, 0, 0);
	m_bAffectedByOutLights = false;
	m_fDistance = 0;
	m_bOceanVisible = m_bSkyOnly = false;
	memset(m_arrOcclCamera, 0, sizeof(m_arrOcclCamera));
	m_fViewDistRatio = 100.f;
	m_bDoubleSide = true;
	m_bUseDeepness = false;
	m_bUseInIndoors = false;
	m_bIgnoreSky = m_bThisIsPortal = false;
	m_bIgnoreGI = false;
	m_bIgnoreOutdoorAO = false;
	m_lstCurCamerasCap = 0;
	m_lstCurCamerasLen = 0;
	m_lstCurCamerasIdx = 0;
	m_nVisGUID = 0;
	m_fPortalBlending = 0.5f;
	m_nStencilRef = 0;
}

CVisArea::CVisArea()
{
	Init();
}

CVisArea::CVisArea(VisAreaGUID visGUID)
{
	Init();
	m_nVisGUID = visGUID;
}

CVisArea::~CVisArea()
{
	for (int i = 0; i < MAX_RECURSION_LEVELS; i++)
		SAFE_DELETE(m_arrOcclCamera[i]);

	GetVisAreaManager()->OnVisAreaDeleted(this);
}

float LineSegDistanceSqr(const Vec3& vPos, const Vec3& vP0, const Vec3& vP1)
{
	// Dist of line seg A(+D) from origin:
	// P = A + D t[0..1]
	// d^2(t) = (A + D t)^2 = A^2 + 2 A*D t + D^2 t^2
	// d^2\t = 2 A*D + 2 D^2 t = 0
	// tmin = -A*D / D^2 clamp_tpl(0,1)
	// Pmin = A + D tmin
	Vec3 vP = vP0 - vPos;
	Vec3 vD = vP1 - vP0;
	float fN = -(vP * vD);
	if (fN > 0.f)
	{
		float fD = vD.GetLengthSquared();
		if (fN >= fD)
			vP += vD;
		else
			vP += vD * (fN / fD);
	}
	return vP.GetLengthSquared();
}

void CVisArea::FindSurroundingVisAreaReqursive(int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*>* pVisitedAreas, int nMaxVisitedAreas, int nDeepness, PodArray<CVisArea*>* pUnavailableAreas)
{
	pUnavailableAreas->Add(this);

	if (pVisitedAreas && pVisitedAreas->Count() < nMaxVisitedAreas)
		pVisitedAreas->Add(this);

	if (nMaxReqursion > (nDeepness + 1))
		for (int p = 0; p < m_lstConnections.Count(); p++)
			if (!bSkipDisabledPortals || m_lstConnections[p]->IsActive())
			{
				if (-1 == pUnavailableAreas->Find(m_lstConnections[p]))
					m_lstConnections[p]->FindSurroundingVisAreaReqursive(nMaxReqursion, bSkipDisabledPortals, pVisitedAreas, nMaxVisitedAreas, nDeepness + 1, pUnavailableAreas);
			}
}

void CVisArea::FindSurroundingVisArea(int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*>* pVisitedAreas, int nMaxVisitedAreas, int nDeepness)
{
	if (pVisitedAreas)
	{
		if (pVisitedAreas->capacity() < (uint32)nMaxVisitedAreas)
			pVisitedAreas->PreAllocate(nMaxVisitedAreas);
	}

	m_lUnavailableAreas.Clear();
	m_lUnavailableAreas.PreAllocate(nMaxVisitedAreas, 0);

	FindSurroundingVisAreaReqursive(nMaxReqursion, bSkipDisabledPortals, pVisitedAreas, nMaxVisitedAreas, nDeepness, &m_lUnavailableAreas);
}

int CVisArea::GetVisFrameId()
{
	return m_nRndFrameId;
}

bool Is2dLinesIntersect(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
	float fDiv = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);

	if (fabs(fDiv) < 0.00001f)
		return false;

	float ua = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / fDiv;
	float ub = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / fDiv;

	return ua > 0 && ua < 1 && ub > 0 && ub < 1;
}

Vec3 CVisArea::GetConnectionNormal(CVisArea* pPortal)
{
	assert(m_lstShapePoints.Count() >= 3);
	// find side of shape intersecting with portal
	int nIntersNum = 0;
	Vec3 arrNormals[2] = { Vec3(0, 0, 0), Vec3(0, 0, 0) };
	for (int v = 0; v < m_lstShapePoints.Count(); v++)
	{
		nIntersNum = 0;
		arrNormals[0] = Vec3(0, 0, 0);
		arrNormals[1] = Vec3(0, 0, 0);
		const Vec3& v0 = m_lstShapePoints[v];
		const Vec3& v1 = m_lstShapePoints[(v + 1) % m_lstShapePoints.Count()];
		for (int p = 0; p < pPortal->m_lstShapePoints.Count(); p++)
		{
			const Vec3& p0 = pPortal->m_lstShapePoints[p];
			const Vec3& p1 = pPortal->m_lstShapePoints[(p + 1) % pPortal->m_lstShapePoints.Count()];

			if (Is2dLinesIntersect(v0.x, v0.y, v1.x, v1.y, p0.x, p0.y, p1.x, p1.y))
			{
				Vec3 vNormal = (v0 - v1).GetNormalized().Cross(Vec3(0, 0, 1.f));
				if (nIntersNum < 2)
					arrNormals[nIntersNum++] = (IsShapeClockwise()) ? -vNormal : vNormal;
			}
		}

		if (nIntersNum == 2)
			break;
	}

	if (nIntersNum == 2 &&
	    //IsEquivalent(arrNormals[0] == arrNormals[1])
	    arrNormals[0].IsEquivalent(arrNormals[1], VEC_EPSILON)
	    )
		return arrNormals[0];

	{
		int nBottomPoints = 0;
		for (int p = 0; p < pPortal->m_lstShapePoints.Count() && p < 4; p++)
			if (IsPointInsideVisArea(pPortal->m_lstShapePoints[p]))
				nBottomPoints++;

		int nUpPoints = 0;
		for (int p = 0; p < pPortal->m_lstShapePoints.Count() && p < 4; p++)
			if (IsPointInsideVisArea(pPortal->m_lstShapePoints[p] + Vec3(0, 0, pPortal->m_fHeight)))
				nUpPoints++;

		if (nBottomPoints == 0 && nUpPoints == 4)
			return Vec3(0, 0, 1);

		if (nBottomPoints == 4 && nUpPoints == 0)
			return Vec3(0, 0, -1);
	}

	return Vec3(0, 0, 0);
}

void CVisArea::UpdatePortalCameraPlanes(CCamera& cam, Vec3* pVerts, bool NotForcePlaneSet, const SRenderingPassInfo& passInfo)
{
	const Vec3& vCamPos = passInfo.GetCamera().GetPosition();
	Plane plane, farPlane;

	farPlane = *passInfo.GetCamera().GetFrustumPlane(FR_PLANE_FAR);
	cam.SetFrustumPlane(FR_PLANE_FAR, farPlane);

	//plane.SetPlane(pVerts[0],pVerts[2],pVerts[1]);  // can potentially create a plane facing the wrong way
	plane.SetPlane(-farPlane.n, pVerts[0]);
	cam.SetFrustumPlane(FR_PLANE_NEAR, plane);

	plane.SetPlane(vCamPos, pVerts[3], pVerts[2]);  // update plane only if it reduces fov
	if (!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_LEFT)->n) <
	    cam.GetFrustumPlane(FR_PLANE_RIGHT)->n.Dot(cam.GetFrustumPlane(FR_PLANE_LEFT)->n))
		cam.SetFrustumPlane(FR_PLANE_RIGHT, plane);

	plane.SetPlane(vCamPos, pVerts[1], pVerts[0]);  // update plane only if it reduces fov
	if (!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_RIGHT)->n) <
	    cam.GetFrustumPlane(FR_PLANE_LEFT)->n.Dot(cam.GetFrustumPlane(FR_PLANE_RIGHT)->n))
		cam.SetFrustumPlane(FR_PLANE_LEFT, plane);

	plane.SetPlane(vCamPos, pVerts[0], pVerts[3]);  // update plane only if it reduces fov
	if (!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_TOP)->n) <
	    cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n.Dot(cam.GetFrustumPlane(FR_PLANE_TOP)->n))
		cam.SetFrustumPlane(FR_PLANE_BOTTOM, plane);

	plane.SetPlane(vCamPos, pVerts[2], pVerts[1]); // update plane only if it reduces fov
	if (!NotForcePlaneSet || plane.n.Dot(cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n) <
	    cam.GetFrustumPlane(FR_PLANE_TOP)->n.Dot(cam.GetFrustumPlane(FR_PLANE_BOTTOM)->n))
		cam.SetFrustumPlane(FR_PLANE_TOP, plane);

	Vec3 arrvPortVertsCamSpace[4];
	for (int i = 0; i < 4; i++)
		arrvPortVertsCamSpace[i] = pVerts[i] - cam.GetPosition();
	cam.SetFrustumVertices(arrvPortVertsCamSpace);

	//cam.UpdateFrustum();

	if (GetCVars()->e_Portals == 5)
	{
		float farrColor[4] = { 1, 1, 1, 1 };

		DrawLine(pVerts[0], pVerts[1]);
		IRenderAuxText::DrawLabelEx(pVerts[0], 1, farrColor, false, true, "0");
		DrawLine(pVerts[1], pVerts[2]);
		IRenderAuxText::DrawLabelEx(pVerts[1], 1, farrColor, false, true, "1");
		DrawLine(pVerts[2], pVerts[3]);
		IRenderAuxText::DrawLabelEx(pVerts[2], 1, farrColor, false, true, "2");
		DrawLine(pVerts[3], pVerts[0]);
		IRenderAuxText::DrawLabelEx(pVerts[3], 1, farrColor, false, true, "3");
	}
}

int __cdecl CVisAreaManager__CmpDistToPortal(const void* v1, const void* v2);

void        CVisArea::PreRender(int nReqursionLevel,
                                CCamera CurCamera, CVisArea* pParent, CVisArea* pCurPortal,
                                bool* pbOutdoorVisible, PodArray<CCamera>* plstOutPortCameras, bool* pbSkyVisible, bool* pbOceanVisible,
                                PodArray<CVisArea*>& lstVisibleAreas,
                                const SRenderingPassInfo& passInfo)
{
	// mark as rendered
	if (m_nRndFrameId != passInfo.GetFrameID())
	{
		m_lstCurCamerasIdx = 0;
		m_lstCurCamerasLen = 0;
		if (m_lstCurCamerasCap)
		{
			m_lstCurCamerasIdx = s_tmpCameras.size();
			s_tmpCameras.resize(s_tmpCameras.size() + m_lstCurCamerasCap);
		}
	}

	m_nRndFrameId = passInfo.GetFrameID();

	if (m_bAffectedByOutLights)
		GetVisAreaManager()->m_bSunIsNeeded = true;

	if (m_lstCurCamerasLen == m_lstCurCamerasCap)
	{
		int newIdx = s_tmpCameras.size();

		m_lstCurCamerasCap += max(1, m_lstCurCamerasCap / 2);
		s_tmpCameras.resize(newIdx + m_lstCurCamerasCap);
		if (m_lstCurCamerasLen)
			memcpy(&s_tmpCameras[newIdx], &s_tmpCameras[m_lstCurCamerasIdx], m_lstCurCamerasLen * sizeof(CCamera));

		m_lstCurCamerasIdx = newIdx;
	}
	s_tmpCameras[m_lstCurCamerasIdx + m_lstCurCamerasLen] = CurCamera;
	++m_lstCurCamerasLen;

	if (lstVisibleAreas.Find(this) < 0 && GetCVars()->e_ClipVolumes)
	{
		lstVisibleAreas.Add(this);
		m_nStencilRef = passInfo.GetIRenderView()->AddClipVolume(this);
	}

	// check recursion and portal activity
	if (!nReqursionLevel || !m_bActive)
		return;

	if (pParent && m_bThisIsPortal && m_lstConnections.Count() == 1 && // detect entrance
	    !IsPointInsideVisArea(passInfo.GetCamera().GetPosition()))     // detect camera in outdoors
	{
		AABB boxAreaEx = m_boxArea;
		float fZNear = CurCamera.GetNearPlane();
		boxAreaEx.min -= Vec3(fZNear, fZNear, fZNear);
		boxAreaEx.max += Vec3(fZNear, fZNear, fZNear);
		if (!CurCamera.IsAABBVisible_E(boxAreaEx)) // if portal is invisible
			return;                                  // stop recursion
	}

	bool bCanSeeThruThisArea = true;

	// prepare new camera for next areas
	if (m_bThisIsPortal && m_lstConnections.Count() &&
	    (this != pCurPortal || !pCurPortal->IsPointInsideVisArea(CurCamera.GetPosition())))
	{
		Vec3 vCenter = m_boxArea.GetCenter();
		float fRadius = m_boxArea.GetRadius();

		Vec3 vPortNorm = (!pParent || pParent == m_lstConnections[0] || m_lstConnections.Count() == 1) ?
		                 m_vConnNormals[0] : m_vConnNormals[1];

		// exit/entrance portal has only one normal in direction to outdoors, so flip it to the camera
		if (m_lstConnections.Count() == 1 && !pParent)
			vPortNorm = -vPortNorm;

		// back face check
		Vec3 vPortToCamDir = CurCamera.GetPosition() - vCenter;
		if (vPortToCamDir.Dot(vPortNorm) < 0)
			return;

		if (!m_bDoubleSide)
			if (vPortToCamDir.Dot(m_vConnNormals[0]) < 0)
				return;

		Vec3 arrPortVerts[4];
		Vec3 arrPortVertsOtherSide[4];
		bool barrPortVertsOtherSideValid = false;
		if (pParent && !vPortNorm.IsEquivalent(Vec3(0, 0, 0), VEC_EPSILON) && vPortNorm.z)
		{
			// up/down portal
			int nEven = IsShapeClockwise();
			if (vPortNorm.z > 0)
				nEven = !nEven;
			for (int i = 0; i < 4; i++)
			{
				arrPortVerts[i] = m_lstShapePoints[nEven ? (3 - i) : i] + Vec3(0, 0, m_fHeight) * (vPortNorm.z > 0);
				arrPortVertsOtherSide[i] = m_lstShapePoints[nEven ? (3 - i) : i] + Vec3(0, 0, m_fHeight) * (vPortNorm.z < 0);
			}
			barrPortVertsOtherSideValid = true;
		}
		else if (!vPortNorm.IsEquivalent(Vec3(0, 0, 0), VEC_EPSILON) && vPortNorm.z == 0)
		{
			// basic portal
			Vec3 arrInAreaPoint[2] = { Vec3(0, 0, 0), Vec3(0, 0, 0) };
			int arrInAreaPointId[2] = { -1, -1 };
			int nInAreaPointCounter = 0;

			Vec3 arrOutAreaPoint[2] = { Vec3(0, 0, 0), Vec3(0, 0, 0) };
			int nOutAreaPointCounter = 0;

			// find 2 points of portal in this area (or in this outdoors)
			for (int i = 0; i < m_lstShapePoints.Count() && nInAreaPointCounter < 2; i++)
			{
				Vec3 vTestPoint = m_lstShapePoints[i] + Vec3(0, 0, m_fHeight * 0.5f);
				CVisArea* pAnotherArea = m_lstConnections[0];
				if ((pParent && (pParent->IsPointInsideVisArea(vTestPoint))) ||
				    (!pParent && (!pAnotherArea->IsPointInsideVisArea(vTestPoint))))
				{
					arrInAreaPointId[nInAreaPointCounter] = i;
					arrInAreaPoint[nInAreaPointCounter++] = m_lstShapePoints[i];
				}
			}

			// find 2 points of portal not in this area (or not in this outdoors)
			for (int i = 0; i < m_lstShapePoints.Count() && nOutAreaPointCounter < 2; i++)
			{
				Vec3 vTestPoint = m_lstShapePoints[i] + Vec3(0, 0, m_fHeight * 0.5f);
				CVisArea* pAnotherArea = m_lstConnections[0];
				if ((pParent && (pParent->IsPointInsideVisArea(vTestPoint))) ||
				    (!pParent && (!pAnotherArea->IsPointInsideVisArea(vTestPoint))))
				{
				}
				else
				{
					arrOutAreaPoint[nOutAreaPointCounter++] = m_lstShapePoints[i];
				}
			}

			if (nInAreaPointCounter == 2)
			{
				// success, take into account volume and portal shape versts order
				int nEven = IsShapeClockwise();
				if (arrInAreaPointId[1] - arrInAreaPointId[0] != 1)
					nEven = !nEven;

				arrPortVerts[0] = arrInAreaPoint[nEven];
				arrPortVerts[1] = arrInAreaPoint[nEven] + Vec3(0, 0, m_fHeight);
				arrPortVerts[2] = arrInAreaPoint[!nEven] + Vec3(0, 0, m_fHeight);
				arrPortVerts[3] = arrInAreaPoint[!nEven];

				nEven = !nEven;

				arrPortVertsOtherSide[0] = arrOutAreaPoint[nEven];
				arrPortVertsOtherSide[1] = arrOutAreaPoint[nEven] + Vec3(0, 0, m_fHeight);
				arrPortVertsOtherSide[2] = arrOutAreaPoint[!nEven] + Vec3(0, 0, m_fHeight);
				arrPortVertsOtherSide[3] = arrOutAreaPoint[!nEven];
				barrPortVertsOtherSideValid = true;
			}
			else
			{
				// something wrong
				Warning("CVisArea::PreRender: Invalid portal: %s", m_pVisAreaColdData->m_sName);
				return;
			}
		}
		else if (!pParent && vPortNorm.z == 0 && m_lstConnections.Count() == 1)
		{
			// basic entrance portal
			Vec3 vBorder = (vPortNorm.Cross(Vec3(0, 0, 1.f))).GetNormalized() * fRadius;
			arrPortVerts[0] = vCenter - Vec3(0, 0, 1.f) * fRadius - vBorder;
			arrPortVerts[1] = vCenter + Vec3(0, 0, 1.f) * fRadius - vBorder;
			arrPortVerts[2] = vCenter + Vec3(0, 0, 1.f) * fRadius + vBorder;
			arrPortVerts[3] = vCenter - Vec3(0, 0, 1.f) * fRadius + vBorder;
		}
		else if (!pParent && vPortNorm.z != 0 && m_lstConnections.Count() == 1)
		{
			// up/down entrance portal
			Vec3 vBorder = (vPortNorm.Cross(Vec3(0, 1, 0.f))).GetNormalized() * fRadius;
			arrPortVerts[0] = vCenter - Vec3(0, 1, 0.f) * fRadius + vBorder;
			arrPortVerts[1] = vCenter + Vec3(0, 1, 0.f) * fRadius + vBorder;
			arrPortVerts[2] = vCenter + Vec3(0, 1, 0.f) * fRadius - vBorder;
			arrPortVerts[3] = vCenter - Vec3(0, 1, 0.f) * fRadius - vBorder;
		}
		else
		{
			// something wrong or areabox portal - use simple solution
			if (vPortNorm.IsEquivalent(Vec3(0, 0, 0), VEC_EPSILON))
				vPortNorm = (vCenter - passInfo.GetCamera().GetPosition()).GetNormalized();

			Vec3 vBorder = (vPortNorm.Cross(Vec3(0, 0, 1.f))).GetNormalized() * fRadius;
			arrPortVerts[0] = vCenter - Vec3(0, 0, 1.f) * fRadius - vBorder;
			arrPortVerts[1] = vCenter + Vec3(0, 0, 1.f) * fRadius - vBorder;
			arrPortVerts[2] = vCenter + Vec3(0, 0, 1.f) * fRadius + vBorder;
			arrPortVerts[3] = vCenter - Vec3(0, 0, 1.f) * fRadius + vBorder;
		}

		//if (GetCVars()->e_Portals == 4) // make color recursion dependent
			//GetRenderer()->SetMaterialColor(1, 1, passInfo.IsGeneralPass(), 1);

		Vec3 vPortalFaceCenter = (arrPortVerts[0] + arrPortVerts[1] + arrPortVerts[2] + arrPortVerts[3]) / 4;
		vPortToCamDir = CurCamera.GetPosition() - vPortalFaceCenter;
		if (vPortToCamDir.GetNormalized().Dot(vPortNorm) < -0.01f)
		{
			UpdatePortalBlendInfo(passInfo);
			return;
		}

		const bool Upright = (fabsf(vPortNorm.z) < FLT_EPSILON);
		CCamera camParent = CurCamera;

		// clip portal quad by camera planes
		PodArray<Vec3>& lstPortVertsClipped = s_tmpLstPortVertsClipped;
		lstPortVertsClipped.Clear();
		lstPortVertsClipped.AddList(arrPortVerts, 4);
		ClipPortalVerticesByCameraFrustum(&lstPortVertsClipped, camParent);

		AABB aabb;
		aabb.Reset();

		if (lstPortVertsClipped.Count() > 2)
		{
			// find screen space bounds of clipped portal
			for (int i = 0; i < lstPortVertsClipped.Count(); i++)
			{
				Vec3 vSS;
				GetRenderer()->ProjectToScreen(lstPortVertsClipped[i].x, lstPortVertsClipped[i].y, lstPortVertsClipped[i].z, &vSS.x, &vSS.y, &vSS.z);
				vSS.y = 100 - vSS.y;
				aabb.Add(vSS);
			}
		}
		else
		{
			if (!CVisArea::IsSphereInsideVisArea(CurCamera.GetPosition(), CurCamera.GetNearPlane()))
				bCanSeeThruThisArea = false;
		}

		if (lstPortVertsClipped.Count() > 2 && aabb.min.z > 0.01f)
		{
			PodArray<Vec3>& lstPortVertsSS = s_tmpLstPortVertsSS;
			lstPortVertsSS.PreAllocate(4, 4);

			// get 3d positions of portal bounds
			{
				const float w = float(CurCamera.GetViewSurfaceX());
				const float h = float(CurCamera.GetViewSurfaceZ());
				const float d = 0.01f;

				int i = 0;
				GetRenderer()->UnProjectFromScreen(aabb.min.x * w / 100, aabb.min.y * h / 100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z);
				i++;
				GetRenderer()->UnProjectFromScreen(aabb.min.x * w / 100, aabb.max.y * h / 100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z);
				i++;
				GetRenderer()->UnProjectFromScreen(aabb.max.x * w / 100, aabb.max.y * h / 100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z);
				i++;
				GetRenderer()->UnProjectFromScreen(aabb.max.x * w / 100, aabb.min.y * h / 100, d, &lstPortVertsSS[i].x, &lstPortVertsSS[i].y, &lstPortVertsSS[i].z);
				i++;

				CurCamera.m_ScissorInfo.x1 = uint16(CLAMP(aabb.min.x * w / 100, 0, w));
				CurCamera.m_ScissorInfo.y1 = uint16(CLAMP(aabb.min.y * h / 100, 0, h));
				CurCamera.m_ScissorInfo.x2 = uint16(CLAMP(aabb.max.x * w / 100, 0, w));
				CurCamera.m_ScissorInfo.y2 = uint16(CLAMP(aabb.max.y * h / 100, 0, h));
			}

			if (GetCVars()->e_Portals == 4)
				for (int i = 0; i < lstPortVertsSS.Count(); i++)
				{
					float farrColor[4] = { float((nReqursionLevel & 1) > 0), float((nReqursionLevel & 2) > 0), float((nReqursionLevel & 4) > 0), 1 };
					ColorF c(farrColor[0], farrColor[1], farrColor[2], farrColor[3]);
					DrawSphere(lstPortVertsSS[i], 0.002f, c);
					IRenderAuxText::DrawLabelExF(lstPortVertsSS[i], 0.1f, farrColor, false, true, "%d", i);
				}

			UpdatePortalCameraPlanes(CurCamera, lstPortVertsSS.GetElements(), Upright, passInfo);

			bCanSeeThruThisArea =
			  (CurCamera.m_ScissorInfo.x1 < CurCamera.m_ScissorInfo.x2) &&
			  (CurCamera.m_ScissorInfo.y1 < CurCamera.m_ScissorInfo.y2);
		}

		if (m_bUseDeepness && bCanSeeThruThisArea && barrPortVertsOtherSideValid)
		{
			Vec3 vOtherSideBoxMax = SetMinBB();
			Vec3 vOtherSideBoxMin = SetMaxBB();
			for (int i = 0; i < 4; i++)
			{
				vOtherSideBoxMin.CheckMin(arrPortVertsOtherSide[i] - Vec3(0.01f, 0.01f, 0.01f));
				vOtherSideBoxMax.CheckMax(arrPortVertsOtherSide[i] + Vec3(0.01f, 0.01f, 0.01f));
			}

			bCanSeeThruThisArea = CurCamera.IsAABBVisible_E(AABB(vOtherSideBoxMin, vOtherSideBoxMax));
		}

		if (bCanSeeThruThisArea && pParent && m_lstConnections.Count() == 1)
		{
			// set this camera for outdoor
			if (nReqursionLevel >= 1)
			{
				if (!m_bSkyOnly)
				{
					if (plstOutPortCameras)
					{
						plstOutPortCameras->Add(CurCamera);
						plstOutPortCameras->Last().m_pPortal = this;
					}
					if (pbOutdoorVisible)
						*pbOutdoorVisible = true;
				}
				else if (pbSkyVisible)
					*pbSkyVisible = true;
			}

			UpdatePortalBlendInfo(passInfo);
			return;
		}
	}

	// sort portals by distance
	if (!m_bThisIsPortal && m_lstConnections.Count())
	{
		for (int p = 0; p < m_lstConnections.Count(); p++)
		{
			CVisArea* pNeibVolume = m_lstConnections[p];
			pNeibVolume->m_fDistance = CurCamera.GetPosition().GetDistance((pNeibVolume->m_boxArea.min + pNeibVolume->m_boxArea.max) * 0.5f);
		}

		qsort(&m_lstConnections[0], m_lstConnections.Count(),
		      sizeof(m_lstConnections[0]), CVisAreaManager__CmpDistToPortal);
	}

	if (m_bOceanVisible && pbOceanVisible)
		*pbOceanVisible = true;

	// recurse to connections
	for (int p = 0; p < m_lstConnections.Count(); p++)
	{
		CVisArea* pNeibVolume = m_lstConnections[p];
		if (pNeibVolume != pParent)
		{
			if (!m_bThisIsPortal)
			{
				// skip far portals
				float fRadius = (pNeibVolume->m_boxArea.max - pNeibVolume->m_boxArea.min).GetLength() * 0.5f * GetFloatCVar(e_ViewDistRatioPortals) / 60.f;
				if (pNeibVolume->m_fDistance * passInfo.GetZoomFactor() > fRadius * pNeibVolume->m_fViewDistRatio)
					continue;

				Vec3 vPortNorm = (this == pNeibVolume->m_lstConnections[0] || pNeibVolume->m_lstConnections.Count() == 1) ?
				                 pNeibVolume->m_vConnNormals[0] : pNeibVolume->m_vConnNormals[1];

				// back face check
				Vec3 vPortToCamDir = CurCamera.GetPosition() - pNeibVolume->GetAABBox()->GetCenter();
				if (vPortToCamDir.Dot(vPortNorm) < 0)
					continue;
			}

			if (bCanSeeThruThisArea && (m_bThisIsPortal || CurCamera.IsAABBVisible_F(pNeibVolume->m_boxStatics)))
				pNeibVolume->PreRender(nReqursionLevel - 1, CurCamera, this, pCurPortal, pbOutdoorVisible, plstOutPortCameras, pbSkyVisible, pbOceanVisible, lstVisibleAreas, passInfo);
		}
	}

	if (m_bThisIsPortal)
		UpdatePortalBlendInfo(passInfo);
}

//! return list of visareas connected to specified visarea (can return portals and sectors)
int CVisArea::GetRealConnections(IVisArea** pAreas, int nMaxConnNum, bool bSkipDisabledPortals)
{
	int nOut = 0;
	for (int nArea = 0; nArea < m_lstConnections.Count(); nArea++)
	{
		if (nOut < nMaxConnNum)
			pAreas[nOut] = (IVisArea*)m_lstConnections[nArea];
		nOut++;
	}
	return nOut;
}

//! return list of sectors conected to specified sector or portal (returns sectors only)
// todo: change the way it returns data
int CVisArea::GetVisAreaConnections(IVisArea** pAreas, int nMaxConnNum, bool bSkipDisabledPortals)
{
	int nOut = 0;
	if (IsPortal())
	{
		/*		for(int nArea=0; nArea<m_lstConnections.Count(); nArea++)
		    {
		      if(nOut<nMaxConnNum)
		        pAreas[nOut] = (IVisArea*)m_lstConnections[nArea];
		      nOut++;
		    }*/
		return min(nMaxConnNum, GetRealConnections(pAreas, nMaxConnNum, bSkipDisabledPortals));
	}
	else
	{
		for (int nPort = 0; nPort < m_lstConnections.Count(); nPort++)
		{
			CVisArea* pPortal = m_lstConnections[nPort];
			assert(pPortal->IsPortal());
			for (int nArea = 0; nArea < pPortal->m_lstConnections.Count(); nArea++)
			{
				if (pPortal->m_lstConnections[nArea] != this)
					if (!bSkipDisabledPortals || pPortal->IsActive())
					{
						if (nOut < nMaxConnNum)
							pAreas[nOut] = (IVisArea*)pPortal->m_lstConnections[nArea];
						nOut++;
						break; // take first valid connection
					}
			}
		}
	}

	return min(nMaxConnNum, nOut);
}

bool CVisArea::IsPortalValid()
{
	int nCount = m_lstConnections.Count();
	if (nCount > 2 || nCount == 0)
		return false;

	for (int i = 0; i < nCount; i++)
	{
		if (m_vConnNormals[i].IsEquivalent(Vec3(0, 0, 0), VEC_EPSILON))
			return false;
	}

	if (nCount > 1)
		if (m_vConnNormals[0].Dot(m_vConnNormals[1]) > -0.99f)
			return false;

	return true;
}

bool CVisArea::IsPortalIntersectAreaInValidWay(CVisArea* pPortal)
{
	const Vec3& v1Min = pPortal->m_boxArea.min;
	const Vec3& v1Max = pPortal->m_boxArea.max;
	const Vec3& v2Min = m_boxArea.min;
	const Vec3& v2Max = m_boxArea.max;

	if (v1Max.x > v2Min.x && v2Max.x > v1Min.x)
		if (v1Max.y > v2Min.y && v2Max.y > v1Min.y)
			if (v1Max.z > v2Min.z && v2Max.z > v1Min.z)
			{
				// vertical portal
				for (int v = 0; v < m_lstShapePoints.Count(); v++)
				{
					int nIntersNum = 0;
					bool arrIntResult[4] = { 0, 0, 0, 0 };
					for (int p = 0; p < pPortal->m_lstShapePoints.Count() && p < 4; p++)
					{
						const Vec3& v0 = m_lstShapePoints[v];
						const Vec3& v1 = m_lstShapePoints[(v + 1) % m_lstShapePoints.Count()];
						const Vec3& p0 = pPortal->m_lstShapePoints[p];
						const Vec3& p1 = pPortal->m_lstShapePoints[(p + 1) % pPortal->m_lstShapePoints.Count()];

						if (Is2dLinesIntersect(v0.x, v0.y, v1.x, v1.y, p0.x, p0.y, p1.x, p1.y))
						{
							nIntersNum++;
							arrIntResult[p] = true;
						}
					}
					if (nIntersNum == 2 && arrIntResult[0] == arrIntResult[2] && arrIntResult[1] == arrIntResult[3])
						return true;
				}

				// horisontal portal
				{
					int nBottomPoints = 0, nUpPoints = 0;
					for (int p = 0; p < pPortal->m_lstShapePoints.Count() && p < 4; p++)
						if (IsPointInsideVisArea(pPortal->m_lstShapePoints[p]))
							nBottomPoints++;

					for (int p = 0; p < pPortal->m_lstShapePoints.Count() && p < 4; p++)
						if (IsPointInsideVisArea(pPortal->m_lstShapePoints[p] + Vec3(0, 0, pPortal->m_fHeight)))
							nUpPoints++;

					if (nBottomPoints == 0 && nUpPoints == 4)
						return true;

					if (nBottomPoints == 4 && nUpPoints == 0)
						return true;
				}
			}

	return false;
}

/*
   void CVisArea::SetTreeId(int nTreeId)
   {
   if(m_nTreeId == nTreeId)
    return;

   m_nTreeId = nTreeId;

   for(int p=0; p<m_lstConnections.Count(); p++)
    m_lstConnections[p]->SetTreeId(nTreeId);
   }
 */
bool CVisArea::IsShapeClockwise()
{
	float fClockWise =
	  (m_lstShapePoints[0].x - m_lstShapePoints[1].x) * (m_lstShapePoints[2].y - m_lstShapePoints[1].y) -
	  (m_lstShapePoints[0].y - m_lstShapePoints[1].y) * (m_lstShapePoints[2].x - m_lstShapePoints[1].x);

	return fClockWise > 0;
}

void CVisArea::DrawAreaBoundsIntoCBuffer(CCullBuffer* pCBuffer)
{
	assert(!"temprary not supported");

	/*  if(m_lstShapePoints.Count()!=4)
	    return;

	   Vec3 arrVerts[8];
	   int arrIndices[24];

	   int v=0;
	   int i=0;
	   for(int p=0; p<4 && p<m_lstShapePoints.Count(); p++)
	   {
	    arrVerts[v++] = m_lstShapePoints[p];
	    arrVerts[v++] = m_lstShapePoints[p] + Vec3(0,0,m_fHeight);

	    arrIndices[i++] = (p*2+0)%8;
	    arrIndices[i++] = (p*2+1)%8;
	    arrIndices[i++] = (p*2+2)%8;
	    arrIndices[i++] = (p*2+3)%8;
	    arrIndices[i++] = (p*2+2)%8;
	    arrIndices[i++] = (p*2+1)%8;
	   }

	   Matrix34 mat;
	   mat.SetIdentity();
	   pCBuffer->AddMesh(arrVerts,8,arrIndices,24,&mat);*/
}

void CVisArea::ClipPortalVerticesByCameraFrustum(PodArray<Vec3>* pPolygon, const CCamera& cam)
{
	Plane planes[5] = {
		*cam.GetFrustumPlane(FR_PLANE_RIGHT),
		*cam.GetFrustumPlane(FR_PLANE_LEFT),
		*cam.GetFrustumPlane(FR_PLANE_TOP),
		*cam.GetFrustumPlane(FR_PLANE_BOTTOM),
		*cam.GetFrustumPlane(FR_PLANE_NEAR)
	};

	const PodArray<Vec3>& clipped = s_tmpClipContext.Clip(*pPolygon, planes, 4);

	pPolygon->Clear();
	pPolygon->AddList(clipped);
}

void CVisArea::GetMemoryUsage(ICrySizer* pSizer)
{
	// pSizer->AddContainer(m_lstEntities[STATIC_OBJECTS]);
	// pSizer->AddContainer(m_lstEntities[DYNAMIC_OBJECTS]);

	if (IsObjectsTreeValid())
	{
		SIZER_COMPONENT_NAME(pSizer, "IndoorObjectsTree");
		GetObjectsTree()->GetMemoryUsage(pSizer);
	}

	// for(int nStatic=0; nStatic<2; nStatic++)
	//   for(int i=0; i<m_lstEntities[nStatic].Count(); i++)
	//     nSize += m_lstEntities[nStatic][i].pNode->GetMemoryUsage();

	pSizer->AddObject(this, sizeof(*this));
}

void CVisArea::UpdateOcclusionFlagInTerrain()
{
	if (m_bAffectedByOutLights && !m_bThisIsPortal)
	{
		Vec3 vCenter = GetAABBox()->GetCenter();
		if (vCenter.z < GetTerrain()->GetZApr(vCenter.x, vCenter.y))
		{
			AABB box = *GetAABBox();
			box.min.z = 0;
			box.min -= Vec3(8, 8, 8);
			box.max.z = 16000;
			box.max += Vec3(8, 8, 8);
			PodArray<CTerrainNode*>& lstResult = s_tmpLstTerrainNodeResult;
			lstResult.Clear();
			GetTerrain()->IntersectWithBox(box, &lstResult);
			for (int i = 0; i < lstResult.Count(); i++)
				if (lstResult[i]->m_nTreeLevel <= 2)
					lstResult[i]->m_bNoOcclusion = true;
		}
	}
}

void CVisArea::AddConnectedAreas(PodArray<CVisArea*>& lstAreas, int nMaxRecursion)
{
	if (lstAreas.Find(this) < 0)
	{
		lstAreas.Add(this);

		// add connected areas
		if (nMaxRecursion)
		{
			nMaxRecursion--;

			for (int p = 0; p < m_lstConnections.Count(); p++)
			{
				m_lstConnections[p]->AddConnectedAreas(lstAreas, nMaxRecursion);
			}
		}
	}
}

bool CVisArea::GetDistanceThruVisAreas(AABB vCurBoxIn, IVisArea* pTargetArea, const AABB& targetBox, int nMaxReqursion, float& fResDist)
{
	return GetDistanceThruVisAreasReq(vCurBoxIn, 0, pTargetArea, targetBox, nMaxReqursion, fResDist, NULL, s_nGetDistanceThruVisAreasCallCounter++);
}

float DistanceAABB(const AABB& aBox, const AABB& bBox)
{
	float result = 0;

	for (int i = 0; i < 3; ++i)
	{
		const float& aMin = aBox.min[i];
		const float& aMax = aBox.max[i];
		const float& bMin = bBox.min[i];
		const float& bMax = bBox.max[i];

		if (aMin > bMax)
		{
			const float delta = bMax - aMin;

			result += delta * delta;
		}
		else if (bMin > aMax)
		{
			const float delta = aMax - bMin;

			result += delta * delta;
		}
		// else the projection intervals overlap.
	}

	return sqrt(result);
}

bool CVisArea::GetDistanceThruVisAreasReq(AABB vCurBoxIn, float fCurDistIn, IVisArea* pTargetArea, const AABB& targetBox, int nMaxReqursion, float& fResDist, CVisArea* pPrevArea, int nCallID)
{
	if (pTargetArea == this || (pTargetArea == NULL && IsConnectedToOutdoor()))
	{
		// target area is found
		fResDist = min(fResDist, fCurDistIn + DistanceAABB(vCurBoxIn, targetBox));
		return true;
	}

	// if we already visited this area and last time input distance was smaller - makes no sence to continue
	if (nCallID == m_nGetDistanceThruVisAreasLastCallID)
		if (fCurDistIn >= m_fGetDistanceThruVisAreasMinDistance)
			return false;

	m_nGetDistanceThruVisAreasLastCallID = nCallID;
	m_fGetDistanceThruVisAreasMinDistance = fCurDistIn;

	fResDist = FLT_MAX;

	bool bFound = false;

	if (nMaxReqursion > 1)
	{
		for (int p = 0; p < m_lstConnections.Count(); p++)
		{
			if ((m_lstConnections[p] != pPrevArea) && m_lstConnections[p]->IsActive())
			{
				AABB vCurBox = vCurBoxIn;
				float fCurDist = fCurDistIn;
				float dist = FLT_MAX;

				if (IsPortal())
				{
					vCurBox = vCurBoxIn;
					fCurDist = fCurDistIn;
				}
				else
				{
					vCurBox = *m_lstConnections[p]->GetAABBox();
					fCurDist = fCurDistIn + DistanceAABB(vCurBox, vCurBoxIn);
				}

				if (m_lstConnections[p]->GetDistanceThruVisAreasReq(vCurBox, fCurDist, pTargetArea, targetBox, nMaxReqursion - 1, dist, this, nCallID))
				{
					bFound = true;
					fResDist = min(fResDist, dist);
				}
			}
		}
	}

	return bFound;
}

void CVisArea::OffsetPosition(const Vec3& delta)
{
	m_boxArea.Move(delta);
	m_boxStatics.Move(delta);
	for (int i = 0; i < m_lstShapePoints.Count(); i++)
	{
		m_lstShapePoints[i] += delta;
	}
	if (IsObjectsTreeValid())
	{
		GetObjectsTree()->OffsetObjects(delta);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool InsidePolygon(const Vec3* __restrict polygon, int N, const Vec3& p)
{
	int counter = 0;
	int i;
	float xinters;
	const Vec3* p1 = &polygon[0];

	for (i = 1; i <= N; i++)
	{
		const Vec3* p2 = &polygon[i % N];
		if (p.y > min(p1->y, p2->y))
		{
			if (p.y <= max(p1->y, p2->y))
			{
				if (p.x <= max(p1->x, p2->x))
				{
					if (p1->y != p2->y)
					{
						xinters = (p.y - p1->y) * (p2->x - p1->x) / (p2->y - p1->y) + p1->x;
						if (p1->x == p2->x || p.x <= xinters)
							counter++;
					}
				}
			}
		}
		p1 = p2;
	}

	if (counter % 2 == 0)
		return(false);
	else
		return(true);
}

///////////////////////////////////////////////////////////////////////////////
bool InsideSpherePolygon(Vec3* polygon, int N, Sphere& S)
{
	int i;
	Vec3 p1, p2;

	p1 = polygon[0];
	for (i = 1; i <= N; i++)
	{
		p2 = polygon[i % N];
		Vec3 vA(p1 - S.center);
		Vec3 vD(p2 - p1);
		vA.z = vD.z = 0;
		vD.NormalizeSafe();
		float fB = vD.Dot(vA);
		float fC = vA.Dot(vA) - S.radius * S.radius;
		if (fB * fB >= fC)     //at least 1 root
			return(true);
		p1 = p2;
	}

	return(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace
{
	inline void HelperGetNearestCubeProbe(PodArray<CVisArea*>& arrayVisArea, float& fMinDistance, int& nMaxPriority, CLightEntity*& pNearestLight, const AABB* pBBox)
	{
		int s = arrayVisArea.Count();
		for (int i = 0; i < s; ++i)
		{
			if (arrayVisArea[i]->IsObjectsTreeValid())
			{
				if (!pBBox || Overlap::AABB_AABB(*arrayVisArea[i]->GetAABBox(), *pBBox))
				{
					arrayVisArea[i]->GetObjectsTree()->GetNearestCubeProbe(fMinDistance, nMaxPriority, pNearestLight, pBBox);
				}
			}
		}
	}

	inline void HelperGetObjectsByType(PodArray<CVisArea*>& arrayVisArea, PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, bool* pInstStreamReady, uint64 dwFlags)
	{		
		int s = arrayVisArea.Count();
		for (int i = 0; i < s; ++i)
		{
			if (arrayVisArea[i]->IsObjectsTreeValid())
			{
				if (!pBBox || Overlap::AABB_AABB(*arrayVisArea[i]->GetAABBox(), *pBBox))
				{
					arrayVisArea[i]->GetObjectsTree()->GetObjectsByType(lstObjects, objType, pBBox, pInstStreamReady, dwFlags);
				}
			}
		}
	}

	inline int HelperGetObjectsCount(PodArray<CVisArea*>& arrayVisArea, EOcTeeNodeListType eListType)
	{
		int nCount = 0;
		int s = arrayVisArea.Count();
		for (int i = 0; i < s; ++i)
		{
			if (arrayVisArea[i]->IsObjectsTreeValid())
			{
				nCount += arrayVisArea[i]->GetObjectsTree()->GetObjectsCount(eListType);
			}
		}
		return nCount;
	}

	inline void HelperGetStreamedInNodesNum(PodArray<CVisArea*>& arrayVisArea, int& nAllStreamable, int& nReady)
	{
		int s = arrayVisArea.Count();
		for (int dwI = 0; dwI < s; ++dwI)
		{
			if (arrayVisArea[dwI]->IsObjectsTreeValid())
			{
				arrayVisArea[dwI]->GetObjectsTree()->GetStreamedInNodesNum(nAllStreamable, nReady);
			}
		}
	}

}

void CVisAreaManager::GetNearestCubeProbe(float& fMinDistance, int& nMaxPriority, CLightEntity*& pNearestLight, const AABB* pBBox)
{
	HelperGetNearestCubeProbe(m_lstVisAreas, fMinDistance, nMaxPriority, pNearestLight, pBBox);
	HelperGetNearestCubeProbe(m_lstPortals,  fMinDistance, nMaxPriority, pNearestLight, pBBox);
}

///////////////////////////////////////////////////////////////////////////////
void CVisAreaManager::GetObjectsByType(PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, bool* pInstStreamReady, uint64 dwFlags)
{
	HelperGetObjectsByType(m_lstVisAreas, lstObjects, objType, pBBox, pInstStreamReady, dwFlags);
	HelperGetObjectsByType(m_lstPortals,  lstObjects, objType, pBBox, pInstStreamReady, dwFlags);
}

///////////////////////////////////////////////////////////////////////////////
int CVisAreaManager::GetObjectsCount(EOcTeeNodeListType eListType)
{
	return HelperGetObjectsCount(m_lstVisAreas, eListType) + HelperGetObjectsCount(m_lstPortals, eListType);
}

///////////////////////////////////////////////////////////////////////////////
void CVisAreaManager::GetStreamedInNodesNum(int& nAllStreamable, int& nReady)
{
	HelperGetStreamedInNodesNum(m_lstVisAreas, nAllStreamable, nReady);
	HelperGetStreamedInNodesNum(m_lstPortals,  nAllStreamable, nReady);
}

///////////////////////////////////////////////////////////////////////////////
bool CVisAreaManager::IsOccludedByOcclVolumes(const AABB& objBox, const SRenderingPassInfo& passInfo, bool bCheckOnlyIndoorVolumes)
{
	FUNCTION_PROFILER_3DENGINE;

	PodArray<CVisArea*>& rList = bCheckOnlyIndoorVolumes ? m_lstIndoorActiveOcclVolumes : m_lstActiveOcclVolumes;

	for (int i = 0; i < rList.Count(); i++)
	{
		CCamera* pCamera = rList[i]->m_arrOcclCamera[passInfo.GetRecursiveLevel()];
		bool bAllIn = 0;
		if (pCamera)
		{
			// reduce code size by skipping many cache lookups in the camera functions as well as the camera constructor
			CRY_ALIGN(128) char bufCamera[sizeof(CCamera)];
			memcpy(bufCamera, pCamera, sizeof(CCamera));
			if (alias_cast<CCamera*>(bufCamera)->IsAABBVisible_EH(objBox, &bAllIn))
				if (bAllIn)
					return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
IVisArea* CVisAreaManager::GetVisAreaFromPos(const Vec3& vPos)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pAABBTree)
	{
		UpdateAABBTree();
	}

	return m_pAABBTree->FindVisarea(vPos);
}

///////////////////////////////////////////////////////////////////////////////
bool CVisArea::IsBoxOverlapVisArea(const AABB& objBox)
{
	if (!Overlap::AABB_AABB(objBox, m_boxArea))
		return false;

	PodArray<Vec3>& polygonA = s_tmpPolygonA;
	polygonA.Clear();
	polygonA.Add(Vec3(objBox.min.x, objBox.min.y, objBox.min.z));
	polygonA.Add(Vec3(objBox.min.x, objBox.max.y, objBox.min.z));
	polygonA.Add(Vec3(objBox.max.x, objBox.max.y, objBox.min.z));
	polygonA.Add(Vec3(objBox.max.x, objBox.min.y, objBox.min.z));
	return Overlap::Polygon_Polygon2D<PodArray<Vec3>>(polygonA, m_lstShapePoints);
}

#define PORTAL_GEOM_BBOX_EXTENT 1.5f

///////////////////////////////////////////////////////////////////////////////
void CVisArea::UpdateGeometryBBox()
{
	m_boxStatics.max = m_boxArea.max;
	m_boxStatics.min = m_boxArea.min;

	if (IsPortal())
	{
		// fix for big objects passing portal
		m_boxStatics.max += Vec3(PORTAL_GEOM_BBOX_EXTENT, PORTAL_GEOM_BBOX_EXTENT, PORTAL_GEOM_BBOX_EXTENT);
		m_boxStatics.min -= Vec3(PORTAL_GEOM_BBOX_EXTENT, PORTAL_GEOM_BBOX_EXTENT, PORTAL_GEOM_BBOX_EXTENT);
	}

	if (IsObjectsTreeValid())
	{
		PodArray<IRenderNode*> lstObjects;
		GetObjectsTree()->GetObjectsByType(lstObjects, eERType_Brush, NULL);

		for (int i = 0; i < lstObjects.Count(); i++)
		{
			AABB aabb;
			lstObjects[i]->FillBBox(aabb);
			m_boxStatics.Add(aabb);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void CVisArea::UpdateClipVolume()
{
	if (m_lstShapePoints.Count() < 3)
		return;

	const int nPoints = m_lstShapePoints.size();
	const int nVertexCount = nPoints * 2;
	const int nIndexCount = (2 * nPoints + 2 * (nPoints - 2)) * 3; // 2*nPoints for sides, nPoints-2 for top and nPoints-2 for bottom

	m_pClipVolumeMesh = NULL;

	if (nPoints < 3)
		return;
	if (!gEnv->pRenderer)
		return;

	std::vector<SVF_P3F_C4B_T2F> vertices;
	std::vector<vtx_idx> indices;

	vertices.resize(nVertexCount);
	indices.resize(nIndexCount);

	std::vector<Vec2> triangulationPoints;
	triangulationPoints.resize(nPoints + 1);
	MARK_UNUSED triangulationPoints[nPoints].x;

	for (int i = 0; i < nPoints; ++i)
	{
		int nPointIdx = IsShapeClockwise() ? nPoints - 1 - i : i;

		vertices[i].xyz = m_lstShapePoints[nPointIdx];
		vertices[i].color.dcolor = 0xFFFFFFFF;
		vertices[i].st = Vec2(ZERO);

		vertices[i + nPoints].xyz = m_lstShapePoints[nPointIdx] + Vec3(0, 0, m_fHeight);
		vertices[i + nPoints].color.dcolor = 0xFFFFFFFF;
		vertices[i + nPoints].st = Vec2(ZERO);

		triangulationPoints[i] = Vec2(m_lstShapePoints[nPointIdx]);
	}

	// triangulate shape first
	std::vector<int> triangleIndices;
	triangleIndices.resize((nPoints - 2) * 3);
	MARK_UNUSED triangleIndices[triangleIndices.size() - 1];

	int nTris = gEnv->pPhysicalWorld->GetPhysUtils()->TriangulatePoly(&triangulationPoints[0], triangulationPoints.size(), &triangleIndices[0], triangleIndices.size());

	if (nTris == nPoints - 2) // triangulation success?
	{
		// top and bottom faces
		size_t nCurIndex = 0;
		for (; nCurIndex < triangleIndices.size(); nCurIndex += 3)
		{
			indices[nCurIndex + 2] = triangleIndices[nCurIndex + 0];
			indices[nCurIndex + 1] = triangleIndices[nCurIndex + 1];
			indices[nCurIndex + 0] = triangleIndices[nCurIndex + 2];

			indices[triangleIndices.size() + nCurIndex + 0] = triangleIndices[nCurIndex + 0] + nPoints;
			indices[triangleIndices.size() + nCurIndex + 1] = triangleIndices[nCurIndex + 1] + nPoints;
			indices[triangleIndices.size() + nCurIndex + 2] = triangleIndices[nCurIndex + 2] + nPoints;
		}

		nCurIndex = 2 * triangleIndices.size();

		// side faces
		for (int i = 0; i < nPoints; i++)
		{
			vtx_idx bl = i;
			vtx_idx br = (i + 1) % nPoints;

			indices[nCurIndex++] = bl;
			indices[nCurIndex++] = br + nPoints;
			indices[nCurIndex++] = bl + nPoints;

			indices[nCurIndex++] = bl;
			indices[nCurIndex++] = br;
			indices[nCurIndex++] = br + nPoints;
		}

		m_pClipVolumeMesh = gEnv->pRenderer->CreateRenderMeshInitialized(&vertices[0], vertices.size(),
			EDefaultInputLayouts::P3F_C4B_T2F, &indices[0], indices.size(), prtTriangleList,
			"ClipVolume", GetName(), eRMT_Dynamic);
	}
}

void CVisArea::GetClipVolumeMesh(_smart_ptr<IRenderMesh>& renderMesh, Matrix34& worldTM) const
{
	renderMesh = m_pClipVolumeMesh;
	worldTM = Matrix34(IDENTITY);
}

uint CVisArea::GetClipVolumeFlags() const
{
	int nFlags = IClipVolume::eClipVolumeIsVisArea;
	nFlags |= IsConnectedToOutdoor() ? IClipVolume::eClipVolumeConnectedToOutdoor : 0;
	nFlags |= IsAffectedByOutLights() ? IClipVolume::eClipVolumeAffectedBySun : 0;
	nFlags |= IsIgnoringGI() ? IClipVolume::eClipVolumeIgnoreGI : 0;
	nFlags |= IsIgnoringOutdoorAO() ? IClipVolume::eClipVolumeIgnoreOutdoorAO : 0;

	return nFlags;
}

///////////////////////////////////////////////////////////////////////////////
void CVisArea::UpdatePortalBlendInfo(const SRenderingPassInfo& passInfo)
{
	if (m_bThisIsPortal && m_lstConnections.Count() > 0 && m_nStencilRef>0 && GetCVars()->e_PortalsBlend > 0 && m_fPortalBlending > 0)
	{
		IClipVolume* blendVolumes[SDeferredClipVolume::MaxBlendInfoCount];
		Plane blendPlanes[SDeferredClipVolume::MaxBlendInfoCount];

		Vec3 vPlanePoints[2][3];
		uint nPointCount[2] = { 0, 0 };

		for (int p = 0; p < m_lstShapePoints.Count(); ++p)
		{
			Vec3 vTestPoint = m_lstShapePoints[p] + Vec3(0, 0, m_fHeight * 0.5f);
			int nVisAreaIndex = (m_lstConnections[0] && m_lstConnections[0]->IsPointInsideVisArea(vTestPoint)) ? 0 : 1;

			if (nPointCount[nVisAreaIndex] < 2)
			{
				vPlanePoints[nVisAreaIndex][nPointCount[nVisAreaIndex]] = m_lstShapePoints[p];
				nPointCount[nVisAreaIndex]++;
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (nPointCount[i] == 2)
			{
				if (IsShapeClockwise())
					std::swap(vPlanePoints[i][0], vPlanePoints[i][1]);

				blendPlanes[i] = Plane::CreatePlane(vPlanePoints[i][0], vPlanePoints[i][0] + Vec3(0, 0, m_fHeight), vPlanePoints[i][1]);
				blendVolumes[i] = i < m_lstConnections.Count() ? m_lstConnections[i] : NULL;

				// make sure plane normal points inside portal
				if (nPointCount[(i + 1) % 2] > 0)
				{
					Vec3 c(ZERO);
					for (uint j = 0; j < nPointCount[(i + 1) % 2]; ++j)
						c += vPlanePoints[(i + 1) % 2][j];
					c /= (float)nPointCount[(i + 1) % 2];

					if (blendPlanes[i].DistFromPlane(c) < 0)
					{
						blendPlanes[i].n = -blendPlanes[i].n;
						blendPlanes[i].d = -blendPlanes[i].d;
					}
				}
			}
			else
			{
				blendPlanes[i] = Plane(Vec3(ZERO), 0);
				blendVolumes[i] = NULL;
			}
		}

		// weight planes by user specified importance. works because we renormalize in the shader
		float planeWeight = clamp_tpl(m_fPortalBlending, 1e-5f, 1.f - 1e-5f);

		blendPlanes[0].n *= planeWeight;
		blendPlanes[0].d *= planeWeight;
		blendPlanes[1].n *= 1 - planeWeight;
		blendPlanes[1].d *= 1 - planeWeight;

		passInfo.GetIRenderView()->SetClipVolumeBlendInfo(this, SDeferredClipVolume::MaxBlendInfoCount, blendVolumes, blendPlanes);
	}
}

///////////////////////////////////////////////////////////////////////////////
namespace
{
	void HelperInsertObject(CVisArea* pVisArea, IRenderNode* pEnt, const AABB& objBox, const float fObjRadiusSqr, Vec3& vEntCenter)
	{
		if (!pVisArea->IsObjectsTreeValid())
		{
			pVisArea->SetObjectsTree(COctreeNode::Create(pVisArea->m_boxArea, pVisArea));
		}
		pVisArea->GetObjectsTree()->InsertObject(pEnt, objBox, fObjRadiusSqr, vEntCenter);
	}
}

bool CVisAreaManager::SetEntityArea(IRenderNode* pEnt, const AABB& objBox, const float fObjRadiusSqr)
{
	assert(pEnt);

	Vec3 vEntCenter = Get3DEngine()->GetEntityRegisterPoint(pEnt);

	// find portal containing object center
	CVisArea* pVisArea = NULL;

	const int kNumPortals = m_lstPortals.Count();
	for (int v = 0; v < kNumPortals; v++)
	{
		CVisArea* pCurrentPortal = m_lstPortals[v];
		PrefetchLine(pCurrentPortal, 256);
		PrefetchLine(pCurrentPortal, 384);
		if (pCurrentPortal->IsPointInsideVisArea(vEntCenter))
		{
			pVisArea = pCurrentPortal;
			HelperInsertObject(pVisArea, pEnt, objBox, fObjRadiusSqr, vEntCenter);
			break;
		}
	}

	if (!pVisArea && pEnt->m_dwRndFlags & ERF_REGISTER_BY_BBOX)
	{
		AABB aabb;
		pEnt->FillBBox(aabb);

		for (int v = 0; v < kNumPortals; v++)
		{
			CVisArea* pCurrentPortal = m_lstPortals[v];
			PrefetchLine(pCurrentPortal, 256);
			PrefetchLine(pCurrentPortal, 384);
			if (pCurrentPortal->IsBoxOverlapVisArea(aabb))
			{
				pVisArea = pCurrentPortal;
				HelperInsertObject(pVisArea, pEnt, objBox, fObjRadiusSqr, vEntCenter);
				break;
			}
		}
	}

	if (!pVisArea) // if portal not found - find volume
	{
		const int kNumVisAreas = m_lstVisAreas.Count();
		for (int v = 0; v < kNumVisAreas; v++)
		{
			CVisArea* pCurrentVisArea = m_lstVisAreas[v];
			PrefetchLine(pCurrentVisArea, 256);
			PrefetchLine(pCurrentVisArea, 384);
			if (pCurrentVisArea->IsPointInsideVisArea(vEntCenter))
			{
				pVisArea = pCurrentVisArea;
				HelperInsertObject(pVisArea, pEnt, objBox, fObjRadiusSqr, vEntCenter);
				break;
			}
		}
	}

	if (pVisArea && pEnt->GetRenderNodeType() == eERType_Brush) // update bbox of exit portal //			if((*pVisArea)->m_lstConnections.Count()==1)
		if (pVisArea->IsPortal())
			pVisArea->UpdateGeometryBBox();

	return pVisArea != 0;
}

///////////////////////////////////////////////////////////////////////////////
CVisArea* SAABBTreeNode::FindVisarea(const Vec3& vPos)
{
	if (nodeBox.IsContainPoint(vPos))
	{
		if (nodeAreas.Count())
		{
			// leaf
			for (int i = 0; i < nodeAreas.Count(); i++)
				if (nodeAreas[i]->m_bActive && nodeAreas[i]->IsPointInsideVisArea(vPos))
					return nodeAreas[i];
		}
		else
		{
			// node
			for (int i = 0; i < 2; i++)
				if (arrChilds[i])
					if (CVisArea* pArea = arrChilds[i]->FindVisarea(vPos))
						return pArea;
		}
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Current scheme: Will never move sphere center, just clip radius, even to 0.
bool CVisArea::ClipToVisArea(bool bInside, Sphere& sphere, Vec3 const& vNormal)
{
	FUNCTION_PROFILER_3DENGINE;

	/*
	Clip		PointZ	PointXY

	In			In			In					inside, clip Z and XY
	In			In			Out					outside, return 0
	In			Out			In					outside, return 0
	In			Out			Out					outside, return 0

	Out			In			In					inside, return 0
	Out			In			Out					outside, clip XY
	Out			Out			In					outside, clip Z
	Out			Out			Out					outside, clip XY
	*/

	bool bClipXY = false, bClipZ = false;
	if (bInside)
	{
		// Clip to 0 if center outside.
		if (!IsPointInsideVisArea(sphere.center))
		{
			sphere.radius = 0.f;
			return true;
		}
		bClipXY = bClipZ = true;
	}
	else
	{
		if (Overlap::Point_AABB(sphere.center, m_boxArea))
		{
			if (InsidePolygon(m_lstShapePoints.begin(), m_lstShapePoints.size(), sphere.center))
			{
				sphere.radius = 0.f;
				return true;
			}
			else
				bClipXY = true;
		}
		else if (InsidePolygon(m_lstShapePoints.begin(), m_lstShapePoints.size(), sphere.center))
			bClipZ = true;
		else
			bClipXY = true;
	}

	float fOrigRadius = sphere.radius;
	if (bClipZ)
	{
		// Check against vertical planes.
		float fDist = min(abs(m_boxArea.max.z - sphere.center.z), abs(sphere.center.z - m_boxArea.min.z));
		float fRadiusScale = sqrt_tpl(max(1.f - sqr(vNormal.z), 0.f));
		if (fDist < sphere.radius * fRadiusScale)
		{
			sphere.radius = fDist / fRadiusScale;
			if (sphere.radius <= 0.f)
				return true;
		}
	}

	if (bClipXY)
	{
		Vec3 vP1 = m_lstShapePoints[0];
		vP1.z = clamp_tpl(sphere.center.z, m_boxArea.min.z, m_boxArea.max.z);
		for (int n = m_lstShapePoints.Count() - 1; n >= 0; n--)
		{
			Vec3 vP0 = m_lstShapePoints[n];
			vP0.z = vP1.z;

			// Compute nearest vector from center to plane.
			Vec3 vP = vP0 - sphere.center;
			Vec3 vD = vP1 - vP0;
			float fN = -(vP * vD);
			if (fN > 0.f)
			{
				float fD = vD.GetLengthSquared();
				if (fN >= fD)
					vP += vD;
				else
					vP += vD * (fN / fD);
			}

			// Check distance only in planar direction.
			float fDist = vP.GetLength() * 0.99f;
			float fRadiusScale = fDist > 0.f ? sqrt_tpl(max(1.f - sqr(vNormal * vP) / sqr(fDist), 0.f)) : 1.f;
			if (fDist < sphere.radius * fRadiusScale)
			{
				sphere.radius = fDist / fRadiusScale;
				if (sphere.radius <= 0.f)
					return true;
			}

			vP1 = vP0;
		}
	}

	return sphere.radius < fOrigRadius;
}

///////////////////////////////////////////////////////////////////////////////
bool CVisArea::IsPointInsideVisArea(const Vec3& vPos) const
{
	const int kNumPoints = m_lstShapePoints.Count();
	if (kNumPoints)
	{
		Vec3* pLstShapePoints = &m_lstShapePoints[0];
		PrefetchLine(pLstShapePoints, 0);
		if (Overlap::Point_AABB(vPos, m_boxArea))
		{
			if (InsidePolygon(pLstShapePoints, kNumPoints, vPos))
				return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
bool CVisArea::IsSphereInsideVisArea(const Vec3& vPos, const f32 fRadius)
{
	Sphere S(vPos, fRadius);
	if (Overlap::Sphere_AABB(S, m_boxArea))
		if (InsidePolygon(&m_lstShapePoints[0], m_lstShapePoints.Count(), vPos) || InsideSpherePolygon(&m_lstShapePoints[0], m_lstShapePoints.Count(), S))
			return true;

	return false;
}

///////////////////////////////////////////////////////////////////////////////
const AABB* CVisArea::GetAABBox() const
{
	return &m_boxArea;
}

///////////////////////////////////////////////////////////////////////////////
const Vec3 CVisArea::GetFinalAmbientColor()
{
	float fHDRMultiplier = ((CTimeOfDay*)Get3DEngine()->GetTimeOfDay())->GetHDRMultiplier();

	Vec3 vAmbColor;
	if (IsAffectedByOutLights() && !m_bIgnoreSky)
	{
		vAmbColor.x = Get3DEngine()->GetSkyColor().x * m_vAmbientColor.x;
		vAmbColor.y = Get3DEngine()->GetSkyColor().y * m_vAmbientColor.y;
		vAmbColor.z = Get3DEngine()->GetSkyColor().z * m_vAmbientColor.z;
	}
	else
		vAmbColor = m_vAmbientColor * fHDRMultiplier;

	if (GetCVars()->e_Portals == 4)
		vAmbColor = Vec3(fHDRMultiplier, fHDRMultiplier, fHDRMultiplier);

	return vAmbColor;
}

///////////////////////////////////////////////////////////////////////////////
bool CVisArea::IsPortal() const
{
	return m_bThisIsPortal;
}

float CVisArea::CalcSignedArea()
{
	float fArea = 0;
	for (int i = 0; i < m_lstShapePoints.Count(); i++)
	{
		const Vec3& v0 = m_lstShapePoints[i];
		const Vec3& v1 = m_lstShapePoints[(i + 1) % m_lstShapePoints.Count()];
		fArea += v0.x * v1.y - v1.x * v0.y;
	}
	return fArea / 2;
}

void CVisArea::GetShapePoints(const Vec3*& pPoints, size_t& nPoints)
{
	if (m_lstShapePoints.empty())
	{
		pPoints = 0;
		nPoints = 0;
	}
	else
	{
		pPoints = &m_lstShapePoints[0];
		nPoints = m_lstShapePoints.size();
	}
}

float CVisArea::GetHeight()
{
	return m_fHeight;
}

///////////////////////////////////////////////////////////////////////////////
bool CVisArea::FindVisArea(IVisArea* pAnotherArea, int nMaxRecursion, bool bSkipDisabledPortals)
{
	// collect visited areas in order to prevent visiting it again
	StaticDynArray<CVisArea*, 1024> lVisitedParents;

	return FindVisAreaReqursive(pAnotherArea, nMaxRecursion, bSkipDisabledPortals, lVisitedParents);
}

///////////////////////////////////////////////////////////////////////////////
bool CVisArea::FindVisAreaReqursive(IVisArea* pAnotherArea, int nMaxReqursion, bool bSkipDisabledPortals, StaticDynArray<CVisArea*, 1024>& arrVisitedParents)
{
	arrVisitedParents.push_back(this);

	if (pAnotherArea == this)
		return true;

	if (pAnotherArea == NULL && IsConnectedToOutdoor())
		return true;

	bool bFound = false;

	if (nMaxReqursion > 1)
	{
		for (int p = 0; p < m_lstConnections.Count(); p++)
		{
			if (!bSkipDisabledPortals || m_lstConnections[p]->IsActive())
			{
				if (std::find(arrVisitedParents.begin(), arrVisitedParents.end(), m_lstConnections[p]) == arrVisitedParents.end())
				{
					if (m_lstConnections[p]->FindVisAreaReqursive(pAnotherArea, nMaxReqursion - 1, bSkipDisabledPortals, arrVisitedParents))
					{
						bFound = true;
						break;
					}
				}
			}
		}
	}

	return bFound;
}

///////////////////////////////////////////////////////////////////////////////
bool CVisArea::IsConnectedToOutdoor() const
{
	if (IsPortal()) // check if this portal has just one conection
		return m_lstConnections.Count() == 1;

	// find portals with just one conection
	for (int p = 0; p < m_lstConnections.Count(); p++)
	{
		CVisArea* pPortal = m_lstConnections[p];
		if (pPortal->m_lstConnections.Count() == 1)
			return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
