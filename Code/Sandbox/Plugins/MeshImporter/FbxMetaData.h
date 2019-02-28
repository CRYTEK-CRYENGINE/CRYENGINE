// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Metadata serialize/deserialize helper for user settings
#pragma once
#if !defined(CRY_PLATFORM) || (!defined(NOT_USE_CRY_STRING) && !defined(CRY_STRING))
	#error The global namespace string type must be included via platform.h before using FbxMetadata.h
#endif
#include "NodeProperties.h"
#include <vector>

class CGlobalImportSettings;
class CAutoLodSettings;

namespace FbxMetaData
{
// This must have been set by CryString.h (via platform.h).
// We can't set any other type than this, as YASLI depends on using that exact type.
// TODO: Investigate if we can switch this to std::string and supply our own YASLI string wrappers (and drop the CE dependency entirely)
typedef ::string TString;

enum class EExportSetting
{
	Include,
	Exclude,
};

enum class EPhysicalizeSetting
{
	None,
	Default,
	Obstruct,
	NoCollide,
	ProxyOnly,
};

//! \sa FbxTool::EGenerateNormals
enum class EGenerateNormals
{
	Never,
	Smooth,
	Hard,
	OverwriteSmooth,
	OverwriteHard,
};

const char* ToString(EExportSetting value);
const char* ToString(EPhysicalizeSetting value);
const char* ToString(EGenerateNormals value);

namespace Units
{

enum EUnitSetting
{
	eUnitSetting_FromFile = 0,   // use the unit specified in the source .fbx
	eUnitSetting_Millimeter,
	eUnitSetting_Centimeter,
	eUnitSetting_Inch,
	eUnitSetting_Decimeter,
	eUnitSetting_Foot,
	eUnitSetting_Yard,
	eUnitSetting_Meter,
	eUnitSetting_Kilometer,
	eUnitSetting_COUNT
};

const char* GetName(EUnitSetting unit);
const char* GetSymbol(EUnitSetting unit);

template<typename FUNC>
static void ForallNames(FUNC f)
{
	for (int i = 0; i < eUnitSetting_COUNT; ++i)
	{
		f(GetName(static_cast<EUnitSetting>(i)));
	}
}

// Returns 'fromFile', if 'unit' is eUnitSetting_FromFile. Otherwise,
// returns scale X, with 1 unit = X cm.
double GetUnitSizeInCm(EUnitSetting unit, double fromFile);

}   // namespace Units

struct SSceneUserSettings
{
	bool                      bTriangulate;
	bool                      bRemoveDegeneratePolygons;
	EGenerateNormals generateNormals;
};

struct SJointLimits
{
	float values[6];
	char mask;

	SJointLimits()
		: mask(0)
	{}
};

struct SJointPhysicsData
{
	std::vector<TString> jointNodePath;

	std::vector<TString> proxyNodePath;
	bool bSnapToJoint;

	SJointLimits jointLimits;

	SJointPhysicsData()
		: bSnapToJoint(false)
	{
	}
};

struct SNodeMeta
{
	TString                name;
	std::vector<TString>   nodePath;
	std::vector<SNodeMeta> children;
	CNodeProperties        properties; // Known UDPs.
	std::vector<string>    udps;       // Unknown (custom) UDPs.
};

struct SMaterialUserSettings
{
	TString materialFile;    // Material file name
	TString subMaterialName; // Sub-material name (or empty)

	// Sub-material index (cached from last time the material was loaded in the UI).
	// Non-negative values are valid sub-material indices.
	// A value of -1 tags this material for deletion.
	int                 subMaterialIndex;

	bool                bAutoAssigned;      // If set, the subMaterial was automatically assigned (provided for UI purposes only)
	EPhysicalizeSetting physicalizeSetting; // Physicalization-attribute to apply on the material

	SMaterialUserSettings()
		: subMaterialIndex(-1)
		, bAutoAssigned(true)
	{}
};

struct SMaterialMetaData
{
	SMaterialUserSettings settings;
	TString               name;
};

struct SAnimationClip
{
	string takeName;
	std::vector<string> motionNodePath;
	int startFrame;
	int endFrame;

	SAnimationClip()
		: startFrame(-1)
		, endFrame(-1)
	{}
};

//! Editor meta-data is the part of the meta-data which is only used to restore state of the importer dialog
//! and is ignored by the RC.
struct IEditorMetaData
{
	virtual ~IEditorMetaData() {}

	virtual std::unique_ptr<IEditorMetaData> Clone() const = 0;

	virtual void Serialize(yasli::Archive& ar) = 0;
};

// Describes all the user-configurable settings and data for processing FBX data.
struct SMetaData
{
	typedef std::vector<SMaterialMetaData> TMaterialVector;

	SSceneUserSettings  settings;
	TString             sourceFilename;
	TString             outputFileExt;
	TString             password;
	TString             materialFilename;

	Units::EUnitSetting unit;
	float               scale;

	bool bMergeAllNodes;
	bool bSceneOrigin;
	bool bComputeNormals;
	bool bComputeUv;
	bool bVertexPositionFormatF32;

	// Format is "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>".
	// Example: "-Y+Z".
	TString                forwardUpAxes;

	TMaterialVector        materialData;
	std::vector<SNodeMeta> nodeData;

	SAnimationClip animationClip;

	std::vector<SJointPhysicsData> jointPhysicsData;

	// Editor state.
	std::unique_ptr<IEditorMetaData> pEditorMetaData;  //!< Used to restore state of editor. Ignored by RC.

	std::shared_ptr<CAutoLodSettings> pAutoLodSettings;

	SMetaData();
	SMetaData(const SMetaData& other);

	SMetaData& operator=(const SMetaData& other);

	void Clear()
	{
		settings = FbxMetaData::SSceneUserSettings();
		animationClip = FbxMetaData::SAnimationClip();
		nodeData.clear();
		materialData.clear();
	}

	TString ToJson() const;
	bool    FromJson(const TString& json);
};
}

