// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <CryCore/CryEnumMacro.h>

namespace ACE
{
enum class ESystemItemType
{
	Invalid = -1,

	// controls
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

using ItemType = unsigned int;
static const ItemType AUDIO_SYSTEM_INVALID_TYPE = 0;

using CID = unsigned int; // TOdo: do we need this?
static const CID ACE_INVALID_ID = 0;

class CImplConnection;
using ConnectionPtr = std::shared_ptr<CImplConnection>;

// available levels where the controls can be stored
struct SScopeInfo
{
	SScopeInfo() = default;
	SScopeInfo(string const& name_, bool const isLocalOnly_) : name(name_), isLocalOnly(isLocalOnly_) {}
	string name;

	// if true, there is a level in the game audio
	// data that doesn't exist in the global list
	// of levels for your project
	bool isLocalOnly;
};

typedef uint32                  Scope;
using ScopeInfoList = std::vector<SScopeInfo>;

enum class EErrorCode
{
	NoError,
	UnkownPlatform           = BIT(0),
	NonMatchedActivityRadius = BIT(1),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EErrorCode);
} // namespace ACE
