// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** GameLobbyData.h
******************************************************************************/

#ifndef __GAMELOBBYDATA_H__
#define __GAMELOBBYDATA_H__

#include "AutoEnum.h"

//------------
// LOBBY DATA
//------------

#if CRY_PLATFORM_ORBIS

// PS4 Note : There are only 8 uint32 attribute fields on PSN, everything else must be packed into 1 of 2 binary attribute fields which
//						are 256 bytes each. If filtering is to be required make sure items you want to filter by are in the 8 uint32 fields.
//						To put it another way, the first 8 eCSUDT_Int32 values registered will be allocated to the 8 searchable uint32 fields,
//						all other data will be placed into binary data and not be able to be filtered!


// Ids
enum ELOBBYIDS
{
	LID_MATCHDATA_GAMEMODE = 0,
	LID_MATCHDATA_MAP,
	LID_MATCHDATA_ACTIVE,
	LID_MATCHDATA_VERSION,
	LID_MATCHDATA_REQUIRED_DLCS,
	LID_MATCHDATA_PLAYLIST,
	LID_MATCHDATA_VARIANT,
	LID_MATCHDATA_SKILL,
	LID_MATCHDATA_LANGUAGE,
	LID_MATCHDATA_COUNTRY,
};

#define REQUIRED_SESSIONS_QUERY	0
#define FIND_GAME_SESSION_QUERY 0
#define FIND_GAME_SESSION_QUERY_WC 0
#define REQUIRED_SESSIONS_SEARCH_PARAM	0

// Rich presence string
#define RICHPRESENCE_ID					0

// Types of rich presence we can have
#define RICHPRESENCE_GAMEPLAY		0        
#define RICHPRESENCE_LOBBY			1
#define RICHPRESENCE_FRONTEND		2
#define RICHPRESENCE_SINGLEPLAYER	3
#define RICHPRESENCE_IDLE			4

// Rich presence string params
#define RICHPRESENCE_GAMEMODES	0
#define RICHPRESENCE_MAPS				1

// These string ids need to match those found in Scripts/Network/RichPresence.xml

// Rich presence strings, game mode
#define RICHPRESENCE_GAMEMODES_INSTANTACTION			0   
#define RICHPRESENCE_GAMEMODES_TEAMINSTANTACTION	1
#define RICHPRESENCE_GAMEMODES_ASSAULT						2          
#define RICHPRESENCE_GAMEMODES_CAPTURETHEFLAG			3
#define RICHPRESENCE_GAMEMODES_CRASHSITE					4
#define RICHPRESENCE_GAMEMODES_ALLORNOTHING				5
#define	RICHPRESENCE_GAMEMODES_BOMBTHEBASE				6

// Rich presence strings for maps
#define RICHPRESENCE_MAPS_ROOFTOPGARDENS			0
#define RICHPRESENCE_MAPS_PIER17              1
#define RICHPRESENCE_MAPS_WALLSTREET          2
#define RICHPRESENCE_MAPS_LIBERTYISLAND       3
#define RICHPRESENCE_MAPS_HARLEMGORGE         4
#define RICHPRESENCE_MAPS_COLLIDEDBUILDINGS   5
#define RICHPRESENCE_MAPS_ALIENCRASHTRAIL     6
#define RICHPRESENCE_MAPS_ALIENVESSEL        7
#define RICHPRESENCE_MAPS_BATTERYPARK         8
#define RICHPRESENCE_MAPS_BRYANTPARK          9
#define RICHPRESENCE_MAPS_CHURCH              10
#define RICHPRESENCE_MAPS_CONEYISLAND         11
#define RICHPRESENCE_MAPS_DOWNTOWN            12
#define RICHPRESENCE_MAPS_LIGHTHOUSE          13
#define RICHPRESENCE_MAPS_PARADE              14
#define RICHPRESENCE_MAPS_ROOSEVELT           15
#define RICHPRESENCE_MAPS_CITYHALL            16
#define RICHPRESENCE_MAPS_ALIENVESSELSMALL    17
#define RICHPRESENCE_MAPS_PIERSMALL           18
#define RICHPRESENCE_MAPS_LIBERTYISLAND_MIL   19
#define RICHPRESENCE_MAPS_TERMINAL            20
#define RICHPRESENCE_MAPS_DLC_1_MAP_1         21
#define RICHPRESENCE_MAPS_DLC_1_MAP_2         22
#define RICHPRESENCE_MAPS_DLC_1_MAP_3         23
#define RICHPRESENCE_MAPS_DLC_2_MAP_1         24
#define RICHPRESENCE_MAPS_DLC_2_MAP_2         25
#define RICHPRESENCE_MAPS_DLC_2_MAP_3         26
#define RICHPRESENCE_MAPS_DLC_3_MAP_1         27
#define RICHPRESENCE_MAPS_DLC_3_MAP_2         28
#define RICHPRESENCE_MAPS_DLC_3_MAP_3         29
#define RICHPRESENCE_MAPS_LIBERTYISLAND_STATUE 30

#else

// Ids
enum ELOBBYIDS
{
	LID_MATCHDATA_GAMEMODE = 0,
	LID_MATCHDATA_MAP,
	LID_MATCHDATA_ACTIVE,
	LID_MATCHDATA_VERSION,
	LID_MATCHDATA_REQUIRED_DLCS,
	LID_MATCHDATA_PLAYLIST,
	LID_MATCHDATA_VARIANT,
	LID_MATCHDATA_SKILL,
	LID_MATCHDATA_LANGUAGE,
};

#define REQUIRED_SESSIONS_QUERY	0
#define FIND_GAME_SESSION_QUERY 0
#define REQUIRED_SESSIONS_SEARCH_PARAM	0

// Rich presence string
#define RICHPRESENCE_ID					0

// Types of rich presence we can have
#define RICHPRESENCE_GAMEPLAY		0        
#define RICHPRESENCE_LOBBY			1
#define RICHPRESENCE_FRONTEND		2
#define RICHPRESENCE_SINGLEPLAYER	3
#define RICHPRESENCE_IDLE			4

// Rich presence string params
#define RICHPRESENCE_GAMEMODES	0
#define RICHPRESENCE_MAPS				1

#endif

#define INVALID_SESSION_FAVOURITE_ID 0	// For eLDI_FavouriteID/LID_MATCHDATA_FAVOURITE_ID

#define GAMELOBBY_USE_COUNTRY_FILTERING 0

enum ELobbyDataIndex
{
	eLDI_Gamemode = 0,
	eLDI_Version = 1,
	eLDI_Playlist = 2,
	eLDI_Variant = 3,
	eLDI_RequiredDLCs = 4,
#if GAMELOBBY_USE_COUNTRY_FILTERING
	eLDI_Country = 5,
#endif
	eLDI_Language,
	eLDI_Map,
	eLDI_Skill,
	eLDI_Active,

	eLDI_Num
};

namespace GameLobbyData
{
	extern char const * const g_sUnknown;

	const uint32 ConvertGameRulesToHash(const char* gameRules);
	const char* GetGameRulesFromHash(uint32 hash, const char* unknownStr=g_sUnknown);

	const uint32 ConvertMapToHash(const char* mapName);
	const char* GetMapFromHash(uint32 hash, const char *pUnknownStr = g_sUnknown);

	const uint32 GetVersion();
	const bool IsCompatibleVersion(uint32 version);

	const uint32 GetPlaylistId();
	const uint32 GetVariantId();
	const uint32 GetGameDataPatchCRC();

	const int32 GetSearchResultsData(SCrySessionSearchResult* session, CryLobbyUserDataID id);
	const bool IsValidServer(SCrySessionSearchResult* session);
};

#endif // __GAMELOBBYDATA_H__
