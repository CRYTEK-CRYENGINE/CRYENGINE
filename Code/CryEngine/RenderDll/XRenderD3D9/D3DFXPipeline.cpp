// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DFXPipeline.cpp : Direct3D specific FX shaders rendering pipeline.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IRenderNode.h>
#include "../Common/ReverseDepth.h"
#include "../Common/ComputeSkinningStorage.h"
#include "DeviceManager/DeviceObjects.h" // SInputLayout

#if defined(FEATURE_SVO_GI)
#include "D3D_SVO.h"
#endif

//====================================================================================

HRESULT CD3D9Renderer::FX_SetVertexDeclaration(int StreamMask, InputLayoutHandle eVF)
{
	FUNCTION_PROFILER_RENDER_FLAT

	if (eVF == InputLayoutHandle::Unspecified)
	{
		if (m_pLastVDeclaration != nullptr)
		{
			// Don't set input layout on fallback shader (crashes in DX11 NV driver)
			if (!CHWShader_D3D::s_pCurInstVS || CHWShader_D3D::s_pCurInstVS->m_bFallback)
				return E_FAIL;

			m_pLastVDeclaration = nullptr;
			m_DevMan.BindVtxDecl(nullptr);
		}

		return S_OK;
	}

	D3DVertexDeclaration* pD3DDecl = !CHWShader_D3D::s_pCurInstVS ? nullptr :
		CDeviceObjectFactory::LookupInputLayout(
		CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&CHWShader_D3D::s_pCurInstVS->m_Shader, StreamMask, 0, 0, nullptr, eVF)).second;

	if (m_pLastVDeclaration != pD3DDecl)
	{
		// Don't set input layout on fallback shader (crashes in DX11 NV driver)
		if (!CHWShader_D3D::s_pCurInstVS || CHWShader_D3D::s_pCurInstVS->m_bFallback)
			return E_FAIL;

		m_pLastVDeclaration = pD3DDecl;
		m_DevMan.BindVtxDecl(pD3DDecl);
	}

	m_pLastVDeclaration = pD3DDecl;

	return pD3DDecl ? S_OK : E_FAIL;
}

// Clear buffers (color, depth/stencil)
void CD3D9Renderer::EF_ClearTargetsImmediately(uint32 nFlags)
{
	nFlags |= FRT_CLEAR_IMMEDIATE;

	EF_ClearTargetsLater(nFlags);

	if (nFlags & FRT_CLEAR_IMMEDIATE)
	{
		FX_SetActiveRenderTargets();
	}
}

void CD3D9Renderer::EF_ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors, float fDepth, uint8 nStencil)
{
	nFlags |= FRT_CLEAR_IMMEDIATE;

	EF_ClearTargetsLater(nFlags, Colors, fDepth, nStencil);

	if (nFlags & FRT_CLEAR_IMMEDIATE)
	{
		FX_SetActiveRenderTargets();
	}
}

void CD3D9Renderer::EF_ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors)
{
	nFlags |= FRT_CLEAR_IMMEDIATE;

	EF_ClearTargetsLater(nFlags, Colors);

	if (nFlags & FRT_CLEAR_IMMEDIATE)
	{
		FX_SetActiveRenderTargets();
	}
}

void CD3D9Renderer::EF_ClearTargetsImmediately(uint32 nFlags, float fDepth, uint8 nStencil)
{
	nFlags |= FRT_CLEAR_IMMEDIATE;

	EF_ClearTargetsLater(nFlags, fDepth, nStencil);

	if (nFlags & FRT_CLEAR_IMMEDIATE)
	{
		FX_SetActiveRenderTargets();
	}
}

// Clear buffers (color, depth/stencil)
void CD3D9Renderer::EF_ClearTargetsLater(uint32 nFlags, const ColorF& Colors, float fDepth, uint8 nStencil)
{
	if (nFlags & FRT_CLEAR_FOGCOLOR)
	{
		for (int i = 0; i < RT_STACK_WIDTH; ++i)
		{
			if (m_pNewTarget[i])
			{
				m_pNewTarget[i]->m_ReqColor = m_cClearColor;
			}
		}
	}
	else if (nFlags & FRT_CLEAR_COLOR)
	{
		for (int i = 0; i < RT_STACK_WIDTH; ++i)
		{
			if (m_pNewTarget[i] && m_pNewTarget[i]->m_pTarget)
			{
				m_pNewTarget[i]->m_ReqColor = Colors;
			}
		}
	}

	if (nFlags & FRT_CLEAR_DEPTH)
	{
		m_pNewTarget[0]->m_fReqDepth = fDepth;
	}

	if (!(nFlags & FRT_CLEAR_IMMEDIATE))
	{
		m_pNewTarget[0]->m_ClearFlags = 0;
	}
	if ((nFlags & FRT_CLEAR_DEPTH) && m_pNewTarget[0]->m_pDepth)
	{
		m_pNewTarget[0]->m_ClearFlags |= CLEAR_ZBUFFER;
	}
	if (nFlags & FRT_CLEAR_COLOR)
	{
		m_pNewTarget[0]->m_ClearFlags |= CLEAR_RTARGET;
	}
	if (nFlags & FRT_CLEAR_COLORMASK)
	{
		m_pNewTarget[0]->m_ClearFlags |= FRT_CLEAR_COLORMASK;
	}

	if (m_sbpp && (nFlags & FRT_CLEAR_STENCIL) && m_pNewTarget[0]->m_pDepth)
	{
#ifdef SUPPORTS_MSAA
		if (gcpRendD3D->m_RP.m_MSAAData.Type)
		{
			m_RP.m_PersFlags2 |= RBPF2_MSAA_RESTORE_SAMPLE_MASK;
		}
#endif
		m_pNewTarget[0]->m_ClearFlags |= CLEAR_STENCIL;
		m_pNewTarget[0]->m_nReqStencil = nStencil;
	}
}

void CD3D9Renderer::EF_ClearTargetsLater(uint32 nFlags, float fDepth, uint8 nStencil)
{
	if (nFlags & FRT_CLEAR_FOGCOLOR)
	{
		for (int i = 0; i < RT_STACK_WIDTH; ++i)
		{
			if (m_pNewTarget[i])
			{
				m_pNewTarget[i]->m_ReqColor = m_cClearColor;
			}
		}
	}
	else if (nFlags & FRT_CLEAR_COLOR)
	{
		for (int i = 0; i < RT_STACK_WIDTH; ++i)
		{
			if (m_pNewTarget[i] && m_pNewTarget[i]->m_pTex)
			{
				m_pNewTarget[i]->m_ReqColor = m_pNewTarget[i]->m_pTex->GetClearColor();
			}
		}
	}

	if (nFlags & FRT_CLEAR_DEPTH)
	{
		m_pNewTarget[0]->m_fReqDepth = fDepth;
	}

	if (!(nFlags & FRT_CLEAR_IMMEDIATE))
	{
		m_pNewTarget[0]->m_ClearFlags = 0;
	}
	if ((nFlags & FRT_CLEAR_DEPTH) && m_pNewTarget[0]->m_pDepth)
	{
		m_pNewTarget[0]->m_ClearFlags |= CLEAR_ZBUFFER;
	}
	if (nFlags & FRT_CLEAR_COLOR)
	{
		m_pNewTarget[0]->m_ClearFlags |= CLEAR_RTARGET;
	}
	if (nFlags & FRT_CLEAR_COLORMASK)
	{
		m_pNewTarget[0]->m_ClearFlags |= FRT_CLEAR_COLORMASK;
	}

	if (m_sbpp && (nFlags & FRT_CLEAR_STENCIL) && m_pNewTarget[0]->m_pDepth)
	{
#ifdef SUPPORTS_MSAA
		if (gcpRendD3D->m_RP.m_MSAAData.Type)
		{
			m_RP.m_PersFlags2 |= RBPF2_MSAA_RESTORE_SAMPLE_MASK;
		}
#endif
		m_pNewTarget[0]->m_ClearFlags |= CLEAR_STENCIL;
		m_pNewTarget[0]->m_nReqStencil = nStencil;
	}
}

void CD3D9Renderer::EF_ClearTargetsLater(uint32 nFlags, const ColorF& Colors)
{
	EF_ClearTargetsLater(nFlags, Colors, Clr_FarPlane.r, 0);
	//		float(m_pNewTarget[0]->m_pSurfDepth->pTex->GetClearColor().r),
	//		uint8(m_pNewTarget[0]->m_pSurfDepth->pTex->GetClearColor().g));
}

void CD3D9Renderer::EF_ClearTargetsLater(uint32 nFlags)
{
	EF_ClearTargetsLater(nFlags, Clr_FarPlane.r, 0);
	//		float(m_pNewTarget[0]->m_pSurfDepth->pTex->GetClearColor().r),
	//		uint8(m_pNewTarget[0]->m_pSurfDepth->pTex->GetClearColor().g));
}

void CD3D9Renderer::FX_SetActiveRenderTargets()
{
	DETAILED_PROFILE_MARKER("FX_SetActiveRenderTargets");
	if (m_RP.m_PersFlags1 & RBPF1_IN_CLEAR)
		return;
	FUNCTION_PROFILER_RENDER_FLAT
	HRESULT hr  = S_OK;
	bool bDirty = false;
	if (m_nMaxRT2Commit >= 0)
	{
		for (int i = 0; i <= m_nMaxRT2Commit; i++)
		{
			if (!m_pNewTarget[i]->m_bWasSetRT)
			{
				m_pNewTarget[i]->m_bWasSetRT = true;
				if (m_pNewTarget[i]->m_pTex)
					m_pNewTarget[i]->m_pTex->SetResolved(false);
				m_pCurTarget[i] = m_pNewTarget[i]->m_pTex;
				bDirty          = true;
#ifndef _RELEASE
				if (m_LogFile)
				{
					Logv(" +++ Set RT");
					if (m_pNewTarget[i]->m_pTex)
					{
						Logv(" '%s'", m_pNewTarget[i]->m_pTex->GetName());
						Logv(" Format:%s", CTexture::NameForTextureFormat(m_pNewTarget[i]->m_pTex->m_eDstFormat));
						Logv(" Type:%s", CTexture::NameForTextureType(m_pNewTarget[i]->m_pTex->m_eTT));
						Logv(" W/H:%d:%d\n", m_pNewTarget[i]->m_pTex->GetWidth(), m_pNewTarget[i]->m_pTex->GetHeight());

					}
					else
					{
						Logv(" 'Unknown'\n");
					}
				}
#endif
				CTexture* pRT = m_pNewTarget[i]->m_pTex;
				if (pRT && pRT->UseMultisampledRTV())
					pRT->Unbind();
			}
		}
		if (!m_pNewTarget[0]->m_bWasSetD)
		{
			m_pNewTarget[0]->m_bWasSetD = true;
			bDirty = true;
		}
		//m_nMaxRT2Commit = -1;
	}

	if (bDirty)
	{
		CTexture* pRT = m_pNewTarget[0]->m_pTex;
		if (pRT && pRT->UseMultisampledRTV())
		{
			// Reset all texture slots which are used as RT currently
			D3DShaderResource* pRes = NULL;
			for (int i = 0; i < MAX_TMU; i++)
			{
				if (CTexture::s_TexStages[i].m_DevTexture == pRT->GetDevTexture())
				{
					m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_PS, pRes, i);
					CTexture::s_TexStages[i].m_DevTexture = NULL;
				}
			}
		}

		const uint32 nMaxRT2Commit = max(m_nMaxRT2Commit + 1, 0);
		
		uint32 nNumViews                 = 0;
		D3DSurface* pRTV[RT_STACK_WIDTH] = { NULL };
		D3DDepthSurface* pDSV            = NULL;

		for (uint32 v = 0; v < nMaxRT2Commit; ++v)
		{
			if (m_pNewTarget[v] && m_pNewTarget[v]->m_pTarget)
			{
				pRTV[v]   = m_pNewTarget[v]->m_pTarget;
				nNumViews = v + 1;
			}
		}

		// NOTE: bottom of the stack is always the active swap-chain back-buffer
		if (m_nRTStackLevel[0] == 0)
		{
			assert(m_pNewTarget[0]->m_pTarget == (D3DSurface*)0xDEADBEEF);
			assert(m_pNewTarget[0]->m_pDepth  == (D3DDepthSurface*)0xDEADBEEF);
			assert(m_pNewTarget[1]->m_pTarget == nullptr);

			assert(nNumViews == 1);
			// Get the current output, can be swap-chain, can also be left/right etc.
			pRTV[0] = GetCurrentTargetOutput()->GetDevTexture()->LookupRTV(EDefaultResourceViews::RenderTarget);
			pDSV    = GetCurrentDepthOutput()->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil);

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			GetDeviceContext().OMSetRenderTargets(1, pRTV, !m_pActiveContext || m_pActiveContext->m_bMainViewport ? pDSV : nullptr);
#endif
		}
		else
		{
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			GetDeviceContext().OMSetRenderTargets(m_pNewTarget[0]->m_pTarget == NULL ? 0 : nNumViews, pRTV, m_pNewTarget[0]->m_pDepth);
#endif
		}
	}

	if (m_nMaxRT2Commit >= 0)
		m_nMaxRT2Commit = -1;

	FX_SetViewport();
	FX_ClearTargets();
}

void CD3D9Renderer::FX_SetViewport()
{
	// Set current viewport
	if (m_bViewportDirty)
	{
		m_bViewportDirty = false;
		if ((m_CurViewport != m_NewViewport))
		{
			m_CurViewport = m_NewViewport;
			D3DViewPort Port;
			Port.Width    = (FLOAT)m_CurViewport.nWidth;
			Port.Height   = (FLOAT)m_CurViewport.nHeight;
			Port.TopLeftX = (FLOAT)m_CurViewport.nX;
			Port.TopLeftY = (FLOAT)m_CurViewport.nY;
			Port.MinDepth = m_CurViewport.fMinZ;
			Port.MaxDepth = m_CurViewport.fMaxZ;

			if (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
				Port = ReverseDepthHelper::Convert(Port);

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			GetDeviceContext().RSSetViewports(1, &Port);
#endif
		}
	}
}

//====================================================================================

void CD3D9Renderer::FX_ClearTarget(D3DSurface* pView, const ColorF& cClear, const uint numRects, const RECT* pRects)
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS) && defined(ENABLE_PROFILING_CODE)
	{
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCleared++;
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTClearedSize += CDeviceTexture::TextureDataSize(pView, numRects, pRects);
	}
#endif

#if (CRY_RENDERER_DIRECT3D >= 120)
	GetDeviceContext().ClearRectsRenderTargetView(
		pView,
		(const FLOAT*)&cClear,
		numRects,
		pRects);
#elif (CRY_RENDERER_DIRECT3D >= 111) && !CRY_PLATFORM_ORBIS
	GetDeviceContext().ClearView(
		pView,
		(const FLOAT*)&cClear,
		numRects,
		pRects);
#elif (CRY_RENDERER_VULKAN >= 10) || CRY_RENDERER_GNM
	CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
	commandList.GetGraphicsInterface()->ClearSurface(pView, cClear, numRects, pRects);
#else
	if (!numRects)
	{
		GetDeviceContext().ClearRenderTargetView(
			pView,
			(const FLOAT*)&cClear);

		return;
	}

	// TODO: implement clears in compute for DX11, gives max performance (pipeline switch cost?)
	__debugbreak();
	abort();
#endif
}

void CD3D9Renderer::FX_ClearTarget(CTexture* pTex, const ColorF& cClear, const uint numRects, const RECT* pRects, const bool bOptionalRects)
{
#if !defined(RELEASE) && defined(_DEBUG) && 0     // will break a lot currently, won't really be coherent in the future
	if (pTex->GetClearColor() != cClear)
	{
		const char* szName = pTex->GetName();
		fprintf(stderr, "%s has wrong clear value\n", szName);
	}
#endif

	// TODO: should not happen, happens in the editor currently
	D3DSurface* pView = pTex->GetSurface(-1, 0);

#if (CRY_RENDERER_DIRECT3D >= 120)
	FX_ClearTarget(
		pView,
		cClear,
		numRects,
		pRects);
#else
	if (bOptionalRects || !numRects)
	{
		FX_ClearTarget(
			pView,
			cClear,
			0U,
			nullptr);

		return;
	}

	GetGraphicsPipeline().SwitchFromLegacyPipeline();
	CClearRegionPass().Execute(pTex, cClear, numRects, pRects);
	GetGraphicsPipeline().SwitchToLegacyPipeline();

#endif
}

void CD3D9Renderer::FX_ClearTarget(CTexture* pTex, const ColorF& cClear)
{
	//	RECT rect = { 0, 0, pTex->GetWidth(), pTex->GetHeight() };

	FX_ClearTarget(
		pTex,
		cClear,
		0U,
		nullptr,
		true);
}

void CD3D9Renderer::FX_ClearTarget(CTexture* pTex)
{
	FX_ClearTarget(
		pTex,
		pTex->GetClearColor());
}

//====================================================================================

void CD3D9Renderer::FX_ClearTarget(D3DDepthSurface* pView, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects, const RECT* pRects)
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS) && defined(ENABLE_PROFILING_CODE)
	if (nFlags)
	{
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCleared++;
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTClearedSize += CDeviceTexture::TextureDataSize(pView, numRects, pRects);
	}
#endif

	assert((
		  (nFlags & CLEAR_ZBUFFER ? D3D11_CLEAR_DEPTH : 0) |
		  (nFlags & CLEAR_STENCIL ? D3D11_CLEAR_STENCIL : 0)
		  ) == nFlags);

#if (CRY_RENDERER_DIRECT3D >= 120)
	GetDeviceContext().ClearRectsDepthStencilView(
		pView,
		nFlags,
		cDepth,
		cStencil,
		numRects,
		pRects);
#elif (CRY_RENDERER_VULKAN >= 10) || CRY_RENDERER_GNM
	CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
	commandList.GetGraphicsInterface()->ClearSurface(pView, nFlags, cDepth, cStencil, numRects, pRects);
#else
	if (!numRects)
	{
		GetDeviceContext().ClearDepthStencilView(
			pView,
			nFlags,
			cDepth,
			cStencil);

		return;
	}

	// TODO: implement clears in compute for DX11, gives max performance (pipeline switch cost?)
	__debugbreak();
	abort();
#endif
}

void CD3D9Renderer::FX_ClearTarget(SDepthTexture* pTex, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects, const RECT* pRects, const bool bOptionalRects)
{
	assert((
		  (nFlags & CLEAR_ZBUFFER ? D3D11_CLEAR_DEPTH : 0) |
		  (nFlags & CLEAR_STENCIL ? D3D11_CLEAR_STENCIL : 0)
		  ) == nFlags);
	assert((
		  (nFlags & CLEAR_ZBUFFER ? FRT_CLEAR_DEPTH : 0) |
		  (nFlags & CLEAR_STENCIL ? FRT_CLEAR_STENCIL : 0)
		  ) == nFlags);

#if (CRY_RENDERER_DIRECT3D >= 120)
	FX_ClearTarget(
	  pTex->pSurface,
		nFlags,
		cDepth,
		cStencil,
		numRects,
		pRects);
#else
	if (bOptionalRects || !numRects)
	{
		FX_ClearTarget(
		  pTex->pSurface,
			nFlags,
			cDepth,
			cStencil,
			0U,
			nullptr);

		return;
	}

	CRY_ASSERT(pTex->pTexture);
	GetGraphicsPipeline().SwitchFromLegacyPipeline();
	CClearRegionPass().Execute(pTex->pTexture, nFlags, cDepth, cStencil, numRects, pRects);
	GetGraphicsPipeline().SwitchToLegacyPipeline();
#endif
}

void CD3D9Renderer::FX_ClearTarget(SDepthTexture* pTex, const int nFlags, const float cDepth, const uint8 cStencil)
{
	FX_ClearTarget(
		pTex,
		nFlags,
		cDepth,
		cStencil,
		0U,
		nullptr,
		true);
}

void CD3D9Renderer::FX_ClearTarget(SDepthTexture* pTex, const int nFlags)
{
	FX_ClearTarget(
		pTex,
		nFlags,
		(gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) ? 0.f : 1.f,
		0U);
}

void CD3D9Renderer::FX_ClearTarget(SDepthTexture* pTex)
{
	FX_ClearTarget(
		pTex,
		CLEAR_ZBUFFER |
		CLEAR_STENCIL);
}

//====================================================================================

void CD3D9Renderer::FX_ClearTargets()
{
	if (m_pNewTarget[0]->m_ClearFlags)
	{
		{
			CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();

			const float fClearDepth   = (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) ? 1.0f - m_pNewTarget[0]->m_fReqDepth : m_pNewTarget[0]->m_fReqDepth;
			const uint8 nClearStencil = m_pNewTarget[0]->m_nReqStencil;
			const int nFlags          = m_pNewTarget[0]->m_ClearFlags & ~CLEAR_RTARGET;
			
			assert((
				  (nFlags & CLEAR_ZBUFFER ? D3D11_CLEAR_DEPTH : 0) |
				  (nFlags & CLEAR_STENCIL ? D3D11_CLEAR_STENCIL : 0)
				  ) == nFlags);

			// NOTE: bottom of the stack is always the active swap-chain back-buffer
			if (m_nRTStackLevel[0] == 0)
			{
				assert(m_pNewTarget[0]->m_pTarget == (D3DSurface*)0xDEADBEEF);
				assert(m_pNewTarget[0]->m_pDepth  == (D3DDepthSurface*)0xDEADBEEF);
				assert(m_pNewTarget[1]->m_pTarget == nullptr);

				// NOTE: optimal value is "black"
				commandList.GetGraphicsInterface()->ClearSurface(GetCurrentTargetOutput()->GetDevTexture()->LookupRTV(EDefaultResourceViews::RenderTarget), m_pNewTarget[0]->m_ReqColor);

				if (nFlags)
				{
					commandList.GetGraphicsInterface()->ClearSurface(GetCurrentDepthOutput()->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil), nFlags, fClearDepth, nClearStencil);
				}
			}
			else
			{
				// TODO: ClearFlags per render-target
				if ((m_pNewTarget[0]->m_pTarget != NULL) && (m_pNewTarget[0]->m_ClearFlags & CLEAR_RTARGET))
				{
					for (int i = 0; i < RT_STACK_WIDTH; ++i)
					{
						if (m_pNewTarget[i]->m_pTarget)
						{
							// NOTE: optimal value is "m_pNewTarget[0]->m_pTex->GetClearColor()"
							commandList.GetGraphicsInterface()->ClearSurface(m_pNewTarget[i]->m_pTarget, m_pNewTarget[i]->m_ReqColor);
						}
					}
				}

				if ((m_pNewTarget[0]->m_pDepth != NULL) && (nFlags))
				{
					commandList.GetGraphicsInterface()->ClearSurface(m_pNewTarget[0]->m_pDepth, nFlags, fClearDepth, nClearStencil);
				}
			}
		}

		CTexture* pRT = m_pNewTarget[0]->m_pTex;
		if (CV_r_stats == 13)
		{
			EF_AddRTStat(pRT, m_pNewTarget[0]->m_ClearFlags, m_CurViewport.nWidth, m_CurViewport.nHeight);
		}

#if !defined(EXCLUDE_RARELY_USED_R_STATS) && defined(ENABLE_PROFILING_CODE)
		{
			// NOTE: bottom of the stack is always the active swap-chain back-buffer
			if (m_nRTStackLevel[0] == 0)
			{
				assert(m_pNewTarget[0]->m_pTarget == (D3DSurface*)0xDEADBEEF);
				assert(m_pNewTarget[0]->m_pDepth  == (D3DDepthSurface*)0xDEADBEEF);
				assert(m_pNewTarget[1]->m_pTarget == nullptr);

				m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCleared++;
				m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTClearedSize += CDeviceTexture::TextureDataSize(GetCurrentTargetOutput()->GetDevTexture()->LookupRTV(EDefaultResourceViews::RenderTarget));

				if (m_pNewTarget[0]->m_ClearFlags & (~CLEAR_RTARGET))
				{
					m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCleared++;
					m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTClearedSize += CDeviceTexture::TextureDataSize(GetCurrentDepthOutput()->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil));
				}
			}
			else
			{
				if ((m_pNewTarget[0]->m_pTarget != NULL) && (m_pNewTarget[0]->m_ClearFlags & CLEAR_RTARGET))
				{
					for (int i = 0; i < RT_STACK_WIDTH; ++i)
					{
						if (m_pNewTarget[i]->m_pTarget)
						{
							m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCleared++;
							m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTClearedSize += CDeviceTexture::TextureDataSize(m_pNewTarget[i]->m_pTarget);
						}
					}
				}

				if (m_pNewTarget[0]->m_ClearFlags & (~CLEAR_RTARGET))
				{
					m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCleared++;
					m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTClearedSize += CDeviceTexture::TextureDataSize(m_pNewTarget[0]->m_pSurfDepth->pSurface);
				}
			}
		}
#endif

		m_pNewTarget[0]->m_ClearFlags = 0;
	}
}

void CD3D9Renderer::FX_Commit()
{
	DETAILED_PROFILE_MARKER("FX_Commit");

	// Commit all changed shader parameters
	if (m_RP.m_nCommitFlags & FC_GLOBAL_PARAMS)
	{
		CHWShader_D3D::mfCommitParamsGlobal();
		m_RP.m_nCommitFlags &= ~FC_GLOBAL_PARAMS;
	}

	if (m_RP.m_nCommitFlags & FC_MATERIAL_PARAMS)
	{
		CHWShader_D3D::mfCommitParamsMaterial();
		m_RP.m_nCommitFlags &= ~FC_MATERIAL_PARAMS;
	}

	CHWShader_D3D::mfCommitParams();

	// Commit all changed RT's
	if (m_RP.m_nCommitFlags & FC_TARGETS)
	{
		FX_SetActiveRenderTargets();
		m_RP.m_nCommitFlags &= ~FC_TARGETS;
	}

	// Adapt viewport dimensions if changed
	FX_SetViewport();

	// Clear render-targets if requested
	FX_ClearTargets();
}

// Set current geometry culling modes
void CD3D9Renderer::D3DSetCull(ECull eCull, bool bSkipMirrorCull)
{
	FUNCTION_PROFILER_RENDER_FLAT
	if (eCull != eCULL_None && !bSkipMirrorCull)
	{
		if (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_MIRRORCULL)
			eCull = (eCull == eCULL_Back) ? eCULL_Front : eCULL_Back;
	}

	if (eCull == m_RP.m_eCull)
		return;

	SStateRaster RS = m_StatesRS[m_nCurStateRS];

	RS.Desc.FrontCounterClockwise = true;

	if (eCull == eCULL_None)
		RS.Desc.CullMode = D3D11_CULL_NONE;
	else
	{
		if (eCull == eCULL_Back)
		{
			RS.Desc.CullMode = D3D11_CULL_BACK;
		}
		else
		{
			RS.Desc.CullMode = D3D11_CULL_FRONT;
		}
	}
	SetRasterState(&RS);
	m_RP.m_eCull = eCull;
}

uint8 g_StencilFuncLookup[8] =
{
	D3D11_COMPARISON_ALWAYS,        // FSS_STENCFUNC_ALWAYS   0x0
	D3D11_COMPARISON_NEVER,         // FSS_STENCFUNC_NEVER    0x1
	D3D11_COMPARISON_LESS,          // FSS_STENCFUNC_LESS     0x2
	D3D11_COMPARISON_LESS_EQUAL,    // FSS_STENCFUNC_LEQUAL   0x3
	D3D11_COMPARISON_GREATER,       // FSS_STENCFUNC_GREATER  0x4
	D3D11_COMPARISON_GREATER_EQUAL, // FSS_STENCFUNC_GEQUAL   0x5
	D3D11_COMPARISON_EQUAL,         // FSS_STENCFUNC_EQUAL    0x6
	D3D11_COMPARISON_NOT_EQUAL      // FSS_STENCFUNC_NOTEQUAL 0x7
};

uint8 g_StencilOpLookup[8] =
{
	D3D11_STENCIL_OP_KEEP,          // FSS_STENCOP_KEEP    0x0
	D3D11_STENCIL_OP_REPLACE,       // FSS_STENCOP_REPLACE 0x1
	D3D11_STENCIL_OP_INCR_SAT,      // FSS_STENCOP_INCR    0x2
	D3D11_STENCIL_OP_DECR_SAT,      // FSS_STENCOP_DECR    0x3
	D3D11_STENCIL_OP_ZERO,          // FSS_STENCOP_ZERO    0x4
	D3D11_STENCIL_OP_INCR,          // FSS_STENCOP_INCR_WRAP 0x5
	D3D11_STENCIL_OP_DECR,          // FSS_STENCOP_DECR_WRAP 0x6
	D3D11_STENCIL_OP_INVERT         // FSS_STENCOP_INVERT 0x7
};

void CRenderer::FX_SetStencilState(int st, uint32 nStencRef, uint32 nStencMask, uint32 nStencWriteMask, bool bForceFullReadMask)
{
	FUNCTION_PROFILER_RENDER_FLAT

	  PrefetchLine(g_StencilFuncLookup, 0);

	const uint32 nPersFlags2 = m_RP.m_PersFlags2;
	if (!bForceFullReadMask && !(nPersFlags2 & RBPF2_READMASK_RESERVED_STENCIL_BIT))
	{
		nStencMask &= ~BIT_STENCIL_RESERVED;
	}

	if (nPersFlags2 & RBPF2_WRITEMASK_RESERVED_STENCIL_BIT)
		nStencWriteMask &= ~BIT_STENCIL_RESERVED;

	nStencRef |= m_RP.m_CurStencilRefAndMask;

	SStateDepth DS = gcpRendD3D->m_StatesDP[gcpRendD3D->m_nCurStateDP];
	DS.Desc.StencilReadMask  = nStencMask;
	DS.Desc.StencilWriteMask = nStencWriteMask;

	int nCurFunc = st & FSS_STENCFUNC_MASK;
	DS.Desc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)g_StencilFuncLookup[nCurFunc];

	int nCurOp = (st & FSS_STENCFAIL_MASK) >> FSS_STENCFAIL_SHIFT;
	DS.Desc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[nCurOp];

	nCurOp = (st & FSS_STENCZFAIL_MASK) >> FSS_STENCZFAIL_SHIFT;
	DS.Desc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[nCurOp];

	nCurOp = (st & FSS_STENCPASS_MASK) >> FSS_STENCPASS_SHIFT;
	DS.Desc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)g_StencilOpLookup[nCurOp];

	if (!(st & FSS_STENCIL_TWOSIDED))
	{
		DS.Desc.BackFace = DS.Desc.FrontFace;
	}
	else
	{
		nCurFunc = (st & (FSS_STENCFUNC_MASK << FSS_CCW_SHIFT)) >> FSS_CCW_SHIFT;
		DS.Desc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)g_StencilFuncLookup[nCurFunc];

		nCurOp = (st & (FSS_STENCFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCFAIL_SHIFT + FSS_CCW_SHIFT);
		DS.Desc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[nCurOp];

		nCurOp = (st & (FSS_STENCZFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCZFAIL_SHIFT + FSS_CCW_SHIFT);
		DS.Desc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[nCurOp];

		nCurOp = (st & (FSS_STENCPASS_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCPASS_SHIFT + FSS_CCW_SHIFT);
		DS.Desc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)g_StencilOpLookup[nCurOp];
	}

	m_RP.m_CurStencRef       = nStencRef;
	m_RP.m_CurStencMask      = nStencMask;
	m_RP.m_CurStencWriteMask = nStencWriteMask;

	gcpRendD3D->SetDepthState(&DS, nStencRef);

	m_RP.m_CurStencilState = st;
}

void CD3D9Renderer::EF_Scissor(bool bEnable, int sX, int sY, int sWdt, int sHgt)
{
	m_pRT->RC_SetScissor(bEnable, sX, sY, sWdt, sHgt);
}

void CD3D9Renderer::RT_SetScissor(bool bEnable, int sX, int sY, int sWdt, int sHgt)
{
	FUNCTION_PROFILER_RENDER_FLAT
	if (!CV_r_scissor)
		return;
	D3D11_RECT scRect;
	if (bEnable)
	{
		if (sX != m_scissorPrevX || sY != m_scissorPrevY || sWdt != m_scissorPrevWidth || sHgt != m_scissorPrevHeight)
		{
			m_scissorPrevX      = sX;
			m_scissorPrevY      = sY;
			m_scissorPrevWidth  = sWdt;
			m_scissorPrevHeight = sHgt;
			scRect.left         = sX;
			scRect.top          = sY;
			scRect.right        = sX + sWdt;
			scRect.bottom       = sY + sHgt;

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			GetDeviceContext().RSSetScissorRects(1, &scRect);
#endif
		}
		if (bEnable != m_bScissorPrev)
		{
			m_bScissorPrev = bEnable;
			SStateRaster RS = m_StatesRS[m_nCurStateRS];
			RS.Desc.ScissorEnable = bEnable;
			SetRasterState(&RS);
		}
	}
	else
	{
		if (bEnable != m_bScissorPrev)
		{
			m_bScissorPrev      = bEnable;
			m_scissorPrevWidth  = 0;
			m_scissorPrevHeight = 0;
			SStateRaster RS = m_StatesRS[m_nCurStateRS];
			RS.Desc.ScissorEnable = bEnable;
			SetRasterState(&RS);
		}
	}
}

bool CD3D9Renderer::EF_GetScissorState(int& sX, int& sY, int& sWdt, int& sHgt)
{
	if (!CV_r_scissor)
		return false;

	sX   = m_scissorPrevX;
	sY   = m_scissorPrevY;
	sWdt = m_scissorPrevWidth;
	sHgt = m_scissorPrevHeight;
	return m_bScissorPrev;
}

void CD3D9Renderer::FX_FogCorrection()
{
	if (m_RP.m_nPassGroupID <= EFSLIST_DECAL)
	{
		uint32 uBlendFlags = m_RP.m_CurState & GS_BLEND_MASK;
		switch (uBlendFlags)
		{
		case GS_BLSRC_ONE | GS_BLDST_ONE:
			EF_SetFogColor(Col_Black);
			break;
		case GS_BLSRC_DSTALPHA | GS_BLDST_ONE:
			EF_SetFogColor(Col_Black);
			break;
		case GS_BLSRC_DSTCOL | GS_BLDST_SRCCOL:
		{
			static ColorF pColGrey = ColorF(0.5f, 0.5f, 0.5f, 1.0f);
			EF_SetFogColor(pColGrey);
			break;
		}
		case GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA:
			EF_SetFogColor(Col_Black);
			break;
		case GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL:
			EF_SetFogColor(Col_Black);
			break;
		case GS_BLSRC_ZERO | GS_BLDST_ONEMINUSSRCCOL:
			EF_SetFogColor(Col_Black);
			break;
		case GS_BLSRC_SRCALPHA | GS_BLDST_ONE:
		case GS_BLSRC_SRCALPHA_A_ZERO | GS_BLDST_ONE:
			EF_SetFogColor(Col_Black);
			break;
		case GS_BLSRC_ZERO | GS_BLDST_ONE:
			EF_SetFogColor(Col_Black);
			break;
		case GS_BLSRC_DSTCOL | GS_BLDST_ZERO:
			EF_SetFogColor(Col_White);
			break;
		default:
			EF_SetFogColor(m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS.m_FogColor);
			break;
		}
	}
	else
	{
		EF_SetFogColor(m_RP.m_TI[m_RP.m_nProcessThreadID].m_FS.m_FogColor);
	}
}

// Set current render states
void CD3D9Renderer::FX_SetState(int st, int AlphaRef, int RestoreState)
{
	FUNCTION_PROFILER_RENDER_FLAT
	int Changed;
	st      |= m_RP.m_StateOr;
	st      &= m_RP.m_StateAnd;
	Changed  = st ^ m_RP.m_CurState;
	Changed |= RestoreState;

	if (!Changed && (AlphaRef == -1 || AlphaRef == m_RP.m_CurAlphaRef))
		return;

	//PROFILE_FRAME(State_RStates);

#ifndef _RELEASE
	m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumStateChanges++;
#endif
	if (m_StatesBL.size() == 0 || m_StatesDP.size() == 0 || m_StatesRS.size() == 0)
		ResetToDefault();
	SStateDepth  DS = m_StatesDP[m_nCurStateDP];
	SStateBlend  BS = m_StatesBL[m_nCurStateBL];
	SStateRaster RS = m_StatesRS[m_nCurStateRS];
	bool bDirtyDS   = false;
	bool bDirtyBS   = false;
	bool bDirtyRS   = false;

	if (Changed & GS_DEPTHFUNC_MASK)
	{
		bDirtyDS = true;

		uint32 nDepthState = st;
		if (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
			nDepthState = ReverseDepthHelper::ConvertDepthFunc(st);

		switch (nDepthState & GS_DEPTHFUNC_MASK)
		{
		case GS_DEPTHFUNC_EQUAL:
			DS.Desc.DepthFunc = D3D11_COMPARISON_EQUAL;
			break;
		case GS_DEPTHFUNC_LEQUAL:
			DS.Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			break;
		case GS_DEPTHFUNC_GREAT:
			DS.Desc.DepthFunc = D3D11_COMPARISON_GREATER;
			break;
		case GS_DEPTHFUNC_LESS:
			DS.Desc.DepthFunc = D3D11_COMPARISON_LESS;
			break;
		case GS_DEPTHFUNC_NOTEQUAL:
			DS.Desc.DepthFunc = D3D11_COMPARISON_NOT_EQUAL;
			break;
		case GS_DEPTHFUNC_GEQUAL:
			DS.Desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
			break;
		}
	}
	if (Changed & (GS_WIREFRAME | GS_POINTRENDERING))
	{
		bDirtyRS = true;
		if (st & GS_WIREFRAME)
			RS.Desc.FillMode = D3D11_FILL_WIREFRAME;
		else
			RS.Desc.FillMode = D3D11_FILL_SOLID;
	}

	if (Changed & GS_COLMASK_MASK)
	{
		bDirtyBS = true;
		for (size_t i = 0; i < 4; ++i)
		{
			uint32 nMask = 0xfffffff0 | (ColorMasks[(st & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT][i]);
			nMask = (~nMask) & 0xf;
			BS.Desc.RenderTarget[i].RenderTargetWriteMask = nMask;
		}
		BS.Desc.IndependentBlendEnable = true;
	}

	if (Changed & GS_BLEND_MASK)
	{
		bDirtyBS = true;
		if (st & GS_BLEND_MASK)
		{
			if (CV_r_measureoverdraw && (m_RP.m_nRendFlags & SHDF_ALLOWHDR))
			{
				st  = (st & ~GS_BLEND_MASK) | (GS_BLSRC_ONE | GS_BLDST_ONE);
        		st &= ~GS_ALPHATEST;
			}

			// todo: add separate alpha blend support for mrt
			for (size_t i = 0; i < RT_STACK_WIDTH; ++i)
				BS.Desc.RenderTarget[i].BlendEnable = TRUE;

			// Source factor
			switch (st & GS_BLSRC_MASK)
			{
			case GS_BLSRC_ZERO:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_ZERO;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
				break;
			case GS_BLSRC_ONE:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_ONE;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
				break;
			case GS_BLSRC_DSTCOL:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_DEST_COLOR;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
				break;
			case GS_BLSRC_ONEMINUSDSTCOL:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_INV_DEST_COLOR;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
				break;
			case GS_BLSRC_SRCALPHA:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_SRC_ALPHA;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				break;
			case GS_BLSRC_ONEMINUSSRCALPHA:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_INV_SRC_ALPHA;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				break;
			case GS_BLSRC_DSTALPHA:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_DEST_ALPHA;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
				break;
			case GS_BLSRC_ONEMINUSDSTALPHA:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_INV_DEST_ALPHA;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
				break;
			case GS_BLSRC_ALPHASATURATE:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_SRC_ALPHA_SAT;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA_SAT;
				break;
			case GS_BLSRC_SRCALPHA_A_ZERO:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_SRC_ALPHA;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
				break;
			case GS_BLSRC_SRC1ALPHA:
				BS.Desc.RenderTarget[0].SrcBlend      = D3D11_BLEND_SRC1_ALPHA;
				BS.Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC1_ALPHA;
				break;
			default:
				iLog->Log("CD3D9Renderer::SetState: invalid src blend state bits '%d'", st & GS_BLSRC_MASK);
				break;
			}

			//Destination factor
			switch (st & GS_BLDST_MASK)
			{
			case GS_BLDST_ZERO:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_ZERO;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
				break;
			case GS_BLDST_ONE:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				break;
			case GS_BLDST_SRCCOL:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_SRC_COLOR;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				break;
			case GS_BLDST_ONEMINUSSRCCOL:
				if (m_nHDRType == 1 && (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_HDR))
				{
					BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
					BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				}
				else
				{
					BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_COLOR;
					BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				}
				break;
			case GS_BLDST_SRCALPHA:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_SRC_ALPHA;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				break;
			case GS_BLDST_ONEMINUSSRCALPHA:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				break;
			case GS_BLDST_DSTALPHA:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_DEST_ALPHA;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
				break;
			case GS_BLDST_ONEMINUSDSTALPHA:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_DEST_ALPHA;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
				break;
			case GS_BLDST_ONE_A_ZERO:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
				break;
			case GS_BLDST_ONEMINUSSRC1ALPHA:
				BS.Desc.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC1_ALPHA;
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
				break;
			default:
				iLog->Log("CD3D9Renderer::SetState: invalid dst blend state bits '%d'", st & GS_BLDST_MASK);
				break;
			}

			//Blending operation
			D3D11_BLEND_OP blendOperation = D3D11_BLEND_OP_ADD;
			switch (st & GS_BLEND_OP_MASK)
			{
			case GS_BLEND_OP_MAX:
				blendOperation = D3D11_BLEND_OP_MAX;
				break;
			case GS_BLEND_OP_MIN:
				blendOperation = D3D11_BLEND_OP_MIN;
				break;
			case GS_BLEND_OP_SUB:
				blendOperation = D3D11_BLEND_OP_SUBTRACT;
				break;
			case GS_BLEND_OP_SUBREV:
				blendOperation = D3D11_BLEND_OP_REV_SUBTRACT;
				break;
			}

			//Separate blend modes for alpha
			D3D11_BLEND_OP blendOperationAlpha = blendOperation;
			switch (st & GS_BLALPHA_MASK)
			{
			case GS_BLALPHA_MAX:
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				BS.Desc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
				blendOperationAlpha = D3D11_BLEND_OP_MAX;
				break;
			case GS_BLALPHA_MIN:
				BS.Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				BS.Desc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
				blendOperationAlpha = D3D11_BLEND_OP_MIN;
				break;
			}

			// todo: add separate alpha blend support for mrt
			for (size_t i = 0; i < RT_STACK_WIDTH; ++i)
			{
				BS.Desc.RenderTarget[i].BlendOp      = blendOperation;
				BS.Desc.RenderTarget[i].BlendOpAlpha = blendOperationAlpha;
			}
		}
		else
		{
			// todo: add separate alpha blend support for mrt
			for (size_t i = 0; i < RT_STACK_WIDTH; ++i)
				BS.Desc.RenderTarget[i].BlendEnable = FALSE;
		}

		for (size_t i = 0; i < RT_STACK_WIDTH; ++i)
			BS.Desc.RenderTarget[i] = BS.Desc.RenderTarget[0];
	}

	if (Changed & GS_DEPTHWRITE)
	{
		bDirtyDS = true;
		if (st & GS_DEPTHWRITE)
			DS.Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		else
			DS.Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	}

	if (Changed & GS_NODEPTHTEST)
	{
		CRY_ASSERT((st & (GS_NODEPTHTEST | GS_DEPTHWRITE)) != (GS_NODEPTHTEST | GS_DEPTHWRITE)); // new graphics pipeline treats this case differently

		bDirtyDS = true;
		if (st & GS_NODEPTHTEST)
			DS.Desc.DepthEnable = FALSE;
		else
			DS.Desc.DepthEnable = TRUE;
	}

	if (Changed & GS_STENCIL)
	{
		bDirtyDS = true;
		if (st & GS_STENCIL)
			DS.Desc.StencilEnable = TRUE;
		else
			DS.Desc.StencilEnable = FALSE;
	}

	{
		// Alpha test must be handled in shader in D3D10 API
		if (((st ^ m_RP.m_CurState) & GS_ALPHATEST) || ((st & GS_ALPHATEST) && (m_RP.m_CurAlphaRef != AlphaRef && AlphaRef != -1)))
		{
			if (st & GS_ALPHATEST)
				m_RP.m_CurAlphaRef = AlphaRef;
		}
	}

	if (bDirtyDS)
		SetDepthState(&DS, m_nCurStencRef);
	if (bDirtyRS)
		SetRasterState(&RS);
	if (bDirtyBS)
		SetBlendState(&BS);

	m_RP.m_CurState = st;
}

void CD3D9Renderer::FX_ZState(uint32& nState)
{
	int nNewState = nState;
	assert(m_RP.m_pRootTechnique);    // cannot be 0 here

	if ((m_RP.m_pRootTechnique->m_Flags & (FHF_WASZWRITE | FHF_POSITION_INVARIANT))
	  && m_RP.m_nPassGroupID == EFSLIST_GENERAL
	  && (!IsRecursiveRenderView())
	  && CV_r_usezpass)
	{
		if ((m_RP.m_nBatchFilter & (FB_GENERAL | FB_MULTILAYERS)) && (m_RP.m_nRendFlags & (SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS)))
		{
			if (!(m_RP.m_pRootTechnique->m_Flags & FHF_POSITION_INVARIANT))
			{
					nNewState |= GS_DEPTHFUNC_EQUAL;
				}
			nNewState &= ~(GS_DEPTHWRITE | GS_ALPHATEST);
		}

		nState = nNewState;
	}
}

void CD3D9Renderer::FX_HairState(uint32& nState, const SShaderPass* pPass)
{
	if ((m_RP.m_nPassGroupID == EFSLIST_GENERAL || m_RP.m_nPassGroupID == EFSLIST_TRANSP) && !(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_ZPASS)
	  && !(m_RP.m_PersFlags2 & (RBPF2_MOTIONBLURPASS)))
	{
		// reset quality settings. BEWARE: these are used by shadows as well
		m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_TILED_SHADING] | g_HWSR_MaskBit[HWSR_QUALITY1]);
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY];

		// force per object fog
		m_RP.m_FlagsShader_RT |= (g_HWSR_MaskBit[HWSR_FOG] | g_HWSR_MaskBit[HWSR_ALPHABLEND]);

		if (CV_r_DeferredShadingTiled && CV_r_DeferredShadingTiledHairQuality > 0)
		{
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

			if (CV_r_DeferredShadingTiledHairQuality > 1)
				m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
		}

		if ((pPass->m_RenderState & GS_DEPTHFUNC_MASK) == GS_DEPTHFUNC_LESS)
		{
			nState  = (nState & ~(GS_BLEND_MASK | GS_DEPTHFUNC_MASK));
			nState |= GS_DEPTHFUNC_LESS;

			if ((m_RP.m_nPassGroupID == EFSLIST_TRANSP) && (m_RP.m_pShader->m_Flags2 & EF2_DEPTH_FIXUP))
				nState |= GS_BLSRC_SRC1ALPHA | GS_BLDST_ONEMINUSSRC1ALPHA | GS_BLALPHA_MIN;
			else
				nState |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;

			if (pPass->m_RenderState & GS_DEPTHWRITE)
				nState |= GS_DEPTHWRITE;
			else
				nState &= ~GS_DEPTHWRITE;
		}
		else
		{
			nState  = (nState & ~(GS_BLEND_MASK | GS_DEPTHFUNC_MASK));
			nState |= GS_DEPTHFUNC_EQUAL /*| GS_DEPTHWRITE*/;
		}
	}
}

void CD3D9Renderer::FX_CommitStates(const SShaderTechnique* pTech, const SShaderPass* pPass, bool bUseMaterialState)
{
	FUNCTION_PROFILER_RENDER_FLAT
	uint32 State = 0;
	int AlphaRef = pPass->m_AlphaRef == 0xff ? -1 : pPass->m_AlphaRef;
	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI     = rRP.m_TI[rRP.m_nProcessThreadID];

	if (rRP.m_pCurObject->m_RState)
	{
		switch (rRP.m_pCurObject->m_RState & OS_TRANSPARENT)
		{
		case OS_ALPHA_BLEND:
			State = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
			break;
		case OS_ADD_BLEND:
			// In HDR mode, this is equivalent to pure additive GS_BLSRC_ONE | GS_BLDST_ONE.
			State = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL;
			break;
		case OS_MULTIPLY_BLEND:
			State = GS_BLSRC_DSTCOL | GS_BLDST_SRCCOL;
			break;
		}

		State = ((rRP.m_pCurObject->m_RState & 7) && (m_RP.m_PersFlags2 & RBPF2_THERMAL_RENDERMODE_PASS)) ? (GS_BLSRC_ONE | GS_BLDST_ONE) : State;

		if (rRP.m_pCurObject->m_RState & OS_NODEPTH_TEST)
			State |= GS_NODEPTHTEST;
		AlphaRef = 0;
	}
	else
		State = pPass->m_RenderState;

	if (bUseMaterialState && rRP.m_MaterialStateOr != 0)
	{
		if (rRP.m_MaterialStateOr & GS_ALPHATEST)
			AlphaRef = rRP.m_MaterialAlphaRef;
		State &= ~rRP.m_MaterialStateAnd;
		State |= rRP.m_MaterialStateOr;
	}

	//This has higher priority than material states as for alphatested material
	//it is forced to use depth writing (FX_SetResourcesState)
	if (rRP.m_pCurObject->m_RState & OS_TRANSPARENT)
		State &= ~GS_DEPTHWRITE;

	if (!(pTech->m_Flags & FHF_POSITION_INVARIANT) && !(pPass->m_PassFlags & SHPF_FORCEZFUNC))
		FX_ZState(State);

	if (bUseMaterialState && (rRP.m_pCurObject->m_fAlpha < 1.0f) && !rRP.m_bIgnoreObjectAlpha)
		State = (State & ~(GS_BLEND_MASK | GS_DEPTHWRITE)) | (GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

	if (m_RP.m_PersFlags2 & RBPF2_THERMAL_RENDERMODE_TRANSPARENT_PASS || CRenderer::CV_r_measureoverdraw == 4)
	{
		State &= ~(GS_BLSRC_MASK | GS_BLDST_MASK);
		State |= GS_BLSRC_ONE | GS_BLDST_ONE;
	}

	State &= ~rRP.m_ForceStateAnd;
	State |= rRP.m_ForceStateOr;

	if (rRP.m_pShader->m_Flags2 & EF2_HAIR)
		FX_HairState(State, pPass);
	else if ((m_RP.m_nPassGroupID == EFSLIST_TRANSP) &&
	  !(rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS) && !(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_ZPASS))
	{
		// Depth fixup for transparent geometry
		if (m_RP.m_pShader->m_Flags2 & EF2_DEPTH_FIXUP)
		{
			if (rRP.m_pCurObject->m_RState & OS_ALPHA_BLEND)
			{
				State                &= ~(GS_NOCOLMASK_A | GS_BLSRC_MASK | GS_BLDST_MASK);
				State                |= GS_BLSRC_SRC1ALPHA | GS_BLDST_ONEMINUSSRC1ALPHA;
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
			}
			else if ((rRP.m_pCurObject->m_RState & OS_TRANSPARENT) == 0)
			{
				State &= ~(GS_NOCOLMASK_A | GS_BLSRC_MASK | GS_BLDST_MASK);
				State |= GS_BLDST_ZERO | GS_BLSRC_ONE;
			}
		}

		// min blending on depth values (alpha channel)
		State &= ~GS_BLALPHA_MASK;
		State |= GS_BLALPHA_MIN;
		if ((State & GS_BLSRC_MASK) * (State & GS_BLDST_MASK) == 0)
		{
			State |= GS_BLDST_ZERO | GS_BLSRC_ONE;
		}
	}

	if ((rRP.m_PersFlags2 & RBPF2_ALLOW_DEFERREDSHADING) && (rRP.m_pShader->m_Flags & EF_SUPPORTSDEFERREDSHADING))
	{
		if (rTI.m_PersFlags & RBPF_ZPASS)
		{
			if ((rRP.m_pShader->m_Flags & EF_DECAL) || rRP.m_nPassGroupID == EFSLIST_TERRAINLAYER)
			{
				State                 = (State & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK));
				State                |= GS_DEPTHFUNC_LEQUAL | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
			}

			// Disable alpha writes - for alpha blend case we use default alpha value as a default power factor
			if (State & GS_BLEND_MASK)
				State |= GS_NOCOLMASK_A;

			// Disable alpha testing/depth writes if geometry had a z-prepass
			if (!(rRP.m_PersFlags2 & RBPF2_ZPREPASS) && (rRP.m_RIs[0][0]->nBatchFlags & FB_ZPREPASS))
			{
			State &= ~(GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_ALPHATEST);
				State                |= GS_DEPTHFUNC_EQUAL;
				rRP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_ALPHATEST];
			}

		}
	}

	if ((rRP.m_ObjFlags & FOB_MOTION_BLUR) != 0 && (rRP.m_ObjFlags & FOB_HAS_PREVMATRIX) != 0
	  && (rRP.m_ObjFlags & FOB_SKINNED) == 0 && (rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS) != 0)
	{
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_VERTEX_VELOCITY];
	}

	if (rRP.m_PersFlags2 & RBPF2_CUSTOM_RENDER_PASS)
	{
		rRP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];
		if (CRenderer::CV_r_customvisions == 2)
		{
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
			State                |= GS_BLSRC_ONE | GS_BLDST_ONE;
		}
		else if (CRenderer::CV_r_customvisions == 3)
		{
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];

			// Ignore depth thresholding in Post3DRender
			if (rRP.m_PersFlags2 & RBPF2_POST_3D_RENDERER_PASS)
			{
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5];
			}
		}

		SRenderObjData *const __restrict pOD =  rRP.m_pCurObject->GetObjData();
		if(pOD->m_nHUDSilhouetteParams && !(pOD->m_nCustomFlags & COB_HUD_REQUIRE_DEPTHTEST))
		{
			State |= GS_NODEPTHTEST;
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5]; //Ignore depth threshold in SilhoueteVisionOptimised
		}
		else
		{
			State &= ~GS_NODEPTHTEST;
			rRP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE5];
		}
	}

	if (m_NewViewport.fMaxZ <= 0.01f)
		State &= ~GS_DEPTHWRITE;

	// Intermediate solution to disable depth testing in 3D HUD
	if (rRP.m_pCurObject->m_ObjFlags & FOB_RENDER_AFTER_POSTPROCESSING)
	{
		State &= ~GS_DEPTHFUNC_MASK;
		State |= GS_NODEPTHTEST;
	}

	if (rRP.m_PersFlags2 & RBPF2_DISABLECOLORWRITES)
	{
		State &= ~GS_COLMASK_MASK;
		State |= GS_NOCOLMASK_RGBA;
	}

	FX_SetState(State, AlphaRef);

	if (State & GS_ALPHATEST)
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

	int nBlend;
	if (nBlend = (rRP.m_CurState & (GS_BLEND_MASK & ~GS_BLALPHA_MASK)))
	{
		//set alpha blend shader flag when the blend mode for color is set to alpha blend.
		if (nBlend == (GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA)
		  || nBlend == (GS_BLSRC_SRCALPHA | GS_BLDST_ONE)
		  || nBlend == (GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA)
		  || nBlend == (GS_BLSRC_SRCALPHA_A_ZERO | GS_BLDST_ONEMINUSSRCALPHA))
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
	}
}

//=====================================================================================

bool CD3D9Renderer::FX_GetTargetSurfaces(CTexture* pTarget, D3DSurface*& pTargSurf, SRenderTargetStack* pCur, int nCMSide, int nTarget, uint32 nTileCount)
{
	if (pTarget)
	{
		if (!CTexture::IsTextureExist(pTarget) && !pTarget->m_bNoDevTexture)
		{
			pTarget->CreateRenderTarget(eTF_Unknown, pTarget->GetClearColor());
		}

		if (!CTexture::IsTextureExist(pTarget))
		{
			return false;
		}

		pTargSurf = pTarget->GetSurface(nCMSide, 0);
	}
	else
	{
		pTargSurf = NULL;
	}

	return true;
}

bool CD3D9Renderer::FX_SetRenderTarget(int nTarget, D3DSurface* pTargetSurf, SDepthTexture* pDepthTarget, uint32 nTileCount)
{
	assert(!nTarget || (pTargetSurf == (D3DSurface*)0xDEADBEEF && pDepthTarget == (SDepthTexture*)0xDEADBEEF));
	if (nTarget >= RT_STACK_WIDTH || m_nRTStackLevel[nTarget] >= MAX_RT_STACK)
		return false;

	HRESULT hr               = 0;
	SRenderTargetStack* pCur = &m_RTStack[nTarget][m_nRTStackLevel[nTarget]];

	pCur->m_pTarget    = pTargetSurf;
	pCur->m_pTex       = nullptr;
	pCur->m_pSurfDepth = pDepthTarget;
	pCur->m_pDepth     = pDepthTarget ? (pDepthTarget == (SDepthTexture*)0xDEADBEEF ? (D3DDepthSurface*)0xDEADBEEF : pDepthTarget->pSurface) : nullptr;

#ifdef _DEBUG
	// NOTE: bottom of the stack is always the active swap-chain back-buffer
	if (m_nRTStackLevel[nTarget] == 0 && nTarget == 0)
	{
		assert((pCur->m_pTarget == (D3DSurface*)0xDEADBEEF /*|| pCur->m_pTarget == m_pBackBuffer*/) && (pCur->m_pDepth == (D3DDepthSurface*)0xDEADBEEF /*|| pCur->m_pDepth == m_DepthBufferNative.pSurface*/));
	}
#endif

	pCur->m_bNeedReleaseRT = false;
	pCur->m_bWasSetRT      = false;
	pCur->m_bWasSetD       = false;
	pCur->m_ClearFlags     = 0;
	m_pNewTarget[nTarget]  = pCur;
	if (nTarget == 0)
	{
		m_RP.m_StateOr &= ~GS_NOCOLMASK_RGBA;
	}
	m_nMaxRT2Commit      = max(m_nMaxRT2Commit, nTarget);
	m_RP.m_nCommitFlags |= FC_TARGETS;
	return (hr == S_OK);
}

bool CD3D9Renderer::FX_PushRenderTarget(int nTarget, D3DSurface* pTargetSurf, SDepthTexture* pDepthTarget, uint32 nTileCount)
{
	assert(m_pRT->IsRenderThread());
	assert(!nTarget || (pTargetSurf == (D3DSurface*)0xDEADBEEF && pDepthTarget == (SDepthTexture*)0xDEADBEEF));
	if (nTarget >= RT_STACK_WIDTH || m_nRTStackLevel[nTarget] >= MAX_RT_STACK)
		return false;

	m_nRTStackLevel[nTarget]++;
	return FX_SetRenderTarget(nTarget, pTargetSurf, pDepthTarget, nTileCount);
}

bool CD3D9Renderer::FX_SetRenderTarget(int nTarget, CTexture* pTarget, SDepthTexture* pDepthTarget, bool bPush, int nCMSide, bool bScreenVP, uint32 nTileCount)
{
	assert(!nTarget || !pDepthTarget);
	assert((unsigned int) nTarget < RT_STACK_WIDTH);

	if (pTarget && !(pTarget->GetFlags() & FT_USAGE_RENDERTARGET))
	{
		CryFatalError("Attempt to bind a non-render-target texture as a render-target");
	}

	if (pTarget && pDepthTarget)
	{
		if (pTarget->GetWidth() > pDepthTarget->nWidth || pTarget->GetHeight() > pDepthTarget->nHeight)
		{
			iLog->LogError("Error: RenderTarget '%s' size:%i x %i DepthSurface size:%i x %i \n", pTarget->GetName(), pTarget->GetWidth(), pTarget->GetHeight(), pDepthTarget->nWidth, pDepthTarget->nHeight);
		}
		assert(pTarget->GetWidth() <= pDepthTarget->nWidth);
		assert(pTarget->GetHeight() <= pDepthTarget->nHeight);
	}

	if (nTarget >= RT_STACK_WIDTH || m_nRTStackLevel[nTarget] >= MAX_RT_STACK)
		return false;

	SRenderTargetStack* pCur = &m_RTStack[nTarget][m_nRTStackLevel[nTarget]];
	D3DSurface* pTargSurf;

	if (pCur->m_pTex)
	{
		if (pCur->m_bNeedReleaseRT)
		{
			pCur->m_bNeedReleaseRT = false;
		}

		m_pNewTarget[0]->m_bWasSetRT = false;
		m_pNewTarget[0]->m_pTarget   = NULL;

		pCur->m_pTex->Unlock();
	}

	if (!pTarget)
		pTargSurf = NULL;
	else
	{
		if (!FX_GetTargetSurfaces(pTarget, pTargSurf, pCur, nCMSide, nTarget, nTileCount))
			return false;
	}

	if (pTarget)
	{
		int nFrameID = m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID;
		if (pTarget && pTarget->m_nUpdateFrameID != nFrameID)
		{
			pTarget->m_nUpdateFrameID = nFrameID;
		}
	}

	if (!bPush && pDepthTarget && pDepthTarget->pSurface != pCur->m_pDepth)
	{
		//assert(pCur->m_pDepth == m_pCurDepth);
		//assert(pCur->m_pDepth != m_pZBuffer);   // Attempt to override default Z-buffer surface
		if (pCur->m_pSurfDepth)
			pCur->m_pSurfDepth->bBusy = false;
	}

	pCur->m_pTarget        = pTargSurf;
	pCur->m_pTex           = nullptr;
	pCur->m_pSurfDepth     = pDepthTarget;
	pCur->m_pDepth         = pDepthTarget ? (pDepthTarget == (SDepthTexture*)0xDEADBEEF ? (D3DDepthSurface*)0xDEADBEEF : pDepthTarget->pSurface) : nullptr;

#ifdef _DEBUG
	// NOTE: bottom of the stack is always the active swap-chain back-buffer
	if (m_nRTStackLevel[nTarget] == 0 && nTarget == 0)
	{
		assert((pCur->m_pTarget == (D3DSurface*)0xDEADBEEF /*|| pCur->m_pTarget == m_pBackBuffer*/) && (pCur->m_pDepth == (D3DDepthSurface*)0xDEADBEEF /*|| pCur->m_pDepth == m_DepthBufferNative.pSurface*/));
	}
#endif

	pCur->m_ClearFlags     = 0;
	pCur->m_bNeedReleaseRT = true;
	pCur->m_bWasSetRT      = false;
	pCur->m_bWasSetD       = false;
	pCur->m_bScreenVP      = bScreenVP;
	if (pTarget)
		pTarget->Lock();
	if (pDepthTarget)
	{
		pDepthTarget->bBusy        = true;
		pDepthTarget->nFrameAccess = m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID;
	}

	if (pTarget)
		pCur->m_pTex = pTarget;
	else if (pDepthTarget)
		pCur->m_pTex = pDepthTarget->pTexture;
	else
		pCur->m_pTex = nullptr;

	if (pTarget)
	{
		pCur->m_Width  = pTarget->GetWidth();
		pCur->m_Height = pTarget->GetHeight();
	}
	else if (pDepthTarget)
	{
		pCur->m_Width  = pDepthTarget->nWidth;
		pCur->m_Height = pDepthTarget->nHeight;
	}
	if (!nTarget)
	{
		if (bScreenVP)
			RT_SetViewport(m_MainViewport.nX, m_MainViewport.nY, m_MainViewport.nWidth, m_MainViewport.nHeight);
		else
			RT_SetViewport(0, 0, pCur->m_Width, pCur->m_Height);
	}
	m_pNewTarget[nTarget] = pCur;
	m_nMaxRT2Commit       = max(m_nMaxRT2Commit, nTarget);
	m_RP.m_nCommitFlags  |= FC_TARGETS;
	return true;
}

bool CD3D9Renderer::FX_PushRenderTarget(int nTarget, CTexture* pTarget, SDepthTexture* pDepthTarget, int nCMSide, bool bScreenVP, uint32 nTileCount)
{
	assert(m_pRT->IsRenderThread());
	if (nTarget >= RT_STACK_WIDTH || m_nRTStackLevel[nTarget] == MAX_RT_STACK)
	{
		assert(0);
		return false;
	}

	m_nRTStackLevel[nTarget]++;
	return FX_SetRenderTarget(nTarget, pTarget, pDepthTarget, true, nCMSide, bScreenVP, nTileCount);
}

bool CD3D9Renderer::FX_RestoreRenderTarget(int nTarget)
{
	if (nTarget >= RT_STACK_WIDTH || m_nRTStackLevel[nTarget] < 0)
		return false;

	SRenderTargetStack* pCur = &m_RTStack[nTarget][m_nRTStackLevel[nTarget]];
#ifdef _DEBUG
	if (m_nRTStackLevel[nTarget] == 0 && nTarget == 0)
	{
		assert((pCur->m_pTarget == (D3DSurface*)0xDEADBEEF /*|| pCur->m_pTarget == m_pBackBuffer*/) && (pCur->m_pDepth == (D3DDepthSurface*)0xDEADBEEF /*|| pCur->m_pDepth == m_DepthBufferNative.pSurface*/));
	}
#endif

	SRenderTargetStack* pPrev = &m_RTStack[nTarget][m_nRTStackLevel[nTarget] + 1];
	if (pPrev->m_bNeedReleaseRT)
	{
		pPrev->m_bNeedReleaseRT = false;
		if (pPrev->m_pTarget && pPrev->m_pTarget == m_pNewTarget[nTarget]->m_pTarget)
		{
			m_pNewTarget[nTarget]->m_bWasSetRT = false;

			pPrev->m_pTarget                 = NULL;
			m_pNewTarget[nTarget]->m_pTarget = NULL;
		}
	}

	if (nTarget == 0)
	{
		if (pPrev->m_pSurfDepth)
		{
			pPrev->m_pSurfDepth->bBusy = false;
			pPrev->m_pSurfDepth        = NULL;
		}
	}

	if (pPrev->m_pTex)
	{
		pPrev->m_pTex->Unlock();
		pPrev->m_pTex = NULL;
	}

	if (!nTarget)
	{
		SDisplayContext* pDC = GetActiveDisplayContext();

		if (pCur->m_bScreenVP)
			RT_SetViewport(m_MainViewport.nX, m_MainViewport.nY, m_MainViewport.nWidth, m_MainViewport.nHeight);
		else if (!m_nRTStackLevel[nTarget])
			RT_SetViewport(0, 0, pDC->m_Width, pDC->m_Height);
		else
			RT_SetViewport(0, 0, pCur->m_Width, pCur->m_Height);
	}

	pCur->m_bWasSetD      = false;
	pCur->m_bWasSetRT     = false;
	m_pNewTarget[nTarget] = pCur;
	m_nMaxRT2Commit       = max(m_nMaxRT2Commit, nTarget);

	m_RP.m_nCommitFlags |= FC_TARGETS;
	return true;
}
bool CD3D9Renderer::FX_PopRenderTarget(int nTarget)
{
	assert(m_pRT->IsRenderThread());
	if (m_nRTStackLevel[nTarget] <= 0)
	{
		assert(0);
		return false;
	}
	m_nRTStackLevel[nTarget]--;
	return FX_RestoreRenderTarget(nTarget);
}

SDepthTexture* CD3D9Renderer::FX_GetDepthSurface(int nWidth, int nHeight, bool bAA, bool bExactMatch)
{
	assert(m_pRT->IsRenderThread());

	SDepthTexture* pSrf = NULL;
	uint32 i;
	int nBestX = -1;
	int nBestY = -1;
	for (i = 0; i < m_TempDepths.Num(); i++)
	{
		pSrf = m_TempDepths[i];
		if (!pSrf->bBusy)
		{
			if (pSrf->nWidth == nWidth && pSrf->nHeight == nHeight)
			{
				nBestX = i;
				break;
			}
			if (!bExactMatch)
			{
				if (nBestX < 0 && pSrf->nWidth == nWidth && pSrf->nHeight >= nHeight)
					nBestX = i;
				else if (nBestY < 0 && pSrf->nWidth >= nWidth && pSrf->nHeight == nHeight)
					nBestY = i;
			}
		}
	}
	if (nBestX >= 0)
		return m_TempDepths[nBestX];
	if (nBestY >= 0)
		return m_TempDepths[nBestY];

	bool allowUsingLargerRT = true;

#if defined(OGL_DO_NOT_ALLOW_LARGER_RT)
	allowUsingLargerRT = false;
#elif defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
	if (CV_r_EnableDebugLayer)
		allowUsingLargerRT = false;
#endif
	if (bExactMatch)
		allowUsingLargerRT = false;

	if (allowUsingLargerRT)
	{
		for (i = 0; i < m_TempDepths.Num(); i++)
		{
			pSrf = m_TempDepths[i];
			if (pSrf->nWidth >= nWidth && pSrf->nHeight >= nHeight && !pSrf->bBusy)
				break;
		}
	}
	else
	{
		i = m_TempDepths.Num();
	}

	if (i == m_TempDepths.Num())
	{
		pSrf = FX_CreateDepthSurface(nWidth, nHeight, bAA);
		if (pSrf != NULL)
			m_TempDepths.AddElem(pSrf);
	}

	return pSrf;
}

SDepthTexture* CD3D9Renderer::FX_CreateDepthSurface(int nWidth, int nHeight, bool bAA)
{
	CDeviceTexture* pZTexture;

	SDepthTexture depthSurface;
	depthSurface.nWidth = nWidth;
	depthSurface.nHeight = nHeight;
	depthSurface.nFrameAccess = -1;
	depthSurface.bBusy = false;

	const float  clearDepth   = (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) ? 0.f : 1.f;
	const uint   clearStencil = 0;
	const ColorF clearValues  = ColorF(clearDepth, FLOAT(clearStencil), 0.f, 0.f);

	STextureLayout Layout =
	{
		nullptr,
		nWidth, nHeight, 1, 1, 1,
		m_preferredDepthFormat, m_preferredDepthFormat, eTT_2D,

		/* TODO: change FT_... to CDeviceObjectFactory::... */
		FT_USAGE_DEPTHSTENCIL /* CDeviceObjectFactory::BIND_DEPTH_STENCIL | CDeviceObjectFactory::HEAP_DEFAULT */, false,
		clearValues
#if CRY_PLATFORM_DURANGO
		, SKIP_ESRAM
#endif
	};

	Layout.m_eDstFormat = CTexture::GetClosestFormatSupported(Layout.m_eDstFormat, Layout.m_pPixelFormat);
	pZTexture = CDeviceTexture::Create(Layout, nullptr);
	if (!pZTexture)
		return nullptr;

	depthSurface.pTexture = new CTexture(FT_USAGE_DEPTHSTENCIL | FT_DONT_STREAM, clearValues, pZTexture);
	depthSurface.pTarget = depthSurface.pTexture->GetDevTexture()->Get2DTexture();
	depthSurface.pSurface = depthSurface.pTexture->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil);

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	depthSurface.pTarget->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Dynamically requested Depth-Buffer"), "Dynamically requested Depth-Buffer");
#endif

	CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
	commandList.GetGraphicsInterface()->ClearSurface(depthSurface.pSurface, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, clearDepth, clearStencil);

	return new SDepthTexture(depthSurface);
}

// Commit changes states to the hardware before drawing
bool CD3D9Renderer::FX_CommitStreams(SShaderPass* sl, bool bSetVertexDecl)
{
	FUNCTION_PROFILER_RENDER_FLAT

	//PROFILE_FRAME(Draw_Predraw);
	SRenderPipeline& RESTRICT_REFERENCE rp(m_RP);

#if ENABLE_NORMALSTREAM_SUPPORT
	if (CHWShader_D3D::s_pCurInstHS)
	{
		rp.m_FlagsStreams_Stream |= (1 << VSF_NORMALS);
		rp.m_FlagsStreams_Decl   |= (1 << VSF_NORMALS);
	}
#endif

	HRESULT hr;
	if (bSetVertexDecl)
	{
		if ((m_RP.m_ObjFlags & FOB_POINT_SPRITE) && !CHWShader_D3D::s_pCurInstHS)
		{
			rp.m_FlagsStreams_Stream |= VSM_INSTANCED;
			rp.m_FlagsStreams_Decl   |= VSM_INSTANCED;
		}
		hr = FX_SetVertexDeclaration(rp.m_FlagsStreams_Decl, rp.m_CurVFormat);
		if (FAILED(hr))
			return false;
	}

	if (rp.m_pRE)
	{
		bool bRet = rp.m_pRE->mfPreDraw(sl);
		return bRet;
	}
	else if (rp.m_RendNumVerts && rp.m_RendNumIndices)
	{
		{
			{
				TempDynVBAny::CreateFillAndBind(rp.m_StreamPtr.Ptr, rp.m_RendNumVerts, 0, rp.m_StreamStride);
				rp.m_FirstVertex = 0;
				rp.m_PS[rp.m_nProcessThreadID].m_DynMeshUpdateBytes += rp.m_RendNumVerts * rp.m_StreamStride;
			}
			{
				TempDynIB16::CreateFillAndBind(rp.m_SysRendIndices, rp.m_RendNumIndices);
				rp.m_FirstIndex = 0;
				rp.m_PS[rp.m_nProcessThreadID].m_DynMeshUpdateBytes += rp.m_RendNumIndices * sizeof(short);
			}

			if (rp.m_FlagsStreams_Stream & VSM_TANGENTS)
			{
				TempDynVB<SPipTangents>::CreateFillAndBind((const SPipTangents*) rp.m_StreamPtrTang.Ptr, rp.m_RendNumVerts, VSF_TANGENTS);
				rp.m_PersFlags1 |= RBPF1_USESTREAM << VSF_TANGENTS;
				rp.m_PS[rp.m_nProcessThreadID].m_DynMeshUpdateBytes += rp.m_RendNumVerts * sizeof(SPipTangents);
			}
			else if (rp.m_PersFlags1 & (RBPF1_USESTREAM << (VSF_TANGENTS | VSF_QTANGENTS)))
			{
				rp.m_PersFlags1 &= ~(RBPF1_USESTREAM << (VSF_TANGENTS | VSF_QTANGENTS));
				FX_SetVStream(1, NULL, 0, 0);
			}
		}
	}

	return true;
}

#ifdef CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK
bool CD3D9Renderer::FX_DebugCheckConsistency(int FirstVertex, int FirstIndex, int RendNumVerts, int RendNumIndices)
{
	if (CV_r_validateDraw != 2)
		return true;
	HRESULT hr = S_OK;
	assert(m_RP.m_VertexStreams[0].pStream);
	D3DVertexBuffer* pVB = m_RP.m_VertexStreams[0].pStream;
	D3DIndexBuffer*  pIB = m_RP.m_pIndexStream;
	assert(pIB && pVB);
	if (!pIB || !pVB)
		return false;
	int i;
	int nVBOffset = m_RP.m_VertexStreams[0].nOffset;

	InputLayoutHandle eVBFormat = m_RP.m_CurVFormat;
	InputLayoutHandle eVBFormatExt = CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&CHWShader_D3D::s_pCurInstVS->m_Shader, m_RP.m_FlagsStreams_Decl, 0, 0, nullptr, eVBFormat);

	const std::pair<SInputLayout, CDeviceInputLayout*>& baseLayout = CDeviceObjectFactory::LookupInputLayout(eVBFormat);
	const std::pair<SInputLayout, CDeviceInputLayout*>& extLayout = CDeviceObjectFactory::LookupInputLayout(eVBFormatExt);

	size_t nVBStride        = baseLayout.first.m_Stride;
	uint16* pIBData         = (uint16*)m_DevBufMan.BeginReadDirectIB(pIB, RendNumIndices * sizeof(uint16), FirstIndex * sizeof(uint16));
	byte* pVBData           = (byte*)m_DevBufMan.BeginReadDirectVB(pVB, RendNumIndices * nVBStride, nVBOffset);
	assert(baseLayout.second);
	assert(extLayout.second);

	Vec3 vMax, vMin;
	vMin = Vec3(100000.0f, 100000.0f, 100000.0f);
	vMax = Vec3(-100000.0f, -100000.0f, -100000.0f);

	for (i = 0; i < RendNumIndices; i++)
	{
		int nInd = pIBData[i];
		assert(nInd >= FirstVertex && nInd < FirstVertex + RendNumVerts);
		byte* pV = &pVBData[nInd * nVBStride];
		Vec3  VV = ((Vec3f16*)pV)->ToVec3();
		vMin.CheckMin(VV);
		vMax.CheckMax(VV);
		Vec3 vAbs = VV.abs();
		assert(vAbs.x < 10000.0f && vAbs.y < 10000.0f && vAbs.z < 10000.0f);
		if (vAbs.x > 10000.0f || vAbs.y > 10000.0f || vAbs.z > 10000.0f || !_finite(vAbs.x) || !_finite(vAbs.y) || !_finite(vAbs.z))
			hr = S_FALSE;
	}
	Vec3 vDif = vMax - vMin;
	assert(vDif.x < 10000.0f && vDif.y < 10000.0f && vDif.z < 10000.0f);
	if (vDif.x >= 10000.0f || vDif.y > 10000.0f || vDif.z > 10000.0f)
		hr = S_FALSE;

	m_DevBufMan.EndReadDirectIB(pIB);
	m_DevBufMan.EndReadDirectVB(pVB);
	if (hr != S_OK)
	{
		iLog->LogError("ERROR: CD3D9Renderer::FX_DebugCheckConsistency: Validation failed for DIP (Shader: '%s')\n", m_RP.m_pShader->GetName());
	}
	return (hr == S_OK);
}
#endif

// Draw current indexed mesh
void CD3D9Renderer::FX_DrawIndexedMesh (const ERenderPrimitiveType nPrimType)
{
	DETAILED_PROFILE_MARKER("FX_DrawIndexedMesh");
	FX_Commit();

	// Don't render fallback in DX11
	if (!CHWShader_D3D::s_pCurInstVS || !CHWShader_D3D::s_pCurInstPS || CHWShader_D3D::s_pCurInstVS->m_bFallback || CHWShader_D3D::s_pCurInstPS->m_bFallback)
		return;
	if (CHWShader_D3D::s_pCurInstGS && CHWShader_D3D::s_pCurInstGS->m_bFallback)
		return;

	PROFILE_FRAME(Draw_DrawCall);

	if (nPrimType != eptHWSkinGroups)
	{
		FX_DebugCheckConsistency(m_RP.m_FirstVertex, m_RP.m_FirstIndex, m_RP.m_RendNumVerts, m_RP.m_RendNumIndices);
		ERenderPrimitiveType eType = nPrimType;
		int nFirstI                = m_RP.m_FirstIndex;
		int nNumI = m_RP.m_RendNumIndices;
#ifdef TESSELLATION_RENDERER
		if (CHWShader_D3D::s_pCurInstHS)
		{
			eType = ept3ControlPointPatchList;
		}
#endif
		FX_DrawIndexedPrimitive(eType, 0, 0, m_RP.m_RendNumVerts, nFirstI, nNumI);

#if defined(ENABLE_PROFILING_CODE)
#ifdef TESSELLATION_RENDERER
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygonsByTypes[m_RP.m_nPassGroupDIP][EVCT_STATIC][m_RP.m_nBatchFilter == FB_Z] += (nPrimType == eptTriangleList || nPrimType == ept3ControlPointPatchList ? nNumI / 3 : nNumI - 2);
#else
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygonsByTypes[m_RP.m_nPassGroupDIP][EVCT_STATIC][m_RP.m_nBatchFilter == FB_Z] += (nPrimType == eptTriangleList ? nNumI / 3 : nNumI - 2);
#endif
#endif
	}
	else
	{
		CRenderChunk* pChunk = m_RP.m_pRE->mfGetMatInfo();
		if (pChunk)
		{
			int nNumVerts = pChunk->nNumVerts;

			int nFirstIndexId = pChunk->nFirstIndexId;
			int nNumIndices   = pChunk->nNumIndices;

			ERenderPrimitiveType eType = eptTriangleList;
			// SHWSkinBatch *pBatch = pChunk->m_pHWSkinBatch;
			FX_DebugCheckConsistency(0, nFirstIndexId, nNumVerts, nNumIndices);

#ifdef TESSELLATION_RENDERER
			if (CHWShader_D3D::s_pCurInstHS)
			{
				eType = ept3ControlPointPatchList;
			}
#endif
			FX_DrawIndexedPrimitive(eType, 0, 0, nNumVerts, nFirstIndexId, nNumIndices);

#if defined(ENABLE_PROFILING_CODE)
			m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygonsByTypes[m_RP.m_nPassGroupDIP][EVCT_SKINNED][m_RP.m_nBatchFilter == FB_Z] += (pChunk->nNumIndices / 3);
#endif
		}
	}
}

//====================================================================================

TArray<CRenderObject*> CD3D9Renderer::s_tempObjects[2];
TArray<SRendItem*>     CD3D9Renderer::s_tempRIs;

// Actual drawing of instances
void CD3D9Renderer::FX_DrawInstances(CShader* ef, SShaderPass* slw, int nRE, uint32 nStartInst, uint32 nLastInst, uint32 nUsedAttr, byte* InstanceData, int nInstAttrMask, byte Attributes[], short dwCBufSlot)
{
	DETAILED_PROFILE_MARKER("FX_DrawInstances");

	CHWShader_D3D* vp = (CHWShader_D3D*)slw->m_VShader;

	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;

	SRendItem** rRIs = &(rRP.m_RIs[nRE][0]);

	if (!CHWShader_D3D::s_pCurInstVS || !CHWShader_D3D::s_pCurInstPS || CHWShader_D3D::s_pCurInstVS->m_bFallback || CHWShader_D3D::s_pCurInstPS->m_bFallback)
		return;

	PREFAST_SUPPRESS_WARNING(6326)
	if (!nStartInst)
	{
		// Set the stream 3 to be per instance data and iterate once per instance
		rRP.m_PersFlags1 &= ~(RBPF1_USESTREAM << 3);
		int nCompared = 0;
		if (!FX_CommitStreams(slw, false))
			return;

		assert(nInstAttrMask != 0);
		ID3D11InputLayout* pDeclaration = CDeviceObjectFactory::LookupInputLayout(CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&CHWShader_D3D::s_pCurInstVS->m_Shader, rRP.m_FlagsStreams_Decl, nInstAttrMask, nUsedAttr, Attributes, rRP.m_CurVFormat)).second;

		if (m_pLastVDeclaration != pDeclaration)
		{
			m_pLastVDeclaration = pDeclaration;
			m_DevMan.BindVtxDecl(pDeclaration);
		}
	}

	int nInsts = nLastInst - nStartInst + 1;
	{
		//PROFILE_FRAME(Draw_ShaderIndexMesh);
		int nPolysPerInst = rRP.m_RendNumIndices / 3;
#ifndef _RELEASE
		char instanceLabel[64];
		if (CV_r_geominstancingdebug)
		{
			cry_sprintf(instanceLabel, "Instances: %d", nInsts);
			PROFILE_LABEL_PUSH(instanceLabel);
		}
#endif

		assert (rRP.m_pRE && rRP.m_pRE->mfGetType() == eDATA_Mesh);
		FX_Commit();
		D3D11_PRIMITIVE_TOPOLOGY eTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
#ifdef TESSELLATION_RENDERER
		if (CHWShader_D3D::s_pCurInstHS)
		{
			eTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		}
#endif
		SetPrimitiveTopology(eTopology);

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
		m_DevMan.DrawIndexedInstanced(rRP.m_RendNumIndices, nInsts, ApplyIndexBufferBindOffset(rRP.m_FirstIndex), 0, 0);
#endif

#ifndef _RELEASE
		if (CV_r_geominstancingdebug)
		{
			PROFILE_LABEL_POP(instanceLabel);
		}
#endif

#if defined(ENABLE_PROFILING_CODE)
		int nPolysAll = nPolysPerInst * nInsts;
		rRP.m_PS[rRP.m_nProcessThreadID].m_nPolygons[rRP.m_nPassGroupDIP] += rRP.m_RendNumIndices / 3;
		rRP.m_PS[rRP.m_nProcessThreadID].m_nDIPs[rRP.m_nPassGroupDIP]     += nInsts;
		rRP.m_PS[rRP.m_nProcessThreadID].m_nPolygons[rRP.m_nPassGroupDIP] += nPolysAll;
		rRP.m_PS[rRP.m_nProcessThreadID].m_nInsts += nInsts;
		rRP.m_PS[rRP.m_nProcessThreadID].m_nInstCalls++;
#if defined(ENABLE_STATOSCOPE_RELEASE)
		rRP.m_PS[rRP.m_nProcessThreadID].m_RendHWInstancesPolysOne += nPolysPerInst;
		rRP.m_PS[rRP.m_nProcessThreadID].m_RendHWInstancesPolysAll += nPolysAll;
		rRP.m_PS[rRP.m_nProcessThreadID].m_NumRendHWInstances      += nInsts;
		rRP.m_PS[rRP.m_nProcessThreadID].m_RendHWInstancesDIPs++;
#endif
#endif
	}
}

#define MAX_HWINST_PARAMS_CONST (240 - VSCONST_INSTDATA)

// Draw geometry instances in single DIP using HW geom. instancing (StreamSourceFreq)
void CD3D9Renderer::FX_DrawShader_InstancedHW(CShader* ef, SShaderPass* slw)
{
#if defined(HW_INSTANCING_ENABLED)
	PROFILE_FRAME(DrawShader_Instanced);

	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI     = rRP.m_TI[rRP.m_nProcessThreadID];

	// Set culling mode
	if (!(rRP.m_FlagsPerFlush & RBSI_LOCKCULL))
	{
		if (slw->m_eCull != -1)
			D3DSetCull((ECull)slw->m_eCull);
	}

	bool bProcessedAll = true;

	uint32 i;
	CHWShader_D3D* vp = (CHWShader_D3D*)slw->m_VShader;
	CHWShader_D3D* ps = (CHWShader_D3D*)slw->m_PShader;

	Matrix44 m;
	SCGBind  bind;
	byte Attributes[32];

	rRP.m_FlagsPerFlush |= RBSI_INSTANCED;

	TempDynInstVB vb;

	uint32 nCurInst;
	byte*  data                = NULL;
	CShaderResources* pCurRes  = rRP.m_pShaderResources;
	CShaderResources* pSaveRes = pCurRes;

	uint64 nRTFlags     = rRP.m_FlagsShader_RT;
	uint64 nSaveRTFlags = nRTFlags;

	// batch further and send everything as if it's rotated (full 3x4 matrix), even if we could
	// just send position
	uint64* __restrict maskBit = g_HWSR_MaskBit;

	//nRTFlags |= maskBit[HWSR_INSTANCING_ATTR];

	if (CV_r_geominstancingdebug > 1)
	{
		// !DEBUG0 && !DEBUG1 && DEBUG2 && DEBUG3
		nRTFlags &= ~(maskBit[HWSR_DEBUG0] | maskBit[HWSR_DEBUG1]);
		nRTFlags |= (maskBit[HWSR_DEBUG2] | maskBit[HWSR_DEBUG3]);
	}

	rRP.m_FlagsShader_RT = nRTFlags;

	short dwCBufSlot = 0;

	// Set Pixel shader and all associated textures
	// Note: Need to set pixel shader first to properly set up modifiers for vertex shader (see ShaderCore.cpp & ModificatorTC.cfi)
	if (!ps->mfSet(HWSF_SETTEXTURES))
	{
		rRP.m_FlagsShader_RT = nSaveRTFlags;

		rRP.m_pShaderResources = pSaveRes;
		rRP.m_PersFlags1      |= RBPF1_USESTREAM << 3;
		return;
	}

	// Set Vertex shader
	if (!vp->mfSet(HWSF_INSTANCED | HWSF_SETTEXTURES))
	{
		rRP.m_FlagsShader_RT = nSaveRTFlags;

		rRP.m_pShaderResources = pSaveRes;
		rRP.m_PersFlags1      |= RBPF1_USESTREAM << 3;
		return;
	}

	CHWShader_D3D::SHWSInstance* pVPInst = vp->m_pCurInst;

	CHWShader_D3D* curGS = (CHWShader_D3D*)slw->m_GShader;
	if (curGS)
		curGS->mfSet(0);
	else
		CHWShader_D3D::mfBindGS(NULL, NULL);

	CHWShader_D3D* pCurHS, * pCurDS;
	bool bTessEnabled = FX_SetTessellationShaders(pCurHS, pCurDS, slw);

	CHWShader_D3D* curCS = (CHWShader_D3D*)slw->m_CShader;
	if (curCS)
		curCS->mfSetCS(0);
	else
		CHWShader_D3D::mfBindCS(NULL, NULL);

	if (!pVPInst || pVPInst->m_bFallback || (ps->m_pCurInst && ps->m_pCurInst->m_bFallback))
	{
		return;
	}

	ps->mfSetParametersPI(NULL, NULL);

	if (curGS)
		curGS->mfSetParametersPI(NULL, NULL);

#ifdef TESSELLATION_RENDERER
	if (pCurDS)
		pCurDS->mfSetParametersPI(NULL, NULL);
	if (pCurHS)
		pCurHS->mfSetParametersPI(NULL, NULL);
#endif

	if (curCS)
		curCS->mfSetParametersPI(NULL, NULL);

	int32 nUsedAttr = 3, nInstAttrMask = 0;
	pVPInst->GetInstancingAttribInfo(Attributes, nUsedAttr, nInstAttrMask);

	CRenderElement* pRE    = NULL;
	CRenderMesh* pRenderMesh = NULL;

	const int nLastRE = rRP.m_nLastRE;
	for (int nRE = 0; nRE <= nLastRE; nRE++)
	{
		uint32 nRIs      = rRP.m_RIs[nRE].size();
		SRendItem** rRIs = &(rRP.m_RIs[nRE][0]);

		// don't process REs that don't make the cut for instancing.
		// these were batched with an instance-ready RE, so leave this to drop through into DrawBatch
		if (nRIs <= (uint32)CRenderer::m_iGeomInstancingThreshold)
		{
			bProcessedAll = false;
			continue;
		}

		CShaderResources* pRes = SRendItem::mfGetRes(rRIs[0]->SortVal);
		pRE              = rRP.m_pRE = rRIs[0]->pElem;
		rRP.m_pCurObject = rRIs[0]->pObj;

		InputLayoutHandle curVFormat      = InputLayoutHandle::Unspecified;
		CREMeshImpl* __restrict pMesh = (CREMeshImpl*) pRE;

		pRE->mfPrepare(false);
		curVFormat = pMesh->m_pRenderMesh->_GetVertexFormat();
		{
			if (pCurRes != pRes)
			{
				rRP.m_pShaderResources = pRes;
				vp->mfSetParametersPB();
				if (vp->m_pCurInst)
					vp->mfSetSamplers(vp->m_pCurInst->m_pSamplers, eHWSC_Vertex);
				ps->mfSetParametersPB();
				if (ps->m_pCurInst)
					ps->mfSetSamplers(ps->m_pCurInst->m_pSamplers, eHWSC_Pixel);
#ifdef TESSELLATION_RENDERER
				if (pCurDS && pCurDS->m_pCurInst)
					pCurDS->mfSetSamplers(pCurDS->m_pCurInst->m_pSamplers, eHWSC_Domain);
#endif
				rRP.m_nCommitFlags |= FC_PER_BATCH_PARAMS | FC_MATERIAL_PARAMS;
				pCurRes             = pRes;
			}

			if (pMesh->m_pRenderMesh != pRenderMesh)
			{
				// Create/Update video mesh (VB management)
				if (!pRE->mfCheckUpdate(curVFormat, rRP.m_FlagsStreams_Stream, rTI.m_nFrameUpdateID, bTessEnabled))
				{
					rRP.m_FlagsShader_RT = nSaveRTFlags;

					rRP.m_pShaderResources = pSaveRes;
					rRP.m_PersFlags1      |= RBPF1_USESTREAM << 3;
					return;
				}

				pRenderMesh = pMesh->m_pRenderMesh;
			}

			{
				nCurInst = 0;

				// Detects possibility of using attributes based instancing
				// If number of used attributes exceed 16 we can't use attributes based instancing (switch to constant based)
				int nStreamMask            = rRP.m_FlagsStreams_Stream >> 1;
				InputLayoutHandle nVFormat = rRP.m_CurVFormat;
				uint32 nCO                 = 0;
				int nCI                    = 0;
				uint32 dwDeclarationSize   = 0;   // todo: later m_RP.m_D3DVertexDeclaration[nStreamMask][nVFormat].m_Declaration.Num();
				if (dwDeclarationSize + nUsedAttr - 1 > 16)
					iLog->LogWarning("WARNING: Attributes based instancing cannot exceed 16 attributes (%s uses %d attr. + %d vertex decl.attr.)[VF: %d, SM: 0x%x]", vp->GetName(), nUsedAttr, dwDeclarationSize - 1, nVFormat, nStreamMask);
				else
					while ((int)nCurInst < nRIs)
					{
						uint32 nLastInst = nRIs - 1;

						{
							uint32 nParamsPerInstAllowed = MAX_HWINST_PARAMS;
							if ((nLastInst - nCurInst + 1) * nUsedAttr >= nParamsPerInstAllowed)
								nLastInst = nCurInst + (nParamsPerInstAllowed / nUsedAttr) - 1;
						}
						byte* inddata = NULL;
						{
							vb.Allocate(nLastInst - nCurInst + 1, nUsedAttr * INST_PARAM_SIZE);
							data = (byte*) vb.Lock();
						}
						CRenderObject* curObj = rRP.m_pCurObject;

						// 3 float4 = inst Matrix
						const uint32 nStride = nUsedAttr * sizeof(float[4]);

						// Fill the stream 3 for per-instance data
						CRenderObject* pRO;
						CRenderObject::SInstanceInfo* pI;
						byte* pWalkData = data;
						for (i = nCurInst; i <= nLastInst; i++)
						{
							UFloat4* __restrict vMatrix = (UFloat4*)pWalkData;

							{
								pRO              = rRIs[nCO++]->pObj;
								rRP.m_pCurObject = pRO;
								pI               = &pRO->m_II;
							}
							const float* fSrc = pI->m_Matrix.GetData();
							vMatrix[0].Load(fSrc);
							vMatrix[1].Load(fSrc + 4);
							vMatrix[2].Load(fSrc + 8);

							float* __restrict fParm = (float*)&pWalkData[3 * sizeof(float[4])];
							pWalkData += nStride;

#ifdef DO_RENDERLOG
							if (CRenderer::CV_r_log >= 3)
								Logv("Inst Pos: (%.3f, %.3f, %.3f, %.3f)\n", fSrc[3], fSrc[7], fSrc[11], fSrc[0]);
#endif

							if (pVPInst->m_nParams_Inst >= 0)
							{
								SCGParamsGroup& Group = CGParamManager::s_Groups[pVPInst->m_nParams_Inst];

								fParm = vp->mfSetParametersPI(Group.pParams, Group.nParams, fParm, eHWSC_Vertex, 40);
							}
						}
						rRP.m_pCurObject = curObj;

						vb.Unlock();

						// Set the first stream to be the indexed data and render N instances
						vb.Bind(3, nUsedAttr * INST_PARAM_SIZE);

						vb.Release();

						FX_DrawInstances(ef, slw, nRE, nCurInst, nLastInst, nUsedAttr, data, nInstAttrMask, Attributes, dwCBufSlot);

						nCurInst = nLastInst + 1;
					}
			}
		}
	}
#ifdef TESSELLATION_RENDERER
	if (bTessEnabled)
	{
		CHWShader_D3D::mfBindDS(NULL, NULL);
		CHWShader_D3D::mfBindHS(NULL, NULL);
	}
#endif

	rRP.m_PersFlags1      |= RBPF1_USESTREAM << 3;
	rRP.m_pShaderResources = pSaveRes;
	rRP.m_nCommitFlags     = FC_ALL;
	rRP.m_FlagsShader_RT   = nSaveRTFlags;
	rRP.m_nNumRendPasses++;
	if (!bProcessedAll)
	{
		FX_DrawBatch(ef, slw);
	}
#else
	CryFatalError("HW Instancing not supported on this platform");
#endif
}
//#endif

//====================================================================================

void CD3D9Renderer::FX_FlushSkinVSParams(CHWShader_D3D* pVS, int nFirst, int nBones, int nOffsVS, int numBonesPerChunk, int nSlot, DualQuat* pSkinQuats, DualQuat* pMBSkinQuats)
{
	FUNCTION_PROFILER_RENDER_FLAT
	if (nSlot >= 0)
	{
		int nVecs = VSCONST_SKINMATRIX + numBonesPerChunk * 2;
		if ((m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS) && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB))
			nVecs += NUM_MAX_BONES_PER_GROUP_WITH_MB * 2;
		pVS->mfParameterReg(VSCONST_SKINMATRIX + (nOffsVS << 1), nSlot, eHWSC_Vertex, &pSkinQuats[nFirst].nq.v.x, (nBones << 1), nVecs);
		if ((m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS) && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB))
		{
			// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
			pVS->mfParameterReg(VSCONST_SKINMATRIX + ((nOffsVS + NUM_MAX_BONES_PER_GROUP_WITH_MB) << 1), nSlot, eHWSC_Vertex, &pMBSkinQuats[nFirst].nq.v.x, (nBones << 1), nVecs);
		}
	}
}

void CD3D9Renderer::FX_DrawBatchSkinned(CShader* pSh, SShaderPass* pPass, SSkinningData* pSkinningData)
{
	DETAILED_PROFILE_MARKER("FX_DrawBatchSkinned");
	PROFILE_FRAME(DrawShader_BatchSkinned);

	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI     = rRP.m_TI[rRP.m_nProcessThreadID];

	CHWShader_D3D* pCurVS = (CHWShader_D3D*)pPass->m_VShader;
	CHWShader_D3D* pCurPS = (CHWShader_D3D*)pPass->m_PShader;

	const int nThreadID           = m_RP.m_nProcessThreadID;
	const int bRenderLog          = CRenderer::CV_r_log;
	CREMeshImpl* pRE              = (CREMeshImpl*) rRP.m_pRE;
	CRenderObject* const pSaveObj = rRP.m_pCurObject;

	size_t size           = 0, offset = 0;
	CHWShader_D3D* pCurGS = (CHWShader_D3D*)pPass->m_GShader;

	CRenderMesh* pRenderMesh = pRE->m_pRenderMesh;

	rRP.m_nNumRendPasses++;
	rRP.m_RendNumGroup    = 0;
	rRP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_VERTEX_VELOCITY];

	static ICVar* cvar_gd = gEnv->pConsole->GetCVar("r_ComputeSkinning");
	bool bDoComputeDeformation = (cvar_gd && cvar_gd->GetIVal()) && (pSkinningData->nHWSkinningFlags & eHWS_DC_deformation_Skinning);

	// here we decide if we go compute or vertex skinning
	// problem is once the rRP.m_FlagsShader_RT gets vertex skinning removed, if the UAV is not available in the below rendering loop,
	// the mesh won't get drawn. There is need for another way to do this
	if (bDoComputeDeformation)
	{
		rRP.m_FlagsShader_RT |= (g_HWSR_MaskBit[HWSR_COMPUTE_SKINNING]);
	if (pSkinningData->nHWSkinningFlags & eHWS_SkinnedLinear)
			rRP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SKELETON_SSD_LINEAR]);
		else
			rRP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SKELETON_SSD]);
	}
	else
  {
	  if (pSkinningData->nHWSkinningFlags & eHWS_SkinnedLinear)
		rRP.m_FlagsShader_RT |= (g_HWSR_MaskBit[HWSR_SKELETON_SSD_LINEAR]);
	else
		rRP.m_FlagsShader_RT |= (g_HWSR_MaskBit[HWSR_SKELETON_SSD]);
  }

	CHWShader_D3D* pCurHS, * pCurDS;
	bool bTessEnabled = FX_SetTessellationShaders(pCurHS, pCurDS, pPass);

	bool bRes = pCurPS->mfSetPS(HWSF_SETTEXTURES);
  bRes &= pCurVS->mfSetVS(HWSF_SETTEXTURES);

	if (pCurGS)
		bRes &= pCurGS->mfSetGS(0);
	else
		CHWShader_D3D::mfBindGS(NULL, NULL);

	const uint numObjects = rRP.m_RIs[0].Num();

	if (!bRes)
		goto done;

	if (CHWShader_D3D::s_pCurInstVS && CHWShader_D3D::s_pCurInstVS->m_bFallback)
		goto done;

	// Create/Update video mesh (VB management)
	if (!pRE->mfCheckUpdate(m_RP.m_CurVFormat, m_RP.m_FlagsStreams_Stream, rTI.m_nFrameUpdateID, bTessEnabled))
		goto done;

	if (ShouldApplyFogCorrection())
		FX_FogCorrection();

	// Unlock all VB (if needed) and set current streams
	if (!FX_CommitStreams(pPass))
		goto done;

	for (uint nObj = 0; nObj < numObjects; ++nObj)
	{
		CRenderObject* pObject = rRP.m_RIs[0][nObj]->pObj;
		rRP.m_pCurObject = pObject;

#ifdef DO_RENDERSTATS
		if (FX_ShouldTrackStats())
			FX_TrackStats(pObject, pRE->m_pRenderMesh);
#endif

#ifdef DO_RENDERLOG
		if (bRenderLog >= 3)
		{
			Vec3 vPos = pObject->GetTranslation();
			Logv("+++ HWSkin Group Pass %d (Obj: %d [%.3f, %.3f, %.3f])\n", m_RP.m_nNumRendPasses, pObject->m_Id, vPos[0], vPos[1], vPos[2]);
		}
#endif

		pCurVS->mfSetParametersPI(pObject, pSh);
		pCurPS->mfSetParametersPI(NULL, NULL);

		if (pCurGS)
			pCurGS->mfSetParametersPI(NULL, NULL);
		else
			CHWShader_D3D::mfBindGS(NULL, NULL);
#ifdef TESSELLATION_RENDERER
		if (pCurDS)
			pCurDS->mfSetParametersPI(NULL, NULL);
		else
			CHWShader_D3D::mfBindDS(NULL, NULL);
		if (pCurHS)
			pCurHS->mfSetParametersPI(NULL, NULL);
		else
			CHWShader_D3D::mfBindHS(NULL, NULL);
#endif

		CConstantBuffer* pBuffer[2] = { NULL };
		SRenderObjData*  pOD        = pObject->GetObjData();
		assert(pOD);
		if (pOD)
		{
			SSkinningData* const pSkinningData = pOD->m_pSkinningData;
			if (pSkinningData)
			{
				bool bUseOldPipeline = true;
				CGpuBuffer* skinnedUAVBuffer = NULL;
				if (bDoComputeDeformation)
				{
					// at this point rRP.m_FlagsShader_RT has the vertex skinning reset so if the UAV is not available
					// the mesh will be skipped. This has to be done in a different way and also changed the way streams 
					// are binded for compute vs vertex
					skinnedUAVBuffer = gcpRendD3D->GetComputeSkinningStorage()->GetOutputVertices(pSkinningData->pCustomTag);
					if (skinnedUAVBuffer)
						bUseOldPipeline = false;
				}

				if (!bUseOldPipeline)
				{
					m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_VS, skinnedUAVBuffer->GetDevBuffer()->LookupSRV(EDefaultResourceViews::Default), 16);
					if (rRP.m_pRE)
						rRP.m_pRE->mfDraw(pSh, pPass);
					else
						FX_DrawIndexedMesh(eptTriangleList);

					m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_VS, NULL, 16);
				}
				else
				{
			if (!pRE->BindRemappedSkinningData(pSkinningData->remapGUID))
			{
				continue;
			}

					pBuffer[0] = alias_cast<SCharacterInstanceCB*>(pSkinningData->pCharInstCB)->boneTransformsBuffer;

			// get previous data for motion blur if available
			if (pSkinningData->pPreviousSkinningRenderData)
			{
						pBuffer[1] = alias_cast<SCharacterInstanceCB*>(pSkinningData->pPreviousSkinningRenderData->pCharInstCB)->boneTransformsBuffer;
		}

#ifndef _RELEASE
		rRP.m_PS[nThreadID].m_NumRendSkinnedObjects++;
#endif

		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, pBuffer[0], 9);
		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, pBuffer[1], 10);
		{
			DETAILED_PROFILE_MARKER("DrawSkinned");
			if (rRP.m_pRE)
				rRP.m_pRE->mfDraw(pSh, pPass);
			else
				FX_DrawIndexedMesh(eptTriangleList);
		}
	}
			}
		}
		else
		{
			continue;
		}
	}

done:

	m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, static_cast<CConstantBuffer*>(NULL), 9);
	m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, static_cast<CConstantBuffer*>(NULL), 10);

	rRP.m_FlagsShader_MD &= ~HWMD_TEXCOORD_FLAG_MASK;
	rRP.m_pCurObject      = pSaveObj;

#ifdef TESSELLATION_RENDERER
	if (bTessEnabled)
	{
		CHWShader_D3D::mfBindDS(NULL, NULL);
		CHWShader_D3D::mfBindHS(NULL, NULL);
	}
#endif
	rRP.m_RendNumGroup = -1;
}

#if defined(DO_RENDERSTATS)
void CD3D9Renderer::FX_TrackStats(CRenderObject* pObj, IRenderMesh* pRenderMesh)
{
	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
#if !defined(_RELEASE)

	if (pObj)
	{
		if (IRenderNode* pRenderNode = (IRenderNode*)pObj->m_pRenderNode)
		{
			//Add to per node map for r_stats 6
			if (CV_r_stats == 6 || m_pDebugRenderNode || m_bCollectDrawCallsInfoPerNode)
			{
				IRenderer::RNDrawcallsMapNode& drawCallsInfoPerNode = rRP.m_pRNDrawCallsInfoPerNode[m_RP.m_nProcessThreadID];

				IRenderer::RNDrawcallsMapNodeItor pItor = drawCallsInfoPerNode.find(pRenderNode);

				if (pItor != drawCallsInfoPerNode.end())
				{
					SDrawCallCountInfo& pInfoDP = pItor->second;
					pInfoDP.Update(pObj, pRenderMesh);
				}
				else
				{
					SDrawCallCountInfo pInfoDP;
					pInfoDP.Update(pObj, pRenderMesh);
					drawCallsInfoPerNode.insert(IRenderer::RNDrawcallsMapNodeItor::value_type(pRenderNode, pInfoDP));
				}
			}

			//Add to per mesh map for perfHUD / Statoscope
			if (m_bCollectDrawCallsInfo)
			{
				IRenderer::RNDrawcallsMapMesh& drawCallsInfoPerMesh = rRP.m_pRNDrawCallsInfoPerMesh[m_RP.m_nProcessThreadID];

				IRenderer::RNDrawcallsMapMeshItor pItor = drawCallsInfoPerMesh.find(pRenderMesh);

				if (pItor != drawCallsInfoPerMesh.end())
				{
					SDrawCallCountInfo& pInfoDP = pItor->second;
					pInfoDP.Update(pObj, pRenderMesh);
				}
				else
				{
					SDrawCallCountInfo pInfoDP;
					pInfoDP.Update(pObj, pRenderMesh);
					drawCallsInfoPerMesh.insert(IRenderer::RNDrawcallsMapMeshItor::value_type(pRenderMesh, pInfoDP));
				}

			}
		}
	}
#endif
}
#endif

bool CD3D9Renderer::FX_SetTessellationShaders(CHWShader_D3D*& pCurHS, CHWShader_D3D*& pCurDS, const SShaderPass* pPass)
{
	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;

#ifdef TESSELLATION_RENDERER
	pCurHS = (CHWShader_D3D*)pPass->m_HShader;
	pCurDS = (CHWShader_D3D*)pPass->m_DShader;

	bool bTessEnabled = (pCurHS != NULL) && (pCurDS != NULL) && !(rRP.m_pCurObject->m_ObjFlags & FOB_NEAREST) && (rRP.m_pCurObject->m_ObjFlags & FOB_ALLOW_TESSELLATION);

#ifndef MOTIONBLUR_TESSELLATION
	bTessEnabled &= !(rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS);
#endif

	if(  bTessEnabled && pCurHS->mfSetHS(HWSF_SETTEXTURES) && pCurDS->mfSetDS(HWSF_SETTEXTURES) )
	{
		if (CV_r_tessellationdebug == 1)
		{
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG1];
		}

		return true;
	}

	CHWShader_D3D::mfBindHS(NULL, NULL);
	CHWShader_D3D::mfBindDS(NULL, NULL);
#endif

	rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];

	pCurHS = NULL;
	pCurDS = NULL;

	return false;
}

void CD3D9Renderer::FX_DrawBatch(CShader* pSh, SShaderPass* pPass)
{
	DETAILED_PROFILE_MARKER("FX_DrawBatch");
#if 0
	if (pSh->m_Flags & EF_FAILED_IMPORT)
	{
		printf("[CShaderSerialize] Rendering with shader which failed import: %s (0x%p) flags: 0x%llx mdvFlags: 0x%x\n", gcpRendD3D->m_RP.m_pShader->GetName(), gcpRendD3D->m_RP.m_pShader, gcpRendD3D->m_RP.m_pShader->m_nMaskGenFX, gcpRendD3D->m_RP.m_pShader->m_nMDV);
	}
#endif

	FUNCTION_PROFILER_RENDER_FLAT
	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI     = rRP.m_TI[rRP.m_nProcessThreadID];

	// Set culling mode
	if (!((rRP.m_FlagsPerFlush & RBSI_LOCKCULL) || (m_RP.m_PersFlags2 & RBPF2_LIGHTSHAFTS)))
	{
		if (pPass->m_eCull != -1)
			D3DSetCull((ECull)pPass->m_eCull);
	}

	bool bHWSkinning             = FX_SetStreamFlags(pPass);
	SSkinningData* pSkinningData = NULL;
	if (bHWSkinning)
	{
		SRenderObjData* pOD = rRP.m_pCurObject->GetObjData();
		if (!pOD || !(pSkinningData = pOD->m_pSkinningData))
		{
			bHWSkinning = false;
			Warning ("Warning: Skinned geometry used without character instance");
		}
	}
	IF (bHWSkinning && (rRP.m_pCurObject->m_ObjFlags & FOB_SKINNED) && !CV_r_character_nodeform, 0)
	{
		FX_DrawBatchSkinned(pSh, pPass, pSkinningData);
	}
	else
	{
		DETAILED_PROFILE_MARKER("FX_DrawBatchStatic");

		// Set shaders
		bool bRes        = true;
		const int rStats = CV_r_stats;
		const int rLog   = CV_r_log;

		CHWShader_D3D* pCurGS = (CHWShader_D3D*)pPass->m_GShader;

		if (pCurGS)
			bRes &= pCurGS->mfSetGS(0);
		else
			CHWShader_D3D::mfBindGS(NULL, NULL);

		CHWShader_D3D* pCurHS, * pCurDS;
		bool bTessEnabled = FX_SetTessellationShaders(pCurHS, pCurDS, pPass);

		CHWShader_D3D* pCurVS = (CHWShader_D3D*)pPass->m_VShader;
		CHWShader_D3D* pCurPS = (CHWShader_D3D*)pPass->m_PShader;
		bRes &= pCurPS->mfSetPS(HWSF_SETTEXTURES);
		bRes &= pCurVS->mfSetVS(HWSF_SETTEXTURES);

		if (bRes)
		{
			if (ShouldApplyFogCorrection())
				FX_FogCorrection();

			assert(rRP.m_pRE || !rRP.m_nLastRE);
			CRenderElement* pRE      = rRP.m_pRE;
			CRenderElement* pRESave  = pRE;
			CRenderObject* pSaveObj    = rRP.m_pCurObject;
			CShaderResources* pCurRes  = rRP.m_pShaderResources;
			CShaderResources* pSaveRes = pCurRes;
			for (int nRE = 0; nRE <= rRP.m_nLastRE; nRE++)
			{
				TArray<SRendItem*>& rRIs = rRP.m_RIs[nRE];
				if (!(rRP.m_FlagsPerFlush & RBSI_INSTANCED) || rRIs.size() <= (unsigned)CRenderer::m_iGeomInstancingThreshold)
				{
					if (pRE)
					{
						pRE              = rRP.m_pRE = rRIs[0]->pElem;
						rRP.m_pCurObject = rRIs[0]->pObj;
						//rRIs[0]->nOcclQuery = 1000;  // Mark this batch list that it cannot be instantiated in subsequent passes
						//assert(nRE || pRE == rRP.m_pRE);
						CShaderResources* pRes = (rRP.m_PersFlags2 & RBPF2_MATERIALLAYERPASS) ? rRP.m_pShaderResources : SRendItem::mfGetRes(rRIs[0]->SortVal);
						uint32 nFrameID        = rTI.m_nFrameUpdateID;
						if (!pRE->mfCheckUpdate(rRP.m_CurVFormat, rRP.m_FlagsStreams_Stream | 0x80000000, nFrameID, bTessEnabled))
							continue;
						if (nRE || rRP.m_nNumRendPasses || pCurRes != pRes) // Only static meshes (CREMeshImpl) can use geom batching
						{
							rRP.m_pShaderResources = pRes;
							pRE->mfPrepare(false);
							CREMeshImpl* pM = (CREMeshImpl*) pRE;
							if (pM->m_CustomData || pCurRes != pRes) // Custom data can indicate some shader parameters are from mesh
							{
								pCurVS->mfSetParametersPB();
								pCurPS->mfSetParametersPB();
								if (pCurPS->m_pCurInst)
									pCurPS->mfSetSamplers(pCurPS->m_pCurInst->m_pSamplers, eHWSC_Pixel);
								if (pCurVS->m_pCurInst)
									pCurVS->mfSetSamplers(pCurVS->m_pCurInst->m_pSamplers, eHWSC_Vertex);
#ifdef TESSELLATION_RENDERER
								if (pCurDS && pCurDS->m_pCurInst)
									pCurDS->mfSetSamplers(pCurDS->m_pCurInst->m_pSamplers, eHWSC_Domain);
#endif
								rRP.m_nCommitFlags |= FC_PER_BATCH_PARAMS | FC_MATERIAL_PARAMS;
								pCurRes             = pRes;
							}
						}
					}

					rRP.m_nNumRendPasses++;

					// Unlock all VBs (if needed) and bind current streams
					if (FX_CommitStreams(pPass))
					{
						uint32 nO;
						const uint32 nNumRI = rRIs.Num();
						CRenderObject* pObj = NULL;
						CRenderObject::SInstanceInfo* pI;

#ifdef DO_RENDERSTATS
						if ((CV_r_stats == 6 || m_pDebugRenderNode || m_bCollectDrawCallsInfo) && !(rTI.m_PersFlags & RBPF_MAKESPRITE))
						{
							for (nO = 0; nO < nNumRI; nO++)
							{
								pObj = rRIs[nO]->pObj;

								CRenderElement* pElemBase = rRIs[nO]->pElem;

								if (pElemBase->mfGetType() == eDATA_Mesh)
								{
									CREMeshImpl* pMesh       = (CREMeshImpl*) pElemBase;
									IRenderMesh* pRenderMesh = pMesh ? pMesh->m_pRenderMesh : NULL;

									FX_TrackStats(pObj, pRenderMesh);
								}
							}
						}
#endif

						for (nO = 0; nO < nNumRI; ++nO)
						{
							pObj             = rRIs[nO]->pObj;
							rRP.m_pCurObject = pObj;
							pI               = &pObj->m_II;

#ifdef DO_RENDERLOG
							if (rLog >= 3)
							{
								Vec3 vPos = pObj->GetTranslation();
								Logv("+++ General Pass %d (Obj: %d [%.3f, %.3f, %.3f], %.3f)\n", m_RP.m_nNumRendPasses, pObj->m_Id, vPos[0], vPos[1], vPos[2], pObj->m_fDistance);
							}
#endif

							pCurVS->mfSetParametersPI(pObj, pSh);
							pCurPS->mfSetParametersPI(NULL, NULL);

							if (pCurGS)
								pCurGS->mfSetParametersPI(NULL, NULL);
							else
								CHWShader_D3D::mfBindGS(NULL, NULL);
#ifdef TESSELLATION_RENDERER
							if (pCurDS)
								pCurDS->mfSetParametersPI(NULL, NULL);
							if (pCurHS)
								pCurHS->mfSetParametersPI(NULL, NULL);
#endif

							{
								if (pRE)
									pRE->mfDraw(pSh, pPass);
								else
									FX_DrawIndexedMesh(eptTriangleList);
							}
							m_RP.m_nCommitFlags &= ~(FC_TARGETS | FC_GLOBAL_PARAMS);
						}

						rRP.m_FlagsShader_MD &= ~(HWMD_TEXCOORD_FLAG_MASK);
						if (pRE)
						{
							pRE->m_Flags &= ~FCEF_PRE_DRAW_DONE;
						}
					}
					rRP.m_pCurObject       = pSaveObj;
					rRP.m_pRE              = pRESave;
					rRP.m_pShaderResources = pSaveRes;
				}
			}
		}
#ifdef TESSELLATION_RENDERER
		if (bTessEnabled)
		{
			CHWShader_D3D::mfBindHS(NULL, NULL);
			CHWShader_D3D::mfBindDS(NULL, NULL);
		}
#endif
	}
	m_RP.m_nCommitFlags = FC_ALL;
}

//============================================================================================

void CD3D9Renderer::FX_DrawShader_General(CShader* ef, SShaderTechnique* pTech)
{
	SShaderPass* slw;
	int32 i;

	PROFILE_FRAME(DrawShader_Generic);

	SThreadInfo& RESTRICT_REFERENCE rTI = m_RP.m_TI[m_RP.m_nProcessThreadID];

	EF_Scissor(false, 0, 0, 0, 0);

	if (pTech->m_Passes.Num())
	{
		slw = &pTech->m_Passes[0];
		const int nCount  = pTech->m_Passes.Num();
		uint32 curPassBit = 1;
		for (i = 0; i < nCount; i++, slw++, (curPassBit <<= 1))
		{
			m_RP.m_pCurPass = slw;

			// Set all textures and HW TexGen modes for the current pass (ShadeLayer)
			assert (slw->m_VShader && slw->m_PShader);
			if (!slw->m_VShader || !slw->m_PShader || (curPassBit & m_RP.m_CurPassBitMask))
				continue;

			FX_CommitStates(pTech, slw, (slw->m_PassFlags & SHPF_NOMATSTATE) == 0);

			bool bSkinned = (m_RP.m_pCurObject->m_ObjFlags & FOB_SKINNED) && !CV_r_character_nodeform;

			bSkinned |= FX_SetStreamFlags(slw);

			if (m_RP.m_FlagsPerFlush & RBSI_INSTANCED && !bSkinned)
			{
				// Using HW geometry instancing approach
				FX_DrawShader_InstancedHW(ef, slw);
			}
			else
			{
				FX_DrawBatch(ef, slw);
			}
		}
	}
}

void CD3D9Renderer::FX_DrawEffectLayerPasses()
{
	if (!m_RP.m_pRootTechnique || m_RP.m_pRootTechnique->m_nTechnique[TTYPE_EFFECTLAYER] < 0)
		return;

	CShader* sh             = m_RP.m_pShader;
	SShaderTechnique* pTech = m_RP.m_pShader->m_HWTechniques[m_RP.m_pRootTechnique->m_nTechnique[TTYPE_EFFECTLAYER]];

	PROFILE_FRAME(DrawShader_EffectLayerPasses);

	PROFILE_LABEL_SCOPE("EFFECT_LAYER_PASS");

	CRenderElement* pRE  = m_RP.m_pRE;
	uint64 nSaveRT         = m_RP.m_FlagsShader_RT;
	uint32 nSaveMD         = m_RP.m_FlagsShader_MD;
	uint32 nSavePersFlags  = m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags;
	uint32 nSavePersFlags2 = m_RP.m_PersFlags2;
	uint32 nSaveStates     = m_RP.m_StateAnd;
	void*  pCustomData     = m_RP.m_pRE->m_CustomData;

	float fDistToCam = 500.0f;
	float fDist      = CV_r_detaildistance;
	for (int nRE = 0; nRE <= m_RP.m_nLastRE; nRE++)
	{
		s_tempRIs.SetUse(0);

		m_RP.m_pRE = m_RP.m_RIs[nRE][0]->pElem;

		if (m_RP.m_pRE)
		{
			CRenderObject* pObj = m_RP.m_pCurObject;
			uint32 nObj;

			for (nObj = 0; nObj < m_RP.m_RIs[nRE].Num(); nObj++)
			{
				pObj = m_RP.m_RIs[nRE][nObj]->pObj;
				//float fDistObj = pObj->m_fDistance;
				//if ( fDistObj <= fDist+4.0f )
				s_tempRIs.AddElem(m_RP.m_RIs[nRE][nObj]);
			}
		} //
		else
			continue;

		if (!s_tempRIs.Num())
			continue;

		m_RP.m_pRE->mfPrepare(false);
		int nSaveLastRE = m_RP.m_nLastRE;
		m_RP.m_nLastRE = 0;
		TArray<SRendItem*> saveArr;
		saveArr.Assign(m_RP.m_RIs[0]);
		m_RP.m_RIs[0].Assign(s_tempRIs);
		CRenderObject* pSaveObject = m_RP.m_pCurObject;
		m_RP.m_pCurObject      = m_RP.m_RIs[0][0]->pObj;
		m_RP.m_FlagsShader_MD &= ~HWMD_TEXCOORD_FLAG_MASK;

		Vec4 data[4];
		m_RP.m_pRE->m_CustomData = &data[0].x;

		SEfResTexture* rt = m_RP.m_pShaderResources->m_Textures[EFTT_CUSTOM];

		const float fDefaultUVTilling = 20;
		float fUScale                 = rt->GetTiling(0);
		float fVScale                 = rt->GetTiling(1);
		if (!fUScale) fUScale = fDefaultUVTilling;
		if (!fVScale) fVScale = fDefaultUVTilling;

		data[0].z = 0;
		if (rt->m_Ext.m_pTexModifier)
			data[0].z = rt->m_Ext.m_pTexModifier->m_OscRate[0];
		data[0].x = fUScale;
		data[0].y = fVScale;
		if (!data[0].z)
			data[0].z = 1.0f;

		// Flicker timming for sparks/plasma
		float fTime = m_RP.m_TI[m_RP.m_nProcessThreadID].m_RealTime;
		data[0].w  = (fTime * 20.0f + fTime * 4.0f);
		data[0].w -= floorf(data[0].w);
		data[0].w  = fabs(data[0].w * 2.0f - 1.0f);
		data[0].w *= data[0].w;

		int32 nMaterialStatePrevOr  = m_RP.m_MaterialStateOr;
		int32 nMaterialStatePrevAnd = m_RP.m_MaterialStateAnd;
		m_RP.m_MaterialStateAnd = GS_BLEND_MASK;
		m_RP.m_MaterialStateOr  = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA;

		m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] | g_HWSR_MaskBit[HWSR_SAMPLE4]);

		// Set sample flags
		SRenderObjData* pRenderObjectData = m_RP.m_pCurObject->GetObjData();
		if (pRenderObjectData)
		{
			uint32 layerEffectParams = pRenderObjectData->m_pLayerEffectParams;

			// m_pLayerEffectParams "x" channel
			if (((layerEffectParams & 0xff000000) >> 24) > 0)
			{
				m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
				data[0].z             *= CRenderer::CV_r_maxSuitPulseSpeedMultiplier;
			}

			// m_pLayerEffectParams "y" channel
			if ((((layerEffectParams & 0x00ff0000) >> 16) > 0) ||
			    (((layerEffectParams & 0x0000ff00) >> 8) > 0)) // Z channel used for "hit effect"
			{
				m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
				data[0].z             *= CRenderer::CV_r_armourPulseSpeedMultiplier;
			}

			// m_pLayerEffectParams "w" channel
			if (((layerEffectParams & 0x000000ff)) > 0)
			{
				m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
			}
		}

		FX_DrawTechnique(sh, pTech);

		m_RP.m_RIs[0].Assign(saveArr);
		saveArr.ClearArr();

		m_RP.m_nLastRE          = nSaveLastRE;
		m_RP.m_pCurObject       = pSaveObject;
		m_RP.m_pPrevObject      = NULL;
		m_RP.m_MaterialStateAnd = nMaterialStatePrevAnd;
		m_RP.m_MaterialStateOr  = nMaterialStatePrevOr;
	}

	m_RP.m_pRE = pRE;

	m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags = nSavePersFlags;
	m_RP.m_PersFlags2        = nSavePersFlags2;
	m_RP.m_StateAnd          = nSaveStates;
	m_RP.m_FlagsShader_RT    = nSaveRT;
	m_RP.m_FlagsShader_MD    = nSaveMD;
	m_RP.m_pRE->m_CustomData = pCustomData;
}

// deprecated (cannot remove at this stage due to cloak effect) - maybe can batch into FX_DrawEffectLayerPasses (?)
void CD3D9Renderer::FX_DrawMultiLayers()
{
	// Verify if current mesh has valid data for layers
	CREMeshImpl* pRE = (CREMeshImpl*) m_RP.m_pRE;
	if (!m_RP.m_pShader || !m_RP.m_pShaderResources || !m_RP.m_pCurObject->m_nMaterialLayers)
		return;

	IMaterial* pObjMat = m_RP.m_pCurObject->m_pCurrMaterial;
	if ((IsRecursiveRenderView() && !(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_MAKESPRITE)) || !m_RP.m_pShaderResources || !pObjMat)
		return;

	if (m_RP.m_PersFlags2 & (RBPF2_CUSTOM_RENDER_PASS | RBPF2_THERMAL_RENDERMODE_PASS | RBPF2_MOTIONBLURPASS))
		return;

	CRenderChunk* pChunk = pRE->m_pChunk;
	if (!pChunk)
	{
		assert(pChunk);
		return;
	}

	// Check if chunk material has layers at all
	IMaterial* pDefaultMtl = gEnv->p3DEngine->GetMaterialManager()->GetDefaultLayersMaterial();
	IMaterial* pCurrMtl    = pObjMat->GetSubMtlCount() ? pObjMat->GetSubMtl(pChunk->m_nMatID) : pObjMat;
	if (!pCurrMtl || !pDefaultMtl || (pCurrMtl->GetFlags() & MTL_FLAG_NODRAW))
		return;

	uint32 nLayerCount = pDefaultMtl->GetLayerCount();
	if (!nLayerCount)
		return;

	// Start multi-layers processing
	PROFILE_FRAME(DrawShader_MultiLayers);

	if (m_LogFile)
		Logv("*** Start Multilayers processing ***\n");

	for (int nRE = 0; nRE <= m_RP.m_nLastRE; nRE++)
	{
		m_RP.m_pRE = m_RP.m_RIs[nRE][0]->pElem;

		// Render all layers
		for (uint32 nCurrLayer(0); nCurrLayer < nLayerCount; ++nCurrLayer)
		{
			IMaterialLayer* pLayer        = const_cast< IMaterialLayer* >(pCurrMtl->GetLayer(nCurrLayer));
			IMaterialLayer* pDefaultLayer = const_cast< IMaterialLayer* >(pDefaultMtl->GetLayer(nCurrLayer));
			bool bDefaultLayer            = false;
			if (!pLayer)
			{
				// Replace with default layer
				pLayer        = pDefaultLayer;
				bDefaultLayer = true;

				if (!pLayer)
					continue;
			}

			if (!pLayer->IsEnabled() || pLayer->DoesFadeOut())
				continue;

			// Set/verify layer shader technique
			SShaderItem& pCurrShaderItem = pLayer->GetShaderItem();
			CShader* pSH                 = static_cast<CShader*>(pCurrShaderItem.m_pShader);
			if (!pSH || pSH->m_HWTechniques.empty())
				continue;

			SShaderTechnique* pTech = pSH->m_HWTechniques[0];
			if (!pTech)
			{
				continue;
			}

			// Re-create render object list, based on layer properties
			{
				s_tempRIs.SetUse(0);

				float fDistToCam    = 500.0f;
				float fDist         = CV_r_detaildistance;
				CRenderObject* pObj = m_RP.m_pCurObject;
				uint32 nObj         = 0;

				for (nObj = 0; nObj < m_RP.m_RIs[nRE].Num(); nObj++)
				{
					pObj = m_RP.m_RIs[nRE][nObj]->pObj;
					uint8 nMaterialLayers = 0;
					nMaterialLayers |= ((pObj->m_nMaterialLayers & MTL_LAYER_BLEND_DYNAMICFROZEN)) ? MTL_LAYER_FROZEN : 0;
					const uint8 mtlLayerBlendCloak = ((pObj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) >> 8);
					if (mtlLayerBlendCloak)
					{
						nMaterialLayers |= MTL_LAYER_CLOAK;
						const bool bCloakInTransition = (mtlLayerBlendCloak < 255) ? true : false;
						if (!bCloakInTransition)
						{
							// Disable cloak sparks layer when not in transition (only visible in transition)
							m_RP.m_CurPassBitMask |= 2;
						}
					}

					if (nMaterialLayers & (1 << nCurrLayer))
						s_tempRIs.AddElem(m_RP.m_RIs[nRE][nObj]);
				}

				// nothing in render list
				if (!s_tempRIs.Num())
					continue;
			}

			SShaderItem& pMtlShaderItem = pCurrMtl->GetShaderItem();
			int nSaveLastRE             = m_RP.m_nLastRE;
			m_RP.m_nLastRE = 0;

			SEfResTexture** pResourceTexs = ((CShaderResources*)pCurrShaderItem.m_pShaderResources)->m_Textures;
			SEfResTexture*  pPrevLayerResourceTexs[EFTT_MAX];

			if (bDefaultLayer)
			{
				// Keep layer resources and replace with resources from base shader
				for (int t = 0; t < EFTT_MAX; ++t)
				{
					pPrevLayerResourceTexs[t] = ((CShaderResources*)pCurrShaderItem.m_pShaderResources)->m_Textures[t];
					((CShaderResources*)pCurrShaderItem.m_pShaderResources)->m_Textures[t] = m_RP.m_pShaderResources->m_Textures[t];
				}
			}

			m_RP.m_pRE->mfPrepare(false);

			// Store current rendering data
			TArray<SRendItem*> pPrevRenderObjLst;
			pPrevRenderObjLst.Assign(m_RP.m_RIs[0]);
			CRenderObject* pPrevObject             = m_RP.m_pCurObject;
			CShaderResources* pPrevShaderResources = m_RP.m_pShaderResources;
			CShader* pPrevSH                       = m_RP.m_pShader;
			uint32   nPrevNumRendPasses            = m_RP.m_nNumRendPasses;
			uint64   nFlagsShaderRTprev            = m_RP.m_FlagsShader_RT;

			SShaderTechnique* pPrevRootTech = m_RP.m_pRootTechnique;
			m_RP.m_pRootTechnique = pTech;

			int nMaterialStatePrevOr  = m_RP.m_MaterialStateOr;
			int nMaterialStatePrevAnd = m_RP.m_MaterialStateAnd;
			uint32 nFlagsShaderLTprev = m_RP.m_FlagsShader_LT;

			int  nPersFlagsPrev        = m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags;
			int  nPersFlags2Prev       = m_RP.m_PersFlags2;
			int  nMaterialAlphaRefPrev = m_RP.m_MaterialAlphaRef;
			bool bIgnoreObjectAlpha    = m_RP.m_bIgnoreObjectAlpha;
			m_RP.m_bIgnoreObjectAlpha = true;

			m_RP.m_pShader = pSH;
			m_RP.m_RIs[0].Assign(s_tempRIs);
			m_RP.m_pCurObject       = m_RP.m_RIs[0][0]->pObj;
			m_RP.m_pPrevObject      = NULL;
			m_RP.m_pShaderResources = (CShaderResources*)pCurrShaderItem.m_pShaderResources;

			// Reset light passes (need ambient)
			m_RP.m_nNumRendPasses = 0;
			m_RP.m_PersFlags2    |= RBPF2_MATERIALLAYERPASS;

			if ((1 << nCurrLayer) & MTL_LAYER_FROZEN)
			{
				m_RP.m_MaterialStateAnd = GS_BLEND_MASK | GS_ALPHATEST;
				m_RP.m_MaterialStateOr  = GS_BLSRC_ONE | GS_BLDST_ONE;
				m_RP.m_MaterialAlphaRef = 0xff;
			}

			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];

			FX_DrawTechnique(pSH, pTech);

			// Restore previous rendering data
			m_RP.m_RIs[0].Assign(pPrevRenderObjLst);
			pPrevRenderObjLst.ClearArr();
			m_RP.m_pShader          = pPrevSH;
			m_RP.m_pShaderResources = pPrevShaderResources;
			m_RP.m_pCurObject       = pPrevObject;
			m_RP.m_pPrevObject      = NULL;
			m_RP.m_PersFlags2       = nPersFlags2Prev;
			m_RP.m_nLastRE          = nSaveLastRE;

			m_RP.m_nNumRendPasses = nPrevNumRendPasses;

			m_RP.m_FlagsShader_LT = nFlagsShaderLTprev;
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags = nPersFlagsPrev;
			m_RP.m_FlagsShader_RT = nFlagsShaderRTprev;

			m_RP.m_nNumRendPasses = 0;

			m_RP.m_pRootTechnique     = pPrevRootTech;
			m_RP.m_bIgnoreObjectAlpha = bIgnoreObjectAlpha;
			m_RP.m_MaterialStateOr    = nMaterialStatePrevOr;
			m_RP.m_MaterialStateAnd   = nMaterialStatePrevAnd;
			m_RP.m_MaterialAlphaRef   = nMaterialAlphaRefPrev;

			if (bDefaultLayer)
			{
				for (int t = 0; t < EFTT_MAX; ++t)
				{
					((CShaderResources*)pCurrShaderItem.m_pShaderResources)->m_Textures[t] = pPrevLayerResourceTexs[t];
					pPrevLayerResourceTexs[t] = 0;
				}
			}

			//break; // only 1 layer allowed
		}
	}

	m_RP.m_pRE = pRE;

	if (m_LogFile)
		Logv("*** End Multilayers processing ***\n");
}

void CD3D9Renderer::FX_SelectTechnique(CShader* pShader, SShaderTechnique* pTech)
{
	SShaderTechniqueStat Stat;
	Stat.pTech   = pTech;
	Stat.pShader = pShader;
	if (pTech->m_Passes.Num())
	{
		SShaderPass* pPass = &pTech->m_Passes[0];
		if (pPass->m_PShader && pPass->m_VShader)
		{
			Stat.pVS     = (CHWShader_D3D*)pPass->m_VShader;
			Stat.pPS     = (CHWShader_D3D*)pPass->m_PShader;
			Stat.pVSInst = Stat.pVS->m_pCurInst;
			Stat.pPSInst = Stat.pPS->m_pCurInst;
			g_SelectedTechs.push_back(Stat);
		}
	}
}

void CD3D9Renderer::FX_DrawTechnique(CShader* ef, SShaderTechnique* pTech)
{
	FUNCTION_PROFILER_RENDER_FLAT
	switch (ef->m_eSHDType)
	{
	case eSHDT_General:
		FX_DrawShader_General(ef, pTech);
		break;
	case eSHDT_Light:
	case eSHDT_Terrain:
		FX_DrawShader_General(ef, pTech);
		break;
	case eSHDT_Fur:
		break;
	case eSHDT_CustomDraw:
	case eSHDT_Sky:
		if (m_RP.m_pRE && (m_RP.m_nRendFlags & SHDF_ALLOWHDR))
		{
			EF_Scissor(false, 0, 0, 0, 0);
			if (pTech && pTech->m_Passes.Num())
				m_RP.m_pRE->mfDraw(ef, &pTech->m_Passes[0]);
			else
				m_RP.m_pRE->mfDraw(ef, NULL);
		}
		break;
	default:
		assert(0);
	}
	if (m_RP.m_ObjFlags & FOB_SELECTED)
		FX_SelectTechnique(ef, pTech);
}

#if defined(HW_INSTANCING_ENABLED)
static void sDetectInstancing(CShader* pShader, CRenderObject* pObj)
{
	CRenderer* rd = gRenDev;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	if (!CRenderer::CV_r_geominstancing || rd->m_bUseGPUFriendlyBatching[rRP.m_nProcessThreadID] || !(pShader->m_Flags & EF_SUPPORTSINSTANCING) || CRenderer::CV_r_measureoverdraw ||
	  // don't instance in motion blur pass or post 3d render
	  rRP.m_PersFlags2 & RBPF2_POST_3D_RENDERER_PASS ||        //rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS ||
	                                                           // only instance meshes
	  !rRP.m_pRE || rRP.m_pRE->mfGetType() != eDATA_Mesh
	  )
	{
		rRP.m_FlagsPerFlush &= ~RBSI_INSTANCED;
		return;
	}

	int i = 0, nLastRE = rRP.m_nLastRE;
	for (; i <= nLastRE; i++)
	{
		int nRIs = rRP.m_RIs[i].Num();

		// instance even with conditional rendering - && RIs[0]->nOcclQuery<0
		if (nRIs > CRenderer::m_iGeomInstancingThreshold || (rRP.m_FlagsPerFlush & RBSI_INSTANCED))
		{
			rRP.m_FlagsPerFlush |= RBSI_INSTANCED;
			break;
		}
	}
	if (i > rRP.m_nLastRE)
		rRP.m_FlagsPerFlush &= ~RBSI_INSTANCED;
}
#endif

// Set/Restore shader resources overrided states
bool CD3D9Renderer::FX_SetResourcesState()
{
	FUNCTION_PROFILER_RENDER_FLAT
	if (!m_RP.m_pShader)
		return false;
	m_RP.m_MaterialStateOr  = 0;
	m_RP.m_MaterialStateAnd = 0;
	if (!m_RP.m_pShaderResources)
		return true;

	PrefetchLine(m_RP.m_pShaderResources, 0);    //Shader Resources fit in a cache line, but they're not 128-byte aligned! We are likely going
	PrefetchLine(m_RP.m_pShaderResources, 124);  //	to cache miss on access to m_ResFlags but will hopefully avoid later ones

	if (m_RP.m_pShader->m_Flags2 & EF2_IGNORERESOURCESTATES)
		return true;

	m_RP.m_ShaderTexResources[EFTT_DECAL_OVERLAY] = NULL;

	const CShaderResources* pRes = m_RP.m_pShaderResources;
	const uint32 uResFlags       = pRes->m_ResFlags;
	if (uResFlags & MTL_FLAG_NOTINSTANCED)
		m_RP.m_FlagsPerFlush &= ~RBSI_INSTANCED;

	bool bRes = true;
	if (uResFlags & MTL_FLAG_2SIDED)
	{
		D3DSetCull(eCULL_None);
		m_RP.m_FlagsPerFlush |= RBSI_LOCKCULL;
	}

	if (pRes->IsAlphaTested())
	{
		if (!(m_RP.m_PersFlags2 & RBPF2_NOALPHATEST))
		{
			const int nAlphaRef = int(pRes->GetAlphaRef() * 255.0f);

			m_RP.m_MaterialAlphaRef = nAlphaRef;
			m_RP.m_MaterialStateOr = GS_ALPHATEST | GS_DEPTHWRITE;
			m_RP.m_MaterialStateAnd = GS_ALPHATEST;
		}
		else
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];
	}

	if (pRes->IsTransparent())
	{
		if (!(m_RP.m_PersFlags2 & RBPF2_NOALPHABLEND))
		{
			const float fOpacity = pRes->GetStrengthValue(EFTT_OPACITY);

			m_RP.m_MaterialStateAnd |= GS_DEPTHWRITE | GS_BLEND_MASK;
			m_RP.m_MaterialStateOr  &= ~GS_DEPTHWRITE;
			if (uResFlags & MTL_FLAG_ADDITIVE)
			{
				m_RP.m_MaterialStateOr  |= GS_BLSRC_ONE | GS_BLDST_ONE;
				m_RP.m_CurGlobalColor[0] = fOpacity;
				m_RP.m_CurGlobalColor[1] = fOpacity;
				m_RP.m_CurGlobalColor[2] = fOpacity;
			}
			else
			{
				m_RP.m_MaterialStateOr  |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
				m_RP.m_CurGlobalColor[3] = fOpacity;
			}
			m_RP.m_fCurOpacity = fOpacity;
		}
	}
	// Specific condition for cloak transition
	if ((m_RP.m_pCurObject->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) && !m_RP.m_bIgnoreObjectAlpha)
	{
		uint32 nResourcesNoDrawFlags = pRes->GetMtlLayerNoDrawFlags();
		if (!(nResourcesNoDrawFlags & MTL_LAYER_CLOAK))
		{
			const float fCloakMinThreshold = 0.85f;
			if ((((m_RP.m_pCurObject->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) >> 8) / 255.0f) > fCloakMinThreshold)
			{
				m_RP.m_MaterialStateAnd |= GS_BLEND_MASK;
				m_RP.m_MaterialStateOr  |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
				m_RP.m_MaterialStateOr  |= (!IsCustomRenderModeEnabled(eRMF_NIGHTVISION) ? GS_DEPTHWRITE : 0);
			}
		}
	}

	if (!(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_MAKESPRITE))
	{
		if (pRes->m_pDeformInfo)
			m_RP.m_FlagsShader_MDV |= pRes->m_pDeformInfo->m_eType;
		m_RP.m_FlagsShader_MDV |= m_RP.m_pCurObject->m_nMDV | m_RP.m_pShader->m_nMDV;
		if (m_RP.m_ObjFlags & FOB_OWNER_GEOMETRY)
			m_RP.m_FlagsShader_MDV &= ~MDV_DEPTH_OFFSET;
	}

	if ((m_RP.m_ObjFlags & FOB_BLEND_WITH_TERRAIN_COLOR) && m_RP.m_pCurObject->m_nTextureID < 0)
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];
	else
		m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

	//if (m_RP.m_PersFlags1 & RBPF1_CAPTURE_INSTANCED)
	//	m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP];

	return true;
}

//===================================================================================================

static void sBatchStats(SRenderPipeline& rRP)
{
#if defined(ENABLE_PROFILING_CODE)
	SPipeStat& rPS = rRP.m_PS[rRP.m_nProcessThreadID];
	rPS.m_NumRendMaterialBatches++;
	rPS.m_NumRendGeomBatches += rRP.m_nLastRE + 1;
	for (int i = 0; i <= rRP.m_nLastRE; i++)
	{
		rPS.m_NumRendInstances += rRP.m_RIs[i].Num();
	}
#endif
}

static void sLogFlush(const char* str, CShader* pSH, SShaderTechnique* pTech)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	if (rd->m_LogFile)
	{
		SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
		rd->Logv("%s: '%s.%s', Id:%d, ResId:%d, Obj:%d, VF:%d\n", str, pSH->GetName(), pTech ? pTech->m_NameStr.c_str() : "Unknown", pSH->GetID(), rRP.m_pShaderResources ? rRP.m_pShaderResources->m_Id : -1, rRP.m_pCurObject->m_Id, pSH->m_eVertexFormat);
		if (rRP.m_ObjFlags & FOB_SELECTED)
		{
      if (rRP.m_MaterialStateOr & GS_ALPHATEST)
				rd->Logv("  %.3f, %.3f, %.3f (0x%llx), (AT) (Selected)\n", rRP.m_pCurObject->m_II.m_Matrix(0, 3), rRP.m_pCurObject->m_II.m_Matrix(1, 3), rRP.m_pCurObject->m_II.m_Matrix(2, 3), rRP.m_pCurObject->m_ObjFlags);
			else if (rRP.m_MaterialStateOr & GS_BLEND_MASK)
				rd->Logv("  %.3f, %.3f, %.3f (0x%llx) (AB) (Dist: %.3f) (Selected)\n", rRP.m_pCurObject->m_II.m_Matrix(0, 3), rRP.m_pCurObject->m_II.m_Matrix(1, 3), rRP.m_pCurObject->m_II.m_Matrix(2, 3), rRP.m_pCurObject->m_ObjFlags, rRP.m_pCurObject->m_fDistance);
			else
				rd->Logv("  %.3f, %.3f, %.3f (0x%llx), RE: 0x%x (Selected)\n", rRP.m_pCurObject->m_II.m_Matrix(0, 3), rRP.m_pCurObject->m_II.m_Matrix(1, 3), rRP.m_pCurObject->m_II.m_Matrix(2, 3), rRP.m_pCurObject->m_ObjFlags, rRP.m_pRE);
		}
		else
		{
      if (rRP.m_MaterialStateOr & GS_ALPHATEST)
				rd->Logv("  %.3f, %.3f, %.3f (0x%llx) (AT), Inst: %d, RE: 0x%x (Dist: %.3f)\n", rRP.m_pCurObject->m_II.m_Matrix(0, 3), rRP.m_pCurObject->m_II.m_Matrix(1, 3), rRP.m_pCurObject->m_II.m_Matrix(2, 3), rRP.m_pCurObject->m_ObjFlags, rRP.m_RIs[0].Num(), rRP.m_pRE, rRP.m_pCurObject->m_fDistance);
			else if (rRP.m_MaterialStateOr & GS_BLEND_MASK)
				rd->Logv("  %.3f, %.3f, %.3f (0x%llx) (AB), Inst: %d, RE: 0x%x (Dist: %.3f)\n", rRP.m_pCurObject->m_II.m_Matrix(0, 3), rRP.m_pCurObject->m_II.m_Matrix(1, 3), rRP.m_pCurObject->m_II.m_Matrix(2, 3), rRP.m_pCurObject->m_ObjFlags, rRP.m_RIs[0].Num(), rRP.m_pRE, rRP.m_pCurObject->m_fDistance);
			else
				rd->Logv("  %.3f, %.3f, %.3f (0x%llx), Inst: %d, RE: 0x%x\n", rRP.m_pCurObject->m_II.m_Matrix(0, 3), rRP.m_pCurObject->m_II.m_Matrix(1, 3), rRP.m_pCurObject->m_II.m_Matrix(2, 3), rRP.m_pCurObject->m_ObjFlags, rRP.m_RIs[0].Num(), rRP.m_pRE);
		}
		if (rRP.m_pRE && rRP.m_pRE->mfGetType() == eDATA_Mesh)
		{
			CREMeshImpl* pRE = (CREMeshImpl*) rRP.m_pRE;
			CRenderMesh* pRM = pRE->m_pRenderMesh;
			if (pRM && pRM->m_Chunks.size() && pRM->m_sSource)
			{
				int nChunk = -1;
				for (int i = 0; i < pRM->m_Chunks.size(); i++)
				{
					CRenderChunk* pCH = &pRM->m_Chunks[i];
					if (pCH->pRE == pRE)
					{
						nChunk = i;
						break;
					}
				}
				rd->Logv("  Mesh: %s (Chunk: %d)\n", pRM->m_sSource.c_str(), nChunk);
			}
		}
	}
}

void CD3D9Renderer::FX_RefractionPartialResolve()
{
	CD3D9Renderer* const __restrict rd      = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;

	{
		int nThreadID           = rRP.m_nProcessThreadID;
		SRenderObjData* ObjData = rRP.m_pCurObject->GetObjData();

		if (ObjData)
		{
			uint8 screenBounds[4];

			screenBounds[0] = ObjData->m_screenBounds[0];
			screenBounds[1] = ObjData->m_screenBounds[1];
			screenBounds[2] = ObjData->m_screenBounds[2];
			screenBounds[3] = ObjData->m_screenBounds[3];

			float boundsI2F[] = {
				(float)(screenBounds[0] << 4),    (float)(screenBounds[1] << 4),
				(float)(min(screenBounds[2] << 4, GetWidth())),                    (float)(min(screenBounds[3] << 4, GetHeight()))
			};

			if (CVrProjectionManager::IsMultiResEnabledStatic())
			{
				CVrProjectionManager::Instance()->MapScreenPosToMultiRes(boundsI2F[0], boundsI2F[1]);
				CVrProjectionManager::Instance()->MapScreenPosToMultiRes(boundsI2F[2], boundsI2F[3]);
			}

			if (((boundsI2F[2] - boundsI2F[0] > 0) && (boundsI2F[3] - boundsI2F[1] > 0)) &&
			  !((rRP.m_nCurrResolveBounds[0] == screenBounds[0])
			  && (rRP.m_nCurrResolveBounds[1] == screenBounds[1])
			  && (rRP.m_nCurrResolveBounds[2] == screenBounds[2])
			  && (rRP.m_nCurrResolveBounds[3] == screenBounds[3])))
			{

				rRP.m_nCurrResolveBounds[0] = screenBounds[0];
				rRP.m_nCurrResolveBounds[1] = screenBounds[1];
				rRP.m_nCurrResolveBounds[2] = screenBounds[2];
				rRP.m_nCurrResolveBounds[3] = screenBounds[3];

				int boundsF2I[] = {
					int(boundsI2F[0] * m_RP.m_CurDownscaleFactor.x),
					int(boundsI2F[1] * m_RP.m_CurDownscaleFactor.y),
					int(boundsI2F[2] * m_RP.m_CurDownscaleFactor.x),
					int(boundsI2F[3] * m_RP.m_CurDownscaleFactor.y)
				};

				int currScissorX, currScissorY, currScissorW, currScissorH;
				CTexture* pTarget = CTexture::s_ptexCurrSceneTarget;

				//cache RP states  - Probably a bit excessive, but want to be safe
				CShaderResources* currRes       = rd->m_RP.m_pShaderResources;
				CShader* currShader             = rd->m_RP.m_pShader;
				int currShaderTechnique         = rd->m_RP.m_nShaderTechnique;
				SShaderTechnique* currTechnique = rd->m_RP.m_pCurTechnique;
				uint32 currCommitFlags          = rd->m_RP.m_nCommitFlags;
				uint32 currFlagsShaderBegin     = rd->m_RP.m_nFlagsShaderBegin;
				ECull  currCull                 = m_RP.m_eCull;

				float currVPMinZ = rd->m_NewViewport.fMinZ;   // Todo: Add to GetViewport / SetViewport
				float currVPMaxZ = rd->m_NewViewport.fMaxZ;

				D3DSetCull(eCULL_None);

				bool bScissored = EF_GetScissorState(currScissorX, currScissorY, currScissorW, currScissorH);

				int newScissorX = int(boundsF2I[0]);
				int newScissorY = int(boundsF2I[1]);
				int newScissorW = max(0, min(int(boundsF2I[2]), GetWidth()) - newScissorX);
				int newScissorH = max(0, min(int(boundsF2I[3]), GetHeight()) - newScissorY);

				EF_Scissor(true, newScissorX, newScissorY, newScissorW, newScissorH);

				FX_ScreenStretchRect(pTarget);

				EF_Scissor(bScissored, currScissorX, currScissorY, currScissorW, currScissorH);

				D3DSetCull(currCull);

				//restore RP states
				rd->m_RP.m_pShaderResources  = currRes;
				rd->m_RP.m_pShader           = currShader;
				rd->m_RP.m_nShaderTechnique  = currShaderTechnique;
				rd->m_RP.m_pCurTechnique     = currTechnique;
				rd->m_RP.m_nCommitFlags      = currCommitFlags | FC_MATERIAL_PARAMS;
				rd->m_RP.m_nFlagsShaderBegin = currFlagsShaderBegin;
				rd->m_NewViewport.fMaxZ      = currVPMinZ;
				rd->m_NewViewport.fMaxZ      = currVPMaxZ;

#if REFRACTION_PARTIAL_RESOLVE_STATS
				{
					const int x1 = (screenBounds[0] << 4);
					const int y1 = (screenBounds[1] << 4);
					const int x2 = (screenBounds[2] << 4);
					const int y2 = (screenBounds[3] << 4);

					const int resolveWidth      = x2 - x1;
					const int resolveHeight     = y2 - y1;
					const int resolvePixelCount = resolveWidth * resolveHeight;

					// Update stats
					SPipeStat& pipeStat = rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID];
					pipeStat.m_refractionPartialResolveCount++;
					pipeStat.m_refractionPartialResolvePixelCount += resolvePixelCount;

					const float resolveCostConversion = 18620398.0f;

					pipeStat.m_fRefractionPartialResolveEstimatedCost += ((float)resolvePixelCount / resolveCostConversion);

#if REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS
					if (CRenderer::CV_r_RefractionPartialResolvesDebug == eRPR_DEBUG_VIEW_2D_AREA || CRenderer::CV_r_RefractionPartialResolvesDebug == eRPR_DEBUG_VIEW_2D_AREA_OVERLAY)
					{
						// Render 2d areas additively on screen
						IRenderAuxGeom* pAuxRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
						if (pAuxRenderer)
						{
							SAuxGeomRenderFlags oldRenderFlags = pAuxRenderer->GetRenderFlags();

							SAuxGeomRenderFlags newRenderFlags;
							newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
							newRenderFlags.SetAlphaBlendMode(e_AlphaAdditive);
							newRenderFlags.SetMode2D3DFlag(e_Mode2D);
							pAuxRenderer->SetRenderFlags(newRenderFlags);

							const float screenWidth  = (float)GetWidth();
							const float screenHeight = (float)GetHeight();

							// Calc resolve area
							const float left   = x1 / screenWidth;
							const float top    = y1 / screenHeight;
							const float right  = x2 / screenWidth;
							const float bottom = y2 / screenHeight;

							// Render resolve area
							ColorB areaColor(20, 0, 0, 255);

							if (CRenderer::CV_r_RefractionPartialResolvesDebug == eRPR_DEBUG_VIEW_2D_AREA_OVERLAY)
							{
								int val = (pipeStat.m_refractionPartialResolveCount) % 3;
								areaColor = ColorB((val == 0) ? 0 : 128, (val == 1)  ? 0 : 128, (val == 2)  ? 0 : 128, 255);
							}

							const uint vertexCount       = 6;
							const Vec3 vert[vertexCount] = {
								Vec3(left,  top,     0.0f),
								Vec3(left,  bottom,  0.0f),
								Vec3(right, top,     0.0f),
								Vec3(left,  bottom,  0.0f),
								Vec3(right, bottom,  0.0f),
								Vec3(right, top,     0.0f)
							};
							pAuxRenderer->DrawTriangles(vert, vertexCount, areaColor);

							// Set previous Aux render flags back again
							pAuxRenderer->SetRenderFlags(oldRenderFlags);
						}
					}
#endif              // REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS
				}
#endif          // REFRACTION_PARTIAL_RESOLVE_STATS
			}
		}
	}
}

bool CD3D9Renderer::FX_UpdateAnimatedShaderResources(CShaderResources* shaderResources)
{
	bool bUpdated = false;

	// update dynamic texture sources based on a shared RT only
	const CShaderResources* rsr = shaderResources;
	if (rsr && gRenDev->m_CurRenderEye == LEFT_EYE)
	{
		// TODO: optimize search (flagsfield?)
		for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
		{
			if (SEfResTexture* pTex = rsr->m_Textures[texType])
			{
				// Update() returns true if texture has been swapped out by a different one
				if (pTex->m_Sampler.Update())
				{
					bUpdated = true;
				}
			}
		}
	}

	if (bUpdated)
	{
		// Don't register the new texture, just trigger RT_UpdateResourceSet
		shaderResources->m_resources.MarkBindingChanged();
	}

	return bUpdated;
}

bool CD3D9Renderer::FX_UpdateDynamicShaderResources(const CShaderResources* shaderResources, uint32 batchFilter, uint32 flags2)
{
	bool bUpdated = false;

	// update dynamic texture sources based on a shared RT only
	const CShaderResources* rsr = shaderResources;
	if (rsr && gRenDev->m_CurRenderEye == LEFT_EYE)
	{
		// TODO: optimize search (flagsfield?)
		for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
		{
			const SEfResTexture* pTex = rsr->m_Textures[texType];
			if (pTex)
			{
				IDynTextureSourceImpl* pDynTexSrc = (IDynTextureSourceImpl*)pTex->m_Sampler.m_pDynTexSource;
				if (pDynTexSrc)
				{
					if (pDynTexSrc->GetSourceType() == IDynTextureSource::DTS_I_FLASHPLAYER)
					{
						if (flags2 & RBPF2_MOTIONBLURPASS)
							return false; // explicitly disable motion blur pass for flash assets

						if (batchFilter & (FB_GENERAL | FB_TRANSPARENT))
						{
							// save pipeline state
							SRenderPipeline& rp = gcpRendD3D->m_RP;                   // !!! don't use rRP as it's taken from rd (__restrict); the compiler will optimize the save/restore instructions !!!
							CShader* const pPrevShader = rp.m_pShader;
							const int prevShaderTechnique = rp.m_nShaderTechnique;
							CShaderResources* const prevShaderResources = rp.m_pShaderResources;
							SShaderTechnique* const prevCurTechnique = rp.m_pCurTechnique;
							const uint32 prevCommitFlags = rp.m_nCommitFlags;
							const uint32 prevFlagsShaderBegin = rp.m_nFlagsShaderBegin;

							bUpdated = bUpdated | pDynTexSrc->Update();

							// restore pipeline state
							rp.m_pShader = pPrevShader;
							rp.m_nShaderTechnique = prevShaderTechnique;
							rp.m_pShaderResources = prevShaderResources;
							rp.m_pCurTechnique = prevCurTechnique;
							rp.m_nCommitFlags = prevCommitFlags;
							rp.m_nFlagsShaderBegin = prevFlagsShaderBegin;
							gcpRendD3D->m_CurViewport.fMaxZ = -1;
						}
					}
				}
			}
		}
	}

	return bUpdated | true;
}

// Flush current render item
void CD3D9Renderer::FX_FlushShader_General()
{
	FUNCTION_PROFILER_RENDER_FLAT
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	if (!rRP.m_pRE && !rRP.m_RendNumVerts)
		return;

	CShader* ef = rRP.m_pShader;
	if (!ef)
		return;

	const CShaderResources* rsr = rRP.m_pShaderResources;
	if (!FX_UpdateDynamicShaderResources(rsr, rRP.m_nBatchFilter, rRP.m_PersFlags2))
		return;

	if ((ef->m_Flags & EF_SUPPORTSDEFERREDSHADING_FULL) && (rRP.m_PersFlags2 & RBPF2_FORWARD_SHADING_PASS) && (!rsr->IsEmissive()))
		return;

	// don't do motion vector generation or reflections on refractive objects.
	// Need to check here since refractive and transparent lists are combined for sorting.
	if (rRP.m_nPassGroupID == EFSLIST_TRANSP
	  && rRP.m_pCurObject->m_ObjFlags & FOB_REQUIRES_RESOLVE
	  && (rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS || (IsRecursiveRenderView())))
	{
		return;
	}

	// Don't render nearest transparent objects in motion blur pass, otherwise camera motion blur
	// will not be applied within the scope
	if ((rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS) &&
	  (rRP.m_nPassGroupID == EFSLIST_TRANSP) &&
	  (rRP.m_pCurObject->m_ObjFlags & FOB_NEAREST))
	{
		return;
	}

	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];
	assert(!(rRP.m_nBatchFilter & FB_Z));

	if (!rRP.m_sExcludeShader.empty())
	{
		char nm[1024];
		cry_strcpy(nm, ef->GetName());
		strlwr(nm);
		if (strstr(rRP.m_sExcludeShader.c_str(), nm))
			return;
	}
#ifdef DO_RENDERLOG
	if (rd->m_LogFile && CV_r_log == 3)
		rd->Logv("\n\n.. Start %s flush: '%s' ..\n", "General", ef->GetName());
#endif

#ifndef _RELEASE
	sBatchStats(rRP);
#endif
	CRenderObject* pObj = rRP.m_pCurObject;

	if (rd->m_RP.m_pRE)
		rd->m_RP.m_pRE = rd->m_RP.m_RIs[0][0]->pElem;

#if defined(HW_INSTANCING_ENABLED)
	sDetectInstancing(ef, pObj);
#endif

	// Techniques draw cycle...
	SShaderTechnique* __restrict pTech = ef->mfGetStartTechnique(rRP.m_nShaderTechnique);

	if (pTech)
	{
		uint32 flags = (FB_CUSTOM_RENDER | FB_MOTIONBLUR | FB_SOFTALPHATEST | FB_WATER_REFL | FB_WATER_CAUSTIC);

		if (rRP.m_pShaderResources && !(rRP.m_nBatchFilter & flags))
		{
			uint32 i;
			// Update render targets if necessary
			if (!(rTI.m_PersFlags & RBPF_DRAWTOTEXTURE))
			{
				uint32 targetNum = rRP.m_pShaderResources->m_RTargets.Num();
				const CShaderResources* const __restrict pShaderResources = rRP.m_pShaderResources;
				for (i = 0; i < targetNum; ++i)
				{
					SHRenderTarget* pTarg = pShaderResources->m_RTargets[i];
					if (pTarg->m_eOrder == eRO_PreDraw)
						rd->FX_DrawToRenderTarget(ef, rRP.m_pShaderResources, pObj, pTech, pTarg, 0, rRP.m_pRE);
				}
				targetNum = pTech->m_RTargets.Num();
				for (i = 0; i < targetNum; ++i)
				{
					SHRenderTarget* pTarg = pTech->m_RTargets[i];
					if (pTarg->m_eOrder == eRO_PreDraw)
						rd->FX_DrawToRenderTarget(ef, rRP.m_pShaderResources, pObj, pTech, pTarg, 0, rRP.m_pRE);
				}
			}
		}
		rRP.m_pRootTechnique = pTech;

		flags = (FB_MOTIONBLUR | FB_CUSTOM_RENDER | FB_SOFTALPHATEST | FB_WATER_REFL | FB_WATER_CAUSTIC);

		if (rRP.m_nBatchFilter & flags)
		{
			int nTech = -1;
			if (rRP.m_nBatchFilter & FB_MOTIONBLUR) nTech = TTYPE_MOTIONBLURPASS;
			else if (rRP.m_nBatchFilter & FB_CUSTOM_RENDER)
				nTech = TTYPE_CUSTOMRENDERPASS;
			else if (rRP.m_nBatchFilter & FB_SOFTALPHATEST)
				nTech = TTYPE_SOFTALPHATESTPASS;
			else if (rRP.m_nBatchFilter & FB_WATER_REFL)
				nTech = TTYPE_WATERREFLPASS;
			else if (rRP.m_nBatchFilter & FB_WATER_CAUSTIC)
				nTech = TTYPE_WATERCAUSTICPASS;

			if (nTech >= 0 && pTech->m_nTechnique[nTech] > 0)
			{
				assert(pTech->m_nTechnique[nTech] < (int)ef->m_HWTechniques.Num());
				pTech = ef->m_HWTechniques[pTech->m_nTechnique[nTech]];
			}
			rRP.m_nShaderTechniqueType = nTech;
		}
#ifndef _RELEASE
		if (CV_r_debugrendermode)
		{
			if (CV_r_debugrendermode & 1)
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG0];
			if (CV_r_debugrendermode & 2)
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG1];
			if (CV_r_debugrendermode & 4)
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG2];
			if (CV_r_debugrendermode & 8)
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG3];
		}
#endif
		if (!rd->FX_SetResourcesState())
			return;

		// Handle emissive materials
		CShaderResources* pCurRes = rRP.m_pShaderResources;
		if (pCurRes && pCurRes->IsEmissive() && !pCurRes->IsTransparent() && (rRP.m_PersFlags2 & RBPF2_HDR_FP16))
		{
			rRP.m_MaterialStateAnd |= GS_BLEND_MASK;
			rRP.m_MaterialStateOr   = (rRP.m_MaterialStateOr & ~GS_BLEND_MASK) | (GS_BLSRC_ONE | GS_BLDST_ONE);
		}

		if (rTI.m_PersFlags & RBPF_MAKESPRITE)
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SPRITE];
		else if (rRP.m_ObjFlags & FOB_BENDED)
			rRP.m_FlagsShader_MDV |= MDV_BENDING;
		rRP.m_FlagsShader_RT |= pObj->m_nRTMask;
		if (rRP.m_RIs[0].Num() <= 1 && !(rRP.m_ObjFlags & FOB_TRANS_MASK))
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_OBJ_IDENTITY];
#ifdef TESSELLATION_RENDERER
		if ((pObj->m_ObjFlags & FOB_NEAREST) || !(pObj->m_ObjFlags & FOB_ALLOW_TESSELLATION))
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
#endif
		if (!(rRP.m_PersFlags2 & RBPF2_NOSHADERFOG) && rTI.m_FS.m_bEnable && !(rRP.m_ObjFlags & FOB_NO_FOG) || !(rRP.m_PersFlags2 & RBPF2_ALLOW_DEFERREDSHADING))
		{
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_FOG];
			if (rd->m_bVolumetricFogEnabled && !rRP.RenderView()->IsRecursive()) // volumetric fog doesn't support recursive pass currently.
			{
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG];
			}
		}

		uint32 nThreadID = rRP.m_nProcessThreadID;

		SRenderObjData* const __restrict pOD = pObj->GetObjData();

		if (pOD && (pOD->m_nCustomFlags & COB_DISABLE_MOTIONBLUR))
		{
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NEAREST];
		}

		const uint64 objFlags = rRP.m_ObjFlags;
		if (objFlags & FOB_NEAREST)
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NEAREST];
		if (!rRP.RenderView()->IsRecursive() && CV_r_ParticlesSoftIsec)
		{
			if ((objFlags & FOB_SOFT_PARTICLE) || (rRP.m_PersFlags2 & RBPF2_HALFRES_PARTICLES))
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SOFT_PARTICLE];
		}

		if (ef->m_eShaderType == eST_Particle)
		{
			if ((objFlags & FOB_MOTION_BLUR) && CV_r_MotionBlur)
				rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_MOTION_BLUR];
			if ((objFlags & (FOB_INSHADOW)))
			{

				rd->FX_SetupShadowsForTransp();
			}
		}
		else if (ef->m_Flags2 & EF2_ALPHABLENDSHADOWS)
			rd->FX_SetupShadowsForTransp();

		if (rRP.m_pCurObject->m_RState & OS_ENVIRONMENT_CUBEMAP)
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP];

		if (rRP.m_pCurObject->m_RState & OS_ANIM_BLEND)
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ANIM_BLEND];
		if (objFlags & FOB_POINT_SPRITE)
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SPRITE];

		// Only enable for resources not using zpass
		//if (!(SRendItem::BatchFlags(rRP.m_nPassGroupID) & FB_Z) || (ef->m_Flags & EF_DECAL))
		//	rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NOZPASS];

		rRP.m_pCurTechnique = pTech;

		if ((rRP.m_nBatchFilter & (FB_MULTILAYERS | FB_LAYER_EFFECT)) && !rRP.m_pReplacementShader)
		{
			if (rRP.m_nBatchFilter & FB_LAYER_EFFECT)
				rd->FX_DrawEffectLayerPasses();

			if (rRP.m_nBatchFilter & FB_MULTILAYERS)
				rd->FX_DrawMultiLayers();
		}
		else
			rd->FX_DrawTechnique(ef, pTech);
	}//pTech
	else if (ef->m_eSHDType == eSHDT_CustomDraw)
		rd->FX_DrawTechnique(ef, 0);

#ifdef DO_RENDERLOG
	sLogFlush("Flush General", ef, pTech);
#endif
}

void CD3D9Renderer::FX_FlushShader_ZPass()
{
	CD3D9Renderer* const __restrict rd      = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	if (!rRP.m_pRE && !rRP.m_RendNumVerts)
		return;

	CShader* ef = rRP.m_pShader;
	if (!ef)
		return;

	if (!rRP.m_sExcludeShader.empty())
	{
		char nm[1024];
		cry_strcpy(nm, ef->GetName());
		strlwr(nm);
		if (strstr(rRP.m_sExcludeShader.c_str(), nm))
			return;
	}

	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];
	assert(rRP.m_nBatchFilter & (FB_Z | FB_ZPREPASS | FB_POST_3D_RENDER));
	assert(!(rTI.m_PersFlags & RBPF_MAKESPRITE));

#ifdef DO_RENDERLOG
	if (rd->m_LogFile)
	{
		if (CV_r_log == 3)
			rd->Logv("\n\n.. Start %s flush: '%s' ..\n", "ZPass", ef->GetName());
		else if (CV_r_log >= 3)
			rd->Logv("\n");
	}
#endif

	if (rd->m_RP.m_pRE)
		rd->m_RP.m_pRE = rd->m_RP.m_RIs[0][0]->pElem;

#ifndef _RELEASE
	sBatchStats(rRP);
#endif

#if defined(HW_INSTANCING_ENABLED)
	sDetectInstancing(ef, rRP.m_pCurObject);
#endif

	// Techniques draw cycle...
	SShaderTechnique* __restrict pTech = ef->mfGetStartTechnique(rRP.m_nShaderTechnique);
	const uint32 nTechniqueID          = (rRP.m_nBatchFilter & FB_Z) ? TTYPE_Z : TTYPE_ZPREPASS;
	if (!pTech || pTech->m_nTechnique[nTechniqueID] < 0)
		return;
	rRP.m_nShaderTechniqueType = nTechniqueID;

	rRP.m_pRootTechnique = pTech;

	// Skip z-pass if appropriate technique does not exist
	assert(pTech->m_nTechnique[nTechniqueID] < (int)ef->m_HWTechniques.Num());
	pTech = ef->m_HWTechniques[pTech->m_nTechnique[nTechniqueID]];

	if (!rd->FX_SetResourcesState())
		return;

	rRP.m_FlagsShader_RT |= rRP.m_pCurObject->m_nRTMask;
	if (rRP.m_ObjFlags & FOB_BENDED)
		rRP.m_FlagsShader_MDV |= MDV_BENDING;

	if (rRP.m_RIs[0].Num() <= 1 && !(rRP.m_ObjFlags & FOB_TRANS_MASK))
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_OBJ_IDENTITY];

	if (CV_r_MotionVectors && CV_r_MotionBlurGBufferVelocity && (rRP.m_nBatchFilter & FB_Z))
	{
		if ((rRP.m_pCurObject->m_ObjFlags & (FOB_HAS_PREVMATRIX)) && (rRP.m_PersFlags2 & RBPF2_NOALPHABLEND))
		{
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_MOTION_BLUR];
			if (!(rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS))
			{
				rRP.m_PersFlags2 |= RBPF2_MOTIONBLURPASS;
				rd->FX_PushRenderTarget(3, GetUtils().GetVelocityObjectRT(), NULL);
			}
		}
		else if (rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS)
		{
			rRP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_MOTION_BLUR];
			rRP.m_PersFlags2     &= ~RBPF2_MOTIONBLURPASS;
			rd->FX_PopRenderTarget(3);
		}
	}

	if (rRP.m_RIs[0].Num() <= 1 && !(rRP.m_ObjFlags & FOB_TRANS_MASK))
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_OBJ_IDENTITY];

#ifdef TESSELLATION_RENDERER
	if ((rRP.m_pCurObject->m_ObjFlags & FOB_NEAREST) || !(rRP.m_pCurObject->m_ObjFlags & FOB_ALLOW_TESSELLATION))
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
#endif

	// Set VisArea and Dynamic objects Stencil Ref
	if (CRenderer::CV_r_DeferredShadingStencilPrepass)
	{
		if (rRP.m_nPassGroupID != EFSLIST_TERRAINLAYER && rRP.m_nPassGroupID != EFSLIST_DECAL && !(rRP.m_nBatchFilter & FB_ZPREPASS))
		{
			rRP.m_ForceStateOr |= GS_STENCIL;

			uint32 nStencilRef = CRenderer::CV_r_VisAreaClipLightsPerPixel ? 0 : ((rd->m_RP.m_RIs[0][0]->pObj->m_nClipVolumeStencilRef + 1) | BIT_STENCIL_INSIDE_CLIPVOLUME);
			nStencilRef |= (!(rRP.m_pCurObject->m_ObjFlags & FOB_DYNAMIC_OBJECT) ? BIT_STENCIL_RESERVED : 0);
			const int32 stencilState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_REPLACE);
			rd->FX_SetStencilState(stencilState, nStencilRef, 0xFF, 0xFF);
		}
		else
			rRP.m_ForceStateOr &= ~GS_STENCIL;
	}

	rRP.m_pCurTechnique = pTech;
	rd->FX_DrawTechnique(ef, pTech);

	rRP.m_ForceStateOr &= ~GS_STENCIL;
	//reset stencil AND mask always
	rRP.m_CurStencilRefAndMask = 0;

#ifdef DO_RENDERLOG
	sLogFlush("Flush ZPass", ef, pTech);
#endif
}

//===================================================================================================

static int sTexLimitRes(uint32 nSrcsize, uint32 nDstSize)
{
	while (true)
	{
		if (nSrcsize > nDstSize)
			nSrcsize >>= 1;
		else
			break;
	}
	return nSrcsize;
}

static Matrix34 sMatrixLookAt(const Vec3& dir, const Vec3& up, float rollAngle = 0)
{
	Matrix34 M;
	// LookAt transform.
	Vec3 xAxis, yAxis, zAxis;
	Vec3 upVector = up;

	yAxis = -dir.GetNormalized();

	//if (zAxis.x == 0.0 && zAxis.z == 0) up.Set( -zAxis.y,0,0 ); else up.Set( 0,1.0f,0 );

	xAxis = upVector.Cross(yAxis).GetNormalized();
	zAxis = xAxis.Cross(yAxis).GetNormalized();

	// OpenGL kind of matrix.
	M(0, 0) = xAxis.x;
	M(0, 1) = yAxis.x;
	M(0, 2) = zAxis.x;
	M(0, 3) = 0;

	M(1, 0) = xAxis.y;
	M(1, 1) = yAxis.y;
	M(1, 2) = zAxis.y;
	M(1, 3) = 0;

	M(2, 0) = xAxis.z;
	M(2, 1) = yAxis.z;
	M(2, 2) = zAxis.z;
	M(2, 3) = 0;

	if (rollAngle != 0)
	{
		Matrix34 RollMtx;
		RollMtx.SetIdentity();

		float cossin[2];
		//   sincos_tpl(rollAngle, cossin);
		sincos_tpl(rollAngle, &cossin[1], &cossin[0]);

		RollMtx(0, 0) = cossin[0];
		RollMtx(0, 2) = -cossin[1];
		RollMtx(2, 0) = cossin[1];
		RollMtx(2, 2) = cossin[0];

		// Matrix multiply.
		M = RollMtx * M;
	}

	return M;
}

bool CD3D9Renderer::FX_DrawToRenderTarget(CShader* pShader, CShaderResources* pRes, CRenderObject* pObj, SShaderTechnique* pTech, SHRenderTarget* pRT, int nPreprType, CRenderElement* pRE)
{
	if (!pRT)
		return false;

	int nThreadList = m_pRT->GetThreadList();

	uint32 nPrFlags = pRT->m_nFlags;
	if (nPrFlags & FRT_RENDTYPE_CURSCENE)
		return false;

	if (!pRT->m_pTarget[0] && !pRT->m_pTarget[1])
	{
		if (pRT->m_refSamplerID >= 0 && pRT->m_refSamplerID < EFTT_MAX)
		{
			IDynTextureSourceImpl* pDynTexSrc = (IDynTextureSourceImpl*) pRes->m_Textures[pRT->m_refSamplerID]->m_Sampler.m_pDynTexSource;
			assert(pDynTexSrc);
			if (pDynTexSrc)
				return m_pRT->RC_DynTexSourceUpdate(pDynTexSrc, pObj->m_fDistance);
		}
		return false;
	}

	CRenderObject* pPrevIgn = m_RP.m_TI[nThreadList].m_pIgnoreObject;
	CTexture* Tex           = pRT->m_pTarget[0];
	SEnvTexture* pEnvTex    = NULL;

	if (nPreprType == SPRID_SCANTEX)
	{
		nPrFlags     |= FRT_CAMERA_REFLECTED_PLANE;
		pRT->m_nFlags = nPrFlags;
	}

	if (nPrFlags & FRT_RENDTYPE_CURSCENE)
		return false;

	uint32 nWidth  = pRT->m_nWidth;
	uint32 nHeight = pRT->m_nHeight;

	if (pRT->m_nIDInPool >= 0)
	{
		assert((int)CTexture::s_CustomRT_2D.Num() > pRT->m_nIDInPool);
		if ((int)CTexture::s_CustomRT_2D.Num() <= pRT->m_nIDInPool)
			return false;
		pEnvTex = &CTexture::s_CustomRT_2D[pRT->m_nIDInPool];

		if (nWidth == -1)
			nWidth = GetWidth();
		if (nHeight == -1)
			nHeight = GetHeight();

		// Very hi specs render reflections at half res - lower specs (and consoles) at quarter res
		float fSizeScale = (CV_r_waterreflections_quality == 5) ? 0.5f : 0.25f;
		nWidth  = sTexLimitRes(nWidth, uint32(GetWidth() * fSizeScale));
		nHeight = sTexLimitRes(nHeight, uint32(GetHeight() * fSizeScale));

		ETEX_Format eTF = pRT->m_eTF;
		// $HDR
		if (eTF == eTF_R8G8B8A8 && IsHDRModeEnabled() && m_nHDRType <= 1)
			eTF = eTF_R16G16B16A16F;
		if (!pEnvTex->m_pTex || pEnvTex->m_pTex->GetFormat() != eTF)
		{
			char name[128];
			cry_sprintf(name, "$RT_2D_%d", m_TexGenID++);
			int flags = FT_NOMIPS | FT_STATE_CLAMP | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
			pEnvTex->m_pTex = new SDynTexture(nWidth, nHeight, eTF, eTT_2D, flags, name);
		}
		assert(nWidth > 0 && nWidth <= m_d3dsdBackBuffer.Width);
		assert(nHeight > 0 && nHeight <= m_d3dsdBackBuffer.Height);
		Tex = pEnvTex->m_pTex->m_pTexture;
	}
	else if (Tex)
	{
		if (Tex->GetCustomID() == TO_RT_2D)
		{
			bool bReflect = false;
			if (nPrFlags & (FRT_CAMERA_REFLECTED_PLANE | FRT_CAMERA_REFLECTED_WATERPLANE))
				bReflect = true;
			Matrix33 orientation = Matrix33(GetCamera().GetMatrix());
			Ang3 Angs            = CCamera::CreateAnglesYPR(orientation);
			Vec3 Pos             = GetCamera().GetPosition();
			bool bNeedUpdate     = false;
			pEnvTex = CTexture::FindSuitableEnvTex(Pos, Angs, false, -1, false, pShader, pRes, pObj, bReflect, pRE, &bNeedUpdate);

			if (!bNeedUpdate)
			{
				if (!pEnvTex)
					return false;
				if (pEnvTex->m_pTex && pEnvTex->m_pTex->m_pTexture)
					return true;
			}
			m_RP.m_TI[nThreadList].m_pIgnoreObject = pObj;
			switch (CRenderer::CV_r_envtexresolution)
			{
			case 0:
				nWidth = 64;
				break;
			case 1:
				nWidth = 128;
				break;
			case 2:
			default:
				nWidth = 256;
				break;
			case 3:
				nWidth = 512;
				break;
			}
			nHeight = nWidth;
			if (!pEnvTex || !pEnvTex->m_pTex)
				return false;
			if (!pEnvTex->m_pTex->m_pTexture)
			{
				pEnvTex->m_pTex->Update(nWidth, nHeight);
			}
			Tex = pEnvTex->m_pTex->m_pTexture;
		}
	}
	if (m_pRT->IsRenderThread() && Tex && Tex->IsLocked())
		return true;

	bool bMGPUAllowNextUpdate = (!(gRenDev->RT_GetCurrGpuID())) && (CRenderer::CV_r_waterreflections_mgpu);

	// always allow for non-mgpu
	if (gRenDev->GetActiveGPUCount() == 1 || !CRenderer::CV_r_waterreflections_mgpu)
		bMGPUAllowNextUpdate = true;

	ETEX_Format eTF = pRT->m_eTF;
	// $HDR
	if (eTF == eTF_R8G8B8A8 && IsHDRModeEnabled() && m_nHDRType <= 1)
		eTF = eTF_R16G16B16A16F;
	if (pEnvTex && (!pEnvTex->m_pTex || pEnvTex->m_pTex->GetFormat() != eTF))
	{
		SAFE_DELETE(pEnvTex->m_pTex);
		char name[128];
		cry_sprintf(name, "$RT_2D_%d", m_TexGenID++);
		int flags = FT_NOMIPS | FT_STATE_CLAMP | FT_DONT_STREAM;
		pEnvTex->m_pTex = new SDynTexture(nWidth, nHeight, eTF, eTT_2D, flags, name);
		assert(nWidth > 0 && nWidth <= m_d3dsdBackBuffer.Width);
		assert(nHeight > 0 && nHeight <= m_d3dsdBackBuffer.Height);
		pEnvTex->m_pTex->Update(nWidth, nHeight);
	}

	bool bEnableAnisotropicBlur = true;
	switch (pRT->m_eUpdateType)
	{
	case eRTUpdate_WaterReflect:
	{
		bool bSkip = false;
		if (m_RP.m_nLastWaterFrameID < 100)
		{
			m_RP.m_nLastWaterFrameID++;
			bSkip = true;
		}
		if (bSkip || !CRenderer::CV_r_waterreflections)
		{
			assert(pEnvTex != NULL);
			if (pEnvTex && pEnvTex->m_pTex && pEnvTex->m_pTex->m_pTexture)
				m_pRT->RC_ClearTarget(pEnvTex->m_pTex->m_pTexture, Clr_Empty);

			return true;
		}

		if (m_RP.m_nLastWaterFrameID == GetFrameID())
			// water reflection already created this frame, share it
			return true;

		I3DEngine* eng               = (I3DEngine*)gEnv->p3DEngine;
		int nVisibleWaterPixelsCount = eng->GetOceanVisiblePixelsCount() / 2;            // bug in occlusion query returns 2x more
		int nPixRatioThreshold       = (int)(GetWidth() * GetHeight() * CRenderer::CV_r_waterreflections_min_visible_pixels_update);

		static int nVisWaterPixCountPrev = nVisibleWaterPixelsCount;
		if (CRenderer::CV_r_waterreflections_mgpu)
		{
			nVisWaterPixCountPrev = bMGPUAllowNextUpdate ? nVisibleWaterPixelsCount : nVisWaterPixCountPrev;
		}
		else
			nVisWaterPixCountPrev = nVisibleWaterPixelsCount;

		float fUpdateFactorMul   = 1.0f;
		float fUpdateDistanceMul = 1.0f;
		if (nVisWaterPixCountPrev < nPixRatioThreshold / 4)
		{
			bEnableAnisotropicBlur = false;
			fUpdateFactorMul       = CV_r_waterreflections_minvis_updatefactormul * 10.0f;
			fUpdateDistanceMul     = CV_r_waterreflections_minvis_updatedistancemul * 5.0f;
		}
		else if (nVisWaterPixCountPrev < nPixRatioThreshold)
		{
			fUpdateFactorMul   = CV_r_waterreflections_minvis_updatefactormul;
			fUpdateDistanceMul = CV_r_waterreflections_minvis_updatedistancemul;
		}

		float fMGPUScale           = CRenderer::CV_r_waterreflections_mgpu ? (1.0f / (float) gRenDev->GetActiveGPUCount()) : 1.0f;
		float fWaterUpdateFactor   = CV_r_waterupdateFactor * fUpdateFactorMul * fMGPUScale;
		float fWaterUpdateDistance = CV_r_waterupdateDistance * fUpdateDistanceMul * fMGPUScale;

		float fTimeUpd = min(0.3f, eng->GetDistanceToSectorWithWater());
		fTimeUpd *= fWaterUpdateFactor;
		//if (fTimeUpd > 1.0f)
		//fTimeUpd = 1.0f;
		Vec3 camView = m_RP.m_TI[nThreadList].m_rcam.ViewDir();
		Vec3 camUp   = m_RP.m_TI[nThreadList].m_rcam.vY;

		m_RP.m_nLastWaterFrameID = GetFrameID();

		Vec3  camPos   = GetCamera().GetPosition();
		float fDistCam = (camPos - m_RP.m_LastWaterPosUpdate).GetLength();
		float fDotView = camView * m_RP.m_LastWaterViewdirUpdate;
		float fDotUp   = camUp * m_RP.m_LastWaterUpdirUpdate;
		float fFOV     = GetCamera().GetFov();
		if (m_RP.m_fLastWaterUpdate - 1.0f > m_RP.m_TI[nThreadList].m_RealTime)
			m_RP.m_fLastWaterUpdate = m_RP.m_TI[nThreadList].m_RealTime;

		const float fMaxFovDiff = 0.1f;       // no exact test to prevent slowly changing fov causing per frame water reflection updates

		static bool bUpdateReflection = true;
		if (bMGPUAllowNextUpdate)
		{
			bUpdateReflection = m_RP.m_TI[nThreadList].m_RealTime - m_RP.m_fLastWaterUpdate >= fTimeUpd || fDistCam > fWaterUpdateDistance;
			bUpdateReflection = bUpdateReflection || fDotView<0.9f || fabs(fFOV - m_RP.m_fLastWaterFOVUpdate)>fMaxFovDiff;
		}

		if (bUpdateReflection && bMGPUAllowNextUpdate)
		{
			m_RP.m_fLastWaterUpdate       = m_RP.m_TI[nThreadList].m_RealTime;
			m_RP.m_LastWaterViewdirUpdate = camView;
			m_RP.m_LastWaterUpdirUpdate   = camUp;
			m_RP.m_fLastWaterFOVUpdate    = fFOV;
			m_RP.m_LastWaterPosUpdate     = camPos;
			assert(pEnvTex != NULL);
			pEnvTex->m_pTex->ResetUpdateMask();
		}
		else if (!bUpdateReflection)
		{
			assert(pEnvTex != NULL);
			PREFAST_ASSUME(pEnvTex != NULL);
			if (pEnvTex && pEnvTex->m_pTex && pEnvTex->m_pTex->IsValid())
				return true;
		}

		assert(pEnvTex != NULL);
		PREFAST_ASSUME(pEnvTex != NULL);
		pEnvTex->m_pTex->SetUpdateMask();
	}
	break;
	}

	// Just copy current BB to the render target and exit
	if (nPrFlags & FRT_RENDTYPE_COPYSCENE)
	{
		CRY_ASSERT(m_pRT->IsRenderThread());

		// Get current render target from the RT stack
		if (!CRenderer::CV_r_debugrefraction)
			FX_ScreenStretchRect(Tex); // should encode hdr format
		else
			FX_ClearTarget(Tex, Clr_Debug);

		return true;
	}

	I3DEngine* eng = (I3DEngine*)gEnv->p3DEngine;
	Matrix44A  matProj, matView;

	float plane[4];
	bool  bUseClipPlane  = false;
	bool  bChangedCamera = false;

	int nPersFlags = m_RP.m_TI[nThreadList].m_PersFlags;
	//int nPersFlags2 = m_RP.m_TI[nThreadList].m_PersFlags2;

	static CCamera tmp_cam_mgpu = GetCamera();
	CCamera tmp_cam             = GetCamera();
	CCamera prevCamera          = tmp_cam;
	bool bMirror                = false;
	bool bOceanRefl             = false;

	// Set the camera
	if (nPrFlags & FRT_CAMERA_REFLECTED_WATERPLANE)
	{
		bOceanRefl = true;

		m_RP.m_TI[nThreadList].m_pIgnoreObject = pObj;
		float fMinDist = min(SKY_BOX_SIZE * 0.5f, eng->GetDistanceToSectorWithWater());      // 16 is half of skybox size
		float fMaxDist = eng->GetMaxViewDistance();

		Vec3 vPrevPos = tmp_cam.GetPosition();
		Vec4 pOceanParams0, pOceanParams1;
		eng->GetOceanAnimationParams(pOceanParams0, pOceanParams1);

		Plane Pl;
		Pl.n = Vec3(0, 0, 1);
		Pl.d = eng->GetWaterLevel();                                                      // + CRenderer::CV_r_waterreflections_offset;// - pOceanParams1.x;
		if ((vPrevPos | Pl.n) - Pl.d < 0)
		{
			Pl.d = -Pl.d;
			Pl.n = -Pl.n;
		}

		plane[0] = Pl.n[0];
		plane[1] = Pl.n[1];
		plane[2] = Pl.n[2];
		plane[3] = -Pl.d;

		Matrix44 camMat;
		GetModelViewMatrix(camMat.GetData());
		Vec3  vPrevDir = Vec3(-camMat(0, 2), -camMat(1, 2), -camMat(2, 2));
		Vec3  vPrevUp  = Vec3(camMat(0, 1), camMat(1, 1), camMat(2, 1));
		Vec3  vNewDir  = Pl.MirrorVector(vPrevDir);
		Vec3  vNewUp   = Pl.MirrorVector(vPrevUp);
		float fDot     = vPrevPos.Dot(Pl.n) - Pl.d;
		Vec3  vNewPos  = vPrevPos - Pl.n * 2.0f * fDot;
		Matrix34 m     = sMatrixLookAt(vNewDir, vNewUp, tmp_cam.GetAngles()[2]);

		// New position + offset along view direction - minimizes projection artefacts
		m.SetTranslation(vNewPos + Vec3(vNewDir.x, vNewDir.y, 0));

		tmp_cam.SetMatrix(m);

		float fDistOffset = fMinDist;
		if (CV_r_waterreflections_use_min_offset)
		{
			fDistOffset = max(fMinDist, 2.0f * gEnv->p3DEngine->GetDistanceToSectorWithWater());
			if (fDistOffset >= fMaxDist)                                                                                                                             // engine returning bad value
				fDistOffset = fMinDist;
		}

		assert(pEnvTex);
		PREFAST_ASSUME(pEnvTex);
		tmp_cam.SetFrustum((int)(pEnvTex->m_pTex->GetWidth() * tmp_cam.GetProjRatio()), pEnvTex->m_pTex->GetHeight(), tmp_cam.GetFov(), fDistOffset, fMaxDist);       //tmp_cam.GetFarPlane());

		// Allow camera update
		if (bMGPUAllowNextUpdate)
			tmp_cam_mgpu = tmp_cam;

		SetCamera(tmp_cam_mgpu);
		bChangedCamera = true;
		bUseClipPlane  = true;
		bMirror        = true;
		//m_RP.m_TI[nThreadList].m_PersFlags |= RBPF_MIRRORCULL;
	}
	else if (nPrFlags & FRT_CAMERA_REFLECTED_PLANE)
	{
		m_RP.m_TI[nThreadList].m_pIgnoreObject = pObj;
		float fMinDist = 0.25f;
		float fMaxDist = eng->GetMaxViewDistance();

		Vec3 vPrevPos = tmp_cam.GetPosition();

		if (pRes && pRes->m_pCamera)
		{
			tmp_cam = *pRes->m_pCamera;                                                                                                                             // Portal case
			//tmp_cam.SetPosition(Vec3(310, 150, 30));
			//tmp_cam.SetAngles(Vec3(-90,0,0));
			//tmp_cam.SetFrustum((int)(Tex->GetWidth()*tmp_cam.GetProjRatio()), Tex->GetHeight(), tmp_cam.GetFov(), fMinDist, tmp_cam.GetFarPlane());

			SetCamera(tmp_cam);
			bUseClipPlane = false;
			bMirror       = false;
		}
		else
		{
			// Mirror case
			Plane Pl;
			pRE->mfGetPlane(Pl);
			//Pl.d = -Pl.d;
			if (pObj)
			{
				Matrix44 mat = pObj->m_II.m_Matrix.GetTransposed();
				Pl = TransformPlane(mat, Pl);
			}
			if ((vPrevPos | Pl.n) - Pl.d < 0)
			{
				Pl.d = -Pl.d;
				Pl.n = -Pl.n;
			}

			plane[0] = Pl.n[0];
			plane[1] = Pl.n[1];
			plane[2] = Pl.n[2];
			plane[3] = -Pl.d;

			//this is the new code to calculate the reflection matrix

			Matrix44A camMat;
			GetModelViewMatrix(camMat.GetData());
			Vec3  vPrevDir = Vec3(-camMat(0, 2), -camMat(1, 2), -camMat(2, 2));
			Vec3  vPrevUp  = Vec3(camMat(0, 1), camMat(1, 1), camMat(2, 1));
			Vec3  vNewDir  = Pl.MirrorVector(vPrevDir);
			Vec3  vNewUp   = Pl.MirrorVector(vPrevUp);
			float fDot     = vPrevPos.Dot(Pl.n) - Pl.d;
			Vec3  vNewPos  = vPrevPos - Pl.n * 2.0f * fDot;
			Matrix34A m    = sMatrixLookAt(vNewDir, vNewUp, tmp_cam.GetAngles()[2]);
			m.SetTranslation(vNewPos);
			tmp_cam.SetMatrix(m);

			//Matrix34 RefMatrix34 = CreateReflectionMat3(Pl);
			//Matrix34 matMir=RefMatrix34*tmp_cam.GetMatrix();
			//tmp_cam.SetMatrix(matMir);
			assert(Tex);
			tmp_cam.SetFrustum((int)(Tex->GetWidth() * tmp_cam.GetProjRatio()), Tex->GetHeight(), tmp_cam.GetFov(), fMinDist, fMaxDist);       //tmp_cam.GetFarPlane());
			bMirror       = true;
			bUseClipPlane = true;
		}
		SetCamera(tmp_cam);
		bChangedCamera = true;
		//m_RP.m_TI[nThreadList].m_PersFlags |= RBPF_MIRRORCULL;
	}
	else if (((nPrFlags & FRT_CAMERA_CURRENT) || (nPrFlags & FRT_RENDTYPE_CURSCENE)) && pRT->m_eOrder == eRO_PreDraw && !(nPrFlags & FRT_RENDTYPE_CUROBJECT))
	{
		CRY_ASSERT(m_pRT->IsRenderThread());

		// Always restore stuff after explicitly changing...

		// get texture surface
		// Get current render target from the RT stack
		if (!CRenderer::CV_r_debugrefraction)
			FX_ScreenStretchRect(Tex);                                                                                                   // should encode hdr format
		else
			FX_ClearTarget(Tex, Clr_Debug);

		m_RP.m_TI[nThreadList].m_pIgnoreObject = pPrevIgn;
		return true;
	}
	/*	if (pRT->m_nFlags & FRT_CAMERA_CURRENT)
	   {
	   //m_RP.m_pIgnoreObject = pObj;

	   SetCamera(tmp_cam);
	   bChangedCamera = true;
	   bUseClipPlane = true;
	   }*/

	bool bRes = true;

	m_pRT->RC_PushVP();
	m_pRT->RC_PushFog();
	m_RP.m_TI[nThreadList].m_PersFlags |= RBPF_DRAWTOTEXTURE | RBPF_ENCODE_HDR;

	if (m_LogFile)
		Logv("*** Set RT for Water reflections ***\n");

	// TODO: ClearTexture()
	assert(pEnvTex);
	PREFAST_ASSUME(pEnvTex);
	CRenderOutput renderOutput(*pEnvTex, *pRT);
	CRenderOutput* pRenderOutput = nullptr;
	if (m_nGraphicsPipeline >= 3)
	{
		pRenderOutput = &renderOutput;
	}
	else
	{
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
		m_pRT->RC_SetEnvTexRT(pEnvTex, pRT->m_bTempDepth ? pEnvTex->m_pTex->GetWidth() : -1, pRT->m_bTempDepth ? pEnvTex->m_pTex->GetHeight() : -1, true);
		m_pRT->RC_ClearTargetsImmediately(1, pRT->m_nFlags, pRT->m_ClearColor, pRT->m_fClearDepth);
#endif
	}

	float fAnisoScale = 1.0f;
	if (pRT->m_nFlags & FRT_RENDTYPE_CUROBJECT)
	{
		CCryNameR& nameTech = pTech->m_NameStr;
		char newTech[128];
		cry_sprintf(newTech, "%s_RT", nameTech.c_str());
		SShaderTechnique* pT = pShader->mfFindTechnique(newTech);
		if (!pT)
			iLog->Log("Error: CD3D9Renderer::FX_DrawToRenderTarget: Couldn't find technique '%s' in shader '%s'\n", newTech, pShader->GetName());
		else
		{
			FX_ObjectChange(pShader, pRes, pObj, pRE);
			FX_Start(pShader, -1, pRes, pRE);
			pRE->mfPrepare(false);
			FX_DrawShader_General(pShader, pT);
		}
	}
	else
	{
		if (bMirror)
		{
			if (bOceanRefl)
				SetCamera(tmp_cam);

			m_pRT->RC_SetEnvTexMatrix(pEnvTex);

			if (bOceanRefl)
				SetCamera(tmp_cam_mgpu);
		}

		m_RP.m_TI[nThreadList].m_PersFlags |= RBPF_OBLIQUE_FRUSTUM_CLIPPING | RBPF_MIRRORCAMERA;     // | RBPF_MIRRORCULL; ??

		Plane p;
		p.n[0]      = plane[0];
		p.n[1]      = plane[1];
		p.n[2]      = plane[2];
		p.d         = plane[3];                                                                  // +0.25f;
		fAnisoScale = plane[3];
		fAnisoScale = fabs(fabs(fAnisoScale) - GetCamera().GetPosition().z);
		m_RP.m_TI[nThreadList].m_bObliqueClipPlane = true;

		// put clipplane in clipspace..
		Matrix44A mView, mProj, mCamProj, mInvCamProj;
		GetModelViewMatrix(&mView(0, 0));
		GetProjectionMatrix(&mProj(0, 0));
		mCamProj    = mView * mProj;
		mInvCamProj = mCamProj.GetInverted();
		m_RP.m_TI[nThreadList].m_pObliqueClipPlane = TransformPlane2(mInvCamProj, p);

		int nRenderPassFlags = (gRenDev->m_RP.m_eQuality) ? SRenderingPassInfo::TERRAIN : 0;

		int nReflQuality = (bOceanRefl) ? (int)CV_r_waterreflections_quality : (int)CV_r_reflections_quality;

		// set reflection quality setting
		switch (nReflQuality)
		{
		case 1:
			nRenderPassFlags |= SRenderingPassInfo::ENTITIES;
			break;
		case 2:
			nRenderPassFlags |= SRenderingPassInfo::TERRAIN_DETAIL_MATERIALS | SRenderingPassInfo::ENTITIES;
			break;
		case 3:
			nRenderPassFlags |= SRenderingPassInfo::STATIC_OBJECTS | SRenderingPassInfo::ENTITIES | SRenderingPassInfo::TERRAIN_DETAIL_MATERIALS;
			break;
		case 4:
		case 5:
			nRenderPassFlags |= SRenderingPassInfo::STATIC_OBJECTS | SRenderingPassInfo::ENTITIES | SRenderingPassInfo::TERRAIN_DETAIL_MATERIALS | SRenderingPassInfo::PARTICLES;
			break;
		}

		int nRFlags = SHDF_ALLOWHDR | SHDF_NO_DRAWNEAR;

		// disable caustics if camera above water
		if (p.d < 0)
			nRFlags |= SHDF_NO_DRAWCAUSTICS;


		auto passInfo = SRenderingPassInfo::CreateRecursivePassRenderingInfo(bOceanRefl ? tmp_cam_mgpu : tmp_cam, nRenderPassFlags);
		
		CRenderView* pRenderView = passInfo.GetRenderView();
		pRenderView->SetRenderOutput(pRenderOutput);

		CCamera cam = passInfo.GetCamera();
		pRenderView->SetCameras(&cam, 1);
		pRenderView->SetPreviousFrameCameras(&cam, 1);

		eng->RenderWorld(nRFlags, passInfo, __FUNCTION__);

		m_RP.m_TI[nThreadList].m_bObliqueClipPlane = false;
		m_RP.m_TI[nThreadList].m_PersFlags        &= ~RBPF_OBLIQUE_FRUSTUM_CLIPPING;
	}

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
	if (!pRenderOutput)
	{
		m_pRT->RC_PopRT(0);
	}
#endif

	// Very Hi specs get anisotropic reflections
	int nReflQuality = (bOceanRefl) ? (int)CV_r_waterreflections_quality : (int)CV_r_reflections_quality;
	if (nReflQuality >= 4 && bEnableAnisotropicBlur && Tex && Tex->GetDevTexture())
		m_pRT->RC_TexBlurAnisotropicVertical(Tex, fAnisoScale);

	if (m_LogFile)
		Logv("*** End RT for Water reflections ***\n");

	// todo: encode hdr format

	m_RP.m_TI[nThreadList].m_PersFlags = nPersFlags;
	//m_RP.m_TI[nThreadList].m_PersFlags2 = nPersFlags2;

	if (bChangedCamera)
		SetCamera(prevCamera);

	m_pRT->RC_PopVP();
	m_pRT->RC_PopFog();

	// increase frame id to support multiple recursive draws
	m_RP.m_TI[nThreadList].m_pIgnoreObject = pPrevIgn;
	//m_RP.m_TI[nThreadList].m_nFrameID++;

	return bRes;
}
