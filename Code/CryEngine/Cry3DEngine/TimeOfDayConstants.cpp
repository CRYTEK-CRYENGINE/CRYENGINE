// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TimeOfDayConstants.h"

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Decorators/Resources.h>

namespace
{

//Get help from CVar values with prefix e_svo/e_svoTI_ for Total Illumination
void AddHelp(Serialization::IArchive& ar, const char* name, const char* refix)
{
	//CVar value = e_svoTI_ + name
	std::string svotiName(refix);
	svotiName += name;
	ICVar* cvar = gEnv->pConsole->GetCVar(svotiName.c_str());
	ar.doc(cvar->GetHelp());
}

template<typename T>
void SerializeSvoTi(Serialization::IArchive& ar, T& val, T min, T max, const char* name, const char* userName, const char* prefix = "e_svoTI_")
{
	ar(yasli::Range<T>(val, min, max), name, userName);
	AddHelp(ar, name, prefix);
}

template<typename T>
void SerializeSvoTi(Serialization::IArchive& ar, T& val, const char* name, const char* userName, const char* prefix = "e_svoTI_")
{
	ar(val, name, userName);
	AddHelp(ar, name, prefix);
}
} // unnamed namespace

//////////////////////////////////////////////////////////////////////////
SunImpl::SunImpl()
{
	ResetVariables();
}

void SunImpl::ResetVariables()
{
	latitude = 240.0f;
	longitude = 90.0f;
	sunLinkedToTOD = true;
}

void SunImpl::Serialize(Serialization::IArchive& ar)
{
	ar(yasli::Range(latitude, 0.0f, 360.0f), "Latitude", "Latitude");
	ar(yasli::Range(longitude, 0.0f, 180.0f), "Longitude", "Longitude");
	ar.doc("NorthPole..Equator..SouthPole");

	ar(sunLinkedToTOD, "SunLinkedToTOD", "Sun Linked to TOD");
}

//////////////////////////////////////////////////////////////////////////
MoonImpl::MoonImpl()
{
	ResetVariables();
}

void MoonImpl::ResetVariables()
{
	latitude = 240.0f;
	longitude = 45.0f;
	size = 0.5f;
	texture = "%ENGINE%/EngineAssets/Textures/Skys/Night/half_moon.dds";
}

void MoonImpl::Serialize(Serialization::IArchive& ar)
{
	ar(yasli::Range(latitude, 0.0f, 360.0f), "Latitude", "Latitude");
	ar(yasli::Range(longitude, 0.0f, 180.0f), "Longitude", "Longitude");
	ar.doc("NorthPole..Equator..SouthPole");
	ar(yasli::Range(size, 0.0f, 1.0f), "Size", "Size");
	ar(Serialization::TextureFilename(texture), "Texture", "Texture");
}

//////////////////////////////////////////////////////////////////////////
WindImpl::WindImpl()
{
	ResetVariables();
}

void WindImpl::ResetVariables()
{
	windVector = Vec3(1.0f, 0.0f, 0.0f);
	breezeGenerationEnabled = false;
	breezeStrength = 1.0f;
	breezeVariance = 1.0f;
	breezeLifeTime = 15.0f;
	breezeCount = 4;
	breezeRadius = 5.0f;
	breezeSpawnRadius = 25.0f;
	breezeSpread = 0.0f;
	breezeMovementSpeed = 8.0f;
	breezeAwakeThreshold = 0.0f;
	breezeFixedHeight = -1.0f;
}

void WindImpl::Serialize(Serialization::IArchive& ar)
{
	ar(windVector, "WindVector", "Wind Vector");
	ar(breezeGenerationEnabled, "BreezeEnabled", "Breeze Enabled");
	ar(breezeStrength, "BreezeStrength", "Breeze Strength");
	ar(breezeVariance, "BreezeVariance", "Breeze Variance");
	ar(breezeLifeTime, "BreezeLifeTime", "Breeze Life Time");
	ar(breezeCount, "BreezeCount", "Breeze Count");
	ar(breezeRadius, "BreezeRadius", "Breeze Radius");
	ar(breezeSpawnRadius, "BreezeSpawnRadius", "Breeze Spawn Radius");
	ar(breezeSpread, "BreezeSpread", "Breeze Spread");
	ar(breezeMovementSpeed, "BreezeMovementSpeed", "Breeze Movement Speed");
	ar(breezeAwakeThreshold, "BreezeAwakeThreshold", "Breeze Awake Threshold");
	ar(breezeFixedHeight, "BreezeFixedHeight", "Breeze Fixed Height");
}

//////////////////////////////////////////////////////////////////////////
CloudShadowsImpl::CloudShadowsImpl()
{
	ResetVariables();
}

void CloudShadowsImpl::ResetVariables()
{
	texture = "";
	speed = Vec3(0.0f, 0.0f, 0.0f);
	tiling = 1.0f;
	brightness = 1.0f;
	invert = false;
}

void CloudShadowsImpl::Serialize(Serialization::IArchive& ar)
{
	ar(Serialization::TextureFilename(texture), "Texture", "Texture");
	ar(speed, "Speed", "Speed");
	ar(tiling, "Tiling", "Tiling");
	ar(brightness, "Brightness", "Brightness");
	ar(invert, "Invert", "Invert");
}

TotalIllumImpl::TotalIllumImpl()
{
	ResetVariables();
}

void TotalIllumImpl::ResetVariables()
{
	active = false;
	injectionMultiplier = 1.0f;
	skyColorMultiplier = 1.0f;
	useTodSkyColor = 0.5f;
	specularAmplifier = 1.0f;
	diffuseBias = 0.05f;
	pointLightsBias = 0.2f;
	coneMaxLength = 12.0f;
	ssaoAmount = 0.7f;
	highGlossOcclusion = 0;
	translucentBrightness = 2.5f;
	minNodeSize = 8.0f;
}

void TotalIllumImpl::Serialize(Serialization::IArchive& ar)
{
	SerializeSvoTi(ar, active, "Active", "Active");
	SerializeSvoTi(ar, injectionMultiplier, 0.0f, 32.0f, "InjectionMultiplier", "Injection Multiplier");
	SerializeSvoTi(ar, skyColorMultiplier, 0.0f, 32.0f, "SkyColorMultiplier", "Sky Color Multiplier");
	SerializeSvoTi(ar, useTodSkyColor, 0.0f, 1.0f, "UseTODSkyColor", "Use TOD Sky Color");
	SerializeSvoTi(ar, specularAmplifier, 0.0f, 10.0f, "SpecularAmplifier", "Specular Amplifier");
	SerializeSvoTi(ar, diffuseBias, -4.0f, 4.0f, "DiffuseBias", "Diffuse Bias");
	SerializeSvoTi(ar, pointLightsBias, 0.0f, 1.0f, "PointLightsBias", "Point Lights Bias");
	SerializeSvoTi(ar, coneMaxLength, 2.0f, 100.0f, "ConeMaxLength", "Cone Max Length");
	SerializeSvoTi(ar, ssaoAmount, 0.0f, 1.0f, "SSAOAmount", "SSAO Amount");
	SerializeSvoTi(ar, highGlossOcclusion, -100.0f, 1.0f, "HighGlossOcclusion", "High Gloss Occlusion");
	SerializeSvoTi(ar, translucentBrightness, 0.0f, 8.0f, "TranslucentBrightness", "Translucent Brightness");
	SerializeSvoTi(ar, minNodeSize, 0.5f, 512.0f, "MinNodeSize", "Min Node Size", "e_svo");
}

//////////////////////////////////////////////////////////////////////////
TotalIllumAdvImpl::TotalIllumAdvImpl()
{
	ResetVariables();
}

void TotalIllumAdvImpl::ResetVariables()
{
	integrationMode = 0;
	portalsDeform = 0.0f;
	portalsInject = 0.0f;
	diffuseAmplifier = 1.0f;
	specularFromDiffuse = false;
	numberOfBounces = 1;
	saturation = 0.8f;
	propagationBooster = 1.5f;
	diffuseConeWidth = 24.0f;
	updateLighting = false;
	updateGeometry = false;
	skipNonGiLights = false;
	forceGIForAllLights = false;
	lowSpecMode = -2;
	halfResKernelPrimary = false;
	halfresKernelSecondary = true;
	useLightProbes = false;
	voxelizationLodRatio = 1;
	voxelizationMapBorder = 0;
	voxelPoolResolution = 128;
	objectsMaxViewDistance = 48;
	sunRSMInject = 0;
	SSDepthTrace = 0;
	shadowsFromSun = false;
	shadowsSoftness = 1.25;
	shadowsFromHeightmap = false;
	troposphere_Active = false;
	troposphere_Brightness = 1.0f;
	troposphere_Ground_Height = 0.0f;
	troposphere_Layer0_Height = 0.0f;
	troposphere_Layer1_Height = 0.0f;
	troposphere_Snow_Height = 0.0f;
	troposphere_Layer0_Rand = 0.0f;
	troposphere_Layer1_Rand = 0.0f;
	troposphere_Layer0_Dens = 0.0f;
	troposphere_Layer1_Dens = 0.0f;
	troposphere_CloudGen_Height = 0.0f;
	troposphere_CloudGen_Freq = 0.0f;
	troposphere_CloudGen_FreqStep = 0.0f;
	troposphere_CloudGen_Scale = 0.0f;
	troposphere_CloudGenTurb_Freq = 0.0f;
	troposphere_CloudGenTurb_Scale = 0.0f;
	troposphere_Density = 1.0f;
	analyticalOccluders = false;
	analyticalGI = false;
	traceVoxels = true;
	rtMaxDist = 0.0f;
	constantAmbientDebug = 0.0f;
	streamVoxels = false;
}

void TotalIllumAdvImpl::Serialize(Serialization::IArchive& ar)
{
	SerializeSvoTi(ar, integrationMode, 0, 2, "IntegrationMode", "Integration Mode");
	SerializeSvoTi(ar, portalsDeform, 0.0f, 1.0f, "PortalsDeform", "Portals Deform");
	SerializeSvoTi(ar, portalsInject, 0.0f, 10.0f, "PortalsInject", "Portals Inject");
	SerializeSvoTi(ar, diffuseAmplifier, 0.0f, 10.0f, "DiffuseAmplifier", "Diffuse Amplifier");
	SerializeSvoTi(ar, specularFromDiffuse, "SpecularFromDiffuse", "Specular From Diffuse");
	SerializeSvoTi(ar, numberOfBounces, 0, 3, "NumberOfBounces", "Number Of Bounces");
	SerializeSvoTi(ar, saturation, 0.0f, 4.0f, "Saturation", "Saturation");
	SerializeSvoTi(ar, propagationBooster, 0.0f, 4.0f, "PropagationBooster", "Propagation Booster");
	SerializeSvoTi(ar, diffuseConeWidth, 1.0f, 36.0f, "DiffuseConeWidth", "Diffuse Cone Width");
	SerializeSvoTi(ar, updateLighting, "UpdateLighting", "Update Lighting");
	SerializeSvoTi(ar, updateGeometry, "UpdateGeometry", "Update Geometry");
	SerializeSvoTi(ar, skipNonGiLights, "SkipNonGILights", "Skip Non GI Lights");
	SerializeSvoTi(ar, forceGIForAllLights, "forceGIForAllLights", "Force GI For All Lights");
	SerializeSvoTi(ar, lowSpecMode, -2, 14, "LowSpecMode", "Low Spec Mode");
	SerializeSvoTi(ar, halfResKernelPrimary, "HalfresKernelPrimary", "Halfres Kernel Primary");
	SerializeSvoTi(ar, halfresKernelSecondary, "HalfresKernelSecondary", "Halfres Kernel Secondary");
	SerializeSvoTi(ar, useLightProbes, "UseLightProbes", "Use Light Probes");
	SerializeSvoTi(ar, voxelizationLodRatio, 0.5f, 14.0f, "VoxelizationLODRatio", "Voxelization LOD Ratio");
	SerializeSvoTi(ar, voxelizationMapBorder, 0, 1000000, "VoxelizationMapBorder", "Voxelization Map Border");
	SerializeSvoTi(ar, voxelPoolResolution, 128, 256, "VoxelPoolResolution", "Voxel Pool Resolution", "e_svo");
	SerializeSvoTi(ar, objectsMaxViewDistance, 0.0f, 128.0f, "ObjectsMaxViewDistance", "Objects Max View Distance");
	SerializeSvoTi(ar, sunRSMInject, 0, 16, "SunRSMInject", "Sun RSM Inject");
	SerializeSvoTi(ar, SSDepthTrace, 0.0f, 1.4f, "SSDepthTrace", "SS Depth Trace");
	SerializeSvoTi(ar, shadowsFromSun, "ShadowsFromSun", "Shadows From Sun");
	SerializeSvoTi(ar, shadowsSoftness, 0.2f, 5.0f, "ShadowsSoftness", "Shadows Softness");
	SerializeSvoTi(ar, shadowsFromHeightmap, "ShadowsFromHeightmap", "Shadows From Heightmap");
	SerializeSvoTi(ar, troposphere_Active, "Troposphere_Active", "Troposphere Active");
	SerializeSvoTi(ar, troposphere_Brightness, "Troposphere_Brightness", "Troposphere Brightness");
	SerializeSvoTi(ar, troposphere_Ground_Height, "Troposphere_Ground_Height", "Troposphere Ground Height");
	SerializeSvoTi(ar, troposphere_Layer0_Height, "Troposphere_Layer0_Height", "Troposphere Layer0 Height");
	SerializeSvoTi(ar, troposphere_Layer1_Height, "Troposphere_Layer1_Height", "Troposphere Layer1 Height");
	SerializeSvoTi(ar, troposphere_Snow_Height, "Troposphere_Snow_Height", "Troposphere Snow Height");
	SerializeSvoTi(ar, troposphere_Layer0_Rand, "Troposphere_Layer0_Rand", "Troposphere Layer0 Rand");
	SerializeSvoTi(ar, troposphere_Layer1_Rand, "Troposphere_Layer1_Rand", "Troposphere Layer1 Rand");
	SerializeSvoTi(ar, troposphere_Layer0_Dens, "Troposphere_Layer0_Dens", "Troposphere Layer0 Dens");
	SerializeSvoTi(ar, troposphere_Layer1_Dens, "Troposphere_Layer1_Dens", "Troposphere Layer1 Dens");
	SerializeSvoTi(ar, troposphere_CloudGen_Height, "Troposphere_CloudGen_Height", "Troposphere CloudGen Height");
	SerializeSvoTi(ar, troposphere_CloudGen_Freq, "Troposphere_CloudGen_Freq", "Troposphere CloudGen Freq");
	SerializeSvoTi(ar, troposphere_CloudGen_FreqStep, "Troposphere_CloudGen_FreqStep", "Troposphere CloudGen FreqStep");
	SerializeSvoTi(ar, troposphere_CloudGen_Scale, "Troposphere_CloudGen_Scale", "Troposphere CloudGen Scale");
	SerializeSvoTi(ar, troposphere_CloudGenTurb_Freq, "Troposphere_CloudGenTurb_Freq", "Troposphere CloudGenTurb Freq");
	SerializeSvoTi(ar, troposphere_CloudGenTurb_Scale, "Troposphere_CloudGenTurb_Scale", "Troposphere CloudGenTurb Scale");
	SerializeSvoTi(ar, troposphere_Density, "Troposphere_Density", "Troposphere Density");
	SerializeSvoTi(ar, analyticalOccluders, "AnalyticalOccluders", "Analytical Occluders");
	SerializeSvoTi(ar, analyticalGI, "AnalyticalGI", "Analytical GI");
	SerializeSvoTi(ar, traceVoxels, "TraceVoxels", "Trace Voxels");
	SerializeSvoTi(ar, rtMaxDist, "RT_MaxDist", "RT Max Dist");
	SerializeSvoTi(ar, constantAmbientDebug, "ConstantAmbientDebug", "Constant Ambient Debug");
	SerializeSvoTi(ar, streamVoxels, "StreamVoxels", "Stream Voxels", "e_svo");
}

//////////////////////////////////////////////////////////////////////////
CTimeOfDayConstants::CTimeOfDayConstants()
{
	ResetVariables();
}

void CTimeOfDayConstants::ResetVariables()
{
	sun.ResetVariables();
	moon.ResetVariables();
	wind.ResetVariables();
	cloudShadows.ResetVariables();
	totalIllumination.ResetVariables();
	totalIlluminationAdvanced.ResetVariables();
}

void CTimeOfDayConstants::Serialize(Serialization::IArchive& ar)
{
	ar(sun, "Sun", "Sun");
	ar(moon, "Moon", "Moon");
	ar(wind, "Wind", "Wind");
	ar(cloudShadows, "CloudShadows", "Cloud Shadows");
	ar(totalIllumination, "TotalIllumination", "Total Illumination");
	ar(totalIlluminationAdvanced, "TotalIlluminationAdv", "Total Illumination Advanced");
}
