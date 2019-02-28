// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ClipVolumes.h"
#include "DriverD3D.h"
#include "VolumetricFog.h"

struct SPrimitiveConstants
{
	Matrix44 transformMatrix;
	Vec4     projRatioScreenScale;
	Vec4     blendPlane0;
	Vec4     blendPlane1;
};

struct SPrimitiveVolFogConstants
{
	Matrix44 transformMatrix;
	uint32   sliceIndex[4];
};

CClipVolumesStage::CClipVolumesStage()
	: m_nShaderParamCount(0)
	, m_pBlendValuesRT(nullptr)
	, m_pDepthTarget(nullptr)
	, m_pClipVolumeStencilVolumeTex(nullptr)
	, m_cleared(MAX_GPU_NUM)
	, m_nearDepth(-1.0f)
	, m_raymarchDistance(0.0f)
	, m_bClipVolumesValid(false)
	, m_bBlendPassReady(false)
	, m_bOutdoorVisible(false)
{
	ZeroArray(m_clipVolumeShaderParams);
#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
	memset(&m_depthTargetVolFog, 0, sizeof(m_depthTargetVolFog));
#endif
}

CClipVolumesStage::~CClipVolumesStage()
{
	Destroy();
}

void CClipVolumesStage::Init()
{
	m_stencilPass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
	m_blendValuesPass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
	m_stencilResolvePass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
	m_volumetricStencilPass.SetFlags(CPrimitiveRenderPass::ePassFlags_None);
#endif

	for (int i = 0; i < MaxDeferredClipVolumes; ++i)
	{
		// Stencil pass: two primitives per clip volume, both share the same constant buffer
		{
			CConstantBufferPtr pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SPrimitiveConstants));

			m_stencilPrimitives[2 * i + 0].SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Vertex);
			m_stencilPrimitives[2 * i + 1].SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Vertex);

#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
			pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SPrimitiveVolFogConstants));
			m_stencilPrimitivesVolFog[2 * i + 0].SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Geometry | EShaderStage_Vertex);
			m_stencilPrimitivesVolFog[2 * i + 1].SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Geometry | EShaderStage_Vertex);
#endif
		}

		// Blend values pass: one primitive per clip volume, shared CB for vertex and pixel shader
		{
			CConstantBufferPtr pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SPrimitiveConstants));

			m_blendPrimitives[i].SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Pixel | EShaderStage_Vertex);
		}

	}

	if (!m_clipVolumeInfoBuf.IsAvailable())
	{
		m_clipVolumeInfoBuf.Create(MaxDeferredClipVolumes, sizeof(SClipVolumeInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	}

	// TODO: move this texture to intra-view-shared data structure after removing old graphics pipeline.
#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
	const uint32 flags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL;
#else
	const uint32 flags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
#endif
	CRY_ASSERT(m_pClipVolumeStencilVolumeTex == nullptr);
	m_pClipVolumeStencilVolumeTex = CTexture::GetOrCreateTextureObject("$VolFogClipVolumeStencil", 0, 0, 0, eTT_2D, flags, eTF_Unknown);
}


void CClipVolumesStage::Destroy()
{
	m_clipVolumeInfoBuf.Release();

	SAFE_RELEASE(m_pClipVolumeStencilVolumeTex);
#ifndef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
	for (auto& pTex : m_pClipVolumeStencilVolumeTexArray)
	{
		SAFE_RELEASE(pTex);
	}
#endif
}


void CClipVolumesStage::Update()
{
	CRenderView* pRenderView = RenderView();

	m_pBlendValuesRT = CRendererResources::s_ptexVelocity;
	m_pDepthTarget = RenderView()->GetDepthTarget();

	m_stencilPass.SetDepthTarget(m_pDepthTarget);
	m_stencilPass.SetViewport(pRenderView->GetViewport());

	m_blendValuesPass.SetRenderTarget(0, m_pBlendValuesRT);
	m_blendValuesPass.SetDepthTarget(m_pDepthTarget);
	m_blendValuesPass.SetViewport(pRenderView->GetViewport());
}


void CClipVolumesStage::GenerateClipVolumeInfo()
{
	CRenderView* pRenderView = RenderView();
	const auto& clipVolumes = pRenderView->GetClipVolumes();

	m_nShaderParamCount = 0;
	
	// Outdoor
	{
		uint32 nFlags = IClipVolume::eClipVolumeConnectedToOutdoor | IClipVolume::eClipVolumeAffectedBySun;
		m_clipVolumeShaderParams[m_nShaderParamCount++] = Vec4(gEnv->p3DEngine->GetSkyColor() * gcpRendD3D->m_fAdaptedSceneScaleLBuffer, alias_cast<float>(nFlags));
	}

	for (const auto& volume : clipVolumes)
	{
		// Update shader params
		const uint32 paramIndex = volume.nStencilRef;
		CRY_ASSERT(paramIndex > STENCIL_VALUE_OUTDOORS && paramIndex < MaxDeferredClipVolumes);

		if (paramIndex < MaxDeferredClipVolumes)
		{
			uint32 nData =
				volume.blendInfo[1].blendID << 24 |
				volume.blendInfo[0].blendID << 16 |
				volume.nFlags;

			m_clipVolumeShaderParams[paramIndex] = Vec4(0, 0, 0, alias_cast<float>(nData));
			m_bOutdoorVisible |= volume.nFlags & IClipVolume::eClipVolumeConnectedToOutdoor ? true : false;

			m_nShaderParamCount = max(m_nShaderParamCount, paramIndex + 1);
		}
	}

	{
		// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
		CryStackAllocWithSizeVector(SClipVolumeInfo, MaxDeferredClipVolumes, clipVolumeInfo, CDeviceBufferManager::AlignBufferSizeForStreaming);

		for (uint32 volumeIndex = 0; volumeIndex < MaxDeferredClipVolumes /* numVolumes */; ++volumeIndex)
			clipVolumeInfo[volumeIndex].data = m_clipVolumeShaderParams[volumeIndex].w;

		m_clipVolumeInfoBuf.UpdateBufferContent(clipVolumeInfo, clipVolumeInfoSize);
	}
}

void CClipVolumesStage::Prepare()
{
	m_bClipVolumesValid = false;
	m_bOutdoorVisible = false;

	m_stencilPass.BeginAddingPrimitives();
	m_blendValuesPass.BeginAddingPrimitives();

	PrepareVolumetricFog();

	if (gcpRendD3D->m_bVolumetricFogEnabled)
	{
#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
		m_volumetricStencilPass.BeginAddingPrimitives();
#else
		for (auto& pass : m_volumetricStencilPassArray)
			pass->BeginAddingPrimitives();
#endif
	}

	const uint32 StencilStateInitialMarkup =
	  STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
	  STENCOP_FAIL(FSS_STENCOP_KEEP) |
	  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
	  STENCOP_PASS(FSS_STENCOP_KEEP);

	const uint32 StencilStateInsideOutsideMarkup =
	  STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
	  STENCOP_FAIL(FSS_STENCOP_KEEP) |
	  STENCOP_ZFAIL(FSS_STENCOP_INVERT) |
	  STENCOP_PASS(FSS_STENCOP_KEEP);

	const uint32 StencilStateTest =
	  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
	  STENCOP_FAIL(FSS_STENCOP_KEEP) |
	  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
	  STENCOP_PASS(FSS_STENCOP_KEEP);

	const uint8 StencilReadWriteMask = 0xFF & ~BIT_STENCIL_RESERVED;

	static CCryNameTSCRC techStencil = "ClipVolumeStencil";
	static CCryNameTSCRC techPortalBlend = "ClipVolumeBlendValue";

	static ICVar* pPortalsBlendCVar = iConsole->GetCVar("e_PortalsBlend");
	const bool bRenderPortalBlendValues = pPortalsBlendCVar->GetIVal() > 0;
	const bool bRenderVisAreas = CRenderer::CV_r_VisAreaClipLightsPerPixel > 0;

	CRenderView* pRenderView = RenderView();
	const auto& clipVolumes = pRenderView->GetClipVolumes();
	SRenderViewInfo viewInfo[2];
	size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);

	const bool bReverseDepth = (viewInfo[0].flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;

	Vec2 projRatio;
	float zn = viewInfo[0].nearClipPlane;
	float zf = viewInfo[0].farClipPlane;
	projRatio.x = bReverseDepth ? zn / (zn - zf) : zf / (zf - zn);
	projRatio.y = bReverseDepth ? zn / (zf - zn) : zn / (zn - zf);

#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
	static CCryNameTSCRC techStencilVolFog = "ClipVolumeStencilVolFog";
	const Vec3 cameraFrontVec = viewInfo[0].cameraVZ;
	SRenderViewShaderConstants& PF = pRenderView->GetShaderConstants();
	const float raymarchStart = PF.pVolumetricFogSamplingParams.x;
	const float invRaymarchDistance = PF.pVolumetricFogSamplingParams.y;
	const int32 volFogDepthNum = CVolumetricFogStage::GetVolumeTextureDepthSize();
	const float fVolFogDepthNum = static_cast<float>(volFogDepthNum);
	const int32 maxDepthSliceNum = 64; // this must exactly match to ClipVolumeVolFogGS shader's limitation.
#endif

	// Create stencil and blend pass primitives
	for (int i = clipVolumes.size() - 1; i >= 0; --i)
	{
		const SDeferredClipVolume& volume = clipVolumes[i];
		if (volume.m_pRenderMesh && volume.nStencilRef < MaxDeferredClipVolumes)
		{
			if ((volume.nFlags & IClipVolume::eClipVolumeIsVisArea) != 0 && !bRenderVisAreas)
				continue;

			CRY_ASSERT(((volume.nStencilRef) & (BIT_STENCIL_RESERVED | BIT_STENCIL_INSIDE_CLIPVOLUME)) == 0);
			const int stencilRef = ~(volume.nStencilRef) & ~(BIT_STENCIL_RESERVED | BIT_STENCIL_INSIDE_CLIPVOLUME);

			buffer_handle_t hVertexStream = ~0u;
			buffer_handle_t hIndexStream = ~0u;
			CRenderMesh* pRenderMesh = nullptr;

			if (pRenderMesh = reinterpret_cast<CRenderMesh*>(volume.m_pRenderMesh.get()))
			{
				hVertexStream = pRenderMesh->_GetVBStream(VSF_GENERAL);
				hIndexStream = pRenderMesh->_GetIBStream();

				if (hVertexStream != ~0u && hIndexStream != ~0u)
				{
					// Initial markup
					CRenderPrimitive& primInit = m_stencilPrimitives[2 * i + 0];
					primInit.SetTechnique(CShaderMan::s_shDeferredShading, techStencil, 0);
					primInit.SetRenderState((bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL) | GS_STENCIL);
					primInit.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
					primInit.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
					primInit.SetCullMode(eCULL_Front);
					primInit.SetStencilState(StencilStateInitialMarkup, stencilRef, StencilReadWriteMask, StencilReadWriteMask);
					primInit.SetDrawInfo(eptTriangleList, 0, 0, pRenderMesh->_GetNumInds());
					primInit.Compile(m_stencilPass);

					// Update constant buffer. NOTE: buffer is assigned to preallocated primitives
					auto& constantManager = primInit.GetConstantManager();

					auto constants = constantManager.BeginTypedConstantUpdate<SPrimitiveConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex);
					constants->transformMatrix = Matrix44(volume.mWorldTM.GetTransposed()) * viewInfo[0].cameraProjMatrix;

					if (viewInfoCount > 1)
					{
						constants.BeginStereoOverride();
						constants->transformMatrix = Matrix44(volume.mWorldTM.GetTransposed()) * viewInfo[1].cameraProjMatrix;
					}

					constantManager.EndTypedConstantUpdate(constants);

					// Inside/outside test
					CRenderPrimitive& primMarkup = m_stencilPrimitives[2 * i + 1];
					primMarkup.SetTechnique(CShaderMan::s_shDeferredShading, techStencil, 0);
					primMarkup.SetRenderState((bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL) | GS_STENCIL);
					primMarkup.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
					primMarkup.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
					primMarkup.SetCullMode(eCULL_None);
					primMarkup.SetStencilState(StencilStateInsideOutsideMarkup, ~stencilRef, StencilReadWriteMask, StencilReadWriteMask);
					primMarkup.SetDrawInfo(eptTriangleList, 0, 0, pRenderMesh->_GetNumInds());
					primMarkup.Compile(m_stencilPass);

					m_stencilPass.AddPrimitive(&primInit);
					m_stencilPass.AddPrimitive(&primMarkup);

					if (gcpRendD3D->m_bVolumetricFogEnabled)
					{
#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
						// calculate min depth index of volumetric fog coordinate.
						const float radius = volume.mAABB.GetRadius();
						const float distToCenter = -viewInfo[0].WorldToCameraZ(volume.mAABB.GetCenter());
						const float minW = max(distToCenter - radius, 0.0f);
						float fMinDepthIndex = fVolFogDepthNum * CVolumetricFogStage::GetDepthTexcoordFromLinearDepthScaled(minW, raymarchStart, invRaymarchDistance, fVolFogDepthNum);
						fMinDepthIndex = max(0.0f, floorf(fMinDepthIndex));
						int32 minDepthIndex = static_cast<int32>(fMinDepthIndex);
						minDepthIndex = max(min(minDepthIndex, volFogDepthNum - 1), 0);

						// prepare primitive for volumetric fog when it's in the range of volumetric fog.
						if (minDepthIndex < volFogDepthNum)
						{
							// Initial markup
							CRenderPrimitive& primInit = m_stencilPrimitivesVolFog[2 * i + 0];
							primInit.SetTechnique(CShaderMan::s_shDeferredShading, techStencilVolFog, 0);
							primInit.SetRenderState((bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL) | GS_STENCIL);
							primInit.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
							primInit.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
							primInit.SetCullMode(eCULL_Front);
							primInit.SetStencilState(StencilStateInitialMarkup, stencilRef, StencilReadWriteMask, StencilReadWriteMask);
							primInit.SetDrawInfo(eptTriangleList, 0, 0, pRenderMesh->_GetNumInds());
							primInit.Compile(m_volumetricStencilPass);

							// Update constant buffer. NOTE: buffer is assigned to preallocated primitives
							auto& constantManager = primInit.GetConstantManager();

							auto constants = constantManager.BeginTypedConstantUpdate<SPrimitiveVolFogConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Geometry | EShaderStage_Vertex);
							constants->transformMatrix = Matrix44(volume.mWorldTM.GetTransposed()) * viewInfo[0].cameraProjMatrix;

							// calculate max depth index of volumetric fog coordinate.
							const float maxW = max(distToCenter + radius, 0.0f);
							float fMaxDepthIndex = fVolFogDepthNum * CVolumetricFogStage::GetDepthTexcoordFromLinearDepthScaled(maxW, raymarchStart, invRaymarchDistance, fVolFogDepthNum);
							fMaxDepthIndex = max(0.0f, ceilf(fMaxDepthIndex));
							int32 maxDepthIndex = static_cast<int32>(fMaxDepthIndex);

							maxDepthIndex = max(maxDepthIndex, 0);
							maxDepthIndex = min(maxDepthIndex, minDepthIndex + maxDepthSliceNum);
							maxDepthIndex = min(maxDepthIndex, volFogDepthNum);

							constants->sliceIndex[0] = minDepthIndex;
							constants->sliceIndex[1] = maxDepthIndex;

							if (viewInfoCount > 1)
							{
								constants.BeginStereoOverride();
								constants->transformMatrix = Matrix44(volume.mWorldTM.GetTransposed()) * viewInfo[1].cameraProjMatrix;
								constants->sliceIndex[0] = minDepthIndex;
								constants->sliceIndex[1] = maxDepthIndex;
							}

							constantManager.EndTypedConstantUpdate(constants);

							// Inside/outside test
							CRenderPrimitive& primMarkup = m_stencilPrimitivesVolFog[2 * i + 1];
							primMarkup.SetTechnique(CShaderMan::s_shDeferredShading, techStencilVolFog, 0);
							primMarkup.SetRenderState((bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL) | GS_STENCIL);
							primMarkup.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
							primMarkup.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
							primMarkup.SetCullMode(eCULL_None);
							primMarkup.SetStencilState(StencilStateInsideOutsideMarkup, ~stencilRef, StencilReadWriteMask, StencilReadWriteMask);
							primMarkup.SetDrawInfo(eptTriangleList, 0, 0, pRenderMesh->_GetNumInds());
							primMarkup.Compile(m_volumetricStencilPass);

							m_volumetricStencilPass.AddPrimitive(&primInit);
							m_volumetricStencilPass.AddPrimitive(&primMarkup);
						}
#else
						for (auto& pass : m_volumetricStencilPassArray)
						{
							pass->AddPrimitive(&primInit);
							pass->AddPrimitive(&primMarkup);
						}
#endif
					}
				}
			}

			// Blend values
			if (bRenderPortalBlendValues && (volume.nFlags & IClipVolume::eClipVolumeBlend))
			{
				const int stencilTestRef = BIT_STENCIL_INSIDE_CLIPVOLUME | volume.nStencilRef;

				CRenderPrimitive& primBlend = m_blendPrimitives[i];
				primBlend.SetTechnique(CShaderMan::s_shDeferredShading, techPortalBlend, CVrProjectionManager::Instance()->GetRTFlags());
				primBlend.SetRenderState(GS_STENCIL | GS_NODEPTHTEST | GS_NOCOLMASK_RBA);
				primBlend.SetTexture(3, CRendererResources::s_ptexLinearDepth);
				primBlend.SetCullMode(eCULL_Front);
				primBlend.SetStencilState(StencilStateTest, stencilTestRef, StencilReadWriteMask, StencilReadWriteMask);
				primBlend.Compile(m_blendValuesPass);

				auto& constantManager = primBlend.GetConstantManager();
				auto constants = constantManager.BeginTypedConstantUpdate<SPrimitiveConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Pixel | EShaderStage_Vertex);

				constants->projRatioScreenScale = Vec4(projRatio.x, projRatio.y, 1.0f / m_pBlendValuesRT->GetWidth(), 1.0f / m_pBlendValuesRT->GetHeight());
				constants->blendPlane0 = viewInfo[0].invCameraProjMatrix * volume.blendInfo[0].blendPlane;
				constants->blendPlane1 = viewInfo[0].invCameraProjMatrix * volume.blendInfo[1].blendPlane;

				if (hVertexStream != ~0u && hIndexStream != ~0u)
				{
					constants->transformMatrix = Matrix44(volume.mWorldTM.GetTransposed()) * viewInfo[0].cameraProjMatrix;

					primBlend.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
					primBlend.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
					primBlend.SetDrawInfo(eptTriangleList, 0, 0, pRenderMesh->_GetNumInds());
				}
				else
				{
					constants->transformMatrix = Matrix44(IDENTITY);

					primBlend.SetPrimitiveType(CRenderPrimitive::ePrim_Triangle);
					primBlend.SetDrawInfo(eptTriangleList, 0, 0, 3);
				}

				if (viewInfoCount > 1)
				{
					constants.BeginStereoOverride();
					constants->blendPlane0 = viewInfo[1].invCameraProjMatrix * volume.blendInfo[0].blendPlane;
					constants->blendPlane1 = viewInfo[1].invCameraProjMatrix * volume.blendInfo[1].blendPlane;

					if (hVertexStream != ~0u && hIndexStream != ~0u)
						constants->transformMatrix = Matrix44(volume.mWorldTM.GetTransposed()) * viewInfo[1].cameraProjMatrix;

				}

				constantManager.EndTypedConstantUpdate(constants);

				m_blendValuesPass.AddPrimitive(&primBlend);
			}
		}
	}
}

void CClipVolumesStage::Execute()
{
	PROFILE_LABEL_SCOPE("CLIPVOLUMES");

	// Render clip volumes to stencil
	m_stencilPass.Execute();

	// Render portal blend values
	m_blendValuesPass.Execute();

	// Resolve final stencil buffer
	{
		static CCryNameTSCRC techResolveStencil("ResolveStencil");

		m_stencilResolvePass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_stencilResolvePass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_stencilResolvePass.SetTechnique(CShaderMan::s_shDeferredShading, techResolveStencil, 0);
		m_stencilResolvePass.SetRenderTarget(0, m_pBlendValuesRT);
		m_stencilResolvePass.SetState(GS_NODEPTHTEST | GS_NOCOLMASK_GBA);
		m_stencilResolvePass.SetTexture(4, m_pDepthTarget, EDefaultResourceViews::StencilOnly);
		m_stencilResolvePass.BeginConstantUpdate();
		m_stencilResolvePass.Execute();
	}

	ExecuteVolumetricFog();

	m_bClipVolumesValid = true;
}


void CClipVolumesStage::PrepareVolumetricFog()
{
	if (gcpRendD3D->m_bVolumetricFogEnabled)
	{
		CRenderView* pRenderView = RenderView();

		const int32 renderWidth  = pRenderView->GetRenderResolution()[0];
		const int32 renderHeight = pRenderView->GetRenderResolution()[1];

		// size for view frustum aligned volume texture.
		const int32 scaledWidth = CVolumetricFogStage::GetVolumeTextureSize(renderWidth, CRenderer::CV_r_VolumetricFogTexScale);
		const int32 scaledHeight = CVolumetricFogStage::GetVolumeTextureSize(renderHeight, CRenderer::CV_r_VolumetricFogTexScale);
		const int32 depth = CVolumetricFogStage::GetVolumeTextureDepthSize();

		D3DViewPort viewport;
		viewport.TopLeftX = viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(scaledWidth);
		viewport.Height = static_cast<float>(scaledHeight);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
		CRY_ASSERT(m_pClipVolumeStencilVolumeTex);
		if (m_pClipVolumeStencilVolumeTex
		    && (m_pClipVolumeStencilVolumeTex->GetWidth() != scaledWidth
		        || m_pClipVolumeStencilVolumeTex->GetHeight() != scaledHeight
		        || !CTexture::IsTextureExist(m_pClipVolumeStencilVolumeTex))
		    || m_depthTargetArrayVolFog.empty()
		    || m_depthTargetArrayVolFog.size() != depth)
		{
			m_volumetricStencilPass.SetViewport(viewport);

			// create jittered depth passes
			{
				for (int32 i = 0; i < m_jitteredDepthPassArray.size(); ++i)
				{
					m_jitteredDepthPassArray[i].reset();
				}

				m_jitteredDepthPassArray.resize(depth);

				for (int32 i = 0; i < depth; ++i)
				{
					m_jitteredDepthPassArray[i] = stl::make_unique<CFullscreenPass>();
					m_jitteredDepthPassArray[i]->SetFlags(CPrimitiveRenderPass::ePassFlags_None);
				}
			}

			m_depthTargetArrayVolFog.clear();
			m_depthTargetArrayVolFog.resize(depth);

			const uint32 commonFlags = FT_NOMIPS | FT_DONT_STREAM | FT_DONT_RELEASE;
			const uint32 rtFlags = commonFlags | FT_USAGE_RENDERTARGET;
			const uint32 dsFlags = commonFlags | FT_USAGE_DEPTHSTENCIL;
			const int32 w = scaledWidth;
			const int32 h = scaledHeight;
			const int32 d = depth;
			const ETEX_Format format = CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(eTF_D24S8);
			CTexture* pTex = CTexture::GetOrCreateTextureArray("$VolFogClipVolumeStencil", w, h, d, 1, eTT_2DArray, dsFlags, format);

			if (pTex == nullptr
			    || m_pClipVolumeStencilVolumeTex != pTex // texture name must exactly match the name when creating texture object.
			    || pTex->GetFlags() & FT_FAILED)
			{
				CryFatalError("Couldn't allocate texture.");
			}

			if (m_pClipVolumeStencilVolumeTex)
			{
				// DSV to clear stencil buffer.
				m_depthTargetVolFog = m_pClipVolumeStencilVolumeTex;

				// separate DSVs to write jittering depth.
				for (int32 i = 0; i < m_depthTargetArrayVolFog.size(); ++i)
				{
					SResourceView sliceView = SResourceView::DepthStencilView(DeviceFormats::ConvertFromTexFormat(m_depthTargetVolFog->GetDstFormat()), i, 1, 0);
					m_depthTargetArrayVolFog[i] = m_depthTargetVolFog->GetDevTexture()->GetOrCreateResourceViewHandle(sliceView); // separate DSV
				}

				// set depth target before AddPrimitive() is called.
				m_volumetricStencilPass.SetDepthTarget(m_depthTargetVolFog);

				// this buffer is managed explicitly on multi GPU.
				m_pClipVolumeStencilVolumeTex->DisableMgpuSync();
			}

			// need to clear depth buffer.
			const int32 safeMargin = 2; // workaround: shader isn't activated in initial several frames.
			m_cleared = gcpRendD3D->GetActiveGPUCount() + safeMargin;
		}
#else
		CRY_ASSERT(m_pClipVolumeStencilVolumeTex);
		if (m_pClipVolumeStencilVolumeTex
		    && (m_pClipVolumeStencilVolumeTex->GetWidth() != scaledWidth
		        || m_pClipVolumeStencilVolumeTex->GetHeight() != scaledHeight
		        || !CTexture::IsTextureExist(m_pClipVolumeStencilVolumeTex))
		    || m_depthTargetArrayVolFog.empty()
		    || m_depthTargetArrayVolFog.size() != depth)
		{
			// create volume stencil passes
			{
				for (int32 i = 0; i < m_volumetricStencilPassArray.size(); ++i)
				{
					m_volumetricStencilPassArray[i].reset();
				}

				m_volumetricStencilPassArray.resize(depth);

				for (int32 i = 0; i < depth; ++i)
				{
					m_volumetricStencilPassArray[i] = stl::make_unique<CFullscreenPass>();
					m_volumetricStencilPassArray[i]->SetViewport(viewport);
					m_volumetricStencilPassArray[i]->SetFlags(CPrimitiveRenderPass::ePassFlags_None);
				}
			}

			// create stencil resolve passes
			{
				for (int32 i = 0; i < m_resolveVolumetricStencilPassArray.size(); ++i)
				{
					m_resolveVolumetricStencilPassArray[i].reset();
				}

				m_resolveVolumetricStencilPassArray.resize(depth);

				for (int32 i = 0; i < depth; ++i)
				{
					m_resolveVolumetricStencilPassArray[i] = stl::make_unique<CFullscreenPass>();
					m_resolveVolumetricStencilPassArray[i]->SetFlags(CPrimitiveRenderPass::ePassFlags_None);
				}
			}

			// create jittered depth passes
			{
				for (int32 i = 0; i < m_jitteredDepthPassArray.size(); ++i)
				{
					m_jitteredDepthPassArray[i].reset();
				}

				m_jitteredDepthPassArray.resize(depth);

				for (int32 i = 0; i < depth; ++i)
				{
					m_jitteredDepthPassArray[i] = stl::make_unique<CFullscreenPass>();
					m_jitteredDepthPassArray[i]->SetFlags(CPrimitiveRenderPass::ePassFlags_None);
				}
			}

			const int32 w = scaledWidth;
			const int32 h = scaledHeight;
			const int32 d = depth;

			// create depth stencil textures
			{
				const uint32 commonFlags = FT_NOMIPS | FT_DONT_STREAM | FT_DONT_RELEASE;
				const uint32 rtFlags = commonFlags | FT_USAGE_RENDERTARGET;
				ETEX_Format format = eTF_R8;

				CTexture* pTex = CTexture::GetOrCreateTextureArray("$VolFogClipVolumeStencil", w, h, d, 1, eTT_2D, rtFlags, format);

				if (pTex == nullptr
				    || m_pClipVolumeStencilVolumeTex != pTex // texture name must exactly match the name when creating texture object.
				    || pTex->GetFlags() & FT_FAILED)
				{
					CryFatalError("Couldn't allocate texture.");
				}

				size_t size = m_pClipVolumeStencilVolumeTexArray.size();

				for (uint32 i = 0; i < size; ++i)
				{
					SAFE_RELEASE(m_pClipVolumeStencilVolumeTexArray[i]);
				}

				m_pClipVolumeStencilVolumeTexArray.resize(depth);

				const uint32 dsFlags = commonFlags | FT_USAGE_DEPTHSTENCIL;
				const ETEX_Format depthFormat = CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(eTF_D24S8);

				for (uint32 i = 0; i < depth; ++i)
				{
					CTexture* pTex = CTexture::GetOrCreateDepthStencil("", w, h, Clr_Empty, eTT_2D, dsFlags, depthFormat);
					if (!pTex || !pTex->IsValid())
					{
						CryFatalError("Couldn't allocate texture.");
					}

					m_pClipVolumeStencilVolumeTexArray[i] = pTex;
				}
			}

			// create depth targets
			{
				m_depthTargetArrayVolFog.clear();
				m_depthTargetArrayVolFog.resize(depth);

				for (int32 i = 0; i < m_depthTargetArrayVolFog.size(); ++i)
				{
					auto& depthTarget = m_depthTargetArrayVolFog[i];
					CTexture* pTex = m_pClipVolumeStencilVolumeTexArray[i];

					// this buffer is managed explicitly on multi GPU.
					pTex->DisableMgpuSync();

					// set depth target before AddPrimitive() is called.
					m_volumetricStencilPassArray[i]->SetDepthTarget(pTex);
				}
			}

			// need to clear depth buffer.
			const int32 safeMargin = 2; // workaround: shader isn't activated in initial several frames.
			m_cleared = gcpRendD3D->GetActiveGPUCount() + safeMargin;
		}
#endif
	}
}

void CClipVolumesStage::ExecuteVolumetricFog()
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	
	bool bFogEnabled = RenderView()->IsGlobalFogEnabled();

	const SRenderViewInfo& viewInfo = GetCurrentViewInfo();

	if (rd->m_bVolumetricFogEnabled && bFogEnabled)
	{
		PROFILE_LABEL_SCOPE("CLIPVOLUMES_FOR_VOLUMETRIC_FOG");

		const bool bReverseDepth = true;
		const float nearDepth = bReverseDepth ? 1.0f : 0.0f;
		const float raymarchDistance = RenderView()->GetShaderConstants().pVolumetricFogDistanceParams.w;

		if (m_nearDepth != nearDepth
		    || (0.25f < abs(m_raymarchDistance - raymarchDistance)))
		{
			// store values to observe the change.
			m_nearDepth = nearDepth;
			m_raymarchDistance = raymarchDistance;

			// need to clear depth buffer.
			m_cleared = (m_cleared > 0) ? m_cleared : rd->GetActiveGPUCount();
		}

		auto maxDSVCount = m_depthTargetArrayVolFog.size();
		const float fFar = viewInfo.farClipPlane;
		const float fNear = viewInfo.nearClipPlane;
		const float factor0 = fFar / (fFar - fNear);
		const float factor1 = fNear * -factor0;
		bool valid = true;

#ifdef FEATURE_RENDER_CLIPVOLUME_GEOMETRY_SHADER
		if (m_cleared > 0)
		{
			// Faster to clear the whole resource than to clear the individual slices one by one
			CClearSurfacePass::Execute(m_depthTargetVolFog, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_Empty.r, Val_Stencil);

			// write jittering depth.
			for (int32 i = 0; i < maxDSVCount; ++i)
			{
				// separate DSV
				auto& depthTargetView = m_depthTargetArrayVolFog[i];
				auto& pass = m_jitteredDepthPassArray[i];

				static CCryNameTSCRC shaderName("StoreJitteringDepthToClipVolumeDepth");
				pass->SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				pass->SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass->SetTechnique(CShaderMan::s_shDeferredShading, shaderName, 0);
				pass->SetState(GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);
				pass->SetDepthTarget(m_depthTargetVolFog, depthTargetView);

				pass->BeginConstantUpdate();

				static CCryNameR paramDepth("ParamDepth");
				const Vec4 vParamDepth(static_cast<float>(i), factor1, factor0, nearDepth);
				pass->SetConstant(paramDepth, vParamDepth, eHWSC_Pixel);

				bool success = pass->Execute();

				valid = valid && success;
			}
		}
		else
		{
			CClearSurfacePass::Execute(m_depthTargetVolFog, CLEAR_STENCIL, Clr_Unused.r, Val_Stencil);
		}

		// Render clip volumes to single DSV for depth stencil texture array.
		m_volumetricStencilPass.Execute();
#else
		if (m_cleared > 0)
		{
			for (int32 i = 0; i < maxDSVCount; ++i)
			{
				CTexture* const pTex = m_pClipVolumeStencilVolumeTexArray[i];
				auto& depthTarget = m_depthTargetArrayVolFog[i];
				auto& pass = m_jitteredDepthPassArray[i];

				CClearSurfacePass::Execute(pTex, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_Empty.r, Val_Stencil);

				// write jittering depth.
				static CCryNameTSCRC shaderName("StoreJitteringDepthToClipVolumeDepth");
				pass->SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				pass->SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass->SetTechnique(CShaderMan::s_shDeferredShading, shaderName, 0);
				pass->SetState(GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);
				pass->SetDepthTarget(pTex, depthTarget);

				pass->BeginConstantUpdate();

				static CCryNameR paramDepth("ParamDepth");
				const Vec4 vParamDepth(static_cast<float>(i), factor1, factor0, nearDepth);
				pass->SetConstant(paramDepth, vParamDepth, eHWSC_Pixel);

				bool success = pass->Execute();

				valid = valid && success;
			}
		}
		else
		{
			for (int32 i = 0; i < maxDSVCount; ++i)
			{
				CClearSurfacePass::Execute(m_pClipVolumeStencilVolumeTexArray[i], CLEAR_STENCIL, Clr_Unused.r, Val_Stencil);
			}
		}

		// Render clip volumes.
		for (int32 i = 0; i < maxDSVCount; ++i)
		{
			CTexture* const pTex = m_pClipVolumeStencilVolumeTexArray[i];
			auto& depthTarget = m_depthTargetArrayVolFog[i];
			auto& pass = m_volumetricStencilPassArray[i];

			pass->SetDepthTarget(pTex, depthTarget);
			pass->Execute();
		}

		// Resolve stencil
		static CCryNameTSCRC techResolveStencil("ResolveStencilVolFog");
		for (int32 i = 0; i < maxDSVCount; ++i)
		{
			auto renderTargetView = SResourceView::RenderTargetView(DeviceFormats::ConvertFromTexFormat(m_pClipVolumeStencilVolumeTex->GetDstFormat()), i, 1, 0, false);
			ResourceViewHandle rtvHandle = m_pClipVolumeStencilVolumeTex->GetDevTexture()->GetOrCreateResourceViewHandle(renderTargetView);

			CTexture* const pTex = m_pClipVolumeStencilVolumeTexArray[i];
			auto stencilOnlyView = SResourceView::ShaderResourceView(DeviceFormats::ConvertFromTexFormat(pTex->GetDstFormat()),
			                                                         0, -1, 0, -1, false, false, SResourceView::eSRV_StencilOnly);
			ResourceViewHandle srvHandle = pTex->GetDevTexture()->GetOrCreateResourceViewHandle(stencilOnlyView);

			auto& pPass = m_resolveVolumetricStencilPassArray[i];
			pPass->SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
			pPass->SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pPass->SetTechnique(CShaderMan::s_shDeferredShading, techResolveStencil, 0);
			pPass->SetRenderTarget(0, m_pClipVolumeStencilVolumeTex, rtvHandle);
			pPass->SetState(GS_NODEPTHTEST);
			pPass->SetTexture(4, pTex, srvHandle);
			pPass->BeginConstantUpdate();
			pPass->Execute();
		}
#endif

		if (valid)
		{
			m_cleared = (m_cleared > 0) ? (m_cleared - 1) : 0;
		}
	}
}
