// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/UtilityPasses.h"
#include "Common/FullscreenPass.h"
#include "Common/PrimitiveRenderPass.h"

class CSnowStage : public CGraphicsPipelineStage
{
public:
	CSnowStage();
	virtual ~CSnowStage();

	void Init() final;
	void Update() final;
	void Resize(int renderWidth, int renderHeight) final;
	void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) final;

	void         ExecuteDeferredSnowGBuffer();
	void         ExecuteDeferredSnowDisplacement();
	void         Execute();

private:
	// Snow particle properties
	struct SSnowCluster
	{
		// set default data
		SSnowCluster()
			: m_pPos(0, 0, 0)
			, m_pPosPrev(0, 0, 0)
			, m_fSpawnTime(0.0f)
			, m_fLifeTime(4.0f)
			, m_fLifeTimeVar(2.5f)
			, m_fWeight(0.3f)
			, m_fWeightVar(0.1f)
		{
		}

		std::unique_ptr<CRenderPrimitive> m_pPrimitive;

		// World position
		Vec3 m_pPos;
		Vec3 m_pPosPrev;

		// Spawn time
		float m_fSpawnTime;

		// Life time and variation
		float m_fLifeTime;
		float m_fLifeTimeVar;

		// Weight and variation
		float m_fWeight;
		float m_fWeightVar;
	};

private:
	void ResizeResource(int renderWidth, int renderHeight);

	bool GenerateSnowClusterVertex();
	void CreateSnowClusters();
	void UpdateSnowClusters();
	void RenderSnowClusters();
	void ExecuteHalfResComposite();
	void GetScissorRegion(const Vec3& cameraOrigin, const Vec3& vCenter, float fRadius, int32& sX, int32& sY, int32& sWidth, int32& sHeight) const;

private:
	_smart_ptr<CTexture>      m_pSnowFlakesTex;
	_smart_ptr<CTexture>      m_pSnowDerivativesTex;
	_smart_ptr<CTexture>      m_pSnowSpatterTex;
	_smart_ptr<CTexture>      m_pFrostBubblesBumpTex;
	_smart_ptr<CTexture>      m_pSnowFrostBumpTex;
	_smart_ptr<CTexture>      m_pSnowDisplacementTex;

	CStretchRectPass          m_passCopyGBufferNormal;
	CStretchRectPass          m_passCopyGBufferSpecular;
	CStretchRectPass          m_passCopyGBufferDiffuse;
	CFullscreenPass           m_passDeferredSnowGBuffer;
	CFullscreenPass           m_passParallaxSnowHeightMapGen;
	CFullscreenPass           m_passParallaxSnowMin;
	CStretchRectPass          m_passCopySceneToParallaxSnowSrc;
	CFullscreenPass           m_passParallaxSnow[3];
	CStretchRectPass          m_passCopySceneTargetTexture;
	CPrimitiveRenderPass      m_passSnow;
	CFullscreenPass           m_passSnowHalfResCompisite;

	SRainParams               m_RainVolParams; // needed for occlusion.
	SSnowParams               m_SnowVolParams;

	std::vector<SSnowCluster> m_snowClusterArray;

	buffer_handle_t           m_snowFlakeVertexBuffer = ~0u;

	int32                     m_nSnowFlakeVertCount = 0;
	int32                     m_nAliveClusters = 0;
	int32                     m_nNumClusters = 0;
	int32                     m_nFlakesPerCluster = 0;

};
