// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderOutput.h"
#include "RenderDisplayContext.h"

//////////////////////////////////////////////////////////////////////////
CRenderOutput::CRenderOutput(CRenderDisplayContext* pDisplayContext)
	: m_pDisplayContext(pDisplayContext)
	, m_outputType(EOutputType::DisplayContext)
{
	InitializeDisplayContext();
}

CRenderOutput::CRenderOutput(CTexture* pColorTexture, uint32 clearFlags, ColorF clearColor, float clearDepth)
	: m_pTexture(pColorTexture)
	, m_outputType(EOutputType::Texture)
	, m_bUseTempDepthBuffer(true)
	, m_clearTargetFlag(clearFlags)
	, m_clearColor(clearColor)
	, m_clearDepth(clearDepth)
{
	int outputWidth  = pColorTexture->GetWidth();
	int outputHeight = pColorTexture->GetHeight();

	ChangeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));

	// this is just for keeping a ref count on pColorTexture (no smart_ptr in unions :( )
	m_pColorTarget = pColorTexture;
}

CRenderOutput::CRenderOutput(SDynTexture* pDynTexture, int32 outputWidth, int32 outputHeight, bool useTemporaryDepthBuffer, uint32 clearFlags, ColorF clearColor, float clearDepth)
	: m_pDynTexture(pDynTexture)
	, m_outputType(EOutputType::DynTexture)
	, m_bUseTempDepthBuffer(true)
	, m_clearTargetFlag(clearFlags)
	, m_clearColor(clearColor)
	, m_clearDepth(clearDepth)
{
	ChangeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

CRenderOutput::~CRenderOutput()
{
}

void CRenderOutput::InitializeDisplayContext()
{
	CRY_ASSERT(m_outputType == EOutputType::DisplayContext);

	const int outputWidth  = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResWidth  ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResWidth ) : m_pDisplayContext->GetDisplayResolution()[0];
	const int outputHeight = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResHeight ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResHeight) : m_pDisplayContext->GetDisplayResolution()[1];

	InitializeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

void CRenderOutput::ReinspectDisplayContext()
{
	CRY_ASSERT(m_outputType == EOutputType::DisplayContext);

	m_bUseTempDepthBuffer = (m_pDisplayContext->m_desc.renderFlags & (FRT_TEMPORARY_DEPTH)) != 0;

	m_clearTargetFlag = m_pDisplayContext->m_desc.renderFlags & (FRT_CLEAR);
	m_clearColor = m_pDisplayContext->m_desc.clearColor;
	m_clearDepth = m_pDisplayContext->m_desc.clearDepthStencil.r;

	const int outputWidth  = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResWidth  ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResWidth ) : m_pDisplayContext->GetDisplayResolution()[0];
	const int outputHeight = m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResHeight ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResHeight) : m_pDisplayContext->GetDisplayResolution()[1];

	ChangeOutputResolution(outputWidth, outputHeight);
	SetViewport(SRenderViewport(0, 0, outputWidth, outputHeight));
}

void CRenderOutput::BeginRendering(CRenderView* pRenderView)
{
	CRY_ASSERT(gcpRendD3D->m_pRT->IsRenderThread());

	//////////////////////////////////////////////////////////////////////////
	// Set HDR Render Target (Back Color Buffer)
	//////////////////////////////////////////////////////////////////////////

	if (m_outputType == EOutputType::DisplayContext)
	{
		CRY_ASSERT(gcpRendD3D.GetActiveDisplayContext()->GetHandle() == m_pDisplayContext->m_hWnd);

		CRY_ASSERT(m_OutputWidth  == (m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResWidth  ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResWidth)  : m_pDisplayContext->GetDisplayResolution()[0]));
		CRY_ASSERT(m_OutputHeight == (m_pDisplayContext->IsScalable() && CRendererCVars::CV_r_CustomResHeight ? std::min(CRendererCVars::CV_r_CustomResMaxSize, CRendererCVars::CV_r_CustomResHeight) : m_pDisplayContext->GetDisplayResolution()[1]));

		if (pRenderView)
		{
			// This scope is the only one allowed to produce HDR data, all the rest is LDR
			m_pDisplayContext->ToggleHDRRendering(pRenderView->IsHDRModeEnabled());
			m_pDisplayContext->SetLastCamera(CCamera::eEye_Left, pRenderView->GetCamera(CCamera::eEye_Left));
			m_pDisplayContext->SetLastCamera(CCamera::eEye_Right, pRenderView->GetCamera(CCamera::eEye_Right));

			// Fetch stereo target HDR texture if rendering Stereo.
			if (m_pDisplayContext->m_bMainViewport) //@TODO: This need fixing
			{
				uint32 nShaderRenderFlags = pRenderView->GetShaderRenderingFlags();

				if (gcpRendD3D.GetS3DRend().IsStereoEnabled() && (nShaderRenderFlags & (SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE)))
				{
					m_pColorTarget = gcpRendD3D.GetS3DRend().GetEyeTarget((nShaderRenderFlags & SHDF_STEREO_LEFT_EYE) ? LEFT_EYE : RIGHT_EYE);

					CRY_ASSERT(m_bUseEyeColorBuffer);
					CRY_ASSERT(
						m_pColorTarget->GetWidth() == m_OutputWidth &&
						m_pColorTarget->GetHeight() == m_OutputHeight
					);
				}
			}
		}
	}
	else if (m_outputType == EOutputType::DynTexture)
	{
		m_pDynTexture->Update(m_OutputWidth, m_OutputHeight);
		m_pColorTarget = m_pDynTexture->m_pTexture;
	}

	//////////////////////////////////////////////////////////////////////////
	// Set Depth Z Render Target
	//////////////////////////////////////////////////////////////////////////
	if (m_bUseTempDepthBuffer)
	{
		m_pTempDepthTexture = gcpRendD3D->FX_GetDepthSurface(m_OutputWidth, m_OutputHeight, false, true);
		CRY_ASSERT(m_pTempDepthTexture);

		m_pDepthTarget = m_pTempDepthTexture->pTexture;
		m_pTempDepthTexture->bBusy = true;
		m_pDepthTarget->Lock();
		m_pDepthTarget->m_nUpdateFrameID = gRenDev->GetRenderFrameID();
	}

	//////////////////////////////////////////////////////////////////////////
	// Clear render targets on demand.
	//////////////////////////////////////////////////////////////////////////
	uint32 clearTargetFlag = m_clearTargetFlag;
	ColorF clearColor = m_clearColor;

	if (pRenderView && pRenderView->IsClearTarget())
	{
		// Override clear color from the Render View if given
		clearTargetFlag |= FRT_CLEAR_COLOR;
		clearColor = pRenderView->GetTargetClearColor();
	}

	if (clearTargetFlag = clearTargetFlag & ~m_hasBeenCleared)
	{
		if (clearTargetFlag & (FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL))
		{
			const bool bReverseDepth = true;
			const float clearDepth = bReverseDepth ? 1.0f - m_clearDepth : m_clearDepth;

			CClearSurfacePass::Execute(m_pDepthTarget,
				(clearTargetFlag & FRT_CLEAR_DEPTH ? CLEAR_ZBUFFER : 0) |
				(clearTargetFlag & FRT_CLEAR_STENCIL ? CLEAR_STENCIL : 0),
				clearDepth, 1);
		}

		if (clearTargetFlag & FRT_CLEAR_COLOR)
		{
			CClearSurfacePass::Execute(m_pColorTarget, clearColor);
		}

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
	CRY_ASSERT(m_pColorTarget && CTexture::IsTextureExist(m_pColorTarget));
	CRY_ASSERT(m_pDepthTarget && CTexture::IsTextureExist(m_pDepthTarget));
}

void CRenderOutput::EndRendering(CRenderView* pRenderView)
{
	CRY_ASSERT(gcpRendD3D->m_pRT->IsRenderThread());

	if (m_pTempDepthTexture)
	{
		m_pTempDepthTexture->bBusy = false;
		if (m_pTempDepthTexture->pTexture)
		{
			m_pTempDepthTexture->pTexture->Unlock();
		}
		m_pTempDepthTexture = nullptr;
	}

	if (pRenderView)
	{
		// NOTE: for debugging to provoke access violation when used wrong
		pRenderView->UnsetRenderOutput();
	}

	if (m_outputType == EOutputType::DisplayContext)
	{
		m_pDisplayContext->ToggleHDRRendering(false);
	}
}

//////////////////////////////////////////////////////////////////////////
CTexture* CRenderOutput::GetColorTarget() const
{
	// TODO: make color and/or depth|stencil optional (currently it's enforced to have all of them)
	// Depth|stencil-only would not need a color target
	CRY_ASSERT(m_pColorTarget && CTexture::IsTextureExist(m_pColorTarget));
	return m_pColorTarget.get();
}

CTexture* CRenderOutput::GetDepthTarget() const
{
	// TODO: make color and/or depth|stencil optional (currently it's enforced to have all of them)
	// Color-only would not need a depth|stencil target
	CRY_ASSERT(m_pDepthTarget && CTexture::IsTextureExist(m_pDepthTarget));
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
	if (m_outputType == EOutputType::DisplayContext)
	{
		Vec2i displayResolution = m_pDisplayContext->GetDisplayResolution();

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
	if (CRenderDisplayContext* pDisplayContext = m_pDisplayContext)
	{
		Vec2i displayResolution = pDisplayContext->GetDisplayResolution();

		if (outputWidth  == displayResolution[0] &&
			outputHeight == displayResolution[1])
		{
			m_pDepthTarget = pDisplayContext->GetStorableDepthOutput();
			m_pColorTarget = pDisplayContext->GetStorableColorOutput();
		}
		else
		{
			if (!m_pColorTarget || !CTexture::IsTextureExist(m_pColorTarget) ||
				m_OutputWidth  != m_pColorTarget->GetWidth() ||
				m_OutputHeight != m_pColorTarget->GetHeight())
			{
				AllocateColorTarget();
			}

			if (!m_pDepthTarget || !CTexture::IsTextureExist(m_pDepthTarget) ||
				m_OutputWidth  != m_pDepthTarget->GetWidth() ||
				m_OutputHeight != m_pDepthTarget->GetHeight())
			{
				AllocateDepthTarget();
			}
		}
	}

	// TODO: make color and/or depth|stencil optional (currently it's enforced to have all of them)
	CRY_ASSERT(m_pColorTarget && CTexture::IsTextureExist(m_pColorTarget) || m_bUseEyeColorBuffer);
	CRY_ASSERT(m_pDepthTarget && CTexture::IsTextureExist(m_pDepthTarget) || m_bUseTempDepthBuffer);
}

Vec2i CRenderOutput::GetDisplayResolution() const
{
	return m_pDisplayContext ? m_pDisplayContext->GetDisplayResolution() : GetOutputResolution();
}

//////////////////////////////////////////////////////////////////////////
void CRenderOutput::AllocateColorTarget()
{
	m_pColorTarget = nullptr;
	if (m_bUseEyeColorBuffer)
		return;

	const ETEX_Format eHDRFormat =
		CRenderer::CV_r_HDRTexFormat == 0 ? eTF_R11G11B10F : eTF_R16G16B16A16F;

	const ColorF clearValue = Clr_Empty;

	// NOTE: Actual device texture allocation happens just before rendering.
	const uint32 renderTargetFlags = FT_NOMIPS | /* FT_DONT_RELEASE | */FT_DONT_STREAM | FT_USAGE_RENDERTARGET;

	m_pColorTarget = CTexture::GetOrCreateRenderTarget("$HDR-Output", m_OutputWidth, m_OutputHeight, clearValue, eTT_2D, renderTargetFlags, eHDRFormat);
}

//////////////////////////////////////////////////////////////////////////
void CRenderOutput::AllocateDepthTarget()
{
	m_pDepthTarget = nullptr;
	if (m_bUseTempDepthBuffer)
		return;

	const ETEX_Format eZFormat =
		gRenDev->GetDepthBpp() == 32 ? eTF_D32FS8 :
		gRenDev->GetDepthBpp() == 24 ? eTF_D24S8  :
		gRenDev->GetDepthBpp() ==  8 ? eTF_D16S8  : eTF_D16;

	const float  clearDepth   = CRenderer::CV_r_ReverseDepth ? 0.f : 1.f;
	const uint   clearStencil = 1;
	const ColorF clearValues  = ColorF(clearDepth, FLOAT(clearStencil), 0.f, 0.f);

	// Create the native resolution depth stencil buffer for overlay rendering if needed
	const uint32 renderTargetFlags = FT_NOMIPS | /* FT_DONT_RELEASE | */FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL;

	m_pDepthTarget = CTexture::GetOrCreateDepthStencil("$Z-Output", m_OutputWidth, m_OutputHeight, clearValues, eTT_2D, renderTargetFlags, eZFormat);
}
