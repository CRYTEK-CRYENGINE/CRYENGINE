// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CWaterRipplesStage : public CGraphicsPipelineStage
{
public:
	CWaterRipplesStage();
	~CWaterRipplesStage();

	void Init() override;
	void Prepare(CRenderView* pRenderView) override;

	void Execute(CRenderView* pRenderView);

	Vec4 GetWaterRippleLookupParam() const
	{
		return m_lookupParam;
	}

	CTexture* GetWaterRippleTex() const;

	bool IsVisible(CRenderView* pRenderView) const;

private:
	bool Update(CRenderView* pRenderView);
	void ExecuteWaterRipples(CRenderView* pRenderView, CTexture* pTargetTex, const D3DViewPort& viewport);
	void UpdateAndDrawDebugInfo(CRenderView* pRenderView);

private:
	static const int32                         sVertexCount = 4;
	static const int32                         sTotalVertexCount = sVertexCount * SWaterRippleInfo::MaxWaterRipplesInScene;
	static const EDefaultInputLayouts::PreDefs sVertexFormat = EDefaultInputLayouts::P3F_C4B_T2F;
	static const size_t                        sVertexStride = sizeof(SVF_P3F_C4B_T2F);

	typedef SVF_P3F_C4B_T2F SVertex;

	struct SWaterRippleRecord
	{
		SWaterRippleInfo info;
		float            lifetime;

		SWaterRippleRecord(const SWaterRippleInfo& srcInfo, float srcLifetime)
			: info(srcInfo)
			, lifetime(srcLifetime)
		{}
	};

private:
	_smart_ptr<CTexture>          m_pTexWaterRipplesDDN; // xy: wave propagation normals, z: frame t-2, w: frame t-1
	_smart_ptr<CTexture>          m_pTempTexture;

	CFullscreenPass               m_passSnapToCenter;
	CStretchRectPass              m_passCopy;
	CFullscreenPass               m_passWaterWavePropagation;
	CPrimitiveRenderPass          m_passAddWaterRipples;
	CMipmapGenPass                m_passMipmapGen;

	CRenderPrimitive              m_ripplePrimitive[SWaterRippleInfo::MaxWaterRipplesInScene];
	buffer_handle_t               m_vertexBuffer; // stored all ripples' vertices.

	SResourceRegionMapping        m_TempCopyParams;

	ICVar*                        m_pCVarWaterRipplesDebug;

	CCryNameTSCRC                 m_ripplesGenTechName;
	CCryNameTSCRC                 m_ripplesHitTechName;
	CCryNameR                     m_ripplesParamName;
	CCryNameR                     m_ripplesTransformParamName;

	int32                         m_frameID;

	float                         m_lastSpawnTime;
	float                         m_lastUpdateTime;

	float                         m_simGridSnapRange;
	Vec2                          m_simOrigin;

	int32                         m_updateMask;
	Vec4                          m_shaderParam;
	Vec4                          m_lookupParam;

	bool                          m_bInitializeSim;
	bool                          m_bSnapToCenter;

	std::vector<SWaterRippleInfo> m_waterRipples;
	std::vector<SWaterRippleInfo> m_waterRipplesMGPU;

#if !defined(_RELEASE)
	std::vector<SWaterRippleRecord> m_debugRippleInfos;
#endif
};
