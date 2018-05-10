// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Responsible for loading downloadable content into the game
and informing the game of what content is loaded.

-------------------------------------------------------------------------
History:
- 18:05:2010  : Created by Martin Sherburn
- 21:08:2012  : Maintained by Andrew Blackwell

*************************************************************************/

#ifndef __DLCMANAGER_H__
#define __DLCMANAGER_H__

class CMenuData;

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//reserve 8 flags for DLC and 8 flags for presale so we're really future proof
#define MAX_DLC_COUNT 16
#define MAX_DLC_ITEM_UNLOCKS 16

#if defined(DEDICATED_SERVER)
	#define DLC_LOAD_ON_CONSTRUCTION
#endif

#include "AutoEnum.h"
#include <CryCore/Containers/CryFixedArray.h>

#include "IPlayerProfiles.h"
#include "ProgressionUnlocks.h"

#include "UI/HUD/HUDUtils.h"

#define ENTITLEMENTS_AUTHORATIVE 0

#define NOTDLCLevelList(f) \
	f(eNOTDLC_c3mp_airport) \
	f(eNOTDLC_mpTestLevel) \
	f(eNOTDLC_c3mp_tanker) \
	
AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(eNOTDLCLevelList, NOTDLCLevelList, eNOTDLC_INVALID_LEVEL, eNOTDLC_NUM_LEVELS);


#define NOTDLCGameModeList(f) \
	f(eNOTDLC_Assault) \
	f(eNOTDLC_CaptureTheFlag) \
	f(eNOTDLC_CrashSite) \
	f(eNOTDLC_Extraction) \
	f(eNOTDLC_InstantAction) \
	f(eNOTDLC_TeamInstantAction) \
	f(eNOTDLC_Gladiator) \
	f(eNOTDLC_Spears) \
	
AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(eNOTDLCGameModeList, NOTDLCGameModeList, eNOTDLC_INVALID_GAMEMODE, eNOTDLC_NUM_GAMEMODES);


// Pre-Declarations
class CCheckPurchaseHistoryTask;

#define MAX_DLC_NAME 32
#define MAX_ROOT_LENGTH 32
#define MAX_TAG_LENGTH 32
#define MAX_DESC_LENGTH 32

struct DLCContents
{
	DLCContents(): bonusID( 0 ), uniqueID( 0 )	{ name [0 ] = '\0';  }
	std::vector<string>										levels;
	uint32																bonusID;	//0 indicates no bonuses/pre-sale promotions included
	wchar_t																name[ MAX_DLC_NAME ];
	CryFixedStringT<MAX_ROOT_LENGTH>			root;
	CryFixedStringT<ICryPak::g_nMaxPath>	scoreRewardsPath;
	CryFixedStringT<ICryPak::g_nMaxPath>	playlistsPath;
	CryFixedStringT<MAX_TAG_LENGTH>				uniqueTag;
	CryFixedStringT<MAX_DESC_LENGTH>			descriptionStr;
	uint32																uniqueID;
	uint32																messageID;
};

class CDLCManager : IPlatformOS::IDLCListener, IPlatformOS::IPlatformListener, IGameWarningsListener
{
public:
	CDLCManager();
	~CDLCManager();

	void LoadDownloadableContent( uint32 userIdOveride = INVALID_CONTROLLER_INDEX );
	void DisableDownloadableContent();
	void PatchMenu(CMenuData& menu);


	bool		IsDLCLoaded(int index) const { return ( (m_loadedDLCs & m_allowedDLCs) & BIT(index)) != 0; }
	uint32	GetLoadedDLCs() const { return m_loadedDLCs & m_allowedDLCs; }

	uint32 GetSquadCommonDLCs();
	uint32 GetRequiredDLCsForLevel(const char* pLevelName);
	uint32 GetRequiredDLCs();

	static bool MeetsDLCRequirements(uint32 requiredDLCs, uint32 loadedDLCs) { return (requiredDLCs & loadedDLCs) == requiredDLCs; }

	bool IsLevelStandard( const char * levelname);	// Note: This function returns false for anything else than the shipped levels
	bool IsGameModeStandard( const char * gamemode );	// Note: This function returns false for anything else than the shipped game modes

	int GetNamesStringOfPlayersMissingDLCsForLevel(const char* pLevelName, stack_string* pPlayersString);

	void ActivatePreSaleBonuses( bool showPopup, bool fromSuitReboot = false );

	const char* ScoreRewardsFilename( const char* pLevelName );

	void AddPlaylists();

	void OnlineAttributesRead();
	void SetAppliedEntitlements( uint32 entitlements ) { m_appliedDLCStat = entitlements; }

	// IPlatformOS::IDLCListener interface implementation
	virtual void OnDLCMounted(const XmlNodeRef &rootNode, const char* sDLCRootFolder);
	virtual void OnDLCMountFailed(IPlatformOS::EDLCMountFail reason);
	virtual void OnDLCMountFinished(int nPacksFound );
	// IPlatformOS::IDLCListener interface implementation

	// IPlatformOS::IPlatformListener interface implementation
	virtual void OnPlatformEvent(const IPlatformOS::SPlatformEvent& event);
	// ~IPlatformOS::IPlatformListener interface implementation

	// IGameWarningsListener
	virtual bool OnWarningReturn(THUDWarningId id, const char* returnValue);
	virtual void OnWarningRemoved(THUDWarningId id) {}
	// ~IGameWarningsListener

	void OnDLCRemoved(const char* sDLCRootFolder);

	void ProcessDelayedWarnings();

	bool HaveUnlocked( EUnlockType type, const char* name, SPPHaveUnlockedQuery & results );

	void Update();

	uint32 GetAppliedDLCBitfield() const { return m_appliedDLCStat; }
	uint32 GetDLCXPToAward() const { return m_DLCXPToAward; }
	void ClearDLCXPToAward() { m_DLCXPToAward = 0; }

	bool IsPIIEntitlementSet() const;

private:
	bool VerifyCRCs(const XmlNodeRef &crcNode, const char* sDLCRootFolder);
	bool CheckLevels( int dlcId, const char* sDLCRootFolder );
	void PopulateDLCContents(const XmlNodeRef &rootNode, int dlcId, const char* name );
	void ClearDLCContents(int dlcId);
	bool LevelExists(const char* pLevelName);

	bool IsDLCReallyLoaded(int index) { return ( m_loadedDLCs & BIT(index)) != 0; }

	void RequestDLCWarning( const char* warningName, uint32 priority, bool alwaysDelay = false );

	//keep private, external callers should continue to use GetRequiredDLCsForLevel
	int DlcIdForLevel( const char* pLevelName );

	void DoDLCUnlocks( XmlNodeRef unlocksXml, int dlcId );

	void AddNewDLCApplied(const int index);
	bool IsNewDLC(const int index) const;

	bool IsPresaleEntitlementSet(int entitlementIndex) const;
	bool IsDLCEntitlementSet(int entitlementIndex) const;
	bool IsEntitlementSet( const char* pTag) const;
	int SetEntitlement( int entitlementIndex, bool presale );

private:
	DLCContents											m_dlcContents[MAX_DLC_COUNT];
	CryFixedArray<SUnlock, MAX_DLC_ITEM_UNLOCKS >			m_itemUnlocks;
	CryFixedArray<uint32, MAX_DLC_ITEM_UNLOCKS >			m_itemUnlockDLCids;

	CryFixedStringT<128>						m_prevLevelName;
	CryFixedStringT<128>						m_dlcWarning;


	uint32													m_loadedDLCs;
	uint32													m_allowedDLCs;
	uint32													m_entitledDLCs;

	uint32													m_warningPriority;
	int															m_requiredDLCs;
	bool														m_dlcLoaded;
	
	bool														m_allowedDLCUpToDate;
	bool														m_allowedDLCCheckFailed;
	bool														m_onlineAttributesRead;
	bool														m_bContentRemoved;
	bool														m_bContentAvailable;

	uint32													m_appliedDLCStat;
	uint32													m_DLCXPToAward;
	bool														m_bNewDLCAdded;
	bool														m_queueEventEntitlement;

	CryLobbyTaskID									m_entitlementTask;
	

};

#endif // __DLCMANAGER_H__
