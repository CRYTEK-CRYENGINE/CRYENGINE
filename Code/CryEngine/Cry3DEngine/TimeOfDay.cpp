// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TimeOfDay.h"

#include "EnvironmentPreset.h"
#include "terrain_water.h"

#include <CryCore/StlUtils.h>
#include <CryGame/IGameFramework.h>
#include <CryMath/ISplines.h>
#include <CryNetwork/IRemoteCommand.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/IArchiveHost.h>

#define SERIALIZATION_ENUM_DEFAULTNAME(x) SERIALIZATION_ENUM(ITimeOfDay::x, # x, # x)

SERIALIZATION_ENUM_BEGIN_NESTED(ITimeOfDay, ETimeOfDayParamID, "ETimeOfDayParamID")
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_INTENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SUN_SPECULAR_MULTIPLIER)

SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYBOX_ANGLE)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYBOX_STRETCHING)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYBOX_COLOR)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYBOX_INTENSITY)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYBOX_FILTER)
SERIALIZATION_ENUM_DEFAULTNAME(PARAM_SKYBOX_OPACITY)

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
		}
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
	}
}

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

CryPathString GetPresetXMLFilenamefromName(const string& presetName, const bool bForWriting)
{
	CryPathString adjustedName;
	gEnv->pCryPak->AdjustFileName(presetName.c_str(), adjustedName, bForWriting ? ICryPak::FLAGS_FOR_WRITING : 0);

	// Backward compatibility with old xml file extension.
	if (!bForWriting && !GetISystem()->GetIPak()->IsFileExist(adjustedName))
	{
		string filePathXml = PathUtil::ReplaceExtension(adjustedName, "xml");
		if (GetISystem()->GetIPak()->IsFileExist(filePathXml))
		{
			return filePathXml;
		}
	}

	return adjustedName;
}

bool SavePresetToXML(const CEnvironmentPreset& preset, const string& presetName)
{
	const CryPathString filename(GetPresetXMLFilenamefromName(presetName, false));

	if (!Serialization::SaveXmlFile(filename.c_str(), preset, sPresetXMLRootNodeName))
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "TimeOfDay: Failed to save preset: %s", presetName.c_str());
		return false;
	}

	return true;
}

bool LoadPresetFromXML(CEnvironmentPreset& pPreset, const string& presetName)
{
	const CryPathString filename(GetPresetXMLFilenamefromName(presetName, false));
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
	: m_pCurrentPreset(nullptr)
	, m_fTime(12)
	, m_bEditMode(false)
	, m_bPaused(false)
	, m_pTimer(nullptr)
	, m_fHDRMultiplier(1.0f)
	, m_timeOfDayRtpcId(CryAudio::StringToId("time_of_day"))
	, m_listeners(16)
{
	SetTimer(gEnv->pTimer);
	m_advancedInfo.fAnimSpeed = 0;
	m_advancedInfo.fStartTime = 0;
	m_advancedInfo.fEndTime = 24;
	m_pTimeOfDaySpeedCVar = gEnv->pConsole->GetCVar("e_TimeOfDaySpeed");
}

bool CTimeOfDay::GetPresetsInfos(SPresetInfo* resultArray, unsigned int arraySize) const
{
	if (arraySize < m_presets.size())
		return false;

	size_t i = 0;
	for (TPresetsSet::const_iterator it = m_presets.begin(); it != m_presets.end(); ++it)
	{
		resultArray[i].szName = it->first.c_str();
		resultArray[i].isCurrent = (m_pCurrentPreset == (it->second).get());
		resultArray[i].isDefault = m_defaultPresetName.CompareNoCase(it->first) == 0;
		++i;
	}

	return true;
}

const char* CTimeOfDay::GetCurrentPresetName() const
{
	return m_currentPresetName.c_str();
}

bool CTimeOfDay::SetCurrentPreset(const char* szPresetName)
{
	TPresetsSet::iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
	{
		return false;
	}

	CEnvironmentPreset* newPreset = (it->second).get();
	if (m_pCurrentPreset != newPreset)
	{
		m_pCurrentPreset = newPreset;
		m_currentPresetName = szPresetName;
		Update(true, true);
		ConstantsChanged();
		NotifyOnChange(IListener::EChangeType::CurrentPresetChanged, szPresetName);
	}
	return true;
}


bool CTimeOfDay::SetDefaultPreset(const char* szPresetName)
{
	TPresetsSet::iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
	{
		return false;
	}

	CEnvironmentPreset* newPreset = (it->second).get();
	if (m_pCurrentPreset != newPreset)
	{
		m_pCurrentPreset = newPreset;
		m_currentPresetName = szPresetName;
		Update(true, true);
		ConstantsChanged();
	}

	m_defaultPresetName = szPresetName;
	NotifyOnChange(IListener::EChangeType::DefaultPresetChanged, szPresetName);
	return true;
}

const char* CTimeOfDay::GetDefaultPresetName() const
{
	return m_defaultPresetName.c_str();
}

bool CTimeOfDay::AddNewPreset(const char* szPresetName)
{
	const std::pair<CEnvironmentPreset*, bool> result = GetOrCreatePreset(szPresetName);
	if (!result.second)
	{
		return false;
	}

	m_pCurrentPreset = result.first;
	m_currentPresetName = szPresetName;
	Update(true, true);
	ConstantsChanged();
	NotifyOnChange(IListener::EChangeType::PresetAdded, szPresetName);
	return true;
}

bool CTimeOfDay::RemovePreset(const char* szPresetName)
{
	const TPresetsSet::iterator findResult = m_presets.find(szPresetName);
	if (findResult == m_presets.end())
	{
		return false;
	}

	if (m_pCurrentPreset == findResult->second.get())
	{
		m_pCurrentPreset = nullptr;
		m_currentPresetName.clear();
	}

	// The preset interface can still be used outside the class, so we never delete the presets, just move them to the list of preview's preset.
	m_previewPresets[findResult->first] = std::move(findResult->second);
	m_presets.erase(findResult);

	if (!m_pCurrentPreset && m_presets.size())
	{
		const auto it = m_presets.begin();
		m_pCurrentPreset = it->second.get();
		m_currentPresetName = it->first;
		Update(true, true);
		ConstantsChanged();
	}
	
	NotifyOnChange(IListener::EChangeType::PresetRemoved, szPresetName);
	return true;
}

bool CTimeOfDay::SavePreset(const char* szPresetName) const
{
	const string sPresetName(szPresetName);
	TPresetsSet::const_iterator it = m_presets.find(sPresetName);
	if (it != m_presets.end())
	{
		const bool bResult = SavePresetToXML(*it->second.get(), sPresetName);
		return bResult;
	}

	// check the preview presets list.
	it = m_previewPresets.find(szPresetName);
	if (it != m_previewPresets.end())
	{
		const bool bResult = SavePresetToXML(*it->second.get(), sPresetName);
		return bResult;
	}

	return false;
}

bool CTimeOfDay::LoadPreset(const char* szFilePath)
{
	const string path(szFilePath);

	std::pair<CEnvironmentPreset*, bool> result = GetOrCreatePreset(path);

	// has been just created?
	if (result.second)
	{
		CEnvironmentPreset& preset = *result.first;
		XmlNodeRef root = GetISystem()->LoadXmlFromFile(szFilePath);
		if (!root)
		{
			m_presets.erase(szFilePath);
			return false;
		}

		if (root->isTag(sPresetXMLRootNodeName))
		{
			Serialization::LoadXmlFile(preset, szFilePath);
		}
		else
		{
			// Try to load old format
			LoadPresetFromOldFormatXML(preset, root);
		}
	}

	m_pCurrentPreset = result.first;
	m_currentPresetName = path;

	if (m_defaultPresetName.empty())
	{
		m_defaultPresetName = path;
	}

	Update(true, true);
	ConstantsChanged();
	NotifyOnChange(result.second ? IListener::EChangeType::PresetLoaded : IListener::EChangeType::CurrentPresetChanged, m_currentPresetName.c_str());
	return true;
}

bool CTimeOfDay::ResetPreset(const char* szPresetName)
{
	TPresetsSet::iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
	{
		it = m_previewPresets.find(szPresetName);
		if (it == m_previewPresets.end())
		{
			return false;
		}
	}

	it->second->Reset();
	if (it->second.get() == m_pCurrentPreset)
	{
		Update(true, true);
		ConstantsChanged();
	}
	return true;
}

void CTimeOfDay::DiscardPresetChanges(const char* szPresetName)
{
	TPresetsSet::iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
	{
		it = m_previewPresets.find(szPresetName);
		if (it == m_previewPresets.end())
		{
			return;
		}
	}

	CEnvironmentPreset& preset = *it->second;
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(szPresetName);
	if (!root)
	{
		return;
	}

	if (root->isTag(sPresetXMLRootNodeName))
	{
		Serialization::LoadXmlFile(preset, szPresetName);
	}
	else
	{
		// Try to load old format
		LoadPresetFromOldFormatXML(preset, root);
	}

	if (it->second.get() == m_pCurrentPreset)
	{
		Update(true, true);
		ConstantsChanged();
		NotifyOnChange(IListener::EChangeType::PresetLoaded, m_currentPresetName.c_str());
	}
}

bool CTimeOfDay::ImportPreset(const char* szPresetName, const char* szFilePath)
{
	TPresetsSet::const_iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
	{
		it = m_previewPresets.find(szPresetName);
		if (it == m_previewPresets.end())
		{
			return false;
		}
	}

	XmlNodeRef root = GetISystem()->LoadXmlFromFile(szFilePath);
	if (!root)
		return false;

	CEnvironmentPreset& preset = *it->second.get();
	if (root->isTag(sPresetXMLRootNodeName))
	{
		Serialization::LoadXmlFile(preset, szFilePath);
	}
	else
	{
		LoadPresetFromOldFormatXML(preset, root);
	}

	Update(true, true);

	return true;
}

bool CTimeOfDay::ExportPreset(const char* szPresetName, const char* szFilePath) const
{
	TPresetsSet::const_iterator it = m_presets.find(szPresetName);
	if (it == m_presets.end())
	{
		it = m_previewPresets.find(szPresetName);
		if (it == m_previewPresets.end())
		{
			return false;
		}
	}

	if (!Serialization::SaveXmlFile(szFilePath, *it->second.get(), sPresetXMLRootNodeName))
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "TimeOfDay: Failed to save preset: %s", szFilePath);
		return false;
	}

	return true;
}

bool CTimeOfDay::PreviewPreset(const char* szPresetName)
{
	// In case if the preset is already used by the current level
	if (SetCurrentPreset(szPresetName))
	{
		return true;
	}

	// Try to load preset.

	const string path(szPresetName);
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(szPresetName);
	if (!root)
	{
		return false;
	}

	std::pair<TPresetsSet::iterator, bool> insertResult = m_previewPresets.emplace(path, nullptr);
	if (insertResult.second)
	{
		insertResult.first->second.reset(new CEnvironmentPreset());
		CEnvironmentPreset& preset = *insertResult.first->second;
		if (root->isTag(sPresetXMLRootNodeName))
		{
			Serialization::LoadXmlFile(preset, szPresetName);
		}
		else
		{
			//try to load old format
			LoadPresetFromOldFormatXML(preset, root);
		}
	}

	m_pCurrentPreset = insertResult.first->second.get();
	m_currentPresetName = insertResult.first->first;

	Update(true, true);
	ConstantsChanged();
	NotifyOnChange(IListener::EChangeType::PresetLoaded, m_currentPresetName.c_str());
	return true;
}


void CTimeOfDay::SetTimer(ITimer* pTimer)
{
	CRY_ASSERT(pTimer);
	m_pTimer = pTimer;

	// Update timer for ocean also - Craig
	COcean::SetTimer(pTimer);
}

float CTimeOfDay::GetAnimTimeSecondsIn24h() const
{
	return CEnvironmentPreset::GetAnimTimeSecondsIn24h();
}

CEnvironmentPreset& CTimeOfDay::GetPreset() const
{
	if (m_pCurrentPreset)
	{
		return *m_pCurrentPreset;
	}

	// Create a default environment preset to provide valid environment values even if no custom preset is loaded.
	static CEnvironmentPreset s_defaultPreset;
	return s_defaultPreset;
}

const Vec3 CTimeOfDay::GetValue(ETimeOfDayParamID id) const
{
	return GetPreset().GetVar(id)->GetValue();
}

void CTimeOfDay::SetValue(ETimeOfDayParamID id, const Vec3& newValue)
{
	GetPreset().GetVar(id)->SetValue(newValue);
}

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

void CTimeOfDay::Reset()
{
	m_pCurrentPreset = nullptr;
	m_currentPresetName.clear();
	m_defaultPresetName.clear();

	if (!gEnv->IsEditor())
	{
		m_previewPresets.clear();
	}
	else // do not delete and overwrite state of loaded presets, as they may have active editing sessions and unsaved changes.
	{
		for (auto&& preset : m_presets)
		{
			m_previewPresets[preset.first] = std::move(preset.second);
		}
	}
	m_presets.clear();

	m_fTime = 12;
	m_bEditMode = false;
	m_bPaused = false;
	m_fHDRMultiplier = 1.0f;
	m_advancedInfo.fAnimSpeed = 0;
	m_advancedInfo.fStartTime = 0;
	m_advancedInfo.fEndTime = 24;
}

ITimeOfDay::IPreset& CTimeOfDay::GetCurrentPreset()
{
	return GetPreset();
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
		gEnv->pAudioSystem->SetParameterGlobally(m_timeOfDayRtpcId, m_fTime);
	}

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_TIME_OF_DAY_SET, 0, 0);
}

void CTimeOfDay::Update(bool bInterpolate, bool bForceUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);

	if (bInterpolate && m_pCurrentPreset)
	{
		// Time for interpolation is in range [0.0-1.0]
		float normalizedTime = m_fTime / 24.0f;

		m_pCurrentPreset->Update(normalizedTime);
	}

	// update environment lighting according to new interpolated values
	UpdateEnvLighting(bForceUpdate);
}

void CTimeOfDay::ConstantsChanged()
{
	C3DEngine* p3DEngine((C3DEngine*)gEnv->p3DEngine);

	p3DEngine->UpdateMoonParams();
	p3DEngine->UpdateWindParams();
	p3DEngine->UpdateCloudShadows();
	
	// Empty texture means disable color grading; Transition time == 0 -> switch immediately
	const auto& cgp = GetConstants().GetColorGradingParams();
	p3DEngine->GetColorGradingCtrl()->SetColorGradingLut(cgp.useTexture ? cgp.texture.c_str() : "", 0.f);

#if defined(FEATURE_SVO_GI)
	p3DEngine->UpdateTISettings();
#endif

	//Will update sun params, and recalculate dependent on it lighting
	UpdateEnvLighting(true);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::UpdateEnvLighting(bool forceUpdate)
{
	C3DEngine* p3DEngine((C3DEngine*)gEnv->p3DEngine);
	IRenderer* pRenderer(gEnv->pRenderer);

	if (pRenderer)
	{
		bool bHDRModeEnabled = false;
		pRenderer->EF_Query(EFQ_HDRModeEnabled, bHDRModeEnabled);
		if (bHDRModeEnabled)
		{
			m_fHDRMultiplier = powf(HDRDynamicMultiplier, GetValue(PARAM_HDR_DYNAMIC_POWER_FACTOR).x);

			const Vec3 vEyeAdaptationParams(GetValue(PARAM_HDR_EYEADAPTATION_EV_MIN).x,
			                                GetValue(PARAM_HDR_EYEADAPTATION_EV_MAX).x,
											GetValue(PARAM_HDR_EYEADAPTATION_EV_AUTO_COMPENSATION).x);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_EYEADAPTATION_PARAMS, vEyeAdaptationParams);

			const Vec3 vEyeAdaptationParamsLegacy(GetValue(PARAM_HDR_EYEADAPTATION_SCENEKEY).x,
			                                      GetValue(PARAM_HDR_EYEADAPTATION_MIN_EXPOSURE).x,
			                                      GetValue(PARAM_HDR_EYEADAPTATION_MAX_EXPOSURE).x);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_EYEADAPTATION_PARAMS_LEGACY, vEyeAdaptationParamsLegacy);

			const float fHDRShoulderScale(GetValue(PARAM_HDR_FILMCURVE_SHOULDER_SCALE).x);
			const float fHDRMidtonesScale(GetValue(PARAM_HDR_FILMCURVE_LINEAR_SCALE).x);
			const float fHDRToeScale(GetValue(PARAM_HDR_FILMCURVE_TOE_SCALE).x);
			const float fHDRWhitePoint(GetValue(PARAM_HDR_FILMCURVE_WHITEPOINT).x);

			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_SHOULDER_SCALE, Vec3(fHDRShoulderScale, 0, 0));
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_LINEAR_SCALE, Vec3(fHDRMidtonesScale, 0, 0));
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_TOE_SCALE, Vec3(fHDRToeScale, 0, 0));
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_FILMCURVE_WHITEPOINT, Vec3(fHDRWhitePoint, 0, 0));

			const float fHDRBloomAmount(GetValue(PARAM_HDR_BLOOM_AMOUNT).x);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_BLOOM_AMOUNT, Vec3(fHDRBloomAmount, 0, 0));

			const float fHDRSaturation(GetValue(PARAM_HDR_COLORGRADING_COLOR_SATURATION).x);
			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_COLORGRADING_COLOR_SATURATION, Vec3(fHDRSaturation, 0, 0));

			p3DEngine->SetGlobalParameter(E3DPARAM_HDR_COLORGRADING_COLOR_BALANCE, GetValue(PARAM_HDR_COLORGRADING_COLOR_BALANCE));
		}
		else
		{
			m_fHDRMultiplier = 1.f;
		}

		pRenderer->SetShadowJittering(GetValue(PARAM_SHADOW_JITTERING).x);
	}

	float sunMultiplier = 1.0f;

	const float skyBrightMultiplier = GetValue(PARAM_TERRAIN_OCCL_MULTIPLIER).x;
	const float GIMultiplier = GetValue(PARAM_GI_MULTIPLIER).x;
	const float sunSpecMultiplier = GetValue(PARAM_SUN_SPECULAR_MULTIPLIER).x;
	const float fogMultiplier = GetValue(PARAM_FOG_COLOR_MULTIPLIER).x * m_fHDRMultiplier;
	const float fogMultiplier2 = GetValue(PARAM_FOG_COLOR2_MULTIPLIER).x * m_fHDRMultiplier;
	const float fogMultiplierRadial = GetValue(PARAM_FOG_RADIAL_COLOR_MULTIPLIER).x * m_fHDRMultiplier;
	const float nightSkyHorizonMultiplier = GetValue(PARAM_NIGHSKY_HORIZON_COLOR_MULTIPLIER).x * m_fHDRMultiplier;
	const float nightSkyZenithMultiplier = GetValue(PARAM_NIGHSKY_ZENITH_COLOR_MULTIPLIER).x * m_fHDRMultiplier;
	const float nightSkyMoonMultiplier = GetValue(PARAM_NIGHSKY_MOON_COLOR_MULTIPLIER).x * m_fHDRMultiplier;
	const float nightSkyMoonInnerCoronaMultiplier = GetValue(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR_MULTIPLIER).x * m_fHDRMultiplier;
	const float nightSkyMoonOuterCoronaMultiplier = GetValue(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR_MULTIPLIER).x * m_fHDRMultiplier;

	// set sun position
	Vec3 sunPos;

	const ITimeOfDay::Sun& sun = GetSunParams();
	const ITimeOfDay::Sky& sky = GetSkyParams();

	if (sun.sunLinkedToTOD)
	{
		const float timeAng(((m_fTime + 12.0f) / 24.0f) * gf_PI * 2.0f);
		const float sunRot = gf_PI * (-sun.latitude) / 180.0f;
		const float longitude = 0.5f * gf_PI - gf_PI * sun.longitude / 180.0f;

		Matrix33 a, b, c, m;

		a.SetRotationZ(timeAng);
		b.SetRotationX(longitude);
		c.SetRotationY(sunRot);

		m = a * b * c;
		sunPos = Vec3(0, 1, 0) * m;

		const float h = sunPos.z;
		sunPos.z = sunPos.y;
		sunPos.y = -h;
	}
	else // when not linked, it behaves like the moon
	{
		const float sunLati(-gf_PI + gf_PI * sun.latitude / 180.0f);
		const float sunLong(0.5f * gf_PI - gf_PI * sun.longitude / 180.0f);

		const float sinLon(sinf(sunLong));
		const float cosLon(cosf(sunLong));
		const float sinLat(sinf(sunLati));
		const float cosLat(cosf(sunLati));

		sunPos = Vec3(sinLon * cosLat, sinLon * sinLat, cosLon);
	}

	const Vec3 sunPosOrig = sunPos;

	// transition phase for sun/moon lighting
	CRY_ASSERT(p3DEngine->m_dawnStart <= p3DEngine->m_dawnEnd);
	CRY_ASSERT(p3DEngine->m_duskStart <= p3DEngine->m_duskEnd);
	CRY_ASSERT(p3DEngine->m_dawnEnd <= p3DEngine->m_duskStart);
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
		CRY_ASSERT(p3DEngine->m_dawnStart < p3DEngine->m_dawnEnd);
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
		CRY_ASSERT(p3DEngine->m_duskStart < p3DEngine->m_duskEnd);
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
	sunIntensityMultiplier = max(GetValue(PARAM_SKYLIGHT_SUN_INTENSITY_MULTIPLIER).x, 0.0f);
	p3DEngine->SetGlobalParameter(E3DPARAM_DAY_NIGHT_INDICATOR, Vec3(dayNightIndicator, 0, 0));

	p3DEngine->SetSunDir(sunPos);

	// set sun, sky, and fog color
	const Vec3 sunColor(GetValue(PARAM_SUN_COLOR));
	const float sunIntensityLux(GetValue(PARAM_SUN_INTENSITY).x * sunMultiplier);
	const Vec3 sunEmission(ConvertIlluminanceToLightColor(sunIntensityLux, sunColor));
	p3DEngine->SetSunColor(sunEmission);
	p3DEngine->SetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER, Vec3(sunSpecMultiplier, 0, 0));
	p3DEngine->SetSkyBrightness(skyBrightMultiplier);
	p3DEngine->SetGIAmount(GIMultiplier);

	const float skyboxAngle(GetValue(PARAM_SKYBOX_ANGLE).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_ANGLE, skyboxAngle);
	const float skyboxStretch(GetValue(PARAM_SKYBOX_STRETCHING).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_STRETCHING, skyboxStretch);
	const Vec3 skyboxColor(GetValue(PARAM_SKYBOX_COLOR));
	const float skyboxIntensityLux(GetValue(PARAM_SKYBOX_INTENSITY).x * sunMultiplier);
	const Vec3 skyboxEmmission(ConvertIlluminanceToLightColor(skyboxIntensityLux, skyboxColor));
	p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_EMITTANCE, skyboxEmmission);
	const Vec3 skyboxFilter(GetValue(PARAM_SKYBOX_FILTER));
	const float skyboxOpacity(GetValue(PARAM_SKYBOX_OPACITY).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_FILTER, skyboxFilter * skyboxOpacity);

	// Selective overwrite
	IMaterial* pMaterialDef = nullptr;
	if (!sky.materialDefSpec.empty())
		p3DEngine->SetSkyMaterial(pMaterialDef = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(sky.materialDefSpec.c_str(), false), eSkySpec_Def);
	else
		p3DEngine->SetSkyMaterial(pMaterialDef, eSkySpec_Def);

	IMaterial* pMaterialLow = nullptr;
	if (!sky.materialLowSpec.empty())
		p3DEngine->SetSkyMaterial(pMaterialLow = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(sky.materialLowSpec.c_str(), false), eSkySpec_Low);
	else
		p3DEngine->SetSkyMaterial(pMaterialDef, eSkySpec_Low);

	const Vec3 fogColor(fogMultiplier * GetValue(PARAM_FOG_COLOR));
	p3DEngine->SetFogColor(fogColor);

	const Vec3 fogColor2 = fogMultiplier2 * GetValue(PARAM_FOG_COLOR2);
	p3DEngine->SetGlobalParameter(E3DPARAM_FOG_COLOR2, fogColor2);

	const Vec3 fogColorRadial = fogMultiplierRadial * GetValue(PARAM_FOG_RADIAL_COLOR);
	p3DEngine->SetGlobalParameter(E3DPARAM_FOG_RADIAL_COLOR, fogColorRadial);

	const Vec3 volFogHeightDensity = Vec3(GetValue(PARAM_VOLFOG_HEIGHT).x, GetValue(PARAM_VOLFOG_DENSITY).x, 0);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY, volFogHeightDensity);

	const Vec3 volFogHeightDensity2 = Vec3(GetValue(PARAM_VOLFOG_HEIGHT2).x, GetValue(PARAM_VOLFOG_DENSITY2).x, 0);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_HEIGHT_DENSITY2, volFogHeightDensity2);

	const Vec3 volFogGradientCtrl = Vec3(GetValue(PARAM_VOLFOG_HEIGHT_OFFSET).x, GetValue(PARAM_VOLFOG_RADIAL_SIZE).x, GetValue(PARAM_VOLFOG_RADIAL_LOBE).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_GRADIENT_CTRL, volFogGradientCtrl);

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_GLOBAL_DENSITY, Vec3(GetValue(PARAM_VOLFOG_GLOBAL_DENSITY).x, 0, GetValue(PARAM_VOLFOG_FINAL_DENSITY_CLAMP).x));

	// set volumetric fog ramp
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_RAMP, Vec3(GetValue(PARAM_VOLFOG_RAMP_START).x, GetValue(PARAM_VOLFOG_RAMP_END).x, GetValue(PARAM_VOLFOG_RAMP_INFLUENCE).x));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_RANGE, Vec3(GetValue(PARAM_VOLFOG_SHADOW_RANGE).x, 0, 0));
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_DARKENING, Vec3(GetValue(PARAM_VOLFOG_SHADOW_DARKENING).x, GetValue(PARAM_VOLFOG_SHADOW_DARKENING_SUN).x, GetValue(PARAM_VOLFOG_SHADOW_DARKENING_AMBIENT).x));

	// set HDR sky lighting properties
	const Vec3 sunIntensity(sunIntensityMultiplier * GetValue(PARAM_SKYLIGHT_SUN_INTENSITY));

	const Vec3 rgbWaveLengths(GetValue(PARAM_SKYLIGHT_WAVELENGTH_R).x, GetValue(PARAM_SKYLIGHT_WAVELENGTH_G).x, GetValue(PARAM_SKYLIGHT_WAVELENGTH_B).x);

	p3DEngine->SetSkyLightParameters(sunPosOrig, sunIntensity, GetValue(PARAM_SKYLIGHT_KM).x,
	                                 GetValue(PARAM_SKYLIGHT_KR).x, GetValue(PARAM_SKYLIGHT_G).x, rgbWaveLengths, forceUpdate);

	// set night sky color properties
	const Vec3 nightSkyHorizonColor(nightSkyHorizonMultiplier * GetValue(PARAM_NIGHSKY_HORIZON_COLOR));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR, nightSkyHorizonColor);

	const Vec3 nightSkyZenithColor(nightSkyZenithMultiplier * GetValue(PARAM_NIGHSKY_ZENITH_COLOR));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR, nightSkyZenithColor);

	const float nightSkyZenithColorShift(GetValue(PARAM_NIGHSKY_ZENITH_SHIFT).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT, Vec3(nightSkyZenithColorShift, 0, 0));

	const float nightSkyStarIntensity(GetValue(PARAM_NIGHSKY_START_INTENSITY).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY, Vec3(nightSkyStarIntensity, 0, 0));

	const Vec3 nightSkyMoonColor(nightSkyMoonMultiplier * GetValue(PARAM_NIGHSKY_MOON_COLOR));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR, nightSkyMoonColor);

	const Vec3 nightSkyMoonInnerCoronaColor(nightSkyMoonInnerCoronaMultiplier * GetValue(PARAM_NIGHSKY_MOON_INNERCORONA_COLOR));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR, nightSkyMoonInnerCoronaColor);

	const float nightSkyMoonInnerCoronaScale(GetValue(PARAM_NIGHSKY_MOON_INNERCORONA_SCALE).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE, Vec3(nightSkyMoonInnerCoronaScale, 0, 0));

	const Vec3 nightSkyMoonOuterCoronaColor(nightSkyMoonOuterCoronaMultiplier * GetValue(PARAM_NIGHSKY_MOON_OUTERCORONA_COLOR));
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, nightSkyMoonOuterCoronaColor);

	const float nightSkyMoonOuterCoronaScale(GetValue(PARAM_NIGHSKY_MOON_OUTERCORONA_SCALE).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE, Vec3(nightSkyMoonOuterCoronaScale, 0, 0));

	// set sun shafts visibility and activate if required

	float fSunShaftsVis = GetValue(PARAM_SUN_SHAFTS_VISIBILITY).x;
	fSunShaftsVis = clamp_tpl<float>(fSunShaftsVis, 0.0f, 0.3f);
	const float fSunRaysVis = GetValue(PARAM_SUN_RAYS_VISIBILITY).x;
	const float fSunRaysAtten = GetValue(PARAM_SUN_RAYS_ATTENUATION).x;
	const float fSunRaySunColInfluence = GetValue(PARAM_SUN_RAYS_SUNCOLORINFLUENCE).x;

	const Vec4 pSunRaysCustomColor = Vec4(GetValue(PARAM_SUN_RAYS_CUSTOMCOLOR), 1.0f);

	p3DEngine->SetPostEffectParam("SunShafts_Active", (fSunShaftsVis > 0.05f || fSunRaysVis > 0.05f) ? 1.f : 0.f);
	p3DEngine->SetPostEffectParam("SunShafts_Amount", fSunShaftsVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAmount", fSunRaysVis);
	p3DEngine->SetPostEffectParam("SunShafts_RaysAttenuation", fSunRaysAtten);
	p3DEngine->SetPostEffectParam("SunShafts_RaysSunColInfluence", fSunRaySunColInfluence);
	p3DEngine->SetPostEffectParamVec4("SunShafts_RaysCustomColor", pSunRaysCustomColor);

	{
		const Vec3 cloudShadingMultipliers = Vec3(GetValue(PARAM_CLOUDSHADING_SUNLIGHT_MULTIPLIER).x, GetValue(PARAM_CLOUDSHADING_SKYLIGHT_MULTIPLIER).x, 0);
		p3DEngine->SetGlobalParameter(E3DPARAM_CLOUDSHADING_MULTIPLIERS, cloudShadingMultipliers);

		const float cloudShadingCustomSunColorMult = GetValue(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR_MULTIPLIER).x;
		const Vec3 cloudShadingCustomSunColor = cloudShadingCustomSunColorMult * GetValue(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR);
		const float cloudShadingCustomSunColorInfluence = GetValue(PARAM_CLOUDSHADING_SUNLIGHT_CUSTOM_COLOR_INFLUENCE).x;

		const Vec3 cloudShadingSunColor = p3DEngine->GetSunColor() * cloudShadingMultipliers.x;

		p3DEngine->SetGlobalParameter(E3DPARAM_CLOUDSHADING_SUNCOLOR, cloudShadingSunColor + (cloudShadingCustomSunColor - cloudShadingSunColor) * cloudShadingCustomSunColorInfluence);

		// set volumetric cloud parameters
		const Vec3 volCloudAtmosAlbedo(GetValue(PARAM_VOLCLOUD_ATMOSPHERIC_ALBEDO));
		const float volCloudRayleighBlue = 2.06e-5f; // Rayleigh scattering coefficient for blue as 488 nm wave length.
		const Vec3 volCloudRayleighScattering = volCloudAtmosAlbedo * volCloudRayleighBlue;
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING, volCloudRayleighScattering);

		const Vec3 volCloudGenParam = Vec3(GetValue(PARAM_VOLCLOUD_GLOBAL_DENSITY).x, GetValue(PARAM_VOLCLOUD_HEIGHT).x, GetValue(PARAM_VOLCLOUD_THICKNESS).x);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_GEN_PARAMS, volCloudGenParam);

		const Vec3 volCloudScatteringLow = Vec3(GetValue(PARAM_VOLCLOUD_SUN_SINGLE_SCATTERING).x, GetValue(PARAM_VOLCLOUD_SUN_LOW_ORDER_SCATTERING).x, GetValue(PARAM_VOLCLOUD_SUN_LOW_ORDER_SCATTERING_ANISTROPY).x);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_LOW, volCloudScatteringLow);

		const Vec3 volCloudScatteringHigh = Vec3(GetValue(PARAM_VOLCLOUD_SUN_HIGH_ORDER_SCATTERING).x, GetValue(PARAM_VOLCLOUD_SKY_LIGHTING_SCATTERING).x, GetValue(PARAM_VOLCLOUD_GOUND_LIGHTING_SCATTERING).x);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_HIGH, volCloudScatteringHigh);

		const Vec3 volCloudGroundColor(GetValue(PARAM_VOLCLOUD_GROUND_LIGHTING_ALBEDO));
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_GROUND_COLOR, volCloudGroundColor);

		const Vec3 volCloudScatteringMulti = Vec3(GetValue(PARAM_VOLCLOUD_MULTI_SCATTERING_ATTENUATION).x, GetValue(PARAM_VOLCLOUD_MULTI_SCATTERING_PRESERVATION).x, GetValue(PARAM_VOLCLOUD_POWDER_EFFECT).x);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_SCATTERING_MULTI, volCloudScatteringMulti);

		const float sunIntensityOriginal = (sunIntensityLux / RENDERER_LIGHT_UNIT_SCALE) / gf_PI; // divided by pi to match to GetSunColor(), it's also divided by pi in ConvertIlluminanceToLightColor().
		const float sunIntensityCustomSun = cloudShadingMultipliers.x * cloudShadingCustomSunColorMult;
		const float sunIntensityCloudAtmosphere = Lerp(sunIntensityOriginal, sunIntensityCustomSun, cloudShadingCustomSunColorInfluence);
		const float atmosphericScatteringMultiplier = GetValue(PARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING).x;
		const Vec3 volCloudWindAtmos = Vec3(GetValue(PARAM_VOLCLOUD_WIND_INFLUENCE).x, 0.0f, sunIntensityCloudAtmosphere * atmosphericScatteringMultiplier);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_WIND_ATMOSPHERIC, volCloudWindAtmos);

		const Vec3 volCloudTurbulence = Vec3(GetValue(PARAM_VOLCLOUD_CLOUD_EDGE_TURBULENCE).x, GetValue(PARAM_VOLCLOUD_CLOUD_EDGE_THRESHOLD).x, GetValue(PARAM_VOLCLOUD_ABSORPTION).x);
		p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_TURBULENCE, volCloudTurbulence);
	}

	// set ocean fog color multiplier
	const float oceanFogColorMultiplier = GetValue(PARAM_OCEANFOG_COLOR_MULTIPLIER).x;
	const Vec3 oceanFogColor(GetValue(PARAM_OCEANFOG_COLOR));
	p3DEngine->SetGlobalParameter(E3DPARAM_OCEANFOG_COLOR, oceanFogColor * oceanFogColorMultiplier);

	const float oceanFogColorDensity = GetValue(PARAM_OCEANFOG_DENSITY).x;
	p3DEngine->SetGlobalParameter(E3DPARAM_OCEANFOG_DENSITY, Vec3(oceanFogColorDensity, 0, 0));

	// set skybox multiplier
	const float skyBoxMulitplier(GetValue(PARAM_SKYBOX_MULTIPLIER).x * m_fHDRMultiplier);
	p3DEngine->SetGlobalParameter(E3DPARAM_SKYBOX_MULTIPLIER, Vec3(skyBoxMulitplier, 0, 0));

	// Set color grading stuff
	float fValue = GetValue(PARAM_COLORGRADING_FILTERS_GRAIN).x;
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_GRAIN, Vec3(fValue, 0, 0));

	const Vec4 photofilterColor = Vec4(GetValue(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR), 1.0f);
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR, Vec3(photofilterColor.x, photofilterColor.y, photofilterColor.z));
	fValue = GetValue(PARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY).x;
	p3DEngine->SetGlobalParameter(E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY, Vec3(fValue, 0, 0));

	fValue = GetValue(PARAM_COLORGRADING_DOF_FOCUSRANGE).x;
	p3DEngine->SetPostEffectParam("Dof_Tod_FocusRange", fValue);

	fValue = GetValue(PARAM_COLORGRADING_DOF_BLURAMOUNT).x;
	p3DEngine->SetPostEffectParam("Dof_Tod_BlurAmount", fValue);

	const float arrDepthConstBias[MAX_SHADOW_CASCADES_NUM] =
	{
		GetValue(PARAM_SHADOWSC0_BIAS).x, GetValue(PARAM_SHADOWSC1_BIAS).x, GetValue(PARAM_SHADOWSC2_BIAS).x, GetValue(PARAM_SHADOWSC3_BIAS).x,
		GetValue(PARAM_SHADOWSC4_BIAS).x, GetValue(PARAM_SHADOWSC5_BIAS).x, GetValue(PARAM_SHADOWSC6_BIAS).x, GetValue(PARAM_SHADOWSC7_BIAS).x,
		2.0f,                             2.0f,                             2.0f,                             2.0f,
		2.0f,                             2.0f,                             2.0f,                             2.0f,
		2.0f,                             2.0f,                             2.0f,                             2.0f 
	};

	const float arrDepthSlopeBias[MAX_SHADOW_CASCADES_NUM] =
	{
		GetValue(PARAM_SHADOWSC0_SLOPE_BIAS).x, GetValue(PARAM_SHADOWSC1_SLOPE_BIAS).x, GetValue(PARAM_SHADOWSC2_SLOPE_BIAS).x, GetValue(PARAM_SHADOWSC3_SLOPE_BIAS).x,
		GetValue(PARAM_SHADOWSC4_SLOPE_BIAS).x, GetValue(PARAM_SHADOWSC5_SLOPE_BIAS).x, GetValue(PARAM_SHADOWSC6_SLOPE_BIAS).x, GetValue(PARAM_SHADOWSC7_SLOPE_BIAS).x,
		0.5f,                                   0.5f,                                   0.5f,                                   0.5f,
		0.5f,                                   0.5f,                                   0.5f,                                   0.5f,
		0.5f,                                   0.5f,                                   0.5f,                                   0.5f 
	};

	p3DEngine->SetShadowsCascadesBias(arrDepthConstBias, arrDepthSlopeBias);

	if (gEnv->IsEditing())
	{
		p3DEngine->SetRecomputeCachedShadows();
	}

	// set volumetric fog 2 params
	const Vec3 volFogCtrlParams = Vec3(GetValue(PARAM_VOLFOG2_RANGE).x, GetValue(PARAM_VOLFOG2_BLEND_FACTOR).x, GetValue(PARAM_VOLFOG2_BLEND_MODE).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volFogCtrlParams);

	const Vec3 volFogScatteringParams = Vec3(GetValue(PARAM_VOLFOG2_INSCATTER).x, GetValue(PARAM_VOLFOG2_EXTINCTION).x, GetValue(PARAM_VOLFOG2_ANISOTROPIC).x);
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_SCATTERING_PARAMS, volFogScatteringParams);

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_RAMP, Vec3(GetValue(PARAM_VOLFOG2_RAMP_START).x, GetValue(PARAM_VOLFOG2_RAMP_END).x, 0.0f));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_COLOR, GetValue(PARAM_VOLFOG2_COLOR));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_GLOBAL_DENSITY, Vec3(GetValue(PARAM_VOLFOG2_GLOBAL_DENSITY).x, GetValue(PARAM_VOLFOG2_FINAL_DENSITY_CLAMP).x, GetValue(PARAM_VOLFOG2_GLOBAL_FOG_VISIBILITY).x));
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY, Vec3(GetValue(PARAM_VOLFOG2_HEIGHT).x, GetValue(PARAM_VOLFOG2_DENSITY).x, GetValue(PARAM_VOLFOG2_ANISOTROPIC1).x));
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_HEIGHT_DENSITY2, Vec3(GetValue(PARAM_VOLFOG2_HEIGHT2).x, GetValue(PARAM_VOLFOG2_DENSITY2).x, GetValue(PARAM_VOLFOG2_ANISOTROPIC2).x));

	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_COLOR1, GetValue(PARAM_VOLFOG2_COLOR1));
	p3DEngine->SetGlobalParameter(E3DPARAM_VOLFOG2_COLOR2, GetValue(PARAM_VOLFOG2_COLOR2));
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::SetAdvancedInfo(const SAdvancedInfo& advInfo)
{
	m_advancedInfo = advInfo;
	if (m_pTimeOfDaySpeedCVar->GetFVal() != m_advancedInfo.fAnimSpeed)
		m_pTimeOfDaySpeedCVar->Set(m_advancedInfo.fAnimSpeed);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::GetAdvancedInfo(SAdvancedInfo& advInfo) const
{
	advInfo = m_advancedInfo;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Serialize(XmlNodeRef& node, bool bLoading)
{
	if (bLoading)
	{
		Reset();

		node->getAttr("Time", m_fTime);

		node->getAttr("TimeStart", m_advancedInfo.fStartTime);
		node->getAttr("TimeEnd", m_advancedInfo.fEndTime);
		node->getAttr("TimeAnimSpeed", m_advancedInfo.fAnimSpeed);

		if (m_pTimeOfDaySpeedCVar->GetFVal() != m_advancedInfo.fAnimSpeed)
			m_pTimeOfDaySpeedCVar->Set(m_advancedInfo.fAnimSpeed);

		if (XmlNodeRef presetsNode = node->findChild("Presets"))
		{
			const int nPresetsCount = presetsNode->getChildCount();
			for (int i = 0; i < nPresetsCount; ++i)
			{
				if (XmlNodeRef presetNode = presetsNode->getChild(i))
				{
					const char* szPresetName = presetNode->getAttr("Name");
					if (!szPresetName)
					{
						continue;
					}

					const string presetName = PathUtil::ReplaceExtension(szPresetName, "env");
					std::pair<CEnvironmentPreset*, bool> result = GetOrCreatePreset(presetName);

					// Try to load only if the preset has just been created.
					if (!result.second || LoadPresetFromXML(*result.first, presetName))
					{
						if (presetNode->haveAttr("Default"))
						{
							m_pCurrentPreset = result.first;
							m_currentPresetName = presetName;
							m_defaultPresetName = presetName;
							ConstantsChanged();
						}
					}
					else // failed to load newly created preset.
					{
						m_presets.erase(presetName);
					}
				}
			}
		}
		else if (node->getChildCount()) // if the root node is not empty, this is probably the old XML format.
		{
			if (gEnv->pGameFramework)
			{
				const char* szLevelName = gEnv->pGameFramework->GetLevelName();
				if (szLevelName && *szLevelName)
				{
					const string presetName = PathUtil::Make(sPresetsLibsPath, szLevelName, "env");
					std::pair<CEnvironmentPreset*, bool> result = GetOrCreatePreset(presetName);
					// Try to load only if the preset has just been created.
					if (!result.second)
					{
						CEnvironmentPreset& preset = *result.first;
						LoadPresetFromOldFormatXML(preset, node);
						SavePreset(presetName);
					}
				}
			}
		}

		if (m_presets.empty()) // create a new default preset
		{
			LoadPreset(GetDefaultPresetFilepath());
		}
		else if (!m_pCurrentPreset)
		{
			const auto it = m_presets.begin();
			m_pCurrentPreset = it->second.get();
			m_currentPresetName = it->first;
			m_defaultPresetName = it->first;
		}

		SetTime(m_fTime, false);
		NotifyOnChange(IListener::EChangeType::CurrentPresetChanged, m_currentPresetName.c_str());
	}
	else // saving
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
				if (XmlNodeRef presetNode = presetsNode->newChild("Preset"))
				{
					presetNode->setAttr("Name", presetName.c_str());
					if (m_defaultPresetName.CompareNoCase(presetName) == 0)
					{
						presetNode->setAttr("Default", 1);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDay::Serialize(TSerialize ser)
{
	CRY_ASSERT(ser.GetSerializationTarget() != eST_Network);

	string tempName;

	ser.Value("time", m_fTime);
	ser.Value("mode", m_bEditMode);

	//Serialize only sun constants, because they can be modified in runtime by artists
	ser.Value("m_sunRotationLatitude", GetSunParams().latitude);
	ser.Value("m_sunRotationLongitude", GetSunParams().longitude);

	const int size = GetVariableCount();
	ser.BeginGroup("VariableValues");
	for (int v = 0; v < size; v++)
	{
		SVariableInfo info;
		GetVariableInfo(v, info);
		tempName = info.szName;
		tempName.replace(' ', '_');
		tempName.replace('(', '_');
		tempName.replace(')', '_');
		tempName.replace(':', '_');
		ser.BeginGroup(tempName);
		Vec3 value = GetValue(static_cast<ETimeOfDayParamID>(v));
		ser.Value("Val0", value[0]);
		ser.Value("Val1", value[1]);
		ser.Value("Val2", value[2]);
		SetValue(static_cast<ETimeOfDayParamID>(v), value);
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

std::pair<CEnvironmentPreset*, bool> CTimeOfDay::GetOrCreatePreset(const string& presetName)
{
	TPresetsSet::iterator findResult = m_presets.find(presetName);
	if (findResult != m_presets.end())
	{
		return std::make_pair(findResult->second.get(), false);
	}

	findResult = m_previewPresets.find(presetName);
	if (findResult != m_previewPresets.end())
	{
		CEnvironmentPreset* const pPreset = findResult->second.get();
		m_presets[presetName] = std::move(findResult->second);
		m_previewPresets.erase(findResult);
		return std::make_pair(pPreset, false);
	}

	std::pair<TPresetsSet::iterator, bool> insertResult = m_presets.emplace(presetName, stl::make_unique<CEnvironmentPreset>());
	return std::make_pair(insertResult.first->second.get(), true);
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
