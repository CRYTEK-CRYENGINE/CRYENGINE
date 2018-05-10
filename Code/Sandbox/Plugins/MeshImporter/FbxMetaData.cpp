// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Helper for tracking identity of nodes in a scene, attempts to follow changes in the source asset.

#include <StdAfx.h>

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Enum.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

#include "FbxMetaData.h"
#include "AutoLodSettings.h"

void LogPrintf(const char* szFormat, ...);

YASLI_ENUM_BEGIN_NESTED(FbxMetaData, EExportSetting, "ExportSetting")
YASLI_ENUM(FbxMetaData::EExportSetting::Include, "include", "Included")
YASLI_ENUM(FbxMetaData::EExportSetting::Exclude, "exclude", "Excluded")
YASLI_ENUM_END()

YASLI_ENUM_BEGIN_NESTED(FbxMetaData, EPhysicalizeSetting, "PhysicalizeSetting")
YASLI_ENUM(FbxMetaData::EPhysicalizeSetting::None, "no", "none")                                  // No physicalization
YASLI_ENUM(FbxMetaData::EPhysicalizeSetting::Default, "default", "default")                       // Default physicalization
YASLI_ENUM(FbxMetaData::EPhysicalizeSetting::Obstruct, "obstruct", "obstruct")                    // Obstructing physicalization
YASLI_ENUM(FbxMetaData::EPhysicalizeSetting::NoCollide, "no_collide", "no collide")               // Non-colliding physicalization
YASLI_ENUM(FbxMetaData::EPhysicalizeSetting::ProxyOnly, "proxy_only", "physical proxy (no draw)") // Proxy-only physicalization
YASLI_ENUM_END()

#define YASLI_ENUM_UNIT_SETTING(value)                  \
  description.add(int(value),                           \
                  FbxMetaData::Units::GetSymbol(value), \
                  FbxMetaData::Units::GetName(value));

YASLI_ENUM_BEGIN_NESTED2(FbxMetaData, Units, EUnitSetting, "UnitSetting")
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_FromFile);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Millimeter);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Centimeter);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Inch);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Decimeter);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Foot);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Yard);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Meter);
YASLI_ENUM_UNIT_SETTING(FbxMetaData::Units::eUnitSetting_Kilometer);
YASLI_ENUM_END()

YASLI_ENUM_BEGIN_NESTED(FbxMetaData, EGenerateNormals, "GenerateNormals")
YASLI_ENUM(FbxMetaData::EGenerateNormals::Never, "no", "Never generate")
YASLI_ENUM(FbxMetaData::EGenerateNormals::Smooth, "smooth", "Generate smooth if none present")
YASLI_ENUM(FbxMetaData::EGenerateNormals::Hard, "hard", "Generate hard if none present")
YASLI_ENUM(FbxMetaData::EGenerateNormals::OverwriteSmooth, "force_smooth", "Generate smooth even if present")
YASLI_ENUM(FbxMetaData::EGenerateNormals::OverwriteHard, "force_hard", "Generate hard even if present")
YASLI_ENUM_END()

namespace FbxMetaData
{
static void Serialize(yasli::Archive& ar, SJointLimits& value)
{
	static const char* axisLimitNames[] =
	{
		"min_x",
		"min_y",
		"min_z",
		"max_x",
		"max_y",
		"max_z"
	};
	char newMask = 0;
	for (int i = 0, N = CRY_ARRAY_COUNT(axisLimitNames); i < N; ++i)
	{
		bool bHasLimit = value.mask & (1 << i);
		if (ar.isInput() || bHasLimit)
		{
			if (ar(value.values[i], axisLimitNames[i]))
			{
				newMask |= (1 << i);
			}
		}
	}
	value.mask = newMask;
}

static void Serialize(yasli::Archive& ar, SJointPhysicsData& value)
{
	ar(value.jointNodePath, "jointNodePath");

	if (ar.isInput() || !value.proxyNodePath.empty())
	{
		ar(value.proxyNodePath, "proxyNodePath");
		ar(value.bSnapToJoint, "snapToJoint", "Move this proxy to its joint origin");
	}
	
	ar(value.jointLimits, "jointLimits");
}

static void Serialize(yasli::Archive& ar, SNodeMeta& value)
{
	ar(value.name, "name");
	ar(value.nodePath, "path");
	value.properties.Serialize(ar);
	if (ar.isInput() || !value.udps.empty())
	{
		ar(value.udps, "udp");
	}
	if (!value.children.empty())
	{
		ar(value.children, "nodes");
	}
}

static void Serialize(yasli::Archive& ar, SMaterialMetaData& value)
{
	ar(value.name, "name", "Material Name");
	ar(value.settings.materialFile, "file", "Material File");
	ar(value.settings.physicalizeSetting, "physicalize", "Physicalization");
	ar(value.settings.subMaterialIndex, "sub_index", "Sub-material index");
	if (ar.isInput() || !value.settings.subMaterialName.empty())
	{
		ar(value.settings.subMaterialName, "ui_name", "Sub-material name");
	}
	ar(value.settings.bAutoAssigned, "ui_autoflag", "Auto-assigned flag");
}

static void Serialize(yasli::Archive& ar, SAnimationClip& value)
{
	ar(value.takeName, "name", "FBX Animation Stack Name");
	ar(value.motionNodePath, "motionNodePath", "Path to a scene node to derive character's motion from");
	ar(value.startFrame, "startFrame", "Start time of the animation, expressed in frames (30fps)");
	ar(value.endFrame, "endFrame", "End time of the animation, expressed in frames (30fps)");
}

static void Serialize(yasli::Archive& ar, SMetaData& value)
{
	int version = 1;
	ar(version, "version", "Version number");

	ar(value.sourceFilename, "source_filename", "Source filename");
	ar(value.outputFileExt, "output_ext", "Extension of the output file");
	ar(value.materialFilename, "material_filename", "Material filename");
	//ar(value.settings.sourceFile.password, "password", "Password of the source file");

	// conversion settings
	ar(value.unit, "unit_size", "Size of the unit ('file', 'cm', 'm', 'in', etc).");
	ar(value.scale, "scale", "Scale (size multiplier).");
	ar(value.forwardUpAxes, "forward_up_axes", "World axes");

	ar(value.bMergeAllNodes, "merge_all_nodes", "Merge all nodes");
	ar(value.bSceneOrigin, "scene_origin", "true - use scene's origin, false - use origins of root nodes");
	ar(value.bComputeNormals, "ignore_custom_normals", "true - use computed normal, false - use fbx normals");
	ar(value.bComputeUv, "ignore_uv", "true - use computed texture coordinates, false - use fbx texture coordinates");

	ar(value.materialData, "materials", "Materials");
	ar(value.nodeData, "nodes");
	ar(value.animationClip, "animation", "Animation to import");
	if (ar.isInput() || !value.jointPhysicsData.empty())
	{
		ar(value.jointPhysicsData, "jointPhysicsData", "Joint physics data");
	}

	if (value.pEditorMetaData)
	{
		value.pEditorMetaData->Serialize(ar);
	}

	// RC does not read this. 
	ar(value.bVertexPositionFormatF32, "use_32_bit_positions", "Use 32bit precision");

	ar(*value.pAutoLodSettings, "autolodsettings");
}

template<typename T>
struct SerializerHelper : yasli::Serializer
{
	SerializerHelper(T& value)
		: yasli::Serializer(yasli::TypeID::get<T>(), static_cast<void*>(&value), sizeof(T), &SerializerHelper::YASLI_SERIALIZE_METHOD)
	{
		// Empty on purpose
	}

	static bool YASLI_SERIALIZE_METHOD(void* pObject, yasli::Archive& ar)
	{
		FbxMetaData::Serialize(ar, *static_cast<T*>(pObject));
		return true;
	}
};
}

#define DEFINE_YASLI_SERIALIZE_OVERRIDE(type)                                                               \
  namespace yasli                                                                                           \
  {                                                                                                         \
  static bool YASLI_SERIALIZE_OVERRIDE(Archive & ar, type & value, const char* szName, const char* szLabel) \
  {                                                                                                         \
    FbxMetaData::SerializerHelper<type> serializer(value);                                                  \
    return ar(static_cast<yasli::Serializer&>(serializer), szName, szLabel);                                \
  }                                                                                                         \
  }

DEFINE_YASLI_SERIALIZE_OVERRIDE(FbxMetaData::SJointLimits)
DEFINE_YASLI_SERIALIZE_OVERRIDE(FbxMetaData::SJointPhysicsData)
DEFINE_YASLI_SERIALIZE_OVERRIDE(FbxMetaData::SNodeMeta)
DEFINE_YASLI_SERIALIZE_OVERRIDE(FbxMetaData::SMetaData)
DEFINE_YASLI_SERIALIZE_OVERRIDE(FbxMetaData::SMaterialMetaData)
DEFINE_YASLI_SERIALIZE_OVERRIDE(FbxMetaData::SAnimationClip)

#undef DEFINE_YASLI_SERIALIZE_OVERRIDE

namespace FbxMetaData
{
template<typename T>
static const char* EnumToString(T value)
{
	auto& desc = yasli::getEnumDescription<T>();
	return desc.label(static_cast<int>(value));
}

const char* ToString(EExportSetting value)
{
	return EnumToString(value);
}

const char* ToString(EPhysicalizeSetting value)
{
	return EnumToString(value);
}

const char* ToString(EGenerateNormals value)
{
	return EnumToString(value);
}

SMetaData::SMetaData()
	: unit(FbxMetaData::Units::eUnitSetting_FromFile)
	, scale(1.0f)
	, bMergeAllNodes(false)
	, bSceneOrigin(false)
	, pAutoLodSettings(new CAutoLodSettings())
	, bVertexPositionFormatF32(false)
	, bComputeNormals(false)
	, bComputeUv(false)
{}

SMetaData::SMetaData(const SMetaData& other)
	: settings(other.settings)
	, sourceFilename(other.sourceFilename)
	, outputFileExt(other.outputFileExt)
	, password(other.password)
	, materialFilename(other.materialFilename)
	, unit(other.unit)
	, scale(other.scale)
	, forwardUpAxes(other.forwardUpAxes)
	, bMergeAllNodes(other.bMergeAllNodes)
	, bSceneOrigin(other.bSceneOrigin)
	, bVertexPositionFormatF32(other.bVertexPositionFormatF32)
	, bComputeNormals(other.bComputeNormals)
	, bComputeUv(other.bComputeUv)
	, materialData(other.materialData)
	, nodeData(other.nodeData)
	, animationClip(other.animationClip)
	, jointPhysicsData(other.jointPhysicsData)
	, pAutoLodSettings(other.pAutoLodSettings)
{
	if (other.pEditorMetaData)
	{
		pEditorMetaData = std::move(other.pEditorMetaData->Clone());
	}
}

SMetaData& SMetaData::operator=(const SMetaData& other)
{
	if (this == &other)
	{
		return *this;
	}

	settings = other.settings;
	sourceFilename = other.sourceFilename;
	outputFileExt = other.outputFileExt;
	password = other.password;
	materialFilename = other.materialFilename;

	unit = other.unit;
	scale = other.scale;
	forwardUpAxes = other.forwardUpAxes;

	bMergeAllNodes = other.bMergeAllNodes;
	bSceneOrigin = other.bSceneOrigin;
	bVertexPositionFormatF32 = other.bVertexPositionFormatF32;
	bComputeNormals = other.bComputeNormals;
	bComputeUv = other.bComputeUv;

	materialData = other.materialData;
	nodeData = other.nodeData;

	animationClip = other.animationClip;

	jointPhysicsData = other.jointPhysicsData;

	if (other.pEditorMetaData)
	{
		pEditorMetaData = std::move(other.pEditorMetaData->Clone());
	}

	pAutoLodSettings = other.pAutoLodSettings;

	return *this;
}

TString SMetaData::ToJson() const
{
	yasli::JSONOArchive ar;
	ar(const_cast<SMetaData&>(*this), "metadata", "Meta data container");
	return ar.c_str();
}

bool SMetaData::FromJson(const TString& json)
{
	materialData.clear();

	yasli::JSONIArchive ar;
	ar.open(json.c_str(), json.size());
	return ar(*this, "metadata", "Meta data container");
}

namespace Units
{

namespace
{

struct UnitDesc
{
	const char* const szSymbol;
	const char* const szName;
	const double      sizeInCm;
};

const UnitDesc& GetUnitDesc(EUnitSetting unit)
{
	static const UnitDesc knownUnits[] =
	{
		{ "file", "file",        -1.0     },
		{ "mm",   "millimeters", 0.1      },
		{ "cm",   "centimeters", 1.0      },
		{ "in",   "inches",      2.54     },
		{ "dm",   "decimeters",  10.0     },
		{ "ft",   "feet",        30.48    },
		{ "yd",   "yards",       91.44    },
		{ "m",    "meters",      100.0    },
		{ "km",   "kilometers",  100000.0 }
	};
	static_assert(CRY_ARRAY_COUNT(knownUnits) == eUnitSetting_COUNT,
	              "Size of array does not match index range. Forgot to update units?");
	assert(0 <= unit && unit < CRY_ARRAY_COUNT(knownUnits));
	return knownUnits[unit];
}

}   // unnamed namespace

const char* GetName(EUnitSetting unit)
{
	return GetUnitDesc(unit).szName;
}

const char* GetSymbol(EUnitSetting unit)
{
	return GetUnitDesc(unit).szSymbol;
}

double GetUnitSizeInCm(EUnitSetting unit, double fromFile)
{
	if (unit == eUnitSetting_FromFile)
	{
		return fromFile;
	}
	else
	{
		return GetUnitDesc(unit).sizeInCm;
	}
}

}   // namespace Units
}

