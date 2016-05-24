// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem.h"
#include "Features/ParamMod.h"
#include "../ParticleEffect.h"
#include <CrySerialization/IArchiveHost.h>
#include <yasli/JSONOArchive.h>

CRY_PFX2_DBG

namespace pfx2
{

// PFX1 to PFX2

///////////////////////////////////////////////////////////////////////
// XML utilities for params

template<typename T, typename D> T ResetValue(T& ref, D def)
{
	T val = ref;
	ref = T(def);
	return val;
}

template<typename T> T ResetValue(T& ref)
{
	return ResetValue(ref, T());
}

template<typename P, typename T>
bool IsConstant(const P& value, const T& defValue)
{
	return value(VMAX) == defValue && value(VMIN) == defValue;
}

XmlNodeRef MakeFeature(cstr name)
{
	return gEnv->pSystem->CreateXmlNode(name);
}

template<typename T>
void AddValue(XmlNodeRef node, cstr name, const T& value)
{
	node->newChild(name)->setAttr("value", value);
}

template<typename T>
void AddValue(XmlNodeRef node, cstr name, const Vec2_tpl<T>& value)
{
	XmlNodeRef paramNode = node->newChild(name);
	AddValue(paramNode, "Element", value.x);
	AddValue(paramNode, "Element", value.y);
}

template<typename T>
void AddValue(XmlNodeRef node, cstr name, const Vec3_tpl<T>& value)
{
	XmlNodeRef paramNode = node->newChild(name);
	AddValue(paramNode, "Element", value.x);
	AddValue(paramNode, "Element", value.y);
	AddValue(paramNode, "Element", value.z);
}

void AddValue(XmlNodeRef node, cstr name, const Color3F& color)
{
	XmlNodeRef paramNode = node->newChild(name);
	AddValue(paramNode, "Element", float_to_ufrac8(color.x));
	AddValue(paramNode, "Element", float_to_ufrac8(color.y));
	AddValue(paramNode, "Element", float_to_ufrac8(color.z));
}

template<typename T>
void AddValue(XmlNodeRef node, cstr name, const Vec3_Zero<T>& value)
{
	// Convert Y-up to Z-up.
	XmlNodeRef paramNode = node->newChild(name);
	AddValue(paramNode, "Element", value.x);
	if (T(-value.z))
		// signed and non-zero
		AddValue(paramNode, "Element", -value.z);
	else
		// unsigned or zero
		AddValue(paramNode, "Element", value.z);
	AddValue(paramNode, "Element", value.y);
}

template<typename T, typename D>
void ConvertValue(XmlNodeRef node, cstr name, T& value, D defValue)
{
	if (value != T(defValue))
	{
		AddValue(node, name, value);
		value = T(defValue);
	}
}

template<typename T>
void ConvertValue(XmlNodeRef node, cstr name, T& value)
{
	ConvertValue(node, name, value, T());
}

template<typename T>
void ConvertValueString(XmlNodeRef node, cstr name, T& value, T defValue = T())
{
	if (value != defValue)
	{
		AddValue(node, name, TypeInfo(&value).ToString(&value));
		value = defValue;
	}
}

template<typename T>
void ConvertScalarValue(XmlNodeRef node, cstr name, Vec3_tpl<T>& value)
{
	if (value.y)
		AddValue(node, name, value.y);
	value.zero();
}

template<typename T>
struct SerializeNames
{
	static cstr value() { return "value"; }
	static cstr mods()  { return "modifiers"; }
	static cstr curve() { return "Curve"; }
};

template<>
struct SerializeNames<Color3F>
{
	static cstr value() { return "Color"; }
	static cstr mods()  { return "Modifiers"; }
	static cstr curve() { return "ColorCurve"; }
};

// Convert pfx1 variant parameters, with randomness and time curves
template<typename T>
void AddParamMods(XmlNodeRef mods, TVarParam<T>& param)
{
	if (param.GetRandomRange())
	{
		XmlNodeRef elem = mods->newChild("Element");
		AddValue(elem, "type", "Random");
		XmlNodeRef data = elem->newChild("data");
		AddValue(data, "Amount", param.GetRandomRange());
	}
}

template<>
void AddParamMods(XmlNodeRef mods, TVarParam<Color3F>& param)
{
	if (param.GetRandomRange())
	{
		XmlNodeRef elem = mods->newChild("Element");
		AddValue(elem, "type", "ColorRandom");
		XmlNodeRef data = elem->newChild("data");
		if (param.GetRandomRange().HasRandomHue())
			AddValue(data, "RGB", param.GetRandomRange());
		else
			AddValue(data, "Luminance", param.GetRandomRange());
	}
}

template<typename T>
void AddCurve(XmlNodeRef node, const TCurve<T>& curve)
{
	AddValue(node, "Curve", curve.ToString());
}

template<>
void AddCurve(XmlNodeRef node, const TCurve<Color3F>& curve)
{
	XmlNodeRef paramNode = node->newChild("ColorCurve");

	// Separate out color components into 3 curves
	for (int c = 0; c < 3; ++c)
	{
		typedef spline::SplineKey<float> KeyType;
		struct ScalarSpline : public spline::CSplineKeyInterpolator<KeyType>
		{
			virtual spline::Formatting GetFormatting() const override
			{
				return ";,:";
			}
		};

		ScalarSpline sspline;

		for (int i = 0, n = curve.num_keys(); i < n; ++i)
		{
			auto key = curve.key(i);
			KeyType scalarkey;
			scalarkey.time = key.time;
			scalarkey.flags = key.flags;
			scalarkey.value = key.value[c];
			sspline.insert_key(scalarkey);
		}
		AddValue(paramNode, "Element", sspline.ToString());
	}
}

template<typename T>
void AddParamMods(XmlNodeRef mods, TVarEParam<T>& param)
{
	AddParamMods(mods, static_cast<TVarParam<T>&>(param));
	if (param.GetStrengthCurve()(VMIN) != T(1.f))
	{
		XmlNodeRef elem = mods->newChild("Element");
		AddValue(elem, "type", SerializeNames<T>::curve());
		XmlNodeRef data = elem->newChild("data");
		AddValue(data, "TimeSource", "ParentTime");
		AddValue(data, "SpawnOnly", "true");
		AddCurve(data, param.GetStrengthCurve());
	}
}

template<typename T>
void AddParamMods(XmlNodeRef mods, TVarEPParam<T>& param)
{
	AddParamMods(mods, static_cast<TVarEParam<T>&>(param));
	if (param.GetAgeCurve()(VMIN) != T(1.f))
	{
		XmlNodeRef elem = mods->newChild("Element");
		AddValue(elem, "type", SerializeNames<T>::curve());
		XmlNodeRef data = elem->newChild("data");
		AddValue(data, "TimeSource", "SelfTime");
		AddCurve(data, param.GetAgeCurve());
	}
}

template<typename P>
bool ConvertParam(XmlNodeRef node, cstr name, P& param, float defValue = 0.f)
{
	typedef typename P::T PT;
	if (param(VMAX) == PT(0))
		ResetValue(param);
	if (IsConstant(param, PT(defValue)))
		return false;

	XmlNodeRef p = node->newChild(name);
	AddValue(p, SerializeNames<PT>::value(), param(VMAX));
	XmlNodeRef mods = p->newChild(SerializeNames<PT>::mods());
	AddParamMods(mods, param);

	// Reset param to default value, to mark converted
	ResetValue(param, defValue);
	return true;
}

// Convert just the base value of a pfx1 variant parameter; the variance remains unconverted
template<typename P>
bool ConvertParamBase(XmlNodeRef node, cstr name, P& param, float defValue = 0.f)
{
	if (param(VMAX) == 0.f)
		ResetValue(param);
	if (param(VMAX) == defValue)
		return false;

	AddValue(node, name, param(VMAX));

	param.Set(defValue);
	return true;
}

///////////////////////////////////////////////////////////////////////
// Feature creation

void AddFeature(IParticleComponent& component, XmlNodeRef params);

void LogError(const string& error, IParticleComponent* component = 0)
{
	if (component)
	{
		gEnv->pLog->Log(" Component %s: ! %s", component->GetName(), error.c_str());

		static int nest = 0;
		if (!nest++)
		{
			// component->SetComment(string(component->GetComment()) + "! " + error + "\n");
			XmlNodeRef comment = MakeFeature("GeneralComment");
			AddValue(comment, "Text", error);
			AddFeature(*component, comment);
		}
		--nest;
	}
	else
	{
		gEnv->pLog->Log("! %s", error.c_str());
	}
}

IParticleFeature* AddFeature(IParticleComponent& component, cstr featureName)
{
	// Search for feature
	uint nFeatures = GetIParticleSystem()->GetNumFeatureParams();
	for (uint i = 0; i < nFeatures; ++i)
	{
		auto const& fp = GetIParticleSystem()->GetFeatureParam(i);
		if (string(fp.m_groupName) + fp.m_featureName == featureName)
		{
			// Add feature
			uint index = component.GetNumFeatures();
			component.AddFeature(index, fp);
			return component.GetFeature(index);
		}
	}

	LogError(string("Feature not found: ") + featureName, &component);
	return nullptr;
}

void AddFeature(IParticleComponent& component, XmlNodeRef params)
{
	if (params->getChildCount())
	{
		if (IParticleFeature* feature = AddFeature(component, params->getTag()))
		{
			if (!Serialization::LoadXmlNode(*feature, params))
				LogError(string("Feature serialization error: ") + params->getTag(), &component);
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Feature-specific conversion

void ConvertChild(IParticleComponent& parent, IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef feature;

	switch (params.eSpawnIndirection)
	{
	case ParticleParams::ESpawn::ParentStart:
		feature = MakeFeature("SecondGenOnSpawn");
		break;
	case ParticleParams::ESpawn::ParentDeath:
		feature = MakeFeature("SecondGenOnDeath");
		break;
	default:
		return;
	}

	XmlNodeRef comp = feature->newChild("Components");
	AddValue(comp, "Element", component.GetName());
	AddFeature(parent, feature);

	ResetValue(params.eSpawnIndirection);
}

void ConvertSpawn(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef spawn;

	auto amount = ResetValue(params.fCount);
	if (ResetValue(params.bContinuous) && params.fParticleLifeTime)
	{
		// Set rate = amount / lifetime
		float life = params.fParticleLifeTime(VMAX);
		if (params.fEmitterLifeTime)
			life = min(life, params.fEmitterLifeTime(VMAX));
		amount.Set(amount(VMAX) / life);
		spawn = MakeFeature("SpawnRate");
	}
	else
	{
		spawn = MakeFeature("SpawnCount");
	}

	ConvertParam(spawn, "Amount", amount);
	if (params.fSpawnDelay)
	{
		XmlNodeRef delay = spawn->newChild("Delay");
		delay->newChild("State")->setAttr("value", "true");
		ConvertParam(delay, "Value", params.fSpawnDelay);
	}
	if (params.fEmitterLifeTime)
	{
		XmlNodeRef delay = spawn->newChild("Duration");
		delay->newChild("State")->setAttr("value", "true");
		ConvertParam(delay, "Value", params.fEmitterLifeTime);
	}

	AddFeature(component, spawn);

	if (params.fParticleLifeTime)
	{
		XmlNodeRef life = MakeFeature("LifeTime");
		ConvertParam(life, "LifeTime", params.fParticleLifeTime);
		AddFeature(component, life);
	}
}

void ConvertAppearance(IParticleComponent& component, ParticleParams& params)
{
	// Blending feature
	XmlNodeRef blending = MakeFeature("AppearanceBlending");
	ConvertValueString(blending, "BlendMode", params.eBlendType, ParticleParams::EBlend(ParticleParams::EBlend::AlphaBased));
	AddFeature(component, blending);

	// Material feature
	XmlNodeRef material = MakeFeature("AppearanceMaterial");

	if (!params.sMaterial.empty())
		ConvertValueString(material, "Material", params.sMaterial);
	if (!params.sTexture.empty())
		ConvertValueString(material, "Texture", params.sTexture);

	AddFeature(component, material);

	// TextureTiling feature
	auto& tiling = params.TextureTiling;
	if (tiling.nTilesX > 1 || tiling.nTilesY > 1)
	{
		XmlNodeRef tile = MakeFeature("AppearanceTexture Tiling");
		ConvertValue(tile, "TilesX", tiling.nTilesX);
		ConvertValue(tile, "TilesY", tiling.nTilesY);
		ConvertValue(tile, "FirstTile", tiling.nFirstTile);
		AddValue(tile, "TileCount", ResetValue(tiling.nVariantCount) * tiling.nAnimFramesCount);

		if (tiling.nAnimFramesCount > 1)
		{
			XmlNodeRef anim = tile->newChild("Animation");
			ConvertValue(anim, "FrameCount", tiling.nAnimFramesCount);
			ConvertValue(anim, "FrameRate", tiling.fAnimFramerate);
			ConvertValueString(anim, "CycleMode", tiling.eAnimCycle);
			ConvertValue(anim, "FrameBlending", tiling.bAnimBlend);
		}
		AddFeature(component, tile);
	}

	// Lighting feature
	XmlNodeRef lighting = MakeFeature("AppearanceLighting");
	if (params.fDiffuseLighting != 1.f)
	{
		AddValue(lighting, "Albedo", params.fDiffuseLighting * 100.f);
		params.fDiffuseLighting = 1.f;
	}
	if (params.fDiffuseLighting && params.fCurvature)
		AddValue(lighting, "Curvature", params.fCurvature);
	params.fCurvature = 1.f;
	ConvertValue(lighting, "BackLight", params.fDiffuseBacklighting);
	ConvertValue(lighting, "Emissive", params.fEmissiveLighting);
	ConvertValue(lighting, "ReceiveShadows", params.bReceiveShadows);

	AddFeature(component, lighting);

	if (params.bSoftParticle)
	{
		XmlNodeRef soft = MakeFeature("AppearanceSoftIntersect");
		AddValue(soft, "Softness", params.bSoftParticle.fSoftness);
		ResetValue(params.bSoftParticle);
		AddFeature(component, soft);
	}
}

void ConvertSprites(IParticleComponent& component, ParticleParams& params)
{
	if ((!params.sTexture.empty() || !params.sMaterial.empty()) && params.sGeometry.empty())
	{
		// RenderSprite feature
		XmlNodeRef sprite = MakeFeature("RenderSprites");

		if (params.eFacing == ParticleParams::EFacing::Camera)
		{
			ResetValue(params.eFacing);
			if (ResetValue(params.bOrientToVelocity) || params.fStretch)
			{
				AddValue(sprite, "FacingMode", "Velocity");
				ConvertParamBase(sprite, "AxisScale", params.fStretch);
			}
			else
				AddValue(sprite, "FacingMode", "Camera");
			AddValue(sprite, "SphericalProjection", ResetValue(params.fSphericalApproximation, 1.f));
		}
		else if (params.eFacing == ParticleParams::EFacing::Free)
		{
			ResetValue(params.eFacing);
			AddValue(sprite, "FacingMode", "Free");
		}

		ConvertParamBase(sprite, "AspectRatio", params.fAspect, 1.f);

		AddFeature(component, sprite);

		ConvertAppearance(component, params);
	}
}

void ConvertGeometry(IParticleComponent& component, ParticleParams& params)
{
	if (!params.sGeometry.empty())
	{
		XmlNodeRef mesh = MakeFeature("RenderMeshes");
		ConvertValueString(mesh, "Mesh", params.sGeometry);

		AddValue(mesh, "Scale", Vec3(params.fSize(VMAX)));
		params.fSize.Set(1.0f);

		AddValue(mesh, "SizeMode", "Scale");
		AddValue(mesh, "OriginMode", params.bNoOffset ? "Origin" : "Center");
		AddValue(mesh, "PiecePlacement", ResetValue(params.bNoOffset) ? "SubPlacement" : "Standad");

		ConvertValueString(mesh, "PiecesMode", params.eGeometryPieces);

		// Material feature
		if (!params.sMaterial.empty())
		{
			XmlNodeRef material = MakeFeature("AppearanceMaterial");
			ConvertValueString(material, "Material", params.sMaterial);
			AddFeature(component, material);
		}

		if (params.eFacing == ParticleParams::EFacing::Free)
			ResetValue(params.eFacing);

		// Clear unused params
		ResetValue(params.sTexture);
		ResetValue(params.eBlendType);
		ResetValue(params.TextureTiling);
		ResetValue(params.fDiffuseLighting);
		ResetValue(params.fDiffuseBacklighting);
		ResetValue(params.fEmissiveLighting);
		ResetValue(params.bCastShadows);
		ResetValue(params.bReceiveShadows);
		ResetValue(params.bSoftParticle);

		AddFeature(component, mesh);
	}
}

void ConvertFields(IParticleComponent& component, ParticleParams& params)
{
	// Opacity
	XmlNodeRef opacity = MakeFeature("FieldOpacity");
	ConvertParam(opacity, "value", params.fAlpha, 1.f);
	ConvertValue(opacity, "AlphaScale", reinterpret_cast<Vec2&>(params.AlphaClip.fScale), Vec2(0, 1));
	ConvertValue(opacity, "ClipLow", reinterpret_cast<Vec2&>(params.AlphaClip.fSourceMin), Vec2(0, 0));
	ConvertValue(opacity, "ClipRange", reinterpret_cast<Vec2&>(params.AlphaClip.fSourceWidth), Vec2(1, 1));
	AddFeature(component, opacity);

	// Color
	XmlNodeRef color = MakeFeature("FieldColor");
	ConvertParam(color, "Color", params.cColor, 1.f);
	AddFeature(component, color);

	// Size
	XmlNodeRef size = MakeFeature("FieldSize");
	ConvertParam(size, "value", params.fSize);
	ResetValue(params.fSize, 1.f);
	AddFeature(component, size);

	// Pixel size
	if (params.fMinPixels)
	{
		XmlNodeRef pix = MakeFeature("FieldPixelSize");
		ConvertValue(pix, "MinSize", params.fMinPixels);
		AddValue(pix, "AffectOpacity", false);
		AddFeature(component, pix);
	}
}

void ConvertLocation(IParticleComponent& component, ParticleParams& params)
{
	if (params.eAttachType)
	{
		XmlNodeRef attach = MakeFeature("LocationGeometry");
		ConvertValueString(attach, "Source", params.eAttachType);
		ConvertValueString(attach, "Location", params.eAttachForm);
		if (params.fSpeed)
		{
			AddValue(attach, "OrientParticles", true);
			if (params.fStretch)
			{
				TVarEParam<::UFloat> offset = params.fStretch;
				offset.Set(offset(VMAX) * params.fStretch.fOffsetRatio * params.fSpeed(VMAX));
				ConvertParam(attach, "Offset", offset);
			}
		}
		ConvertParam(attach, "Velocity", params.fSpeed);
		AddFeature(component, attach);
	}

	if (ResetValue(params.bSpaceLoop))
	{
		XmlNodeRef omni = MakeFeature("LocationOmni");
		TVarParam<UFloat> vis(max<float>(params.fCameraMaxDistance, params.vPositionOffset.y + params.vRandomOffset.y));
		ResetValue(params.fCameraMaxDistance);
		ResetValue(params.vPositionOffset);
		ResetValue(params.vRandomOffset);
		ConvertParam(omni, "Visibility", vis);
		AddFeature(component, omni);
	}

	XmlNodeRef loc = MakeFeature("LocationOffset");
	ConvertValue(loc, "Offset", params.vPositionOffset);
	AddFeature(component, loc);

	if (params.vRandomOffset.IsZero())
	{
		params.fOffsetRoundness = params.fOffsetInnerFraction = 0.f;
	}
	if (params.fOffsetRoundness < 0.5f)
	{
		XmlNodeRef box = MakeFeature("LocationBox");
		ConvertValue(box, "Dimension", params.vRandomOffset);
		AddFeature(component, box);
	}
	else
	{
		XmlNodeRef sphere = MakeFeature("LocationSphere");
		ConvertValue(sphere, "AxisScale", params.vRandomOffset);
		TVarParam<::UFloat> radius(1.f);
		ConvertParam(sphere, "Radius", radius);
		AddFeature(component, sphere);
		if (params.fOffsetRoundness == 1.f)
			params.fOffsetRoundness = 0.f;
	}
}

void ConvertVelocity(IParticleComponent& component, ParticleParams& params)
{
	if (params.fSpeed)
	{
		if (!params.fEmitAngle)
		{
			XmlNodeRef vel = MakeFeature("VelocityDirectional");
			ConvertParam(vel, "Scale", params.fSpeed);
			AddValue(vel, "Velocity", Vec3(0, 0, 1));
			AddFeature(component, vel);
		}
		else if (params.fEmitAngle(VMIN) == 180.f)
		{
			XmlNodeRef vel = MakeFeature("VelocityOmniDirectional");
			ConvertParam(vel, "Velocity", params.fSpeed);
			AddFeature(component, vel);
		}
		else
		{
			XmlNodeRef vel = MakeFeature("VelocityCone");
			ConvertParam(vel, "Velocity", params.fSpeed);
			ConvertParam(vel, "Angle", params.fEmitAngle);
			AddFeature(component, vel);
		}
	}
	else
	{
		ResetValue(params.fSpeed);
		ResetValue(params.fEmitAngle);
	}
	if (params.fInheritVelocity)
	{
		XmlNodeRef vel = MakeFeature("VelocityInherit");
		TVarParam<SFloat> scale(ResetValue(params.fInheritVelocity));
		ConvertParam(vel, "Scale", scale);
		AddFeature(component, vel);
	}
	if (ResetValue(params.bMoveRelativeEmitter))
	{
		XmlNodeRef vel = MakeFeature("VelocityMoveRelativeToEmitter");
		AddValue(vel, "PositionInherit", 1.f);
		AddFeature(component, vel);
	}
}

void ConvertAngles2D(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef ang = MakeFeature("AnglesRotate2D");
	ConvertScalarValue(ang, "InitialAngle", params.vInitAngles);
	ConvertScalarValue(ang, "RandomAngle", params.vRandomAngles);
	ConvertScalarValue(ang, "InitialSpin", params.vRotationRate);
	ConvertScalarValue(ang, "RandomSpin", params.vRandomRotationRate);
	AddFeature(component, ang);
}

void ConvertAngles3D(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef ang = MakeFeature("AnglesRotate3D");
	ConvertValue(ang, "InitialAngle", params.vInitAngles);
	ConvertValue(ang, "RandomAngle", params.vRandomAngles);
	ConvertValue(ang, "InitialSpin", params.vRotationRate);
	ConvertValue(ang, "RandomSpin", params.vRandomRotationRate);
	AddFeature(component, ang);
}

void ConvertMotion(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef phys = MakeFeature("MotionPhysics");
	ConvertParam(phys, "gravity", params.fGravityScale);
	if (params.fAirResistance(VMAX) == 0.f)
		ResetValue(params.fAirResistance);
	ConvertParam(phys, "drag", static_cast<TVarEPParam<::UFloat>&>(params.fAirResistance));
	ConvertValue(phys, "UniformAcceleration", params.vAcceleration);
	ConvertValue(phys, "WindMultiplier", params.fAirResistance.fWindScale, 1.f);
	AddFeature(component, phys);
}

void ConvertVisibility(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef vis = MakeFeature("AppearanceVisibility");
	ConvertValue(vis, "ViewDistanceMultiple", params.fViewDistanceAdjust, 1.f);
	ConvertValue(vis, "MinCameraDistance", params.fCameraMinDistance);
	ConvertValue(vis, "MaxCameraDistance", params.fCameraMaxDistance);
	ConvertValue(vis, "DrawNear", params.bDrawNear);
	ConvertValue(vis, "DrawOnTop", params.bDrawOnTop);
	AddFeature(component, vis);
}

// Convert all features
void ConvertParamsToFeatures(IParticleComponent& component, const ParticleParams& origParams, IParticleComponent* pParent = nullptr)
{
	ParticleParams params = origParams;

	// Convert params to features, resetting params as we do.
	// Any non-default params afterwards have not been converted.
	if (pParent)
		ConvertChild(*pParent, component, params);
	ConvertSpawn(component, params);
	ConvertSprites(component, params);
	ConvertGeometry(component, params);
	ConvertFields(component, params);
	ConvertLocation(component, params);
	ConvertVelocity(component, params);
	if (origParams.eFacing == ParticleParams::EFacing::Free || !origParams.sGeometry.empty())
		ConvertAngles3D(component, params);
	else
		ConvertAngles2D(component, params);
	ConvertMotion(component, params);
	ConvertVisibility(component, params);

	static ParticleParams defaultParams;
	string unconverted = TypeInfo(&params).ToString(&params, FToString().NamedFields(1).SkipDefault(1), &defaultParams);
	if (!unconverted.empty())
	{
		unconverted.replace(",", ", ");
		LogError("Unconverted parameters: " + unconverted, &component);
	}
}

void ConvertSubEffects(IParticleEffectPfx2& newEffect, const ::IParticleEffect& oldSubEffect, IParticleComponent* pParent = nullptr)
{
	IParticleComponent* component = nullptr;
	if (oldSubEffect.IsEnabled())
	{
		if (!oldSubEffect.GetParticleParams().eSpawnIndirection || pParent)
		{
			uint index = newEffect.GetNumComponents();
			newEffect.AddComponent(index);
			component = newEffect.GetComponent(index);

			cstr name = oldSubEffect.GetName();
			if (cstr base = strrchr(name, '.'))
			{
				name = base + 1;
			}
			component->SetName(name);

			ConvertParamsToFeatures(*component, oldSubEffect.GetParticleParams(), pParent);
		}
	}

	int childCount = oldSubEffect.GetChildCount();
	for (int i = 0; i < childCount; ++i)
	{
		ConvertSubEffects(newEffect, *oldSubEffect.GetChild(i), component);
	}
}

string CParticleSystem::ConvertPfx1Name(cstr oldEffectName)
{
	string newName = string(gEnv->pCryPak->GetGameFolder()) + "/Particles/pfx1/";
	newName += oldEffectName;
	newName.replace('.', '/');
	newName += ".pfx";
	return newName;
}

PParticleEffect CParticleSystem::ConvertEffect(const ::IParticleEffect* pOldEffect, bool bReplace)
{
	string oldName = pOldEffect->GetFullName();
	string newName = ConvertPfx1Name(oldName);
	PParticleEffect pNewEffect = FindEffect(newName);
	if (pNewEffect && !bReplace)
		return pNewEffect;

	pNewEffect = CreateEffect();
	RenameEffect(pNewEffect, newName);
	gEnv->pLog->Log("PFX1 to PFX2 \"%s\":", oldName.c_str());
	ConvertSubEffects(*pNewEffect, *pOldEffect);

	gEnv->pLog->Log(" - Saving as \"%s\":", newName.c_str());
	gEnv->pCryPak->MakeDir(PathUtil::GetParentDirectory(newName).c_str());
	Serialization::SaveJsonFile(newName, *pNewEffect);

	return pNewEffect;
}

}
