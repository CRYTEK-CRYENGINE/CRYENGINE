// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PhysCallbacks.cpp
//  Created:     14/11/2006 by Anton.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PhysCallbacks.h"
#include "RenderMeshUtils.h"
#include "Brush.h"
#include "VisAreas.h"
#include "MatMan.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryParticleSystem/IParticles.h>
#include <CryThreading/IJobManager_JobDelegator.h>

#define RENDER_MESH_COLLISION_IGNORE_DISTANCE 30.0f
#ifndef RENDER_MESH_TEST_DISTANCE
	#define RENDER_MESH_TEST_DISTANCE           (0.2f)
#endif

DECLARE_JOB("RayIntersection", TRayIntersectionJob, CRenderMeshUtils::RayIntersectionAsync);
DECLARE_JOB("DefColEvent_OnPhysCollision", TDeferredCollisionEventOnPhysCollision, CDeferredCollisionEventOnPhysCollision::Execute);

CPhysStreamer g_PhysStreamer;

// allocator for CDeferredCollisionEventOnPhysCollision(use 4kb pages, should be enough for deferred collision events)
static stl::PoolAllocatorNoMT<sizeof(CDeferredCollisionEventOnPhysCollision), 16> gCDeferredCollisionEventOnPhysCollisionPoolAllocator;

void CPhysCallbacks::Init()
{
	GetPhysicalWorld()->AddEventClient(EventPhysBBoxOverlap::id, &CPhysCallbacks::OnFoliageTouched, 1);
	GetPhysicalWorld()->AddEventClient(EventPhysCollision::id, &CDeferredCollisionEventOnPhysCollision::OnCollision, 1, 2000.0f);
	GetPhysicalWorld()->AddEventClient(EventPhysStateChange::id, &CPhysCallbacks::OnPhysStateChange, 1);
	//GetPhysicalWorld()->AddEventClient(EventPhysAreaChange::id, &CPhysCallbacks::OnPhysAreaChange, 0);
	GetPhysicalWorld()->SetPhysicsStreamer(&g_PhysStreamer);
}

//////////////////////////////////////////////////////////////////////////
void CPhysCallbacks::Done()
{
	GetPhysicalWorld()->RemoveEventClient(EventPhysBBoxOverlap::id, &CPhysCallbacks::OnFoliageTouched, 1);
	GetPhysicalWorld()->RemoveEventClient(EventPhysCollision::id, &CDeferredCollisionEventOnPhysCollision::OnCollision, 1);
	GetPhysicalWorld()->RemoveEventClient(EventPhysStateChange::id, &CPhysCallbacks::OnPhysStateChange, 1);
	//GetPhysicalWorld()->RemoveEventClient(EventPhysAreaChange::id, &CPhysCallbacks::OnPhysAreaChange, 0);
	GetPhysicalWorld()->SetPhysicsStreamer(0);
	gCDeferredCollisionEventOnPhysCollisionPoolAllocator.FreeMemory();
}

//////////////////////////////////////////////////////////////////////////

IRenderNode* GetRenderNodeFromPhys(void* pForeignData, int iForeignData)
{
	IEntityRender* pRndProxy;
	if (iForeignData == PHYS_FOREIGN_ID_STATIC)
		return (IRenderNode*)pForeignData;
	else if (iForeignData == PHYS_FOREIGN_ID_ENTITY && (pRndProxy = ((IEntity*)pForeignData)->GetRenderInterface()))
		return pRndProxy->GetRenderNode();
	return 0;
}
IStatObj* GetIStatObjFromPhysicalEntity(IPhysicalEntity* pent, int nPartId, Matrix34* pTM, int& nMats, int*& pMatMapping);

int       CPhysCallbacks::OnFoliageTouched(const EventPhys* pEvent)
{
	EventPhysBBoxOverlap* pOverlap = (EventPhysBBoxOverlap*)pEvent;
	if (pOverlap->iForeignData[1] == PHYS_FOREIGN_ID_STATIC || pOverlap->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY)
	{
		pe_params_bbox pbb;
		pOverlap->pEntity[1]->GetParams(&pbb);
		pe_params_part pp;
		pe_status_pos sp;
		pe_status_nparts snp;
		primitives::box bbox;
		OBB obb;
		Vec3 sz = pbb.BBox[1] - pbb.BBox[0];
		int nWheels = 0, nParts = pOverlap->pEntity[0]->GetStatus(&snp);
		if (pOverlap->pEntity[0]->GetType() == PE_WHEELEDVEHICLE)
		{
			pe_params_car pc;
			pOverlap->pEntity[0]->GetParams(&pc);
			nWheels = pc.nWheels;
		}

		pOverlap->pEntity[0]->GetStatus(&sp);
		for (pp.ipart = 0; pp.ipart < nParts - nWheels && pOverlap->pEntity[0]->GetParams(&pp); pp.ipart++, MARK_UNUSED pp.partid)
			if (pp.flagsOR & (geom_colltype0 | geom_colltype1 | geom_colltype2 | geom_colltype3))
			{
				pp.pPhysGeomProxy->pGeom->GetBBox(&bbox);
				obb.SetOBB(Matrix33(sp.q * pp.q) * bbox.Basis.T(), bbox.size * pp.scale, Vec3(ZERO));
				if (Overlap::AABB_OBB(AABB(pbb.BBox[0], pbb.BBox[1]), sp.q * (pp.q * bbox.center * pp.scale + pp.pos) + sp.pos, obb))
					goto foundbox;
			}
		return 1;
foundbox:

		int i;
		pe_params_foreign_data pfd;
		C3DEngine* pEngine = (C3DEngine*)gEnv->p3DEngine;
		pOverlap->pEntity[0]->GetParams(&pfd);
		if ((i = (pfd.iForeignFlags >> 8 & 255) - 1) < 0)
		{
			if ((i = pEngine->m_arrEntsInFoliage.size()) < 255)
			{
				SEntInFoliage eif;
				eif.id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pOverlap->pEntity[0]);
				eif.timeIdle = 0;
				pEngine->m_arrEntsInFoliage.push_back(eif);
				MARK_UNUSED pfd.pForeignData, pfd.iForeignData, pfd.iForeignFlags;
				pfd.iForeignFlagsOR = (i + 1) << 8;
				pOverlap->pEntity[0]->SetParams(&pfd, 1);
			}
		}
		else if (i < (int)pEngine->m_arrEntsInFoliage.size())
			pEngine->m_arrEntsInFoliage[i].timeIdle = 0;

		IRenderNode* pVeg = GetRenderNodeFromPhys(pOverlap->pForeignData[1], pOverlap->iForeignData[1]);
		const CCamera& cam = gEnv->pSystem->GetViewCamera();
		int cullDist = GetCVars()->e_CullVegActivation;
		int iSource = 0;

		if (!cullDist || static_cast<unsigned>(pVeg->GetDrawFrame()) + 10 > gEnv->nMainFrameID &&
		    (cam.GetPosition() - pVeg->GetPos()).len2() * sqr(cam.GetFov()) < sqr(cullDist * 1.04f))
			if (pVeg->GetPhysics() == pOverlap->pEntity[1] && !pVeg->PhysicalizeFoliage(true, iSource) && nWheels > 0 && sz.x * sz.y < 0.5f)
				for (pp.ipart = nParts - 1; pp.ipart >= nParts - nWheels && pOverlap->pEntity[0]->GetParams(&pp); pp.ipart--, MARK_UNUSED pp.partid)
					if (pp.pPhysGeomProxy->pGeom->PointInsideStatus(
					      (((pbb.BBox[1] + pbb.BBox[0]) * 0.5f - sp.pos) * sp.q - pp.pos) * pp.q * (pp.scale == 1.0f ? 1.0f : 1.0f / pp.scale)))
					{
						// break the vegetation object
						CStatObj* pStatObj = (CStatObj*)pVeg->GetEntityStatObj();
						if (pStatObj)
						{
							IParticleEffect* pDisappearEffect = pStatObj->GetSurfaceBreakageEffect(SURFACE_BREAKAGE_TYPE("destroy"));
							if (pDisappearEffect)
								pDisappearEffect->Spawn(true, IParticleEffect::ParticleLoc(pVeg->GetBBox().GetCenter(), Vec3(0, 0, 1), (sz.x + sz.y + sz.z) * 0.3f));
						}
						pVeg->Dephysicalize();
						pEngine->UnRegisterEntityAsJob(pVeg);
						break;
					}
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
int CPhysCallbacks::OnPhysStateChange(const EventPhys* pEvent)
{
	EventPhysStateChange* pepsc = (EventPhysStateChange*)pEvent;
	int i;
	pe_params_foreign_data pfd;

	if (pepsc->iSimClass[0] == 2 && pepsc->iSimClass[1] == 1 && pepsc->pEntity->GetParams(&pfd) && (i = (pfd.iForeignFlags >> 8) - 1) >= 0)
	{
		((C3DEngine*)gEnv->p3DEngine)->RemoveEntInFoliage(i, pepsc->pEntity);
		pe_params_bbox pbb;
		IPhysicalEntity* pentlist[8], ** pents = pentlist;
		IRenderNode* pBrush;
		pepsc->pEntity->GetParams(&pbb);
		for (i = gEnv->pPhysicalWorld->GetEntitiesInBox(pbb.BBox[0], pbb.BBox[1], pents, ent_static | ent_allocate_list, 6) - 1; i >= 0; i--)
			if (pents[i]->GetParams(&pfd) && (pBrush = GetRenderNodeFromPhys(pfd.pForeignData, pfd.iForeignData)))
				pBrush->PhysicalizeFoliage(true, 2, -1);
		if (pents != pentlist)
			gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(pents);
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CDeferredCollisionEventOnPhysCollision::RayTraceVegetation()
{
	EventPhysCollision* pCollision = &m_CollisionEvent;

	if (!(pCollision->iForeignData[1] == PHYS_FOREIGN_ID_STATIC || pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY))
		return;

	ISurfaceType* pSrfType = GetMatMan()->GetSurfaceType(pCollision->idmat[1]);
	if (!(pSrfType && pSrfType->GetFlags() & SURFACE_TYPE_NO_COLLIDE))
		return;

	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);
	IRenderNode* pVeg = GetRenderNodeFromPhys(pCollision->pForeignData[1], pCollision->iForeignData[1]);
	if (!pVeg)
	{
		MarkFinished(1);
		return;
	}

	const CCamera& cam = gEnv->pSystem->GetViewCamera();
	int cullDist = GetCVars()->e_CullVegActivation;
	if (cullDist && (static_cast<unsigned>(pVeg->GetDrawFrame() + 10) < gEnv->nMainFrameID ||
	                 (cam.GetPosition() - pVeg->GetPos()).len2() * sqr(cam.GetFov()) > sqr(cullDist * 1.04f)))
	{
		MarkFinished(0);
		return;
	}

	int i, j, bHit = 0, * pMats, nSlot = pCollision->partid[1] & 1023;
	float tx = 0, ty = 0, len2 = 0, denom2 = 0;
	Matrix34 mtx, mtxInv, mtxw;
	Vec3* pVtx, pt, pt0, dir, pthit, nup, edge, axisx, axisy;
	bool bLocal;
	pe_action_impulse ai;
	pe_status_rope sr;
	CStatObj* pStatObj = (CStatObj*)GetIStatObjFromPhysicalEntity(pCollision->pEntity[1], pCollision->partid[1], &mtx, i, pMats);
	if (!pStatObj)
	{
		MarkFinished(false);
		return;
	}
	pe_status_pos sp;
	sp.pMtx3x4 = &mtxw;
	pCollision->pEntity[1]->GetStatus(&sp);
	mtx = mtxw * mtx;

	if (pStatObj->m_nSpines && pCollision->n * pCollision->vloc[0] > 0)
	{
		ai.impulse = pCollision->vloc[0] * (pCollision->mass[0] * 0.5f);
		pt = pt0 = pCollision->pt;
		dir = pCollision->vloc[0].normalized();
		if (bLocal = (pVeg->GetFoliage(nSlot) == 0))
		{
			// if branches are not physicalized, operate in local space
			mtxInv = mtx.GetInverted();
			pt = mtxInv.TransformPoint(pt);
			dir = mtxInv.TransformVector(dir);
		}
		for (i = 0; i < pStatObj->m_nSpines; i++)
			if (pStatObj->m_pSpines[i].bActive)
			{
				nup = pStatObj->m_pSpines[i].navg;
				if (bLocal)
					pVtx = pStatObj->m_pSpines[i].pVtx;
				else
				{
					pVtx = sr.pPoints = pStatObj->m_pSpines[i].pVtxCur;
					if (!pVeg->GetBranchPhys(i, nSlot))
						continue;
					pVeg->GetBranchPhys(i, nSlot)->GetStatus(&sr);
					nup = mtx.TransformVector(nup);
				}
				//axisx = pVtx[pStatObj->m_pSpines[i].nVtx-1]-pVtx[0]; axisy = pt-pVtx[0];
				//if (sqr_signed(axisx*axisy) < 0)//axisx.len2()*axisy.len2()*0.25f)
				//	continue;
				for (j = 0; j < pStatObj->m_pSpines[i].nVtx - 1; j++)
				{
					edge = pVtx[j + 1] - pVtx[j];
					len2 = edge.len2();
					axisx = edge ^ nup;
					denom2 = axisx.len2();
					axisy = axisx ^ edge;
					tx = (pVtx[j] - pt) * axisx;
					ty = dir * axisx;
					pthit = (pt - pVtx[j]) * ty + dir * tx;
					if (inrange(pthit * edge, 0.0f, len2 * ty) && inrange(sqr_signed(pthit * axisy),
					                                                      sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].z * ty) * denom2, sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].w * ty) * denom2))
						break; // hit y fin
					tx = (pVtx[j] - pt) * axisy;
					ty = dir * axisy;
					pthit = (pt - pVtx[j]) * ty + dir * tx;
					if (inrange(pthit * edge, 0.0f, len2 * ty) && inrange(sqr_signed(pthit * axisx),
					                                                      sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].x * ty) * denom2, sqr_signed(pStatObj->m_pSpines[i].pSegDim[j].y * ty) * denom2))
						break; // hit x fin
				}
				if (j < pStatObj->m_pSpines[i].nVtx - 1 && pVeg->PhysicalizeFoliage(true, 1, nSlot))
				{
					pthit = pthit / ty + pVtx[j];
					if (bLocal)
						pthit = mtx * pthit;
					if ((pthit - pt0) * pCollision->vloc[0] < (pCollision->pt - pt0) * pCollision->vloc[0])
						pCollision->pt = pthit;
					ai.point = pthit;
					ai.partid = j;
					IPhysicalEntity* pent = pCollision->pEntity[1];
					(pCollision->pEntity[1] = pVeg->GetBranchPhys(i, nSlot))->Action(&ai);
					pCollision->idmat[1] = pStatObj->m_pSpines[i].idmat;
					((CStatObjFoliage*)pCollision->pEntity[1]->GetForeignData(PHYS_FOREIGN_ID_FOLIAGE))->OnHit(pCollision);
					pCollision->pEntity[1] = pent;
					bHit = 1;
				}
			}
	}
	else if (!pStatObj->m_nSpines && pCollision->n * pCollision->vloc[0] < 0)
	{
		if (((C3DEngine*)gEnv->p3DEngine)->m_idMatLeaves < 0)
		{
			ISurfaceType* pLeavesMat = GetMatMan()->GetSurfaceTypeByName("mat_leaves", "Physical Collision");
			((C3DEngine*)gEnv->p3DEngine)->m_idMatLeaves = pLeavesMat ? pLeavesMat->GetId() : 0;
		}
		if (!pCollision->idmat[1])
			pCollision->idmat[1] = ((C3DEngine*)gEnv->p3DEngine)->m_idMatLeaves;
		bHit = 1;
	}

	if (!bHit)
		pCollision->normImpulse = 0;
	MarkFinished(bHit);
	return;
}

//////////////////////////////////////////////////////////////////////////
void CDeferredCollisionEventOnPhysCollision::AdjustBulletVelocity()
{
	if (m_bPierceable)
		return;

	EventPhysCollision* pCollision = &m_CollisionEvent;

	// Let bullet fly some more.
	pe_action_set_velocity actionVelocity;
	memcpy(&actionVelocity.v, &pCollision->vloc[0], sizeof(Vec3)); // gcc bug workaround
	pCollision->pEntity[0]->Action(&actionVelocity);

	//pe_params_particle paramsPart;
	//pCollision->pEntity[0]->GetParams(&paramsPart);

	pe_params_pos ppos;
	ppos.pos = pCollision->pt + pCollision->vloc[0].GetNormalized() * 0.003f;//(paramsPart.size + 0.01f);
	pCollision->pEntity[0]->SetParams(&ppos);
}

//////////////////////////////////////////////////////////////////////////
void CDeferredCollisionEventOnPhysCollision::PostStep()
{
	EventPhysCollision* pCollision = &m_CollisionEvent;
	pe_params_rope pr;

	if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ROPE && pCollision->vloc[0].len2() > sqr(20.0f) && !gEnv->bMultiplayer &&
	    !(((IRopeRenderNode*)pCollision->pForeignData[1])->GetParams().nFlags & IRopeRenderNode::eRope_Nonshootable) &&
	    pCollision->pEntity[1]->GetParams(&pr) && pr.pEntTiedTo[0] && pr.pEntTiedTo[1])
	{
		pe_action_slice as;
		Vec3 pt;
		as.pt = &pt;
		as.npt = 1;
		pt.x = (pCollision->partid[1] + 1 + ((pCollision->partid[1] * 2 - pr.nSegments) >> 31)) * pr.length / pr.nSegments;
		pCollision->pEntity[1]->Action(&as);
	}

	UpdateFoliage();
}

void CDeferredCollisionEventOnPhysCollision::UpdateFoliage()
{
	EventPhysCollision* pCollision = &m_CollisionEvent;

	if (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_FOLIAGE)
	{
		((CStatObjFoliage*)pCollision->pForeignData[1])->m_timeIdle = 0;
		((CStatObjFoliage*)pCollision->pForeignData[1])->OnHit(pCollision);
	}
}

//////////////////////////////////////////////////////////////////////////
IStatObj* GetIStatObjFromPhysicalEntity(IPhysicalEntity* pent, int nPartId, Matrix34* pTM, int& nMats, int*& pMatMapping)
{
	pe_params_part ppart;
	ppart.partid = nPartId;
	if (pTM)
		ppart.pMtx3x4 = pTM;
	if (pent->GetParams(&ppart))
	{
		if (ppart.pPhysGeom && ppart.pPhysGeom->pGeom)
		{
			IStatObj* pStatObj = (IStatObj*)ppart.pPhysGeom->pGeom->GetForeignData(0);
			if (ppart.pPhysGeom->pMatMapping == ppart.pMatMapping)
			{
				// No custom material.
				nMats = 0;
				pMatMapping = 0;
			}
			else
			{
				nMats = ppart.nMats;
				pMatMapping = ppart.pMatMapping;
			}

			IStatObj* pStatObjParent;
			IStatObj::SSubObject* pSubObj;
			if (pStatObj &&
			    pStatObj->GetPhysGeom() != ppart.pPhysGeom &&
			    pStatObj->GetPhysGeom(PHYS_GEOM_TYPE_NO_COLLIDE) != ppart.pPhysGeom && pStatObj->GetPhysGeom(PHYS_GEOM_TYPE_OBSTRUCT) != ppart.pPhysGeom &&
			    (pStatObjParent = pStatObj->GetParentObject()) && pStatObjParent != pStatObj)
				for (int i = pStatObjParent->GetSubObjectCount() - 1; i >= 0; i--)
					if (pTM && (pSubObj = pStatObjParent->GetSubObject(i))->pStatObj == pStatObj)
					{
						*pTM = Matrix34::CreateScale(Vec3(ppart.scale)) * pSubObj->localTM;
						break;
					}

			return pStatObj;
		}
	}
	return 0;
}

void CDeferredCollisionEventOnPhysCollision::TestCollisionWithRenderMesh()
{
	EventPhysCollision* pCollisionEvent = &m_CollisionEvent;
	assert(pCollisionEvent);

	if (pCollisionEvent->iForeignData[0] != PHYS_FOREIGN_ID_ENTITY)
	{
		MarkFinished(1);
		return;
	}

	if (pCollisionEvent->vloc[0].IsZero())
	{
		MarkFinished(1);
		return;
	}

	// Ignore it if too far.
	Vec3 vCamPos = GetSystem()->GetViewCamera().GetPosition();

	// do not spawn if too far
	float fZoom = gEnv->p3DEngine->GetZoomFactor();
	float fCollisionDistance = pCollisionEvent->pt.GetDistance(vCamPos);
	if (fCollisionDistance * fZoom > GetFloatCVar(e_DecalsRange) && pCollisionEvent->mass[0] < GetFloatCVar(e_MinMassDistanceCheckRenderMeshCollision))
	{
		// only apply distance check for bullets; heavier particles should use it always
		MarkFinished(1);
		return;
	}

	// Check if backface collision.
	if (pCollisionEvent->vloc[0].Dot(pCollisionEvent->n) > 0)
	{
		// Ignore testing render-mesh for backface collisions.
		MarkFinished(0);
		return;
	}

	// Get IStatObj from physical entity.
	Matrix34 partTM;
	IStatObj* pStatObj = GetIStatObjFromPhysicalEntity(pCollisionEvent->pEntity[1], pCollisionEvent->partid[1], &partTM, m_nMats, m_pMatMapping);

	unsigned int pierceability0 = 0, pierceability1;
	float dummy;

	if (pCollisionEvent->pEntity[0])
	{
		if (pCollisionEvent->pEntity[0]->GetType() == PE_PARTICLE)
			pierceability0 = pCollisionEvent->partid[0];
		else
			GetPhysicalWorld()->GetSurfaceParameters(pCollisionEvent->idmat[0], dummy, dummy, pierceability0);
		GetPhysicalWorld()->GetSurfaceParameters(pCollisionEvent->idmat[1], dummy, dummy, pierceability1);
		m_bPierceable = (pierceability0 & 15) < (pierceability1 & 15);
	}

	if (pStatObj)
		if (int nMinLod = GetCVars()->e_RenderMeshCollisionLod)
			if (IStatObj* pLodObj = pStatObj->GetLodObject(nMinLod))
				pStatObj = pLodObj;

	if (pStatObj == NULL)
	{
		MarkFinished(1);
		return;
	}

	m_pRenderMesh = pStatObj->GetRenderMesh();

	IRenderNode* pRenderNode = GetRenderNodeFromPhys(pCollisionEvent->pForeignData[1], pCollisionEvent->iForeignData[1]);
	IMaterial* pRenderNodeMaterial = pRenderNode ? pRenderNode->GetMaterial() : NULL;
	m_pMaterial = pRenderNodeMaterial ? pRenderNodeMaterial : pStatObj->GetMaterial();

	m_bDecalPlacementTestRequested = (pCollisionEvent->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY);

	if (m_pRenderMesh == NULL)
	{
		MarkFinished(1);
		return;
	}

	pe_status_pos ppos;
	ppos.pMtx3x4 = &m_worldTM;
	pCollisionEvent->pEntity[1]->GetStatus(&ppos);
	m_worldTM = m_worldTM * partTM;
	m_worldRot = Matrix33(m_worldTM);
	m_worldRot /= m_worldRot.GetColumn(0).GetLength();
	Vec3 dir = pCollisionEvent->vloc[0].GetNormalized();

	// transform decal into object space
	Matrix34 worldTM_Inverted = m_worldTM.GetInverted();

	// put hit normal into the object space
	Vec3 vRayDir = dir * m_worldRot;

	// put hit position into the object space
	Vec3 vHitPos = worldTM_Inverted.TransformPoint(pCollisionEvent->pt);
	Vec3 vLineP1 = vHitPos - vRayDir * RENDER_MESH_TEST_DISTANCE;

	// set up hit data
	m_HitInfo.inReferencePoint = vHitPos;
	m_HitInfo.inRay.origin = vLineP1;
	m_HitInfo.inRay.direction = vRayDir;
	m_HitInfo.bInFirstHit = false;
	m_HitInfo.bUseCache = false; // no cache with parallel execution

	if (pCollisionEvent->pEntity[0])
	{
		// Cap the rendermesh linetest, anything past fMaxHitDistance is considered a hole
		Vec3 ptlim;
		if (m_bPierceable && pCollisionEvent->next && pCollisionEvent->next->idval == EventPhysCollision::id &&
		    ((EventPhysCollision*)pCollisionEvent->next)->pEntity[0] == pCollisionEvent->pEntity[0])
		{
			ptlim = ((EventPhysCollision*)pCollisionEvent->next)->pt;
		}
		else
		{
			ptlim = pCollisionEvent->pt + dir * (GetCVars()->e_RenderMeshCollisionTolerance + dir * pCollisionEvent->vloc[0] * (GetTimer()->GetFrameTime() * 1.1f));
		}
		m_HitInfo.fMaxHitDistance = (worldTM_Inverted * ptlim - m_HitInfo.inRay.origin) * m_HitInfo.inRay.direction;
	}
}

void CDeferredCollisionEventOnPhysCollision::FinishTestCollisionWithRenderMesh()
{
	EventPhysCollision* pCollisionEvent = &m_CollisionEvent;
	assert(pCollisionEvent);

	//CryLogAlways( "Do Test %f,%f",vHitPos.x,vHitPos.y );
	if (m_RayIntersectionData.bResult == false)
	{
		pCollisionEvent->normImpulse = 0;
		MarkFinished(0);
		return;
	}

	Vec3 ptnew = m_worldTM.TransformPoint(m_HitInfo.vHitPos);
	// prevent reporting collisions before for the case that we have a previous valid hit
	//  if ((ptnew-pCollisionEvent->pt)*pCollisionEvent->vloc[0] > (m_ptlim-pCollisionEvent->pt)*pCollisionEvent->vloc[0] )
	//	{
	//		MarkFinished(0);
	//		return;
	//	}

	pCollisionEvent->pt = ptnew;
	pCollisionEvent->n = m_worldRot.TransformVector(m_HitInfo.vHitNormal);
	if (pCollisionEvent->penetration >= 0)
	{
		if (m_pMatMapping)
		{
			// When using custom material mapping.
			if (m_HitInfo.nHitMatID < m_nMats)
			{
				m_HitInfo.nHitSurfaceID = m_pMatMapping[m_HitInfo.nHitMatID];
			}
		}
		if (m_HitInfo.nHitSurfaceID)
		{
			pCollisionEvent->idmat[1] = m_HitInfo.nHitSurfaceID;
		}
	}

	pCollisionEvent->fDecalPlacementTestMaxSize = m_RayIntersectionData.fDecalPlacementTestMaxSize;
}

int CPhysStreamer::CreatePhysicalEntity(void* pForeignData, int iForeignData, int iForeignFlags)
{
	if (iForeignData == PHYS_FOREIGN_ID_STATIC)
		if (iForeignFlags & (PFF_BRUSH | PFF_VEGETATION))
			((IRenderNode*)pForeignData)->Physicalize(true);
		else
			return 0;
	return 1;
}

int CPhysStreamer::CreatePhysicalEntitiesInBox(const Vec3& boxMin, const Vec3& boxMax)
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);
	if (((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree)
		((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree->PhysicalizeInBox(AABB(boxMin, boxMax));
	if (((C3DEngine*)gEnv->p3DEngine)->m_pVisAreaManager)
		((C3DEngine*)gEnv->p3DEngine)->m_pVisAreaManager->PhysicalizeInBox(AABB(boxMin, boxMax));
	return 1;
}

int CPhysStreamer::DestroyPhysicalEntitiesInBox(const Vec3& boxMin, const Vec3& boxMax)
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);
	if (((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree)
		((C3DEngine*)gEnv->p3DEngine)->m_pObjectsTree->DephysicalizeInBox(AABB(boxMin, boxMax));
	if (((C3DEngine*)gEnv->p3DEngine)->m_pVisAreaManager)
		((C3DEngine*)gEnv->p3DEngine)->m_pVisAreaManager->DephysicalizeInBox(AABB(boxMin, boxMax));
	return 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int CDeferredCollisionEventOnPhysCollision::OnCollision(const EventPhys* pEvent)
{
	IDeferredPhysicsEventManager* pDeferredPhysicsEventManager = gEnv->p3DEngine->GetDeferredPhysicsEventManager();

	// deferred physics event manager handles the logic if an event needs to be deferred and creating/ destruction of it
	return pDeferredPhysicsEventManager->HandleEvent(pEvent, &CDeferredCollisionEventOnPhysCollision::CreateCollisionEvent, IDeferredPhysicsEvent::PhysCallBack_OnCollision);

}

CDeferredCollisionEventOnPhysCollision::CDeferredCollisionEventOnPhysCollision(const EventPhysCollision* pCollisionEvent) :
	m_bHit(false),
	m_bFinished(false),
	m_bPierceable(false),
	m_nResult(0),
	m_CollisionEvent(*pCollisionEvent),
	m_pMatMapping(NULL),
	m_nMats(-1),
	m_pRenderMesh(NULL),
	m_pMaterial(NULL),
	m_bTaskRunning(false),
	m_bDecalPlacementTestRequested(false),
	m_bNeedMeshThreadUnLock(false)
{
	memset(&m_HitInfo, 0, sizeof(m_HitInfo));

	m_nWaitForPrevEvent = 0;
	m_pNextEvent = 0;
	if (IDeferredPhysicsEvent* pLastEvent = gEnv->p3DEngine->GetDeferredPhysicsEventManager()->GetLastCollisionEventForEntity(pCollisionEvent->pEntity[0]))
	{
		m_nWaitForPrevEvent = 10;
		((CDeferredCollisionEventOnPhysCollision*)pLastEvent)->m_pNextEvent = this;
	}
}

CDeferredCollisionEventOnPhysCollision::~CDeferredCollisionEventOnPhysCollision()
{
	// make sure nothing is in flight if we are destructed
	Sync();

	if (m_pRenderMesh && m_bNeedMeshThreadUnLock)
	{
		m_pRenderMesh->UnLockForThreadAccess();
		m_bNeedMeshThreadUnLock = false;
	}

	IDeferredPhysicsEventManager* pDeferredPhysicsEventManager = gEnv->p3DEngine->GetDeferredPhysicsEventManager();
	pDeferredPhysicsEventManager->UnRegisterDeferredEvent(this);
}

void CDeferredCollisionEventOnPhysCollision::Execute()
{
	// init data for async ray intersection
	if (!m_bNeedMeshThreadUnLock)
	{
		m_pRenderMesh->LockForThreadAccess();
		m_bNeedMeshThreadUnLock = true;
	}
	if (m_RayIntersectionData.Init(m_pRenderMesh, &m_HitInfo, m_pMaterial, m_bDecalPlacementTestRequested))
	{
		TRayIntersectionJob job(&m_RayIntersectionData);
		job.SetPriorityLevel(JobManager::eLowPriority);
		job.RegisterJobState(&m_jobStateRayInter);
		job.Run();
	}

	m_bTaskRunning = false;
}

void CDeferredCollisionEventOnPhysCollision::Start()
{
	EventPhysCollision* pCollision = &m_CollisionEvent;
	assert(pCollision->pEntity[0]);

	if (pCollision->pEntity[0]->GetType() != PE_PARTICLE)
	{
		UpdateFoliage();
		MarkFinished(1);
		return;
	}

	RayTraceVegetation();

	if (m_bFinished)  // RayTracevegetation already found a hit
		return;

	TestCollisionWithRenderMesh();

	if (m_bFinished)  // Early out in TestCollisionWithRenderMesh
	{
		PostStep();
		return;
	}

	// do the remaining work in a task
	m_bTaskRunning = true;
	IDeferredPhysicsEventManager* pDeferredPhysicsEventManager = gEnv->p3DEngine->GetDeferredPhysicsEventManager();
	pDeferredPhysicsEventManager->DispatchDeferredEvent(this);
}

void CDeferredCollisionEventOnPhysCollision::ExecuteAsJob()
{
	TDeferredCollisionEventOnPhysCollision job;
	job.SetClassInstance(this);
	job.SetPriorityLevel(JobManager::eRegularPriority);
	job.RegisterJobState(&m_jobStateOnCollision);
	job.Run();
}

int CDeferredCollisionEventOnPhysCollision::Result(EventPhys* pOrigEvent)
{
	Sync();
	if (m_pNextEvent)
		m_pNextEvent->m_nWaitForPrevEvent = 0;

	if (pOrigEvent)
		((EventPhysCollision*)pOrigEvent)->normImpulse = m_CollisionEvent.normImpulse;

	if (m_bFinished)
		return m_nResult;

	FinishTestCollisionWithRenderMesh();

	if (pOrigEvent)
	{
		EventPhysCollision* pCEvent = (EventPhysCollision*)pOrigEvent;
		pCEvent->pt = m_CollisionEvent.pt;
		pCEvent->n = m_CollisionEvent.n;
		pCEvent->normImpulse = m_CollisionEvent.normImpulse;
		pCEvent->fDecalPlacementTestMaxSize = m_CollisionEvent.fDecalPlacementTestMaxSize;
		pCEvent->idmat[1] = m_CollisionEvent.idmat[1];
	}

	// no hit, no need for Post-Step
	if (m_bFinished)
	{
		AdjustBulletVelocity();
		return m_nResult;
	}

	PostStep();

	m_bFinished = true;
	m_nResult = 1;
	return m_nResult;
}

void CDeferredCollisionEventOnPhysCollision::MarkFinished(int nResult)
{
	m_nResult = nResult;
	m_bFinished = true;
}

void CDeferredCollisionEventOnPhysCollision::Sync()
{
	FUNCTION_PROFILER_3DENGINE;
	gEnv->GetJobManager()->WaitForJob(m_jobStateRayInter);
	gEnv->GetJobManager()->WaitForJob(m_jobStateOnCollision);

	// in normal execution case, m_bTaskRunning should always be false when we are here
	while (m_bTaskRunning)
	{
		CrySleep(1); // yield
	}
}

bool CDeferredCollisionEventOnPhysCollision::HasFinished()
{
	if (m_bTaskRunning || m_nWaitForPrevEvent > 0)
	{
		--m_nWaitForPrevEvent;
		return false;
	}

	if (m_jobStateRayInter.IsRunning())
		return false;

	return true;
}

IDeferredPhysicsEvent* CDeferredCollisionEventOnPhysCollision::CreateCollisionEvent(const EventPhys* pCollisionEvent)
{
	return new CDeferredCollisionEventOnPhysCollision((EventPhysCollision*)pCollisionEvent);
}

void* CDeferredCollisionEventOnPhysCollision::operator new(size_t)
{
	return gCDeferredCollisionEventOnPhysCollisionPoolAllocator.Allocate();
}

void CDeferredCollisionEventOnPhysCollision::operator delete(void* ptr)
{
	gCDeferredCollisionEventOnPhysCollisionPoolAllocator.Deallocate(ptr);
}
