// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================

   Revision history:
   Created Jan 2013 by Nicolas Schulz

   =============================================================================*/

#ifndef _D3DTILEDSHADING_H_
#define _D3DTILEDSHADING_H_

#include "GraphicsPipeline/Common/ComputeRenderPass.h"
#include "DeviceManager/D3D11/DeviceSubmissionQueue_D3D11.h" // CSubmissionQueue_DX11

const uint32 MaxNumTileLights = 255;
const uint32 LightTileSizeX = 8;
const uint32 LightTileSizeY = 8;

// Sun area light parameters (same as in standard deferred shading)
const float TiledShading_SunDistance = 10000.0f;
const float TiledShading_SunSourceDiameter = 94.0f;  // atan(AngDiameterSun) * 2 * SunDistance, where AngDiameterSun=0.54deg

struct STiledLightInfo
{
	uint32 lightType;
	uint32 volumeType;
	Vec2   depthBoundsVS;
	Vec4   posRad;
	Vec4   volumeParams0;
	Vec4   volumeParams1;
	Vec4   volumeParams2;
};

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

struct STiledLightShadeInfo
{
	uint32   lightType;
	uint32   resIndex;
	uint32   shadowMaskIndex;
	uint16   stencilID0;
	uint16   stencilID1;
	Vec4     posRad;
	Vec2     attenuationParams;
	Vec2     shadowParams;
	Vec4     color;
	Matrix44 projectorMatrix;
	Matrix44 shadowMatrix;
};  // 192 bytes

class CTiledShading
{
protected:
	friend class CTiledShadingStage;
	friend class CSceneForwardStage;
	friend class CMobileCompositionStage;
	friend class CWaterStage;

	struct AtlasItem
	{
		ITexture* texture;
		int       updateFrameID;
		int       accessFrameID;
		bool      invalid;

		AtlasItem() : texture(NULL), updateFrameID(-1), accessFrameID(0), invalid(false) {}
	};

	struct TextureAtlas
	{
		CTexture*              texArray;
		std::vector<AtlasItem> items;

		TextureAtlas() : texArray(NULL) {}
	};

public:
	enum ETiledVolumeTypes
	{
		tlVolumeSphere = 1,
		tlVolumeCone   = 2,
		tlVolumeOBB    = 3,
		tlVolumeSun    = 4,
	};

	struct SGPUResources
	{
		CGpuBuffer*  lightCullInfoBuf;
		CGpuBuffer*  lightShadeInfoBuf;
		CGpuBuffer*  tileOpaqueLightMaskBuf;
		CGpuBuffer*  tileTranspLightMaskBuf;
		CGpuBuffer*  clipVolumeInfoBuf;
		CTexture*    specularProbeAtlas;
		CTexture*    diffuseProbeAtlas;
		CTexture*    spotTexAtlas;
	};
	
	CTiledShading();

	void                  CreateResources();
	void                  DestroyResources(bool destroyResolutionIndependentResources);
	void                  Clear();

	void                  Render(CRenderView* pRenderView, Vec4* clipVolumeParams);

	void                  BindForwardShadingResources(CShader* pShader, CSubmissionQueue_DX11::SHADER_TYPE shType = CSubmissionQueue_DX11::TYPE_PS);
	void                  UnbindForwardShadingResources(CSubmissionQueue_DX11::SHADER_TYPE shType = CSubmissionQueue_DX11::TYPE_PS);

	STiledLightInfo*      GetTiledLightInfo()                                                  { return m_tileLights; }
	STiledLightCullInfo*  GetTiledLightCullInfo();
	STiledLightShadeInfo* GetTiledLightShadeInfo();
	uint32                GetValidLightCount()                                                 { return m_numValidLights; }
	const SGPUResources   GetTiledShadingResources();

	int                   InsertTextureToSpecularProbeAtlas(CTexture* texture, int arrayIndex) { return InsertTexture(texture, m_specularProbeAtlas, arrayIndex); }
	int                   InsertTextureToDiffuseProbeAtlas(CTexture* texture, int arrayIndex)  { return InsertTexture(texture, m_diffuseProbeAtlas, arrayIndex); }
	int                   InsertTextureToSpotTexAtlas(CTexture* texture, int arrayIndex)       { return InsertTexture(texture, m_spotTexAtlas, arrayIndex); }

	void                  NotifyCausticsVisible()                                              { m_bApplyCaustics = true; }

	// #PFX2_TODO overly specific function. Re-implement as an algorithm.
	uint32                GetLightShadeIndexBySpecularTextureId(int textureId) const;

protected:
	int  InsertTexture(CTexture* texture, TextureAtlas& atlas, int arrayIndex);
	void PrepareLightList(CRenderView* pRenderView);
	void PrepareClipVolumeList(Vec4* clipVolumeParams);

protected:
	STiledLightInfo m_tileLights[MaxNumTileLights];
	
	uint32         m_dispatchSizeX, m_dispatchSizeY;

	CGpuBuffer     m_lightCullInfoBuf;
	CGpuBuffer     m_lightShadeInfoBuf;
	CGpuBuffer     m_tileOpaqueLightMaskBuf;
	CGpuBuffer     m_tileTranspLightMaskBuf;

	CGpuBuffer     m_clipVolumeInfoBuf;

	TextureAtlas   m_specularProbeAtlas;
	TextureAtlas   m_diffuseProbeAtlas;
	TextureAtlas   m_spotTexAtlas;

	uint32         m_numValidLights;
	uint32         m_numSkippedLights;
	uint32         m_numAtlasUpdates;

	TArray<uint32> m_arrShadowCastingLights;

	bool           m_bApplyCaustics;
};

#endif
