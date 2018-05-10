// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <CryAudio/IAudioSystem.h>

namespace ACE
{
enum class EAssetType
{
	None,
	Trigger,
	Parameter,
	Switch,
	State,
	Environment,
	Preload,
	Folder,
	Library,
	NumTypes
};

enum class EItemFlags
{
	None          = 0,
	IsPlaceHolder = BIT(0),
	IsLocalized   = BIT(1),
	IsConnected   = BIT(2),
	IsContainer   = BIT(3),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EItemFlags);

enum class EErrorCode
{
	None                     = 0,
	UnkownPlatform           = BIT(0),
	NonMatchedActivityRadius = BIT(1),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EErrorCode);

using ControlId = CryAudio::IdType;
static ControlId const s_aceInvalidId = 0;
using ControlIds = std::vector<ControlId>;

struct IConnection;
using ConnectionPtr = std::shared_ptr<IConnection>;

class CAsset;
using Assets = std::vector<CAsset*>;

class CControl;
using Controls = std::vector<CControl*>;

class CLibrary;
using Libraries = std::vector<CLibrary*>;

class CFolder;
using Folders = std::vector<CFolder*>;

using Platforms = std::vector<char const*>;
using FileNames = std::set<string>;
using AssetNames = std::vector<string>;

using Scope = uint32;
static constexpr char const* const s_szGlobalScopeName = "global";
static constexpr Scope GlobalScopeId = CryAudio::StringToId(s_szGlobalScopeName);

using PlatformIndexType = uint16;

// Available levels where the controls can be stored.
struct SScopeInfo
{
	SScopeInfo(string const& name_ = "", bool const isLocalOnly_ = false)
		: name(name_)
		, isLocalOnly(isLocalOnly_)
	{}

	string name;

	// If true, there is a level in the game audio data that doesn't
	// exist in the global list of levels for your project.
	bool isLocalOnly;
};

using ScopeInfoList = std::vector<SScopeInfo>;
} // namespace ACE

