// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   brush.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "PolygonClipContext.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "Brush.h"
#include "terrain.h"

const char* CBrush::GetEntityClassName() const
{
	return "Brush";
}

const char* CBrush::GetName() const
{
	if (m_pStatObj)
		return m_pStatObj->GetFilePath();
	return "StatObjNotSet";
}

bool CBrush::HasChanged()
{
	return false;
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )               //AMD Port
	#pragma warning( disable : 4311 )
#endif

CLodValue CBrush::ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo)
{
	CVars* pCVars = GetCVars();

	uint8 nDissolveRefA = 0;
	int nLodA = -1;
	int nLodB = -1;

	if (CStatObj* pStatObj = (CStatObj*)CBrush::GetEntityStatObj())
	{
		const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
		const float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

		// update LOD faster in zoom mode
		if (passInfo.IsGeneralPass() && passInfo.IsZoomActive())
		{
			wantedLod = CObjManager::GetObjectLOD(this, fEntDistance);
		}

		if (pCVars->e_Dissolve && passInfo.IsGeneralPass() && !(pStatObj->m_nFlags & STATIC_OBJECT_COMPOUND))
		{
			int nLod = CLAMP(wantedLod, pStatObj->GetMinUsableLod(), (int)pStatObj->m_nMaxUsableLod);
			nLod = pStatObj->FindNearesLoadedLOD(nLod, true);

			SLodDistDissolveTransitionState& rState = m_pTempData->userData.lodDistDissolveTransitionState;

			// if we're more than one LOD away we've either zoomed in quickly
			// or we streamed in some LODs really late. In either case we want
			// to pop rather than get stuck in a dissolve that started way too late.
			if (rState.nOldLod >= 0 && nLod >= 0 &&
			    (rState.nOldLod > nLod + 1 || rState.nOldLod < nLod - 1)
			    )
				rState.nOldLod = nLod;

			// when we first load before streaming we get a lod of -1. When a lod streams in
			// we kick off a transition to N, but without moving there's nothing to continue the transition.
			// Catch this case when we claim to be in lod -1 but are too close, and snap.
			if (rState.nOldLod == -1 && fEntDistance < 0.5f * m_fWSMaxViewDist)
				rState.nOldLod = nLod;

			uint32 prevState = (((uint32)rState.nOldLod) << 8) | rState.nNewLod;

			float fDissolve = GetObjManager()->GetLodDistDissolveRef(&rState, fEntDistance, nLod, passInfo);

			uint32 newState = (((uint32)rState.nOldLod) << 8) | rState.nNewLod;

			// ensure old lod is still around. If not find closest lod
			if (rState.nOldLod != rState.nNewLod && rState.nOldLod >= 0)
			{
				rState.nOldLod = pStatObj->FindNearesLoadedLOD(rState.nOldLod, true);
			}
			else if (rState.nOldLod >= 0)
			{
				// we can actually fall back into this case (even though we know nLod is valid).
				rState.nOldLod = rState.nNewLod = pStatObj->FindNearesLoadedLOD(rState.nOldLod, true);
			}

			// only bother to check if we are dissolving and we've just kicked off a new dissolve transition
			if (rState.nOldLod != rState.nNewLod && prevState != newState)
			{
				// LOD cutoff point, this is about where the transition should be triggered.
				const float fEntityLodRatio = std::max(GetLodRatioNormalized(), FLT_MIN);
				const float fDistMultiplier = 1.0f / (fEntityLodRatio * Get3DEngine()->GetFrameLodInfo().fTargetSize);
				float dist = pStatObj->GetLodDistance() * max(rState.nOldLod, rState.nNewLod) * fDistMultiplier;

				// we started way too late, most likely object LOD streamed in very late, just snap.
				if (fabsf(rState.fStartDist - dist) > GetFloatCVar(e_DissolveDistband))
				{
					rState.nOldLod = rState.nNewLod;
				}
			}

			nDissolveRefA = (uint8)(255.f * SATURATE(fDissolve));
			nLodA = rState.nOldLod;
			nLodB = rState.nNewLod;
		}
		else
		{
			nDissolveRefA = 0;

			nLodA = CLAMP(wantedLod, pStatObj->GetMinUsableLod(), (int)pStatObj->m_nMaxUsableLod);
			if (!(pStatObj->m_nFlags & STATIC_OBJECT_COMPOUND))
				nLodA = pStatObj->FindNearesLoadedLOD(nLodA, true);
			nLodB = -1;
		}

		if (pCVars->e_Dissolve && !passInfo.IsCachedShadowPass())
		{
			float fDissolveDist = CLAMP(0.1f * m_fWSMaxViewDist, GetFloatCVar(e_DissolveDistMin), GetFloatCVar(e_DissolveDistMax));

			const float fDissolveStartDist = m_fWSMaxViewDist - fDissolveDist;

			if (fEntDistance > fDissolveStartDist)
			{
				float fDissolve = (fEntDistance - fDissolveStartDist)
				                  / fDissolveDist;
				nDissolveRefA = (uint8)(255.f * SATURATE(fDissolve));
				nLodB = -1;
			}
		}
	}

	return CLodValue(nLodA, nDissolveRefA, nLodB);
}

void CBrush::Render(const struct SRendParams& _EntDrawParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pStatObj || m_dwRndFlags & ERF_HIDDEN)
		return; //false;

	if (m_dwRndFlags & ERF_COLLISION_PROXY || m_dwRndFlags & ERF_RAYCAST_PROXY)
	{
		// Collision proxy is visible in Editor while in editing mode.
		if (!gEnv->IsEditor() || !gEnv->IsEditing())
		{
			if (GetCVars()->e_DebugDraw == 0)
				return; //true;
		}
	}

	// some parameters will be modified
	SRendParams rParms = _EntDrawParams;

	if (m_nMaterialLayers)
		rParms.nMaterialLayers = m_nMaterialLayers;

	rParms.pMatrix = &m_Matrix;
	rParms.nClipVolumeStencilRef = 0;
	rParms.pMaterial = m_pMaterial;

	if (!m_Matrix.m01 && !m_Matrix.m02 && !m_Matrix.m10 && !m_Matrix.m12 && !m_Matrix.m20 && !m_Matrix.m21)
		rParms.dwFObjFlags &= ~FOB_TRANS_ROTATE;
	else
		rParms.dwFObjFlags |= FOB_TRANS_ROTATE;

	// get statobj for rendering
	IStatObj* pStatObj = m_pStatObj;

	// render
	if (pStatObj)
	{
		pStatObj->Render(rParms, passInfo);
	}
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )                //AMD Port
#endif

void CBrush::SetMatrix(const Matrix34& mat)
{
	Get3DEngine()->UnRegisterEntityAsJob(this);

	bool replacePhys = false;

	if (!IsMatrixValid(mat))
	{
		Warning("Error: IRenderNode::SetMatrix: Invalid matrix passed from the editor - ignored, reset to identity: %s", GetName());
		replacePhys = true;
		m_Matrix.SetIdentity();
	}
	else
	{
		replacePhys = fabs(mat.GetColumn(0).len() - m_Matrix.GetColumn(0).len())
		              + fabs(mat.GetColumn(1).len() - m_Matrix.GetColumn(1).len())
		              + fabs(mat.GetColumn(2).len() - m_Matrix.GetColumn(2).len()) > FLT_EPSILON;
		m_Matrix = mat;
	}
	pe_params_foreign_data foreignData;
	foreignData.iForeignFlags = 0;
	if (!replacePhys && m_pPhysEnt)
	{
		m_pPhysEnt->GetParams(&foreignData);
		replacePhys = !(foreignData.iForeignFlags & PFF_OUTDOOR_AREA) != !(m_dwRndFlags & ERF_NODYNWATER);
	}

	CalcBBox();

	Get3DEngine()->RegisterEntity(this);
	if (replacePhys)
		Dephysicalize();
	if (!m_pPhysEnt)
		Physicalize();
	else
	{
		// Just move physics.
		pe_status_placeholder spc;
		if (m_pPhysEnt->GetStatus(&spc) && !spc.pFullEntity)
		{
			pe_params_bbox pbb;
			pbb.BBox[0] = m_WSBBox.min;
			pbb.BBox[1] = m_WSBBox.max;
			m_pPhysEnt->SetParams(&pbb);
		}
		else
		{
			pe_params_pos par_pos;
			par_pos.pos = m_Matrix.GetTranslation();
			par_pos.q = Quat(Matrix33(m_Matrix) * Diag33(m_Matrix.GetColumn(0).len(), m_Matrix.GetColumn(1).len(), m_Matrix.GetColumn(2).len()).invert());
			m_pPhysEnt->SetParams(&par_pos);
		}

		//////////////////////////////////////////////////////////////////////////
		// Update physical flags.
		//////////////////////////////////////////////////////////////////////////
		if (m_dwRndFlags & ERF_HIDABLE)
			foreignData.iForeignFlags |= PFF_HIDABLE;
		else
			foreignData.iForeignFlags &= ~PFF_HIDABLE;
		if (m_dwRndFlags & ERF_HIDABLE_SECONDARY)
			foreignData.iForeignFlags |= PFF_HIDABLE_SECONDARY;
		else
			foreignData.iForeignFlags &= ~PFF_HIDABLE_SECONDARY;
		// flag to exclude from AI triangulation
		if (m_dwRndFlags & ERF_EXCLUDE_FROM_TRIANGULATION)
			foreignData.iForeignFlags |= PFF_EXCLUDE_FROM_STATIC;
		else
			foreignData.iForeignFlags &= ~PFF_EXCLUDE_FROM_STATIC;
		m_pPhysEnt->SetParams(&foreignData);
	}

	if (m_pDeform)
		m_pDeform->BakeDeform(m_Matrix);
}

void CBrush::CalcBBox()
{
	m_WSBBox.min = SetMaxBB();
	m_WSBBox.max = SetMinBB();

	if (!m_pStatObj)
		return;

	m_WSBBox.min = m_pStatObj->GetBoxMin();
	m_WSBBox.max = m_pStatObj->GetBoxMax();
	m_WSBBox.SetTransformedAABB(m_Matrix, m_WSBBox);
	m_fMatrixScale = m_Matrix.GetColumn0().GetLength();
}

CBrush::CBrush()
{
	m_WSBBox.min = m_WSBBox.max = Vec3(ZERO);
	m_dwRndFlags = 0;
	m_Matrix.SetIdentity();
	m_pPhysEnt = 0;
	m_Matrix.SetIdentity();
	m_pMaterial = 0;
	m_nLayerId = 0;
	m_pStatObj = NULL;
	m_pMaterial = NULL;
	m_bVehicleOnlyPhysics = false;
	m_bMerged = 0;
	m_bDrawLast = false;
	m_fMatrixScale = 1.f;
	m_collisionClassIdx = 0;
	m_pDeform = NULL;

	GetInstCount(GetRenderNodeType())++;
}

CBrush::~CBrush()
{
	INDENT_LOG_DURING_SCOPE(true, "Destroying brush \"%s\"", this->GetName());

	Dephysicalize();
	Get3DEngine()->FreeRenderNodeState(this);

	m_pStatObj = NULL;
	if (m_pDeform)
		delete m_pDeform;

	if (m_pTempData)
		Get3DEngine()->FreeRenderNodeTempData(&m_pTempData);
	assert(!m_pTempData);

	GetInstCount(GetRenderNodeType())--;
}

void CBrush::Physicalize(bool bInstant)
{
	PhysicalizeOnHeap(NULL, bInstant);
}

void CBrush::PhysicalizeOnHeap(IGeneralMemoryHeap* pHeap, bool bInstant)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Physics, 0, "Brush: %s", m_pStatObj ? m_pStatObj->GetFilePath() : "(unknown)");

	if (m_pStatObj && (m_pStatObj->GetBreakableByGame() || m_pStatObj->GetIDMatBreakable() != -1))
	{
		pHeap = m_p3DEngine->GetBreakableBrushHeap();
	}

	float fScaleX = m_Matrix.GetColumn(0).len();
	float fScaleY = m_Matrix.GetColumn(1).len();
	float fScaleZ = m_Matrix.GetColumn(2).len();

	if (!m_pStatObj || !m_pStatObj->IsPhysicsExist())
	{
		// skip non uniform scaled object or objects without physics

		// Check if we are acompound object.
		if (m_pStatObj && !m_pStatObj->IsPhysicsExist() && (m_pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND))
		{
			// Try to physicalize compound object.
		}
		else
		{
			Dephysicalize();
			return;
		}
	}

	AABB WSBBox = GetBBox();
	bool notPodable = max(WSBBox.max.x - WSBBox.min.x, WSBBox.max.y - WSBBox.min.y) > Get3DEngine()->GetCVars()->e_OnDemandMaxSize;
	if (!(GetCVars()->e_OnDemandPhysics & 0x2)
	    || notPodable
	    || (m_pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND))
		bInstant = true;

	if (m_pDeform)
	{
		m_pDeform->CreateDeformableSubObject(true, GetMatrix(), pHeap);
	}

	if (!bInstant)
	{
		gEnv->pPhysicalWorld->RegisterBBoxInPODGrid(&WSBBox.min);
		return;
	}

	// create new
	if (!m_pPhysEnt)
	{
		m_pPhysEnt = GetSystem()->GetIPhysicalWorld()->CreatePhysicalEntity(PE_STATIC, NULL, (IRenderNode*)this, PHYS_FOREIGN_ID_STATIC, -1, pHeap);
		if (!m_pPhysEnt)
			return;
	}

	pe_action_remove_all_parts remove_all;
	m_pPhysEnt->Action(&remove_all);

	pe_geomparams params;
	if (m_pStatObj->GetPhysGeom(PHYS_GEOM_TYPE_DEFAULT))
	{
		if (GetRndFlags() & ERF_COLLISION_PROXY)
		{
			// Collision proxy only collides with players and vehicles.
			params.flags = geom_colltype_player | geom_colltype_vehicle;
		}
		if (GetRndFlags() & ERF_RAYCAST_PROXY)
		{
			// Collision proxy only collides with players and vehicles.
			params.flags = geom_colltype_ray;
		}
		/*if (m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE])
		   params.flags &= ~geom_colltype_ray;*/
		if (m_bVehicleOnlyPhysics || (m_pStatObj->GetVehicleOnlyPhysics() != 0))
			params.flags = geom_colltype_vehicle;
		if (GetCVars()->e_ObjQuality != CONFIG_LOW_SPEC)
		{
			params.idmatBreakable = m_pStatObj->GetIDMatBreakable();
			if (m_pStatObj->GetBreakableByGame())
				params.flags |= geom_manually_breakable;
		}
		else
			params.idmatBreakable = -1;
	}

	Matrix34 mtxScale;
	mtxScale.SetScale(Vec3(fScaleX, fScaleY, fScaleZ));
	params.pMtx3x4 = &mtxScale;
	m_pStatObj->Physicalize(m_pPhysEnt, &params);

	if (m_dwRndFlags & (ERF_HIDABLE | ERF_HIDABLE_SECONDARY | ERF_EXCLUDE_FROM_TRIANGULATION | ERF_NODYNWATER))
	{
		pe_params_foreign_data foreignData;
		m_pPhysEnt->GetParams(&foreignData);
		if (m_dwRndFlags & ERF_HIDABLE)
			foreignData.iForeignFlags |= PFF_HIDABLE;
		if (m_dwRndFlags & ERF_HIDABLE_SECONDARY)
			foreignData.iForeignFlags |= PFF_HIDABLE_SECONDARY;
		//[PETAR] new flag to exclude from triangulation
		if (m_dwRndFlags & ERF_EXCLUDE_FROM_TRIANGULATION)
			foreignData.iForeignFlags |= PFF_EXCLUDE_FROM_STATIC;
		if (m_dwRndFlags & ERF_NODYNWATER)
			foreignData.iForeignFlags |= PFF_OUTDOOR_AREA;
		m_pPhysEnt->SetParams(&foreignData);
	}

	pe_params_flags par_flags;
	par_flags.flagsOR = pef_never_affect_triggers | pef_log_state_changes;
	m_pPhysEnt->SetParams(&par_flags);

	pe_params_pos par_pos;
	par_pos.pos = m_Matrix.GetTranslation();
	par_pos.q = Quat(Matrix33(m_Matrix) * Diag33(fScaleX, fScaleY, fScaleZ).invert());
	par_pos.bEntGridUseOBB = 1;
	m_pPhysEnt->SetParams(&par_pos);

	pe_params_collision_class pcc;
	Get3DEngine()->GetCollisionClass(pcc.collisionClassOR, m_collisionClassIdx);
	m_pPhysEnt->SetParams(&pcc);

	if (m_pMaterial)
		UpdatePhysicalMaterials();

	if (GetRndFlags() & ERF_NODYNWATER)
	{
		pe_params_part ppart;
		ppart.flagsAND = ~geom_floats;
		m_pPhysEnt->SetParams(&ppart);
	}
}

bool CBrush::PhysicalizeFoliage(bool bPhysicalize, int iSource, int nSlot)
{
	if (nSlot < 0)
	{
		bool res = false;
		for (int i = 0; i < m_pStatObj->GetSubObjectCount(); i++)
			res = res || PhysicalizeFoliage(bPhysicalize, iSource, i);
		return res;
	}

	if (IStatObj::SSubObject* pSubObj = m_pStatObj->GetSubObject(nSlot))
		if (bPhysicalize)
		{
			if (!pSubObj->pStatObj || !((CStatObj*)pSubObj->pStatObj)->m_nSpines)
				return false;
			if (!(m_pStatObj->GetFlags() & STATIC_OBJECT_CLONE))
			{
				m_pStatObj = m_pStatObj->Clone(false, false, false);
				pSubObj = m_pStatObj->GetSubObject(nSlot);
			}
			Matrix34 mtx = m_Matrix * pSubObj->localTM;
			pSubObj->pStatObj->PhysicalizeFoliage(m_pPhysEnt, mtx, pSubObj->pFoliage, GetCVars()->e_FoliageBranchesTimeout, iSource);
			return pSubObj->pFoliage != 0;
		}
		else if (pSubObj->pFoliage)
		{
			pSubObj->pFoliage->Release();
			return true;
		}
	return false;
}

IFoliage* CBrush::GetFoliage(int nSlot)
{
	if (IStatObj::SSubObject* pSubObj = m_pStatObj->GetSubObject(nSlot))
		return pSubObj->pFoliage;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CBrush::UpdatePhysicalMaterials(int bThreadSafe)
{
	if (!m_pPhysEnt)
		return;

	if ((GetRndFlags() & ERF_COLLISION_PROXY) && m_pPhysEnt)
	{
		pe_params_part ppart;
		ppart.flagsAND = 0;
		ppart.flagsOR = geom_colltype_player | geom_colltype_vehicle;
		m_pPhysEnt->SetParams(&ppart);
	}

	if ((GetRndFlags() & ERF_RAYCAST_PROXY) && m_pPhysEnt)
	{
		pe_params_part ppart;
		ppart.flagsAND = 0;
		ppart.flagsOR = geom_colltype_ray;
		m_pPhysEnt->SetParams(&ppart);
	}

	if (m_pMaterial)
	{
		// Assign custom material to physics.
		int surfaceTypesId[MAX_SUB_MATERIALS];
		memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
		int i, numIds = m_pMaterial->FillSurfaceTypeIds(surfaceTypesId);
		ISurfaceTypeManager* pSurfaceTypeManager = Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager();
		ISurfaceType* pMat;
		bool bBreakable = false;

		for (i = 0, m_bVehicleOnlyPhysics = false; i < numIds; i++)
			if (pMat = pSurfaceTypeManager->GetSurfaceType(surfaceTypesId[i]))
			{
				if (pMat->GetFlags() & SURFACE_TYPE_VEHICLE_ONLY_COLLISION)
					m_bVehicleOnlyPhysics = true;
				if (pMat->GetBreakability())
					bBreakable = true;
			}

		if (bBreakable && m_pStatObj)
		{
			// mark the rendermesh as KepSysMesh so that it is kept in system memory
			if (m_pStatObj->GetRenderMesh())
				m_pStatObj->GetRenderMesh()->KeepSysMesh(true);

			m_pStatObj->SetFlags(m_pStatObj->GetFlags() | STATIC_OBJECT_DYNAMIC);
		}
		pe_params_part ppart;
		ppart.nMats = numIds;
		ppart.pMatMapping = surfaceTypesId;
		if (m_bVehicleOnlyPhysics)
			ppart.flagsAND = geom_colltype_vehicle;
		if ((pMat = m_pMaterial->GetSurfaceType()) && pMat->GetPhyscalParams().collType >= 0)
			ppart.flagsAND = ~(geom_collides | geom_floats), ppart.flagsOR = pMat->GetPhyscalParams().collType;
		m_pPhysEnt->SetParams(&ppart, bThreadSafe);
	}
	else if (m_bVehicleOnlyPhysics)
	{
		m_bVehicleOnlyPhysics = false;
		if (!m_pStatObj->GetVehicleOnlyPhysics())
		{
			pe_params_part ppart;
			ppart.flagsOR = geom_colltype_solid | geom_colltype_ray | geom_floats | geom_colltype_explosion;
			m_pPhysEnt->SetParams(&ppart, bThreadSafe);
		}
	}
}

void CBrush::Dephysicalize(bool bKeepIfReferenced)
{
	AABB WSBBox = GetBBox();

	// delete old physics
	if (m_pPhysEnt && 0 != GetSystem()->GetIPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt, ((int)bKeepIfReferenced) * 4))
		m_pPhysEnt = 0;

	if (!bKeepIfReferenced)
	{
		if (m_pDeform)
		{
			m_pDeform->CreateDeformableSubObject(false, GetMatrix(), NULL);
		}
	}
}

void CBrush::Dematerialize()
{
	if (m_pMaterial)
		m_pMaterial = 0;

	UpdateExecuteAsPreProcessJobFlag();
}

IPhysicalEntity* CBrush::GetPhysics() const
{
	return m_pPhysEnt;
}

//////////////////////////////////////////////////////////////////////////
void CBrush::SetPhysics(IPhysicalEntity* pPhys)
{
	m_pPhysEnt = pPhys;
}

//////////////////////////////////////////////////////////////////////////
bool CBrush::IsMatrixValid(const Matrix34& mat)
{
	Vec3 vScaleTest = mat.TransformVector(Vec3(0, 0, 1));
	float fDist = mat.GetTranslation().GetDistance(Vec3(0, 0, 0));

	if (vScaleTest.GetLength() > 1000.f || vScaleTest.GetLength() < 0.01f || fDist > 256000 ||
	    !_finite(vScaleTest.x) || !_finite(vScaleTest.y) || !_finite(vScaleTest.z))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBrush::SetMaterial(IMaterial* pMat)
{
	m_pMaterial = pMat;

	bool bCollisionProxy = false;
	bool bRaycastProxy = false;

	if (pMat)
	{
		if (pMat->GetFlags() & MTL_FLAG_COLLISION_PROXY)
			bCollisionProxy = true;

		if (pMat->GetFlags() & MTL_FLAG_RAYCAST_PROXY)
		{
			bRaycastProxy = true;
		}
	}
	SetRndFlags(ERF_COLLISION_PROXY, bCollisionProxy);
	SetRndFlags(ERF_RAYCAST_PROXY, bRaycastProxy);

	UpdatePhysicalMaterials();

	// register and get brush material id
	m_pMaterial = pMat;

	InvalidatePermanentRenderObject();
	UpdateExecuteAsPreProcessJobFlag();
}

void CBrush::UpdateExecuteAsPreProcessJobFlag()
{
	IMaterial* pMat = GetMaterial();
	m_bExecuteAsPreprocessJob = false;

	// check if this Brush needs to be executed as a preprocess job
	if (pMat)
	{
		SShaderItem& shaderItem = pMat->GetShaderItem();
		if (shaderItem.m_pShader)
		{
			uint32 nFlags = shaderItem.m_pShader->GetFlags2();
			m_bExecuteAsPreprocessJob = m_bExecuteAsPreprocessJob || ((nFlags & EF2_FORCE_WATERPASS) != 0);
		}

		// also check submaterials
		for (int i = 0, nNum = pMat->GetSubMtlCount(); i < nNum; ++i)
		{
			SShaderItem& subShaderItem = pMat->GetShaderItem(i);
			if (subShaderItem.m_pShader)
			{
				uint32 nFlags = subShaderItem.m_pShader->GetFlags2();
				m_bExecuteAsPreprocessJob = m_bExecuteAsPreprocessJob || ((nFlags & EF2_FORCE_WATERPASS) != 0);
			}
		}

#if defined(FEATURE_SVO_GI)
		if (pMat && (GetCVars()->e_svoTI_Active >= 0) && (gEnv->IsEditor() || GetCVars()->e_svoTI_Apply))
			pMat->SetKeepLowResSysCopyForDiffTex();
#endif
	}
}

void CBrush::CheckPhysicalized()
{
	if (!m_pPhysEnt)
		Physicalize();
}

void CBrush::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "Brush");
	pSizer->AddObject(this, sizeof(*this));
}

void CBrush::SetEntityStatObj(unsigned int nSlot, IStatObj* pStatObj, const Matrix34A* pMatrix)
{
	//assert(pStatObj);

	IStatObj* pPrevStatObj = m_pStatObj;

	m_pStatObj = (CStatObj*)pStatObj;

	if (pMatrix)
		SetMatrix(*pMatrix);

	// If object differ we must re-physicalize.
	if (pStatObj != pPrevStatObj)
		if (!pPrevStatObj || !pStatObj || (pStatObj->GetCloneSourceObject() != pPrevStatObj))// ||
			//(pStatObj->GetFlags() & (STATIC_OBJECT_GENERATED|STATIC_OBJECT_CLONE))==STATIC_OBJECT_CLONE)
			Physicalize();

	if (m_pTempData)
		Get3DEngine()->FreeRenderNodeTempData(&m_pTempData);
	assert(!m_pTempData);

	m_nInternalFlags |= UPDATE_DECALS;

	if (m_pStatObj && m_pStatObj->IsDeformable())
	{
		if (!m_pDeform)
			m_pDeform = new CDeformableNode(m_nSID);
		m_pDeform->SetStatObj(static_cast<CStatObj*>(m_pStatObj.get()));
		m_pDeform->BakeDeform(GetMatrix());
	}
	else
	{
		SAFE_DELETE(m_pDeform);
	}

	UpdateExecuteAsPreProcessJobFlag();
}

void CBrush::SetStatObj(IStatObj* pStatObj)
{
	m_pStatObj = pStatObj;
	if (m_pStatObj && m_pStatObj->IsDeformable())
	{
		if (!m_pDeform)
			m_pDeform = new CDeformableNode(m_nSID);
		m_pDeform->SetStatObj(static_cast<CStatObj*>(m_pStatObj.get()));
		m_pDeform->BakeDeform(GetMatrix());
	}
	else
		SAFE_DELETE(m_pDeform);

	UpdateExecuteAsPreProcessJobFlag();
	InvalidatePermanentRenderObject();
}

IRenderNode* CBrush::Clone() const
{
	CBrush* pDestBrush = new CBrush();

	//CBrush member vars
	//	potential issues with Smart Pointers
	pDestBrush->m_Matrix = m_Matrix;
	pDestBrush->m_fMatrixScale = m_fMatrixScale;
	//pDestBrush->m_pPhysEnt		//Don't want to copy the phys ent pointer
	pDestBrush->m_pMaterial = m_pMaterial;
	pDestBrush->m_pStatObj = m_pStatObj;

	pDestBrush->m_bVehicleOnlyPhysics = m_bVehicleOnlyPhysics;
	pDestBrush->m_bMerged = m_bMerged;
	pDestBrush->m_bDrawLast = m_bDrawLast;
	pDestBrush->m_WSBBox = m_WSBBox;

	//IRenderNode member vars
	//	We cannot just copy over due to issues with the linked list of IRenderNode objects
	CopyIRenderNodeData(pDestBrush);

	pDestBrush->UpdateExecuteAsPreProcessJobFlag();

	return pDestBrush;
}

IRenderMesh* CBrush::GetRenderMesh(int nLod)
{
	IStatObj* pStatObj = m_pStatObj ? m_pStatObj->GetLodObject(nLod) : NULL;
	return pStatObj ? pStatObj->GetRenderMesh() : NULL;
}

void CBrush::OffsetPosition(const Vec3& delta)
{
	if (m_pTempData) m_pTempData->OffsetPosition(delta);
	m_Matrix.SetTranslation(m_Matrix.GetTranslation() + delta);
	m_WSBBox.Move(delta);

	if (m_pPhysEnt)
	{
		pe_params_pos par_pos;
		par_pos.pos = m_Matrix.GetTranslation();
		m_pPhysEnt->SetParams(&par_pos);
	}
}

bool CBrush::GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const
{
	const float fEntityLodRatio = GetLodRatioNormalized();
	if (fEntityLodRatio > 0.0f)
	{
		const float fDistMultiplier = 1.0f / (fEntityLodRatio * frameLodInfo.fTargetSize);
		const float lodDistance = m_pStatObj ? m_pStatObj->GetLodDistance() : FLT_MAX;

		for (uint i = 0; i < SMeshLodInfo::s_nMaxLodCount; ++i)
		{
			distances[i] = lodDistance * (i + 1) * fDistMultiplier;
		}
	}
	else
	{
		for (uint i = 0; i < SMeshLodInfo::s_nMaxLodCount; ++i)
		{
			distances[i] = FLT_MAX;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
void CBrush::Render(const CLodValue& lodValue, const SRenderingPassInfo& passInfo, SSectorTextureSet* pTerrainTexInfo, PodArray<CDLight*>* pAffectingLights)
{
	FUNCTION_PROFILER_3DENGINE;
	CVars* pCVars = GetCVars();

	// Collision proxy is visible in Editor while in editing mode.
	if (m_dwRndFlags & ERF_COLLISION_PROXY)
	{
		if (!gEnv->IsEditor() || !gEnv->IsEditing())
			if (pCVars->e_DebugDraw == 0)
				return;
	}
	if (m_dwRndFlags & ERF_HIDDEN)
		return;

	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	const Vec3 vObjCenter = CBrush::GetBBox().GetCenter();
	const Vec3 vObjPos = CBrush::GetPos();

	SRenderNodeTempData::SUserData& userData = m_pTempData->userData;
	CRenderObject* pObj = 0;

	if (GetObjManager()->AddOrCreatePersistentRenderObject(m_pTempData, pObj, &lodValue, passInfo))
		return;

	pObj->m_fDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

	pObj->m_pRenderNode = this;
	pObj->m_II.m_Matrix = userData.objMat;
	pObj->m_fAlpha = 1.f;
	IF(!m_bDrawLast, 1)
		pObj->m_nSort = fastround_positive(pObj->m_fDistance * 2.0f);
	else
		pObj->m_fSort = 10000.0f;

	IMaterial* pMat = pObj->m_pCurrMaterial = CBrush::GetMaterial();
	pObj->m_ObjFlags |= FOB_INSHADOW | FOB_TRANS_MASK;

	if (!userData.objMat.m01 && !userData.objMat.m02 && !userData.objMat.m10 && !userData.objMat.m12 && !userData.objMat.m20 && !userData.objMat.m21)
		pObj->m_ObjFlags &= ~FOB_TRANS_ROTATE;
	else
		pObj->m_ObjFlags |= FOB_TRANS_ROTATE;

	if (m_dwRndFlags & ERF_NO_DECALNODE_DECALS)
		pObj->m_ObjFlags |= FOB_DYNAMIC_OBJECT;
	else
		pObj->m_ObjFlags &= ~FOB_DYNAMIC_OBJECT;

	if (uint8 nMaterialLayers = IRenderNode::GetMaterialLayers())
	{
		uint8 nFrozenLayer = (nMaterialLayers & MTL_LAYER_FROZEN) ? MTL_LAYER_FROZEN_MASK : 0;
		uint8 nWetLayer = (nMaterialLayers & MTL_LAYER_WET) ? MTL_LAYER_WET_MASK : 0;
		pObj->m_nMaterialLayers = (uint32)(nFrozenLayer << 24) | (nWetLayer << 16);
	}
	else
		pObj->m_nMaterialLayers = 0;

	if (!passInfo.IsShadowPass() && m_nInternalFlags & IRenderNode::REQUIRES_NEAREST_CUBEMAP)
	{
		if (!(pObj->m_nTextureID = GetObjManager()->CheckCachedNearestCubeProbe(this)) || !pCVars->e_CacheNearestCubePicking)
			pObj->m_nTextureID = GetObjManager()->GetNearestCubeProbe(pAffectingLights, m_pOcNode->m_pVisArea, CBrush::GetBBox());

		m_pTempData->userData.nCubeMapId = pObj->m_nTextureID;
	}

	//////////////////////////////////////////////////////////////////////////
	// temp fix to update ambient color (Vlad please review!)
	pObj->m_nClipVolumeStencilRef = userData.m_pClipVolume ? userData.m_pClipVolume->GetStencilRef() : 0;
	if (m_pOcNode && m_pOcNode->m_pVisArea)
		pObj->m_II.m_AmbColor = m_pOcNode->m_pVisArea->GetFinalAmbientColor();
	else
		pObj->m_II.m_AmbColor = Get3DEngine()->GetSkyColor();
	//////////////////////////////////////////////////////////////////////////

	if (pTerrainTexInfo)
	{
		bool bUseTerrainColor = (GetCVars()->e_BrushUseTerrainColor == 2);
		if (pMat && GetCVars()->e_BrushUseTerrainColor == 1)
		{
			if (pMat->GetSafeSubMtl(0)->GetFlags() & MTL_FLAG_BLEND_TERRAIN)
				bUseTerrainColor = true;
		}

		if (bUseTerrainColor)
		{
			m_pTempData->userData.bTerrainColorWasUsed = true;
			pObj->m_ObjFlags |= FOB_BLEND_WITH_TERRAIN_COLOR;

			pObj->m_data.m_pTerrainSectorTextureInfo = pTerrainTexInfo;
			pObj->m_nTextureID = -(int)pTerrainTexInfo->nSlot0 - 1; // nTextureID is set only for proper batching, actual texture id is same for all terrain sectors

			{
				float fBlendDistance = GetCVars()->e_VegetationUseTerrainColorDistance;

				if (fBlendDistance == 0)
				{
					pObj->m_data.m_fMaxViewDistance = m_fWSMaxViewDist;
				}
				else if (fBlendDistance > 0)
				{
					pObj->m_data.m_fMaxViewDistance = fBlendDistance;
				}
				else // if(fBlendDistance < 0)
				{
					pObj->m_data.m_fMaxViewDistance = abs(fBlendDistance) * GetCVars()->e_ViewDistRatio;
				}
			}
		}
		else
			pObj->m_ObjFlags &= ~FOB_BLEND_WITH_TERRAIN_COLOR;
	}

	// check the object against the water level
	if (CObjManager::IsAfterWater(vObjCenter, vCamPos, passInfo, Get3DEngine()->GetWaterLevel()))
		pObj->m_ObjFlags |= FOB_AFTER_WATER;
	else
		pObj->m_ObjFlags &= ~FOB_AFTER_WATER;

	if (CStatObj* pStatObj = (CStatObj*)CBrush::GetEntityStatObj())
	{
		// temporary fix for autoreload from max export, Vladimir needs to properly fix it!
		if (pObj->m_pCurrMaterial != GetMaterial())
			pObj->m_pCurrMaterial = GetMaterial();

		if (Get3DEngine()->IsTessellationAllowed(pObj, passInfo, true))
		{
			// Allow this RO to be tessellated, however actual tessellation will be applied if enabled in material
			pObj->m_ObjFlags |= FOB_ALLOW_TESSELLATION;
		}
		else
			pObj->m_ObjFlags &= ~FOB_ALLOW_TESSELLATION;

		if (pCVars->e_BBoxes)
			GetObjManager()->RenderObjectDebugInfo((IRenderNode*)this, pObj->m_fDistance, passInfo);

		if (lodValue.LodA() <= 0 && Cry3DEngineBase::GetCVars()->e_MergedMeshes != 0 && m_pDeform && m_pDeform->HasDeformableData())
		{
			if (GetCVars()->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
			{
				GetObjManager()->PushIntoCullOutputQueue(SCheckOcclusionOutput::CreateDeformableBrushOutput(this, gEnv->pRenderer->EF_DuplicateRO(pObj, passInfo), lodValue.LodA(), passInfo));
			}
			else
			{
				m_pDeform->RenderInternalDeform(gEnv->pRenderer->EF_DuplicateRO(pObj, passInfo), lodValue.LodA(), CBrush::GetBBox(), passInfo);
			}
		}

		pStatObj->RenderInternal(pObj, 0, lodValue, passInfo);
	}
}

///////////////////////////////////////////////////////////////////////////////
IStatObj* CBrush::GetEntityStatObj(unsigned int nPartId, unsigned int nSubPartId, Matrix34A* pMatrix, bool bReturnOnlyVisible)
{
	if (nPartId != 0)
		return 0;

	if (pMatrix)
		*pMatrix = m_Matrix;

	return m_pStatObj;
}

///////////////////////////////////////////////////////////////////////////////
void CBrush::OnRenderNodeBecomeVisible(const SRenderingPassInfo& passInfo)
{
	assert(m_pTempData);
	SRenderNodeTempData::SUserData& userData = m_pTempData->userData;

	userData.objMat = m_Matrix;
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

	userData.nWantedLod = CObjManager::GetObjectLOD(this, fEntDistance);

	int nLod = userData.nWantedLod;

	if (CStatObj* pStatObj = (CStatObj*)CBrush::GetEntityStatObj())
	{
		nLod = CLAMP(nLod, pStatObj->GetMinUsableLod(), (int)pStatObj->m_nMaxUsableLod);
		nLod = pStatObj->FindNearesLoadedLOD(nLod);
	}

	userData.lodDistDissolveTransitionState.nNewLod = userData.lodDistDissolveTransitionState.nOldLod = userData.nWantedLod;
	userData.lodDistDissolveTransitionState.fStartDist = 0.0f;
	userData.lodDistDissolveTransitionState.bFarside = false;
}

///////////////////////////////////////////////////////////////////////////////
bool CBrush::CanExecuteRenderAsJob()
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////
void CBrush::FillBBox(AABB& aabb)
{
	aabb = CBrush::GetBBox();
}

///////////////////////////////////////////////////////////////////////////////
EERType CBrush::GetRenderNodeType()
{
	return eERType_Brush;
}

///////////////////////////////////////////////////////////////////////////////
float CBrush::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, min(GetFloatCVar(e_ViewDistCompMaxSize), CBrush::GetBBox().GetRadius()) * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_ViewDistMin, min(GetFloatCVar(e_ViewDistCompMaxSize), CBrush::GetBBox().GetRadius()) * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized());
}

///////////////////////////////////////////////////////////////////////////////
Vec3 CBrush::GetPos(bool bWorldOnly) const
{
	assert(bWorldOnly);
	return m_Matrix.GetTranslation();
}

///////////////////////////////////////////////////////////////////////////////
IMaterial* CBrush::GetMaterial(Vec3* pHitPos) const
{
	if (m_pMaterial)
		return m_pMaterial;

	if (m_pStatObj)
		return m_pStatObj->GetMaterial();

	return NULL;
}
