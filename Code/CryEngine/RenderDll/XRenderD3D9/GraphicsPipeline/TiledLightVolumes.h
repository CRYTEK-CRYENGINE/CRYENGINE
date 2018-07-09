// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/ComputeRenderPass.h"
#include "Common/FullscreenPass.h"

constexpr uint32 MaxNumTileLights = 255;

// Sun area light parameters (same as in standard deferred shading)
constexpr float TiledShading_SunDistance = 10000.0f;
constexpr float TiledShading_SunSourceDiameter = 94.0f;  // atan(AngDiameterSun) * 2 * SunDistance, where AngDiameterSun=0.54deg

class CVolumetricFogStage;
class CSvoRenderer;

class CTiledLightVolumesStage : public CGraphicsPipelineStage
{
	struct TextureAtlas;
	struct STiledLightInfo;
	struct STiledLightCullInfo;
	struct STiledLightShadeInfo;

public:
	CTiledLightVolumesStage();
	~CTiledLightVolumesStage();

	uint32                GetDispatchSizeX() const { return m_dispatchSizeX; }
	uint32                GetDispatchSizeY() const { return m_dispatchSizeY; }

	CGpuBuffer*           GetTiledTranspLightMaskBuffer() { return &m_tileTranspLightMaskBuf; }
	CGpuBuffer*           GetTiledOpaqueLightMaskBuffer() { return &m_tileOpaqueLightMaskBuf; }
	CGpuBuffer*           GetLightCullInfoBuffer() { return &m_lightCullInfoBuf; }
	CGpuBuffer*           GetLightShadeInfoBuffer() { return &m_lightShadeInfoBuf; }
	CTexture*             GetProjectedLightAtlas() const { return m_spotTexAtlas.texArray; }
	CTexture*             GetSpecularProbeAtlas() const { return m_specularProbeAtlas.texArray; }
	CTexture*             GetDiffuseProbeAtlas() const { return m_diffuseProbeAtlas.texArray; }

	STiledLightInfo*      GetTiledLightInfo() { return m_tileLights; }
	STiledLightCullInfo*  GetTiledLightCullInfo() { return &tileLightsCull[0]; }
	STiledLightShadeInfo* GetTiledLightShadeInfo() { return &tileLightsShade[0]; }
	uint32                GetValidLightCount() { return m_numValidLights; }

	bool                  IsCausticsVisible() const { return m_bApplyCaustics; }
	void                  NotifyCausticsVisible() { m_bApplyCaustics = true; }
	void                  NotifyCausticsInvisible() { m_bApplyCaustics = false; }

	void Init() final;
	void Resize(int renderWidth, int renderHeight) final;
	void Clear();
	void Destroy(bool destroyResolutionIndependentResources);
	void Update() final;

	void GenerateLightList();
	void Execute();

	bool IsSeparateVolumeListGen();

	// Cubemap Array(s) ==================================================================

	// #PFX2_TODO overly specific function. Re-implement as an algorithm.
	uint32 GetLightShadeIndexBySpecularTextureId(int textureId) const;

private:
	// Cubemap Array(s) ==================================================================

	int  InsertTexture(CTexture* texture, float mipFactor, TextureAtlas& atlas, int arrayIndex);
	void UploadTextures(TextureAtlas& atlas);
	std::pair<size_t, size_t> MeasureTextures(TextureAtlas& atlas);

	// Tiled Light Volumes ===============================================================

	void GenerateLightVolumeInfo();

	void ExecuteVolumeListGen(uint32 dispatchSizeX, uint32 dispatchSizeY);

	void InjectSunIntoTiledLights(uint32_t& counter);

private:
	typedef _smart_ptr<CTexture> TexSmartPtr;

	friend class CVolumetricFogStage;
	friend class CSvoRenderer;

	// Cubemap Array(s) ==================================================================

	struct AtlasItem
	{
		static constexpr uint8 highestMip = 100;
		static constexpr float mipFactorMinSize = highestMip * highestMip;

		TexSmartPtr texture;
		int         updateFrameID;
		int         accessFrameID;
		float       mipFactorRequested;
		uint8       lowestTransferedMip;
		uint8       lowestRenderableMip;
		bool        invalid;

		AtlasItem() : texture(nullptr), updateFrameID(-1), accessFrameID(0),
			lowestTransferedMip(highestMip), lowestRenderableMip(highestMip),
			mipFactorRequested(mipFactorMinSize), invalid(false) {}
	};

	struct TextureAtlas
	{
		TexSmartPtr            texArray;
		std::vector<AtlasItem> items;

		TextureAtlas() : texArray(nullptr) {}
	};

	// Tiled Light Lists =================================================================
	// The following structs and constants have to match the shader code

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
		uint32 miscFlag;
		Vec2   depthBounds;
		Vec4   posRad;
		Vec4   volumeParams0;
		Vec4   volumeParams1;
		Vec4   volumeParams2;
	};  // 80 bytes

	struct STiledLightShadeInfo
	{
		static constexpr uint16 resNoIndex = 0xFFFF;
		static constexpr uint8  resMipLimit = 0x00;

		uint32   lightType;
		uint16   resIndex;
		uint8    resMipClamp0;
		uint8    resMipClamp1;
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

	// Tiled Light Volumes ===============================================================

	enum ETiledVolumeTypes
	{
		tlVolumeSphere = 1,
		tlVolumeCone   = 2,
		tlVolumeOBB    = 3,
		tlVolumeSun    = 4,
	};

	struct STiledLightVolumeInfo
	{
		Matrix44 worldMat;
		Vec4     volumeTypeInfo;
		Vec4     volumeParams0;
		Vec4     volumeParams1;
		Vec4     volumeParams2;
		Vec4     volumeParams3;
	};

	enum EVolumeTypes
	{
		eVolumeType_Box,
		eVolumeType_Sphere,
		eVolumeType_Cone,

		eVolumeType_Count
	};
	
	struct SVolumeGeometry
	{
		CGpuBuffer       vertexDataBuf;
		buffer_handle_t  vertexBuffer;
		buffer_handle_t  indexBuffer;
		uint32           numVertices;
		uint32           numIndices;
	};

private:
	uint32                m_dispatchSizeX, m_dispatchSizeY;

	CGpuBuffer            m_lightCullInfoBuf;
	CGpuBuffer            m_lightShadeInfoBuf;
	CGpuBuffer            m_tileOpaqueLightMaskBuf;
	CGpuBuffer            m_tileTranspLightMaskBuf;

	uint32                m_numValidLights;
	uint32                m_numSkippedLights;
	uint32                m_numAtlasUpdates;
	uint32                m_numAtlasEvictions;

	bool                  m_bApplyCaustics;

	// Cubemap Array(s) ==================================================================

	TextureAtlas          m_specularProbeAtlas;
	TextureAtlas          m_diffuseProbeAtlas;
	TextureAtlas          m_spotTexAtlas;

	// Tiled Light Lists =================================================================

	STiledLightInfo       m_tileLights[MaxNumTileLights];

	STiledLightCullInfo*  tileLightsCull;
	STiledLightShadeInfo* tileLightsShade;

	// Tiled Light Volumes ===============================================================

	CGpuBuffer            m_lightVolumeInfoBuf;

	SVolumeGeometry       m_volumeMeshes[eVolumeType_Count];

	CFullscreenPass       m_passCopyDepth;
	CPrimitiveRenderPass  m_passLightVolumes;
	CRenderPrimitive      m_volumePasses[eVolumeType_Count * 2];  // Inside and outside of volume for each type
	uint32                m_numVolumesPerPass[eVolumeType_Count * 2];
};
