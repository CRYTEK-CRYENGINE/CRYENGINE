// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Equipment Loadouts for C2MP

-------------------------------------------------------------------------
History:
- 18:09:2009  : Created by Ben Johnson

*************************************************************************/

#ifndef __EQUIPMENT_LOADOUT_H__
#define __EQUIPMENT_LOADOUT_H__

# pragma once

#include "ItemString.h"
#include "GameRules.h"
#include "GameRulesModules/IGameRulesClientConnectionListener.h"
#include "ProgressionUnlocks.h"
#include "AutoEnum.h"
#include "IPlayerProfiles.h"
#include <CryCore/Containers/VectorMap.h>
#include "UI/UITypes.h"

#ifndef _RELEASE
	#define LIST_LOADOUT_CONTENTS_ON_SCREEN 0 // Feel free to turn this on locally, but please don't commit it as anything but 0 [TF]
#else
	#define LIST_LOADOUT_CONTENTS_ON_SCREEN 0
#endif

#define MAX_DISPLAYNAME_LENGTH 15
#define MAX_SERVER_OVERRIDE_PACKAGES 10

#define MAX_DISPLAY_ATTACHMENTS 32
#define MAX_DISPLAY_DEFAULT_ATTACHMENTS 3
#define INVALID_PACKAGE -1

struct IFlashPlayer;
struct IItemSystem;
class CGameRules;
struct SEquipmentItem; 

class CEquipmentLoadout : public IGameRulesClientConnectionListener
	, public IPlayerProfileListener
{
public:

#define FPWEAPONCACHEITEMS 1
	enum EPrecacheGeometryEvent
	{
		ePGE_OnDeath=0,
		ePGE_OnTabSwap,

		ePGE_All,
		
		ePGE_Total = ePGE_All*FPWEAPONCACHEITEMS,
	};

	enum EEquipmentLoadoutFlags
	{
		ELOF_NONE											= 0,
		ELOF_HAS_CHANGED							= (1<<0),		// Var to indicate a loadout has changed since last being sent.
		ELOF_HAS_BEEN_SET_ON_SERVER		= (1<<1),		// On the server. Has the loadout ever been set for a client - for use in m_clientLoadouts 
		ELOF_CAN_BE_CUSTOMIZED				= (1<<2),		// Whether the loadout can be customized in the menus
		ELOF_HIDE_WHEN_LOCKED					= (1<<3),		// Hide this package when it is locked
	};

	// Categories of loadout slots
	enum EEquipmentLoadoutCategory
	{
		eELC_NONE             = 0,
		eELC_WEAPON						= 1,
		eELC_EXPLOSIVE        = 2,
		eELC_ATTACHMENT       = 3,
		eELC_SIZE             = 4
	};

	// Categories used by the UI when customising
	enum EEquipmentLoadoutUICategories
	{
		eELUIC_None										= 0,
		eELUIC_PrimaryWeapon          = 1,
		eELUIC_SecondaryWeapon				= 2,
		eELUIC_ExplosiveWeapon        = 3,
		eELUIC_BowAccessory						= 4,
	};

#define EQUIPMENT_PACKAGE_TYPES(f) \
	f(SDK)					

	AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(EEquipmentPackageGroup, EQUIPMENT_PACKAGE_TYPES, eEPG_NumGroups);

	struct SLoadoutWeapon
	{
		SLoadoutWeapon() {}
		SLoadoutWeapon(const char *pDisplayName, int id, bool showSwitchWeapon) : m_displayName(pDisplayName), m_id(id), m_showSwitchWeapon(showSwitchWeapon) {}

		CryFixedStringT<32> m_displayName;
		int m_id;
		bool m_showSwitchWeapon;
	};
	typedef std::vector<SLoadoutWeapon> TWeaponsVec;

	struct SClassicModel
	{
		enum EModelType
		{
			eMT_Rebel=0,
			eMT_Cell,
			eMT_Total
		};

		SClassicModel()
		{
			memset(&m_modelIndex, MP_MODEL_INDEX_DEFAULT, sizeof(m_modelIndex[0])*eMT_Total);
		}
		uint8 m_modelIndex[eMT_Total];
	};

	CEquipmentLoadout();
	~CEquipmentLoadout();

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId);
	virtual void OnClientDisconnect(int channelId, EntityId playerId);
	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId) {}
	virtual void OnOwnClientEnteredGame() {};
	// ~IGameRulesClientConnectionListener

	void PrecacheLevel();

	//IPlayerProfileListener
	virtual void SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);
	virtual void LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);
	//~IPlayerProfileListener
	
	void SelectProfileLoadout();

	void ClSendCurrentEquipmentLoadout(int channelId);
	
	void SvSetClientLoadout(int channelId, const CGameRules::EquipmentLoadoutParams &params);

	void SvAssignClientEquipmentLoadout(int channelId, int playerId);
	void ClAssignLastSentEquipmentLoadout(CActor *pActor);

	bool SvHasClientEquipmentLoadout(int channelId);
	void SvRemoveClientEquipmentLoadout(int channelId);

	void FlagItemAsNew(const char* itemName);
	void FlagItemAttachmentAsNew(const char* itemName);
	void ClearItemNewFlags(const int itemId);

	// Init
	void UpdateSpawnStatusMessage(); // Soon to be depreciated by equipment menu object CMenuMPEquipmentSelect
	void SendSpawnStatusMessageToFlash(const char *pMessage); // Soon to be depreciated by equipment menu object CMenuMPEquipmentSelect

	void SetPackageGroup( EEquipmentPackageGroup loadoutSet ) { m_currentPackageGroup = loadoutSet; }
	EEquipmentPackageGroup GetPackageGroup() const { return m_currentPackageGroup; }

	// Customizing
	bool SetPackageCustomizeItem(int packageId, int itemId, int category, int Nth);

	void SetCurrentCustomizePackage();
	void SetCurrentCustomizeItem(int itemId);
	const bool HasCurrentCustomizePackageChanged() const;
	bool HasLastSavedPackageChanged();

	void SetSelectedPackage(int index);
	int GetSelectedPackage() const;
	const char* GetCurrentSelectPackageName();

	void ResetLoadoutsToDefault();
	void CheckPresale();

	// Loadout select highlights
	void SetHighlightedPackage(int packageIndex);
	int GetCurrentHighlightPackageIndex() const;
	const char* GetCurrentHighlightPackageName();

	void SetHasPreGameLoadoutSent(bool hasSent) { m_hasPreGamePackageSent = hasSent; }
	bool GetHasPreGameLoadoutSent() { return m_hasPreGamePackageSent; }

	bool CanCustomizePackage(int index);
	bool IsPackageLocked(int index);

	bool TokenUnlockPackage();
	void AddPackageToBeUnlocked(int index) { m_unlockPack = index; }
	int GetPackageToBeUnlocked() const { return m_unlockPack; }
	
	EUnlockType GetUnlockTypeForItem(int index) const;
	
	bool IsItemLocked(int index, bool bNeedUnlockDescriptions, SPPHaveUnlockedQuery* pResults=NULL) const;
	bool TokenUnlockItem();
	void AddItemToBeUnlocked(int index) { m_unlockItem = index; }
	int GetTokenUnlockItem() { return m_unlockItem; }

	void SetCurrentCategory(int category) { m_currentCategory = category; }
	void SetCurrentNth(int nth) { m_currentNth = nth; }

	// HostMigration
	void ClGetCurrentLoadoutParams(CGameRules::EquipmentLoadoutParams &params);

	void OnGameEnded();
	void OnRoundEnded();
	void OnGameReset();

	const TWeaponsVec &GetWeaponsList();

	void InvalidateLastSentLoadout();
	void ForceSelectLastSelectedLoadout();

	void Display3DItem(uint8 itemNumber);

#if LIST_LOADOUT_CONTENTS_ON_SCREEN
	void ListLoadoutContentsOnScreen();
#endif

  const char* GetItemNameFromIndex(int idx);

	const char* GetLastSelectedWeaponName() const { return m_lastSelectedWeaponName.c_str(); }
	ILINE void InformNoLongerCustomizingWeapon()  { m_currentlyCustomizingWeapon = false;    }
	ILINE bool IsCustomizingWeapon() const        { return m_currentlyCustomizingWeapon;     }

	const char* GetPackageDisplayFromIndex(int index);
	const char* GetPackageDisplayFromName(const char *name);

	const char* GetPlayerModel(const int index);
	EEquipmentLoadoutCategory GetSlotCategory(const int slot);
	int GetCurrentSelectedPackageIndex();
	bool IsLocked(EUnlockType type, const char* name, SPPHaveUnlockedQuery &results) const;
	const char* GetCurrentPackageGroupName();

	void SetCurrentUICategory(EEquipmentLoadoutUICategories uiCategory) { m_curUICategory = uiCategory; }
	EEquipmentLoadoutUICategories GetCurrentUICategory() const { return m_curUICategory; }

	void SetUICustomiseAttachments(bool bCustomiseAttachments) { m_curUICustomiseAttachments = bCustomiseAttachments; }
	bool GetUICustomiseAttachments() const { return m_curUICustomiseAttachments; }

	void NewAttachmentInLoadout(const ItemString &newAttachmentName, int weapon);
	void DetachInLoadout(const ItemString &oldAttachmentName, int weapon);

	int IsWeaponInCurrentLoadout(const ItemString &weaponName);
	bool IsItemInCurrentLoadout(const int index) const;

	void ApplyAttachmentOverrides(uint8 * contents) const;

	const char *GetModelName(uint8 modelIndex) const;
	uint8 GetModelIndexOverride(uint16 channelId) const;
	uint8 AddModel(const char *pModelName);

	void UpdateClassicModeModel(uint16 channelId, int teamId);

	void StreamFPGeometry( const EPrecacheGeometryEvent event, const int package);
	void ReleaseStreamedFPGeometry( const EPrecacheGeometryEvent event );
	void Debug();

	void InitUnlockedAttachmentFlags();
	void SetCurrentAttachmentsToUnlocked();
	void UpdateWeaponAttachments( IEntityClass* pWeaponClass, IEntityClass* pAccessory );
	uint32 GetWeaponAttachmentFlags( const char* pWeaponName );
	void SpawnedWithLoadout( uint8 loadoutIdx );

public:

	typedef uint8 TEquipmentPackageContents[EQUIPMENT_LOADOUT_NUM_SLOTS];
	typedef CryFixedStringT<32> TFixedString;
	typedef CryFixedStringT<64> TFixedString64;
	typedef CryFixedArray<int, MAX_DISPLAY_DEFAULT_ATTACHMENTS> TDefaultAttachmentsArr;
	typedef std::map<int, SClassicModel> TClassicMap;

	// A local equipment package representation.
	struct SEquipmentPackage
	{
		SEquipmentPackage(const char* name, const char* displayName, const char* displayDescription)
			:	m_name(name), 
				m_displayName(displayName), 
				m_displayDescription(displayDescription), 
				m_flags(ELOF_NONE), 
				m_id(0),
				m_bUseIcon(false),
				m_modelIndex(MP_MODEL_INDEX_DEFAULT)
		{
			memset(&m_contents,0,sizeof(m_contents));
			memset(&m_defaultContents,0,sizeof(m_defaultContents));
		}

		const bool GetDisplayString(string& outDisplayStr, const char* szPostfix) const;
		const uint8 GetSlotItem(const EEquipmentLoadoutCategory slotCategory, const int Nth, const bool bDefault=false) const;
		void GetAttachmentItems(int weaponNth, CEquipmentLoadout::TDefaultAttachmentsArr& attachmentsArr) const;

		TEquipmentPackageContents m_contents;
		TEquipmentPackageContents m_defaultContents;
		TFixedString m_name;
		TFixedString m_displayName;
		TFixedString64 m_displayDescription;
		uint32 m_flags;
		int m_id;
		bool m_bUseIcon;
		uint8 m_modelIndex;
	};

	// Loadout Item definition.
	struct SEquipmentItem
	{
		SEquipmentItem(const char*name)
			: m_name(name), m_category(eELC_NONE), m_uniqueIndex(0), m_parentItemId(0), m_subcategory(0), m_allowedPackageGroups(0),
			  m_hideWhenLocked(false), m_bShowNew(false), m_bShowAttachmentsNew(false), 
				m_bUseIcon(false),m_showSwitchWeapon(true)
		{
		}

		TFixedString m_name;
		TFixedString m_displayName;
		TFixedString m_displayTypeName;
		TFixedString m_description;

		EEquipmentLoadoutCategory m_category;
		uint32 m_uniqueIndex;
		uint32 m_parentItemId;	// If not zero this item is a skin as the id is it's base item id

		uint8 m_subcategory;
		uint8 m_allowedPackageGroups;

		bool m_hideWhenLocked;
		bool m_bShowNew;							// Show as new in the UI
		bool m_bShowAttachmentsNew;		// Show as having new attachments in the UI
		bool m_bUseIcon;							// Use icon instead of 3D model in the loadouts
		bool m_showSwitchWeapon;
	};

	struct SAttachmentInfo
	{
		SAttachmentInfo(int itemId)
			:	m_itemId(itemId), m_locked(false), m_bShowNew(false)
		{
		}

		int m_itemId;
		bool m_locked;
		bool m_bShowNew;
	};
	typedef CryFixedArray<SAttachmentInfo, MAX_DISPLAY_ATTACHMENTS> TAttachmentsArr;

	typedef std::vector<SEquipmentItem> TEquipmentItems;
	typedef std::vector<SEquipmentPackage> TEquipmentPackages;

	struct SPackageGroup
	{
		SPackageGroup()
		{
			Reset();
		}

		void Reset()
		{
			m_packages.clear();
			m_selectedPackage = 0;
			m_profilePackage = 0;
			m_lastSentPackage = INVALID_PACKAGE;
			m_highlightedPackage = 0;
		}

		TEquipmentPackages m_packages;
		int m_selectedPackage;
		int m_profilePackage;
		int m_lastSentPackage;
		int m_highlightedPackage;
	};

	const CEquipmentLoadout::SPackageGroup& GetCurrentPackageGroup();
	const SEquipmentPackage* GetPackageFromId(const int index);
	const TEquipmentItems& GetAllItems() { return m_allItems; }
	const SEquipmentItem* GetItem(const int index);
	const SEquipmentItem* GetItemByName(const char *name) const;

	void GetWeaponSkinsForWeapon(int itemId, TAttachmentsArr& arrayAttachments);
	void GetAvailableAttachmentsForWeapon(int itemId, TAttachmentsArr* arrayAttachments, TDefaultAttachmentsArr* arrayDefaultAttachments, bool bNeedUnlockDescriptions = true);
	void GetListedAttachmentsForWeapon(int itemId, TAttachmentsArr* arrayAttachments );
	bool SetPackageNthWeaponAttachments(const int packageId, uint32 Nth, TDefaultAttachmentsArr& arrayAttachments);
	int  GetItemIndexFromName(const char *name);

	void RandomChoiceModel ();

	void GoingBack(){m_bGoingBack=true;}

	const bool ClIsAttachmentIncluded(const SEquipmentItem* pAttachmentItem, const char* pWeaponItemName,const TEquipmentPackageContents& contents) const;


private:
	// A clients equipment package representation on the server.
	struct SClientEquipmentPackage
	{
		SClientEquipmentPackage()
			:	m_flags(ELOF_NONE), m_modelIndex(MP_MODEL_INDEX_DEFAULT), m_loadoutIdx(0)
		{
			memset(&m_contents,0,sizeof(m_contents));
			m_weaponAttachmentFlags = 0;
		}

		uint32 m_flags;
		// Specify which attachments the player has access to. 
		uint32 m_weaponAttachmentFlags;

		TEquipmentPackageContents m_contents;
		uint8 m_modelIndex;
		uint8 m_loadoutIdx;
	};

	struct SAttachmentOverride
	{
		uint8 m_id;
		bool m_bInUse;

		SAttachmentOverride()
			: m_id(0)
			, m_bInUse(false)
		{
		}

		void Reset()
		{
			m_bInUse = false;
		}
	};

	typedef std::vector<SAttachmentOverride> TAttachmentsOverridesVector;
	typedef std::vector<TAttachmentsOverridesVector> TAttachmentsOverridesWeapons;

  void Reset();

	SEquipmentPackage* GetPackageFromIdInternal(const int index);
	SEquipmentPackage* GetPackageFromName(const char *name);
	int         GetPackageIndexFromName(EEquipmentPackageGroup group, const char *name);
	int         GetPackageIndexFromId(EEquipmentPackageGroup group, int id);

	int  CountOfCategoryUptoIndex(EEquipmentLoadoutCategory category, int Nth, int upToIndex);
	int  GetSlotIndex(EEquipmentLoadoutCategory category, int Nth);

	void SetNthWeaponAttachments(uint32 Nth, TDefaultAttachmentsArr& arrayAttachments);
	void SetDefaultAttachmentsForWeapon(int itemId, uint32 Nth);
	void ApplyWeaponAttachment(const SEquipmentItem *pItem, const IItemSystem *pItemSystem, EntityId lastWeaponId);

	void LoadDefaultEquipmentPackages();

	void LoadSavedEquipmentPackages(EEquipmentPackageGroup group, IPlayerProfile* pProfile, bool online);

	void LoadItemDefinitions();
	void LoadItemDefinitionParams(XmlNodeRef itemXML, SEquipmentItem &item);
	void LoadItemDefinitionFromParent(const char* parentItemName, SEquipmentItem &item);

	bool SaveEquipmentPackage(IPlayerProfile* pProfile, EEquipmentPackageGroup group, int packIndex, int count);
	
	EMenuButtonFlags GetNewFlagForItemCategory(EEquipmentLoadoutCategory category, int subcategory);

	static bool CompareWeapons(const SLoadoutWeapon &elem1, const SLoadoutWeapon &elem2);

	void	   ClSetAttachmentInclusionFlags(const TEquipmentPackageContents &contents, uint32& flags);
	const bool SvIsAttachmentIncluded(const SEquipmentItem* pAttachmentItem,const uint32& inclusionFlags) const;
	void	   SvAssignWeaponAttachments(const uint8 itemId, const SClientEquipmentPackage& package, CActor * pActor, int playerId );
	const bool AttachmentIncludedInPackageContents(const SEquipmentItem* pAttachmentItem,  const TEquipmentPackageContents& contents) const;
	void	   SetAttachmentInclusionFlag(uint32& inclusionFlags, const int attachmentItemId);
	const bool IsAttachmentInclusionFlagSet(const uint32& inclusionFlags, const int attachmentItemId) const;
	uint8 GetClassicModeModel(int itemId, SClassicModel::EModelType modelType) const;

	void ResetOverrides();

	typedef std::map<int, SClientEquipmentPackage> TClientEquipmentPackages;
	typedef CryFixedArray<TFixedString64, MP_MODEL_INDEX_DEFAULT> TModelNamesArray;
	typedef CryFixedStringT<128> TStreamedGeometry[ePGE_Total];

	TEquipmentItems m_allItems;
	TWeaponsVec m_allWeapons;
	TModelNamesArray m_modelNames;

	uint8 m_weaponsStartIndex;
	uint8 m_attachmentsStartIndex;
	uint8 m_slotCategories[EQUIPMENT_LOADOUT_NUM_SLOTS];

	// Local Client
	SPackageGroup m_packageGroups[eEPG_NumGroups];
	TClassicMap m_classicMap;
	TEquipmentPackageContents m_currentCustomizePackage;
	TFixedString m_lastSelectedWeaponName;
	EEquipmentPackageGroup m_currentPackageGroup;
	TAttachmentsOverridesWeapons m_attachmentsOverridesWeapon;
	TStreamedGeometry m_streamedGeometry;

	bool m_haveSortedWeapons;
	bool m_hasPreGamePackageSent;
	bool m_currentlyCustomizingWeapon;

	int m_unlockPack;
	int m_unlockItem;

	int m_currentCategory;
	int m_currentNth;

	// UI
	EEquipmentLoadoutUICategories m_curUICategory;
	bool m_curUICustomiseAttachments;

	bool m_currentCustomizeNameChanged;
	bool m_bShowCellModel;
	bool m_bGoingBack;

	TClientEquipmentPackages m_clientLoadouts;

	typedef VectorMap<uint32, uint32>	TWeaponAttachmentFlagMap;	//weapon class name crc to attach flags
	TWeaponAttachmentFlagMap	m_unlockedAttachments;
	TWeaponAttachmentFlagMap	m_currentAvailableAttachments;
};

#endif // ~__EQUIPMENT_LOADOUT_H__
