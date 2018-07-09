// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This is a direct port from legacy FX_ZTargetReadback.
// There are still issues here, and refactoring may definitely be needed at some point.
// The following optimizations should be considered:
// - During CPU read-back we are making a copy of the texture data into a temporary buffer, and we remap the data ranges.
//   Instead, we should do this in the shader so the texture readback is already in the correct range.
// - We are doing depth-down-sampling (with either min or max), but there is already depth-down-sampling happening just before this stage (on standard-graphics-pipeline).
//   We should consider taking the appropriate, already-downsampled target and read back it's min or max channel.
//   Note: There are precision and resolution differences that need to be accounted for.
// - There are 4 read-back textures, each with a staging resource for download.
//   Instead, we should have one read-back texture with 4 staging resource for download.

#include "StdAfx.h"
#include "DepthReadback.h"
#include "Common/ReverseDepth.h"
#include "Common/PostProcess/PostProcessUtils.h"
#include "DriverD3D.h"

void CDepthReadbackStage::Init()
{
	if (!IsReadbackRequired())
	{
		return;
	}

	EConfigurationFlags flags;
	CTexture* const pSource = GetInputTexture(flags);

	if (!CreateResources(pSource->GetWidth(), pSource->GetHeight()))
	{
		CryFatalError("Cannot create depth readback resources");
	}

	ConfigurePasses(pSource, flags);
}

void CDepthReadbackStage::Execute()
{
	if (!IsReadbackRequired())
	{
		ReleaseResources();
		return;
	}

	EConfigurationFlags flags;
	CTexture* pSource = GetInputTexture(flags);
	const uint32 sourceWidth = pSource->GetWidth();
	const uint32 sourceHeight = pSource->GetHeight();
	const int32 sourceTexId = pSource->GetID();

	const bool bNeedRecreateResources = m_resourceWidth != sourceWidth || m_resourceHeight != sourceHeight;
	if (bNeedRecreateResources)
	{
		if (!CreateResources(sourceWidth, sourceHeight))
		{
			CRY_ASSERT(false && "Unable to create resources, this should only be possible in response to CVar changes (ie, developer mode)");
			return;
		}
	}

	const bool bNeedReconfigurePasses = bNeedRecreateResources || m_resourceFlags != flags || m_sourceTexId != sourceTexId;
	if (bNeedReconfigurePasses)
	{
		ConfigurePasses(pSource, flags);
	}

	ReadbackLatestData();

	// Support for sampling a region (in the top-left) of the source texture.
	const Vec2& downscaleFactor = gRenDev->GetRenderQuality().downscaleFactor;
	const float sampledWidth = std::floorf((float)sourceWidth * downscaleFactor.x);
	const float sampledHeight = std::floorf((float)sourceHeight * downscaleFactor.y);

	ExecutePasses((float)sourceWidth, (float)sourceHeight, sampledWidth, sampledHeight);
}

bool CDepthReadbackStage::IsReadbackRequired()
{
	// This function is not ideal, but for now we cannot get to e_* CVars on Initialize().
	// So instead we keep retrying every frame instead until 3DEngine is alive.
	if (!m_pCheckOcclusion)
	{
		m_pCheckOcclusion = gEnv->pConsole->GetCVar("e_CheckOcclusion");
	}
	if (!m_pStatObjRenderTasks)
	{
		m_pStatObjRenderTasks = gEnv->pConsole->GetCVar("e_StatObjBufferRenderTasks");
	}
	if (!m_pCoverageBufferReproj)
	{
		m_pCoverageBufferReproj = gEnv->pConsole->GetCVar("e_CoverageBufferReproj");
	}
	if (!m_pCheckOcclusion || !m_pStatObjRenderTasks || !m_pCoverageBufferReproj)
	{
		return false;
	}
	return m_pCheckOcclusion->GetIVal() != 0 && m_pStatObjRenderTasks->GetIVal() != 0 && m_pCoverageBufferReproj->GetIVal() != 4;
}

CTexture* CDepthReadbackStage::GetInputTexture(EConfigurationFlags& flags)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	const auto pRenderView = RenderView();
	const bool bMultiResEnabled = CVrProjectionManager::IsMultiResEnabledStatic();
	const bool bUseNativeDepth = pRenderView && CRenderer::CV_r_CBufferUseNativeDepth && !bMultiResEnabled && !gEnv->IsEditor();
	const bool bReverseDepth = true;
	const bool bMSAA = bUseNativeDepth && gRenDev->IsMSAAEnabled();

	int allFlags = bMultiResEnabled ? kMultiRes : kNone;
	allFlags |= bReverseDepth ? kReverseDepth : kNone;
	allFlags |= bUseNativeDepth ? kNativeDepth : kNone;
	allFlags |= bMSAA ? kMSAA : kNone;
	flags = static_cast<EConfigurationFlags>(allFlags);

	return
	  bUseNativeDepth ? pRenderView->GetDepthTarget()
	  : bMultiResEnabled ? CVrProjectionManager::Instance()->GetZTargetFlattened()
	  : CRendererResources::s_ptexLinearDepth;
}

bool CDepthReadbackStage::CreateResources(uint32 sourceWidth, uint32 sourceHeight)
{
	CRY_ASSERT(sourceWidth != 0 && sourceHeight != 0 && "Bad input size requested");
	bool bFailed = false;

	// Find number of downsample passes needed for source resolution
	const uint32 downSampleX = sourceWidth > CULL_SIZEX ? IntegerLog2((sourceWidth - 1) / CULL_SIZEX) : 0;
	const uint32 downSampleY = sourceHeight > CULL_SIZEY ? IntegerLog2((sourceHeight - 1) / CULL_SIZEY) : 0;
	const uint32 maxDownsamplePasses = kMaxDownsamplePasses;
	m_downsamplePassCount = std::min(maxDownsamplePasses, std::max(downSampleX, downSampleY));

	// Create downsample textures
	// Note: The texture instances are always at the "tail" of the CTexture array, to increase the chance of resize not being needed on a particular instance.
	for (uint32 i = 0; i < m_downsamplePassCount && !bFailed; ++i)
	{
		const uint32 downsampleIndex = i + (kMaxDownsamplePasses - m_downsamplePassCount);
		CTexture* const pTarget = CRendererResources::s_ptexLinearDepthDownSample[downsampleIndex];
		CRY_ASSERT(pTarget && "Z Downsample target should already exist");

		// Ensure the intermediate textures are never larger than the source image
		const uint32 width = CULL_SIZEX << std::min(m_downsamplePassCount - i, downSampleX);
		const uint32 height = CULL_SIZEY << std::min(m_downsamplePassCount - i, downSampleY);

		if (!pTarget->GetDevTexture() || pTarget->GetWidth() != width || pTarget->GetHeight() != height)
		{
			const uint32 downsampleFlags = FT_DONT_STREAM | FT_DONT_RELEASE;

			pTarget->SetFlags(downsampleFlags);
			pTarget->SetWidth(width);
			pTarget->SetHeight(height);
			bFailed |= !pTarget->CreateRenderTarget(CRendererResources::s_eTFZ, Clr_Unused);
		}
	}

	// Release down-sample textures not needed for the current source size.
	for (uint32 i = m_downsamplePassCount; i < kMaxDownsamplePasses; ++i)
	{
		const uint32 downsampleIndex = i - m_downsamplePassCount;
		CTexture* const pTarget = CRendererResources::s_ptexLinearDepthDownSample[downsampleIndex];
		CRY_ASSERT(pTarget && "Z Downsample target should already exist");

		if (pTarget->GetDevTexture())
		{
			pTarget->SetWidth(0);
			pTarget->SetHeight(0);
			pTarget->ReleaseDeviceTexture(false);
		}
	}

	for (uint32 i = 0; i < kMaxReadbackPasses && !bFailed; ++i)
	{
		const uint32 readbackIndex = i;
		CTexture* const pTarget = CRendererResources::s_ptexLinearDepthReadBack[readbackIndex];
		CRY_ASSERT(pTarget && "Z Readback target should already exist");

		if (pTarget->GetDevTexture())
		{
			// Let all previous requested readbacks finish
			if (m_readback[readbackIndex].bIssued && !m_readback[readbackIndex].bCompleted)
				m_readback[readbackIndex].bCompleted = pTarget->GetDevTexture()->AccessCurrStagingResource(0, false);
		}

		if (!pTarget->GetDevTexture() || pTarget->GetWidth() != CULL_SIZEX || pTarget->GetHeight() != CULL_SIZEY)
		{
			const uint32 readbackFlags = FT_DONT_STREAM | FT_DONT_RELEASE | FT_STAGE_READBACK;

			pTarget->SetFlags(readbackFlags);
			pTarget->SetWidth(CULL_SIZEX);
			pTarget->SetHeight(CULL_SIZEY);
			bFailed |= !pTarget->CreateRenderTarget(eTF_R32F, Clr_Unused);
		}
	}

	for (auto& readback : m_readback)
	{
		if (!bFailed)
		{
			readback.bIssued    = false;
			readback.bCompleted = false;
			readback.bReceived  = false;
		}
	}

	m_resourceWidth = sourceWidth;
	m_resourceHeight = sourceHeight;

	if (bFailed)
	{
		ReleaseResources();
	}
	return !bFailed;
}

void CDepthReadbackStage::ReleaseResources()
{
	if (m_resourceWidth != 0 || m_resourceHeight != 0)
	{
		for (auto* pTexReadback : CRendererResources::s_ptexLinearDepthReadBack)
		{
			pTexReadback->ReleaseDeviceTexture(false);
		}

		for (auto* pTexDownsample : CRendererResources::s_ptexLinearDepthDownSample)
		{
			pTexDownsample->ReleaseDeviceTexture(false);
		}

		for (auto& readback : m_readback)
		{
			readback.bIssued    = false;
			readback.bCompleted = false;
			readback.bReceived  = false;
		}

		m_resourceWidth = 0;
		m_resourceHeight = 0;
	}
}

void CDepthReadbackStage::ConfigurePasses(CTexture* pSource, EConfigurationFlags flags)
{
	static const CCryNameTSCRC techniqueNameSimple("TextureToTextureResampled");
	static const CCryNameTSCRC techniqueNameRegion("TextureToTextureResampledReg");

	const auto configurePass = [](CFullscreenPass& pass, CTexture* pSource, CTexture* pTarget, bool bInitial, uint64 rtFlags)
	{
		pass.SetTextureSamplerPair(0, pSource, EDefaultSamplerStates::PointClamp);
		pass.SetRenderTarget(0, pTarget);
		pass.SetState(GS_NODEPTHTEST);

		if (bInitial)
		{
			// Region shader also has VS constants, and expects full-screen quad
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuadCentered);
			pass.SetTechnique(CShaderMan::s_shPostEffects, techniqueNameRegion, rtFlags);
		}
		else
		{
			// Simple shader only has PS constants, and expects full-screen triangle
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_shPostEffects, techniqueNameSimple, rtFlags);
		}
	};

	const SPostEffectsUtils::EDepthDownsample downsampleMode =
	  ((flags & kNativeDepth) && (flags & kReverseDepth))
	  ? SPostEffectsUtils::eDepthDownsample_Min
	  : SPostEffectsUtils::eDepthDownsample_Max;

	bool bInitial = true;
	uint64 rtFlags =
	  g_HWSR_MaskBit[HWSR_SAMPLE4] |
	  (flags & kMSAA ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0) |
	  (downsampleMode == SPostEffectsUtils::eDepthDownsample_Min ? g_HWSR_MaskBit[HWSR_REVERSE_DEPTH] : 0);

	for (uint32 i = kMaxDownsamplePasses - m_downsamplePassCount; i < kMaxDownsamplePasses; ++i)
	{
		CFullscreenPass& pass = m_downsamplePass[i];
		CTexture* const pTarget = CRendererResources::s_ptexLinearDepthDownSample[i];

		configurePass(pass, pSource, pTarget, bInitial, rtFlags);

		pSource = pTarget;
		if (bInitial)
		{
			rtFlags &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];
			bInitial = false;
		}
	}

	for (uint32 i = 0; i < kMaxReadbackPasses; ++i)
	{
		CFullscreenPass& pass = m_readback[i].pass;
		CTexture* const pTarget = CRendererResources::s_ptexLinearDepthReadBack[i];

		configurePass(pass, pSource, pTarget, bInitial, rtFlags);
	}

	m_sourceTexId = pSource->GetID();
	m_resourceFlags = flags;
}

void CDepthReadbackStage::ExecutePasses(float sourceWidth, float sourceHeight, float sampledWidth, float sampledHeight)
{
	static const CCryNameR psParam0Name("texToTexParams0");
	static const CCryNameR psParam1Name("texToTexParams1");
	static const CCryNameR vsParamName("texToTexParamsTC");

	PROFILE_LABEL_SCOPE("DEPTH READBACK");

	// Helper to execute a pass, the pass must already be configured
	const auto executePass = [sampledWidth, sampledHeight](CFullscreenPass& pass, bool bInitial, float sourceWidth, float sourceHeight)
	{
		pass.BeginConstantUpdate();
		if (bInitial)
		{
			const float right = sampledWidth / sourceWidth;
			const float bottom = sampledHeight / sourceHeight;
			const Vec4 vsParams(0.0f, 0.0f, right, bottom);
			pass.SetConstant(vsParamName, vsParams, eHWSC_Vertex);
		}

		const float s1 = 0.5f / sourceWidth;
		const float t1 = 0.5f / sourceHeight;
		const Vec4 psParams0 = Vec4(-s1, -t1, s1, -t1);
		const Vec4 psParams1 = Vec4(s1, t1, -s1, t1);
		pass.SetConstant(psParam0Name, psParams0, eHWSC_Pixel);
		pass.SetConstant(psParam1Name, psParams1, eHWSC_Pixel);

		if (pass.IsDirty())
			return false;

		pass.Execute();

		return true;
	};

	// Issue downsample
	bool bInitial = true;
	for (uint32 i = kMaxDownsamplePasses - m_downsamplePassCount; i < kMaxDownsamplePasses; ++i)
	{
		CFullscreenPass& pass = m_downsamplePass[i];
		CTexture* const pTarget = CRendererResources::s_ptexLinearDepthDownSample[i];

		// If a pass is invalid, stop the downsample.
		// This avoids reading-back invalid depth data.
		if (!executePass(pass, bInitial, sourceWidth, sourceHeight))
			return;

		bInitial = false;
		sourceWidth = (float)pTarget->GetWidth();
		sourceHeight = (float)pTarget->GetHeight();
	}

	// Issue readback
	const uint32 readbackIndex = m_readbackIndex % kMaxReadbackPasses;
	++m_readbackIndex;
	CTexture* const pTarget = CRendererResources::s_ptexLinearDepthReadBack[readbackIndex];
	SReadback& readback = m_readback[readbackIndex];
	CFullscreenPass& pass = readback.pass;
	executePass(pass, bInitial, sourceWidth, sourceHeight);

	readback.bIssued = true; pTarget->GetDevTexture()->DownloadToStagingResource(0);
	readback.bCompleted =    pTarget->GetDevTexture()->AccessCurrStagingResource(0, false);
	readback.bReceived = false;
	
	const auto& viewInfo = GetCurrentViewInfo();
	// Associate the information of this frame with the readback.
	Matrix44 modelView = viewInfo.viewMatrix;
	Matrix44 proj = viewInfo.projMatrix;

	readback.flags = m_resourceFlags;
	if (readback.flags & kReverseDepth)
	{
		proj = ReverseDepthHelper::Convert(proj);
	}
	readback.camera = modelView * proj;
	readback.zNear = viewInfo.nearClipPlane;
	readback.zFar = viewInfo.farClipPlane;
}

void CDepthReadbackStage::ReadbackLatestData()
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CDepthReadbackStage::ReadbackLatestData");

	// Determine the last completed readback.
	const uint32 oldestReadback = m_readbackIndex % kMaxReadbackPasses;
	uint32 receivableIndex = kMaxReadbackPasses;
	for (uint32 i = 0; i < kMaxReadbackPasses; ++i)
	{
		const uint32 readbackIndex = (oldestReadback - i - 1) % kMaxReadbackPasses;
		const bool bOldest = oldestReadback == readbackIndex;
		CTexture* const pTarget = CRendererResources::s_ptexLinearDepthReadBack[readbackIndex];
		SReadback& readback = m_readback[readbackIndex];

		if (readback.bIssued && !readback.bCompleted)
		{
			readback.bCompleted = pTarget->GetDevTexture()->AccessCurrStagingResource(0, false);
		}
		if ((readback.bCompleted || (readback.bIssued && bOldest)) && !readback.bReceived)
		{
			receivableIndex = readbackIndex; break;
		}
		if (readback.bReceived)
		{
			return; // We now have the latest available data read back
		}
	}

	TAutoLock lock(m_lock);
	if (receivableIndex == kMaxReadbackPasses)
	{
		// No readback has finished yet, and there are at least some slots not queued into.
		// This can happen for the first N frames after CreateResources(), where N < kMaxReadbackPasses.
		// In this case, there is nothing we can do.
		m_lastResult = kMaxResults;
		return;
	}
	
	// Currently, we only have 2 result slots, this requires us to hold the lock for the duration of the readback.
	// - Lock to reserve an index that is not "pinned" (m_lastPinned)
	// - Perform readback into "reserved" while still holding the lock (to prevent partially finished data from being accessed)
	// - Set "available" (m_lastResult) to "reserved"
	// It is also possible to have 3 result slots, and only hold the lock during index updating (ie, take it for a short time twice).
	// - Lock once to reserve the index that is neither "pinned" (m_lastPinned) nor "available" (m_lastResult).
	// - Perform readback into "reserved" without holding the lock.
	// - Lock once more to set "available" (m_lastResult) to "reserved"
	// This makes it much less likely that a client thread contends on the lock.
	// However, the expectation is that it's OK to pay for the contention, if as a "reward" you get one frame "fresher" data.
	// Should we get more clients on critical paths for the occlusion buffer, this decision should be revisited.
	const uint32 reservedIndex = m_lastPinned < kMaxResults ? 1 - m_lastPinned : m_lastResult < kMaxResults ? m_lastResult : 0;
	CRY_ASSERT(reservedIndex < kMaxResults && reservedIndex != m_lastPinned && (kMaxResults == 3 ? reservedIndex != m_lastResult : true));

	SResult& result = m_result[reservedIndex];
	SReadback& readback = m_readback[receivableIndex];
	CDeviceTexture* const pReadback = CRendererResources::s_ptexLinearDepthReadBack[receivableIndex]->GetDevTexture();
	CRY_ASSERT(readback.bIssued == true);

	const auto readbackData = [&readback, &result](void* pData, uint32 rowPitch, uint32 slicePitch) -> bool
	{
		CRY_ASSERT(rowPitch == CULL_SIZEX * sizeof(float) && slicePitch >= CULL_SIZEX * CULL_SIZEY * sizeof(float));

		const float* const pDepths = static_cast<const float*>(pData);
		if (readback.flags & kNativeDepth)
		{
			for (uint32 i = 0; i < CULL_SIZEX * CULL_SIZEY; ++i)
			{
				const float depthVal = (readback.flags & kReverseDepth) ? 1.0f - pDepths[i] : pDepths[i];
				result.data[i] = std::max(depthVal, FLT_EPSILON);
			}
		}
		else
		{
			const float projRatioX = readback.zFar / (readback.zFar - readback.zNear);
			const float projRatioY = readback.zNear / (readback.zNear - readback.zFar);
			for (uint32 i = 0; i < CULL_SIZEX * CULL_SIZEY; ++i)
			{
				result.data[i] = std::max(projRatioY / std::max(pDepths[i], FLT_EPSILON) + projRatioX, FLT_EPSILON);
			}
		}

		result.camera = readback.camera;
		readback.bCompleted = true;
		readback.bReceived = true;
		return true;
	};

	pReadback->AccessCurrStagingResource(0, false, readbackData);

	m_lastResult = reservedIndex;
}
