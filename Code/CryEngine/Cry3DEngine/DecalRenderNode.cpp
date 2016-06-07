// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DecalRenderNode.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "MatMan.h"
#include "terrain.h"

int CDecalRenderNode::m_nFillBigDecalIndicesCounter = 0;

CDecalRenderNode::CDecalRenderNode()
	: m_pos(0, 0, 0)
	, m_localBounds(Vec3(-1, -1, -1), Vec3(1, 1, 1))
	, m_pMaterial(NULL)
	, m_updateRequested(false)
	, m_decalProperties()
	, m_decals()
	, m_nLastRenderedFrameId(0)
	, m_nLayerId(0)
{
	GetInstCount(GetRenderNodeType())++;

	m_Matrix.SetIdentity();
}

CDecalRenderNode::~CDecalRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	DeleteDecals();
	Get3DEngine()->FreeRenderNodeState(this);
}

const SDecalProperties* CDecalRenderNode::GetDecalProperties() const
{
	return &m_decalProperties;
}

void CDecalRenderNode::DeleteDecals()
{
	for (size_t i(0); i < m_decals.size(); ++i)
		delete m_decals[i];

	m_decals.resize(0);
}

void CDecalRenderNode::CreatePlanarDecal()
{
	CryEngineDecalInfo decalInfo;

	// necessary params
	decalInfo.vPos = m_decalProperties.m_pos;
	decalInfo.vNormal = m_decalProperties.m_normal;
	decalInfo.fSize = m_decalProperties.m_radius;
	decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
	cry_strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
	decalInfo.sortPrio = m_decalProperties.m_sortPrio;

	// default for all other
	decalInfo.pIStatObj = NULL;
	decalInfo.ownerInfo.pRenderNode = NULL;
	decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
	decalInfo.vHitDirection = Vec3(0, 0, 0);
	decalInfo.fGrowTime = 0;
	decalInfo.preventDecalOnGround = true;
	decalInfo.fAngle = 0;

	CDecal* pDecal(new CDecal);
	if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
		m_decals.push_back(pDecal);
	else
		delete pDecal;
}

void CDecalRenderNode::CreateDecalOnStaticObjects()
{
	CTerrain* pTerrain(GetTerrain());
	CVisAreaManager* pVisAreaManager(GetVisAreaManager());
	PodArray<SRNInfo> decalReceivers;

	if (pTerrain && m_pOcNode && !m_pOcNode->m_pVisArea)
		pTerrain->GetObjectsAround(m_decalProperties.m_pos, m_decalProperties.m_radius, &decalReceivers, true, false);
	else if (pVisAreaManager && m_pOcNode && m_pOcNode->m_pVisArea)
		pVisAreaManager->GetObjectsAround(m_decalProperties.m_pos, m_decalProperties.m_radius, &decalReceivers, true);

	// delete vegetations
	for (int nRecId(0); nRecId < decalReceivers.Count(); ++nRecId)
	{
		EERType eType = decalReceivers[nRecId].pNode->GetRenderNodeType();
		if (eType != eERType_Brush && eType != eERType_RenderProxy)
		{
			decalReceivers.DeleteFastUnsorted(nRecId);
			nRecId--;
		}
	}

	for (int nRecId(0); nRecId < decalReceivers.Count(); ++nRecId)
	{
		CryEngineDecalInfo decalInfo;

		// necessary params
		decalInfo.vPos = m_decalProperties.m_pos;
		decalInfo.vNormal = m_decalProperties.m_normal;
		decalInfo.vHitDirection = -m_decalProperties.m_normal;
		decalInfo.fSize = m_decalProperties.m_radius;
		decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
		cry_strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
		decalInfo.sortPrio = m_decalProperties.m_sortPrio;

		if (GetCVars()->e_DecalsMerge)
			decalInfo.ownerInfo.pDecalReceivers = &decalReceivers;
		else
			decalInfo.ownerInfo.pRenderNode = decalReceivers[nRecId].pNode;

		decalInfo.ownerInfo.nRenderNodeSlotId = 0;
		decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = -1;

		// default for all other
		decalInfo.pIStatObj = NULL;
		decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
		decalInfo.fGrowTime = 0;
		decalInfo.preventDecalOnGround = false;
		decalInfo.fAngle = 0;

		IStatObj* pMainStatObj = decalInfo.ownerInfo.pRenderNode->GetEntityStatObj();
		if (!pMainStatObj)
			continue;

		// in case of multi-sub-objects, spawn decal on each visible sub-object
		int nSubObjCount = pMainStatObj->GetSubObjectCount();
		if (!GetCVars()->e_DecalsMerge && nSubObjCount)
		{
			for (int nSubObjId = 0; nSubObjId < nSubObjCount; nSubObjId++)
			{
				CStatObj::SSubObject* const __restrict pSubObj = (CStatObj::SSubObject*)pMainStatObj->GetSubObject(nSubObjId);
				if (CStatObj* const __restrict pSubStatObj = (CStatObj*)pSubObj->pStatObj)
					if (pSubObj->nType == STATIC_SUB_OBJECT_MESH && !pSubObj->bHidden && !(pSubStatObj->m_nFlags & STATIC_OBJECT_HIDDEN))
					{
						CDecal* pDecal(new CDecal);
						decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = nSubObjId;

						if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
						{
							m_decals.push_back(pDecal);
							assert(!GetCVars()->e_DecalsMerge || pDecal->m_eDecalType == eDecalType_WS_Merged);
						}
						else
						{
							delete pDecal;
						}
					}
			}
		}
		else
		{
			CDecal* pDecal(new CDecal);
			if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
			{
				m_decals.push_back(pDecal);
				assert(!GetCVars()->e_DecalsMerge || pDecal->m_eDecalType == eDecalType_WS_Merged);
			}
			else
			{
				delete pDecal;
			}
		}

		if (GetCVars()->e_DecalsMerge)
			break;
	}
}

void CDecalRenderNode::CreateDecalOnTerrain()
{
	float terrainHeight(GetTerrain()->GetZApr(m_decalProperties.m_pos.x, m_decalProperties.m_pos.y, m_nSID));
	float terrainDelta(m_decalProperties.m_pos.z - terrainHeight);
	if (terrainDelta < m_decalProperties.m_radius && terrainDelta > -0.5f)
	{
		CryEngineDecalInfo decalInfo;

		// necessary params
		decalInfo.vPos = Vec3(m_decalProperties.m_pos.x, m_decalProperties.m_pos.y, terrainHeight);
		decalInfo.vNormal = Vec3(0, 0, 1);
		decalInfo.vHitDirection = Vec3(0, 0, -1);
		decalInfo.fSize = m_decalProperties.m_radius;// - terrainDelta;
		decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
		cry_strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
		decalInfo.sortPrio = m_decalProperties.m_sortPrio;

		// default for all other
		decalInfo.pIStatObj = NULL;
		decalInfo.ownerInfo.pRenderNode = 0;
		decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
		decalInfo.fGrowTime = 0;
		decalInfo.preventDecalOnGround = false;
		decalInfo.fAngle = 0;

		CDecal* pDecal(new CDecal);
		if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
			m_decals.push_back(pDecal);
		else
			delete pDecal;
	}
}

void CDecalRenderNode::CreateDecals()
{
	DeleteDecals();

	if (m_decalProperties.m_deferred)
		return;

	IMaterial* pMaterial(GetMaterial());

	assert(0 != pMaterial && "CDecalRenderNode::CreateDecals() -- Invalid Material!");
	if (!pMaterial)
		return;

	switch (m_decalProperties.m_projectionType)
	{
	case SDecalProperties::ePlanar:
		{
			CreatePlanarDecal();
			break;
		}
	case SDecalProperties::eProjectOnStaticObjects:
		{
			CreateDecalOnStaticObjects();
			break;
		}
	case SDecalProperties::eProjectOnTerrain:
		{
			CreateDecalOnTerrain();
			break;
		}
	case SDecalProperties::eProjectOnTerrainAndStaticObjects:
		{
			CreateDecalOnStaticObjects();
			CreateDecalOnTerrain();
			break;
		}
	default:
		{
			assert(!"CDecalRenderNode::CreateDecals() : Unsupported decal projection type!");
			break;
		}
	}
}

void CDecalRenderNode::ProcessUpdateRequest()
{
	if (!m_updateRequested || m_nFillBigDecalIndicesCounter >= GetCVars()->e_DecalsMaxUpdatesPerFrame)
		return;

	CreateDecals();
	m_updateRequested = false;
}

void CDecalRenderNode::UpdateAABBFromRenderMeshes()
{
	if (m_decalProperties.m_projectionType == SDecalProperties::eProjectOnStaticObjects ||
	    m_decalProperties.m_projectionType == SDecalProperties::eProjectOnTerrain ||
	    m_decalProperties.m_projectionType == SDecalProperties::eProjectOnTerrainAndStaticObjects)
	{
		AABB WSBBox;
		WSBBox.Reset();
		for (size_t i(0); i < m_decals.size(); ++i)
		{
			CDecal* pDecal(m_decals[i]);
			if (pDecal && pDecal->m_pRenderMesh && pDecal->m_eDecalType != eDecalType_OS_OwnersVerticesUsed)
			{
				AABB aabb;
				pDecal->m_pRenderMesh->GetBBox(aabb.min, aabb.max);
				if (pDecal->m_eDecalType == eDecalType_WS_Merged || pDecal->m_eDecalType == eDecalType_WS_OnTheGround)
				{
					aabb.min += pDecal->m_vPos;
					aabb.max += pDecal->m_vPos;
				}
				WSBBox.Add(aabb);
			}
		}

		if (!WSBBox.IsReset())
			m_WSBBox = WSBBox;
	}
}

//special check for def decals forcing
bool CDecalRenderNode::CheckForceDeferred()
{
	if (m_pMaterial != NULL)
	{
		SShaderItem& sItem = m_pMaterial->GetShaderItem(0);
		if (sItem.m_pShaderResources != NULL)
		{
			float fCosA = m_decalProperties.m_normal.GetNormalized().Dot(Vec3(0, 0, 1));
			if (fCosA > 0.5f)
				return false;

			if (SEfResTexture* pEnvRes0 = sItem.m_pShaderResources->GetTexture(EFTT_ENV))
			{
				if (pEnvRes0->m_Sampler.m_pITex == NULL)
				{
					m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
					m_decalProperties.m_deferred = true;
					return true;
				}
			}
			else
			{
				m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
				m_decalProperties.m_deferred = true;
				return true;
			}
		}
	}
	return false;
}

void CDecalRenderNode::SetDecalProperties(const SDecalProperties& properties)
{
	// update bounds
	m_localBounds = AABB(-properties.m_radius * Vec3(1, 1, 1), properties.m_radius * Vec3(1, 1, 1));

	// register material
	m_pMaterial = GetMatMan()->LoadMaterial(properties.m_pMaterialName, false);

	// copy decal properties
	m_decalProperties = properties;
	m_decalProperties.m_pMaterialName = 0; // reset this as it's assumed to be a temporary pointer only, refer to m_materialID to get material

	// request update
	m_updateRequested = true;

	bool bForced = 0;

	// TOOD: Revive support for planar decals in new pipeline
	if (properties.m_deferred || (GetCVars()->e_DecalsDeferredStatic
	                              /*&& (m_decalProperties.m_projectionType != SDecalProperties::ePlanar && m_decalProperties.m_projectionType != SDecalProperties::eProjectOnTerrain)*/))
	{
		m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
		m_decalProperties.m_deferred = true;
	}

	if (GetCVars()->e_DecalsForceDeferred)
	{
		if (CheckForceDeferred())
			bForced = true;
	}

	// set matrix
	m_Matrix.SetRotation33(m_decalProperties.m_explicitRightUpFront);
	Matrix33 matScale;
	if (bForced && !properties.m_deferred)
		matScale.SetScale(Vec3(properties.m_radius, properties.m_radius, properties.m_radius * 0.05f));
	else
		matScale.SetScale(Vec3(properties.m_radius, properties.m_radius, properties.m_radius * properties.m_depth));

	m_Matrix = m_Matrix * matScale;
	m_Matrix.SetTranslation(properties.m_pos);
}

IRenderNode* CDecalRenderNode::Clone() const
{
	CDecalRenderNode* pDestDecal = new CDecalRenderNode();

	// CDecalRenderNode member vars
	pDestDecal->m_pos = m_pos;
	pDestDecal->m_localBounds = m_localBounds;
	pDestDecal->m_pMaterial = m_pMaterial;
	pDestDecal->m_updateRequested = true;
	pDestDecal->m_decalProperties = m_decalProperties;
	pDestDecal->m_WSBBox = m_WSBBox;
	pDestDecal->m_Matrix = m_Matrix;
	pDestDecal->m_nLayerId = m_nLayerId;

	//IRenderNode member vars
	//	We cannot just copy over due to issues with the linked list of IRenderNode objects
	CopyIRenderNodeData(pDestDecal);

	return pDestDecal;
}

void CDecalRenderNode::SetMatrix(const Matrix34& mat)
{
	m_pos = mat.GetTranslation();

	if (m_decalProperties.m_projectionType == SDecalProperties::ePlanar)
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 0.5f), Vec3(1, 1, 0.5f)));
	else
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 1), Vec3(1, 1, 1)));

	Get3DEngine()->RegisterEntity(this);
}

void CDecalRenderNode::SetMatrixFull(const Matrix34& mat)
{
	m_Matrix = mat;
	m_pos = mat.GetTranslation();

	if (m_decalProperties.m_projectionType == SDecalProperties::ePlanar)
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 0.5f), Vec3(1, 1, 0.5f)));
	else
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 1), Vec3(1, 1, 1)));
}

const char* CDecalRenderNode::GetEntityClassName() const
{
	return "Decal";
}

const char* CDecalRenderNode::GetName() const
{
	return "Decal";
}

void CDecalRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!passInfo.RenderDecals())
		return; // false;

	// Calculate distance fading once as it's the same for
	// all decals belonging to this node, regardless of type
	float fDistFading = SATURATE((1.f - rParam.fDistance / m_fWSMaxViewDist) * DIST_FADING_FACTOR);

	if (m_decalProperties.m_deferred)
	{
		if (passInfo.IsShadowPass())
			return; // otherwise causing flickering with GI

		SDeferredDecal newItem;
		newItem.fAlpha = fDistFading;
		newItem.pMaterial = m_pMaterial;
		newItem.projMatrix = m_Matrix;
		newItem.nSortOrder = m_decalProperties.m_sortPrio;
		newItem.nFlags = DECAL_STATIC;
		GetRenderer()->EF_AddDeferredDecal(newItem, passInfo);
		return;
	}

	// update last rendered frame id
	m_nLastRenderedFrameId = passInfo.GetMainFrameID();

	bool bUpdateAABB = m_updateRequested;

	if (passInfo.IsGeneralPass())
		ProcessUpdateRequest();

	float waterLevel(m_p3DEngine->GetWaterLevel());
	for (size_t i(0); i < m_decals.size(); ++i)
	{
		CDecal* pDecal(m_decals[i]);
		if (pDecal && 0 != pDecal->m_pMaterial)
		{
			pDecal->m_vAmbient.x = rParam.AmbientColor.r;
			pDecal->m_vAmbient.y = rParam.AmbientColor.g;
			pDecal->m_vAmbient.z = rParam.AmbientColor.b;
			bool bAfterWater = CObjManager::IsAfterWater(pDecal->m_vWSPos, passInfo.GetCamera().GetPosition(), passInfo, waterLevel);
			pDecal->Render(0, bAfterWater, fDistFading, rParam.fDistance, passInfo);
		}
	}

	// terrain decal meshes are created only during rendering so only after that bbox can be computed
	if (bUpdateAABB)
		UpdateAABBFromRenderMeshes();

	//	return true;
}

IPhysicalEntity* CDecalRenderNode::GetPhysics() const
{
	return 0;
}

void CDecalRenderNode::SetPhysics(IPhysicalEntity*)
{
}

void CDecalRenderNode::SetMaterial(IMaterial* pMat)
{
	for (size_t i(0); i < m_decals.size(); ++i)
	{
		CDecal* pDecal(m_decals[i]);
		if (pDecal)
			pDecal->m_pMaterial = pMat;
	}

	m_pMaterial = pMat;

	//special check for def decals forcing
	if (GetCVars()->e_DecalsForceDeferred)
	{
		CheckForceDeferred();
	}

}

void CDecalRenderNode::Precache()
{
	ProcessUpdateRequest();
}

void CDecalRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "DecalNode");
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_decals);
}

void CDecalRenderNode::CleanUpOldDecals()
{
	if (m_nLastRenderedFrameId != 0 && // was rendered at least once
	    (int)GetRenderer()->GetFrameID(false) > (int)m_nLastRenderedFrameId + GetCVars()->e_DecalsMaxValidFrames)
	{
		DeleteDecals();
		m_nLastRenderedFrameId = 0;
		m_updateRequested = true; // make sure if rendered again, that the decal is recreated
	}
}

void CDecalRenderNode::OffsetPosition(const Vec3& delta)
{
	if (m_pTempData) m_pTempData->OffsetPosition(delta);
	m_pos += delta;
	m_WSBBox.Move(delta);
	m_Matrix.SetTranslation(m_Matrix.GetTranslation() + delta);
}

void CDecalRenderNode::FillBBox(AABB& aabb)
{
	aabb = CDecalRenderNode::GetBBox();
}

EERType CDecalRenderNode::GetRenderNodeType()
{
	return eERType_Decal;
}

float CDecalRenderNode::GetMaxViewDist()
{
	float fMatScale = m_Matrix.GetColumn0().GetLength();

	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, fMatScale * 0.75f * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return(max(GetCVars()->e_ViewDistMin, fMatScale * 0.75f * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized()));
}

Vec3 CDecalRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pos;
}

IMaterial* CDecalRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}
