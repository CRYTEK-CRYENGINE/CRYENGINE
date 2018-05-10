// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   TimeOfDay.cpp
//  Version:     v1.00
//  Created:     25/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TimeOfDay.h"
#include "terrain_water.h"
#include <CryMath/ISplines.h>
#include <CryNetwork/IRemoteCommand.h>
#include <CryGame/IGameFramework.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/IArchiveHost.h>
#include "EnvironmentPreset.h"

#define SERIALIZATION_ENUM_DEFAULTNAME(x) SERIALIZATION_ENUM(ITimeOfDay::x, # x, # x)

SERIALIZATION_ENUM_BEGIN_NESTED(ITimeOfDay, ETimeOfDayParamID, "ETimeOfDayParamID")
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_INTENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_SPECULAR_MULTIPLIER)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_FOG_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_FOG_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_HEIGHT)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_DENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_FOG_COLOR2)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_FOG_COLOR2_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_HEIGHT2)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_DENSITY2)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_HEIGHT_OFFSET)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_FOG_RADIAL_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_FOG_RADIAL_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_RADIAL_SIZE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_RADIAL_LOBE)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_FINAL_DENSITY_CLAMP)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_GLOBAL_DENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_RAMP_START)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_RAMP_END)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_RAMP_INFLUENCE)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_SHADOW_DARKENING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_SHADOW_DARKENING_SUN)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_SHADOW_DARKENING_AMBIENT)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG_SHADOW_RANGE)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_HEIGHT)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_DENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_HEIGHT2)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_DENSITY2)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_GLOBAL_DENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_RAMP_START)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_RAMP_END)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_COLOR1)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_ANISOTROPIC1)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_COLOR2)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_ANISOTROPIC2)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_BLEND_FACTOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_BLEND_MODE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_ANISOTROPIC)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_RANGE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_INSCATTER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_EXTINCTION)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_GLOBAL_FOG_VISIBILITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLFOG2_FINAL_DENSITY_CLAMP)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_SUN_INTENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_SUN_INTENSITY_MULTIPLIER)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_KM)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_KR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_G)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_WAVELENGTH_R)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_WAVELENGTH_G)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYLIGHT_WAVELENGTH_B)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_HORIZON_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_HORIZON_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_ZENITH_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_ZENITH_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_ZENITH_SHIFT)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_START_INTENSITY)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_INNERCORONA_SCALE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_NIGHSKY_MOON_OUTERCORONA_SCALE)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_CLOUDSHADING_SUNLIGHT_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_CLOUDSHADING_SKYLIGHT_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR_INFLUENCE)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_GLOBAL_DENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_HEIGHT)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_THICKNESS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_CLOUD_EDGE_TURBULENCE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_CLOUD_EDGE_THRESHOLD)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_SUN_SINGLE_SCATTERING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_SUN_LOW_ORDER_SCATTERING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_SUN_LOW_ORDER_SCATTERING_ANISTROPY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_SUN_HIGH_ORDER_SCATTERING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_SKY_LIGHTING_SCATTERING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_GOUND_LIGHTING_SCATTERING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_GROUND_LIGHTING_ALBEDO)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_MULTI_SCATTERING_ATTENUATION)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_MULTI_SCATTERING_PRESERVATION)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_POWDER_EFFECT)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_ABSORPTION)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_ATMOSPHERIC_ALBEDO)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_VOLCLOUD_WIND_INFLUENCE)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_SHAFTS_VISIBILITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_RAYS_VISIBILITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_RAYS_ATTENUATION)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_RAYS_SUNCOLORINFLUENCE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_RAYS_CUSTOMCOLOR)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_OCEANFOG_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_OCEANFOG_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_OCEANFOG_DENSITY)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYBOX_MULTIPLIER)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_FILMCURVE_SHOULDER_SCALE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_FILMCURVE_LINEAR_SCALE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_FILMCURVE_TOE_SCALE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_FILMCURVE_WHITEPOINT)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_COLORGRADING_COLOR_SATURATION)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_COLORGRADING_COLOR_BALANCE)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_EYEADAPTATION_SCENEKEY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_EYEADAPTATION_MIN_EXPOSURE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_EYEADAPTATION_MAX_EXPOSURE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_EYEADAPTATION_EV_MIN)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_EYEADAPTATION_EV_MAX)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_EYEADAPTATION_EV_AUTO_COMPENSATION)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_BLOOM_AMOUNT)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_COLORGRADING_FILTERS_GRAIN)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_COLORGRADING_DOF_FOCUSRANGE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_COLORGRADING_DOF_BLURAMOUNT)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC0_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC0_SLOPE_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC1_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC1_SLOPE_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC2_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC2_SLOPE_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC3_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC3_SLOPE_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC4_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC4_SLOPE_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC5_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC5_SLOPE_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC6_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC6_SLOPE_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC7_BIAS)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOWSC7_SLOPE_BIAS)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SHADOW_JITTERING)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_HDR_DYNAMIC_POWER_FACTOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_TERRAIN_OCCL_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_GI_MULTIPLIER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_COLOR_MULTIPLIER)
SERIALIZATION_ENUM_END()
#undef SERIALIZATION_ENUM_DEFAULTNAME

SERIALIZATION_ENUM_BEGIN_NESTED(ITimeOfDay, EVariableType, "ETimeOfDayVariableType")
SERIALIZATION_ENUM(ITimeOfDay::TYPE_FLOAT, "TYPE_FLOAT", "TYPE_FLOAT")
SERIALIZATION_ENUM(ITimeOfDay::TYPE_COLOR, "TYPE_COLOR", "TYPE_COLOR")
SERIALIZATION_ENUM_END()

//////////////////////////////////////////////////////////////////////////

class CBezierSplineFloat : public spline::CSplineKeyInterpolator<spline::BezierKey<float>>
{
public:
	float m_fMinValue;
	float m_fMaxValue;

	virtual spline::Formatting GetFormatting() const override
	{
		return ",::";
	}
};

//////////////////////////////////////////////////////////////////////////
class CBezierSplineVec3 : public spline::CSplineKeyInterpolator<spline::BezierKey<Vec3>>
{
public:

	void ClampValues(float fMinValue, float fMaxValue)
	{
		for (int i = 0, nkeys = num_keys(); i < nkeys; i++)
		{
			ValueType val;
			if (GetKeyValue(i, val))
			{
				//val[0] = clamp_tpl(val[0],fMinValue,fMaxValue);
				//val[1] = clamp_tpl(val[1],fMinValue,fMaxValue);
				//val[2] = clamp_tpl(val[2],fMinValue,fMaxValue);
				SetKeyValue(i, val);
			}
		}
	}
	virtual spline::Formatting GetFormatting() const override
	{
		return ",::";
	}
};

//////////////////////////////////////////////////////////////////////////

namespace
{
// for compatibility with the old ISplines.h format
SBezierControlPoint::ETangentType ESplineKeyTangentTypeToETangentType(ESplineKeyTangentType type)
{
	return SBezierControlPoint::ETangentType(type);
}

void ReadCTimeOfDayVariableFromXmlNode(CTimeOfDayVariable* pVar, XmlNodeRef varNode)
{
	CTimeOfDayVariable& var = *pVar;
	XmlNodeRef splineNode = varNode->findChild("Spline");
	if (var.GetType() == ITimeOfDay::TYPE_FLOAT)
	{
		CBezierSpline* pSpline = var.GetSpline(0);

		CBezierSplineFloat floatSpline;
		floatSpline.SerializeSpline(splineNode, true);

		ISplineInterpolator::ValueType fValue;
		const int nKeyCount = floatSpline.GetKeyCount();
		pSpline->Resize(nKeyCount);
		for (int k = 0; k < nKeyCount; ++k)
		{
			const float fKeyTime = floatSpline.GetKeyTime(k) * CEnvironmentPreset::GetAnimTimeSecondsIn24h();

			ISplineInterpolator::ZeroValue(fValue);
			floatSpline.GetKeyValue(k, fValue);

			float fTangentsIn[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			float fTangentsOut[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			floatSpline.GetKeyTangents(k, fTangentsIn, fTangentsOut);

			const int nKeyFlags = floatSpline.GetKeyFlags(k);
			ESplineKeyTangentType inTangentType = ESplineKeyTangentType((nKeyFlags & SPLINE_KEY_TANGENT_IN_MASK) >> SPLINE_KEY_TANGENT_IN_SHIFT);
			ESplineKeyTangentType outTangentType = ESplineKeyTangentType((nKeyFlags & SPLINE_KEY_TANGENT_OUT_MASK) >> SPLINE_KEY_TANGENT_OUT_SHIFT);

			SBezierKey& key = pSpline->GetKey(k);
			key.m_controlPoint.m_value = fValue[0];
			key.m_time = SAnimTime(fKeyTime);
			key.m_controlPoint.m_inTangentType = ESplineKeyTangentTypeToETangentType(inTangentType);
			key.m_controlPoint.m_inTangent = Vec2(fTangentsIn[0], fTangentsIn[1]);
			key.m_controlPoint.m_outTangentType = ESplineKeyTangentTypeToETangentType(outTangentType);
			key.m_controlPoint.m_outTangent = Vec2(fTangentsOut[0], fTangentsOut[1]);
		}   //for k
	}
	else if (var.GetType() == ITimeOfDay::TYPE_COLOR)
	{
		CBezierSplineVec3 colorSpline;
		colorSpline.SerializeSpline(splineNode, true);
		colorSpline.ClampValues(-100, 100);

		ISplineInterpolator::ValueType fValue;
		const int nKeyCount = colorSpline.GetKeyCount();

		var.GetSpline(0)->Resize(nKeyCount);
		var.GetSpline(1)->Resize(nKeyCount);
		var.GetSpline(2)->Resize(nKeyCount);
		for (int k = 0; k < nKeyCount; ++k)
		{
			float fKeyTime = colorSpline.GetKeyTime(k) * CEnvironmentPreset::GetAnimTimeSecondsIn24h();

			ISplineInterpolator::ZeroValue(fValue);
			colorSpline.GetKeyValue(k, fValue);

			float fTangentsIn[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			float fTangentsOut[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			colorSpline.GetKeyTangents(k, fTangentsIn, fTangentsOut);

			const int nKeyFlags = colorSpline.GetKeyFlags(k);
			ESplineKeyTangentType inTangentType = ESplineKeyTangentType((nKeyFlags & SPLINE_KEY_TANGENT_IN_MASK) >> SPLINE_KEY_TANGENT_IN_SHIFT);
			ESplineKeyTangentType outTangentType = ESplineKeyTangentType((nKeyFlags & SPLINE_KEY_TANGENT_OUT_MASK) >> SPLINE_KEY_TANGENT_OUT_SHIFT);

			SBezierKey key;
			key.m_time = SAnimTime(fKeyTime);
			key.m_controlPoint.m_inTangentType = ESplineKeyTangentTypeToETangentType(inTangentType);
			key.m_controlPoint.m_inTangent = Vec2(fTangentsIn[0], fTangentsIn[1]);
			key.m_controlPoint.m_outTangentType = ESplineKeyTangentTypeToETangentType(outTangentType);
			key.m_controlPoint.m_outTangent = Vec2(fTangentsOut[0], fTangentsOut[1]);

			key.m_controlPoint.m_value = fValue[0];
			var.GetSpline(0)->GetKey(k) = key;
			key.m_controlPoint.m_value = fValue[1];
			var.GetSpline(1)->GetKey(k) = key;
			key.m_controlPoint.m_value = fValue[2];
			var.GetSpline(2)->GetKey(k) = key;
		}   //for k
	}
}

void ReadPresetFromToDXMLNode(CEnvironmentPreset& pPreset, XmlNodeRef node)
{
	const int nChildCount = node->getChildCount();
	for (int i = 0; i < nChildCount; ++i)
	{
		XmlNodeRef varNode = node->getChild(i);
		const char* varName = varNode->getAttr("Name");
		if (!varName)
			continue;

		CTimeOfDayVariable* pVar = pPreset.GetVar(varName);
		if (!pVar)
			continue;

		ReadCTimeOfDayVariableFromXmlNode(pVar, varNode);
	} // for i
}   //void ReadWeatherPresetFromToDXMLNode

void MigrateLegacyData(CEnvironmentPreset& preset, bool bSunIntensity)
{
	if (bSunIntensity)    // Convert sun intensity as specified up to CE 3.8.2 to illuminance
	{
		CTimeOfDayVariable* varSunIntensity = preset.GetVar(ITimeOfDay::PARAM_SUN_INTENSITY);
		CTimeOfDayVariable* varSunMult = preset.GetVar(ITimeOfDay::PARAM_SUN_COLOR_MULTIPLIER);
		CTimeOfDayVariable* varSunColor = preset.GetVar(ITimeOfDay::PARAM_SUN_COLOR);
		CTimeOfDayVariable* varHDRPower = preset.GetVar(ITimeOfDay::PARAM_HDR_DYNAMIC_POWER_FACTOR);

		CBezierSpline* varSunMultSpline = varSunMult->GetSpline(0);
		const int numKeys = varSunMultSpline->GetKeyCount();
		for (int key = 0; key < numKeys; ++key)
		{
			SBezierKey& varSunMultKey = varSunMultSpline->GetKey(key);
			SAnimTime anim_time = varSunMultKey.m_time;
			float time = anim_time.ToFloat();
			float sunMult = varSunMultKey.m_controlPoint.m_value;

			float sunColorR = varSunColor->GetSpline(0)->Evaluate(time);
			float sunColorG = varSunColor->GetSpline(1)->Evaluate(time);
			float sunColorB = varSunColor->GetSpline(2)->Evaluate(time);

			float hdrPower = varHDRPower->GetSpline(0)->Evaluate(time);

			float hdrMult = powf(HDRDynamicMultiplier, hdrPower);
			float sunColorLum = sunColorR * 0.2126f + sunColorG * 0.7152f + sunColorB * 0.0722f;
			float sunIntensity = sunMult * sunColorLum * hdrMult * 10000.0f * gf_PI;

			varSunIntensity->GetSpline(0)->InsertKey(anim_time, sunIntensity);
		}
	}
}

void LoadPresetFromOldFormatXML(CEnvironmentPreset& preset, XmlNodeRef node)
{
	ReadPresetFromToDXMLNode(preset, node);

	bool bSunIntensityFound = false;
	const int nChildCount = node->getChildCount();
	for (int i = 0; i < nChildCount; ++i)
	{
		XmlNodeRef varNode = node->getChild(i);
		const char* varName = varNode->getAttr("Name");
		if (!varName)
			continue;

		if (!strcmp(varName, "Sun intensity"))
			bSunIntensityFound = true;

		// copy data from old node to new node if old nodes exist.
		// this needs to maintain compatibility from CE 3.8.2 to above.
		// this should be removed in the future.
		if (!strcmp(varName, "Volumetric fog 2: Fog albedo color"))
		{
			ReadCTimeOfDayVariableFromXmlNode(preset.GetVar(ITimeOfDay::PARAM_VOLFOG2_COLOR1), varNode);
			ReadCTimeOfDayVariableFromXmlNode(preset.GetVar(ITimeOfDay::PARAM_VOLFOG2_COLOR2), varNode);
			ReadCTimeOfDayVariableFromXmlNode(preset.GetVar(ITimeOfDay::PARAM_VOLFOG2_COLOR), varNode);
		}
		if (!strcmp(varName, "Volumetric fog 2: Anisotropic factor"))
		{
			ReadCTimeOfDayVariableFromXmlNode(preset.GetVar(ITimeOfDay::PARAM_VOLFOG2_ANISOTROPIC1), varNode);
			ReadCTimeOfDayVariableFromXmlNode(preset.GetVar(ITimeOfDay::PARAM_VOLFOG2_ANISOTROPIC), varNode);
		}
	}
	MigrateLegacyData(preset, !bSunIntensityFound);
}

static const char* sPresetsLibsPath = "libs/environmentpresets/";
static const char* sPresetXMLRootNodeName = "EnvironmentPreset";

string GetPresetXMLFilenamefromName(const string& presetName, const bool bForWriting)
{
	//const string filePath = sPresetsLibsPath + presetName + ".xml";
	const string filePath = presetName;

	char szAdjustedFile[ICryPak::g_nMaxPath];
	const char* szTemp = gEnv->pCryPak->AdjustFileName(filePath.c_str(), szAdjustedFile, bForWriting ? ICryPak::FLAGS_FOR_WRITING : 0);
	return string(szAdjustedFile);
}

bool SavePresetToXML(const CEnvironmentPreset& preset, const string& presetName)
{
	const string filename = GetPresetXMLFilenamefromName(presetName, false);

	if (!Serialization::SaveXmlFile(filename.c_str(), preset, sPresetXMLRootNodeName))
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "TimeOfDay: Failed to save preset: %s", presetName.c_str());
		return false;
	}

	return true;
}

bool LoadPresetFromXML(CEnvironmentPreset& pPreset, const string& presetName)
{
	const string filename = GetPresetXMLFilenamefromName(presetName, false);
	const bool bFileExist = gEnv->pCryPak->IsFileExist(filename.c_str());
	if (!bFileExist)
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "TimeOfDay: Failed to load preset: %s", presetName.c_str());
		return true;   // return true, so preset stays in list. Sandbox will indicate that no file found
	}

	Serialization::LoadXmlFile(pPreset, filename.c_str());
	return true;
}

Vec3 ConvertIlluminanceToLightColor(float illuminance, Vec3 colorRGB)
{
	illuminance /= RENDERER_LIGHT_UNIT_SCALE;

	ColorF color(colorRGB);
	color.adjust_luminance(illuminance);

	// Divide by PI as this is not done in the BRDF at the moment
	Vec3 finalColor = color.toVec3() / gf_PI;

	return finalColor;
}
}

//////////////////////////////////////////////////////////////////////////
CTimeOfDay::CTimeOfDay()
	: m_timeOfDayRtpcId(CryAudio::InvalidControlId)
	, m_listeners(16)
{
	m_pTimer = 0;
	SetTimer(gEnv->pTimer);
	m_fTime = 12;
	m_bEditMode = false;
	m_advancedInfo.fAnimSpeed = 0;
	m_advancedInfo.fStartTime = 0;
	m_advancedInfo.fEndTime = 24;
	m_fHDRMultiplier = 1.f;
	m_pTimeOfDaySpeedCVar = gEnv->pConsole->GetCVar("e_TimeOfDaySpeed");
	m_sunRotationLatitude = 0;
	m_sunRotationLongitude = 0;
	m_bPaused = false;
	m_bSunLinkedToTOD = true;
	memset(m_vars, 0, sizeof(SVariableInfo) * ITimeOfDay::PARAM_TOTAL);

	// fill local var list so, sandbox can access var list without level being loaded
	CEnvironmentPreset tempPreset;
	for (int i = 0; i < PARAM_TOTAL; ++i)
	{
		const CTimeOfDayVariable* presetVar = tempPreset.GetVar((ETimeOfDayParamID)i);
		SVariableInfo& var = m_vars[i];

		var.name = presetVar->GetName();
		var.displayName = presetVar->GetDisplayName();
		var.group = presetVar->GetGroupName();

		var.nParamId = i;
		var.type = presetVar->GetType();
		var.pInterpolator = NULL;

		Vec3 presetVal = presetVar->GetValue();
		var.fValue[0] = presetVal.x;
		const EVariableType varType = presetVar->GetType();
		if (varType == TYPE_FLOAT)
		{
			var.fValue[1] = presetVar->GetMinValue();
			var.fValue[2] = presetVar->GetMaxValue();
		}
		else if (varType == TYPE_COLOR)
		{
			var.fValue[1] = presetVal.y;
			var.fValue[2] = presetVal.z;
		}
	}

	m_pCurrentPreset = NULL;

	m_timeOfDayRtpcId = CryAudio::StringToId("time_of_day");
}

//////////////////////////////////////////////////////////////////////////
CTimeOfDay::~CTimeOfDay()
{
	//delete m_pCurrentPreset;
}

void CTimeOfDay::SetTimer(ITimer* pTimer)
{
	assert(pTimer);
	m_pTimer = pTimer;

	// Update timer for ocean also - Craig
	COcean::SetTimer(pTimer);
}

ITimeOfDay::SVariableInfo& CTimeOfDay::GetVar(ETimeOfDayParamID id)
{
	assert(id == m_vars[id].nParamId);
	return(m_vars[id]);
}

bool CTimeOfDay::GetPresetsInfos(SPresetInfo* resultArray, unsigned int arraySize) const
{
	if (arraySize < m_presets.size())
		return false;

	size_t i = 0;
	for (TPresetsSet::const_iterator it = m_presets.begin(); it != m_presets.end(); ++it)
	{
		resultArray[i].m_pName = it->first.c_str();
		resultArray[i].m_bCurrent = (m_pCurrentPreset == &(it->second));
		++i;
	}

	return true;
}

bool CTimeOfDay::SetCurrentPreset(const char* szPresetName)
{
	TPresetsSet::iterator it = m_presets.find(szPresetName);
	if (it != m_presets.end())
	{
		CEnvironmentPreset* newPreset = &(it->second);
		if (m_pCurrentPreset != newPreset)
		{
			m_pCurrentPreset = newPreset;
			m_currentPresetName = szPresetName;
			Update(true, true);
			NotifyOnChange(IListener::EChangeType::CurrentPresetChanged, szPresetName);
		}
		return true;
	}
	return false;
}

const char* CTimeOfDay::GetCurrentPresetName() const
{
	return m_currentPresetName.c_str();
}

bool CTimeOfDay::AddNewPreset(const char* szPresetName)
{
	TPresetsSet::const_iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
	{
		string presetFileName = GetPresetXMLFilenamefromName(szPresetName, true);

		std::pair<TPresetsSet::iterator, bool> insertResult = m_presets.emplace(string(szPresetName), CEnvironmentPreset());
		if (insertResult.second)
		{
			m_pCurrentPreset = &(insertResult.first->second);
			m_currentPresetName = szPresetName;
			Update(true, true);
			NotifyOnChange(IListener::EChangeType::PresetAdded, szPresetName);
		}
		return true;
	}
	return false;
}

bool CTimeOfDay::RemovePreset(const char* szPresetName)
{
	TPresetsSet::const_iterator it = m_presets.find(szPresetName);
	if (it != m_presets.end())
	{
		if (m_pCurrentPreset == &(it->second))
		{
			m_pCurrentPreset = NULL;
			m_currentPresetName.clear();
		}

		m_presets.erase(it);

		if (!m_pCurrentPreset && m_presets.size())
		{
			const auto it = m_presets.begin();
			m_pCurrentPreset = &(it->second);
			m_currentPresetName = it->first;
		}

		Update(true, true);
		NotifyOnChange(IListener::EChangeType::PresetRemoved, szPresetName);
	}
	return true;
}

bool CTimeOfDay::SavePreset(const char* szPresetName) const
{
	const string sPresetName(szPresetName);
	TPresetsSet::const_iterator it = m_presets.find(sPresetName);
	if (it != m_presets.end())
	{
		const bool bResult = SavePresetToXML(it->second, sPresetName);
		return bResult;
	}
	return false;
}

bool CTimeOfDay::LoadPreset(const char* szFilePath)
{
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(szFilePath);
	if (root)
	{
		const string path(szFilePath);

		TPresetsSet::const_iterator it = m_presets.find(path);
		if (it != m_presets.end())
			return false;

		std::pair<TPresetsSet::iterator, bool> insertResult = m_presets.emplace(path, CEnvironmentPreset());
		CEnvironmentPreset& preset = insertResult.first->second;
		if (root->isTag(sPresetXMLRootNodeName))
		{
			Serialization::LoadXmlFile(preset, szFilePath);
		}
		else
		{
			//try to load old format
			LoadPresetFromOldFormatXML(preset, root);
		}

		m_pCurrentPreset = &preset;
		m_currentPresetName = insertResult.first->first;
		Update(true, true);
		NotifyOnChange(IListener::EChangeType::PresetLoaded, m_currentPresetName.c_str());
		return true;
	}

	return false;
}

void CTimeOfDay::ResetPreset(const char* szPresetName)
{
	TPresetsSet::iterator it = m_presets.find(szPresetName);
	if (it != m_presets.end())
	{
		it->second.ResetVariables();
		Update(true, true);
	}
}

bool CTimeOfDay::ImportPreset(const char* szPresetName, const char* szFilePath)
{
	TPresetsSet::iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
		return false;

	XmlNodeRef root = GetISystem()->LoadXmlFromFile(szFilePath);
	if (!root)
		return false;

	CEnvironmentPreset& preset = it->second;
	if (root->isTag(sPresetXMLRootNodeName))
	{
		Serialization::LoadXmlFile(preset, szFilePath);
	}
	else
	{
		LoadPresetFromOldFormatXML(preset, root);
	}
	//preset.SetDirty(true);
	Update(true, true);

	return true;
}

bool CTimeOfDay::ExportPreset(const char* szPresetName, const char* szFilePath) const
{
	const string sPresetName(szPresetName);
	TPresetsSet::const_iterator it = m_presets.find(sPresetName);
	if (it != m_presets.end())
	{
		if (!Serialization::SaveXmlFile(szFilePath, it->second, sPresetXMLRootNodeName))
		{
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "TimeOfDay: Failed to save preset: %s", szFilePath);
			return false;
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTimeOfDay::GetVariableInfo(int nIndex, SVariableInfo& varInfo)
{

	if (nIndex < 0 || nIndex >= (int)ITimeOfDay::PARAM_TOTAL)
		return false;

	varInfo = m_vars[nIndex];
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SetVariableValue(int nIndex, float fValue[3])
{
	if (nIndex < 0 || nIndex >= (int)ITimeOfDay::PARAM_TOTAL)
		return;

	m_vars[nIndex].fValue[0] = fValue[0];
	m_vars[nIndex].fValue[1] = fValue[1];
	m_vars[nIndex].fValue[2] = fValue[2];
}
//////////////////////////////////////////////////////////////////////////

bool CTimeOfDay::InterpolateVarInRange(int nIndex, float fMin, float fMax, unsigned int nCount, Vec3* resultArray) const
{
	if (nIndex >= 0 && nIndex < ITimeOfDay::PARAM_TOTAL && m_pCurrentPreset)
	{
		m_pCurrentPreset->InterpolateVarInRange((ITimeOfDay::ETimeOfDayParamID)nIndex, fMin, fMax, nCount, resultArray);
		return true;
	}

	return false;
}

uint CTimeOfDay::GetSplineKeysCount(int nIndex, int nSpline) const
{
	if (nIndex >= 0 && nIndex < ITimeOfDay::PARAM_TOTAL && m_pCurrentPreset)
	{
		CTimeOfDayVariable* pVar = m_pCurrentPreset->GetVar((ITimeOfDay::ETimeOfDayParamID)nIndex);
		return pVar->GetSplineKeyCount(nSpline);
	}
	return 0;
}

bool CTimeOfDay::GetSplineKeysForVar(int nIndex, int nSpline, SBezierKey* keysArray, unsigned int keysArraySize) const
{
	if (nIndex >= 0 && nIndex < ITimeOfDay::PARAM_TOTAL && m_pCurrentPreset)
	{
		CTimeOfDayVariable* pVar = m_pCurrentPreset->GetVar((ITimeOfDay::ETimeOfDayParamID)nIndex);
		return pVar->GetSplineKeys(nSpline, keysArray, keysArraySize);
	}

	return false;
}

bool CTimeOfDay::SetSplineKeysForVar(int nIndex, int nSpline, const SBezierKey* keysArray, unsigned int keysArraySize)
{
	if (nIndex >= 0 && nIndex < ITimeOfDay::PARAM_TOTAL && m_pCurrentPreset)
	{
		CTimeOfDayVariable* pVar = m_pCurrentPreset->GetVar((ITimeOfDay::ETimeOfDayParamID)nIndex);
		const bool bResult = pVar->SetSplineKeys(nSpline, keysArray, keysArraySize);
		return bResult;
	}
	return false;
}

bool CTimeOfDay::UpdateSplineKeyForVar(int nIndex, int nSpline, float fTime, float newValue)
{
	if (nIndex >= 0 && nIndex < ITimeOfDay::PARAM_TOTAL && m_pCurrentPreset)
	{
		CTimeOfDayVariable* pVar = m_pCurrentPreset->GetVar((ITimeOfDay::ETimeOfDayParamID)nIndex);
		const bool bResult = pVar->UpdateSplineKeyForTime(nSpline, fTime, newValue);
		return bResult;
	}
	return false;
}

float CTimeOfDay::GetAnimTimeSecondsIn24h()
{
	return CEnvironmentPreset::GetAnimTimeSecondsIn24h();
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::ResetVariables()
{
	if (!m_pCurrentPreset)
		return;

	m_pCurrentPreset->ResetVariables();

	for (int i = 0; i < PARAM_TOTAL; ++i)
	{
		const CTimeOfDayVariable* presetVar = m_pCurrentPreset->GetVar((ETimeOfDayParamID)i);
		SVariableInfo& var = m_vars[i];

		var.name = presetVar->GetName();
		var.displayName = presetVar->GetDisplayName();
		var.group = presetVar->GetGroupName();

		var.nParamId = i;
		var.type = presetVar->GetType();
		var.pInterpolator = NULL;

		Vec3 presetVal = presetVar->GetValue();
		var.fValue[0] = presetVal.x;
		const EVariableType varType = presetVar->GetType();
		if (varType == TYPE_FLOAT)
		{
			var.fValue[1] = presetVar->GetMinValue();
			var.fValue[2] = presetVar->GetMaxValue();
		}
		else if (varType == TYPE_COLOR)
		{
			var.fValue[1] = presetVal.y;
			var.fValue[2] = presetVal.z;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SetEnvironmentSettings(const SEnvironmentInfo& envInfo)
{
	m_sunRotationLongitude = envInfo.sunRotationLongitude;
	m_sunRotationLatitude = envInfo.sunRotationLatitude;
	m_bSunLinkedToTOD = envInfo.bSunLinkedToTOD;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SaveInternalState(struct IDataWriteStream& writer)
{
	// current time
	writer.WriteFloat(GetTime());

	// TOD data
	string todXML;
	XmlNodeRef xmlNode = GetISystem()->CreateXmlNode("TimeOfDay");
	if (xmlNode)
	{
		Serialize(xmlNode, false);
		todXML = xmlNode->getXML().c_str();
	}
	writer.WriteString(todXML);
}

void CTimeOfDay::LoadInternalState(struct IDataReadStream& reader)
{
	// Load time data
	const float timeOfDay = reader.ReadFloat();

	// Load TOD data
	std::vector<int8> timeOfDatXML;
	reader << timeOfDatXML;
	if (!timeOfDatXML.empty())
	{
		XmlNodeRef node = gEnv->pSystem->LoadXmlFromBuffer((const char*)&timeOfDatXML[0], timeOfDatXML.size());
		if (node)
		{
			Serialize(node, true);
			Update(true, true);
			SetTime(timeOfDay, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTimeOfDay::RegisterListenerImpl(IListener* const pListener, const char* const szDbgName, const bool staticName)
{
	return m_listeners.Add(pListener, szDbgName, staticName);
}

void CTimeOfDay::UnRegisterListenerImpl(IListener* const pListener)
{
	m_listeners.Remove(pListener);
}

//////////////////////////////////////////////////////////////////////////
// Time of day is specified in hours.
void CTimeOfDay::SetTime(float fHour, bool bForceUpdate)
{
	// set new time
	m_fTime = fHour;

	// Change time variable.
	Cry3DEngineBase::GetCVars()->e_TimeOfDay = m_fTime;

	Update(true, bForceUpdate);

	// Inform audio of this change.
	if (m_timeOfDayRtpcId != CryAudio::InvalidControlId)
	{
		gEnv->pAudioSystem->SetParameter(m_timeOfDayRtpcId, m_fTime);
	}

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_TIME_OF_DAY_SET, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SetSunPos(float longitude, float latitude)
{
	m_sunRotationLongitude = longitude;
	m_sunRotationLatitude = latitude;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Update(bool bInterpolate, bool bForceUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);

	if (bInterpolate)
	{
		// normalized time for interpolation
		float t = m_fTime / 24.0f;

		if (m_pCurrentPreset)
		{
			m_pCurrentPreset->Update(t);

			for (int i = 0; i < PARAM_TOTAL; ++i)
			{
				const CTimeOfDayVariable* pVar = m_pCurrentPreset->GetVar((ETimeOfDayParamID)i);
				const Vec3 varValue = pVar->GetValue();
				float* pDst = m_vars[i].fValue;
				pDst[0] = varValue.x;
				if (pVar->GetType() == TYPE_COLOR)
				{
					pDst[1] = varValue.y;
					pDst[2] = varValue.z;
				}
			}
		}
	}//if(bInterpolate)

	// update environment lighting according to new interpolated values
	UpdateEnvLighting(bForceUpdate);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::UpdateEnvLighting(bool forceUpdate)
{
	C3DEngine* p3DEngine((C3DEngine*)gEnv->p3DEngine);
	IRenderer* pRenderer(gEnv->pRenderer);
	const float fRecip255 = 1.0f / 255.0f;

	if (pRenderer)
	{
		bool bHDRModeEnabled = false;
		pRenderer->EF_Query(EFQ_HDRModeEnabled, bHDRModeEnabled);
		if (bHDRModeEnabled)
		{
			m_fHDRMultiplier = powf(HDRDynamicMultiplier, GetVar(PARAM_HDR_DYNAMIC_POWER_FACTOR).fValue[0]);

			const Vec3 vEyeAdaptationParams(GetVar(PARAM_HDR_EYEADAPTATION_EV_MIN).fValue[0],
			                                GetVar(PARAM_HDR_EYEADAPTATION_EV_MAX).fValue[0],
			                                GetVar(PARAM_HDR_EYEADAPTATION_EV_AUTO_COMPENSATION).fValue[0]);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_EYEADAPTATION_PARAMS, vEyeAdaptationParams);

			const Vec3 vEyeAdaptationParamsLegacy(GetVar(PARAM_HDR_EYEADAPTATION_SCENEKEY).fValue[0],
			                                      GetVar(PARAM_HDR_EYEADAPTATION_MIN_EXPOSURE).fValue[0],
			                                      GetVar(PARAM_HDR_EYEADAPTATION_MAX_EXPOSURE).fValue[0]);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_EYEADAPTATION_PARAMS_LEGACY, vEyeAdaptationParamsLegacy);

			float fHDRShoulderScale(GetVar(PARAM_HDR_FILMCURVE_SHOULDER_SCALE).fValue[0]);
			float fHDRMidtonesScale(GetVar(PARAM_HDR_FILMCURVE_LINEAR_SCALE).fValue[0]);
			float fHDRToeScale(GetVar(PARAM_HDR_FILMCURVE_TOE_SCALE).fValue[0]);
			float fHDRWhitePoint(GetVar(PARAM_HDR_FILMCURVE_WHITEPOINT).fValue[0]);

			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_SHOULDER_SCALE, Vec3(fHDRShoulderScale, 0, 0));
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_LINEAR_SCALE, Vec3(fHDRMidtonesScale, 0, 0));
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_TOE_SCALE, Vec3(fHDRToeScale, 0, 0));
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_WHITEPOINT, Vec3(fHDRWhitePoint, 0, 0));

			float fHDRBloomAmount(GetVar(PARAM_HDR_BLOOM_AMOUNT).fValue[0]);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_BLOOM_AMOUNT, Vec3(fHDRBloomAmount, 0, 0));

			float fHDRSaturation(GetVar(PARAM_HDR_COLORGRADING_COLOR_SATURATION).fValue[0]);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_COLORGRADING_COLOR_SATURATION, Vec3(fHDRSaturation, 0, 0));

			Vec3 vColorBalance(GetVar(PARAM_HDR_COLORGRADING_COLOR_BALANCE).fValue[0],
			                   GetVar(PARAM_HDR_COLORGRADING_COLOR_BALANCE).fValue[1],
			                   GetVar(PARAM_HDR_COLORGRADING_COLOR_BALANCE).fValue[2]);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_COLORGRADING_COLOR_BALANCE, vColorBalance);
		}
		else
		{
			m_fHDRMultiplier = 1.f;
		}

		pRenderer->SetShadowJittering(GetVar(PARAM_SHADOW_JITTERING).fValue[0]);
	}

	float skyBrightMultiplier(GetVar(PARAM_TERRAIN_OCCL_MULTIPLIER).fValue[0]);
	float GIMultiplier(GetVar(PARAM_GI_MULTIPLIER).fValue[0]);
	float sunMultiplier(1.0f);
	float sunSpecMultiplier(GetVar(PARAM_SUN_SPECULAR_MULTIPLIER).fValue[0]);
	float fogMultiplier(GetVar(PARAM_FOG_COLOR_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	float fogMultiplier2(GetVar(PARAM_FOG_COLOR2_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	float fogMultiplierRadial(GetVar(PARAM_FOG_RADIAL_COLOR_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	float nightSkyHorizonMultiplier(GetVar(PARAM_NIGHSKY_HORIZON_COLOR_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	float nightSkyZenithMultiplier(GetVar(PARAM_NIGHSKY_ZENITH_COLOR_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	float nightSkyMoonMultiplier(GetVar(PARAM_NIGHSKY_MOON_COLOR_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	float nightSkyMoonInnerCoronaMultiplier(GetVar(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	float nightSkyMoonOuterCoronaMultiplier(GetVar(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR_MULTIPLIER).fValue[0] * m_fHDRMultiplier);

	// set sun position
	Vec3 sunPos;

	if (m_bSunLinkedToTOD)
	{
		float timeAng(((m_fTime + 12.0f) / 24.0f) * gf_PI * 2.0f);
		float sunRot = gf_PI * (-m_sunRotationLatitude) / 180.0f;
		float longitude = 0.5f * gf_PI - gf_PI * m_sunRotationLongitude / 180.0f;

		Matrix33 a, b, c, m;

		a.SetRotationZ(timeAng);
		b.SetRotationX(longitude);
		c.SetRotationY(sunRot);

		m = a * b * c;
		sunPos = Vec3(0, 1, 0) * m;

		float h = sunPos.z;
		sunPos.z = sunPos.y;
		sunPos.y = -h;
	}
	else // when not linked, it behaves like the moon
	{
		float sunLati(-gf_PI + gf_PI * m_sunRotationLatitude / 180.0f);
		float sunLong(0.5f * gf_PI - gf_PI * m_sunRotationLongitude / 180.0f);

		float sinLon(sinf(sunLong));
		float cosLon(cosf(sunLong));
		float sinLat(sinf(sunLati));
		float cosLat(cosf(sunLati));

		sunPos = Vec3(sinLon * cosLat, sinLon * sinLat, cosLon);
	}

	Vec3 sunPosOrig = sunPos;

	// transition phase for sun/moon lighting
	assert(p3DEngine->m_dawnStart <= p3DEngine->m_dawnEnd);
	assert(p3DEngine->m_duskStart <= p3DEngine->m_duskEnd);
	assert(p3DEngine->m_dawnEnd <= p3DEngine->m_duskStart);
	float sunIntensityMultiplier(m_fHDRMultiplier);
	float dayNightIndicator(1.0);
	if (m_fTime < p3DEngine->m_dawnStart || m_fTime >= p3DEngine->m_duskEnd)
	{
		// night
		sunIntensityMultiplier = 0.0;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunPos);
		dayNightIndicator = 0.0;
	}
	else if (m_fTime < p3DEngine->m_dawnEnd)
	{
		// dawn
		assert(p3DEngine->m_dawnStart < p3DEngine->m_dawnEnd);
		float b(0.5f * (p3DEngine->m_dawnStart + p3DEngine->m_dawnEnd));
		if (m_fTime < b)
		{
			// fade out moon
			sunMultiplier *= (b - m_fTime) / (b - p3DEngine->m_dawnStart);
			sunIntensityMultiplier = 0.0;
			p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunPos);
		}
		else
		{
			// fade in sun
			float t((m_fTime - b) / (p3DEngine->m_dawnEnd - b));
			sunMultiplier *= t;
			sunIntensityMultiplier = t;
		}

		dayNightIndicator = (m_fTime - p3DEngine->m_dawnStart) / (p3DEngine->m_dawnEnd - p3DEngine->m_dawnStart);
	}
	else if (m_fTime < p3DEngine->m_duskStart)
	{
		// day
		dayNightIndicator = 1.0;
	}
	else if (m_fTime < p3DEngine->m_duskEnd)
	{
		// dusk
		assert(p3DEngine->m_duskStart < p3DEngine->m_duskEnd);
		float b(0.5f * (p3DEngine->m_duskStart + p3DEngine->m_duskEnd));
		if (m_fTime < b)
		{
			// fade out sun
			float t((b - m_fTime) / (b - p3DEngine->m_duskStart));
			sunMultiplier *= t;
			sunIntensityMultiplier = t;
		}
		else
		{
			// fade in moon
			sunMultiplier *= (m_fTime - b) / (p3DEngine->m_duskEnd - b);
			sunIntensityMultiplier = 0.0;
			p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, sunPos);
		}

		dayNightIndicator = (p3DEngine->m_duskEnd - m_fTime) / (p3DEngine->m_duskEnd - p3DEngine->m_duskStart);
	}
	sunIntensityMultiplier = max(GetVar(PARAM_SKYLIGHT_SUN_INTENSITY_MULTIPLIER).fValue[0], 0.0f);
	p3DEngine->SetGlobalParameter(E3DPARAM_DAY_NIGHT_INDICATOR, Vec3(dayNightIndicator, 0, 0));

	p3DEngine->SetSunDir(sunPos);

	// set sun, sky, and fog color
	Vec3 sunColor(Vec3(GetVar(PARAM_SUN_COLOR).fValue[0],
	                   GetVar(PARAM_SUN_COLOR).fValue[1], GetVar(PARAM_SUN_COLOR).fValue[2]));
	float sunIntensityLux(GetVar(PARAM_SUN_INTENSITY).fValue[0] * sunMultiplier);
	p3DEngine->SetSunColor(ConvertIlluminanceToLightColor(sunIntensityLux, sunColor));
	p3DEngine->SetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER, Vec3(sunSpecMultiplier, 0, 0));
	p3DEngine->SetSkyBrightness(skyBrightMultiplier);
	p3DEngine->SetGIAmount(GIMultiplier);

	Vec3 fogColor(fogMultiplier * Vec3(GetVar(PARAM_FOG_COLOR).fValue[0],
	                                   GetVar(PARAM_FOG_COLOR).fValue[1], GetVar(PARAM_FOG_COLOR).fValue[2]));
	p3DEngine->SetFogColor(fogColor);

	const Vec3 fogColor2 = fogMultiplier2 * Vec3(GetVar(PARAM_FOG_COLOR2).fValue[0], GetVar(PARAM_FOG_COLOR2).fValue[1], GetVar(PARAM_FOG_COLOR2).fValue[2]);
	p3DEngine->SetGlobalParameter(E3DPARAM_FOG_COLOR2, fogColor2);

	const Vec3 fogColorRadial = fogMultiplierRadial * Vec3(GetVar(PARAM_FOG_RADIAL_COLOR).fValue[0], GetVar(PARAM_FOG_RADIAL_COLOR).fValue[1], GetVar(PARAM_FOG_RADIAL_COLOR).fValue[2]);
	p3DEngine->SetGlobalParameter(E3DPARAM_FOG_RADIAL_COLOR, fogColorRadial);

	const Vec3 volFogHeightDensity = Vec3(GetVar(PARAM_VOLFOG_HEIGHT).fValue[0], GetVar(PARAM_VOLFOG_DENSITY).fValue[0], 0);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY, volFogHeightDensity);

	const Vec3 volFogHeightDensity2 = Vec3(GetVar(PARAM_VOLFOG_HEIGHT2).fValue[0], GetVar(PARAM_VOLFOG_DENSITY2).fValue[0], 0);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY2, volFogHeightDensity2);

	const Vec3 volFogGradientCtrl = Vec3(GetVar(PARAM_VOLFOG_HEIGHT_OFFSET).fValue[0], GetVar(PARAM_VOLFOG_RADIAL_SIZE).fValue[0], GetVar(PARAM_VOLFOG_RADIAL_LOBE).fValue[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_GRADIENT_CTRL, volFogGradientCtrl);

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_GLOBAL_DENSITY, Vec3(GetVar(PARAM_VOLFOG_GLOBAL_DENSITY).fValue[0], 0, GetVar(PARAM_VOLFOG_FINAL_DENSITY_CLAMP).fValue[0]));

	// set volumetric fog ramp
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_RAMP, Vec3(GetVar(PARAM_VOLFOG_RAMP_START).fValue[0], GetVar(PARAM_VOLFOG_RAMP_END).fValue[0], GetVar(PARAM_VOLFOG_RAMP_INFLUENCE).fValue[0]));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_RANGE, Vec3(GetVar(PARAM_VOLFOG_SHADOW_RANGE).fValue[0], 0, 0));
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_DARKENING, Vec3(GetVar(PARAM_VOLFOG_SHADOW_DARKENING).fValue[0], GetVar(PARAM_VOLFOG_SHADOW_DARKENING_SUN).fValue[0], GetVar(PARAM_VOLFOG_SHADOW_DARKENING_AMBIENT).fValue[0]));

	// set HDR sky lighting properties
	Vec3 sunIntensity(sunIntensityMultiplier * Vec3(GetVar(PARAM_SKYLIGHT_SUN_INTENSITY).fValue[0],
	                                                GetVar(PARAM_SKYLIGHT_SUN_INTENSITY).fValue[1], GetVar(PARAM_SKYLIGHT_SUN_INTENSITY).fValue[2]));

	Vec3 rgbWaveLengths(GetVar(PARAM_SKYLIGHT_WAVELENGTH_R).fValue[0],
	                    GetVar(PARAM_SKYLIGHT_WAVELENGTH_G).fValue[0], GetVar(PARAM_SKYLIGHT_WAVELENGTH_B).fValue[0]);

	p3DEngine->SetSkyLightParameters(sunPosOrig, sunIntensity, GetVar(PARAM_SKYLIGHT_KM).fValue[0],
	                                 GetVar(PARAM_SKYLIGHT_KR).fValue[0], GetVar(PARAM_SKYLIGHT_G).fValue[0], rgbWaveLengths, forceUpdate);

	// set night sky color properties
	Vec3 nightSkyHorizonColor(nightSkyHorizonMultiplier * Vec3(GetVar(PARAM_NIGHSKY_HORIZON_COLOR).fValue[0],
	                                                           GetVar(PARAM_NIGHSKY_HORIZON_COLOR).fValue[1], GetVar(PARAM_NIGHSKY_HORIZON_COLOR).fValue[2]));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR, nightSkyHorizonColor);

	Vec3 nightSkyZenithColor(nightSkyZenithMultiplier * Vec3(GetVar(PARAM_NIGHSKY_ZENITH_COLOR).fValue[0],
	                                                         GetVar(PARAM_NIGHSKY_ZENITH_COLOR).fValue[1], GetVar(PARAM_NIGHSKY_ZENITH_COLOR).fValue[2]));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR, nightSkyZenithColor);

	float nightSkyZenithColorShift(GetVar(PARAM_NIGHSKY_ZENITH_SHIFT).fValue[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT, Vec3(nightSkyZenithColorShift, 0, 0));

	float nightSkyStarIntensity(GetVar(PARAM_NIGHSKY_START_INTENSITY).fValue[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY, Vec3(nightSkyStarIntensity, 0, 0));

	Vec3 nightSkyMoonColor(nightSkyMoonMultiplier * Vec3(GetVar(PARAM_NIGHSKY_MOON_COLOR).fValue[0],
	                                                     GetVar(PARAM_NIGHSKY_MOON_COLOR).fValue[1], GetVar(PARAM_NIGHSKY_MOON_COLOR).fValue[2]));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR, nightSkyMoonColor);

	Vec3 nightSkyMoonInnerCoronaColor(nightSkyMoonInnerCoronaMultiplier * Vec3(GetVar(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR).fValue[0],
	                                                                           GetVar(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR).fValue[1], GetVar(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR).fValue[2]));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR, nightSkyMoonInnerCoronaColor);

	float nightSkyMoonInnerCoronaScale(GetVar(PARAM_NIGHSKY_MOON_INNERCORONA_SCALE).fValue[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE, Vec3(nightSkyMoonInnerCoronaScale, 0, 0));

	Vec3 nightSkyMoonOuterCoronaColor(nightSkyMoonOuterCoronaMultiplier * Vec3(GetVar(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR).fValue[0],
	                                                                           GetVar(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR).fValue[1], GetVar(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR).fValue[2]));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, nightSkyMoonOuterCoronaColor);

	float nightSkyMoonOuterCoronaScale(GetVar(PARAM_NIGHSKY_MOON_OUTERCORONA_SCALE).fValue[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE, Vec3(nightSkyMoonOuterCoronaScale, 0, 0));

	// set sun shafts visibility and activate if required

	float fSunShaftsVis = GetVar(PARAM_SUN_SHAFTS_VISIBILITY).fValue[0];
	fSunShaftsVis = clamp_tpl<float>(fSunShaftsVis, 0.0f, 0.3f);
	float fSunRaysVis = GetVar(PARAM_SUN_RAYS_VISIBILITY).fValue[0];
	float fSunRaysAtten = GetVar(PARAM_SUN_RAYS_ATTENUATION).fValue[0];
	float fSunRaySunColInfluence = GetVar(PARAM_SUN_RAYS_SUNCOLORINFLUENCE).fValue[0];

	float* pSunRaysCustomColorVar = GetVar(PARAM_SUN_RAYS_CUSTOMCOLOR).fValue;
	Vec4 pSunRaysCustomColor = Vec4(pSunRaysCustomColorVar[0], pSunRaysCustomColorVar[1], pSunRaysCustomColorVar[2], 1.0f);

	p3DEngine->SetPostEffectParam("SunShafts_Active", (fSunShaftsVis > 0.05f || fSunRaysVis > 0.05f) ? 1.f : 0.f);
	p3DEngine->SetPostEffectParam("SunShafts_Amount", fSunShaftsVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAmount", fSunRaysVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAttenuation", fSunRaysAtten);
	p3DEngine->SetPostEffectParam("SunShafts_RaysSunColInfluence", fSunRaySunColInfluence);
	p3DEngine->SetPostEffectParamVec4("SunShafts_RaysCustomColor", pSunRaysCustomColor);

	{
		const Vec3 cloudShadingMultipliers = Vec3(GetVar(PARAM_CLOUDSHADING_SUNLIGHT_MULTIPLIER).fValue[0], GetVar(PARAM_CLOUDSHADING_SKYLIGHT_MULTIPLIER).fValue[0], 0);
		p3DEngine->SetGlobalParameter(E3DPARAM_CLOUDSHADING_MULTIPLIERS, cloudShadingMultipliers);

		const float cloudShadingCustomSunColorMult = GetVar(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR_MULTIPLIER).fValue[0];
		const Vec3 cloudShadingCustomSunColor = cloudShadingCustomSunColorMult * Vec3(GetVar(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR).fValue[0], GetVar(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR).fValue[1], GetVar(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR).fValue[2]);
		const float cloudShadingCustomSunColorInfluence = GetVar(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR_INFLUENCE).fValue[0];

		const Vec3 cloudShadingSunColor = p3DEngine->GetSunColor() * cloudShadingMultipliers.x;
		const Vec3 cloudShadingSkyColor = p3DEngine->GetSunColor() * cloudShadingMultipliers.y;

		p3DEngine->SetGlobalParameter(E3DPARAM_CLOUDSHADING_SUNCOLOR, cloudShadingSunColor + (cloudShadingCustomSunColor - cloudShadingSunColor) * cloudShadingCustomSunColorInfluence);

		// set volumetric cloud parameters
		const Vec3 volCloudAtmosAlbedo = Vec3(GetVar(PARAM_VOLCLOUD_ATMOSPHERIC_ALBEDO).fValue[0], GetVar(PARAM_VOLCLOUD_ATMOSPHERIC_ALBEDO).fValue[1], GetVar(PARAM_VOLCLOUD_ATMOSPHERIC_ALBEDO).fValue[2]);
		const float volCloudRayleighBlue = 2.06e-5f; // Rayleigh scattering coefficient for blue as 488 nm wave length.
		const Vec3 volCloudRayleighScattering = volCloudAtmosAlbedo * volCloudRayleighBlue;
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING, volCloudRayleighScattering);

		const Vec3 volCloudGenParam = Vec3(GetVar(PARAM_VOLCLOUD_GLOBAL_DENSITY).fValue[0], GetVar(PARAM_VOLCLOUD_HEIGHT).fValue[0], GetVar(PARAM_VOLCLOUD_THICKNESS).fValue[0]);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_GEN_PARAMS, volCloudGenParam);

		const Vec3 volCloudScatteringLow = Vec3(GetVar(PARAM_VOLCLOUD_SUN_SINGLE_SCATTERING).fValue[0], GetVar(PARAM_VOLCLOUD_SUN_LOW_ORDER_SCATTERING).fValue[0], GetVar(PARAM_VOLCLOUD_SUN_LOW_ORDER_SCATTERING_ANISTROPY).fValue[0]);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_LOW, volCloudScatteringLow);

		const Vec3 volCloudScatteringHigh = Vec3(GetVar(PARAM_VOLCLOUD_SUN_HIGH_ORDER_SCATTERING).fValue[0], GetVar(PARAM_VOLCLOUD_SKY_LIGHTING_SCATTERING).fValue[0], GetVar(PARAM_VOLCLOUD_GOUND_LIGHTING_SCATTERING).fValue[0]);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_HIGH, volCloudScatteringHigh);

		const Vec3 volCloudGroundColor = Vec3(GetVar(PARAM_VOLCLOUD_GROUND_LIGHTING_ALBEDO).fValue[0], GetVar(PARAM_VOLCLOUD_GROUND_LIGHTING_ALBEDO).fValue[1], GetVar(PARAM_VOLCLOUD_GROUND_LIGHTING_ALBEDO).fValue[2]);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_GROUND_COLOR, volCloudGroundColor);

		const Vec3 volCloudScatteringMulti = Vec3(GetVar(PARAM_VOLCLOUD_MULTI_SCATTERING_ATTENUATION).fValue[0], GetVar(PARAM_VOLCLOUD_MULTI_SCATTERING_PRESERVATION).fValue[0], GetVar(PARAM_VOLCLOUD_POWDER_EFFECT).fValue[0]);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_MULTI, volCloudScatteringMulti);

		const float sunIntensityOriginal = (sunIntensityLux / RENDERER_LIGHT_UNIT_SCALE) / gf_PI; // divided by pi to match to GetSunColor(), it's also divided by pi in ConvertIlluminanceToLightColor().
		const float sunIntensityCustomSun = cloudShadingMultipliers.x * cloudShadingCustomSunColorMult;
		const float sunIntensityCloudAtmosphere = Lerp(sunIntensityOriginal, sunIntensityCustomSun, cloudShadingCustomSunColorInfluence);
		const float atmosphericScatteringMultiplier = GetVar(PARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING).fValue[0];
		const Vec3 volCloudWindAtmos = Vec3(GetVar(PARAM_VOLCLOUD_WIND_INFLUENCE).fValue[0], 0.0f, sunIntensityCloudAtmosphere * atmosphericScatteringMultiplier);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_WIND_ATMOSPHERIC, volCloudWindAtmos);

		const Vec3 volCloudTurbulence = Vec3(GetVar(PARAM_VOLCLOUD_CLOUD_EDGE_TURBULENCE).fValue[0], GetVar(PARAM_VOLCLOUD_CLOUD_EDGE_THRESHOLD).fValue[0], GetVar(PARAM_VOLCLOUD_ABSORPTION).fValue[0]);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE, volCloudTurbulence);
	}

	// set ocean fog color multiplier
	const float oceanFogColorMultiplier = GetVar(PARAM_OCEANFOG_COLOR_MULTIPLIER).fValue[0];
	const Vec3 oceanFogColor = Vec3(GetVar(PARAM_OCEANFOG_COLOR).fValue[0], GetVar(PARAM_OCEANFOG_COLOR).fValue[1], GetVar(PARAM_OCEANFOG_COLOR).fValue[2]);
	p3DEngine->SetGlobalParameter(E3DPARAM_OCEANFOG_COLOR, oceanFogColor * oceanFogColorMultiplier);

	const float oceanFogColorDensity = GetVar(PARAM_OCEANFOG_DENSITY).fValue[0];
	p3DEngine->SetGlobalParameter(E3DPARAM_OCEANFOG_DENSITY, Vec3(oceanFogColorDensity, 0, 0));

	// set skybox multiplier
	float skyBoxMulitplier(GetVar(PARAM_SKYBOX_MULTIPLIER).fValue[0] * m_fHDRMultiplier);
	p3DEngine->SetGlobalParameter(E3DPARAM_SKYBOX_MULTIPLIER, Vec3(skyBoxMulitplier, 0, 0));

	// Set color grading stuff
	float fValue = GetVar(PARAM_COLORGRADING_FILTERS_GRAIN).fValue[0];
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_GRAIN, Vec3(fValue, 0, 0));

	Vec4 pColor = Vec4(GetVar(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR).fValue[0],
	                   GetVar(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR).fValue[1],
	                   GetVar(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR).fValue[2], 1.0f);
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR, Vec3(pColor.x, pColor.y, pColor.z));
	fValue = GetVar(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY).fValue[0];
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY, Vec3(fValue, 0, 0));

	fValue = GetVar(PARAM_COLORGRADING_DOF_FOCUSRANGE).fValue[0];
	p3DEngine->SetPostEffectParam("Dof_Tod_FocusRange", fValue);

	fValue = GetVar(PARAM_COLORGRADING_DOF_BLURAMOUNT).fValue[0];
	p3DEngine->SetPostEffectParam("Dof_Tod_BlurAmount", fValue);

	const float arrDepthConstBias[MAX_SHADOW_CASCADES_NUM] =
	{
		GetVar(PARAM_SHADOWSC0_BIAS).fValue[0], GetVar(PARAM_SHADOWSC1_BIAS).fValue[0], GetVar(PARAM_SHADOWSC2_BIAS).fValue[0], GetVar(PARAM_SHADOWSC3_BIAS).fValue[0],
		GetVar(PARAM_SHADOWSC4_BIAS).fValue[0], GetVar(PARAM_SHADOWSC5_BIAS).fValue[0], GetVar(PARAM_SHADOWSC6_BIAS).fValue[0], GetVar(PARAM_SHADOWSC7_BIAS).fValue[0],
		2.0f,                                   2.0f,                                   2.0f,                                   2.0f,
		2.0f,                                   2.0f,                                   2.0f,                                   2.0f,
		2.0f,                                   2.0f,                                   2.0f,                                   2.0f
	};

	const float arrDepthSlopeBias[MAX_SHADOW_CASCADES_NUM] =
	{
		GetVar(PARAM_SHADOWSC0_SLOPE_BIAS).fValue[0], GetVar(PARAM_SHADOWSC1_SLOPE_BIAS).fValue[0], GetVar(PARAM_SHADOWSC2_SLOPE_BIAS).fValue[0], GetVar(PARAM_SHADOWSC3_SLOPE_BIAS).fValue[0],
		GetVar(PARAM_SHADOWSC4_SLOPE_BIAS).fValue[0], GetVar(PARAM_SHADOWSC5_SLOPE_BIAS).fValue[0], GetVar(PARAM_SHADOWSC6_SLOPE_BIAS).fValue[0], GetVar(PARAM_SHADOWSC7_SLOPE_BIAS).fValue[0],
		0.5f,                                         0.5f,                                         0.5f,                                         0.5f,
		0.5f,                                         0.5f,                                         0.5f,                                         0.5f,
		0.5f,                                         0.5f,                                         0.5f,                                         0.5f
	};

	p3DEngine->SetShadowsCascadesBias(arrDepthConstBias, arrDepthSlopeBias);

	if (gEnv->IsEditing())
	{
		p3DEngine->SetRecomputeCachedShadows();
	}

	// set volumetric fog 2 params
	const Vec3 volFogCtrlParams = Vec3(GetVar(PARAM_VOLFOG2_RANGE).fValue[0], GetVar(PARAM_VOLFOG2_BLEND_FACTOR).fValue[0], GetVar(PARAM_VOLFOG2_BLEND_MODE).fValue[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);

	const Vec3 volFogScatteringParams = Vec3(GetVar(PARAM_VOLFOG2_INSCATTER).fValue[0], GetVar(PARAM_VOLFOG2_EXTINCTION).fValue[0], GetVar(PARAM_VOLFOG2_ANISOTROPIC).fValue[0]);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_SCATTERING_PARAMS, volFogScatteringParams);

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_RAMP, Vec3(GetVar(PARAM_VOLFOG2_RAMP_START).fValue[0], GetVar(PARAM_VOLFOG2_RAMP_END).fValue[0], 0.0f));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_COLOR, Vec3(GetVar(PARAM_VOLFOG2_COLOR).fValue[0], GetVar(PARAM_VOLFOG2_COLOR).fValue[1], GetVar(PARAM_VOLFOG2_COLOR).fValue[2]));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_GLOBAL_DENSITY, Vec3(GetVar(PARAM_VOLFOG2_GLOBAL_DENSITY).fValue[0], GetVar(PARAM_VOLFOG2_FINAL_DENSITY_CLAMP).fValue[0], GetVar(PARAM_VOLFOG2_GLOBAL_FOG_VISIBILITY).fValue[0]));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY, Vec3(GetVar(PARAM_VOLFOG2_HEIGHT).fValue[0], GetVar(PARAM_VOLFOG2_DENSITY).fValue[0], GetVar(PARAM_VOLFOG2_ANISOTROPIC1).fValue[0]));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY2, Vec3(GetVar(PARAM_VOLFOG2_HEIGHT2).fValue[0], GetVar(PARAM_VOLFOG2_DENSITY2).fValue[0], GetVar(PARAM_VOLFOG2_ANISOTROPIC2).fValue[0]));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_COLOR1, Vec3(GetVar(PARAM_VOLFOG2_COLOR1).fValue[0], GetVar(PARAM_VOLFOG2_COLOR1).fValue[1], GetVar(PARAM_VOLFOG2_COLOR1).fValue[2]));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_COLOR2, Vec3(GetVar(PARAM_VOLFOG2_COLOR2).fValue[0], GetVar(PARAM_VOLFOG2_COLOR2).fValue[1], GetVar(PARAM_VOLFOG2_COLOR2).fValue[2]));
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SetAdvancedInfo(const SAdvancedInfo& advInfo)
{
	m_advancedInfo = advInfo;
	if (m_pTimeOfDaySpeedCVar->GetFVal() != m_advancedInfo.fAnimSpeed)
		m_pTimeOfDaySpeedCVar->Set(m_advancedInfo.fAnimSpeed);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::GetAdvancedInfo(SAdvancedInfo& advInfo)
{
	advInfo = m_advancedInfo;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Serialize(XmlNodeRef& node, bool bLoading)
{
	if (bLoading)
	{
		node->getAttr("Time", m_fTime);

		node->getAttr("TimeStart", m_advancedInfo.fStartTime);
		node->getAttr("TimeEnd", m_advancedInfo.fEndTime);
		node->getAttr("TimeAnimSpeed", m_advancedInfo.fAnimSpeed);

		if (m_pTimeOfDaySpeedCVar->GetFVal() != m_advancedInfo.fAnimSpeed)
			m_pTimeOfDaySpeedCVar->Set(m_advancedInfo.fAnimSpeed);

		m_pCurrentPreset = NULL;
		m_currentPresetName.clear();
		m_presets.clear();
		if (XmlNodeRef presetsNode = node->findChild("Presets"))
		{
			const int nPresetsCount = presetsNode->getChildCount();
			for (int i = 0; i < nPresetsCount; ++i)
			{
				if (XmlNodeRef presetNode = presetsNode->getChild(i))
				{
					const char* presetName = presetNode->getAttr("Name");
					if (presetName)
					{
						string name = presetName;
						if (!strchr(presetName, '/'))
						{
							name = string(sPresetsLibsPath) + name + ".xml"; // temporary for backwards compatibility
						}

						std::pair<TPresetsSet::iterator, bool> insertResult = m_presets.emplace(std::make_pair(name, CEnvironmentPreset()));
						const TPresetsSet::key_type& presetName = insertResult.first->first;
						CEnvironmentPreset& preset = insertResult.first->second;

						if (LoadPresetFromXML(preset, name))
						{
							if (presetNode->haveAttr("Default"))
							{
								m_pCurrentPreset = &preset;
								m_currentPresetName = presetName;
							}
						}
						else
						{
							m_presets.erase(insertResult.first);
						}
					}
				}
			}

		}
		else
		{
			// old format - convert to the new one
			string presetName("default");
			if (gEnv->pGameFramework)
			{
				const char* pLevelName = gEnv->pGameFramework->GetLevelName();
				if (pLevelName && *pLevelName)
				{
					//presetName = pLevelName;
					presetName = string(sPresetsLibsPath) + pLevelName + ".xml";
				}
			}

			std::pair<TPresetsSet::iterator, bool> insertRes = m_presets.emplace(presetName, CEnvironmentPreset());
			CEnvironmentPreset& preset = insertRes.first->second;

			LoadPresetFromOldFormatXML(preset, node);
		}

		if (!m_pCurrentPreset && m_presets.size())
		{
			const auto it = m_presets.begin();
			m_pCurrentPreset = &(it->second);
			m_currentPresetName = it->first;
		}

		SetTime(m_fTime, false);
		NotifyOnChange(IListener::EChangeType::CurrentPresetChanged, m_currentPresetName.c_str());
	}
	else //if (bLoading)
	{
		node->setAttr("Time", m_fTime);
		node->setAttr("TimeStart", m_advancedInfo.fStartTime);
		node->setAttr("TimeEnd", m_advancedInfo.fEndTime);
		node->setAttr("TimeAnimSpeed", m_advancedInfo.fAnimSpeed);
		if (XmlNodeRef presetsNode = node->newChild("Presets"))
		{
			for (TPresetsSet::const_iterator it = m_presets.begin(); it != m_presets.end(); ++it)
			{
				const TPresetsSet::key_type& presetName = it->first;
				const CEnvironmentPreset& preset = it->second;
				if (XmlNodeRef presetNode = presetsNode->newChild("Preset"))
				{
					presetNode->setAttr("Name", presetName.c_str());
					if (&preset == m_pCurrentPreset)
						presetNode->setAttr("Default", 1);
				}
				SavePresetToXML(preset, presetName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Serialize(TSerialize ser)
{
	assert(ser.GetSerializationTarget() != eST_Network);

	string tempName;

	ser.Value("time", m_fTime);
	ser.Value("mode", m_bEditMode);
	ser.Value("m_sunRotationLatitude", m_sunRotationLatitude);
	ser.Value("m_sunRotationLongitude", m_sunRotationLongitude);
	const int size = GetVariableCount();
	ser.BeginGroup("VariableValues");
	for (int v = 0; v < size; v++)
	{
		tempName = m_vars[v].name;
		tempName.replace(' ', '_');
		tempName.replace('(', '_');
		tempName.replace(')', '_');
		tempName.replace(':', '_');
		ser.BeginGroup(tempName);
		ser.Value("Val0", m_vars[v].fValue[0]);
		ser.Value("Val1", m_vars[v].fValue[1]);
		ser.Value("Val2", m_vars[v].fValue[2]);
		ser.EndGroup();
	}
	ser.EndGroup();

	ser.Value("AdvInfoSpeed", m_advancedInfo.fAnimSpeed);
	ser.Value("AdvInfoStart", m_advancedInfo.fStartTime);
	ser.Value("AdvInfoEnd", m_advancedInfo.fEndTime);

	if (ser.IsReading())
	{
		SetTime(m_fTime, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::NetSerialize(TSerialize ser, float lag, uint32 flags)
{
	if (0 == (flags & NETSER_STATICPROPS))
	{
		if (ser.IsWriting())
		{
			ser.Value("time", m_fTime, 'tod');
		}
		else
		{
			float serializedTime;
			ser.Value("time", serializedTime, 'tod');
			float remoteTime = serializedTime + ((flags & NETSER_COMPENSATELAG) != 0) * m_advancedInfo.fAnimSpeed * lag;
			float setTime = remoteTime;
			if (0 == (flags & NETSER_FORCESET))
			{
				const float adjustmentFactor = 0.05f;
				const float wraparoundGuardHours = 2.0f;

				float localTime = m_fTime;
				// handle wraparound
				if (localTime < wraparoundGuardHours && remoteTime > (24.0f - wraparoundGuardHours))
					localTime += 24.0f;
				else if (remoteTime < wraparoundGuardHours && localTime > (24.0f - wraparoundGuardHours))
					remoteTime += 24.0f;
				// don't blend times if they're very different
				if (fabsf(remoteTime - localTime) < 1.0f)
				{
					setTime = adjustmentFactor * remoteTime + (1.0f - adjustmentFactor) * m_fTime;
					if (setTime > 24.0f)
						setTime -= 24.0f;
				}
			}
			SetTime(setTime, (flags & NETSER_FORCESET) != 0);
		}
	}
	else
	{
		// no static serialization (yet)
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Tick()
{
	//	if(!gEnv->bServer)
	//		return;
	if (!m_bEditMode && !m_bPaused)
	{
		if (fabs(m_advancedInfo.fAnimSpeed) > 0.0001f)
		{
			// advance (forward or backward)
			float fTime = m_fTime + m_advancedInfo.fAnimSpeed * m_pTimer->GetFrameTime();

			// full cycle mode
			if (m_advancedInfo.fStartTime <= 0.05f && m_advancedInfo.fEndTime >= 23.5f)
			{
				if (fTime > m_advancedInfo.fEndTime)
					fTime = m_advancedInfo.fStartTime;
				if (fTime < m_advancedInfo.fStartTime)
					fTime = m_advancedInfo.fEndTime;
			}
			else if (fabs(m_advancedInfo.fStartTime - m_advancedInfo.fEndTime) <= 0.05f)//full cycle mode
			{
				if (fTime > 24.0f)
					fTime -= 24.0f;
				else if (fTime < 0.0f)
					fTime += 24.0f;
			}
			else
			{
				// clamp advancing time
				if (fTime > m_advancedInfo.fEndTime)
					fTime = m_advancedInfo.fEndTime;
				if (fTime < m_advancedInfo.fStartTime)
					fTime = m_advancedInfo.fStartTime;
			}

			SetTime(fTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::NotifyOnChange(const IListener::EChangeType changeType, const char* const szPresetName)
{
	m_listeners.ForEachListener([changeType, szPresetName](IListener* pListener)
	{
		pListener->OnChange(changeType, szPresetName);
	});
}
