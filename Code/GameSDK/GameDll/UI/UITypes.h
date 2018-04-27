// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ___HUDTYPES___
#define ___HUDTYPES___

// These remap to flash frames
enum ECrosshairTypes {
	eHCH_None = 0,
	eHCH_Normal = 1,
	eHCH_Shotgun = 3,
	eHCH_LTAG = 5,
	eHCH_Gaus = 6,

	eHCH_END_DONOTUSE,
};

enum EWeaponSwitchSpecialParam
{
	eWSSP_None					= 0,
	eWSSP_Next					= 1,
	eWSSP_Prev					= 2
};

enum EInfoSystem
{
	eInfo_Info = 0,
	eInfo_Warning,
	eInfo_NumSystems
};

enum EInfoSystemPriority
{
	eInfoPriority_High = 0,
	eInfoPriority_Normal,
	eInfoPriority_Low
};

enum EHUDEventHitType
{
	eHUDEventHT_Bullet = 0,
	eHUDEventHT_Explosive = 1,
	eHUDEventHT_numTypes = 2
};

enum EHUDStyle
{
	eHUDStyle_Normal = 0,
	eHUDStyle_NUMSTYLES,
};

enum ETacticalInformationIconType
{
	eIconType_Usable = 0,
	eIconType_EnemyGreen,
	eIconType_EnemyOrange,
	eIconType_EnemyRed,
	eIconType_Friendly,
	eIconType_AmmoBox,
	eIconType_ArrowBox,
	eIconType_Collectible,
	eIconType_Arrow,
	eIconType_Hazard,
	eIconType_Explosive,
	eIconType_SmartMine_Cell,
	eIconType_MineField_Cell,
	eIconType_Weapons,
	eIconType_Ay89,
	eIconType_C4,
	eIconType_Dsg1,
	eIconType_Feline,
	eIconType_Frag_Grenade,
	eIconType_Gauss,
	eIconType_Grendel,
	eIconType_Hammer,
	eIconType_Hmg,
	eIconType_Jackall,
	eIconType_Jaw,
	eIconType_K_Volt,
	eIconType_Ltag,
	eIconType_Majestic,
	eIconType_Marshall,
	eIconType_Microwave,
	eIconType_Mk60,
	eIconType_Nova,
	eIconType_Scar,
	eIconType_AY69,
	eIconType_HeavyMortar,
	eIconType_HeavyMinigun,
	eIconType_LightningGun,
	eIconType_NumIcons,
};

enum EDPadDirections
{
	eDPAD_Invalid,
	eDPAD_Bottom,
	eDPAD_Right,
	eDPAD_Top,
	eDPAD_Left,
};

enum EControlScheme
{
	eControlScheme_NotSpecified,
	eControlScheme_Keyboard, // Mouse is available but focus is on keyboard
	eControlScheme_KeyboardMouse,
	eControlScheme_XBoxOneController,
	eControlScheme_PS4Controller,
};

enum EPreGameCountdownType
{
	ePreGameCountdown_WaitingForPlayers = 0,
	ePreGameCountdown_MatchStarting,
	ePreGameCountdown_RoundStarting,
};

enum EHUDInteractionMsgType // Order is the priority shown
{
	eHIMT_INTERACTION_HIGHESTPRIORITY = 0,
	eHIMT_HACKMSG,
	eHIMT_USABLE,
	eHIMT_USABLE_LOOK,
	eHIMT_INTERACTION,
	eHIMT_INTERACTION_GAMERULES,
	eHIMT_INTERACTION_LEANING,
	eHIMT_HELPERHINT,
	eHIMT_NUMTYPES,
};

#define PlayerInteractionTypeList(f)   \
	f(eInteraction_None)                 \
	f(eInteraction_PickupItem)           \
	f(eInteraction_ExchangeItem)         \
	f(eInteraction_PickupAmmo)           \
	f(eInteraction_Use)                  \
	f(eInteraction_Grab)                 \
	f(eInteraction_GrabEnemy)            \
	f(eInteraction_Stealthkill)					 \
	f(eInteraction_LetGo)                \
	f(eInteraction_StopUsing)            \
	f(eInteraction_LargeObject)					 \
	f(eInteraction_GameRulesPickup)			 \
	f(eInteraction_GameRulesDrop)				 \
	f(eInteraction_HijackVehicle)				 \
	f(eInteraction_Ladder)							 \

AUTOENUM_BUILDENUMWITHTYPE(EInteractionType, PlayerInteractionTypeList);

enum EHUDPowerStruggleCaptureBarType
{
	eHPSCBT_None = -1,
	eHPSCBT_FriendlyCaptureFromNeutral,
	eHPSCBT_EnemyCaptureFromNeutral,
	eHPSCBT_FriendlyCaptureFromEnemy,
	eHPSCBT_EnemyCaptureFromFriendly,
	eHPSCBT_NumTypes,
};

enum ECinematicVTOLUpdateFlags
{
	eVTOLFlag_Pitch								= BIT(0),
	eVTOLFlag_Roll								= BIT(1),
	eVTOLFlag_Speed								= BIT(2),
	eVTOLFlag_AddIcon							= BIT(3),
	eVTOLFlag_RemoveIcon					= BIT(4),
	eVTOLFlag_HighlightIcon				= BIT(5),
	eVTOLFlag_RemoveHighlightIcon	= BIT(6),
};

enum EObjectiveLineDrawMode
{
	eObjectiveLineDrawMode_EntityToEntity = 0,
	eObjectiveLineDrawMode_UIToEntity,
};

enum EObjectiveAnalysisEvent
{
	eOAE_Start = 0,
	eOAE_WorldSectionAnimStart,
	eOAE_ScreenSectionAnimStart,
	eOAE_LineDrawStart,
	eOAE_AdvanceNextSection,
	eOAE_Choosing,
	eOAE_Stop,
};

enum EObjectiveAnalysisMode
{
	eOAM_ObjectiveCinematic = 0,
	eOAM_ObjectiveGameplay,
};

enum EMenuButtonFlags
{
	eMBF_None = 0,
	eMBF_LoadoutToken							= BIT(1),
	eMBF_WeaponToken							= BIT(2),
	eMBF_Weapon										= BIT(3),
	eMBF_SecondaryWeapon					= BIT(4),
	eMBF_ExplosiveWeapon					= BIT(5),
	eMBF_WeaponAttachmentToken		= BIT(6),
	eMBF_SecondaryWeaponAttachmentToken = BIT(7),
	eMBF_AttachmentToken					= BIT(8),
	eMBF_CustomLoadout						= BIT(15),
	eMBF_SkillAssesment						= BIT(16),
};

enum EBootableSystemAction
{
	eBootAction_NormalState = 0,
	eBootAction_Boot,
	eBootAction_Destroy,
	eBootAction_Malfunction,
	eBootAction_Logoff,
	eBootAction_Hidden,
	eBootAction_All
};

struct IUIControlSchemeListener
{
	virtual ~IUIControlSchemeListener() {};
	virtual bool OnControlSchemeChanged(const EControlScheme controlScheme) = 0;
};

typedef CListenerSet<IUIControlSchemeListener*> TUIControlSchemeListeners;


#define OBJECTIVEANALYSIS_MAXITEMS 3

#endif // ___HUDTYPES___
