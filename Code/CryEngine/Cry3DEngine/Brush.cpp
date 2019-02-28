// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

CBrush::CBrush()
	: m_bVehicleOnlyPhysics(0)
	, m_bDrawLast(0)
	, m_bNoPhysicalize(0)
{
	m_WSBBox.min = m_WSBBox.max = Vec3(ZERO);
	m_dwRndFlags = 0;
	m_Matrix.SetIdentity();
	m_pPhysEnt = 0;
	m_pMaterial = 0;
	m_nLayerId = 0;
	m_pStatObj = NULL;
	m_pMaterial = NULL;
	m_collisionClassIdx = 0;
	m_pDeform = NULL;

	GetInstCount(GetRenderNodeType())++;
}

CBrush::~CBrush()
{
	INDENT_LOG_DURING_SCOPE(true, "Destroying brush \"%s\"", this->GetName());

	if (m_pFoliage)
	{
		static IFoliage* g_pFoliage0 = nullptr;
		((CStatObjFoliage*)m_pFoliage)->m_ppThis = &g_pFoliage0;
		m_pFoliage->Release();
	}
	Dephysicalize();
	Get3DEngine()->FreeRenderNodeState(this);

	m_pStatObj = NULL;
	if (m_pDeform)
		delete m_pDeform;

	if (m_pCameraSpacePos)
	{
		delete m_pCameraSpacePos;
		m_pCameraSpacePos = nullptr;
	}

	RemoveAndMarkForAutoDeleteTempData();

	GetInstCount(GetRenderNodeType())--;
}

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
	int nLodA = -1;

	if (CStatObj* pStatObj = m_pStatObj)
	{
		const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
		const float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

		// update LOD faster in zoom mode
		if (passInfo.IsGeneralPass() && passInfo.IsZoomActive())
		{
			wantedLod = CObjManager::GetObjectLOD(this, fEntDistance);

			if (auto pTempData = m_pTempData.load())
				pTempData->userData.nWantedLod = wantedLod;
		}

		nLodA = CLAMP(wantedLod, pStatObj->GetMinUsableLod(), (int)pStatObj->m_nMaxUsableLod);

		if (!(pStatObj->m_nFlags & STATIC_OBJECT_COMPOUND))
			nLodA = pStatObj->FindNearestLoadedLOD(nLodA, true);
	}

	return CLodValue(nLodA, 0, -1);
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
	rParms.pMaterial = m_pMaterial;
	rParms.nEditorSelectionID = m_nEditorSelectionID;

	rParms.dwFObjFlags |= (m_dwRndFlags & ERF_FOB_RENDER_AFTER_POSTPROCESSING) ? FOB_RENDER_AFTER_POSTPROCESSING : 0;
	rParms.dwFObjFlags |= FOB_TRANS_MASK;

	rParms.nHUDSilhouettesParams = m_nHUDSilhouettesParam;
	rParms.nSubObjHideMask = m_nSubObjHideMask;

	if ((m_dwRndFlags & (ERF_NO_DECALNODE_DECALS | ERF_MOVES_EVERY_FRAME)) ||
	    (gEnv->nMainFrameID - m_lastMoveFrameId < 3))
	{
		rParms.dwFObjFlags |= FOB_DYNAMIC_OBJECT;
	}

	if ((m_dwRndFlags & ERF_FOB_NEAREST) != 0)
	{
		if (passInfo.IsRecursivePass()) // Nearest objects are not rendered in the recursive passes.
			return;

		rParms.dwFObjFlags |= FOB_NEAREST;
		if (rParms.dwFObjFlags & FOB_DYNAMIC_OBJECT)
		{
			rParms.pInstance = this;
		}

		// Nearest objects recalculate instance matrix every frame
		CalcNearestTransform(m_Matrix, passInfo);
	}

	m_pStatObj->Render(rParms, passInfo);
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )                //AMD Port
#endif

void CBrush::SetMatrix(const Matrix34& mat)
{
	if (!IsMatrixValid(mat))
	{
		const Vec3 pos = mat.GetTranslation();
		stack_string message;
		message.Format("Error: IRenderNode::SetMatrix: Invalid matrix ignored (name=\"%s\", position=[%.2f,%.2f,%.2f])", GetName(), pos.x, pos.y, pos.z);
		Warning(message.c_str());
		return;
	}

	if (m_Matrix == mat)
		return;

	m_Matrix = mat;

	bool replacePhys = fabs(mat.GetColumn(0).len() - m_Matrix.GetColumn(0).len())
	                   + fabs(mat.GetColumn(1).len() - m_Matrix.GetColumn(1).len())
	                   + fabs(mat.GetColumn(2).len() - m_Matrix.GetColumn(2).len()) > FLT_EPSILON;

	InvalidatePermanentRenderObjectMatrix();

	pe_params_foreign_data foreignData;
	foreignData.iForeignFlags = 0;
	if (!replacePhys && m_pPhysEnt)
	{
		m_pPhysEnt->GetParams(&foreignData);
		replacePhys = !(foreignData.iForeignFlags & PFF_OUTDOOR_AREA) != !(m_dwRndFlags & ERF_NODYNWATER);
	}

	CalcBBox();

	Get3DEngine()->UnRegisterEntityAsJob(this);
	Get3DEngine()->RegisterEntity(this);

	if (replacePhys)
		Dephysicalize();
	if (!m_pPhysEnt)
	{
		if (!m_bNoPhysicalize)
		{
			Physicalize();
		}
	}
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

	m_lastMoveFrameId = gEnv->nMainFrameID;
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
}

void CBrush::Physicalize(bool bInstant)
{
	m_bNoPhysicalize = false;
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
	if (nSlot < 0 || !m_pStatObj->GetSubObjectCount())
	{
		bool res = false;
		for (int i = 0; i < m_pStatObj->GetSubObjectCount(); i++)
			res = res || PhysicalizeFoliage(bPhysicalize, iSource, i);
		if (!res)
		{
			if (bPhysicalize)
			{
				res = m_pStatObj->PhysicalizeFoliage(m_pPhysEnt, m_Matrix, m_pFoliage) != 0;
			}
			else if (m_pFoliage)
			{
				m_pFoliage->Release();
				m_pFoliage = nullptr;
			}
		}
		return res;
	}

	if (IStatObj::SSubObject* pSubObj = m_pStatObj->GetSubObject(nSlot))
		if (bPhysicalize)
		{
			if (!pSubObj->pStatObj || !((CStatObj*)pSubObj->pStatObj)->m_nSpines)
				return false;
			if (!(m_pStatObj->GetFlags() & STATIC_OBJECT_CLONE))
			{
				m_pStatObj = (CStatObj*)m_pStatObj->Clone(false, false, false);
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
	if (!m_pStatObj)
		return nullptr;
	IStatObj::SSubObject* pSubObj = m_pStatObj->GetSubObject(nSlot);
	if (pSubObj)
		return pSubObj->pFoliage;
	return m_pFoliage;
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

	InvalidatePermanentRenderObject();
}

void CBrush::CheckPhysicalized()
{
	if (!m_pPhysEnt && !m_bNoPhysicalize)
	{
		Physicalize();
	}
}

void CBrush::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "Brush");
	pSizer->AddObject(this, sizeof(*this));
}

void CBrush::SetEntityStatObj(IStatObj* pStatObj, const Matrix34A* pMatrix)
{
	//assert(pStatObj);

	IStatObj* pPrevStatObj = m_pStatObj;

	SetStatObj(pStatObj);

	if (pMatrix)
	{
		SetMatrix(*pMatrix);
	}

	// If object differ we must re-physicalize.
	bool bRePhysicalize = (pStatObj != pPrevStatObj) && (!pPrevStatObj || !pStatObj || (pStatObj->GetCloneSourceObject() != pPrevStatObj));

	if (bRePhysicalize && !m_bNoPhysicalize)
	{
		Physicalize();
	}
	InvalidatePermanentRenderObject();
}

//////////////////////////////////////////////////////////////////////////
void CBrush::SetStatObj(IStatObj* pStatObj)
{
	if (pStatObj == m_pStatObj)
		return;

	RemoveAndMarkForAutoDeleteTempData();

	m_pStatObj = (CStatObj*)pStatObj;
	if (m_pStatObj && m_pStatObj->IsDeformable())
	{
		if (!m_pDeform)
			m_pDeform = new CDeformableNode();
		m_pDeform->SetStatObj(static_cast<CStatObj*>(m_pStatObj.get()));
		m_pDeform->BakeDeform(GetMatrix());
	}
	else
		SAFE_DELETE(m_pDeform);

	m_nInternalFlags |= UPDATE_DECALS;
}

IRenderNode* CBrush::Clone() const
{
	CBrush* pDestBrush = new CBrush();

	pDestBrush->m_Matrix = m_Matrix;
	//pDestBrush->m_pPhysEnt		//Don't want to copy the phys ent pointer
	pDestBrush->m_pMaterial = m_pMaterial;
	pDestBrush->m_pStatObj = m_pStatObj;

	pDestBrush->m_bVehicleOnlyPhysics = m_bVehicleOnlyPhysics;
	pDestBrush->m_bDrawLast = m_bDrawLast;
	pDestBrush->m_WSBBox = m_WSBBox;

	pDestBrush->m_collisionClassIdx = m_collisionClassIdx;
	pDestBrush->m_nLayerId = m_nLayerId;

	//IRenderNode member vars
	//	We cannot just copy over due to issues with the linked list of IRenderNode objects
	CopyIRenderNodeData(pDestBrush);

	return pDestBrush;
}

void CBrush::SetLayerId(uint16 nLayerId)
{
	bool bChanged = m_nLayerId != nLayerId;
	m_nLayerId = nLayerId;

	if (bChanged)
	{
		Get3DEngine()->C3DEngine::UpdateObjectsLayerAABB(this);
	}
	InvalidatePermanentRenderObject();
}

IRenderMesh* CBrush::GetRenderMesh(int nLod)
{
	IStatObj* pStatObj = m_pStatObj ? m_pStatObj->GetLodObject(nLod) : NULL;
	return pStatObj ? pStatObj->GetRenderMesh() : NULL;
}

void CBrush::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_Matrix.SetTranslation(m_Matrix.GetTranslation() + delta);
	InvalidatePermanentRenderObjectMatrix();
	m_WSBBox.Move(delta);

	if (m_pPhysEnt)
	{
		pe_params_pos par_pos;
		par_pos.pos = m_Matrix.GetTranslation();
		m_pPhysEnt->SetParams(&par_pos);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrush::SetCameraSpacePos(Vec3* pCameraSpacePos)
{
	if (pCameraSpacePos)
	{
		if (!m_pCameraSpacePos)
			m_pCameraSpacePos = new Vec3;
		*m_pCameraSpacePos = *pCameraSpacePos;
	}
	else
	{
		delete m_pCameraSpacePos;
		m_pCameraSpacePos = nullptr;
	}
}

void CBrush::SetSubObjectHideMask(hidemask subObjHideMask)
{
	m_nSubObjHideMask = subObjHideMask;
	InvalidatePermanentRenderObject();
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
void CBrush::Render(const CLodValue& lodValue, const SRenderingPassInfo& passInfo, SSectorTextureSet* pTerrainTexInfo, PodArray<SRenderLight*>* pAffectingLights)
{
	FUNCTION_PROFILER_3DENGINE;

	// Collision proxy is visible in Editor while in editing mode.
	if (m_dwRndFlags & (ERF_COLLISION_PROXY | ERF_RAYCAST_PROXY))
	{
		if (!gEnv->IsEditor() || !gEnv->IsEditing())
			if (GetCVars()->e_DebugDraw == 0)
				return;
	}

	if (!m_pStatObj || m_dwRndFlags & ERF_HIDDEN)
		return;

	/*
	   if (!passInfo.IsShadowPass())
	   {
	   if (pFoliage)
	   {
	    pFoliage->SetFlags(pFoliage->GetFlags() & ~IFoliage::FLAG_FROZEN | -(int)(rParams.nMaterialLayers&MTL_LAYER_FROZEN) & IFoliage::FLAG_FROZEN);
	    float maxdist = GetCVars()->e_FoliageWindActivationDist;
	    Vec3 pos = m_worldTM.GetTranslation();
	    if (pStatObj && (gEnv->pSystem->GetViewCamera().GetPosition() - pos).len2() < sqr(maxdist) && gEnv->p3DEngine->GetWind(AABB(pos), false).len2() > 101.0f)
	      pStatObj->PhysicalizeFoliage(pEntity->GetPhysics(), m_worldTM, pFoliage, 0, 4);
	   }
	   }
	 */

	const uint8 nMaterialLayers = IRenderNode::GetMaterialLayers();
	Matrix34 transformMatrix = m_Matrix;
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	bool isNearestObject = (GetRndFlags() & ERF_FOB_NEAREST) != 0;
	if (isNearestObject)
	{
		if (passInfo.IsRecursivePass()) // Nearest objects are not rendered in the recursive passes.
			return;

		bool isNearestShadowPass = passInfo.IsShadowPass() && passInfo.GetIRenderView()->GetShadowFrustumOwner()->m_eFrustumType == ShadowMapFrustum::e_Nearest;
		if (passInfo.IsGeneralPass() || isNearestShadowPass)
		{
			// Nearest objects recalculate instance matrix every frame
			CalcNearestTransform(transformMatrix, passInfo);
		}
	}

	auto pTempData = m_pTempData.load();
	if (!pTempData)
	{
		CRY_ASSERT(false);
		return;
	}

	CRenderObject* pObj = nullptr;
	if (m_pFoliage || m_pDeform || (m_dwRndFlags & ERF_HUD) || isNearestObject)
	{
		// Foliage and deform do not support permanent render objects
		// HUD is managed in custom render-lists and also doesn't support it
		pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
		pObj->SetMatrix(transformMatrix, passInfo);
	}
	if (!pObj)
	{
		if (GetObjManager()->AddOrCreatePersistentRenderObject(pTempData, pObj, &lodValue, transformMatrix, passInfo))
		{
			if (pObj && !passInfo.IsShadowPass())
			{
 				pObj->m_fDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();
				IF(!m_bDrawLast, 1)
					pObj->m_nSort = fastround_positive(pObj->m_fDistance * 2.0f);
				else
					pObj->m_fSort = 10000.0f;
			}
			return;
		}
	}

	const auto& userData = pTempData->userData;

	const Vec3 vObjCenter = CBrush::GetBBox().GetCenter();
	const Vec3 vObjPos = CBrush::GetPos();

	pObj->m_fDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

	pObj->m_pRenderNode = this;
	pObj->m_fAlpha = 1.f;
	IF (!m_bDrawLast, 1)
		pObj->m_nSort = fastround_positive(pObj->m_fDistance * 2.0f);
	else
		pObj->m_fSort = 10000.0f;

	IMaterial* pMat = pObj->m_pCurrMaterial = CBrush::GetMaterial();
	pObj->m_ObjFlags |= FOB_INSHADOW | FOB_TRANS_MASK;

	pObj->m_ObjFlags |= (m_dwRndFlags & ERF_FOB_RENDER_AFTER_POSTPROCESSING) ? FOB_RENDER_AFTER_POSTPROCESSING : 0;
	pObj->m_ObjFlags |= (m_dwRndFlags & ERF_FOB_NEAREST) ? FOB_NEAREST : 0;

	if (m_dwRndFlags & ERF_NO_DECALNODE_DECALS && !(gEnv->nMainFrameID - m_lastMoveFrameId < 3))
	{
		pObj->m_ObjFlags |= FOB_DYNAMIC_OBJECT;
	}

	if (m_dwRndFlags & ERF_HUD_REQUIRE_DEPTHTEST)
	{
		pObj->m_ObjFlags |= FOB_HUD_REQUIRE_DEPTHTEST;
	}

	if (nMaterialLayers)
	{
		uint8 nFrozenLayer = (nMaterialLayers & MTL_LAYER_FROZEN) ? MTL_LAYER_FROZEN_MASK : 0;
		uint8 nWetLayer = (nMaterialLayers & MTL_LAYER_WET) ? MTL_LAYER_WET_MASK : 0;
		pObj->m_nMaterialLayers = (uint32)(nFrozenLayer << 24) | (nWetLayer << 16);
	}
	else
		pObj->m_nMaterialLayers = 0;

	if (!passInfo.IsShadowPass() && m_nInternalFlags & IRenderNode::REQUIRES_NEAREST_CUBEMAP)
	{
		if (!(pObj->m_nTextureID = GetObjManager()->CheckCachedNearestCubeProbe(this)) || !GetCVars()->e_CacheNearestCubePicking)
			pObj->m_nTextureID = GetObjManager()->GetNearestCubeProbe(pAffectingLights, m_pOcNode->GetVisArea(), CBrush::GetBBox());

		pTempData->userData.nCubeMapId = pObj->m_nTextureID;
	}

	//////////////////////////////////////////////////////////////////////////
	// temp fix to update ambient color (Vlad please review!)
	pObj->m_nClipVolumeStencilRef = userData.m_pClipVolume ? userData.m_pClipVolume->GetStencilRef() : 0;
	if (m_pOcNode && m_pOcNode->GetVisArea())
		pObj->SetAmbientColor(m_pOcNode->GetVisArea()->GetFinalAmbientColor(), passInfo);
	else
		pObj->SetAmbientColor(Get3DEngine()->GetSkyColor(), passInfo);
	//////////////////////////////////////////////////////////////////////////
	pObj->m_editorSelectionID = m_nEditorSelectionID;

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
			pTempData->userData.bTerrainColorWasUsed = true;

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

	if ((GetRndFlags() & ERF_RECVWIND) && GetCVars()->e_VegetationBending)
	{
		// this is default value for vegetation if bending in veg group is set to 1
		pObj->SetBendingData({ 0.1f, m_pStatObj->m_fRadiusVert }, passInfo);
		pObj->m_ObjFlags |= FOB_BENDED | FOB_DYNAMIC_OBJECT;
	}
	else
	{
		pObj->SetBendingData({ 0.0f, 0.0f }, passInfo);
	}

	//IFoliage* pFoliage = GetFoliage(-1);
	if (m_pFoliage)
	{
		if (SRenderObjData* pOD = pObj->GetObjData())
		{
			pOD->m_pSkinningData = m_pFoliage->GetSkinningData(m_Matrix, passInfo);
			pObj->m_ObjFlags |= FOB_SKINNED | FOB_DYNAMIC_OBJECT;
			//m_pFoliage->SetFlags(m_pFoliage->GetFlags() & ~IFoliage::FLAG_FROZEN | -(int)(pObj->m_nMaterialLayers & MTL_LAYER_FROZEN) & IFoliage::FLAG_FROZEN);
		}
	}

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

		if (GetCVars()->e_BBoxes)
			GetObjManager()->RenderObjectDebugInfo((IRenderNode*)this, pObj->m_fDistance, passInfo);

		if (lodValue.LodA() <= 0 && Cry3DEngineBase::GetCVars()->e_MergedMeshes != 0 && m_pDeform && m_pDeform->HasDeformableData())
		{
			if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
			{
				GetObjManager()->PushIntoCullOutputQueue(SCheckOcclusionOutput::CreateDeformableBrushOutput(this, gEnv->pRenderer->EF_DuplicateRO(pObj, passInfo), lodValue.LodA(), passInfo));
			}
			else
			{
				m_pDeform->RenderInternalDeform(gEnv->pRenderer->EF_DuplicateRO(pObj, passInfo), lodValue.LodA(), CBrush::GetBBox(), passInfo);
			}
		}

		pObj->m_data.m_nHUDSilhouetteParams = m_nHUDSilhouettesParam;

		m_pStatObj->RenderInternal(pObj, m_nSubObjHideMask, lodValue, passInfo);
	}
}

///////////////////////////////////////////////////////////////////////////////
IStatObj* CBrush::GetEntityStatObj(unsigned int nSubPartId, Matrix34A* pMatrix, bool bReturnOnlyVisible)
{
	if (pMatrix)
		*pMatrix = m_Matrix;

	return m_pStatObj;
}

///////////////////////////////////////////////////////////////////////////////
void CBrush::OnRenderNodeBecomeVisibleAsync(SRenderNodeTempData* pTempData, const SRenderingPassInfo& passInfo)
{
	// Not reentrant, multiple simultaneous calls to this method on the same Render Node from multiple threads is not supported
	SRenderNodeTempData::SUserData& userData = pTempData->userData;

	userData.objMat = m_Matrix;
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, CBrush::GetBBox())) * passInfo.GetZoomFactor();

	userData.nWantedLod = CObjManager::GetObjectLOD(this, fEntDistance);
}

//////////////////////////////////////////////////////////////////////////
void CBrush::CalcNearestTransform(Matrix34& transformMatrix, const SRenderingPassInfo& passInfo)
{
	// Camera space
	if (m_pCameraSpacePos)
	{
		// Use camera space relative position
		const Matrix33 cameraRotation = Matrix33(passInfo.GetCamera().GetViewMatrix());
		transformMatrix.SetTranslation(*m_pCameraSpacePos * cameraRotation);
	}
	else
	{
		// We don't have camera space relative position, so calculate it out from world space
		// (This will not have the precision advantages of camera space rendering)
		transformMatrix.AddTranslation(-passInfo.GetCamera().GetPosition());
	}

	InvalidatePermanentRenderObjectMatrix();
}

void CBrush::InvalidatePermanentRenderObjectMatrix()
{
	if (m_pStatObj && m_pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		// Compound unmerged stat objects create duplicate sub render objects and do not support fast matrix only instance update for PermanentRenderObject
		InvalidatePermanentRenderObject();
	}
	else if (const auto pTempData = m_pTempData.load())
	{
		// Special optimization when only matrix change, we invalidate render object instance data flag
		pTempData->InvalidateRenderObjectsInstanceData();
	}
}

///////////////////////////////////////////////////////////////////////////////
bool CBrush::CanExecuteRenderAsJob()
{
	return (GetCVars()->e_ExecuteRenderAsJobMask & BIT(GetRenderNodeType())) != 0;
}

//////////////////////////////////////////////////////////////////////////
void CBrush::DisablePhysicalization(bool bDisable)
{
	m_bNoPhysicalize = bDisable;
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
	if (GetRndFlags() & ERF_FORCE_POST_3D_RENDER)
	{
		// Always want to render models in post 3d render (menus), whatever distance they are
		return FLT_MAX;
	}

	if (GetRndFlags() & ERF_CUSTOM_VIEW_DIST_RATIO)
	{
		float s = max(max((m_WSBBox.max.x - m_WSBBox.min.x), (m_WSBBox.max.y - m_WSBBox.min.y)), (m_WSBBox.max.z - m_WSBBox.min.z));
		return max(GetCVars()->e_ViewDistMin, s * GetCVars()->e_ViewDistRatioCustom * GetViewDistRatioNormilized());
	}

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

//////////////////////////////////////////////////////////////////////////
float CBrush::GetScale() const
{
	return m_Matrix.GetColumn0().GetLength();
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
