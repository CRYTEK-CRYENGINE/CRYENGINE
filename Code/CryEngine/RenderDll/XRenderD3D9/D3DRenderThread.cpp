// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

#include "D3DStereo.h"

#include <CrySystem/Scaleform/IFlashPlayer.h>

#include "D3DREBreakableGlassBuffer.h"

#include <CryCore/CryCustomTypes.h>

#include "Gpu/Particles/GpuParticleManager.h"
#include <CrySystem/VR/IHMDManager.h>

#include "D3D_SVO.h"

//=======================================================================

bool CD3D9Renderer::RT_CreateDevice()
{
	LOADING_TIME_PROFILE_SECTION;
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_D3D, 0, "Renderer CreateDevice");

#if CRY_PLATFORM_WINDOWS && !defined(SUPPORT_DEVICE_INFO)
	if (!m_bShaderCacheGen && !SetWindow(m_width, m_height, m_bFullScreen, m_hWnd))
		return false;
#endif

	return SetBaseResolution();
}

void CD3D9Renderer::RT_ReleaseVBStream(void* pVB, int nStream)
{
	D3DVertexBuffer* pBuf = (D3DVertexBuffer*)pVB;
	SAFE_RELEASE(pBuf);
}
void CD3D9Renderer::RT_ReleaseCB(void* pVCB)
{
	CConstantBuffer* pCB = (CConstantBuffer*)pVCB;
	CHWShader_D3D::mfUnbindCB(pCB);
	SAFE_RELEASE(pCB);
}

void CD3D9Renderer::RT_ReleaseRS(std::shared_ptr<CDeviceResourceSet>& pRS)
{
	pRS.reset();
}

void CD3D9Renderer::RT_ClearTarget(CTexture* pTex, const ColorF& color)
{
	CDeviceTexture* pDevTex = pTex->GetDevTexture(pTex->IsMSAA());
	if (auto* pView = pDevTex->LookupResourceView(EDefaultResourceViews::RasterizerTarget).second)
	{
		CDeviceGraphicsCommandInterface* pGraphicsInterface = GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface();

		// FT_USAGE_DEPTHSTENCIL takes preference over RenderTarget if both are possible
		if (pTex->GetFlags() & FT_USAGE_DEPTHSTENCIL)
			pGraphicsInterface->ClearSurface(reinterpret_cast<D3DDepthSurface*>(pView), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, color.r, (uint8)color.g); // NOTE: normalized depth in color.r and unnormalized stencil in color.g
		else if (pTex->GetFlags() & FT_USAGE_RENDERTARGET)
			pGraphicsInterface->ClearSurface(reinterpret_cast<D3DSurface*>(pView), color);
	}
}

void CD3D9Renderer::RT_DrawDynVB(SVF_P3F_C4B_T2F* pBuf, uint16* pInds, uint32 nVerts, uint32 nInds, const PublicRenderPrimitiveType nPrimType)
{
	TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(pBuf, nVerts, 0);
	if (pInds)
		TempDynIB16::CreateFillAndBind(pInds, nInds);

	FX_SetFPMode();

	if (!FAILED(FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
	{
		if (pInds)
			FX_DrawIndexedPrimitive(GetInternalPrimitiveType(nPrimType), 0, 0, nVerts, 0, nInds);
		else
			FX_DrawPrimitive(GetInternalPrimitiveType(nPrimType), 0, nVerts);
	}
}

void CD3D9Renderer::RT_Draw2dImageInternal(S2DImage* images, uint32 numImages, bool stereoLeftEye)
{
	bool bSaveZTest = ((m_RP.m_CurState & GS_NODEPTHTEST) == 0);
	SetCullMode(R_CULL_DISABLE);

	float maxParallax = 0;
	float screenDist = 0;

	if (GetS3DRend().IsStereoEnabled() && !GetS3DRend().DisplayStereoDone())
	{
		maxParallax = GetS3DRend().GetMaxSeparationScene();
		screenDist = GetS3DRend().GetZeroParallaxPlaneDist();
	}

	// Set orthographic projection
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Push();
	Matrix44A* m = m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->GetTop();
	int vx, vy, vw, vh;
	GetViewport(&vx, &vy, &vw, &vh);
	mathMatrixOrthoOffCenterLH(m, (float)vx, (float)vw, (float)vh, (float)vy, 0.0f, 1.0f);
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Push();
	m = m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->GetTop();
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->LoadIdentity();

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
	// Create dynamic geometry
	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, numImages * 4, vQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);

	for (uint32 i = 0; i < numImages; ++i)
	{
		S2DImage& img = images[i];
		uint32 baseIdx = i * 4;

		float parallax = 0;
		if (img.stereoDepth > 0)
			parallax = 800 * maxParallax * (1 - screenDist / img.stereoDepth);

		float xpos = (float)ScaleCoordX(img.xpos + parallax * (stereoLeftEye ? -1 : 1));
		float w = (float)ScaleCoordX(img.w);
		float ypos = (float)ScaleCoordY(img.ypos);
		float h = (float)ScaleCoordY(img.h);

		if (img.angle != 0)
		{
			float xsub = (float)(xpos + w / 2.0f);
			float ysub = (float)(ypos + h / 2.0f);

			float x, y, x1, y1;
			float mcos = cos_tpl(DEG2RAD(img.angle));
			float msin = sin_tpl(DEG2RAD(img.angle));

			x = xpos - xsub;
			y = ypos - ysub;
			x1 = x * mcos - y * msin;
			y1 = x * msin + y * mcos;
			x1 += xsub;
			y1 += ysub;
			vQuad[baseIdx].xyz.x = x1;
			vQuad[baseIdx].xyz.y = y1;

			x = xpos + w - xsub;
			y = ypos - ysub;
			x1 = x * mcos - y * msin;
			y1 = x * msin + y * mcos;
			x1 += xsub;
			y1 += ysub;
			vQuad[baseIdx + 1].xyz.x = x1;//xpos + fw;
			vQuad[baseIdx + 1].xyz.y = y1;// fy;

			x = xpos + w - xsub;
			y = ypos + h - ysub;
			x1 = x * mcos - y * msin;
			y1 = x * msin + y * mcos;
			x1 += xsub;
			y1 += ysub;
			vQuad[baseIdx + 3].xyz.x = x1;//xpos + fw;
			vQuad[baseIdx + 3].xyz.y = y1;//fy + fh;

			x = xpos - xsub;
			y = ypos + h - ysub;
			x1 = x * mcos - y * msin;
			y1 = x * msin + y * mcos;
			x1 += xsub;
			y1 += ysub;
			vQuad[baseIdx + 2].xyz.x = x1;//xpos;
			vQuad[baseIdx + 2].xyz.y = y1;//fy + fh;
		}
		else
		{
			vQuad[baseIdx].xyz.x = xpos;
			vQuad[baseIdx].xyz.y = ypos;

			vQuad[baseIdx + 1].xyz.x = xpos + w;
			vQuad[baseIdx + 1].xyz.y = ypos;

			vQuad[baseIdx + 2].xyz.x = xpos;
			vQuad[baseIdx + 2].xyz.y = ypos + h;

			vQuad[baseIdx + 3].xyz.x = xpos + w;
			vQuad[baseIdx + 3].xyz.y = ypos + h;
		}

		vQuad[baseIdx + 0].st = Vec2(img.s0, 1.0f - img.t0);
		vQuad[baseIdx + 1].st = Vec2(img.s1, 1.0f - img.t0);
		vQuad[baseIdx + 2].st = Vec2(img.s0, 1.0f - img.t1);
		vQuad[baseIdx + 3].st = Vec2(img.s1, 1.0f - img.t1);

		for (int j = 0; j < 4; ++j)
		{
			vQuad[baseIdx + j].color.dcolor = img.col;
			vQuad[baseIdx + j].xyz.z = img.z;
		}
	}

	TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(vQuad, numImages * 4, 0);

	CTexture* prevTex = NULL;
	EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, (eCA_Diffuse | (eCA_Diffuse << 3)), (eCA_Diffuse | (eCA_Diffuse << 3)));
	FX_SetFPMode();

	if (FAILED(FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
	{
		m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Pop();
		m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Pop();
		return;
	}

	// Draw quads
	for (uint32 i = 0; i < numImages; ++i)
	{
		S2DImage& img = images[i];

		if (img.pTex != prevTex)
		{
			prevTex = img.pTex;
			if (img.pTex)
			{
				img.pTex->Apply(0, (m_bDraw2dImageStretchMode || img.pTex->IsHighQualityFiltered()) ? EDefaultSamplerStates::TrilinearClamp : EDefaultSamplerStates::PointClamp);
				EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
			}
			else
				EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, (eCA_Diffuse | (eCA_Diffuse << 3)), (eCA_Diffuse | (eCA_Diffuse << 3)));

			FX_SetFPMode();

#if CRY_PLATFORM_ORBIS
			// Workaround for vertex declaration not always being fully bound
			if (FAILED(FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
			{
				m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Pop();
				m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Pop();
				return;
			}
#endif
		}

		FX_DrawPrimitive(eptTriangleStrip, i * 4, 4);
	}

#endif

	EF_PopMatrix();
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Pop();
}

void CD3D9Renderer::RT_DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround)
{
	if (m_bDeviceLost)
		return;

	int i;
	int st;
	HRESULT hr = S_OK;

	EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, eCA_Texture | (eCA_Diffuse << 3), eCA_Texture | (eCA_Diffuse << 3));
	st = GS_NODEPTHTEST;
	if (flags & 1)
		st |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
	FX_SetState(st);
	CTexture::s_ptexWhite->Apply(0);

	DWORD c = D3DRGBA(col.r, col.g, col.b, col.a);

	if (fGround >= 0)
	{
		// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
		CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, nump * 2, vQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);

		for (i = 0; i < nump; i++)
		{
			vQuad[i * 2 + 0].xyz.x = v[i][0];
			vQuad[i * 2 + 0].xyz.y = fGround;
			vQuad[i * 2 + 0].xyz.z = 0;
			vQuad[i * 2 + 0].color.dcolor = c;
			vQuad[i * 2 + 0].st = Vec2(0.0f, 0.0f);
			vQuad[i * 2 + 1].xyz = v[i];
			vQuad[i * 2 + 1].color.dcolor = c;
			vQuad[i * 2 + 1].st = Vec2(0.0f, 0.0f);
		}

		TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(vQuad, nump * 2, 0);

		FX_SetFPMode();
		if (!FAILED(FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
			FX_DrawPrimitive(eptLineList, 0, nump * 2);
	}
	else
	{
		// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
		CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, nump, vQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);

		for (i = 0; i < nump; i++)
		{
			vQuad[i].xyz = v[i];
			vQuad[i].color.dcolor = c;
			vQuad[i].st = Vec2(0, 0);
		}

		TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(vQuad, nump, 0);

		FX_SetFPMode();
		if (!FAILED(FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
			FX_DrawPrimitive(eptLineStrip, 0, nump);
	}
}

void CD3D9Renderer::RT_Draw2dImageStretchMode(bool bStretch)
{
	m_bDraw2dImageStretchMode = bStretch;
}

void CD3D9Renderer::RT_Draw2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, DWORD col, float z)
{
	S2DImage img(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, col, z, 0);

	SetProfileMarker("DRAW2DIMAGE", CRenderer::ESPM_PUSH);

	if (GetS3DRend().IsStereoEnabled() && !GetS3DRend().DisplayStereoDone())
	{
		GetS3DRend().BeginRenderingTo(LEFT_EYE);
		RT_Draw2dImageInternal(&img, 1, true);
		GetS3DRend().EndRenderingTo(LEFT_EYE);

		if (GetS3DRend().RequiresSequentialSubmission())
		{
			GetS3DRend().BeginRenderingTo(RIGHT_EYE);
			RT_Draw2dImageInternal(&img, 1, false);
			GetS3DRend().EndRenderingTo(RIGHT_EYE);
		}
	}
	else
	{
		RT_Draw2dImageInternal(&img, 1);
	}

	SetProfileMarker("DRAW2DIMAGE", CRenderer::ESPM_POP);
}

void CD3D9Renderer::RT_Push2dImage(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, float angle, DWORD col, float z, float stereoDepth)
{
	m_2dImages.Add(S2DImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, col, z, stereoDepth));
}

void CD3D9Renderer::RT_PushUITexture(float xpos, float ypos, float w, float h, CTexture* pTexture, float s0, float t0, float s1, float t1, CTexture* pTarget, DWORD col)
{
	m_uiImages.Add(S2DImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, 0, col, 0, 0, pTarget));
}

void CD3D9Renderer::RT_Draw2dImageList()
{
	FUNCTION_PROFILER_RENDERER

	if (m_2dImages.empty())
		return;

	SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
	SetProfileMarker("DRAW2DIMAGELIST", CRenderer::ESPM_PUSH);

	if (GetS3DRend().IsStereoEnabled() && !GetS3DRend().DisplayStereoDone())
	{
		GetS3DRend().BeginRenderingTo(LEFT_EYE);
		RT_Draw2dImageInternal(m_2dImages.Data(), m_2dImages.size(), true);
		GetS3DRend().EndRenderingTo(LEFT_EYE);

		if (GetS3DRend().RequiresSequentialSubmission())
		{
			GetS3DRend().BeginRenderingTo(RIGHT_EYE);
			RT_Draw2dImageInternal(m_2dImages.Data(), m_2dImages.size(), false);
			GetS3DRend().EndRenderingTo(RIGHT_EYE);
		}
	}
	else
	{
		RT_Draw2dImageInternal(m_2dImages.Data(), m_2dImages.size());
	}

	SetProfileMarker("DRAW2DIMAGELIST", CRenderer::ESPM_POP);

	m_2dImages.resize(0);
}

void CD3D9Renderer::FlashRenderInternal(IFlashPlayer_RenderProxy* pPlayer, bool bStereo, bool bDoRealRender)
{
	FUNCTION_PROFILER_RENDERER

	if (bDoRealRender)
	{
		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_PUSH);

		if (GetS3DRend().IsStereoEnabled() && bStereo)
		{
			if (GetS3DRend().IsQuadLayerEnabled())
			{
				GetS3DRend().BeginRenderingToVrQuadLayer(RenderLayer::eQuadLayers_0);
				pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_Mono);
				GetS3DRend().EndRenderingToVrQuadLayer(RenderLayer::eQuadLayers_0);
			}
			else
			{
				GetS3DRend().BeginRenderingTo(LEFT_EYE);
				pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_StereoLeft, false);
				GetS3DRend().EndRenderingTo(LEFT_EYE);

				if (GetS3DRend().RequiresSequentialSubmission())
				{
					GetS3DRend().BeginRenderingTo(RIGHT_EYE);
					pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_StereoRight, true);
					GetS3DRend().EndRenderingTo(RIGHT_EYE);
				}
			}
		}
		else
		{
			pPlayer->RenderCallback(IFlashPlayer_RenderProxy::EFT_Mono);
		}

		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_POP);
	}
	else
	{
		pPlayer->DummyRenderCallback(IFlashPlayer_RenderProxy::EFT_Mono, true);
	}
}

void CD3D9Renderer::FlashRenderPlaybackLocklessInternal(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool bStereo, bool bFinalPlayback, bool bDoRealRender)
{
	if (bDoRealRender)
	{
		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_PUSH);

		if (GetS3DRend().IsStereoEnabled() && bStereo)
		{
			if (GetS3DRend().IsQuadLayerEnabled())
			{
				GetS3DRend().BeginRenderingToVrQuadLayer(RenderLayer::eQuadLayers_0);
				pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_Mono, bFinalPlayback);
				GetS3DRend().EndRenderingToVrQuadLayer(RenderLayer::eQuadLayers_0);
			}
			else
			{
				GetS3DRend().BeginRenderingTo(LEFT_EYE);
				pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_StereoLeft, false, false);
				GetS3DRend().EndRenderingTo(LEFT_EYE);

				if (GetS3DRend().RequiresSequentialSubmission())
				{
					GetS3DRend().BeginRenderingTo(RIGHT_EYE);
					pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_StereoRight, bFinalPlayback, true);
					GetS3DRend().EndRenderingTo(RIGHT_EYE);
				}
			}
		}
		else
		{
			pPlayer->RenderPlaybackLocklessCallback(cbIdx, IFlashPlayer_RenderProxy::EFT_Mono, bFinalPlayback);
		}

		SetProfileMarker("FLASH_RENDERING", CRenderer::ESPM_POP);
	}
	else
	{
		pPlayer->DummyRenderCallback(IFlashPlayer_RenderProxy::EFT_Mono, true);
	}
}

void CD3D9Renderer::RT_PushRenderTarget(int nTarget, CTexture* pTex, SDepthTexture* pDepth, int nS)
{
	FX_PushRenderTarget(nTarget, pTex, pDepth, nS);
}

void CD3D9Renderer::RT_PopRenderTarget(int nTarget)
{
	FX_PopRenderTarget(nTarget);
}

void CD3D9Renderer::RT_Init()
{
	EF_Init();
}

void CD3D9Renderer::RT_CreateResource(SResourceAsync* pRes)
{
	if (pRes->eClassName == eRCN_Texture)
	{
		CTexture* pTex = NULL;

		if (pRes->nTexId)
		{
			// only create device texture
			pTex = CTexture::GetByID(pRes->nTexId);
			const void* pData[] = { pRes->pData, nullptr, nullptr };
			pTex->CreateDeviceTexture(pData);
		}
		else
		{
			// create full texture
			char* pName = pRes->Name;
			char szName[128];
			if (!pName)
			{
				cry_sprintf(szName, "$AutoDownloadAsync_%d", m_TexGenID++);
				pName = szName;
			}
			pTex = CTexture::GetOrCreate2DTexture(pName, pRes->nWidth, pRes->nHeight, pRes->nMips, pRes->nTexFlags, pRes->pData, (ETEX_Format)pRes->nFormat);
		}

		SAFE_DELETE_ARRAY(pRes->pData);
		pRes->pResource = pTex;
		pRes->nReady = (CTexture::IsTextureExist(pTex));
	}
	else
	{
		assert(0);
	}

	delete pRes;
}
void CD3D9Renderer::RT_ReleaseResource(SResourceAsync* pRes)
{
	if (pRes->eClassName == eRCN_Texture)
	{
		CTexture* pTex = (CTexture*)pRes->pResource;
		pTex->Release();
	}
	else
	{
		assert(0);
	}

	delete pRes;
}

void CD3D9Renderer::RT_ReleaseOptics(IOpticsElementBase* pOpticsElement)
{
	SAFE_RELEASE(pOpticsElement);
}

void CD3D9Renderer::RT_UnbindTMUs()
{
	D3DShaderResource* pTex[MAX_TMU] = { NULL };
	for (uint32 i = 0; i < MAX_TMU; ++i)
	{
		CTexture::s_TexStages[i].m_DevTexture = NULL;
	}
	m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_VS, pTex, 0, MAX_TMU);
	m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_GS, pTex, 0, MAX_TMU);
	m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_DS, pTex, 0, MAX_TMU);
	m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_HS, pTex, 0, MAX_TMU);
	m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pTex, 0, MAX_TMU);
	m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_PS, pTex, 0, MAX_TMU);

	m_DevMan.CommitDeviceStates();
}

void CD3D9Renderer::RT_UnbindResources()
{
	// Clear set Constant Buffers
	CHWShader_D3D::mfClearCB();

	D3DBuffer* pBuffers[16] = { 0 };
	UINT StrideOffset[16] = { 0 };

	m_DevMan.BindIB(NULL, 0, DXGI_FORMAT_R16_UINT);
	m_RP.m_pIndexStream = NULL;

	m_DevMan.BindVB(0, 16, pBuffers, StrideOffset, StrideOffset);
	m_RP.m_VertexStreams[0].pStream = NULL;

	m_DevMan.BindVtxDecl(NULL);
	m_pLastVDeclaration = NULL;

	m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_PS, NULL);
	m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_VS, NULL);
	m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_GS, NULL);
	m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_DS, NULL);
	m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_HS, NULL);
	m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_CS, NULL);

	m_DevMan.CommitDeviceStates();
}

void CD3D9Renderer::RT_ReleaseRenderResources(uint32 nFlags)
{
	if (nFlags & FRR_FLUSH_TEXTURESTREAMING)
	{
		CTexture::RT_FlushStreaming(true);
	}

	if (nFlags & FRR_PERMANENT_RENDER_OBJECTS)
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		{
			FreePermanentRenderObjects(i);
		}
	}

	if (nFlags & FRR_DELETED_MESHES)
	{
		CRenderMesh::Tick(MAX_RELEASED_MESH_FRAMES);
	}

	if (nFlags & FRR_POST_EFFECTS)
	{
		if (m_pPostProcessMgr)
			m_pPostProcessMgr->ReleaseResources();
	}

#if defined(FEATURE_SVO_GI)
	if (nFlags & FRR_SVOGI)
	{
		if (auto pSvoRenderer = CSvoRenderer::GetInstance())
			pSvoRenderer->Release();
	}
#endif

	if (nFlags & FRR_SYSTEM_RESOURCES)
	{
		// 1) Make sure all high level objects (CRenderMesh, CRenderElement,..) are gone
		RT_UnbindResources();
		RT_ResetGlass();

		CRenderMesh::Tick(MAX_RELEASED_MESH_FRAMES);
		CRenderElement::Cleanup();

		// 2) Release renderer created high level stuff (CStandardGraphicsPipeline, CPrimitiveRenderPass, CSceneRenderPass,..)	
		SF_DestroyResources();
		RT_GraphicsPipelineShutdown();

		// 3) At this point all device objects should be gone and we can safely reset PSOs, ResourceLayouts,..
		RT_ResetDeviceObjectFactory();
		
		// 4) Now release textures and shaders
		m_cEF.mfReleaseSystemShaders();
		m_cEF.m_Bin.InvalidateCache();

		RT_UnbindTMUs();

		CTexture::ResetTMUs();
		CTexture::ReleaseSystemTextures();

		FX_PipelineShutdown();
		GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);
		m_nMaxRT2Commit = -1;
	}

	if (nFlags & FRR_TEXTURES)
	{
		CTexture::ShutDown();
	}
	else
	{
		// Clear core render targets on shut down, just to be sure
		// they are in a sane state for next render and prevent flicks
		CTexture* clearTextures[] =
		{
			CTexture::s_ptexSceneNormalsMap,
			CTexture::s_ptexSceneDiffuse,
			CTexture::s_ptexSceneSpecular,
			CTexture::s_ptexSceneDiffuseAccMap,
			CTexture::s_ptexSceneSpecularAccMap,
			CTexture::s_ptexBackBuffer,
			CTexture::s_ptexPrevBackBuffer[0][0],
			CTexture::s_ptexPrevBackBuffer[1][0],
			CTexture::s_ptexPrevBackBuffer[0][1],
			CTexture::s_ptexPrevBackBuffer[1][1],
			CTexture::s_ptexSceneTarget,
			CTexture::s_ptexZTarget,
			CTexture::s_ptexHDRTarget,
			CTexture::s_ptexStereoL,
			CTexture::s_ptexStereoR,
		};

		for (auto pTex : clearTextures)
		{
			if (CTexture::IsTextureExist(pTex))
				FX_ClearTarget(pTex, Clr_Empty);
		}
	}

	// sync dev buffer only once per frame, to prevent syncing to the currently rendered frame
	// which would result in a deadlock
	if (nFlags & (FRR_SYSTEM_RESOURCES | FRR_DELETED_MESHES))
	{
		gRenDev->m_DevBufMan.Sync(gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameUpdateID);
	}
}

void CD3D9Renderer::RT_CreateRenderResources()
{
	EF_Init();

	if (m_pPostProcessMgr)
	{
		m_pPostProcessMgr->CreateResources();
	}

	if (!m_pGraphicsPipeline)
	{
		m_pGraphicsPipeline = new CStandardGraphicsPipeline;
		m_pGraphicsPipeline->Init();
	}

#if RENDERER_SUPPORT_SCALEFORM
	if (!m_pSFResD3D)
	{
		SF_CreateResources();
		assert(m_pSFResD3D);
	}
#endif
}

void CD3D9Renderer::RT_PrecacheDefaultShaders()
{
	SShaderCombination cmb;
	m_cEF.s_ShaderStereo->mfPrecache(cmb, true, NULL);

#if CRY_RENDERER_GNM
	gGnmDevice->GnmCreateBuiltinPrograms();
#endif

#if RENDERER_SUPPORT_SCALEFORM
	SF_PrecacheShaders();
#endif
}

void CD3D9Renderer::RT_ResetGlass()
{
	CREBreakableGlassBuffer::RT_ReleaseInstance();
}

void CD3D9Renderer::SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode)
{
	m_pRT->RC_SetRendererCVar(pCVar, pArgText, bSilentMode);
}

void CD3D9Renderer::RT_SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode)
{
	if (pCVar)
	{
		pCVar->Set(pArgText);

		if (!bSilentMode)
		{
			if (gEnv->IsEditor())
				gEnv->pLog->LogWithType(ILog::eInputResponse, "%s = %s (Renderer CVar)", pCVar->GetName(), pCVar->GetString());
			else
				gEnv->pLog->LogWithType(ILog::eInputResponse, "    $3%s = $6%s $5(Renderer CVar)", pCVar->GetName(), pCVar->GetString());
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::StartLoadtimeFlashPlayback(ILoadtimeCallback* pCallback)
{
	// make sure we can't access loading mode twice!
	assert(!m_pRT->m_pLoadtimeCallback);
	if (m_pRT->m_pLoadtimeCallback)
	{
		return;
	}

	// TODO: check for r_shadersnocompile to prevent issue with device access from render load thread
	if (pCallback)
	{
		FlushRTCommands(true, true, true);

		SDisplayContext* pDC = GetActiveDisplayContext();
		m_pRT->m_pLoadtimeCallback = pCallback;
		SetViewport(0, 0, pDC->m_Width, pDC->m_Height);
		m_pRT->RC_StartVideoThread();

		// wait until render thread has fully processed the start of the video
		// to reduce the congestion on the IO reading (make sure nothing else
		// beats the video to actually start reading something from the DVD)
		while (m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Active)
		{
			m_pRT->FlushAndWait();
			Sleep(0);
		}
	}
}

void CD3D9Renderer::StopLoadtimeFlashPlayback()
{
	if (m_pRT->m_pLoadtimeCallback)
	{
		FlushRTCommands(true, true, true);

		m_pRT->RC_StopVideoThread();

		// wait until render thread has fully processed the shutdown of the loading thread
		while (m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Disabled)
		{
			m_pRT->FlushAndWait();
			Sleep(0);
		}

		m_pRT->m_pLoadtimeCallback = 0;

		m_pRT->RC_BeginFrame();
		m_pRT->RC_ClearTargetsImmediately(2, FRT_CLEAR_COLOR, Col_Black, Clr_FarPlane.r);

#if !defined(STRIP_RENDER_THREAD)
		if(m_pRT->m_CommandsLoading.size() > 0)
		{
			// Blit the accumulated commands from the renderloading thread into the current fill command queue
			// : Currently hacked into the RC_UpdateMaterialConstants command, but will be generalized soon
			void* buf = m_pRT->m_Commands[m_pRT->m_nCurThreadFill].Grow(m_pRT->m_CommandsLoading.size());
			memcpy(buf, &m_pRT->m_CommandsLoading[0], m_pRT->m_CommandsLoading.size());
			m_pRT->m_CommandsLoading.Free();
		}
#endif // !defined(STRIP_RENDER_THREAD)
	}
}

void CRenderer::RT_SubmitWind(const SWindGrid* pWind)
{
	m_pCurWindGrid = pWind;
	if (!CTexture::IsTextureExist(CTexture::s_ptexWindGrid))
	{
		CTexture::s_ptexWindGrid->Create2DTexture(pWind->m_nWidth, pWind->m_nHeight, 1, FT_DONT_RELEASE | FT_DONT_STREAM | FT_STAGE_UPLOAD, nullptr, eTF_R16G16F);
	}
	CDeviceTexture* pDevTex = CTexture::s_ptexWindGrid->GetDevTexture();
	int nThreadID = m_pRT->m_nCurThreadProcess;
	pDevTex->UploadFromStagingResource(0, [=](void* pData, uint32 rowPitch, uint32 slicePitch)
	{
		cryMemcpy(pData, pWind->m_pData, CTexture::TextureDataSize(pWind->m_nWidth, pWind->m_nHeight, 1, 1, 1, eTF_R16G16F));
		return true;
	});
}
