// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================

   Revision history:
   Created Jan 2013 by Nicolas Schulz

   =============================================================================*/

#include "StdAfx.h"
#include "D3DTiledShading.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#include "Common/RenderView.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/ShadowMask.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32 AtlasArrayDim = 64;
const uint32 SpotTexSize = 512;
const uint32 DiffuseProbeSize = 32;
const uint32 SpecProbeSize = 256;
const uint32 ShadowSampleCount = 16;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following structs and constants have to match the shader code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32 MaxNumClipVolumes = CClipVolumesStage::MaxDeferredClipVolumes;

struct STiledClipVolumeInfo
{
	float data;
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{
STiledLightCullInfo* tileLightsCull;
STiledLightShadeInfo* tileLightsShade;
}

CTiledShading::CTiledShading()
{
	// 16 byte alignment is important for performance on nVidia cards
	static_assert(sizeof(STiledLightCullInfo) % 16 == 0, "STiledLightCullInfo should be 16 byte aligned for GPU performance");
	static_assert(sizeof(STiledLightShadeInfo) % 16 == 0, "STiledLightShadeInfo should be 16 byte aligned for GPU performance");

	tileLightsCull = reinterpret_cast<STiledLightCullInfo*>(CryModuleMemalign(sizeof(STiledLightCullInfo) * MaxNumTileLights, CDeviceBufferManager::GetBufferAlignmentForStreaming()));
	tileLightsShade = reinterpret_cast<STiledLightShadeInfo*>(CryModuleMemalign(sizeof(STiledLightShadeInfo) * MaxNumTileLights, CDeviceBufferManager::GetBufferAlignmentForStreaming()));

	m_dispatchSizeX = 0;
	m_dispatchSizeY = 0;

	m_numAtlasUpdates = 0;
	m_numValidLights = 0;
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
		assert(m_lightCullInfoBuf.GetDevBuffer() && m_specularProbeAtlas.texArray);
		return;
	}
	else
	{
		DestroyResources(false);
	}

	m_dispatchSizeX = dispatchSizeX;
	m_dispatchSizeY = dispatchSizeY;

	if (!m_lightCullInfoBuf.IsAvailable())
	{
		m_lightCullInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightCullInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	}

	if (!m_lightShadeInfoBuf.IsAvailable())
	{
		m_lightShadeInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightShadeInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	}

	if (!m_tileOpaqueLightMaskBuf.IsAvailable())
	{
		m_tileOpaqueLightMaskBuf.Create(dispatchSizeX * dispatchSizeY * 8, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, NULL);
	}

	if (!m_tileTranspLightMaskBuf.IsAvailable())
	{
		m_tileTranspLightMaskBuf.Create(dispatchSizeX * dispatchSizeY * 8, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, NULL);
	}

	if (!m_clipVolumeInfoBuf.IsAvailable())
	{
		m_clipVolumeInfoBuf.Create(MaxNumClipVolumes, sizeof(STiledClipVolumeInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	}

#if CRY_RENDERER_OPENGLES || CRY_PLATFORM_ANDROID
	ETEX_Format textureAtlasFormatSpecDiff = eTF_R16G16B16A16F;
	ETEX_Format textureAtlasFormatSpot = eTF_EAC_R11;
#else
	ETEX_Format textureAtlasFormatSpecDiff = eTF_BC6UH;
	ETEX_Format textureAtlasFormatSpot = eTF_BC4U;
#endif

	if (!m_specularProbeAtlas.texArray)
	{
		m_specularProbeAtlas.texArray = CTexture::GetOrCreateTextureArray("$TiledSpecProbeTexArr", SpecProbeSize, SpecProbeSize, AtlasArrayDim * 6, IntegerLog2(SpecProbeSize) - 1, eTT_CubeArray, FT_DONT_STREAM, textureAtlasFormatSpecDiff);
		m_specularProbeAtlas.items.resize(AtlasArrayDim);

		if (m_specularProbeAtlas.texArray->GetFlags() & FT_FAILED)
			CryFatalError("Couldn't allocate specular probe texture atlas");

#if DEVRES_USE_PINNING
		// Perma-pin, as they need to be pinned each frame anyway, and doing it upfront avoids the cost of doing it each frame.
		m_specularProbeAtlas.texArray->GetDevTexture()->Pin();
#endif
	}

	if (!m_diffuseProbeAtlas.texArray)
	{
		m_diffuseProbeAtlas.texArray = CTexture::GetOrCreateTextureArray("$TiledDiffuseProbeTexArr", DiffuseProbeSize, DiffuseProbeSize, AtlasArrayDim * 6, 1, eTT_CubeArray, FT_DONT_STREAM, textureAtlasFormatSpecDiff);
		m_diffuseProbeAtlas.items.resize(AtlasArrayDim);

		if (m_diffuseProbeAtlas.texArray->GetFlags() & FT_FAILED)
			CryFatalError("Couldn't allocate diffuse probe texture atlas");

#if DEVRES_USE_PINNING
		// Perma-pin, as they need to be pinned each frame anyway, and doing it upfront avoids the cost of doing it each frame.
		m_diffuseProbeAtlas.texArray->GetDevTexture()->Pin();
#endif
	}

	if (!m_spotTexAtlas.texArray)
	{
		// Note: BC4 has 4x4 as lowest mipmap
		m_spotTexAtlas.texArray = CTexture::GetOrCreateTextureArray("$TiledSpotTexArr", SpotTexSize, SpotTexSize, AtlasArrayDim * 1, IntegerLog2(SpotTexSize) - 1, eTT_2DArray, FT_DONT_STREAM, textureAtlasFormatSpot);
		m_spotTexAtlas.items.resize(AtlasArrayDim);

		if (m_spotTexAtlas.texArray->GetFlags() & FT_FAILED)
			CryFatalError("Couldn't allocate spot texture atlas");

#if DEVRES_USE_PINNING
		// Perma-pin, as they need to be pinned each frame anyway, and doing it upfront avoids the cost of doing it each frame.
		m_spotTexAtlas.texArray->GetDevTexture()->Pin();
#endif
	}
}

void CTiledShading::DestroyResources(bool destroyResolutionIndependentResources)
{
	m_dispatchSizeX = 0;
	m_dispatchSizeY = 0;

	m_lightCullInfoBuf.Release();
	m_lightShadeInfoBuf.Release();
	m_tileOpaqueLightMaskBuf.Release();
	m_tileTranspLightMaskBuf.Release();
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

int CTiledShading::InsertTexture(CTexture* pTexInput, TextureAtlas& atlas, int arrayIndex)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const int frameID = rd->GetFrameID(false);

	if (pTexInput == NULL)
		return -1;

	// Make sure the texture is loaded already
	if (pTexInput->GetWidthNonVirtual() == 0 && pTexInput->GetHeightNonVirtual() == 0)
		return -1;

	bool isEditor = gEnv->IsEditor();

	// Check if texture is already in atlas
	for (uint32 i = 0, si = atlas.items.size(); i < si; ++i)
	{
		if (atlas.items[i].texture == pTexInput)
		{
			bool textureUpToDate = !isEditor ? true : (pTexInput->GetUpdateFrameID() == abs(atlas.items[i].updateFrameID) || pTexInput->GetUpdateFrameID() == 0);

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

	atlas.items[arrayIndex].texture = pTexInput;
	atlas.items[arrayIndex].accessFrameID = frameID;
	atlas.items[arrayIndex].updateFrameID = pTexInput->GetUpdateFrameID();
	atlas.items[arrayIndex].invalid = false;

	// Check if texture is valid
	if (!pTexInput->IsLoaded() ||
	    pTexInput->GetWidthNonVirtual() < atlas.texArray->GetWidthNonVirtual() ||
	    pTexInput->GetWidthNonVirtual() != pTexInput->GetHeightNonVirtual() ||
	    pTexInput->GetPixelFormat() != atlas.texArray->GetPixelFormat() ||
	    (pTexInput->IsStreamed() && pTexInput->IsPartiallyLoaded()))
	{
		atlas.items[arrayIndex].invalid = true;

		if (!pTexInput->IsLoaded())
		{
			iLog->LogError("TiledShading: Texture not found: %s", pTexInput->GetName());
		}
		else if (pTexInput->IsStreamed() && pTexInput->IsPartiallyLoaded())
		{
			iLog->LogError("TiledShading: Texture not fully streamed so impossible to add: %s (W:%i H:%i F:%s)",
			               pTexInput->GetName(), pTexInput->GetWidth(), pTexInput->GetHeight(), pTexInput->GetFormatName());
		}
		else if (pTexInput->GetPixelFormat() != atlas.texArray->GetPixelFormat())
		{
			iLog->LogError("TiledShading: Unsupported texture format: %s (W:%i H:%i F:%s), it has to be equal to the tile-atlas (F:%s), please change the texture's preset by re-exporting with CryTif",
			               pTexInput->GetName(), pTexInput->GetWidth(), pTexInput->GetHeight(), pTexInput->GetFormatName(), atlas.texArray->GetFormatName());
		}
		else
		{
			iLog->LogError("TiledShading: Unsupported texture properties: %s (W:%i H:%i F:%s)",
			               pTexInput->GetName(), pTexInput->GetWidth(), pTexInput->GetHeight(), pTexInput->GetFormatName());
		}
		return -1;
	}

	// Update atlas
	uint32 numSrcMips = (uint32)pTexInput->GetNumMipsNonVirtual();
	uint32 numDstMips = (uint32)atlas.texArray->GetNumMipsNonVirtual();
	uint32 firstSrcMip = IntegerLog2((uint32)pTexInput->GetWidthNonVirtual() / atlas.texArray->GetWidthNonVirtual());
	uint32 numFaces = atlas.texArray->GetTexType() == eTT_CubeArray ? 6 : 1;

	CDeviceTexture* pTexInputDevTex = pTexInput->GetDevTexture();
	CDeviceTexture* pTexArrayDevTex = atlas.texArray->GetDevTexture();

	ASSERT_DEVICE_TEXTURE_IS_FIXED(pTexArrayDevTex);

	UINT w = atlas.texArray->GetWidthNonVirtual();
	UINT h = atlas.texArray->GetHeightNonVirtual();
	UINT d = atlas.texArray->GetDepthNonVirtual();

	if ((numDstMips == numSrcMips) && (firstSrcMip == 0) && 0 && "TODO: kill this branch, after taking a careful look for side-effects")
	{
		// MipMaps are laid out consecutively in the sub-resource specification, which allows us to issue a batch-copy for N sub-resources
		const UINT NumSubresources = numDstMips * numFaces;
		const UINT SrcSubresource = D3D11CalcSubresource(0, 0, numSrcMips);
		const UINT DstSubresource = D3D11CalcSubresource(0, arrayIndex * numFaces, numDstMips);

		const SResourceRegionMapping mapping =
		{
			{ 0, 0, 0, SrcSubresource  }, // src position
			{ 0, 0, 0, DstSubresource  }, // dst position
			{ w, h, d, NumSubresources }, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
			pTexInputDevTex,
			pTexArrayDevTex,
			mapping);
	}
	else
	{
		for (uint32 i = 0; i < numFaces; ++i)
		{
			// MipMaps are laid out consecutively in the sub-resource specification, which allows us to issue a batch-copy for N sub-resources
			const UINT NumSubresources = numDstMips;
			const UINT SrcSubresource = D3D11CalcSubresource(0 + firstSrcMip, i, numSrcMips);
			const UINT DstSubresource = D3D11CalcSubresource(0, arrayIndex * numFaces + i, numDstMips);
			
			const SResourceRegionMapping mapping =
			{
				{ 0, 0, 0, SrcSubresource  }, // src position
				{ 0, 0, 0, DstSubresource  }, // dst position
				{ w, h, d, NumSubresources }, // size
				D3D11_COPY_NO_OVERWRITE_REVERT
			};

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
				pTexInputDevTex,
				pTexArrayDevTex,
				mapping);
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

	const auto& defLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	const auto& envProbes = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	const auto& ambientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

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

	const uint32 lightArraySize = 3;
	const RenderLightsList* lightLists[3] = {
		CRenderer::CV_r_DeferredShadingEnvProbes ? &envProbes : NULL,
		CRenderer::CV_r_DeferredShadingAmbientLights ? &ambientLights : NULL,
		CRenderer::CV_r_DeferredShadingLights ? &defLights : NULL
	};

	for (uint32 lightListIdx = 0; lightListIdx < lightArraySize; ++lightListIdx)
	{
		if (lightLists[lightListIdx] == NULL)
			continue;

		auto stp = lightLists[lightListIdx]->end();
		auto itr = lightLists[lightListIdx]->begin();
		for (uint32 lightIdx = 0; itr != stp; ++itr, ++lightIdx)
		{
			const SRenderLight& renderLight = *itr;
			STiledLightInfo& lightInfo = m_tileLights[numTileLights];
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
			lightInfo.posRad = Vec4(pos, volumeSize);
			Vec4 posVS = Vec4(pos, 1) * matView;
			lightInfo.depthBoundsVS = Vec2(posVS.z - volumeSize, posVS.z + volumeSize) * invCameraFar;
			lightShadeInfo.posRad = Vec4(pos.x, pos.y, pos.z, volumeSize);
			lightShadeInfo.attenuationParams = Vec2(areaLightRect ? (renderLight.m_fAreaWidth + renderLight.m_fAreaHeight) * 0.25f : renderLight.m_fAttenuationBulbSize, renderLight.m_fAreaHeight * 0.5f);
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
				lightInfo.volumeType = tlVolumeOBB;
				lightShadeInfo.lightType = tlTypeProbe;
				lightShadeInfo.resIndex = 0xFFFFFFFF;
				lightShadeInfo.attenuationParams = Vec2(renderLight.m_Color.a, max(renderLight.GetFalloffMax(), 0.001f));

				AABB aabb = RotateAABB(AABB(-renderLight.m_ProbeExtents, renderLight.m_ProbeExtents), Matrix33(renderLight.m_ObjMatrix));
				aabb = RotateAABB(aabb, Matrix33(matView));
				lightInfo.depthBoundsVS = Vec2(posVS.z + aabb.min.z, posVS.z + aabb.max.z) * invCameraFar;

				lightInfo.volumeParams0 = Vec4(renderLight.m_ObjMatrix.GetColumn0().GetNormalized(), renderLight.m_ProbeExtents.x);
				lightInfo.volumeParams1 = Vec4(renderLight.m_ObjMatrix.GetColumn1().GetNormalized(), renderLight.m_ProbeExtents.y);
				lightInfo.volumeParams2 = Vec4(renderLight.m_ObjMatrix.GetColumn2().GetNormalized(), renderLight.m_ProbeExtents.z);

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

				lightInfo.volumeType = tlVolumeSphere;
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
					lightInfo.volumeType = tlVolumeCone;
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
					lightInfo.volumeParams0 = Vec4(lightDir, 0);
					lightInfo.volumeParams0.w = renderLight.m_fRadius * tanf(DEG2RAD(min(renderLight.m_fLightFrustumAngle + frustumAngleDelta, 89.9f))) * sqrt_2;
					lightInfo.volumeParams1.w = DEG2RAD(renderLight.m_fLightFrustumAngle);

					Vec3 coneTipVS = Vec3(posVS);
					Vec3 coneDirVS = Vec3(Vec4(-lightDir.x, -lightDir.y, -lightDir.z, 0) * matView);
					AABB coneBounds = AABB::CreateAABBfromCone(Cone(coneTipVS, coneDirVS, renderLight.m_fRadius, lightInfo.volumeParams0.w));
					lightInfo.depthBoundsVS.x = std::min(lightInfo.depthBoundsVS.x, coneBounds.min.z * invCameraFar);
					lightInfo.depthBoundsVS.y = std::min(lightInfo.depthBoundsVS.y, coneBounds.max.z * invCameraFar);

					Matrix44A projMatT;
					CShadowUtils::GetProjectiveTexGen(&renderLight, 0, &projMatT);
					projMatT.Transpose();

					lightShadeInfo.projectorMatrix = projMatT;
				}

				// Handle rectangular area lights
				if (areaLightRect)
				{
					lightInfo.volumeType = tlVolumeOBB;
					lightShadeInfo.lightType = ambientLight ? tlTypeAmbientArea : tlTypeRegularArea;

					float expensionRadius = renderLight.m_fRadius * 1.08f;
					Vec3 scale(expensionRadius, expensionRadius, expensionRadius);
					Matrix34 areaLightMat = CShadowUtils::GetAreaLightMatrix(&renderLight, scale);

					lightInfo.volumeParams0 = Vec4(areaLightMat.GetColumn0().GetNormalized(), areaLightMat.GetColumn0().GetLength() * 0.5f);
					lightInfo.volumeParams1 = Vec4(areaLightMat.GetColumn1().GetNormalized(), areaLightMat.GetColumn1().GetLength() * 0.5f);
					lightInfo.volumeParams2 = Vec4(areaLightMat.GetColumn2().GetNormalized(), areaLightMat.GetColumn2().GetLength() * 0.5f);

					float volumeSize = renderLight.m_fRadius + max(renderLight.m_fAreaWidth, renderLight.m_fAreaHeight);
					lightInfo.depthBoundsVS = Vec2(posVS.z - volumeSize, posVS.z + volumeSize) * invCameraFar;

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

							// Pre-multiply by inverse frustum far plane distance
							(Vec4&)shadowMat.m20 *= rd->m_cEF.m_TempVecs[2].x;

							Vec4 spotParams = Vec4(cubeDirs[side].x, cubeDirs[side].y, cubeDirs[side].z, 0);
							// slightly enlarge the frustum to prevent culling errors
							spotParams.w = renderLight.m_fRadius * tanf(DEG2RAD(45.0f + 14.5f)) * sqrt_2;

							Vec3 coneTipVS = Vec3(posVS);
							Vec3 coneDirVS = Vec3(Vec4(-spotParams.x, -spotParams.y, -spotParams.z, 0) * matView);
							AABB coneBounds = AABB::CreateAABBfromCone(Cone(coneTipVS, coneDirVS, renderLight.m_fRadius, spotParams.w));
							Vec2 depthBoundsVS = Vec2(coneBounds.min.z, coneBounds.max.z) * invCameraFar;
							Vec2 sideShadowParams = (firstFrustum.nShadowGenMask & (1 << side)) ? shadowParams : Vec2(ZERO);

							if (side == 0)
							{
								lightShadeInfo.shadowParams = sideShadowParams;
								lightShadeInfo.shadowMatrix = shadowMat;
								lightShadeInfo.shadowMaskIndex = renderLight.m_ShadowMaskIndex;

								if (numSides > 1)
								{
									lightInfo.volumeType = tlVolumeCone;
									lightInfo.volumeParams0 = spotParams;
									lightInfo.volumeParams1.w = DEG2RAD(45.0f);
									lightInfo.depthBoundsVS = depthBoundsVS;
									lightShadeInfo.lightType = tlTypeRegularPointFace;
									lightShadeInfo.resIndex = side;
								}
							}
							else
							{
								// Split point light
								++numTileLights;
								m_tileLights[numTileLights] = lightInfo;
								m_tileLights[numTileLights].volumeParams0 = spotParams;
								m_tileLights[numTileLights].volumeParams1.w = DEG2RAD(45.0f);
								m_tileLights[numTileLights].depthBoundsVS = depthBoundsVS;
								tileLightsShade[numTileLights] = lightShadeInfo;
								tileLightsShade[numTileLights].shadowParams = sideShadowParams;
								tileLightsShade[numTileLights].shadowMatrix = shadowMat;
								tileLightsShade[numTileLights].resIndex = side;
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
			STiledLightInfo& lightInfo = m_tileLights[numTileLights];
			STiledLightShadeInfo& lightShadeInfo = tileLightsShade[numTileLights];

			lightInfo.volumeType = tlVolumeSun;
			lightInfo.posRad = Vec4(0, 0, 0, 100000);
			lightInfo.depthBoundsVS = Vec2(-100000, 100000);

			lightShadeInfo.lightType = tlTypeSun;
			lightShadeInfo.attenuationParams = Vec2(TiledShading_SunSourceDiameter, TiledShading_SunSourceDiameter);
			lightShadeInfo.shadowParams = Vec2(1, 0);
			lightShadeInfo.shadowMaskIndex = 0;
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

	m_numValidLights = numTileLights;
#ifndef _RELEASE
	rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumTiledShadingSkippedLights = m_numSkippedLights;
#endif

	// Prepare cull info
	for (uint32 i = 0; i < numTileLights; i++)
	{
		STiledLightInfo& lightInfo = m_tileLights[i];
		STiledLightCullInfo& lightCullInfo = tileLightsCull[i];

		lightCullInfo.volumeType = lightInfo.volumeType;
		lightCullInfo.posRad = Vec4(Vec3(Vec4(Vec3(lightInfo.posRad), 1) * matView), lightInfo.posRad.w);
		lightCullInfo.depthBounds = lightInfo.depthBoundsVS;
		
		if (lightInfo.volumeType == tlVolumeSun)
		{
			lightCullInfo.posRad = lightInfo.posRad;
		}
		else if (lightInfo.volumeType == tlVolumeOBB)
		{
			lightCullInfo.volumeParams0 = Vec4(Vec3(Vec4(Vec3(lightInfo.volumeParams0), 0) * matView), lightInfo.volumeParams0.w);
			lightCullInfo.volumeParams1 = Vec4(Vec3(Vec4(Vec3(lightInfo.volumeParams1), 0) * matView), lightInfo.volumeParams1.w);
			lightCullInfo.volumeParams2 = Vec4(Vec3(Vec4(Vec3(lightInfo.volumeParams2), 0) * matView), lightInfo.volumeParams2.w);
		}
		else if (lightInfo.volumeType == tlVolumeCone)
		{
			lightCullInfo.volumeParams0 = Vec4(Vec3(Vec4(Vec3(lightInfo.volumeParams0), 0) * matView), lightInfo.volumeParams0.w);
			lightCullInfo.volumeParams1.w = lightInfo.volumeParams1.w;
		}
	}

	// NOTE: Update full light buffer, because there are "num-threads" checks for Zero-struct size needs to be aligned to that (0,64,128,192,255 are the only possible ones for 64 threat-group size)
	const size_t tileLightsCullUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(STiledLightCullInfo) * std::min(MaxNumTileLights, Align(numTileLights, 64) + 64));
	const size_t tileLightsShadeUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(STiledLightShadeInfo) * std::min(MaxNumTileLights, Align(numTileLights, 64) + 64));

	m_lightCullInfoBuf.UpdateBufferContent(tileLightsCull, tileLightsCullUploadSize);
	m_lightShadeInfoBuf.UpdateBufferContent(tileLightsShade, tileLightsShadeUploadSize);
}

void CTiledShading::PrepareClipVolumeList(Vec4* clipVolumeParams)
{
	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVector(STiledClipVolumeInfo, MaxNumClipVolumes, clipVolumeInfo, CDeviceBufferManager::AlignBufferSizeForStreaming);

	for (uint32 volumeIndex = 0; volumeIndex < MaxNumClipVolumes; ++volumeIndex)
		clipVolumeInfo[volumeIndex].data = clipVolumeParams[volumeIndex].w;

	m_clipVolumeInfoBuf.UpdateBufferContent(clipVolumeInfo, clipVolumeInfoSize);
}

void CTiledShading::Render(CRenderView* pRenderView, Vec4* clipVolumeParams)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	// Temporary hack until tiled shading has proper MSAA support
	if (CRenderer::CV_r_DeferredShadingTiled >= 2 && CRenderer::CV_r_msaa)
		CRenderer::CV_r_DeferredShadingTiled = 1;

	auto& defLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	auto& envProbes = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	auto& ambientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

	PROFILE_LABEL_SCOPE("TILED_SHADING");

	PrepareClipVolumeList(clipVolumeParams);

	PrepareLightList(pRenderView);

	if (rd->m_nGraphicsPipeline > 0)
	{
		rd->GetGraphicsPipeline().RenderTiledShading();
		return;
	}

	// Make sure HDR target is not bound as RT any more
	rd->FX_PushRenderTarget(0, CTexture::s_ptexSceneSpecularAccMap, NULL);

	uint64 nPrevRTFlags = rd->m_RP.m_FlagsShader_RT;
	rd->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5]);

	if (CRenderer::CV_r_DeferredShadingTiled > 1)   // Tiled deferred
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	if (CRenderer::CV_r_DeferredShadingTiledDebug == 2)  // Light coverage visualization
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
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

	const bool shaderAvailable = SD3DPostEffectsUtils::ShBeginPass(CShaderMan::s_shDeferredShading, techTiledShading, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	if (shaderAvailable)  // Temporary workaround for PS4 shader cache issue
	{
		D3DShaderResource* pTiledBaseRes[8] = {
			m_lightShadeInfoBuf.             GetDevBuffer ()->LookupSRV(EDefaultResourceViews::Default),
			m_specularProbeAtlas.texArray->  GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			m_diffuseProbeAtlas.texArray->   GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			m_spotTexAtlas.texArray->        GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexRT_ShadowPool->  GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexShadowJitterMap->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			m_lightCullInfoBuf.              GetDevBuffer ()->LookupSRV(EDefaultResourceViews::Default),
			m_clipVolumeInfoBuf.             GetDevBuffer ()->LookupSRV(EDefaultResourceViews::Default),
		};
		rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pTiledBaseRes, 16, 8);

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
			CTexture::s_ptexZTarget->                                         GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexSceneNormalsMap->                                 GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexSceneSpecular->                                   GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexSceneDiffuse->                                    GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexShadowMask->                                      GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexSceneNormalsBent->                                GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexHDRTargetScaledTmp[0]->                           GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CTexture::s_ptexEnvironmentBRDF->                                 GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			texClipVolumeIndex->                                              GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			CRenderer::CV_r_ssdoColorBleeding ? CTexture::s_ptexAOColorBleed->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default)
			                                  : CTexture::s_ptexBlack->       GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			ptexGiDiff->                                                      GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			ptexGiSpec->                                                      GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
			pTexCaustics->                                                    GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
		};
		rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pDeferredShadingRes, 0, 13);

		// Samplers
		D3DSamplerState* pSamplers[1] = {
			(D3DSamplerState*)CDeviceObjectFactory::LookupSamplerState(EDefaultSamplerStates::TrilinearClamp).second,
		};
		rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_CS, pSamplers, 0, 1);

		static CCryNameR paramProj("ProjParams");
		Vec4 vParamProj(rd->m_ProjMatrix.m00, rd->m_ProjMatrix.m11, rd->m_ProjMatrix.m20, rd->m_ProjMatrix.m21);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramProj, &vParamProj, 1);

		static CCryNameR paramScreenSize("ScreenSize");
		float fScreenWidth = (float)nScreenWidth;
		float fScreenHeight = (float)nScreenHeight;
		Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		CShaderMan::s_shDeferredShading->FXSetCSFloat(paramScreenSize, &vParamScreenSize, 1);

		Vec3 sunDir = gEnv->p3DEngine->GetSunDirNormalized();
		static CCryNameR paramSunDir("SunDir");
		Vec4 vParamSunDir(sunDir.x, sunDir.y, sunDir.z, TiledShading_SunDistance);
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
			m_tileTranspLightMaskBuf.                 GetDevBuffer ()->LookupUAV(EDefaultResourceViews::UnorderedAccess),
			CTexture::s_ptexHDRTarget->               GetDevTexture()->LookupUAV(EDefaultResourceViews::UnorderedAccess),
			CTexture::s_ptexSceneTargetR11G11B10F[0]->GetDevTexture()->LookupUAV(EDefaultResourceViews::UnorderedAccess),
		};
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
		rd->GetDeviceContext().CSSetUnorderedAccessViews(0, 3, pUAV, NULL);

		rd->m_DevMan.Dispatch(dispatchSizeX, dispatchSizeY, 1);
#endif
		SD3DPostEffectsUtils::ShEndPass();
	}

	D3DUAV* pUAVNull[3] = { NULL };
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
	rd->GetDeviceContext().CSSetUnorderedAccessViews(0, 3, pUAVNull, NULL);
#endif

	D3DShaderResource* pSRVNull[13] = { NULL };
	rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pSRVNull, 0, 13);
	rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pSRVNull, 16, 8);

	D3DSamplerState* pNullSamplers[1] = { NULL };
	rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_CS, pNullSamplers, 0, 1);

	rd->FX_PopRenderTarget(0);

	rd->m_RP.m_FlagsShader_RT = nPrevRTFlags;

	// Output debug information
	if (CRenderer::CV_r_DeferredShadingTiledDebug == 1)
	{
		IRenderAuxText::Draw2dLabel(20, 60, 2.0f, Col_Blue, false, "Tiled Shading Debug");
		IRenderAuxText::Draw2dLabel(20, 95, 1.7f, m_numSkippedLights > 0 ? Col_Red : Col_Blue, false, "Skipped Lights: %i", m_numSkippedLights);
		IRenderAuxText::Draw2dLabel(20, 120, 1.7f, Col_Blue, false, "Atlas Updates: %i", m_numAtlasUpdates);
	}

	m_bApplyCaustics = false;  // Reset flag
}

const CTiledShading::SGPUResources CTiledShading::GetTiledShadingResources()
{
	CTiledShading::SGPUResources resources;

	resources.lightCullInfoBuf = &m_lightCullInfoBuf;
	resources.lightShadeInfoBuf = &m_lightShadeInfoBuf;
	resources.tileOpaqueLightMaskBuf = &m_tileOpaqueLightMaskBuf;
	resources.tileTranspLightMaskBuf = &m_tileTranspLightMaskBuf;
	resources.clipVolumeInfoBuf = &m_clipVolumeInfoBuf;
	
	resources.specularProbeAtlas = CRenderer::CV_r_DeferredShadingTiled > 0 ? m_specularProbeAtlas.texArray : CTexture::s_ptexBlack;
	resources.diffuseProbeAtlas = CRenderer::CV_r_DeferredShadingTiled > 0 ? m_diffuseProbeAtlas.texArray : CTexture::s_ptexBlack;
	resources.spotTexAtlas = CRenderer::CV_r_DeferredShadingTiled > 0 ? m_spotTexAtlas.texArray : CTexture::s_ptexBlack;

	return resources;
}

void CTiledShading::BindForwardShadingResources(CShader*, CSubmissionQueue_DX11::SHADER_TYPE shaderType)
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
		rd->m_DevMan.BindSRV(shaderType, ptexGiDiff->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default), 24);
		rd->m_DevMan.BindSRV(shaderType, ptexGiSpec->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default), 25);
		rd->m_DevMan.BindSRV(shaderType, ptexRsmCol->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default), 26);
		rd->m_DevMan.BindSRV(shaderType, ptexRsmNor->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default), 27);

		return;
	}

	D3DShaderResource* pTiledBaseRes[10] = {
		m_tileTranspLightMaskBuf.        GetDevBuffer() ->LookupSRV(EDefaultResourceViews::Default),
		m_lightShadeInfoBuf.             GetDevBuffer() ->LookupSRV(EDefaultResourceViews::Default),
		m_clipVolumeInfoBuf.             GetDevBuffer() ->LookupSRV(EDefaultResourceViews::Default),
		m_specularProbeAtlas.texArray->  GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
		m_diffuseProbeAtlas.texArray->   GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
		m_spotTexAtlas.texArray->        GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
		CTexture::s_ptexRT_ShadowPool->  GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
		CTexture::s_ptexShadowJitterMap->GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
		ptexGiDiff->                     GetDevTexture()->LookupSRV(EDefaultResourceViews::Default),
		ptexGiSpec->                     GetDevTexture()->LookupSRV(EDefaultResourceViews::Default)
	};
	rd->m_DevMan.BindSRV(shaderType, pTiledBaseRes, 17, 10);

	D3DSamplerState* pSamplers[1] = {
		(D3DSamplerState*)CDeviceObjectFactory::LookupSamplerState(EDefaultSamplerStates::LinearCompare).second,
	};
	rd->m_DevMan.BindSampler(shaderType, pSamplers, 14, 1);
}

void CTiledShading::UnbindForwardShadingResources(CSubmissionQueue_DX11::SHADER_TYPE shaderType)
{
	if (!CRenderer::CV_r_DeferredShadingTiled)
		return;

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	D3DShaderResource* pNullViews[10] = { NULL };
	rd->m_DevMan.BindSRV(shaderType, pNullViews, 17, 10);

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
	if(shaderType == CSubmissionQueue_DX11::TYPE_VS)
		CTexture::s_TexStateIDs[eHWSC_Vertex][14] = EDefaultSamplerStates::Unspecified;
	else if (shaderType == CSubmissionQueue_DX11::TYPE_PS)
		CTexture::s_TexStateIDs[eHWSC_Pixel][14] = EDefaultSamplerStates::Unspecified;
	else if (shaderType == CSubmissionQueue_DX11::TYPE_GS)
		CTexture::s_TexStateIDs[eHWSC_Geometry][14] = EDefaultSamplerStates::Unspecified;
	else if (shaderType == CSubmissionQueue_DX11::TYPE_DS)
		CTexture::s_TexStateIDs[eHWSC_Domain][14] = EDefaultSamplerStates::Unspecified;
	else if (shaderType == CSubmissionQueue_DX11::TYPE_HS)
		CTexture::s_TexStateIDs[eHWSC_Hull][14] = EDefaultSamplerStates::Unspecified;
	else if (shaderType == CSubmissionQueue_DX11::TYPE_CS)
		CTexture::s_TexStateIDs[eHWSC_Compute][14] = EDefaultSamplerStates::Unspecified;
}

struct STiledLightCullInfo* CTiledShading::GetTiledLightCullInfo()
{
	return &tileLightsCull[0];
}

struct STiledLightShadeInfo* CTiledShading::GetTiledLightShadeInfo()
{
	return &tileLightsShade[0];
}

uint32 CTiledShading::GetLightShadeIndexBySpecularTextureId(int textureId) const
{
	CTexture* pTexture = CTexture::GetByID(textureId);
	auto begin = tileLightsShade;
	auto end = tileLightsShade + m_numValidLights;
	auto it = std::find_if(begin, end, [this, pTexture](const STiledLightShadeInfo& light)
	{
		if (light.lightType != tlTypeProbe)
			return false;
		return m_specularProbeAtlas.items[light.resIndex].texture == pTexture;
	});
	if (it == end)
		return -1;
	return uint32(it - begin);
}
