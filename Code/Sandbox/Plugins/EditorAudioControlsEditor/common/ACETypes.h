// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <memory>

namespace ACE
{

enum EItemType
{
	eItemType_Invalid = -1,

	// controls
	eItemType_Trigger,
	eItemType_RTPC,
	eItemType_Switch,
	eItemType_State,
	eItemType_Environment,
	eItemType_Preload,

	eItemType_Folder,
	eItemType_Library,
	eItemType_NumTypes
};

typedef unsigned int ItemType;
static const ItemType AUDIO_SYSTEM_INVALID_TYPE = 0;

typedef unsigned int CID; // TOdo: do we need this?
static const CID ACE_INVALID_ID = 0;

class IAudioConnection;
typedef std::shared_ptr<IAudioConnection> ConnectionPtr;

// available levels where the controls can be stored
struct SScopeInfo
{
	SScopeInfo() {}
	SScopeInfo(const string& _name, bool _bOnlyLocal) : name(_name), bOnlyLocal(_bOnlyLocal) {}
	string name;

	// if true, there is a level in the game audio
	// data that doesn't exist in the global list
	// of levels for your project
	bool bOnlyLocal;
};

typedef uint32                  Scope;
typedef std::vector<SScopeInfo> ScopeInfoList;

enum EErrorCode
{
	eErrorCode_NoError                  = 0,
	eErrorCode_UnkownPlatform           = BIT(0),
	eErrorCode_NonMatchedActivityRadius = BIT(1),
};

}
