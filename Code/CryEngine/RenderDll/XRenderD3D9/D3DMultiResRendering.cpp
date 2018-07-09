// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "D3DMultiResRendering.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "../Common/CryNameR.h"
#include "../Common/Shaders/CShader.h"
#include "../Common/Shaders/Shader.h"
#include "../Common/Textures/Texture.h"
#include "../Common/Renderer.h"
#include "../Common/RenderFrameProfiler.h"
#include "../Common/RenderMesh.h"
#include "../Common/Include_HLSL_CPP_Shared.h"

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
#include <NVIDIA/multiprojection_dx_2.0/nv_lens_matched_shading.cpp>
#include <NVIDIA/multiprojection_dx_2.0/nv_multi_res.cpp>
#include <NVIDIA/multiprojection_dx_2.0/nv_planar.cpp>
#endif

CVrProjectionManager* CVrProjectionManager::m_pInstance = nullptr;

CVrProjectionManager::CVrProjectionManager(CD3D9Renderer* const pRenderer)
	: m_pRenderer(pRenderer)
	, m_isConfigured(false)
	, m_multiResSupported(false)
	, m_lensMatchedSupported(false)
	, m_currentConfigMirrored(false)
	, m_currentPreset(0)
	, m_projection(eVrProjection_Planar)

{
	m_ptexZTargetFlattened = std::move(CTexture::GetOrCreateTextureObject("$ZTargetFlattened", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, CRendererResources::s_eTFZ));
}

void CVrProjectionManager::Init(CD3D9Renderer* const pRenderer)
{
	CRY_ASSERT(!m_pInstance);
	m_pInstance = new CVrProjectionManager(pRenderer);
	m_pInstance->m_projection = eVrProjection_Planar;

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	m_pInstance->m_pRenderer->GetDeviceContext().QueryNvidiaProjectionFeatureSupport(
		m_pInstance->m_multiResSupported,
		m_pInstance->m_lensMatchedSupported
	);

	if (CRenderer::CV_r_VrProjectionType == 1 && m_pInstance->m_multiResSupported)
		m_pInstance->m_projection = eVrProjection_MultiRes;
	else if (CRenderer::CV_r_VrProjectionType == 2 && m_pInstance->m_lensMatchedSupported)
		m_pInstance->m_projection = eVrProjection_LensMatched;
	else
		m_pInstance->m_projection = eVrProjection_Planar;
#endif
}

void CVrProjectionManager::Reset()
{
	SAFE_DELETE(m_pInstance);
}

bool CVrProjectionManager::IsMultiResEnabled() const
{
	return m_projection == eVrProjection_MultiRes || m_projection == eVrProjection_LensMatched;
}

bool CVrProjectionManager::IsMultiResEnabledStatic()
{
	return Instance() && Instance()->IsMultiResEnabled();
}

bool CVrProjectionManager::IsProjectionConfigured() const
{ 
	return m_isConfigured; 
}

void CVrProjectionManager::Configure(const SRenderViewport& originalViewport, bool bMirrored)
{
	m_isConfigured = false;
	m_currentConfigMirrored = bMirrored;

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	if (IsMultiResEnabled())
	{
		if (m_currentPreset != CRenderer::CV_r_VrProjectionPreset)
		{
			m_projectionCache.clear();
			m_currentPreset = CRenderer::CV_r_VrProjectionPreset;
		}

		m_originalViewport.TopLeftX = static_cast<float>(originalViewport.x);
		m_originalViewport.TopLeftY = static_cast<float>(originalViewport.y);
		m_originalViewport.Width = static_cast<float>(originalViewport.width);
		m_originalViewport.Height = static_cast<float>(originalViewport.height);
		m_originalViewport.MinDepth = originalViewport.zmin;
		m_originalViewport.MaxDepth = originalViewport.zmax;

		// In this particular integration, original viewport covers the entire texture.
		// In general, that's not necessarily the case.
		Nv::VR::Float2 textureSize = Nv::VR::Float2{ m_originalViewport.Width, m_originalViewport.Height };

		CalculateViewportsAndBufferData(textureSize, m_originalViewport, textureSize, m_planarConfig, m_planarData);

		switch (m_projection)
		{
		case eVrProjection_Planar:
			m_data = m_planarData;
			m_isConfigured = true;
			break;

		case eVrProjection_MultiRes:
			switch (CRenderer::CV_r_VrProjectionPreset)
			{
			case 1:
				m_multiResConfig = Nv::VR::MultiRes::Configuration_Balanced;
				break;

			case 2:
				m_multiResConfig = Nv::VR::MultiRes::Configuration_Aggressive;
				break;

			case 3:
			case 4:
			case 5:
				m_multiResConfig = Nv::VR::MultiRes::ConfigurationSet_OculusRift_CV1[CRenderer::CV_r_VrProjectionPreset - 3];
				break;

			case 6:
			case 7:
			case 8:
				m_multiResConfig = Nv::VR::MultiRes::ConfigurationSet_HTC_Vive[CRenderer::CV_r_VrProjectionPreset - 6];
				break;

			case 0:
			default:
				m_multiResConfig = Nv::VR::MultiRes::Configuration_Null;
				break;
			}

			Nv::VR::MultiRes::Configuration mirroredMultiResConfig;
			if (CRenderer::CV_r_stereoMirrorProjection)
				Nv::VR::CalculateMirroredConfig(m_multiResConfig, mirroredMultiResConfig);
			else
				mirroredMultiResConfig = m_multiResConfig;

			if (bMirrored)
			{
				std::swap(m_multiResConfig, mirroredMultiResConfig);
			}

			CalculateViewportsAndBufferData(textureSize, m_originalViewport, textureSize, m_multiResConfig, m_data);
			CalculateViewportsAndBufferData(textureSize, m_originalViewport, textureSize, mirroredMultiResConfig, m_mirroredData);
			m_isConfigured = true;
			break;

		case eVrProjection_LensMatched:
			switch (CRenderer::CV_r_VrProjectionPreset)
			{
			case 1:
				m_lensMatchedConfig = Nv::VR::LensMatched::Configuration_Balanced;
				break;

			case 2:
				m_lensMatchedConfig = Nv::VR::LensMatched::Configuration_Aggressive;
				break;

			case 3:
			case 4:
			case 5:
				m_lensMatchedConfig = Nv::VR::LensMatched::ConfigurationSet_OculusRift_CV1[CRenderer::CV_r_VrProjectionPreset - 3];
				break;

			case 6:
			case 7:
			case 8:
				m_lensMatchedConfig = Nv::VR::LensMatched::ConfigurationSet_HTC_Vive[CRenderer::CV_r_VrProjectionPreset - 6];
				break;

			case 0:
			default:
				m_lensMatchedConfig = Nv::VR::LensMatched::Configuration_Null;
				break;
			}

			Nv::VR::LensMatched::Configuration mirroredLensMatchedConfig;
			if (CRenderer::CV_r_stereoMirrorProjection)
				Nv::VR::CalculateMirroredConfig(m_lensMatchedConfig, mirroredLensMatchedConfig);
			else
				mirroredLensMatchedConfig = m_lensMatchedConfig;

			if (bMirrored)
			{
				std::swap(m_lensMatchedConfig, mirroredLensMatchedConfig);
			}

			CalculateViewportsAndBufferData(textureSize, m_originalViewport, textureSize, m_lensMatchedConfig, m_data);
			CalculateViewportsAndBufferData(textureSize, m_originalViewport, textureSize, mirroredLensMatchedConfig, m_mirroredData);
			m_isConfigured = true;
			m_currentConfigMirrored = bMirrored;
			break;
		}
	}
#endif
}


bool CVrProjectionManager::SetRenderingState(CDeviceCommandListRef RESTRICT_REFERENCE commandList, const D3D11_VIEWPORT& viewport, bool bSetViewports, bool bBindConstantBuffer)
{
#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	if ((!bSetViewports && !bBindConstantBuffer) || !IsMultiResEnabled())
		return false;

	CRY_ASSERT(IsProjectionConfigured());

	const auto& projectionInfo = GetProjectionForViewport(viewport);

	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
	CDeviceNvidiaCommandInterface*   pNvidiaCommandInterface = commandList.GetNvidiaCommandInterface();
	CRY_ASSERT(pNvidiaCommandInterface);

	if (bSetViewports)
	{
		const auto& viewports = projectionInfo.data.Viewports;

		pCommandInterface->SetViewports(viewports.NumViewports, (D3D11_VIEWPORT*)viewports.Viewports);
		pCommandInterface->SetScissorRects(viewports.NumViewports, (D3D11_RECT*)viewports.Scissors);

		if (m_projection == eVrProjection_LensMatched)
		{
			float fA[4] = { -m_lensMatchedConfig.WarpLeft, m_lensMatchedConfig.WarpRight, -m_lensMatchedConfig.WarpLeft,  m_lensMatchedConfig.WarpRight };
			float fB[4] = {  m_lensMatchedConfig.WarpUp,   m_lensMatchedConfig.WarpUp,    -m_lensMatchedConfig.WarpDown, -m_lensMatchedConfig.WarpDown };

			pNvidiaCommandInterface->SetModifiedWMode(true, 4, fA, fB);
		}
	}

	if (bBindConstantBuffer)
	{
		pCommandInterface->SetInlineConstantBuffer(EResourceLayoutSlot_VrProjectionCB, projectionInfo.pConstantBuffer, eConstantBufferShaderSlot_VrProjection, EShaderStage_AllWithoutCompute);
	}

	return bSetViewports;
#else
	return false;
#endif
}

void CVrProjectionManager::RestoreState(CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	if (m_projection == eVrProjection_LensMatched)
	{
		CDeviceNvidiaCommandInterface* pNvidiaCommandInterface = commandList.GetNvidiaCommandInterface();
		CRY_ASSERT(pNvidiaCommandInterface);

		pNvidiaCommandInterface->SetModifiedWMode(false, 0, nullptr, nullptr);
	}
}

uint64 CVrProjectionManager::GetRTFlags() const
{
	if (m_projection == eVrProjection_MultiRes)
		return g_HWSR_MaskBit[HWSR_PROJECTION_MULTI_RES];
	else if(m_projection == eVrProjection_LensMatched)
		return g_HWSR_MaskBit[HWSR_PROJECTION_LENS_MATCHED];

	return 0;
}

void CVrProjectionManager::GetProjectionSize(int flattenedWidth, int flattenedHeight, int & projectionWidth, int & projectionHeight)
{
	CRY_ASSERT(IsMultiResEnabled() && m_isConfigured);

	projectionWidth  = flattenedWidth;
	projectionHeight = flattenedHeight;

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	projectionWidth  = int(ceil(float(flattenedWidth)  / float(m_originalViewport.Width)  * float(m_data.Viewports.BoundingRect.Right)));
	projectionHeight = int(ceil(float(flattenedHeight) / float(m_originalViewport.Height) * float(m_data.Viewports.BoundingRect.Bottom)));
#endif
}

CConstantBufferPtr CVrProjectionManager::GetProjectionConstantBuffer(int flattenedWidth, int flattenedHeight)
{
	CRY_ASSERT(IsMultiResEnabled());
	CConstantBufferPtr pResult;

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	const auto& projectionInfo = GetProjectionForViewport(D3D11_VIEWPORT{ 0.f, 0.f, float(flattenedWidth), float(flattenedHeight), 0.f, 1.f });
	pResult = projectionInfo.pConstantBuffer;
#endif

	return pResult;
}

void CVrProjectionManager::PrepareProjectionParameters(CDeviceCommandListRef RESTRICT_REFERENCE commandList, const D3D11_VIEWPORT& viewport)
{
	CRY_ASSERT(IsMultiResEnabled());

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	const auto& projectionInfo = GetProjectionForViewport(viewport);
	commandList.GetGraphicsInterface()->PrepareInlineConstantBufferForUse(EResourceLayoutSlot_VrProjectionCB, projectionInfo.pConstantBuffer, eConstantBufferShaderSlot_VrProjection, EShaderStage_All);
#endif
}

void CVrProjectionManager::MapScreenPosToMultiRes(float& x, float& y) const
{
	CRY_ASSERT(IsMultiResEnabled());

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	Nv::VR::Float2 planarWindowPos = Nv::VR::Float2{ x, y };
	Nv::VR::Float2 clipPos = Nv::VR::MapWindowToClip<Nv::VR::Projection::PLANAR>(m_planarData, planarWindowPos);
	Nv::VR::Float2 windowPos = planarWindowPos;
	switch (m_projection)
	{
	case eVrProjection_MultiRes:
		windowPos = Nv::VR::MapClipToWindow<Nv::VR::Projection::MULTI_RES>(m_data, clipPos);
		break;
	case eVrProjection_LensMatched:
		windowPos = Nv::VR::MapClipToWindow<Nv::VR::Projection::LENS_MATCHED>(m_data, clipPos);
		break;
	}
	x = windowPos.x;
	y = windowPos.y;
#endif
}

void CVrProjectionManager::ExecuteFlattenDepth(CTexture* pSrcRT, CTexture* pDestRT)
{
	CRY_ASSERT(IsMultiResEnabled());

	PROFILE_LABEL_SCOPE("FLATTEN_DEPTH");

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CShader* pShader = CShaderMan::s_shPostEffects;

	const int dstWidth  = pSrcRT->GetWidth();
	const int dstHeight = pSrcRT->GetHeight();

	if (!CTexture::IsTextureExist(pDestRT) || pDestRT->GetWidth() != dstWidth || pDestRT->GetHeight() != dstHeight)
	{
		pDestRT->SetWidth(dstWidth);
		pDestRT->SetHeight(dstHeight);
		pDestRT->CreateRenderTarget(CRendererResources::s_eTFZ, ColorF(1.0f, 1.0f, 1.0f, 1.0f));
	}

	if (m_passDepthFlattening.IsDirty())
	{
		static CCryNameTSCRC techFlattenDepth("FlattenDepth");

		m_passDepthFlattening.SetTechnique(pShader, techFlattenDepth, CVrProjectionManager::Instance()->GetRTFlags());
		m_passDepthFlattening.SetRenderTarget(0, pDestRT);
		m_passDepthFlattening.SetState(GS_NODEPTHTEST);
		m_passDepthFlattening.SetFlags(CPrimitiveRenderPass::ePassFlags_RequireVrProjectionConstants);
		m_passDepthFlattening.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passDepthFlattening.SetRequirePerViewConstantBuffer(true);
		m_passDepthFlattening.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);

		m_passDepthFlattening.BeginConstantUpdate();
		
		m_passDepthFlattening.SetTexture(16, pSrcRT);
	}

	m_passDepthFlattening.Execute();
}

void CVrProjectionManager::ExecuteLensMatchedOctagon(CTexture* pDestRT)
{
	CRY_ASSERT(IsMultiResEnabled());

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CShader* pShader = CShaderMan::s_shPostEffects;

	static const CCryNameTSCRC techName("DrawLensMatchedOctagon");

	m_primitiveLensMatchedOctagon.SetTechnique(pShader, techName, CVrProjectionManager::Instance()->GetRTFlags());
	m_primitiveLensMatchedOctagon.SetRenderState(GS_DEPTHFUNC_LESS | GS_DEPTHWRITE);
	m_primitiveLensMatchedOctagon.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);

	if (m_primitiveLensMatchedOctagon.IsDirty())
	{
		m_passLensMatchedOctagon.SetFlags(CPrimitiveRenderPass::ePassFlags_UseVrProjectionState);
		m_passLensMatchedOctagon.SetViewport(D3D11_VIEWPORT{ 0.f, 0.f, float(pDestRT->GetWidth()), float(pDestRT->GetHeight()), 0.f, 0.f });
		m_passLensMatchedOctagon.SetDepthTarget(pDestRT);

		m_passLensMatchedOctagon.BeginAddingPrimitives();
		m_passLensMatchedOctagon.AddPrimitive(&m_primitiveLensMatchedOctagon);
	}

	if (m_passLensMatchedOctagon.GetPrimitiveCount() > 0)
		m_passLensMatchedOctagon.Execute();
}


#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)

void CVrProjectionManager::GetDerivedData(float width, float height, Nv::VR::Data* pOutData, bool bMirrored) const
{
	if (!m_isConfigured)
		return;

	CRY_ASSERT(pOutData);

	// The simplest way is to just call CalculateViewportsAndBufferData with the new dimensions.
	// But that function has some rounding inside, so an MRS grid configured for a lower res will not
	// exactly match the high-res grid downsized by a certain factor (normally 1/2 or 1/4x).
	// To avoid that rounding, all relevant members of m_data are multiplied by the resolution factor.

	if (bMirrored)
		*pOutData = m_mirroredData;
	else
		*pOutData = m_data;

	if (width == m_originalViewport.Width && height == m_originalViewport.Height)
		return;

	float fx = width / m_originalViewport.Width;
	float fy = height / m_originalViewport.Height;

	for (int vp = 0; vp < m_data.Viewports.NumViewports; vp++)
	{
		Nv::VR::Viewport& viewport = pOutData->Viewports.Viewports[vp];
		viewport.TopLeftX *= fx;
		viewport.TopLeftY *= fy;
		viewport.Width *= fx;
		viewport.Height *= fy;

		Nv::VR::ScissorRect& scissor = pOutData->Viewports.Scissors[vp];
		scissor.Left = int(floor(scissor.Left * fx));
		scissor.Right = int(floor(scissor.Right * fx));
		scissor.Top = int(floor(scissor.Top * fy));
		scissor.Bottom = int(floor(scissor.Bottom * fy));
	}

	pOutData->Viewports.FlattenedSize.x = width;
	pOutData->Viewports.FlattenedSize.y = height;

	Nv::VR::ScissorRect& bounding = pOutData->Viewports.BoundingRect;
	bounding.Left = int(floor(bounding.Left * fx));
	bounding.Right = int(floor(bounding.Right * fx));
	bounding.Top = int(floor(bounding.Top * fy));
	bounding.Bottom = int(floor(bounding.Bottom * fy));

	pOutData->RemapCbData.BoundingRectOrigin.x *= fx;
	pOutData->RemapCbData.BoundingRectOrigin.y *= fy;
	pOutData->RemapCbData.BoundingRectSize.x *= fx;
	pOutData->RemapCbData.BoundingRectSize.y *= fy;
	pOutData->RemapCbData.BoundingRectSizeInv.x /= fx;
	pOutData->RemapCbData.BoundingRectSizeInv.y /= fy;
	pOutData->RemapCbData.TextureSize.x = width;
	pOutData->RemapCbData.TextureSize.y = height;
	pOutData->RemapCbData.TextureSizeInv.x = 1.0f / width;
	pOutData->RemapCbData.TextureSizeInv.y = 1.0f / height;

	for (int i = 0; i < 3; i++)
	{
		pOutData->RemapCbData.ClipToWindowX[i].Bias *= fx;
		pOutData->RemapCbData.ClipToWindowX[i].Scale *= fx;
		pOutData->RemapCbData.ClipToWindowY[i].Bias *= fy;
		pOutData->RemapCbData.ClipToWindowY[i].Scale *= fy;

		pOutData->RemapCbData.WindowToClipX[i].Scale /= fx;
		pOutData->RemapCbData.WindowToClipY[i].Scale /= fy;
	}

	for (int i = 0; i < 2; i++)
	{
		pOutData->RemapCbData.WindowToClipSplitsX[i] *= fx;
		pOutData->RemapCbData.WindowToClipSplitsY[i] *= fy;
	}
}

CVrProjectionManager::VrProjectionInfo& CVrProjectionManager::GetProjectionForViewport(const D3D11_VIEWPORT& viewport)
{
	for (auto& entry : m_projectionCache)
	{
		if (entry.data.Viewports.FlattenedSize.x == viewport.Width
			&& entry.data.Viewports.FlattenedSize.y == viewport.Height
			&& entry.data.Viewports.Viewports[0].MinDepth == viewport.MinDepth
			&& entry.data.Viewports.Viewports[0].MaxDepth == viewport.MaxDepth
			&& entry.bMirrored == m_currentConfigMirrored)
		{
			return entry;
		}
	}

	m_projectionCache.resize(m_projectionCache.size() + 1);
	CVrProjectionManager::VrProjectionInfo& entry = m_projectionCache[m_projectionCache.size() - 1];
	Nv::VR::Data mirroredData;
	GetDerivedData(viewport.Width, viewport.Height, &entry.data, m_currentConfigMirrored);
	GetDerivedData(viewport.Width, viewport.Height, &mirroredData, !m_currentConfigMirrored);

	for (int nViewport = 0; nViewport < entry.data.Viewports.NumViewports; nViewport++)
	{
		entry.data.Viewports.Viewports[nViewport].MinDepth = viewport.MinDepth;
		entry.data.Viewports.Viewports[nViewport].MaxDepth = viewport.MaxDepth;
	}

	HLSL_VrProjectionConstantBuffer cb;

	// CB for the left eye
	memcpy(cb.CVP_GeometryShaderParams, &entry.data.FastGsCbData, sizeof(cb.CVP_GeometryShaderParams));
	memcpy(cb.CVP_ProjectionParams, &entry.data.RemapCbData, sizeof(cb.CVP_ProjectionParams));
	memcpy(cb.CVP_ProjectionParamsOtherEye, &mirroredData.RemapCbData, sizeof(cb.CVP_ProjectionParamsOtherEye));
	entry.pConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(cb));
	entry.pConstantBuffer->UpdateBuffer(&cb, sizeof(cb));

	return entry;
}

#endif