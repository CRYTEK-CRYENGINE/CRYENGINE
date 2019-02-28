// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   decals.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: draw, create decals on the world
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/IStatObj.h>

#include "DecalManager.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "MatMan.h"
#include "terrain.h"
#include "PolygonClipContext.h"
#include "RenderMeshMerger.h"
#include "RenderMeshUtils.h"
#include "VisAreas.h"
#include "DecalRenderNode.h"

#ifndef RENDER_MESH_TEST_DISTANCE
	#define RENDER_MESH_TEST_DISTANCE (0.2f)
#endif

static const int MAX_ASSEMBLE_SIZE = 5;

CDecalManager::CDecalManager()
{
	m_nCurDecal = 0;
	memset(m_arrbActiveDecals, 0, sizeof(m_arrbActiveDecals));
}

CDecalManager::~CDecalManager()
{
	CDecal::ResetStaticData();
}

bool CDecalManager::AdjustDecalPosition(CryEngineDecalInfo& DecalInfo, bool bMakeFatTest)
{
	Matrix34A objMat, objMatInv;
	Matrix33 objRot, objRotInv;

	CStatObj* pEntObject = (CStatObj*)DecalInfo.ownerInfo.GetOwner(objMat);
	if (!pEntObject || !pEntObject->GetRenderMesh() || !pEntObject->GetRenderTrisCount())
		return false;

	objRot = Matrix33(objMat);
	objRot.NoScale(); // No scale.
	objRotInv = objRot;
	objRotInv.Invert();

	float fWorldScale = objMat.GetColumn(0).GetLength(); // GetScale
	float fWorldScaleInv = 1.0f / fWorldScale;

	// transform decal into object space
	objMatInv = objMat;
	objMatInv.Invert();

	// put into normal object space hit direction of projection
	Vec3 vOS_HitDir = objRotInv.TransformVector(DecalInfo.vHitDirection).GetNormalized();

	// put into position object space hit position
	Vec3 vOS_HitPos = objMatInv.TransformPoint(DecalInfo.vPos);
	vOS_HitPos -= vOS_HitDir * RENDER_MESH_TEST_DISTANCE * fWorldScaleInv;

	IMaterial* pMat = DecalInfo.ownerInfo.pRenderNode ? DecalInfo.ownerInfo.pRenderNode->GetMaterial() : NULL;

	Vec3 vOS_OutPos(0, 0, 0), vOS_OutNormal(0, 0, 0), vTmp;
	IRenderMesh* pRM = pEntObject->GetRenderMesh();

	AABB aabbRNode;
	pRM->GetBBox(aabbRNode.min, aabbRNode.max);
	Vec3 vOut(0, 0, 0);
	if (!Intersect::Ray_AABB(Ray(vOS_HitPos, vOS_HitDir), aabbRNode, vOut))
		return false;

	if (!pRM || !pRM->GetVerticesCount())
		return false;

	if (RayRenderMeshIntersection(pRM, vOS_HitPos, vOS_HitDir, vOS_OutPos, vOS_OutNormal, false, 0, pMat))
	{
		// now check that none of decal sides run across edges
		Vec3 srcp = vOS_OutPos + 0.01f * fWorldScaleInv * vOS_OutNormal; /// Rise hit point a little bit above hit plane.
		Vec3 vDecalNormal = vOS_OutNormal;
		float fMaxHitDistance = 0.02f * fWorldScaleInv;

		// get decal directions
		Vec3 vRi(0, 0, 0), vUp(0, 0, 0);
		if (fabs(vOS_OutNormal.Dot(Vec3(0, 0, 1))) > 0.999f)
		{
			// horiz surface
			vRi = Vec3(0, 1, 0);
			vUp = Vec3(1, 0, 0);
		}
		else
		{
			vRi = vOS_OutNormal.Cross(Vec3(0, 0, 1));
			vRi.Normalize();
			vUp = vOS_OutNormal.Cross(vRi);
			vUp.Normalize();
		}

		vRi *= DecalInfo.fSize * 0.65f;
		vUp *= DecalInfo.fSize * 0.65f;

		if (!bMakeFatTest || (
		      RayRenderMeshIntersection(pRM, srcp + vUp, -vDecalNormal, vTmp, vTmp, true, fMaxHitDistance, pMat) &&
		      RayRenderMeshIntersection(pRM, srcp - vUp, -vDecalNormal, vTmp, vTmp, true, fMaxHitDistance, pMat) &&
		      RayRenderMeshIntersection(pRM, srcp + vRi, -vDecalNormal, vTmp, vTmp, true, fMaxHitDistance, pMat) &&
		      RayRenderMeshIntersection(pRM, srcp - vRi, -vDecalNormal, vTmp, vTmp, true, fMaxHitDistance, pMat)))
		{
			DecalInfo.vPos = objMat.TransformPoint(vOS_OutPos + vOS_OutNormal * 0.001f * fWorldScaleInv);
			DecalInfo.vNormal = objRot.TransformVector(vOS_OutNormal);
			return true;
		}
	}
	return false;
}

struct HitPosInfo
{
	HitPosInfo() { memset(this, 0, sizeof(HitPosInfo)); }
	Vec3  vPos, vNormal;
	float fDistance;
};

int __cdecl CDecalManager__CmpHitPos(const void* v1, const void* v2)
{
	HitPosInfo* p1 = (HitPosInfo*)v1;
	HitPosInfo* p2 = (HitPosInfo*)v2;

	if (p1->fDistance > p2->fDistance)
		return 1;
	else if (p1->fDistance < p2->fDistance)
		return -1;

	return 0;
}

bool CDecalManager::RayRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const Vec3& vInDir,
                                              Vec3& vOutPos, Vec3& vOutNormal, bool bFastTest, float fMaxHitDistance, IMaterial* pMat)
{
	SRayHitInfo hitInfo;
	hitInfo.bUseCache = GetCVars()->e_DecalsHitCache != 0;
	hitInfo.bInFirstHit = bFastTest;
	hitInfo.inRay.origin = vInPos;
	hitInfo.inRay.direction = vInDir.GetNormalized();
	hitInfo.inReferencePoint = vInPos;
	hitInfo.fMaxHitDistance = fMaxHitDistance;
	bool bRes = CRenderMeshUtils::RayIntersection(pRenderMesh, hitInfo, pMat);
	vOutPos = hitInfo.vHitPos;
	vOutNormal = hitInfo.vHitNormal;
	return bRes;
}

bool CDecalManager::SpawnHierarchical(const CryEngineDecalInfo& rootDecalInfo, CDecal* pCallerManagedDecal)
{
	// decal on terrain or simple decal on always static object
	if (!rootDecalInfo.ownerInfo.pRenderNode)
		return Spawn(rootDecalInfo, pCallerManagedDecal);

	bool bSuccess = false;

	AABB decalBoxWS;
	float fSize = rootDecalInfo.fSize;
	decalBoxWS.max = rootDecalInfo.vPos + Vec3(fSize, fSize, fSize);
	decalBoxWS.min = rootDecalInfo.vPos - Vec3(fSize, fSize, fSize);

	CStatObj* _pStatObj = NULL;
	Matrix34A entSlotMatrix;
	entSlotMatrix.SetIdentity();
	if (_pStatObj = (CStatObj*)rootDecalInfo.ownerInfo.pRenderNode->GetEntityStatObj(~0, &entSlotMatrix, true))
	{
		if (_pStatObj->m_nFlags & STATIC_OBJECT_COMPOUND)
		{
			if (int nSubCount = _pStatObj->GetSubObjectCount())
			{
				// spawn decals on stat obj sub objects
				CryEngineDecalInfo decalInfo = rootDecalInfo;
				decalInfo.ownerInfo.nRenderNodeSlotId = 0;
				if (rootDecalInfo.ownerInfo.nRenderNodeSlotSubObjectId >= 0)
				{
					decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = rootDecalInfo.ownerInfo.nRenderNodeSlotSubObjectId;
					bSuccess |= Spawn(decalInfo, pCallerManagedDecal);
				}
				else
					for (int nSubId = 0; nSubId < nSubCount; nSubId++)
					{
						IStatObj::SSubObject& subObj = _pStatObj->SubObject(nSubId);
						if (subObj.pStatObj && !subObj.bHidden && subObj.nType == STATIC_SUB_OBJECT_MESH)
						{
							Matrix34 subObjMatrix = entSlotMatrix * subObj.tm;
							AABB subObjAABB = AABB::CreateTransformedAABB(subObjMatrix, subObj.pStatObj->GetAABB());
							if (Overlap::AABB_AABB(subObjAABB, decalBoxWS))
							{
								decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = nSubId;
								bSuccess |= Spawn(decalInfo, pCallerManagedDecal);
							}
						}
					}
			}
		}
		else
		{
			AABB subObjAABB = AABB::CreateTransformedAABB(entSlotMatrix, _pStatObj->GetAABB());
			if (Overlap::AABB_AABB(subObjAABB, decalBoxWS))
			{
				CryEngineDecalInfo decalInfo = rootDecalInfo;
				decalInfo.ownerInfo.nRenderNodeSlotId = 0;
				decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = -1; // no childs
				bSuccess |= Spawn(decalInfo, pCallerManagedDecal);
			}
		}
	}
	else if (ICharacterInstance* pChar = rootDecalInfo.ownerInfo.pRenderNode->GetEntityCharacter(&entSlotMatrix))
	{
		// spawn decals on CGA components
		ISkeletonPose* pSkeletonPose = pChar->GetISkeletonPose();
		uint32 numJoints = pChar->GetIDefaultSkeleton().GetJointCount();
		CryEngineDecalInfo decalInfo = rootDecalInfo;
		decalInfo.ownerInfo.nRenderNodeSlotId = 0;

		if (rootDecalInfo.ownerInfo.nRenderNodeSlotSubObjectId >= 0)
		{
			decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = rootDecalInfo.ownerInfo.nRenderNodeSlotSubObjectId;
			bSuccess |= Spawn(decalInfo, pCallerManagedDecal);
		}
		else
			// spawn decal on every sub-object intersecting decal bbox
			for (uint32 nJointId = 0; nJointId < numJoints; nJointId++)
			{
				IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(nJointId);

				if (pStatObj && !(pStatObj->GetFlags() & STATIC_OBJECT_HIDDEN) && pStatObj->GetRenderMesh())
				{
					assert(!pStatObj->GetSubObjectCount());

					Matrix34 tm34 = entSlotMatrix * Matrix34(pSkeletonPose->GetAbsJointByID(nJointId));
					AABB objBoxWS = AABB::CreateTransformedAABB(tm34, pStatObj->GetAABB());
					//				DrawBBox(objBoxWS);
					//			DrawBBox(decalBoxWS);
					if (Overlap::AABB_AABB(objBoxWS, decalBoxWS))
					{
						decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = nJointId;
						bSuccess |= Spawn(decalInfo, pCallerManagedDecal);
					}
				}
			}
	}

	return bSuccess;
}

bool CDecalManager::Spawn(CryEngineDecalInfo DecalInfo, CDecal* pCallerManagedDecal)
{
	FUNCTION_PROFILER_3DENGINE;

	Vec3 vCamPos = GetSystem()->GetViewCamera().GetPosition();

	// do not spawn if too far
	float fZoom = GetObjManager() ? Get3DEngine()->GetZoomFactor() : 1.f;
	float fDecalDistance = DecalInfo.vPos.GetDistance(vCamPos);
	float fMaxDist = max(GetCVars()->e_ViewDistMin, min(GetFloatCVar(e_ViewDistCompMaxSize), DecalInfo.fSize) * GetCVars()->e_ViewDistRatio * GetCVars()->e_DecalsSpawnDistRatio);
	if (!pCallerManagedDecal && (fDecalDistance > Get3DEngine()->GetMaxViewDistance() || fDecalDistance * fZoom > fMaxDist))
		return false;

	int overlapCount(0);
	int targetSize(0);
	int overlapIds[MAX_ASSEMBLE_SIZE];

	// do not spawn new decals if they could overlap the existing and similar ones
	if (!pCallerManagedDecal && !GetCVars()->e_DecalsOverlapping && DecalInfo.fSize && !DecalInfo.bSkipOverlappingTest)
	{
		for (int i = 0; i < DECAL_COUNT; i++)
		{
			if (m_arrbActiveDecals[i])
			{
				// skip overlapping check if decals are very different in size
				if ((m_arrDecals[i].m_iAssembleSize > 0) == DecalInfo.bAssemble)
				{
					float fSizeRatio = m_arrDecals[i].m_fWSSize / DecalInfo.fSize;
					if (((m_arrDecals[i].m_iAssembleSize > 0) || (fSizeRatio > 0.5f && fSizeRatio < 2.f)) && m_arrDecals[i].m_nGroupId != DecalInfo.nGroupId)
					{
						float fDist = m_arrDecals[i].m_vWSPos.GetSquaredDistance(DecalInfo.vPos);
						if (fDist < sqr(m_arrDecals[i].m_fWSSize * 0.5f + DecalInfo.fSize * 0.5f) && (DecalInfo.vNormal.Dot(m_arrDecals[i].m_vFront) > 0.f))
						{
							if (DecalInfo.bAssemble && m_arrDecals[i].m_iAssembleSize < MAX_ASSEMBLE_SIZE)
							{
								if (overlapCount < MAX_ASSEMBLE_SIZE)
								{
									overlapIds[overlapCount] = i;
									overlapCount++;
								}
								else
								{
									m_arrbActiveDecals[i] = false;
								}
							}
							else
								return true;
						}
					}
				}
			}
		}
	}

	float fAssembleSizeModifier(1.0f);
	if (DecalInfo.bAssemble)
	{
		Vec3 avgPos(0.0f, 0.0f, 0.0f);
		int validAssembles(0);
		for (int i = 0; i < overlapCount; i++)
		{
			int id = overlapIds[i];

			float fDist = m_arrDecals[id].m_vWSPos.GetSquaredDistance(DecalInfo.vPos);
			float minDist = sqr(m_arrDecals[id].m_fWSSize * 0.4f);
			if (fDist > minDist)
			{
				avgPos += m_arrDecals[id].m_vWSPos;
				targetSize += m_arrDecals[id].m_iAssembleSize;
				validAssembles++;
			}
		}

		if (overlapCount && !validAssembles)
			return true;

		for (int i = 0; i < overlapCount; i++)
		{
			int id = overlapIds[i];
			m_arrbActiveDecals[id] = false;
		}

		++validAssembles;
		++targetSize;
		avgPos += DecalInfo.vPos;

		if (targetSize > 1)
		{
			avgPos /= float(validAssembles);
			DecalInfo.vPos = avgPos;
			targetSize = min(targetSize, MAX_ASSEMBLE_SIZE);

			const float sizetable[MAX_ASSEMBLE_SIZE] = { 1.0f, 1.5f, 2.3f, 3.5f, 3.5f };
			const char sValue[2] = { char('0' + targetSize), 0 };
			cry_strcat(DecalInfo.szMaterialName, sValue);
			fAssembleSizeModifier = sizetable[targetSize - 1];
		}
	}

	if (GetCVars()->e_Decals > 1)
		DrawSphere(DecalInfo.vPos, DecalInfo.fSize);

	// update lifetime for near decals under control by the decal manager
	if (!pCallerManagedDecal)
	{
		if (DecalInfo.fSize > 1 && GetCVars()->e_DecalsNeighborMaxLifeTime)
		{
			// force near decals to fade faster
			float fCurrTime = GetTimer()->GetCurrTime();
			for (int i = 0; i < DECAL_COUNT; i++)
				if (m_arrbActiveDecals[i] && m_arrDecals[i].m_nGroupId != DecalInfo.nGroupId)
				{
					if (m_arrDecals[i].m_vWSPos.GetSquaredDistance(DecalInfo.vPos) < sqr(m_arrDecals[i].m_fWSSize / 1.5f + DecalInfo.fSize / 2.0f))
						if ((m_arrDecals[i]).m_fLifeBeginTime < fCurrTime - 0.1f)
							if (m_arrDecals[i].m_fLifeTime > GetCVars()->e_DecalsNeighborMaxLifeTime)
								if (m_arrDecals[i].m_fLifeTime < 10000) // decals spawn by cut scenes need to stay
									m_arrDecals[i].m_fLifeTime = GetCVars()->e_DecalsNeighborMaxLifeTime;
				}
		}

		// loop position in array
		m_nCurDecal = (m_nCurDecal + 1) & (DECAL_COUNT - 1);
		//if(m_nCurDecal>=DECAL_COUNT)
		//	m_nCurDecal=0;
	}

	// create reference to decal which is to be filled
	CDecal& newDecal(pCallerManagedDecal ? *pCallerManagedDecal : m_arrDecals[m_nCurDecal]);

	newDecal.m_bDeferred = DecalInfo.bDeferred;

	newDecal.m_iAssembleSize = targetSize;
	// free old pRM
	newDecal.FreeRenderData();

	newDecal.m_nGroupId = DecalInfo.nGroupId;

	// get material if specified
	newDecal.m_pMaterial = 0;

	if (DecalInfo.szMaterialName[0] != '0')
	{
		newDecal.m_pMaterial = GetMatMan()->LoadMaterial(DecalInfo.szMaterialName, false, true);
		if (!newDecal.m_pMaterial)
		{
			newDecal.m_pMaterial = GetMatMan()->LoadMaterial("Materials/Decals/Default", true, true);
			newDecal.m_pMaterial->AddRef();
			Warning("CDecalManager::Spawn: Specified decal material \"%s\" not found!\n", DecalInfo.szMaterialName);
		}
	}
	else
		Warning("CDecalManager::Spawn: Decal material name is not specified");

	newDecal.m_sortPrio = DecalInfo.sortPrio;

	// set up user defined decal basis if provided
	bool useDefinedUpRight(false);
	Vec3 userDefinedUp;
	Vec3 userDefinedRight;
	if (DecalInfo.pExplicitRightUpFront)
	{
		userDefinedRight = DecalInfo.pExplicitRightUpFront->GetColumn(0);
		userDefinedUp = DecalInfo.pExplicitRightUpFront->GetColumn(1);
		DecalInfo.vNormal = DecalInfo.pExplicitRightUpFront->GetColumn(2);
		useDefinedUpRight = true;
	}

	// just in case
	DecalInfo.vNormal.NormalizeSafe();

	// remember object we need to follow
	newDecal.m_ownerInfo.nRenderNodeSlotId = DecalInfo.ownerInfo.nRenderNodeSlotId;
	newDecal.m_ownerInfo.nRenderNodeSlotSubObjectId = DecalInfo.ownerInfo.nRenderNodeSlotSubObjectId;

	newDecal.m_vWSPos = DecalInfo.vPos;
	newDecal.m_fWSSize = DecalInfo.fSize * fAssembleSizeModifier;

	// If owner entity and object is specified - make decal use entity geometry
	float _fObjScale = 1.f;

	Matrix34A _objMat;
	Matrix33 worldRot;
	IStatObj* pStatObj = DecalInfo.ownerInfo.GetOwner(_objMat);
	if (pStatObj)
	{
		worldRot = Matrix33(_objMat);
		_objMat.Invert();
	}

	float fWrapMinSize = GetFloatCVar(e_DecalsDeferredDynamicMinSize);

	EERType ownerRenderNodeType = (DecalInfo.ownerInfo.pRenderNode) ? DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() : eERType_NotRenderNode;

	bool bSpawnOnVegetationWithBending = false;
	if (ownerRenderNodeType == eERType_Vegetation && DecalInfo.fSize > fWrapMinSize && !DecalInfo.bDeferred)
	{
		//    CVegetation * pVeg = (CVegetation*)DecalInfo.ownerInfo.pRenderNode;
		//    StatInstGroup & rGroup = GetObjManager()->m_lstStaticTypes[pVeg->m_nObjectTypeID];
		//  if(rGroup.fBending>0)
		bSpawnOnVegetationWithBending = true;

		// Check owner material ID (need for decals vertex modifications)
		SRayHitInfo hitInfo;
		memset(&hitInfo, 0, sizeof(hitInfo));
		Vec3 vHitPos = _objMat.TransformPoint(DecalInfo.vPos);
		// put hit normal into the object space
		Vec3 vRayDir = DecalInfo.vHitDirection.GetNormalized() * worldRot;
		hitInfo.inReferencePoint = vHitPos;
		hitInfo.inRay.origin = vHitPos - vRayDir * 4.0f;
		hitInfo.inRay.direction = vRayDir;
		hitInfo.bInFirstHit = false;
		hitInfo.bUseCache = true;
		if (pStatObj->RayIntersection(hitInfo, 0))
			newDecal.m_ownerInfo.nMatID = hitInfo.nHitMatID;
	}

	if (DecalInfo.ownerInfo.pRenderNode && DecalInfo.ownerInfo.nRenderNodeSlotId >= 0 && (DecalInfo.fSize > fWrapMinSize || pCallerManagedDecal || bSpawnOnVegetationWithBending) && !DecalInfo.bDeferred)
	{
		newDecal.m_eDecalType = eDecalType_OS_OwnersVerticesUsed;

		IRenderMesh* pSourceRenderMesh = NULL;

		if (pStatObj)
			pSourceRenderMesh = pStatObj->GetRenderMesh();

		if (!pSourceRenderMesh)
			return false;

		// transform decal into object space
		Matrix33 objRotInv = Matrix33(_objMat);
		objRotInv.NoScale();

		if (useDefinedUpRight)
		{
			userDefinedRight = objRotInv.TransformVector(userDefinedRight).GetNormalized();
			userDefinedUp = objRotInv.TransformVector(userDefinedUp).GetNormalized();
			assert(fabsf(DecalInfo.vNormal.Dot(-DecalInfo.vHitDirection.GetNormalized()) - 1.0f) < 1e-4f);
		}

		// make decals smaller but longer if hit direction is near perpendicular to surface normal
		float fSizeModificator = 0.25f + 0.75f * fabs(DecalInfo.vHitDirection.GetNormalized().Dot(DecalInfo.vNormal));

		// put into normal object space hit direction of projection
		DecalInfo.vNormal = -objRotInv.TransformVector((DecalInfo.vHitDirection - DecalInfo.vNormal * 0.25f).GetNormalized());

		if (!DecalInfo.vNormal.IsZero())
			DecalInfo.vNormal.Normalize();

		// put into position object space hit position
		DecalInfo.vPos = _objMat.TransformPoint(DecalInfo.vPos);

		// find object scale
		Vec3 vTest(0, 0, 1.f);
		vTest = _objMat.TransformVector(vTest);
		float fObjScale = 1.0f / vTest.len();

		if (fObjScale < 0.01f)
			return false;

		// transform size into object space
		DecalInfo.fSize /= fObjScale;

		DecalInfo.fSize *= (DecalInfo.bAssemble ? fAssembleSizeModifier : fSizeModificator);

		if (DecalInfo.bForceEdge)
		{
			SRayHitInfo hitInfo;
			hitInfo.bUseCache = GetCVars()->e_DecalsHitCache != 0;
			hitInfo.bInFirstHit = false;
			hitInfo.inRay.origin = DecalInfo.vPos + DecalInfo.vNormal;
			hitInfo.inRay.direction = -DecalInfo.vNormal;
			hitInfo.inReferencePoint = DecalInfo.vPos + DecalInfo.vNormal;
			hitInfo.inRetTriangle = true;
			CRenderMeshUtils::RayIntersection(pSourceRenderMesh, hitInfo, pStatObj ? pStatObj->GetMaterial() : NULL);

			MoveToEdge(pSourceRenderMesh, DecalInfo.fSize, hitInfo.vHitPos, hitInfo.vHitNormal, hitInfo.vTri0, hitInfo.vTri1, hitInfo.vTri2);
			DecalInfo.vPos = hitInfo.vHitPos;
			DecalInfo.vNormal = hitInfo.vHitNormal;
		}

		// make decal geometry
		newDecal.m_pRenderMesh = MakeBigDecalRenderMesh(pSourceRenderMesh, DecalInfo.vPos, DecalInfo.fSize, DecalInfo.vNormal, newDecal.m_pMaterial, pStatObj ? pStatObj->GetMaterial() : NULL);

		if (!newDecal.m_pRenderMesh)
			return false; // no geometry found
	}
	else if (!DecalInfo.ownerInfo.pRenderNode && DecalInfo.ownerInfo.pDecalReceivers && (DecalInfo.fSize > fWrapMinSize || pCallerManagedDecal) && !DecalInfo.bDeferred)
	{
		newDecal.m_eDecalType = eDecalType_WS_Merged;

		assert(!newDecal.m_pRenderMesh);

		// put into normal hit direction of projection
		DecalInfo.vNormal = -DecalInfo.vHitDirection;
		if (!DecalInfo.vNormal.IsZero())
			DecalInfo.vNormal.Normalize();

		Vec3 vSize(DecalInfo.fSize * 1.333f, DecalInfo.fSize * 1.333f, DecalInfo.fSize * 1.333f);
		AABB decalAABB(DecalInfo.vPos - vSize, DecalInfo.vPos + vSize);

		// build list of affected brushes
		PodArray<SRenderMeshInfoInput> lstRMI;
		for (int nObj = 0; nObj < DecalInfo.ownerInfo.pDecalReceivers->Count(); nObj++)
		{
			IRenderNode* pDecalOwner = DecalInfo.ownerInfo.pDecalReceivers->Get(nObj)->pNode;
			Matrix34A objMat;
			if (IStatObj* pEntObject = pDecalOwner->GetEntityStatObj(0, &objMat))
			{
				SRenderMeshInfoInput rmi;
				rmi.pMesh = pEntObject->GetRenderMesh();
				rmi.pMat = pEntObject->GetMaterial();
				rmi.mat = objMat;

				if (rmi.pMesh)
				{
					AABB transAABB = AABB::CreateTransformedAABB(rmi.mat, pEntObject->GetAABB());
					if (Overlap::AABB_AABB(decalAABB, transAABB))
						lstRMI.Add(rmi);
				}
				else if (int nSubObjCount = pEntObject->GetSubObjectCount())
				{
					// multi sub objects
					for (int nSubObj = 0; nSubObj < nSubObjCount; nSubObj++)
					{
						IStatObj::SSubObject* pSubObj = pEntObject->GetSubObject(nSubObj);
						if (pSubObj->pStatObj)
						{
							rmi.pMesh = pSubObj->pStatObj->GetRenderMesh();
							rmi.pMat = pSubObj->pStatObj->GetMaterial();
							rmi.mat = objMat * pSubObj->tm;
							if (rmi.pMesh)
							{
								AABB transAABB = AABB::CreateTransformedAABB(rmi.mat, pSubObj->pStatObj->GetAABB());
								if (Overlap::AABB_AABB(decalAABB, transAABB))
									lstRMI.Add(rmi);
							}
						}
					}
				}
			}
		}

		if (!lstRMI.Count())
			return false;

		SDecalClipInfo DecalClipInfo;
		DecalClipInfo.vPos = DecalInfo.vPos;
		DecalClipInfo.fRadius = DecalInfo.fSize;
		DecalClipInfo.vProjDir = DecalInfo.vNormal;

		PodArray<SRenderMeshInfoOutput> outRenderMeshes;
		CRenderMeshMerger Merger;
		SMergeInfo info;
		info.sMeshName = "MergedDecal";
		info.sMeshType = "MergedDecal";
		info.pDecalClipInfo = &DecalClipInfo;
		info.vResultOffset = DecalInfo.vPos;
		newDecal.m_pRenderMesh = Merger.MergeRenderMeshes(lstRMI.GetElements(), lstRMI.Count(), outRenderMeshes, info);

		if (!newDecal.m_pRenderMesh)
			return false; // no geometry found

		assert(newDecal.m_pRenderMesh->GetChunks().size() == 1);
	}
	else if (ownerRenderNodeType == eERType_MovableBrush || ownerRenderNodeType == eERType_Vegetation)
	{
		newDecal.m_eDecalType = eDecalType_OS_SimpleQuad;

		Matrix34A objMat;

		// transform decal from world space into entity space
		IStatObj* pEntObject = DecalInfo.ownerInfo.GetOwner(objMat);
		if (!pEntObject)
			return false;
		assert(pEntObject);
		objMat.Invert();

		if (useDefinedUpRight)
		{
			userDefinedRight = objMat.TransformVector(userDefinedRight).GetNormalized();
			userDefinedUp = objMat.TransformVector(userDefinedUp).GetNormalized();
			assert(fabsf(DecalInfo.vNormal.Dot(-DecalInfo.vHitDirection.GetNormalized()) - 1.0f) < 1e-4f);
		}

		DecalInfo.vNormal = objMat.TransformVector(DecalInfo.vNormal).GetNormalized();
		DecalInfo.vPos = objMat.TransformPoint(DecalInfo.vPos);

		// find object scale
		if (DecalInfo.ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Vegetation)
			_fObjScale = ((CVegetation*)(DecalInfo.ownerInfo.pRenderNode))->GetScale();
		else
		{
			Vec3 vTest(0, 0, 1.f);
			vTest = objMat.TransformVector(vTest);
			_fObjScale = 1.f / vTest.len();
		}

		DecalInfo.fSize /= _fObjScale;
	}
	else
	{
		CTerrain* pTerrain = GetTerrain();
		if (!DecalInfo.preventDecalOnGround && DecalInfo.fSize > (fWrapMinSize * 2.f) && !DecalInfo.ownerInfo.pRenderNode &&
		    (DecalInfo.vPos.z - pTerrain->GetZApr(DecalInfo.vPos.x, DecalInfo.vPos.y)) < DecalInfo.fSize && !DecalInfo.bDeferred)
		{
			newDecal.m_eDecalType = eDecalType_WS_OnTheGround;

			float unitSize = CTerrain::GetHeightMapUnitSize();
			float x1 = std::floor((DecalInfo.vPos.x - DecalInfo.fSize) / unitSize) * unitSize - unitSize;
			float x2 = std::floor((DecalInfo.vPos.x + DecalInfo.fSize) / unitSize) * unitSize + unitSize;
			float y1 = std::floor((DecalInfo.vPos.y - DecalInfo.fSize) / unitSize) * unitSize - unitSize;
			float y2 = std::floor((DecalInfo.vPos.y + DecalInfo.fSize) / unitSize) * unitSize + unitSize;

			for (float x = x1; x <= x2; x += CTerrain::GetHeightMapUnitSize())
			{
				for (float y = y1; y <= y2; y += CTerrain::GetHeightMapUnitSize())
				{
					if (pTerrain->GetHole(x, y))
					{
						return false;
					}
				}
			}
		}
		else
			newDecal.m_eDecalType = eDecalType_WS_SimpleQuad;

		DecalInfo.ownerInfo.pRenderNode = NULL;
	}

	// spawn
	if (!useDefinedUpRight)
	{
		if (DecalInfo.vNormal.Dot(Vec3(0, 0, 1)) > 0.999f)
		{
			// floor
			newDecal.m_vRight = Vec3(0, 1, 0);
			newDecal.m_vUp = Vec3(-1, 0, 0);
		}
		else if (DecalInfo.vNormal.Dot(Vec3(0, 0, -1)) > 0.999f)
		{
			// ceil
			newDecal.m_vRight = Vec3(1, 0, 0);
			newDecal.m_vUp = Vec3(0, -1, 0);
		}
		else if (!DecalInfo.vNormal.IsZero())
		{
			newDecal.m_vRight = DecalInfo.vNormal.Cross(Vec3(0, 0, 1));
			newDecal.m_vRight.Normalize();
			newDecal.m_vUp = DecalInfo.vNormal.Cross(newDecal.m_vRight);
			newDecal.m_vUp.Normalize();
		}

		// rotate vectors
		if (!DecalInfo.vNormal.IsZero())
		{
			AngleAxis rotation(DecalInfo.fAngle, DecalInfo.vNormal);
			newDecal.m_vRight = rotation * newDecal.m_vRight;
			newDecal.m_vUp = rotation * newDecal.m_vUp;
		}
	}
	else
	{
		newDecal.m_vRight = userDefinedRight;
		newDecal.m_vUp = userDefinedUp;
	}

	newDecal.m_vFront = DecalInfo.vNormal;

	newDecal.m_vPos = DecalInfo.vPos;
	newDecal.m_vPos += DecalInfo.vNormal * 0.001f / _fObjScale;

	newDecal.m_fSize = DecalInfo.fSize;
	newDecal.m_fLifeTime = DecalInfo.fLifeTime * GetCVars()->e_DecalsLifeTimeScale;
	assert(!DecalInfo.pIStatObj); // not used -> not supported
	newDecal.m_vAmbient = Get3DEngine()->GetAmbientColorFromPosition(newDecal.m_vWSPos);

	newDecal.m_ownerInfo.pRenderNode = DecalInfo.ownerInfo.pRenderNode;
	if (DecalInfo.ownerInfo.pRenderNode)
		DecalInfo.ownerInfo.pRenderNode->m_nInternalFlags |= IRenderNode::DECAL_OWNER;

	newDecal.m_fGrowTime = DecalInfo.fGrowTime;
	newDecal.m_fGrowTimeAlpha = DecalInfo.fGrowTimeAlpha;
	newDecal.m_fLifeBeginTime = GetTimer()->GetCurrTime();

	if (DecalInfo.pIStatObj && !pCallerManagedDecal)
	{
		//assert(!"Geometry decals neede to be re-debugged");
		DecalInfo.pIStatObj->AddRef();
	}

	if (!pCallerManagedDecal)
	{
		m_arrbActiveDecals[m_nCurDecal] = true;
		++m_nCurDecal;
	}

#ifdef _DEBUG
	if (newDecal.m_ownerInfo.pRenderNode)
	{
		cry_strcpy(newDecal.m_decalOwnerEntityClassName, newDecal.m_ownerInfo.pRenderNode->GetEntityClassName());
		cry_strcpy(newDecal.m_decalOwnerName, newDecal.m_ownerInfo.pRenderNode->GetName());
		newDecal.m_decalOwnerType = newDecal.m_ownerInfo.pRenderNode->GetRenderNodeType();
	}
	else
	{
		newDecal.m_decalOwnerEntityClassName[0] = '\0';
		newDecal.m_decalOwnerName[0] = '\0';
		newDecal.m_decalOwnerType = eERType_NotRenderNode;
	}
#endif

	return true;
}

void CDecalManager::Update(const float fFrameTime)
{
	CryPrefetch(&m_arrbActiveDecals[0]);
	CryPrefetch(&m_arrbActiveDecals[128]);
	CryPrefetch(&m_arrbActiveDecals[256]);
	CryPrefetch(&m_arrbActiveDecals[384]);

	for (int i = 0; i < DECAL_COUNT; i++)
	{
		if (m_arrbActiveDecals[i])
		{
			IRenderNode* pRenderNode = m_arrDecals[i].m_ownerInfo.pRenderNode;
			if (m_arrDecals[i].Update(m_arrbActiveDecals[i], fFrameTime))
				if (pRenderNode && m_arrTempUpdatedOwners.Find(pRenderNode) < 0)
					m_arrTempUpdatedOwners.Add(pRenderNode);
		}
	}

	for (int i = 0; i < m_arrTempUpdatedOwners.Count(); i++)
	{
		m_arrTempUpdatedOwners[i]->m_nInternalFlags &= ~IRenderNode::UPDATE_DECALS;
	}

	m_arrTempUpdatedOwners.Clear();
}

void CDecalManager::Render(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!passInfo.RenderDecals() || !GetObjManager())
		return;

	float fCurrTime = GetTimer()->GetCurrTime();
	float fZoom = passInfo.GetZoomFactor();
	float fWaterLevel = m_p3DEngine->GetWaterLevel();

	static int nLastUpdateStreamingPrioriryRoundId = 0;
	bool bPrecacheMaterial = nLastUpdateStreamingPrioriryRoundId != GetObjManager()->m_nUpdateStreamingPrioriryRoundId;
	nLastUpdateStreamingPrioriryRoundId = GetObjManager()->m_nUpdateStreamingPrioriryRoundId;

	static int nLastUpdateStreamingPrioriryRoundIdFast = 0;
	bool bPrecacheMaterialFast = nLastUpdateStreamingPrioriryRoundIdFast != GetObjManager()->m_nUpdateStreamingPrioriryRoundIdFast;
	nLastUpdateStreamingPrioriryRoundIdFast = GetObjManager()->m_nUpdateStreamingPrioriryRoundIdFast;

	const CCamera& rCamera = passInfo.GetCamera();

	// draw
	for (int i = 0; i < DECAL_COUNT; i++)
		if (m_arrbActiveDecals[i])
		{
			CDecal* pDecal = &m_arrDecals[i];
			pDecal->m_vWSPos = pDecal->GetWorldPosition();
			float fDist = rCamera.GetPosition().GetDistance(pDecal->m_vWSPos) * fZoom;
			float fMaxViewDist = max(GetCVars()->e_ViewDistMin, min(GetFloatCVar(e_ViewDistCompMaxSize), pDecal->m_fWSSize) * GetCVars()->e_ViewDistRatio * GetCVars()->e_ViewDistRatioModifierGameDecals);
			if (fDist < fMaxViewDist)
				if (rCamera.IsSphereVisible_F(Sphere(pDecal->m_vWSPos, pDecal->m_fWSSize)))
				{
					bool bAfterWater = CObjManager::IsAfterWater(pDecal->m_vWSPos, rCamera.GetPosition(), passInfo, fWaterLevel);
					if (pDecal->m_pMaterial)
					{
						if (passInfo.IsGeneralPass())
						{
							if (bPrecacheMaterialFast && (fDist < GetFloatCVar(e_StreamPredictionMinFarZoneDistance)))
							{
								if (CMatInfo* pMatInfo = (CMatInfo*)(IMaterial*)pDecal->m_pMaterial)
									pMatInfo->PrecacheMaterial(fDist, NULL, true);
							}

							if (bPrecacheMaterial)
							{
								pDecal->m_vAmbient = Get3DEngine()->GetAmbientColorFromPosition(pDecal->m_vWSPos);
								if (CMatInfo* pMatInfo = (CMatInfo*)(IMaterial*)pDecal->m_pMaterial)
									pMatInfo->PrecacheMaterial(fDist, NULL, false);
							}
						}

						// TODO: take entity orientation into account
						Vec3 vSize(pDecal->m_fWSSize, pDecal->m_fWSSize, pDecal->m_fWSSize);
						AABB aabb(pDecal->m_vWSPos - vSize, pDecal->m_vWSPos + vSize);

						float fDistFading = SATURATE((1.f - fDist / fMaxViewDist) * DIST_FADING_FACTOR);
						pDecal->Render(fCurrTime, bAfterWater, fDistFading, fDist, passInfo);

						if (GetCVars()->e_Decals > 1)
						{
							Vec3 vCenter = pDecal->m_vWSPos;
							AABB aabbCenter(vCenter - vSize * 0.05f, vCenter + vSize * 0.05f);

							DrawBBox(aabb);
							DrawBBox(aabbCenter, Col_Yellow);

							Vec3 vNormal(Vec3(pDecal->m_vUp).Cross(-pDecal->m_vRight).GetNormalized());

							Matrix34A objMat;
							IStatObj* pEntObject = pDecal->m_ownerInfo.GetOwner(objMat);
							if (pEntObject)
								vNormal = objMat.TransformVector(vNormal).GetNormalized();

							DrawLine(vCenter, vCenter + vNormal * pDecal->m_fWSSize);

							if (pDecal->m_pRenderMesh)
							{
								pDecal->m_pRenderMesh->GetBBox(aabb.min, aabb.max);
								DrawBBox(aabb, Col_Red);
							}
						}
					}
				}
		}
}

//////////////////////////////////////////////////////////////////////////
void CDecalManager::OnEntityDeleted(IRenderNode* pRenderNode)
{
	FUNCTION_PROFILER_3DENGINE;

	// remove decals of this entity
	for (int i = 0; i < DECAL_COUNT; i++)
	{
		if (m_arrbActiveDecals[i])
		{
			if (m_arrDecals[i].m_ownerInfo.pRenderNode == pRenderNode)
			{
				if (GetCVars()->e_Decals == 2)
				{
					CDecal& decal = m_arrDecals[i];
					Vec3 vPos = decal.GetWorldPosition();
					char* szOwnerName = "none";
#ifdef _DEBUG
					szOwnerName = decal.m_decalOwnerName;
#endif
					PrintMessage("Debug: C3DEngine::OnDecalDeleted: Pos=(%.1f,%.1f,%.1f) Size=%.2f DecalMaterial=%s OwnerName=%s",
					             vPos.x, vPos.y, vPos.z, decal.m_fSize, decal.m_pMaterial ? decal.m_pMaterial->GetName() : "none", szOwnerName);
				}

				m_arrbActiveDecals[i] = false;
				m_arrDecals[i].FreeRenderData();
			}
		}
	}

	// update decal render nodes
	PodArray<IRenderNode*> lstObjects;
	Get3DEngine()->GetObjectsByTypeGlobal(lstObjects, eERType_Decal, NULL);

	if (Get3DEngine()->GetVisAreaManager())
		Get3DEngine()->GetVisAreaManager()->GetObjectsByType(lstObjects, eERType_Decal, NULL);

	for (int i = 0; i < lstObjects.Count(); i++)
		((CDecalRenderNode*)lstObjects[i])->RequestUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CDecalManager::OnRenderMeshDeleted(IRenderMesh* pRenderMesh)
{
	// remove decals of this entity
	for (int i = 0; i < DECAL_COUNT; i++)
	{
		if (m_arrbActiveDecals[i])
		{
			if (
			  (m_arrDecals[i].m_ownerInfo.pRenderNode && (
			     m_arrDecals[i].m_ownerInfo.pRenderNode->GetRenderMesh(0) == pRenderMesh ||
			     m_arrDecals[i].m_ownerInfo.pRenderNode->GetRenderMesh(1) == pRenderMesh ||
			     m_arrDecals[i].m_ownerInfo.pRenderNode->GetRenderMesh(2) == pRenderMesh))
			  ||
			  (m_arrDecals[i].m_pRenderMesh && m_arrDecals[i].m_pRenderMesh->GetVertexContainer() == pRenderMesh)
			  )
			{
				m_arrbActiveDecals[i] = false;
				m_arrDecals[i].FreeRenderData();
				//				PrintMessage("CDecalManager::OnRenderMeshDeleted succseed");
			}
		}
	}
}

void CDecalManager::MoveToEdge(IRenderMesh* pRM, const float fRadius, Vec3& vOutPos, Vec3& vOutNormal, const Vec3& vTri0, const Vec3& vTri1, const Vec3& vTri2)
{
	FUNCTION_PROFILER_3DENGINE;

	AABB boxRM;
	pRM->GetBBox(boxRM.min, boxRM.max);
	Sphere sphere(vOutPos, fRadius);
	if (!Overlap::Sphere_AABB(sphere, boxRM))
		return;

	// get position offset and stride
	int nPosStride = 0;
	byte* pPos = pRM->GetPosPtr(nPosStride, FSL_READ);

	vtx_idx* pInds = pRM->GetIndexPtr(FSL_READ);

	if (!pPos || !pInds)
		return;

	int nInds = pRM->GetIndicesCount();

	//	if(nInds>6000)
	//	return; // skip insane objects

	assert(nInds % 3 == 0);

	if (!vOutNormal.IsZero())
		vOutNormal.Normalize();
	else
		return;

	float bestDot = 2.0f;
	Vec3 bestNormal(ZERO);
	Vec3 bestPoint(ZERO);

	// render tris
	TRenderChunkArray& Chunks = pRM->GetChunks();
	for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
	{
		CRenderChunk* pChunk = &Chunks[nChunkId];
		if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;

		int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;

		for (int i = pChunk->nFirstIndexId; i < nLastIndexId; i += 3)
		{
			assert(pInds[i + 0] < pChunk->nFirstVertId + pChunk->nNumVerts);
			assert(pInds[i + 1] < pChunk->nFirstVertId + pChunk->nNumVerts);
			assert(pInds[i + 2] < pChunk->nFirstVertId + pChunk->nNumVerts);
			assert(pInds[i + 0] >= pChunk->nFirstVertId);
			assert(pInds[i + 1] >= pChunk->nFirstVertId);
			assert(pInds[i + 2] >= pChunk->nFirstVertId);

			// get tri vertices
			Vec3 v0 = (*(Vec3*)&pPos[nPosStride * pInds[i + 0]]);
			Vec3 v1 = (*(Vec3*)&pPos[nPosStride * pInds[i + 1]]);
			Vec3 v2 = (*(Vec3*)&pPos[nPosStride * pInds[i + 2]]);

			bool first = false;
			bool second = false;
			bool third = false;

			if (v0 == vTri0 || v0 == vTri1 || v0 == vTri2)
				first = true;
			else if (v1 == vTri0 || v1 == vTri1 || v1 == vTri2)
				second = true;
			else if (v2 == vTri0 || v2 == vTri1 || v2 == vTri2)
				third = true;

			if (first || second || third)
			{
				// get triangle normal
				Vec3 vNormal = (v1 - v0).Cross(v2 - v0).GetNormalized();

				float testDot = vNormal.Dot(vOutNormal);
				if (testDot < bestDot)
				{
					bestDot = testDot;
					bestNormal = vNormal;
					if (first)
						bestPoint = v0;
					else if (second)
						bestPoint = v1;
					else if (third)
						bestPoint = v2;
				}

			}
		}

	}

	if (bestDot < 1.0f)
	{
		vOutNormal = (bestNormal + vOutNormal).GetNormalized();
		vOutPos.x = bestPoint.x;
		vOutPos.y = bestPoint.y;
	}
}

void CDecalManager::FillBigDecalIndices(IRenderMesh* pRM, Vec3 vPos, float fRadius, Vec3 vProjDirIn, PodArray<vtx_idx>* plstIndices, IMaterial* pMat, AABB& meshBBox, float& texelAreaDensity)
{
	FUNCTION_PROFILER_3DENGINE;

	AABB boxRM;
	pRM->GetBBox(boxRM.min, boxRM.max);

	Sphere sphere(vPos, fRadius);
	if (!Overlap::Sphere_AABB(sphere, boxRM))
		return;

	IRenderMesh::ThreadAccessLock lockrm(pRM);

	// get position offset and stride
	int nInds = pRM->GetIndicesCount();

	if (nInds > GetCVars()->e_DecalsMaxTrisInObject * 3)
		return; // skip insane objects

	CDecalRenderNode::m_nFillBigDecalIndicesCounter++;

	int nPosStride = 0;
	byte* pPos = pRM->GetPosPtr(nPosStride, FSL_READ);
	if (!pPos)
		return;
	vtx_idx* pInds = pRM->GetIndexPtr(FSL_READ);
	if (!pInds)
		return;

	assert(nInds % 3 == 0);

	plstIndices->Clear();

	bool bPointProj(vProjDirIn.IsZeroFast());

	if (!bPointProj)
		vProjDirIn.Normalize();

	if (!pMat)
		return;

	plstIndices->PreAllocate(16);

	Vec3 vProjDir = vProjDirIn;

	int usedTrianglesTotal = 0;

	TRenderChunkArray& Chunks = pRM->GetChunks();

	{
		Vec3 meshBBoxMin = meshBBox.min;
		Vec3 meshBBoxMax = meshBBox.max;

		const float fEpsilon = 0.001f;

		const int kNumChunks = Chunks.size();

		if (bPointProj)
		{
			for (int nChunkId = 0; nChunkId < kNumChunks; nChunkId++)
			{
				CRenderChunk* pChunk = &Chunks[nChunkId];

				PrefetchLine(&Chunks[nChunkId + 1], 0);
				PrefetchLine(&pInds[pChunk->nFirstIndexId], 0);

				if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
					continue;

				const SShaderItem& shaderItem = pMat->GetShaderItem(pChunk->m_nMatID);

				if (!shaderItem.m_pShader || !shaderItem.m_pShaderResources)
					continue;

				if (shaderItem.m_pShader->GetFlags() & (EF_NODRAW | EF_DECAL))
					continue;

				PrefetchLine(plstIndices->GetElements(), 0);

				int usedTriangles = 0;
				int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;

				int i = pChunk->nFirstIndexId;

				int iPosIndex0 = nPosStride * pInds[i + 0];
				int iPosIndex1 = nPosStride * pInds[i + 1];
				int iPosIndex2 = nPosStride * pInds[i + 2];

				for (; i < nLastIndexId; i += 3)
				{
					assert(pInds[i + 0] < pChunk->nFirstVertId + pChunk->nNumVerts);
					assert(pInds[i + 1] < pChunk->nFirstVertId + pChunk->nNumVerts);
					assert(pInds[i + 2] < pChunk->nFirstVertId + pChunk->nNumVerts);
					assert(pInds[i + 0] >= pChunk->nFirstVertId);
					assert(pInds[i + 1] >= pChunk->nFirstVertId);
					assert(pInds[i + 2] >= pChunk->nFirstVertId);

					PrefetchLine(&pInds[i], 128);

					int iNextPosIndex0 = 0;
					int iNextPosIndex1 = 0;
					int iNextPosIndex2 = 0;

					if (i + 5 < nLastIndexId)
					{
						iNextPosIndex0 = nPosStride * pInds[i + 3];
						iNextPosIndex1 = nPosStride * pInds[i + 4];
						iNextPosIndex2 = nPosStride * pInds[i + 5];

						PrefetchLine(&pPos[iNextPosIndex0], 0);
						PrefetchLine(&pPos[iNextPosIndex1], 0);
						PrefetchLine(&pPos[iNextPosIndex2], 0);
					}

					// get tri vertices
					const Vec3 v0 = *reinterpret_cast<Vec3*>(&pPos[iPosIndex0]);
					const Vec3 v1 = *reinterpret_cast<Vec3*>(&pPos[iPosIndex1]);
					const Vec3 v2 = *reinterpret_cast<Vec3*>(&pPos[iPosIndex2]);

					// test the face
					Vec3 v0v1Diff = v0 - v1;
					Vec3 v2v1Diff = v2 - v1;
					Vec3 vPosv0Diff = vPos - v0;

					Vec3 vCrossResult = v0v1Diff ^ v2v1Diff;

					float fDot = vPosv0Diff | vCrossResult;

					if (fDot > fEpsilon)
					{
						if (Overlap::Sphere_Triangle(sphere, Triangle(v0, v1, v2)))
						{
							plstIndices->AddList(&pInds[i], 3);

							Vec3 triBBoxMax1 = max(v1, v0);
							Vec3 triBBoxMax2 = max(meshBBoxMax, v2);

							Vec3 triBBoxMin1 = min(v1, v0);
							Vec3 triBBoxMin2 = min(meshBBoxMin, v2);

							meshBBoxMax = max(triBBoxMax1, triBBoxMax2);
							meshBBoxMin = min(triBBoxMin1, triBBoxMin2);

							usedTriangles++;
						}
					}

					iPosIndex0 = iNextPosIndex0;
					iPosIndex1 = iNextPosIndex1;
					iPosIndex2 = iNextPosIndex2;
				}

				if (pChunk->m_texelAreaDensity > 0.0f && pChunk->m_texelAreaDensity != (float)UINT_MAX)
				{
					texelAreaDensity += usedTriangles * pChunk->m_texelAreaDensity;
					usedTrianglesTotal += usedTriangles;
				}
			}
		}
		else
		{
			for (int nChunkId = 0; nChunkId < kNumChunks; nChunkId++)
			{
				CRenderChunk* pChunk = &Chunks[nChunkId];

				if (nChunkId + 1 < kNumChunks)
					PrefetchLine(&Chunks[nChunkId + 1], 0);
				PrefetchLine(&pInds[pChunk->nFirstIndexId], 0);

				if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
					continue;

				const SShaderItem& shaderItem = pMat->GetShaderItem(pChunk->m_nMatID);

				if (!shaderItem.m_pShader || !shaderItem.m_pShaderResources)
					continue;

				if (shaderItem.m_pShader->GetFlags() & (EF_NODRAW | EF_DECAL))
					continue;

				PrefetchLine(plstIndices->GetElements(), 0);

				int usedTriangles = 0;
				const int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
				const int nLastValidIndexId = nLastIndexId - 1;

				int i = pChunk->nFirstIndexId;

				int iNextPosIndex0 = 0;
				int iNextPosIndex1 = 0;
				int iNextPosIndex2 = 0;

				if (i + 5 < nLastIndexId)
				{
					iNextPosIndex0 = nPosStride * pInds[i + 3];
					iNextPosIndex1 = nPosStride * pInds[i + 4];
					iNextPosIndex2 = nPosStride * pInds[i + 5];

					PrefetchLine(&pPos[iNextPosIndex0], 0);
					PrefetchLine(&pPos[iNextPosIndex1], 0);
					PrefetchLine(&pPos[iNextPosIndex2], 0);
				}

				Vec3 v0Next = *reinterpret_cast<Vec3*>(&pPos[nPosStride * pInds[i + 0]]);
				Vec3 v1Next = *reinterpret_cast<Vec3*>(&pPos[nPosStride * pInds[i + 1]]);
				Vec3 v2Next = *reinterpret_cast<Vec3*>(&pPos[nPosStride * pInds[i + 2]]);

				const int nLastIndexToUse = nLastIndexId - 3;

				for (; i < nLastIndexToUse; i += 3)
				{
					assert(pInds[i + 0] < pChunk->nFirstVertId + pChunk->nNumVerts);
					assert(pInds[i + 1] < pChunk->nFirstVertId + pChunk->nNumVerts);
					assert(pInds[i + 2] < pChunk->nFirstVertId + pChunk->nNumVerts);
					assert(pInds[i + 0] >= pChunk->nFirstVertId);
					assert(pInds[i + 1] >= pChunk->nFirstVertId);
					assert(pInds[i + 2] >= pChunk->nFirstVertId);

					const int iLookaheadIdx = min_branchless(i + 8, nLastValidIndexId);
					const int iPrefetchIndex2 = nPosStride * pInds[iLookaheadIdx];

					// get tri vertices
					const Vec3 v0 = v0Next;
					const Vec3 v1 = v1Next;
					const Vec3 v2 = v2Next;

					//Need to prefetch further ahead
					byte* pPrefetch = &pPos[iPrefetchIndex2];
					PrefetchLine(pPrefetch, 0);

					v0Next = *reinterpret_cast<Vec3*>(&pPos[iNextPosIndex0]);

					// get triangle normal
					Vec3 v1v0Diff = v1 - v0;
					Vec3 v2v0Diff = v2 - v0;

					v1Next = *reinterpret_cast<Vec3*>(&pPos[iNextPosIndex1]);

					Vec3 vNormal = v1v0Diff ^ v2v0Diff;
					float fDot = vNormal | vProjDir;

					v2Next = *reinterpret_cast<Vec3*>(&pPos[iNextPosIndex2]);

					// test the face
					if (fDot > fEpsilon)
					{
						if (Overlap::Sphere_Triangle(sphere, Triangle(v0, v1, v2)))
						{
							plstIndices->AddList(&pInds[i], 3);

							Vec3 triBBoxMax1 = max(v1, v0);
							Vec3 triBBoxMax2 = max(meshBBoxMax, v2);

							Vec3 triBBoxMin1 = min(v1, v0);
							Vec3 triBBoxMin2 = min(meshBBoxMin, v2);

							meshBBoxMax = max(triBBoxMax1, triBBoxMax2);
							meshBBoxMin = min(triBBoxMin1, triBBoxMin2);

							usedTriangles++;
						}
					}

					iNextPosIndex0 = nPosStride * pInds[iLookaheadIdx - 2];
					iNextPosIndex1 = nPosStride * pInds[iLookaheadIdx - 1];
					iNextPosIndex2 = iPrefetchIndex2;
				}

				const Vec3 v0 = v0Next;
				const Vec3 v1 = v1Next;
				const Vec3 v2 = v2Next;

				// get triangle normal
				Vec3 v1v0Diff = v1 - v0;
				Vec3 v2v0Diff = v2 - v0;
				Vec3 vNormal = v1v0Diff ^ v2v0Diff;
				float fDot = vNormal | vProjDir;

				// test the face
				if (fDot > fEpsilon)
				{
					if (Overlap::Sphere_Triangle(sphere, Triangle(v0, v1, v2)))
					{
						plstIndices->AddList(&pInds[i], 3);

						Vec3 triBBoxMax1 = max(v1, v0);
						Vec3 triBBoxMax2 = max(meshBBoxMax, v2);

						Vec3 triBBoxMin1 = min(v1, v0);
						Vec3 triBBoxMin2 = min(meshBBoxMin, v2);

						meshBBoxMax = max(triBBoxMax1, triBBoxMax2);
						meshBBoxMin = min(triBBoxMin1, triBBoxMin2);

						usedTriangles++;
					}
				}

				if (pChunk->m_texelAreaDensity > 0.0f && pChunk->m_texelAreaDensity != (float)UINT_MAX)
				{
					texelAreaDensity += usedTriangles * pChunk->m_texelAreaDensity;
					usedTrianglesTotal += usedTriangles;
				}
			}
		}

		meshBBox.max = meshBBoxMax;
		meshBBox.min = meshBBoxMin;
	}

	if (usedTrianglesTotal != 0)
	{
		texelAreaDensity = texelAreaDensity / usedTrianglesTotal;
	}
}

_smart_ptr<IRenderMesh> CDecalManager::MakeBigDecalRenderMesh(IRenderMesh* pSourceRenderMesh, Vec3 vPos, float fRadius, Vec3 vProjDir, IMaterial* pDecalMat, IMaterial* pSrcMat)
{
	if (!pSourceRenderMesh || pSourceRenderMesh->GetVerticesCount() == 0)
		return 0;

	// make indices of this decal
	PodArray<vtx_idx> lstIndices;

	AABB meshBBox(vPos, vPos);
	float texelAreaDensity = 0.0f;

	if (pSourceRenderMesh && pSourceRenderMesh->GetVerticesCount())
		FillBigDecalIndices(pSourceRenderMesh, vPos, fRadius, vProjDir, &lstIndices, pSrcMat, meshBBox, texelAreaDensity);

	if (!lstIndices.Count())
		return 0;

	// make fake vert buffer with one vertex // todo: remove this
	PodArray<SVF_P3S_C4B_T2S> EmptyVertBuffer;
	EmptyVertBuffer.Add(SVF_P3S_C4B_T2S());

	_smart_ptr<IRenderMesh> pRenderMesh = 0;
	pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(EmptyVertBuffer.GetElements(), EmptyVertBuffer.Count(), EDefaultInputLayouts::P3S_C4B_T2S,
	                                                         lstIndices.GetElements(), lstIndices.Count(), prtTriangleList, "BigDecalOnStatObj", "BigDecal", eRMT_Static, 1, 0, 0, 0, false, false, 0);
	pRenderMesh->SetVertexContainer(pSourceRenderMesh);
	pRenderMesh->SetChunk(pDecalMat, 0, pSourceRenderMesh->GetVerticesCount(), 0, lstIndices.Count(), texelAreaDensity);
	pRenderMesh->SetBBox(meshBBox.min, meshBBox.max);

	return pRenderMesh;
}

void CDecalManager::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
}

void CDecalManager::DeleteDecalsInRange(AABB* pAreaBox, IRenderNode* pEntity)
{
	if (GetCVars()->e_Decals > 1 && pAreaBox)
		DrawBBox(*pAreaBox);

	for (int i = 0; i < DECAL_COUNT; i++)
	{
		if (!m_arrbActiveDecals[i])
			continue;

		if (pEntity && (pEntity != m_arrDecals[i].m_ownerInfo.pRenderNode))
			continue;

		if (pAreaBox)
		{
			Vec3 vPos = m_arrDecals[i].GetWorldPosition();
			Vec3 vSize(m_arrDecals[i].m_fWSSize, m_arrDecals[i].m_fWSSize, m_arrDecals[i].m_fWSSize);
			AABB decalBox(vPos - vSize, vPos + vSize);
			if (!Overlap::AABB_AABB(*pAreaBox, decalBox))
				continue;
		}

		if (m_arrDecals[i].m_eDecalType != eDecalType_WS_OnTheGround)
			m_arrbActiveDecals[i] = false;

		m_arrDecals[i].FreeRenderData();

		if (GetCVars()->e_Decals == 2)
		{
			CDecal& decal = m_arrDecals[i];
			PrintMessage("Debug: CDecalManager::DeleteDecalsInRange: Pos=(%.1f,%.1f,%.1f) Size=%.2f DecalMaterial=%s",
			             decal.m_vPos.x, decal.m_vPos.y, decal.m_vPos.z, decal.m_fSize, decal.m_pMaterial ? decal.m_pMaterial->GetName() : "none");
		}
	}
}

void CDecalManager::Serialize(TSerialize ser)
{
	ser.BeginGroup("StaticDecals");

	if (ser.IsReading())
		Reset();

	uint32 dwDecalCount = 0;

	for (uint32 dwI = 0; dwI < DECAL_COUNT; dwI++)
		if (m_arrbActiveDecals[dwI])
			dwDecalCount++;

	ser.Value("DecalCount", dwDecalCount);

	if (ser.IsWriting())
	{
		for (uint32 _dwI = 0; _dwI < DECAL_COUNT; _dwI++)
			if (m_arrbActiveDecals[_dwI])
			{
				CDecal& ref = m_arrDecals[_dwI];

				ser.BeginGroup("Decal");
				ser.Value("Pos", ref.m_vPos);
				ser.Value("Right", ref.m_vRight);
				ser.Value("Up", ref.m_vUp);
				ser.Value("Front", ref.m_vFront);
				ser.Value("Size", ref.m_fSize);
				ser.Value("WSPos", ref.m_vWSPos);
				ser.Value("WSSize", ref.m_fWSSize);
				ser.Value("fLifeTime", ref.m_fLifeTime);

				// serialize material, handle legacy decals with textureID converted to material created at runtime
				string matName("");
				if (ref.m_pMaterial && ref.m_pMaterial->GetName())
					matName = ref.m_pMaterial->GetName();
				ser.Value("MatName", matName);

				//			TODO:  IStatObj *		m_pStatObj;												// only if real 3d object is used as decal ()
				ser.Value("vAmbient", ref.m_vAmbient);
				//			TODO:  IRenderNode * m_pDecalOwner;										// transformation parent object (0 means parent is the world)
				ser.Value("nRenderNodeSlotId", ref.m_ownerInfo.nRenderNodeSlotId);
				ser.Value("nRenderNodeSlotSubObjectId", ref.m_ownerInfo.nRenderNodeSlotSubObjectId);

				int nDecalType = (int)ref.m_eDecalType;
				ser.Value("eDecalType", nDecalType);

				ser.Value("fGrowTime", ref.m_fGrowTime);
				ser.Value("fGrowTimeAlpha", ref.m_fGrowTimeAlpha);
				ser.Value("fLifeBeginTime", ref.m_fLifeBeginTime);

				bool bBigDecalUsed = ref.IsBigDecalUsed();

				ser.Value("bBigDecal", bBigDecalUsed);

				// m_arrBigDecalRMs[] will be created on the fly so no need load/save it

				if (bBigDecalUsed)
				{
					for (uint32 dwI = 0; dwI < sizeof(ref.m_arrBigDecalRMCustomData) / sizeof(ref.m_arrBigDecalRMCustomData[0]); ++dwI)
					{
						char szName[16];

						cry_sprintf(szName, "BDCD%u", dwI);
						ser.Value(szName, ref.m_arrBigDecalRMCustomData[dwI]);
					}
				}
				ser.EndGroup();
			}
	}
	else if (ser.IsReading())
	{
		m_nCurDecal = 0;

		for (uint32 _dwI = 0; _dwI < dwDecalCount; _dwI++)
			if (m_nCurDecal < DECAL_COUNT)
			{
				CDecal& ref = m_arrDecals[m_nCurDecal];

				ref.FreeRenderData();

				ser.BeginGroup("Decal");
				ser.Value("Pos", ref.m_vPos);
				ser.Value("Right", ref.m_vRight);
				ser.Value("Up", ref.m_vUp);
				ser.Value("Front", ref.m_vFront);
				ser.Value("Size", ref.m_fSize);
				ser.Value("WSPos", ref.m_vWSPos);
				ser.Value("WSSize", ref.m_fWSSize);
				ser.Value("fLifeTime", ref.m_fLifeTime);

				// serialize material, handle legacy decals with textureID converted to material created at runtime
				string matName("");
				ser.Value("MatName", matName);
				bool isTempMat(false);
				ser.Value("IsTempMat", isTempMat);

				ref.m_pMaterial = 0;
				if (!matName.empty())
				{
					ref.m_pMaterial = GetMatMan()->LoadMaterial(matName.c_str(), false, true);
					if (!ref.m_pMaterial)
						Warning("Decal material \"%s\" not found!\n", matName.c_str());
				}

				//			TODO:  IStatObj *		m_pStatObj;												// only if real 3d object is used as decal ()
				ser.Value("vAmbient", ref.m_vAmbient);
				//			TODO:  IRenderNode * m_pDecalOwner;										// transformation parent object (0 means parent is the world)
				ser.Value("nRenderNodeSlotId", ref.m_ownerInfo.nRenderNodeSlotId);
				ser.Value("nRenderNodeSlotSubObjectId", ref.m_ownerInfo.nRenderNodeSlotSubObjectId);

				int nDecalType = eDecalType_Undefined;
				ser.Value("eDecalType", nDecalType);
				ref.m_eDecalType = (EDecal_Type)nDecalType;

				ser.Value("fGrowTime", ref.m_fGrowTime);
				ser.Value("fGrowTimeAlpha", ref.m_fGrowTimeAlpha);
				ser.Value("fLifeBeginTime", ref.m_fLifeBeginTime);

				// no need to to store m_arrBigDecalRMs[] as it becomes recreated

				bool bBigDecalsAreaUsed = false;

				ser.Value("bBigDecals", bBigDecalsAreaUsed);

				if (bBigDecalsAreaUsed)
					for (uint32 dwI = 0; dwI < sizeof(ref.m_arrBigDecalRMCustomData) / sizeof(ref.m_arrBigDecalRMCustomData[0]); ++dwI)
					{
						char szName[16];

						cry_sprintf(szName, "BDCD%u", dwI);
						ser.Value(szName, ref.m_arrBigDecalRMCustomData[dwI]);
					}

				// m_arrBigDecalRMs[] will be created on the fly so no need load/save it

				m_arrbActiveDecals[m_nCurDecal] = bool(nDecalType != eDecalType_Undefined);

				++m_nCurDecal;
				ser.EndGroup();
			}
	}

	ser.EndGroup();
}

IMaterial* CDecalManager::GetMaterialForDecalTexture(const char* pTextureName)
{
	if (!pTextureName || *pTextureName == 0)
		return 0;

	IMaterialManager* pMatMan = GetMatMan();
	IMaterial* pMat = pMatMan->FindMaterial(pTextureName);
	if (pMat)
		return pMat;

	IMaterial* pMatSrc = pMatMan->LoadMaterial("Materials/Decals/Default", false, true);
	if (pMatSrc)
	{
		IMaterial* pMatDst = pMatMan->CreateMaterial(pTextureName, pMatSrc->GetFlags() | MTL_FLAG_NON_REMOVABLE);
		if (pMatDst)
		{
			SShaderItem& si = pMatSrc->GetShaderItem();
			SInputShaderResourcesPtr pIsr = gEnv->pRenderer->EF_CreateInputShaderResource(si.m_pShaderResources);

			pIsr->m_Textures[EFTT_DIFFUSE].m_Name = pTextureName;

			SShaderItem siDst = GetRenderer()->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, pIsr, si.m_pShader->GetGenerationMask());

			pMatDst->AssignShaderItem(siDst);

			return pMatDst;
		}
	}

	return 0;
}

IStatObj* SDecalOwnerInfo::GetOwner(Matrix34A& objMat)
{
	if (!pRenderNode)
		return NULL;

	IStatObj* pStatObj = NULL;
	if (pStatObj = pRenderNode->GetEntityStatObj(nRenderNodeSlotSubObjectId, &objMat, true))
	{
		if (nRenderNodeSlotSubObjectId >= 0 && nRenderNodeSlotSubObjectId < pStatObj->GetSubObjectCount())
		{
			IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(nRenderNodeSlotSubObjectId);
			pStatObj = pSubObj->pStatObj;
			objMat = objMat * pSubObj->tm;
		}
	}
	else if (ICharacterInstance* pChar = pRenderNode->GetEntityCharacter(&objMat))
	{
		if (nRenderNodeSlotSubObjectId >= 0)
		{
			pStatObj = pChar->GetISkeletonPose()->GetStatObjOnJoint(nRenderNodeSlotSubObjectId);
			const QuatT q = pChar->GetISkeletonPose()->GetAbsJointByID(nRenderNodeSlotSubObjectId);
			objMat = objMat * Matrix34(q);
		}
	}

	if (pStatObj && (pStatObj->GetFlags() & STATIC_OBJECT_HIDDEN))
		return NULL;

	if (pStatObj)
		if (int nMinLod = ((CStatObj*)pStatObj)->GetMinUsableLod())
			if (IStatObj* pLodObj = pStatObj->GetLodObject(nMinLod))
				pStatObj = pLodObj;

	return pStatObj;
}
