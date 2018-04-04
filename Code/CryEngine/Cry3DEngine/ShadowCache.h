// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef STATIC_SHADOWS_H
#define STATIC_SHADOWS_H

#include <CryCore/Platform/platform.h>
#include "../RenderDll/Common/Shadow_Renderer.h"

class ShadowCache : public Cry3DEngineBase
{
public:
	ShadowCache(CLightEntity* pLightEntity, ShadowMapFrustum::ShadowCacheData::eUpdateStrategy nUpdateStrategy)
		: m_pLightEntity(pLightEntity)
		, m_nUpdateStrategy(nUpdateStrategy)
	{}

	void InitShadowFrustum(ShadowMapFrustumPtr& pFr, int nLod, int nFirstStaticLod, float fDistFromViewDynamicLod, float fRadiusDynamicLod, const SRenderingPassInfo& passInfo);
	void InitHeightMapAOFrustum(ShadowMapFrustumPtr& pFr, int nLod, const SRenderingPassInfo& passInfo);

private:
	static const int    MAX_RENDERNODES_PER_FRAME = 50;
	static const float  AO_FRUSTUM_SLOPE_BIAS;
	static const uint64 kHashMul = 0x9ddfea08eb382d69ULL;

	void         InitCachedFrustum(ShadowMapFrustumPtr& pFr, ShadowMapFrustum::ShadowCacheData::eUpdateStrategy nUpdateStrategy, int nLod, int nTexSize, const Vec3& vLightPos, const AABB& projectionBoundsLS, const SRenderingPassInfo& passInfo);
	void         AddTerrainCastersToFrustum(ShadowMapFrustum* pFr, const SRenderingPassInfo& passInfo);

	void         GetCasterBox(AABB& BBoxWS, AABB& BBoxLS, float fRadius, const Matrix34& matView, const SRenderingPassInfo& passInfo);
	Matrix44     GetViewMatrix(const SRenderingPassInfo& passInfo);

	ILINE uint64 HashValue(uint64 value);
	ILINE uint64 HashTerrainNode(const CTerrainNode* pNode, int lod);

	CLightEntity* m_pLightEntity;
	ShadowMapFrustum::ShadowCacheData::eUpdateStrategy m_nUpdateStrategy;
};
#endif
