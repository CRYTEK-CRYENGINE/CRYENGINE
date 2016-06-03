// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DRenderRE.cpp : implementation of the Rendering RenderElements pipeline.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "../Common/RendElements/Stars.h"
#include <Cry3DEngine/I3DEngine.h>
#include "../Common/PostProcess/PostProcessUtils.h"
#include "../Common/PostProcess/PostEffects.h"
#include <CryRenderer/RenderElements/CREParticle.h>

#pragma warning(disable: 4244)

//=======================================================================

bool CRESky::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CD3D9Renderer* rd = gcpRendD3D;

#if !defined(_RELEASE)
	if (ef->m_eShaderType != eST_Sky)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Incorrect shader set for sky");
		return false;
	}
#endif

	if (!rd->m_RP.m_pShaderResources || !rd->m_RP.m_pShaderResources->m_pSky)
	{
		return false;
	}

	// pass 0 - skybox
	SSkyInfo* pSky = rd->m_RP.m_pShaderResources->m_pSky;
	if (!pSky->m_SkyBox[0])
		return false;

	float v(gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKYBOX_MULTIPLIER));
	rd->SetMaterialColor(v, v, v, m_fAlpha);

	if (!sfm)
	{
		ef->FXSetTechnique(CCryNameTSCRC((uint32)0));
	}

	uint32 nPasses = 0;
	ef->FXBegin(&nPasses, FEF_DONTSETTEXTURES);
	//ef->FXBegin(&nPasses, 0 );
	if (!nPasses)
	{
		return false;
	}
	ef->FXBeginPass(0);

	rd->FX_PushVP();
	rd->m_NewViewport.fMinZ = 1.0f;
	rd->m_NewViewport.fMaxZ = 1.0f;
	rd->m_bViewportDirty = true;

	STexState pTexState;
	pTexState.SetFilterMode(FILTER_LINEAR);
	pTexState.SetClampMode(1, 1, 1);

	int texStateID = CTexture::GetTexState(pTexState);

	const float fSkyBoxSize = SKY_BOX_SIZE;

	if (rd->m_RP.m_nBatchFilter & FB_Z)
	{
		CTexture::s_ptexBlack->Apply(0, texStateID);
		{
			// top
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(+fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(+fSkyBoxSize, +fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, +fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) }
			};
			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 4, eptTriangleStrip);
		}
		{
			// nesw
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, +fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(+fSkyBoxSize, -fSkyBoxSize, +fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(+fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(+fSkyBoxSize, +fSkyBoxSize, +fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(+fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, +fSkyBoxSize, +fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, +fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
			};
			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 10, eptTriangleStrip);
		}
		{
			// b
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(+fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(+fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) },
				{ Vec3(-fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 0) }
			};
			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 4, eptTriangleStrip);
		}
	}
	else
	{
		{
			// top
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(fSkyBoxSize,  -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(1, 1.f - 1) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 1.f - 1) },
				{ Vec3(fSkyBoxSize,  fSkyBoxSize,  fSkyBoxSize), {
								{ 0 }
					    }, Vec2(1, 1.f - 0) },
				{ Vec3(-fSkyBoxSize, fSkyBoxSize,  fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0, 1.f - 0) }
			};

			((CTexture*)(pSky->m_SkyBox[2]))->Apply(0, texStateID);
			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 4, eptTriangleStrip);
		}

		Vec3 camera = iSystem->GetViewCamera().GetPosition();
		camera.z = max(0.f, camera.z);

		float fWaterCamDiff = max(0.f, camera.z - m_fTerrainWaterLevel);

		float fMaxDist = gEnv->p3DEngine->GetMaxViewDistance() / 1024.f;
		float P = (fWaterCamDiff) / 128 + max(0.f, (fWaterCamDiff) * 0.03f / fMaxDist);

		P *= m_fSkyBoxStretching;

		float D = (fWaterCamDiff) / 10.0f * fSkyBoxSize / 124.0f - /*P*/ 0 + 8;

		D = max(0.f, D);

		if (m_fTerrainWaterLevel > camera.z && !(IsRecursiveRenderView()))
		{
			P = (fWaterCamDiff);
			D = (fWaterCamDiff);
		}

		float fTexOffset;
		fTexOffset = 1.0f / max(pSky->m_SkyBox[1]->GetHeight(), 1);
		{
			// s
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(1.0, 1.f - 1.0) },
				{ Vec3(fSkyBoxSize,  -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0.0, 1.f - 1.0) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, -P),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5 - fTexOffset) },
				{ Vec3(fSkyBoxSize,  -fSkyBoxSize, -P),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5 - fTexOffset) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, -D),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5 - fTexOffset) },
				{ Vec3(fSkyBoxSize,  -fSkyBoxSize, -D),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5 - fTexOffset) }
			};

			((CTexture*)(pSky->m_SkyBox[1]))->Apply(0, texStateID);
			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}
		{
			// e
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(-fSkyBoxSize, fSkyBoxSize,  fSkyBoxSize), {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.0) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.0) },
				{ Vec3(-fSkyBoxSize, fSkyBoxSize,  -P),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5f + fTexOffset) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, -P),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5f + fTexOffset) },
				{ Vec3(-fSkyBoxSize, fSkyBoxSize,  -D),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5f + fTexOffset) },
				{ Vec3(-fSkyBoxSize, -fSkyBoxSize, -D),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5f + fTexOffset) }
			};

			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}

		fTexOffset = 1.0f / max(pSky->m_SkyBox[0]->GetHeight(), 1);
		{
			// n
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(fSkyBoxSize,  fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(1.0, 1.f - 1.0) },
				{ Vec3(-fSkyBoxSize, fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0.0, 1.f - 1.0) },
				{ Vec3(fSkyBoxSize,  fSkyBoxSize, -P),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5 - fTexOffset) },
				{ Vec3(-fSkyBoxSize, fSkyBoxSize, -P),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5 - fTexOffset) },
				{ Vec3(fSkyBoxSize,  fSkyBoxSize, -D),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5 - fTexOffset) },
				{ Vec3(-fSkyBoxSize, fSkyBoxSize, -D),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5 - fTexOffset) }
			};

			((CTexture*)(pSky->m_SkyBox[0]))->Apply(0, texStateID);
			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}
		{
			// w
			SVF_P3F_C4B_T2F data[] =
			{
				{ Vec3(fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.0) },
				{ Vec3(fSkyBoxSize, fSkyBoxSize,  fSkyBoxSize), {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.0) },
				{ Vec3(fSkyBoxSize, -fSkyBoxSize, -P),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5f + fTexOffset) },
				{ Vec3(fSkyBoxSize, fSkyBoxSize,  -P),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5f + fTexOffset) },
				{ Vec3(fSkyBoxSize, -fSkyBoxSize, -D),          {
								{ 0 }
					    }, Vec2(1.0, 1.f - 0.5f + fTexOffset) },
				{ Vec3(fSkyBoxSize, fSkyBoxSize,  -D),          {
								{ 0 }
					    }, Vec2(0.0, 1.f - 0.5f + fTexOffset) }
			};
			CVertexBuffer vertexBuffer(data, eVF_P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}
	}

	rd->FX_PopVP();
	rd->FX_ResetPipe();

	return true;
}

static void FillSkyTextureData(CTexture* pTexture, const void* pData, const uint32 width, const uint32 height, const uint32 pitch)
{
	assert(pTexture && pTexture->GetWidth() == width && pTexture->GetHeight() == height);
	CDeviceTexture* pDevTex = pTexture->GetDevTexture();
	assert(pDevTex);

	if (!pDevTex)
		return;

	gcpRendD3D->GetDeviceContext().UpdateSubresource(pDevTex->Get2DTexture(), 0, 0, pData, sizeof(CryHalf4) * width, sizeof(CryHalf4) * width * height);
}

bool CREHDRSky::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CD3D9Renderer* rd(gcpRendD3D);

#if !defined(_RELEASE)
	if (ef->m_eShaderType != eST_Sky)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Incorrect shader set for sky");
		return false;
	}
#endif

	if (!rd->m_RP.m_pShaderResources || !rd->m_RP.m_pShaderResources->m_pSky)
		return false;
	SSkyInfo* pSky(rd->m_RP.m_pShaderResources->m_pSky);
	if (!pSky->m_SkyBox[0])
		return false;

	assert(m_pRenderParams);
	if (!m_pRenderParams)
		return false;

	assert(m_pRenderParams->m_pSkyDomeMesh.get());
	if (!m_pRenderParams->m_pSkyDomeMesh)
		return false;

	bool isNotZPass = (rd->m_RP.m_nBatchFilter & FB_Z) == 0;
	if (isNotZPass)
	{
		// re-create sky dome textures if necessary
		bool forceTextureUpdate(false);
		if (!CTexture::IsTextureExist(m_pSkyDomeTextureMie) || !CTexture::IsTextureExist(m_pSkyDomeTextureRayleigh))
		{
			GenerateSkyDomeTextures(SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight);
			forceTextureUpdate = true;
		}

		// dyn tex data lost due to device reset?
		if (m_frameReset != rd->m_nFrameReset)
		{
			forceTextureUpdate = true;
			m_frameReset = rd->m_nFrameReset;
		}

		// update sky dome texture if new data is available
		if (m_skyDomeTextureLastTimeStamp != m_pRenderParams->m_skyDomeTextureTimeStamp || forceTextureUpdate)
		{
			FillSkyTextureData(m_pSkyDomeTextureMie, m_pRenderParams->m_pSkyDomeTextureDataMie, SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, m_pRenderParams->m_skyDomeTexturePitch);
			FillSkyTextureData(m_pSkyDomeTextureRayleigh, m_pRenderParams->m_pSkyDomeTextureDataRayleigh, SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, m_pRenderParams->m_skyDomeTexturePitch);

			// update time stamp of last update
			m_skyDomeTextureLastTimeStamp = m_pRenderParams->m_skyDomeTextureTimeStamp;
		}
	}

	// render
	uint32 nPasses(0);
	ef->FXBegin(&nPasses, 0);
	if (!nPasses)
		return false;
	ef->FXBeginPass(0);

	I3DEngine* p3DEngine(gEnv->p3DEngine);

	rd->FX_PushVP();
	rd->m_NewViewport.fMinZ = 1.0f;
	rd->m_NewViewport.fMaxZ = 1.0f;
	rd->m_bViewportDirty = true;

	if (isNotZPass)
	{
		// shader constants -- set sky dome texture and texel size
		assert(m_pSkyDomeTextureMie && m_pSkyDomeTextureMie->GetWidth() == SSkyLightRenderParams::skyDomeTextureWidth && m_pSkyDomeTextureMie->GetHeight() == SSkyLightRenderParams::skyDomeTextureHeight);
		assert(m_pSkyDomeTextureRayleigh && m_pSkyDomeTextureRayleigh->GetWidth() == SSkyLightRenderParams::skyDomeTextureWidth && m_pSkyDomeTextureRayleigh->GetHeight() == SSkyLightRenderParams::skyDomeTextureHeight);
		Vec4 skyDomeTexSizeVec((float) SSkyLightRenderParams::skyDomeTextureWidth, (float) SSkyLightRenderParams::skyDomeTextureHeight, 0.0f, 0.0f);
		static CCryNameR Param1Name("SkyDome_TextureSize");
		ef->FXSetPSFloat(Param1Name, &skyDomeTexSizeVec, 1);
		Vec4 skyDomeTexelSizeVec(1.0f / (float) SSkyLightRenderParams::skyDomeTextureWidth, 1.0f / (float) SSkyLightRenderParams::skyDomeTextureHeight, 0.0f, 0.0f);
		static CCryNameR Param2Name("SkyDome_TexelSize");
		ef->FXSetPSFloat(Param2Name, &skyDomeTexelSizeVec, 1);

		// shader constants -- phase function constants
		static CCryNameR Param3Name("SkyDome_PartialMieInScatteringConst");
		static CCryNameR Param4Name("SkyDome_PartialRayleighInScatteringConst");
		static CCryNameR Param5Name("SkyDome_SunDirection");
		static CCryNameR Param6Name("SkyDome_PhaseFunctionConstants");
		ef->FXSetPSFloat(Param3Name, &m_pRenderParams->m_partialMieInScatteringConst, 1);
		ef->FXSetPSFloat(Param4Name, &m_pRenderParams->m_partialRayleighInScatteringConst, 1);
		ef->FXSetPSFloat(Param5Name, &m_pRenderParams->m_sunDirection, 1);
		ef->FXSetPSFloat(Param6Name, &m_pRenderParams->m_phaseFunctionConsts, 1);

		// shader constants -- night sky relevant constants
		Vec3 nightSkyHorizonCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR, nightSkyHorizonCol);
		Vec3 nightSkyZenithCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR, nightSkyZenithCol);
		float nightSkyZenithColShift(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT));
		const float minNightSkyZenithGradient(-0.1f);

		static CCryNameR Param7Name("SkyDome_NightSkyColBase");
		static CCryNameR Param8Name("SkyDome_NightSkyColDelta");
		static CCryNameR Param9Name("SkyDome_NightSkyZenithColShift");

		Vec4 nsColBase(nightSkyHorizonCol, 0);
		ef->FXSetPSFloat(Param7Name, &nsColBase, 1);
		Vec4 nsColDelta(nightSkyZenithCol - nightSkyHorizonCol, 0);
		ef->FXSetPSFloat(Param8Name, &nsColDelta, 1);
		Vec4 nsZenithColShift(1.0f / (nightSkyZenithColShift - minNightSkyZenithGradient), -minNightSkyZenithGradient / (nightSkyZenithColShift - minNightSkyZenithGradient), 0, 0);
		ef->FXSetPSFloat(Param9Name, &nsZenithColShift, 1);

		CREHDRSky::SetCommonMoonParams(ef, true);

		Vec3 nightMoonColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR, nightMoonColor);
		Vec4 nsMoonColor(nightMoonColor, 0);
		static CCryNameR Param11Name("SkyDome_NightMoonColor");
		ef->FXSetPSFloat(Param11Name, &nsMoonColor, 1);

		Vec3 nightMoonInnerCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR, nightMoonInnerCoronaColor);
		float nightMoonInnerCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE));
		Vec4 nsMoonInnerCoronaColorScale(nightMoonInnerCoronaColor, nightMoonInnerCoronaScale);
		static CCryNameR Param12Name("SkyDome_NightMoonInnerCoronaColorScale");
		ef->FXSetPSFloat(Param12Name, &nsMoonInnerCoronaColorScale, 1);

		Vec3 nightMoonOuterCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, nightMoonOuterCoronaColor);
		float nightMoonOuterCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE));
		Vec4 nsMoonOuterCoronaColorScale(nightMoonOuterCoronaColor, nightMoonOuterCoronaScale);
		static CCryNameR Param13Name("SkyDome_NightMoonOuterCoronaColorScale");
		ef->FXSetPSFloat(Param13Name, &nsMoonOuterCoronaColorScale, 1);
	}

	HRESULT hr(S_OK);

	// commit all render changes
	rd->FX_Commit();

	// set vertex declaration and streams of sky dome
	CRenderMesh* pSkyDomeMesh = static_cast<CRenderMesh*>(m_pRenderParams->m_pSkyDomeMesh.get());
	hr = rd->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F);
	if (!FAILED(hr))
	{
		// set vertex and index buffer
		pSkyDomeMesh->CheckUpdate(pSkyDomeMesh->_GetVertexFormat(), 0);
		size_t vbOffset(0);
		size_t ibOffset(0);
		D3DVertexBuffer* pVB = rd->m_DevBufMan.GetD3DVB(pSkyDomeMesh->_GetVBStream(VSF_GENERAL), &vbOffset);
		D3DIndexBuffer* pIB = rd->m_DevBufMan.GetD3DIB(pSkyDomeMesh->_GetIBStream(), &ibOffset);
		assert(pVB);
		assert(pIB);
		if (!pVB || !pIB)
			return false;

		rd->FX_SetVStream(0, pVB, vbOffset, pSkyDomeMesh->GetStreamStride(VSF_GENERAL));
		rd->FX_SetIStream(pIB, ibOffset, (sizeof(vtx_idx) == 2 ? Index16 : Index32));

		// draw sky dome
		rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, pSkyDomeMesh->_GetNumVerts(), 0, pSkyDomeMesh->_GetNumInds());

	}

	ef->FXEndPass();
	ef->FXEnd();

	if (m_pStars)
		m_pStars->Render(m_moonTexId > 0 ? true : false);

	rd->FX_PopVP();

	gcpRendD3D->FX_ResetPipe();

	return true;
}

void CStars::Render(bool bUseMoon)
{
	CD3D9Renderer* rd(gcpRendD3D);

	I3DEngine* p3DEngine(gEnv->p3DEngine);
	float starIntensity(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY));

	if (m_pStarMesh && m_pShader && rd->m_RP.m_nPassGroupID == EFSLIST_FORWARD_OPAQUE && (rd->m_RP.m_nBatchFilter & FB_GENERAL) && starIntensity > 1e-3f)
	{
		//////////////////////////////////////////////////////////////////////////
		// set shader

		static CCryNameTSCRC shaderTech("Stars");
		m_pShader->FXSetTechnique(shaderTech);
		uint32 nPasses(0);
		m_pShader->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		if (!nPasses)
			return;
		m_pShader->FXBeginPass(0);

		//////////////////////////////////////////////////////////////////////////
		// setup params

		int vpX(0), vpY(0), vpWidth(0), vpHeight(0);
		rd->GetViewport(&vpX, &vpY, &vpWidth, &vpHeight);
		const float size = 5.0f * min(1.f, min(vpWidth / 1280.f, vpHeight / 720.f));
		float flickerTime(gEnv->pTimer->GetCurrTime());
		static CCryNameR vspnStarSize("StarSize");
		Vec4 paramStarSize(size / (float) vpWidth, size / (float) vpHeight, 0, flickerTime * 0.5f);
		m_pShader->FXSetVSFloat(vspnStarSize, &paramStarSize, 1);

		static CCryNameR pspnStarIntensity("StarIntensity");
		Vec4 paramStarIntensity(starIntensity * min(1.f, size), 0, 0, 0);
		m_pShader->FXSetPSFloat(pspnStarIntensity, &paramStarIntensity, 1);

		CREHDRSky::SetCommonMoonParams(m_pShader, bUseMoon);

		//////////////////////////////////////////////////////////////////////////
		// commit & draw

		int32 nRenderState = GS_BLSRC_ONE | GS_BLDST_ONE;

		rd->FX_SetState(nRenderState);

		rd->FX_Commit();
		if (!FAILED(rd->FX_SetVertexDeclaration(0, eVF_P3S_C4B_T2S)))
		{
			size_t offset(0);
			//void* pVB(m_pStarVB->GetStream(VSF_GENERAL, &offset));
			//rd->FX_SetVStream(0, pVB, offset, m_VertexSize[m_pStarVB->m_vertexformat]);
			CRenderMesh* pStarMesh = static_cast<CRenderMesh*>(m_pStarMesh.get());
			pStarMesh->CheckUpdate(pStarMesh->_GetVertexFormat(), 0);
			D3DVertexBuffer* pVB = rd->m_DevBufMan.GetD3DVB(pStarMesh->_GetVBStream(VSF_GENERAL), &offset);
			rd->FX_SetVStream(0, pVB, offset, pStarMesh->GetStreamStride(VSF_GENERAL));
			rd->FX_SetIStream(0, 0, Index16);

			rd->FX_DrawPrimitive(eptTriangleList, 0, 6 * m_numStars);
		}

		m_pShader->FXEndPass();
		m_pShader->FXEnd();
	}
}

bool CREFogVolume::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CD3D9Renderer* rd(gcpRendD3D);

#if !defined(_RELEASE)
	if (ef->m_eShaderType != eST_PostProcess)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Incorrect shader set for fog volume");
		return false;
	}
#endif

	PROFILE_LABEL_SCOPE("FOG_VOLUME");

	rd->m_RP.m_PersFlags2 |= RBPF2_MSAA_SAMPLEFREQ_PASS;
	rd->FX_SetMSAAFlagsRT();

	// render
	uint32 nPasses(0);
	ef->FXBegin(&nPasses, 0);
	if (0 == nPasses)
	{
		assert(0);
		rd->m_RP.m_PersFlags2 &= ~RBPF2_MSAA_SAMPLEFREQ_PASS;
		return(false);
	}
	ef->FXBeginPass(0);

	if (m_viewerInsideVolume)
	{
		rd->SetCullMode(R_CULL_FRONT);
		int nState = GS_COLMASK_RGB | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
		nState |= m_nearCutoff ? 0 : GS_NODEPTHTEST;
		rd->FX_SetState(nState);
	}
	else
	{
		rd->SetCullMode(R_CULL_BACK);
		rd->FX_SetState(GS_COLMASK_RGB | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
	}

	// set vs constants
	static CCryNameR invObjSpaceMatrixName("invObjSpaceMatrix");
	ef->FXSetVSFloat(invObjSpaceMatrixName, (const Vec4*) &m_matWSInv.m00, 3);

	const Vec4 cEyePosVec(m_eyePosInWS, !m_viewerInsideVolume ? 1 : 0);
	static CCryNameR eyePosInWSName("eyePosInWS");
	ef->FXSetVSFloat(eyePosInWSName, &cEyePosVec, 1);

	const Vec4 cEyePosInOSVec(m_eyePosInOS, 0);
	static CCryNameR eyePosInOSName("eyePosInOS");
	ef->FXSetVSFloat(eyePosInOSName, &cEyePosInOSVec, 1);

	const Vec4 cNearCutoffVec(m_nearCutoff, m_nearCutoff, m_nearCutoff, m_nearCutoff);
	static CCryNameR nearCutoffName("nearCutoff");
	ef->FXSetVSFloat(nearCutoffName, &cNearCutoffVec, 1);

	// set ps constants
	const Vec4 cFogColVec(m_fogColor.r, m_fogColor.g, m_fogColor.b, 0);
	static CCryNameR fogColorName("fogColor");
	ef->FXSetPSFloat(fogColorName, &cFogColVec, 1);

	const Vec4 cGlobalDensityVec(m_globalDensity, 1.44269502f * m_globalDensity, 0, 0);
	static CCryNameR globalDensityName("globalDensity");
	ef->FXSetPSFloat(globalDensityName, &cGlobalDensityVec, 1);

	const Vec4 cDensityOffsetVec(m_densityOffset, m_densityOffset, m_densityOffset, m_densityOffset);
	static CCryNameR densityOffsetName("densityOffset");
	ef->FXSetPSFloat(densityOffsetName, &cDensityOffsetVec, 1);

	const Vec4 cHeigthFallOffBasePointVec(m_heightFallOffBasePoint, 0);
	static CCryNameR heightFallOffBasePointName("heightFallOffBasePoint");
	ef->FXSetPSFloat(heightFallOffBasePointName, &cHeigthFallOffBasePointVec, 1);

	const Vec4 cHeightFallOffDirScaledVec(m_heightFallOffDirScaled, 0);
	static CCryNameR heightFallOffDirScaledName("heightFallOffDirScaled");
	ef->FXSetPSFloat(heightFallOffDirScaledName, &cHeightFallOffDirScaledVec, 1);

	const Vec4 cOutsideSoftEdgesLerpVec(m_softEdgesLerp.x, m_softEdgesLerp.y, 0, 0);
	static CCryNameR outsideSoftEdgesLerpName("outsideSoftEdgesLerp");
	ef->FXSetPSFloat(outsideSoftEdgesLerpName, &cOutsideSoftEdgesLerpVec, 1);

	const Vec4 cEyePosInWSVec(m_eyePosInWS, 0);
	ef->FXSetPSFloat(eyePosInWSName, &cEyePosInWSVec, 1);

	const Vec4 cEyePosInOSx2Vec(2.0f * m_eyePosInOS, 0);
	static CCryNameR eyePosInOSx2Name("eyePosInOSx2");
	ef->FXSetPSFloat(eyePosInOSx2Name, &cEyePosInOSx2Vec, 1);

	// commit all render changes
	rd->FX_Commit();

	// set vertex declaration and streams of skydome
	if (!FAILED(rd->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
	{
		// define bounding box geometry
		const uint32 c_numBBVertices(8);
		SVF_P3F_C4B_T2F bbVertices[c_numBBVertices] =
		{
			{ Vec3(m_localAABB.min.x, m_localAABB.min.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.min.x, m_localAABB.max.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.max.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.min.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.min.x, m_localAABB.min.y, m_localAABB.max.z) },
			{ Vec3(m_localAABB.min.x, m_localAABB.max.y, m_localAABB.max.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.max.y, m_localAABB.max.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.min.y, m_localAABB.max.z) }
		};

		const uint32 c_numBBIndices(36);
		static const uint16 bbIndices[c_numBBIndices] =
		{
			0, 1, 2, 0, 2, 3,
			7, 6, 5, 7, 5, 4,
			3, 2, 6, 3, 6, 7,
			4, 5, 1, 4, 1, 0,
			1, 5, 6, 1, 6, 2,
			4, 0, 3, 4, 3, 7
		};

		// copy vertices into dynamic VB
		TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(bbVertices, c_numBBVertices, 0);

		// copy indices into dynamic IB
		TempDynIB16::CreateFillAndBind(bbIndices, c_numBBIndices);
		// draw skydome
		rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, c_numBBVertices, 0, c_numBBIndices);
	}

	ef->FXEndPass();
	ef->FXEnd();

	rd->m_RP.m_PersFlags2 &= ~RBPF2_MSAA_SAMPLEFREQ_PASS;

	return(true);
}

bool CREVolumeObject::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CD3D9Renderer* rd(gcpRendD3D);
	I3DEngine* p3DEngine(gEnv->p3DEngine);

	uint32 nFlagsPS2 = rd->m_RP.m_PersFlags2;
	rd->m_RP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF | RBPF2_COMMIT_CM);

	// render
	uint32 nPasses(0);
	ef->FXBegin(&nPasses, 0);
	if (!nPasses)
		return false;

	ef->FXBeginPass(0);

	if (m_nearPlaneIntersectsVolume)
	{
		rd->SetCullMode(R_CULL_FRONT);
		rd->FX_SetState(GS_COLMASK_RGB | GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
	}
	else
	{
		rd->SetCullMode(R_CULL_BACK);
		rd->FX_SetState(GS_COLMASK_RGB | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
	}

	// set vs constants
	static CCryNameR invObjSpaceMatrixName("invObjSpaceMatrix");
	ef->FXSetVSFloat(invObjSpaceMatrixName, (const Vec4*) &m_matInv.m00, 3);

	const Vec4 cEyePosVec(m_eyePosInWS, 0);
	static CCryNameR eyePosInWSName("eyePosInWS");
	ef->FXSetVSFloat(eyePosInWSName, &cEyePosVec, 1);

	const Vec4 cViewerOutsideVec(!m_viewerInsideVolume ? 1 : 0, m_nearPlaneIntersectsVolume ? 1 : 0, 0, 0);
	static CCryNameR viewerIsOutsideName("viewerIsOutside");
	ef->FXSetVSFloat(viewerIsOutsideName, &cViewerOutsideVec, 1);

	const Vec4 cEyePosInOSVec(m_eyePosInOS, 0);
	static CCryNameR eyePosInOSName("eyePosInOS");
	ef->FXSetVSFloat(eyePosInOSName, &cEyePosInOSVec, 1);

	// set ps constants
	const Vec4 cEyePosInWSVec(m_eyePosInWS, 0);
	ef->FXSetPSFloat(eyePosInWSName, &cEyePosInWSVec, 1);

	ColorF specColor(1, 1, 1, 1);
	ColorF diffColor(1, 1, 1, 1);

	CShaderResources* pRes(rd->m_RP.m_pShaderResources);
	if (pRes && pRes->HasLMConstants())
	{
		specColor = pRes->GetColorValue(EFTT_SPECULAR);
		diffColor = pRes->GetColorValue(EFTT_DIFFUSE);
	}

	Vec3 cloudShadingMultipliers;
	p3DEngine->GetGlobalParameter(E3DPARAM_CLOUDSHADING_MULTIPLIERS, cloudShadingMultipliers);

	Vec3 brightColor(p3DEngine->GetSunColor() * cloudShadingMultipliers.x);
	brightColor = brightColor.CompMul(Vec3(specColor.r, specColor.g, specColor.b));

	Vec3 darkColor(p3DEngine->GetSkyColor() * cloudShadingMultipliers.y);
	darkColor = darkColor.CompMul(Vec3(diffColor.r, diffColor.g, diffColor.b));

	{
		static CCryNameR darkColorName("darkColor");
		const Vec4 data(darkColor, m_alpha);
		ef->FXSetPSFloat(darkColorName, &data, 1);
	}
	{
		static CCryNameR brightColorName("brightColor");
		const Vec4 data(brightColor, m_alpha);
		ef->FXSetPSFloat(brightColorName, &data, 1);
	}

	const Vec4 cVolumeTraceStartPlane(m_volumeTraceStartPlane.n, m_volumeTraceStartPlane.d);
	static CCryNameR volumeTraceStartPlaneName("volumeTraceStartPlane");
	ef->FXSetPSFloat(volumeTraceStartPlaneName, &cVolumeTraceStartPlane, 1);

	const Vec4 cScaleConsts(m_scale, 0, 0, 0);
	static CCryNameR scaleConstsName("scaleConsts");
	ef->FXSetPSFloat(scaleConstsName, &cScaleConsts, 1);

	// TODO: optimize shader and remove need to pass inv obj space matrix
	ef->FXSetPSFloat(invObjSpaceMatrixName, (const Vec4*) &m_matInv.m00, 3);

	// commit all render changes
	rd->FX_Commit();

	// set vertex declaration and streams
	if (!FAILED(rd->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
	{
		CRenderMesh* pHullMesh = static_cast<CRenderMesh*>(m_pHullMesh.get());

		// set vertex and index buffer
		pHullMesh->CheckUpdate(pHullMesh->_GetVertexFormat(), 0);
		size_t vbOffset(0);
		size_t ibOffset(0);
		D3DVertexBuffer* pVB = rd->m_DevBufMan.GetD3DVB(pHullMesh->_GetVBStream(VSF_GENERAL), &vbOffset);
		D3DIndexBuffer* pIB = rd->m_DevBufMan.GetD3DIB(pHullMesh->_GetIBStream(), &ibOffset);
		assert(pVB);
		assert(pIB);
		if (!pVB || !pIB)
			return false;

		rd->FX_SetVStream(0, pVB, vbOffset, pHullMesh->GetStreamStride(VSF_GENERAL));
		rd->FX_SetIStream(pIB, ibOffset, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
		// draw
		rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, pHullMesh->_GetNumVerts(), 0, pHullMesh->_GetNumInds());
	}

	ef->FXEndPass();
	ef->FXEnd();

	rd->FX_ResetPipe();
	rd->m_RP.m_PersFlags2 = nFlagsPS2;

	return true;
}

#if !defined(EXCLUDE_DOCUMENTATION_PURPOSE)
bool CREPrismObject::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CD3D9Renderer* rd(gcpRendD3D);

	// render
	uint32 nPasses(0);
	ef->FXBegin(&nPasses, 0);
	if (!nPasses)
		return false;

	ef->FXBeginPass(0);

	// commit all render changes
	//	rd->FX_Commit();

	static SVF_P3F_C4B_T2F pScreenQuad[] =
	{
		{ Vec3(0, 0, 0), {
						{ 0 }
			    }, Vec2(0, 0) },
		{ Vec3(0, 1, 0), {
						{ 0 }
			    }, Vec2(0, 1) },
		{ Vec3(1, 0, 0), {
						{ 0 }
			    }, Vec2(1, 0) },
		{ Vec3(1, 1, 0), {
						{ 0 }
			    }, Vec2(1, 1) },
	};

	pScreenQuad[0].xyz = Vec3(0, 0, 0);
	pScreenQuad[1].xyz = Vec3(0, 1, 0);
	pScreenQuad[2].xyz = Vec3(1, 0, 0);
	pScreenQuad[3].xyz = Vec3(1, 1, 0);

	CVertexBuffer strip(pScreenQuad, eVF_P3F_C4B_T2F);
	gcpRendD3D->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);

	// count rendered polygons
	//	rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nPolygons[rd->m_RP.m_nPassGroupDIP] += 2;
	//	rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nDIPs[rd->m_RP.m_nPassGroupDIP]++;

	ef->FXEndPass();
	ef->FXEnd();

	return true;
}
#endif // EXCLUDE_DOCUMENTATION_PURPOSE

bool CREWaterVolume::mfDraw(CShader* ef, SShaderPass* sfm)
{
	assert(m_pParams);
	if (!m_pParams)
		return false;

	CD3D9Renderer* rd(gcpRendD3D);

	IF (ef->m_eShaderType != eST_Water, 0)
	{
#if !defined(_RELEASE)
		// CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Incorrect shader set for water / water fog volume");
#endif
		return false;
	}

	bool bCaustics = CRenderer::CV_r_watercaustics &&
	                 CRenderer::CV_r_watercausticsdeferred &&
	                 CRenderer::CV_r_watervolumecaustics &&
	                 m_pParams->m_caustics &&
	                 (-m_pParams->m_fogPlane.d >= 1.0f); // unfortunately packing to RG8 limits us to heights over 1 meter, so we just disable if volume goes below.

	// Don't render caustics pass unless needed.
	if ((rd->m_RP.m_nBatchFilter & FB_WATER_CAUSTIC) && !bCaustics)
		return false;

	uint64 nFlagsShaderRTSave = gcpRendD3D->m_RP.m_FlagsShader_RT;
	rd->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE5]);
	const bool renderFogShadowWater = (CRenderer::CV_r_FogShadowsWater > 0) && (m_pParams->m_fogShadowing > 0.01f);

	Vec4 volFogShadowRange(0, 0, clamp_tpl(m_pParams->m_fogShadowing, 0.0f, 1.0f), 1.0f - clamp_tpl(m_pParams->m_fogShadowing, 0.0f, 1.0f));
#if defined(VOLUMETRIC_FOG_SHADOWS)
	const bool renderFogShadow = gcpRendD3D->m_bVolFogShadowsEnabled;
	{
		Vec3 volFogShadowRangeP;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_RANGE, volFogShadowRangeP);
		volFogShadowRangeP.x = clamp_tpl(volFogShadowRangeP.x, 0.01f, 1.0f);
		volFogShadowRange.x = volFogShadowRangeP.x;
		volFogShadowRange.y = volFogShadowRangeP.y;
	}

	if (renderFogShadow)
		gcpRendD3D->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5];

	if (!renderFogShadowWater) // set up forward shadows in case they will not be set up below
		rd->FX_SetupShadowsForTransp();

#endif

	if (renderFogShadowWater)
	{
		rd->FX_SetupShadowsForTransp();
		gcpRendD3D->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	}

	if (!m_drawWaterSurface && m_pParams->m_viewerInsideVolume)
	{
		// set projection matrix for full screen quad
		rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_matProj->Push();
		Matrix44A* m = rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_matProj->GetTop();
		mathMatrixOrthoOffCenterLH(m, -1, 1, -1, 1, -1, 1);
		if (!IsRecursiveRenderView())
		{
			const SRenderTileInfo& rti = rd->GetRenderTileInfo();
			if (rti.nGridSizeX > 1.f || rti.nGridSizeY > 1.f)
			{
				// shift and scale viewport
				m->m00 *= rti.nGridSizeX;
				m->m11 *= rti.nGridSizeY;
				m->m30 = -((rti.nGridSizeX - 1.f) - rti.nPosX * 2.0f);
				m->m31 = ((rti.nGridSizeY - 1.f) - rti.nPosY * 2.0f);
			}
		}

		// set view matrix to identity
		rd->PushMatrix();
		rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_matView->LoadIdentity();

		rd->EF_DirtyMatrix();
	}

	// render
	uint32 nPasses(0);
	ef->FXBegin(&nPasses, 0);
	if (0 == nPasses)
	{
		// reset matrices
		rd->PopMatrix();
		rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_matProj->Pop();
		rd->EF_DirtyMatrix();
		return false;
	}
	ef->FXBeginPass(0);

	// set ps constants
	if (!m_drawWaterSurface || m_drawFastPath && !m_pParams->m_viewerInsideVolume)
	{
		if (!m_pOceanParams)
		{
			// fog color & density
			const Vec3 col = m_pParams->m_fogColorAffectedBySun ? m_pParams->m_fogColor.CompMul(gEnv->p3DEngine->GetSunColor()) : m_pParams->m_fogColor;
			const Vec4 fogColorDensity(col, 1.44269502f * m_pParams->m_fogDensity);   // log2(e) = 1.44269502
			static CCryNameR Param1Name("cFogColorDensity");
			ef->FXSetPSFloat(Param1Name, &fogColorDensity, 1);
		}
		else
		{
			// fog color & density
			Vec4 fogColorDensity(m_pOceanParams->m_fogColor.CompMul(gEnv->p3DEngine->GetSunColor()), 1.44269502f * m_pOceanParams->m_fogDensity);     // log2(e) = 1.44269502
			static CCryNameR Param1Name("cFogColorDensity");
			ef->FXSetPSFloat(Param1Name, &fogColorDensity, 1);

			// fog color shallow & water level
			Vec4 fogColorShallowWaterLevel(m_pOceanParams->m_fogColorShallow.CompMul(gEnv->p3DEngine->GetSunColor()), -m_pParams->m_fogPlane.d);
			static CCryNameR Param2Name("cFogColorShallowWaterLevel");
			ef->FXSetPSFloat(Param2Name, &fogColorShallowWaterLevel, 1);

			if (m_pParams->m_viewerInsideVolume)
			{
				// under water in-scattering constant term = exp2( -fogDensity * ( waterLevel - cameraPos.z) )
				float c(expf(-m_pOceanParams->m_fogDensity * (-m_pParams->m_fogPlane.d - rd->GetCamera().GetPosition().z)));
				Vec4 underWaterInScatterConst(c, 0, 0, 0);
				static CCryNameR Param3Name("cUnderWaterInScatterConst");
				ef->FXSetPSFloat(Param3Name, &underWaterInScatterConst, 1);
			}
		}

		Vec4 fogPlane(m_pParams->m_fogPlane.n.x, m_pParams->m_fogPlane.n.y, m_pParams->m_fogPlane.n.z, m_pParams->m_fogPlane.d);
		static CCryNameR Param4Name("cFogPlane");
		ef->FXSetPSFloat(Param4Name, &fogPlane, 1);

		if (m_pParams->m_viewerInsideVolume)
		{
			Vec4 perpDist(m_pParams->m_fogPlane | rd->GetRCamera().vOrigin, 0, 0, 0);
			static CCryNameR Param5Name("cPerpDist");
			ef->FXSetPSFloat(Param5Name, &perpDist, 1);
		}
	}

	// Disable fog when inside volume or when not using fast path - could assign custom RT flag for this instead
	if (m_drawFastPath && m_pParams->m_viewerInsideVolume || !m_drawFastPath && m_drawWaterSurface)
	{
		// fog color & density
		const Vec4 fogColorDensity(0, 0, 0, 0);
		static CCryNameR Param1Name("cFogColorDensity");
		ef->FXSetPSFloat(Param1Name, &fogColorDensity, 1);
	}

	{
		static CCryNameR pszParamBBoxMin("vBBoxMin");
		static CCryNameR pszParamBBoxMax("vBBoxMax");
		const Vec4 vBBoxMin(m_pParams->m_WSBBox.min, 1.0f);
		const Vec4 vBBoxMax(m_pParams->m_WSBBox.max, 1.0f);
		ef->FXSetPSFloat(pszParamBBoxMin, &vBBoxMin, 1);
		ef->FXSetPSFloat(pszParamBBoxMax, &vBBoxMax, 1);
	}

	// set vs constants
	Vec4 viewerColorToWaterPlane(m_pParams->m_viewerCloseToWaterPlane && m_pParams->m_viewerCloseToWaterVolume ? 0.0f : 1.0f,
	                             m_pParams->m_viewerCloseToWaterVolume ? 2.0f * 2.0f : 0.0f,
	                             0.0f,
	                             0.0f);
	static CCryNameR Param6Name("cViewerColorToWaterPlane");
	ef->FXSetVSFloat(Param6Name, &viewerColorToWaterPlane, 1);

	if (bCaustics)
	{
		Vec4 pCausticsParams = Vec4(CRenderer::CV_r_watercausticsdistance, m_pParams->m_causticIntensity, m_pParams->m_causticTiling, m_pParams->m_causticHeight);

		static CCryNameR m_pCausticParams("vCausticParams");
		ef->FXSetPSFloat(m_pCausticParams, &pCausticsParams, 1);
	}

	static CCryNameR volFogShadowRangeN("volFogShadowRange");
	ef->FXSetPSFloat(volFogShadowRangeN, &volFogShadowRange, 1);

	if (renderFogShadowWater)
	{
		//set world basis
		float maskRTWidth = float(rd->GetWidth());
		float maskRTHeight = float(rd->GetHeight());

		Vec4 vScreenScale(1.0f / maskRTWidth, 1.0f / maskRTHeight, 0.0f, 0.0f);

		Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
		Vec4 vParamValue, vMag;
		CShadowUtils::ProjectScreenToWorldExpansionBasis(rd->m_IdentityMatrix, rd->GetCamera(), rd->m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, true, NULL);

		Vec4 vWorldBasisX = vWBasisX;
		Vec4 vWorldBasisY = vWBasisY;
		Vec4 vWorldBasisZ = vWBasisZ;
		Vec4 vCameraPos = vCamPos;

		static CCryNameR m_pScreenScale("ScreenScale");
		static CCryNameR m_pCamPos("vCamPos");
		static CCryNameR m_pWBasisX("vWBasisX");
		static CCryNameR m_pWBasisY("vWBasisY");
		static CCryNameR m_pWBasisZ("vWBasisZ");

		ef->FXSetPSFloat(m_pScreenScale, &vScreenScale, 1);
		ef->FXSetPSFloat(m_pCamPos, &vCameraPos, 1);
		ef->FXSetPSFloat(m_pWBasisX, &vWorldBasisX, 1);
		ef->FXSetPSFloat(m_pWBasisY, &vWorldBasisY, 1);
		ef->FXSetPSFloat(m_pWBasisZ, &vWorldBasisZ, 1);
	}

#if defined(VOLUMETRIC_FOG_SHADOWS)
	if (renderFogShadow)
	{
		Vec3 volFogShadowDarkeningP;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_DARKENING, volFogShadowDarkeningP);

		Vec4 volFogShadowDarkening(volFogShadowDarkeningP, 0);
		static CCryNameR volFogShadowDarkeningN("volFogShadowDarkening");
		ef->FXSetPSFloat(volFogShadowDarkeningN, &volFogShadowDarkening, 1);

		const float aSun = (1.0f - clamp_tpl(volFogShadowDarkeningP.y, 0.0f, 1.0f)) * 1.0f;
		const float bSun = 1.0f - aSun;
		const float aAmb = (1.0f - clamp_tpl(volFogShadowDarkeningP.z, 0.0f, 1.0f)) * 0.4f;
		const float bAmb = 1.0f - aAmb;

		Vec4 volFogShadowDarkeningSunAmb(aSun, bSun, aAmb, bAmb);
		static CCryNameR volFogShadowDarkeningSunAmbN("volFogShadowDarkeningSunAmb");
		ef->FXSetPSFloat(volFogShadowDarkeningSunAmbN, &volFogShadowDarkeningSunAmb, 1);
	}
#endif

	if (m_drawWaterSurface || !m_pParams->m_viewerInsideVolume)
	{
		// copy vertices into dynamic VB
		TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(m_pParams->m_pVertices, m_pParams->m_numVertices, 0);

		// copy indices into dynamic IB
		TempDynIB16::CreateFillAndBind(m_pParams->m_pIndices, m_pParams->m_numIndices);

		// set vertex declaration
		if (!FAILED(rd->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
		{
			// commit all render changes
			rd->FX_Commit();

			// draw
			ERenderPrimitiveType eType = eptTriangleList;
			if (CHWShader_D3D::s_pCurInstHS)
				eType = ept3ControlPointPatchList;
			rd->FX_DrawIndexedPrimitive(eType, 0, 0, m_pParams->m_numVertices, 0, m_pParams->m_numIndices);
		}
	}
	else
	{
		// copy vertices into dynamic VB
		TempDynVB<SVF_P3F_T3F> vb;
		vb.Allocate(4);
		SVF_P3F_T3F* pVB = vb.Lock();

		Vec3 coords[8];
		rd->GetRCamera().CalcVerts(coords);

		pVB[0].p.x = -1;
		pVB[0].p.y = 1;
		pVB[0].p.z = 0.5f;
		pVB[0].st = coords[5] - coords[1];

		pVB[1].p.x = 1;
		pVB[1].p.y = 1;
		pVB[1].p.z = 0.5f;
		pVB[1].st = coords[4] - coords[0];

		pVB[2].p.x = -1;
		pVB[2].p.y = -1;
		pVB[2].p.z = 0.5f;
		pVB[2].st = coords[6] - coords[2];

		pVB[3].p.x = 1;
		pVB[3].p.y = -1;
		pVB[3].p.z = 0.5f;
		pVB[3].st = coords[7] - coords[3];

		vb.Unlock();
		vb.Bind(0);
		vb.Release();

		// set vertex declaration
		if (!FAILED(rd->FX_SetVertexDeclaration(0, eVF_P3F_T3F)))
		{
			// commit all render changes
			rd->FX_Commit();
			// draw
			rd->FX_DrawPrimitive(eptTriangleStrip, 0, 4);
		}

		// reset matrices
		rd->PopMatrix();
		rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_matProj->Pop();
		rd->EF_DirtyMatrix();
	}

	ef->FXEndPass();
	ef->FXEnd();

	gcpRendD3D->m_RP.m_FlagsShader_RT = nFlagsShaderRTSave;

	return true;
}

void CREWaterOcean::Create(uint32 nVerticesCount, SVF_P3F_C4B_T2F* pVertices, uint32 nIndicesCount, const void* pIndices, uint32 nIndexSizeof)
{
	if (!nVerticesCount || !pVertices || !nIndicesCount || !pIndices || (nIndexSizeof != 2 && nIndexSizeof != 4))
		return;

	CD3D9Renderer* rd(gcpRendD3D);
	ReleaseOcean();

	m_nVerticesCount = nVerticesCount;
	m_nIndicesCount = nIndicesCount;
	m_nIndexSizeof = nIndexSizeof;
	HRESULT hr(S_OK);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Create vertex buffer
	//////////////////////////////////////////////////////////////////////////////////////////////////
	{
		D3DVertexBuffer* pVertexBuffer = 0;
		D3D11_BUFFER_DESC BufDesc;
		SVF_P3F_C4B_T2F* dst = 0;

		uint32 size = nVerticesCount * sizeof(SVF_P3F_C4B_T2F);
		BufDesc.ByteWidth = size;
		BufDesc.Usage = D3D11_USAGE_DEFAULT;
		BufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufDesc.CPUAccessFlags = 0;
		BufDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA pInitData;
		pInitData.pSysMem = pVertices;
		pInitData.SysMemPitch = 0;
		pInitData.SysMemSlicePitch = 0;

		gcpRendD3D->GetDevice().CreateBuffer(&BufDesc, &pInitData, &pVertexBuffer);
		m_pVertices = pVertexBuffer;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Create index buffer
	//////////////////////////////////////////////////////////////////////////////////////////////////
	{
		D3DIndexBuffer* pIndexBuffer = 0;
		const uint32 size = nIndicesCount * m_nIndexSizeof;

		D3D11_BUFFER_DESC BufDesc;
		BufDesc.ByteWidth = size;
		BufDesc.Usage = D3D11_USAGE_DEFAULT;
		BufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		BufDesc.CPUAccessFlags = 0;
		BufDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA pInitData;
		pInitData.pSysMem = pIndices;
		pInitData.SysMemPitch = 0;
		pInitData.SysMemSlicePitch = 0;

		gcpRendD3D->GetDevice().CreateBuffer(&BufDesc, &pInitData, &pIndexBuffer);
		m_pIndices = pIndexBuffer;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::FrameUpdate()
{
	// Update Water simulator
	//  UpdateFFT();

	static bool bInitialize = true;
	static Vec4 pParams0(0, 0, 0, 0), pParams1(0, 0, 0, 0);

	Vec4 pCurrParams0, pCurrParams1;
	gEnv->p3DEngine->GetOceanAnimationParams(pCurrParams0, pCurrParams1);

	// why no comparison operator on Vec4 ??
	if (bInitialize || pCurrParams0.x != pParams0.x || pCurrParams0.y != pParams0.y ||
	    pCurrParams0.z != pParams0.z || pCurrParams0.w != pParams0.w || pCurrParams1.x != pParams1.x ||
	    pCurrParams1.y != pParams1.y || pCurrParams1.z != pParams1.z || pCurrParams1.w != pParams1.w)
	{
		pParams0 = pCurrParams0;
		pParams1 = pCurrParams1;
		WaterSimMgr()->Create(1.0, pParams0.x, pParams0.z, 1.0f, 1.0f);
		bInitialize = false;
	}

	const int nGridSize = 64;

	// Update Vertex Texture
	if (!CTexture::IsTextureExist(CTexture::s_ptexWaterOcean))
	{
		CTexture::s_ptexWaterOcean->Create2DTexture(nGridSize, nGridSize, 1,
		                                            FT_DONT_RELEASE | FT_NOMIPS | FT_STAGE_UPLOAD,
		                                            0, eTF_R32G32B32A32F, eTF_R32G32B32A32F);
		//	CTexture::s_ptexWaterOcean->SetVertexTexture(true);
	}

	CTexture* pTexture = CTexture::s_ptexWaterOcean;

	// Copy data..
	if (CTexture::IsTextureExist(pTexture))
	{
		const float fUpdateTime = 0.125f * gEnv->pTimer->GetCurrTime();// / clamp_tpl<float>(pParams1.x, 0.55f, 1.0f);
		int nFrameID = gRenDev->GetFrameID();
		void* pRawPtr = NULL;
		WaterSimMgr()->Update(nFrameID, fUpdateTime, false, pRawPtr);

		Vec4* pDispGrid = WaterSimMgr()->GetDisplaceGrid();
		if (pDispGrid == NULL)
			return;

		const uint32 pitch = 4 * sizeof(f32) * nGridSize;
		const uint32 width = nGridSize;
		const uint32 height = nGridSize;

		STALL_PROFILER("update subresource")

		CDeviceTexture * pDevTex = pTexture->GetDevTexture();
		pDevTex->UploadFromStagingResource(0, [=](void* pData, uint32 rowPitch, uint32 slicePitch)
		{
			cryMemcpy(pData, pDispGrid, 4 * width * height * sizeof(f32));
			return true;
		});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::ReleaseOcean()
{
	ID3D11Buffer* pVertices = (ID3D11Buffer*)m_pVertices;
	ID3D11Buffer* pIndices = (ID3D11Buffer*)m_pIndices;

	SAFE_RELEASE(pVertices);
	SAFE_RELEASE(pIndices);

	m_pVertices = nullptr;
	m_pIndices = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CREWaterOcean::mfDraw(CShader* ef, SShaderPass* sfm)
{
	if (!m_nVerticesCount || !m_nIndicesCount || !m_pVertices || !m_pIndices)
		return false;

	CD3D9Renderer* rd(gcpRendD3D);

	FrameUpdate();

	int32 nGpuID = 0;// gRenDev->RT_GetCurrGpuID();
	if (CTexture::s_ptexWaterOcean)
	{
		CTexture::s_ptexWaterOcean->SetFilterMode(FILTER_LINEAR);
		CTexture::s_ptexWaterOcean->SetClampingMode(0, 0, 1);
		CTexture::s_ptexWaterOcean->UpdateTexStates();
	}

	if (CTexture::s_ptexWaterRipplesDDN)
	{
		CTexture::s_ptexWaterRipplesDDN->SetVertexTexture(true);
		CTexture::s_ptexWaterRipplesDDN->SetFilterMode(FILTER_LINEAR);
		CTexture::s_ptexWaterRipplesDDN->SetClampingMode(0, 0, 1);
		CTexture::s_ptexWaterRipplesDDN->UpdateTexStates();
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////

	uint64 nFlagsShaderRTprev = rd->m_RP.m_FlagsShader_RT;
	uint32 nFlagsPF2prev = rd->m_RP.m_PersFlags2;

	//  rd->m_RP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF|RBPF2_COMMIT_CM);

	// render
	uint32 nPasses(0);

	// set streams
	HRESULT hr(S_OK);

	STexStageInfo pPrevTexState0 = CTexture::s_TexStages[0];
	STexStageInfo pPrevTexState1 = CTexture::s_TexStages[1];

	STexState pState(FILTER_BILINEAR, false);
	const int texStateID(CTexture::GetTexState(pState));

	N3DEngineCommon::SOceanInfo& OceanInfo = gRenDev->m_p3DEngineCommon.m_OceanInfo;
	Vec4& pParams = OceanInfo.m_vMeshParams;

	uint32 nPrevStateOr = rd->m_RP.m_StateOr;
	uint32 nPrevStateAnd = rd->m_RP.m_StateAnd;

	ef->FXSetTechnique("Water");
	ef->FXBegin(&nPasses, 0);

	if (0 == nPasses)
		return false;

	if (gRenDev->GetRCamera().vOrigin.z > OceanInfo.m_fWaterLevel)
	{
		rd->m_RP.m_StateAnd |= GS_DEPTHFUNC_MASK;
		rd->m_RP.m_StateOr |= GS_DEPTHWRITE | GS_DEPTHFUNC_LEQUAL;
	}

	ef->FXBeginPass(0);

	if (CTexture::s_ptexWaterOcean)
	{
		CTexture::s_ptexWaterOcean->SetVertexTexture(true);
		CTexture::s_ptexWaterOcean->Apply(0, texStateID);
		CTexture::s_ptexWaterOcean->SetVertexTexture(false);
	}

	if (CTexture::s_ptexWaterRipplesDDN)
	{
		CTexture::s_ptexWaterRipplesDDN->SetVertexTexture(true);
		CTexture::s_ptexWaterRipplesDDN->Apply(1, texStateID);
		CTexture::s_ptexWaterRipplesDDN->SetVertexTexture(false);
	}

	hr = rd->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F);
	if (!FAILED(hr))
	{
		// commit all render changes
		rd->FX_Commit();

		rd->FX_SetVStream(0, m_pVertices, 0, sizeof(SVF_P3F_C4B_T2F));
		rd->FX_SetIStream(m_pIndices, 0, (m_nIndexSizeof == 2 ? Index16 : Index32));

		ERenderPrimitiveType eType = rd->m_bUseWaterTessHW ? eptTriangleList : eptTriangleStrip;
#ifdef WATER_TESSELLATION_RENDERER
		if (CHWShader_D3D::s_pCurInstHS)
			eType = ept3ControlPointPatchList;
#endif
		rd->FX_DrawIndexedPrimitive(eType, 0, 0, m_nVerticesCount, 0, m_nIndicesCount);
	}

	ef->FXEndPass();
	ef->FXEnd();

	rd->m_RP.m_StateOr = nPrevStateOr;
	rd->m_RP.m_StateAnd = nPrevStateAnd;

	CTexture::s_TexStages[0] = pPrevTexState0;
	CTexture::s_TexStages[1] = pPrevTexState1;

	gcpRendD3D->FX_ResetPipe();

	rd->m_RP.m_FlagsShader_RT = nFlagsShaderRTprev;
	rd->m_RP.m_PersFlags2 = nFlagsPF2prev;

	return true;
}

//=========================================================================================

CREOcclusionQuery::~CREOcclusionQuery()
{
	mfReset();
}

void CREOcclusionQuery::mfReset()
{
	ID3D11Query* pVizQuery = (ID3D11Query*)m_nOcclusionID;
	SAFE_RELEASE(pVizQuery);

	m_nOcclusionID = 0;
	m_nDrawFrame = 0;
	m_nCheckFrame = 0;
	m_nVisSamples = 0;
	m_bSucceeded = false;
}

uint32 CREOcclusionQuery::m_nQueriesPerFrameCounter = 0;
uint32 CREOcclusionQuery::m_nReadResultNowCounter = 0;
uint32 CREOcclusionQuery::m_nReadResultTryCounter = 0;

bool CREOcclusionQuery::mfDraw(CShader* ef, SShaderPass* sfm)
{
	PROFILE_FRAME(CREOcclusionQuery::mfDraw);

	CD3D9Renderer* r = gcpRendD3D;

	gRenDev->m_cEF.mfRefreshSystemShader("OcclusionTest", CShaderMan::s_ShaderOcclTest);

	CShader* pSh = CShaderMan::s_ShaderOcclTest;
	if (!pSh || pSh->m_HWTechniques.empty())
		return false;

	if (!(r->m_Features & RFT_OCCLUSIONTEST))
	{
		// If not supported
		m_nVisSamples = r->GetWidth() * r->GetHeight();
		return true;
	}

	int w = r->GetWidth();
	int h = r->GetHeight();

	if (!m_nOcclusionID)
	{
		HRESULT hr;
		ID3D11Query* pVizQuery = NULL;
		D3D11_QUERY_DESC qdesc;
		qdesc.MiscFlags = 0; //D3D11_QUERY_MISC_PREDICATEHINT;
		qdesc.Query = D3D11_QUERY_OCCLUSION;
		hr = r->GetDevice().CreateQuery(&qdesc, &pVizQuery);
		if (pVizQuery)
			m_nOcclusionID = (UINT_PTR)pVizQuery;
	}

	int nFrame = r->GetFrameID();

	if (!m_nDrawFrame) // only allow queries update, if finished already with previous query
	{
		// draw test box
		if (m_nOcclusionID)
		{
			D3DQuery* pVizQuery = (D3DQuery*)m_nOcclusionID;
			r->GetDeviceContext().Begin(pVizQuery);

			const t_arrDeferredMeshIndBuff& arrDeferredInds = r->GetDeferredUnitBoxIndexBuffer();
			const t_arrDeferredMeshVertBuff& arrDeferredVerts = r->GetDeferredUnitBoxVertexBuffer();

			//allocate vertices
			TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(&arrDeferredVerts[0], arrDeferredVerts.size(), 0);

			//allocate indices
			TempDynIB16::CreateFillAndBind(&arrDeferredInds[0], arrDeferredInds.size());

			r->m_RP.m_TI[r->m_RP.m_nProcessThreadID].m_matView->Push();
			Matrix34 mat;
			mat.SetIdentity();
			mat.SetScale(m_vBoxMax - m_vBoxMin, m_vBoxMin);

			const Matrix44 cTransPosed = Matrix44(mat).GetTransposed();
			r->m_RP.m_TI[r->m_RP.m_nProcessThreadID].m_matView->MultMatrixLocal(&cTransPosed);
			r->EF_DirtyMatrix();

			uint32 nPasses;
			pSh->FXSetTechnique("General");
			pSh->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			pSh->FXBeginPass(0);

			int nPersFlagsSave = r->m_RP.m_TI[r->m_RP.m_nProcessThreadID].m_PersFlags;
			uint64 nObjFlagsSave = r->m_RP.m_ObjFlags;
			CRenderObject* pCurObjectSave = r->m_RP.m_pCurObject;
			CShader* pShaderSave = r->m_RP.m_pShader;
			SShaderTechnique* pCurTechniqueSave = r->m_RP.m_pCurTechnique;

			if (r->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F) == S_OK)
			{
				r->m_RP.m_TI[r->m_RP.m_nProcessThreadID].m_PersFlags &= ~RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY;
				r->m_RP.m_ObjFlags &= ~FOB_TRANS_MASK;
				r->m_RP.m_pCurObject = r->m_RP.m_pIdendityRenderObject;
				r->m_RP.m_pShader = pSh;
				r->m_RP.m_pCurTechnique = pSh->m_HWTechniques[0];
				r->FX_SetState(GS_COLMASK_NONE | GS_DEPTHFUNC_LEQUAL);
				r->SetCullMode(R_CULL_NONE);

				r->FX_Commit();

				r->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, arrDeferredVerts.size(), 0, arrDeferredInds.size());
			}

			pSh->FXEndPass();
			pSh->FXEnd();

			r->m_RP.m_TI[r->m_RP.m_nProcessThreadID].m_matView->Pop();
			r->m_RP.m_TI[r->m_RP.m_nProcessThreadID].m_PersFlags = nPersFlagsSave;
			r->m_RP.m_ObjFlags = nObjFlagsSave;
			r->m_RP.m_pCurObject = pCurObjectSave;
			r->m_RP.m_pShader = pShaderSave;
			r->m_RP.m_pCurTechnique = pCurTechniqueSave;

			r->GetDeviceContext().End(pVizQuery);

			CREOcclusionQuery::m_nQueriesPerFrameCounter++;
			m_nDrawFrame = 1;
		}

		m_bSucceeded = false;
		// m_nDrawFrame = nFrame;
	}

	return true;
}

bool CREOcclusionQuery::mfReadResult_Now()
{
	int nFrame = gcpRendD3D->GetFrameID();

	ID3D11Query* pVizQuery = (ID3D11Query*)m_nOcclusionID;
	if (pVizQuery)
	{
		HRESULT hRes = S_FALSE;
		while (hRes == S_FALSE)
			hRes = gcpRendD3D->GetDeviceContext().GetData(pVizQuery, (void*) &m_nVisSamples, sizeof(uint64), 0);

		if (hRes == S_OK)
		{
			m_nDrawFrame = 0;
			m_nCheckFrame = nFrame;
		}

	}

	m_nReadResultNowCounter++;

	m_bSucceeded = (m_nCheckFrame == nFrame);
	return m_bSucceeded;
}

bool CREOcclusionQuery::mfReadResult_Try(uint32 nDefaultNumSamples)
{
	return gRenDev->m_pRT->RC_OC_ReadResult_Try(nDefaultNumSamples, this);
}
bool CREOcclusionQuery::RT_ReadResult_Try(uint32 nDefaultNumSamples)
{
	PROFILE_FRAME(CREOcclusionQuery::mfReadResult_Try);

	int nFrame = gcpRendD3D->GetFrameID();

	ID3D11Query* pVizQuery = (ID3D11Query*)m_nOcclusionID;
	if (pVizQuery)
	{
		HRESULT hRes = gcpRendD3D->GetDeviceContext().GetData(pVizQuery, (void*) &m_nVisSamples, sizeof(uint64), D3D11_ASYNC_GETDATA_DONOTFLUSH);

		if (hRes == S_OK)
		{
			m_nDrawFrame = 0;
			m_nCheckFrame = nFrame;
		}
	}

	m_nReadResultTryCounter++;

#ifdef DO_RENDERLOG
	if (!m_nVisSamples)
	{
		if (CRenderer::CV_r_log)
			gRenDev->Logv("OcclusionQuery: Water is not visible\n");
	}
	else
	{
		if (CRenderer::CV_r_log)
			gRenDev->Logv("OcclusionQuery: Water is visible (%d samples)\n", m_nVisSamples);
	}
#endif

	m_bSucceeded = (m_nCheckFrame == nFrame);
	return m_bSucceeded;
}

//===================================================================================

static float sInterpolate(float& pprev, float& prev, float& next, float& nnext, float ppweight, float pweight, float nweight, float nnweight)
{
	return pprev * ppweight + prev * pweight + next * nweight + nnext * nnweight;
}

static float sSpline(float x)
{
	float fX = fabsf(x);

	if (fX > 2.0f)
		return 0;
	if (fX > 1.0f)
		return (2.0f - fX) * (2.0f - fX) * (2.0f - fX) / 6.0f;
	return 2.0f / 3.0f - fX * fX + 0.5f * fX * fX * fX;
}

void CRenderMesh::DrawImmediately()
{
	CD3D9Renderer* rd = gcpRendD3D;

	HRESULT hr = rd->FX_SetVertexDeclaration(0, _GetVertexFormat());

	if (FAILED(hr))
	{
		assert(!"CRenderMesh::DrawImmediately failed");
		return;
	}

	// set vertex and index buffer
	CheckUpdate(_GetVertexFormat(), 0);

	size_t vbOffset(0);
	size_t ibOffset(0);
	D3DVertexBuffer* pVB = rd->m_DevBufMan.GetD3DVB(_GetVBStream(VSF_GENERAL), &vbOffset);
	D3DIndexBuffer* pIB = rd->m_DevBufMan.GetD3DIB(_GetIBStream(), &ibOffset);
	assert(pVB);
	assert(pIB);

	if (!pVB || !pIB)
	{
		assert(!"CRenderMesh::DrawImmediately failed");
		return;
	}

	rd->FX_SetVStream(0, pVB, vbOffset, GetStreamStride(VSF_GENERAL));
	rd->FX_SetIStream(pIB, ibOffset, (sizeof(vtx_idx) == 2 ? Index16 : Index32));

	// draw indexed mesh
	rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, _GetNumVerts(), 0, _GetNumInds());
}

bool CREHDRProcess::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CD3D9Renderer* rd = gcpRendD3D;
	assert(rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_HDR || rd->m_RP.m_CurState & GS_WIREFRAME);
	if (!(rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_HDR))
		return false;

	rd->FX_HDRPostProcessing();
	return true;
}

bool CREBeam::mfDraw(CShader* ef, SShaderPass* sl)
{
	CD3D9Renderer* rd = gcpRendD3D;
	int nThreadID = rd->m_RP.m_nProcessThreadID;

	if (IsRecursiveRenderView())
		return false;

	PROFILE_LABEL_SCOPE("LIGHT BEAM");

	EShaderQuality nShaderQuality = (EShaderQuality) gcpRendD3D->EF_GetShaderQuality(eST_FX);
	ERenderQuality nRenderQuality = gRenDev->m_RP.m_eQuality;
	bool bLowSpecShafts = (nShaderQuality == eSQ_Low) || (nRenderQuality == eRQ_Low);

	STexState pState(FILTER_BILINEAR, true);
	const int texStateID(CTexture::GetTexState(pState));

	STexState pStatePoint(FILTER_POINT, true);
	const int texStateIDPoint(CTexture::GetTexState(pStatePoint));

	bool bViewerInsideCone = false;

	CTexture* pLowResRT = CTexture::s_ptexZTargetScaled2; // CTexture::s_ptexBackBufferScaled[1];
	CTexture* pLowResRTDepth = CTexture::s_ptexDepthBufferQuarter;

	SDepthTexture* pCurrDepthSurf = NULL;

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID // Depth surface nastiness
	if (CTexture::IsTextureExist(pLowResRTDepth))
	{
		SDepthTexture D3dDepthSurface;
		D3dDepthSurface.nWidth = pLowResRTDepth->GetWidth();
		D3dDepthSurface.nHeight = pLowResRTDepth->GetHeight();
		D3dDepthSurface.nFrameAccess = -1;
		D3dDepthSurface.bBusy = false;

		D3dDepthSurface.pTexture = pLowResRTDepth;
		D3dDepthSurface.pSurface = pLowResRTDepth->GetDeviceDepthStencilView();
		D3dDepthSurface.pTarget = pLowResRTDepth->GetDevTexture()->Get2DTexture();

		pCurrDepthSurf = &D3dDepthSurface;
	}
#endif

	CRenderObject* pObj = rd->m_RP.m_pCurObject;
	SRenderObjData* pOD = pObj->GetObjData();
	uint16 nLightID = pOD->m_nLightID;
	SRenderLight* pDL = &rd->m_RP.m_pCurrentRenderView->GetLight(eDLT_DeferredLight, pOD->m_nLightID);

	bool bCastsShadows = ((pDL->m_Flags & (DLF_CASTSHADOW_MAPS | DLF_PROJECT)) == (DLF_CASTSHADOW_MAPS | DLF_PROJECT)) ? true : false;

	const CRenderObject::SInstanceInfo& rInstInfo = pObj->m_II;

	Matrix34A objMatInv = rInstInfo.m_Matrix.GetInverted();

	Matrix44A mLightProj, mLightView;
	CShadowUtils::GetCubemapFrustumForLight(pDL, 0, pDL->m_fLightFrustumAngle * 2.f, &mLightProj, &mLightView, true);

	Matrix44 projMat = mLightView * mLightProj;

	const CRenderCamera& RCam = gRenDev->GetRCamera();

	float fLightAngle = pDL->m_fLightFrustumAngle;
	float fAngleCoeff = 1.0f / tan_tpl((90.0f - fLightAngle) * gf_PI / 180.0f);
	float fNear = pDL->m_fProjectorNearPlane;
	float fFar = pDL->m_fRadius;
	float fScaleNear = fNear * fAngleCoeff;
	float fScaleFar = fFar * fAngleCoeff;
	Vec3 vLightPos = pDL->m_Origin;
	Vec3 vAxis = rInstInfo.m_Matrix.GetColumn0();

	float fSin, fCos;
	sincos_tpl(fLightAngle * gf_PI / 180.0f, &fSin, &fCos);

	Vec4 vLightParams = Vec4(fFar, fAngleCoeff, fNear, fFar);
	Vec4 vSphereParams = Vec4(vLightPos, fFar);
	Vec4 vConeParams = Vec4(vAxis, fCos);
	Vec4 pLightPos = Vec4(vLightPos, 1.0f);
	Vec4 cLightDiffuse = Vec4(pDL->m_Color.r, pDL->m_Color.g, pDL->m_Color.b, pDL->m_SpecMult);

	Vec3 vEye = RCam.vOrigin;

	Vec3 vCoords[9];  // Evaluate campos to near plane verts as a sphere.
	RCam.CalcVerts(vCoords);
	vCoords[4] = vEye;
	AABB camExtents(vCoords, 5);

	float fRadius = camExtents.GetRadius();
	Vec3 vCentre = camExtents.GetCenter();

	float fCosSq = fCos * fCos;

	Vec3 vVertToSphere = vCentre - vLightPos;
	Vec3 d = vVertToSphere + vAxis * (fRadius / fSin);
	float dSq = d.dot(d);
	float e = d.dot(vAxis);
	float eSq = e * e;

	if ((e > 0.0f) && (eSq >= (dSq * fCosSq)))
	{
		float fSinSq = fSin * fSin;
		dSq = vVertToSphere.dot(vVertToSphere);
		e = vVertToSphere.dot(vAxis);

		if ((e < (fFar + fRadius)) && (e > (fNear - fRadius))) // test capping planes
		{
			bViewerInsideCone = true;
		}
	}

	Vec4 cEyePosVec(vEye, !bViewerInsideCone ? 1 : 0);

	Vec4 vShadowCoords = Vec4(0.0f, 0.0f, 1.0f, 1.0f);

	CTexture* pShadowTex = NULL;
	CTexture* pProjTex = NULL;

	if (bCastsShadows)
	{
		const ShadowMapFrustum& shadowFrustum = CShadowUtils::GetFirstFrustum(rd->m_RP.m_pCurrentRenderView, nLightID);
		if (shadowFrustum.bUseShadowsPool)
		{
			pShadowTex = CTexture::s_ptexRT_ShadowPool;
			float fTexWidth = CTexture::s_ptexRT_ShadowPool->GetWidth();
			float fTexHeight = CTexture::s_ptexRT_ShadowPool->GetHeight();
			vShadowCoords = Vec4(shadowFrustum.packX[0] / fTexWidth, shadowFrustum.packY[0] / fTexHeight, shadowFrustum.packWidth[0] / fTexWidth, shadowFrustum.packHeight[0] / fTexHeight);
		}
	}

	if (pDL->m_pLightImage)
	{
		pProjTex = (CTexture*)pDL->m_pLightImage;
	}

	Vec4 sampleOffsets[5];
	{
		const float tU = 1.0f / (float) pLowResRT->GetWidth();
		const float tV = 1.0f / (float) pLowResRT->GetHeight();

		sampleOffsets[0] = Vec4(0, 0, 0, 0);
		sampleOffsets[1] = Vec4(0, -tV, tU, tV);
		sampleOffsets[2] = Vec4(-tU, 0, -tU, tV);
		sampleOffsets[3] = Vec4(tU, 0, tU, -tV);
		sampleOffsets[4] = Vec4(0, tV, -tU, -tV);
	}

	Vec4 vMisc = Vec4(1.0f / (float)gcpRendD3D->m_nShadowPoolWidth, 1.0f / (float)gcpRendD3D->m_nShadowPoolHeight, 0.0f, 0.0f);

	const int ZPass = 2; // passes can be buggy, use manual ordering
	const int VolumetricPass = 1;
	const int FinalPass = 0;

	rd->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3]);

	if (bCastsShadows)
	{
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	}

	//Setup geometry
	const int nNumSides = BEAM_RE_CONE_SIDES;
	const uint32 c_numBBVertices(nNumSides * 2 + 2);
	SVF_P3F_C4B_T2F bbVertices[c_numBBVertices];

	const uint32 c_numBBIndices((nNumSides) * 6 * 2);
	uint16 bbIndices[c_numBBIndices];

	SetupGeometry(&bbVertices[0], &bbIndices[0], fAngleCoeff, fNear, fFar);

	// copy vertices into dynamic VB
	TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(bbVertices, c_numBBVertices, 0);

	// copy indices into dynamic IB
	TempDynIB16::CreateFillAndBind(bbIndices, c_numBBIndices);

	uint32 nPasses = 0;
	ef->FXBegin(&nPasses, FEF_DONTSETSTATES);

	assert(nPasses == (ZPass + 1));

	int nStartPass = (bViewerInsideCone || !pCurrDepthSurf) ? VolumetricPass : ZPass;

	for (int nCurPass = nStartPass; nCurPass > -1; nCurPass--)
	{
		ef->FXBeginPass(nCurPass);

		//set world basis
		float maskRTWidthL = pLowResRT->GetWidth();
		float maskRTHeightL = pLowResRT->GetHeight();
		float maskRTWidthH = rd->GetWidth();
		float maskRTHeightH = rd->GetHeight();
		Vec4 vScreenScale(1.0f / maskRTWidthL, 1.0f / maskRTHeightL, 1.0f / maskRTWidthH, 1.0f / maskRTHeightH);

		if (nCurPass == nStartPass && pLowResRT)
		{
			rd->FX_ClearTarget(pLowResRT, Clr_Transparent);
			rd->FX_ClearTarget(pCurrDepthSurf, CLEAR_ZBUFFER);
			rd->FX_PushRenderTarget(0, pLowResRT, pCurrDepthSurf, -1, false, 1);
		}

		uint32 nState = (nCurPass == FinalPass) ? (GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA) : 0;

		if (bViewerInsideCone)
		{
			rd->SetCullMode(R_CULL_FRONT);
		}
		else
		{
			rd->SetCullMode(R_CULL_BACK);
		}

		if (bViewerInsideCone || !pCurrDepthSurf)
		{
			nState |= GS_NODEPTHTEST;
		}
		else
		{
			nState |= (nCurPass == ZPass) ? (GS_DEPTHWRITE | GS_COLMASK_NONE) : 0;
		}

		rd->FX_SetState(nState);

		// set vs constants
		if (nCurPass == VolumetricPass)
		{
			ef->FXSetVSFloat(m_eyePosInWSName, &cEyePosVec, 1);

			ef->FXSetPSFloat(m_eyePosInWSName, &cEyePosVec, 1);
			ef->FXSetPSFloat(m_projMatrixName, (const Vec4*) &projMat.m00, 4);
			ef->FXSetPSFloat(m_shadowCoordsName, (const Vec4*) &vShadowCoords, 1);
			ef->FXSetPSFloat(m_lightParamsName, &vLightParams, 1);
			ef->FXSetPSFloat(m_sphereParamsName, &vSphereParams, 1);
			ef->FXSetPSFloat(m_coneParamsName, &vConeParams, 1);
			ef->FXSetPSFloat(m_lightPosName, &pLightPos, 1);
			ef->FXSetPSFloat(m_miscOffsetsName, &vMisc, 1);
		}
		else if (nCurPass == FinalPass)
		{
			ef->FXSetPSFloat(m_sampleOffsetsName, &sampleOffsets[0], 5);
		}

		ef->FXSetPSFloat(m_lightDiffuseName, &cLightDiffuse, 1);
		ef->FXSetPSFloat(m_screenScaleName, &vScreenScale, 1);

		if (nCurPass == FinalPass && pLowResRT)
			pLowResRT->Apply(7, texStateID);
		if (pProjTex)
			pProjTex->Apply(5, texStateID);
		if (bCastsShadows && pShadowTex)
			pShadowTex->Apply(6, texStateID); // bilinear is a hack, but looks better
		//pShadowTex->Apply(6, texStateIDPoint);

		rd->m_RP.m_nCommitFlags |= FC_MATERIAL_PARAMS;

		// commit all render changes
		rd->FX_Commit();

		// set vertex declaration and streams of skydome
		if (!FAILED(rd->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
		{
			// draw skydome
			rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, c_numBBVertices, 0, c_numBBIndices);
		}

		if ((nCurPass == VolumetricPass) && pLowResRT)
		{
			rd->FX_PopRenderTarget(0);
		}

	}

	return true;
}

bool CREParticle::mfDraw(CShader* ef, SShaderPass* shaderPass)
{
	CD3D9Renderer* rd(gcpRendD3D);
	SRenderPipeline& rp(rd->m_RP);
	CRenderObject* pRenderObject = rp.m_pCurObject;
	const bool isPulledVertices = (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_USE_VERTEX_PULL_MODEL) != 0;
	const bool hasIndices = (rp.m_RendNumIndices != 0);
	const bool isPointSprites = (rp.m_ObjFlags & FOB_POINT_SPRITE) != 0;
	const bool isOctogonal = (rp.m_ObjFlags & FOB_OCTAGONAL) != 0;

	rd->FX_Commit();
	rp.m_lightVolumeBuffer.BindSRVs();

	if (!isPulledVertices)
	{
		if (!CHWShader_D3D::s_pCurInstVS && !CHWShader_D3D::s_pCurInstPS || CHWShader_D3D::s_pCurInstPS->m_bFallback || CHWShader_D3D::s_pCurInstVS->m_bFallback)
			return false;

		rp.m_particleBuffer.BindVB();
		rp.m_particleBuffer.BindIB();

		// rendered to volume texture
		if (rd->m_RP.m_nPassGroupID == EFSLIST_FOG_VOLUME)
		{
			if (rd->m_bVolumetricFogEnabled
			    && (rp.m_pCurTechnique && !(rp.m_pCurTechnique->m_Flags & FHF_USE_HULL_SHADER) && (rp.m_pCurTechnique->m_Flags & FHF_USE_GEOMETRY_SHADER)))
			{
				// TODO: calculate depth bounds of all patticles.
				// don't render when all particles are out of fog density texture.

				if (rp.m_RendNumIndices == 0)
				{
					// Draw a quad or octagon, with instanced sprite data
					int nInstanceVerts = (rp.m_ObjFlags & FOB_OCTAGONAL) ? 8 : 4;
					rd->FX_DrawPrimitive(eptTriangleStrip, rp.m_FirstVertex, rp.m_RendNumVerts, nInstanceVerts);
				}
				else
				{
					// Draw non-instanced tri list
					rd->FX_DrawIndexedPrimitive(eptTriangleList, rp.m_FirstVertex, 0, rp.m_RendNumVerts, rp.m_FirstIndex, rp.m_RendNumIndices);
				}

				return true;
			}

			return false;
		}

		if (rp.m_pCurTechnique && (rp.m_pCurTechnique->m_Flags & FHF_USE_HULL_SHADER))
		{
			// Tessellated shader
			if (!CHWShader_D3D::s_pCurInstHS && !CHWShader_D3D::s_pCurInstDS || CHWShader_D3D::s_pCurInstHS->m_bFallback || CHWShader_D3D::s_pCurInstDS->m_bFallback)
				return false;

			// Set vertex-buffer to empty eVF_Unknown
			//	gcpRendD3D->FX_SetVStream(0, nullptr, 0, 3 * sizeof(float));

			if (rp.m_RendNumIndices == 0)
				// Draw separated point sprites
				rd->FX_DrawPrimitive(ept1ControlPointPatchList, rp.m_FirstVertex, rp.m_RendNumVerts);
			else
				// Draw connected quads
				rd->FX_DrawIndexedPrimitive(ept4ControlPointPatchList, rp.m_FirstVertex, 0, rp.m_RendNumVerts, rp.m_FirstIndex, rp.m_RendNumIndices);
		}
		else
		{
			if (!hasIndices && !isPointSprites)
			{
				// ribbons with degenerate tris between strips
				rd->FX_DrawPrimitive(eptTriangleStrip, rp.m_FirstVertex, rp.m_RendNumVerts);
			}
			else if (hasIndices && !isPointSprites)
			{
				// non instanced point sprites. allow index buffer to roll over itself
				// only PS4 using this since it does not support DrawInstanced atm
				const int maxVertices = 1 << 16;
				int numVertices = rp.m_RendNumVerts;
				int firstVertex = rp.m_FirstVertex;
				int firstIndex = rp.m_FirstIndex;
				while (numVertices > 0)
				{
					int thisNumVertices = min(numVertices, maxVertices);
					int thisNumIndices = min(rp.m_RendNumIndices, (thisNumVertices >> 2) * 6);
					rd->FX_DrawIndexedPrimitive(
					  eptTriangleList, firstVertex, 0,
					  thisNumVertices, firstIndex,
					  thisNumIndices);
					numVertices -= thisNumVertices;
					firstVertex += thisNumVertices;
					firstIndex += thisNumIndices;
				}
			}
			else if (!hasIndices && isPointSprites)
			{
				// instanced geometry
				const int nInstanceVerts = isOctogonal ? 8 : 4;
				rd->FX_DrawPrimitive(eptTriangleStrip, rp.m_FirstVertex, rp.m_RendNumVerts, nInstanceVerts);
			}
		}
	}
	else
	{
		// Set vertex-buffer to empty eVF_Unknown
		gcpRendD3D->FX_SetVStream(0, nullptr, 0, 3 * sizeof(float));

		const uint maxNumSprites = rp.m_particleBuffer.GetMaxNumSprites();
		rp.m_particleBuffer.BindSRVs();
		if (isPointSprites)
		{
			rp.m_particleBuffer.BindSpriteIB();

			for (;; )
			{
				const uint numSprites = min(uint(rp.m_RendNumVerts), maxNumSprites);
				rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, 0, 0, numSprites * 6);
				rp.m_RendNumVerts -= numSprites;
				rp.m_FirstVertex += numSprites;
				if (rp.m_RendNumVerts <= 0)
					break;
				CHWShader_D3D* pVertexShader = (CHWShader_D3D*)shaderPass->m_VShader;
				pVertexShader->mfSetParametersPB();
				rd->FX_Commit();
			}
		}
		else
		{
			rd->FX_DrawPrimitive(eptTriangleStrip, 0, rp.m_RendNumVerts * 2);
		}
	}

	return true;
}

bool CREGameEffect::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CRY_ASSERT_MESSAGE(gRenDev->m_pRT->IsRenderThread(), "Trying to render from wrong thread");
	CRY_ASSERT(ef);
	CRY_ASSERT(sfm);

	if (m_pImpl)
	{
#ifndef _RELEASE
		IMaterial* pMaterial = (gRenDev->m_RP.m_pCurObject) ? (gRenDev->m_RP.m_pCurObject->m_pCurrMaterial) : NULL;
		const char* pEffectName = (pMaterial) ? (PathUtil::GetFileName(pMaterial->GetName())) : "GameEffectRenderElement";
		PROFILE_LABEL_SCOPE(pEffectName);
#endif

		uint32 passCount = 0;
		bool successFlag = true;

		// Begin drawing
		ef->FXBegin(&passCount, 0);
		if (passCount > 0)
		{
			// Begin pass
			ef->FXBeginPass(0);

			// Draw element
			successFlag = m_pImpl->mfDraw(ef, sfm, gRenDev->m_RP.m_pCurObject);

			// End pass
			ef->FXEndPass();
		}
		// End drawing
		ef->FXEnd();

		return successFlag;
	}
	return false;
}

//====================================================================================================================================================

inline static void getObjMatrix(UFloat4* sData, const register float* pData, const bool bRelativeToCamPos, const Vec3& vRelativeToCamPos)
{
#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)
	sData[0].m128 = _mm_load_ps(&pData[0]);
	sData[1].m128 = _mm_load_ps(&pData[4]);
	sData[2].m128 = _mm_load_ps(&pData[8]);
#else
	sData[0].f[0] = pData[0];
	sData[0].f[1] = pData[1];
	sData[0].f[2] = pData[2];
	sData[0].f[3] = pData[3];
	sData[1].f[0] = pData[4];
	sData[1].f[1] = pData[5];
	sData[1].f[2] = pData[6];
	sData[1].f[3] = pData[7];
	sData[2].f[0] = pData[8];
	sData[2].f[1] = pData[9];
	sData[2].f[2] = pData[10];
	sData[2].f[3] = pData[11];
#endif

	if (bRelativeToCamPos)
	{
		sData[0].f[3] -= vRelativeToCamPos.x;
		sData[1].f[3] -= vRelativeToCamPos.y;
		sData[2].f[3] -= vRelativeToCamPos.z;
	}
}

// Each call of CREGeomCache::mfDraw render *all* meshes that share the same material in the geom cache. See CGeomCacheRenderNode::Render
bool CREGeomCache::mfDraw(CShader* ef, SShaderPass* sfm)
{
	PROFILE_FRAME(CREGeomCache::mfDraw);

	const uint numMeshes = m_meshRenderData.size();
	CD3D9Renderer* const pRenderer = gcpRendD3D;

	SRenderPipeline& rRP = pRenderer->m_RP;
	CRenderObject* const pRenderObject = rRP.m_pCurObject;
	Matrix34A matrix = pRenderObject->m_II.m_Matrix;
	CHWShader_D3D* const pCurVS = (CHWShader_D3D*)sfm->m_VShader;

	const bool bIsShadowPass = (rRP.m_TI[rRP.m_nProcessThreadID].m_PersFlags & RBPF_SHADOWGEN) != 0;
	const CCamera& camera = bIsShadowPass ? rRP.m_ShadowInfo.m_pCurShadowFrustum->FrustumPlanes[rRP.m_ShadowInfo.m_nOmniLightSide] : gRenDev->GetCamera();

	Matrix44A prevMatrix;
	CMotionBlur::GetPrevObjToWorldMat(rRP.m_pCurObject, prevMatrix);

	const uint64 oldFlagsShader_RT = rRP.m_FlagsShader_RT;
	uint64 flagsShader_RT = rRP.m_FlagsShader_RT;
	const int oldFlagsPerFlush = rRP.m_FlagsPerFlush;
	bool bResetVertexDecl = false;

	const bool bRelativeToCam = (!(rRP.m_TI[rRP.m_nProcessThreadID].m_PersFlags & RBPF_SHADOWGEN) && !(rRP.m_pCurObject->m_ObjFlags & FOB_NEAREST));
	const Vec3& vRelativeToCamPos = bRelativeToCam ? gRenDev->GetRCamera().vOrigin : Vec3(ZERO);

#ifdef SUPPORTS_STATIC_INST_CB
	if (pRenderer->CV_r_StaticInstCB)
	{
		flagsShader_RT &= ~g_HWSR_MaskBit[HWSR_STATIC_INST_DATA];
	}
#endif

	for (uint nMesh = 0; nMesh < numMeshes; ++nMesh)
	{
		const SMeshRenderData& meshData = m_meshRenderData[nMesh];

		CRenderMesh* const pRenderMesh = static_cast<CRenderMesh*>(meshData.m_pRenderMesh.get());
		const uint numInstances = meshData.m_instances.size();

		if (pRenderMesh && numInstances > 0)
		{
			PROFILE_LABEL(pRenderMesh->GetSourceName() ? pRenderMesh->GetSourceName() : "Unknown mesh-resource name");

			const CRenderMesh* const pVertexContainer = pRenderMesh->_GetVertexContainer();

			if (!pVertexContainer->_HasVBStream(VSF_GENERAL) || !pRenderMesh->_HasIBStream())
			{
				// Should never happen. Video buffer is missing
				continue;
			}

			const bool bHasVelocityStream = pRenderMesh->_HasVBStream(VSF_VERTEX_VELOCITY);
			const bool bIsMotionBlurPass = (rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS) != 0;

			pRenderMesh->BindStreamsToRenderPipeline();

			rRP.m_RendNumVerts = pRenderMesh->_GetNumVerts();

			if (ef->m_HWTechniques.Num() && pRenderMesh->CanRender())
			{
				const TRenderChunkArray& chunks = pRenderMesh->GetChunks();
				const uint numChunks = chunks.size();

				for (uint i = 0; i < numChunks; ++i)
				{
					const CRenderChunk& chunk = chunks[i];
					if (chunk.m_nMatID != m_materialId)
					{
						continue;
					}

					rRP.m_FirstIndex = chunk.nFirstIndexId;
					rRP.m_RendNumIndices = chunk.nNumIndices;

#if defined(HW_INSTANCING_ENABLED) && !CRY_PLATFORM_ORBIS
					const bool bUseInstancing = (CRenderer::CV_r_geominstancing != 0) && (numInstances > CRenderer::CV_r_GeomCacheInstanceThreshold);
#else
					const bool bUseInstancing = false;
#endif

					TempDynInstVB instVB;
					uint numInstancesToDraw = 0;
					byte* __restrict pInstanceMatricesVB = NULL;

					// Note: Geom cache instancing is a horrible mess at the moment, because it re-uses
					// FX_DrawInstances which supports both constant based and attribute based instancing
					// and all platforms.
					//
					// This only sets up the data structures for D3D11 PC & Durango attribute based
					// instancing. Need to clean this up later and ideally use constant based instancing.

					const uint64 lastFlagsShader_RT = rRP.m_FlagsShader_RT;
					rRP.m_FlagsShader_RT = flagsShader_RT | (bUseInstancing ? g_HWSR_MaskBit[HWSR_INSTANCING_ATTR] : 0);
					if (lastFlagsShader_RT != rRP.m_FlagsShader_RT)
					{
						pCurVS->mfSet(bUseInstancing ? HWSF_INSTANCED : 0);
					}

					CHWShader_D3D::SHWSInstance* pVPInst = pCurVS->m_pCurInst;
					int32 nUsedAttr = 3, nInstAttrMask = 0;
					byte Attributes[32];

					if (bUseInstancing)
					{
						pVPInst->GetInstancingAttribInfo(Attributes, nUsedAttr, nInstAttrMask);
						instVB.Allocate(numInstances, nUsedAttr * INST_PARAM_SIZE);
						pInstanceMatricesVB = (byte*)(instVB.Lock());
					}

					const uint32 nStride = nUsedAttr * sizeof(float[4]);

					// Fill the stream 3 for per-instance data
					byte* pWalkData = pInstanceMatricesVB;
					for (uint nInstance = 0; nInstance < numInstances; ++nInstance)
					{
						const SMeshInstance& instance = meshData.m_instances[nInstance];

						Matrix34A pieceMatrix = matrix * instance.m_matrix;

						AABB pieceWorldAABB;
						pieceWorldAABB.SetTransformedAABB(pieceMatrix, instance.m_aabb);
						if (!camera.IsAABBVisible_F(pieceWorldAABB))
						{
							continue;
						}

						// Needs to be in this scope, because it's used by FX_DrawIndexedMesh
						Matrix44A prevPieceMatrix = prevMatrix * instance.m_prevMatrix;

						if (bIsMotionBlurPass)
						{
							const float fThreshold = 0.01f;
							if (bUseInstancing || (rRP.m_nBatchFilter & FB_Z) || !Matrix34::IsEquivalent(pieceMatrix, Matrix34(prevPieceMatrix), fThreshold) || bHasVelocityStream)
							{
								rRP.m_FlagsPerFlush |= RBSI_CUSTOM_PREVMATRIX;
								rRP.m_pPrevMatrix = &prevPieceMatrix;
							}
							else
							{
								// Don't draw pieces without any motion in motion blur pass
								continue;
							}
						}

						if (!bUseInstancing)
						{
							pRenderObject->m_II.m_Matrix = pieceMatrix;
							pCurVS->mfSetParametersPI(NULL, ef);

							// Check if instancing messed with vertex declaration
							if (bResetVertexDecl)
							{
								pRenderer->FX_SetVertexDeclaration(rRP.m_FlagsStreams_Decl, rRP.m_CurVFormat);
								bResetVertexDecl = false;
							}

							pRenderer->FX_DrawIndexedMesh(pRenderMesh->_GetPrimitiveType());
						}
						else
						{
							UFloat4* __restrict vMatrix = (UFloat4*)pWalkData;
							getObjMatrix(vMatrix, pieceMatrix.GetData(), bRelativeToCam, vRelativeToCamPos);

							float* __restrict fParm = (float*)&pWalkData[3 * sizeof(float[4])];
							pWalkData += nStride;

							if (pVPInst->m_nParams_Inst >= 0)
							{
								SCGParamsGroup& Group = CGParamManager::s_Groups[pVPInst->m_nParams_Inst];
								fParm = pCurVS->mfSetParametersPI(Group.pParams, Group.nParams, fParm, eHWSC_Vertex, 40);
							}

							++numInstancesToDraw;
						}
					}

					if (bUseInstancing)
					{
						instVB.Unlock();
						instVB.Bind(3, nUsedAttr * INST_PARAM_SIZE);
						instVB.Release();

						pCurVS->mfSetParametersPI(NULL, ef);
						pRenderer->FX_DrawInstances(ef, sfm, 0, 0, numInstancesToDraw - 1, nUsedAttr, pInstanceMatricesVB, nInstAttrMask, Attributes, 0);
						bResetVertexDecl = true;
					}
				}
			}
		}
	}

	// Reset matrix to original value for cases when render object gets reused
	pRenderObject->m_II.m_Matrix = matrix;
	rRP.m_FlagsShader_RT = oldFlagsShader_RT;
	rRP.m_FlagsPerFlush = oldFlagsPerFlush;

	return true;
}
