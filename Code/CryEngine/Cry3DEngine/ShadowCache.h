// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef STATIC_SHADOWS_H
#define STATIC_SHADOWS_H

#include <CryCore/Platform/platform.h>
#include "../RenderDll/Common/Shadow_Renderer.h"

class ShadowCacheGenerator : public Cry3DEngineBase
{
public:
	ShadowCacheGenerator(CLightEntity* pLightEntity, ShadowMapFrustum::ShadowCacheData::eUpdateStrategy nUpdateStrategy)
		: m_pLightEntity(pLightEntity)
		, m_nUpdateStrategy(nUpdateStrategy)
	{}

	static void  ResetGenerationID() { m_cacheGenerationId = 0; }

	void InitShadowFrustum(ShadowMapFrustumPtr& pFr, int nLod, int nFirstStaticLod, float fDistFromViewDynamicLod, float fRadiusDynamicLod, const SRenderingPassInfo& passInfo);
	void InitHeightMapAOFrustum(ShadowMapFrustumPtr& pFr, int nLod, int nFirstStaticLod, const SRenderingPassInfo& passInfo);

private:
	static const int    MAX_RENDERNODES_PER_FRAME = 50;
	static const float  AO_FRUSTUM_SLOPE_BIAS;

	static int   m_cacheGenerationId;

	void         InitCachedFrustum(ShadowMapFrustumPtr& pFr, ShadowMapFrustum::ShadowCacheData::eUpdateStrategy nUpdateStrategy, int nLod, int cacheLod, int nTexSize, const Vec3& vLightPos, const AABB& projectionBoundsLS, const SRenderingPassInfo& passInfo);
	void         AddTerrainCastersToFrustum(ShadowMapFrustum* pFr, const SRenderingPassInfo& passInfo);

	void         GetCasterBox(AABB& BBoxWS, AABB& BBoxLS, float fRadius, const Matrix34& matView, const SRenderingPassInfo& passInfo);
	Matrix44     GetViewMatrix(const SRenderingPassInfo& passInfo);

	uint8        GetNextGenerationID() const;

	CLightEntity* m_pLightEntity;
	ShadowMapFrustum::ShadowCacheData::eUpdateStrategy m_nUpdateStrategy;
};
#endif
