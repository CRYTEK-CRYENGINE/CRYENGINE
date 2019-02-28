// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShadowCache.h"
#include "LightEntity.h"
#include "VisAreas.h"

const float ShadowCacheGenerator::AO_FRUSTUM_SLOPE_BIAS = 0.5f;
int ShadowCacheGenerator::m_cacheGenerationId = 0;

uint8 ShadowCacheGenerator::GetNextGenerationID() const
{
	// increase generation ID. Make sure we never return a value that
	// wraps around to 0 as this is used for invalidating render nodes
	int nextID = m_cacheGenerationId++;
	if (uint8(nextID) == 0)
		nextID = m_cacheGenerationId++;

	return uint8(nextID);
}

void ShadowCacheGenerator::InitShadowFrustum(ShadowMapFrustumPtr& pFr, int nLod, int nFirstStaticLod, float fDistFromViewDynamicLod, float fRadiusDynamicLod, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;
	assert(nLod >= 0);

	if (!pFr)
		pFr = new ShadowMapFrustum;

	if (!pFr->pShadowCacheData)
		pFr->pShadowCacheData = std::make_shared<ShadowMapFrustum::ShadowCacheData>();

	const int shadowCacheLod = nLod - nFirstStaticLod;
	CRY_ASSERT(shadowCacheLod >= 0 && shadowCacheLod < MAX_GSM_CACHED_LODS_NUM);

	// check if we have come too close to the border of the map
	ShadowMapFrustum::ShadowCacheData::eUpdateStrategy nUpdateStrategy = m_nUpdateStrategy;
	if (nUpdateStrategy == ShadowMapFrustum::ShadowCacheData::eIncrementalUpdate && Get3DEngine()->m_CachedShadowsBounds.IsReset())
	{
		const float fDistFromCenter = (passInfo.GetCamera().GetPosition() - pFr->aabbCasters.GetCenter()).GetLength() + fDistFromViewDynamicLod + fRadiusDynamicLod;
		if (fDistFromCenter > pFr->aabbCasters.GetSize().x / 2.0f)
		{
			nUpdateStrategy = ShadowMapFrustum::ShadowCacheData::eFullUpdate;

			if (!gEnv->IsEditing())
			{
				CryLog("Update required for cached shadow map %d.", shadowCacheLod);
				CryLog("\tConsider increasing shadow cache resolution (r_ShadowsCacheResolutions) " \
				  "or setting up manual bounds for cached shadows via flow graph if this happens too often");
			}
		}
	}

	AABB projectionBoundsLS(AABB::RESET);
	const int nTexRes = GetRenderer()->GetCachedShadowsResolution()[shadowCacheLod];

	// non incremental update: set new bounding box and estimate near/far planes
	if (nUpdateStrategy != ShadowMapFrustum::ShadowCacheData::eIncrementalUpdate)
	{
		Matrix34 matView = Matrix34(GetViewMatrix(passInfo).GetTransposed());

		if (!Get3DEngine()->m_CachedShadowsBounds.IsReset())
		{
			float fBoxScale = powf(Get3DEngine()->m_fCachedShadowsCascadeScale, float(shadowCacheLod));
			Vec3 fBoxScaleXY(max(1.f, fBoxScale));
			fBoxScaleXY.z = 1.f;

			Vec3 vExt = Get3DEngine()->m_CachedShadowsBounds.GetSize().CompMul(fBoxScaleXY * 0.5f);
			Vec3 vCenter = Get3DEngine()->m_CachedShadowsBounds.GetCenter();

			pFr->aabbCasters = AABB(vCenter - vExt, vCenter + vExt);
			projectionBoundsLS = AABB::CreateTransformedAABB(matView, pFr->aabbCasters);
		}
		else
		{
			const float fDesiredPixelDensity = fRadiusDynamicLod / GetCVars()->e_ShadowsMaxTexRes;
			GetCasterBox(pFr->aabbCasters, projectionBoundsLS, fDesiredPixelDensity * nTexRes, matView, passInfo);
		}
	}

	// finally init frustum
	pFr->m_eFrustumType = ShadowMapFrustum::e_GsmCached;
	pFr->bBlendFrustum = GetCVars()->e_ShadowsBlendCascades > 0;
	pFr->fBlendVal = pFr->bBlendFrustum ? GetCVars()->e_ShadowsBlendCascadesVal : 1.0f;
	InitCachedFrustum(pFr, nUpdateStrategy, nLod, shadowCacheLod, nTexRes, m_pLightEntity->GetLightProperties().m_Origin, projectionBoundsLS, passInfo);

	// frustum debug
	if (GetCVars()->e_ShadowsCacheUpdate > 2 || GetCVars()->e_ShadowsFrustums > 0)
	{
		if (IRenderAuxGeom* pAux = GetRenderer()->GetIRenderAuxGeom())
		{
			SAuxGeomRenderFlags prevAuxFlags = pAux->GetRenderFlags();
			pAux->SetRenderFlags(e_Mode3D | e_AlphaNone | e_DepthTestOn);

			const ColorF cascadeColors[] = { Col_Red, Col_Green, Col_Blue, Col_Yellow, Col_Magenta, Col_Cyan };
			const uint colorCount = CRY_ARRAY_COUNT(cascadeColors);

			if (GetCVars()->e_ShadowsCacheUpdate > 2)
				pAux->DrawAABB(pFr->aabbCasters, false, cascadeColors[pFr->nShadowMapLod % colorCount], eBBD_Faceted);

			if (GetCVars()->e_ShadowsFrustums > 0)
				pFr->DrawFrustum(GetRenderer(), std::numeric_limits<int>::max());

			pAux->SetRenderFlags(prevAuxFlags);
		}
	}
}

void ShadowCacheGenerator::InitCachedFrustum(ShadowMapFrustumPtr& pFr, ShadowMapFrustum::ShadowCacheData::eUpdateStrategy nUpdateStrategy, int nLod, int cacheLod, int nTexSize, const Vec3& vLightPos, const AABB& projectionBoundsLS, const SRenderingPassInfo& passInfo)
{
	const auto frameID = passInfo.GetFrameID();

	pFr->ResetCasterLists();
	pFr->nTexSize = nTexSize;

	if (nUpdateStrategy != ShadowMapFrustum::ShadowCacheData::eIncrementalUpdate)
	{
		CRY_ASSERT(cacheLod >= 0 && cacheLod < MAX_GSM_CACHED_LODS_NUM);

		pFr->pShadowCacheData->Reset(GetNextGenerationID());
		pFr->GetSideSampleMask().store(1);

		assert(m_pLightEntity->GetLightProperties().m_pOwner);
		pFr->pLightOwner = m_pLightEntity->GetLightProperties().m_pOwner;
		pFr->m_Flags = m_pLightEntity->GetLightProperties().m_Flags;
		pFr->nUpdateFrameId = frameID;
		pFr->nShadowMapLod = nLod;
		pFr->nShadowCacheLod = cacheLod;
		pFr->vProjTranslation = pFr->aabbCasters.GetCenter();
		pFr->vLightSrcRelPos = vLightPos - pFr->aabbCasters.GetCenter();
		pFr->fNearDist = -projectionBoundsLS.max.z;
		pFr->fFarDist = -projectionBoundsLS.min.z;
		pFr->fRendNear = pFr->fNearDist;
		pFr->fFOV = (float)RAD2DEG(atan_tpl(0.5 * projectionBoundsLS.GetSize().y / pFr->fNearDist)) * 2.f;
		pFr->fProjRatio = projectionBoundsLS.GetSize().x / projectionBoundsLS.GetSize().y;
		pFr->fRadius = m_pLightEntity->GetLightProperties().m_fRadius;
		pFr->fRendNear = pFr->fNearDist;
		pFr->fFrustrumSize = 1.0f / (Get3DEngine()->m_fGsmRange * pFr->aabbCasters.GetRadius() * 2.0f);
		pFr->bUseShadowsPool = false;

		const float arrWidthS[] = { 1.94f, 1.0f, 0.8f, 0.5f, 0.3f, 0.3f, 0.3f, 0.3f };
		pFr->fWidthS = pFr->fWidthT = arrWidthS[nLod];
		pFr->fBlurS = pFr->fBlurT = 0.0f;
	}

	// set up frustum planes for culling
	const ShadowMapInfo* pShadowMapInfo = m_pLightEntity->GetShadowMapInfo();
	const bool isExtendedFrustum = GetCVars()->e_ShadowsCacheExtendLastCascade && nLod == pShadowMapInfo->GetLodCount() - 1 && pFr->m_eFrustumType == ShadowMapFrustum::e_GsmCached;

	if (isExtendedFrustum)
	{
		CCamera frustumCam;
		Vec3 vLightDir = -pFr->vLightSrcRelPos.normalized();

		Matrix34A mat = Matrix33::CreateRotationVDir(vLightDir);
		mat.SetTranslation(pFr->vLightSrcRelPos + pFr->vProjTranslation);

		frustumCam.SetMatrixNoUpdate(mat);
		frustumCam.SetFrustum(256, 256, pFr->fFOV * (gf_PI / 180.0f), pFr->fNearDist, pFr->fFarDist);

		pFr->FrustumPlanes[0] = pFr->FrustumPlanes[1] = frustumCam;
	}
	else
	{
		ShadowMapFrustum* pDynamicFrustum = m_pLightEntity->GetShadowFrustum(nLod);
		CRY_ASSERT(pDynamicFrustum);

		pFr->FrustumPlanes[0] = pDynamicFrustum->FrustumPlanes[0];
		pFr->FrustumPlanes[1] = pDynamicFrustum->FrustumPlanes[1];
	}

	const bool bExcludeDynamicDistanceShadows = GetCVars()->e_DynamicDistanceShadows != 0;
	const bool bUseCastersHull = (nUpdateStrategy == ShadowMapFrustum::ShadowCacheData::eFullUpdateTimesliced);
	const int maxNodesPerFrame = (nUpdateStrategy == ShadowMapFrustum::ShadowCacheData::eIncrementalUpdate)
	                             ? GetCVars()->e_ShadowsCacheMaxNodesPerFrame * GetRenderer()->GetActiveGPUCount()
	                             : std::numeric_limits<int>::max();

	m_pObjManager->MakeStaticShadowCastersList(((CLightEntity*)m_pLightEntity->GetLightProperties().m_pOwner)->GetCastingException(), pFr,
	                                           bUseCastersHull ? &m_pLightEntity->GetCastersHull() : nullptr,
	                                           bExcludeDynamicDistanceShadows ? ERF_DYNAMIC_DISTANCESHADOWS : 0, maxNodesPerFrame, passInfo);
	AddTerrainCastersToFrustum(pFr, passInfo);

	pFr->Invalidate();
	pFr->bIncrementalUpdate = nUpdateStrategy == ShadowMapFrustum::ShadowCacheData::eIncrementalUpdate && pFr->pShadowCacheData->mObjectsRendered != 0;
}

void ShadowCacheGenerator::InitHeightMapAOFrustum(ShadowMapFrustumPtr& pFr, int nLod, int nFirstStaticLod, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;
	assert(nLod >= 0);

	if (!pFr)
		pFr = new ShadowMapFrustum;

	if (!pFr->pShadowCacheData)
		pFr->pShadowCacheData = std::make_shared<ShadowMapFrustum::ShadowCacheData>();

	static ICVar* pHeightMapAORes = gEnv->pConsole->GetCVar("r_HeightMapAOResolution");
	static ICVar* pHeightMapAORange = gEnv->pConsole->GetCVar("r_HeightMapAORange");

	ShadowMapFrustum::ShadowCacheData::eUpdateStrategy nUpdateStrategy = m_nUpdateStrategy;

	// check if we have come too close to the border of the map
	const float fDistFromCenter = (passInfo.GetCamera().GetPosition() - pFr->aabbCasters.GetCenter()).GetLength() + pHeightMapAORange->GetFVal() * 0.25f;
	if (fDistFromCenter > pFr->aabbCasters.GetSize().x / 2.0f)
	{
		nUpdateStrategy = ShadowMapFrustum::ShadowCacheData::eFullUpdate;

		if (!gEnv->IsEditing())
		{
			CryLog("Update required for height map AO.");
			CryLog("\tConsider increasing height map AO range (r_HeightMapAORange) if this happens too often");
		}
	}

	AABB projectionBoundsLS(AABB::RESET);

	// non incremental update: set new bounding box and estimate near/far planes
	if (nUpdateStrategy != ShadowMapFrustum::ShadowCacheData::eIncrementalUpdate)
	{
		// Top down view
		Matrix34 topDownView(IDENTITY);
		topDownView.m03 = -passInfo.GetCamera().GetPosition().x;
		topDownView.m13 = -passInfo.GetCamera().GetPosition().y;
		topDownView.m23 = -passInfo.GetCamera().GetPosition().z - m_pLightEntity->GetLightProperties().m_Origin.GetLength();

		GetCasterBox(pFr->aabbCasters, projectionBoundsLS, pHeightMapAORange->GetFVal() / 2.0f, topDownView, passInfo);

		// snap to texels
		const float fSnap = pHeightMapAORange->GetFVal() / pHeightMapAORes->GetFVal();
		pFr->aabbCasters.min.x = fSnap * int(pFr->aabbCasters.min.x / fSnap);
		pFr->aabbCasters.min.y = fSnap * int(pFr->aabbCasters.min.y / fSnap);
		pFr->aabbCasters.min.z = fSnap * int(pFr->aabbCasters.min.z / fSnap);

		pFr->aabbCasters.max.x = pFr->aabbCasters.min.x + pHeightMapAORange->GetFVal();
		pFr->aabbCasters.max.y = pFr->aabbCasters.min.y + pHeightMapAORange->GetFVal();
		pFr->aabbCasters.max.z = fSnap * int(pFr->aabbCasters.max.z / fSnap);

		pFr->fDepthSlopeBias = AO_FRUSTUM_SLOPE_BIAS;
		pFr->fDepthConstBias = 0;

		pFr->mLightViewMatrix.SetIdentity();
		pFr->mLightViewMatrix.m30 = -pFr->aabbCasters.GetCenter().x;
		pFr->mLightViewMatrix.m31 = -pFr->aabbCasters.GetCenter().y;
		pFr->mLightViewMatrix.m32 = -pFr->aabbCasters.GetCenter().z - m_pLightEntity->GetLightProperties().m_Origin.GetLength();

		mathMatrixOrtho(&pFr->mLightProjMatrix, projectionBoundsLS.GetSize().x, projectionBoundsLS.GetSize().y, -projectionBoundsLS.max.z, -projectionBoundsLS.min.z);
	}

	const Vec3 lightPos = pFr->aabbCasters.GetCenter() + Vec3(0, 0, 1) * m_pLightEntity->GetLightProperties().m_Origin.GetLength();

	// finally init frustum
	const int nTexRes = (int)clamp_tpl(pHeightMapAORes->GetFVal(), 0.f, 16384.f);
	pFr->m_eFrustumType = ShadowMapFrustum::e_HeightMapAO;
	InitCachedFrustum(pFr, nUpdateStrategy, nLod, nLod - nFirstStaticLod, nTexRes, lightPos, projectionBoundsLS, passInfo);
}

void ShadowCacheGenerator::GetCasterBox(AABB& BBoxWS, AABB& BBoxLS, float fRadius, const Matrix34& matView, const SRenderingPassInfo& passInfo)
{
	AABB projectionBoundsLS;

	BBoxWS = AABB(passInfo.GetCamera().GetPosition(), fRadius);
	BBoxLS = AABB(matView.TransformPoint(passInfo.GetCamera().GetPosition()), fRadius);

	// try to get tighter near/far plane from casters
	AABB casterBoxLS = Get3DEngine()->m_pObjectsTree->GetShadowCastersBox(&BBoxWS, &matView);

	if (CVisAreaManager* pVisAreaManager = GetVisAreaManager())
	{
		for (int i = 0; i < pVisAreaManager->m_lstVisAreas.Count(); ++i)
		{
			if (pVisAreaManager->m_lstVisAreas[i] && pVisAreaManager->m_lstVisAreas[i]->IsObjectsTreeValid())
			{
				casterBoxLS.Add(pVisAreaManager->m_lstVisAreas[i]->GetObjectsTree()->GetShadowCastersBox(&BBoxWS, &matView));
			}
		}

		for (int i = 0; i < pVisAreaManager->m_lstPortals.Count(); ++i)
		{
			if (pVisAreaManager->m_lstPortals[i] && pVisAreaManager->m_lstPortals[i]->IsObjectsTreeValid())
			{
				casterBoxLS.Add(pVisAreaManager->m_lstPortals[i]->GetObjectsTree()->GetShadowCastersBox(&BBoxWS, &matView));
			}
		}
	}

	if (!casterBoxLS.IsReset() && casterBoxLS.GetSize().z < 2 * fRadius)
	{
		float fDepthRange = 2.0f * max(Get3DEngine()->m_fSunClipPlaneRange, casterBoxLS.GetSize().z);
		BBoxLS.max.z = casterBoxLS.max.z + 0.5f; // slight offset here to counter edge case where polygons are projection plane aligned and would come to lie directly on the near plane
		BBoxLS.min.z = casterBoxLS.max.z - fDepthRange;
	}
}

Matrix44 ShadowCacheGenerator::GetViewMatrix(const SRenderingPassInfo& passInfo)
{
	const Vec3 zAxis(0.f, 0.f, 1.f);
	const Vec3 yAxis(0.f, 1.f, 0.f);

	Vec3 At = passInfo.GetCamera().GetPosition();
	Vec3 Eye = m_pLightEntity->GetLightProperties().m_Origin;
	Vec3 Up = fabsf((Eye - At).GetNormalized().Dot(zAxis)) > 0.9995f ? yAxis : zAxis;

	Matrix44 result;
	mathMatrixLookAt(&result, Eye, At, Up);

	return result;
}

void ShadowCacheGenerator::AddTerrainCastersToFrustum(ShadowMapFrustum* pFr, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if ((Get3DEngine()->m_bSunShadowsFromTerrain || pFr->m_eFrustumType == ShadowMapFrustum::e_HeightMapAO) && !pFr->bIsMGPUCopy)
	{
		PodArray<CTerrainNode*> lstTerrainNodes;
		GetTerrain()->IntersectWithBox(pFr->aabbCasters, &lstTerrainNodes);

		bool bCastersFound = false;

		for (int s = 0; s < lstTerrainNodes.Count(); s++)
		{
			CTerrainNode* pNode = lstTerrainNodes[s];

			const float optimalTerrainSegmentSize = 128.f;
			if (pNode->GetBBox().GetSize().x != optimalTerrainSegmentSize)
				continue;

			if (!pFr->NodeRequiresShadowCacheUpdate(pNode))
				continue;

			bCastersFound = true;

			pNode->SetTraversalFrameId(passInfo.GetMainFrameID(), pFr->nShadowMapLod);
		}

		if (bCastersFound)
			pFr->RequestUpdate();
	}
}
