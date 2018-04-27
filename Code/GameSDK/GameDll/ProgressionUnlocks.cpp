// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 12:05:2010		Created by Ben Parbury
*************************************************************************/

#include "StdAfx.h"
#include "ProgressionUnlocks.h"

#include "PlayerProgression.h"
#include "EquipmentLoadout.h"
#include "Utility/DesignerWarning.h"
#include "GameParameters.h"
#include "ItemSharedParams.h"
#include "UI/HUD/HUDUtils.h"
#include "PersistantStats.h"

struct SUnlockData
{
	EUnlockType			m_type;
	const char *		m_name;
	const char *		m_description;

	SUnlockData(EUnlockType type, const char * name, const char * desc)
	{
		m_type = type;
		m_name = name;
		m_description = desc;
	}
};

const SUnlockData s_unlockData[] =
{
	SUnlockData (eUT_CreateCustomClass, "customClass",				"@ui_menu_unlock_createcustomclass"),
	SUnlockData (eUT_Loadout,						"loadout",						"@ui_menu_unlock_loadout"),
	SUnlockData (eUT_Weapon,						"weapon",							"@ui_menu_unlock_weapon"),
	SUnlockData (eUT_Playlist,					"playlist",						"@ui_menu_unlock_playlist"),
	SUnlockData (eUT_Attachment,				"attachment",					"@ui_menu_unlock_attachment"),
};

struct SUnlockReasonData
{
	EUnlockReason m_reason;
	const char* m_name;

	SUnlockReasonData(EUnlockReason reason, const char * name)
	{
		m_reason = reason;
		m_name = name;
	}

};

//TODO: not used currently, remove if this doesn't change
const SUnlockReasonData s_unlockReasonData[] =
{
	SUnlockReasonData(eUR_None, "none"),
	SUnlockReasonData(eUR_SuitRank, "suitrank"),
	SUnlockReasonData(eUR_Rank, "rank"),
	SUnlockReasonData(eUR_Token, "token"),
	SUnlockReasonData(eUR_Assessment, "assessment"),
};

SUnlock::SUnlock(XmlNodeRef node, int rank)
{
	m_name[0] = '\0';
	m_rank = rank;
	m_reincarnation = 0;
	m_unlocked = false;
	m_type = eUT_Invalid;

	DesignerWarning(strcmpi(node->getTag(), "unlock") == 0 || strcmpi(node->getTag(), "allow") == 0, "expect tag of unlock or allow at %d", node->getLine());

	const char * theName = node->getAttr("name");
	const char * theType = node->getAttr("type");
	const char * theReincarnationLevel = node->getAttr("reincarnation");

	// These pointers should always be valid... if an attribute isn't found, getAttr returns a pointer to an empty string [TF]
	assert (theName && theType);

	if (theType && theType[0] != '\0')
	{
		m_type = SUnlock::GetUnlockTypeFromName(theType);
		cry_strcpy(m_name, theName);

		bool expectName = (m_type == eUT_Loadout || m_type == eUT_Weapon || m_type == eUT_Attachment || m_type == eUT_Playlist || m_type == eUT_CreateCustomClass);
		bool gotName = (theName[0] != '\0');

		if (expectName != gotName && m_type != eUT_Invalid) // If it's invalid, we'll already have displayed a warning...
		{
			GameWarning("[PROGRESSION] An unlock of type '%s' %s have a name but XML says name='%s'", theType, expectName ? "should" : "shouldn't", theName);
		}
	}
	else
	{
		GameWarning("[PROGRESSION] XML node contains an 'unlock' tag with no type (name='%s')", theName);
	}

	if (theReincarnationLevel != NULL && theReincarnationLevel[0] != '\0')
	{
		m_reincarnation = atoi(theReincarnationLevel);
		CPlayerProgression *pPlayerProgression = CPlayerProgression::GetInstance();
		DesignerWarning(m_reincarnation > 0 && m_reincarnation < pPlayerProgression->GetMaxReincarnations()+1, "Unlock %s reincarnation parameter is outside of the range 0 - %d", theName, pPlayerProgression->GetMaxReincarnations()+1);
	}
}

void SUnlock::Unlocked(bool isNew)
{
	if(!m_unlocked)
	{
    m_unlocked = true;

		CPlayerProgression *pPlayerProgression = CPlayerProgression::GetInstance();

		switch(m_type)
		{				
		case eUT_CreateCustomClass:
			{
				pPlayerProgression->UnlockCustomClassUnlocks( isNew );
					
				if( isNew )
				{
					pPlayerProgression->AddUINewDisplayFlags(eMBF_CustomLoadout);
				}
			}
			break;
		case eUT_Weapon:
			{
				if( isNew )
				{
					if (CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout())
					{
						pEquipmentLoadout->FlagItemAsNew(m_name);
					}
				}

			}
			break;
		}
	}

  if ( m_unlocked )
  {
	  //This needs to always happen because of switching between SP and MP
	  switch(m_type)
	  {
			case eUT_Weapon:
		  {
				CPersistantStats::GetInstance()->IncrementMapStats(EMPS_WeaponUnlocked,m_name);
		  }
		  break;
	  }
  }
}

///static
EUnlockType SUnlock::GetUnlockTypeFromName(const char* name)
{
	if (name && name[0] != '\0')
	{
		for(int i = 0; i < eUT_Max; i++)
		{
			if (s_unlockData[i].m_name && s_unlockData[i].m_name[0] != '\0')
			{
				if(strcmpi(name, s_unlockData[i].m_name) == 0)
				{
					return s_unlockData[i].m_type;
				}
			}
		}

		GameWarning("[PROGRESSION] Invalid unlock type '%s'", name);
	}

	return eUT_Invalid;
}


///static
EUnlockReason SUnlock::GetUnlockReasonFromName( const char* name )
{
	for(int i = 0; i < eUR_Max; i++)
	{
		if(strcmpi(name, s_unlockReasonData[i].m_name) == 0)
		{
			return s_unlockReasonData[i].m_reason;
		}
	}

	GameWarning("[PROGRESSION] Invalid unlock reason '%s'", name);
	return eUR_Invalid;
}


///static
const char * SUnlock::GetUnlockReasonName( EUnlockReason reason )
{
	for(int i = 0; i < eUR_Max; i++)
	{
		if(reason == s_unlockReasonData[i].m_reason)
		{
			return s_unlockReasonData[i].m_name;
		}
	}

	CRY_ASSERT(0);
	return "";
}


///static
const char* SUnlock::GetUnlockTypeName(EUnlockType type)
{
	for(int i = 0; i < eUT_Max; i++)
	{
		if(type == s_unlockData[i].m_type)
		{
			return s_unlockData[i].m_name;
		}
	}

	CRY_ASSERT(0);
	return "";
}

//static
const char * SUnlock::GetUnlockTypeDescriptionString(EUnlockType type)
{
#ifndef _RELEASE
	const char *result = "Unknown";
#else
	const char *result = "";
#endif

	for(int i = 0; i < eUT_Max; i++)
	{
		if(type == s_unlockData[i].m_type)
		{
			return s_unlockData[i].m_description;
		}
	}

	CRY_ASSERT(0);
	return result;
}

//static
const char * SUnlock::GetUnlockReasonDescriptionString(EUnlockReason reason, int data)
{
#ifndef _RELEASE
	const char *result = "Other";
#else
	const char *result = "";
#endif
	switch (reason)
	{
	case eUR_Rank:
		result = "@ui_menu_unlock_reason_rank";
		break;
	};

	return result;
}

bool SUnlock::operator ==(const SUnlock &rhs) const
{
	if(m_type != rhs.m_type || m_rank != rhs.m_rank || m_reincarnation != rhs.m_reincarnation || m_unlocked != rhs.m_unlocked || strcmp(m_name, rhs.m_name) != 0)
	{
		return false;
	}

	return true;
}

bool SUnlock::GetUnlockDisplayString( EUnlockType type, const char* name, CryFixedStringT<32>& outStr )
{
	// TODO: Setup Playlist unlocks and any others
	bool retval = false;

	switch( type )
	{
	case eUT_Weapon:
		{
			const CItemSharedParams* pItemShared = g_pGame->GetGameSharedParametersStorage()->GetItemSharedParameters( name, false );
			if( pItemShared )
			{
				outStr.Format( pItemShared->params.display_name.c_str() );
				retval = true;
			}
			break;
		}
	case eUT_CreateCustomClass:
		{
			CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
			if( pEquipmentLoadout )
			{
				const char* packageName = pEquipmentLoadout->GetPackageDisplayFromName( name );
				if( packageName )
				{
					outStr.Format( CHUDUtils::LocalizeString(packageName) );
					retval = true;
				}
			}

			break;
		}
	case eUT_Attachment:
		{
			const char* pAttachmentName = strstr(name, ".");
			if( pAttachmentName && pAttachmentName[0] )
			{	
				CEquipmentLoadout* pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
				if( pEquipmentLoadout )
				{
					if( const CEquipmentLoadout::SEquipmentItem *pUnlockItem = pEquipmentLoadout->GetItemByName( pAttachmentName+1 ) )
					{
						outStr.Format( pUnlockItem->m_displayName.c_str() );
						retval = true;
					}
				}
			}
			break;
		}
	}

	return retval;
}
