// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaterRipples.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"

//////////////////////////////////////////////////////////////////////////

CWaterRipplesStage::CWaterRipplesStage()
	: m_vertexBuffer(~0u)
	, m_pCVarWaterRipplesDebug(nullptr)
	, m_ripplesGenTechName("WaterRipplesGen")
	, m_ripplesHitTechName("WaterRipplesHit")
	, m_frameID(-1)
	, m_lastSpawnTime(0.0f)
	, m_lastUpdateTime(0.0f)
	, m_simGridSnapRange(5.0f)
	, m_simOrigin(0.0f)
	, m_updateMask(0)
	, m_shaderParam(0.0f)
	, m_lookupParam(0.0f)
	, m_bInitializeSim(true)
	, m_bSnapToCenter(false)
{
}

CWaterRipplesStage::~CWaterRipplesStage()
{
	if (m_vertexBuffer != ~0u)
	{
		gRenDev->m_DevBufMan.Destroy(m_vertexBuffer);
	}
}

void CWaterRipplesStage::Init()
{
	CRY_ASSERT(m_vertexBuffer == ~0u);
	m_vertexBuffer = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, sTotalVertexCount * sVertexStride);

	const int32 flags = FT_FORCE_MIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;

	CRY_ASSERT(m_pTexWaterRipplesDDN == nullptr);
	m_pTexWaterRipplesDDN = CTexture::GetOrCreateTextureObjectPtr("$WaterRipplesDDN_0", nGridSize, nGridSize, 1, eTT_2D, flags, eTF_R8G8B8A8S);

	CRY_ASSERT(m_pTempTexture == nullptr);
	m_pTempTexture = CTexture::GetOrCreateTextureObjectPtr("$WaterRippleGenTemp", nGridSize, nGridSize, 1, eTT_2D, flags, eTF_R8G8B8A8);

	// Shared constant buffer
	m_constants.CreateDeviceBuffer();
	m_constants.UploadZeros();

	m_passSnapToCenter.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, m_constants.GetDeviceConstantBuffer(), EShaderStage_Vertex | EShaderStage_Pixel);
	m_passWaterWavePropagation.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, m_constants.GetDeviceConstantBuffer(), EShaderStage_Pixel);

	int32 vertexOffset = 0;
	for (auto& prim : m_ripplePrimitive)
	{
		prim.SetCullMode(eCULL_None);
		prim.SetCustomVertexStream(m_vertexBuffer, sVertexFormat, sVertexStride);
		prim.SetCustomIndexStream(~0u, RenderIndexType(0));
		prim.SetDrawInfo(eptTriangleStrip, 0, vertexOffset, sVertexCount);
		prim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, m_constants.GetDeviceConstantBuffer(), EShaderStage_Vertex);

		// only update blue channel: current frame
		prim.SetRenderState(GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST | GS_NOCOLMASK_RGA);

		vertexOffset += sVertexCount;
	}
}

void CWaterRipplesStage::Update()
{
	CRY_ASSERT(m_pTexWaterRipplesDDN);

	const auto shouldApplyRipples = IsVisible();

	// Create/release the occlusion texture on demand
	if (!shouldApplyRipples && CTexture::IsTextureExist(m_pTexWaterRipplesDDN))
		m_pTexWaterRipplesDDN->ReleaseDeviceTexture(false);
	else if (shouldApplyRipples && !CTexture::IsTextureExist(m_pTexWaterRipplesDDN))
		m_pTexWaterRipplesDDN->CreateRenderTarget(eTF_R8G8B8A8S, Clr_Transparent);

	// Create/release the occlusion texture on demand
	if (!shouldApplyRipples && CTexture::IsTextureExist(m_pTempTexture))
		m_pTempTexture->ReleaseDeviceTexture(false);
	else if (shouldApplyRipples && !CTexture::IsTextureExist(m_pTempTexture))
		m_pTempTexture->CreateRenderTarget(eTF_R8G8B8A8, Clr_Transparent);

	if (shouldApplyRipples && CTexture::IsTextureExist(m_pTempTexture))
	{
		m_TempCopyParams =
		{
			{ 0, 0, 0, 0 }, // src position
			{ 0, 0, 0, 0 }, // dst position
			m_pTempTexture->GetDevTexture()->GetDimension()
		};
	}

	if (!m_pCVarWaterRipplesDebug)
	{
		m_pCVarWaterRipplesDebug = gEnv->pConsole->GetCVar("e_WaterRipplesDebug");
	}
}

bool CWaterRipplesStage::RefreshParameters()
{
	const CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	const bool bAlreadyUpdated = (m_frameID == pRenderView->GetFrameId());
	if (bAlreadyUpdated)
	{
		return false;
	}
	m_frameID = pRenderView->GetFrameId();

	const auto& waterRipples = pRenderView->GetWaterRipples();
	for (auto& ripple : waterRipples)
	{
		if (m_waterRipples.size() < SWaterRippleInfo::MaxWaterRipplesInScene)
		{
			m_waterRipples.emplace_back(ripple);
		}
	}

#if !defined(_RELEASE)
	UpdateAndDrawDebugInfo();
#endif

	const uint32 nThreadID = gRenDev->GetRenderThreadID();
	const float fTime = GetGraphicsPipeline().GetAnimationTime().GetSeconds();
	const uint32 gpuCount = gRenDev->GetActiveGPUCount();

	const float simGridSize = static_cast<float>(SWaterRippleInfo::WaveSimulationGridSize);
	const float halfSimGridSize = 0.5f * simGridSize;

	const auto& viewInfo = GetCurrentViewInfo();

	// always snap by entire pixels to avoid errors when displacing the simulation
	const float fPixelSizeWS =
	  (nGridSize > 0)
	  ? (simGridSize / nGridSize)
	  : 1.0f;
	m_simGridSnapRange = max(ceilf(m_simGridSnapRange / fPixelSizeWS), 1.f) * fPixelSizeWS;

	// only allow updates every 25ms - for lower frames, would need to iterate multiple times
	if (gpuCount == 1)
	{
		if (std::abs(fTime - m_lastUpdateTime) < 0.025f)
		{
			return false;
		}

		m_lastUpdateTime = fTime - fmodf(fTime - m_lastUpdateTime, 0.025f);
	}
	else
	{
		if (fTime - m_lastUpdateTime <= 0.0f)
		{
			return false;
		}

		m_lastUpdateTime = fTime;
	}

	Vec4 vParams = Vec4(0, 0, 0, 0);
	if (!m_waterRipples.empty())
	{
		vParams = Vec4(m_waterRipples[0].position.x, m_waterRipples[0].position.y, 0.0f, 1.0f);
		m_lastSpawnTime = fTime;
	}

	if (!m_updateMask)
	{
		const Vec3& cameraPos = viewInfo.cameraOrigin;
		const float xsnap = ceilf(cameraPos.x / m_simGridSnapRange) * m_simGridSnapRange;
		const float ysnap = ceilf(cameraPos.y / m_simGridSnapRange) * m_simGridSnapRange;

		m_bSnapToCenter = false;
		if (m_simOrigin.x != xsnap || m_simOrigin.y != ysnap)
		{
			m_bSnapToCenter = true;
			vParams.x = (xsnap - m_simOrigin.x) * m_lookupParam.x;
			vParams.y = (ysnap - m_simOrigin.y) * m_lookupParam.x;

			m_simOrigin = Vec2(xsnap, ysnap);
		}

		m_shaderParam = vParams;
		m_waterRipplesMGPU = m_waterRipples;
		m_waterRipples.clear();

		// update world space -> sim space transform
		m_lookupParam.x = 1.f / simGridSize;
		m_lookupParam.y = halfSimGridSize - m_simGridSnapRange;
		m_lookupParam.z = -m_simOrigin.x * m_lookupParam.x + 0.5f;
		m_lookupParam.w = -m_simOrigin.y * m_lookupParam.x + 0.5f;

		if (gpuCount > 1)
		{
			m_updateMask = (1 << gpuCount) - 1;
		}
	}

	return true;
}

void CWaterRipplesStage::Execute()
{
	if (!IsVisible())
	{
		m_bInitializeSim = true;

		// don't process water ripples.
		return;
	}

	if (!CTexture::IsTextureExist(m_pTexWaterRipplesDDN)
	    || !CTexture::IsTextureExist(m_pTempTexture))
	{
		return;
	}

	if (!RefreshParameters())
	{
		return;
	}

	{
		PROFILE_LABEL_SCOPE("WATER RIPPLES GEN");

		auto* pTempTex = m_pTempTexture.get();

		// Initialize sim on first frame
		if (m_bInitializeSim)
		{
			m_bInitializeSim = false;

			CClearSurfacePass::Execute(m_pTexWaterRipplesDDN, Clr_Transparent);
		}

		D3DViewPort viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(nGridSize);
		viewport.Height = static_cast<float>(nGridSize);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		// update shared constant buffer
		m_constants->params = m_shaderParam;
		m_constants.CopyToDevice();

		// Snapping occurred - update simulation to new offset
		if (m_bSnapToCenter)
		{
			auto& pass = m_passSnapToCenter;

			if (pass.IsDirty())
			{
				uint64 rtMask = g_HWSR_MaskBit[HWSR_SAMPLE0];
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
				pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass.SetTechnique(CShaderMan::s_shPostEffects, m_ripplesGenTechName, rtMask);

				pass.SetTexture(0, m_pTexWaterRipplesDDN);
				pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

				pass.SetRenderTarget(0, pTempTex);

				pass.SetState(GS_NODEPTHTEST);
			}
			
			pass.Execute();
		}
		else
		{
			m_passCopy.Execute(m_pTexWaterRipplesDDN, pTempTex);
		}

		// Compute wave propagation
		{
			auto& pass = m_passWaterWavePropagation;

			if (pass.IsDirty())
			{
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
				pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass.SetTechnique(CShaderMan::s_shPostEffects, m_ripplesGenTechName, 0);

				pass.SetTexture(0, pTempTex);
				pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

				pass.SetRenderTarget(0, m_pTexWaterRipplesDDN);

				pass.SetState(GS_NODEPTHTEST);
			}

			pass.Execute();
		}

		ExecuteWaterRipples(m_pTexWaterRipplesDDN, viewport);

		m_passMipmapGen.Execute(m_pTexWaterRipplesDDN);
	}

	m_updateMask &= ~(1 << gRenDev->RT_GetCurrGpuID());
}

CTexture* CWaterRipplesStage::GetWaterRippleTex() const
{
	return (CTexture::IsTextureExist(m_pTexWaterRipplesDDN) ? m_pTexWaterRipplesDDN.get() : CRendererResources::s_ptexBlack);
}

bool CWaterRipplesStage::IsVisible() const
{
	const CRenderView* pRenderView = RenderView();
	const float fTimeOut = 10.0f; // 10 seconds
	const bool bSimTimeOut = (gEnv->pTimer->GetCurrTime() - m_lastSpawnTime) > fTimeOut;

	auto& waterRipples = pRenderView->GetWaterRipples();
	const bool bEmpty = waterRipples.empty()
	                    && m_waterRipples.empty();

	bool bRunnable = (m_updateMask
	                  || ((CRenderer::CV_r_PostProcessGameFx != 0)
	                      && (!bEmpty || !bSimTimeOut)));

	return bRunnable;
}

void CWaterRipplesStage::ExecuteWaterRipples(CTexture* pTargetTex, const D3DViewPort& viewport)
{
	auto& waterRipples = m_waterRipplesMGPU;
	if (waterRipples.empty())
	{
		return;
	}

	const float z = 1.0f;

	const float fWidth = viewport.Width;
	const float fHeight = viewport.Height;
	const float fWidthRcp = 1.0f / fWidth;
	const float fHeightRcp = 1.0f / fHeight;
	const float fRatio = fHeightRcp / fWidthRcp;

	const float fWidthHit = 2.0f * fWidthRcp * fRatio;
	const float fHeightHit = 2.0f * fHeightRcp;

	{
		auto& pass = m_passAddWaterRipples;
		pass.SetRenderTarget(0, pTargetTex);
		pass.SetViewport(viewport);
		pass.BeginAddingPrimitives();

		Vec4 param = m_shaderParam;
		int32 vertexOffset = 0;

		CRY_ASSERT(waterRipples.size() <= SWaterRippleInfo::MaxWaterRipplesInScene);
		CryStackAllocWithSizeVector(SVertex, sTotalVertexCount, ripplesVertices, CDeviceBufferManager::AlignBufferSizeForStreaming);

		for (int32 i = 0; i < waterRipples.size(); ++i)
		{
			const auto& ripple = waterRipples[i];

			// Map hit world space to simulation space
			float xmapped = ripple.position.x * m_lookupParam.x + m_lookupParam.z;
			float ymapped = ripple.position.y * m_lookupParam.x + m_lookupParam.w;

			// Render a sprite at hit location
			float x0 = (xmapped - 0.5f * (fWidthHit * ripple.scale));
			float y0 = (ymapped - 0.5f * (fHeightHit * ripple.scale));

			float x1 = (xmapped + 0.5f * (fWidthHit * ripple.scale));
			float y1 = (ymapped + 0.5f * (fHeightHit * ripple.scale));

			// sprite vertices for a water ripple
			// *INDENT-OFF*
			ripplesVertices[vertexOffset++] = { Vec3(x0, y0, ripple.strength), { { 0 } }, Vec2(0,  0) };
			ripplesVertices[vertexOffset++] = { Vec3(x0, y1, ripple.strength), { { 0 } }, Vec2(0,  1) };
			ripplesVertices[vertexOffset++] = { Vec3(x1, y0, ripple.strength), { { 0 } }, Vec2(1,  0) };
			ripplesVertices[vertexOffset++] = { Vec3(x1, y1, ripple.strength), { { 0 } }, Vec2(1,  1) };
			// *INDENT-ON*

			auto& primitive = m_ripplePrimitive[i];

			primitive.SetTechnique(CShaderMan::s_shPostEffects, m_ripplesHitTechName, 0);
			primitive.Compile(pass);

			pass.AddPrimitive(&primitive);
		}

		// update all sprite vertices
		CRY_ASSERT(vertexOffset > 0);
		gRenDev->m_DevBufMan.UpdateBuffer(m_vertexBuffer, ripplesVertices, vertexOffset * sVertexStride);

		pass.Execute();
	}
}

void CWaterRipplesStage::UpdateAndDrawDebugInfo()
{
#if !defined(_RELEASE)
	const CRenderView* pRenderView = RenderView();
	const float maxLifetime = 2.0f;
	const float frameTime = std::max(0.0f, gEnv->pTimer->GetFrameTime());

	// process lifetime.
	for (auto& debugInfo : m_debugRippleInfos)
	{
		if (debugInfo.lifetime > 0.0f)
		{
			debugInfo.lifetime -= frameTime;
		}
	}

	const auto& waterRipples = pRenderView->GetWaterRipples();
	const auto newRippleDebugCount = waterRipples.size();
	const auto remainingRippleDebugCount = std::count_if(m_debugRippleInfos.begin(), m_debugRippleInfos.end(),
	                                                     [](const SWaterRippleRecord& record)
	{
		return (record.lifetime > 0.0f);
	});
	const auto deadRippleDebugCount = m_debugRippleInfos.size() - remainingRippleDebugCount;
	const auto maxRippleDebugCount = 2 * SWaterRippleInfo::MaxWaterRipplesInScene; // assign enough size to hold debug infos.

	// remove old ripple debug infos to make empty spaces for newly added ripples.
	auto rippleDebugCount = newRippleDebugCount + remainingRippleDebugCount;
	if (rippleDebugCount > maxRippleDebugCount)
	{
		std::sort(m_debugRippleInfos.begin(), m_debugRippleInfos.end(),
		          [](const SWaterRippleRecord& left, const SWaterRippleRecord& right)
		{
			return (left.lifetime < right.lifetime);
		});

		const auto removedCount = rippleDebugCount - maxRippleDebugCount;
		auto removeBegin = std::next(m_debugRippleInfos.begin(), deadRippleDebugCount); // old infos exist after dead infos.
		for_each(removeBegin, std::next(removeBegin, removedCount),
		         [](SWaterRippleRecord& record)
		{
			record.lifetime = 0.0f;
		});
	}

	// add new ripple debug infos.
	for (auto& info : waterRipples)
	{
		auto element = std::find_if(m_debugRippleInfos.begin(), m_debugRippleInfos.end(),
		                            [](const SWaterRippleRecord& record)
		{
			return (record.lifetime <= 0.0f);
		});

		if (element != m_debugRippleInfos.end())
		{
			element->info = info;
			element->lifetime = maxLifetime;
		}
		else
		{
			m_debugRippleInfos.emplace_back(info, maxLifetime);
		}
	}

	CRY_ASSERT(m_debugRippleInfos.size() <= maxRippleDebugCount);

	if (m_pCVarWaterRipplesDebug && m_pCVarWaterRipplesDebug->GetIVal() > 0)
	{
		auto* pAuxRenderer = gRenDev->GetIRenderAuxGeom();

		const SAuxGeomRenderFlags oldRenderFlags = pAuxRenderer->GetRenderFlags();

		pAuxRenderer->SetRenderFlags(SAuxGeomRenderFlags());

		for (auto& debugInfo : m_debugRippleInfos)
		{
			if (debugInfo.lifetime > 0.0f)
			{
				Vec3 vHitPos(debugInfo.info.position.x, debugInfo.info.position.y, debugInfo.info.position.z);
				pAuxRenderer->DrawSphere(vHitPos, 0.15f, ColorB(255, 0, 0, 255));
			}
		}

		Vec3 simCenter = Vec3(m_simOrigin.x, m_simOrigin.y, GetCurrentViewInfo().cameraOrigin.z - 1.5f);
		pAuxRenderer->DrawSphere(simCenter, 0.15f, ColorB(0, 255, 0, 255));

		pAuxRenderer->SetRenderFlags(oldRenderFlags);
	}
#endif
}
