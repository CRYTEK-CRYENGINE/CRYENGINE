// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderOutput.h"
#include "RenderDisplayContext.h"

std::string GenerateUniqueTextureName(const std::string prefix, uint32 id, const std::string name);

//////////////////////////////////////////////////////////////////////////
CRenderOutput::CRenderOutput(CRenderDisplayContext* pDisplayContext)
	: m_pDisplayContext(pDisplayContext)
{
	m_name     = pDisplayContext->m_name;
	m_uniqueId = pDisplayContext->GetID();

	InitializeDisplayContext();
}

CRenderOutput::CRenderOutput(CTexture* pColorTexture, uint32 clearFlags, ColorF clearColor, float clearDepth)
	: m_pColorTarget(pColorTexture)
	, m_clearTargetFlag(clearFlags)
	, m_clearColor(clearColor)
	, m_clearDepth(clearDepth)
	, m_bUseTempDepthBuffer(true)
{
	int outputWidth  = pColorTexture->GetWidth();
	int outputHeight = pColorTexture->GetHeight();

	ChangeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

CRenderOutput::CRenderOutput(CTexture* pColorTexture, CTexture* pDepthTexture, uint32 clearFlags, ColorF clearColor, float clearDepth)
	: m_pColorTarget(pColorTexture)
	, m_pDepthTarget(pDepthTexture)
	, m_clearTargetFlag(clearFlags)
	, m_clearColor(clearColor)
	, m_clearDepth(clearDepth)
	, m_bUseTempDepthBuffer(false)
{
	m_name     = pColorTexture->GetName();
	m_uniqueId = pColorTexture->GetID();

	int outputWidth  = pColorTexture->GetWidth();
	int outputHeight = pColorTexture->GetHeight();

	CRY_ASSERT(pDepthTexture &&
		outputWidth  == pDepthTexture->GetWidth() &&
		outputHeight == pDepthTexture->GetHeight());

	ChangeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

CRenderOutput::CRenderOutput(SDynTexture* pDynTexture, int32 outputWidth, int32 outputHeight, bool useTemporaryDepthBuffer, uint32 clearFlags, ColorF clearColor, float clearDepth)
	: m_pDynTexture(pDynTexture)
	, m_clearTargetFlag(clearFlags)
	, m_clearColor(clearColor)
	, m_clearDepth(clearDepth)
	, m_bUseTempDepthBuffer(useTemporaryDepthBuffer)
{
	m_name     = pDynTexture->GetTexture()->GetName();
	m_uniqueId = pDynTexture->GetTextureID();

	ChangeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

void CRenderOutput::InitializeDisplayContext()
{
	CRY_ASSERT(m_pDisplayContext);

	const int outputWidth  = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResWidth  ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResWidth ) : m_pDisplayContext->GetDisplayResolution()[0];
	const int outputHeight = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResHeight ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResHeight) : m_pDisplayContext->GetDisplayResolution()[1];

	InitializeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

void CRenderOutput::ReinspectDisplayContext()
{
	CRY_ASSERT(m_pDisplayContext);

	m_bUseTempDepthBuffer = (m_pDisplayContext->m_desc.renderFlags & (FRT_TEMPORARY_DEPTH)) != 0;

	m_clearTargetFlag = m_pDisplayContext->m_desc.renderFlags & (FRT_CLEAR);
	m_clearColor = m_pDisplayContext->m_desc.clearColor;
	m_clearDepth = m_pDisplayContext->m_desc.clearDepthStencil.r;

	const int outputWidth  = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResWidth  ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResWidth ) : m_pDisplayContext->GetDisplayResolution()[0];
	const int outputHeight = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResHeight ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResHeight) : m_pDisplayContext->GetDisplayResolution()[1];

	ChangeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

void CRenderOutput::BeginRendering(CRenderView* pRenderView, stl::optional<uint32> overrideClearFlags)
{
	m_bHDRRendering = pRenderView && pRenderView->AllowsHDRRendering();

	CRY_ASSERT(gcpRendD3D->m_pRT->IsRenderThread());

	//////////////////////////////////////////////////////////////////////////
	// Set HDR Render Target (Back Color Buffer)
	//////////////////////////////////////////////////////////////////////////

	if (m_pDisplayContext)
	{
		CRY_ASSERT(m_OutputWidth  == (m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResWidth  ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResWidth)  : m_pDisplayContext->GetDisplayResolution()[0]));
		CRY_ASSERT(m_OutputHeight == (m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResHeight ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResHeight) : m_pDisplayContext->GetDisplayResolution()[1]));

		if (pRenderView)
		{
			// This scope is the only one allowed to produce HDR data, all the rest is LDR
			m_pDisplayContext->BeginRendering();
// 			m_pDisplayContext->SetLastCamera(CCamera::eEye_Left, pRenderView->GetCamera(CCamera::eEye_Left));
// 			m_pDisplayContext->SetLastCamera(CCamera::eEye_Right, pRenderView->GetCamera(CCamera::eEye_Right));
		}
	}
	else if (m_pDynTexture)
	{
		m_pDynTexture->Update(m_OutputWidth, m_OutputHeight);
		m_pColorTarget = m_pDynTexture->m_pTexture;
	}

	//////////////////////////////////////////////////////////////////////////
	// Set Depth Z Render Target
	//////////////////////////////////////////////////////////////////////////
	if (m_bUseTempDepthBuffer)
	{
		m_pDepthTarget = nullptr;
		m_pDepthTarget.Assign_NoAddRef(CRendererResources::CreateDepthTarget(m_OutputWidth, m_OutputHeight, Clr_Empty, eTF_Unknown));
	}

	//////////////////////////////////////////////////////////////////////////
	// Clear render targets on demand.
	//////////////////////////////////////////////////////////////////////////
	uint32 clearTargetFlag = overrideClearFlags.value_or(m_clearTargetFlag);
	ColorF clearColor = m_clearColor;

	if (pRenderView && pRenderView->IsClearTarget() && CRendererCVars::CV_r_wireframe)
	{
		// Override clear color from the Render View if given
		clearTargetFlag |= FRT_CLEAR_COLOR;
		clearColor = pRenderView->GetTargetClearColor();
	}

	if ((clearTargetFlag = clearTargetFlag & ~m_hasBeenCleared))
	{
		if (clearTargetFlag & (FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL))
			CClearSurfacePass::Execute(m_pDepthTarget,
				(clearTargetFlag & FRT_CLEAR_DEPTH   ? CLEAR_ZBUFFER : 0) |
				(clearTargetFlag & FRT_CLEAR_STENCIL ? CLEAR_STENCIL : 0),
				Clr_FarPlane_Rev.r,
				Val_Stencil);

		if (clearTargetFlag & FRT_CLEAR_COLOR)
			CClearSurfacePass::Execute(m_pColorTarget, clearColor);

		m_hasBeenCleared |= clearTargetFlag;
	}

	//////////////////////////////////////////////////////////////////////////
	// Assign resources to RenderView
	//////////////////////////////////////////////////////////////////////////
	if (pRenderView)
	{
		// NOTE: for debugging to revert unsetting the target-pointers
		pRenderView->InspectRenderOutput();
	}

	//////////////////////////////////////////////////////////////////////////
	// TODO: make color and/or depth|stencil optional (currently it's enforced to have all of them)
	CRY_ASSERT(m_pColorTarget);
	CRY_ASSERT(m_pDepthTarget);
}

void CRenderOutput::EndRendering(CRenderView* pRenderView)
{
	CRY_ASSERT(gcpRendD3D->m_pRT->IsRenderThread());

	if (m_bUseTempDepthBuffer)
	{
		m_pDepthTarget = nullptr;
	}

	if (m_pDisplayContext)
	{
		m_pDisplayContext->EndRendering();
	}

	if (pRenderView)
	{
		// NOTE: for debugging to provoke access violation when used wrong
		pRenderView->UnsetRenderOutput();
	}

	m_bHDRRendering = false;
}

//////////////////////////////////////////////////////////////////////////
CTexture* CRenderOutput::GetColorTarget() const
{
	// Toggle back-buffer on first use
	if (m_bRenderToSwapChain)
		m_pDisplayContext->PostPresent();

	CRY_ASSERT(m_pColorTarget);
	return m_pColorTarget.get();
}

CTexture* CRenderOutput::GetDepthTarget() const
{
	if (m_pDisplayContext && !m_pDepthTarget)
		return m_pDisplayContext->GetCurrentDepthOutput();

	CRY_ASSERT(m_pDepthTarget || m_bUseTempDepthBuffer);
	return m_pDepthTarget.get();
}

void CRenderOutput::SetViewport(const SRenderViewport& viewport)
{
	m_viewport = viewport;
}

//////////////////////////////////////////////////////////////////////////
void CRenderOutput::ReleaseResources()
{
	m_pColorTarget = nullptr;
	m_pDepthTarget = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CRenderOutput::InitializeOutputResolution(int outputWidth, int outputHeight)
{
	// Custom resolution local targets are only allowed when a display-context is used.
	if (m_pDisplayContext)
	{
		const auto& displayResolution = m_pDisplayContext->GetDisplayResolution();

		if (0 != displayResolution[0] &&
			0 != displayResolution[1])
		{
			// Rule is: factor between [0.5,1.0] for output-to-display upscaling
			CRY_ASSERT((outputWidth  >= ((displayResolution[0] + 1) >> 1)) && (outputWidth  <= displayResolution[0]));
			CRY_ASSERT((outputHeight >= ((displayResolution[1] + 1) >> 1)) && (outputHeight <= displayResolution[1]));
		}
	}

	m_OutputWidth  = outputWidth;
	m_OutputHeight = outputHeight;
}

void CRenderOutput::ChangeOutputResolution(int outputWidth, int outputHeight)
{
	InitializeOutputResolution(outputWidth, outputHeight);

	// Custom resolution local targets are only allowed when a display-context is used.
	// Otherwise color- and depth-target(s) need to be supplied by the constructor above.
	m_bRenderToSwapChain = false;
	if (m_pDisplayContext)
	{
		const auto& displayResolution = m_pDisplayContext->GetDisplayResolution();

		if (outputWidth  == displayResolution[0] &&
		    outputHeight == displayResolution[1])
		{
			m_pDepthTarget = m_pDisplayContext->GetStorableDepthOutput();
			m_pColorTarget = m_pDisplayContext->GetStorableColorOutput();

			m_bRenderToSwapChain = !m_pDisplayContext->NeedsTempColor();
		}
	}

	if (!m_pColorTarget ||
	     m_OutputWidth  != m_pColorTarget->GetWidth() ||
	     m_OutputHeight != m_pColorTarget->GetHeight())
	{
		AllocateColorTarget();
	}

	if (!m_bUseTempDepthBuffer)
	{
		if (!m_pDepthTarget ||
		     m_OutputWidth  != m_pDepthTarget->GetWidth() ||
		     m_OutputHeight != m_pDepthTarget->GetHeight())
		{
			AllocateDepthTarget();
		}
	}

	// TODO: make color and/or depth|stencil optional (currently it's enforced to have all of them)
	CRY_ASSERT(m_pColorTarget);
	CRY_ASSERT(m_pDepthTarget || m_bUseTempDepthBuffer);
}

Vec2_tpl<uint32_t> CRenderOutput::GetDisplayResolution() const
{
	return m_pDisplayContext ? m_pDisplayContext->GetDisplayResolution() : GetOutputResolution();
}

//////////////////////////////////////////////////////////////////////////
ETEX_Format CRenderOutput::GetColorFormat() const
{
	// Make temporary resource in the same format as the owning display-context
	if (CRenderDisplayContext* pDisplayContext = m_pDisplayContext)
	{
		if (CTexture* pColorTarget = pDisplayContext->GetCurrentColorOutput())
			return pColorTarget->GetDstFormat();
		else
			return eTF_Unknown;
	}

	return m_bHDRRendering
		? CRendererResources::GetHDRFormat(false, false)
		: CRendererResources::GetLDRFormat();
}

void CRenderOutput::AllocateColorTarget()
{
	const ETEX_Format eCFormat = GetColorFormat();
	const ColorF clearValue = Clr_Empty;

	// NOTE: Actual device texture allocation happens just before rendering.
	const uint32 renderTargetFlags = FT_NOMIPS | /* FT_DONT_RELEASE | */FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
	const std::string uniqueTexName = GenerateUniqueTextureName(CImageExtensionHelper::IsDynamicRange(eCFormat) ? "$HDR-Output" : "$LDR-Output", m_uniqueId, m_name);

	m_pColorTarget = nullptr;
	if (eCFormat != eTF_Unknown)
		m_pColorTarget.Assign_NoAddRef(CTexture::GetOrCreateRenderTarget(uniqueTexName.c_str(), m_OutputWidth, m_OutputHeight, clearValue, eTT_2D, renderTargetFlags, eCFormat));
}

//////////////////////////////////////////////////////////////////////////
ETEX_Format CRenderOutput::GetDepthFormat() const
{
	// Make temporary resource in the same format as the owning display-context
	if (CRenderDisplayContext* pDisplayContext = m_pDisplayContext)
	{
		if (CTexture* pDepthTarget = pDisplayContext->GetCurrentDepthOutput())
			return pDepthTarget->GetDstFormat();
		else
			return eTF_Unknown;
	}

	return CRendererResources::GetDepthFormat();
}

void CRenderOutput::AllocateDepthTarget()
{
	const ETEX_Format eZFormat = GetDepthFormat();
	const float  clearDepth   = Clr_FarPlane_Rev.r;
	const uint8  clearStencil = Val_Stencil;
	const ColorF clearValues  = ColorF(clearDepth, FLOAT(clearStencil), 0.f, 0.f);

	// Create the native resolution depth stencil buffer for overlay rendering if needed
	const uint32 renderTargetFlags = FT_NOMIPS | /* FT_DONT_RELEASE | */FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL;
	const std::string uniqueTexName = GenerateUniqueTextureName("$Z-Output", m_uniqueId, m_name);

	m_pDepthTarget = nullptr;
	if (eZFormat != eTF_Unknown)
		m_pDepthTarget.Assign_NoAddRef(CTexture::GetOrCreateDepthStencil(uniqueTexName.c_str(), m_OutputWidth, m_OutputHeight, clearValues, eTT_2D, renderTargetFlags, eZFormat));
}
