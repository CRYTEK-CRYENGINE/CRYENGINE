// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"
#include "Common/RenderView.h"
#include <array>

class CShadowMapStage : public CGraphicsPipelineStage
{
	enum EPerPassTexture
	{
		EPerPassTexture_TerrainElevMap = 26,
		EPerPassTexture_WindGrid = 27,
		EPerPassTexture_TerrainBaseMap = 29,
		EPerPassTexture_DissolveNoise  = 31
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
	virtual void Init() override;
	virtual void Prepare(CRenderView* pRenderView) override;

	void         Execute();

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);

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

		void                    PrepareResources();
		void                    PreRender();

		CDeviceResourceSetPtr   GetResources() { return m_pPerPassResources; }
		SShadowFrustumToRender* GetFrustum()   { return m_pFrustumToRender; }

		SShadowFrustumToRender*  m_pFrustumToRender;
		int                      m_nShadowFrustumSide;
		bool                     m_bRequiresRender;

		SDepthTexture            m_currentDepthTarget;
		std::array<CTexture*, 2> m_currentColorTarget;
		CConstantBufferPtr       m_pPerPassConstantBuffer;
		CConstantBufferPtr       m_pPerViewConstantBuffer;
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
		CShadowMapPassGroup() : m_PassCount(0){}

		void                     Init(CShadowMapStage* pStage, int nSize);
		void                     Reset()               { m_PassCount = 0; }
		void                     Clear()               { m_Passes.clear(); }

		PassList::iterator       begin()               { return m_Passes.begin(); }
		PassList::iterator       end()                 { return m_Passes.begin() + m_PassCount; }
		PassList::const_iterator begin() const         { return m_Passes.begin(); }
		PassList::const_iterator end()   const         { return m_Passes.begin() + m_PassCount; }

		int                      GetCount() const      { return m_PassCount; }
		int                      GetCapacity() const   { return m_Passes.size(); }
		CShadowMapPass&          operator[](int index) { return m_Passes[index]; }

		CShadowMapPass&          AddPass()             { CRY_ASSERT(m_PassCount < m_Passes.size()); return m_Passes[m_PassCount++]; }
		void                     UndoAddPass()         { CRY_ASSERT(m_PassCount > 0); --m_PassCount; }

	private:
		PassList m_Passes;
		int      m_PassCount;
	};

private:
	bool CreatePipelineState(const SGraphicsPipelineStateDescription& description, EPass passID, CDeviceGraphicsPSOPtr& outPSO);

	void PrepareShadowPool(CRenderView* pMainView);
	void PrepareShadowPasses(SShadowFrustumToRender& frustumToRender, CRenderView::eShadowFrustumRenderType frustumRenderType);

	void PreparePassIDForFrustum(const SShadowFrustumToRender& frustumToRender, CRenderView::eShadowFrustumRenderType frustumRenderType, EPass& passID, ProfileLabel& profileLabel) const;
	void PrepareShadowPassForFrustum(const SShadowFrustumToRender& frustumToRender, int nSide, CShadowMapPass& targetPass) const;
	bool PrepareOutputsForPass(const SShadowFrustumToRender& frustumToRender, int nSide, CShadowMapPass& targetPass) const;
	void PrepareOutputsForFrustumWithCaching(const ShadowMapFrustum& frustum, CTexture*& pDepthTarget, const CShadowMapPass*& pClearDepthMapProvider, CShadowMapPass::eClearMode& clearMode) const;

	void UpdateShadowFrustumFromPass(const CShadowMapPass& sourcePass, ShadowMapFrustum& targetFrustum) const;
	void CopyShadowMap(const CShadowMapPass& sourcePass, CShadowMapPass& targetPass);

	typedef std::array<CShadowMapPassGroup, CRenderView::eShadowFrustumRenderType_Count> PassGroupList;

	PassGroupList            m_ShadowMapPasses;
	CFullscreenPass          m_CopyShadowMapPass;
	CDeviceResourceLayoutPtr m_pResourceLayout;
	CDeviceResourceSetPtr    m_pPerPassResourceSetTemplate;
};
