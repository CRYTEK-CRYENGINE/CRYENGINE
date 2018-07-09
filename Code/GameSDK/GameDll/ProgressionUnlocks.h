// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 08:04:2010		Created by Ben Parbury
*************************************************************************/

#ifndef __PROGRESSIONUNLOCKS_H__
#define __PROGRESSIONUNLOCKS_H__

#include "Utility/StringUtils.h"
#include <CryCore/Containers/CryFixedArray.h>
#include <CryString/StringUtils.h>

// These are in order sorted for display. Highest displayed first
// This enum matches Flash Actionscript code, so must be kept in sync
enum EUnlockType
{
	eUT_Invalid = -1,
	eUT_TokenMax,		//Used for progressionTokenSystem (Tokens should run from 0 to TokenMax)
	eUT_CreateCustomClass = eUT_TokenMax,
	eUT_Loadout,
	eUT_Weapon,
	eUT_Playlist,
	eUT_Attachment,
	eUT_Max,
};

// These are in order sorted for display. Highest displayed first
enum EUnlockReason
{
	eUR_Invalid = -1,
	eUR_None = 0,
	eUR_SuitRank = 1,
	eUR_Rank = 2,
	eUR_Token = 3,
	eUR_Assessment = 4,
	eUR_Max,
};

struct SPPHaveUnlockedQuery
{
	SPPHaveUnlockedQuery() { Clear(); }

	void Clear() { unlockString.clear(); reason = eUR_None; exists = false; unlocked = false; available = false; getstring = true; }

	CryFixedStringT<128> unlockString;
	EUnlockReason reason;
	bool exists;
	bool unlocked;
	bool available;	// if you're at the right rank but it may still be locked
	bool getstring; // set to false to avoid obtaining and localizing the weapon description. Saves 0.1ms on 360
};

struct SUnlock
{
	SUnlock(XmlNodeRef node, int rank);

	void Unlocked(bool isNew);

	const static int k_maxNameLength = 32;
	char m_name[k_maxNameLength];

	EUnlockType m_type;
	int8 m_rank;
	int8 m_reincarnation;
	bool m_unlocked;

	static EUnlockType GetUnlockTypeFromName(const char* name);
	static const char * GetUnlockTypeName(EUnlockType type);

	static EUnlockReason GetUnlockReasonFromName(const char* name);
	static const char * GetUnlockReasonName(EUnlockReason reason);

	static const char * GetUnlockTypeDescriptionString(EUnlockType type);
	static const char * GetUnlockReasonDescriptionString(EUnlockReason reason, int data=0);

	static bool GetUnlockDisplayString( EUnlockType type, const char* name, CryFixedStringT<32>& outStr );

	bool operator ==(const SUnlock &rhs) const;
};

#endif // __PROGRESSIONUNLOCKS_H__
