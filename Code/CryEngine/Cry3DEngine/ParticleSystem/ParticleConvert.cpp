// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem.h"
#include "Features/ParamMod.h"
#include "../ParticleEffect.h"
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/yasli/JSONOArchive.h>

CRY_PFX2_DBG

namespace pfx2
{

// PFX1 to PFX2

///////////////////////////////////////////////////////////////////////
// XML utilities for params

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

template<typename T, typename D> T ResetValue(T& ref, const D& def)
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

void AddValue(XmlNodeRef node, cstr name, const Color3F& color)
{
	XmlNodeRef paramNode = node->newChild(name);
	AddValue(paramNode, "Element", float_to_ufrac8(color.x));
	AddValue(paramNode, "Element", float_to_ufrac8(color.y));
	AddValue(paramNode, "Element", float_to_ufrac8(color.z));
}

template<typename T>
void AddValue(XmlNodeRef node, cstr name, const Vec3_tpl<T>& value)
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
void ConvertValue(XmlNodeRef node, cstr name, T& value, D inDefault, D outDefault)
{
	if (value != T(outDefault))
		AddValue(node, name, value);
	value = T(inDefault);
}

template<typename T, typename D>
void ConvertValue(XmlNodeRef node, cstr name, T& value, D defValue)
{
	ConvertValue(node, name, value, defValue, defValue);
}

template<typename T>
void ConvertValue(XmlNodeRef node, cstr name, T& value)
{
	ConvertValue(node, name, value, T(), T());
}

template<typename T, typename D>
void ConvertValueString(XmlNodeRef node, cstr name, T& value, D defValue)
{
	if (value != defValue)
	{
		AddValue(node, name, TypeInfo(&value).ToString(&value));
		value = defValue;
	}
}

template<typename T>
void ConvertValueString(XmlNodeRef node, cstr name, T& value)
{
	ConvertValueString(node, name, value, T());
}

template<typename T>
void ConvertValueParam(XmlNodeRef node, cstr name, T& value, float defValue = 0.f)
{
	if (value != defValue)
	{
		XmlNodeRef p = node->newChild(name);
		ConvertValue(p, SerializeNames<T>::value(), value);
	}
}

template<typename T>
void ConvertScalarValue(XmlNodeRef node, cstr name, Vec3_tpl<T>& value)
{
	if (value.y)
		AddValue(node, name, value.y);
	value.zero();
}

XmlNodeRef AddPtrElement(XmlNodeRef container, cstr typeName)
{
	XmlNodeRef elem = container->newChild("Element");
	AddValue(elem, "type", typeName);
	return elem->newChild("data");
}

// Convert pfx1 variant parameters, with randomness and time curves
template<typename T>
void AddParamMods(XmlNodeRef mods, TVarParam<T>& param)
{
	if (param.GetRandomRange())
	{
		XmlNodeRef data = AddPtrElement(mods, "Random");
		AddValue(data, "Amount", param.GetRandomRange());
	}
}

template<>
void AddParamMods(XmlNodeRef mods, TVarParam<Color3F>& param)
{
	if (param.GetRandomRange())
	{
		XmlNodeRef data = AddPtrElement(mods, "ColorRandom");
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
		XmlNodeRef data = AddPtrElement(mods, SerializeNames<T>::curve());
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
		XmlNodeRef data = AddPtrElement(mods, SerializeNames<T>::curve());
		AddValue(data, "TimeSource", "SelfTime");
		AddCurve(data, param.GetAgeCurve());
	}
}

template<typename P>
void ConvertParam(XmlNodeRef node, cstr name, P& param, float inDefault = 0.f, float outDefault = 0.f, bool bInherit = false)
{
	typedef typename P::T PT;
	if (param(VMAX) == PT(0))
		ResetValue(param);
	if (!bInherit && IsConstant(param, PT(outDefault)))
	{
		ResetValue(param, inDefault);
		return;
	}

	XmlNodeRef p = node->newChild(name);
	XmlNodeRef mods = gEnv->pSystem->CreateXmlNode(SerializeNames<PT>::mods());

	if (bInherit)
	{
		AddValue(p, SerializeNames<PT>::value(), 1.0f);
		XmlNodeRef inherit = AddPtrElement(mods, "Inherit");
		AddValue(inherit, "SpawnOnly", "false");
	}
	else
	{
		AddValue(p, SerializeNames<PT>::value(), param(VMAX));
		AddParamMods(mods, param);

		// Reset param to default value, to mark converted
		ResetValue(param, inDefault);
	}

	if (mods->getChildCount())
		p->addChild(mods);
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

template<typename P>
P& ZeroIsInfinity(P& param, bool yes = true)
{
	if (yes && !param)
		param = gInfinity;
	return param;
}

// Special-purpose param to convert StrengthCurve to AgeCurve
template<class S>
struct TVarEtoPParam : TVarEPParam<S>
{
	TVarEtoPParam() {}
	TVarEtoPParam(const TVarEParam<S>& in)
	{
		this->Set(in(VMAX), in.GetRandomRange());
		this->m_ParticleAge = in.GetStrengthCurve();
	}
};

///////////////////////////////////////////////////////////////////////
// Feature creation

void AddFeature(IParticleComponent& component, XmlNodeRef params, bool force = false);

void LogError(const string& error, IParticleComponent* component = 0)
{
	if (component)
	{
		CryLog(" Component %s: ! %s", component->GetName(), error.c_str());

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
		CryLog("! %s", error.c_str());
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

void AddFeature(IParticleComponent& component, XmlNodeRef params, bool force)
{
	if (force || params->getChildCount())
	{
		if (IParticleFeature* feature = AddFeature(component, params->getTag()))
		{
			if (!Serialization::LoadXmlNode(*feature, params))
				LogError(string("Feature serialization error: ") + params->getTag(), &component);
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Component creation

static const Vec2 NodePositionIncrement(256, 0);

IParticleComponent* AddComponent(IParticleEffectPfx2& effect, cstr name)
{
	uint index = effect.GetNumComponents();
	IParticleComponent* component = effect.AddComponent();
	component->SetName(name);
	component->SetNodePosition(NodePositionIncrement * (float)index);
	return component;
}

///////////////////////////////////////////////////////////////////////
// Feature-specific conversion

void ConvertChild(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef feature;

	switch (params.eSpawnIndirection)
	{
	case ParticleParams::ESpawn::ParentStart:
		feature = MakeFeature("ChildOnBirth");
		break;
	case ParticleParams::ESpawn::ParentDeath:
		feature = MakeFeature("ChildOnDeath");
		break;
	case ParticleParams::ESpawn::ParentCollide:
		feature = MakeFeature("ChildOnCollide");
		break;
	default:
		return;
	}

	AddFeature(component, feature, true);
	ResetValue(params.eSpawnIndirection);
}

void ConvertSpawn(IParticleComponent& component, ParticleParams& params, bool allowZeroLifetime = false)
{
	XmlNodeRef spawn = MakeFeature("SpawnCount");

	ConvertParam(spawn, "Amount", params.fCount, 0.0f, 1.0f);
	ConvertParam(spawn, "Delay", params.fSpawnDelay);
	ConvertParam(spawn, "Duration", ZeroIsInfinity(params.fEmitterLifeTime, ResetValue(params.bContinuous)));
	ConvertParam(spawn, "Restart", ZeroIsInfinity(params.fPulsePeriod), 0.0f, gInfinity);

	AddFeature(component, spawn);

	XmlNodeRef life = MakeFeature("LifeTime");
	ConvertParam(life, "LifeTime", ZeroIsInfinity(params.fParticleLifeTime, !allowZeroLifetime));
	AddFeature(component, life);
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
	if (params.fDiffuseLighting || params.fEmissiveLighting)
	{
		XmlNodeRef lighting = MakeFeature("AppearanceLighting");
		ConvertValue(lighting, "Diffuse", params.fDiffuseLighting, 1.f);
		ConvertValue(lighting, "BackLight", params.fDiffuseBacklighting);
		ConvertValue(lighting, "Curvature", params.fCurvature, 1.f, 0.f);
		ConvertValue(lighting, "ReceiveShadows", params.bReceiveShadows);
		ConvertValue(lighting, "Emissive", params.fEmissiveLighting);
		AddValue(lighting, "AffectedByFog", !ResetValue(params.bNotAffectedByFog));
		AddFeature(component, lighting);
	}

	// Soft particle
	if (params.bSoftParticle)
	{
		XmlNodeRef soft = MakeFeature("AppearanceSoftIntersect");
		AddValue(soft, "Softness", params.bSoftParticle.fSoftness);
		ResetValue(params.bSoftParticle);
		AddFeature(component, soft);
	}

	// Pixel size
	if (params.fMinPixels)
	{
		XmlNodeRef pix = MakeFeature("FieldPixelSize");
		ConvertValue(pix, "MinSize", params.fMinPixels);
		AddValue(pix, "AffectOpacity", false);
		AddFeature(component, pix);
	}
}

void ClearAppearance(ParticleParams& params)
{
	// Clear appearance params, after processing sprite and geometry fields
	ResetValue(params.eFacing);
	ResetValue(params.sTexture);
	ResetValue(params.eBlendType, ParticleParams::EBlend::AlphaBased);
	ResetValue(params.TextureTiling);
	ResetValue(params.fDiffuseLighting, 1.f);
	ResetValue(params.fDiffuseBacklighting);
	ResetValue(params.fEmissiveLighting);
	ResetValue(params.bCastShadows);
	ResetValue(params.bReceiveShadows);
	ResetValue(params.bNotAffectedByFog);
	ResetValue(params.bSoftParticle);
}

void ConvertSprites(IParticleComponent& component, ParticleParams& params)
{
	if ((!params.sTexture.empty() || !params.sMaterial.empty()) && params.sGeometry.empty())
	{
		XmlNodeRef render;
		XmlNodeRef ang = MakeFeature("AnglesAlign");

		if (params.eFacing == ParticleParams::EFacing::Decal)
		{
			// RenderDecals feature
			render = MakeFeature("RenderDecals");
			ConvertValue(render, "Thickness", params.fThickness, 1.0f);
			ConvertValue(render, "SortBias", params.fSortOffset);
		}

		else if (params.Connection)
		{
			// RenderRibbon feature
			render = MakeFeature("RenderRibbon");
			if (params.eFacing == ParticleParams::EFacing::Camera || params.eFacing == ParticleParams::EFacing::Free)
				ConvertValueString(render, "RibbonMode", params.eFacing);

			ConvertValue(render, "ConnectToOrigin", params.Connection.bConnectToOrigin);
			AddValue(render, "StreamSource",
				ResetValue(params.Connection.eTextureMapping) == ParticleParams::SConnection::ETextureMapping::PerParticle ? "Spawn" : "Age");
			ConvertValue(render, "TextureFrequency", params.Connection.fTextureFrequency, 1.f);
			ResetValue(static_cast<TSmallBool&>(params.Connection));
		}
		else
		{
			// RenderSprites feature
			render = MakeFeature("RenderSprites");
			if (params.eFacing == ParticleParams::EFacing::Camera)
			{
				if (params.bOrientToVelocity || params.fStretch)
				{
					AddValue(render, "FacingMode", "Velocity");
					ConvertParamBase(render, "AxisScale", params.fStretch, -1.0f);

					// Facing=Velocity requires a rotation
					params.vInitAngles.y = params.vInitAngles.y - 90.f;
					ResetValue(params.bOrientToVelocity);
				}
				else
					AddValue(render, "FacingMode", "Camera");
				AddValue(render, "SphericalProjection", ResetValue(params.fSphericalApproximation, 1.f));
				ResetValue(params.eFacing);
			}
			else if (params.eFacing == ParticleParams::EFacing::Free)
			{
				ConvertValueString(render, "FacingMode", params.eFacing);
				if (ResetValue(params.bOrientToVelocity))
				{
					AddValue(ang, "ParticleAxis", "Forward");
					AddValue(ang, "Type", "Velocity");
				}
			}
			else if (params.eFacing == ParticleParams::EFacing::Velocity)
			{
				AddValue(render, "FacingMode", "Free");
				AddValue(ang, "ParticleAxis", "Normal");
				AddValue(ang, "Type", "Velocity");
				ResetValue(params.eFacing);
				ResetValue(params.bOrientToVelocity);
			}

			ConvertValue(render, "CameraOffset", params.fCameraDistanceOffset);
			ConvertParamBase(render, "AspectRatio", params.fAspect, 1.f);
			Vec2 pivot(params.fPivotX, -params.fPivotY);
			ConvertValue(render, "Offset", pivot, Vec2(ZERO));
 			params.fPivotX.Set(0); params.fPivotY.Set(0);
		}

		if (XmlNodeRef project =
			(params.eFacing == ParticleParams::EFacing::Terrain ? MakeFeature("ProjectTerrain")
			: params.eFacing == ParticleParams::EFacing::Water ? MakeFeature("ProjectWater")
			: nullptr))
		{
			AddValue(project, "SpawnOnly", false);
			AddValue(project, "ProjectAngles", true);
			AddFeature(component, project);
			ResetValue(params.eFacing);
		}

		AddValue(render, "SortMode", "NewToOld");
		if (params.eFacing != ParticleParams::EFacing::Decal)
			if (float bias = ResetValue(params.fSortOffset))
				AddValue(render, "SortBias", -bias);  // Approximate conversion: pfx1 dist += SortOffset; pfx2 dist -= dist * SortBias / 1024

		AddFeature(component, render, true);
		AddFeature(component, ang);

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
		params.fSize.Set(1.f);

		AddValue(mesh, "SizeMode", "Scale");
		AddValue(mesh, "OriginMode", params.bNoOffset ? "Origin" : "Center");
		AddValue(mesh, "PiecePlacement", ResetValue(params.bNoOffset) ? "SubPlacement" : "Standard");

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

		AddFeature(component, mesh);
	}
}

void ConvertFields(IParticleComponent& component, ParticleParams& params, bool bInherit = false)
{
	// Opacity
	XmlNodeRef opacity = MakeFeature("FieldOpacity");
	ConvertParam(opacity, "value", params.fAlpha, 1.f, 1.f, bInherit);
	ConvertValue(opacity, "AlphaScale", reinterpret_cast<Vec2&>(params.AlphaClip.fScale), Vec2(0, 1));
	ConvertValue(opacity, "ClipLow", reinterpret_cast<Vec2&>(params.AlphaClip.fSourceMin), Vec2(0, 0));
	ConvertValue(opacity, "ClipRange", reinterpret_cast<Vec2&>(params.AlphaClip.fSourceWidth), Vec2(1, 1));
	AddFeature(component, opacity);

	// Color
	XmlNodeRef color = MakeFeature("FieldColor");
	ConvertParam(color, "Color", params.cColor, 1.f, 1.f, bInherit);
	AddFeature(component, color);

	// Size
	XmlNodeRef size = MakeFeature("FieldSize");
	ConvertParam(size, "value", params.fSize, 1.f, 0.f, bInherit);
	AddFeature(component, size);
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

void ConvertMotion(IParticleComponent& component, ParticleParams& params)
{
	bool hasMotion = false;
	if (params.fSpeed)
	{
		hasMotion = true;
		if (!params.fEmitAngle)
		{
			XmlNodeRef vel = MakeFeature("VelocityDirectional");
			ConvertParam(vel, "Scale", params.fSpeed);
			AddValue(vel, "Velocity", Vec3(0, 1, 0));
			AddFeature(component, vel);
		}
		else if (params.fEmitAngle(VMAX) == 180.f && params.fEmitAngle(VMIN) == 0.f)
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
		{
			XmlNodeRef vel = MakeFeature("VelocityCompass");
			ConvertParam(vel, "Azimuth", params.fFocusAzimuth);
			ConvertParam(vel, "Angle", params.fFocusAngle);
			ConvertParam(vel, "Velocity", params.fSpeed);
			AddFeature(component, vel);
		}
	}
	ResetValue(params.fSpeed);
	ResetValue(params.fEmitAngle);
	if (params.fInheritVelocity)
	{
		hasMotion = true;
		XmlNodeRef vel = MakeFeature("VelocityInherit");
		TVarParam<SFloat> scale(ResetValue(params.fInheritVelocity));
		ConvertParam(vel, "Scale", scale);
		AddFeature(component, vel);
	}
	if (params.bMoveRelativeEmitter)
	{
		hasMotion = true;
		XmlNodeRef rel = MakeFeature("MotionMoveRelativeToEmitter");
		AddValue(rel, "PositionInherit", 1.f);
		AddValue(rel, "AngularInherit", 1.f * !params.bMoveRelativeEmitter.bIgnoreRotation);
		ResetValue(params.bMoveRelativeEmitter);
		AddFeature(component, rel);
	}
	if (ResetValue(params.bBindEmitterToCamera))
	{
		XmlNodeRef cam = MakeFeature("LocationBindToCamera");
		AddValue(cam, "SpawnOnly", true);
		AddFeature(component, cam);
	}

	switch (params.ePhysicsType)
	{
	case ParticleParams::EPhysics::SimplePhysics:
	case ParticleParams::EPhysics::RigidBody:
	{
		// CryPhysics particle physics
		XmlNodeRef phys = MakeFeature("MotionCryPhysics");
		AddValue(phys, "PhysicsType", ResetValue(params.ePhysicsType) == ParticleParams::EPhysics::RigidBody ? "Mesh" : "Particle");
		ConvertParamBase(phys, "gravity", params.fGravityScale);
		ConvertParamBase(phys, "drag", params.fAirResistance);
		ConvertValue(phys, "UniformAcceleration", params.vAcceleration);
		AddValue(phys, "density", ResetValue(params.fDensity, 1000.0f) * 0.001f);
		ConvertValue(phys, "thickness", params.fThickness, 1.0f, 0.0f);
		AddValue(phys, "SurfaceType", TypeInfo(&params.sSurfaceType).ToString(&params.sSurfaceType));
		ResetValue(params.sSurfaceType);

		AddFeature(component, phys, hasMotion);
		break;
	}
	default:
	{
		// General motion physics
		XmlNodeRef phys = MakeFeature("MotionPhysics");
		ConvertParam(phys, "gravity", params.fGravityScale);
		ConvertParam(phys, "drag", static_cast<TVarEPParam<::UFloat>&>(params.fAirResistance));
		ConvertValue(phys, "UniformAcceleration", params.vAcceleration);
		ConvertValue(phys, "WindMultiplier", params.fAirResistance.fWindScale, 1.f);
		ConvertValue(phys, "AngularDragMultiplier", params.fAirResistance.fRotationalDragScale, 1.f);

		bool spiraling = params.fTurbulenceSize && params.fTurbulenceSpeed;
		if (spiraling || params.fTurbulence3DSpeed)
		{
			XmlNodeRef effectors = phys->newChild("localEffectors");
			if (params.fTurbulence3DSpeed)
			{
				XmlNodeRef data = AddPtrElement(effectors, "Turbulence");
				AddValue(data, "mode", "Brownian");
				ConvertParamBase(data, "speed", params.fTurbulence3DSpeed);
			}
			if (spiraling)
			{
				XmlNodeRef data = AddPtrElement(effectors, "Spiral");
				ConvertParamBase(data, "Speed", params.fTurbulenceSpeed);
				ConvertParamBase(data, "Size", params.fTurbulenceSize);
			}
		}

		AddFeature(component, phys, hasMotion);
	}
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

void ConvertCollision(IParticleComponent& component, ParticleParams& params)
{
	if (params.ePhysicsType == ParticleParams::EPhysics::SimpleCollision)
	{
		XmlNodeRef coll = MakeFeature("MotionCollisions");

		ResetValue(params.ePhysicsType);
		ConvertValue(coll, "Terrain", params.bCollideTerrain, false, true);
		ConvertValue(coll, "StaticObjects", params.bCollideStaticObjects, false, true);
		ConvertValue(coll, "DynamicObjects", params.bCollideDynamicObjects, false, true);
		ConvertValue(coll, "Elasticity", params.fElasticity);
		ConvertValue(coll, "Friction", params.fDynamicFriction);
		AddValue(coll, "RotateToNormal", true);
		if (params.nMaxCollisionEvents)
		{
			ConvertValue(coll, "MaxCollisions", params.nMaxCollisionEvents);
			switch (ResetValue(params.eFinalCollision))
			{
			case ParticleParams::ECollisionResponse::Ignore: AddValue(coll, "CollisionsLimitMode", "Ignore"); break;
			case ParticleParams::ECollisionResponse::Stop:   AddValue(coll, "CollisionsLimitMode", "Stop"); break;
			case ParticleParams::ECollisionResponse::Die:    AddValue(coll, "CollisionsLimitMode", "Kill"); break;
			}
		}
		AddFeature(component, coll);
	}
	else if (!params.ePhysicsType)
	{
		ResetValue(params.bCollideTerrain);
		ResetValue(params.bCollideStaticObjects);
		ResetValue(params.bCollideDynamicObjects);
		ResetValue(params.fElasticity);
		ResetValue(params.fDynamicFriction);
		ResetValue(params.nMaxCollisionEvents);
		ResetValue(params.eFinalCollision);
	}
}

void ConvertVisibility(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef vis = MakeFeature("AppearanceVisibility");
	ConvertValue(vis, "ViewDistanceMultiple", params.fViewDistanceAdjust, 1.f);
	ConvertValue(vis, "MinCameraDistance", params.fCameraMinDistance);
	ConvertValue(vis, "MaxCameraDistance", ZeroIsInfinity(params.fCameraMaxDistance), 0.0f, gInfinity);
	switch (ResetValue(params.tVisibleIndoors, ETrinary::Both))
	{
		case ETrinary::If_True:  AddValue(vis, "IndoorVisibility", "IndoorOnly"); break;
		case ETrinary::If_False: AddValue(vis, "IndoorVisibility", "OutdoorOnly"); break;
	}
	switch (ResetValue(params.tVisibleUnderwater, ETrinary::Both))
	{
		case ETrinary::If_True:  AddValue(vis, "WaterVisibility", "BelowWaterOnly"); break;
		case ETrinary::If_False: AddValue(vis, "WaterVisibility", "AboveWaterOnly"); break;
	}
	ConvertValue(vis, "DrawNear", params.bDrawNear);
	ConvertValue(vis, "DrawOnTop", params.bDrawOnTop);
	AddFeature(component, vis);
}

void ConvertLightSource(IParticleComponent& component, ParticleParams& params)
{
	auto& ls = params.LightSource;
	if (ls.fIntensity && ls.fRadius)
	{
		XmlNodeRef light = MakeFeature("LightLight");
		ConvertParamBase(light, "Intensity", ls.fIntensity);
		ConvertParamBase(light, "Radius", ls.fRadius);
		ConvertValueString(light, "Affectsfog", ls.eLightAffectsFog);
		ConvertValue(light, "AffectsThisAreaOnly", ls.bAffectsThisAreaOnly);
		AddFeature(component, light);
	}
}

void ConvertAudioSource(IParticleComponent& component, ParticleParams& params, const ParticleParams& paramsOrig, IParticleEffectPfx2& newEffect, IParticleComponent* parent)
{
	if (!params.sStartTrigger.empty() || !params.sStopTrigger.empty())
	{
		// pfx1 audio is instantiated per emitter, pfx2 is per particle.
		// Thus, we add a pfx2 audio feature to the parent component.
		if (!parent)
		{
			// Must create a new component with a separate single particle.
			string name = string(component.GetName()) + "_Audio";
			parent = AddComponent(newEffect, name);

			ParticleParams paramsAudio = paramsOrig;
			paramsAudio.fCount = 1.0f;
			params.bContinuous = false;

			// Set particle lifetime
			TVarParam<::UFloat> lifetime =
				params.eSoundControlTime == ParticleParams::ESoundControlTime::EmitterPulsePeriod ? params.fPulsePeriod : params.fEmitterLifeTime;
			if (params.eSoundControlTime == ParticleParams::ESoundControlTime::EmitterExtendedLifeTime)
			{
				float lifetimeMin = lifetime(VMIN) + params.fParticleLifeTime(VMIN);
				float lifetimeMax = lifetime(VMAX) + params.fParticleLifeTime(VMAX);
				lifetime.Set(lifetimeMax, (lifetimeMax - lifetimeMin) / lifetimeMax);
			}
			paramsAudio.fParticleLifeTime.Set(max(lifetime(VMAX), 3.0f), lifetime.GetRandomRange());

			ConvertSpawn(*parent, paramsAudio, true);

			XmlNodeRef rel = MakeFeature("MotionMoveRelativeToEmitter");
			AddValue(rel, "PositionInherit", 1.f);
			AddValue(rel, "AngularInherit", 1.f);
			AddFeature(*parent, rel);
		}

		XmlNodeRef audio = MakeFeature("AudioTrigger");
		if (!params.sStartTrigger.empty())
		{
			ConvertValueString(audio, "Name", params.sStartTrigger);
			AddValue(audio, "Trigger", "OnSpawn");
		}
		else
		{
			ConvertValueString(audio, "Name", params.sStopTrigger);
			AddValue(audio, "Trigger", "OnDeath");
		}
		AddFeature(*parent, audio);

		if (params.fSoundFXParam(VMIN) < 1)
		{
			// Incomplete conversion: In pfx1, the Rtpc name can be specified in entity SpawnParams,
			// otherwise defaults to "particlefx".
			XmlNodeRef rtpc = MakeFeature("AudioRtpc");
			TVarEtoPParam<::UFloat> partParam(params.fSoundFXParam);
			ConvertParam(rtpc, "Value", partParam);
			AddValue(rtpc, "Name", "particlefx");
			AddFeature(*parent, rtpc);
		}
	}
}

void ConvertConfigSpec(IParticleComponent& component, ParticleParams& params)
{
	XmlNodeRef spec = MakeFeature("ComponentEnableByConfig");
	ConvertValueString(spec, "Minimum", params.eConfigMin, ParticleParams::EConfigSpecBrief::Low);
	ConvertValueString(spec, "Maximum", params.eConfigMax, ParticleParams::EConfigSpecBrief::VeryHigh);
	ConvertValue(spec, "PC", params.Platforms.PCDX11);
	ConvertValue(spec, "XBoxOne", params.Platforms.XBoxOne);
	ConvertValue(spec, "PS4", params.Platforms.PS4);
	AddFeature(component, spec);
}
 
IParticleComponent* ConvertTail(IParticleComponent& component, ParticleParams& params, IParticleEffectPfx2& newEffect)
{
	if (!params.GetTailSteps())
		return nullptr;

	// Implement tail with ribbon child effect
	string childName = string(component.GetName()) + "_tail";
	IParticleComponent* child = AddComponent(newEffect, childName);
	child->SetParent(&component);

	AddFeature(*child, MakeFeature("ChildOnBirth"), true);

	// Spawn TailSteps particles continuously
	XmlNodeRef spawn = MakeFeature("SpawnCount");
	ConvertValueParam(spawn, "Amount", params.fTailLength.nTailSteps);
	AddValue(spawn->newChild("Duration"), "value", gInfinity);

	AddFeature(*child, spawn);

	XmlNodeRef life = MakeFeature("LifeTime");
	ConvertParam(life, "LifeTime", static_cast<TVarEPParam<::UFloat>&>(params.fTailLength));
	AddFeature(*child, life);

	ResetValue(params.fTailLength);

	// Render as connected ribbon
	XmlNodeRef ribbon = MakeFeature("RenderRibbon");
	if (params.eFacing == ParticleParams::EFacing::Camera)
	{
		ResetValue(params.eFacing);
		AddValue(ribbon, "RibbonMode", "Camera");
	}
	else if (params.eFacing == ParticleParams::EFacing::Free)
	{
		ResetValue(params.eFacing);
		AddValue(ribbon, "RibbonMode", "Free");
	}

	AddValue(ribbon, "ConnectToOrigin", true);
	AddValue(ribbon, "StreamSource", "Age");
	AddValue(ribbon, "TextureFrequency", 1.f);

	if (float bias = ResetValue(params.fSortOffset))
		AddValue(ribbon, "SortBias", -bias);  // Approximate conversion: pfx1 dist += SortOffset; pfx2 dist -= dist * SortBias / 1024

	AddFeature(*child, ribbon);

	ConvertAppearance(*child, params);

	ConvertFields(*child, params, true);

	return child;
}

// Convert all features
void ConvertParamsToFeatures(IParticleComponent& component, const ParticleParams& paramsOrig, IParticleEffectPfx2& newEffect, IParticleComponent* parent = nullptr)
{
	ParticleParams params = paramsOrig;

	if (!params.sComment.empty())
	{
		XmlNodeRef comment = MakeFeature("GeneralComment");
		AddValue(comment, "Text", params.sComment);
		AddFeature(component, comment);
	}

	// Convert params to features, resetting params as we do.
	// Any non-default params afterwards have not been converted.
	ConvertConfigSpec(component, params);
	ConvertChild(component, params);
	ConvertSpawn(component, params);
	if (!ConvertTail(component, params, newEffect))
	{
		ConvertSprites(component, params);
		ConvertGeometry(component, params);
		ClearAppearance(params);
	}
	ConvertFields(component, params);
	ConvertLocation(component, params);
	ConvertMotion(component, params);
	ConvertCollision(component, params);
	if (paramsOrig.eFacing == ParticleParams::EFacing::Free || !paramsOrig.sGeometry.empty())
		ConvertAngles3D(component, params);
	else
		ConvertAngles2D(component, params);
	ConvertLightSource(component, params);
	ConvertAudioSource(component, params, paramsOrig, newEffect, parent);
	ConvertVisibility(component, params);

	// Report unconverted params.
	static ParticleParams defaultParams;
	string unconverted = TypeInfo(&params).ToString(&params, FToString().NamedFields(1).SkipDefault(1), &defaultParams);
	if (!unconverted.empty())
	{
		unconverted.replace(",", ", ");
		LogError("Unconverted pfx1 parameters: " + unconverted, &component);
	}
}

void ConvertSubEffects(IParticleEffectPfx2& newEffect, const ::IParticleEffect& oldSubEffect, IParticleComponent* parent = nullptr)
{
	IParticleComponent* component = nullptr;
	if (oldSubEffect.IsEnabled(IParticleEffect::eCheckFeatures | IParticleEffect::eCheckChildren))
	{
		if (oldSubEffect.GetParticleParams().eSpawnIndirection && !parent)
			return;
		if (!oldSubEffect.GetParticleParams().eSpawnIndirection)
			parent = nullptr;
		cstr name = oldSubEffect.GetName();
		if (cstr base = strrchr(name, '.'))
		{
			name = base + 1;
		}
		component = AddComponent(newEffect, name);
		component->SetParent(parent);

		ConvertParamsToFeatures(*component, oldSubEffect.GetParticleParams(), newEffect, parent);
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
	PParticleEffect pNewEffect = FindEffect(newName, !bReplace);
	if (pNewEffect && !bReplace)
		return pNewEffect;

	pNewEffect = CreateEffect();
	RenameEffect(pNewEffect, newName);
	CryLog("PFX1 to PFX2 \"%s\":", oldName.c_str());
	non_const(pOldEffect)->LoadResources();
	ConvertSubEffects(*pNewEffect, *pOldEffect);

	CryLog(" - Saving as \"%s\":", newName.c_str());
	gEnv->pCryPak->MakeDir(PathUtil::GetParentDirectory(newName).c_str());
	Serialization::SaveJsonFile(newName, *pNewEffect);

	return pNewEffect;
}

}

#if 0
///////////////////////////////////////////////////////////////////////////////
// A subset of ParticleParams, listing parameters which are not yet converted
struct ParticleParamsUnconverted
{
	// HIGH PRIORITY

	enum EFacing {                                  // Not all facing combinations supported. For geometry, ONLY Free. 
		CameraX,                                    // NO for sprites, NO for ribbons, NO for geometry
		Horizontal,                                 // NO for sprites, NO for ribbons
		Water,                                      // NO for ribbons
		Terrain                                     // NO for ribbons
	};

	TSmallBool             bTessellation;           //!< If hardware supports, tessellate particles for better shadowing and curved connected particles.

	struct STargetAttraction
	{
		enum ETargeting {
			External,
			OwnEmitter,
			Ignore
		}                   eTarget;                //!< Source of target attractor.
		TSmallBool          bExtendSpeed;           //!< Extend particle speed as necessary to reach target in normal lifetime.
		TSmallBool          bShrink;                //!< Shrink particle as it approaches target.
		TSmallBool          bOrbit;                 //!< Orbit target at specified distance, rather than disappearing.
		TVarEPParam<SFloat> fRadius;                //!< Radius of attractor, for vanishing or orbiting.
	}                      TargetAttraction;        //!< Specify target attractor behavior.

	enum EForce {
		None,
		Wind,
		Gravity
	}                      eForceGeneration;        //!< Generate physical forces if set.

	// MEDIUM PRIORITY

	TSmallBool             bEnabled = true;         // Only enabled sub-effects are converted

	struct SMaintainDensity : UFloat
	{
		UFloat fReduceLifeTime;
		UFloat fReduceAlpha;                        //!< <SoftMax=1> Reduce alpha inversely to count increase.
		UFloat fReduceSize;
	} fMaintainDensity;                             //!< <SoftMax=1> Increase count when emitter moves to maintain spatial density.
	TSmallBool             bRemainWhileVisible;     //!< Particles will only die when not rendered (by any viewport).

	UnitFloat              fOffsetRoundness;        // ONLY 0 (cube) or 1 (sphere)
	UnitFloat              fOffsetInnerFraction;    //!< Fraction of inner emit volume to avoid

	TVarEParam<UHalfAngle> fFocusAngle;             // APPROX
	TVarEParam<SFloat>     fFocusAzimuth;           // APPROX
	TVarEParam<UnitFloat>  fFocusCameraDir;         //!< Rotate emitter focus partially or fully to face camera.
	TSmallBool             bFocusGravityDir;        //!< Uses negative gravity dir, rather than emitter Y, as focus dir.
	TSmallBool             bEmitOffsetDir;          //!< Default emission direction parallel to emission offset from origin.

	TVarEPParam<UFloat>    fAspect = 1;             // BASE only
	TVarEPParam<SFloat>    fPivotX;                 // BASE only
	TVarEPParam<SFloat>    fPivotY;                 // BASE only

	struct SStretch : TVarEPParam<UFloat>
	{
		SFloat fOffsetRatio;
	}                         fStretch;             // BASE only

	UnitFloat8                fCollisionFraction;   //!< Fraction of emitted particles that actually perform collisions.

	TSmallBool                bVolumeFog;           //!< Use as a participating media of volumetric fog.
	Unit4Float                fVolumeThickness = 1; //!< Thickness factor for particle size.

	TRangedType<uint8, 0, 2>  nSortQuality;         //!< Sort new particles as accurately as possible into list, by main camera distance.
	SFloat                    fSortBoundsScale;     //!< <SoftMin=-1> <SoftMax=1> Choose emitter point for sorting; 1 = bounds nearest, 0 = origin, -1 = bounds farthest.

	UFloat                    fFillRateCost = 1;    //!< Adjustment to max screen fill allowed per emitter.


	// LOW PRIORITY

	enum EInheritance {
		System,
		Standard,
		Parent
	}                      eInheritance;            //!< Source of ParticleParams used as base for this effect (for serialization, display, etc).

	TSmallBool             bFocusRotatesEmitter;    //!< Focus rotation affects offset and particle orientation; else affects just emission direction.

	UFloat                 fPlaneAlignBlendDistance;//!< Distance when blend to camera plane aligned particles starts.

	struct STextureTiling
	{
		UnitFloat8     fFlipChance;                 //!< Chance each particle will flip in X direction.
		TCurve<UFloat> fAnimCurve;                  //!< Animation curve.
	};
	TSmallBool             bOctagonalShape;         //!< Use octagonal shape for textures instead of quad.

	TVarEParam<UFloat>     fSoundFXParam = 1;       // NO ability to take RTPC param name from SpawnParams

	struct SConnection
	{
		TSmallBool bTextureMirror;                  //!< Mirror alternating texture tiles; else wrap.
	};

	struct SMoveRelativeEmitter
	{
		TSmallBool  bIgnoreSize;                    // ALWAYS
		TSmallBool  bMoveTail;                      // ALWAYS
	};

	UFloat                 fCollisionCutoffDistance;//!< Maximum distance up until collisions are respected (0 = infinite).
	UFloat                 fThickness = 1;          // YES: Particle Physics, Decal thickness; NO Geometry Z scale, 

	SFloat                 fSortOffset;             // YES Decal offset, APPROX general offset; NO Geometry dist
	UnitFloat              fFadeAtViewCosAngle;     //!< Angle to camera at which particles start to fade out.

	TFixed<uint8, MAX_HEATSCALE> fHeatScale;        //!< Multiplier to thermal vision.

	TSmallBool             bForceDynamicBounds;     //!< Always update particles and compute actual bounds for visibility.
	TSmallBool             bHalfRes;                //!< Use half resolution rendering.
	TSmallBoolTrue         bStreamable;             //!< Texture/geometry allowed to be streamed.
};
#endif
