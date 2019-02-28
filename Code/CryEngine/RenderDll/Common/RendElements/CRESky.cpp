// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/RendElement.h>
#include <CryRenderer/RenderElements/CRESky.h>
#include "Stars.h"
#include <Cry3DEngine/I3DEngine.h>

#include "../../XRenderD3D9/DriverD3D.h"
#include "GraphicsPipeline/SceneForward.h"

InputLayoutHandle CRESky::GetVertexFormat() const
{
	return EDefaultInputLayouts::P3F_C4B_T2F;
}

bool CRESky::GetGeometryInfo(SGeometryInfo& streams, bool bSupportTessellation)
{
	streams.eVertFormat = GetVertexFormat();
	streams.primitiveType = eptTriangleList;

	return true;
}

bool CRESky::Compile(CRenderObject* pObj, CRenderView *pRenderView, bool updateInstanceDataOnly)
{
	return true;
}

void CRESky::DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList)
{
	gcpRendD3D->GetGraphicsPipeline().GetSceneForwardStage()->SetSkyRE(this, nullptr);
}

CRESky::~CRESky()
{
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
	GenerateSkyDomeTextures(SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight);
}

InputLayoutHandle CREHDRSky::GetVertexFormat() const
{
	return EDefaultInputLayouts::P3F_C4B_T2F;
}

bool CREHDRSky::GetGeometryInfo(SGeometryInfo& streams, bool bSupportTessellation)
{
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

static void FillSkyTextureData(CTexture* pTexture, const void* pData, const uint32 width, const uint32 height, const uint32 pitch)
{
	CDeviceTexture* pDevTex = pTexture->GetDevTexture();
	assert(pTexture && pTexture->GetWidth() == width && pTexture->GetHeight() == height && pDevTex);
	const SResourceMemoryAlignment layout = SResourceMemoryAlignment::Linear<CryHalf4>(width, height);
	CRY_ASSERT(pitch == layout.rowStride);
	GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pDevTex);
	GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pData, pDevTex, layout);
}

bool CREHDRSky::Compile(CRenderObject* pObj, CRenderView *pRenderView, bool updateInstanceDataOnly)
{
	return true;
}

void CREHDRSky::DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList)
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
	if (!LoadData())
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "CStars: Failed to load data");
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
	const char c_fileName[] = "%ENGINE%/engineassets/sky/stars.dat";

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
	// OLD PIPELINE
	ASSERT_LEGACY_PIPELINE

	/*
	CD3D9Renderer* rd(gcpRendD3D);

	I3DEngine* p3DEngine(gEnv->p3DEngine);
	float starIntensity(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY));

	if (m_pStarMesh && m_pShader && gRenDev->m_RP.m_nPassGroupID == EFSLIST_FORWARD_OPAQUE && (gRenDev->m_RP.m_nBatchFilter & FB_GENERAL) && starIntensity > 1e-3f)
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
	*/
}