// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "TiledLightVolumes.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3D_SVO.h"

#include "OmniCamera.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32 LightTileSizeX = 8;
const uint32 LightTileSizeY = 8;

const uint32 AtlasArrayDim = 64;
const uint32 SpotTexSize = 512;
const uint32 DiffuseProbeSize = 32;
const uint32 SpecProbeSize = 256;
const uint32 ShadowSampleCount = 16;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct HLSL_VolumeLightListGenConstants
{
	Matrix44 matViewProj;
	uint32   lightIndexOffset, numVertices;
	Vec4     screenScale;
	Vec4     viewerPos;
	Vec4     worldBasisX;
	Vec4     worldBasisY;
	Vec4     worldBasisZ;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CTiledLightVolumesStage::CTiledLightVolumesStage()
{
	// 16 byte alignment is important for performance on nVidia cards
	static_assert(sizeof(STiledLightVolumeInfo) % 16 == 0, "STiledLightVolumeInfo should be 16 byte aligned for GPU performance");
	static_assert(MaxNumTileLights <= 256 && LightTileSizeX == 8 && LightTileSizeY == 8, "Volumes don't support other settings");

	static_assert(sizeof(STiledLightCullInfo) % 16 == 0, "STiledLightCullInfo should be 16 byte aligned for GPU performance");
	static_assert(sizeof(STiledLightShadeInfo) % 16 == 0, "STiledLightShadeInfo should be 16 byte aligned for GPU performance");

	tileLightsCull = reinterpret_cast<STiledLightCullInfo*>(CryModuleMemalign(sizeof(STiledLightCullInfo) * MaxNumTileLights, CDeviceBufferManager::GetBufferAlignmentForStreaming()));
	tileLightsShade = reinterpret_cast<STiledLightShadeInfo*>(CryModuleMemalign(sizeof(STiledLightShadeInfo) * MaxNumTileLights, CDeviceBufferManager::GetBufferAlignmentForStreaming()));

	m_dispatchSizeX = 0;
	m_dispatchSizeY = 0;

	m_numAtlasUpdates = 0;
	m_numValidLights = 0;
	m_numSkippedLights = 0;

	m_bApplyCaustics = false;
}

CTiledLightVolumesStage::~CTiledLightVolumesStage()
{
	Destroy(true);
}


void CTiledLightVolumesStage::Init()
{
	// Cubemap Array(s) ==================================================================

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

	// Tiled Light Lists =================================================================

	if (!m_lightCullInfoBuf.IsAvailable())
	{
		m_lightCullInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightCullInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	}

	if (!m_lightShadeInfoBuf.IsAvailable())
	{
		m_lightShadeInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightShadeInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	}

	// Tiled Light Volumes ===============================================================

	m_lightVolumeInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightVolumeInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	
	// Create geometry for light volumes
	{
		CD3D9Renderer* pRenderer = gcpRendD3D;
		t_arrDeferredMeshVertBuff vertices;
		t_arrDeferredMeshIndBuff indices;
		Vec4 vertexData[256];
		
		for (uint32 i = 0; i < eVolumeType_Count; i++)
		{
			switch (i)
			{
			case eVolumeType_Box:
				CDeferredRenderUtils::CreateUnitBox(indices, vertices);
				for (uint32 i = 0; i < vertices.size(); i++)
				{
					vertices[i].xyz = vertices[i].xyz * 2.0f - Vec3(1.0f, 1.0f, 1.0f);  // Centered box
				}
				break;
			case eVolumeType_Sphere:
				CDeferredRenderUtils::CreateUnitSphere(0, indices, vertices);
				break;
			case eVolumeType_Cone:
				CDeferredRenderUtils::CreateUnitSphere(0, indices, vertices);  // Currently we use a sphere proxy for cones
				break;
			}

			assert(vertices.size() < CRY_ARRAY_COUNT(vertexData));
			
			SVolumeGeometry& volumeMesh = m_volumeMeshes[i];

			volumeMesh.numIndices = indices.size();
			volumeMesh.numVertices = vertices.size();
			indices.resize(volumeMesh.numIndices * 256);
			for (uint32 i = 0; i < 256; i++)
			{
				for (uint32 j = 0; j < volumeMesh.numIndices; j++)
				{
					indices[i * volumeMesh.numIndices + j] = indices[j] + i * volumeMesh.numVertices;
				}
			}

			volumeMesh.indexBuffer  = pRenderer->m_DevBufMan.Create(BBT_INDEX_BUFFER , BU_STATIC, volumeMesh.numIndices  * sizeof(uint16) * 256);
			volumeMesh.vertexBuffer = pRenderer->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, volumeMesh.numVertices * sizeof(vertices[0]));

			pRenderer->m_DevBufMan.UpdateBuffer(volumeMesh.indexBuffer , indices);
			pRenderer->m_DevBufMan.UpdateBuffer(volumeMesh.vertexBuffer, vertices);

			for (uint32 i = 0; i < volumeMesh.numVertices; i++)
			{
				vertexData[i] = Vec4(vertices[i].xyz, 1);
			}
			volumeMesh.vertexDataBuf.Create(volumeMesh.numVertices, sizeof(Vec4), DXGI_FORMAT_R32G32B32A32_FLOAT, CDeviceObjectFactory::BIND_SHADER_RESOURCE, vertexData);
		}
	}

	for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_volumePasses); i++)
	{
		m_volumePasses[i].AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerBatch, sizeof(HLSL_VolumeLightListGenConstants), EShaderStage_Vertex | EShaderStage_Pixel);
	}
}

void CTiledLightVolumesStage::Resize(int renderWidth, int renderHeight)
{
	// Tiled Light Lists =================================================================

	uint32 dispatchSizeX = renderWidth  / LightTileSizeX + (renderWidth  % LightTileSizeX > 0 ? 1 : 0);
	uint32 dispatchSizeY = renderHeight / LightTileSizeY + (renderHeight % LightTileSizeY > 0 ? 1 : 0);

	if (m_dispatchSizeX == dispatchSizeX &&
		m_dispatchSizeY == dispatchSizeY)
		return;

	Destroy(false);

	m_dispatchSizeX = dispatchSizeX;
	m_dispatchSizeY = dispatchSizeY;

	if (!m_tileOpaqueLightMaskBuf.IsAvailable())
	{
		m_tileOpaqueLightMaskBuf.Create(dispatchSizeX * dispatchSizeY * 8, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, NULL);
	}

	if (!m_tileTranspLightMaskBuf.IsAvailable())
	{
		m_tileTranspLightMaskBuf.Create(dispatchSizeX * dispatchSizeY * 8, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, NULL);
	}

}

void CTiledLightVolumesStage::Clear()
{
	// Cubemap Array(s) ==================================================================

	for (uint32 i = 0, si = m_specularProbeAtlas.items.size(); i < si; ++i)
		m_specularProbeAtlas.items[i] = AtlasItem();
	for (uint32 i = 0, si = m_diffuseProbeAtlas.items.size(); i < si; ++i)
		m_diffuseProbeAtlas.items[i] = AtlasItem();
	for (uint32 i = 0, si = m_spotTexAtlas.items.size(); i < si; ++i)
		m_spotTexAtlas.items[i] = AtlasItem();

	m_numAtlasUpdates = 0;
	m_numSkippedLights = 0;
}


void CTiledLightVolumesStage::Destroy(bool destroyResolutionIndependentResources)
{
//	Clear();

	// Cubemap Array(s) ==================================================================

	if (destroyResolutionIndependentResources)
	{
		m_specularProbeAtlas.items.clear();
		m_diffuseProbeAtlas.items.clear();
		m_spotTexAtlas.items.clear();

		SAFE_RELEASE_FORCE(m_specularProbeAtlas.texArray);
		SAFE_RELEASE_FORCE(m_diffuseProbeAtlas.texArray);
		SAFE_RELEASE_FORCE(m_spotTexAtlas.texArray);
	}

	// Tiled Light Lists =================================================================

	m_dispatchSizeX = 0;
	m_dispatchSizeY = 0;

	if (destroyResolutionIndependentResources)
	{
		m_lightCullInfoBuf.Release();
		m_lightShadeInfoBuf.Release();
	}

	m_tileOpaqueLightMaskBuf.Release();
	m_tileTranspLightMaskBuf.Release();

	// Tiled Light Volumes ===============================================================

	if (destroyResolutionIndependentResources)
	{
		for (uint32 i = 0; i < eVolumeType_Count; i++)
		{
			gRenDev->m_DevBufMan.Destroy(m_volumeMeshes[i].vertexBuffer);
			gRenDev->m_DevBufMan.Destroy(m_volumeMeshes[i].indexBuffer);
		}
	}
}


void CTiledLightVolumesStage::Update()
{
	// Tiled Light Volumes ===============================================================

	if (CRenderer::CV_r_DeferredShadingTiled >= 3)
	{
		uint clearNull[4] = { 0 };
		GetDeviceObjectFactory().GetCoreCommandList().GetComputeInterface()->ClearUAV(m_tileOpaqueLightMaskBuf.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), clearNull, 0, nullptr);
		GetDeviceObjectFactory().GetCoreCommandList().GetComputeInterface()->ClearUAV(m_tileTranspLightMaskBuf.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), clearNull, 0, nullptr);
	}
}


void CTiledLightVolumesStage::GenerateLightVolumeInfo()
{
	const uint32 kNumPasses = eVolumeType_Count * 2;
	uint16 volumeLightIndices[kNumPasses][MaxNumTileLights];

	for (uint32 i = 0; i < kNumPasses; i++)
		m_numVolumesPerPass[i] = 0;

	CD3D9Renderer* pRenderer = gcpRendD3D;
	STiledLightInfo* pLightInfo = GetTiledLightInfo();

	const SRenderViewInfo& viewInfo = GetCurrentViewInfo();

	Vec3 camPos = viewInfo.cameraOrigin;

	int screenWidth = GetViewport().width;
	int screenHeight = GetViewport().height;
	float tileCoverageX = (float)LightTileSizeX / (float)screenWidth;
	float tileCoverageY = (float)LightTileSizeY / (float)screenHeight;
	float camFovScaleX = 0.5f * viewInfo.projMatrix.m00;
	float camFovScaleY = 0.5f * viewInfo.projMatrix.m11;

	// Classify volume type
	uint32 numLights = GetValidLightCount();
	for (uint32 lightIdx = 0; lightIdx < numLights; ++lightIdx)
	{
		STiledLightInfo& lightInfo = pLightInfo[lightIdx];

		bool bInsideVolume = false;
		Vec3 lightPos = Vec3(lightInfo.posRad);
		if (lightPos.GetDistance(camPos) < lightInfo.posRad.w * 1.25f)
		{
			bInsideVolume = true;
		}

		uint32 pass = eVolumeType_Sphere;
		if (lightInfo.volumeType == tlVolumeOBB)
			pass = eVolumeType_Box;
		else if (lightInfo.volumeType == tlVolumeCone)
			pass = eVolumeType_Cone;

		pass += bInsideVolume ? 0 : eVolumeType_Count;
		volumeLightIndices[pass][m_numVolumesPerPass[pass]++] = lightIdx;
	}

	// Prepare buffer with light volume info
	uint32 curIndex = 0;
	STiledLightVolumeInfo volumeInfo[MaxNumTileLights];
	for (uint32 i = 0; i < kNumPasses; i++)
	{
		bool bInsideVolume = i < eVolumeType_Count;

		for (uint32 j = 0; j < m_numVolumesPerPass[i]; j++)
		{
			STiledLightInfo& lightInfo = pLightInfo[volumeLightIndices[i][j]];

			// To ensure conservative rasterization, we need to enlarge the projected volume by at least half a tile
			// Compute the size of a tile in view space at the position of the volume
			//     ScreenCoverage = FovScale * ObjectSize / Distance  =>  ObjectSize = ScreenCoverage * Distance / FovScale
			float distance = lightInfo.depthBoundsVS.y * viewInfo.farClipPlane;
			float tileSizeVS = sqrtf(sqr(tileCoverageX * distance / camFovScaleX) + sqr(tileCoverageY * distance / camFovScaleY));

			Vec3 lightPos = Vec3(lightInfo.posRad);

			Matrix44 worldMat;
			Vec4 volumeParams0(0, 0, 0, 0);
			Vec4 volumeParams1(0, 0, 0, 0);
			Vec4 volumeParams2(0, 0, 0, 0);
			Vec4 volumeParams3(0, 0, 0, 0);

			volumeInfo[curIndex].volumeTypeInfo = Vec4((float)lightInfo.volumeType, 0, 0, (float)volumeLightIndices[i][j]);

			if (lightInfo.volumeType == tlVolumeOBB)
			{
				float minBoxSize = std::min(std::min(lightInfo.volumeParams0.w, lightInfo.volumeParams1.w), lightInfo.volumeParams2.w);
				float volumeScale = (minBoxSize + tileSizeVS) / minBoxSize;

				volumeParams0 = Vec4(Vec3(lightInfo.volumeParams0), lightInfo.volumeParams0.w * volumeScale);
				volumeParams1 = Vec4(Vec3(lightInfo.volumeParams1), lightInfo.volumeParams1.w * volumeScale);
				volumeParams2 = Vec4(Vec3(lightInfo.volumeParams2), lightInfo.volumeParams2.w * volumeScale);
				volumeParams3 = Vec4(volumeParams0.w, volumeParams1.w, volumeParams2.w, 0);

				Matrix33 rotScaleMat(Vec3(volumeParams0) * volumeParams0.w, Vec3(volumeParams1) * volumeParams1.w, Vec3(volumeParams2) * volumeParams2.w);
				Matrix34 unitVolumeToWorldMat = Matrix34::CreateTranslationMat(lightPos) * rotScaleMat;
				worldMat = unitVolumeToWorldMat.GetTransposed();

				volumeParams0 = Vec4(Vec3(volumeParams0), lightPos.x);
				volumeParams1 = Vec4(Vec3(volumeParams1), lightPos.y);
				volumeParams2 = Vec4(Vec3(volumeParams2), lightPos.z);
			}
			else if (lightInfo.volumeType == tlVolumeSun)
			{
				volumeParams0 = Vec4(camPos, viewInfo.farClipPlane - 1.0f);

				Matrix34 unitVolumeToWorldMat = Matrix34::CreateScale(Vec3(volumeParams0.w), camPos);
				worldMat = unitVolumeToWorldMat.GetTransposed();
			}
			else if (lightInfo.volumeType == tlVolumeCone)
			{
				float coneAngle = std::min(lightInfo.volumeParams1.w + 0.02f, PI / 2.0f);
				volumeInfo[curIndex].volumeTypeInfo.y = sinf(coneAngle);
				volumeInfo[curIndex].volumeTypeInfo.z = cosf(coneAngle);

				volumeParams1 = Vec4(Vec3(lightInfo.volumeParams0), cosf(coneAngle));

				Vec3 coneTip = lightPos + Vec3(volumeParams1) * tileSizeVS;
				float volumeScale = (lightInfo.posRad.w + tileSizeVS * 2.0f) / lightInfo.posRad.w;
				volumeParams0 = Vec4(coneTip, lightInfo.posRad.w * volumeScale);

				float volumeScaleLowRes = (lightInfo.posRad.w * 1.2f + (tileSizeVS * 2.0f)) / lightInfo.posRad.w;
				Matrix34 unitVolumeToWorldMat = Matrix34::CreateScale(Vec3(lightInfo.posRad.w * volumeScaleLowRes), coneTip);
				worldMat = unitVolumeToWorldMat.GetTransposed();
			}
			else  // tlVolumeSphere
			{
				float volumeScale = (lightInfo.posRad.w + tileSizeVS) / lightInfo.posRad.w;
				volumeParams0 = Vec4(lightPos, lightInfo.posRad.w * volumeScale);

				float volumeScaleLowRes = (lightInfo.posRad.w * 1.2f + tileSizeVS) / lightInfo.posRad.w;
				Matrix34 unitVolumeToWorldMat = Matrix34::CreateScale(Vec3(lightInfo.posRad.w * volumeScaleLowRes), lightPos);
				worldMat = unitVolumeToWorldMat.GetTransposed();
			}

			volumeInfo[curIndex].worldMat = worldMat;
			volumeInfo[curIndex].volumeParams0 = volumeParams0;
			volumeInfo[curIndex].volumeParams1 = volumeParams1;
			volumeInfo[curIndex].volumeParams2 = volumeParams2;
			volumeInfo[curIndex].volumeParams3 = volumeParams3;
			curIndex += 1;
		}
	}

	m_lightVolumeInfoBuf.UpdateBufferContent(volumeInfo, sizeof(volumeInfo));
}


int CTiledLightVolumesStage::InsertTexture(CTexture* pTexInput, TextureAtlas& atlas, int arrayIndex)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const int frameID = gRenDev->GetRenderFrameID();

	if (pTexInput == NULL)
		return -1;

	// Make sure the texture is loaded already
	if (pTexInput->GetWidth() == 0 && pTexInput->GetHeight() == 0)
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
		pTexInput->GetWidth() < atlas.texArray->GetWidth() ||
		pTexInput->GetWidth() != pTexInput->GetHeight() ||
		pTexInput->GetDstFormat() != atlas.texArray->GetDstFormat() ||
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
		else if (pTexInput->GetDstFormat() != atlas.texArray->GetDstFormat())
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
	uint32 numSrcMips = (uint32)pTexInput->GetNumMips();
	uint32 numDstMips = (uint32)atlas.texArray->GetNumMips();
	uint32 firstSrcMip = IntegerLog2((uint32)pTexInput->GetWidth() / atlas.texArray->GetWidth());
	uint32 numFaces = atlas.texArray->GetTexType() == eTT_CubeArray ? 6 : 1;

	CDeviceTexture* pTexInputDevTex = pTexInput->GetDevTexture();
	CDeviceTexture* pTexArrayDevTex = atlas.texArray->GetDevTexture();

	ASSERT_DEVICE_TEXTURE_IS_FIXED(pTexArrayDevTex);

	UINT w = atlas.texArray->GetWidth();
	UINT h = atlas.texArray->GetHeight();
	UINT d = atlas.texArray->GetDepth();

	if ((numDstMips == numSrcMips) && (firstSrcMip == 0) && 0 && "TODO: kill this branch, after taking a careful look for side-effects")
	{
		// MipMaps are laid out consecutively in the sub-resource specification, which allows us to issue a batch-copy for N sub-resources
		const UINT NumSubresources = numDstMips * numFaces;
		const UINT SrcSubresource = D3D11CalcSubresource(0, 0, numSrcMips);
		const UINT DstSubresource = D3D11CalcSubresource(0, arrayIndex * numFaces, numDstMips);

		const SResourceRegionMapping mapping =
		{
			{ 0, 0, 0, SrcSubresource }, // src position
			{ 0, 0, 0, DstSubresource }, // dst position
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
				{ 0, 0, 0, SrcSubresource }, // src position
				{ 0, 0, 0, DstSubresource }, // dst position
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


void CTiledLightVolumesStage::GenerateLightList()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CRenderView* pRenderView = RenderView();

	const auto& defLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	const auto& envProbes = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	const auto& ambientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

	const SRenderViewInfo& viewInfo = pRenderView->GetViewInfo(CCamera::eEye_Left);

	const float invCameraFar = 1.0f / viewInfo.farClipPlane;

	// Prepare view matrix with flipped z-axis
	Matrix44A matView = viewInfo.viewMatrix;
	matView.m02 *= -1;
	matView.m12 *= -1;
	matView.m22 *= -1;
	matView.m32 *= -1;

	int nThreadID = gRenDev->GetRenderThreadID();

	uint32 firstShadowLight = CDeferredShading::Instance().m_nFirstShadowPoolLight;
	uint32 curShadowPoolLight = CDeferredShading::Instance().m_nLastShadowPoolLight;

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
							rd->ConfigShadowTexgen(pRenderView, 0, &firstFrustum, side);
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
	if (pRenderView->HaveSunLight())
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
#if defined(ENABLE_PROFILING_CODE)
	SRenderStatistics::Write().m_NumTiledShadingSkippedLights = m_numSkippedLights;
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

	// Output debug information
	if (CRenderer::CV_r_DeferredShadingTiledDebug == 1)
	{
		IRenderAuxText::Draw2dLabel(20, 60, 2.0f, Col_Blue, false, "Tiled Shading Debug");
		IRenderAuxText::Draw2dLabel(20, 95, 1.7f, m_numSkippedLights > 0 ? Col_Red : Col_Blue, false, "Skipped Lights: %i", m_numSkippedLights);
		IRenderAuxText::Draw2dLabel(20, 120, 1.7f, Col_Blue, false, "Atlas Updates: %i", m_numAtlasUpdates);
	}
}


bool CTiledLightVolumesStage::IsSeparateVolumeListGen()
{
	return !(CRenderer::CV_r_DeferredShadingTiled < 3 && !CRenderer::CV_r_GraphicsPipelineMobile) && !gcpRendD3D->GetGraphicsPipeline().GetOmniCameraStage()->IsEnabled();
}


void CTiledLightVolumesStage::ExecuteVolumeListGen(uint32 dispatchSizeX, uint32 dispatchSizeY)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	CTexture* pDepthRT = CRendererResources::s_ptexSceneDepthScaled[2];
	assert(CRendererCVars::CV_r_VrProjectionType > 0 || (pDepthRT->GetWidth() == dispatchSizeX && pDepthRT->GetHeight() == dispatchSizeY));

	{
		PROFILE_LABEL_SCOPE("COPY_DEPTH_EIGHTH");

		static CCryNameTSCRC techCopy("CopyToDeviceDepth");

		m_passCopyDepth.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passCopyDepth.SetTechnique(CShaderMan::s_shPostEffects, techCopy, 0);
		m_passCopyDepth.SetRequirePerViewConstantBuffer(true);
		m_passCopyDepth.SetDepthTarget(pDepthRT);
		m_passCopyDepth.SetState(GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);
		m_passCopyDepth.SetTexture(0, CRendererResources::s_ptexLinearDepthScaled[2]);
		
		m_passCopyDepth.BeginConstantUpdate();
		m_passCopyDepth.Execute();
	}

	PROFILE_LABEL_SCOPE("TILED_VOLUMES");

	GenerateLightVolumeInfo();
	
	SRenderViewInfo viewInfo[2];
	size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);
	
	Matrix44 matViewProj[2];
	Vec4 worldBasisX[2], worldBasisY[2], worldBasisZ[2], viewerPos[2];
	for (int i = 0; i < viewInfoCount; ++i)
	{
		matViewProj[i] = viewInfo[i].viewMatrix * viewInfo[i].projMatrix;

		Vec4r wBasisX, wBasisY, wBasisZ, camPos;
		CShadowUtils::ProjectScreenToWorldExpansionBasis(
			Matrix44(IDENTITY), *viewInfo[i].pCamera, RenderView()->m_vProjMatrixSubPixoffset,
			(float)pDepthRT->GetWidth(), (float)pDepthRT->GetHeight(), wBasisX, wBasisY, wBasisZ, camPos, true);

		worldBasisX[i] = wBasisX;
		worldBasisY[i] = wBasisY;
		worldBasisZ[i] = wBasisZ;
		viewerPos[i]   = camPos;
	}
	
	{
		D3DViewPort viewport;
		viewport.TopLeftX = viewport.TopLeftY = 0.0f;
		viewport.Width = (float)pDepthRT->GetWidth();
		viewport.Height = (float)pDepthRT->GetHeight();
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		
		m_passLightVolumes.SetDepthTarget(pDepthRT);
		m_passLightVolumes.SetViewport(viewport);
		m_passLightVolumes.SetOutputUAV(0, &m_tileOpaqueLightMaskBuf);
		m_passLightVolumes.SetOutputUAV(1, &m_tileTranspLightMaskBuf);
		m_passLightVolumes.BeginAddingPrimitives();
		
		uint32 curIndex = 0;
		for (uint32 pass = 0; pass < eVolumeType_Count * 2; pass++)
		{
			uint32 volumeType = pass % eVolumeType_Count;
			bool bInsideVolume = pass < eVolumeType_Count;
			
			if (m_numVolumesPerPass[pass] > 0)
			{
				CRenderPrimitive& primitive = m_volumePasses[pass];

				static CCryNameTSCRC techLightVolume("VolumeLightListGen");
				primitive.SetTechnique(CShaderMan::s_shDeferredShading, techLightVolume, 0);
				primitive.SetRenderState(bInsideVolume ? GS_NODEPTHTEST : GS_DEPTHFUNC_GEQUAL);
				primitive.SetEnableDepthClip(!bInsideVolume);
				primitive.SetCullMode(bInsideVolume ? eCULL_Front : eCULL_Back);
				primitive.SetTexture(3, CRendererResources::s_ptexLinearDepthScaled[2], EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel);
				primitive.SetBuffer(1, &m_lightVolumeInfoBuf, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel);

				SVolumeGeometry& volumeMesh = m_volumeMeshes[volumeType];
				uint32 numIndices = volumeMesh.numIndices;
				uint32 numVertices = volumeMesh.numVertices;
				uint32 numInstances = m_numVolumesPerPass[pass];
			
				primitive.SetCustomVertexStream(volumeMesh.vertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
				primitive.SetCustomIndexStream(volumeMesh.indexBuffer, Index16);
				primitive.SetDrawInfo(eptTriangleList, 0, 0, numIndices * numInstances);
				primitive.SetBuffer(0, &volumeMesh.vertexDataBuf, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel);
				primitive.Compile(m_passLightVolumes);

				{
					auto constants = primitive.GetConstantManager().BeginTypedConstantUpdate<HLSL_VolumeLightListGenConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Vertex | EShaderStage_Pixel);

					constants->screenScale = Vec4((float)pDepthRT->GetWidth(), (float)pDepthRT->GetHeight(), 0, 0);

					float zn = viewInfo[0].nearClipPlane;
					float zf = viewInfo[0].farClipPlane;
					constants->lightIndexOffset = curIndex;
					constants->numVertices = numVertices;

					constants->matViewProj = matViewProj[0];
					constants->viewerPos   = viewerPos[0];
					constants->worldBasisX = worldBasisX[0];
					constants->worldBasisY = worldBasisY[0];
					constants->worldBasisZ = worldBasisZ[0];

					if (viewInfoCount > 1)
					{
						constants.BeginStereoOverride(true);
						constants->matViewProj = matViewProj[1];
						constants->viewerPos   = viewerPos[1];
						constants->worldBasisX = worldBasisX[1];
						constants->worldBasisY = worldBasisY[1];
						constants->worldBasisZ = worldBasisZ[1];
					}

					primitive.GetConstantManager().EndTypedConstantUpdate(constants);
				}
				
				m_passLightVolumes.AddPrimitive(&primitive);
			}

			curIndex += m_numVolumesPerPass[pass];
		}
	}

	m_passLightVolumes.Execute();
}


void CTiledLightVolumesStage::Execute()
{
	PROFILE_LABEL_SCOPE("TILED_LIGHT_VOLUMES");

	int screenWidth  = GetViewport().width;
	int screenHeight = GetViewport().height;
	int gridWidth  = screenWidth;
	int gridHeight = screenHeight;

	if (CVrProjectionManager::IsMultiResEnabledStatic())
		CVrProjectionManager::Instance()->GetProjectionSize(screenWidth, screenHeight, gridWidth, gridHeight);

	uint32 dispatchSizeX = gridWidth  / LightTileSizeX + (gridWidth  % LightTileSizeX > 0 ? 1 : 0);
	uint32 dispatchSizeY = gridHeight / LightTileSizeY + (gridHeight % LightTileSizeY > 0 ? 1 : 0);

	bool bSeparateCullingPass = IsSeparateVolumeListGen();

	if (bSeparateCullingPass)
	{
		ExecuteVolumeListGen(dispatchSizeX, dispatchSizeY);
	}
}

uint32 CTiledLightVolumesStage::GetLightShadeIndexBySpecularTextureId(int textureId) const
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
