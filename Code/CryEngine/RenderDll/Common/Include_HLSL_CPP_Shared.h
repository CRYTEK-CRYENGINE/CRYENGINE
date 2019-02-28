// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifdef __cplusplus
// C++
	#pragma once
	#pragma pack(push,4)

	#define hlsl_cbuffer(name) struct HLSL_ ## name
	#define hlsl_cbuffer_register(name, reg, slot) \
	  enum { SLOT_ ## name = slot };               \
	  struct HLSL_ ## name

	#define hlsl_uint(member)     uint member
	#define hlsl_float(member)    float member
	#define hlsl_float2(member)   Vec2 member
	#define hlsl_float3(member)   Vec3 member
	#define hlsl_float4(member)   Vec4 member
	#define hlsl_matrix44(member) Matrix44 member
	#define hlsl_matrix34(member) Matrix34 member
#else //__cplusplus
// HLSL
	#define hlsl_cbuffer(name)                      cbuffer name
	#define hlsl_cbuffer_register(name, reg, float) cbuffer name: reg
	#define hlsl_uint(member)                       uint member
	#define hlsl_float(member)                      float member
	#define hlsl_float2(member)                     float2 member
	#define hlsl_float3(member)                     float3 member
	#define hlsl_float4(member)                     float4 member
	#define hlsl_matrix44(member)                   float4x4 member
	#define hlsl_matrix34(member)                   float3x4 member
#endif //__cplusplus

// TODO: include in shaders

hlsl_cbuffer(PerPassConstantBuffer_ShadowGen)
{
	hlsl_float4(CP_ShadowGen_LightPos);
	hlsl_float4(CP_ShadowGen_ViewPos);

	hlsl_float4(CP_ShadowGen_DepthTestBias);
	hlsl_float4(CP_ShadowGen_FrustrumInfo);

	hlsl_float4(CP_ShadowGen_VegetationAlphaClamp);
};

hlsl_cbuffer(PerPassConstantBuffer_Custom)
{
	hlsl_float4(CP_Custom_ViewMode);
};

hlsl_cbuffer_register(PerDrawConstantBuffer_Base, register (b0), 1) // eConstantBufferShaderSlot_PerDraw
{
	hlsl_matrix34(CD_WorldMatrix);
	hlsl_matrix34(CD_PrevWorldMatrix);
	hlsl_float4(CD_CustomData);
	hlsl_float4(CD_CustomData1);
	hlsl_float4(CD_CustomData2);
};

hlsl_cbuffer_register(PerDrawConstantBuffer_TeVe, register (b0), 1) // eConstantBufferShaderSlot_PerDraw
{
	hlsl_matrix34(CD_WorldMatrix);
	hlsl_matrix34(CD_PrevWorldMatrix);
	hlsl_float4(CD_CustomData);
	hlsl_float4(CD_CustomData1);
	// TODO: customdata2 should be added after terrainlayerinfo, make sure a vegetation shader is correctly detected when uploading data to
	// constant buffer
	hlsl_float4(CD_CustomData2);

	hlsl_float4(CD_BlendTerrainColInfo);
	hlsl_matrix44(CD_TerrainLayerInfo);
};

hlsl_cbuffer_register(PerDrawConstantBuffer_Skin, register (b0), 1) // eConstantBufferShaderSlot_PerDraw
{
	hlsl_matrix34(CD_WorldMatrix);
	hlsl_matrix34(CD_PrevWorldMatrix);
	hlsl_float4(CD_CustomData);
	hlsl_float4(CD_CustomData1);
	// TODO: customdata2 should be added after WrinklesMask2. Since constant buffer definition is shared with vegetation, we need this here as well
	hlsl_float4(CD_CustomData2);

	hlsl_float4(CD_SkinningInfo);
	hlsl_float4(CD_WrinklesMask0);
	hlsl_float4(CD_WrinklesMask1);
	hlsl_float4(CD_WrinklesMask2);
};

hlsl_cbuffer_register(PerViewGlobalConstantBuffer, register (b6), 6) //eConstantBufferShaderSlot_PerView
{
	hlsl_matrix44(CV_ViewProjZeroMatr);
	hlsl_float4(CV_AnimGenParams);

	hlsl_matrix44(CV_ViewProjMatr);
	hlsl_matrix44(CV_ViewProjNearestMatr);
	hlsl_matrix44(CV_InvViewProj);
	hlsl_matrix44(CV_PrevViewProjMatr);
	hlsl_matrix44(CV_PrevViewProjNearestMatr);
	hlsl_matrix34(CV_ScreenToWorldBasis);
	hlsl_float4(CV_TessInfo);
	hlsl_float4(CV_WorldViewPosition); // TODO: remove me, data is already available in CV_ScreenToWorldBasis, via GetWorldViewPos
	hlsl_float4(CV_CamRightVector);
	hlsl_float4(CV_CamFrontVector);
	hlsl_float4(CV_CamUpVector);

	hlsl_float4(CV_ScreenSize);
	hlsl_float4(CV_HPosScale);
	hlsl_float4(CV_ProjRatio);
	hlsl_float4(CV_NearestScaled);
	hlsl_float4(CV_NearFarClipDist);

	hlsl_float4(CV_SunLightDir);
	hlsl_float4(CV_SunColor);
	hlsl_float4(CV_SkyColor);
	hlsl_float4(CV_FogColor);
	hlsl_float4(CV_TerrainInfo);

	hlsl_float4(CV_DecalZFightingRemedy);

	hlsl_matrix44(CV_FrustumPlaneEquation);

	hlsl_float4(CV_WindGridOffset);

	hlsl_matrix44(CV_ViewMatr);
	hlsl_matrix44(CV_InvViewMatr);
};

hlsl_cbuffer_register(VrProjectionConstantBuffer, register (b7), 7) // eConstantBufferShaderSlot_VrProjection
{
	hlsl_float4(CVP_GeometryShaderParams)[2];
	hlsl_float4(CVP_ProjectionParams)[12];
	hlsl_float4(CVP_ProjectionParamsOtherEye)[12];
};

struct SLightVolumeInfo
{
	hlsl_float3(wPosition);
	hlsl_float(radius);
	hlsl_float3(cColor);
	hlsl_float(bulbRadius);
	hlsl_float3(wProjectorDirection);
	hlsl_float(projectorCosAngle);
};

struct SLightVolumeRange
{
	hlsl_uint(begin);
	hlsl_uint(end);
};

#ifdef __cplusplus
// C++

	#pragma pack(pop)

#else //__cplusplus
// HLSL

#endif //__cplusplus
