// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================

   Revision history:
   Created Jan 2013 by Nicolas Schulz

   =============================================================================*/

#include "StdAfx.h"
#include "D3DTiledShading.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3DVolumetricFog.h"

#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#include "Common/RenderView.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32 AtlasArrayDim = 64;
const uint32 SpotTexSize = 512;
const uint32 DiffuseProbeSize = 32;
const uint32 SpecProbeSize = 256;
const uint32 ShadowSampleCount = 16;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following structs and constants have to match the shader code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32 MaxNumTileLights = 255;
const uint32 LightTileSizeX = 8;
const uint32 LightTileSizeY = 8;
const uint32 MaxNumClipVolumes = MAX_DEFERRED_CLIP_VOLUMES;

struct STiledLightCullInfo
{
	uint32 volumeType;
	uint32 PADDING0;
	Vec2   depthBounds;
	Vec4   posRad;
	Vec4   volumeParams0;
	Vec4   volumeParams1;
	Vec4   volumeParams2;
};  // 80 bytes

struct STiledClipVolumeInfo
{
	float data;
};

enum ETiledVolumeTypes
{
	tlVolumeSphere = 1,
	tlVolumeCone   = 2,
	tlVolumeOBB    = 3,
	tlVolumeSun    = 4,
};

enum ETiledLightTypes
{
	tlTypeProbe            = 1,
	tlTypeAmbientPoint     = 2,
	tlTypeAmbientProjector = 3,
	tlTypeAmbientArea      = 4,
	tlTypeRegularPoint     = 5,
	tlTypeRegularProjector = 6,
	tlTypeRegularPointFace = 7,
	tlTypeRegularArea      = 8,
	tlTypeSun              = 9,
};

// Sun area light parameters (same as in standard deferred shading)
const float SunDistance       = 10000.0f;
const float SunSourceDiameter = 94.0f;  // atan(AngDiameterSun) * 2 * SunDistance, where AngDiameterSun=0.54deg

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{
STiledLightCullInfo tileLightsCull[MaxNumTileLights];
STiledLightShadeInfo tileLightsShade[MaxNumTileLights];
}

CTiledShading::CTiledShading()
{
	// 16 byte alignment is important for performance on nVidia cards
	STATIC_ASSERT(sizeof(STiledLightCullInfo) % 16 == 0, "STiledLightCullInfo should be 16 byte aligned for GPU performance");
	STATIC_ASSERT(sizeof(STiledLightShadeInfo) % 16 == 0, "STiledLightShadeInfo should be 16 byte aligned for GPU performance");

	m_dispatchSizeX = 0;
	m_dispatchSizeY = 0;

	m_nTexStateCompare = 0xFFFFFFFF;

	m_numAtlasUpdates = 0;
	m_numSkippedLights = 0;

	m_arrShadowCastingLights.Reserve(16);

	m_bApplyCaustics = false;
}

void CTiledShading::CreateResources()
{
	uint32 dispatchSizeX = gRenDev->GetWidth() / LightTileSizeX + (gRenDev->GetWidth() % LightTileSizeX > 0 ? 1 : 0);
	uint32 dispatchSizeY = gRenDev->GetHeight() / LightTileSizeY + (gRenDev->GetHeight() % LightTileSizeY > 0 ? 1 : 0);

	if (m_dispatchSizeX == dispatchSizeX && m_dispatchSizeY == dispatchSizeY)
	{
		assert(m_lightCullInfoBuf.GetBuffer() && m_specularProbeAtlas.texArray);
		return;
	}
	else
	{
		DestroyResources(false);
	}

	m_dispatchSizeX = dispatchSizeX;
	m_dispatchSizeY = dispatchSizeY;

	if (!m_lightCullInfoBuf.GetBuffer())
	{
		m_lightCullInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightCullInfo), DXGI_FORMAT_UNKNOWN, DX11BUF_DYNAMIC | DX11BUF_STRUCTURED | DX11BUF_BIND_SRV, NULL);
	}

	if (!m_LightShadeInfoBuf.GetBuffer())
	{
		m_LightShadeInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightShadeInfo), DXGI_FORMAT_UNKNOWN, DX11BUF_DYNAMIC | DX11BUF_STRUCTURED | DX11BUF_BIND_SRV, NULL);
	}

	if (!m_tileLightIndexBuf.GetBuffer())
	{
		PREFAST_SUPPRESS_WARNING(6326)
		DXGI_FORMAT format = MaxNumTileLights < 256 ? DXGI_FORMAT_R8_UINT : DXGI_FORMAT_R16_UINT;
		PREFAST_SUPPRESS_WARNING(6326)
		uint32 stride = MaxNumTileLights < 256 ? sizeof(char) : sizeof(short);

		m_tileLightIndexBuf.Create(dispatchSizeX * dispatchSizeY * MaxNumTileLights, stride, format, DX11BUF_BIND_SRV | DX11BUF_BIND_UAV, NULL);
	}

	if (!m_clipVolumeInfoBuf.GetBuffer())
	{
		m_clipVolumeInfoBuf.Create(MaxNumClipVolumes, sizeof(STiledClipVolumeInfo), DXGI_FORMAT_UNKNOWN, DX11BUF_DYNAMIC | DX11BUF_STRUCTURED | DX11BUF_BIND_SRV, NULL);
	}

	if (!m_specularProbeAtlas.texArray)
	{
		m_specularProbeAtlas.texArray = CTexture::CreateTextureArray("$TiledSpecProbeTexArr", eTT_Cube, SpecProbeSize, SpecProbeSize, AtlasArrayDim, IntegerLog2(SpecProbeSize) - 1, 0, eTF_BC6UH);
		m_specularProbeAtlas.items.resize(AtlasArrayDim);

		if (m_specularProbeAtlas.texArray->GetFlags() & FT_FAILED)
			CryFatalError("Couldn't allocate specular probe texture atlas");

#if CRY_PLATFORM_DURANGO
		// Perma-pin, as they need to be pinned each frame anyway, and doing it upfront avoids the cost of doing it each frame.
		//m_specularProbeAtlas.texArray->GetDevTexture()->Pin();
#endif
	}

	if (!m_diffuseProbeAtlas.texArray)
	{
		m_diffuseProbeAtlas.texArray = CTexture::CreateTextureArray("$TiledDiffuseProbeTexArr", eTT_Cube, DiffuseProbeSize, DiffuseProbeSize, AtlasArrayDim, 1, 0, eTF_BC6UH);
		m_diffuseProbeAtlas.items.resize(AtlasArrayDim);

		if (m_diffuseProbeAtlas.texArray->GetFlags() & FT_FAILED)
			CryFatalError("Couldn't allocate diffuse probe texture atlas");

#if CRY_PLATFORM_DURANGO
		// Perma-pin, as they need to be pinned each frame anyway, and doing it upfront avoids the cost of doing it each frame.
		//m_diffuseProbeAtlas.texArray->GetDevTexture()->Pin();
#endif
	}

	if (!m_spotTexAtlas.texArray)
	{
		// Note: BC4 has 4x4 as lowest mipmap
		m_spotTexAtlas.texArray = CTexture::CreateTextureArray("$TiledSpotTexArr", eTT_2D, SpotTexSize, SpotTexSize, AtlasArrayDim, IntegerLog2(SpotTexSize) - 1, 0, eTF_BC4U);
		m_spotTexAtlas.items.resize(AtlasArrayDim);

		if (m_spotTexAtlas.texArray->GetFlags() & FT_FAILED)
			CryFatalError("Couldn't allocate spot texture atlas");

#if CRY_PLATFORM_DURANGO
		// Perma-pin, as they need to be pinned each frame anyway, and doing it upfront avoids the cost of doing it each frame.
		//m_spotTexAtlas.texArray->GetDevTexture()->Pin();
#endif
	}

	STexState ts1(FILTER_LINEAR, true);
	ts1.SetComparisonFilter(true);
	m_nTexStateCompare = CTexture::GetTexState(ts1);
}

void CTiledShading::DestroyResources(bool destroyResolutionIndependentResources)
{
	m_dispatchSizeX = 0;
	m_dispatchSizeY = 0;

	m_lightCullInfoBuf.Release();
	m_LightShadeInfoBuf.Release();
	m_tileLightIndexBuf.Release();
	m_clipVolumeInfoBuf.Release();

	if (destroyResolutionIndependentResources)
	{
		m_specularProbeAtlas.items.clear();
		m_diffuseProbeAtlas.items.clear();
		m_spotTexAtlas.items.clear();

		SAFE_RELEASE_FORCE(m_specularProbeAtlas.texArray);
		SAFE_RELEASE_FORCE(m_diffuseProbeAtlas.texArray);
		SAFE_RELEASE_FORCE(m_spotTexAtlas.texArray);
	}
}

void CTiledShading::Clear()
{
	for (uint32 i = 0, si = m_specularProbeAtlas.items.size(); i < si; ++i)
		m_specularProbeAtlas.items[i] = AtlasItem();
	for (uint32 i = 0, si = m_diffuseProbeAtlas.items.size(); i < si; ++i)
		m_diffuseProbeAtlas.items[i] = AtlasItem();
	for (uint32 i = 0, si = m_spotTexAtlas.items.size(); i < si; ++i)
		m_spotTexAtlas.items[i] = AtlasItem();

	m_numAtlasUpdates = 0;
	m_numSkippedLights = 0;
}

int CTiledShading::InsertTexture(CTexture* texture, TextureAtlas& atlas, int arrayIndex)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const int frameID = rd->GetFrameID(false);

	if (texture == NULL)
		return -1;

	// Make sure the texture is loaded already
	if (texture->GetWidthNonVirtual() == 0 && texture->GetHeightNonVirtual() == 0)
		return -1;

	bool isEditor = gEnv->IsEditor();

	// Check if texture is already in atlas
	for (uint32 i = 0, si = atlas.items.size(); i < si; ++i)
	{
		if (atlas.items[i].texture == texture)
		{
			bool textureUpToDate = !isEditor ? true : (texture->GetUpdateFrameID() == abs(atlas.items[i].updateFrameID) || texture->GetUpdateFrameID() == 0);

			if (textureUpToDate)
			{
				if (atlas.items[i].invalid)
				{
					// Texture was processed before and is invalid
					return -1;
				}
				else
				{
					atlas.items[i].accessFrameID = frameID;
					return (int)i;
				}
			}
		}
	}

	// Find least recently used entry in atlas
	if (arrayIndex < 0)
	{
		uint32 minIndex = 0, minValue = 0xFFFFFFFF;
		for (int i = 0, si = atlas.items.size(); i < si; ++i)
		{
			uint32 accessFrameID = atlas.items[i].accessFrameID;
			if (accessFrameID < minValue)
			{
				minValue = accessFrameID;
				minIndex = i;
			}
		}

		arrayIndex = minIndex;
	}

	atlas.items[arrayIndex].texture = texture;
	atlas.items[arrayIndex].accessFrameID = frameID;
	atlas.items[arrayIndex].updateFrameID = texture->GetUpdateFrameID();
	atlas.items[arrayIndex].invalid = false;

	// Check if texture is valid
	if (!texture->IsLoaded() ||
	    texture->GetWidthNonVirtual() < atlas.texArray->GetWidthNonVirtual() ||
	    texture->GetWidthNonVirtual() != texture->GetHeightNonVirtual() ||
	    texture->GetPixelFormat() != atlas.texArray->GetPixelFormat() ||
	    (texture->IsStreamed() && texture->IsPartiallyLoaded()))
	{
		atlas.items[arrayIndex].invalid = true;

		if (!texture->IsLoaded())
		{
			iLog->LogError("TiledShading: Texture not found: %s", texture->GetName());
		}
		else if (texture->IsStreamed() && texture->IsPartiallyLoaded())
		{
			iLog->LogError("TiledShading: Texture not fully streamed so impossible to add: %s (W:%i H:%i F:%s)",
			               texture->GetName(), texture->GetWidth(), texture->GetHeight(), texture->GetFormatName());
		}
		else if (texture->GetPixelFormat() != atlas.texArray->GetPixelFormat())
		{
			iLog->LogError("TiledShading: Unsupported texture format: %s (W:%i H:%i F:%s), it has to be equal to the tile-atlas (F:%s), please change the texture's preset by re-exporting with CryTif",
			               texture->GetName(), texture->GetWidth(), texture->GetHeight(), texture->GetFormatName(), atlas.texArray->GetFormatName());
		}
		else
		{
			iLog->LogError("TiledShading: Unsupported texture properties: %s (W:%i H:%i F:%s)",
			               texture->GetName(), texture->GetWidth(), texture->GetHeight(), texture->GetFormatName());
		}
		return -1;
	}

	// Update atlas
	uint32 numSrcMips = (uint32)texture->GetNumMipsNonVirtual();
	uint32 numDstMips = (uint32)atlas.texArray->GetNumMipsNonVirtual();
	uint32 firstSrcMip = IntegerLog2((uint32)texture->GetWidthNonVirtual() / atlas.texArray->GetWidthNonVirtual());
	uint32 numFaces = atlas.texArray->GetTexType() == eTT_Cube ? 6 : 1;

	CDeviceTexture* pTexArrayDevTex = atlas.texArray->GetDevTexture();

	//ASSERT_DEVICE_TEXTURE_IS_FIXED(pTexArrayDevTex);

	for (uint32 i = 0; i < numDstMips; ++i)
	{
		for (uint32 j = 0; j < numFaces; ++j)
		{
			rd->GetDeviceContext().CopySubresourceRegion(
			  pTexArrayDevTex->GetBaseTexture(), D3D11CalcSubresource(i, arrayIndex * numFaces + j, numDstMips), 0, 0, 0,
			  texture->GetDevTexture()->GetBaseTexture(), D3D11CalcSubresource(i + firstSrcMip, j, numSrcMips), NULL);
		}
	}

	++m_numAtlasUpdates;

	return arrayIndex;
}

AABB RotateAABB(const AABB& aabb, const Matrix33& mat)
{
	Matrix33 matAbs;
	matAbs.m00 = fabs_tpl(mat.m00);
	matAbs.m01 = fabs_tpl(mat.m01);
	matAbs.m02 = fabs_tpl(mat.m02);
	matAbs.m10 = fabs_tpl(mat.m10);
	matAbs.m11 = fabs_tpl(mat.m11);
	matAbs.m12 = fabs_tpl(mat.m12);
	matAbs.m20 = fabs_tpl(mat.m20);
	matAbs.m21 = fabs_tpl(mat.m21);
	matAbs.m22 = fabs_tpl(mat.m22);

	Vec3 sz = ((aabb.max - aabb.min) * 0.5f) * matAbs;
	Vec3 pos = ((aabb.max + aabb.min) * 0.5f) * mat;

	return AABB(pos - sz, pos + sz);
}

void CTiledShading::PrepareLightList(CRenderView* pRenderView)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	auto& defLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	auto& envProbes = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	auto& ambientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

	const float invCameraFar = 1.0f / rd->GetRCamera().fFar;

	// Prepare view matrix with flipped z-axis
	Matrix44A matView = rd->m_ViewMatrix;
	matView.m02 *= -1;
	matView.m12 *= -1;
	matView.m22 *= -1;
	matView.m32 *= -1;

	int nThreadID = rd->m_RP.m_nProcessThreadID;

	uint32 firstShadowLight = CDeferredShading::Instance().m_nFirstCandidateShadowPoolLight;
	uint32 curShadowPoolLight = CDeferredShading::Instance().m_nCurrentShadowPoolLight;

	uint32 numTileLights = 0;
	uint32 numRenderLights = 0;
	uint32 numValidRenderLights = 0;

	// Reset lights
	ZeroMemory(tileLightsCull, sizeof(STiledLightCullInfo) * MaxNumTileLights);
	ZeroMemory(tileLightsShade, sizeof(STiledLightShadeInfo) * MaxNumTileLights);

	RenderLightsArray* lightLists[3] = {
		CRenderer::CV_r_DeferredShadingEnvProbes ? &envProbes : NULL,
		CRenderer::CV_r_DeferredShadingAmbientLights ? &ambientLights : NULL,
		CRenderer::CV_r_DeferredShadingLights ? &defLights : NULL
	};

	for (uint32 lightListIdx = 0; lightListIdx < 3; ++lightListIdx)
	{
		if (lightLists[lightListIdx] == NULL)
			continue;

		for (uint32 lightIdx = 0, lightListSize = lightLists[lightListIdx]->size(); lightIdx < lightListSize; ++lightIdx)
		{
			SRenderLight& renderLight = (*lightLists[lightListIdx])[lightIdx];
			STiledLightCullInfo& lightCullInfo = tileLightsCull[numTileLights];
			STiledLightShadeInfo& lightShadeInfo = tileLightsShade[numTileLights];

			if (renderLight.m_Flags & (DLF_FAKE | DLF_VOLUMETRIC_FOG_ONLY))
				continue;

			// Skip non-ambient area light if support is disabled
			if ((renderLight.m_Flags & DLF_AREA_LIGHT) && !(renderLight.m_Flags & DLF_AMBIENT) && !CRenderer::CV_r_DeferredShadingAreaLights)
				continue;

			++numRenderLights;

			if (numTileLights == MaxNumTileLights)
				continue;  // Skip light

			// Setup standard parameters
			bool areaLightRect = (renderLight.m_Flags & DLF_AREA_LIGHT) && renderLight.m_fAreaWidth && renderLight.m_fAreaHeight && renderLight.m_fLightFrustumAngle;
			float volumeSize = (lightListIdx == 0) ? renderLight.m_ProbeExtents.len() : renderLight.m_fRadius;
			Vec3 pos = renderLight.GetPosition();
			Vec3 worldViewPos = rd->GetRCamera().vOrigin;
			lightShadeInfo.posRad = Vec4(pos.x - worldViewPos.x, pos.y - worldViewPos.y, pos.z - worldViewPos.z, volumeSize);
			Vec4 posVS = Vec4(pos, 1) * matView;
			lightCullInfo.posRad = Vec4(posVS.x, posVS.y, posVS.z, volumeSize);
			lightShadeInfo.attenuationParams = Vec2(areaLightRect ? (renderLight.m_fAreaWidth + renderLight.m_fAreaHeight) * 0.25f : renderLight.m_fAttenuationBulbSize, renderLight.m_fAreaHeight * 0.5f);
			lightCullInfo.depthBounds = Vec2(posVS.z - volumeSize, posVS.z + volumeSize) * invCameraFar;
			float itensityScale = rd->m_fAdaptedSceneScaleLBuffer;
			lightShadeInfo.color = Vec4(renderLight.m_Color.r * itensityScale, renderLight.m_Color.g * itensityScale,
			                            renderLight.m_Color.b * itensityScale, renderLight.m_SpecMult);
			lightShadeInfo.resIndex = 0;
			lightShadeInfo.shadowParams = Vec2(0, 0);
			lightShadeInfo.stencilID0 = renderLight.m_nStencilRef[0] + 1;
			lightShadeInfo.stencilID1 = renderLight.m_nStencilRef[1] + 1;

			// Environment probes
			if (lightListIdx == 0)
			{
				lightCullInfo.volumeType = tlVolumeOBB;
				lightShadeInfo.lightType = tlTypeProbe;
				lightShadeInfo.resIndex = 0xFFFFFFFF;
				lightShadeInfo.attenuationParams = Vec2(renderLight.m_Color.a, max(renderLight.GetFalloffMax(), 0.001f));

				AABB aabb = RotateAABB(AABB(-renderLight.m_ProbeExtents, renderLight.m_ProbeExtents), Matrix33(renderLight.m_ObjMatrix));
				aabb = RotateAABB(aabb, Matrix33(matView));
				lightCullInfo.depthBounds = Vec2(posVS.z + aabb.min.z, posVS.z + aabb.max.z) * invCameraFar;

				Vec4 u0 = Vec4(renderLight.m_ObjMatrix.GetColumn0().GetNormalized(), 0) * matView;
				Vec4 u1 = Vec4(renderLight.m_ObjMatrix.GetColumn1().GetNormalized(), 0) * matView;
				Vec4 u2 = Vec4(renderLight.m_ObjMatrix.GetColumn2().GetNormalized(), 0) * matView;
				lightCullInfo.volumeParams0 = Vec4(u0.x, u0.y, u0.z, renderLight.m_ProbeExtents.x);
				lightCullInfo.volumeParams1 = Vec4(u1.x, u1.y, u1.z, renderLight.m_ProbeExtents.y);
				lightCullInfo.volumeParams2 = Vec4(u2.x, u2.y, u2.z, renderLight.m_ProbeExtents.z);

				lightShadeInfo.projectorMatrix.SetRow4(0, Vec4(renderLight.m_ObjMatrix.GetColumn0().GetNormalized() / renderLight.m_ProbeExtents.x, 0));
				lightShadeInfo.projectorMatrix.SetRow4(1, Vec4(renderLight.m_ObjMatrix.GetColumn1().GetNormalized() / renderLight.m_ProbeExtents.y, 0));
				lightShadeInfo.projectorMatrix.SetRow4(2, Vec4(renderLight.m_ObjMatrix.GetColumn2().GetNormalized() / renderLight.m_ProbeExtents.z, 0));

				Vec3 boxProxyMin(-1000000, -1000000, -1000000);
				Vec3 boxProxyMax(1000000, 1000000, 1000000);

				if (renderLight.m_Flags & DLF_BOX_PROJECTED_CM)
				{
					boxProxyMin = Vec3(-renderLight.m_fBoxLength * 0.5f, -renderLight.m_fBoxWidth * 0.5f, -renderLight.m_fBoxHeight * 0.5f);
					boxProxyMax = Vec3(renderLight.m_fBoxLength * 0.5f, renderLight.m_fBoxWidth * 0.5f, renderLight.m_fBoxHeight * 0.5f);
				}

				lightShadeInfo.shadowMatrix.SetRow4(0, Vec4(boxProxyMin, 0));
				lightShadeInfo.shadowMatrix.SetRow4(1, Vec4(boxProxyMax, 0));

				int arrayIndex = InsertTexture((CTexture*)renderLight.GetSpecularCubemap(), m_specularProbeAtlas, -1);
				if (arrayIndex >= 0)
				{
					if (InsertTexture((CTexture*)renderLight.GetDiffuseCubemap(), m_diffuseProbeAtlas, arrayIndex) >= 0)
						lightShadeInfo.resIndex = arrayIndex;
					else
						continue;  // Skip light
				}
				else
					continue;  // Skip light
			}
			// Regular and ambient lights
			else
			{
				const float sqrt_2 = 1.414214f;  // Scale for cone so that it's base encloses a pyramid base

				const bool ambientLight = (lightListIdx == 1);

				lightCullInfo.volumeType = tlVolumeSphere;
				lightShadeInfo.lightType = ambientLight ? tlTypeAmbientPoint : tlTypeRegularPoint;

				if (!ambientLight)
				{
					lightShadeInfo.attenuationParams.x = max(lightShadeInfo.attenuationParams.x, 0.001f);

					// Adjust light intensity so that the intended brightness is reached 1 meter from the light's surface
					// Solve I * 1 / (1 + d/lightsize)^2 = 1
					float intensityMul = 1.0f + 1.0f / lightShadeInfo.attenuationParams.x;
					intensityMul *= intensityMul;
					lightShadeInfo.color.x *= intensityMul;
					lightShadeInfo.color.y *= intensityMul;
					lightShadeInfo.color.z *= intensityMul;
				}

				// Handle projectors
				if (renderLight.m_Flags & DLF_PROJECT)
				{
					lightCullInfo.volumeType = tlVolumeCone;
					lightShadeInfo.lightType = ambientLight ? tlTypeAmbientProjector : tlTypeRegularProjector;
					lightShadeInfo.resIndex = 0xFFFFFFFF;

					int arrayIndex = InsertTexture((CTexture*)renderLight.m_pLightImage, m_spotTexAtlas, -1);
					if (arrayIndex >= 0)
						lightShadeInfo.resIndex = (uint32)arrayIndex;
					else
						continue;  // Skip light

					// Prevent culling errors for frustums with large FOVs by slightly enlarging the frustum
					const float frustumAngleDelta = renderLight.m_fLightFrustumAngle > 50 ? 7.5f : 0.0f;

					Matrix34 objMat = renderLight.m_ObjMatrix;
					objMat.m03 = objMat.m13 = objMat.m23 = 0;  // Remove translation
					Vec3 lightDir = objMat * Vec3(-1, 0, 0);
					lightCullInfo.volumeParams0 = Vec4(lightDir.x, lightDir.y, lightDir.z, 0) * matView;
					lightCullInfo.volumeParams0.w = renderLight.m_fRadius * tanf(DEG2RAD(min(renderLight.m_fLightFrustumAngle + frustumAngleDelta, 89.9f))) * sqrt_2;

					Vec3 coneTip = Vec3(lightCullInfo.posRad.x, lightCullInfo.posRad.y, lightCullInfo.posRad.z);
					Vec3 coneDir = Vec3(-lightCullInfo.volumeParams0.x, -lightCullInfo.volumeParams0.y, -lightCullInfo.volumeParams0.z);
					AABB coneBounds = AABB::CreateAABBfromCone(Cone(coneTip, coneDir, renderLight.m_fRadius, lightCullInfo.volumeParams0.w));
					lightCullInfo.depthBounds = Vec2(coneBounds.min.z, coneBounds.max.z) * invCameraFar;

					Matrix44A projMatT;
					CShadowUtils::GetProjectiveTexGen(&renderLight, 0, &projMatT);

					// Translate into camera space
					projMatT.Transpose();
					const Vec4 vEye(rd->GetRCamera().vOrigin, 0.0f);
					Vec4 vecTranslation(vEye.Dot((Vec4&)projMatT.m00), vEye.Dot((Vec4&)projMatT.m10), vEye.Dot((Vec4&)projMatT.m20), vEye.Dot((Vec4&)projMatT.m30));
					projMatT.m03 += vecTranslation.x;
					projMatT.m13 += vecTranslation.y;
					projMatT.m23 += vecTranslation.z;
					projMatT.m33 += vecTranslation.w;

					lightShadeInfo.projectorMatrix = projMatT;
				}

				// Handle rectangular area lights
				if (areaLightRect)
				{
					lightCullInfo.volumeType = tlVolumeOBB;
					lightShadeInfo.lightType = ambientLight ? tlTypeAmbientArea : tlTypeRegularArea;

					float expensionRadius = renderLight.m_fRadius * 1.08f;
					Vec3 scale(expensionRadius, expensionRadius, expensionRadius);
					Matrix34 areaLightMat = CShadowUtils::GetAreaLightMatrix(&renderLight, scale);

					Vec4 u0 = Vec4(areaLightMat.GetColumn0().GetNormalized(), 0) * matView;
					Vec4 u1 = Vec4(areaLightMat.GetColumn1().GetNormalized(), 0) * matView;
					Vec4 u2 = Vec4(areaLightMat.GetColumn2().GetNormalized(), 0) * matView;
					lightCullInfo.volumeParams0 = Vec4(u0.x, u0.y, u0.z, areaLightMat.GetColumn0().GetLength() * 0.5f);
					lightCullInfo.volumeParams1 = Vec4(u1.x, u1.y, u1.z, areaLightMat.GetColumn1().GetLength() * 0.5f);
					lightCullInfo.volumeParams2 = Vec4(u2.x, u2.y, u2.z, areaLightMat.GetColumn2().GetLength() * 0.5f);

					float volumeSize = renderLight.m_fRadius + max(renderLight.m_fAreaWidth, renderLight.m_fAreaHeight);
					lightCullInfo.depthBounds = Vec2(posVS.z - volumeSize, posVS.z + volumeSize) * invCameraFar;

					float areaFov = renderLight.m_fLightFrustumAngle * 2.0f;
					if (renderLight.m_Flags & DLF_CASTSHADOW_MAPS)
						areaFov = min(areaFov, 135.0f); // Shadow can only cover ~135 degree FOV without looking bad, so we clamp the FOV to hide shadow clipping

					const float cosAngle = cosf(areaFov * (gf_PI / 360.0f));

					Matrix44 areaLightParams;
					areaLightParams.SetRow4(0, Vec4(renderLight.m_ObjMatrix.GetColumn0().GetNormalized(), 1.0f));
					areaLightParams.SetRow4(1, Vec4(renderLight.m_ObjMatrix.GetColumn1().GetNormalized(), 1.0f));
					areaLightParams.SetRow4(2, Vec4(renderLight.m_ObjMatrix.GetColumn2().GetNormalized(), 1.0f));
					areaLightParams.SetRow4(3, Vec4(renderLight.m_fAreaWidth * 0.5f, renderLight.m_fAreaHeight * 0.5f, 0, cosAngle));

					lightShadeInfo.projectorMatrix = areaLightParams;
				}

				// Handle shadow casters
				if (!ambientLight && lightIdx >= firstShadowLight && lightIdx < curShadowPoolLight)
				{
					int numDLights = pRenderView->GetDynamicLightsCount();
					int frustumIdx = lightIdx + numDLights;

					auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(frustumIdx);
					if (!SMFrustums.empty())
					{
						ShadowMapFrustum& firstFrustum = *SMFrustums.front()->pFrustum;
						assert(firstFrustum.bUseShadowsPool);

						int numSides = firstFrustum.bOmniDirectionalShadow ? 6 : 1;
						float kernelSize = firstFrustum.bOmniDirectionalShadow ? 2.5f : 1.5f;

						if (numTileLights + numSides > MaxNumTileLights)
							continue;  // Skip light

						static ICVar* pShadowAtlasResCVar = iConsole->GetCVar("e_ShadowsPoolSize");
						const Vec2 shadowParams = Vec2(kernelSize * ((float)firstFrustum.nTexSize / (float)pShadowAtlasResCVar->GetIVal()), firstFrustum.fDepthConstBias);

						const Vec3 cubeDirs[6] = { Vec3(-1, 0, 0), Vec3(1, 0, 0), Vec3(0, -1, 0), Vec3(0, 1, 0), Vec3(0, 0, -1), Vec3(0, 0, 1) };

						for (int side = 0; side < numSides; ++side)
						{
							rd->ConfigShadowTexgen(0, &firstFrustum, side);
							Matrix44A shadowMat = rd->m_TempMatrices[0][0];

							// Translate into camera space
							const Vec4 vEye(rd->GetRCamera().vOrigin, 0.0f);
							Vec4 vecTranslation(vEye.Dot((Vec4&)shadowMat.m00), vEye.Dot((Vec4&)shadowMat.m10), vEye.Dot((Vec4&)shadowMat.m20), vEye.Dot((Vec4&)shadowMat.m30));
							shadowMat.m03 += vecTranslation.x;
							shadowMat.m13 += vecTranslation.y;
							shadowMat.m23 += vecTranslation.z;
							shadowMat.m33 += vecTranslation.w;

							// Pre-multiply by inverse frustum far plane distance
							(Vec4&)shadowMat.m20 *= rd->m_cEF.m_TempVecs[2].x;

							Vec3 cubeDir = cubeDirs[side];
							Vec4 spotParamsVS = Vec4(cubeDir.x, cubeDir.y, cubeDir.z, 0) * matView;

							// slightly enlarge the frustum to prevent culling errors
							spotParamsVS.w = renderLight.m_fRadius * tanf(DEG2RAD(45.0f + 14.5f)) * sqrt_2;

							Vec3 coneTip = Vec3(lightCullInfo.posRad.x, lightCullInfo.posRad.y, lightCullInfo.posRad.z);
							Vec3 coneDir = Vec3(-spotParamsVS.x, -spotParamsVS.y, -spotParamsVS.z);
							AABB coneBounds = AABB::CreateAABBfromCone(Cone(coneTip, coneDir, renderLight.m_fRadius, spotParamsVS.w));
							Vec2 depthBoundsVS = Vec2(coneBounds.min.z, coneBounds.max.z) * invCameraFar;
							Vec2 sideShadowParams = (firstFrustum.nShadowGenMask & (1 << side)) ? shadowParams : Vec2(ZERO);

							if (side == 0)
							{
								lightShadeInfo.shadowParams = sideShadowParams;
								lightShadeInfo.shadowMatrix = shadowMat;
								lightShadeInfo.shadowChannelIndex = Vec4(
								  renderLight.m_ShadowChanMask % 4 == 0,
								  renderLight.m_ShadowChanMask % 4 == 1,
								  renderLight.m_ShadowChanMask % 4 == 2,
								  renderLight.m_ShadowChanMask % 4 == 3
								  );
								lightShadeInfo.shadowMaskIndex = renderLight.m_ShadowMaskIndex;

								if (numSides > 1)
								{
									lightCullInfo.volumeType = tlVolumeCone;
									lightShadeInfo.lightType = tlTypeRegularPointFace;
									lightShadeInfo.resIndex = side;
									lightCullInfo.volumeParams0 = spotParamsVS;
									lightCullInfo.depthBounds = depthBoundsVS;
								}
							}
							else
							{
								// Split point light
								++numTileLights;
								tileLightsCull[numTileLights] = lightCullInfo;
								tileLightsShade[numTileLights] = lightShadeInfo;
								tileLightsShade[numTileLights].shadowParams = sideShadowParams;
								tileLightsShade[numTileLights].shadowMatrix = shadowMat;
								tileLightsShade[numTileLights].resIndex = side;
								tileLightsCull[numTileLights].volumeParams0 = spotParamsVS;
								tileLightsCull[numTileLights].depthBounds = depthBoundsVS;
							}
						}
					}
				}
			}

			// Add current light
			++numTileLights;
			++numValidRenderLights;
		}
	}

	// Invalidate last light in case it got skipped
	if (numTileLights < MaxNumTileLights)
	{
		ZeroMemory(&tileLightsCull[numTileLights], sizeof(STiledLightCullInfo));
		ZeroMemory(&tileLightsShade[numTileLights], sizeof(STiledLightShadeInfo));
	}

	m_numSkippedLights = numRenderLights - numValidRenderLights;

	// Add sun
	if (rd->m_RP.m_pSunLight)
	{
		if (numTileLights < MaxNumTileLights)
		{
			STiledLightCullInfo& lightCullInfo = tileLightsCull[numTileLights];
			STiledLightShadeInfo& lightShadeInfo = tileLightsShade[numTileLights];

			lightCullInfo.volumeType = tlVolumeSun;
			lightCullInfo.depthBounds = Vec2(-100000, 100000);
			lightCullInfo.posRad = Vec4(0, 0, 0, 100000);

			lightShadeInfo.lightType = tlTypeSun;
			lightShadeInfo.attenuationParams = Vec2(SunSourceDiameter, SunSourceDiameter);
			lightShadeInfo.shadowParams = Vec2(1, 0);
			lightShadeInfo.shadowMaskIndex = 0;
			lightShadeInfo.shadowChannelIndex = Vec4(1, 0, 0, 0);
			lightShadeInfo.stencilID0 = lightShadeInfo.stencilID1 = 0;

			Vec3 sunColor;
			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, sunColor);
			sunColor *= rd->m_fAdaptedSceneScaleLBuffer;  // Apply LBuffers range rescale
			lightShadeInfo.color = Vec4(sunColor.x, sunColor.y, sunColor.z, gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER));

			++numTileLights;
		}
		else
		{
			m_numSkippedLights += 1;
		}
	}

#ifndef _RELEASE
	rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumTiledShadingSkippedLights = m_numSkippedLights;
#endif

	// Update light buffer
	m_lightCullInfoBuf.UpdateBufferContent(tileLightsCull, sizeof(STiledLightCullInfo) * MaxNumTileLights);
	m_LightShadeInfoBuf.UpdateBufferContent(tileLightsShade, sizeof(STiledLightShadeInfo) * MaxNumTileLights);

	rd->GetVolumetricFog().PrepareLightList(pRenderView, lightLists[0], lightLists[1], lightLists[2], firstShadowLight, curShadowPoolLight);
}

void CTiledShading::PrepareShadowCastersList(CRenderView* pRenderView)
{
	uint32 firstShadowLight = CDeferredShading::Instance().m_nFirstCandidateShadowPoolLight;
	uint32 curShadowPoolLight = CDeferredShading::Instance().m_nCurrentShadowPoolLight;

	m_arrShadowCastingLights.SetUse(0);

	for (int lightIdx = firstShadowLight; lightIdx < curShadowPoolLight; ++lightIdx)
	{
		const SRenderLight& light = pRenderView->GetLight(eDLT_DeferredLight, lightIdx);
		if (light.m_Flags & DLF_CASTSHADOW_MAPS)
			m_arrShadowCastingLights.Add(lightIdx);
	}
}

void CTiledShading::PrepareClipVolumeList(Vec4* clipVolumeParams)
{
	STiledClipVolumeInfo clipVolumeInfo[MaxNumClipVolumes];
	for (uint32 volumeIndex = 0; volumeIndex < MaxNumClipVolumes; ++volumeIndex)
		clipVolumeInfo[volumeIndex].data = clipVolumeParams[volumeIndex].w;

	m_clipVolumeInfoBuf.UpdateBufferContent(clipVolumeInfo, sizeof(STiledClipVolumeInfo) * MaxNumClipVolumes);
}

void CTiledShading::Render(CRenderView* pRenderView, Vec4* clipVolumeParams)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	if (!CTexture::s_ptexHDRTarget)  // Sketch mode
		return;

	// Temporary hack until tiled shading has proper MSAA support
	if (CRenderer::CV_r_DeferredShadingTiled == 2 && CRenderer::CV_r_msaa)
		CRenderer::CV_r_DeferredShadingTiled = 1;

	auto& defLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	auto& envProbes = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	auto& ambientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

	// Generate shadow mask. Note that in tiled forward only mode the shadow mask is generated in CDeferredShading::DeferredShadingPass()
	if (CRenderer::CV_r_DeferredShadingTiled > 1)
	{
		PROFILE_LABEL_SCOPE("SHADOWMASK");

		PrepareShadowCastersList(pRenderView);
		rd->FX_DeferredShadowMaskGen(pRenderView, m_arrShadowCastingLights);

		rd->FX_SetActiveRenderTargets();
	}

	PROFILE_LABEL_SCOPE("TILED_SHADING");

	PrepareClipVolumeList(clipVolumeParams);

	PrepareLightList(pRenderView);

	// Make sure HDR target is not bound as RT any more
	rd->FX_PushRenderTarget(0, CTexture::s_ptexSceneSpecularAccMap, NULL);

	uint64 nPrevRTFlags = rd->m_RP.m_FlagsShader_RT;
	rd->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5]);

	if (CRenderer::CV_r_DeferredShadingTiled > 1)   // Tiled deferred
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	if (CRenderer::CV_r_DeferredShadingTiled == 4)  // Light coverage visualization
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	if (CRenderer::CV_r_ssdoColorBleeding)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	if (CRenderer::CV_r_SSReflections)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];
	if (CRenderer::CV_r_DeferredShadingSSS)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
	if (CRenderer::CV_r_DeferredShadingAreaLights > 0)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5];

#if defined(FEATURE_SVO_GI)
	rd->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_CUBEMAP0];
	rd->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];

	if (CSvoRenderer::GetInstance()->IsActive())
	{
		int nModeGI = CSvoRenderer::GetInstance()->GetIntegratioMode();

		if (nModeGI == 0 && CSvoRenderer::GetInstance()->GetUseLightProbes())
		{
			// AO modulates diffuse and specular
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
		}
		else if (nModeGI <= 1)
		{
			// GI replaces diffuse and modulates specular
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];
		}
		else if (nModeGI == 2)
		{
			// GI replaces diffuse and specular
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];
		}
	}
#endif

	static CCryNameTSCRC techTiledShading("TiledDeferredShading");

	uint32 nScreenWidth = rd->GetWidth();
	uint32 nScreenHeight = rd->GetHeight();
	uint32 dispatchSizeX = nScreenWidth / LightTileSizeX + (nScreenWidth % LightTileSizeX > 0 ? 1 : 0);
	uint32 dispatchSizeY = nScreenHeight / LightTileSizeY + (nScreenHeight % LightTileSizeY > 0 ? 1 : 0);

	const bool shaderAvailable = SD3DPostEffectsUtils::ShBeginPass(CShaderMan::s_shDeferredShading, techTiledShading, FEF_DONTSETSTATES);
	if (shaderAvailable)  // Temporary workaround for PS4 shader cache issue
	{
		D3DShaderResource* pTiledBaseRes[8] = {
			m_LightShadeInfoBuf.GetSRV(),
			m_specularProbeAtlas.texArray->GetShaderResourceView(),
			m_diffuseProbeAtlas.texArray->GetShaderResourceView(),
			m_spotTexAtlas.texArray->GetShaderResourceView(),
			CTexture::s_ptexRT_ShadowPool->GetShaderResourceView(),
			CTexture::s_ptexShadowJitterMap->GetShaderResourceView(),
			m_lightCullInfoBuf.GetSRV(),
			m_clipVolumeInfoBuf.GetSRV(),
		};
		rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pTiledBaseRes, 16, 8);

		CTexture* texClipVolumeIndex = CTexture::s_ptexVelocity;
		CTexture* pTexCaustics = m_bApplyCaustics ? CTexture::s_ptexSceneTargetR11G11B10F[1] : CTexture::s_ptexBlack;

		CTexture* ptexGiDiff = CTexture::s_ptexBlack;
		CTexture* ptexGiSpec = CTexture::s_ptexBlack;

#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance()->IsActive() && CSvoRenderer::GetInstance()->GetSpecularFinRT())
		{
			ptexGiDiff = (CTexture*)CSvoRenderer::GetInstance()->GetDiffuseFinRT();
			ptexGiSpec = (CTexture*)CSvoRenderer::GetInstance()->GetSpecularFinRT();
		}
#endif

		D3DShaderResource* pDeferredShadingRes[13] = {
			CTexture::s_ptexZTarget->GetShaderResourceView(),
			CTexture::s_ptexSceneNormalsMap->GetShaderResourceView(),
			CTexture::s_ptexSceneSpecular->GetShaderResourceView(),
			CTexture::s_ptexSceneDiffuse->GetShaderResourceView(),
			CTexture::s_ptexShadowMask->GetShaderResourceView(),
			CTexture::s_ptexSceneNormalsBent->GetShaderResourceView(),
			CTexture::s_ptexHDRTargetScaledTmp[0]->GetShaderResourceView(),
			CTexture::s_ptexEnvironmentBRDF->GetShaderResourceView(),
			texClipVolumeIndex->GetShaderResourceView(),
			CTexture::s_ptexAOColorBleed->GetShaderResourceView(),
			ptexGiDiff->GetShaderResourceView(),
			ptexGiSpec->GetShaderResourceView(),
			pTexCaustics->GetShaderResourceView(),
		};
		rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pDeferredShadingRes, 0, 13);

		static CCryNameR paramProj("ProjParams");
		Vec4 vParamProj(rd->m_ProjMatrix.m00, rd->m_ProjMatrix.m11, rd->m_ProjMatrix.m20, rd->m_ProjMatrix.m21);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramProj, &vParamProj, 1);

		static CCryNameR paramScreenSize("ScreenSize");
		float fScreenWidth = (float)nScreenWidth;
		float fScreenHeight = (float)nScreenHeight;
		Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramScreenSize, &vParamScreenSize, 1);

		Vec3 worldViewPos = rd->GetRCamera().vOrigin;
		static CCryNameR paramWorldViewPos("WorldViewPos");
		Vec4 vParamWorldViewPos(worldViewPos.x, worldViewPos.y, worldViewPos.z, 0);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramWorldViewPos, &vParamWorldViewPos, 1);

		SD3DPostEffectsUtils::UpdateFrustumCorners();
		static CCryNameR paramFrustumTL("FrustumTL");
		Vec4 vParamFrustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramFrustumTL, &vParamFrustumTL, 1);
		static CCryNameR paramFrustumTR("FrustumTR");
		Vec4 vParamFrustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramFrustumTR, &vParamFrustumTR, 1);
		static CCryNameR paramFrustumBL("FrustumBL");
		Vec4 vParamFrustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramFrustumBL, &vParamFrustumBL, 1);

		Vec3 sunDir = gEnv->p3DEngine->GetSunDirNormalized();
		static CCryNameR paramSunDir("SunDir");
		Vec4 vParamSunDir(sunDir.x, sunDir.y, sunDir.z, SunDistance);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramSunDir, &vParamSunDir, 1);

		Vec3 sunColor;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, sunColor);
		sunColor *= rd->m_fAdaptedSceneScaleLBuffer;  // Apply LBuffers range rescale
		static CCryNameR paramSunColor("SunColor");
		Vec4 vParamSunColor(sunColor.x, sunColor.y, sunColor.z, gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER));
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramSunColor, &vParamSunColor, 1);

		static CCryNameR paramSSDO("SSDOParams");
		Vec4 vParamSSDO(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);

#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance()->IsActive())
			vParamSSDO *= CSvoRenderer::GetInstance()->GetSsaoAmount();
#endif

		Vec4 vParamSSDONull(0, 0, 0, 0);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramSSDO, CRenderer::CV_r_ssdo ? &vParamSSDO : &vParamSSDONull, 1);

		rd->FX_Commit();

		D3DUAV* pUAV[3] = {
			m_tileLightIndexBuf.GetUAV(),
			CTexture::s_ptexHDRTarget->GetDeviceUAV(),
			CTexture::s_ptexSceneTargetR11G11B10F[0]->GetDeviceUAV(),
		};
		rd->GetDeviceContext().CSSetUnorderedAccessViews(0, 3, pUAV, NULL);

		rd->m_DevMan.Dispatch(dispatchSizeX, dispatchSizeY, 1);
		SD3DPostEffectsUtils::ShEndPass();
	}

	D3DUAV* pUAVNull[3] = { NULL };
	rd->GetDeviceContext().CSSetUnorderedAccessViews(0, 3, pUAVNull, NULL);

	D3DShaderResource* pSRVNull[13] = { NULL };
	rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pSRVNull, 0, 13);
	rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pSRVNull, 16, 8);

	rd->FX_PopRenderTarget(0);

	rd->m_RP.m_FlagsShader_RT = nPrevRTFlags;

	// Output debug information
	if (CRenderer::CV_r_DeferredShadingTiled == 3)
	{
		rd->Draw2dLabel(20, 60, 2.0f, Col_Blue, false, "Tiled Shading Debug");
		rd->Draw2dLabel(20, 95, 1.7f, m_numSkippedLights > 0 ? Col_Red : Col_Blue, false, "Skipped Lights: %i", m_numSkippedLights);
		rd->Draw2dLabel(20, 120, 1.7f, Col_Blue, false, "Atlas Updates: %i", m_numAtlasUpdates);
	}

	m_bApplyCaustics = false;  // Reset flag
}

void CTiledShading::BindForwardShadingResources(CShader*, CDeviceManager::SHADER_TYPE shaderType)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CTexture* ptexGiDiff = CTexture::s_ptexBlack;
	CTexture* ptexGiSpec = CTexture::s_ptexBlack;
	CTexture* ptexRsmCol = CTexture::s_ptexBlack;
	CTexture* ptexRsmNor = CTexture::s_ptexBlack;

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetInstance()->IsActive() && CSvoRenderer::GetInstance()->GetSpecularFinRT())
	{
		ptexGiDiff = (CTexture*)CSvoRenderer::GetInstance()->GetDiffuseFinRT();
		ptexGiSpec = (CTexture*)CSvoRenderer::GetInstance()->GetSpecularFinRT();
		if(CSvoRenderer::GetInstance()->GetRsmPoolCol())
			ptexRsmCol = (CTexture*)CSvoRenderer::GetInstance()->GetRsmPoolCol();
		if(CSvoRenderer::GetInstance()->GetRsmPoolNor())
			ptexRsmNor = (CTexture*)CSvoRenderer::GetInstance()->GetRsmPoolNor();
	}
#endif

	if (!CRenderer::CV_r_DeferredShadingTiled)
	{
		rd->m_DevMan.BindSRV(shaderType, ptexGiDiff->GetShaderResourceView(), 24);
		rd->m_DevMan.BindSRV(shaderType, ptexGiSpec->GetShaderResourceView(), 25);
		rd->m_DevMan.BindSRV(shaderType, ptexRsmCol->GetShaderResourceView(), 26);
		rd->m_DevMan.BindSRV(shaderType, ptexRsmNor->GetShaderResourceView(), 27);

		return;
	}

	D3DShaderResource* pTiledBaseRes[12] = {
		m_LightShadeInfoBuf.GetSRV(),
		m_specularProbeAtlas.texArray->GetShaderResourceView(),
		m_diffuseProbeAtlas.texArray->GetShaderResourceView(),
		m_spotTexAtlas.texArray->GetShaderResourceView(),
		CTexture::s_ptexRT_ShadowPool->GetShaderResourceView(),
		CTexture::s_ptexShadowJitterMap->GetShaderResourceView(),
		m_tileLightIndexBuf.GetSRV(),
		m_clipVolumeInfoBuf.GetSRV(),
		ptexGiDiff->GetShaderResourceView(),
		ptexGiSpec->GetShaderResourceView(),
		ptexRsmCol->GetShaderResourceView(),
		ptexRsmNor->GetShaderResourceView(),
	};
	rd->m_DevMan.BindSRV(shaderType, pTiledBaseRes, 16, 12);

	D3DSamplerState* pSamplers[1] = {
		(D3DSamplerState*)CTexture::s_TexStates[m_nTexStateCompare].m_pDeviceState,
	};
	rd->m_DevMan.BindSampler(shaderType, pSamplers, 14, 1);
}

void CTiledShading::UnbindForwardShadingResources(CDeviceManager::SHADER_TYPE shaderType)
{
	if (!CRenderer::CV_r_DeferredShadingTiled)
		return;

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	D3DShaderResource* pNullViews[12] = { NULL };
	rd->m_DevMan.BindSRV(shaderType, pNullViews, 16, 12);

#ifndef _RELEASE
	// Verify that the sampler states did not get overwritten
	//if (rd->m_DevMan.m_Samplers[shaderType].samplers[14] != CTexture::s_TexStates[m_nTexStateCompare].m_pDeviceState)
	//{
	//	assert( 0 );
	//	iLog->LogError( "Tiled Shading Error: Sampler got overwritten" );
	//}
#endif

	D3DSamplerState* pNullSamplers[1] = { NULL };
	rd->m_DevMan.BindSampler(shaderType, pNullSamplers, 14, 1);

	// reset sampler state cache because we accessed states directly
	if(shaderType == CDeviceManager::TYPE_VS)
		CTexture::s_TexStateIDs[eHWSC_Vertex][14] = -1;
	else if (shaderType == CDeviceManager::TYPE_PS)
		CTexture::s_TexStateIDs[eHWSC_Pixel][14] = -1;
	else if (shaderType == CDeviceManager::TYPE_GS)
		CTexture::s_TexStateIDs[eHWSC_Geometry][14] = -1;
	else if (shaderType == CDeviceManager::TYPE_DS)
		CTexture::s_TexStateIDs[eHWSC_Domain][14] = -1;
	else if (shaderType == CDeviceManager::TYPE_HS)
		CTexture::s_TexStateIDs[eHWSC_Hull][14] = -1;
	else if (shaderType == CDeviceManager::TYPE_CS)
		CTexture::s_TexStateIDs[eHWSC_Compute][14] = -1;
}

struct STiledLightShadeInfo* CTiledShading::GetTiledLightShadeInfo()
{
	return &tileLightsShade[0];
}
