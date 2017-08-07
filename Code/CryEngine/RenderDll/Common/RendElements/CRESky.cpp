// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/RendElement.h>
#include <CryRenderer/RenderElements/CRESky.h>
#include "Stars.h"
#include <Cry3DEngine/I3DEngine.h>

#include "../../XRenderD3D9/DriverD3D.h"
#include "GraphicsPipeline/SceneForward.h"

void CRESky::mfPrepare(bool bCheckOverflow)
{
	if (bCheckOverflow)
		gRenDev->FX_CheckOverflow(0, 0, this);

	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;
}

InputLayoutHandle CRESky::GetVertexFormat() const
{
	return EDefaultInputLayouts::P3F_C4B_T2F;
}

bool CRESky::GetGeometryInfo(SGeometryInfo& streams, bool bSupportTessellation)
{
	ZeroStruct(streams);
	streams.eVertFormat = GetVertexFormat();
	streams.primitiveType = eptTriangleList;
	return true;
}

CRESky::~CRESky()
{
}

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

	const float fSkyBoxSize = SKY_BOX_SIZE;

	if (rd->m_RP.m_nBatchFilter & FB_Z)
	{
		CTexture::s_ptexBlack->Apply(0, EDefaultSamplerStates::LinearClamp);
		{
			// top
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 4, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(+fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[1] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[2] = { Vec3(+fSkyBoxSize, +fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[3] = { Vec3(-fSkyBoxSize, +fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0, 0) };

			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 4, eptTriangleStrip);
		}
		{
			// nesw
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 10, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, +fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[1] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[2] = { Vec3(+fSkyBoxSize, -fSkyBoxSize, +fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[3] = { Vec3(+fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[4] = { Vec3(+fSkyBoxSize, +fSkyBoxSize, +fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[5] = { Vec3(+fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[6] = { Vec3(-fSkyBoxSize, +fSkyBoxSize, +fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[7] = { Vec3(-fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[8] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, +fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[9] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };

			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 10, eptTriangleStrip);
		}
		{
			// b
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 4, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(+fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[1] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[2] = { Vec3(+fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };
			data[3] = { Vec3(-fSkyBoxSize, +fSkyBoxSize, -fSkyBoxSize), { { 0 } }, Vec2(0, 0) };

			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 4, eptTriangleStrip);
		}
	}
	else
	{
		{
			// top
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 4, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(fSkyBoxSize,  -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(1, 1.f - 1) };
			data[1] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0, 1.f - 1) };
			data[2] = { Vec3(fSkyBoxSize,  fSkyBoxSize,  fSkyBoxSize), { { 0 } }, Vec2(1, 1.f - 0) };
			data[3] = { Vec3(-fSkyBoxSize, fSkyBoxSize,  fSkyBoxSize), { { 0 } }, Vec2(0, 1.f - 0) };

			((CTexture*)(pSky->m_SkyBox[2]))->Apply(0, EDefaultSamplerStates::LinearClamp);
			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
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
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 6, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(1.0, 1.f - 1.0) };
			data[1] = { Vec3(fSkyBoxSize,  -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0.0, 1.f - 1.0) };
			data[2] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, -P),          { { 0 } }, Vec2(1.0, 1.f - 0.5f - fTexOffset) };
			data[3] = { Vec3(fSkyBoxSize,  -fSkyBoxSize, -P),          { { 0 } }, Vec2(0.0, 1.f - 0.5f - fTexOffset) };
			data[4] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, -D),          { { 0 } }, Vec2(1.0, 1.f - 0.5f - fTexOffset) };
			data[5] = { Vec3(fSkyBoxSize,  -fSkyBoxSize, -D),          { { 0 } }, Vec2(0.0, 1.f - 0.5f - fTexOffset) };

			((CTexture*)(pSky->m_SkyBox[1]))->Apply(0, EDefaultSamplerStates::LinearClamp);
			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}
		{
			// e
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 6, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(-fSkyBoxSize, fSkyBoxSize,  fSkyBoxSize), { { 0 } }, Vec2(1.0, 1.f - 0.0) };
			data[1] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0.0, 1.f - 0.0) };
			data[2] = { Vec3(-fSkyBoxSize, fSkyBoxSize,  -P),          { { 0 } }, Vec2(1.0, 1.f - 0.5f + fTexOffset) };
			data[3] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, -P),          { { 0 } }, Vec2(0.0, 1.f - 0.5f + fTexOffset) };
			data[4] = { Vec3(-fSkyBoxSize, fSkyBoxSize,  -D),          { { 0 } }, Vec2(1.0, 1.f - 0.5f + fTexOffset) };
			data[5] = { Vec3(-fSkyBoxSize, -fSkyBoxSize, -D),          { { 0 } }, Vec2(0.0, 1.f - 0.5f + fTexOffset) };

			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}

		fTexOffset = 1.0f / max(pSky->m_SkyBox[0]->GetHeight(), 1);
		{
			// n
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 6, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(fSkyBoxSize,  fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(1.0, 1.f - 1.0) };
			data[1] = { Vec3(-fSkyBoxSize, fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(0.0, 1.f - 1.0) };
			data[2] = { Vec3(fSkyBoxSize,  fSkyBoxSize, -P),          { { 0 } }, Vec2(1.0, 1.f - 0.5f - fTexOffset) };
			data[3] = { Vec3(-fSkyBoxSize, fSkyBoxSize, -P),          { { 0 } }, Vec2(0.0, 1.f - 0.5f - fTexOffset) };
			data[4] = { Vec3(fSkyBoxSize,  fSkyBoxSize, -D),          { { 0 } }, Vec2(1.0, 1.f - 0.5f - fTexOffset) };
			data[5] = { Vec3(-fSkyBoxSize, fSkyBoxSize, -D),          { { 0 } }, Vec2(0.0, 1.f - 0.5f - fTexOffset) };

			((CTexture*)(pSky->m_SkyBox[0]))->Apply(0, EDefaultSamplerStates::LinearClamp);
			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}
		{
			// w
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 6, data, CDeviceBufferManager::AlignBufferSizeForStreaming);

			data[0] = { Vec3(fSkyBoxSize, -fSkyBoxSize, fSkyBoxSize), { { 0 } }, Vec2(1.0, 1.f - 0.0) };
			data[1] = { Vec3(fSkyBoxSize, fSkyBoxSize,  fSkyBoxSize), { { 0 } }, Vec2(0.0, 1.f - 0.0) };
			data[2] = { Vec3(fSkyBoxSize, -fSkyBoxSize, -P),          { { 0 } }, Vec2(1.0, 1.f - 0.5f + fTexOffset) };
			data[3] = { Vec3(fSkyBoxSize, fSkyBoxSize,  -P),          { { 0 } }, Vec2(0.0, 1.f - 0.5f + fTexOffset) };
			data[4] = { Vec3(fSkyBoxSize, -fSkyBoxSize, -D),          { { 0 } }, Vec2(1.0, 1.f - 0.5f + fTexOffset) };
			data[5] = { Vec3(fSkyBoxSize, fSkyBoxSize,  -D),          { { 0 } }, Vec2(0.0, 1.f - 0.5f + fTexOffset) };

			CVertexBuffer vertexBuffer(data, EDefaultInputLayouts::P3F_C4B_T2F);
			rd->DrawPrimitivesInternal(&vertexBuffer, 6, eptTriangleStrip);
		}
	}

	rd->FX_PopVP();
	rd->FX_ResetPipe();

	return true;
}

bool CRESky::Compile(CRenderObject* pObj)
{
	return true;
}

void CRESky::DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx)
{
	gcpRendD3D->GetGraphicsPipeline().GetSceneForwardStage()->SetSkyRE(this, nullptr);
}


//////////////////////////////////////////////////////////////////////////
// HDR Sky
//////////////////////////////////////////////////////////////////////////

void CREHDRSky::GenerateSkyDomeTextures(int32 width, int32 height)
{
	SAFE_RELEASE(m_pSkyDomeTextureMie);
	SAFE_RELEASE(m_pSkyDomeTextureRayleigh);

	const uint32 creationFlags = FT_STATE_CLAMP | FT_NOMIPS | FT_DONT_STREAM;

	m_pSkyDomeTextureMie = CTexture::GetOrCreateTextureObject("$SkyDomeTextureMie", width, height, 1, eTT_2D, creationFlags, eTF_R16G16B16A16F);
	m_pSkyDomeTextureRayleigh = CTexture::GetOrCreateTextureObject("$SkyDomeTextureRayleigh", width, height, 1, eTT_2D, creationFlags, eTF_R16G16B16A16F);

	m_pSkyDomeTextureMie->Create2DTexture(width, height, 1, creationFlags, nullptr, eTF_R16G16B16A16F);
	m_pSkyDomeTextureRayleigh->Create2DTexture(width, height, 1, creationFlags, nullptr, eTF_R16G16B16A16F);
}

void CREHDRSky::Init()
{
	if (!m_pStars)
		m_pStars = new CStars;

	//No longer defer texture creation, MT resource creation now supported
	//gRenDev->m_pRT->RC_GenerateSkyDomeTextures(this, SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight);
	GenerateSkyDomeTextures(SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight);
}

void CREHDRSky::mfPrepare(bool bCheckOverflow)
{
	if (bCheckOverflow)
		gRenDev->FX_CheckOverflow(0, 0, this);

	//gRenDev->FX_CheckOverflow( 0, 0, this );
	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;
}

InputLayoutHandle CREHDRSky::GetVertexFormat() const
{
	return EDefaultInputLayouts::P3F_C4B_T2F;
}

bool CREHDRSky::GetGeometryInfo(SGeometryInfo& streams, bool bSupportTessellation)
{
	ZeroStruct(streams);
	streams.eVertFormat = GetVertexFormat();
	streams.primitiveType = eptTriangleList;
	return true;
}

CREHDRSky::~CREHDRSky()
{
	SAFE_DELETE(m_pStars);

	SAFE_RELEASE(m_pSkyDomeTextureMie);
	SAFE_RELEASE(m_pSkyDomeTextureRayleigh);
}

void CREHDRSky::SetCommonMoonParams(CShader* ef, bool bUseMoon)
{

	I3DEngine* p3DEngine(gEnv->p3DEngine);

	Vec3 mr;
	p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, mr);
	float moonLati = -gf_PI + gf_PI * mr.x / 180.0f;
	float moonLong = 0.5f * gf_PI - gf_PI * mr.y / 180.0f;

	float sinLonR = sinf(-0.5f * gf_PI);
	float cosLonR = cosf(-0.5f * gf_PI);
	float sinLatR = sinf(moonLati + 0.5f * gf_PI);
	float cosLatR = cosf(moonLati + 0.5f * gf_PI);
	Vec3 moonTexGenRight(sinLonR * cosLatR, sinLonR * sinLatR, cosLonR);

	Vec4 nsMoonTexGenRight(moonTexGenRight, 0);
	static CCryNameR ParamNameTGR("SkyDome_NightMoonTexGenRight");
	ef->FXSetVSFloat(ParamNameTGR, &nsMoonTexGenRight, 1);

	float sinLonU = sinf(moonLong + 0.5f * gf_PI);
	float cosLonU = cosf(moonLong + 0.5f * gf_PI);
	float sinLatU = sinf(moonLati);
	float cosLatU = cosf(moonLati);
	Vec3 moonTexGenUp(sinLonU * cosLatU, sinLonU * sinLatU, cosLonU);

	Vec4 nsMoonTexGenUp(moonTexGenUp, 0);
	static CCryNameR ParamNameTGU("SkyDome_NightMoonTexGenUp");
	ef->FXSetVSFloat(ParamNameTGU, &nsMoonTexGenUp, 1);

	Vec3 nightMoonDirection;
	p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, nightMoonDirection);
	float nightMoonSize(25.0f - 24.0f * clamp_tpl(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_SIZE), 0.0f, 1.0f));
	Vec4 nsMoonDirSize(nightMoonDirection, bUseMoon ? nightMoonSize : 9999.0f);
	static CCryNameR ParamNameDirSize("SkyDome_NightMoonDirSize");
	ef->FXSetVSFloat(ParamNameDirSize, &nsMoonDirSize, 1);
	ef->FXSetPSFloat(ParamNameDirSize, &nsMoonDirSize, 1);
}

static void FillSkyTextureData(CTexture* pTexture, const void* pData, const uint32 width, const uint32 height, const uint32 pitch)
{
	CDeviceTexture* pDevTex = pTexture->GetDevTexture();
	assert(pTexture && pTexture->GetWidth() == width && pTexture->GetHeight() == height && pDevTex);
	const SResourceMemoryAlignment layout = SResourceMemoryAlignment::Linear<CryHalf4>(width, height);
	CRY_ASSERT(pitch == layout.rowStride);

	GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pDevTex);
	GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pData, pDevTex, layout);
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
		Vec4 skyDomeTexSizeVec((float)SSkyLightRenderParams::skyDomeTextureWidth, (float)SSkyLightRenderParams::skyDomeTextureHeight, 0.0f, 0.0f);
		static CCryNameR Param1Name("SkyDome_TextureSize");
		ef->FXSetPSFloat(Param1Name, &skyDomeTexSizeVec, 1);
		Vec4 skyDomeTexelSizeVec(1.0f / (float)SSkyLightRenderParams::skyDomeTextureWidth, 1.0f / (float)SSkyLightRenderParams::skyDomeTextureHeight, 0.0f, 0.0f);
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
	hr = rd->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F);
	if (!FAILED(hr))
	{
		// set vertex and index buffer
		pSkyDomeMesh->CheckUpdate(pSkyDomeMesh->_GetVertexFormat(), 0);
		buffer_size_t vbOffset(0);
		buffer_size_t ibOffset(0);
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

bool CREHDRSky::Compile(CRenderObject* pObj)
{
	return true;
}

void CREHDRSky::DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx)
{
	gcpRendD3D->GetGraphicsPipeline().GetSceneForwardStage()->SetSkyRE(nullptr, this);
}


//////////////////////////////////////////////////////////////////////////
// Stars
//////////////////////////////////////////////////////////////////////////

CStars::CStars()
	: m_numStars(0)
	, m_pStarMesh(0)
	, m_pShader(0)
{
	if (LoadData())
	{

		gRenDev->m_cEF.mfRefreshSystemShader("Stars", gRenDev->m_cEF.s_ShaderStars);
		m_pShader = gRenDev->m_cEF.s_ShaderStars;

	}
}

CStars::~CStars()
{
	m_pStarMesh = NULL;
}

bool CStars::LoadData()
{
	const uint32 c_fileTag(0x52415453);       // "STAR"
	const uint32 c_fileVersion(0x00010001);
	const char c_fileName[] = "engineassets/sky/stars.dat";

	ICryPak* pPak(gEnv->pCryPak);
	if (pPak)
	{
		CInMemoryFileLoader file(pPak);
		if (file.FOpen(c_fileName, "rb"))
		{
			// read and validate header
			size_t itemsRead(0);
			uint32 fileTag(0);
			itemsRead = file.FRead(&fileTag, 1);
			if (itemsRead != 1 || fileTag != c_fileTag)
			{
				file.FClose();
				return false;
			}

			uint32 fileVersion(0);
			itemsRead = file.FRead(&fileVersion, 1);
			if (itemsRead != 1 || fileVersion != c_fileVersion)
			{
				file.FClose();
				return false;
			}

			// read in stars
			file.FRead(&m_numStars, 1);

			SVF_P3S_C4B_T2S* pData(new SVF_P3S_C4B_T2S[6 * m_numStars]);

			for (unsigned int i(0); i < m_numStars; ++i)
			{
				float ra(0);
				file.FRead(&ra, 1);

				float dec(0);
				file.FRead(&dec, 1);

				uint8 r(0);
				file.FRead(&r, 1);

				uint8 g(0);
				file.FRead(&g, 1);

				uint8 b(0);
				file.FRead(&b, 1);

				uint8 mag(0);
				file.FRead(&mag, 1);

				Vec3 v;
				v.x = -cosf(DEG2RAD(dec)) * sinf(DEG2RAD(ra * 15.0f));
				v.y = cosf(DEG2RAD(dec)) * cosf(DEG2RAD(ra * 15.0f));
				v.z = sinf(DEG2RAD(dec));

				for (int k = 0; k < 6; k++)
				{
					pData[6 * i + k].xyz = v;
					pData[6 * i + k].color.dcolor = (mag << 24) + (b << 16) + (g << 8) + r;
				}
			}

			m_pStarMesh = gRenDev->CreateRenderMeshInitialized(pData, 6 * m_numStars, EDefaultInputLayouts::P3S_C4B_T2S, 0, 0, prtTriangleList, "Stars", "Stars");

			delete[] pData;

			// check if we read entire file
			long curPos(file.FTell());
			file.FSeek(0, SEEK_END);
			long endPos(file.FTell());
			if (curPos != endPos)
			{
				file.FClose();
				return false;
			}

			file.FClose();
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
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
		Vec4 paramStarSize(size / (float)vpWidth, size / (float)vpHeight, 0, flickerTime * 0.5f);
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
		if (!FAILED(rd->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3S_C4B_T2S)))
		{
			buffer_size_t offset(0);
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