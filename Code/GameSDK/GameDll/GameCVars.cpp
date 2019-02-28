// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:8:2004   10:50 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "ItemSharedParams.h"
#include "WeaponSharedParams.h"
#include "Player.h"
#include "PlayerProgression.h"
#include "HitDeathReactions.h"
#include "HitDeathReactionsSystem.h"
#include "MovementTransitionsSystem.h"
#include "PickAndThrowProxy.h"
#include "ActorImpulseHandler.h"

#include <CryNetwork/INetwork.h>
#include <IGameObject.h>
#include <IActorSystem.h>
#include <IItemSystem.h>
#include <IVehicleSystem.h>
#include "WeaponSystem.h"

#include "ItemString.h"
#include "NetInputChainDebug.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"
#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/Utils/ScreenLayoutManager.h"
#include "UI/Utils/ILoadingMessageProvider.h"
#include <CryNetwork/INetworkService.h>

#include "LagOMeter.h"
#include "GameRulesModules/IGameRulesTeamsModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameActions.h"
#include "GodMode.h"
#include "BodyManagerCVars.h"
#include "DummyPlayer.h"
#include "PlayerInput.h"
#include "StatsRecordingMgr.h"
#include "Network/Squad/SquadManager.h"
#include "PlaylistManager.h"
#include "Utility/DesignerWarning.h"
#include "AI/GameAISystem.h"
#include "PersistantStats.h"
#include "Battlechatter.h"

#include "EquipmentLoadout.h"
#include <CrySystem/Profilers/IStatoscope.h>

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
#define DATA_CENTRE_DEFAULT_CONFIG                  "telemetry"
#define DOWNLOAD_MGR_DATA_CENTRE_DEFAULT_CONFIG     "download"
#else
#define DATA_CENTRE_DEFAULT_CONFIG									"telemetry"
#define DOWNLOAD_MGR_DATA_CENTRE_DEFAULT_CONFIG			"download"
#endif


#ifdef STATE_DEBUG
static void ChangeDebugState( ICVar* piCVar )
{
	const char* pVal = piCVar->GetString();
	CPlayer::DebugStateMachineEntity( pVal );
}
#endif

static void OnGameRulesChanged( ICVar * pCVar )
{
	CGameRules * pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
		pGameRules->UpdateGameRulesCvars();
}

//------------------------------------------------------------------------
static void BroadcastChangeSafeMode( ICVar * )
{
	SGameObjectEvent event(eCGE_ResetMovementController, eGOEF_ToExtensions);
	IEntitySystem * pES = gEnv->pEntitySystem;
	IEntityItPtr pIt = pES->GetEntityIterator();
	while (!pIt->IsEnd())
	{
		if (IEntity * pEnt = pIt->Next())
			if (IActor * pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEnt->GetId()))
				pActor->HandleEvent( event );
	}
}

static void BroadcastChangeHUDAdjust( ICVar * )
{
		g_pGame->GetUI()->GetLayoutManager()->UpdateHUDCanvasSize();
}

//------------------------------------------------------------------------
static void OnTelemetryConfigChanged(ICVar* pCVar)
{
	CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
	if(pCVar && pMgr)
	{
		pMgr->LoadEventConfig(pCVar->GetString());
	}
}

//------------------------------------------------------------------------
static void OnFOVChanged(ICVar* pPass)
{
	g_pGameCVars->cl_fov = clamp_tpl<float>(g_pGameCVars->cl_fov, 25.f, 80.f);
}

//------------------------------------------------------------------------
static void OnFOVMPChanged(ICVar* pPass)
{
	g_pGameCVars->cl_mp_fov_scalar = clamp_tpl<float>(g_pGameCVars->cl_mp_fov_scalar, 1.f, 1.45f);
}

//------------------------------------------------------------------------
static void OnSubtitlesChanged(ICVar* pCVar)
{
	CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnSubtitlesChanged));
}

//------------------------------------------------------------------------
static void OnPLRenderInNearestChanged(ICVar* pCVar = NULL)
{
	if(pCVar)
	{
		// Toggle draw nearest flag on player
		IActor* pLocalActor = gEnv->pGameFramework->GetClientActor();
		if(pLocalActor)
		{
			IEntity* pEntity = pLocalActor->GetEntity();
			if(pEntity)
			{
				int slotFlags = pEntity->GetSlotFlags(eIGS_FirstPerson);

				const bool bDrawNear = (pCVar->GetIVal()==1);
				if(bDrawNear && !pLocalActor->IsThirdPerson() && !g_pGameCVars->g_detachCamera)
				{
					slotFlags |= ENTITY_SLOT_RENDER_NEAREST;
				}
				else
				{
					slotFlags &= ~ENTITY_SLOT_RENDER_NEAREST;
				}
				pEntity->SetSlotFlags(eIGS_FirstPerson,slotFlags);
			}
		}
	}
}

//------------------------------------------------------------------------
static void OnDetachCameraChanged(ICVar* pCVar = NULL)
{
	// Turn off camera space rendering when the camera is detached
	IActor* pLocalActor = gEnv->pGameFramework->GetClientActor();
	if(pLocalActor)
	{
		IEntity* pEntity = pLocalActor->GetEntity();
		if(pEntity)
		{
			int slotFlags = pEntity->GetSlotFlags(eIGS_FirstPerson);

			if(g_pGameCVars->g_detachCamera || pLocalActor->IsThirdPerson() || !g_pGameCVars->pl_renderInNearest)
			{
				slotFlags &= ~ENTITY_SLOT_RENDER_NEAREST;
			}
			else
			{
				slotFlags |= ENTITY_SLOT_RENDER_NEAREST;
			}
			pEntity->SetSlotFlags(eIGS_FirstPerson,slotFlags);
		}
	}
}

#if USE_LAGOMETER
//------------------------------------------------------------------------
static void OnNetGraphChanged(ICVar* pCVar)
{
	CLagOMeter* pLagOMeter = g_pGame->GetLagOMeter();
	if (pLagOMeter)
	{
		pLagOMeter->UpdateBandwidthGraphVisibility();
	}
}
#endif

#if (USE_DEDICATED_INPUT)
//------------------------------------------------------------------------
static void OnDedicatedInputChanged(ICVar* pCVar)
{
	if (IActor* localActor = gEnv->pGameFramework->GetClientActor())
	{
		if (localActor->IsPlayer())
		{
			CPlayer* player = static_cast<CPlayer*>(localActor);
			player->CreateInputClass(true);
		}
	}
}
#endif


#if !defined(_RELEASE)

struct SAutoComplete_v_profile_graph : public IConsoleArgumentAutoComplete
{
	enum { count = 4 };
	static const char* s_names[count];
	// Gets number of matches for the argument to auto complete.
	virtual int GetCount() const { return count; }
	// Gets argument value by index, nIndex must be in 0 <= nIndex < GetCount()
	virtual const char* GetValue( int nIndex ) const { return s_names[nIndex]; }
};

const char* SAutoComplete_v_profile_graph::s_names[SAutoComplete_v_profile_graph::count] = 
{
	"slip-speed",
	"slip-speed-lateral",
	"centrif",
	"ideal-centrif",
};

static SAutoComplete_v_profile_graph s_auto_v_profile_graph;

#endif


//------------------------------------------------------------------------
static void OnMatchmakingVersionOrBlockChanged(ICVar* pCVar)
{
 // TODO: michiel
}

//------------------------------------------------------------------------
void SCVars::InitAIPerceptionCVars(IConsole *pConsole)
{
	REGISTER_CVAR(ai_perception.movement_useSurfaceType, 0, VF_CHEAT, "Toggle if surface type should be used to get the base radius instead of cvars");
	REGISTER_CVAR(ai_perception.movement_movingSurfaceDefault, 1.0f, VF_CHEAT, "Default value for movement speed effect on footstep radius (overridden by surface type)");
	REGISTER_CVAR(ai_perception.movement_standingRadiusDefault, 4.0f, VF_CHEAT, "Default value for standing footstep sound radius (overridden by surface type)");
	REGISTER_CVAR(ai_perception.movement_crouchRadiusDefault, 2.0f, VF_CHEAT, "Default value for crouching footstep sound radius multiplier (overridden by surface type)");
	REGISTER_CVAR(ai_perception.movement_standingMovingMultiplier, 2.5f, VF_CHEAT, "Multiplier for standing movement speed effect on footstep sound radius");
	REGISTER_CVAR(ai_perception.movement_crouchMovingMultiplier, 2.0f, VF_CHEAT, "Multiplier for crouched movement speed effect on footstep sound radius");

	REGISTER_CVAR(ai_perception.landed_baseRadius, 5.0f, VF_CHEAT, "Base radius for the AI sound generated when player lands");
	REGISTER_CVAR(ai_perception.landed_speedMultiplier, 1.5f, VF_CHEAT, "Multiplier applied to fall speed which is added on to the radius for the AI sound generated when player lands");
}

void DifficultyLevelChanged(ICVar* difficultyLevel)
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pSystem);
	CRY_ASSERT(g_pGame);

//	if (gEnv->pSystem->IsDevMode())
	{
		const int iCVarDifficulty = difficultyLevel->GetIVal();
		EDifficulty difficulty = (iCVarDifficulty >= eDifficulty_Default && iCVarDifficulty < eDifficulty_COUNT ? (EDifficulty)iCVarDifficulty : eDifficulty_Default);
		g_pGame->SetDifficultyLevel(difficulty);
	}
}

void ReloadPickAndThrowProxiesOnChange(ICVar* useProxies)
{
	CRY_ASSERT(g_pGame);

	g_pGame->GetIGameFramework()->GetIActorSystem()->Reload();
	gEnv->pGameFramework->GetISharedParamsManager()->RemoveByType(CPickAndThrowProxy::SPnTProxyParams::s_typeInfo);

	IActorIteratorPtr pIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	while (IActor* pIActor = pIt->Next())
	{
		if (pIActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pIActor);
			pPlayer->ReloadPickAndThrowProxy();
		}
	}
}

#if PAIRFILE_GEN_ENABLED
static void Generate1P3PPairFiles( IConsoleCmdArgs* )
{ 
	CAnimationProxyDualCharacterBase::Generate1P3PPairFile();
}
#endif //PAIRFILE_GEN_ENABLED

static void SetControllerLayout( IConsoleCmdArgs* pArgs )
{ 
	if (pArgs->GetArgCount() < 3)
		return;

	const char* layoutType = pArgs->GetArg(1);
	const char* layoutName = pArgs->GetArg(2);
	if (strcmp(layoutType, "button") == 0)
	{
		g_pGame->SetControllerLayouts(layoutName, g_pGame->GetControllerLayout(eControllerLayout_Stick));
	}
	else if (strcmp(layoutType, "stick") == 0)
	{
		g_pGame->SetControllerLayouts(g_pGame->GetControllerLayout(eControllerLayout_Button), layoutName);
	}
	else
	{
		GameWarning("SetControllerLayout: Invalid layoutType: %s", layoutType);
	}
}

void WeaponOffsetOutput(IConsoleCmdArgs* pArgs)
{
	gEnv->pLog->LogAlways("Right:");

	SWeaponOffset offset = CWeaponOffsetInput::Get()->GetRightDebugOffset();

	offset.m_rotation.x = RAD2DEG(offset.m_rotation.x);
	offset.m_rotation.y = RAD2DEG(offset.m_rotation.y);
	offset.m_rotation.z = RAD2DEG(offset.m_rotation.z);

	gEnv->pLog->LogAlways(
		"Recover Right: %f %f %f %f %f %f",
		offset.m_position.x, offset.m_position.y, offset.m_position.z,
		offset.m_rotation.x, offset.m_rotation.y, offset.m_rotation.z);

	gEnv->pLog->LogAlways(
		"Position: \"%f, %f, %f\"",
		offset.m_position.x, offset.m_position.y, offset.m_position.z);
	gEnv->pLog->LogAlways(
		"Angles: \"%f, %f, %f\"",
		offset.m_rotation.x, offset.m_rotation.y, offset.m_rotation.z);


	gEnv->pLog->LogAlways("Left:");

	offset = CWeaponOffsetInput::Get()->GetLeftDebugOffset();

	offset.m_rotation.x = RAD2DEG(offset.m_rotation.x);
	offset.m_rotation.y = RAD2DEG(offset.m_rotation.y);
	offset.m_rotation.z = RAD2DEG(offset.m_rotation.z);

	gEnv->pLog->LogAlways(
		"Recover Right: %f %f %f %f %f %f",
		offset.m_position.x, offset.m_position.y, offset.m_position.z,
		offset.m_rotation.x, offset.m_rotation.y, offset.m_rotation.z);

	gEnv->pLog->LogAlways(
		"Position: \"%f, %f, %f\"",
		offset.m_position.x, offset.m_position.y, offset.m_position.z);
	gEnv->pLog->LogAlways(
		"Angles: \"%f, %f, %f\"",
		offset.m_rotation.x, offset.m_rotation.y, offset.m_rotation.z);
}



void WeaponOffsetReset(IConsoleCmdArgs* pArgs)
{
	CWeaponOffsetInput::Get()->SetRightDebugOffset(SWeaponOffset(ZERO));
	CWeaponOffsetInput::Get()->SetLeftDebugOffset(SWeaponOffset(ZERO));
}


void WeaponOffsetInput(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() != 7)
	{
		gEnv->pLog->LogError("Invalid offset format, needs 6 floating point numbers");
		return;
	}

	Vec3 pos(ZERO);
	Ang3 ang(ZERO);

	pos.x = float(atof(pArgs->GetArg(1)));
	pos.y = float(atof(pArgs->GetArg(2)));
	pos.z = float(atof(pArgs->GetArg(3)));

	ang.x = float(DEG2RAD(atof(pArgs->GetArg(4))));
	ang.y = float(DEG2RAD(atof(pArgs->GetArg(5))));
	ang.z = float(DEG2RAD(atof(pArgs->GetArg(6))));

	CWeaponOffsetInput::Get()->SetRightDebugOffset(SWeaponOffset(pos, ang));
}



void WeaponOffsetToMannequin(IConsoleCmdArgs* pArgs)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(gEnv->pGameFramework->GetClientActor());
	if (!pPlayer)
		return;

	char cmdBuffer[256];

	SWeaponOffset offset = CWeaponOffsetInput::Get()->GetRightDebugOffset();
	if (pArgs->GetArgCount() == 2 && strcmp(pArgs->GetArg(1), "left")==0)
		offset = CWeaponOffsetInput::Get()->GetLeftDebugOffset();

	cry_sprintf(cmdBuffer, "e_mannequin_setkeyproperty position_x %f", offset.m_position.x);
	gEnv->pConsole->ExecuteString(cmdBuffer);
	cry_sprintf(cmdBuffer, "e_mannequin_setkeyproperty position_y %f", offset.m_position.y);
	gEnv->pConsole->ExecuteString(cmdBuffer);
	cry_sprintf(cmdBuffer, "e_mannequin_setkeyproperty position_z %f", offset.m_position.z);
	gEnv->pConsole->ExecuteString(cmdBuffer);

	cry_sprintf(cmdBuffer, "e_mannequin_setkeyproperty rotation_x %f", RAD2DEG(offset.m_rotation.x));
	gEnv->pConsole->ExecuteString(cmdBuffer);
	cry_sprintf(cmdBuffer, "e_mannequin_setkeyproperty rotation_y %f", RAD2DEG(offset.m_rotation.y));
	gEnv->pConsole->ExecuteString(cmdBuffer);
	cry_sprintf(cmdBuffer, "e_mannequin_setkeyproperty rotation_z %f", RAD2DEG(offset.m_rotation.z));
	gEnv->pConsole->ExecuteString(cmdBuffer);
}



CWeapon* GetLocalPlayerWeapon()
{
	CPlayer* pPlayer = static_cast<CPlayer*>(gEnv->pGameFramework->GetClientActor());
	if (!pPlayer)
		return 0;

	IItem* pICurrentItem = pPlayer->GetCurrentItem();
	if (!pICurrentItem)
		return 0;

	IWeapon* pIWeapon = pICurrentItem->GetIWeapon();
	return static_cast<CWeapon*>(pIWeapon);
}



void WeaponZoomIn(IConsoleCmdArgs* pArgs)
{
	CWeapon* pWeapon = GetLocalPlayerWeapon();
	if (!pWeapon)
		return;

	EntityId actorId = pWeapon->GetOwnerId();
	int zoomLevel = 1;
	pWeapon->StartZoom(actorId, zoomLevel);
}



void WeaponZoomOut(IConsoleCmdArgs* pArgs)
{
	CWeapon* pWeapon = GetLocalPlayerWeapon();
	if (!pWeapon)
		return;

	pWeapon->ExitZoom(true);
}


// game related cvars must start with an g_
// game server related cvars must start with sv_
// game client related cvars must start with cl_
// no other types of cvars are allowed to be defined here!
void SCVars::InitCVars(IConsole *pConsole)
{
	m_releaseConstants.Init( pConsole );

	REGISTER_CVAR(g_enableLanguageSelectionMenu,1,VF_READONLY,"Enable the language selection menu.");
	
	REGISTER_CVAR(g_ProcessOnlineCallbacks,1,VF_CHEAT|VF_READONLY,"Process online callbacks in the gamelobbymanager.");

	REGISTER_CVAR(g_multiplayerDefault,0,0,"Enable multiplayer gameplay by default");
	REGISTER_CVAR(g_multiplayerModeOnly,0,0,"Sets exclusively multiplayer. Will not allow switching to singleplayer");
	
	REGISTER_CVAR(g_frontEndUnicodeInput, 0, VF_NULL, "Enables unicode input");
	REGISTER_CVAR(g_EnableDevMenuOptions, 0, VF_REQUIRE_APP_RESTART|VF_CHEAT, "Enable DEV menu options");
	REGISTER_CVAR(g_EnablePersistantStatsDebugScreen, 1, VF_NULL, "Enable persistant stats debug screen (needs dev menu options enabled)");
	
	//Exclusive Playable Demo version
	REGISTER_CVAR(g_EPD, 0, VF_REQUIRE_APP_RESTART, "Enable Exclusive Playable Demo version");
	REGISTER_CVAR(g_frontEndRequiredEPD, 3, VF_REQUIRE_APP_RESTART, "If a UI element in the MenuData XML has 'EPD=true' set then the global g_EPD value will need to match this value for the element to be enabled");
	REGISTER_CVAR_CB(g_MatchmakingVersion, 5367 , VF_REQUIRE_APP_RESTART, "Defines your matchmaking version (Only join games over the same version)", OnMatchmakingVersionOrBlockChanged);
	REGISTER_CVAR_CB(g_MatchmakingBlock, 2008, VF_REQUIRE_APP_RESTART, "Used to shift matchmaking version for QA and EA builds - please leave as zero", OnMatchmakingVersionOrBlockChanged);
	REGISTER_CVAR(g_craigNetworkDebugLevel, 0, VF_REQUIRE_APP_RESTART|VF_CHEAT, "Craig network debug options");

	REGISTER_CVAR(g_presaleUnlock, 0, VF_REQUIRE_APP_RESTART|VF_CHEAT, "Cheat to unlock the presale content without voucher");
	REGISTER_CVAR(g_dlcPurchaseOverwrite, 0, VF_CHEAT, "Cheat to unlock DLC content on PC without purchase");
	REGISTER_CVAR(g_enableInitialLoadoutScreen,1,VF_CHEAT,"Enable the loadout screen when joining a game.");
	REGISTER_CVAR(g_post3DRendererDebug,0,VF_CHEAT,"Enable Post 3D Rendering debug screen. 1 = enable. 2 = draw grid");
	REGISTER_CVAR(g_post3DRendererDebugGridSegmentCount,10,VF_CHEAT,"Number of segments in Post 3D Rendering debug grid.");

#if !defined(_RELEASE)
	REGISTER_CVAR(g_debugTestProtectedScripts, 0, VF_CHEAT, "Enable this to make the game check protected script binds cannot be accessed when they are disabled" );
#endif //!defined(_RELEASE)

	//client cvars
	REGISTER_CVAR_CB(cl_fov, 55.0f, 0, "field of view.", OnFOVChanged);
	REGISTER_CVAR_CB(cl_mp_fov_scalar, 1.f, 0, "field of view scale (multiplayer)", OnFOVMPChanged);
	REGISTER_CVAR(cl_tpvCameraCollision, 1, 0, "enable camera collision in 3rd person view");
	REGISTER_CVAR(cl_tpvCameraCollisionOffset, 0.3f, 0, "offset that is used to move the 3rd person camera out of collisions");
	REGISTER_CVAR(cl_tpvDebugAim, 0, 0, "show aim debug helpers in 3rd person view");
	REGISTER_CVAR(cl_tpvDist, 2.0f, 0, "camera distance in 3rd person view");
	REGISTER_CVAR(cl_tpvDistHorizontal, 0.5f, 0, "horizontal camera distance in 3rd person view");
	REGISTER_CVAR(cl_tpvDistVertical, 0.0f, 0, "vertical camera distance in 3rd person view");
	REGISTER_CVAR(cl_tpvInterpolationSpeed, 5.0f, 0, "interpolation speed used when the camera hits an object");
	REGISTER_CVAR(cl_tpvMaxAimDist, 1000.0f, 0, "maximum range in which the player can aim at an object");
	REGISTER_CVAR(cl_tpvMinDist, 0.5f, 0, "minimum distance in 3rd person view");
	REGISTER_CVAR(cl_tpvYaw, 0, 0, "camera angle offset in 3rd person view");
	REGISTER_CVAR(cl_sensitivity, 30.0f, VF_DUMPTODISK, "Set mouse sensitivity!");
	REGISTER_CVAR(cl_sensitivityController, 0.8f, VF_DUMPTODISK, "Set controller sensitivity! Expecting 0.0f to 2.0f");
	REGISTER_CVAR(cl_sensitivityControllerMP, 1.1f, VF_DUMPTODISK, "Set controller sensitivity! Expecting 0.0f to 2.0f");
	REGISTER_CVAR(cl_invertMouse, 0, VF_DUMPTODISK, "mouse invert?");
	REGISTER_CVAR(cl_invertController, 0, VF_DUMPTODISK, "Controller Look Up-Down invert");
	REGISTER_CVAR(cl_invertLandVehicleInput, 0, VF_DUMPTODISK, "Controller Look Up-Down invert for land vehicles (and amphibious vehicles)");
	REGISTER_CVAR(cl_invertAirVehicleInput, 0, VF_DUMPTODISK, "Controller Look Up-Down invert for flying vehicles");
	REGISTER_CVAR(cl_zoomToggle, 0, VF_DUMPTODISK, "To make the zoom key work as a toggle");
	REGISTER_CVAR(cl_crouchToggle, 1, VF_DUMPTODISK, "To make the crouch key work as a toggle");
	REGISTER_CVAR(cl_sprintToggle, 1, VF_DUMPTODISK, "To make the sprint key work as a toggle");
	REGISTER_CVAR(cl_logAsserts, 0, 0, "1 = log assert only. 2 = log assert and callstack");
	REGISTER_CVAR_CB(hud_canvas_width_adjustment, 1.0f, 0, "MP only. Multiplies the width of the HUD's virtual canvas in cases where it may overlap monitor boundaries in multi-monitor setups. Not that, before this multiplier is applied, the HUD clamps itself to a 16:9 res.", BroadcastChangeHUDAdjust);

	// This is necessary since the cvar won't have been registered when system.cfg is parsed
	// and a temporary string/value pair will have been cached by XConsole when parsing the
	// system.cfg.  The cached value will be applied when the cvar is registered above,
	// bypassing this OnChanged callback because there's no way to associate an OnChanged
	// callback at cvar registration...
	OnFOVChanged(NULL);
	OnFOVMPChanged(NULL);

	// Another necessary forced cvar update due to OnChanged callback not being called when msaa is registered
	ICVar* pAntialiasingModeCVar = pConsole->GetCVar("r_AntialiasingMode");
	if (pAntialiasingModeCVar)
	{
		const int iCurValue = pAntialiasingModeCVar->GetIVal();
		pAntialiasingModeCVar->Set(0);
		pAntialiasingModeCVar->Set(iCurValue);
	}

	REGISTER_CVAR(g_infiniteAmmoTutorialMode, 0, 0, "Enables infinite inventory ammo for the tutorial.");
		
	REGISTER_CVAR(i_lighteffects, 1, VF_DUMPTODISK, "Enable/Disable lights spawned during item effects.");
	REGISTER_CVAR(i_particleeffects,	1, VF_DUMPTODISK, "Enable/Disable particles spawned during item effects.");
	REGISTER_CVAR(i_rejecteffects, 1, VF_DUMPTODISK, "Enable/Disable ammo reject effects during weapon firing.");
	REGISTER_CVAR(i_ironsight_while_jumping_mp, 1, 0, "Whether players can use weapon ironsights whilst jumping");
	REGISTER_CVAR(i_ironsight_while_falling_mp, 1, 0, "Whether players can use weapon ironsights whilst falling (Only if whilst jumping is disabled)");
	REGISTER_CVAR(i_ironsight_falling_unzoom_minAirTime, 0.8f, 0, "If no zooming is allowed whilst falling then unzoom after you've fallen for this amount of time");
	REGISTER_CVAR(i_weapon_customisation_transition_time, 1.3f, 0, "The time taken to enter/exit the in fiction customization menu");
	REGISTER_CVAR(i_highlight_dropped_weapons, 2, 0, "Weapons on the ground should be highlighted. 0 = Off, 1 = Heavy weapons only, 2 = All weapons");
	
	REGISTER_CVAR(cl_speedToBobFactor, 0.35f, 0, "Frequency of bobbing effect based on players speed");
	REGISTER_CVAR(cl_bobWidth, 0.025f, 0, "Desired amount of horizontal camera movement while the player is moving");
	REGISTER_CVAR(cl_bobHeight, 0.04f, 0, "Desired amount of vertical camera movement while the player is moving. Value cannot be larger than cl_bobMaxHeight");
	REGISTER_CVAR(cl_bobSprintMultiplier, 1.5f, 0, "Multiplier to add additional camera bobbing while player is sprinting");
	REGISTER_CVAR(cl_bobVerticalMultiplier, 4.0f, 0, "Multiplier to add additional camera bobbing while the player is moving vertically");
	REGISTER_CVAR(cl_bobMaxHeight, 0.08f, 0, "Clamps the maximum amount of vertical camera movement while the player is moving at maximum speed");
	REGISTER_CVAR(cl_strafeHorzScale, 0.05f, 0, "Desired amount of horizontal camera movement while the player is strafing");
	REGISTER_CVAR(cl_controllerYawSnapEnable, 0, 0, "Enable snap rotation with controller/mouse when using a head mounted display");
	REGISTER_CVAR(cl_controllerYawSnapAngle, 30.0f, 0, "Snap amount in degrees");
	REGISTER_CVAR(cl_controllerYawSnapTime, 0.1f, 0, "Snap time in seconds");
	REGISTER_CVAR(cl_controllerYawSnapMax, 0.8f, 0, "Input threshold that must be reached to trigger snapping");
	REGISTER_CVAR(cl_controllerYawSnapMin, 0.5f, 0, "Input threshold that must be reached to reset snapping");

	REGISTER_CVAR(i_grenade_showTrajectory, 0, 0, "Switches on trajectory display");
	REGISTER_CVAR(i_grenade_trajectory_resolution, 0.03f, 0, "Trajectory display resolution");
	REGISTER_CVAR(i_grenade_trajectory_dashes, 0.5f, 0, "Trajectory display dashes length");
	REGISTER_CVAR(i_grenade_trajectory_gaps, 0.3f, 0, "Trajectory gaps length");
	REGISTER_CVAR(i_grenade_trajectory_useGeometry, 1, 0, "Use real geometry instead of AuxGeom to render the trajectory");

	REGISTER_CVAR(i_laser_hitPosOffset, 0.1f, 0, "Distance from hit to position the aim dot");

	REGISTER_CVAR(i_failedDetonation_speedMultiplier, 0.15f, 0, "Multiplier to current velocity upon a failed detonation");
	REGISTER_CVAR(i_failedDetonation_lifetime, 1.f, 0, "Length of time from failed detonation until deletion");

	REGISTER_CVAR(i_hmg_detachWeaponAnimFraction, 0.55f, 0, "Fraction of time through deslection animation to physicalize and detach weapon");
	REGISTER_CVAR(i_hmg_impulseLocalDirection_x, 1.f, 0, "x value for local impulse direction");
	REGISTER_CVAR(i_hmg_impulseLocalDirection_y, 0.f, 0, "y value for local impulse direction");
	REGISTER_CVAR(i_hmg_impulseLocalDirection_z, 0.f, 0, "z value for local impulse direction");

	REGISTER_CVAR(cl_motionBlurVectorScale, 1.5f, VF_CHEAT, "Default motion blur vector scale");
	REGISTER_CVAR(cl_motionBlurVectorScaleSprint, 3.0f, VF_CHEAT, "Motion blur vector scale while sprinting");

	REGISTER_CVAR(cl_slidingBlurRadius, 0.8f, VF_CHEAT, "Blur radius during slide (proportion of the screen)");
	REGISTER_CVAR(cl_slidingBlurAmount, 0.2f, VF_CHEAT, "Blur amount during slide");
	REGISTER_CVAR(cl_slidingBlurBlendSpeed, 0.4f, VF_CHEAT, "Blur blend-in time during slide");
	
	REGISTER_CVAR(cl_debugSwimming, 0, VF_CHEAT, "enable swimming debugging");

	REGISTER_CVAR(cl_angleLimitTransitionTime, 0.2f, VF_CHEAT, "Transition time when applying up/down angle limits");

	ca_GameControlledStrafingPtr = pConsole->GetCVar("ca_GameControlledStrafing");

	REGISTER_CVAR(cl_shallowWaterSpeedMulPlayer, 0.6f, 0, "shallow water speed multiplier (Players only)");
	REGISTER_CVAR(cl_shallowWaterSpeedMulAI, 0.8f, VF_CHEAT, "Shallow water speed multiplier (AI only)");
	REGISTER_CVAR(cl_shallowWaterDepthLo, 0.5f, 0, "Shallow water depth low (below has zero slowdown)");
	REGISTER_CVAR(cl_shallowWaterDepthHi, 1.2f, 0, "Shallow water depth high (above has full slowdown)");

	REGISTER_CVAR(cl_idleBreaksDelayTime, 8.0f, VF_CHEAT, "Time delay between idle breaks");

	REGISTER_CVAR(cl_postUpdateCamera, 2, VF_CHEAT, "Apply post animation updates to the camera (0 = none, 1 = position, 2 = position&rotation)");

	REGISTER_CVAR(p_collclassdebug, 0, VF_CHEAT, "Show debug info about CollisionClass data for physics objects.");

	REGISTER_CVAR(pl_cameraTransitionTime, 0.25f, VF_CHEAT, "Time over which to transition the camera between modes");
	REGISTER_CVAR(pl_cameraTransitionTimeLedgeGrabIn, 0.0f, VF_CHEAT, "Time over which to transition into the animated camera mode going into ledge grab");
	REGISTER_CVAR(pl_cameraTransitionTimeLedgeGrabOut, 0.25f, VF_CHEAT, "Time over which to transition into the animated camera mode going out of the ledge grab");

	REGISTER_CVAR(pl_inputAccel, 30.0f, 0, "Movement input acceleration");
	REGISTER_CVAR(pl_debug_energyConsumption, 0, VF_CHEAT, "Display debug energy consumption rate info");
	REGISTER_CVAR(pl_debug_pickable_items, 0, 0, "Display information about pickable item at which the player is about to interact with");

	REGISTER_CVAR(pl_nanovision_timeToRecharge, 10.0f, 0, "Time it takes nanovision energy to recharge to full from zero");
	REGISTER_CVAR(pl_nanovision_timeToDrain,		5.0f, 0, "Time it takes nanovision energy to drain from full to zero");
	REGISTER_CVAR(pl_nanovision_minFractionToUse, 0.25f, 0, "The minimum fraction of nano vision energy you need to have recharged to re-engage it");	

	REGISTER_CVAR(pl_debug_projectileAimHelper, 0, VF_CHEAT, "Enables debug info for any active projectile aim assist");

	REGISTER_CVAR(pl_useItemHoldTime, 0.3f, VF_CHEAT, "hold time to pick up an item");
	REGISTER_CVAR(pl_autoPickupItemsWhenUseHeld, 1, 0, "When holding down the use button and moving over an item the item will be used");
	REGISTER_CVAR(pl_autoPickupMinTimeBetweenPickups, 2.f, 0, "After auto-picking an item the system will be disabled for this many seconds");

	REGISTER_CVAR(pl_refillAmmoDelay, 0.8f, VF_CHEAT, "When refilling ammo from an ammo box, the weapon will be lowered and disabled for this time");

	REGISTER_CVAR(pl_spawnCorpseOnDeath, 1, 0, "Clone dying players to create longer-lasting corpses (in multiplayer)");

	REGISTER_CVAR(pl_doLocalHitImpulsesMP, 1, 0, "Do local hit impulses in multiplayer");
	
#if !defined(_RELEASE)
	REGISTER_CVAR(pl_watch_projectileAimHelper, 0, VF_CHEAT, "Enables debug watch info for any active projectile aim assist");
#endif
	//

#if !defined(_RELEASE)
	REGISTER_CVAR(g_preloadUIAssets, 1, 0, "UI Flash Assets won't be preloaded, if this is set to 0. This will cause invalid file accesses at runtime.");

	REGISTER_CVAR(g_gameRules_skipStartTimer,									0,    0, "Skip the 15 second countdown?");
#endif
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	g_gameRules_startCmdCVar = REGISTER_STRING("g_gameRules_StartCmd", "", 0, "Console command to run when MP game rules transition to Start Game");
#endif
	REGISTER_CVAR(g_gameRules_startTimerLength,            10.f,    0, "The N in 'Game will start in N seconds' (triggered once there are enough players)");

	REGISTER_CVAR(g_gameRules_postGame_HUDMessageTime, 3.f, 0, "How long to show the endgame HUD message for");
	REGISTER_CVAR(g_gameRules_postGame_Top3Time, 7.f, 0, "How long to show the top 3 scoreboard for");
	REGISTER_CVAR(g_gameRules_postGame_ScoreboardTime, 7.f, 0, "How long to show the full scoreboard for");

	REGISTER_CVAR(g_gameRules_startTimerMinPlayers,					2,  0, "Min number of players to trigger countdown (deathmatch mode)");
	REGISTER_CVAR(g_gameRules_startTimerMinPlayersPerTeam,	1,	0, "Min number of players on each team to trigger countdown (team mode)");
	REGISTER_CVAR(g_gameRules_startTimerPlayersRatio,				1.0f,	0, "Percentage of players loaded to trigger countdown (0.0f -> 1.0f)");
	REGISTER_CVAR(g_gameRules_startTimerOverrideWait,				20.f, 0, "Override time in seconds to trigger countdown without min player limits met");
	REGISTER_CVAR(g_gameRules_preGame_StartSpawnedFrozen,			1, 0, "Enable to start the gamemode with the player spawned but controls frozen. If disabled then they spawn when the start countdown has finished.");

	REGISTER_CVAR(g_flashBangMinSpeedMultiplier, 0.2f, VF_CHEAT, "Set the minimum movement and rotation speed multiplier when stunned by a flashbang");
	REGISTER_CVAR(g_flashBangSpeedMultiplierFallOffEase, 5.f, VF_CHEAT, "Alters the falloff curve for the flashbang multiplier");
	REGISTER_CVAR(g_flashBangNotInFOVRadiusFraction, 0.66f, VF_CHEAT, "Set the radius fraction which will still blind a player even if not looking at a flashbang");
	REGISTER_CVAR(g_flashBangMinFOVMultiplier, 0.75f, VF_CHEAT, "Set the minimum multiplier for the dot product comparison");
#ifndef _RELEASE
	REGISTER_CVAR(g_flashBangFriends, 0, VF_CHEAT, "Applies flash bangs to friends");
	REGISTER_CVAR(g_flashBangSelf, 1, VF_CHEAT, "Applies flash bangs to your self");
#endif
	REGISTER_CVAR(g_friendlyLowerItemMaxDistance, 10.0f, VF_CHEAT, "Max distance a friendly AI needs to be before the player will lower his weapon.");
	
	REGISTER_CVAR(g_holdObjectiveDebug, 0, VF_CHEAT, "Debugging for hold objective  1. Draw debug cylinder.  2. Fade ring in on pulse");
	
	REGISTER_CVAR2_CB("g_difficultyLevel", &g_difficultyLevel, 2, VF_NULL, "Difficulty level", DifficultyLevelChanged);
	REGISTER_CVAR(g_difficultyLevelLowestPlayed, -1, VF_NULL, "Sets the lowest difficulty played (Used in completion for achievement determination)(Value is set to -1 in difficulty selection screen, becomes lowest diffculty 1,2,3,4 (Easy,Normal,Hard,Supersolder) when setting difficulty");
	REGISTER_CVAR(g_enableFriendlyFallAndPlay, 0, 0, "Enables fall&play feedback for friendly actors.");
	REGISTER_CVAR(g_enableFriendlyAIHits, 0, VF_CHEAT, "Enables AI-owning bullet hit feedback for friendly actors.");
	REGISTER_CVAR(g_enableFriendlyPlayerHits, 1, VF_CHEAT, "Enables Player-owning bullet hit feedback for friendly actors.");

	REGISTER_CVAR(g_gameRayCastQuota, 16, VF_CHEAT, "Amount of deferred rays allowed to be cast per frame by Game");
	REGISTER_CVAR(g_gameIntersectionTestQuota, 6, VF_CHEAT, "Amount of deferred intersection tests allowed to be cast per frame by Game");

	REGISTER_CVAR(g_STAPCameraAnimation, 1, VF_CHEAT, "Enable STAP camera animation");
	
	REGISTER_CVAR(g_mpAllSeeingRadar, 0, VF_READONLY,	"Player has radar that permanently shows all friends and enemies (for Attackers in Assault mode).");
	REGISTER_CVAR(g_mpAllSeeingRadarSv, 0, VF_READONLY,	"All players have radar that permanently shows all friends and enemies, controlled by server.");
	REGISTER_CVAR(g_mpDisableRadar, 0, VF_READONLY,	"Player has no radar (for Defenders when starting Assault mode).");
	REGISTER_CVAR(g_mpNoEnemiesOnRadar, 0, VF_READONLY,	"Player will not see any enemies on radar (for Attackers when Defenders are at high alert level in Assault mode).");
	REGISTER_CVAR(g_mpHatsBootsOnRadar, 1, VF_READONLY,	"specifies if players will see any hats/boots (indicating higher/lower positioning) on radar");
		
	REGISTER_CVAR(g_playerLowHealthThreshold, 20.0f, VF_NULL, "The player health threshold when the low health effect kicks in.");
	REGISTER_CVAR(g_playerMidHealthThreshold, 40.0f, VF_NULL, "The player health threshold when the mid health feedback kicks in.");

	REGISTER_CVAR(g_SurvivorOneVictoryConditions_watchLvl, 0, VF_CHEAT, "Debug watch level for the SurvivorOneVictoryConditions gamerules module.");
	REGISTER_CVAR(g_SimpleEntityBasedObjective_watchLvl, 0, VF_CHEAT, "Debug watch level for the SimpleEntityBasedObjective gamerules module.");
	REGISTER_CVAR(g_KingOfTheHillObjective_watchLvl, 0, VF_CHEAT, "Debug watch level for the KingOfTheHill objective implementation.");
	REGISTER_CVAR(g_HoldObjective_secondsBeforeStartForSpawn, 8.0f, VF_CHEAT, "The first crash site starts coming in this many seconds before the start of the game.");

	REGISTER_CVAR(g_CombiCaptureObjective_watchLvl, 0, VF_CHEAT, "Debug watch level for the CombiCaptureObjective gamerules module.");
	REGISTER_CVAR(g_CombiCaptureObjective_watchTerminalSignalPlayers, 0, VF_CHEAT, "Watch the audio signal players attached to the objective terminals in the CombiCaptureObjective gamerules module.");

	REGISTER_CVAR(g_disable_OpponentsDisconnectedGameEnd, 0, VF_CHEAT, "Stop the game from automatically ending when all opponents have disconnected. Useful for debugging failed migrations, etc.");
	REGISTER_CVAR(g_victoryConditionsDebugDrawResolution, 0, VF_CHEAT, "Debug the current draw resolution stats on the server");

#if USE_PC_PREMATCH
	REGISTER_CVAR(g_restartWhenPrematchFinishes, 2, VF_NULL, "0: Disables level restart when the PC Prematch finishes; 1: Full restart; 2: Only respawn players");
#endif	

#ifndef _RELEASE
	REGISTER_CVAR(g_predator_forceSpawnTeam, 0, 0, "Force teams in predator gamemode - 1=Soldier, 2=Predator");
	REGISTER_CVAR(g_predator_predatorHasSuperRadar, 1, 0, "Give the predators a permanent radar");
	g_predator_forcePredator1 = REGISTER_STRING("g_predator_forcePredator1", "", 0, "Force predator to be a specific player");
	g_predator_forcePredator2 = REGISTER_STRING("g_predator_forcePredator2", "", 0, "Force predator to be a specific player");
	g_predator_forcePredator3 = REGISTER_STRING("g_predator_forcePredator3", "", 0, "Force predator to be a specific player");
	g_predator_forcePredator4 = REGISTER_STRING("g_predator_forcePredator4", "", 0, "Force predator to be a specific player");

#endif
	REGISTER_CVAR(g_predator_marineRedCrosshairDelay, 0.0001f, 0, "Overrides hud_tagnames_EnemyTimeUntilLockObtained for marine team in hunter game mode");

	REGISTER_CVAR(g_bulletPenetrationDebug, 0, VF_CHEAT, "Enable bullet penetration debugging");
	REGISTER_CVAR(g_bulletPenetrationDebugTimeout, 8.0f, VF_CHEAT, "Display time of debug messages");

	REGISTER_CVAR(g_fpDbaManagementEnable, 1, 0, "Enable/Disable fp dbas load/unload management");
	REGISTER_CVAR(g_fpDbaManagementDebug, 0, VF_CHEAT, "Debug information about currently loaded fp dbas");
#ifndef _RELEASE
	REGISTER_CVAR(g_charactersDbaManagementEnable, 1, VF_CHEAT, "Enable/Disable character dbas load/unload management");
	REGISTER_CVAR(g_charactersDbaManagementDebug, 0, VF_CHEAT, "Debug information about currently loaded character dbas");
#endif

	REGISTER_CVAR(g_thermalVisionDebug, 0, VF_CHEAT, "Enable/Disable debugging info on entities with a heat controller");

	REGISTER_CVAR(g_droppedItemVanishTimer, 45.f, VF_NULL, "Number of seconds before dropped items vanish");
	REGISTER_CVAR(g_droppedHeavyWeaponVanishTimer, 30.f, VF_NULL, "Number of seconds before dropped heavy weapons vanish");

	REGISTER_CVAR(g_corpseManager_maxNum, 6, VF_NULL, "Limit for number of corpses active at any time");
	REGISTER_CVAR(g_corpseManager_thermalHeatFadeDuration, 20.0f, VF_NULL, "Time Duration it takes for corpses heat to fade in thermal vision mode");
	REGISTER_CVAR(g_corpseManager_thermalHeatMinValue, 0.22f, VF_NULL, "Min heat value in thermal vision mode");
	REGISTER_CVAR(g_corpseManager_timeoutInSeconds, 0.0f, VF_NULL, "Number of seconds to keep corpses around before removing them. A value <= 0 means: never remove due to timeout.");

	REGISTER_CVAR(g_explosion_materialFX_raycastLength, 1.f, VF_CHEAT, "Length of raycast for non-direct impact explosions to find appropriate surface effect");

	// explosion culling
	REGISTER_CVAR(g_ec_enable, 1, VF_CHEAT, "Enable/Disable explosion culling of small objects. Default = 1");
	REGISTER_CVAR(g_ec_radiusScale, 2.0f, VF_CHEAT, "Explosion culling scale to apply to explosion radius for object query.");
	REGISTER_CVAR(g_ec_volume, 0.75f, VF_CHEAT, "Explosion culling volume which needs to be exceed for objects to not be culled.");
	REGISTER_CVAR(g_ec_extent, 2.0f, VF_CHEAT, "Explosion culling length of an AABB side which needs to be exceed for objects to not be culled.");
	REGISTER_CVAR(g_ec_removeThreshold, 20, VF_CHEAT, "At how many items in exploding area will it start removing items.");
	REGISTER_CVAR(g_radialBlur, 1.0f, VF_CHEAT, "Radial blur on explosions. Default = 1, 0 to disable");

	REGISTER_CVAR(g_aiCorpses_DebugDraw, 0, VF_CHEAT, "Enable AI corpse debugging");
	REGISTER_CVAR(g_aiCorpses_DelayTimeToSwap, 10.0f, 0, "Time in seconds the Ai will remain on the ground before being swap for a corpse");
	
	REGISTER_CVAR(g_aiCorpses_Enable, 1, 0, "Enable AI corpse spawning");
	REGISTER_CVAR(g_aiCorpses_MaxCorpses, 24, 0, "Max number of corpses allowed");
	REGISTER_CVAR(g_aiCorpses_CullPhysicsDistance, 50.0f, 0, "Corpses at this distance from the player will have their physics disabled");
	REGISTER_CVAR(g_aiCorpses_ForceDeleteDistance, 250.0f, 0, "Corpses at this distance will be removed as soon as not visible");

	REGISTER_CVAR(g_debugaimlook, 0, VF_CHEAT, "Debug aim/look direction");

	// Crysis supported gamemode CVars
	REGISTER_CVAR_CB(g_timelimit, 60.0f, 0, "Duration of a time-limited game (in minutes). 0 means no time-limit.", OnGameRulesChanged);
	REGISTER_CVAR(g_timelimitextratime, 0.0f, VF_READONLY, "TEMPORARILY READ-ONLY UNTIL FRONTEND CAN SET THIS. Amount of time to use as Extra Time/Sudden Death Time (in minutes) for MP game modes that support it. Default is 0, 0 means no extra time.");
	REGISTER_CVAR(g_roundScoreboardTime, 5.f, 0, "Time spent on the end of round scoreboard (in seconds).");
	REGISTER_CVAR(g_roundStartTime, 10.f, 0, "Time between the round scoreboard being removed and the next round starting (in seconds).");
	REGISTER_CVAR_CB(g_roundlimit, 30, 0, "Maximum numbers of rounds to be played. Default is 0, 0 means no limit.", OnGameRulesChanged);
	REGISTER_CVAR(g_friendlyfireratio, 0.0f, 0, "Sets friendly damage ratio.");
	REGISTER_CVAR(g_revivetime, 7, 0, "Revive wave timer.");
	REGISTER_CVAR(g_minplayerlimit, DEFAULT_MINPLAYERLIMIT, VF_NET_SYNCED, "Minimum number of players to start a match.");
	REGISTER_CVAR(g_tk_punish, 1, 0, "Turns on punishment for team kills");
	REGISTER_CVAR(g_tk_punish_limit, 5, 0, "Number of team kills user will be banned for");
	REGISTER_CVAR_CB(g_scoreLimit, 3, 0, "Max number of points need to win in a team game.  Default is 3", OnGameRulesChanged);
	REGISTER_CVAR_CB(g_scoreLimitOverride, 0, 0, "Override for score limit - because score limit is set by the variant", OnGameRulesChanged);
	REGISTER_CVAR(g_hostMigrationResumeTime, 3.0f, VF_CHEAT, "Time after players have rejoined before the game resumes");
#ifndef _RELEASE
	REGISTER_CVAR(g_hostMigrationUseAutoLobbyMigrateInPrivateGames, 0, VF_CHEAT, "1=Make calls to EnsureBestHost when in private games");
#endif
	REGISTER_CVAR(g_mpRegenerationRate, 0, 0, "Health regeneration rate (0=slow, 1=normal, 2=fast)");
	REGISTER_CVAR(g_mpHeadshotsOnly, 0, 0, "Only allow damage from headshots");
	REGISTER_CVAR(g_mpNoVTOL, 0, 0, "Disable the VTOL");
	REGISTER_CVAR(g_mpNoEnvironmentalWeapons, 0, 0, "Disable environmental weapons");
	REGISTER_CVAR(g_allowCustomLoadouts, 1, 0, "Allow players to use custom loadouts");
	REGISTER_CVAR(g_allowFatalityBonus, 1, 0, "Allow players to gain fatality bonuses, 1=enable, 2=force");
	
	// do not access directly use GetAutoReviveTimeScaleForTeam() instead
	// [pb] Also be aware there is a default that will override this in  PlaylistManager.cpp ~Ln426 AddGameModeOptionFloat("g_autoReviveTime", "MP/Options/%s/RespawnDelay", true, 1.f, 1.f, XXXX, 0);
	REGISTER_CVAR(g_autoReviveTime, 9.f, 0, "Time from death till the player is automatically revived"); 

	REGISTER_CVAR(g_spawnPrecacheTimeBeforeRevive, 1.5f, 0, "This long before revive, start precaching expected spawnpoints.");
	REGISTER_CVAR(g_spawn_timeToRetrySpawnRequest, 0.4f, 0, "Send spawn requests to the server this many seconds apart");
	REGISTER_CVAR(g_spawn_recentSpawnTimer, 2.5f, 0, "How recently a spawn has to have occured for it to be considered 'recent' and result in a subsequent spawn being denied.");
	REGISTER_CVAR(g_forcedReviveTime, 14.f, 0, "Time from death till the player is forced to revive by the server, without any client request being received");
	REGISTER_CVAR(g_numLives, 3, 0, "Number of lives in assault and all or nothing gamemodes");
	REGISTER_CVAR(g_autoAssignTeams, 1, 0, "1 = auto assign teams, 0 = players choose teams");
	REGISTER_CVAR(g_maxHealthMultiplier, 1.f, 0, "Player health multiplier");
	REGISTER_CVAR(g_mp_as_DefendersMaxHealth, 180, 0, "Max health for Defenders in MP Assault gamemode");
	REGISTER_CVAR(g_xpMultiplyer, 1.f, 0, "XP multiplyer (for promotions)");
	REGISTER_CVAR(g_allowExplosives, 1, 0, "Allow players to use explosives");
	REGISTER_CVAR(g_forceWeapon, -1, 0, "Force the loadouts to all use the same weapon, -1=disable");
	REGISTER_CVAR(g_spawn_explosiveSafeDist, 7.0f, 0, "minimum distance between new spawnpoint and any explosives");
	REGISTER_CVAR(g_allowSpectators, 0, 0, "whether players can join the game as pure spectators");
	REGISTER_CVAR(g_infiniteCloak, 0, 0, "whether players are locked in stealth mode");
	REGISTER_CVAR(g_allowWeaponCustomisation, 1, 0, "whether players can use the in-game weapon customisation menu");

	REGISTER_CVAR(g_switchTeamAllowed, 1, 0, "Allow players to switch teams when the teams are unbalanced");
	REGISTER_CVAR(g_switchTeamRequiredPlayerDifference, 2, 0, "Minimum difference between team counts to allow team switching");
	REGISTER_CVAR(g_switchTeamUnbalancedWarningDifference, 2, 0, "Difference between team counts before the unbalanced game warning kicks in");
	REGISTER_CVAR(g_switchTeamUnbalancedWarningTimer, 30.f, 0, "Time in seconds between unbalanced game warnings");

	g_forceHeavyWeapon = REGISTER_STRING("g_forceHeavyWeapon", "", 0, "Force players to use a specific heavy weapon");
	g_forceLoadoutPackage = REGISTER_STRING("g_forceLoadoutPackage", "", 0, "Force players to use a specific loadout package");

	REGISTER_CVAR(g_logPrimaryRound, 0, 0, "Log various operations and calculations concerning the \"primary\" round and round changes. FOR DEBUGGING");

#if USE_REGION_FILTER
	REGISTER_CVAR(g_server_region, 0, 0, "Server region");
#endif

#if TALOS
	pl_talos = REGISTER_STRING( "pl_talos", "", VF_CHEAT | VF_NET_SYNCED, "String for Talos system" );
#endif


	REGISTER_CVAR(g_maxGameBrowserResults, 50, VF_NULL, "Maximum number of servers returned in game browser");

	REGISTER_CVAR(g_enableInitialLoginSilent, 1, VF_NULL, "1=Attempt silent login on startup");

	REGISTER_CVAR(g_inventoryNoLimits, 0, VF_CHEAT, "Removes inventory slots restrictions. Allows carrying as many weapons as desired");
	REGISTER_CVAR(g_inventoryWeaponCapacity, 2, VF_CHEAT, "Capacity for weapons slot in inventory");
	REGISTER_CVAR(g_inventoryExplosivesCapacity, 1, VF_CHEAT, "Capacity for explosives slot in inventory");
	REGISTER_CVAR(g_inventoryGrenadesCapacity, 3, VF_CHEAT, "Capacity for grenades slot in inventory");
	REGISTER_CVAR(g_inventorySpecialCapacity, 1, VF_CHEAT, "Capacity for special slot in inventory");

#if !defined(_RELEASE)
	REGISTER_CVAR(g_keepMPAudioSignalsPermanently, 0, VF_CHEAT, "If MP-specific audio signals are in memory only when needed (0) or all the time (1)");
#endif

#if !defined(_RELEASE)
	REGISTER_CVAR(g_debugShowGainedAchievementsOnHUD, 0, 0, "When an achievement is/would be given, the achievement name is shown on the HUD");
#endif

	REGISTER_CVAR(g_hmdFadeoutNearWallThreshold, 0.0f, VF_NULL, "Distance from the wall when fadeout to black starts (0 = disabled)");

	REGISTER_CVAR(g_debugNetPlayerInput, 0, VF_NULL, "Show some debug for player input");
	REGISTER_CVAR(g_debug_fscommand, 0, 0, "Print incoming fscommands to console");
	REGISTER_CVAR(g_skipIntro, 0, VF_NULL, "Skip all the intro videos.");
	REGISTER_CVAR(g_skipAfterLoadingScreen, 0, VF_NULL, "Automatically skip the ready screen, that is shown after level loading in singleplayer.");
	REGISTER_CVAR(g_goToCampaignAfterTutorial, 1, VF_NULL, "After finishing the tutorial continue to campaign.");
	
	REGISTER_CVAR(kc_enable, 1, VF_REQUIRE_APP_RESTART, "Enables the KillCam");
#ifndef _RELEASE
	REGISTER_CVAR_DEV_ONLY(kc_debug, 0, VF_NULL, "Enables debugging of the KillCam with the P key");
	REGISTER_CVAR_DEV_ONLY(kc_debugStressTest, 0, VF_NULL, "Enables stress testing of the killcam.");
	REGISTER_CVAR_DEV_ONLY(kc_debugVictimPos, 0, VF_NULL, "Enables debug drawing of the victim position in KillCam");
	REGISTER_CVAR_DEV_ONLY(kc_debugWinningKill, 0, VF_NULL, "Treats every kill as the winning kill");
	REGISTER_CVAR_DEV_ONLY(kc_debugSkillKill, 0, VF_NULL, "Treats every kill as a skill kill");
	REGISTER_CVAR_DEV_ONLY(kc_debugMannequin, 0, VF_NULL, "Dumps out additional information about each of the replay actors");
	REGISTER_CVAR_DEV_ONLY(kc_debugPacketData, 0, VF_NULL, "Logs all the recordingpackets used to playback a Kill Replay or Highlight.");
	REGISTER_CVAR_DEV_ONLY(kc_debugStream, 0, VF_NULL, "Display Stream Data information.");
#endif
	REGISTER_CVAR_DEV_ONLY(kc_memStats, 0, VF_NULL, "Shows memory statistics of the KillCam buffers");
	REGISTER_CVAR_DEV_ONLY(kc_length, 4.f, VF_NULL, "Sets the killcam replay length (in seconds)");
	REGISTER_CVAR_DEV_ONLY(kc_skillKillLength, 4.f, VF_NULL, "Sets the killcam replay length (in seconds)");
	REGISTER_CVAR_DEV_ONLY(kc_bulletSpeed, 100.f, VF_NULL, "The speed of a bullet for skill kill (in meters per second)");
	REGISTER_CVAR_DEV_ONLY(kc_bulletHoverDist, 4.f, VF_NULL, "The distance at which the bullet slows down in a skill kill");
	REGISTER_CVAR_DEV_ONLY(kc_bulletHoverTime, 1.f, VF_NULL, "The time for which the bullet slows down in a skill kill");
	REGISTER_CVAR_DEV_ONLY(kc_bulletHoverTimeScale, 0.01f, VF_NULL, "The time scale to use when the bullet slows down to a hover");
	REGISTER_CVAR_DEV_ONLY(kc_bulletPostHoverTimeScale, 1.0f, VF_NULL, "The time scale to use when the bullet speeds up after hover");
	REGISTER_CVAR_DEV_ONLY(kc_bulletTravelTimeScale, 0.5f, VF_NULL, "The time scale to use when the bullet is moving towards its target");
	REGISTER_CVAR_DEV_ONLY(kc_bulletCamOffsetX, 0.f, VF_NULL, "Camera offset for bullet time");
	REGISTER_CVAR_DEV_ONLY(kc_bulletCamOffsetY, -0.3f, VF_NULL, "Camera offset for bullet time");
	REGISTER_CVAR_DEV_ONLY(kc_bulletCamOffsetZ, 0.06f, VF_NULL, "Camera offset for bullet time");
	REGISTER_CVAR_DEV_ONLY(kc_bulletRiflingSpeed, 6.5f, VF_NULL, "Speed bullets spin in the killcam");
	REGISTER_CVAR_DEV_ONLY(kc_bulletZoomDist, 0.5f, VF_NULL, "The distance the camera zooms in on impact in bullet time killcam");
	REGISTER_CVAR_DEV_ONLY(kc_bulletZoomTime, 0.02f, VF_NULL, "The time taken for the camera to zoom in on impact in bullet time killcam");
	REGISTER_CVAR_DEV_ONLY(kc_bulletZoomOutRatio, 1.0f, VF_NULL, "The distance the camera zooms back out on impact in bullet time killcam as a ratio of kc_bulletZoomDist");
	REGISTER_CVAR_DEV_ONLY(kc_kickInTime, 2.f, VF_NULL, "The amount of time (in seconds) to record after the kill. This added to the length of recording, gives a minimum post revive time to avoid getting anything cut off from the playback. Also note that reducing this to 1 second will cut off the bullet time replays.");
	REGISTER_CVAR_DEV_ONLY(kc_maxFramesToPlayAtOnce, 10, VF_NULL, "The maximum number of frames that will be replayed at once if the killcam is lagging");
	REGISTER_CVAR_DEV_ONLY(kc_cameraCollision, 0, VF_NULL, "Use the additional collision tests to ensure killcam doesn't clip through walls/floors");
	REGISTER_CVAR_DEV_ONLY(kc_showHighlightsAtEndOfGame, 1, VF_NULL, "Show the highlight reel as part of the post game HUD sequence");
	REGISTER_CVAR_DEV_ONLY(kc_enableWinningKill, 0, VF_NULL, "Enable showing the winning kill at the end of a game");
	REGISTER_CVAR_DEV_ONLY(kc_canSkip, 0, VF_NULL, "Allows the player to skip the KillCam with a button press.");
	REGISTER_CVAR_DEV_ONLY(kc_projectileDistance, 0.3f, VF_NULL, "Sets the distance behind which the killcam will follow projectiles");
	REGISTER_CVAR_DEV_ONLY(kc_projectileHeightOffset, 0.05f, VF_NULL, "Sets the height offset of the camera during projectile killcam replay");
	REGISTER_CVAR_DEV_ONLY(kc_largeProjectileDistance, 0.625f, VF_NULL, "Sets the distance behind which the killcam will follow large projectiles (e.g. JAW)");
	REGISTER_CVAR_DEV_ONLY(kc_largeProjectileHeightOffset, 0.18f, VF_NULL, "Sets the height offset of the camera during large projectile (e.g. JAW) killcam replay");
	REGISTER_CVAR_DEV_ONLY(kc_projectileVictimHeightOffset, 0.65f, VF_NULL, "Sets the height offset of the camera focus point during projectile killcam replay");
	REGISTER_CVAR_DEV_ONLY(kc_projectileMinimumVictimDist, 4.3f, VF_NULL, "Sets the minimum distance between the victim and the camera");
	REGISTER_CVAR_DEV_ONLY(kc_smoothing, 0.05f, VF_NULL, "Sets the amount of camera smoothing to use during projectile replay (range from 0 to less than 1)");
	REGISTER_CVAR_DEV_ONLY(kc_grenadeSmoothingDist, 10.0f, VF_NULL, "Sets the distance at which camera smoothing kicks in for grenades");
	REGISTER_CVAR_DEV_ONLY(kc_cameraRaiseHeight, 1.f, VF_NULL, "Distance killcam should raytest upwards when trying to raise camera from the floor.");
	REGISTER_CVAR_DEV_ONLY(kc_resendThreshold, 0.5f, VF_NULL, "Maximum time between kills that we'll consider using same killcam data");
	REGISTER_CVAR_DEV_ONLY(kc_chunkStreamTime, 3.0f, VF_NULL, "How often to stream chunks of post-kill KillCam data (for the duration of the kc_kickInTime)");

#if !defined(_RELEASE)
	REGISTER_CVAR_DEV_ONLY(kc_copyKillCamIntoHighlightsBuffer, 0, VF_NULL, "Automatically save kill cams to the highlights reel (uses index n-1)");
#endif

	REGISTER_CVAR(g_multikillTimeBetweenKills, 5.0f, VF_CHEAT, "Time between kills needed for a multikill");
	REGISTER_CVAR(g_flushed_timeBetweenGrenadeBounceAndSkillKill, 4.0f, VF_CHEAT, "Time between grenade bouncing near someone and flushed skill kill");
	REGISTER_CVAR(g_gotYourBackKill_FOVRange, 45.f, 0, "FOV range to be used in determining if actors are facing each other");
	REGISTER_CVAR(g_gotYourBackKill_targetDistFromFriendly, 5.f, 0, "Distance check for victim near friendly actor");
	REGISTER_CVAR(g_guardian_maxTimeSinceLastDamage, 1.f, 0, "Max time in seconds since victim attacked a friendly");
	REGISTER_CVAR(g_defiant_timeAtLowHealth, 1.f, 0, "Period of time in seconds that player must have been at low health before making the kill");
	REGISTER_CVAR(g_defiant_lowHealthFraction, 0.25f, 0, "Fraction of max health that counts as 'low health'");
	REGISTER_CVAR(g_intervention_timeBetweenZoomedAndKill, 1.0f, 0, "Time between ironsighting and killing");
	REGISTER_CVAR(g_blinding_timeBetweenFlashbangAndKill, 5.0f, 0, "Time between flashbang and killing");
	REGISTER_CVAR(g_blinding_flashbangRecoveryDelayFrac, 0.29f, 0, "For the first X fraction of the flashbang effect, no blindness recovery will take place");
	REGISTER_CVAR(g_neverFlagging_maxMatchTimeRemaining, 10.0f, 0, "(Secs.) The maximum amount of time allowed left on the clock in the last round of CTF for the \"Never Flagging\" award to be awarded. (Remember that strings will also need to be changed if this is changed!!)");
	REGISTER_CVAR(g_combinedFire_maxTimeBetweenWeapons, 7.f, 0, "Time between last hit of previous weapon and kill for it count as combined fire");

	REGISTER_CVAR(g_fovToRotationSpeedInfluence, 0.0f, 0, "Reduces player rotation speed when fov changes");
	
	REGISTER_CVAR(dd_maxRMIsPerFrame, 3, VF_CHEAT, "Sets the maximum number of delayed detonation perk RMI's sent per frame");
	REGISTER_CVAR(dd_waitPeriodBetweenRMIBatches, 0.1f, VF_CHEAT, "Sets delay before allowing more delayed detonation perk RMI's to be sent");

	REGISTER_CVAR(g_debugSpawnPointsRegistration, 0, 0, "Enabled extra logging to debug spawn point registration");
	REGISTER_CVAR(g_debugSpawnPointValidity, 0, 0, "Fatal error if a client is requesting an initial spawn point that is in the wrong spawn group");
	REGISTER_CVAR(g_randomSpawnPointCacheTime, 1.f, VF_CHEAT, "Time to cache a spawn point for when picking random spawns, if you are the only one in the level.");

	REGISTER_CVAR_CB(g_detachCamera, 0, VF_CHEAT, "Detach camera",OnDetachCameraChanged);
	REGISTER_CVAR(g_moveDetachedCamera, 0, VF_CHEAT, "Move detached camera");
	REGISTER_CVAR(g_detachedCameraMoveSpeed, 6.0f, VF_CHEAT, "Move detached camera");
	REGISTER_CVAR(g_detachedCameraRotateSpeed, 1.5f, VF_CHEAT, "Move detached camera");
	REGISTER_CVAR(g_detachedCameraTurboBoost, 4.0f, VF_CHEAT, "Move speed turbo boost when holding down (360) A button");
	REGISTER_CVAR(g_detachedCameraDebug, 0, VF_CHEAT, "Display debug graphics for detached camera spline playback.");

#if !defined(_RELEASE)
	REGISTER_CVAR_DEV_ONLY(g_skipStartupSignIn, 0, VF_NULL, "Skip Sign-in for online services at startup. (useful for Xbox One offline demonstration)");

	REGISTER_CVAR(g_debugOffsetCamera, 0, VF_CHEAT, "Debug offset the camera");
	REGISTER_CVAR(g_debugLongTermAwardDays, 182, VF_CHEAT, "Debug number of days required for long term award");
#endif //!defined(_RELEASE)

	REGISTER_CVAR(g_debugCollisionDamage, 0, VF_DUMPTODISK, "Log collision damage");
	REGISTER_CVAR(g_debugHits, 0, VF_DUMPTODISK, "Log hits");
	REGISTER_CVAR(g_suppressHitSanityCheckWarnings, 1, VF_DUMPTODISK, "Suppress warnings emitted by sanity checks about direction vectors of hits");

#ifndef _RELEASE
	REGISTER_CVAR(g_debugFakeHits, 0, VF_DUMPTODISK, "Display Fake Hits");
	REGISTER_CVAR(g_FEMenuCacheSaveList, 0, VF_CHEAT, "Save the file list required to make the front end model cache pak");
#endif

	REGISTER_CVAR(g_useHitSoundFeedback, 1, 0, "use hit sound feedback");
	REGISTER_CVAR(g_useSkillKillSoundEffects, 0, 0, "'Headshot' and 'Denied' Audio on skill kills");

	pl_debug_filter = REGISTER_STRING("pl_debug_filter","",VF_CHEAT,"");
	REGISTER_CVAR(pl_debug_movement, 0, VF_CHEAT,"");
	REGISTER_CVAR(pl_debug_jumping, 0, VF_CHEAT,"");
	REGISTER_CVAR(pl_debug_aiming, 0, VF_CHEAT,"");
#ifndef _RELEASE
	REGISTER_CVAR(pl_debug_aiming_input, 0, VF_CHEAT,"");
#endif
	REGISTER_CVAR(pl_debug_vistable, 0, VF_CHEAT, "View debug information for vistable");
#ifndef _RELEASE
	REGISTER_CVAR(pl_debug_view, 0, VF_CHEAT,"");
	REGISTER_CVAR(pl_debug_vistableIgnoreEntities,0,0,"view currently ignored entities set in vistable"); 
	REGISTER_CVAR(pl_debug_vistableAddEntityId,0,0,"test ent id to add"); 
	REGISTER_CVAR(pl_debug_vistableRemoveEntityId,0,0,"test ent id to remove"); 
#endif
	REGISTER_CVAR(pl_debug_hit_recoil, 0, VF_CHEAT,"");
	REGISTER_CVAR(pl_debug_look_poses, 0, VF_CHEAT,"");

#if !defined(_RELEASE)
	REGISTER_CVAR(pl_debug_watch_camera_mode, 0, VF_CHEAT, "Display on-screen text showing player's current camera mode (requires 'watch_enabled')");
	REGISTER_CVAR(pl_debug_log_camera_mode_changes, 0, VF_CHEAT, "Write CPlayerCamera-related messages to game log");
	REGISTER_CVAR(pl_debug_log_player_plugins, 0, 0, "Write player plug-in info to the console and log file");
	REGISTER_CVAR(pl_shotDebug, 0, VF_CHEAT, "Debug local player's shots.");
#endif

	REGISTER_CVAR_CB(pl_renderInNearest, 1, 0, "Render player in nearest pass",OnPLRenderInNearestChanged);

	REGISTER_CVAR(pl_aim_cloaked_multiplier, 0.0f, 0, "Multiplier for autoaim slow and follow vs cloaked targets");
	REGISTER_CVAR(pl_aim_near_lookat_target_distance, 0.65f, 0, "Multiplier for autoaim slow and follow vs cloaked targets");

	REGISTER_CVAR(pl_aim_assistance_enabled, 1, 0, "Aim Assistance: Enable/Disable aim assistance.");
	REGISTER_CVAR(pl_aim_assistance_disabled_atDifficultyLevel, 4, VF_CHEAT, "Aim Assistance: Force disabling at certain difficulty level");
	REGISTER_CVAR(pl_aim_acceleration_enabled, 1, 0, "Aim Acceleration: Enable/Disable aim acceleration.");
	REGISTER_CVAR(pl_targeting_debug, 0, 0, "Targeting Assistance: Enable debugging of current target.");
	REGISTER_CVAR(pl_TacticalScanDuration, 0.5f, 0, "Time in seconds it takes to scan a interest point object using the visor.");
	REGISTER_CVAR(pl_TacticalScanDurationMP, 0.0f, 0, "Time in seconds it takes to scan a interest point object using the visor, for MP.");
	REGISTER_CVAR(pl_TacticalTaggingDuration, 0.5f, 0, "Time in seconds it takes to tag a interest point object using the visor.");
	REGISTER_CVAR(pl_TacticalTaggingDurationMP, 0.0f, 0, "Time in seconds it takes to Tag a interest point object using the visor, for MP.");

	REGISTER_CVAR(pl_switchTPOnKill, 1, VF_CHEAT, "Enable/Disable auto switch to third person on kill in multiplayer");
	REGISTER_CVAR(pl_stealthKill_allowInMP, 2, VF_CHEAT, "Enable/Disable stealth kills in multiplayer. 2 = stealth mode only");
	REGISTER_CVAR(pl_stealthKill_uncloakInMP, 0, VF_CHEAT, "Enable/Disable uncloaking of the killer during stealth kills in multiplayer");
	REGISTER_CVAR(pl_stealthKill_usePhysicsCheck, 1, VF_CHEAT, "Enable physics checks to make sure the player is able to do a stealth kill");
	REGISTER_CVAR(pl_stealthKill_useExtendedRange, 0, 0, "Allows stealth kills to be executed from a larger range");
	REGISTER_CVAR(pl_stealthKill_debug, 0, VF_CHEAT, "Show debug lines for stealth kill angles");
	REGISTER_CVAR(pl_stealthKill_aimVsSpineLerp, 0.5, 0, "Lerp factor between entity rotation and spine rotation");
	REGISTER_CVAR(pl_stealthKill_maxVelocitySquared, 36.f, 0, "Maximum speed of target for attempting stealth kill in multiplayer");
	REGISTER_CVAR(pl_slealth_cloakinterference_onactionMP, 0, VF_CHEAT, "Enable/Disable stealth interference effect on actions");
	
	REGISTER_CVAR(i_fastSelectMultiplier, 4.0f, VF_CHEAT, "Multiplier to use for deselecting prev weapon when switching to a fast select");

	REGISTER_CVAR(pl_stealth_shotgunDamageCap, 1000.0f, 0, "Shotgun damage cap when firing from cloak, MP ONLY");
	REGISTER_CVAR(pl_shotgunDamageCap, FLT_MAX, 0, "Shotgun damage cap when firing normally, MP ONLY");

	REGISTER_CVAR(pl_freeFallDeath_cameraAngle, -30.f, VF_CHEAT, "Angle to aim camera when falling death begins");
	REGISTER_CVAR(pl_freeFallDeath_fadeTimer, 0.75f, VF_CHEAT, "How long to wait before fading camera to black");

	REGISTER_CVAR(pl_fall_intensity_multiplier, 0.1f,0, "how much fall speed influences view shake intensity");
	REGISTER_CVAR(pl_fall_intensity_hit_multiplier, 0.3f, 0, "how much fall speed influences view shake intensity after a melee hit");
	REGISTER_CVAR(pl_fall_intensity_max, 1.5f, 0, "maximum view shake intensity");
	REGISTER_CVAR(pl_fall_time_multiplier, 0.05f, 0, "how much fall speed influences view shake time");
	REGISTER_CVAR(pl_fall_time_max, 0.05f, 0, "maximum view shake time");

	REGISTER_CVAR(pl_power_sprint.foward_angle, 45.0f, 0, "Power sprint: Stick angle threshold for sprinting (0.0 - 90.0f");

	REGISTER_CVAR(pl_jump_control.air_control_scale, 1.0f, 0, "Scales base air control while in air");
	REGISTER_CVAR(pl_jump_control.air_resistance_scale, 1.3f, 0, "Scales base air resitance while in air");
	REGISTER_CVAR(pl_jump_control.air_inertia_scale, 0.3f, 0, "Scales inertia while in air");

	REGISTER_CVAR(pl_stampTimeout, 3.0f, 0, "Timeout for bailing out of stamp. Essentially a coverall in case of emergency");
	
	REGISTER_CVAR(pl_jump_maxTimerValue, 2.5f, 0, "The maximum value of the timer");
	REGISTER_CVAR(pl_jump_baseTimeAddedPerJump, 0.4f, 0, "The amount of time that is added on per jump");
	REGISTER_CVAR(pl_jump_currentTimeMultiplierOnJump, 1.5f, 0, "Multiplier for the current timer per jump");

	REGISTER_CVAR(pl_boostedMelee_allowInMP, 0, 0, "Define whether or not the boosted melee mechanic can be used in multiplayer");

	REGISTER_CVAR(pl_velocityInterpAirControlScale, 1.0f, 0, "Use velocity based interpolation method with gravity adjustment");	
	REGISTER_CVAR(pl_velocityInterpSynchJump, 2, 0, "Velocity interp jump velocity synching");
	REGISTER_CVAR(pl_velocityInterpAirDeltaFactor, 0.75f, 0, "Interpolation air motion damping factor (0-1)");
	REGISTER_CVAR(pl_velocityInterpPathCorrection, 1.0f, 0, "Percentage of velocity to apply tangentally to the current velocity, used to reduce oscillation");
	REGISTER_CVAR(pl_velocityInterpAlwaysSnap, 0, 0, "Set to true to continually snap the remote player to the desired position, for debug usage only");
	REGISTER_CVAR(pl_adjustJumpAngleWithFloorNormal, 0, 0, "Set to true to adjust the angle a player jumps relative to the floor normal they're stood on. Can make jumping on slopes frustrating.");
	REGISTER_CVAR(pl_debugInterpolation, 0, 0, "Debug interpolation");
	REGISTER_CVAR(pl_serialisePhysVel, 1, 0, "Serialise the physics vel rathe rthan the stick");
	REGISTER_CVAR(pl_clientInertia, 0.0f, 0, "Override the interia of clients");
	REGISTER_CVAR(pl_fallHeight, 0.3f, VF_CHEAT, "The height above the ground at which the player starts to fall");
	
	REGISTER_CVAR(pl_netAimLerpFactor, 0.5f, 0, "Factor to lerp the remote aim directions by");
	REGISTER_CVAR(pl_netSerialiseMaxSpeed, 9.0f, 0, "Maximum char speed, used by interpolation");
	
#ifndef _RELEASE
	REGISTER_CVAR(pl_watchPlayerState, 0, 0, "0=Off, 1=Display state of local player, 2=Display state of all players (including AI)");
#endif
	
	REGISTER_CVAR(pl_ledgeClamber.debugDraw, 0, VF_CHEAT, "Turn on debug drawing of Ledge Clamber");
	REGISTER_CVAR(pl_ledgeClamber.cameraBlendWeight, 1.0f, VF_CHEAT, "FP camera blending weight when performing ledge grab action. Do NOT change this from 1.0f lightly!");
	REGISTER_CVAR(pl_ledgeClamber.enableVaultFromStanding, 1, VF_CHEAT, "0 = No vault from standing; 1 = Vault when push towards obstacle and press jump; 2 = Vault when press jump; 3 = Vault when push towards obstacle");

	//==============================================
	// Ladders
	//==============================================

	REGISTER_CVAR(pl_ladderControl.ladder_renderPlayerLast, 1, 0, "Render a player climbing a ladder in front of other geometry");
#ifndef _RELEASE
	REGISTER_CVAR(pl_ladderControl.ladder_logVerbosity, 0, 0, "Do verbose logs whenever the player uses a ladder");
#endif

	REGISTER_CVAR(pl_pickAndThrow.debugDraw, 0, VF_CHEAT, "Turn on debug drawing of Pick And Throw");
	REGISTER_CVAR2_CB("pl_pickAndThrow.useProxies", &pl_pickAndThrow.useProxies, 1, 0, "Enables/Disables PickAndThrow proxies. Needs reload", ReloadPickAndThrowProxiesOnChange);

	// Melee weaps
	REGISTER_CVAR(pl_pickAndThrow.maxOrientationCorrectionTime, 0.3f, 0,	"Maximum time period over which orientation/position correction will occur before grab anim is commenced");
	REGISTER_CVAR(pl_pickAndThrow.orientationCorrectionTimeMult, 1.0f, 0,	"speed multiplier for pick and throw orientation/position correction");

#ifndef _RELEASE
	// Dist based override
	REGISTER_CVAR(pl_pickAndThrow.correctOrientationDistOverride, -1.0f, 0, "If set to > 0.0f this distance will be used over xml specified dist to lerp player to prior to commencing grab anim.");

	// helper Pos based override
	REGISTER_CVAR(pl_pickAndThrow.correctOrientationPosOverrideEnabled, 0, 0, "Enable debug output for charged weapon throw.");
	REGISTER_CVAR(pl_pickAndThrow.correctOrientationPosOverrideX, 0.0f, 0, "Local helper Xpos override for grabbable entity");
	REGISTER_CVAR(pl_pickAndThrow.correctOrientationPosOverrideY, 0.0f, 0, "Local helper Ypos override for grabbable entity");

	// Charged throw
	REGISTER_CVAR(pl_pickAndThrow.chargedThrowDebugOutputEnabled, 0, 0, "Enable debug output for charged weapon throw.");

	// Health
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponHealthDebugEnabled, 0, 0, "Enable debug output for environmental weapon health");

	// Impacts
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponImpactDebugEnabled, 0, 0 , "Enable debug output for environmental weapon impacts");

	// Combos
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponComboDebugEnabled, 0, 0 , "Enable debug output for environmental weapon combos");

	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponRenderStatsEnabled, 0, 0 , "Enable property rendering");
	
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponDebugSwing, 0, 0 , "Enable debug of swing sweep test");
#endif // #ifndef _RELEASE

	REGISTER_CVAR(pl_pickAndThrow.comboInputWindowSize, 0.5f, 0,	 " e.g 0.35 == input window is within the final 0.35 [0.0,1.0] T of the prev anim");
	REGISTER_CVAR(pl_pickAndThrow.minComboInputWindowDurationInSecs, 0.4f, 0, "but window will be artificially extended to be no shorter than X e.g. 0.33 seconds" );

	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponDesiredRootedGrabAnimDuration, -1.0f, 0, "duration override for grab anim when obj rooted (< 0.0f == no desired override)");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponDesiredGrabAnimDuration, -1.0f, 0, "duration override for grab anim when obj on floor(< 0.0f == no desired override)");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponDesiredPrimaryAttackAnimDuration, -1.0f, 0, "duration override for primary attack anim (< 0.0f == no desired override)");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponDesiredComboAttackAnimDuration, -1.0f, 0, "duration override for primary combo attack anim (< 0.0f == no desired override)");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponUnrootedPickupTimeMult, 0.2f, 0, "duration multiplier for wait time when picking up un-rooted weapons from the floor");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponThrowAnimationSpeed, -1.0f, 0, "speed multiplier for weapon throw anim. less than 1 = longer than standard duration. -1 = default speed" );
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponLivingToArticulatedImpulseRatio, 0.6f, 0, "Splits environmental weapon impulse between the hit location on the articulated ragdoll, and the living entity as a whole [0.0f, 1.0f]");

	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponFlipImpulseThreshold, -0.33f, 0, "When impulsing ragdolls, override impulses specified in xml, will have their z component flipped, if the player's view dir z component is lower than this threshold" );
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponFlippedImpulseOverrideMult, 3.5f, 0, "When impulsing ragdolls, if this cvar is > 0.0f, any z component of an override impulse specified in xml,will be multiplied 0->1 (based on the z component of the look dir) and this multiplier" );
	
	REGISTER_CVAR(pl_pickAndThrow.enviromentalWeaponUseThrowInitialFacingOveride, 0, 0, "Set to 1 to enable. If 0, object will be thrown using it's current orientation(dependent on throw anim)" );

	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponSweepTestsEnabled, 1, 0, "Set to 1 to enable. If 1, sweep tests will be performed during melee swings for weapons flagged as requiring sweeptests in pickandthrow.xml" ); 

	REGISTER_CVAR(pl_pickAndThrow.objectImpulseLowMassThreshold, 250.0f, 0, "impulses to objects below this mass are scaled smaller to improve impulse appearance");
	REGISTER_CVAR(pl_pickAndThrow.objectImpulseLowerScaleLimit, 0.1f, 0, "lower end [0.0f,1.0f] range for impulse multiplier to low mass objects (e.g. at 0.00001 mass this multiplier is used), at objectImpulseLowMassthreshold (above) 1.0f * impulse scale is used");

	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponViewLerpZOffset, -0.35f, 0, "target view lerp pos offset when using env weap melee");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponViewLerpSmoothTime, 1.0, 0, "Time over which view dir is blended to target during env weap melee");

	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponObjectImpulseScale, 0.2f, 0, "Scale for impulse applied to object on hit with environmental weapon");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponImpulseScale, 0.7f, 0, "Scale for impulse appled to victim on hit with environmental weapon");
	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponHitConeInDegrees, 160.0f, 0, "The frontal cone in which env weapon impacts are acted upon e.g. ignore hits with objects behind player");
	REGISTER_CVAR(pl_pickAndThrow.minRequiredThrownEnvWeaponHitVelocity, 5.0f, 0, "The minimum thrown hit velocity (m/s) that will be acted on");
	REGISTER_CVAR(pl_pickAndThrow.awayFromPlayerImpulseRatio, 0.15f, 0, "When applying impulses, [0.0,1.0] of the impulse will be directly away from the attacker (to keep the ragdoll death visible) the inverse will be in the impact direction");
	REGISTER_CVAR(pl_pickAndThrow.cloakedEnvironmentalWeaponsAllowed, 1, 0, "Specifies whether large environmental weapons can be cloaked in MP games with a cloaked user");
	
	REGISTER_CVAR(pl_pickAndThrow.chargedThrowAutoAimDistance, 20.0f, 0, "Max dist auto aim will track targets to"); 
	REGISTER_CVAR(pl_pickAndThrow.chargedThrowAutoAimConeSize, 50.0f, 0, "Auto aim cone size(degrees) - 50% either side of forward view dir"); 
	REGISTER_CVAR(pl_pickAndThrow.chargedThrowAutoAimAngleHeuristicWeighting, 1.0f, 0, "Score multiplier for the 'cone' part of the auto aim algorithm"); 
	REGISTER_CVAR(pl_pickAndThrow.chargedThrowAutoAimDistanceHeuristicWeighting, 2.0f, 0, "Score multiplier for the 'distance' part of the auto aim algorithm"); 
	REGISTER_CVAR(pl_pickAndThrow.chargedthrowAutoAimEnabled, 1, 0, "Specifies whether charged throw auto aim is enabled");
	REGISTER_CVAR(pl_pickAndThrow.chargedThrowAimHeightOffset, 0.25f, 0, "Height above auto aim target throw will aim at. Helps account for curved throw path");

	REGISTER_CVAR(pl_pickAndThrow.intersectionAssistDebugEnabled, 0, 0, "Enable intersection assist debugging");
	REGISTER_CVAR(pl_pickAndThrow.intersectionAssistCollisionsPerSecond, 5.0, 0, "Collisions per second threshold considered stuck");
	REGISTER_CVAR(pl_pickAndThrow.intersectionAssistTimePeriod, 1.75, 0, "time period over which we will assess if something is stuck");
	REGISTER_CVAR(pl_pickAndThrow.intersectionAssistTranslationThreshold, 0.35f, 0, "How far an object must move per sec to be considered not stuck");
	REGISTER_CVAR(pl_pickAndThrow.intersectionAssistPenetrationThreshold, 0.07f, 0, "How far an object must penetrate to be considered embedded");
	REGISTER_CVAR(pl_pickAndThrow.intersectionAssistDeleteObjectOnEmbed, 1, 0, "if 1, embedded objects will be deleted, 0 - object will just be put to sleep");
	
	REGISTER_CVAR(pl_pickAndThrow.complexMelee_snap_angle_limit, 90.0f, 0, "Angle limit (in degrees) in which melee snap applies");
	REGISTER_CVAR(pl_pickAndThrow.complexMelee_lerp_target_speed, 2.0f, 0, "Movement speed for environmental weapon melee auto aim");
 
	REGISTER_CVAR(pl_pickAndThrow.impactNormalGroundClassificationAngle, 25.0f, 0, "Angle tolerance (in degrees - from upright) in which a surface normal is considered as ground ( and an impact to stick rather than rebound )");
	REGISTER_CVAR(pl_pickAndThrow.impactPtValidHeightDiffForImpactStick, 0.7f, 0, "height tolerance (in metres) from the player entity, in which we consider sticking rather than rebound");
	REGISTER_CVAR(pl_pickAndThrow.reboundAnimPlaybackSpeedMultiplier, 0.7f, 0, "the speed at which the rebound anim is played back - based on speed of original anim being reversed");

	REGISTER_CVAR(pl_pickAndThrow.environmentalWeaponMinViewClamp, -50.0f, 0, "The minimum pitch that can be achieved when holding a pick and throw weapon in mp"); 
	
	REGISTER_CVAR(pl_sliding_control.min_speed_threshold, 4.0f, 0, "Min sprinting movement speed to trigger sliding");
	REGISTER_CVAR(pl_sliding_control.min_speed, 2.0f, 0, "Speed threshold at which sliding will stop automatically");
	REGISTER_CVAR(pl_sliding_control.deceleration_speed, 4.0f, 0, "Deceleration while sliding (in m/s)");

	REGISTER_CVAR(pl_sliding_control.min_downhill_threshold, 5.0f, 0, "Slope angle threshold for down hill sliding");
	REGISTER_CVAR(pl_sliding_control.max_downhill_threshold, 50.0f, 0, "Slope angle threshold at which we reach maximum downhill acceleration");
	REGISTER_CVAR(pl_sliding_control.max_downhill_acceleration, 15.0f, 0, "Extra speed added to sliding, when going down hill (scaled linearly to slope)");

	REGISTER_CVAR(pl_sliding_control_mp.min_speed_threshold, 8.0f, 0, "Min sprinting movement speed to trigger sliding (for MP)");
	REGISTER_CVAR(pl_sliding_control_mp.min_speed, 7.0f, 0, "Speed threshold at which sliding will stop automatically (for MP)");
	REGISTER_CVAR(pl_sliding_control_mp.deceleration_speed, 2.2f, 0, "Deceleration while sliding (in m/s) (for MP)");
	REGISTER_CVAR(pl_sliding_control_mp.min_downhill_threshold, 5.0f, 0, "Slope angle threshold for down hill sliding (for MP)");
	REGISTER_CVAR(pl_sliding_control_mp.max_downhill_threshold, 50.0f, 0, "Slope angle threshold at which we reach maximum downhill acceleration (for MP)");
	REGISTER_CVAR(pl_sliding_control_mp.max_downhill_acceleration, 0.0f, 0, "Extra speed added to sliding, when going down hill (scaled linearly to slope) (for MP)");

	REGISTER_CVAR(pl_slideCameraFactor, 1.0f, VF_CHEAT, "Slide Camera Factor");

	REGISTER_CVAR(pl_enemy_ramming.player_to_player, 1.0f, 0, "Max damage if both actors are alive (and in movement)");
	REGISTER_CVAR(pl_enemy_ramming.ragdoll_to_player, 50.0f, 0, "Max ragdoll to player damage");
	REGISTER_CVAR(pl_enemy_ramming.fall_damage_threashold, 4.0f, 0, "Damage threshold to activate fall and play");
	REGISTER_CVAR(pl_enemy_ramming.safe_falling_speed, 6.0f, 0, "the falling speed the player starts to hurt the enemy from falling");
	REGISTER_CVAR(pl_enemy_ramming.fatal_falling_speed, 10.0f, 0, "the speed at where the falling is always fatal to the enemy");
	REGISTER_CVAR(pl_enemy_ramming.max_falling_damage, 250.0f, 0, "the maximum hit damage to the enemy can receive from being rammed from a falling player");
	REGISTER_CVAR(pl_enemy_ramming.min_momentum_to_fall, 600.0f, 0, "Minimum relative momentum to make an actor fall from the collision with another actor");

	REGISTER_CVAR(AICollisions.minSpeedForFallAndPlay, 10.0f, 0, "Min collision speed that can cause a fall and play");
	REGISTER_CVAR(AICollisions.minMassForFallAndPlay, 5.0f, 0, "Min collision mass that can cause a fall and play");
	REGISTER_CVAR(AICollisions.dmgFactorWhenCollidedByObject, 1.0f, 0, "generic multiplier applied to the dmg calculated when an object impacts into an AI.");
	REGISTER_CVAR(AICollisions.showInLog, 0, 0, "Logs collisions ( 0=no Log, 1=Only collisions with damage, 2=All collisions");

	REGISTER_CVAR(pl_melee.melee_snap_angle_limit, 90.0f, 0, "Angle limit (in degrees) in which melee snap applies (x2 for full range)");
	REGISTER_CVAR(pl_melee.melee_snap_blend_speed, 0.33f, 0, "Smooths transition to target direction. Range[0.0 - 1.0f]. Set to 1 for instant alignment");
	REGISTER_CVAR(pl_melee.melee_snap_move_speed_multiplier, 5.f, 0, "Multiplier to distance to determine movement velocity");
	REGISTER_CVAR(pl_melee.melee_snap_target_select_range, 2.5f, 0, "melee lunge target selection range");
	REGISTER_CVAR(pl_melee.melee_snap_end_position_range, 1.f, 0, "melee lunge target distance from player");
	REGISTER_CVAR(pl_melee.debug_gfx, 0, 0, "Enable/Disables debug gfx for melees");
	REGISTER_CVAR(pl_melee.damage_multiplier_from_behind, 1.f, 0, "Damage multiplier for melee attacking somebody from behind");
	REGISTER_CVAR(pl_melee.damage_multiplier_mp, 1.5f, 0, "Damage multiplier for melee attacking somebody in Multiplayer (eg. the Defenders in Assault)");
	REGISTER_CVAR(pl_melee.angle_limit_from_behind, 60.f, 0, "Angle limit (in degrees) in which behind damage multiplier will apply (x2 for full range)");
	REGISTER_CVAR(pl_melee.mp_victim_screenfx_intensity, 1.f, 0, "MP Only. Intensity of the full-screen hit effects that affect the victim when hit by a melee attack");
	REGISTER_CVAR(pl_melee.mp_victim_screenfx_duration, 2.0f, 0, "MP Only. Duration of the full-screen hit effects that affect the victim when hit by a melee attack");
	REGISTER_CVAR(pl_melee.mp_victim_screenfx_blendout_duration, 1.2f, 0, "MP Only. Duration of the blend-out of the full-screen hit effects that affect the victim when hit by a melee attack");
	REGISTER_CVAR(pl_melee.mp_victim_screenfx_dbg_force_test_duration, 0.f, VF_CHEAT, "MP DEBUG Only. Setting this to X will immediately trigger a full-screen melee hit effect for X secs, useful for testing the effect with just 1 player");
	REGISTER_CVAR(pl_melee.impulses_enable, SPlayerMelee::ei_OnlyToDead, 0, "Enables/Disables melee impulse handling. 0: Disabled, 1: Only impulses on alive characters, 2: Only impulses on ragdolls, 3: All impulses enabled");
	REGISTER_CVAR(pl_melee.mp_melee_system, 0, 0, "Enables the three-part melee system");
	REGISTER_CVAR(pl_melee.mp_melee_system_camera_lock_and_turn, 1, 0, "When meleeing with the 3-part system the camera will lock and face the target during the intro phase");
	REGISTER_CVAR(pl_melee.mp_melee_system_camera_lock_time, 0.15f, 0, "How long the camera is locked for when meleeing with 3-part system");
	REGISTER_CVAR(pl_melee.mp_melee_system_camera_lock_crouch_height_offset, 0.7f, 0, "When targets are crouching the camera will adjust down by this amount");
	REGISTER_CVAR(pl_melee.mp_knockback_enabled, 1, 0, "Enable knockbacks on successful melee hits");
	REGISTER_CVAR(pl_melee.mp_knockback_strength_vert, 0.f, 0, "Strength of melee attack knock back vertically");
	REGISTER_CVAR(pl_melee.mp_knockback_strength_hor, 4.f, 0, "Strength of melee attack knock back horizontally");
	REGISTER_CVAR(pl_melee.mp_sliding_auto_melee_enabled, 1, 0, "If a sliding player moves in range of an enemy player they will attempt an automatic melee attack");

	REGISTER_CVAR(pl_health.normal_regeneration_rateSP, 250.0f, 0, "Regeneration rate in points per second");
	REGISTER_CVAR(pl_health.critical_health_thresholdSP, 10.0f, 0, "If player health percentage drops below this threshold it's considered critical.");
	REGISTER_CVAR(pl_health.critical_health_updateTimeSP, 5.0f, 0, "Interval used to keep critical health msg shown (SP)");
	REGISTER_CVAR(pl_health.normal_threshold_time_to_regenerateSP, 4.0f, 0, "Time between hits to start regeneration");

	REGISTER_CVAR(pl_health.normal_regeneration_rateMP, 50.0f, 0, "Regeneration rate in points per second");
	REGISTER_CVAR(pl_health.critical_health_thresholdMP, 10.0f, 0, "If player health percentage drops below this threshold it's considered critical.");
	REGISTER_CVAR(pl_health.fast_regeneration_rateMP, 75.0f, 0, "Fast regeneration rate in points per second");
	REGISTER_CVAR(pl_health.slow_regeneration_rateMP, 20.0f, 0, "Slow regeneration rate in points per second");
	REGISTER_CVAR(pl_health.normal_threshold_time_to_regenerateMP, 5.0f, 0, "Time between hits to start regeneration");

	REGISTER_CVAR(pl_health.enable_FallandPlay, 1, 0, "Enables/Disables fall&play for the player");
	REGISTER_CVAR(pl_health.collision_health_threshold, 41, 0, "Collision damage will never put the player below this health");

	REGISTER_CVAR(pl_health.fallDamage_SpeedSafe, 12.0f, VF_CHEAT, "Safe fall speed.");
	REGISTER_CVAR(pl_health.fallDamage_SpeedFatal, 17.0f, VF_CHEAT, "Fatal fall");
	REGISTER_CVAR(pl_health.fallSpeed_HeavyLand, 14.0f, VF_CHEAT, "Fall speed to play heavyLand");
	REGISTER_CVAR(pl_health.fallDamage_SpeedFatalArmor, 21.0f, VF_CHEAT, "Fatal fall in armor mode");
	REGISTER_CVAR(pl_health.fallSpeed_HeavyLandArmor, 17.0f, VF_CHEAT, "Fall speed to play heavyLand in armor mode");
	REGISTER_CVAR(pl_health.fallDamage_SpeedSafeArmorMP, 18.0f, VF_CHEAT, "Safe fall speed in armor mode (in MP).");
	REGISTER_CVAR(pl_health.fallDamage_SpeedFatalArmorMP, 24.0f, VF_CHEAT, "Fatal fall in armor mode (in MP)");
	REGISTER_CVAR(pl_health.fallSpeed_HeavyLandArmorMP, 22.0f, VF_CHEAT, "Fall speed to play heavyLand in armor mode (in MP)");
	REGISTER_CVAR(pl_health.fallDamage_CurveAttack, 2.0f, VF_CHEAT, "Damage curve attack for medium fall speed");
	REGISTER_CVAR(pl_health.fallDamage_CurveAttackMP, 1.25f, VF_CHEAT, "MP Damage curve attack for medium fall speed");
	REGISTER_CVAR(pl_health.fallDamage_health_threshold, 1, VF_CHEAT, "Falling damage will never pu the player below this health, unless falling above fatal speed");
	REGISTER_CVAR(pl_health.debug_FallDamage, 0, VF_CHEAT, "Enables console output of fall damage information.");
	REGISTER_CVAR(pl_health.enableNewHUDEffect, 1, VF_CHEAT, "Enables new Health/Hits HUD effect (only on level reload)");
	REGISTER_CVAR(pl_health.minimalHudEffect, 0, VF_CHEAT, "only shows a bit of blood on screen when player is hit. Use this with g_godMode on.");
	//REGISTER_CVAR(pl_health.mercy_time, 3.0f, 0, "Mercy time");

	REGISTER_CVAR(pl_movement.nonCombat_heavy_weapon_speed_scale, 0.75f, 0, "Base speed multiplier in non combat mode while carrying a heavy (ripped-off) weapon");
	REGISTER_CVAR(pl_movement.nonCombat_heavy_weapon_sprint_scale, 1.5f, 0, "Sprint speed multiplier in non combat mode while carrying a heavy (ripped-off) weapon");
	REGISTER_CVAR(pl_movement.nonCombat_heavy_weapon_strafe_speed_scale, 0.9f, 0, "Strafe speed multiplier in non combat mode while carrying a heavy (ripped-off) weapon");
	REGISTER_CVAR(pl_movement.nonCombat_heavy_weapon_crouch_speed_scale, 1.0f, 0, "Crouch speed multiplier in non combat mode while carrying a heavy (ripped-off) weapon");
	REGISTER_CVAR(pl_movement.power_sprint_targetFov, 55.0f, 0, "Fov while sprinting in power mode");

	REGISTER_CVAR(pl_movement.ground_timeInAirToFall, 0.3f, VF_CHEAT, "Amount of time (in seconds) the player/ai can be in the air and still be considered on ground");
	REGISTER_CVAR(pl_movement.speedScale, 1.2f, 0, "General speed scale");
	REGISTER_CVAR(pl_movement.strafe_SpeedScale, 0.9f, 0, "Strafe speed scale");
	REGISTER_CVAR(pl_movement.sprint_SpeedScale, 1.8f, 0, "Sprint speed scale");
	REGISTER_CVAR(pl_movement.crouch_SpeedScale, 1.0f, 0, "Crouch speed scale");
	REGISTER_CVAR(pl_movement.sprintStamina_debug, false, VF_CHEAT, "For MP characters (eg. Assault defenders). Display debug values for the sprint stamina");
	REGISTER_CVAR(pl_movement.mp_slope_speed_multiplier_uphill, 1.0f, 0, "Changes how drastically slopes affect a players movement speed when moving uphill. Lower = less effect.");
	REGISTER_CVAR(pl_movement.mp_slope_speed_multiplier_downhill, 1.0f, 0, "Changes how drastically slopes affect a players movement speed when moving downhill. Lower = less effect.");
	REGISTER_CVAR(pl_movement.mp_slope_speed_multiplier_minHill, 0.f, 0, "Minimum threshold for the slope steepness before speed is affected (in degrees).");

#ifdef STATE_DEBUG
	REGISTER_STRING_CB( "pl_state_debug", "", VF_CHEAT, "For PlayerMovement StateMachine Debugging", ChangeDebugState );
#endif

	REGISTER_CVAR(mp_ctfParams.carryingFlag_SpeedScale, 0.8f, 0, "Speed multiplier whilst carrying the flag in Capture the Flag game mode.");

	REGISTER_CVAR(mp_extractionParams.carryingTick_SpeedScale, 0.8f, 0, "Speed multiplier whilst carrying the tick in extraction game mode.");
	REGISTER_CVAR(mp_extractionParams.carryingTick_EnergyCostPerHit, 0.05f, 0, "Energy cost used in armor mode whilst carrying the tick in extraction game mode.");
	REGISTER_CVAR(mp_extractionParams.carryingTick_DamageAbsorbDesperateEnergyCost, 0.45f, 0, "Energy cost used in armor mode whilst carrying the tick in extraction game mode.");

	REGISTER_CVAR(mp_predatorParams.hudTimerAlertWhenTimeRemaining, 15.f, 0, "When the round time is <= this value start flashing/beeping the timer");
	REGISTER_CVAR(mp_predatorParams.hintMessagePauseTime, 2.f, 0, "The time that the intro hint messages are paused to stay on screen longer than normal");

	REGISTER_CVAR(pl_mike_debug, 0, 0, "Show debug information for MIKE weapon");
	REGISTER_CVAR(pl_mike_maxBurnPoints, 8, 0, "maximum number of burning points");

	REGISTER_CVAR(pl_impulseEnabled, 0, 0, "Enable procedural impulses");
	REGISTER_CVAR(pl_impulseDuration, 0.3f, 0, "Duration of impulse");
	REGISTER_CVAR(pl_impulseLayer, 2, 0, "Animation layer to apply impulse to");
	REGISTER_CVAR(pl_impulseFullRecoilFactor, 0.25f, 0, "Tiume factor at which to apply maximum deflection (0->1)");
	REGISTER_CVAR(pl_impulseMaxPitch, 0.3f, 0, "Maximum angular pitch in rads");
	REGISTER_CVAR(pl_impulseMaxTwist, 0.25f, 0, "Maximum angular twist in rads");
	REGISTER_CVAR(pl_impulseCounterFactor, 0.9f, 0, "Percentage of deflection to counter by rotation of the higher joints");
#ifndef _RELEASE
	REGISTER_CVAR(pl_impulseDebug, 0, VF_CHEAT, "Show the Impulse Handler debug.");
#endif //_RELEASE
	REGISTER_CVAR(pl_legs_colliders_dist, 0.f, 0, "Distance to camera that activates leg collider proxies on characters");
	REGISTER_CVAR(pl_legs_colliders_scale, 1.0f, 0, "Scales leg colliders relative to skeleton bones");

	REGISTER_CVAR(g_assertWhenVisTableNotUpdatedForNumFrames, 255, 0, "");
	REGISTER_CVAR(gl_waitForBalancedGameTime, 180.f, 0, "Time to wait for enough players to make a balanced game before splitting squads and starting");

	// ui2 cvars
	REGISTER_CVAR(hud_ContextualHealthIndicator, 0, 0, "Display the HealthBar directly on the enemy screen position when scanned.");

	// hud cvars
	REGISTER_CVAR(hud_colorLine, 4481854, 0, "HUD line color.");
	REGISTER_CVAR(hud_colorOver, 14125840, 0, "HUD hovered color.");
	REGISTER_CVAR(hud_colorText, 12386209, 0, "HUD text color.");

	REGISTER_CVAR_CB(hud_subtitles, 0,0,"Subtitle mode. 0==Off, 1=All, 2=CutscenesOnly", OnSubtitlesChanged);

	REGISTER_CVAR(hud_psychoPsycho, 0, 0, "Psycho subtitles enabled.");

	REGISTER_CVAR(hud_startPaused, 1, 0, "The game starts paused, waiting for user input.");

	// mouse navigation enabled for all platforms now, but is only shown depending on control scheme
	REGISTER_CVAR(hud_allowMouseInput, 1, 0, "Allows mouse input on menus and special hud objects.");

	REGISTER_CVAR(menu3D_enabled, 1, 0, "Enable rendering of 3D objects over certain front end and in-game menus");

	REGISTER_CVAR(hud_faderDebug, 0, 0, "Show Debug Information for FullScreen Faders. 2 = Disable screen fading");
	REGISTER_CVAR(hud_objectiveIcons_flashTime, 1.6f, 0, "Time between icon changes for flashing objective icons");

	REGISTER_CVAR(g_flashrenderingduringloading, 1,0,"Enable active flash rendering during level loading"); 
	REGISTER_CVAR(g_levelfadein_levelload, 4, 0, "Fade in time after a level load (seconds)"); 
	REGISTER_CVAR(g_levelfadein_quickload, 2, 0, "Fade in time after a quick load (seconds)"); 

	// Controller aim helper cvars
	REGISTER_CVAR(aim_assistMinDistance, 0.0f, 0, "The minimum range at which autoaim operates");
	REGISTER_CVAR(aim_assistMaxDistance, 50.0f, 0, "The maximum range at which autoaim operates");
	REGISTER_CVAR(aim_assistFalloffDistance, 50.0f, 0, "The range at which follow autoaim starts to fall off. Set this as the same value as aim_assistMinDistance for the old behaviour");	
	REGISTER_CVAR(aim_assistInputForFullFollow_Ironsight, 1.0f, 0, "The required deflection on the analogue stick for maximum follow autoaim to be applied");	
	REGISTER_CVAR(aim_assistMaxDistanceTagged, 50.0f, 0, "The maximum range at which autoaim operates for tagged AI");
	REGISTER_CVAR(aim_assistMinTurnScale, 0.45f, 0, "The minimum turn speed as a fraction when aimed right at a target.");
	REGISTER_CVAR(aim_assistSlowFalloffStartDistance, 5.0f, 0, "The minimum range at which autoaim operates");
	REGISTER_CVAR(aim_assistSlowDisableDistance, 100.f, 0, "The maximum range at which autoaim operates");
	REGISTER_CVAR(aim_assistSlowStartFadeinDistance, 3.f, 0, "The distance at which the aiming slowdown starts to fade in");
	REGISTER_CVAR(aim_assistSlowStopFadeinDistance, 5.f, 0, "The distance at which the aiming slowdown is fully faded in");
	REGISTER_CVAR(aim_assistSlowDistanceModifier, 1.0f, 0, "Modify the distance for slowing by this amount.");
	REGISTER_CVAR(aim_assistSlowThresholdOuter, 2.7f, 0, "The distance in meters from the line the player is aiming along at which a target starts to slow the player's aim.");	
	REGISTER_CVAR(aim_assistStrength, 0.5f, 0, "A multiplier to the additional rotation added based upon controller input");
	REGISTER_CVAR(aim_assistSnapRadiusScale, 1.0f, 0, "Scales globally snap radius (for difficulty settings)");
	REGISTER_CVAR(aim_assistSnapRadiusTaggedScale, 1.0f, 0, "Scales globally snap radius on tagged enemies (for difficulty settings)");	

	REGISTER_CVAR(aim_assistStrength_IronSight, 0.5f, 0, "A multiplier to the additional rotation added based upon controller input");
	REGISTER_CVAR(aim_assistMaxDistance_IronSight, 50.0f, 0, "The maximum range at which autoaim operates");
	REGISTER_CVAR(aim_assistMinTurnScale_IronSight, 0.45f, 0, "The minimum turn speed as a fraction when aimed right at a target.");

	REGISTER_CVAR(aim_assistStrength_SniperScope, 0.2f, 0, "A multiplier to the additional rotation added based upon controller input");
	REGISTER_CVAR(aim_assistMaxDistance_SniperScope, 50.0f, 0, "The maximum range at which autoaim operates");
	REGISTER_CVAR(aim_assistMinTurnScale_SniperScope, 0.45f, 0, "The minimum turn speed as a fraction when aimed right at a target.");

	// Controller control
	REGISTER_CVAR(hud_aspectCorrection, 0, 0, "Aspect ratio corrections for controller rotation: 0-off, 1-direct, 2-inverse");

	REGISTER_CVAR(controller_power_curve, 2.5f, 0, "Analog controller input curve for both axes");

	REGISTER_CVAR(controller_multiplier_z, 2.0f, VF_RESTRICTEDMODE, "Horizontal linear sensitivity multiplier");
	REGISTER_CVAR(controller_multiplier_x, 1.5f, VF_RESTRICTEDMODE, "Vertical linear sensitivity multiplier");
	
	REGISTER_CVAR(controller_full_turn_multiplier_x, 1.8f, VF_RESTRICTEDMODE, "The multiplier applied when x rotation has been at full lock for the required time");
	REGISTER_CVAR(controller_full_turn_multiplier_z, 2.35f, VF_RESTRICTEDMODE, "The multiplier applied when x rotation has been at full lock for the required time");

	REGISTER_CVAR(hud_ctrlZoomMode, 0, 0, "Weapon aiming mode with controller. 0 is same as mouse zoom, 1 cancels at release");

	REGISTER_CVAR(ctrlr_OUTPUTDEBUGINFO, 0, VF_RESTRICTEDMODE, "Enable controller debugging.");
	REGISTER_CVAR(ctrlr_corner_smoother, 1, VF_CHEAT, "Sets the maximally allowed method for taking smooth corners; 0 = none; 1 = C2 method, smoothcd of angle; 2 = C3 method, using splines");
	REGISTER_CVAR(ctrlr_corner_smoother_debug, 0, VF_CHEAT, "Enables debugging for corner smoother.");

	REGISTER_CVAR(vehicle_steering_curve, 2.5, 0, "Analogue controller vehicle steering curve");
	REGISTER_CVAR(vehicle_steering_curve_scale, 1, 0, "Analogue controller vehicle steering curve scale");
	REGISTER_CVAR(vehicle_acceleration_curve, 2.5, 0, "Analogue controller vehicle acceleration curve");
	REGISTER_CVAR(vehicle_acceleration_curve_scale, 1, 0, "Analogue controller vehicle acceleration curve scale");
	REGISTER_CVAR(vehicle_deceleration_curve, 2.5, 0, "Analogue controller vehicle deceleration curve");
	REGISTER_CVAR(vehicle_deceleration_curve_scale, 1, 0, "Analogue controller vehicle steering deceleration scale");

	// Alternative player input normalization code
	REGISTER_CVAR(aim_altNormalization.enable, 0, 0, "Enables/disables alternative input code");
	REGISTER_CVAR(aim_altNormalization.hud_ctrl_Curve_Unified, 2.5f, 0, "Analog controller rotation curve");
	REGISTER_CVAR(aim_altNormalization.hud_ctrl_Coeff_Unified, 1.0f, 0, "Analog controller rotation scale"); 
	// Alternative player input normalization code
		
	//movement cvars
#if !defined(_RELEASE)
	REGISTER_CVAR(v_debugMovement, 0, VF_CHEAT, "Cheat mode, freeze the vehicle and activate debug movement");    
	REGISTER_CVAR(v_debugMovementMoveVertically, 0.f, VF_CHEAT, "Add this value to the vertical position of the vehicle");    
	REGISTER_CVAR(v_debugMovementX, 0.f, VF_CHEAT, "Add this rotation to the x axis");    
	REGISTER_CVAR(v_debugMovementY, 0.f, VF_CHEAT, "Add this rotation to the y axis");    
	REGISTER_CVAR(v_debugMovementZ, 0.f, VF_CHEAT, "Add this rotation to the z axis");    
	REGISTER_CVAR(v_debugMovementSensitivity, 30.f, VF_CHEAT, "if v_debugMovement is set you can rotate the vehicle, this controls the speed"); 
#endif

	REGISTER_CVAR(v_tankReverseInvertYaw, 1, 0, "When a tank goes in reverse, if this is enabled then the left/right controls will be inverted (same as with a wheeled vehicle)");    
	REGISTER_CVAR(v_profileMovement, 0, 0, "Used to enable profiling of the current vehicle movement (1 to enable)");    
	REGISTER_CVAR2("v_profile_graph", &v_profile_graph, "", 0,
			"Show a vehicle movement debug graph:\n"
			"    slip-speed           :     show the average FOWARD slip of the wheels\n"
			"    slip-speed-lateral   :     show the average SIDE slip of the wheels\n"
			"    centrif              :     show the actual centrifugal force\n"
			"    ideal-centrif        :     show ideal centrifugal force\n"
		);

#if !defined(_RELEASE)
	gEnv->pConsole->RegisterAutoComplete("v_profile_graph", &s_auto_v_profile_graph);
#endif
	
	REGISTER_CVAR(v_pa_surface, 1, VF_CHEAT, "Enables/disables vehicle surface particles");
	REGISTER_CVAR(v_wind_minspeed, 0.f, VF_CHEAT, "If non-zero, vehicle wind areas always set wind >= specified value");
	REGISTER_CVAR(v_draw_suspension, 0, VF_DUMPTODISK, "Enables/disables display of wheel suspension, for the vehicle that has v_profileMovement enabled");
	REGISTER_CVAR(v_draw_slip, 0, VF_DUMPTODISK, "Draw wheel slip status");  
	REGISTER_CVAR(v_invertPitchControl, 0, VF_DUMPTODISK, "Invert the pitch control for driving some vehicles, including the helicopter and the vtol");
	REGISTER_CVAR(v_sprintSpeed, 0.f, 0, "Set speed for acceleration measuring");
	REGISTER_CVAR(v_rockBoats, 1, 0, "Enable/disable boats idle rocking");  
	REGISTER_CVAR(v_debugSounds, 0, 0, "Enable/disable vehicle sound debug drawing");

	pAltitudeLimitCVar = REGISTER_CVAR(v_altitudeLimit, v_altitudeLimitDefault(), VF_CHEAT, "Used to restrict the helicopter and VTOL movement from going higher than a set altitude. If set to zero, the altitude limit is disabled.");

	REGISTER_CVAR(v_stabilizeVTOL, 0.35f, VF_DUMPTODISK, "Specifies if the air movements should automatically stabilize");
	REGISTER_CVAR(v_mouseRotScaleSP, 0.0833f, VF_CHEAT, "SinglePlayer - Scales the mouse rotation input to help match the controller rotation values");
	REGISTER_CVAR(v_mouseRotLimitSP, 20.f, VF_CHEAT, "SinglePlayer - Limits the mouse rotation input to help match the controller rotation values");
	REGISTER_CVAR(v_mouseRotScaleMP, 0.025f, VF_CHEAT, "Multiplayer - Scales the mouse rotation input to help match the controller rotation values");
	REGISTER_CVAR(v_mouseRotLimitMP, 4.f, VF_CHEAT, "Multiplayer - Limits the mouse rotation input to help match the controller rotation values");
	REGISTER_CVAR(v_MPVTOLNetworkSyncFreq, 1.0f, VF_CHEAT, "The frequency in seconds that the client VTOL will receive network updates from the server.");
	REGISTER_CVAR(v_MPVTOLNetworkSnapThreshold, 20.f, VF_CHEAT, "If the client VTOL is more than this distance along the path away from the server vtol, it will snap to the correct position. Otherwise it will accel/decel to match.");
	REGISTER_CVAR(v_MPVTOLNetworkCatchupSpeedLimit, 7.5f, VF_CHEAT, "Max VTOL catchup speed (added to the base speed to catch up to the server VTOL).");

	REGISTER_CVAR(pl_swimBackSpeedMul, 0.8f, VF_CHEAT, "Swimming backwards speed mul.");
	REGISTER_CVAR(pl_swimSideSpeedMul, 0.9f, VF_CHEAT, "Swimming sideways speed mul.");
	REGISTER_CVAR(pl_swimVertSpeedMul, 0.5f, VF_CHEAT, "Swimming vertical speed mul.");
	REGISTER_CVAR(pl_swimNormalSprintSpeedMul, 1.5f, VF_CHEAT, "Swimming Non-Speed sprint speed mul.");
	REGISTER_CVAR(pl_swimAlignArmsToSurface, 1, VF_CHEAT, "Enable/Disable FP arms alignement to surface while swimming");

	REGISTER_CVAR(pl_drownTime, 10.f, VF_CHEAT, "Time until first damage interval gets applied to the actor");
	REGISTER_CVAR(pl_drownDamage, 10, VF_CHEAT, "Damage (percentage of max health)" );
	REGISTER_CVAR(pl_drownDamageInterval, 1.5f, VF_CHEAT, "Damage interval in secs");
	REGISTER_CVAR(pl_drownRecoveryTime, 2.f, VF_CHEAT, "Time to recover oxygen level (multiplier on drownTime, 1 = pl_drownTime)");

	// animation triggered footsteps
	REGISTER_CVAR(g_FootstepSoundsFollowEntity, 1, VF_CHEAT, "Toggles moving of footsteps sounds with it's entity.");
	REGISTER_CVAR(g_FootstepSoundsDebug, 0, VF_CHEAT, "Toggles debug messages of footstep sounds.");
	REGISTER_CVAR(g_footstepSoundMaxDistanceSq, 2500.0f, 0, "Maximum squared distance for footstep sounds / fx spawned by Players.");

	// weapon system
	i_debuggun_1 = REGISTER_STRING("i_debuggun_1", "ai_statsTarget", VF_DUMPTODISK, "Command to execute on primary DebugGun fire");
	i_debuggun_2 = REGISTER_STRING("i_debuggun_2", "ai_BehaviorStatsTarget", VF_DUMPTODISK, "Command to execute on secondary DebugGun fire");

	REGISTER_CVAR(i_debug_projectiles, 0, VF_CHEAT, "Displays info about projectile status, where available.");
#ifndef _RELEASE
	REGISTER_CVAR(i_debug_weaponActions, 0, VF_CHEAT, "Displays info about weapon actions that are happening (1 shows only new actions, 2 shows you when duplicate actions are called)");
#endif
	REGISTER_CVAR(i_debug_recoil, 0, VF_CHEAT, "Displays info about current recoil");
	REGISTER_CVAR(i_debug_spread, 0, VF_CHEAT, "Displays info about current spread");
	REGISTER_CVAR(slide_spread, 1.f, 0, "Spread multiplier when sliding");
	REGISTER_CVAR(i_auto_turret_target, 1, VF_CHEAT, "Enables/Disables auto turrets aquiring targets.");
	REGISTER_CVAR(i_auto_turret_target_tacshells, 0, 0, "Enables/Disables auto turrets aquiring TAC shells as targets");

	REGISTER_CVAR(i_debug_itemparams_memusage, 0, VF_CHEAT, "Displays info about the item params memory usage");
	REGISTER_CVAR(i_debug_weaponparams_memusage, 0, VF_CHEAT, "Displays info about the weapon params memory usage");
	REGISTER_CVAR(i_debug_zoom_mods, 0, VF_CHEAT, "Use zoom mode spread/recoil mods");
	REGISTER_CVAR(i_debug_sounds, 0, VF_CHEAT, "Enable item sound debugging");
	REGISTER_CVAR(i_debug_turrets, 0, VF_CHEAT, 
		"Enable GunTurret debugging.\n"
		"Values:\n"
		"0:  off\n"
		"1:  basics\n"
		"2:  prediction\n"
		"3:  sweeping\n"
		"4:  searching\n"      
		"5:  deviation\n"
		"6:  Always Hostile (will shoot at you)\n"
		);
	REGISTER_CVAR(i_debug_mp_flowgraph, 0, VF_CHEAT, "Displays info on the MP flowgraph node");
	REGISTER_CVAR(i_flashlight_has_shadows, 1, 0, "Enables shadows on flashlight attachments");
	REGISTER_CVAR(i_flashlight_has_fog_volume, 1, 0, "Enables a fog volume on flashlight attachments");

	REGISTER_CVAR(g_displayIgnoreList,1,VF_DUMPTODISK,"Display ignore list in chat tab.");
	REGISTER_CVAR(g_buddyMessagesIngame,1,VF_DUMPTODISK,"Output incoming buddy messages in chat while playing game.");

	REGISTER_CVAR(g_persistantStats_gamesCompletedFractionNeeded, 0.5f, VF_CHEAT, "Fraction of games that need to be completed so that you post on leaderboards");

#ifndef _RELEASE
	REGISTER_CVAR(g_PS_debug, 0, 0, "for debugging. Display debugging info in watches on screen for gamestate");
#endif

	REGISTER_CVAR(sv_votingTimeout, 60, 0, "Voting timeout");
	REGISTER_CVAR(sv_votingCooldown, 180, 0, "Voting cooldown");
	REGISTER_CVAR(sv_votingRatio, 0.51f, 0, "Part of player's votes needed for successful vote.");

	int votingEnabledDefault = 0;
	if (gEnv->IsDedicated())
	{
		votingEnabledDefault = 1;
	}
	REGISTER_CVAR(sv_votingEnable, votingEnabledDefault, 0, "Enable voting system. Vote kicking is for dedicated servers only");
	REGISTER_CVAR(sv_votingBanTime, 10.0f, 0, "The duration of the kickban in minutes.");
	REGISTER_CVAR(sv_votingMinVotes, 2, 0, "The minimum number of votes required, on top of sv_votingRatio. Set to 0 to not use");

	REGISTER_CVAR(sv_input_timeout, 0, 0, "Experimental timeout in ms to stop interpolating client inputs since last update.");

	REGISTER_CVAR(g_MicrowaveBeamStaticObjectMaxChunkThreshold, 10, 0, "The microwave beam will ignore static geometry with more sub-parts than this value (To help limit worst case performance)");

	sv_aiTeamName = REGISTER_STRING("sv_aiTeamName", "", 0, "Team name for AIs");
	performance_profile_logname = REGISTER_STRING("performance_profile_logname", "performance.log", 0, "Filename for framerate and memory logging.");

#if !defined(_RELEASE)
	REGISTER_CVAR(g_spectate_Debug, 0, 0, "Show debug information for spectator mode. (1: show local player only, 2: show all players (however only so many will fit on screen)");
	REGISTER_CVAR(g_spectate_follow_orbitEnable, 0, 0, "Allow user input to orbit about the follow cam target (Irrelevant of data setup)");
#endif //!defined(_RELEASE)
	REGISTER_CVAR(g_spectate_TeamOnly, 1, 0, "If true, you can only spectate players on your team");
	REGISTER_CVAR(g_spectate_DisableManual, 1, 0, "");
	REGISTER_CVAR(g_spectate_DisableDead, 0, 0, "");
	REGISTER_CVAR(g_spectate_DisableFree, 1, 0, "");
	REGISTER_CVAR(g_spectate_DisableFollow, 0, 0, "");
	REGISTER_CVAR(g_spectate_skipInvalidTargetAfterTime, 4.0f, 0, "Time after which a new valid target is selected");
	REGISTER_CVAR(g_spectate_follow_orbitYawSpeedDegrees, 180.f, 0, "The max yaw rotation on a pad. In degrees per second");
	REGISTER_CVAR(g_spectate_follow_orbitAlsoRotateWithTarget, 0, 0, "If user orbiting is enabled then should the camera also rotate with the target rotation");
	REGISTER_CVAR(g_spectate_follow_orbitMouseSpeedMultiplier, 0.05f, 0, "Additional scalar to control mouse speed");
	REGISTER_CVAR(g_spectate_follow_orbitMinPitchRadians, -1.05f, 0, "How low the pitch can be rotated by the user");
	REGISTER_CVAR(g_spectate_follow_orbitMaxPitchRadians, 1.05f, 0, "How high the pitch can be rotated by the user");

	REGISTER_CVAR(g_spectatorOnly, 0, 0, "Enable spectate mode on this client");
	REGISTER_CVAR(g_spectatorOnlySwitchCooldown, 120, 0, "Cooldown in seconds between spectator switching");
	REGISTER_CVAR(g_forceIntroSequence, 0, 0, "Forces any intro sequence present (flownode) to play regardless of objectives etc");
	REGISTER_CVAR(g_IntroSequencesEnabled, 1, 0, "Enables any intro sequence setup for a level to play");
	
	REGISTER_CVAR(g_deathCam, 1, 0, "Enables / disables the MP death camera (shows the killer's location)");
	REGISTER_CVAR(g_deathCamSP.enable, 1, 0, "Enables / disables the SP death camera");
	REGISTER_CVAR(g_deathCamSP.dof_enable, 1, 0, "Enables / disables the Depth of field effect in single player death camera");
	REGISTER_CVAR(g_deathCamSP.updateFrequency, 0.1f, 0, "Update frequency for death camera dof effect values");
	REGISTER_CVAR(g_deathCamSP.dofRange, 20.0f, 0, "FocusRange value used when focusing on the killer");
	REGISTER_CVAR(g_deathCamSP.dofRangeNoKiller, 5.0f, 0, "FocusRange value used when not focusing on the killer");
	REGISTER_CVAR(g_deathCamSP.dofRangeSpeed, 10.0f, 0, "Speed the FocusRange value changes over time");
	REGISTER_CVAR(g_deathCamSP.dofDistanceSpeed, 75.0f, 0, "Speed the FocusDistance changes over time");

#ifndef _RELEASE
	REGISTER_CVAR(g_tpdeathcam_dbg_gfxTimeout, 0.f, VF_CHEAT, "DEBUG: timeout for the third-person death cam debug graphics. 0 turns gfx off completely");
	REGISTER_CVAR(g_tpdeathcam_dbg_alwaysOn, 0, VF_CHEAT, "DEBUG: turn the third-person \"death cam\" on all the time, helps for tweaking other variables");
#endif
	REGISTER_CVAR(g_tpdeathcam_timeOutKilled, 1.5f, VF_CHEAT, "If you are killed by another player, swap to the killerCam after this amount of time. May be reduced or not used if g_autorevivetime is too small."); // Please do not access directly; use CGameRulesMPSpectator::GetPostDeathDisplayDurations()");
	REGISTER_CVAR(g_tpdeathcam_timeOutSuicide, 10.0f, VF_CHEAT, "Died by suicide, swap to the next mode after this amount of time. May be reduced or not used if g_autorevivetime is too small."); // Please do not access directly; use CGameRulesMPSpectator::GetPostDeathDisplayDurations()");
	REGISTER_CVAR(g_tpdeathcam_lookDistWhenNoKiller, 10.f, VF_CHEAT, "When watching a third-person death but have no valid killer, an artificial look-at pos is created at this distance away");
	REGISTER_CVAR(g_tpdeathcam_camDistFromPlayerStart, 2.2f, VF_CHEAT, "The distance between the third-person death camera and the player being watched dying at the start.");
	REGISTER_CVAR(g_tpdeathcam_camDistFromPlayerEnd, 1.2f, VF_CHEAT, "The distance between the third-person death camera and the player being watched dying at the end.");
	REGISTER_CVAR(g_tpdeathcam_camDistFromPlayerMin, 1.0f, VF_CHEAT, "The minimum distance the camera can be from the player being watched dying.");
	REGISTER_CVAR(g_tpdeathcam_camHeightTweak, -0.02f, VF_CHEAT, "By default the third-person death cam's height is at the centre-height of the player dying, but this variable can be used to tweak it up (+) or down (-)");
	REGISTER_CVAR(g_tpdeathcam_camCollisionRadius, /*0.179f*/0.2f, VF_CHEAT, "The radius of the collision sphere used for the camera to stop the third-person death cam from intersecting geometry");
	REGISTER_CVAR(g_tpdeathcam_maxBumpCamUpOnCollide, 2.0f/*3.0f*/, VF_CHEAT, "When the death cam collides with a wall it'll start to raise depending on how close the collision was. This is the maximum it can rise by");
	REGISTER_CVAR(g_tpdeathcam_zVerticalLimit, 0.983f, VF_CHEAT, "Stop the death cam from getting too vertical as it causes the rotation to freak out");
	REGISTER_CVAR(g_tpdeathcam_testLenIncreaseRestriction, 0.3f, VF_CHEAT, "Restrict by how much the sphere test can be longer than the clear part of the previous one. An attempt to stop back-and-forth glitchiness");
	REGISTER_CVAR(g_tpdeathcam_collisionEpsilon, 0.001f, VF_CHEAT, "A Small Number, used to pull collision spheres away from walls, etc.");
	REGISTER_CVAR(g_tpdeathcam_directionalFocusGroundTestLen, 0.5f, VF_CHEAT, "Alternative method to find ground for focus pos, using directional test from the direction of the camera. This is the length of that test. Needs to be small, to avoid penetrating walls");
	REGISTER_CVAR(g_tpdeathcam_camSmoothSpeed, 5.0f, VF_CHEAT, "Smoothing speed for the DeathCam.");
	REGISTER_CVAR(g_tpdeathcam_maxTurn, 120.f, VF_CHEAT, "The furthest angle by which the DeathCam will turn as is displays.");

	REGISTER_CVAR(g_killercam_disable, 0, VF_CHEAT, "Disables KillerCam game-wide.");
	REGISTER_CVAR(g_killercam_displayDuration, 3.5f, VF_CHEAT, "The amount of time to show display the KillerCam for. May be reduced or not used if g_autorevivetime is too small."); // Please do not access directly; use CGameRulesMPSpectator::GetPostDeathDisplayDurations()
	REGISTER_CVAR(g_killercam_dofBlurAmount, 0.8f, VF_CHEAT, "Blur amount for the background on the Killer Cam" );
	REGISTER_CVAR(g_killercam_dofFocusRange, 5.0f, VF_CHEAT, "The focus range for the depth of field on the Killer Cam" );
	REGISTER_CVAR(g_killercam_canSkip, 0, VF_CHEAT, "Allows the user to skip the KillerCam" );

	REGISTER_CVAR(g_postkill_minTimeForDeathCamAndKillerCam, 3.0f, VF_CHEAT, "The minimum time needed post-kill to display both of the KillerCam and DeathCam.");
	REGISTER_CVAR(g_postkill_splitScaleDeathCam, 0.166f, VF_CHEAT, "Percentage (0-1) of the remaining post-kill time to be used to display the DeathCam when the time available to show the screens is less than the chosen amount. The other percent will be used for the KillerCam. (0.25 with a total of 4 seconds to display the screens would show DeathCam for 1sec(25%) and KillerCam for 3secs(75%))" );

	REGISTER_CVAR(sv_pacifist, 0, 0, "Pacifist mode (only works on dedicated server)");
	REGISTER_CVAR(g_devDemo, 0, VF_CHEAT, "To enable developer demos (intended to be checked from flowgraph)");

	REGISTER_CVAR(g_teamDifferentiation, 1, 0, "Enable different character models for different teams");

	REGISTER_CVAR(g_postEffect.FilterGrain_Amount, 0.0f, 0, "Filter grain amount");
	REGISTER_CVAR(g_postEffect.FilterRadialBlurring_Amount, 0.0f, 0, "Radial blurring amount");
	REGISTER_CVAR(g_postEffect.FilterRadialBlurring_ScreenPosX, 0.5f, 0, "Radial blurring screen position X");
	REGISTER_CVAR(g_postEffect.FilterRadialBlurring_ScreenPosY, 0.5f, 0, "Radial blurring screen position Y");
	REGISTER_CVAR(g_postEffect.FilterRadialBlurring_Radius, 1.0f, 0, "Radial blurring radius");
	REGISTER_CVAR(g_postEffect.Global_User_ColorC, 0.0f, 0, "Global cyan");
	REGISTER_CVAR(g_postEffect.Global_User_ColorM, 0.0f, 0, "Global magenta");
	REGISTER_CVAR(g_postEffect.Global_User_ColorY, 0.0f, 0, "Global yellow");
	REGISTER_CVAR(g_postEffect.Global_User_ColorK, 0.0f, 0, "Global luminance");
	REGISTER_CVAR(g_postEffect.Global_User_Brightness, 1.0f, 0, "Global brightness");
	REGISTER_CVAR(g_postEffect.Global_User_Contrast, 1.0f, 0, "Global contrast");
	REGISTER_CVAR(g_postEffect.Global_User_Saturation, 1.0f, 0, "Global saturation");
	REGISTER_CVAR(g_postEffect.Global_User_ColorHue, 0.0f, 0, "Global color hue");
	REGISTER_CVAR(g_postEffect.HUD3D_Interference, 0.0f, 0, "3D HUD interference");
	REGISTER_CVAR(g_postEffect.HUD3D_FOV, 0.0f, 0, "3D HUD field of view");

	REGISTER_CVAR(g_gameFXSystemDebug, 0, 0, "Toggles game effects system debug state");
	REGISTER_CVAR(g_gameFXLightningProfile, 0, 0, "Toggles game effects system lightning arc profiling");
	REGISTER_CVAR(g_DebugDrawPhysicsAccess, 0, VF_CHEAT|VF_DUMPTODISK, "Displays current physics access statistics for the game module.");

	REGISTER_CVAR(g_hasWindowFocus, 1, 0, "");

	pVehicleQuality = pConsole->GetCVar("v_vehicle_quality");		assert(pVehicleQuality);

	REGISTER_CVAR(g_displayPlayerDamageTaken, 0, 0, "Display the amount of damage being taken above a player who's getting injured");
	REGISTER_CVAR(g_displayDbgText_hud, 0, 0, "Show HUD-related debugging text on the screen");
	REGISTER_CVAR(g_displayDbgText_plugins, 0, 0, "Show player plug-in-related debugging text on the screen");
	REGISTER_CVAR(g_displayDbgText_pmv, 0, 0, "Show list of 'player modifiable values' on the screen");
	REGISTER_CVAR(g_displayDbgText_actorState, 0, 0, "Show information (health, current state etc.) about each actor");
	REGISTER_CVAR(g_displayDbgText_silhouettes, 0, 0, "Show silhouette-related debugging text on the screen");

	REGISTER_CVAR(g_spawn_vistable_numLineTestsPerFrame, 10, 0, "on the server - the number of linetests the vistable should do per frame");
	REGISTER_CVAR(g_spawn_vistable_numAreaTestsPerFrame, 20, 0, "on the server - the number of linetest replacing areatests the vistable should do per frame");

	REGISTER_CVAR(g_showShadowChar, false, 0, "Render the shadow casting character");
	REGISTER_CVAR(g_infiniteAmmo, 0, 0, "Infinite inventory ammo");		// Used by MP private matches, is NOT a cheat variable

	REGISTER_CVAR(g_animatorDebug, false, 0, "Animator Debug Info");
	REGISTER_CVAR(g_hideArms, false, 0, "Hide arms in first person");
	REGISTER_CVAR(g_debugSmokeGrenades, false, 0, "Debug smoke grenade LOS blocking");
	REGISTER_CVAR(g_smokeGrenadeRadius, 2.0f, 0, "The radius of the smoke blast from smoke grenades for LOS blocking");
	REGISTER_CVAR(g_empOverTimeGrenadeLife, 6.0f, 0, "The time an EMP over time grenade stays alive for once initially exploding");

	REGISTER_CVAR(g_kickCarDetachesEntities, 1, 0, "1=drop on kick" );
	REGISTER_CVAR(g_kickCarDetachStartTime, 0.01f, 0, "Time in seconds after kicking a car that doors etc. should start falling off" );
	REGISTER_CVAR(g_kickCarDetachEndTime,1.5f, 0, "Time in seconds after kicking a car that doors etc. will have all fallen off" );

#if !defined(_RELEASE)
	REGISTER_CVAR(g_DisableScoring, 0, VF_CHEAT, "Disable players being awarded points");
	REGISTER_CVAR(g_DisableCollisionDamage, 0, VF_CHEAT, "Disable entities being damaged by collisions");
	REGISTER_CVAR(g_MaxSimpleCollisions, 999, VF_CHEAT, "Imposes a cap on the amount of rigid body collision events per frame (only active when g_DisableCollisionDamage is on)");
	REGISTER_CVAR(g_LogDamage, 0, VF_NULL, "Log all damage being taken");
	REGISTER_CVAR(g_ProjectilePathDebugGfx, 0, VF_NULL, "Draw paths of projectiles through the air");
#endif
	
#if (USE_DEDICATED_INPUT)
	REGISTER_CVAR_CB(g_playerUsesDedicatedInput, 0, 0, "player's will use the automated dedicated input", OnDedicatedInputChanged);
#endif

	REGISTER_CVAR(watch_enabled, 1, 0, "On-screen watch text is enabled/disabled");
	REGISTER_CVAR(watch_text_render_start_pos_x, 35.0f, 0, "On-screen watch text render start x position");
	REGISTER_CVAR(watch_text_render_start_pos_y, 180.0f, 0, "On-screen watch text render start y position");
	REGISTER_CVAR(watch_text_render_size, 1.75f, 0, "On-screen watch text render size");
	REGISTER_CVAR(watch_text_render_lineSpacing, 9.3f, 0, "On-screen watch text line spacing (to cram more text on screen without shrinking the font)");
	REGISTER_CVAR(watch_text_render_fxscale, 13.0f, 0, "Draw2d label to IFFont x scale value (for calcing sizes)." );

	REGISTER_CVAR(autotest_enabled, 0, 0, "1 = enabled autotesting, 2 = enabled autotesting with no output results written.");

	autotest_state_setup = REGISTER_STRING("autotest_state_setup", "", 0, "setup string for autotesting");
	REGISTER_CVAR(autotest_quit_when_done, 0, 0, "quit the game when tests are done");
	REGISTER_CVAR(autotest_verbose, 1, 0, "output detailed logging whilst running feature tests");

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	net_onlyListGameServersContainingText = REGISTER_STRING("net_onlyListGameServersContainingText", "", 0, "Server list will only display host names containing this text");
	net_nat_type = REGISTER_STRING("net_nat_type", "Uninitialized", VF_READONLY, "The current NAT Type set.");
	REGISTER_CVAR(net_initLobbyServiceToLan, 0, 0, "Always initialize lobby in LAN mode (NB: mode can still be changed afterwards!)");
#endif

	REGISTER_CVAR(designer_warning_enabled, 1, 0, "designer warnings are enabled");
	REGISTER_CVAR(designer_warning_level_resources, 0, 0, "Designer warnings about resource load during run-time");
	REGISTER_CVAR(ai_DebugVisualScriptErrors, 0, VF_CHEAT|VF_DUMPTODISK,
		"Toggles the visual signal when something goes wrong in the AI scripts.\n"
		"Usage: ai_DebugVisualScriptErrors [0/1]\n"
		"Default is 0 (off)."
		"when an error occurs. When that happens there should be an error description in the log.\n");

	REGISTER_CVAR(ai_EnablePressureSystem, 1, VF_CHEAT|VF_DUMPTODISK, "Toggles AI's pressure system.\n");
	REGISTER_CVAR(ai_DebugPressureSystem, 0, VF_CHEAT|VF_DUMPTODISK, "Toggles AI's pressure system debug mode.\n");
	REGISTER_CVAR(ai_DebugAggressionSystem, 0, VF_CHEAT|VF_DUMPTODISK, "Toggles AI's aggression system debug mode.\n");
	REGISTER_CVAR(ai_DebugBattleFront, 0, VF_CHEAT|VF_DUMPTODISK, "Toggles AI's battlefront system debug mode.\n");
	REGISTER_CVAR(ai_DebugSearch, 0, VF_CHEAT|VF_DUMPTODISK, "Toggles AI's search module debug mode.\n");
	REGISTER_CVAR(ai_DebugDeferredDeath, 0, VF_CHEAT|VF_DUMPTODISK, "Toggles debugging of deferred death on/off.\n");
	REGISTER_CVAR(ai_SquadManager_DebugDraw, 0, VF_CHEAT|VF_DUMPTODISK, "1 = Toggles debugging of the Squad Manager [1=on - 0=off]\n");
	REGISTER_CVAR2("ai_SquadManager_MaxDistanceFromSquadCenter", &ai_SquadManager_MaxDistanceFromSquadCenter,
		40.0f, 0, "Max distance to be considered into a Squad.\n");
	REGISTER_CVAR2("ai_SquadManager_UpdateTick", &ai_SquadManager_UpdateTick,
		5.0f, 0, "Tick time to request a new update or the squads calculation.\n");

	REGISTER_CVAR2("ai_SOMDebugName", &ai_threatModifiers.DebugAgentName, "none", 0, "Debug the threat modifier for the given AI");
	REGISTER_CVAR2("ai_SOMIgnoreVisualRatio", &ai_threatModifiers.SOMIgnoreVisualRatio, 0.35f, 0,
		"Ratio from [0,1] where the AI will ignore seeing an enemy (keep the threat at none).\n"
		"Usage: ai_SOMIgnoreVisualRatio 0.35\n"
		"Default is 35%. Putting this at a higher value will result in the AI first considering a visual target to be non-threatening for a percentage of the SOM time period");
	REGISTER_CVAR2("ai_SOMDecayTime", &ai_threatModifiers.SOMDecayTime, 1.0f, 0,
		"How long it takes to decay one bar when visual sighting is lost on the target\n"
		"Usage: ai_SOMDecayTime 1\n"
		"Default is 1 second. A higher value means the AI will take longer to go aggressive if they see the target again");

	REGISTER_CVAR2("ai_SOMMinimumRelaxed", &ai_threatModifiers.SOMMinimumRelaxed, 2.0f, 0,
		"Minimum time that must pass before AI will see the enemy while relaxed, regardless of distance.\n"
		"Usage: ai_SOMMinimumRelaxed 2.0\n"
		"Default is 2.0 seconds. A lower value causes the AI to potentially react to the enemy sooner.");
	REGISTER_CVAR2("ai_SOMMinimumCombat", &ai_threatModifiers.SOMMinimumCombat, 0.5f, 0,
		"Minimum time that must pass before AI will see the enemy while alarmed, regardless of distance.\n"
		"Usage: ai_SOMMinimumCombat 0.5\n"
		"Default is 0.5 seconds. A lower value causes the AI to potentially react to the enemy sooner.");
	REGISTER_CVAR2("ai_SOMCrouchModifierRelaxed", &ai_threatModifiers.SOMCrouchModifierRelaxed, 1.0f, 0,
		"Modifier applied to time before the AI will see the enemy if the enemy is crouched while relaxed.\n"
		"Usage: ai_SOMCrouchModifierRelaxed 1.0\n"
		"Default is 1.0. A higher value will cause the AI to not see the enemy for a longer period of time while the enemy is crouched.");
	REGISTER_CVAR2("ai_SOMCrouchModifierCombat", &ai_threatModifiers.SOMCrouchModifierCombat, 1.0f, 0,
		"Modifier applied to time before the AI will see the enemy if the enemy is crouched while alarmed.\n"
		"Usage: ai_SOMCrouchModifierCombat 1.0\n"
		"Default is 1.0. A higher value will cause the AI to not see the enemy for a longer period of time while the enemy is crouched.");
	REGISTER_CVAR2("ai_SOMMovementModifierRelaxed", &ai_threatModifiers.SOMMovementModifierRelaxed, 1.0f, 0,
		"Modifier applied to time before the AI will see the enemy based on the enemy's movement speed while relaxed.\n"
		"Usage: ai_SOMMovementModifierRelaxed 1.0\n"
		"Default is 1.0. A lower value will cause the AI to see the enemy sooner relative to how fast the enemy is moving - the faster, the sooner.");
	REGISTER_CVAR2("ai_SOMMovementModifierCombat", &ai_threatModifiers.SOMMovementModifierCombat, 1.0f, 0,
		"Modifier applied to time before the AI will see the enemy based on the enemy's movement speed while alarmed.\n"
		"Usage: ai_SOMMovementModifierCombat 1.0\n"
		"Default is 1.0. A lower value will cause the AI to see the enemy sooner relative to how fast the enemy is moving - the faster, the sooner.");
	REGISTER_CVAR2("ai_SOMWeaponFireModifierRelaxed", &ai_threatModifiers.SOMWeaponFireModifierRelaxed, 1.0f, 0,
		"Modifier applied to time before the AI will see the enemy based on if the enemy is firing their weapon.\n"
		"Usage: ai_SOMWeaponFireModifierRelaxed 0.1\n"
		"Default is 0.1. A lower value will cause the AI to see the enemy sooner if the enemy is firing their weapon.");
	REGISTER_CVAR2("ai_SOMWeaponFireModifierCombat", &ai_threatModifiers.SOMWeaponFireModifierCombat, 1.0f, 0,
		"Modifier applied to time before the AI will see the enemy based on if the enemy is firing their weapon.\n"
		"Usage: ai_SOMWeaponFireModifierCombat 0.1\n"
		"Default is 0.1. A lower value will cause the AI to see the enemy sooner if the enemy is firing their weapon.");
	
	REGISTER_CVAR2("ai_SOMCloakMaxTimeRelaxed", &ai_threatModifiers.SOMCloakMaxTimeRelaxed, 5.0f, 0,
		"Time the AI cannot react to a cloaked target when the target is close enough to be seen while relaxed.\n"
		"Usage: ai_SOMCloakMaxTimeRelaxed 5.0\n"
		"Default is 5.0. A lower value will cause the AI to see the enemy sooner when cloaked relative to how close the enemy is.");
	REGISTER_CVAR2("ai_SOMCloakMaxTimeCombat", &ai_threatModifiers.SOMCloakMaxTimeCombat, 5.0f, 0,
		"Time the AI cannot react to a cloaked target when the target is close enough to be seen while alarmed.\n"
		"Usage: ai_SOMCloakMaxTimeCombat 5.0\n"
		"Default is 5.0. A lower value will cause the AI to see the enemy sooner when cloaked relative to how close the enemy is.");
	REGISTER_CVAR2("ai_SOMUncloakMinTimeRelaxed", &ai_threatModifiers.SOMUncloakMinTimeRelaxed, 1.0f, 0,
		"Minimum time the AI cannot react to a target uncloaking while relaxed.\n"
		"Usage: ai_SOMUncloakMinTimeRelaxed 1.0\n"
		"Default is 1.0. A lower value will cause the AI to see the enemy sooner when uncloaking.");
	REGISTER_CVAR2("ai_SOMUncloakMinTimeCombat", &ai_threatModifiers.SOMUncloakMinTimeCombat, 1.0f, 0,
		"Minimum time the AI cannot react to a target uncloaking while alarmed.\n"
		"Usage: ai_SOMUncloakMinTimeCombat 1.0\n"
		"Default is 1.0. A lower value will cause the AI to see the enemy sooner when uncloaking.");
	REGISTER_CVAR2("ai_SOMUncloakMaxTimeRelaxed", &ai_threatModifiers.SOMUncloakMaxTimeRelaxed, 5.0f, 0,
		"Time the AI cannot react to a target uncloaking when the target is at their max sight range while relaxed.\n"
		"Usage: ai_SOMUncloakMaxTimeRelaxed 5.0\n"
		"Default is 5.0. A lower value will cause the AI to see the enemy sooner when uncloaking relative to how close the enemy is.");
	REGISTER_CVAR2("ai_SOMUncloakMaxTimeCombat", &ai_threatModifiers.SOMUncloakMaxTimeCombat, 5.0f, 0,
		"Time the AI cannot react to a target uncloaking when the target is at their max sight range while alarmed.\n"
		"Usage: ai_SOMUncloakMaxTimeCombat 5.0\n"
		"Default is 5.0. A lower value will cause the AI to see the enemy sooner when uncloaking relative to how close the enemy is.");
	REGISTER_CVAR2("ai_CloakingDelay", &ai_CloakingDelay, 0.55f, 0,
		"Time it takes for AI perception to become effected by cloak.\n"
		"Usage: ai_CloakingDelay 0.7\n"
		"Default is 1.5.");
	REGISTER_CVAR2("ai_UnCloakingTime", &ai_UnCloakingDelay, 0.75f, 0,
		"Time it takes the player to uncloak, after which he becomes visible to the AI.\n"
		"Usage: ai_UnCloakingTime 0.9\n"
		"Default is 0.5.");
	REGISTER_CVAR2("ai_CompleteCloakDelay", &ai_CompleteCloakDelay, 2.0f, 0,
		"Time it takes the AI to decide cloaked target's position is lost.\n"
		"Usage: ai_CompleteCloakDelay 2.0\n"
		"Default is 0.5.");

	REGISTER_CVAR(ai_HazardsDebug, 0, VF_CHEAT,
		"[0-1] Enable/disable debug visualization of the hazard system.");

	REGISTER_CVAR2("ai_ProximityToHostileAlertnessIncrementThresholdDistance", &ai_ProximityToHostileAlertnessIncrementThresholdDistance, 10.0f, VF_CHEAT,
		"Threshold distance used to calculate the proximity to hostile target alertness increment.");

	REGISTER_CVAR(g_actorViewDistRatio, 127, 0, "Sets the view dist ratio for actors.\n");
	REGISTER_CVAR(g_playerLodRatio, 80, VF_REQUIRE_LEVEL_RELOAD, "Sets the lod ratio for players.\n");

	REGISTER_CVAR(g_itemsLodRatioScale, 1.0f, VF_NULL, "Sets the view dist ratio for items owned by AI/Player in SP.\n");
	REGISTER_CVAR(g_itemsViewDistanceRatioScale, 2.0f, VF_NULL, "Sets the view dist ratio for items owned by AI/Player in SP.\n");

	REGISTER_CVAR(g_hitDeathReactions_enable, 1, 0, "Enables/Disables Hit/Death reaction system");
	REGISTER_CVAR(g_hitDeathReactions_useLuaDefaultFunctions, 0, 0, "If enabled, it'll use the default lua methods inside HitDeathReactions script instead of the default c++ version");
	REGISTER_CVAR(g_hitDeathReactions_disable_ai, 1, 0, "If enabled, it'll not allow to execute any AI instruction during the hit/death reaction");
	REGISTER_CVAR(g_hitDeathReactions_debug, 0, 0, "Enables/Disables debug information for hit and death reactions system");
	REGISTER_CVAR(g_hitDeathReactions_disableRagdoll, 0, 0, "Disables switching to ragdoll at the end of animations");
	REGISTER_CVAR(g_hitDeathReactions_logReactionAnimsOnLoading, eHDRLRAT_DontLog, 0, "Non-Release only CVar: Enables logging of animations used by non-animation graph-based reactions. 0: don't log, 1: log anim names, 2: log filepaths");
	REGISTER_CVAR(g_hitDeathReactions_streaming, gEnv->bMultiplayer ? eHDRSP_EntityLifespanBased : eHDRSP_ActorsAliveAndNotInPool, 0, "Enables/Disables reactionAnims streaming. 0: Disabled, 1: DBA Registering-based, 2: Entity lifespan-based");
	REGISTER_CVAR(g_hitDeathReactions_usePrecaching, 1, 0, "Enables/Disables precaching of of hitreactions: Requires game restart.");

	REGISTER_CVAR(g_spectacularKill.maxDistanceError, 2.0f, 0, "Maximum error allowed from the optimal distance to the target");
	REGISTER_CVAR(g_spectacularKill.minTimeBetweenKills, 1.0f, 0, "Minimum time allowed between spectacular kills");
	REGISTER_CVAR(g_spectacularKill.minTimeBetweenSameKills, 10.0f, 0, "Minimum time allowed between spectacular kills using the same animation");
	REGISTER_CVAR(g_spectacularKill.minKillerToTargetDotProduct, 0.0f /*max angle of 90º*/, 0, "Minimum dot product allowed between the killer look dir and the distance to target vector");
	REGISTER_CVAR(g_spectacularKill.maxHeightBetweenActors, 0.2f, 0, "Maximum height distance between killer and target for the spectacular kill to be allowed");
	REGISTER_CVAR(g_spectacularKill.sqMaxDistanceFromPlayer, 900.0f /* max distance of 30 */, 0, "Square of the maximum distance in meters allowed between the target of the spectacular kill and the player. If set to a negative number is ignored");
	REGISTER_CVAR(g_spectacularKill.debug, 0, 0, "Enables/Disables debugging information for the spectacularKill");

	REGISTER_CVAR(g_movementTransitions_enable, 1, 0, "Enables/Disables special movement transitions system"); // disabled by default now until fully tested
	REGISTER_CVAR(g_movementTransitions_log, 0, 0, "Enables/Disables logging of special movement transitions");
	REGISTER_CVAR(g_movementTransitions_debug, 0, 0, "Enables/Disables on-screen debugging of movement transitions");

	REGISTER_CVAR(g_maximumDamage, -1.0f, 0, "Maximum health reduction allowed on actors. If negative it is ignored");
	REGISTER_CVAR(g_instantKillDamageThreshold, -1.0f, 0, "If positive, any damage caused greater than this value will kill the character");

	REGISTER_CVAR(g_muzzleFlashCull, 1, VF_NULL, "Enable muzzle flash culling");
	REGISTER_CVAR(g_muzzleFlashCullDistance, 30000.0f, 0, "Culls distant muzzle flashes");
	REGISTER_CVAR(g_rejectEffectVisibilityCull, 1, VF_NULL, "Enable reject effect culling");
	REGISTER_CVAR(g_rejectEffectCullDistance, 25.f*25.f, 0, "Culls distant shell casing effects");

	REGISTER_CVAR(g_flyCamLoop, 0, 0, "Toggles whether the flycam should loop at the end of playback");
	
#if (USE_DEDICATED_INPUT)
	REGISTER_CVAR(g_dummyPlayersFire, 0, 0, "Enable/Disable firing on the dummy players");
	REGISTER_CVAR(g_dummyPlayersMove, 0, 0, "Enable/Disable moving on the dummy players");
	REGISTER_CVAR(g_dummyPlayersChangeWeapon, 0, 0, "0 = no weapon change, 1 = sequential through weapon list, 2 = random weapons");
	REGISTER_CVAR(g_dummyPlayersJump, 0.5f, 0, "Control how often dummy players jump - 0.0 is never, 1.0 is whenever they're not crouching");
	REGISTER_CVAR(g_dummyPlayersRespawnAtDeathPosition, 0, 0, "0 = default respawn location, 1 = respawn at position where they were killed");
	REGISTER_CVAR(g_dummyPlayersCommitSuicide, 0, 0, "Enable/Disable killing (and respawning) dummy players who've been alive for too long");
	REGISTER_CVAR(g_dummyPlayersShowDebugText, 0, 0, "Enable/Disable on-screen messages about dummy players");
	REGISTER_CVAR(g_dummyPlayersMinInTime, -1.0F, 0, "Minimum time for dummy player to remain in game (seconds, < 0.0F to disable");
	REGISTER_CVAR(g_dummyPlayersMaxInTime, -1.0F, 0, "Maximum time for dummy player to remain in game (seconds, < 0.0F to disable");
	REGISTER_CVAR(g_dummyPlayersMinOutTime, -1.0F, 0, "Minimum time for dummy player to remain out of game (seconds, < 0.0F to disable");
	REGISTER_CVAR(g_dummyPlayersMaxOutTime, -1.0F, 0, "Maximum time for dummy player to remain out of game (seconds, < 0.0F to disable");
	g_dummyPlayersGameRules = REGISTER_STRING("g_dummyPlayersGameRules", "TeamInstantAction", 0, "Game rules for dummy player matchmaking");
	REGISTER_CVAR(g_dummyPlayersRanked, 1, 0, "Ranked flag for dummy player matchmaking");
#endif // #if (USE_DEDICATED_INPUT)

	REGISTER_CVAR(g_mpCullShootProbablyHits, 1, 0, "Culls ProbablyHits in Shoot function, to avoid line tests");

	REGISTER_CVAR(g_cloakRefractionScale, 1.0f, 0, "Sets cloak refraction scale");
	REGISTER_CVAR(g_cloakBlendSpeedScale, 1.0f, 0, "Sets cloak blend speed scale");

	REGISTER_CVAR(g_telemetry_onlyInGame, 1, 0, "Only capture performance & bandwidth telemetry when in a real in game state");
	REGISTER_CVAR(g_telemetry_drawcall_budget, 2000, 0, "drawcall budget");
	REGISTER_CVAR(g_telemetry_memory_display, 0, 0, "display some info on the memory usage of the gameplay stats circular buffer");
	REGISTER_CVAR(g_telemetry_memory_size_sp, 0, 0, "this is the size of the gameplay stats circular buffer used in bytes for singleplayer, 0 is unlimited");

#define DEFAULT_TELEMETRY_MEMORY_SIZE_MP 2*1024*1024
	REGISTER_CVAR(g_telemetry_memory_size_mp, DEFAULT_TELEMETRY_MEMORY_SIZE_MP, 0, "this is the size of the gameplay stats circular buffer used in bytes for multiplayer, 0 is unlimited");
	REGISTER_CVAR(g_telemetry_gameplay_enabled, 1, 0, "if telemetry is enabled are gameplay related telemetry stats being gathered");
	REGISTER_CVAR(g_telemetry_serialize_method, 1, 0, "method used to convert telemetry data structures to xml, 1 = chunked telemetry serializer (new), 2 = class build xml tree (mem hungry), 3 = both (for comparison)");
	REGISTER_CVAR(g_telemetryDisplaySessionId, 0, 0, "Displays the current telemetry session id to the screen");
	int		saveToDisk=1;
#if defined(_RELEASE)
	saveToDisk=0;
#endif
	REGISTER_CVAR(g_telemetry_gameplay_gzip, 1, 0, "gameplay telemetry is gzipped before uploading to the server");
	REGISTER_CVAR(g_telemetry_gameplay_save_to_disk, saveToDisk, 0, "gameplay telemetry is uploaded to the server, set this to also save to disk");
	REGISTER_CVAR(g_telemetry_gameplay_copy_to_global_heap, 1, 0, "compressed gameplay telemetry is copied to global heap when the session is ended to avoid corrupting the level heap");
	REGISTER_CVAR(g_telemetryEnabledSP, 0, 0, "Enables telemetry collection in singleplayer");
	REGISTER_CVAR(g_telemetrySampleRatePerformance, -1.0f, 0, "How often to gather performance telemetry statistics (negative to disable)");
	REGISTER_CVAR(g_telemetrySampleRateBandwidth, -1.0f, 0, "How often to gather bandwidth telemetry statistics (negative to disable)");
	REGISTER_CVAR(g_telemetrySampleRateMemory, -1.0f, 0, "How often to gather memory telemetry statistics (negative to disable)");
	REGISTER_CVAR(g_telemetrySampleRateSound, -1.0f, 0, "How often to gather sound telemetry statistics (negative to disable)");

	REGISTER_CVAR(g_telemetry_xp_event_send_interval, 1.0f, 0, "How often in seconds the client should send its XP events to the server for logging in the telemetry files");
	REGISTER_CVAR(g_telemetry_mp_upload_delay, 5.0f, 0, "How long to wait in seconds before uploading the statslog after the game ends in multiplayer");
	REGISTER_CVAR(g_dataRefreshFrequency, 1.0f, 0, "How many hours to wait before refreshing data from web server");
#if defined(DEDICATED_SERVER)
	REGISTER_CVAR(g_quitOnNewDataFound, 1, 0, "Close the server down if a new data patch is found on the web server");
	REGISTER_CVAR(g_quitNumRoundsWarning, 3, 0, "Number of rounds to wait before closing the server down when a new data patch is available (only applicable if g_quitOnNewDataFound = 1)");
	REGISTER_CVAR(g_allowedDataPatchFailCount, 1, 0, "Number of times downloading of data patch can fail before restarting the server (only applicable if g_quitOnNewDataFound = 1)");
	REGISTER_CVAR(g_shutdownMessageRepeatTime, 60.0f, 0, "Frequency (in seconds) at which messages are sent out to clients warning them of server shutdown (only applicable if g_quitOnNewDataFound = 1)");
	g_shutdownMessage=REGISTER_STRING("g_shutdownMessage", "Server shutdown in \\1 rounds", 0, "Server shutdown message (\\1 will be replaced with the number of rounds remaining)");
#endif

	// game mode variant cvars - these will be reset every time a different playlist or variant is chosen and then set according to the newly chosen variant
	// -- pro
	REGISTER_CVAR(g_modevarivar_proHud, 0, 0, "Cuts back HUD elements to essentials");
	REGISTER_CVAR(g_modevarivar_disableKillCam, 0, 0, "Disables the kill cam");
	REGISTER_CVAR(g_modevarivar_disableSpectatorCam, 0, 0, "Disables spectator cam when you die");
	// -- vanilla
#if !defined(_RELEASE)
	g_dataCentreConfigCVar=REGISTER_STRING("g_data_centre_config",DATA_CENTRE_DEFAULT_CONFIG,VF_REQUIRE_APP_RESTART, "high level config switch of which data centre to use for telemetry and web data");
	g_downloadMgrDataCentreConfigCVar=REGISTER_STRING("g_download_mgr_data_centre_config", DOWNLOAD_MGR_DATA_CENTRE_DEFAULT_CONFIG, VF_REQUIRE_APP_RESTART, "high level config switch of which data centre to use for downloadmgr");
#else
	g_dataCentreConfigStr=DATA_CENTRE_DEFAULT_CONFIG;
	g_downloadMgrDataCentreConfigStr=DOWNLOAD_MGR_DATA_CENTRE_DEFAULT_CONFIG;
#endif
	
	REGISTER_CVAR(g_ignoreDLCRequirements, 1, 0, "Shows all servers in the game browser even if the DLC requirements are not met");
	REGISTER_CVAR(sv_netLimboTimeout, 2.f, 0, "Time to wait before considering a channel stalled");
	REGISTER_CVAR(g_idleKickTime, 120.f, 0, "Time to wait before kicking a player for being idle for too long (in seconds)");

  REGISTER_CVAR(g_stereoIronsightWeaponDistance, 0.375f, 0, "Distance of convergence plane when in ironsight");
  REGISTER_CVAR(g_stereoFrameworkEnable  , 1 , VF_NULL, "Enables the processing of the game stereo framework. (2=extra debug output)");
	
	REGISTER_CVAR(g_useOnlineServiceForDedicated, 0, 0, "Use Online Service For Dedicated Server (defaults to off = LAN)");

#if USE_LAGOMETER
	REGISTER_CVAR_CB(net_graph, 0, 0, "Display a graph showing download/upload bandwidth (0 = off, 1 = download, 2 = upload, 3 = both)", OnNetGraphChanged);
#endif
	REGISTER_CVAR(g_enablePoolCache, 1, 0, "Enable the caching of data associated with entities pooled e.g., equipment pack, body damage, etc.");
	REGISTER_CVAR(g_setActorModelFromLua, 0, 0, "Toggle if the actor model should be set from Lua or internally");
	REGISTER_CVAR(g_loadPlayerModelOnLoad, 1, 0, "Sets if the client player's model should be loaded on level load");
	REGISTER_CVAR(g_enableActorLuaCache, 1, 0, "Enable the caching of actor properties from Lua to avoid Lua access at run-time");

	REGISTER_CVAR(g_enableSlimCheckpoints, 0, 0, "Enable the use of console style checkpoints instead of full save.");

	REGISTER_CVAR(g_mpLoaderScreenMaxTimeSoftLimit, 15.f, 0, "Max time mp loader screen will wait for optional loading tasks before moving on (eg patch and playlist updates) should be < g_mpLoaderScreenMaxTime or optional tasks will cause an abort");
	REGISTER_CVAR(g_mpLoaderScreenMaxTime, 25.f, 0, "Max time mp loader screen is allowed to wait before timing out and aborting MP");
	REGISTER_CVAR2("g_telemetryTags", &g_telemetryTags, "", 0, "A list of comma seperated tags to apply to the telemetry session (e.g. nettest,qa)");
	REGISTER_CVAR2_CB("g_telemetryConfig", &g_telemetryConfig, "none", 0, "Sets the telemetry config to use (see Libs/Telemetry/).", OnTelemetryConfigChanged);
	REGISTER_CVAR2("g_telemetryEntityClassesToExport", &g_telemetryEntityClassesToExport, "", 0, "Specifies additional entity classes to export to minimap file\nfor viewing in StatsTool.\nSeperate with commas, eg \"AmmoCrate,MissionObjective\"");
	
	
#if !defined(_RELEASE)
	REGISTER_CVAR2("pl_viewFollow", &pl_viewFollow, "", VF_CHEAT, "Follow an entity, by name");
	REGISTER_CVAR(pl_viewFollowOffset, 1.3f, VF_CHEAT, "Vertical offset on entity to follow");
	REGISTER_CVAR(pl_viewFollowMinRadius, 2.f, VF_CHEAT, "Min radius for follow cam from target");
	REGISTER_CVAR(pl_viewFollowMaxRadius, 5.f, VF_CHEAT, "Max radius for follow cam from target");
	REGISTER_CVAR(menu_forceShowPii, 0, VF_CHEAT, "Force show Pii Screen on SP->MP transisiton. Can't unset entitlement, just for viewing" );
#endif

	// MP kickable cars
	REGISTER_CVAR(g_mpKickableCars, 1, 0, "Turn on MP kickable car settings/Disable SP kickable car settings");

#if PAIRFILE_GEN_ENABLED
	REGISTER_COMMAND("g_generateAnimPairFile", Generate1P3PPairFiles, VF_CHEAT, "Saves out the anim pair file used to hook up 1P & 3P animations");
#endif //PAIRFILE_GEN_ENABLED
	REGISTER_COMMAND("g_setcontrollerlayout", SetControllerLayout, VF_CHEAT, "Sets controller layout (params: type(button,stick) name(default,lefty, etc.. See Libs/Config/controller");

	REGISTER_CVAR(g_forceItemRespawnTimer, 0.f, 0, "Override the amount of time it takes to respawn an item (0=disable override)");
	REGISTER_CVAR(g_defaultItemRespawnTimer, 5.f, 0, "Default amount of time to respawn an item - used if the actual timer length is lost in a host migration");

	REGISTER_CVAR(g_updateRichPresenceInterval, 15.f, 0, "Interval rich presence should wait if it fails");

	REGISTER_CVAR(g_useNetSyncToSpeedUpRMIs, 1, 0, "Speed up hit and kill RMIs to reduce network lag");
	
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
  REGISTER_CVAR(g_testStatus, 0, 0, "Provided as a mechanism for tests to signal their status to Amble scripts");
#endif

	// VTOL Vehicles
	REGISTER_CVAR(g_VTOLVehicleManagerEnabled, 1, 0, "Enable VTOLVehicle manager update logic");
	REGISTER_CVAR(g_VTOLMaxGroundVelocity, 50.f, 0, "Players standing inside the vtol will stick to it up to this speed faster than by default");
	REGISTER_CVAR(g_VTOLDeadStateRemovalTime, 10.f, 0, "Once a VTOL has been destroyed and crash lands it will hang around for x seconds");
	REGISTER_CVAR(g_VTOLDestroyedContinueVelocityTime, 5.f, 0, "When the VTOL is destroyed it will continue accelerating in its current direction for x seconds");
	REGISTER_CVAR(g_VTOLRespawnTime, 10.f, 0, "How many seconds to wait after a VTOL is destroyed before spawning a new one");
	REGISTER_CVAR(g_VTOLGravity, 0.75f, 0, "How fast the VTOL falls post-destruction");
	REGISTER_CVAR(g_VTOLPathingLookAheadDistance, 10.f, 0, "How far ahead the vehicle starts looking at when looking for the next node to ravel to");
	REGISTER_CVAR(g_VTOLOnDestructionPlayerDamage, 1000.f, 0, "When the VTOL is destroyed any players inside will receive this much damage");
	REGISTER_CVAR(g_VTOLInsideBoundsScaleX, 0.3f, 0, "scale of the VTOLs bounds to get a bounds more representative of the internal cabin");
	REGISTER_CVAR(g_VTOLInsideBoundsScaleY, 0.65f, 0, "scale of the VTOLs bounds to get a bounds more representative of the internal cabin");
	REGISTER_CVAR(g_VTOLInsideBoundsScaleZ, 0.5f, 0, "scale of the VTOLs bounds to get a bounds more representative of the internal cabin");
	REGISTER_CVAR(g_VTOLInsideBoundsOffsetZ, 0.25f, 0, "offset the VTOLs bounds in the z direction");

#ifndef _RELEASE
	REGISTER_CVAR(g_VTOLVehicleManagerDebugEnabled, 0, 0, "Enable VTOLVehicle manager Debug logic");
	REGISTER_CVAR(g_VTOLVehicleManagerDebugPathingEnabled, 0, 0, "Enable VTOLVehicle manager Debug logic");
	REGISTER_CVAR(g_VTOLVehicleManagerDebugSimulatePath, 0, 0, "Simulate VTOLs moving around the path to provide a more accurate debug drawing of the VTOLs path");
	REGISTER_CVAR(g_VTOLVehicleManagerDebugSimulatePathTimestep, 0.1f, 0, "Time increment when simulating movement. Lower = more accurate but slower");
	REGISTER_CVAR(g_VTOLVehicleManagerDebugSimulatePathMinDotValueCheck, 0.99f, 0, "If the dot from 3 neighboring nodes is greater than this value we can disregard the middle node (Higher = more accurate but more memory/render work)");
#endif
	// ~VTOL Vehicles

#ifndef _RELEASE
	REGISTER_CVAR(g_mptrackview_debug, 0, 0, "Enable/Disable debugging of trackview syncing in multiplayer");
	REGISTER_CVAR(g_hud_postgame_debug, 0, 0, "Enable/Disable debugging of post game state");
	// Interactive objects
	REGISTER_CVAR(g_interactiveObjectDebugRender, 0, 0, "Enable/Disable the rendering of debug visuals for interactive object e.g. helper cones + radii");

#endif //#ifndef _RELEASE

	REGISTER_CVAR(g_highlightingMaxDistanceToHighlightSquared, 100.f, 0, "How close interactive entities should be before highlighting");
	REGISTER_CVAR(g_highlightingMovementDistanceToUpdateSquared, 0.025f, 0, "How far the player needs to move before reassessing nearby interactive entities");
	REGISTER_CVAR(g_highlightingTimeBetweenForcedRefresh, 1.f, 0, "Maximum time before entities are re-checked for highlight requirements");
	
	REGISTER_CVAR(g_ledgeGrabClearHeight, 0.2f, 0,	"Distance a player must clear a ledge grab entity by to avoid doing a ledge grab");
	REGISTER_CVAR(g_ledgeGrabMovingledgeExitVelocityMult, 1.5f, 0,	"On exiting a grab onto a moving ledge a movement will be added to the players exit velocity in the direction of the ledge movement");
	REGISTER_CVAR(g_vaultMinHeightDiff, 1.2f, 0,	"Minimum height difference between a player and a ledge for a vault to be possible");
	REGISTER_CVAR(g_vaultMinAnimationSpeed, 0.7f, 0, "When vaulting at slow speeds the resulting animation speed will never go beneath this value");

	// Lower weapon during stealth
	REGISTER_CVAR(g_useQuickGrenadeThrow, 2, 0, "Use left shoulder button for quick grenade throws, 1=always, 2=mp only");
	REGISTER_CVAR(g_enableMPDoubleTapGrenadeSwitch, 1, 0, "Double tapping Y will switch to your grenades/explosives");
	REGISTER_CVAR(g_cloakFlickerOnRun, 0, 0, "Use cloak flickering effect when running, 1=always, 2=mp only");
	
	REGISTER_CVAR_DEV_ONLY(kc_useAimAdjustment, 1, VF_NULL, "Enable aim adjustment for lag reduction in the killcam");
	REGISTER_CVAR_DEV_ONLY(kc_aimAdjustmentMinAngle, 0.785f, VF_NULL, "Aim angle in radians within which aim adjustment is applied");
	REGISTER_CVAR_DEV_ONLY(kc_precacheTimeStartPos, 0.5f, VF_NULL, "Time before playback to add pre-cached cam point.");
	REGISTER_CVAR_DEV_ONLY(kc_precacheTimeWeaponModels, 1.5f, VF_NULL, "Time before playback to pre-cache the weapons involved.");

#ifndef _RELEASE
	REGISTER_CVAR(g_vtolManagerDebug, 0, 0, "1: Enable debug options in the vtol manager");

	REGISTER_CVAR(g_mpPathFollowingRenderAllPaths, 0, 0, "Render debug information for all paths in the current level");
	REGISTER_CVAR(g_mpPathFollowingNodeTextSize, 1.f, 0, "The size of the per-node information for the paths");
#endif

	REGISTER_CVAR(g_minPlayersForRankedGame, 4, VF_NET_SYNCED, "Amount of players required before a game actually starts");
	REGISTER_CVAR(g_gameStartingMessageTime, 10.f, 0, "Time between messages telling the player that the game hasn't started yet");

	g_presaleURL=REGISTER_STRING("g_presaleURL","http://www.mycrysis.com/redeemcode/",VF_CHEAT|VF_READONLY, "url to redirect presale users to");

	g_messageOfTheDay=REGISTER_STRING("g_messageOfTheDay","",VF_INVISIBLE, " message of the day");
	g_serverImageUrl=REGISTER_STRING("g_serverImageUrl","",VF_INVISIBLE, "server image");

#ifndef _RELEASE
	REGISTER_CVAR(g_disableSwitchVariantOnSettingChanges, 0, 0, "If set to true settings can be changed and the playlist manager will not flag game as custom variant - dev only - allows testing of full functionality");
	REGISTER_CVAR(g_startOnQuorum, 1, 0, "Start on quorum: 0 never, 1 default, 2 always");
#endif //#ifndef _RELEASE

	NetInputChainInitCVars();

	CGodMode::GetInstance().RegisterConsoleVars(pConsole);

	CBodyManagerCVars::RegisterVariables();

	InitAIPerceptionCVars(pConsole);

#ifdef INCLUDE_GAME_AI_RECORDER
	CGameAIRecorderCVars::RegisterVariables();
#endif //INCLUDE_GAME_AI_RECORDER

	REGISTER_CVAR(g_debugWeaponOffset, 0, 0, "0 - disables any weapon sway debug\n1 - shows cross hair guide lines to help aligning the weapon\n2 - will override player input to allow to manually align the weapon");
	REGISTER_COMMAND("g_weaponOffsetReset", WeaponOffsetReset, 0, "Resets all weapon offsets");
	REGISTER_COMMAND("g_weaponOffsetOutput", WeaponOffsetOutput, 0, "Outputs to the console the current weapon");
	REGISTER_COMMAND("g_weaponOffsetInput", WeaponOffsetInput, 0, "Restores the weapon offset");
	REGISTER_COMMAND("g_weaponOffsetToMannequin", WeaponOffsetToMannequin, 0, "Moves the current weapon offset into current cry mannequin's selected key. g_weaponOffsetToMannequin left will select the left hand offset.");
	REGISTER_COMMAND("g_weaponZoomIn", WeaponZoomIn, 0, "Zooms in the weapon, useful when when seting up offsets");
	REGISTER_COMMAND("g_weaponZoomOut", WeaponZoomOut, 0, "Zooms out the weapon");
#if ENABLE_RMI_BENCHMARK
	REGISTER_CVAR( g_RMIBenchmarkInterval, 0, 0, "RMI benchmark interval (milliseconds)" );
	REGISTER_CVAR( g_RMIBenchmarkTwoTrips, 0, 0, "RMI benchmark should do two round trips?" );
#endif

	// 3d hud
	REGISTER_CVAR(g_hud3d_cameraDistance, 1.f, VF_NULL, "3D Hud distance to camera");
	REGISTER_CVAR(g_hud3d_cameraOffsetZ, 0.f, VF_NULL, "3D Hud z pos offset");
	REGISTER_CVAR(g_hud3D_cameraOverride, 0, VF_NULL, "if true g_hud3d_cameraDistance and g_hud3d_cameraOffsetZ can be used to change offsets, otherwise auto offsets");
	
	m_pGameLobbyCVars = new CGameLobbyCVars();
}


//------------------------------------------------------------------------
void SCVars::ReleaseAIPerceptionCVars(IConsole* pConsole)
{
	pConsole->UnregisterVariable("ai_perception.movement_useSurfaceType", true);
	pConsole->UnregisterVariable("ai_perception.movement_movingSurfaceDefault", true);
	pConsole->UnregisterVariable("ai_perception.movement_standingRadiusDefault", true);
	pConsole->UnregisterVariable("ai_perception.movement_crouchRadiusDefault", true);
	pConsole->UnregisterVariable("ai_perception.movement_standingMovingMultiplier", true);
	pConsole->UnregisterVariable("ai_perception.movement_crouchMovingMultiplier", true);
	pConsole->UnregisterVariable("ai_perception.landed_baseRadius", true);
	pConsole->UnregisterVariable("ai_perception.landed_speedMultiplier", true);
}

//------------------------------------------------------------------------
void SCVars::ReleaseCVars()
{
	IConsole* pConsole = gEnv->pConsole;

	pConsole->UnregisterVariable("g_ProcessOnlineCallbacks", true);

	pConsole->UnregisterVariable("g_enableLanguageSelectionMenu", true);
	pConsole->UnregisterVariable("g_multiplayerDefault", true);
	pConsole->UnregisterVariable("g_multiplayerModeOnly", true);

	pConsole->UnregisterVariable("g_frontEndUnicodeInput", true);
	pConsole->UnregisterVariable("g_EnableDevMenuOptions", true);
	pConsole->UnregisterVariable("g_EnablePersistantStatsDebugScreen", true);

	pConsole->UnregisterVariable("g_EPD", true);
	pConsole->UnregisterVariable("g_frontEndRequiredEPD", true);
	pConsole->UnregisterVariable("g_MatchmakingVersion", true);
	pConsole->UnregisterVariable("g_MatchmakingBlock", true);

	pConsole->UnregisterVariable("g_craigNetworkDebugLevel", true);
	pConsole->UnregisterVariable("g_presaleUnlock", true);
	pConsole->UnregisterVariable("g_dlcPurchaseOverwrite", true);

	pConsole->UnregisterVariable("g_enableInitialLoadoutScreen", true);
	pConsole->UnregisterVariable("g_post3DRendererDebug", true);
	pConsole->UnregisterVariable("g_post3DRendererDebugGridSegmentCount", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("g_debugTestProtectedScripts", true);
#endif

	pConsole->UnregisterVariable("cl_fov", true);
	pConsole->UnregisterVariable("cl_mp_fov_scalar", true);
	pConsole->UnregisterVariable("cl_tpvCameraCollision", true);
	pConsole->UnregisterVariable("cl_tpvCameraCollisionOffset", true);
	pConsole->UnregisterVariable("cl_tpvDebugAim", true);

	pConsole->UnregisterVariable("cl_tpvDist", true);
	pConsole->UnregisterVariable("cl_tpvDistHorizontal", true);
	pConsole->UnregisterVariable("cl_tpvDistVertical", true);
	pConsole->UnregisterVariable("cl_tpvInterpolationSpeed", true);
	pConsole->UnregisterVariable("cl_tpvMaxAimDist", true);
	pConsole->UnregisterVariable("cl_tpvMinDist", true);
	pConsole->UnregisterVariable("cl_tpvYaw", true);
	pConsole->UnregisterVariable("cl_sensitivity", true);
	pConsole->UnregisterVariable("cl_sensitivityController", true);
	pConsole->UnregisterVariable("cl_sensitivityControllerMP", true);
	pConsole->UnregisterVariable("cl_invertMouse", true);
	pConsole->UnregisterVariable("cl_invertController", true);
	pConsole->UnregisterVariable("cl_invertLandVehicleInput", true);
	pConsole->UnregisterVariable("cl_invertAirVehicleInput", true);
	pConsole->UnregisterVariable("cl_zoomToggle", true);
	pConsole->UnregisterVariable("cl_crouchToggle", true);
	pConsole->UnregisterVariable("cl_sprintToggle", true);
	pConsole->UnregisterVariable("cl_logAsserts", true);
	pConsole->UnregisterVariable("cl_idleBreaksDelayTime", true);
	pConsole->UnregisterVariable("cl_postUpdateCamera", true);
	pConsole->UnregisterVariable("hud_canvas_width_adjustment", true);

	pConsole->UnregisterVariable("g_infiniteAmmoTutorialMode", true);

	pConsole->UnregisterVariable("i_lighteffects", true);
	pConsole->UnregisterVariable("i_particleeffects", true);
	pConsole->UnregisterVariable("i_rejecteffects", true);

	pConsole->UnregisterVariable("i_offset_front", true);
	pConsole->UnregisterVariable("i_offset_up", true);
	pConsole->UnregisterVariable("i_offset_right", true);

	pConsole->UnregisterVariable("i_grenade_showTrajectory", true);
	pConsole->UnregisterVariable("i_grenade_trajectory_resolution", true);
	pConsole->UnregisterVariable("i_grenade_trajectory_dashes", true);
	pConsole->UnregisterVariable("i_grenade_trajectory_gaps", true);
	pConsole->UnregisterVariable("i_grenade_trajectory_useGeometry", true);

	pConsole->UnregisterVariable("i_laser_hitPosOffset", true);

	pConsole->UnregisterVariable("i_failedDetonation_speedMultiplier", true);
	pConsole->UnregisterVariable("i_failedDetonation_lifetime", true);

	pConsole->UnregisterVariable("i_hmg_detachWeaponAnimFraction", true);
	pConsole->UnregisterVariable("i_hmg_impulseLocalDirection_x", true);
	pConsole->UnregisterVariable("i_hmg_impulseLocalDirection_y", true);
	pConsole->UnregisterVariable("i_hmg_impulseLocalDirection_z", true);

	pConsole->UnregisterVariable("cl_motionBlurVectorScale", true);
	pConsole->UnregisterVariable("cl_motionBlurVectorScaleSprint", true);

	pConsole->UnregisterVariable("i_ironsight_while_jumping_mp", true);
	pConsole->UnregisterVariable("i_ironsight_while_falling_mp", false);
	pConsole->UnregisterVariable("i_ironsight_falling_unzoom_minAirTime", false);
	pConsole->UnregisterVariable("i_weapon_customisation_transition_time", false);
	pConsole->UnregisterVariable("i_highlight_dropped_weapons", true);
	
	pConsole->UnregisterVariable("cl_slidingBlurRadius", true);
	pConsole->UnregisterVariable("cl_slidingBlurAmount", true);
	pConsole->UnregisterVariable("cl_slidingBlurBlendSpeed", true);

	pConsole->UnregisterVariable("cl_debugSwimming", true);
	pConsole->UnregisterVariable("cl_angleLimitTransitionTime", true);

	pConsole->UnregisterVariable("cl_shallowWaterSpeedMulPlayer", true);
	pConsole->UnregisterVariable("cl_shallowWaterSpeedMulAI", true);
	pConsole->UnregisterVariable("cl_shallowWaterDepthLo", true);
	pConsole->UnregisterVariable("cl_shallowWaterDepthHi", true);

	pConsole->UnregisterVariable("p_collclassdebug", true);

	pConsole->UnregisterVariable("pl_cameraTransitionTime", true);
	pConsole->UnregisterVariable("pl_cameraTransitionTimeLedgeGrabIn", true); 
	pConsole->UnregisterVariable("pl_cameraTransitionTimeLedgeGrabOut", true); 

	pConsole->UnregisterVariable("pl_inputAccel", true);
	pConsole->UnregisterVariable("pl_debug_energyConsumption", true);
	pConsole->UnregisterVariable("pl_debug_pickable_items", true);
	pConsole->UnregisterVariable("pl_debug_projectileAimHelper", true); 
	pConsole->UnregisterVariable("pl_useItemHoldTime", true);

	pConsole->UnregisterVariable("pl_nanovision_timeToRecharge", true);
	pConsole->UnregisterVariable("pl_nanovision_timeToDrain", true);
	pConsole->UnregisterVariable("pl_nanovision_minFractionToUse", true);

	pConsole->UnregisterVariable("pl_autoPickupItemsWhenUseHeld", true);
	pConsole->UnregisterVariable("pl_autoPickupMinTimeBetweenPickups", true);
	pConsole->UnregisterVariable("pl_refillAmmoDelay", true);

	pConsole->UnregisterVariable("pl_spawnCorpseOnDeath", true);
	pConsole->UnregisterVariable("pl_doLocalHitImpulsesMP", true);

	pConsole->UnregisterVariable("g_hunterIK", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("pl_watch_projectileAimHelper", true);
	pConsole->UnregisterVariable("g_preloadUIAssets", true);
	pConsole->UnregisterVariable("g_gameRules_skipStartTimer");
	pConsole->UnregisterVariable("pl_debug_vistableIgnoreEntities", true);
	pConsole->UnregisterVariable("pl_debug_vistableAddEntityId", true);
	pConsole->UnregisterVariable("pl_debug_vistableRemoveEntityId", true);
#endif

	pConsole->UnregisterVariable("pl_debug_hit_recoil", true);
	pConsole->UnregisterVariable("pl_debug_look_poses", true);

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	pConsole->UnregisterVariable("g_gameRules_StartCmd", true);
#endif

	pConsole->UnregisterVariable("g_gameRules_startTimerLength", true);

	pConsole->UnregisterVariable("g_gameRules_postGame_HUDMessageTime", true);
	pConsole->UnregisterVariable("g_gameRules_postGame_Top3Time", true);
	pConsole->UnregisterVariable("g_gameRules_postGame_ScoreboardTime", true);

	pConsole->UnregisterVariable("g_gameRules_startTimerMinPlayers", true);
	pConsole->UnregisterVariable("g_gameRules_startTimerMinPlayersPerTeam", true);
	pConsole->UnregisterVariable("g_gameRules_startTimerPlayersRatio", true);
	pConsole->UnregisterVariable("g_gameRules_startTimerOverrideWait", true);
	pConsole->UnregisterVariable("g_gameRules_preGame_StartSpawnedFrozen", true);
	
		
	pConsole->UnregisterVariable("g_flashBangMinSpeedMultiplier", true);
	pConsole->UnregisterVariable("g_flashBangSpeedMultiplierFallOffEase", true);
	pConsole->UnregisterVariable("g_flashBangNotInFOVRadiusFraction", true);
	pConsole->UnregisterVariable("g_flashBangMinFOVMultiplier", true);
#ifndef _RELEASE
	pConsole->UnregisterVariable("g_flashBangFriends", true);
	pConsole->UnregisterVariable("g_flashBangSelf", true);
#endif
	pConsole->UnregisterVariable("g_friendlyLowerItemMaxDistance", true);

	pConsole->UnregisterVariable("g_holdObjectiveDebug", true);
	pConsole->UnregisterVariable("g_difficultyLevel", true);
	pConsole->UnregisterVariable("g_difficultyLevelLowestPlayed", true);
	pConsole->UnregisterVariable("g_enableFriendlyFallAndPlay", true);
	pConsole->UnregisterVariable("g_enableFriendlyAIHits", true);
	pConsole->UnregisterVariable("g_enableFriendlyPlayerHits", true);
	pConsole->UnregisterVariable("g_gameRayCastQuota", true);
	pConsole->UnregisterVariable("g_gameIntersectionTestQuota", true);
	
	pConsole->UnregisterVariable("g_STAPCameraAnimation", true);
	
	pConsole->UnregisterVariable("g_mpAllSeeingRadar", true);
	pConsole->UnregisterVariable("g_mpAllSeeingRadarSv", true);
	pConsole->UnregisterVariable("g_mpDisableRadar", true);
	pConsole->UnregisterVariable("g_mpNoEnemiesOnRadar", true);
	pConsole->UnregisterVariable("g_mpHatsBootsOnRadar", true);

	pConsole->UnregisterVariable("g_playerLowHealthThreshold", true);
	pConsole->UnregisterVariable("g_playerMidHealthThreshold", true);

	pConsole->UnregisterVariable("g_radialBlur", true);

	pConsole->UnregisterVariable("g_aiCorpses_DebugDraw", true);
	pConsole->UnregisterVariable("g_aiCorpses_DelayTimeToSwap", true);
	pConsole->UnregisterVariable("g_aiCorpses_Enable", true);
	pConsole->UnregisterVariable("g_aiCorpses_MaxCorpses", true);
	pConsole->UnregisterVariable("g_aiCorpses_CullPhysicsDistance", true);
	pConsole->UnregisterVariable("g_aiCorpses_ForceDeleteDistance", true);

	pConsole->UnregisterVariable("g_SurvivorOneVictoryConditions_watchLvl", true);
	pConsole->UnregisterVariable("g_SimpleEntityBasedObjective_watchLvl", true);
	pConsole->UnregisterVariable("g_KingOfTheHillObjective_watchLvl", true);
	pConsole->UnregisterVariable("g_HoldObjective_secondsBeforeStartForSpawn", true);	

	pConsole->UnregisterVariable("g_CombiCaptureObjective_watchLvl", true);
	pConsole->UnregisterVariable("g_CombiCaptureObjective_watchTerminalSignalPlayers", true);

	pConsole->UnregisterVariable("g_disable_OpponentsDisconnectedGameEnd", true);
	pConsole->UnregisterVariable("g_victoryConditionsDebugDrawResolution", true);

#if USE_PC_PREMATCH
	pConsole->UnregisterVariable("g_restartWhenPrematchFinishes", true);
#endif

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_predator_forceSpawnTeam", true);
	pConsole->UnregisterVariable("g_predator_predatorHasSuperRadar", true);
	pConsole->UnregisterVariable("g_predator_soldierHasTargetedWarning", true);
	pConsole->UnregisterVariable("g_predator_forcePredator1", true);
	pConsole->UnregisterVariable("g_predator_forcePredator2", true);
	pConsole->UnregisterVariable("g_predator_forcePredator3", true);
	pConsole->UnregisterVariable("g_predator_forcePredator4", true);
#endif

	pConsole->UnregisterVariable("g_predator_showBeingTargetedWarning", true);
	pConsole->UnregisterVariable("g_predator_showBeingTargetedDirection", true);
	pConsole->UnregisterVariable("g_predator_marineRedCrosshairDelay", true);

	pConsole->UnregisterVariable("cl_speedToBobFactor", true);
	pConsole->UnregisterVariable("cl_bobWidth", true);
	pConsole->UnregisterVariable("cl_bobHeight", true);
	pConsole->UnregisterVariable("cl_bobSprintMultiplier", true);
	pConsole->UnregisterVariable("cl_bobVerticalMultiplier", true);
	pConsole->UnregisterVariable("cl_bobMaxHeight", true);
	pConsole->UnregisterVariable("cl_strafeHorzScale", true);

	pConsole->UnregisterVariable("cl_controllerYawSnapEnable", true);
	pConsole->UnregisterVariable("cl_controllerYawSnapAngle", true);
	pConsole->UnregisterVariable("cl_controllerYawSnapTime", true);
	pConsole->UnregisterVariable("cl_controllerYawSnapMax", true);
	pConsole->UnregisterVariable("cl_controllerYawSnapMin", true);

	pConsole->UnregisterVariable("g_hmdFadeoutNearWallThreshold", true);

	pConsole->UnregisterVariable("g_timelimit", true);
	pConsole->UnregisterVariable("g_timelimitextratime", true);
	pConsole->UnregisterVariable("g_roundScoreboardTime", true);
	pConsole->UnregisterVariable("g_roundStartTime", true);
	pConsole->UnregisterVariable("g_roundlimit", true);
	pConsole->UnregisterVariable("g_friendlyfireratio", true);
	pConsole->UnregisterVariable("g_revivetime", true);
	pConsole->UnregisterVariable("g_minplayerlimit", true);
	pConsole->UnregisterVariable("g_debugNetPlayerInput", true);
	pConsole->UnregisterVariable("g_debug_fscommand", true);
	pConsole->UnregisterVariable("g_skipIntro", true);
	pConsole->UnregisterVariable("g_skipAfterLoadingScreen", true);
	pConsole->UnregisterVariable("g_goToCampaignAfterTutorial", true);
	pConsole->UnregisterVariable("kc_enable", true);
#ifndef _RELEASE
	pConsole->UnregisterVariable("g_skipStartupSignIn", true);
	pConsole->UnregisterVariable("g_debugOffsetCamera", true);
	pConsole->UnregisterVariable("g_debugLongTermAwardDays", true);
	pConsole->UnregisterVariable("kc_debug", true);
	pConsole->UnregisterVariable("kc_debugStressTest", true);
	pConsole->UnregisterVariable("kc_debugVictimPos", true);
	pConsole->UnregisterVariable("kc_debugWinningKill", true);
	pConsole->UnregisterVariable("kc_debugSkillKill", true);
	pConsole->UnregisterVariable("kc_debugMannequin", true);
	pConsole->UnregisterVariable("kc_debugPacketData", true);
	pConsole->UnregisterVariable("kc_debugStream", true);
#endif
	pConsole->UnregisterVariable("kc_memStats", true);
	pConsole->UnregisterVariable("kc_length", true);
	pConsole->UnregisterVariable("kc_skillKillLength", true);
	pConsole->UnregisterVariable("kc_bulletSpeed", true);
	pConsole->UnregisterVariable("kc_bulletHoverDist", true);
	pConsole->UnregisterVariable("kc_bulletHoverTime", true);
	pConsole->UnregisterVariable("kc_bulletHoverTimeScale", true);
	pConsole->UnregisterVariable("kc_bulletPostHoverTimeScale", true);
	pConsole->UnregisterVariable("kc_bulletTravelTimeScale", true);
	pConsole->UnregisterVariable("kc_bulletCamOffsetX", true);
	pConsole->UnregisterVariable("kc_bulletCamOffsetY", true);
	pConsole->UnregisterVariable("kc_bulletCamOffsetZ", true);
	pConsole->UnregisterVariable("kc_bulletRiflingSpeed", true);
	pConsole->UnregisterVariable("kc_bulletZoomDist", true);
	pConsole->UnregisterVariable("kc_bulletZoomTime", true);
	pConsole->UnregisterVariable("kc_bulletZoomOutRatio", true);
	pConsole->UnregisterVariable("kc_kickInTime", true);
	pConsole->UnregisterVariable("kc_maxFramesToPlayAtOnce", true);
	pConsole->UnregisterVariable("kc_cameraCollision", true);
	pConsole->UnregisterVariable("kc_showHighlightsAtEndOfGame", true);
	pConsole->UnregisterVariable("kc_enableWinningKill", true);
	pConsole->UnregisterVariable("kc_canSkip", true);
	pConsole->UnregisterVariable("kc_projectileDistance", true);
	pConsole->UnregisterVariable("kc_projectileHeightOffset", true);
	pConsole->UnregisterVariable("kc_largeProjectileDistance", true);
	pConsole->UnregisterVariable("kc_largeProjectileHeightOffset", true);
	pConsole->UnregisterVariable("kc_projectileVictimHeightOffset", true);
	pConsole->UnregisterVariable("kc_projectileMinimumVictimDist", true);
	pConsole->UnregisterVariable("kc_smoothing", true);
	pConsole->UnregisterVariable("kc_grenadeSmoothingDist", true);
	pConsole->UnregisterVariable("kc_cameraRaiseHeight", true);
	pConsole->UnregisterVariable("kc_resendThreshold", true);
	pConsole->UnregisterVariable("kc_chunkStreamTime", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("kc_copyKillCamIntoHighlightsBuffer", true);
#endif

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("kc_saveKillCams", true);
#endif

	pConsole->UnregisterVariable("g_multikillTimeBetweenKills", true);
	pConsole->UnregisterVariable("g_flushed_timeBetweenGrenadeBounceAndSkillKill", true);
	pConsole->UnregisterVariable("g_gotYourBackKill_FOVRange", true);
	pConsole->UnregisterVariable("g_gotYourBackKill_targetDistFromFriendly", true);
	pConsole->UnregisterVariable("g_guardian_maxTimeSinceLastDamage", true);
	pConsole->UnregisterVariable("g_defiant_timeAtLowHealth", true);
	pConsole->UnregisterVariable("g_defiant_lowHealthFraction", true);
	pConsole->UnregisterVariable("g_intervention_timeBetweenZoomedAndKill", true);
	pConsole->UnregisterVariable("g_blinding_timeBetweenFlashbangAndKill", true);
	pConsole->UnregisterVariable("g_blinding_flashbangRecoveryDelayFrac", true);
	pConsole->UnregisterVariable("g_neverFlagging_maxMatchTimeRemaining", true);
	pConsole->UnregisterVariable("g_combinedFire_maxTimeBetweenWeapons", true);
	pConsole->UnregisterVariable("g_fovToRotationSpeedInfluence", true);
	pConsole->UnregisterVariable("dd_maxRMIsPerFrame", true);
	pConsole->UnregisterVariable("dd_waitPeriodBetweenRMIBatches", true);
	pConsole->UnregisterVariable("g_debugSpawnPointsRegistration", true);
	pConsole->UnregisterVariable("g_debugSpawnPointValidity", true);

	pConsole->UnregisterVariable("g_scoreLimit", true);
	pConsole->UnregisterVariable("g_scoreLimitOverride", true);
	pConsole->UnregisterVariable("g_hostMigrationResumeTime", true);
#ifndef _RELEASE
	pConsole->UnregisterVariable("g_hostMigrationUseAutoLobbyMigrateInPrivateGames", true);
#endif
	pConsole->UnregisterVariable("g_mpRegenerationRate", true);
	pConsole->UnregisterVariable("g_mpHeadshotsOnly", true);
	pConsole->UnregisterVariable("g_mpNoVTOL", true);
	pConsole->UnregisterVariable("g_mpNoEnvironmentalWeapons", true);
	pConsole->UnregisterVariable("g_allowCustomLoadouts", true);
	pConsole->UnregisterVariable("g_allowFatalityBonus", true);
	pConsole->UnregisterVariable("g_autoReviveTime", true);
	pConsole->UnregisterVariable("g_spawnPrecacheTimeBeforeRevive", true);
	pConsole->UnregisterVariable("g_spawn_timeToRetrySpawnRequest", true);
	pConsole->UnregisterVariable("g_spawn_recentSpawnTimer", true);
	pConsole->UnregisterVariable("g_forcedReviveTime", true);
	pConsole->UnregisterVariable("g_numLives", true);
	pConsole->UnregisterVariable("g_autoAssignTeams", true);
	pConsole->UnregisterVariable("g_maxHealthMultiplier", true);
	pConsole->UnregisterVariable("g_mp_as_DefendersMaxHealth", true);
	pConsole->UnregisterVariable("g_xpMultiplyer", true);
	pConsole->UnregisterVariable("g_allowExplosives", true);
	pConsole->UnregisterVariable("g_forceWeapon", true);
	pConsole->UnregisterVariable("g_spawn_explosiveSafeDist", true);
	pConsole->UnregisterVariable("g_allowSpectators", true);
	pConsole->UnregisterVariable("g_infiniteCloak", true);
	pConsole->UnregisterVariable("g_allowWeaponCustomisation", true);

	pConsole->UnregisterVariable("g_switchTeamAllowed", true);
	pConsole->UnregisterVariable("g_switchTeamRequiredPlayerDifference", true);
	pConsole->UnregisterVariable("g_switchTeamUnbalancedWarningDifference", true);
	pConsole->UnregisterVariable("g_switchTeamUnbalancedWarningTimer", true);

	pConsole->UnregisterVariable("g_forceHeavyWeapon", true);
	pConsole->UnregisterVariable("g_forceLoadoutPackage", true);

	pConsole->UnregisterVariable("g_spectatorOnly", true);
	pConsole->UnregisterVariable("g_spectatorOnlySwitchCooldown", true);
	pConsole->UnregisterVariable("g_forceIntroSequence", true);
	pConsole->UnregisterVariable("g_IntroSequencesEnabled", true);

	pConsole->UnregisterVariable("g_deathCam", true);
	pConsole->UnregisterVariable("g_deathCamSP.enable", true);
	pConsole->UnregisterVariable("g_deathCamSP.dof_enable", true);
	pConsole->UnregisterVariable("g_deathCamSP.updateFrequency", true);
	pConsole->UnregisterVariable("g_deathCamSP.dofRange", true);
	pConsole->UnregisterVariable("g_deathCamSP.dofRangeNoKiller", true);
	pConsole->UnregisterVariable("g_deathCamSP.dofRangeSpeed", true);
	pConsole->UnregisterVariable("g_deathCamSP.dofDistanceSpeed", true);

	pConsole->UnregisterVariable("g_logPrimaryRound", true);

#if USE_REGION_FILTER
	pConsole->UnregisterVariable("g_server_region", true);
#endif

#if TALOS
	pConsole->UnregisterVariable("pl_talos", true);
#endif

	pConsole->UnregisterVariable("pl_sliding_control.min_speed_threshold", true);
	pConsole->UnregisterVariable("pl_sliding_control.min_speed", true);
	pConsole->UnregisterVariable("pl_sliding_control.deceleration_speed", true);
	pConsole->UnregisterVariable("pl_sliding_control.min_downhill_threshold", true);
	pConsole->UnregisterVariable("pl_sliding_control.max_downhill_threshold", true);
	pConsole->UnregisterVariable("pl_sliding_control.max_downhill_acceleration", true);

	pConsole->UnregisterVariable("pl_sliding_control_mp.min_speed_threshold", true);
	pConsole->UnregisterVariable("pl_sliding_control_mp.min_speed", true);
	pConsole->UnregisterVariable("pl_sliding_control_mp.deceleration_speed", true);
	pConsole->UnregisterVariable("pl_sliding_control_mp.min_downhill_threshold", true);
	pConsole->UnregisterVariable("pl_sliding_control_mp.max_downhill_threshold", true);
	pConsole->UnregisterVariable("pl_sliding_control_mp.max_downhill_acceleration", true);

	pConsole->UnregisterVariable("pl_slideCameraFactor", true);
	pConsole->UnregisterVariable("pl_enemy_ramming.player_to_player", true);
	pConsole->UnregisterVariable("pl_enemy_ramming.ragdoll_to_player", true);
	pConsole->UnregisterVariable("pl_enemy_ramming.fall_damage_threashold", true);
	pConsole->UnregisterVariable("pl_enemy_ramming.safe_falling_speed", true);
	pConsole->UnregisterVariable("pl_enemy_ramming.fatal_falling_speed", true);
	pConsole->UnregisterVariable("pl_enemy_ramming.max_falling_damage", true);
	pConsole->UnregisterVariable("pl_enemy_ramming.min_momentum_to_fall", true);

	pConsole->UnregisterVariable("AICollisions.minSpeedForFallAndPlay", true);
	pConsole->UnregisterVariable("AICollisions.minMassForFallAndPlay", true);
	pConsole->UnregisterVariable("AICollisions.dmgFactorWhenCollidedByObject", true);
	pConsole->UnregisterVariable("AICollisions.showInLog", true);

	pConsole->UnregisterVariable("pl_melee.melee_snap_angle_limit", true);
	pConsole->UnregisterVariable("pl_melee.melee_snap_blend_speed", true);
	pConsole->UnregisterVariable("pl_melee.melee_snap_move_speed_multiplier", true);
	pConsole->UnregisterVariable("pl_melee.melee_snap_target_select_range", true);
	pConsole->UnregisterVariable("pl_melee.melee_snap_end_position_range", true);
	pConsole->UnregisterVariable("pl_melee.debug_gfx", true);
	pConsole->UnregisterVariable("pl_melee.damage_multiplier_from_behind", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.debugDraw", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.useProxies", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.useEnergy", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.delayPlayerAnimations", true);

	// Melee weaps
	pConsole->UnregisterVariable("pl_pickAndThrow.maxOrientationCorrectionTime", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.orientationCorrectionTimeMult", true);

#ifndef _RELEASE
	// Dist based override
	pConsole->UnregisterVariable("pl_pickAndThrow.correctOrientationDistOverride", true);
	// helper Pos based override
	pConsole->UnregisterVariable("pl_pickAndThrow.correctOrientationPosOverrideEnabled", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.correctOrientationPosOverrideX", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.correctOrientationPosOverrideY", true);
	// Charged throw
	pConsole->UnregisterVariable("pl_pickAndThrow.chargedThrowDebugOutputEnabled", true);
	// Health
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponHealthDebugEnabled", true);
	// Impacts
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponImpactDebugEnabled", true);
	// Combos
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponComboDebugEnabled", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponRenderStatsEnabled", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponDebugSwing", true);
#endif // #ifndef _RELEASE

	pConsole->UnregisterVariable("pl_pickAndThrow.comboInputWindowSize", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.minComboInputWindowDurationInSecs", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponDesiredRootedGrabAnimDuration", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponDesiredGrabAnimDuration", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponDesiredPrimaryAttackAnimDuration", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponDesiredComboAttackAnimDuration", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponUnrootedPickupTimeMult", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponThrowAnimationSpeed", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponLivingToArticulatedImpulseRatio", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.enviromentalWeaponUseThrowInitialFacingOveride", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponDebugSwing", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponSweepTestsEnabled", true); 
	pConsole->UnregisterVariable("pl_pickAndThrow.intersectionAssistPenetrationThreshold", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.objectImpulseLowMassThreshold", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.objectImpulseLowerScaleLimit", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponViewLerpZOffset", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponViewLerpSmoothTime", true);
	
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponObjectImpulseScale", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponImpulseScale", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponHitConeInDegrees", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.minRequiredThrownEnvWeaponHitVelocity", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.awayFromPlayerImpulseRatio", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.cloakedEnvironmentalWeaponsAllowed", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.chargedThrowAutoAimDistance", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.chargedThrowAutoAimConeSize", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.chargedThrowAutoAimAngleHeuristicWeighting", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.chargedThrowAutoAimDistanceHeuristicWeighting", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.chargedthrowAutoAimEnabled", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.chargedThrowAimHeightOffset", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.intersectionAssistDebugEnabled", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.intersectionAssistDeleteObjectOnEmbed", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.intersectionAssistCollisionsPerSecond", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.intersectionAssistTimePeriod", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.intersectionAssistTranslationThreshold", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.IntersectionAssistPenetrationThreshold", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.complexMelee_snap_angle_limit", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.complexMelee_lerp_target_speed", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.impactNormalGroundClassificationAngle", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.impactPtValidHeightDiffForImpactStick", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.reboundAnimPlaybackSpeedMultiplier", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponMinViewClamp", true);

	pConsole->UnregisterVariable("pl_melee.damage_multiplier_mp", true);
	pConsole->UnregisterVariable("pl_melee.angle_limit_from_behind", true);
	pConsole->UnregisterVariable("pl_melee.mp_victim_screenfx_intensity", true);
	pConsole->UnregisterVariable("pl_melee.mp_victim_screenfx_duration", true);
	pConsole->UnregisterVariable("pl_melee.mp_victim_screenfx_blendout_duration", true);
	pConsole->UnregisterVariable("pl_melee.mp_victim_screenfx_dbg_force_test_duration", true);
	pConsole->UnregisterVariable("pl_melee.impulses_enable", true);
	pConsole->UnregisterVariable("pl_melee.mp_melee_system", true);
	pConsole->UnregisterVariable("pl_melee.mp_melee_system_camera_lock_and_turn", true);
	pConsole->UnregisterVariable("pl_melee.mp_melee_system_camera_lock_time", true);
	pConsole->UnregisterVariable("pl_melee.mp_melee_system_camera_lock_crouch_height_offset", true);

	pConsole->UnregisterVariable("pl_melee.mp_knockback_enabled", true);
	pConsole->UnregisterVariable("pl_melee.mp_knockback_strength_vert", true);
	pConsole->UnregisterVariable("pl_melee.mp_knockback_strength_hor", true);
	pConsole->UnregisterVariable("pl_melee.mp_sliding_auto_melee_enabled", true);

	pConsole->UnregisterVariable("pl_health.normal_regeneration_rateSP", true);
	pConsole->UnregisterVariable("pl_health.critical_health_thresholdSP", true);
	pConsole->UnregisterVariable("pl_health.critical_health_updateTimeSP", true);
	pConsole->UnregisterVariable("pl_health.normal_threshold_time_to_regenerateSP", true);

	pConsole->UnregisterVariable("pl_health.normal_regeneration_rateMP", true);
	pConsole->UnregisterVariable("pl_health.critical_health_thresholdMP", true);

	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponFlipImpulseThreshold", true);
	pConsole->UnregisterVariable("pl_pickAndThrow.environmentalWeaponFlippedImpulseOverrideMult", true);

	pConsole->UnregisterVariable("g_enableInitialLoginSilent", true);
	pConsole->UnregisterVariable("g_maxGameBrowserResults", true);

	pConsole->UnregisterVariable("g_inventoryNoLimits", true);
	pConsole->UnregisterVariable("g_inventoryWeaponCapacity", true);
	pConsole->UnregisterVariable("g_inventoryExplosivesCapacity", true);
	pConsole->UnregisterVariable("g_inventoryGrenadesCapacity", true);

	pConsole->UnregisterVariable("g_inventorySpecialCapacity", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("g_keepMPAudioSignalsPermanently", true);
#endif

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("g_debugShowGainedAchievementsOnHUD", true);
#endif

	pConsole->UnregisterVariable("g_tk_punish", true);
	pConsole->UnregisterVariable("g_tk_punish_limit", true);

	pConsole->UnregisterVariable("g_randomSpawnPointCacheTime", true);

	pConsole->UnregisterVariable("g_detachCamera", true);
	pConsole->UnregisterVariable("g_moveDetachedCamera", true);
	pConsole->UnregisterVariable("g_detachedCameraMoveSpeed", true);
	pConsole->UnregisterVariable("g_detachedCameraRotateSpeed", true);
	pConsole->UnregisterVariable("g_detachedCameraTurboBoost", true);
	pConsole->UnregisterVariable("g_detachedCameraDebug", true);
	
	pConsole->UnregisterVariable("g_debugCollisionDamage", true);
	pConsole->UnregisterVariable("g_debugHits", true);
	pConsole->UnregisterVariable("g_suppressHitSanityCheckWarnings", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_debugFakeHits", true);
#endif

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_FEMenuCacheSaveList", true);
#endif // #ifndef _RELEASE

	pConsole->UnregisterVariable("g_useHitSoundFeedback", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_bulletPenetrationEnable", true);
#endif
	pConsole->UnregisterVariable("g_bulletPenetrationDebug", true);
	pConsole->UnregisterVariable("g_bulletPenetrationDebugTimeout", true);

	pConsole->UnregisterVariable("g_fpDbaManagementEnable", true);
	pConsole->UnregisterVariable("g_fpDbaManagementDebug", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_charactersDbaManagementEnable", true);
	pConsole->UnregisterVariable("g_charactersDbaManagementDebug", true);
#endif

	pConsole->UnregisterVariable("g_bulletPenetrationTimeout", true);

	pConsole->UnregisterVariable("g_thermalVisionDebug", true);

	pConsole->UnregisterVariable("g_droppedItemVanishTimer", true);
	pConsole->UnregisterVariable("g_droppedHeavyWeaponVanishTimer", true);
	
	pConsole->UnregisterVariable("g_corpseManager_maxNum", true);
	pConsole->UnregisterVariable("g_corpseManager_thermalHeatFadeDuration", true);
	pConsole->UnregisterVariable("g_corpseManager_thermalHeatMinValue", true);
	pConsole->UnregisterVariable("g_corpseManager_timeoutInSeconds", true);

	pConsole->UnregisterVariable("g_explosion_materialFX_raycastLength", true);

	pConsole->UnregisterVariable("g_ec_enable", true);
	pConsole->UnregisterVariable("g_ec_radiusScale", true);
	pConsole->UnregisterVariable("g_ec_volume", true);
	pConsole->UnregisterVariable("g_ec_extent", true);
	pConsole->UnregisterVariable("g_ec_removeThreshold", true);

	pConsole->UnregisterVariable("v_profileMovement", true);
	pConsole->UnregisterVariable("v_profile_graph", true);
	pConsole->UnregisterVariable("v_pa_surface", true);
	pConsole->UnregisterVariable("v_wind_minspeed", true);
	pConsole->UnregisterVariable("v_draw_suspension", true);
	pConsole->UnregisterVariable("v_draw_slip", true);  
	pConsole->UnregisterVariable("v_invertPitchControl", true);
	pConsole->UnregisterVariable("v_sprintSpeed", true);
	pConsole->UnregisterVariable("v_rockBoats", true);  
	pConsole->UnregisterVariable("v_debugSounds", true);
	pConsole->UnregisterVariable("v_altitudeLimit", true);
	pConsole->UnregisterVariable("v_stabilizeVTOL", true);
	pConsole->UnregisterVariable("v_altitudeLimitLowerOffset", true);
	pConsole->UnregisterVariable("v_mouseRotScaleSP", true);
	pConsole->UnregisterVariable("v_mouseRotLimitSP", true);
	pConsole->UnregisterVariable("v_mouseRotScaleMP", true);
	pConsole->UnregisterVariable("v_mouseRotLimitMP", true);
	
	pConsole->UnregisterVariable("v_MPVTOLNetworkSyncFreq", true);
	pConsole->UnregisterVariable("v_MPVTOLNetworkSnapThreshold", true);
	pConsole->UnregisterVariable("v_MPVTOLNetworkCatchupSpeedLimit", true);

	pConsole->UnregisterVariable("pl_swimBackSpeedMul", true);
	pConsole->UnregisterVariable("pl_swimSideSpeedMul", true);
	pConsole->UnregisterVariable("pl_swimVertSpeedMul", true);
	pConsole->UnregisterVariable("pl_swimNormalSprintSpeedMul", true);
	pConsole->UnregisterVariable("pl_swimAlignArmsToSurface", true);

	pConsole->UnregisterVariable("pl_drownTime", true);
	pConsole->UnregisterVariable("pl_drownDamage", true);
	pConsole->UnregisterVariable("pl_drownDamageInterval", true);
	pConsole->UnregisterVariable("pl_drownRecoveryTime", true);

	// animation triggered footsteps
	pConsole->UnregisterVariable("g_FootstepSoundsFollowEntity", true);
	pConsole->UnregisterVariable("g_FootstepSoundsDebug", true);
	pConsole->UnregisterVariable("g_footstepSoundMaxDistanceSq", true);

	// variables from CPlayer
	pConsole->UnregisterVariable("player_DrawIK", true);
	pConsole->UnregisterVariable("player_NoIK", true);
	pConsole->UnregisterVariable("pl_debug_movement", true);
	pConsole->UnregisterVariable("pl_debug_jumping", true);
	pConsole->UnregisterVariable("pl_debug_aiming", true);
	pConsole->UnregisterVariable("pl_debug_filter", true);
#ifndef _RELEASE
	pConsole->UnregisterVariable("pl_debug_view", true);	
#endif
	pConsole->UnregisterVariable("pl_switchTPOnKill", true);
	pConsole->UnregisterVariable("pl_stealthKill_allowInMP", true);
	pConsole->UnregisterVariable("pl_stealthKill_uncloakInMP", true);
	pConsole->UnregisterVariable("pl_stealthKill_usePhysicsCheck", true);
	pConsole->UnregisterVariable("pl_stealthKill_useExtendedRange", true);
	pConsole->UnregisterVariable("pl_stealthKill_debug", true);
	pConsole->UnregisterVariable("pl_stealthKill_aimVsSpineLerp", true);
	pConsole->UnregisterVariable("pl_stealthKill_maxVelocitySquared", true);

	pConsole->UnregisterVariable("pl_slealth_cloakinterference_onactionMP", true);
	pConsole->UnregisterVariable("i_fastSelectMultiplier", true);

	pConsole->UnregisterVariable("pl_stealth_shotgunDamageCap", true);
	pConsole->UnregisterVariable("pl_shotgunDamageCap", true);

	pConsole->UnregisterVariable("pl_freeFallDeath_cameraAngle", true);
	pConsole->UnregisterVariable("pl_freeFallDeath_fadeTimer", true);

	pConsole->UnregisterVariable("pl_fall_intensity_multiplier", true);
	pConsole->UnregisterVariable("pl_fall_intensity_hit_multiplier", true);
	pConsole->UnregisterVariable("pl_fall_intensity_max", true);
	pConsole->UnregisterVariable("pl_fall_time_multiplier", true);
	pConsole->UnregisterVariable("pl_fall_time_max", true);

	pConsole->UnregisterVariable("pl_power_sprint.foward_angle", true);
	pConsole->UnregisterVariable("pl_jump_control.air_control_scale", true);
	pConsole->UnregisterVariable("pl_jump_control.air_resistance_scale", true);
	pConsole->UnregisterVariable("pl_jump_control.air_inertia_scale", true);
	pConsole->UnregisterVariable("pl_stampTimeout", true);

	pConsole->UnregisterVariable("pl_jump_maxTimerValue", true);
	pConsole->UnregisterVariable("pl_jump_baseTimeAddedPerJump", true);
	pConsole->UnregisterVariable("pl_jump_currentTimeMultiplierOnJump", true);

	pConsole->UnregisterVariable("pl_boostedMelee_allowInMP", true);

	pConsole->UnregisterVariable("pl_velocityInterpAirControlScale", true);
	pConsole->UnregisterVariable("pl_velocityInterpSynchJump", true);
	pConsole->UnregisterVariable("pl_velocityInterpAirDeltaFactor", true);
	pConsole->UnregisterVariable("pl_velocityInterpPathCorrection", true);
	pConsole->UnregisterVariable("pl_velocityInterpAlwaysSnap", true);
	pConsole->UnregisterVariable("pl_adjustJumpAngleWithFloorNormal", true);
	pConsole->UnregisterVariable("pl_debugInterpolation", true);
	pConsole->UnregisterVariable("pl_serialisePhysVel", true);
	pConsole->UnregisterVariable("pl_clientInertia", true);
	pConsole->UnregisterVariable("pl_fallHeight", true);
	pConsole->UnregisterVariable("pl_netAimLerpFactor", true);
	pConsole->UnregisterVariable("pl_netSerialiseMaxSpeed", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("pl_watchPlayerState", true);
#endif

	pConsole->UnregisterVariable("pl_ledgeClamber.debugDraw", true);
	pConsole->UnregisterVariable("pl_ledgeClamber.cameraBlendWeight", true);
	pConsole->UnregisterVariable("pl_ledgeClamber.enableVaultFromStanding", true);

	pConsole->UnregisterVariable("pl_ladderControl.ladder_renderPlayerLast", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("pl_ladderControl.ladder_logVerbosity", true);
#endif

	pConsole->UnregisterVariable("pl_legs_colliders_dist", true);
	pConsole->UnregisterVariable("pl_legs_colliders_scale", true);

	pConsole->UnregisterVariable("g_assertWhenVisTableNotUpdatedForNumFrames", true);
	pConsole->UnregisterVariable("gl_waitForBalancedGameTime", true);

	pConsole->UnregisterVariable("g_useSkillKillSoundEffects", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("pl_debug_watch_camera_mode", true);
	pConsole->UnregisterVariable("pl_debug_log_camera_mode_changes", true);
	pConsole->UnregisterVariable("pl_debug_log_player_plugins", true);
	pConsole->UnregisterVariable("pl_shotDebug", true);
#endif

	pConsole->UnregisterVariable("pl_renderInNearest", true);
	pConsole->UnregisterVariable("pl_aim_cloaked_multiplier", true);
	pConsole->UnregisterVariable("pl_aim_near_lookat_target_distance", true);
	pConsole->UnregisterVariable("pl_aim_assistance_enabled", true);
	pConsole->UnregisterVariable("pl_aim_assistance_disabled_atDifficultyLevel", true);
	pConsole->UnregisterVariable("pl_aim_acceleration_enabled", true);

	pConsole->UnregisterVariable("pl_targeting_debug", true);
	pConsole->UnregisterVariable("pl_TacticalScanDuration", true);
	pConsole->UnregisterVariable("pl_TacticalScanDurationMP", true);
	pConsole->UnregisterVariable("pl_TacticalTaggingDuration", true);
	pConsole->UnregisterVariable("pl_TacticalTaggingDurationMP", true);

	// variables from CPlayerMovementController
	pConsole->UnregisterVariable("g_showIdleStats", true);
	pConsole->UnregisterVariable("g_debugaimlook", true);

	// variables from CHUD
	pConsole->UnregisterVariable("hud_onScreenNearDistance", true);
	pConsole->UnregisterVariable("hud_onScreenFarDistance", true);
	pConsole->UnregisterVariable("hud_onScreenNearSize", true);
	pConsole->UnregisterVariable("hud_onScreenFarSize", true);
	pConsole->UnregisterVariable("hud_colorLine", true);
	pConsole->UnregisterVariable("hud_colorOver", true);
	pConsole->UnregisterVariable("hud_colorText", true);
	pConsole->UnregisterVariable("hud_centernames", true);
	pConsole->UnregisterVariable("hud_panoramic", true);
	pConsole->UnregisterVariable("hud_attachBoughEquipment", true);
	pConsole->UnregisterVariable("hud_subtitles", true);
	pConsole->UnregisterVariable("hud_psychoPsycho", true);
	pConsole->UnregisterVariable("hud_startPaused", true);
	pConsole->UnregisterVariable("hud_allowMouseInput", true);
	pConsole->UnregisterVariable("menu3D_enabled", true);

	pConsole->UnregisterVariable("hud_faderDebug", true);
	pConsole->UnregisterVariable("hud_objectiveIcons_flashTime", true);
	pConsole->UnregisterVariable("g_flashrenderingduringloading", true);
	pConsole->UnregisterVariable("g_levelfadein_levelload", true);
	pConsole->UnregisterVariable("g_levelfadein_quickload", true);

	pConsole->UnregisterVariable("aim_assistMinDistance", true);
	pConsole->UnregisterVariable("aim_assistMaxDistance", true);
	pConsole->UnregisterVariable("aim_assistFalloffDistance", true);
	pConsole->UnregisterVariable("aim_assistInputForFullFollow_Ironsight", true);
	pConsole->UnregisterVariable("aim_assistMaxDistanceTagged", true);
	pConsole->UnregisterVariable("aim_assistMinTurnScale", true);
	pConsole->UnregisterVariable("aim_assistSlowFalloffStartDistance", true);
	pConsole->UnregisterVariable("aim_assistSlowDisableDistance", true);
	pConsole->UnregisterVariable("aim_assistSlowStartFadeinDistance", true);
	pConsole->UnregisterVariable("aim_assistSlowStopFadeinDistance", true);
	pConsole->UnregisterVariable("aim_assistSlowDistanceModifier", true);
	pConsole->UnregisterVariable("aim_assistSlowThresholdOuter", true);
	pConsole->UnregisterVariable("aim_assistStrength", true);
	pConsole->UnregisterVariable("aim_assistSnapRadiusScale", true);
	pConsole->UnregisterVariable("aim_assistSnapRadiusTaggedScale", true);

	pConsole->UnregisterVariable("aim_assistStrength_IronSight", true);
	pConsole->UnregisterVariable("aim_assistMaxDistance_IronSight", true);
	pConsole->UnregisterVariable("aim_assistMinTurnScale_IronSight", true);

	pConsole->UnregisterVariable("aim_assistStrength_SniperScope", true);
	pConsole->UnregisterVariable("aim_assistMaxDistance_SniperScope", true);
	pConsole->UnregisterVariable("aim_assistMinTurnScale_SniperScope", true);

	pConsole->UnregisterVariable("hud_ContextualHealthIndicator", true);

	pConsole->UnregisterVariable("hud_aspectCorrection", true);
	pConsole->UnregisterVariable("controller_power_curve", true);
	pConsole->UnregisterVariable("controller_multiplier_z", true);
	pConsole->UnregisterVariable("controller_multiplier_x", true);
	pConsole->UnregisterVariable("controller_full_turn_multiplier_z", true);
	pConsole->UnregisterVariable("controller_full_turn_multiplier_x", true);
	pConsole->UnregisterVariable("hud_ctrlZoomMode", true);

	pConsole->UnregisterVariable("ctrlr_OUTPUTDEBUGINFO", true);
	pConsole->UnregisterVariable("ctrlr_corner_smoother", true);
	pConsole->UnregisterVariable("ctrlr_corner_smoother_debug", true);
	pConsole->UnregisterVariable("vehicle_steering_curve", true);
	pConsole->UnregisterVariable("vehicle_steering_curve_scale", true);
	pConsole->UnregisterVariable("vehicle_acceleration_curve", true);
	pConsole->UnregisterVariable("vehicle_acceleration_curve_scale", true);
	pConsole->UnregisterVariable("vehicle_deceleration_curve", true);
	pConsole->UnregisterVariable("vehicle_deceleration_curve_scale", true);

	// weapon system
	pConsole->UnregisterVariable("i_debuggun_1", true);
	pConsole->UnregisterVariable("i_debuggun_2", true);

	pConsole->UnregisterVariable("i_debug_projectiles", true);
#ifndef _RELEASE
	pConsole->UnregisterVariable("i_debug_weaponActions", true);
#endif
	pConsole->UnregisterVariable("i_debug_recoil", true);
	pConsole->UnregisterVariable("slide_spread", true);
	pConsole->UnregisterVariable("i_debug_spread", true);
	pConsole->UnregisterVariable("i_auto_turret_target", true);
	pConsole->UnregisterVariable("i_auto_turret_target_tacshells", true);

	pConsole->UnregisterVariable("i_debug_itemparams_memusage", true);
	pConsole->UnregisterVariable("i_debug_weaponparams_memusage", true);
	pConsole->UnregisterVariable("i_debug_zoom_mods", true);
	pConsole->UnregisterVariable("i_debug_sounds", true);
	pConsole->UnregisterVariable("i_debug_turrets", true);
	pConsole->UnregisterVariable("i_debug_mp_flowgraph", true);
	pConsole->UnregisterVariable("i_flashlight_has_shadows", true);
	pConsole->UnregisterVariable("i_flashlight_has_fog_volume", true);

	pConsole->UnregisterVariable("g_displayIgnoreList",true);
	pConsole->UnregisterVariable("g_buddyMessagesIngame",true);
	pConsole->UnregisterVariable("g_persistantStats_gamesCompletedFractionNeeded", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_PS_debug", true);
#endif

	pConsole->UnregisterVariable("sv_votingTimeout", true);
	pConsole->UnregisterVariable("sv_votingCooldown", true);
	pConsole->UnregisterVariable("sv_votingRatio", true);
	pConsole->UnregisterVariable("sv_votingEnable", true);
	pConsole->UnregisterVariable("sv_votingBanTime", true);
	pConsole->UnregisterVariable("sv_votingMinVotes", true);
	pConsole->UnregisterVariable("sv_input_timeout", true);

	pConsole->UnregisterVariable("g_MicrowaveBeamStaticObjectMaxChunkThreshold", true);

	pConsole->UnregisterVariable("sv_aiTeamName", true);
	pConsole->UnregisterVariable("performance_profile_logname", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("g_spectate_Debug", true);
	pConsole->UnregisterVariable("g_spectate_follow_orbitEnable", true);
#endif //!defined(_RELEASE)
	pConsole->UnregisterVariable("g_spectate_TeamOnly", true);
	pConsole->UnregisterVariable("g_spectate_DisableManual", true);
	pConsole->UnregisterVariable("g_spectate_DisableDead", true);
	pConsole->UnregisterVariable("g_spectate_DisableFree", true);
	pConsole->UnregisterVariable("g_spectate_DisableFollow", true);
	pConsole->UnregisterVariable("g_spectate_skipInvalidTargetAfterTime", true);
	pConsole->UnregisterVariable("g_spectate_follow_orbitYawSpeedDegrees", true);
	pConsole->UnregisterVariable("g_spectate_follow_orbitAlsoRotateWithTarget", true);
	pConsole->UnregisterVariable("g_spectate_follow_orbitMouseSpeedMultiplier", true);
	pConsole->UnregisterVariable("g_spectate_follow_orbitMinPitchRadians", true);
	pConsole->UnregisterVariable("g_spectate_follow_orbitMaxPitchRadians", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_tpdeathcam_dbg_gfxTimeout", true);
	pConsole->UnregisterVariable("g_tpdeathcam_dbg_alwaysOn", true);
#endif
	pConsole->UnregisterVariable("g_tpdeathcam_timeOutKilled", true);
	pConsole->UnregisterVariable("g_tpdeathcam_timeOutSuicide", true);
	pConsole->UnregisterVariable("g_tpdeathcam_lookDistWhenNoKiller", true);
	pConsole->UnregisterVariable("g_tpdeathcam_camDistFromPlayerStart", true);
	pConsole->UnregisterVariable("g_tpdeathcam_camDistFromPlayerEnd", true);
	pConsole->UnregisterVariable("g_tpdeathcam_camDistFromPlayerMin", true);
	pConsole->UnregisterVariable("g_tpdeathcam_camHeightTweak", true);
	pConsole->UnregisterVariable("g_tpdeathcam_camCollisionRadius", true);
	pConsole->UnregisterVariable("g_tpdeathcam_maxBumpCamUpOnCollide", true);
	pConsole->UnregisterVariable("g_tpdeathcam_zVerticalLimit", true);
	pConsole->UnregisterVariable("g_tpdeathcam_testLenIncreaseRestriction", true);
	pConsole->UnregisterVariable("g_tpdeathcam_collisionEpsilon", true);
	pConsole->UnregisterVariable("g_tpdeathcam_directionalFocusGroundTestLen", true);
	pConsole->UnregisterVariable("g_tpdeathcam_camSmoothSpeed", true);
	pConsole->UnregisterVariable("g_tpdeathcam_maxTurn", true);

	pConsole->UnregisterVariable("g_killercam_disable", true);
	pConsole->UnregisterVariable("g_killercam_displayDuration", true);
	pConsole->UnregisterVariable("g_killercam_dofBlurAmount", true );
	pConsole->UnregisterVariable("g_killercam_dofFocusRange", true );
	pConsole->UnregisterVariable("g_killercam_canSkip", true );

	pConsole->UnregisterVariable("g_postkill_minTimeForDeathCamAndKillerCam", true );
	pConsole->UnregisterVariable("g_postkill_splitScaleDeathCam", true );

	pConsole->UnregisterVariable("sv_pacifist", true);
	pConsole->UnregisterVariable("g_devDemo", true);

	// Alternative player input normalization code
	pConsole->UnregisterVariable("aim_altNormalization.enable", true);
	pConsole->UnregisterVariable("aim_altNormalization.hud_ctrl_Curve_Unified", true);
	pConsole->UnregisterVariable("aim_altNormalization.hud_ctrl_Coeff_Unified", true);
	// Alternative player input normalization code

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("v_debugMovement", true);
	pConsole->UnregisterVariable("v_debugMovementMoveVertically", true);
	pConsole->UnregisterVariable("v_debugMovementX", true);
	pConsole->UnregisterVariable("v_debugMovementY", true);
	pConsole->UnregisterVariable("v_debugMovementZ", true);
	pConsole->UnregisterVariable("v_debugMovementSensitivity", true);
#endif

	pConsole->UnregisterVariable("v_tankReverseInvertYaw", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("pl_debug_aiming_input", true);
#endif

	pConsole->UnregisterVariable("pl_debug_vistable", true);

	pConsole->UnregisterVariable("g_teamDifferentiation", true);

	pConsole->UnregisterVariable("g_postEffect.FilterGrain_Amount",true);
	pConsole->UnregisterVariable("g_postEffect.FilterRadialBlurring_Amount",true);
	pConsole->UnregisterVariable("g_postEffect.FilterRadialBlurring_ScreenPosX",true);
	pConsole->UnregisterVariable("g_postEffect.FilterRadialBlurring_ScreenPosY",true);
	pConsole->UnregisterVariable("g_postEffect.FilterRadialBlurring_Radius",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_ColorC",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_ColorM",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_ColorY",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_ColorK",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_Brightness",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_Contrast",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_Saturation",true);
	pConsole->UnregisterVariable("g_postEffect.Global_User_ColorHue",true);
	pConsole->UnregisterVariable("g_postEffect.HUD3D_Interference",true);
	pConsole->UnregisterVariable("g_postEffect.HUD3D_FOV",true);

	pConsole->UnregisterVariable("g_gameFXSystemDebug",true);
	pConsole->UnregisterVariable("g_gameFXLightningProfile", true);
	pConsole->UnregisterVariable("g_DebugDrawPhysicsAccess", true);
	pConsole->UnregisterVariable("g_hasWindowFocus", true);

	pConsole->UnregisterVariable("g_displayPlayerDamageTaken", true);
	pConsole->UnregisterVariable("g_displayDbgText_hud", true);
	pConsole->UnregisterVariable("g_displayDbgText_plugins", true);
	pConsole->UnregisterVariable("g_displayDbgText_pmv", true);
	pConsole->UnregisterVariable("g_displayDbgText_actorState", true);
	pConsole->UnregisterVariable("g_displayDbgText_silhouettes", true);

	pConsole->UnregisterVariable("g_spawn_vistable_numLineTestsPerFrame", true);
	pConsole->UnregisterVariable("g_spawn_vistable_numAreaTestsPerFrame", true);

	pConsole->UnregisterVariable("g_showShadowChar", true);
	pConsole->UnregisterVariable("g_infiniteAmmo", true);

	pConsole->UnregisterVariable("g_animatorDebug", true);
	pConsole->UnregisterVariable("g_hideArms", true);
	pConsole->UnregisterVariable("g_debugSmokeGrenades", true);
	pConsole->UnregisterVariable("g_smokeGrenadeRadius", true);
	pConsole->UnregisterVariable("g_empOverTimeGrenadeLife", true);
	pConsole->UnregisterVariable("g_kickCarDetachesEntities", true);
	pConsole->UnregisterVariable("g_kickCarDetachStartTime", true);
	pConsole->UnregisterVariable("g_kickCarDetachEndTime", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("g_playerUsesDedicatedInput", true);
	pConsole->UnregisterVariable("g_DisableScoring", true);
	pConsole->UnregisterVariable("g_DisableCollisionDamage", true);
	pConsole->UnregisterVariable("g_MaxSimpleCollisions", true);
	pConsole->UnregisterVariable("g_LogDamage", true);
	pConsole->UnregisterVariable("g_ProjectilePathDebugGfx", true);
	pConsole->UnregisterVariable("net_onlyListGameServersContainingText", true);
	pConsole->UnregisterVariable("net_nat_type", true);
	pConsole->UnregisterVariable("net_initLobbyServiceToLan", true);
#endif

	pConsole->UnregisterVariable("watch_enabled", true);
	pConsole->UnregisterVariable("watch_text_render_start_pos_x", true);
	pConsole->UnregisterVariable("watch_text_render_start_pos_y", true);
	pConsole->UnregisterVariable("watch_text_render_size", true);
	pConsole->UnregisterVariable("watch_text_render_lineSpacing", true);
	pConsole->UnregisterVariable("watch_text_render_fxscale", true);

	pConsole->UnregisterVariable("autotest_verbose", true);

	pConsole->UnregisterVariable("autotest_enabled", true);
	pConsole->UnregisterVariable("autotest_state_setup", true);
	pConsole->UnregisterVariable("autotest_quit_when_done", true);
	pConsole->UnregisterVariable("designer_warning_enabled", true);
	pConsole->UnregisterVariable("designer_warning_level_resources", true);

	pConsole->UnregisterVariable("ai_DebugVisualScriptErrors", true);
	pConsole->UnregisterVariable("ai_EnablePressureSystem", true);
	pConsole->UnregisterVariable("ai_DebugPressureSystem", true);
	pConsole->UnregisterVariable("ai_DebugAggressionSystem", true);
	pConsole->UnregisterVariable("ai_DebugBattleFront", true);
	pConsole->UnregisterVariable("ai_DebugSearch", true);
	pConsole->UnregisterVariable("ai_DebugDeferredDeath", true);
	pConsole->UnregisterVariable("ai_SquadManager_DebugDraw", true);

	pConsole->UnregisterVariable("ai_SquadManager_MaxDistanceFromSquadCenter", true);
	pConsole->UnregisterVariable("ai_SquadManager_UpdateTick", true);

	pConsole->UnregisterVariable("ai_SOMDebugName", true);
	pConsole->UnregisterVariable("ai_SOMIgnoreVisualRatio", true);
	pConsole->UnregisterVariable("ai_SOMDecayTime", true);
	pConsole->UnregisterVariable("ai_SOMMinimumRelaxed", true);
	pConsole->UnregisterVariable("ai_SOMMinimumCombat", true);

	pConsole->UnregisterVariable("ai_SOMCrouchModifierRelaxed", true);
	pConsole->UnregisterVariable("ai_SOMCrouchModifierCombat", true);
	pConsole->UnregisterVariable("ai_SOMMovementModifierRelaxed", true);
	pConsole->UnregisterVariable("ai_SOMMovementModifierCombat", true);
	pConsole->UnregisterVariable("ai_SOMWeaponFireModifierRelaxed", true);
	pConsole->UnregisterVariable("ai_SOMWeaponFireModifierCombat", true);
	pConsole->UnregisterVariable("ai_SOMCloakMaxTimeRelaxed", true);
	pConsole->UnregisterVariable("ai_SOMCloakMaxTimeCombat", true);
	pConsole->UnregisterVariable("ai_SOMUncloakMinTimeRelaxed", true);
	pConsole->UnregisterVariable("ai_SOMUncloakMinTimeCombat", true);
	pConsole->UnregisterVariable("ai_SOMUncloakMaxTimeRelaxed", true);
	pConsole->UnregisterVariable("ai_SOMUncloakMaxTimeCombat", true);
	pConsole->UnregisterVariable("ai_CloakingDelay", true);
	pConsole->UnregisterVariable("ai_UnCloakingTime", true);
	pConsole->UnregisterVariable("ai_CompleteCloakDelay", true);
	pConsole->UnregisterVariable("ai_HazardsDebug", true);
	pConsole->UnregisterVariable("ai_ProximityToHostileAlertnessIncrementThresholdDistance", true);

	pConsole->UnregisterVariable("g_actorViewDistRatio", true);
	pConsole->UnregisterVariable("g_playerLodRatio", true);
	pConsole->UnregisterVariable("g_itemsLodRatioScale", true);
	pConsole->UnregisterVariable("g_itemsViewDistanceRatioScale", true);

	CGodMode::GetInstance().UnregisterConsoleVars(pConsole);

	NetInputChainUnregisterCVars();

	pConsole->UnregisterVariable("g_hitDeathReactions_enable", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_useLuaDefaultFunctions", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_disable_ai", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_debug", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_disableRagdoll", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_logReactionAnimsOnLoading", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_streaming", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_usePrecaching", true);
	pConsole->UnregisterVariable("g_hitDeathReactions_disableHitAnimatedCollisions", true);

	pConsole->UnregisterVariable("g_spectacularKill.maxDistanceError", true);
	pConsole->UnregisterVariable("g_spectacularKill.minTimeBetweenKills", true);
	pConsole->UnregisterVariable("g_spectacularKill.minTimeBetweenSameKills", true);
	pConsole->UnregisterVariable("g_spectacularKill.minKillerToTargetDotProduct", true);
	pConsole->UnregisterVariable("g_spectacularKill.maxHeightBetweenActors", true);
	pConsole->UnregisterVariable("g_spectacularKill.sqMaxDistanceFromPlayer", true);
	pConsole->UnregisterVariable("g_spectacularKill.debug", true);

	pConsole->UnregisterVariable("g_movementTransitions_enable", true);
	pConsole->UnregisterVariable("g_movementTransitions_log", true);
	pConsole->UnregisterVariable("g_movementTransitions_debug", true);

	pConsole->UnregisterVariable("g_maximumDamage", true);
	pConsole->UnregisterVariable("g_instantKillDamageThreshold", true);

	pConsole->UnregisterVariable("g_muzzleFlashCull", true);
	pConsole->UnregisterVariable("g_muzzleFlashCullDistance", true);
	pConsole->UnregisterVariable("g_rejectEffectVisibilityCull", true);
	pConsole->UnregisterVariable("g_rejectEffectCullDistance", true);
	
	pConsole->UnregisterVariable("g_flyCamLoop", true);
	pConsole->UnregisterVariable("g_mpCullShootProbablyHits", true);

	pConsole->UnregisterVariable("g_cloakRefractionScale", true);
	pConsole->UnregisterVariable("g_cloakBlendSpeedScale", true);

	pConsole->UnregisterVariable("g_telemetry_onlyInGame", true);
	pConsole->UnregisterVariable("g_telemetry_drawcall_budget", true);
	pConsole->UnregisterVariable("g_telemetry_memory_display", true);
	pConsole->UnregisterVariable("g_telemetry_memory_size_sp", true);
	pConsole->UnregisterVariable("g_telemetry_memory_size_mp", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_disableSwitchVariantOnSettingChanges", true); 
	pConsole->UnregisterVariable("g_startOnQuorum", true);
#endif //#ifndef _RELEASE

#if (USE_DEDICATED_INPUT)
	pConsole->UnregisterVariable("g_dummyPlayersFire", true);
	pConsole->UnregisterVariable("g_dummyPlayersMove", true);
	pConsole->UnregisterVariable("g_dummyPlayersJump", true);
	pConsole->UnregisterVariable("g_dummyPlayersChangeWeapon", true);
	pConsole->UnregisterVariable("g_dummyPlayersRespawnAtDeathPosition", true);
	pConsole->UnregisterVariable("g_dummyPlayersCommitSuicide", true);
	pConsole->UnregisterVariable("g_dummyPlayersShowDebugText", true);
	pConsole->UnregisterVariable("g_dummyPlayersMinInTime", true);
	pConsole->UnregisterVariable("g_dummyPlayersMaxInTime", true);
	pConsole->UnregisterVariable("g_dummyPlayersMinOutTime", true);
	pConsole->UnregisterVariable("g_dummyPlayersMaxOutTime", true);
	pConsole->UnregisterVariable("g_dummyPlayersGameRules", true);
	pConsole->UnregisterVariable("g_dummyPlayersRanked", true);
#endif // #if (USE_DEDICATED_INPUT)
	CBodyManagerCVars::UnregisterVariables(pConsole);

	pConsole->UnregisterVariable("g_telemetry_gameplay_enabled", true);
	pConsole->UnregisterVariable("g_telemetry_serialize_method", true);
	pConsole->UnregisterVariable("g_telemetryDisplaySessionId", true);
	pConsole->UnregisterVariable("g_telemetry_gameplay_gzip", true);
	pConsole->UnregisterVariable("g_telemetry_gameplay_save_to_disk", true);
	pConsole->UnregisterVariable("g_telemetry_gameplay_copy_to_global_heap", true);
	pConsole->UnregisterVariable("g_telemetryEnabledSP", true);
	pConsole->UnregisterVariable("g_telemetrySampleRatePerformance", true);
	pConsole->UnregisterVariable("g_telemetrySampleRateBandwidth", true);
	pConsole->UnregisterVariable("g_telemetrySampleRateMemory", true);
	pConsole->UnregisterVariable("g_telemetrySampleRateSound", true);
	pConsole->UnregisterVariable("g_telemetry_xp_event_send_interval", true);
	pConsole->UnregisterVariable("g_telemetry_mp_upload_delay", true);
	pConsole->UnregisterVariable("g_dataRefreshFrequency", true);

#if defined(DEDICATED_SERVER)
	pConsole->UnregisterVariable("g_quitOnNewDataFound", true);
	pConsole->UnregisterVariable("g_quitNumRoundsWarning", true);
	pConsole->UnregisterVariable("g_allowedDataPatchFailCount", true);
	pConsole->UnregisterVariable("g_shutdownMessageRepeatTime", true);
	pConsole->UnregisterVariable("g_shutdownMessage", true);
#endif

	pConsole->UnregisterVariable("g_modevarivar_proHud", true);
	pConsole->UnregisterVariable("g_modevarivar_disableKillCam", true);
	pConsole->UnregisterVariable("g_modevarivar_disableSpectatorCam", true);

#if !defined(_RELEASE)
	pConsole->UnregisterVariable("g_data_centre_config", true);
	pConsole->UnregisterVariable("g_download_mgr_data_centre_config", true);
#endif

	pConsole->UnregisterVariable("pl_health.fast_regeneration_rateMP", true);
	pConsole->UnregisterVariable("pl_health.slow_regeneration_rateMP", true);
	pConsole->UnregisterVariable("pl_health.normal_threshold_time_to_regenerateMP", true);
	pConsole->UnregisterVariable("pl_health.enable_FallandPlay", true);
	pConsole->UnregisterVariable("pl_health.collision_health_threshold", true);

	pConsole->UnregisterVariable("pl_health.fallDamage_SpeedSafe", true);
	pConsole->UnregisterVariable("pl_health.fallDamage_SpeedFatal", true);
	pConsole->UnregisterVariable("pl_health.fallSpeed_HeavyLand", true);
	pConsole->UnregisterVariable("pl_health.fallDamage_SpeedFatalArmor", true);
	pConsole->UnregisterVariable("pl_health.fallSpeed_HeavyLandArmor", true);
	pConsole->UnregisterVariable("pl_health.fallDamage_SpeedSafeArmorMP", true);
	pConsole->UnregisterVariable("pl_health.fallDamage_SpeedFatalArmorMP", true);
	pConsole->UnregisterVariable("pl_health.fallSpeed_HeavyLandArmorMP", true);
	pConsole->UnregisterVariable("pl_health.fallDamage_CurveAttack", true);
	pConsole->UnregisterVariable("pl_health.fallDamage_CurveAttackMP", true);
	pConsole->UnregisterVariable("pl_health.fallDamage_health_threshold", true);
	pConsole->UnregisterVariable("pl_health.debug_FallDamage", true);
	pConsole->UnregisterVariable("pl_health.enableNewHUDEffect", true);
	pConsole->UnregisterVariable("pl_health.minimalHudEffect", true);
	//pConsole->UnregisterVariable("pl_health.mercy_time", true);

	pConsole->UnregisterVariable("pl_movement.nonCombat_heavy_weapon_speed_scale", true);
	pConsole->UnregisterVariable("pl_movement.nonCombat_heavy_weapon_sprint_scale", true);
	pConsole->UnregisterVariable("pl_movement.nonCombat_heavy_weapon_strafe_speed_scale", true);
	pConsole->UnregisterVariable("pl_movement.nonCombat_heavy_weapon_crouch_speed_scale", true);
	pConsole->UnregisterVariable("pl_movement.power_sprint_targetFov", true);

	pConsole->UnregisterVariable("g_ignoreDLCRequirements", true);

	pConsole->UnregisterVariable("sv_netLimboTimeout", true);
	pConsole->UnregisterVariable("g_idleKickTime", true);

	pConsole->UnregisterVariable("g_stereoIronsightWeaponDistance", true);
	pConsole->UnregisterVariable("g_stereoFrameworkEnable", true);

	pConsole->UnregisterVariable("g_useOnlineServiceForDedicated", true);

#if USE_LAGOMETER
	pConsole->UnregisterVariable("net_graph", true);
#endif

	pConsole->UnregisterVariable("g_enablePoolCache", true);
	pConsole->UnregisterVariable("g_setActorModelFromLua", true);
	pConsole->UnregisterVariable("g_loadPlayerModelOnLoad", true);
	pConsole->UnregisterVariable("g_enableActorLuaCache", true);
	pConsole->UnregisterVariable("g_enableSlimCheckpoints", true);
	pConsole->UnregisterVariable("g_mpLoaderScreenMaxTimeSoftLimit", true);
	pConsole->UnregisterVariable("g_mpLoaderScreenMaxTime", true);
	pConsole->UnregisterVariable("g_telemetryTags", true);
	pConsole->UnregisterVariable("g_telemetryConfig", true);
	pConsole->UnregisterVariable("g_telemetryEntityClassesToExport", true);

	pConsole->UnregisterVariable("pl_movement.ground_timeInAirToFall", true);
	pConsole->UnregisterVariable("pl_movement.speedScale", true);
	pConsole->UnregisterVariable("pl_movement.sprint_SpeedScale", true);
	pConsole->UnregisterVariable("pl_movement.strafe_SpeedScale", true);
	pConsole->UnregisterVariable("pl_movement.crouch_SpeedScale", true);
	pConsole->UnregisterVariable("pl_movement.sprintStamina_debug", true);
	pConsole->UnregisterVariable("pl_movement.mp_slope_speed_multiplier_uphill", true);
	pConsole->UnregisterVariable("pl_movement.mp_slope_speed_multiplier_downhill", true);
	pConsole->UnregisterVariable("pl_movement.mp_slope_speed_multiplier_minHill", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable( "pl_state_debug", true );
#endif

#ifndef _RELEASE
	pConsole->UnregisterVariable("pl_viewFollow", true);
	pConsole->UnregisterVariable("pl_viewFollowOffset", true);
	pConsole->UnregisterVariable("pl_viewFollowMinRadius", true);
	pConsole->UnregisterVariable("pl_viewFollowMaxRadius", true);
	pConsole->UnregisterVariable("menu_forceShowPii", true);
#endif

	pConsole->UnregisterVariable("g_mpKickableCars", true);

#if PAIRFILE_GEN_ENABLED
	pConsole->RemoveCommand("g_generateAnimPairFile");
#endif

	pConsole->RemoveCommand("g_setcontrollerlayout");

	pConsole->UnregisterVariable("mp_ctfParams.carryingFlag_SpeedScale", true);
	pConsole->UnregisterVariable("mp_extractionParams.carryingTick_SpeedScale", true);
	pConsole->UnregisterVariable("mp_extractionParams.carryingTick_EnergyCostPerHit", true);
	pConsole->UnregisterVariable("mp_extractionParams.carryingTick_DamageAbsorbDesperateEnergyCost", true);
	pConsole->UnregisterVariable("mp_predatorParams.hudTimerAlertWhenTimeRemaining", true);
	pConsole->UnregisterVariable("mp_predatorParams.hintMessagePauseTime", true);

	pConsole->UnregisterVariable("pl_mike_debug", true);
	pConsole->UnregisterVariable("pl_mike_maxBurnPoints", true);
	pConsole->UnregisterVariable("pl_impulseEnabled", true);
	pConsole->UnregisterVariable("pl_impulseDuration", true);
	pConsole->UnregisterVariable("pl_impulseLayer", true);
	pConsole->UnregisterVariable("pl_impulseFullRecoilFactor", true);
	pConsole->UnregisterVariable("pl_impulseMaxPitch", true);
	pConsole->UnregisterVariable("pl_impulseMaxTwist", true);
	pConsole->UnregisterVariable("pl_impulseCounterFactor", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("pl_impulseDebug", true);
#endif

	pConsole->UnregisterVariable("g_forceItemRespawnTimer", true);
	pConsole->UnregisterVariable("g_defaultItemRespawnTimer", true);
	pConsole->UnregisterVariable("g_updateRichPresenceInterval", true);
	pConsole->UnregisterVariable("g_useNetSyncToSpeedUpRMIs", true);

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	pConsole->UnregisterVariable("g_testStatus", true);
#endif

	// VTOL Vehicle variables
	pConsole->UnregisterVariable("g_VTOLVehicleManagerEnabled", true); 
	pConsole->UnregisterVariable("g_VTOLMaxGroundVelocity", true); 
	pConsole->UnregisterVariable("g_VTOLDeadStateRemovalTime", true); 
	pConsole->UnregisterVariable("g_VTOLDestroyedContinueVelocityTime", true); 
	pConsole->UnregisterVariable("g_VTOLRespawnTime", true); 
	pConsole->UnregisterVariable("g_VTOLGravity", true);
	pConsole->UnregisterVariable("g_VTOLPathingLookAheadDistance", true);
	pConsole->UnregisterVariable("g_VTOLOnDestructionPlayerDamage", true);
	pConsole->UnregisterVariable("g_VTOLInsideBoundsScaleX", true);
	pConsole->UnregisterVariable("g_VTOLInsideBoundsScaleY", true);
	pConsole->UnregisterVariable("g_VTOLInsideBoundsScaleZ", true);
	pConsole->UnregisterVariable("g_VTOLInsideBoundsOffsetZ", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_VTOLVehicleManagerDebugEnabled", true);
	pConsole->UnregisterVariable("g_VTOLVehicleManagerDebugPathingEnabled", true);
	pConsole->UnregisterVariable("g_VTOLVehicleManagerDebugSimulatePath", true);
	pConsole->UnregisterVariable("g_VTOLVehicleManagerDebugSimulatePathTimestep", true);
	pConsole->UnregisterVariable("g_VTOLVehicleManagerDebugSimulatePathMinDotValueCheck", true);
#endif
	// ~ VTOL Vehicle variables

#ifndef _RELEASE
		pConsole->UnregisterVariable("g_mptrackview_debug", true);
		pConsole->UnregisterVariable("g_hud_postgame_debug", true);
		pConsole->UnregisterVariable("g_interactiveObjectDebugRender", true);
#endif

	pConsole->UnregisterVariable("g_highlightingMaxDistanceToHighlightSquared", true);
	pConsole->UnregisterVariable("g_highlightingMovementDistanceToUpdateSquared", true);
	pConsole->UnregisterVariable("g_highlightingTimeBetweenForcedRefresh", true);

	pConsole->UnregisterVariable("g_ledgeGrabClearHeight", true);
	pConsole->UnregisterVariable("g_ledgeGrabMovingledgeExitVelocityMult", true);

	pConsole->UnregisterVariable("g_vaultMinHeightDiff", true);
	pConsole->UnregisterVariable("g_vaultMinAnimationSpeed", true);

	// Lower weapon during stealth
	pConsole->UnregisterVariable("g_useQuickGrenadeThrow", true);
	pConsole->UnregisterVariable("g_enableMPDoubleTapGrenadeSwitch", true);
	pConsole->UnregisterVariable("g_cloakFlickerOnRun", true);

	pConsole->UnregisterVariable("kc_useAimAdjustment", true);
	pConsole->UnregisterVariable("kc_aimAdjustmentMinAngle", true);
	pConsole->UnregisterVariable("kc_precacheTimeStartPos", true);
	pConsole->UnregisterVariable("kc_precacheTimeWeaponModels", true);

#ifndef _RELEASE
	pConsole->UnregisterVariable("g_vtolManagerDebug", true);

	pConsole->UnregisterVariable("g_mpPathFollowingRenderAllPaths", true);
	pConsole->UnregisterVariable("g_mpPathFollowingNodeTextSize", true);
#endif

	pConsole->UnregisterVariable("g_minPlayersForRankedGame", true);
	pConsole->UnregisterVariable("g_gameStartingMessageTime", true);

#if ENABLE_RMI_BENCHMARK
	pConsole->UnregisterVariable("g_RMIBenchmarkInterval", true);
	pConsole->UnregisterVariable("g_RMIBenchmarkTwoTrips", true);
#endif

	pConsole->UnregisterVariable("g_hud3d_cameraDistance", true);
	pConsole->UnregisterVariable("g_hud3d_cameraOffsetZ", true);
	pConsole->UnregisterVariable("g_hud3D_cameraOverride", true);

	pConsole->UnregisterVariable("g_presaleURL", true);
	pConsole->UnregisterVariable("g_messageOfTheDay", true);
	pConsole->UnregisterVariable("g_serverImageUrl", true);

	ReleaseAIPerceptionCVars(pConsole);

#ifdef INCLUDE_GAME_AI_RECORDER
	CGameAIRecorderCVars::UnregisterVariables(pConsole);
#endif //INCLUDE_GAME_AI_RECORDER

	pConsole->UnregisterVariable("g_debugWeaponOffset", true);
	pConsole->RemoveCommand("g_weaponOffsetReset");
	pConsole->RemoveCommand("g_weaponOffsetOutput");
	pConsole->RemoveCommand("g_weaponOffsetInput");
	pConsole->RemoveCommand("g_weaponOffsetToMannequin");
	pConsole->RemoveCommand("g_weaponZoomIn");
	pConsole->RemoveCommand("g_weaponZoomOut");

	SAFE_DELETE(m_pGameLobbyCVars);
}

//------------------------------------------------------------------------
void CGame::RegisterConsoleVars()
{
	assert(m_pConsole);

	if (m_pCVars)
	{
		m_pCVars->InitCVars(m_pConsole);    
	}
}

//------------------------------------------------------------------------
void CmdDumpItemNameTable(IConsoleCmdArgs *pArgs)
{
	SharedString::CSharedString::DumpNameTable();
}

void CmdLoadDebugSave(IConsoleCmdArgs* pArgs)
{
	bool bStartImmediately =
			(pArgs->GetArgCount() > 1 && !strcmp(pArgs->GetArg(1), "1"))
		||(pArgs->GetArgCount() > 2 && !strcmp(pArgs->GetArg(2), "1"));

	if(GetISystem()->GetPlatformOS()->ConsoleLoadGame(pArgs))
	{
		if(bStartImmediately)
		{
			gEnv->pConsole->ExecuteString("loadLastSave");
		}
	}
}

static FILE* s_debugSaveFile;

static bool CB_OpenFile(const char * filename, bool append)
{
	s_debugSaveFile = fxopen(filename, append ? "ab" : "wb");
	return s_debugSaveFile != 0;
}

static bool CB_WriteToFile(const void * ptr, unsigned int size, unsigned int count)
{
	size_t written = fwrite(ptr, size, count, s_debugSaveFile);
	return written == count;
}

static bool CB_CloseFile()
{
	if(s_debugSaveFile)
	{
		fclose(s_debugSaveFile);
		s_debugSaveFile = NULL;
		return true;
	}
	return false;
}

void CmdSaveDebugSave(IConsoleCmdArgs* pArgs)
{
	if(IPlatformOS* pOS = GetISystem()->GetPlatformOS())
	{
		IPlatformOS::SDebugDump dump = { CB_OpenFile, CB_WriteToFile, CB_CloseFile };
		pOS->DebugSave(dump);
	}
}

void CmdPlayerGoto(IConsoleCmdArgs *pArgs)
{
	int iArgCount = pArgs->GetArgCount();
	if (iArgCount == 4 || iArgCount == 7)
	{
		CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());

		if (pPlayer)
		{
			IEntity * pEntity = pPlayer->GetEntity();

			Vec3 pos(ZERO);
			Ang3 rot(ZERO);

			CryLogAlways("CmdPlayerGoto() %s '%s' (%s, health=%8.2f/%8.2f%s) BEFORE pos=<%.2f %.2f %.2f> dir=<%.2f %.2f %.2f>", pEntity->GetClass()->GetName(), pEntity->GetName(),
				pPlayer->IsDead() ? "dead" : "alive", pPlayer->GetHealth(), pPlayer->GetMaxHealth(),
				pPlayer->GetActorStats()->isRagDoll ? ", RAGDOLL" : "", pEntity->GetWorldPos().x, pEntity->GetWorldPos().y, pEntity->GetWorldPos().z, pEntity->GetForwardDir().x, pEntity->GetForwardDir().y, pEntity->GetForwardDir().z);

			pos.x = (float)atof(pArgs->GetArg(1));
			pos.y = (float)atof(pArgs->GetArg(2));
			pos.z = (float)atof(pArgs->GetArg(3));

			CryLogAlways("CmdPlayerGoto() %s '%s' pos=%f, %f, %f", pEntity->GetClass()->GetName(), pEntity->GetName(), pos.x, pos.y, pos.z);
			if (iArgCount == 7)
			{
				rot.x = (float)atof(pArgs->GetArg(4));
				rot.y = (float)atof(pArgs->GetArg(5));
				rot.z = (float)atof(pArgs->GetArg(6));
				CryLogAlways("CmdPlayerGoto() %s '%s' rot=%f, %f, %f", pEntity->GetClass()->GetName(), pEntity->GetName(), rot.x, rot.y, rot.z);
			}
			
			if (gEnv->pStatoscope)
			{
				char buffer[100];
				Vec3 oldPos = pEntity->GetWorldPos();
				cry_sprintf(buffer, "Teleported from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)", oldPos.x, oldPos.y, oldPos.z, pos.x, pos.y, pos.z);
				gEnv->pStatoscope->AddUserMarker("Player", buffer);
			}

			Quat q;
			q.SetRotationXYZ(DEG2RAD(rot));
	
			pEntity->SetPos(pos);
			pEntity->SetRotation(q);
			pPlayer->SetViewRotation(q);
			pPlayer->OnTeleported();

			CryLogAlways("CmdPlayerGoto() %s '%s' (%s, health=%8.2f/%8.2f%s) AFTER pos=<%.2f %.2f %.2f> dir=<%.2f %.2f %.2f>", pEntity->GetClass()->GetName(), pEntity->GetName(),
				pPlayer->IsDead() ? "dead" : "alive", pPlayer->GetHealth(), pPlayer->GetMaxHealth(),
				pPlayer->GetActorStats()->isRagDoll ? ", RAGDOLL" : "", pEntity->GetWorldPos().x, pEntity->GetWorldPos().y, pEntity->GetWorldPos().z, pEntity->GetForwardDir().x, pEntity->GetForwardDir().y, pEntity->GetForwardDir().z);
		}
		else
		{
			GameWarning("Something called the '%s' command when there's no local player", pArgs->GetArg(0));
		}
	}
	else
	{
		GameWarning("Something called the '%s' command with the wrong number of arguments (given %d but expected either 4 or 7)", pArgs->GetArg(0), iArgCount);
	}
}

//------------------------------------------------------------------------
void CmdGoto(IConsoleCmdArgs *pArgs)
{
	// feature is mostly useful for QA purposes, the editor has a similar feature, here we can call the editor command as well

	// todo:
	// * move to CryAction
	// * if in editor and game is not active it should move editor camera
	// * third person game should work by using player position
	// * level name could be part of the string

	const CCamera &rCam = GetISystem()->GetViewCamera();
	Matrix33 m = Matrix33(rCam.GetMatrix());

	int iArgCount = pArgs->GetArgCount();

	Ang3 aAngDeg = RAD2DEG(Ang3::GetAnglesXYZ(m));		// in degrees
	Vec3 vPos = rCam.GetPosition();

	if(iArgCount==1)
	{
		gEnv->pLog->Log("$5GOTO %.2f %.2f %.2f %.2f %.2f %.2f",vPos.x,vPos.y,vPos.z,aAngDeg.x,aAngDeg.y,aAngDeg.z);
		return;
	}

	// complicated but maybe the best Entity we can move to the given spot
	IGameFramework *pGameFramework=gEnv->pGameFramework;											if(!pGameFramework)return;
	IViewSystem *pViewSystem=pGameFramework->GetIViewSystem();								if(!pViewSystem)return;
	IView *pView=pViewSystem->GetActiveView();																if(!pView)return;
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pView->GetLinkedId());	if(!pEntity)return;

	if((iArgCount==4 || iArgCount==7)
		&& sscanf(pArgs->GetArg(1),"%f",&vPos.x)==1
		&& sscanf(pArgs->GetArg(2),"%f",&vPos.y)==1
		&& sscanf(pArgs->GetArg(3),"%f",&vPos.z)==1)
	{
		Matrix34 tm = pEntity->GetWorldTM();

		tm.SetTranslation(vPos);

		if(iArgCount==7
			&& sscanf(pArgs->GetArg(4),"%f",&aAngDeg.x)==1
			&& sscanf(pArgs->GetArg(5),"%f",&aAngDeg.y)==1
			&& sscanf(pArgs->GetArg(6),"%f",&aAngDeg.z)==1)
		{
			tm.SetRotation33( Matrix33::CreateRotationXYZ(DEG2RAD(aAngDeg)) );
		}

		// if there is an editor
		char str[256];
		cry_sprintf(str,"ED_GOTO %.2f %.2f %.2f %.2f %.2f %.2f",vPos.x,vPos.y,vPos.z,aAngDeg.x,aAngDeg.y,aAngDeg.z);
		gEnv->pConsole->ExecuteString(str,true);

		pEntity->SetWorldTM(tm);
		return;
	}

	gEnv->pLog->LogError("GOTO: Invalid arguments");
}

struct SPlayerNameAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const
	{ 
		CGameLobby * pGameLobby = g_pGame->GetGameLobby();
		if(pGameLobby)
		{
			return pGameLobby->GetSessionNames().Size();
		}
		else
		{
			return 0;
		}
	};

	virtual const char* GetValue( int nIndex ) const 
	{
		CGameLobby * pGameLobby = g_pGame->GetGameLobby();
		if(pGameLobby)
		{		
			return pGameLobby->GetSessionNames().m_sessionNames[nIndex].m_name;
		}
		else
		{
			return "";
		}		
	};
};

static SPlayerNameAutoComplete s_playerNameAutoComplete;

//------------------------------------------------------------------------
void CGame::RegisterConsoleCommands()
{
	assert(m_pConsole);

	REGISTER_COMMAND("playerGoto", CmdPlayerGoto, VF_CHEAT, 
		"Get or set the current position and orientation for the player - unlike goto it actually sets the player rotation correctly\n"
		"Usage: goto\n"
		"Usage: goto x y z\n"
		"Usage: goto x y z wx wy wz\n");
	REGISTER_COMMAND("goto", CmdGoto, VF_CHEAT, 
		"Get or set the current position and orientation for the player\n"
		"Usage: goto\n"
		"Usage: goto x y z\n"
		"Usage: goto x y z wx wy wz\n");

	REGISTER_COMMAND("gotoe", "local e=System.GetEntityByName(%1); if (e) then g_localActor:SetWorldPos(e:GetWorldPos()); end", VF_CHEAT, "Set the position of a entity with an given name");

	REGISTER_COMMAND("loadactionmap", CmdLoadActionmap, 0, "Loads a key configuration file");
	REGISTER_COMMAND("restartgame", CmdRestartGame, 0, "Restarts Crysis completely.");
	REGISTER_COMMAND("i_dump_ammo_pool_stats", CmdDumpAmmoPoolStats, 0, "Dumps statistics related to the weapon ammo pool.");

	REGISTER_COMMAND("lastinv", CmdLastInv, 0, "Selects last inventory item used.");
	REGISTER_COMMAND("name", CmdName, VF_RESTRICTEDMODE, "Sets player name.");
	REGISTER_COMMAND("team", CmdTeam, VF_RESTRICTEDMODE, "Sets player team.");
	REGISTER_COMMAND("loadLastSave", CmdLoadLastSave, 0, "Loads the last savegame if available.");
	REGISTER_COMMAND("spectator", CmdSpectator, 0, "Sets the player as a spectator.");
	REGISTER_COMMAND("join_game", CmdJoinGame, VF_RESTRICTEDMODE, "Enter the current ongoing game.");
	REGISTER_COMMAND("kill", CmdKill, VF_RESTRICTEDMODE, "Kills the player.");
	REGISTER_COMMAND("takeDamage", CmdTakeDamage, VF_RESTRICTEDMODE, "Forces the player to take damage");
	REGISTER_COMMAND("revive", CmdRevive, VF_RESTRICTEDMODE, "Revives the player.");
	REGISTER_COMMAND("v_kill", CmdVehicleKill, VF_CHEAT, "Kills the players vehicle.");
	REGISTER_COMMAND("sv_restart", CmdRestart, 0, "Restarts the round.");
	REGISTER_COMMAND("sv_say", CmdSay, 0, "Broadcasts a message to all clients.");

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	REGISTER_COMMAND("sv_sendConsoleCommand", CmdSendConsoleCommand, 0, "Makes all other connected machines execute a console command.");
	REGISTER_COMMAND("net_setOnlineMode", CmdNetSetOnlineMode, 0, "Sets the net mode where available. Options are online or lan.");
#endif

	REGISTER_COMMAND("echo", CmdEcho, 0, "Echo's the text back to the console and log files.");
	REGISTER_COMMAND("i_reload", CmdReloadItems, 0, "Reloads item scripts.");

	REGISTER_COMMAND("dumpnt", CmdDumpItemNameTable, 0, "Dump ItemString table.");

	REGISTER_COMMAND("g_reloadGameRules", CmdReloadGameRules, 0, "Reload GameRules script");

	REGISTER_COMMAND("g_hitDeathReactions_reload", CmdReloadHitDeathReactions, VF_CHEAT, "Reloads hitDeathReactions for the specified actor, or for everyone if not specified");
	REGISTER_COMMAND("g_hitDeathReactions_dumpAssetUsage", CmdDumpHitDeathReactionsAssetUsage, VF_CHEAT, "Dumps information about asset usage in the system, streaming, and so.");

	REGISTER_COMMAND("g_spectacularKill_reload", CmdReloadSpectacularKill, VF_CHEAT, "Reloads the parameters for the spectacular kill class for every actor");
	REGISTER_COMMAND("pl_pickAndThrow.reloadProxies", CmdReloadPickAndThrowProxies, VF_CHEAT, "Reloads the parameters for the pick and throw proxies");
	REGISTER_COMMAND("g_impulseHandler_reload", CmdReloadImpulseHandler, VF_CHEAT, "Reloads the parameters for the impulse handler of every actor");

	REGISTER_COMMAND("g_movementTransitions_reload", CmdReloadMovementTransitions, VF_CHEAT, "Reloads all movementTransitions");

	REGISTER_COMMAND("g_nextlevel", CmdNextLevel,0,"Switch to next level in rotation or restart current one.");

	//Kick vote commands
	REGISTER_COMMAND("vote", CmdVote, VF_RESTRICTEDMODE, "Vote to kick the player the vote is against");	
	REGISTER_COMMAND("votekick", CmdStartKickVoting, VF_RESTRICTEDMODE, "Initiate kick vote.");	
	if(gEnv->pConsole != NULL)
		gEnv->pConsole->RegisterAutoComplete("votekick", &s_playerNameAutoComplete);

	REGISTER_COMMAND("preloadforstats","PreloadForStats()",VF_CHEAT,"Preload multiplayer assets for memory statistics.");

	REGISTER_COMMAND("DumpLoadingMessages", CmdListAllRandomLoadingMessages, 0, "List all messages which could appear during loading");

	REGISTER_COMMAND("FreeCamEnable", CmdFreeCamEnable, VF_CHEAT, "Enable the freecam");
	REGISTER_COMMAND("FreeCamDisable", CmdFreeCamDisable, VF_CHEAT, "Disable the freecam");
	REGISTER_COMMAND("FreeCamLockCamera", CmdFreeCamLockCamera, VF_CHEAT, "Stay in freecam but lock the camera, allowing player controls to resume");
	REGISTER_COMMAND("FreeCamUnlockCamera", CmdFreeCamUnlockCamera, VF_CHEAT, "Stay in freecam unlock the camera, stopping player controls");
	REGISTER_COMMAND("FlyCamSetPoint", CmdFlyCamSetPoint, VF_CHEAT, "Sets a fly cam point");
	REGISTER_COMMAND("FlyCamPlay", CmdFlyCamPlay, VF_CHEAT, "Plays the flycam path");

#if defined(USE_CRY_ASSERT)
	REGISTER_COMMAND("IgnoreAllAsserts", CmdIgnoreAllAsserts, VF_CHEAT, "Ignore all asserts");
#endif

	REGISTER_COMMAND("pl_reload", CmdReloadPlayer, VF_CHEAT, "Reload player's data.");
	REGISTER_COMMAND("pl_health", CmdSetPlayerHealth, VF_CHEAT, "Sets a player's health.");
	REGISTER_COMMAND("switch_game_multiplayer", CmdSwitchGameMultiplayer, VF_CHEAT, "Switches game between single and multiplayer");

	REGISTER_COMMAND("spawnActor", CmdSpawnActor, VF_CHEAT, "(<actorName>, [<x>,<y>,<z>]) Spawn an actor of class <actorName>, at position (<x>,<y>,<z>). If position is not provided it will be spawned in front of the camera.");

#if ENABLE_CONSOLE_GAME_DEBUG_COMMANDS
	REGISTER_COMMAND("g_giveAchievement", CmdGiveGameAchievement, VF_CHEAT, "Give an achievement");
#endif

#if (USE_DEDICATED_INPUT)
	REGISTER_COMMAND("spawnDummyPlayers", CmdSpawnDummyPlayer, VF_CHEAT, "Spawn some dummy players for profiling purposes.\n"
		"usage: spawnDummyPlayers [count=1]\n"
		"   or: spawnDummyPlayers [count=1] noteams  - To not assign the player to a team]\n"
		"   or: spawnDummyPlayers [count=1] team [idx] - To assign the player to a specified team number.");
	REGISTER_COMMAND("removeDummyPlayers", CmdRemoveDummyPlayers, VF_CHEAT, "Remove all dummy players.");
	REGISTER_COMMAND("setDummyPlayerState", CmdSetDummyPlayerState, VF_CHEAT, "(<entityName>, <state(s)>) Set the state of a dummy player.");
	REGISTER_COMMAND("hideAllDummyPlayers", CmdHideAllDummyPlayers, VF_CHEAT, "1/0");
#endif

	REGISTER_COMMAND("g_loadSave", CmdLoadDebugSave, VF_CHEAT, "Load all profile & game data for use with bug reporting\n"
		"Usage: g_loadSave [filename, default=SaveGame.bin] 0/1\n"
		"0=without executing resume game\n"
		"1=execute resume game");

#ifndef _RELEASE
	REGISTER_COMMAND("g_saveSave", CmdSaveDebugSave, VF_CHEAT, "Save all profile & game data for use with bug reporting\n"
		"Usage: g_saveSave [filename, default=SaveGame.bin]\n");
#endif

#if CRY_PLATFORM_DURANGO
	REGISTER_COMMAND("g_inspectConnectedStorage", CmdInspectConnectedStorage, 0, "dump connected storage blob: g_inspectConnectedStorage <blob> [0:ERA|1:SRA] [<container>] [0:log|1:dump]");
#endif

	REGISTER_COMMAND( "g_reloadGameFx", CmdReloadGameFx, 0, "Reload all game fx");

	CBodyManagerCVars::RegisterCommands();
	
#ifdef INCLUDE_GAME_AI_RECORDER
	CGameAIRecorderCVars::RegisterCommands();
#endif //INCLUDE_GAME_AI_RECORDER
}

//------------------------------------------------------------------------
void CGame::UnregisterConsoleCommands()
{
	assert(m_pConsole);

	m_pConsole->RemoveCommand("playerGoto");
	m_pConsole->RemoveCommand("goto");
	m_pConsole->RemoveCommand("sv_moveClientsTo");
	m_pConsole->RemoveCommand("gotoe");

	m_pConsole->RemoveCommand("loadactionmap");
	m_pConsole->RemoveCommand("restartgame");
	m_pConsole->RemoveCommand("i_dump_ammo_pool_stats");

	m_pConsole->RemoveCommand("lastinv");
	m_pConsole->RemoveCommand("name");
	m_pConsole->RemoveCommand("team");
	m_pConsole->RemoveCommand("loadLastSave");
	m_pConsole->RemoveCommand("spectator");
	m_pConsole->RemoveCommand("join_game");
	m_pConsole->RemoveCommand("kill");
	m_pConsole->RemoveCommand("takeDamage"); 
	m_pConsole->RemoveCommand("revive");
	m_pConsole->RemoveCommand("v_kill");
	m_pConsole->RemoveCommand("sv_restart");
	m_pConsole->RemoveCommand("sv_say");

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	m_pConsole->RemoveCommand("sv_sendConsoleCommand");
	m_pConsole->RemoveCommand("net_setOnlineMode");
#endif

	m_pConsole->RemoveCommand("echo");
	m_pConsole->RemoveCommand("i_reload");

	m_pConsole->RemoveCommand("dumpss");
	m_pConsole->RemoveCommand("dumpnt");

	m_pConsole->RemoveCommand("g_reloadGameRules");
	m_pConsole->RemoveCommand("g_hitDeathReactions_reload");
	m_pConsole->RemoveCommand("g_hitDeathReactions_dumpAssetUsage");
	m_pConsole->RemoveCommand("g_spectacularKill_reload");
	m_pConsole->RemoveCommand("pl_pickAndThrow.reloadProxies");
	m_pConsole->RemoveCommand("g_impulseHandler_reload");
	m_pConsole->RemoveCommand("g_movementTransitions_reload");

	m_pConsole->RemoveCommand("g_reloadHitDeathReactions");

	m_pConsole->RemoveCommand("g_reloadMovementTransitions");

	m_pConsole->RemoveCommand("g_nextlevel");
	m_pConsole->RemoveCommand("vote");
	m_pConsole->RemoveCommand("votekick");

	m_pConsole->RemoveCommand("preloadforstats");

	m_pConsole->RemoveCommand("DumpLoadingMessages");

	m_pConsole->RemoveCommand("FreeCamEnable");
	m_pConsole->RemoveCommand("FreeCamDisable");
	m_pConsole->RemoveCommand("FreeCamLockCamera");
	m_pConsole->RemoveCommand("FreeCamUnlockCamera");
	m_pConsole->RemoveCommand("FlyCamSetPoint");
	m_pConsole->RemoveCommand("FlyCamPlay");

	m_pConsole->RemoveCommand("IgnoreAllAsserts");

	m_pConsole->RemoveCommand("pl_reload");
	m_pConsole->RemoveCommand("pl_health");

	m_pConsole->RemoveCommand("switch_game_multiplayer");
	m_pConsole->RemoveCommand("spawnActor");
#if ENABLE_CONSOLE_GAME_DEBUG_COMMANDS
	m_pConsole->RemoveCommand("g_giveAchievement");
#endif

	m_pConsole->RemoveCommand("spawnActorAtPos");

#if (USE_DEDICATED_INPUT)
	m_pConsole->RemoveCommand("spawnDummyPlayers");
	m_pConsole->RemoveCommand("removeDummyPlayers");
	m_pConsole->RemoveCommand("setDummyPlayerState");
	m_pConsole->RemoveCommand("hideAllDummyPlayers");
#endif

	m_pConsole->RemoveCommand("g_loadSave");
#ifndef _RELEASE
	m_pConsole->RemoveCommand("g_saveSave");
#endif

#if CRY_PLATFORM_DURANGO
	m_pConsole->RemoveCommand("g_inspectConnectedStorage");
#endif

	m_pConsole->RemoveCommand("g_reloadGameFx");
	m_pConsole->RemoveCommand("bulletTimeMode");
	m_pConsole->RemoveCommand("GOCMode");

	// variables from CHUDCommon
	m_pConsole->RemoveCommand("ShowGODMode");
	m_pConsole->RemoveCommand("reloadUI");

	CBodyManagerCVars::UnregisterCommands(m_pConsole);
	
#ifdef INCLUDE_GAME_AI_RECORDER
	CGameAIRecorderCVars::UnregisterCommands(m_pConsole);
#endif //INCLUDE_GAME_AI_RECORDER
}

void CGame::CmdListAllRandomLoadingMessages(IConsoleCmdArgs *pArgs)
{
	CLoadingMessageProviderListNode::ListAll();
}

//------------------------------------------------------------------------
static bool s_freeCamActive=false;

void CGame::CmdFreeCamEnable(IConsoleCmdArgs *pArgs)
{
	if (s_freeCamActive)
	{
		CryLogAlways("free cam already enabled");
	}
	else
	{
		CryLogAlways("Enabling free cam");

		s_freeCamActive=true;

		g_pGame->GetUI()->ActivateState("no_hud");
		g_pGame->GetUI()->GetCVars()->hud_hide = 1;
		g_pGameCVars->g_detachCamera = 1;
		OnDetachCameraChanged();
		g_pGameCVars->g_moveDetachedCamera = 1;
		g_pGameCVars->watch_enabled = 0;

		// TODO cache existing enabled status?
		IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		pActionMapMan->EnableActionMap("player", false);
		pActionMapMan->EnableActionMap("flycam", true);

		IActionMap *flyCamActionMap = pActionMapMan->GetActionMap("flycam");
		IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
		assert(flyCamActionMap);
		assert(pClientActor);
		if (flyCamActionMap && pClientActor)
		{
			flyCamActionMap->SetActionListener(pClientActor->GetEntityId());
		}

		CPlayer *pClientPlayer=static_cast<CPlayer *>(pClientActor);
		assert(pClientPlayer);
		if (pClientPlayer && !pClientPlayer->IsThirdPerson())
		{
			pClientPlayer->ToggleThirdPerson();
		}

		/*
		if (g_pGameActions->FilterNoMove() )
		{
		g_pGameActions->FilterNoMove()->Enable(true);
		}
		if (g_pGameActions->FilterNoMouse())
		{
		g_pGameActions->FilterNoMouse()->Enable(true);
		}
		*/
	}
}
	
void CGame::CmdFreeCamDisable(IConsoleCmdArgs *pArgs)
{
	if (s_freeCamActive)
	{
		CryLogAlways("Disabling free cam");
		s_freeCamActive=false;

		g_pGame->GetUI()->ActivateDefaultState();
		g_pGame->GetUI()->GetCVars()->hud_hide = 0;
		g_pGameCVars->g_detachCamera = 0;
		OnDetachCameraChanged();
		g_pGameCVars->g_moveDetachedCamera = 0;
		g_pGameCVars->watch_enabled = 1;

		IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		pActionMapMan->EnableActionMap("player", true);
		pActionMapMan->EnableActionMap("flycam", false);

		/*
		if (g_pGameActions->FilterNoMove() )
		{
		g_pGameActions->FilterNoMove()->Enable(false);
		}
		if (g_pGameActions->FilterNoMouse())
		{
		g_pGameActions->FilterNoMouse()->Enable(false);
		}
		*/

		IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();

		CPlayer *pClientPlayer=static_cast<CPlayer *>(pClientActor);
		assert(pClientPlayer);
		if (pClientPlayer && pClientPlayer->IsThirdPerson())
		{
			pClientPlayer->ToggleThirdPerson();
		}
	}
	else
	{
		CryLogAlways("free cam is not enabled");
	}
}

void CGame::CmdFreeCamLockCamera(IConsoleCmdArgs *pArgs)
{
	if (s_freeCamActive)
	{
		CryLogAlways("free cam - locking camera, unlocking player");

		IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		pActionMapMan->EnableActionMap("player", true);
		pActionMapMan->EnableActionMap("flycam", false);
	}
	else
	{
		CryLogAlways("free cam is not enabled");
	}
}

void CGame::CmdFreeCamUnlockCamera(IConsoleCmdArgs *pArgs)
{
	if (s_freeCamActive)
	{
		CryLogAlways("free cam - unlocking camera, locking player");

		IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		pActionMapMan->EnableActionMap("player", false);
		pActionMapMan->EnableActionMap("flycam", true);
	}
	else
	{
		CryLogAlways("free cam is not enabled");
	}
}

void CGame::CmdFlyCamSetPoint(IConsoleCmdArgs *pArgs)
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());
	if(pPlayer)
	{
		CPlayerInput* pPlayerInput = (CPlayerInput*)pPlayer->GetPlayerInput();
		if (pPlayerInput)
		{
			if (pArgs->GetArgCount() > 3)
			{
				Vec3 position(ZERO), lookAt(0, 1, 0);
				position.x = (float)atof(pArgs->GetArg(1));
				position.y = (float)atof(pArgs->GetArg(2));
				position.z = (float)atof(pArgs->GetArg(3));
				if (pArgs->GetArgCount() > 6)
				{
					lookAt.x = (float)atof(pArgs->GetArg(4));
					lookAt.y = (float)atof(pArgs->GetArg(5));
					lookAt.z = (float)atof(pArgs->GetArg(6));
				}
				pPlayerInput->AddFlyCamPoint(position, lookAt);
			}
			else
			{
				pPlayerInput->AddFlyCamPoint();
			}
		}
	}	
}

void CGame::CmdFlyCamPlay(IConsoleCmdArgs *pArgs)
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());
	if(pPlayer)
	{
		CPlayerInput* pPlayerInput = (CPlayerInput*)pPlayer->GetPlayerInput();
		if (pPlayerInput)
		{
			pPlayerInput->FlyCamPlay();
		}
	}
}

#if defined(USE_CRY_ASSERT)
void CGame::CmdIgnoreAllAsserts(IConsoleCmdArgs *pArgs)
{
	gEnv->ignoreAllAsserts = true;	
	gEnv->bTesting = true;
}
#endif

//------------------------------------------------------------------------
void CGame::CmdLastInv(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	if (CActor *pClientActor=static_cast<CActor *>(g_pGame->GetIGameFramework()->GetClientActor()))
		pClientActor->SelectLastItem(true);
}

//------------------------------------------------------------------------
void CGame::CmdName(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
		pGameRules->RenamePlayer(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), pArgs->GetArg(1));
}

//------------------------------------------------------------------------
void CGame::CmdTeam(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
		pGameRules->ChangeTeam(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), pArgs->GetArg(1), false);
}

//------------------------------------------------------------------------
void CGame::CmdLoadLastSave(IConsoleCmdArgs *pArgs)
{
#if CRY_PLATFORM_DURANGO
	g_pGame->GetIGameFramework()->LoadGame(CRY_SAVEGAME_FILENAME CRY_SAVEGAME_FILE_EXT, true);
#else
	g_pGame->LoadLastSave();
#endif
}

//------------------------------------------------------------------------
void CGame::CmdSpectator(IConsoleCmdArgs *pArgs)
{
	if (g_pGame->GetCVars()->g_spectate_DisableManual)
		return;

	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules && pGameRules->GetSpectatorModule())
	{
		int mode=2;
		if (pArgs->GetArgCount()==2)
			mode=atoi(pArgs->GetArg(1));
		pGameRules->GetSpectatorModule()->ChangeSpectatorMode(pClientActor, mode, 0, true);
		CActor* pCActor = static_cast<CActor*>(pClientActor);

		if(pCActor)
		{
			if (gEnv->bMultiplayer && mode >= CActor::eASM_FirstMPMode && mode <= CActor::eASM_LastMPMode)
			{
				pCActor->SetSpectatorState(CActor::eASS_SpectatorMode);
			}
			else
			{
				pCActor->SetSpectatorState(CActor::eASS_Ingame);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGame::CmdJoinGame(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	if (g_pGame->GetGameRules()->GetTeamCount()>0)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules && pGameRules->GetSpectatorModule())
		pGameRules->GetSpectatorModule()->ChangeSpectatorMode(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), 0, 0, true);
}

//------------------------------------------------------------------------
void CGame::CmdKill(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	assert (pClientActor->IsClient());
	assert (pClientActor->IsPlayer());

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		HitInfo suicideInfo(pClientActor->GetEntityId(), pClientActor->GetEntityId(), pClientActor->GetEntityId(),
			10000, 0, 0, -1, CGameRules::EHitType::Punish, ZERO, ZERO, ZERO);

#if !defined(_RELEASE)
		CActor* pActor = (CActor*)pClientActor;
		CryLogAlways ("Killing %s '%s' (health=%8.2f/%8.2f, %s%s%s, stance=%s, spectating=%d)", pClientActor->GetEntity()->GetClass()->GetName(), pClientActor->GetEntity()->GetName(), pClientActor->GetHealth(), pClientActor->GetMaxHealth(), pClientActor->IsDead() ? "DEAD" : "ALIVE", pClientActor->IsGod() ? ", GOD" : "", pClientActor->IsFallen() ? ", FALLEN" : "", pActor->GetStanceInfo(pActor->GetStance())->name, pActor->GetSpectatorMode());
#endif

		pGameRules->SanityCheckHitInfo(suicideInfo, "CGame::CmdKill");
		pGameRules->ClientHit(suicideInfo);

		//Execute a second time, to skip 'mercy time' protection
		if (!gEnv->bMultiplayer)
		{
			pGameRules->ClientHit(suicideInfo);
		}
	}
}

//------------------------------------------------------------------------
void CGame::CmdTakeDamage(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	assert (pClientActor->IsClient());
	assert (pClientActor->IsPlayer());

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		const float damage = (pArgs->GetArgCount() > 1) ? atoi(pArgs->GetArg(1)) : 100.0f;
		HitInfo suicideInfo(pClientActor->GetEntityId(), pClientActor->GetEntityId(), pClientActor->GetEntityId(),
			damage, 0, 0, -1, CGameRules::EHitType::Bullet, ZERO, ZERO, ZERO);

#if !defined(_RELEASE)
		CActor* pActor = (CActor*)pClientActor;
		CryLogAlways ("Forcing damage %s '%s' (health=%8.2f/%8.2f, %s%s%s, stance=%s, spectating=%d)", pClientActor->GetEntity()->GetClass()->GetName(), pClientActor->GetEntity()->GetName(), pClientActor->GetHealth(), pClientActor->GetMaxHealth(), pClientActor->IsDead() ? "DEAD" : "ALIVE", pClientActor->IsGod() ? ", GOD" : "", pClientActor->IsFallen() ? ", FALLEN" : "", pActor->GetStanceInfo(pActor->GetStance())->name, pActor->GetSpectatorMode());
#endif

		pGameRules->SanityCheckHitInfo(suicideInfo, "CGame::CmdTakeDamage");
		pGameRules->ClientHit(suicideInfo);
	}
}

//------------------------------------------------------------------------
void CGame::CmdRevive(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CActor *pActor = static_cast<CActor*>(pClientActor);
	pActor->Revive();
}

//------------------------------------------------------------------------
void CGame::CmdVehicleKill(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	IVehicle* pVehicle = pClientActor->GetLinkedVehicle();
	if (!pVehicle)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		HitInfo suicideInfo(pVehicle->GetEntityId(), pVehicle->GetEntityId(), pVehicle->GetEntityId(),
			10000, 0, 0, -1, 0, pVehicle->GetEntity()->GetWorldPos(), ZERO, ZERO);
		pGameRules->ClientHit(suicideInfo);
	}
}

//------------------------------------------------------------------------
void CGame::CmdRestart(IConsoleCmdArgs *pArgs)
{
	if(g_pGame && g_pGame->GetGameRules())
		g_pGame->GetGameRules()->Restart();
}

//------------------------------------------------------------------------
void CGame::CmdSay(IConsoleCmdArgs *pArgs)
{
	if (gEnv->bServer)
	{
		if (pArgs->GetArgCount()>1)
		{
			const char *msg=pArgs->GetCommandLine()+strlen(pArgs->GetArg(0))+1;
			g_pGame->GetGameRules()->SendTextMessage(eTextMessageServer, msg, eRMI_ToAllClients);

			if (!gEnv->IsClient())
				CryLogAlways("** Server: %s **", msg);
		}
	}
	else
	{
		GameWarning("%s is a server-only command! It can't be used on a client.", pArgs->GetArg(0));
	}
}

//------------------------------------------------------------------------
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
void CGame::CmdSendConsoleCommand(IConsoleCmdArgs *pArgs)
{
	if (gEnv->bServer)
	{
		if (pArgs->GetArgCount()>1)
		{
			const char *msg=pArgs->GetCommandLine()+strlen(pArgs->GetArg(0))+1;
			g_pGame->GetGameRules()->SendNetConsoleCommand(msg, eRMI_ToRemoteClients);
		}
	}
	else
	{
		GameWarning("%s is a server-only command! It can't be used on a client.", pArgs->GetArg(0));
	}
}

//------------------------------------------------------------------------
void CGame::CmdNetSetOnlineMode(IConsoleCmdArgs *pArgs)
{
	if (g_pGame->GetIGameFramework()->StartedGameContext()==false && g_pGame->GetIGameFramework()->GetClientChannel()==NULL) // Not in a game.
	{
		if (pArgs->GetArgCount()>1)
		{
			const char *arg = pArgs->GetArg(1);
			if (arg && strcmpi(arg,"online")==0)
			{
				CGameLobby::SetLobbyService(eCLS_Online);
			}
			else if (arg && strcmpi(arg,"lan")==0)
			{
				CGameLobby::SetLobbyService(eCLS_LAN);
			}
			else
			{
				GameWarning("%s unknown type. Options are lan or online.", pArgs->GetArg(0));
			}
		}
	}
	else
	{
		GameWarning("%s cannot be used while in a game.", pArgs->GetArg(0));
	}
}
#endif

//------------------------------------------------------------------------
void CGame::CmdEcho(IConsoleCmdArgs *pArgs)
{
	const char* message = pArgs->GetCommandLine();
	if (strlen(message) > 5)
	{
		CryLogAlways("%s", pArgs->GetCommandLine() + 5);
	}
}

//------------------------------------------------------------------------
void CGame::CmdLoadActionmap(IConsoleCmdArgs *pArgs)
{
	if(pArgs->GetArg(1))
		g_pGame->LoadActionMaps(pArgs->GetArg(1));
}

//------------------------------------------------------------------------
void CGame::CmdRestartGame(IConsoleCmdArgs *pArgs)
{
	GetISystem()->Relaunch(true);
	GetISystem()->Quit();
}

//------------------------------------------------------------------------
void CGame::CmdDumpAmmoPoolStats(IConsoleCmdArgs *pArgs)
{
	g_pGame->GetWeaponSystem()->DumpPoolSizes();
}

//------------------------------------------------------------------------
void CGame::CmdReloadItems(IConsoleCmdArgs *pArgs)
{
	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	pItemSystem->PreReload();

	g_pGame->GetGameSharedParametersStorage()->ResetItemParameters();
	g_pGame->GetGameSharedParametersStorage()->ResetWeaponParameters();
	g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().FlushCaches();
	
	g_pGame->GetWeaponSystem()->Reload();
	pItemSystem->PostReload();
}

//------------------------------------------------------------------------
void CGame::CmdReloadGameRules(IConsoleCmdArgs *pArgs)
{
	if (gEnv->bMultiplayer)
		return;

	IGameRulesSystem* pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
	IGameRules* pGameRules = pGameRulesSystem->GetCurrentGameRules();

	const char* name = "SinglePlayer";
	IEntityClass* pEntityClass = 0; 

	if (pGameRules)    
	{
		pEntityClass = pGameRules->GetEntity()->GetClass();
		name = pEntityClass->GetName();
	}  
	else
		pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);

	if (pEntityClass)
	{
		pEntityClass->LoadScript(true);

		if (pGameRulesSystem->CreateGameRules(name))
			CryLog("reloaded GameRules <%s>", name);
		else
			GameWarning("reloading GameRules <%s> failed!", name);
	}  
}

//------------------------------------------------------------------------
void CGame::CmdReloadHitDeathReactions(IConsoleCmdArgs* pArgs)
{
	g_pGame->GetHitDeathReactionsSystem().Reload();

	if (pArgs->GetArgCount() > 1)
	{
		IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pArgs->GetArg(1));
		if (pEntity)
		{
			IActor* pIActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
			if (pIActor && (pIActor->GetActorClass() == CPlayer::GetActorClassType()))
			{
				CPlayer* pActor = static_cast<CPlayer*>(pIActor);
				CHitDeathReactionsPtr pHitDeathReactions = pActor->GetHitDeathReactions();
				if (pHitDeathReactions)
					pHitDeathReactions->Reload();
			}
		}
	}
	else
	{
		IActorIteratorPtr pIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
		while (IActor* pIActor = pIt->Next())
		{
			if (pIActor->GetActorClass() == CPlayer::GetActorClassType())
			{
				CPlayer* pActor = static_cast<CPlayer*>(pIActor);
				CHitDeathReactionsPtr pHitDeathReactions = pActor->GetHitDeathReactions();
				if (pHitDeathReactions)
					pHitDeathReactions->Reload();
			}
		}
	}
}

//------------------------------------------------------------------------
void CGame::CmdDumpHitDeathReactionsAssetUsage(IConsoleCmdArgs* pArgs)
{
	g_pGame->GetHitDeathReactionsSystem().DumpHitDeathReactionsAssetUsage();
}

//------------------------------------------------------------------------
void CGame::CmdReloadSpectacularKill(IConsoleCmdArgs* pArgs)
{
	g_pGame->GetIGameFramework()->GetIActorSystem()->Reload();
	gEnv->pGameFramework->GetISharedParamsManager()->RemoveByType(CSpectacularKill::SSharedSpectacularParams::s_typeInfo);

	IActorIteratorPtr pIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	while (IActor* pIActor = pIt->Next())
	{
		if (pIActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pIActor);
			const IItemParamsNode* pEntityClassParamsNode = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(pPlayer->GetEntityClassName());
			if (pEntityClassParamsNode)
			{
				CSpectacularKill& spectacularKill = pPlayer->GetSpectacularKill();
				spectacularKill.ReadXmlData(pEntityClassParamsNode);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGame::CmdReloadImpulseHandler(IConsoleCmdArgs* pArgs)
{
	g_pGame->GetIGameFramework()->GetIActorSystem()->Reload();
	gEnv->pGameFramework->GetISharedParamsManager()->RemoveByType(CActorImpulseHandler::SSharedImpulseHandlerParams::s_typeInfo);

	IActorIteratorPtr pIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	while (IActor* pIActor = pIt->Next())
	{
		CActor* pActor = static_cast<CActor*>(pIActor);
		const IItemParamsNode* pEntityClassParamsNode = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(pActor->GetEntityClassName());
		if (pEntityClassParamsNode)
		{
			CActorImpulseHandlerPtr pImpulseHandler = pActor->GetImpulseHander();
			CRY_ASSERT(pImpulseHandler != NULL);
			pImpulseHandler->ReadXmlData(pEntityClassParamsNode);
		}
	}
}

//------------------------------------------------------------------------
void CGame::CmdReloadPickAndThrowProxies(IConsoleCmdArgs* pArgs)
{
	ReloadPickAndThrowProxiesOnChange(NULL);
}

//------------------------------------------------------------------------
void CGame::CmdReloadMovementTransitions(IConsoleCmdArgs* pArgs)
{
	g_pGame->GetMovementTransitionsSystem().Reload(); // TODO specific reload of specific entity classes (and then change command description)
}


//------------------------------------------------------------------------
void CGame::CmdNextLevel(IConsoleCmdArgs* pArgs)
{
	ILevelRotation *pLevelRotation = g_pGame->GetPlaylistManager()->GetLevelRotation();
	if (pLevelRotation->GetLength())
		pLevelRotation->ChangeLevel(pArgs);
}

void CGame::CmdStartKickVoting(IConsoleCmdArgs* pArgs)
{
	if (!gEnv->IsClient())
		return;

	if (pArgs->GetArgCount() < 2)
	{
		GameWarning("usage: votekick <player_name>");
		return;
	}

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pArgs->GetArg(1));
	if(pEntity)
	{
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
		if(pActor && pActor->IsPlayer())
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();
			if (pGameRules)
			{
				pGameRules->StartVoting(pClientActor,eVS_kick,pEntity->GetId(),"");
			}
		}
	}
}


void CGame::CmdVote(IConsoleCmdArgs* pArgs)
{
	if (!gEnv->IsClient())
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->Vote(pClientActor, true);
	}
}

void CGame::CmdReloadPlayer(IConsoleCmdArgs* pArgs)
{
	gEnv->pGameFramework->GetIActorSystem()->Reload();

	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());
	if(pPlayer)
	{
		pPlayer->ReloadClientXmlData();
	}
}

#if ENABLE_CONSOLE_GAME_DEBUG_COMMANDS

void CGame::CmdGiveGameAchievement(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() < 2)
		return;

	CGameAchievements* pGameAchievements = g_pGame->GetGameAchievements();
	if (pGameAchievements)
	{
		const int achievementId = atoi(pArgs->GetArg(1));
		if ((achievementId >= 0) && (achievementId < eC3A_NumAchievements))
		{
			pGameAchievements->GiveAchievement( ECrysis3Achievement(achievementId) );
		}
	}
}

#endif //#if ENABLE_CONSOLE_GAME_DEBUG_COMMANDS

void CGame::CmdSetPlayerHealth(IConsoleCmdArgs* pArgs)
{
	IActor *pActor = gEnv->pGameFramework->GetClientActor();
	if(pActor)
	{
		if( pArgs->GetArgCount() != 2 )
		{
			CryLog("pl_health : invalid input : 'pl_health <new_health>' (max health is %8.2f)", pActor->GetMaxHealth() );
			return;
		}

		float newHealth = (float)atof( pArgs->GetArg(1) );
		if( newHealth == 0.0f && (strcmp( "0", pArgs->GetArg(1) ) || stricmp( "0.0f", pArgs->GetArg(1) ) ) ) // newHealth is 0 and string input is not "0" i.e. atof fails to parse
		{
			CryLog("pl_health : float required for input. 'pl_health <new_health>' (max health is %8.2f)", pActor->GetMaxHealth() );
			return;		
		}

		pActor->SetHealth(newHealth);
	}
	else
	{
		CryLog("pl_health : no actor found!");
	}
}

void CGame::CmdSwitchGameMultiplayer(IConsoleCmdArgs* pArgs)
{
	if( pArgs->GetArgCount() != 2 )
	{
		CryLogAlways("switch_game_multiplayer : invalid input : 'switch_game_multiplayer <0 - singleplayer/1 - multiplayer>'" );
		return;
	}
}

void CalculatePointInFrontOfCamera(float distanceInFront, Vec3* position)
{
	assert(position);

	if (IActor* localActor = gEnv->pGameFramework->GetClientActor())
	{
		if (localActor->IsPlayer())
		{
			CPlayer* player = static_cast<CPlayer*>(localActor);
			*position = player->GetFPCameraPosition(true) + player->GetViewMatrix().GetColumn1() * distanceInFront;
		}
	}
}

void CGame::CmdSpawnActor(IConsoleCmdArgs *pArgs)
{
	int argCount = pArgs->GetArgCount();

	if (argCount >= 2)
	{
		const char* actorClass = pArgs->GetArg(1);

		// Create a unique entity name
		stack_string actorName;
		unsigned int index = 0;
		do
	{
			actorName.Format("%s%d", actorClass, index++);
		}
		while (gEnv->pEntitySystem->FindEntityByName(actorName) != NULL);

		Vec3 spawnPosition(ZERO);

		if (argCount >= 5)
		{
			// Spawn actor at position specified by caller
			spawnPosition.x = (float)atof(pArgs->GetArg(2));
			spawnPosition.y = (float)atof(pArgs->GetArg(3));
			spawnPosition.z = (float)atof(pArgs->GetArg(4));
		}
		else
		{
			// No position specified, spawn actor in front of camera
			const float distanceInFront = 3.0f;
			CalculatePointInFrontOfCamera(distanceInFront, &spawnPosition);
		}

		CryLogAlways("CmdSpawnActorAtPos() spawning actorClass=%s; with name=%s; at pos=(%f, %f, %f)", actorClass, actorName.c_str(), spawnPosition.x, spawnPosition.y, spawnPosition.z);
		IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->CreateActor(0, actorName.c_str(), actorClass, spawnPosition, Quat(IDENTITY), Vec3Constants<float>::fVec3_One);
		if (pActor)
		{
			CryLogAlways("CmdSpawnActorAtPos() successfully created actor");
		}
		else
		{
			CryLogAlways("CmdSpawnActorAtPos() failed to create actor");
		}
	}
	else
	{
		CryLogAlways("spawnActor actorClass [posx posy posz]");
	}
}

void CGame::CmdReloadGameFx(IConsoleCmdArgs *pArgs)
{
	GAME_FX_SYSTEM.ReloadData();
}

#if (USE_DEDICATED_INPUT)

void CGame::CmdSpawnDummyPlayer(IConsoleCmdArgs* pArgs)
{
	if(!gEnv->bServer)
		return;
#if 0
	const CCamera &rCam = gEnv->pRenderer->GetCamera();

	Vec3 pos = rCam.GetPosition();
	Vec3 forward = rCam.GetViewdir();
#else
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGameFramework->GetClientActor());
	Vec3 pos = pPlayer ? pPlayer->GetEntity()->GetPos() : Vec3(1,1,1);
	Vec3 forward = pPlayer ? pPlayer->GetEntity()->GetWorldRotation().GetColumn1() : Vec3(0,0,1);
#endif

	Vec3 scale(1.0f, 1.0f, 1.0f);

	static int numDummiesSoFar = 0;
	int numDummies = 1;
	bool chooseTeams = true;
	int specifiedTeam = 0; 

	
	for( int i=1; i<pArgs->GetArgCount();i++)
	{
		string currentArg = pArgs->GetArg(i);
		if(currentArg.compareNoCase("noteams") == 0)
		{
			chooseTeams = false;
		}
		else if(currentArg.compareNoCase("team") == 0)
		{
			chooseTeams = false;
			// The parameter after the team argument should be the team number. Default to 0 if not specified. 
			specifiedTeam = (i+1 < pArgs->GetArgCount())? atoi(pArgs->GetArg(i+1)):0;
		}
		else if(i==1)
		{
			// You are not forced to specify a number of dummy players to spawn. If you do specify this 
			// it must always be the first parameter. If not specified, The default is 1. 
			numDummies = atoi(pArgs->GetArg(1));
			if( numDummies == 0)
			{
				numDummies = 1;
			}
		}
	}

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		for (int i=0; i<numDummies; i++)
		{
			pos += forward * 2;
			CryFixedStringT<16> name("DummyPlayer");
			name.Format("%s%d", name.c_str(), numDummiesSoFar+i);
			CDummyPlayer* pDummy = (CDummyPlayer*)gEnv->pGameFramework->GetIActorSystem()->CreateActor(0, name.c_str(), "DummyPlayer", pos, Quat(IDENTITY), scale);
			if (chooseTeams)
			{
				IGameRulesTeamsModule *teamModule = pGameRules->GetTeamsModule();
				if (teamModule)
				{
					EntityId playerEntityId = pDummy->GetEntityId();
					pGameRules->SetTeam(teamModule->GetAutoAssignTeamId(playerEntityId), playerEntityId);
				}
			}
			else if(specifiedTeam>0 && pGameRules->IsValidPlayerTeam(specifiedTeam))
			{
				EntityId playerEntityId = pDummy->GetEntityId();
				pGameRules->SetTeam(specifiedTeam, playerEntityId);
			}
		}
	}

	numDummiesSoFar += numDummies;
}

void CGame::CmdRemoveDummyPlayers(IConsoleCmdArgs* pArgs)
{
	if(!gEnv->bServer)
		return;

	int numPlayersRemoved = 0;
	IEntityClass* pDummyClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DummyPlayer");
	IEntityItPtr entIt = gEnv->pEntitySystem->GetEntityIterator();
	if (entIt)
	{
		entIt->MoveFirst();
		while (!entIt->IsEnd())
		{
			IEntity* ent = entIt->Next();
			if (ent && ent->GetClass() == pDummyClass)
			{
				gEnv->pEntitySystem->RemoveEntity(ent->GetId());
				numPlayersRemoved++;
			}
		}
	}
	CryLogAlways("CmdRemoveDummyPlayers() removed %d dummy players", numPlayersRemoved);
}

void CGame::CmdSetDummyPlayerState(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 1)
	{
		const char* entityName = pArgs->GetArg(1);
		IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(entityName);
		if (pEntity)
		{
			IEntityClass* pDummyClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DummyPlayer");
			if (pEntity->GetClass() == pDummyClass)
			{
				CDummyPlayer* pDummyPlayer = (CDummyPlayer*)g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
				for (int i = 2; i<pArgs->GetArgCount(); i++)
				{
					bool parsed = false;
					string stateString = pArgs->GetArg(i);
					int pos = 0;
					string key = stateString.Tokenize(":", pos);
					if (!key.empty())
					{
						string valueString = stateString.Tokenize(":", pos);
						if (!valueString.empty())
						{
							parsed = true;
							bool validValue = true;
							EDefaultableBool value = eDB_Default;
							if (valueString.compareNoCase("default") == 0 || valueString.compareNoCase("-1") == 0)
							{
								value = eDB_Default;
							}
							else if (valueString.compareNoCase("false") == 0 || valueString.compareNoCase("0") == 0)
							{
								value = eDB_False;
							}
							else if (valueString.compareNoCase("true") == 0 || valueString.compareNoCase("1") == 0)
							{
								value = eDB_True;
							}
							else
							{
								validValue = false;
								CryLogAlways("CmdSetDummyPlayerState() unrecognised value: %s in %s", valueString.c_str(), stateString.c_str());
							}
							if (validValue)
							{
								if (key.compareNoCase("fire") == 0)
								{
									pDummyPlayer->SetFire(value);
								}
								else if (key.compareNoCase("move") == 0)
								{
									pDummyPlayer->SetMove(value);
								}
								else
								{
									CryLogAlways("CmdSetDummyPlayerState() unrecognised key: %s in %s", key.c_str(), stateString.c_str());
								}
							}
						}
					}
					if (!parsed)
					{
						CryLogAlways("CmdSetDummyPlayerState() unrecognised argument: %s", stateString.c_str());
					}
				}
			}
			else
			{
				CryLogAlways("CmdSetDummyPlayerState() wrong type: %s is a %s and not a DummyPlayer", entityName, pEntity->GetClass()->GetName());
			}
		}
		else
		{
			CryLogAlways("CmdSetDummyPlayerState() couldn't find %s", entityName);
		}
	}
	else
	{
		CryLogAlways("CmdSetDummyPlayerState() must be given some parameters e.g. setDummyPlayerState DummyPlayer0 fire:true move:default");
	}
}


#if CRY_PLATFORM_DURANGO
void CGame::CmdInspectConnectedStorage(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() > 1)
	{
		const char* blobName = pArgs->GetArg(1);
		IPlatformOS::EConnectedStorageType type = IPlatformOS::eCST_ERA;
		const char* containerName = "PlayerProfileAttributes";
		bool dumpToFile = false;
		if (pArgs->GetArgCount() > 2)
		{
			type = atoi(pArgs->GetArg(2)) == 0 ? IPlatformOS::eCST_ERA : IPlatformOS::eCST_SRA;
		}
		if (pArgs->GetArgCount() > 3)
		{
			containerName = pArgs->GetArg(3);
		}
		if (pArgs->GetArgCount() > 4)
		{
			dumpToFile = atoi(pArgs->GetArg(4)) != 0;
		}

		size_t numConverted = 0;

		wstring containerNameW;
		Unicode::Convert(containerNameW, containerName);

		wstring blobNameW;
		Unicode::Convert(blobNameW, blobName);

		IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();

		const size_t expectedBlockSize = 512*1024;
		static uint8 readBuffer[expectedBlockSize] = {0};
		memset(readBuffer, 0, expectedBlockSize);
		IPlatformOS::TContainerDataBlocks block;
		block.resize(1);

		block[0].blockName = blobNameW.c_str();
		block[0].pDataBlock = readBuffer;
		block[0].dataBlockSize = expectedBlockSize;

		if (pOS->LoadFromStorage(type, containerNameW.c_str(), block))
		{
			if (dumpToFile)
			{
				char path[ICryPak::g_nMaxPath] = "";
				gEnv->pCryPak->AdjustFileName(string("%USER%\\ConnectedStorageDump\\") + containerName + "\\", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
				if (gEnv->pCryPak->MakeDir(path))
				{
					cry_strcat(path, blobName);

					if (FILE* pFile = gEnv->pCryPak->FOpen(path, "wt"))
					{
						gEnv->pCryPak->FWrite(block[0].pDataBlock, 1, block[0].dataBlockSize, pFile);
						gEnv->pCryPak->FClose(pFile);

						CryLogAlways("CmdInspectConnectedStorage: Container:%s blob:%s dump to:%s", containerName, blobName, path);
					}
				}
			}
			else
			{
				assert(block[0].dataBlockSize < expectedBlockSize);
				block[0].pDataBlock[expectedBlockSize-1] = '\0';

				CryLogAlways("CmdInspectConnectedStorage: Container:%s blob:%s\n%s", containerName, blobName, block[0].pDataBlock);

				string blob((char*)block[0].pDataBlock, block[0].dataBlockSize);
				while (!blob.empty())
				{
					const size_t charsPerLine = 80;
					string display = blob.Left(charsPerLine);
					CryLogAlways("> %s", display.c_str());

					blob.erase(0, charsPerLine);
				}
			}
		}
		else
		{
			GameWarning("CmdInspectConnectedStorage: LoadFromStorage failed: Container:%s blob:%s", containerName, blobName);
		}
	}
}
#endif

void CGame::CmdHideAllDummyPlayers(IConsoleCmdArgs* pCmdArgs)
{
	if(!gEnv->bServer)
		return;

	if(pCmdArgs && (pCmdArgs->GetArgCount() == 2))
	{
		const bool bHidePlayers = (atoi(pCmdArgs->GetArg(1))) ? true : false;

		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		if(pEntitySystem)
		{
			IEntityClassRegistry* pEntityClassRegistry = pEntitySystem->GetClassRegistry();
			if(pEntityClassRegistry)
			{
				IEntityClass* pDummyClass = pEntityClassRegistry->FindClass("DummyPlayer");
				if(pDummyClass)
				{
					IEntityItPtr pEntityIterator = pEntitySystem->GetEntityIterator();
					if(pEntityIterator)
					{
						// Itterate over all dummy players
						pEntityIterator->MoveFirst();
						while(!pEntityIterator->IsEnd())
						{
							IEntity* pEntity = pEntityIterator->Next();
							if(pEntity && (pEntity->GetClass() == pDummyClass))
							{
								// Hide character
								IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
								if(pIEntityRender)
								{
									IRenderNode* pRenderNode = pIEntityRender->GetRenderNode();
									if(pRenderNode)
									{
										pRenderNode->Hide(bHidePlayers);
									}
								}

								// Hide weapon
								if(g_pGame)
								{
									IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
									if(pGameFramework)
									{
										IActorSystem* pActorSystem = pGameFramework->GetIActorSystem();
										if(pActorSystem)
										{
											IActor* pActor = pActorSystem->GetActor(pEntity->GetId());
											if(pActor && pActor->IsPlayer())
											{
												CPlayer* pPlayer = (CPlayer*)pActor;
												IItem* pItem = pPlayer->GetCurrentItem();
												if(pItem)
												{
													pItem->HideItem(bHidePlayers);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}


#endif
