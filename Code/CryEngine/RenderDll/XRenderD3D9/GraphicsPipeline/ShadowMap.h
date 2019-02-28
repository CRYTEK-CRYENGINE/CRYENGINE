// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"
#include "Common/RenderView.h"
#include "Common/UtilityPasses.h"
#include "SceneGBuffer.h"
#include <array>

class CShadowMapStage : public CGraphicsPipelineStage
{
	enum EPerPassTexture
	{
		EPerPassTexture_PerlinNoiseMap = CSceneGBufferStage::ePerPassTexture_PerlinNoiseMap,
		EPerPassTexture_WindGrid       = CSceneGBufferStage::ePerPassTexture_WindGrid,
		EPerPassTexture_TerrainElevMap = CSceneGBufferStage::ePerPassTexture_TerrainElevMap,
		EPerPassTexture_TerrainBaseMap = CSceneGBufferStage::ePerPassTexture_TerrainBaseMap,
		EPerPassTexture_DissolveNoise  = CSceneGBufferStage::ePerPassTexture_DissolveNoise,
	};

	enum EPass
	{
		ePass_DirectionalLight       = 0,
		ePass_DirectionalLightCached = 1,
		ePass_LocalLight             = 2,
		ePass_DirectionalLightRSM    = 3,
		ePass_LocalLightRSM          = 4,

		ePass_Count,
		ePass_First = ePass_DirectionalLight
	};

public:
	CShadowMapStage();

	void Init()   final;
	void Update() final;

	void ReAllocateResources();
	void Execute();

	bool CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);

private:
	typedef char ProfileLabel[32];

	class CShadowMapPass : public CSceneRenderPass
	{
	public:
		enum eClearMode
		{
			eClearMode_None,
			eClearMode_Fill,
			eClearMode_FillRect,
			eClearMode_CopyDepthMap
		};

	public:
		CShadowMapPass(CShadowMapStage* pStage);
		CShadowMapPass(CShadowMapPass&& other);

		bool                         PrepareResources(const CRenderView* pMainView);
		void                         PreRender();

		CDeviceResourceSetPtr        GetResources()       { return m_pPerPassResourceSet; }
		SShadowFrustumToRender*      GetFrustum()         { return m_pFrustumToRender; }
		const CDeviceRenderPassDesc& GetPassDesc()  const { return m_renderPassDesc; }

		SShadowFrustumToRender*  m_pFrustumToRender;
		int                      m_nShadowFrustumSide;
		bool                     m_bRequiresRender;

		CConstantBufferPtr       m_pPerPassConstantBuffer;
		CConstantBufferPtr       m_pPerViewConstantBuffer;
		CDeviceResourceSetDesc   m_perPassResources;
		Matrix44A                m_ViewProjMatrix;
		Matrix44A                m_ViewProjMatrixOrig;
		Vec4                     m_FrustumInfo;
		ProfileLabel             m_ProfileLabel;
		CShadowMapStage*         m_pShadowMapStage;

		eClearMode               m_clearMode;
		const CShadowMapPass*    m_pClearDepthMapProvider;
	};

	class CShadowMapPassGroup
	{
		typedef std::vector<CShadowMapPass> PassList;

	public:
		void                     Init(CShadowMapStage* pStage, int nSize, CTexture* pDepthTarget, CTexture* pColorTarget0, CTexture* pColorTarget1);
		void                     Reset()               { m_PassCount = 0; }
		void                     Clear()               { m_Passes.clear(); }

		PassList::iterator       begin()               { return m_Passes.begin(); }
		PassList::iterator       end()                 { return m_Passes.begin() + m_PassCount; }
		PassList::const_iterator begin() const         { return m_Passes.begin(); }
		PassList::const_iterator end()   const         { return m_Passes.begin() + m_PassCount; }

		int                      GetCount() const      { return m_PassCount; }
		int                      GetCapacity() const   { return m_Passes.size(); }
		CShadowMapPass&          operator[](int index) { return m_Passes[index]; }

		CShadowMapPass&          AddPass();
		void                     UndoAddPass()         { CRY_ASSERT(m_PassCount > 0); --m_PassCount; }

	private:
		PassList         m_Passes;
		int              m_PassCount = 0;
		CShadowMapStage* m_pStage    = nullptr;

	};

private:
	typedef std::array<CShadowMapPassGroup, CShadowMapStage::ePass_Count> PassGroupList;

	bool CreatePipelineState(const SGraphicsPipelineStateDescription& description, EPass passID, CDeviceGraphicsPSOPtr& outPSO);

	void PrepareShadowPasses(SShadowFrustumToRender& frustumToRender, CRenderView::eShadowFrustumRenderType frustumRenderType);

	void PreparePassIDForFrustum(const SShadowFrustumToRender& frustumToRender, CRenderView::eShadowFrustumRenderType frustumRenderType, EPass& passID, ProfileLabel& profileLabel) const;
	void PrepareShadowPassForFrustum(const SShadowFrustumToRender& frustumToRender, int nSide, CShadowMapPass& targetPass) const;
	bool PrepareOutputsForPass(const SShadowFrustumToRender& frustumToRender, int nSide, CShadowMapPass& targetPass) const;
	void PrepareOutputsForFrustumWithCaching(const ShadowMapFrustum& frustum, CTexture*& pDepthTarget, const CShadowMapPass*& pClearDepthMapProvider, CShadowMapPass::eClearMode& clearMode) const;

	void UpdateShadowFrustumFromPass(const CShadowMapPass& sourcePass, ShadowMapFrustum& targetFrustum) const;
	void CopyShadowMap(const CShadowMapPass& sourcePass, CShadowMapPass& targetPass);
	void ClearShadowMaps(PassGroupList& shadowMapPasses);

	ETEX_Format GetShadowTexFormat(EPass passID) const;

	_smart_ptr<CTexture>     m_pRsmColorTex;
	_smart_ptr<CTexture>     m_pRsmNormalTex;
	_smart_ptr<CTexture>     m_pRsmPoolColorTex;
	_smart_ptr<CTexture>     m_pRsmPoolNormalTex;
	_smart_ptr<CTexture>     m_pRsmPoolDepth;

	PassGroupList            m_ShadowMapPasses;
	CFullscreenPass          m_CopyShadowMapPass;
	CClearRegionPass         m_ClearShadowPoolDepthPass;
	CClearRegionPass         m_ClearShadowPoolColorPass;
	CClearRegionPass         m_ClearShadowPoolNormalsPass;
	CDeviceResourceLayoutPtr m_pResourceLayout;
	CDeviceResourceSetDesc   m_perPassResources;
};
