// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VolumetricClouds.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "GraphicsPipeline/VolumetricFog.h"
#include "Common/TypedConstantBuffer.h"

//////////////////////////////////////////////////////////////////////////
//#define ENABLE_DETAILED_PROFILING
//#define ENABLE_FULL_SIZE_FOG

//////////////////////////////////////////////////////////////////////////
struct VCCloudRenderContext
{
	CTexture* scaledTarget;
	CTexture* scaledZTarget;
	int32     cloudVolumeTexId;
	int32     cloudNoiseTexId;
	int32     edgeNoiseTexId;
	int32     fullscreenWidth;
	int32     fullscreenHeight;
	int32     screenWidth;
	int32     screenHeight;
	int32     screenDepth;
	bool      stereoReproj;
	bool      bRightEye;
	bool      bReprojectionFromLeftToRight;
	bool      bFog;
	bool      renderFogShadow;
	bool      bVolumetricFog;
	bool      bCloudVolTex;
	bool      bCloudBlocker;
	bool      bScreenSpaceCloudBlocker;
	bool      bFastPath;
	bool      bStereoMultiGPURendering;
};

namespace
{
const static int32 VCVerticalShadowTexSize = 8;
const static f32 VCMinSphereRadius = 100000.0f;
const static f32 VCMaxSphereRadius = 10000000.0f;
const static f32 VCDropletDensity = 1e9f; // 10^8 to 10^9 is typical droplets density in clouds.
const static f32 VCDropletRadius = 0.000015f;// 2 to 15 micro meter is typical droplet size in clouds.
const static f32 VCSigmaE = PI * VCDropletRadius * VCDropletRadius;
const static f32 VCScatterCoefficient = VCDropletDensity * VCSigmaE;
const static Vec3 VCBaseNoiseScale = Vec3(0.00003125f, 0.00003125f, 0.00003125f);
const static f32 VCMinRaymarchStepNum = 8.0f;
const static f32 VCEdgeNoiseScale = 19.876521f;
const static int32 VCFrameCountCycle = 1024;

struct SCloudShadowShaderParam
{
	Vec4 sunLightDirection;    // xyz: sun light direction, w: unused
	Vec4 cloudGenParams;       // x: cloud bottom altitude, y: cloud layer thickness, z: global cloudiness, w: extinction coefficient
	Vec4 tilingParams;         // xy: tiling shadow region size, zw: unused
	Vec4 texParams;            // xyz: shadow voxel size, w: unused
	Vec4 noiseParams;          // xyz: noise scale, w: unused
	Vec4 baseScaleParams;      // xyz: tiling size for base cloud density texture, w: unused
	Vec4 baseOffsetParams;     // xyz: tiling offset, w: unused
	Vec4 densityParams;        // x: global cloud density scale, y: density remap min, z: density remap max, w: additional noise intensity
	Vec4 edgeNoiseScaleParams; // xyz: edge noise scale, w: unused
	Vec4 edgeTurbulenceParams; // x: edge turbulence, y: edge threshold, z: enable edge erode, w: enable abs for edge noise

	Vec4 cloudShadowAnimParams; // PB_CloudShadowAnimParams

	// Cloud blockers
	Vec4 cloudBlockerPos[4];   // xyz: the center position of the cloud blocker, w: number of blockers
	Vec4 cloudBlockerParam[4]; // x: reciprocal of length between decay start and end, y: decay start, z: decay influence, w: one minus decay influence
};

struct SReprojectionParam
{
	Vec4     screenSize;
	Vec4     reprojectionParams; // xy: factors for converting linear depth to non-linear depth, zw: unused
	Matrix44 reprojectionMatrix;
};

// this structure must exactly match VolumetricCloudsConstantBuffer in Clouds.cfx.
struct SVolumetricCloudsShaderParam
{
	Vec3     shadeColorFromSun; // PB_CloudShadingColorSun
	f32      waterLevel;        // PB_WaterLevel

	Vec3     cameraPos; // PF_CameraPos
	f32      nearClipDist;

	Vec3     cameraFrontVector;
	float    farClipDist;

	Vec3     sunLightDirection;
	f32      reciprocalFarClipDist;

	Vec4     cloudShadowAnimParams; // PB_CloudShadowAnimParams

	Vec4     screenSize;
	Vec4     frustumTL;
	Vec4     frustumTR;
	Vec4     frustumBL;

	Vec3     skylightRayleighInScatter;
	f32      skyLightingFactor;

	Vec3     atmosphericScattering;
	f32      sunIntensityForAtmospheric;

	f32      sunSingleScatter;
	f32      sunLowOrderScatter;
	f32      sunLowOrderScatterAnisotropy;
	f32      sunHighOrderScatter;

	Vec3     groundAlbedo;
	f32      groundLightingFactor;

	f32      scatterCoefficient;
	f32      extinction;
	f32      powderFactor;
	f32      shadingLOD;

	f32      altitude;
	f32      thickness;
	f32      cloudiness;
	f32      padding0;

	Vec3     vInvTiling;
	f32      radius;

	f32      multiScatterAttenuation;
	f32      multiScatterPreservation;
	f32      projRatio0;
	f32      projRatio1;

	Vec3     cloudNoiseScale;
	f32      frameCount;

	Vec3     invBaseTexTiling;
	f32      jitteredSamplingPosition;

	Vec3     cloudOffset;
	f32      padding1;

	f32      maxGlobalCloudDensity;
	f32      minRescaleCloudTexDensity;
	f32      maxRescaleCloudTexDensity;
	f32      additionalNoiseIntensity;

	Vec3     edgeTurbulenceNoiseScale;
	f32      padding2;

	f32      edgeTurbulence;
	f32      edgeThreshold;
	f32      edgeNoiseErode;
	f32      edgeNoiseAbs;

	f32      maxViewableDistance;
	f32      maxRayMarchDistance;
	f32      maxRayMarchStep;
	f32      horizonHeight;

	Vec4     cloudBlockerPos[4];
	Vec4     cloudBlockerParam[4];

	Matrix44 leftToRightReprojMatrix;

	// voluemtric fog shadow parameters
	Vec4 volFogShadowDarkening;
	Vec4 volFogShadowDarkeningSunAmb;

	// fog parameters
	Vec4 vfParams;      // PB_VolumetricFogParams
	Vec4 vfRampParams;  // PB_VolumetricFogRampParams
	Vec4 vfSunDir;      // PB_VolumetricFogSunDir
	Vec3 vfColGradBase; // PB_FogColGradColBase
	f32  padding3;
	Vec3 vfColGradDelta; // PB_FogColGradColDelta
	f32  padding4;
	Vec4 vfColGradParams; // PB_FogColGradParams
	Vec4 vfColGradRadial; // PB_FogColGradRadial

	// voxel-based volumetric fog parameters
	Vec4 vfSamplingParams;           // PB_VolumetricFogSamplingParams
	Vec4 vfDistributionParams;       // PB_VolumetricFogDistributionParams
	Vec4 vfScatteringParams;         // PB_VolumetricFogScatteringParams
	Vec4 vfScatteringBlendParams;    // PB_VolumetricFogScatteringBlendParams
	Vec4 vfScatteringColor;          // PB_VolumetricFogScatteringColor
	Vec4 vfScatteringSecondaryColor; // PB_VolumetricFogScatteringSecondaryColor
	Vec4 vfHeightDensityParams;      // PB_VolumetricFogHeightDensityParams
	Vec4 vfHeightDensityRampParams;  // PB_VolumetricFogHeightDensityRampParams
	Vec4 vfDistanceParams;           // PB_VolumetricFogDistanceParams
	Vec4 vfGlobalEnvProbeParams0;    // PB_VolumetricFogGlobalEnvProbe0
	Vec4 vfGlobalEnvProbeParams1;    // PB_VolumetricFogGlobalEnvProbe1
	Vec4 vfTimeParams;               // PB_time

	// upscale parameters
	Vec4 fullScreenSize;
	Vec4 cloudBlockerSSPos;
	Vec4 cloudBlockerSSParam;
};

int32 VCGetDownscaleRatio()
{
	return clamp_tpl<int32>(CRenderer::CV_r_VolumetricClouds, 1, 2); // half and quarter res are available.
}

int32 VCGetDownscaledResolution(int32 resolution, int32 downscaleRatio)
{
	for (int32 i = 0; i < downscaleRatio; ++i)
	{
		resolution = (resolution + 1) / 2;
	}
	return resolution;
}

// calculate Van der Corput sequence
f32 VCGetVdC(uint32 index)
{
	index = (index << 16u) | (index >> 16u);
	index = ((index & 0x55555555u) << 1u) | ((index & 0xAAAAAAAAu) >> 1u);
	index = ((index & 0x33333333u) << 2u) | ((index & 0xCCCCCCCCu) >> 2u);
	index = ((index & 0x0F0F0F0Fu) << 4u) | ((index & 0xF0F0F0F0u) >> 4u);
	index = ((index & 0x00FF00FFu) << 8u) | ((index & 0xFF00FF00u) >> 8u);
	return static_cast<f32>(index) / static_cast<f32>(0xFFFFFFFFu);
}

f32 VCGetCloudBlockerDecayDistance(f32 decayStart, f32 decayEnd)
{
	f32 decayDistance = (decayEnd - decayStart);
	decayDistance = (abs(decayDistance) < 1.0f) ? ((decayDistance < 0.0f) ? -1.0f : 1.0f) : decayDistance;
	return decayDistance;
}

int32 VCGetCloudShadowTexSize()
{
	int32 size = clamp_tpl<int32>(CRenderer::CV_r_VolumetricCloudsShadowResolution, 16, 512);
	size = size - (size % 16); // keep multiple of 16.
	return size;
}

int32 VCGetCloudRaymarchStepNum()
{
	int32 num = clamp_tpl<int32>(CRenderer::CV_r_VolumetricCloudsRaymarchStepNum, 16, 256);
	num = num - (num % 16); // keep multiple of 16.
	return num;
}

uint64 GetRTMask(const VCCloudRenderContext& context)
{
	uint64 rtMask = 0;
	rtMask |= context.bFog ? g_HWSR_MaskBit[HWSR_FOG] : 0;
	rtMask |= context.bVolumetricFog ? g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG] : 0;
	rtMask |= context.renderFogShadow ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
#if 0 // temporarily disabled until these features are implemented.
	rtMask |= context.bUseLightning ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
#endif
	rtMask |= context.bReprojectionFromLeftToRight ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
	rtMask |= context.bCloudVolTex ? g_HWSR_MaskBit[HWSR_SAMPLE3] : 0;
	rtMask |= context.bCloudBlocker ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;
	rtMask |= context.bFastPath ? g_HWSR_MaskBit[HWSR_SAMPLE5] : 0;

	// TODO: remove after old graphics pipeline is removed.
	rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2]; // enables explicit constant buffer.

	return rtMask;
}

Matrix44 VCGetReprojectionMatrix(
  const Matrix44& matViewReprojTarget,
  const Matrix44& matProjReprojTarget,
  const Matrix44& matViewCurrent,
  const Matrix44& matProjCurrent)
{
	// Compute reprojection matrix with highest possible precision to minimize numeric diffusion
	Matrix44_tpl<f64> matReprojection64;

	Matrix44A matProj = matProjCurrent;
	if (gcpRendD3D->GetS3DRend().GetStereoMode() != EStereoMode::STEREO_MODE_DUAL_RENDERING)
	{
		// Ensure jittering is removed from projection matrix
		matProj.m20 = 0.0f;
		matProj.m21 = 0.0f;
	}

	Matrix44_tpl<f64> matViewInv, matProjInv;
	mathMatrixLookAtInverse(&matViewInv, &matViewCurrent);
	const bool bCanInvert = mathMatrixPerspectiveFovInverse(&matProjInv, &matProj);
	CRY_ASSERT(bCanInvert);

	Matrix44_tpl<f64> matScaleBias1 = Matrix44_tpl<f64>(
	  0.5, 0, 0, 0,
	  0, -0.5, 0, 0,
	  0, 0, 1, 0,
	  0.5, 0.5, 0, 1);
	Matrix44_tpl<f64> matScaleBias2 = Matrix44_tpl<f64>(
	  2.0, 0, 0, 0,
	  0, -2.0, 0, 0,
	  0, 0, 1, 0,
	  -1.0, 1.0, 0, 1);

	matReprojection64 = matProjInv * matViewInv * Matrix44_tpl<f64>(matViewReprojTarget) * Matrix44_tpl<f64>(matProjReprojTarget);
	matReprojection64 = matScaleBias2 * matReprojection64 * matScaleBias1;

	Matrix44 result = matReprojection64;
	return result;
}
}

//////////////////////////////////////////////////////////////////////////

bool CVolumetricCloudsStage::IsRenderable()
{
	static ICVar* pClouds = gEnv->pConsole->GetCVar("e_Clouds");
	bool renderClouds = false;
	if (pClouds && (pClouds->GetIVal() != 0))
	{
		renderClouds = true;
	}

	return renderClouds && (CRenderer::CV_r_VolumetricClouds > 0);
}

Vec4 CVolumetricCloudsStage::GetVolumetricCloudShadowParams(const CCamera& camera, const Vec2& cloudOffset, const Vec2& vTiling)
{
	const int32 shadowTexSize = VCGetCloudShadowTexSize();
	const Vec2 texSize((f32)shadowTexSize, (f32)shadowTexSize);
	const Vec2 vInvTiling(1.0f / vTiling.x, 1.0f / vTiling.y);
	const Vec2 voxelLength(vTiling.x / texSize.x, vTiling.y / texSize.y);
	const Vec2 invVoxelLength(1.0f / voxelLength.x, 1.0f / voxelLength.y);

	// don't need to take care of stereo multi-GPU rendering here because of no effect for the result.
	const Vec3& camPos = camera.GetPosition();

	// shift world position along with camera movement, which is aligned to certain interval.
	Vec2 worldAlignmentOffset;
	worldAlignmentOffset.x = std::floor(camPos.x * invVoxelLength.x) * voxelLength.x;
	worldAlignmentOffset.y = std::floor(camPos.y * invVoxelLength.y) * voxelLength.y;

	// offset wind movement to align the position to certain interval.
	f32 intpart;
	worldAlignmentOffset.x -= std::modf(cloudOffset.x * invVoxelLength.x, &intpart) * voxelLength.x;
	worldAlignmentOffset.y -= std::modf(cloudOffset.y * invVoxelLength.y, &intpart) * voxelLength.y;

	return Vec4(worldAlignmentOffset.x, worldAlignmentOffset.y, vInvTiling.x, vInvTiling.y);
}

CVolumetricCloudsStage::CVolumetricCloudsStage()
	: m_cleared(MaxFrameNum)
	, m_tick(-1)
{
	for (int32 e = 0; e < MaxEyeNum; ++e)
	{
		m_nUpdateFrameID[e] = -1;

		for (int32 i = 0; i < MaxFrameNum; ++i)
		{
			m_viewMatrix[e][i].SetIdentity();
			m_projMatrix[e][i].SetIdentity();
		}

		for (int32 i = 0; i < 2; ++i)
		{
			m_pDownscaledMaxTex[e][i] = nullptr;
			m_pDownscaledMinTex[e][i] = nullptr;
		}
		m_pScaledPrevDepthTex[e] = nullptr;
	}

	m_pCloudDepthTex = nullptr;
	m_pDownscaledMaxTempTex = nullptr;
	m_pDownscaledMinTempTex = nullptr;
	m_pDownscaledLeftEyeTex = nullptr;
	m_pCloudDensityTex = nullptr;
	m_pCloudShadowTex = nullptr;

	m_blockerPosArray.Reserve(CRenderView::MaxCloudBlockerWorldSpaceNum);
	m_blockerParamArray.Reserve(CRenderView::MaxCloudBlockerWorldSpaceNum);
	m_blockerSSPosArray.Reserve(CRenderView::MaxCloudBlockerScreenSpaceNum);
	m_blockerSSParamArray.Reserve(CRenderView::MaxCloudBlockerScreenSpaceNum);
}

CVolumetricCloudsStage::~CVolumetricCloudsStage()
{
}

void CVolumetricCloudsStage::Init()
{
	m_pCloudMiePhaseFuncTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Shading/cloud_mie_phase_function.dds", FT_DONT_STREAM | FT_NOMIPS, eTF_Unknown);
	m_pNoiseTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/noise3d.dds", FT_DONT_STREAM | FT_NOMIPS, eTF_Unknown);

	const char* tname[MaxEyeNum][2] =
	{
		{
			"$VolCloudScaledMax0",
			"$VolCloudScaledMax1",
		},
		{
			"$VolCloudScaledMax0_R",
			"$VolCloudScaledMax1_R",
		},
	};
	const char* tnameMin[MaxEyeNum][2] =
	{
		{
			"$VolCloudScaledMin0",
			"$VolCloudScaledMin1",
		},
		{
			"$VolCloudScaledMin0_R",
			"$VolCloudScaledMin1_R",
		},
	};
	const char* tnamePrevDepth[MaxEyeNum] =
	{
		"$VolCloudPrevDepthScaled",
		"$VolCloudPrevDepthScaled_R",
	};

	const uint32 rtFlags = FT_NOMIPS | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_RENDERTARGET;

	for (int e = 0; e < MaxEyeNum; ++e)
	{
		for (int i = 0; i < 2; ++i)
		{
			CRY_ASSERT(m_pDownscaledMaxTex[e][i] == nullptr);
			m_pDownscaledMaxTex[e][i] = CTexture::GetOrCreateTextureObject(tname[e][i], 0, 0, 0, eTT_2D, rtFlags, eTF_Unknown);

			CRY_ASSERT(m_pDownscaledMinTex[e][i] == nullptr);
			m_pDownscaledMinTex[e][i] = CTexture::GetOrCreateTextureObject(tnameMin[e][i], 0, 0, 0, eTT_2D, rtFlags, eTF_Unknown);
		}

		CRY_ASSERT(m_pScaledPrevDepthTex[e] == nullptr);
		m_pScaledPrevDepthTex[e] = CTexture::GetOrCreateTextureObject(tnamePrevDepth[e], 0, 0, 0, eTT_2D, CRendererResources::s_ptexLinearDepthScaled[0]->GetFlags(), eTF_Unknown);
	}

	CRY_ASSERT(m_pCloudDepthTex == nullptr);
	m_pCloudDepthTex = CTexture::GetOrCreateTextureObject("$VolCloudDepthScaled", 0, 0, 0, eTT_2D, rtFlags, eTF_Unknown);

	CRY_ASSERT(m_pDownscaledMaxTempTex == nullptr);
	m_pDownscaledMaxTempTex = CTexture::GetOrCreateTextureObject("$VolCloudMaxTmp", 0, 0, 0, eTT_2D, rtFlags, eTF_Unknown);

	CRY_ASSERT(m_pDownscaledMinTempTex == nullptr);
	m_pDownscaledMinTempTex = CTexture::GetOrCreateTextureObject("$VolCloudMinTmp", 0, 0, 0, eTT_2D, rtFlags, eTF_Unknown);

	CRY_ASSERT(m_pDownscaledLeftEyeTex == nullptr);
	m_pDownscaledLeftEyeTex = CTexture::GetOrCreateTextureObject("$VolCloudLeftEye", 0, 0, 0, eTT_2D, rtFlags, eTF_Unknown);

	CRY_ASSERT(m_pCloudDensityTex == nullptr);
	m_pCloudDensityTex = CTexture::GetOrCreateTextureObject("$VolCloudDensityTmp", 0, 0, 0, eTT_3D, rtFlags, eTF_Unknown);

	CRY_ASSERT(m_pCloudShadowTex == nullptr);
	m_pCloudShadowTex = CTexture::GetOrCreateTextureObject("$VolCloudShadowTmp", 0, 0, 0, eTT_3D, rtFlags, eTF_Unknown);

	CRY_ASSERT(m_pCloudShadowConstantBuffer.get() == nullptr);
	m_pCloudShadowConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SCloudShadowShaderParam));

	CRY_ASSERT(m_pRenderCloudConstantBuffer.get() == nullptr);
	m_pRenderCloudConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SVolumetricCloudsShaderParam));

	CRY_ASSERT(m_pReprojectionConstantBuffer.get() == nullptr);
	m_pReprojectionConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SReprojectionParam));
}

void CVolumetricCloudsStage::Update()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const bool bVolumetricClouds = rd->m_bVolumetricCloudsEnabled;
	if (!bVolumetricClouds)
		return;

	const int32 screenWidth  = CRendererResources::s_renderWidth;
	const int32 screenHeight = CRendererResources::s_renderHeight;
	const bool bStereo = rd->IsStereoEnabled();
	const bool stereoReproj = bStereo && CRenderer::CV_r_VolumetricCloudsStereoReprojection != 0;

	// TODO: move this texture to view-dependent structure if volumetric clouds is rendered in multiple view ports such as split screens.
	if (CRendererResources::s_ptexVolCloudShadow)
	{
		const int shadowTexSize = VCGetCloudShadowTexSize();
		const int w = shadowTexSize;
		const int h = shadowTexSize;
		const int d = VCVerticalShadowTexSize;
		const ETEX_Format shadowTexFormat = eTF_R11G11B10F;
		const int nRTFlags = FT_NOMIPS | FT_USAGE_UNORDERED_ACCESS;

		const int width = CRendererResources::s_ptexVolCloudShadow->GetWidth();
		const int height = CRendererResources::s_ptexVolCloudShadow->GetHeight();
		const bool exist = CTexture::IsTextureExist(CRendererResources::s_ptexVolCloudShadow);

		if (w != width || h != height || !exist)
		{
			if (!CRendererResources::s_ptexVolCloudShadow->Create3DTexture(w, h, d, 1, nRTFlags, nullptr, shadowTexFormat))
			{
				CryFatalError("Couldn't allocate texture.");
			}
		}
	}

	const int32 scale = VCGetDownscaleRatio();
	const int32 scaledWidth = VCGetDownscaledResolution(screenWidth, scale);
	const int32 scaledHeight = VCGetDownscaledResolution(screenHeight, scale);
	const int32 depth = VCGetCloudRaymarchStepNum();
	const uint32 rtFlags = FT_NOMIPS | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_RENDERTARGET;

	bool bReset = false;

	const auto createTexture2D = [=](CTexture* pTex, ETEX_Format texFormat, uint32 flags, bool cond) -> bool
	{
		if (pTex != nullptr
		    && (scaledWidth != pTex->GetWidth()
		        || scaledHeight != pTex->GetHeight()
		        || !CTexture::IsTextureExist(pTex))
		    && cond)
		{
			if (!pTex->Create2DTexture(scaledWidth, scaledHeight, 1, flags, nullptr, texFormat))
			{
				CryFatalError("Couldn't allocate texture.");
			}

			return true;
		}
		return false;
	};

	for (int e = 0; e < MaxEyeNum; ++e)
	{
		auto condStereo = e == 0 || (e == 1 && bStereo);

		for (int i = 0; i < 2; ++i)
		{
			bReset |= createTexture2D(m_pDownscaledMaxTex[e][i], eTF_R16G16B16A16F, rtFlags, condStereo);
			bReset |= createTexture2D(m_pDownscaledMinTex[e][i], eTF_R16G16B16A16F, rtFlags, condStereo);
		}

		bReset |= createTexture2D(m_pScaledPrevDepthTex[e], CRendererResources::s_ptexLinearDepthScaled[0]->GetDstFormat(), CRendererResources::s_ptexLinearDepthScaled[0]->GetFlags(), condStereo);
	}

	createTexture2D(m_pCloudDepthTex, eTF_R32F, rtFlags, true);

	createTexture2D(m_pDownscaledMaxTempTex, eTF_R16G16B16A16F, rtFlags, true);

	createTexture2D(m_pDownscaledMinTempTex, eTF_R16G16B16A16F, rtFlags, true);

	if (stereoReproj)
	{
		createTexture2D(m_pDownscaledLeftEyeTex, eTF_R16G16B16A16F, rtFlags, true);
	}
	else
	{
		if (m_pDownscaledLeftEyeTex != nullptr
		    && CTexture::IsTextureExist(m_pDownscaledLeftEyeTex))
		{
			m_pDownscaledLeftEyeTex->ReleaseDeviceTexture(false);
		}
	}

	auto createTexture3D = [=](CTexture* pTex, ETEX_Format texFormat)
	{
		if (pTex != nullptr
		    && (scaledWidth != pTex->GetWidth()
		        || scaledHeight != pTex->GetHeight()
		        || depth != pTex->GetDepth()
		        || !CTexture::IsTextureExist(pTex)))
		{
			if (!pTex->Create3DTexture(scaledWidth, scaledHeight, depth, 1, rtFlags, nullptr, texFormat))
			{
				CryFatalError("Couldn't allocate texture.");
			}
		}
	};

	if (CRenderer::CV_r_VolumetricCloudsPipeline != 0)
	{
		createTexture3D(m_pCloudDensityTex, eTF_R16F);

		createTexture3D(m_pCloudShadowTex, eTF_R11G11B10F);
	}
	else
	{
		if (m_pCloudDensityTex != nullptr
		    && CTexture::IsTextureExist(m_pCloudDensityTex))
		{
			m_pCloudDensityTex->ReleaseDeviceTexture(false);
		}

		if (m_pCloudShadowTex != nullptr
		    && CTexture::IsTextureExist(m_pCloudShadowTex))
		{
			m_pCloudShadowTex->ReleaseDeviceTexture(false);
		}
	}

	if (bReset)
	{
		m_cleared = MaxFrameNum;
	}
}

void CVolumetricCloudsStage::ExecuteShadowGen()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const bool bVolumetricClouds = rd->m_bVolumetricCloudsEnabled;
	if (!bVolumetricClouds)
		return;

	const int64 nCurrentFrameID = RenderView()->GetFrameId();
	const int32 currEye = static_cast<int32>(RenderView()->GetCurrentEye());
	const bool bLeftEye = (nCurrentFrameID != m_nUpdateFrameID[currEye]) && (currEye == CCamera::eEye_Left);
	const bool bRightEye = (nCurrentFrameID != m_nUpdateFrameID[currEye]) && (currEye == CCamera::eEye_Right);

	if (!AreTexturesValid())
		return;

	if (bLeftEye)
	{
		++m_tick;

		GenerateCloudBlockerList();
		GenerateCloudBlockerSSList();
	}

	// update previous view-projection matrix
	if (bLeftEye || bRightEye)
	{
		SRenderViewInfo viewInfos[2];
		const size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfos);

		const int32 currentFrameIndex = GetCurrentFrameIndex();

		CRY_ASSERT(viewInfoCount == 1 || viewInfoCount == 2);
		const bool bStereoMultiGPURendering = (viewInfoCount == 2);

		if (bStereoMultiGPURendering)
		{
			m_viewMatrix[CCamera::eEye_Left][currentFrameIndex] = viewInfos[CCamera::eEye_Left].viewMatrix;
			m_projMatrix[CCamera::eEye_Left][currentFrameIndex] = viewInfos[CCamera::eEye_Left].projMatrix;
			m_viewMatrix[CCamera::eEye_Right][currentFrameIndex] = viewInfos[CCamera::eEye_Right].viewMatrix;
			m_projMatrix[CCamera::eEye_Right][currentFrameIndex] = viewInfos[CCamera::eEye_Right].projMatrix;
		}
		else
		{
			m_viewMatrix[currEye][currentFrameIndex] = viewInfos[0].viewMatrix;
			m_projMatrix[currEye][currentFrameIndex] = viewInfos[0].projMatrix;
		}
	}

	// Only generate once per frame.
	if (bLeftEye)
	{
		ExecuteVolumetricCloudShadowGen();
	}

	m_nUpdateFrameID[currEye] = nCurrentFrameID;
}

void CVolumetricCloudsStage::Execute()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const bool bVolumetricClouds = rd->m_bVolumetricCloudsEnabled;

	if (!bVolumetricClouds)
		return;

	if (!AreTexturesValid())
		return;

	PROFILE_LABEL_SCOPE("VOLUMETRIC_CLOUDS");

	VCCloudRenderContext context;
	GenerateCloudShaderParam(context);

	ExecuteComputeDensityAndShadow(context);
	ExecuteRenderClouds(context);

	{
		const int32 curEye = static_cast<int32>(RenderView()->GetCurrentEye());
		const int32 gpuCount = rd->GetActiveGPUCount();
		auto* pVolFogStage = rd->GetGraphicsPipeline().GetVolumetricFogStage();

		CShader* pShader = CShaderMan::s_ShaderClouds;

		const int32 bufferIndex = GetBufferIndex(gpuCount, context.bStereoMultiGPURendering);
		const int32 previousFrameIndex = GetPreviousFrameIndex(gpuCount, context.bStereoMultiGPURendering);

		CRY_ASSERT(curEye == 0 || curEye == 1);
		CTexture* prevMaxTex = bufferIndex ? m_pDownscaledMaxTex[curEye][0] : m_pDownscaledMaxTex[curEye][1];
		CTexture* currMaxTex = bufferIndex ? m_pDownscaledMaxTex[curEye][1] : m_pDownscaledMaxTex[curEye][0];
		CTexture* prevMinTex = bufferIndex ? m_pDownscaledMinTex[curEye][0] : m_pDownscaledMinTex[curEye][1];
		CTexture* currMinTex = bufferIndex ? m_pDownscaledMinTex[curEye][1] : m_pDownscaledMinTex[curEye][0];
		CTexture* prevScaledDepth = m_pScaledPrevDepthTex[curEye];

		CRY_ASSERT(prevScaledDepth->GetWidth() == context.scaledZTarget->GetWidth());
		CRY_ASSERT(prevScaledDepth->GetHeight() == context.scaledZTarget->GetHeight());

		const SResourceRegionMapping mapping =
		{
			{ 0, 0, 0, 0 },                                             // src position
			{ 0, 0, 0, 0 },                                             // dst position
			m_pDownscaledMinTempTex->GetDevTexture()->GetDimension(),   // dst size
			1, //D3D11_COPY_NO_OVERWRITE
		};

		if (m_cleared > 0)
		{
			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(context.scaledTarget->GetDevTexture(), prevMaxTex->GetDevTexture(), mapping);
			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(m_pDownscaledMinTempTex->GetDevTexture(), prevMinTex->GetDevTexture(), mapping);
			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(context.scaledZTarget->GetDevTexture(), prevScaledDepth->GetDevTexture(), mapping);

			m_cleared -= 1;
		}

		// apply temporal re-projection to clouds texture for max-depth.
		{
#ifdef ENABLE_DETAILED_PROFILING
			PROFILE_LABEL_SCOPE("REPROJECTION_CLOUDS_MAX_DEPTH");
#endif

			// each eyes and ticks are rendered with specific shader and shader resources.
			auto& pass = m_passTemporalReprojectionDepthMax[curEye][bufferIndex];

			// enables less flicker temporal reprojection filter.
			const bool bNewTemporalFilter = (CRenderer::CV_r_VolumetricCloudsTemporalReprojection != 0);

			uint64 rtMask = g_HWSR_MaskBit[HWSR_SAMPLE0]; // activates using max-depth.
			rtMask |= bNewTemporalFilter ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
			// TODO: remove after old graphics pipeline is removed.
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2]; // enables explicit constant buffer.

			if (pass.IsDirty(rtMask, bNewTemporalFilter, currMaxTex->GetID(), prevMaxTex->GetID()))
			{
				static CCryNameTSCRC shaderName = "ReprojectClouds";
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
				pass.SetTechnique(pShader, shaderName, rtMask);

				pass.SetRenderTarget(0, currMaxTex);

				pass.SetState(GS_NODEPTHTEST);

				pass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

				pass.SetTexture(0, context.scaledZTarget);
				pass.SetTexture(1, context.scaledTarget);
				pass.SetTexture(2, prevMaxTex);
				pass.SetTexture(3, m_pCloudDepthTex);
				pass.SetTexture(4, bNewTemporalFilter ? CRendererResources::s_ptexBlack : prevScaledDepth);

				pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);

				pass.SetRequireWorldPos(true);
			}

			pass.SetInlineConstantBuffer(EConstantBufferShaderSlot(3), m_pReprojectionConstantBuffer.get());

			pass.Execute();
		}

		// apply temporal re-projection to clouds texture for min-depth.
		{
#ifdef ENABLE_DETAILED_PROFILING
			PROFILE_LABEL_SCOPE("REPROJECTION_CLOUDS_MIN_DEPTH");
#endif

			// each eyes and ticks are rendered with specific shader and shader resources.
			auto& pass = m_passTemporalReprojectionDepthMin[curEye][bufferIndex];

			// enables less flicker temporal reprojection filter.
			const bool bNewTemporalFilter = (CRenderer::CV_r_VolumetricCloudsTemporalReprojection != 0);

			uint64 rtMask = bNewTemporalFilter ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
			// TODO: remove after old graphics pipeline is removed.
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2]; // enables explicit constant buffer.

			if (pass.IsDirty(rtMask, bNewTemporalFilter, currMinTex->GetID(), prevMinTex->GetID()))
			{

				static CCryNameTSCRC shaderName = "ReprojectClouds";
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
				pass.SetTechnique(pShader, shaderName, rtMask);

				pass.SetRenderTarget(0, currMinTex);

				pass.SetState(GS_NODEPTHTEST);
				
				pass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

				pass.SetTexture(0, context.scaledZTarget);
				pass.SetTexture(1, m_pDownscaledMinTempTex);
				pass.SetTexture(2, prevMinTex);
				pass.SetTexture(3, m_pCloudDepthTex);
				pass.SetTexture(4, bNewTemporalFilter ? CRendererResources::s_ptexBlack : prevScaledDepth);

				pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);

				pass.SetRequireWorldPos(true);
			}

			pass.SetInlineConstantBuffer(EConstantBufferShaderSlot(3), m_pReprojectionConstantBuffer.get());

			pass.Execute();
		}

		// copy current downscaled depth for next frame's temporal re-projection
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(context.scaledZTarget->GetDevTexture(), prevScaledDepth->GetDevTexture(), mapping);

		// upscale and blend it with scene.
		{
#ifdef ENABLE_DETAILED_PROFILING
			PROFILE_LABEL_SCOPE("UPSCALE_CLOUDS");
#endif

			// each eyes and ticks are rendered with specific shader and shader resources.
			auto& pass = m_passUpscale[curEye][bufferIndex];

			int32 inputFlag = 0;
			inputFlag |= context.bScreenSpaceCloudBlocker ? BIT(0) : 0;
#ifdef ENABLE_FULL_SIZE_FOG
			inputFlag |= context.bFog ? BIT(1) : 0;
			inputFlag |= context.bVolumetricFog ? BIT(2) : 0;
			inputFlag |= context.renderFogShadow ? BIT(3) : 0;
#endif

			CTexture* zTarget = RenderView()->GetDepthTarget();

			if (pass.IsDirty(inputFlag, CRenderer::CV_r_VolumetricClouds, currMinTex->GetID(), currMaxTex->GetID(), zTarget->GetID(), context.scaledZTarget->GetID()))
			{
				uint64 rtMask = 0;
				rtMask |= context.bScreenSpaceCloudBlocker ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;
#ifdef ENABLE_FULL_SIZE_FOG
				rtMask |= context.bFog ? g_HWSR_MaskBit[HWSR_FOG] : 0;
				rtMask |= context.bVolumetricFog ? g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG] : 0;
				rtMask |= context.renderFogShadow ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
#endif

				// TODO: remove after old graphics pipeline is removed.
				rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2]; // enables explicit constant buffer.

				static CCryNameTSCRC shaderName = "CloudsBlend";
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
				pass.SetTechnique(pShader, shaderName, rtMask);

				pass.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);
				pass.SetDepthTarget(zTarget);

				// using GS_BLDST_SRCALPHA because GS_BLDST_ONEMINUSSRCALPHA causes banding artifact when alpha value is very low.
				pass.SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_SRCALPHA);
				
				pass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

				pass.SetTexture(0, CRendererResources::s_ptexLinearDepth);
				pass.SetTexture(1, context.scaledZTarget);
				pass.SetTexture(2, currMaxTex);
				pass.SetTexture(3, currMinTex);

				pass.SetSampler(0, EDefaultSamplerStates::PointClamp);

#ifdef ENABLE_FULL_SIZE_FOG
				pass.SetTexture(4, m_pCloudDepthTex);

				if (context.bVolumetricFog)
				{
					pVolFogStage->BindVolumetricFogResources(pass, 9, 2);
				}
	#if defined(VOLUMETRIC_FOG_SHADOWS)
				else if (context.renderFogShadow)
				{
					pass.SetTexture(8, CRendererResources::s_ptexVolFogShadowBuf[0]);
					pass.SetSampler(2, EDefaultSamplerStates::TrilinearClamp);
				}
	#endif
#endif

				pass.SetRequireWorldPos(true);

				pass.SetRequirePerViewConstantBuffer(true);
			}

			pass.SetInlineConstantBuffer(EConstantBufferShaderSlot(3), m_pRenderCloudConstantBuffer.get());

			pass.Execute();
		}
	}
}

void CVolumetricCloudsStage::ExecuteVolumetricCloudShadowGen()
{
	PROFILE_LABEL_SCOPE("VOLUMETRIC_CLOUD_SHADOWS_GEN");

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	SVolumetricCloudTexInfo texInfo;
	rd->GetVolumetricCloudTextureInfo(texInfo);

	CShader* pShader = CShaderMan::s_ShaderClouds;
	auto& pass = m_passGenerateCloudShadow;

	assert(CRendererResources::s_ptexVolCloudShadow != nullptr);
	const int32 widthVolShadowTex = CRendererResources::s_ptexVolCloudShadow->GetWidth();
	const int32 heightVolShadowTex = CRendererResources::s_ptexVolCloudShadow->GetHeight();
	const int32 depthVolShadowTex = CRendererResources::s_ptexVolCloudShadow->GetDepth();

	CTexture* cloudVolumeTex = CRendererResources::s_ptexBlack;
	if (texInfo.cloudTexId > 0)
	{
		CTexture* pTex = CTexture::GetByID(texInfo.cloudTexId);
		if (pTex)
		{
			cloudVolumeTex = pTex;
		}
	}
	const bool bCloudVolTex = (texInfo.cloudTexId > 0);
	const bool bCloudBlocker = (m_blockerPosArray.Num() > 0);

	CTexture* cloudNoiseTex = m_pNoiseTex;
	if (texInfo.cloudNoiseTexId > 0)
	{
		CTexture* pTex = CTexture::GetByID(texInfo.cloudNoiseTexId);
		if (pTex)
		{
			cloudNoiseTex = pTex;
		}
	}

	CTexture* cloudEdgeNoiseTex = m_pNoiseTex;
	if (texInfo.edgeNoiseTexId > 0)
	{
		CTexture* pTex = CTexture::GetByID(texInfo.edgeNoiseTexId);
		if (pTex)
		{
			cloudEdgeNoiseTex = pTex;
		}
	}

	int32 inputFlag = 0;
	inputFlag |= bCloudVolTex ? BIT(0) : 0;
	inputFlag |= bCloudBlocker ? BIT(1) : 0;

	if (pass.IsDirty(inputFlag, cloudVolumeTex->GetID(), cloudNoiseTex->GetID(), cloudEdgeNoiseTex->GetID()))
	{
		uint64 rtMask = 0;
		rtMask |= bCloudVolTex ? g_HWSR_MaskBit[HWSR_SAMPLE3] : 0;
		rtMask |= bCloudBlocker ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;

		// TODO: remove after old graphics pipeline is removed.
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2]; // enables explicit constant buffer.

		static CCryNameTSCRC shaderName = "CloudShadowGen";
		pass.SetTechnique(pShader, shaderName, rtMask);

		pass.SetOutputUAV(0, CRendererResources::s_ptexVolCloudShadow);

		pass.SetTexture(0, cloudNoiseTex);
		pass.SetTexture(1, cloudEdgeNoiseTex);

		pass.SetSampler(0, EDefaultSamplerStates::TrilinearWrap);

		// set cloud base texture.
		pass.SetTexture(15, cloudVolumeTex);
	}

	const Vec3 texSize((f32)widthVolShadowTex, (f32)heightVolShadowTex, (f32)depthVolShadowTex);
	GenerateCloudShadowGenShaderParam(texSize);

	pass.SetInlineConstantBuffer(3, m_pCloudShadowConstantBuffer);

	// Tile sizes defined in [numthreads()] in shader
	const uint32 nTileSizeX = 16;
	const uint32 nTileSizeY = 16;
	const uint32 nTileSizeZ = 1;
	const uint32 dispatchSizeX = (widthVolShadowTex / nTileSizeX) + ((widthVolShadowTex % nTileSizeX) > 0 ? 1 : 0);
	const uint32 dispatchSizeY = (heightVolShadowTex / nTileSizeY) + ((heightVolShadowTex % nTileSizeY) > 0 ? 1 : 0);
	const uint32 dispatchSizeZ = (depthVolShadowTex / nTileSizeZ) + ((depthVolShadowTex % nTileSizeZ) > 0 ? 1 : 0);

	pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);

	{
		// Prepare buffers and textures which have been used by pixel shaders for use in the compute queue
		// Reduce resource state switching by requesting the most inclusive resource state
		pass.PrepareResourcesForUse(GetDeviceObjectFactory().GetCoreCommandList());

		{
			const bool bAsynchronousCompute = (CRenderer::CV_r_D3D12AsynchronousCompute & BIT(eStage_VolumetricClouds - eStage_FIRST_ASYNC_COMPUTE)) ? true : false;
			SScopedComputeCommandList computeCommandList(bAsynchronousCompute);

			pass.Execute(computeCommandList, EShaderStage_Compute);
		}
	}
}

void CVolumetricCloudsStage::GenerateCloudShadowGenShaderParam(const Vec3& texSize)
{
	SRenderViewShaderConstants& PF = RenderView()->GetShaderConstants();

	// Fetch the cloud parameters.
	Vec3 cloudGenParams;
	Vec3 cloudTurbulenceParams;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_GEN_PARAMS, cloudGenParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE, cloudTurbulenceParams);

	Vec3 cloudGlobalNoiseScale;
	Vec3 cloudTurbulenceNoiseScale;
	Vec3 cloudTurbulenceNoiseParams;
	Vec3 cloudDensityParams;
	Vec3 cloudMiscParams;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_GLOBAL_NOISE_SCALE, cloudGlobalNoiseScale);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_SCALE, cloudTurbulenceNoiseScale);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_PARAMS, cloudTurbulenceNoiseParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_DENSITY_PARAMS, cloudDensityParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_MISC_PARAM, cloudMiscParams);

	const f32 cloudiness = cloudGenParams.x;
	const f32 altitude = cloudGenParams.y;
	const f32 thickness = max(1e-3f, cloudGenParams.z);
	const Vec3 cloudNoiseScale = VCBaseNoiseScale.CompMul(cloudGlobalNoiseScale);
	const f32 edgeNoiseErode = cloudTurbulenceNoiseParams.x;
	const f32 edgeNoiseAbs = cloudTurbulenceNoiseParams.y;
	const f32 edgeTurbulence = (edgeNoiseErode > 0.0f) ? 2.0f * cloudTurbulenceParams.x : 8.0f * cloudTurbulenceParams.x;
	const f32 edgeThreshold = cloudTurbulenceParams.y + 1e-4f;
	const f32 absorptionFactor = cloudTurbulenceParams.z; // 1 to 5 percent is absorbed in clouds at most.

	const f32 extinction = VCScatterCoefficient + (VCScatterCoefficient * absorptionFactor);

	const f32 cloudShadowTilingSize = cloudMiscParams.z;
	const Vec3 vTiling(cloudShadowTilingSize, cloudShadowTilingSize, thickness);

	const Vec3 voxelLength(vTiling.x / texSize.x, vTiling.y / texSize.y, vTiling.z / texSize.z);

	const Vec3& baseTexTiling = PF.pVolCloudTilingSize;
	const Vec3 invBaseTexTiling(1.0f / baseTexTiling.x, 1.0f / baseTexTiling.y, 1.0f / baseTexTiling.z);

	const Vec3 edgeTurbulenceNoiseScale = VCBaseNoiseScale.CompMul(cloudTurbulenceNoiseScale * VCEdgeNoiseScale);

	const f32 maxGlobalCloudDensity = cloudDensityParams.x;
	const f32 minRescaleCloudTexDensity = cloudDensityParams.y;
	const f32 maxRescaleCloudTexDensity = cloudDensityParams.z;

	const f32 additionalNoiseIntensity = cloudMiscParams.x;

	const Vec3 cloudOffset(PF.pVolCloudTilingOffset.x, PF.pVolCloudTilingOffset.y, PF.pVolCloudTilingOffset.z - altitude);

	// set up constant buffer and upload to GPU.
	CTypedConstantBuffer<SCloudShadowShaderParam, 256> cb(m_pCloudShadowConstantBuffer);
	CRY_ASSERT(m_pCloudShadowConstantBuffer->m_size >= sizeof(SCloudShadowShaderParam));

	const Vec3 sunLightDirection = PF.pSunDirection;
	cb->sunLightDirection = Vec4(sunLightDirection, 0.0f);

	cb->cloudGenParams = Vec4(altitude, thickness, cloudiness, extinction);

	cb->tilingParams = Vec4(vTiling.x, vTiling.y, 0, 0);

	cb->texParams = Vec4(voxelLength.x, voxelLength.y, voxelLength.z, 0);

	cb->noiseParams = Vec4(cloudNoiseScale.x, cloudNoiseScale.y, cloudNoiseScale.z, 0);

	cb->baseScaleParams = Vec4(invBaseTexTiling.x, invBaseTexTiling.y, invBaseTexTiling.z, 0);

	cb->baseOffsetParams = Vec4(cloudOffset.x, cloudOffset.y, cloudOffset.z, 0);

	cb->densityParams = Vec4(maxGlobalCloudDensity, minRescaleCloudTexDensity, maxRescaleCloudTexDensity, additionalNoiseIntensity);

	cb->edgeNoiseScaleParams = Vec4(edgeTurbulenceNoiseScale.x, edgeTurbulenceNoiseScale.y, edgeTurbulenceNoiseScale.z, 0);

	cb->edgeTurbulenceParams = Vec4(edgeTurbulence, edgeThreshold, edgeNoiseErode, edgeNoiseAbs);

	cb->cloudShadowAnimParams = PF.pCloudShadowAnimParams;

	std::copy_n(m_blockerPosArray.begin(), m_blockerPosArray.size(), cb->cloudBlockerPos);
	std::copy_n(m_blockerParamArray.begin(), m_blockerParamArray.size(), cb->cloudBlockerParam);

	cb.CopyToDevice();
}

void CVolumetricCloudsStage::ExecuteComputeDensityAndShadow(const VCCloudRenderContext& context)
{
	if (CRenderer::CV_r_VolumetricCloudsPipeline != 0) // Inject clouds density and shadow to frustum aligned volume texture.
	{
#ifdef ENABLE_DETAILED_PROFILING
		PROFILE_LABEL_SCOPE("INJECT_CLOUD_SHADOW");
#endif

		CD3D9Renderer* const __restrict rd = gcpRendD3D;
		const int32 currentRenderEye = static_cast<int32>(RenderView()->GetCurrentEye());

		CShader* pShader = CShaderMan::s_ShaderClouds;

		// each eyes are rendered with specific shader and shader resources.
		auto& pass = m_passComputeDensityAndShadow[currentRenderEye];

		const uint64 rtMask = GetRTMask(context);

		// set cloud base texture.
		CTexture* cloudVolumeTex = CRendererResources::s_ptexBlack;
		if (context.bCloudVolTex)
		{
			CTexture* pTex = CTexture::GetByID(context.cloudVolumeTexId);
			if (pTex)
			{
				cloudVolumeTex = pTex;
			}
		}

		if (pass.IsDirty(rtMask, CRenderer::CV_r_VolumetricClouds, cloudVolumeTex->GetID()))
		{
			static CCryNameTSCRC shaderName = "InjectCloudDensityAndShadow";
			pass.SetTechnique(pShader, shaderName, rtMask);

			pass.SetOutputUAV(0, m_pCloudShadowTex);
			pass.SetOutputUAV(1, m_pCloudDensityTex);

			pass.SetTexture(0, context.scaledZTarget);

			const bool bLtoR = context.bReprojectionFromLeftToRight;
			pass.SetTexture(2, bLtoR ? m_pScaledPrevDepthTex[CCamera::eEye_Left].get() : CRendererResources::s_ptexBlack);

			pass.SetTexture(5, CRendererResources::s_ptexVolCloudShadow);

			pass.SetSampler(0, EDefaultSamplerStates::BilinearBorder_Black);
			pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);
			pass.SetSampler(2, EDefaultSamplerStates::TrilinearClamp);

			pass.SetTexture(15, cloudVolumeTex);
		}

		// set cloud noise texture.
		CTexture* cloudNoiseTex = m_pNoiseTex;
		if (context.cloudNoiseTexId > 0)
		{
			CTexture* pTex = CTexture::GetByID(context.cloudNoiseTexId);
			if (pTex)
			{
				cloudNoiseTex = pTex;
			}
		}
		CTexture* cloudEdgeNoiseTex = m_pNoiseTex;
		if (context.edgeNoiseTexId > 0)
		{
			CTexture* pTex = CTexture::GetByID(context.edgeNoiseTexId);
			if (pTex)
			{
				cloudEdgeNoiseTex = pTex;
			}
		}
		pass.SetTexture(6, cloudNoiseTex);
		pass.SetTexture(7, cloudEdgeNoiseTex);

		pass.SetInlineConstantBuffer(3, m_pRenderCloudConstantBuffer);

		int screenWidth = context.screenWidth;
		int screenHeight = context.screenHeight;
		int gridWidth = screenWidth;
		int gridHeight = screenHeight;

		if (CVrProjectionManager::IsMultiResEnabledStatic())
		{
			CVrProjectionManager::Instance()->GetProjectionSize(screenWidth, screenHeight, gridWidth, gridHeight);

			auto constantBuffer = CVrProjectionManager::Instance()->GetProjectionConstantBuffer(screenWidth, screenHeight);
			pass.SetInlineConstantBuffer(eConstantBufferShaderSlot_VrProjection, constantBuffer);
		}
		
		// Tile sizes defined in [numthreads()] in shader
		const uint32 nTileSizeX = 8;
		const uint32 nTileSizeY = 8;
		const uint32 nTileSizeZ = 1;
		const uint32 dispatchSizeX = gridWidth / nTileSizeX + (gridWidth % nTileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = gridHeight / nTileSizeY + (gridHeight % nTileSizeY > 0 ? 1 : 0);
		const uint32 dispatchSizeZ = 1;

		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);

		{
			// Prepare buffers and textures which have been used by pixel shaders for use in the compute queue
			// Reduce resource state switching by requesting the most inclusive resource state
			pass.PrepareResourcesForUse(GetDeviceObjectFactory().GetCoreCommandList());

			{
				const bool bAsynchronousCompute = (CRenderer::CV_r_D3D12AsynchronousCompute & BIT(eStage_VolumetricClouds - eStage_FIRST_ASYNC_COMPUTE)) ? true : false;
				SScopedComputeCommandList computeCommandList(bAsynchronousCompute);

				pass.Execute(computeCommandList, EShaderStage_Compute);
			}
		}
	}
}

void CVolumetricCloudsStage::ExecuteRenderClouds(const VCCloudRenderContext& context)
{
#ifdef ENABLE_DETAILED_PROFILING
	PROFILE_LABEL_SCOPE("RENDER_CLOUDS");
#endif

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const int32 currentRenderEye = static_cast<int32>(RenderView()->GetCurrentEye());
	auto* pVolFogStage = rd->GetGraphicsPipeline().GetVolumetricFogStage();

	CShader* pShader = CShaderMan::s_ShaderClouds;

	// each eyes are rendered with specific shader and shader resources.
	auto& pass = m_passRenderClouds[currentRenderEye];

	const uint64 rtMask = GetRTMask(context);

	int32 inputFlag = 0;
	inputFlag |= CRenderer::CV_r_VolumetricClouds & 0xF;
	inputFlag |= (CRenderer::CV_r_VolumetricCloudsPipeline & 0xF) << 4;

	// set cloud base texture.
	CTexture* cloudVolumeTex = CRendererResources::s_ptexBlack;
	if (context.bCloudVolTex)
	{
		CTexture* pTex = CTexture::GetByID(context.cloudVolumeTexId);
		if (pTex)
		{
			cloudVolumeTex = pTex;
		}
	}

	if (pass.IsDirty(rtMask, inputFlag, cloudVolumeTex->GetID()))
	{
		const bool bMono = (CRenderer::CV_r_VolumetricCloudsPipeline == 0);
		static CCryNameTSCRC shaderName0 = "RenderCloud_Monolithic";
		static CCryNameTSCRC shaderName1 = "RenderCloud";
		CCryNameTSCRC& shaderName = bMono ? shaderName0 : shaderName1;
		pass.SetTechnique(pShader, shaderName, rtMask);

		pass.SetOutputUAV(0, context.scaledTarget);
		pass.SetOutputUAV(1, m_pCloudDepthTex);
		pass.SetOutputUAV(2, m_pDownscaledMinTempTex);

		pass.SetTexture(0, context.scaledZTarget);

		const bool bLtoR = context.bReprojectionFromLeftToRight;
		pass.SetTexture(1, bLtoR ? m_pDownscaledLeftEyeTex.get() : CRendererResources::s_ptexBlack);
		pass.SetTexture(2, bLtoR ? m_pScaledPrevDepthTex[CCamera::eEye_Left].get() : CRendererResources::s_ptexBlack);

		pass.SetTexture(3, bMono ? CRendererResources::s_ptexBlack : m_pCloudDensityTex.get());
		pass.SetTexture(4, bMono ? CRendererResources::s_ptexBlack : m_pCloudShadowTex.get());

		pass.SetTexture(5, CRendererResources::s_ptexVolCloudShadow);
		pass.SetTexture(12, m_pCloudMiePhaseFuncTex);

		pass.SetSampler(0, EDefaultSamplerStates::BilinearBorder_Black);
		pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);
		pass.SetSampler(2, EDefaultSamplerStates::TrilinearClamp);

		if (context.bVolumetricFog)
		{
			pVolFogStage->BindVolumetricFogResources(pass, 9, -1);
		}
#if defined(VOLUMETRIC_FOG_SHADOWS)
		else if (context.renderFogShadow)
		{
			pass.SetTexture(8, CRendererResources::s_ptexVolFogShadowBuf[0]);
			pass.SetSampler(3, EDefaultSamplerStates::PointClamp);
		}
#endif

		pass.SetTexture(15, cloudVolumeTex);
	}

	// set cloud noise texture.
	CTexture* cloudNoiseTex = m_pNoiseTex;
	if (context.cloudNoiseTexId > 0)
	{
		CTexture* pTex = CTexture::GetByID(context.cloudNoiseTexId);
		if (pTex)
		{
			cloudNoiseTex = pTex;
		}
	}
	CTexture* cloudEdgeNoiseTex = m_pNoiseTex;
	if (context.edgeNoiseTexId > 0)
	{
		CTexture* pTex = CTexture::GetByID(context.edgeNoiseTexId);
		if (pTex)
		{
			cloudEdgeNoiseTex = pTex;
		}
	}
	pass.SetTexture(6, cloudNoiseTex);
	pass.SetTexture(7, cloudEdgeNoiseTex);

	pass.SetInlineConstantBuffer(3, m_pRenderCloudConstantBuffer);

	int screenWidth = context.screenWidth;
	int screenHeight = context.screenHeight;
	int gridWidth = screenWidth;
	int gridHeight = screenHeight;

	if (CVrProjectionManager::IsMultiResEnabledStatic())
	{
		CVrProjectionManager::Instance()->GetProjectionSize(screenWidth, screenHeight, gridWidth, gridHeight);

		auto constantBuffer = CVrProjectionManager::Instance()->GetProjectionConstantBuffer(screenWidth, screenHeight);
		pass.SetInlineConstantBuffer(eConstantBufferShaderSlot_VrProjection, constantBuffer);
	}

	// Tile sizes defined in [numthreads()] in shader
	const uint32 nTileSizeX = 8;
	const uint32 nTileSizeY = 8;
	const uint32 nTileSizeZ = 1;
	const uint32 dispatchSizeX = gridWidth / nTileSizeX + (gridWidth % nTileSizeX > 0 ? 1 : 0);
	const uint32 dispatchSizeY = gridHeight / nTileSizeY + (gridHeight % nTileSizeY > 0 ? 1 : 0);
	const uint32 dispatchSizeZ = 1;

	pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);

	{
		// Prepare buffers and textures which have been used by pixel shaders for use in the compute queue
		// Reduce resource state switching by requesting the most inclusive resource state
		pass.PrepareResourcesForUse(GetDeviceObjectFactory().GetCoreCommandList());

		{
			const bool bAsynchronousCompute = (CRenderer::CV_r_D3D12AsynchronousCompute & BIT(eStage_VolumetricClouds - eStage_FIRST_ASYNC_COMPUTE)) ? true : false;
			SScopedComputeCommandList computeCommandList(bAsynchronousCompute);

			pass.Execute(computeCommandList, EShaderStage_Compute);
		}
	}

	// keep texture's ref-counter more than zero until the render-passes which use the texture are destructed.
	m_pVolCloudTex = cloudVolumeTex;
	m_pVolCloudNoiseTex = cloudNoiseTex;
	m_pVolCloudEdgeNoiseTex = cloudEdgeNoiseTex;
}

void CVolumetricCloudsStage::GenerateCloudShaderParam(::VCCloudRenderContext& context)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	SRenderViewShaderConstants& PF = RenderView()->GetShaderConstants();

	auto& gp = rd->GetGraphicsPipeline();
	auto* pVolFogStage = gp.GetVolumetricFogStage();
	CRY_ASSERT(pVolFogStage);

	SRenderViewInfo viewInfos[2];
	const size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfos);

	const int32 width  = CRendererResources::s_renderWidth;
	const int32 height = CRendererResources::s_renderHeight;
	const bool bStereo = rd->IsStereoEnabled();
	const bool bReverseDepth = true;
	const f32 time = GetGraphicsPipeline().GetAnimationTime().GetSeconds();
	const int32 curEye = static_cast<int32>(RenderView()->GetCurrentEye());
	const int32 gpuCount = rd->GetActiveGPUCount();

	SVolumetricCloudTexInfo texInfo;
	rd->GetVolumetricCloudTextureInfo(texInfo);

	context.cloudVolumeTexId = texInfo.cloudTexId;
	context.cloudNoiseTexId = texInfo.cloudNoiseTexId;
	context.edgeNoiseTexId = texInfo.edgeNoiseTexId;
	context.bRightEye = (curEye == CCamera::eEye_Right);
	context.bFog = (RenderView()->IsGlobalFogEnabled() && !(GetGraphicsPipeline().IsPipelineFlag(CGraphicsPipeline::EPipelineFlags::NO_SHADER_FOG)));

#if defined(VOLUMETRIC_FOG_SHADOWS)
	context.renderFogShadow = rd->m_bVolFogShadowsEnabled;
#else
	context.renderFogShadow = false;
#endif
	context.bVolumetricFog = rd->m_bVolumetricFogEnabled;

	Vec3 cloudGenParams;
	Vec3 cloudScatteringLowParams;
	Vec3 cloudScatteringHighParams;
	Vec3 cloudGroundColorParams;
	Vec3 cloudScatteringMulti;
	Vec3 cloudWindAndAtmosphericParams;
	Vec3 cloudTurbulenceParams;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_GEN_PARAMS, cloudGenParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_LOW, cloudScatteringLowParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_HIGH, cloudScatteringHighParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_GROUND_COLOR, cloudGroundColorParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_MULTI, cloudScatteringMulti);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_WIND_ATMOSPHERIC, cloudWindAndAtmosphericParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE, cloudTurbulenceParams);

	Vec3 cloudEnvParams;
	Vec3 cloudGlobalNoiseScale;
	Vec3 cloudRenderParams;
	Vec3 cloudTurbulenceNoiseScale;
	Vec3 cloudTurbulenceNoiseParams;
	Vec3 cloudDensityParams;
	Vec3 cloudMiscParams;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_ENV_PARAMS, cloudEnvParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_GLOBAL_NOISE_SCALE, cloudGlobalNoiseScale);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_RENDER_PARAMS, cloudRenderParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_SCALE, cloudTurbulenceNoiseScale);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_PARAMS, cloudTurbulenceNoiseParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_DENSITY_PARAMS, cloudDensityParams);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_MISC_PARAM, cloudMiscParams);

	context.fullscreenWidth = width;
	context.fullscreenHeight = height;

	const int32 scale = VCGetDownscaleRatio();
	context.screenWidth = VCGetDownscaledResolution(width, scale);
	context.screenHeight = VCGetDownscaledResolution(height, scale);
	context.screenDepth = VCGetCloudRaymarchStepNum();
	context.stereoReproj = bStereo && CRenderer::CV_r_VolumetricCloudsStereoReprojection != 0;

	// enable cloud blocker which decays cloud density.
	context.bCloudBlocker = (m_blockerPosArray.Num() > 0);

	// decay cloud density with cloud blocker entity on screen space.
	context.bScreenSpaceCloudBlocker = (m_blockerSSPosArray.Num() > 0);

	// enable reprojection from left eye to right eye.
	context.bReprojectionFromLeftToRight = (context.stereoReproj && context.bRightEye);

	// enable user specified cloud volume texture.
	context.bCloudVolTex = (context.cloudVolumeTexId > 0);

	const uint32 downscaleIndex = max(0, scale - 1);
	context.scaledTarget = m_pDownscaledMaxTempTex;
	if (context.stereoReproj && !context.bRightEye)
	{
		context.scaledTarget = m_pDownscaledLeftEyeTex; // left eye is re-projected to right eye.
	}
	context.scaledZTarget = CRendererResources::s_ptexLinearDepthScaled[downscaleIndex > 0];

	// enable fast path when clouds are always rendered in low shading mode, and viewable region is covered by cloud shadow map.
	const f32 shadingLOD = cloudMiscParams.y;
	const f32 cloudShadowTilingSize = cloudMiscParams.z;
	const f32 maxViewableDistance = cloudRenderParams.x;
	context.bFastPath = (shadingLOD >= 1.0f && cloudShadowTilingSize >= (maxViewableDistance * 2.0f));

	CRY_ASSERT(viewInfoCount == 1 || viewInfoCount == 2);
	context.bStereoMultiGPURendering = (viewInfoCount == 2);
	CRY_ASSERT(!context.bStereoMultiGPURendering || (context.bStereoMultiGPURendering && (gpuCount == 2)));

	// update constant buffer for cloud rendering and upscaling.
	{
		CryStackAllocWithSizeVectorCleared(SVolumetricCloudsShaderParam, viewInfoCount, bufferData, CDeviceBufferManager::AlignBufferSizeForStreaming);

		// if stereo multi-GPU rendering is enabled, this loop fills two constant buffer data for left and right eye.
		for (int32 i = 0; i < viewInfoCount; ++i)
		{
			const auto& viewInfo = viewInfos[i];
			SVolumetricCloudsShaderParam& cb = bufferData[i];

			cb.shadeColorFromSun = PF.pCloudShadingColorSun;
			cb.waterLevel = PF.vWaterLevel.x; // static water ocean level

			cb.cameraPos = viewInfo.cameraOrigin;
			cb.cameraFrontVector = viewInfo.cameraVZ.GetNormalized();

			cb.sunLightDirection = PF.pSunDirection;

			cb.nearClipDist = viewInfo.nearClipPlane;
			cb.farClipDist = viewInfo.farClipPlane;
			cb.reciprocalFarClipDist = 1.0f / viewInfo.farClipPlane;

			cb.cloudShadowAnimParams = PF.pCloudShadowAnimParams;

			const f32 fScreenWidth = static_cast<f32>(context.screenWidth);
			const f32 fScreenHeight = static_cast<f32>(context.screenHeight);
			cb.screenSize = Vec4(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);

			Vec3 vRT, vLT, vLB, vRB;
			const bool bMirrorCull = (viewInfo.flags & SRenderViewInfo::eFlags_MirrorCull) != 0;
			SD3DPostEffectsUtils::GetFrustumCorners(vRT, vLT, vLB, vRB, *viewInfo.pCamera, bMirrorCull);
			cb.frustumTL = Vec4(vLT.x, vLT.y, vLT.z, 0.0f);
			cb.frustumTR = Vec4(vRT.x, vRT.y, vRT.z, 0.0f);
			cb.frustumBL = Vec4(vLB.x, vLB.y, vLB.z, 0.0f);

			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKYLIGHT_RAYLEIGH_INSCATTER, cb.skylightRayleighInScatter);

			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING, cb.atmosphericScattering);
			cb.sunIntensityForAtmospheric = cloudWindAndAtmosphericParams.z;

			cb.sunSingleScatter = cloudScatteringLowParams.x;
			cb.sunLowOrderScatter = cloudScatteringLowParams.y;
			cb.sunLowOrderScatterAnisotropy = cloudScatteringLowParams.z;
			cb.sunHighOrderScatter = cloudScatteringHighParams.x;
			cb.skyLightingFactor = cloudScatteringHighParams.y;
			cb.groundLightingFactor = cloudScatteringHighParams.z;
			cb.groundAlbedo = cloudGroundColorParams;

			const f32 altitude = cloudGenParams.y;
			const f32 thickness = max(1e-3f, cloudGenParams.z);
			cb.cloudiness = cloudGenParams.x;
			cb.altitude = altitude;
			cb.thickness = thickness;
			cb.padding0 = 0.0f;

			const f32 absorptionFactor = cloudTurbulenceParams.z; // 1 to 5 percent is absorbed in clouds at most.
			cb.extinction = VCScatterCoefficient + (VCScatterCoefficient * absorptionFactor);
			cb.scatterCoefficient = VCScatterCoefficient;
			cb.powderFactor = cloudScatteringMulti.z;
			cb.shadingLOD = shadingLOD;

			cb.multiScatterAttenuation = cloudScatteringMulti.x;
			cb.multiScatterPreservation = cloudScatteringMulti.y;
			const f64 zn = viewInfo.nearClipPlane;
			const f64 zf = viewInfo.farClipPlane;
			cb.projRatio0 = static_cast<f32>(bReverseDepth ? zn / (zn - zf) : zf / (zf - zn));
			cb.projRatio1 = static_cast<f32>(bReverseDepth ? zn / (zf - zn) : zn / (zn - zf));

			cb.radius = clamp_tpl<f32>(cloudEnvParams.x, VCMinSphereRadius, VCMaxSphereRadius);
			cb.horizonHeight = cloudEnvParams.z;
			cb.cloudNoiseScale = VCBaseNoiseScale.CompMul(cloudGlobalNoiseScale);

			const Vec3 vTiling(cloudShadowTilingSize, cloudShadowTilingSize, thickness);
			cb.vInvTiling = Vec3(1.0f / vTiling.x, 1.0f / vTiling.y, 1.0f / vTiling.z);

			const Vec3& baseTexTiling = PF.pVolCloudTilingSize;
			cb.invBaseTexTiling = Vec3(1.0f / baseTexTiling.x, 1.0f / baseTexTiling.y, 1.0f / baseTexTiling.z);

			const int32 iFrameCount = m_tick % VCFrameCountCycle;
			cb.frameCount = f32(iFrameCount);
			cb.jitteredSamplingPosition = VCGetVdC(iFrameCount);

			cb.cloudOffset = Vec3(PF.pVolCloudTilingOffset.x, PF.pVolCloudTilingOffset.y, PF.pVolCloudTilingOffset.z - altitude);
			cb.padding1 = 0.0f;

			cb.maxGlobalCloudDensity = cloudDensityParams.x;
			cb.minRescaleCloudTexDensity = cloudDensityParams.y;
			cb.maxRescaleCloudTexDensity = cloudDensityParams.z;
			cb.additionalNoiseIntensity = cloudMiscParams.x;

			cb.edgeTurbulenceNoiseScale = VCBaseNoiseScale.CompMul(cloudTurbulenceNoiseScale * VCEdgeNoiseScale);
			cb.padding2 = 0.0f;

			const f32 edgeNoiseErode = cloudTurbulenceNoiseParams.x;
			cb.edgeTurbulence = (edgeNoiseErode > 0.0f) ? 2.0f * cloudTurbulenceParams.x : 8.0f * cloudTurbulenceParams.x;
			cb.edgeThreshold = cloudTurbulenceParams.y + 1e-4f;
			cb.edgeNoiseErode = edgeNoiseErode;
			cb.edgeNoiseAbs = cloudTurbulenceNoiseParams.y;

			cb.maxViewableDistance = maxViewableDistance;
			cb.maxRayMarchDistance = cloudRenderParams.y;
			cb.maxRayMarchStep = static_cast<f32>(VCGetCloudRaymarchStepNum());

			std::copy_n(m_blockerPosArray.begin(), m_blockerPosArray.size(), cb.cloudBlockerPos);
			std::copy_n(m_blockerParamArray.begin(), m_blockerParamArray.size(), cb.cloudBlockerParam);

			// don't need to take care of stereo multi-GPU rendering because stereo reprojection isn't used with it.
			const int32 currentFrameIndex = GetCurrentFrameIndex();
			cb.leftToRightReprojMatrix = VCGetReprojectionMatrix(
			  m_viewMatrix[CCamera::eEye_Left][currentFrameIndex],
			  m_projMatrix[CCamera::eEye_Left][currentFrameIndex],
			  viewInfo.viewMatrix,
			  viewInfo.projMatrix);

			// volumetric fog shadow parameters
#if defined(VOLUMETRIC_FOG_SHADOWS)
			if (context.renderFogShadow)
			{
				Vec3 volFogShadowDarkeningP;
				gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_DARKENING, volFogShadowDarkeningP);
				cb.volFogShadowDarkening = Vec4(volFogShadowDarkeningP, 0);

				const f32 aSun = (1.0f - clamp_tpl(volFogShadowDarkeningP.y, 0.0f, 1.0f)) * 1.0f;
				const f32 bSun = 1.0f - aSun;
				const f32 aAmb = (1.0f - clamp_tpl(volFogShadowDarkeningP.z, 0.0f, 1.0f)) * 0.4f;
				const f32 bAmb = 1.0f - aAmb;
				cb.volFogShadowDarkeningSunAmb = Vec4(aSun, bSun, aAmb, bAmb);
			}
			else
			{
				cb.volFogShadowDarkening = Vec4(0.0f);
				cb.volFogShadowDarkeningSunAmb = Vec4(0.0f);
			}
#else
			cb.volFogShadowDarkening = Vec4(0.0f);
			cb.volFogShadowDarkeningSunAmb = Vec4(0.0f);
#endif

			// fog parameters
			cb.vfParams = PF.pVolumetricFogParams;          // PB_VolumetricFogParams
			cb.vfRampParams = PF.pVolumetricFogRampParams;  // PB_VolumetricFogRampParams
			cb.vfSunDir = PF.pVolumetricFogSunDir;          // PB_VolumetricFogSunDir
			cb.vfColGradBase = Vec3(PF.pFogColGradColBase); // PB_FogColGradColBase
			cb.padding3 = 0.0f;
			cb.vfColGradDelta = Vec3(PF.pFogColGradColDelta); // PB_FogColGradColDelta
			cb.padding4 = 0.0f;
			cb.vfColGradParams = PF.pFogColGradParams; // PB_FogColGradParams
			cb.vfColGradRadial = PF.pFogColGradRadial; // PB_FogColGradRadial

			// voluemtric for parameters
			cb.vfSamplingParams = PF.pVolumetricFogSamplingParams;                      // PB_VolumetricFogSamplingParams
			cb.vfDistributionParams = PF.pVolumetricFogDistributionParams;              // PB_VolumetricFogDistributionParams
			cb.vfScatteringParams = PF.pVolumetricFogScatteringParams;                  // PB_VolumetricFogScatteringParams
			cb.vfScatteringBlendParams = PF.pVolumetricFogScatteringBlendParams;        // PB_VolumetricFogScatteringBlendParams
			cb.vfScatteringColor = PF.pVolumetricFogScatteringColor;                    // PB_VolumetricFogScatteringColor
			cb.vfScatteringSecondaryColor = PF.pVolumetricFogScatteringSecondaryColor;  // PB_VolumetricFogScatteringSecondaryColor
			cb.vfHeightDensityParams = PF.pVolumetricFogHeightDensityParams;            // PB_VolumetricFogHeightDensityParams
			cb.vfHeightDensityRampParams = PF.pVolumetricFogHeightDensityRampParams;    // PB_VolumetricFogHeightDensityRampParams
			cb.vfDistanceParams = PF.pVolumetricFogDistanceParams;                      // PB_VolumetricFogDistanceParams
			cb.vfGlobalEnvProbeParams0 = pVolFogStage->GetGlobalEnvProbeShaderParam0(); // PB_VolumetricFogGlobalEnvProbe0
			cb.vfGlobalEnvProbeParams1 = pVolFogStage->GetGlobalEnvProbeShaderParam1(); // PB_VolumetricFogGlobalEnvProbe1
			cb.vfTimeParams = Vec4(time, time * 0.5f, time * 0.25f, time * 0.125f);     // PB_time

			// upscale parameters
			const f32 fullScreenWidth = static_cast<f32>(context.fullscreenWidth);
			const f32 fullScreenHeight = static_cast<f32>(context.fullscreenHeight);
			cb.fullScreenSize = Vec4(fullScreenWidth, fullScreenHeight, 0.0f, 0.0f);
			std::copy_n(m_blockerSSPosArray.begin(), m_blockerSSPosArray.size(), &(cb.cloudBlockerSSPos));
			std::copy_n(m_blockerSSParamArray.begin(), m_blockerSSParamArray.size(), &(cb.cloudBlockerSSParam));
		}

		// upload data to constant buffer on GPU.
		const auto size = sizeof(SVolumetricCloudsShaderParam);
		CRY_ASSERT(m_pRenderCloudConstantBuffer->m_size >= size);
		m_pRenderCloudConstantBuffer->UpdateBuffer(&bufferData[0], size, 0, viewInfoCount);
	}

	// update constant buffer for temporal reprojection.
	{
		CryStackAllocWithSizeVectorCleared(SReprojectionParam, viewInfoCount, bufferData, CDeviceBufferManager::AlignBufferSizeForStreaming);
		const int32 previousFrameIndex = GetPreviousFrameIndex(gpuCount, context.bStereoMultiGPURendering);

		// if stereo multi-GPU rendering is enabled, this loop fills two constant buffer data for left and right eye.
		for (int32 i = 0; i < viewInfoCount; ++i)
		{
			const auto& viewInfo = viewInfos[i];
			SReprojectionParam& cb = bufferData[i];

			const int32 eyeIndex = context.bStereoMultiGPURendering ? i : curEye;

			const Matrix44 matReproj = VCGetReprojectionMatrix(
			  m_viewMatrix[eyeIndex][previousFrameIndex],
			  m_projMatrix[eyeIndex][previousFrameIndex],
			  viewInfo.viewMatrix,
			  viewInfo.projMatrix);
			const f64 zn = viewInfo.nearClipPlane;
			const f64 zf = viewInfo.farClipPlane;
			const f32 projRatio0 = static_cast<f32>(bReverseDepth ? zn / (zn - zf) : zf / (zf - zn));
			const f32 projRatio1 = static_cast<f32>(bReverseDepth ? zn / (zf - zn) : zn / (zn - zf));
			const f32 fScreenWidth = static_cast<f32>(context.screenWidth);
			const f32 fScreenHeight = static_cast<f32>(context.screenHeight);

			cb.screenSize = Vec4(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
			cb.reprojectionParams = Vec4(projRatio0, projRatio1, 0.0f, 0.0f);
			cb.reprojectionMatrix = matReproj;
		}

		const auto size = sizeof(SReprojectionParam);
		CRY_ASSERT(m_pReprojectionConstantBuffer->m_size >= size);
		m_pReprojectionConstantBuffer->UpdateBuffer(&bufferData[0], size, 0, viewInfoCount);
	}
}

int32 CVolumetricCloudsStage::GetBufferIndex(const int32 gpuCount, bool bStereoMultiGPURendering) const
{
	int32 bufferIndex;
	if (bStereoMultiGPURendering)
	{
		bufferIndex = m_tick & 0x1;
	}
	else
	{
		bufferIndex = (m_tick / gpuCount) & 0x1;
	}
	return bufferIndex;
}

int32 CVolumetricCloudsStage::GetCurrentFrameIndex() const
{
	return m_tick % MaxFrameNum;
}

int32 CVolumetricCloudsStage::GetPreviousFrameIndex(const int32 gpuCount, bool bStereoMultiGPURendering) const
{
	int32 frameIndex;
	if (bStereoMultiGPURendering)
	{
		frameIndex = max((m_tick - 1) % MaxFrameNum, 0);
	}
	else
	{
		frameIndex = max((m_tick - gpuCount) % MaxFrameNum, 0);
	}
	return frameIndex;
}

bool CVolumetricCloudsStage::AreTexturesValid() const
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const bool bStereo = rd->IsStereoEnabled();

	bool bDownscaledRTs = CTexture::IsTextureExist(m_pCloudDepthTex)
	                      && CTexture::IsTextureExist(m_pDownscaledMaxTempTex)
	                      && CTexture::IsTextureExist(m_pDownscaledMinTempTex);

	const int eyeNum = bStereo ? MaxEyeNum : 1;
	for (int e = 0; e < eyeNum; ++e)
	{
		for (int i = 0; i < 2; ++i)
		{
			bDownscaledRTs = bDownscaledRTs && CTexture::IsTextureExist(m_pDownscaledMaxTex[e][i]);
			bDownscaledRTs = bDownscaledRTs && CTexture::IsTextureExist(m_pDownscaledMinTex[e][i]);
		}
		bDownscaledRTs = bDownscaledRTs && CTexture::IsTextureExist(m_pScaledPrevDepthTex[e]);
	}

	bool stereoReproj = bStereo && CRenderer::CV_r_VolumetricCloudsStereoReprojection != 0;
	bDownscaledRTs = bDownscaledRTs && (!stereoReproj || CTexture::IsTextureExist(m_pDownscaledLeftEyeTex));

	if (CRenderer::CV_r_VolumetricCloudsPipeline != 0)
	{
		bDownscaledRTs = bDownscaledRTs && CTexture::IsTextureExist(m_pCloudDensityTex);
		bDownscaledRTs = bDownscaledRTs && CTexture::IsTextureExist(m_pCloudShadowTex);
	}

	return (CTexture::IsTextureExist(CRendererResources::s_ptexVolCloudShadow) && bDownscaledRTs);
}

void CVolumetricCloudsStage::GenerateCloudBlockerList()
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	const auto& blockers = pRenderView->GetCloudBlockers(0);
	auto numBlocker = blockers.size();
	CRY_ASSERT(numBlocker <= CRenderView::MaxCloudBlockerWorldSpaceNum);

	// build blocker list for current frame.
	m_blockerPosArray.SetUse(0);
	m_blockerParamArray.SetUse(0);

	for (auto& blocker : blockers)
	{
		m_blockerPosArray.push_back(Vec4(blocker.position, (float)numBlocker));

		const Vec3& param = blocker.param;
		const float decayStart = param.x;
		const float decayEnd = param.y;
		const float decayDistance = VCGetCloudBlockerDecayDistance(decayStart, decayEnd); // to avoid floating point exception.
		const float invDecayDist = 1.0f / decayDistance;
		const float decayInfluence = param.z;
		const float invDecayInfluence = 1.0f - decayInfluence;
		m_blockerParamArray.push_back(Vec4(invDecayDist, decayStart, decayInfluence, invDecayInfluence));
	}
}

void CVolumetricCloudsStage::GenerateCloudBlockerSSList()
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	const auto& blockers = pRenderView->GetCloudBlockers(1);
	auto numBlocker = blockers.size();
	CRY_ASSERT(numBlocker <= CRenderView::MaxCloudBlockerScreenSpaceNum);

	// build screen space blocker list for current frame.
	m_blockerSSPosArray.SetUse(0);
	m_blockerSSParamArray.SetUse(0);

	for (auto& blocker : blockers)
	{
		m_blockerSSPosArray.push_back(Vec4(blocker.position, 0.0f));

		const Vec3& param = blocker.param;
		const float decayIntensity = clamp_tpl(1.0f - param.z, 0.0f, 1.0f) * 10.0f;
		m_blockerSSParamArray.push_back(Vec4(param.y, decayIntensity, 0.0f, 0.0f));
	}
}
