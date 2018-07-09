// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LocalPlayerComponent.h"
#include "ScreenEffects.h"
#include "Player.h"
#include "Utility/CryWatch.h"
#include "RecordingSystem.h"
#include "AutoAimManager.h"
#include <CryGame/GameUtils.h>
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesDamageHandlingModule.h"
#include "GameCVars.h"
#include "Item.h"
#include "Weapon.h"
#include "PlayerStateSwim.h"
#include "PersistantStats.h"
#include "GamePhysicsSettings.h"
#include "GameActions.h"
#include "NetPlayerInput.h"
#include "ActorManager.h"


#ifndef _RELEASE
static int		g_spectate_follow_debug_enable;
static int		g_spectate_follow_debug_setDebugSettingsToCurrentViewSettings;
static float	g_spectate_follow_debug_targetOffset_x;
static float	g_spectate_follow_debug_targetOffset_y;
static float	g_spectate_follow_debug_targetOffset_z;
static float	g_spectate_follow_debug_viewOffset_x;
static float	g_spectate_follow_debug_viewOffset_y;
static float	g_spectate_follow_debug_viewOffset_z;
static float	g_spectate_follow_debug_offsetspeed_x;
static float	g_spectate_follow_debug_offsetspeed_y;
static float	g_spectate_follow_debug_offsetspeed_z;
static float	g_spectate_follow_debug_desiredDistance;
static float	g_spectate_follow_debug_cameraHeightOffset;
static float	g_spectate_follow_debug_cameraYaw;
static float	g_spectate_follow_debug_cameraSpeed;
static float	g_spectate_follow_debug_cameraFov;
static int		g_spectate_follow_debug_allowOrbitYaw;
static int		g_spectate_follow_debug_allowOrbitPitch;
static int		g_spectate_follow_debug_useEyeDir;
static int		g_spectate_follow_debug_userSelectable;
#endif //_RELEASE

CLocalPlayerComponent::CLocalPlayerComponent(CPlayer& rParentPlayer)
: m_playedMidHealthSound(false)
, m_fpCompleteBodyVisible(false)
, m_freeFallDeathFadeTimer(0.0f)
, m_screenFadeEffectId(InvalidEffectId)
, m_bIsInFreeFallDeath(false)
, m_rPlayer(rParentPlayer)
, m_currentFollowCameraSettingsIndex(0)
, m_autoArmourFlags(0)
{
	m_freeFallLookTarget.Set(0.f, 0.f, 0.f);
	
	m_lastMeleeParams.m_boostedMelee = false;
	m_lastMeleeParams.m_hitNormal.Set(0.f, 0.f, 0.f);
	m_lastMeleeParams.m_hitOffset.Set(0.f, 0.f, 0.f);
	m_lastMeleeParams.m_surfaceIdx = 0;
	m_lastMeleeParams.m_targetId = 0;

	m_timeForBreath = 0;

	m_stapElevLimit = gf_PI * 0.5f;
	m_lastSTAPCameraDelta.SetIdentity();

	if( gEnv->bMultiplayer )
	{
		m_rPlayer.GetInventory()->AddListener( this );
	}

#ifndef _RELEASE
	REGISTER_CVAR(g_spectate_follow_debug_enable, 0, 0, "Enables A cvar controlled camera mode");
	REGISTER_CVAR(g_spectate_follow_debug_setDebugSettingsToCurrentViewSettings, 0, 0, "Sets the debug camera mode settings to match the currently selected camera settings");
	REGISTER_CVAR(g_spectate_follow_debug_targetOffset_x, 0.f, 0, "where the camera should point at relative to the target entity. The value is in the target's local space");
	REGISTER_CVAR(g_spectate_follow_debug_targetOffset_y, 0.f, 0, "where the camera should point at relative to the target entity. The value is in the target's local space");
	REGISTER_CVAR(g_spectate_follow_debug_targetOffset_z, 1.5f,  0, "where the camera should point at relative to the target entity. The value is in the target's local space");
	REGISTER_CVAR(g_spectate_follow_debug_viewOffset_x, 0.f, 0, "the amount the camera should be shifted by. Does not affect rotation. The value is in the view space.");
	REGISTER_CVAR(g_spectate_follow_debug_viewOffset_y, 0.f, 0, "the amount the camera should be shifted by. Does not affect rotation. The value is in the view space.");
	REGISTER_CVAR(g_spectate_follow_debug_viewOffset_z, 0.f, 0, "the amount the camera should be shifted by. Does not affect rotation. The value is in the view space.");
	REGISTER_CVAR(g_spectate_follow_debug_offsetspeed_x, 1.f, 0, "the speed for the left-right camera offset from the target (in cam space).");
	REGISTER_CVAR(g_spectate_follow_debug_offsetspeed_y, 1.f, 0, "the speed for the in-out camera offset from the target (in cam space).");
	REGISTER_CVAR(g_spectate_follow_debug_offsetspeed_z, 1.f, 0, "the speed for the up-down camera offset from the target (in cam space).");
	REGISTER_CVAR(g_spectate_follow_debug_desiredDistance, 3.0f,  0, "how far the camera should be placed from the target. This is an ideal value which will change to avoid obstructions");
	REGISTER_CVAR(g_spectate_follow_debug_cameraHeightOffset, 0.75f,  0, "how high/low the camera should be placed in relation to the target entity");
	REGISTER_CVAR(g_spectate_follow_debug_cameraYaw, 0.0f,  0, "camera's rotation about the target entity. The value is in degrees and moves clockwise about the target. 0.0 faces the same direction as the target");
	REGISTER_CVAR(g_spectate_follow_debug_cameraSpeed, 5.f,  0, "how much the camera 'lags' when following a moving target. A value of 0 is an instant snap");
	REGISTER_CVAR(g_spectate_follow_debug_cameraFov, 0.0f,  0, "camera's Field of view. The value is in degrees. 0.0 uses the default existing");
	REGISTER_CVAR(g_spectate_follow_debug_allowOrbitYaw, 1, 0, "allows the player to adjust the camera yaw value themselves");
	REGISTER_CVAR(g_spectate_follow_debug_allowOrbitPitch, 1, 0, "allows the player to adjust the camera pitch value themselves");
	REGISTER_CVAR(g_spectate_follow_debug_useEyeDir, 0, 0, "uses the target's eye direction to control the camera rather than the entity matrix");
	REGISTER_CVAR(g_spectate_follow_debug_userSelectable, 1, 0, "allows the view to be used changed to during the follow mode.");

	m_followCameraDebugSettings.m_settingsName = "Debug";
#endif //_RELEASE
}

CLocalPlayerComponent::~CLocalPlayerComponent()
{
	if( gEnv->bMultiplayer )
	{
		m_rPlayer.GetInventory()->RemoveListener( this );
	}

#ifndef _RELEASE
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_enable");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_setDebugSettingsToCurrentViewSettings");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_targetOffset_x");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_targetOffset_y");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_targetOffset_z");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_viewOffset_x");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_viewOffset_y");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_viewOffset_z");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_offsetspeed_x");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_offsetspeed_y");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_offsetspeed_z");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_desiredDistance");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_cameraHeightOffset");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_cameraYaw");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_cameraSpeed");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_cameraFov");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_allowOrbitYaw");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_allowOrbitPitch");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_useEyeDir");
	gEnv->pConsole->UnregisterVariable("g_spectate_follow_debug_userSelectable");
#endif //_RELEASE
}

void CLocalPlayerComponent::Revive()
{
	if(gEnv->bMultiplayer)
	{
		// Reset NotYetSpawned filter.
		IActionFilter* pNYSFilter = g_pGameActions->FilterNotYetSpawned();
		if(pNYSFilter && pNYSFilter->Enabled())
		{
			pNYSFilter->Enable(false);
		}

		// For Modes where we can swap teams per round, refresh everyone else's cloak colour on revive.
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if( pGameRules->GetGameMode()==eGM_Gladiator )
		{
			IActorSystem* pActorSys = g_pGame->GetIGameFramework()->GetIActorSystem();
			CActorManager* pActorManager = CActorManager::GetActorManager();
			pActorManager->PrepareForIteration();
			const int kNumActors = pActorManager->GetNumActors();
			for(int i=0; i<kNumActors; i++)
			{
				SActorData actorData;
				pActorManager->GetNthActorData(i, actorData);		
				if(CActor* pActor = static_cast<CActor*>(pActorSys->GetActor(actorData.entityId)))
				{
					if(pActor->IsCloaked())
					{
						pActor->SetCloakLayer(false);
						pActor->SetCloakLayer(true);
					}
				}
			}
		}
	}

	m_bIsInFreeFallDeath = false;
	m_playedMidHealthSound = false;
}

void CLocalPlayerComponent::OnKill()
{
	// Stop Movement Input
	g_pGameActions->FilterNoMove()->Enable(true);

	if(gEnv->bMultiplayer)
	{
		// Set NotYetSpawned filter.
		IActionFilter* pNYSFilter = g_pGameActions->FilterNotYetSpawned();
		if(pNYSFilter && !pNYSFilter->Enabled())
		{
			pNYSFilter->Enable(true);
		}

		// Re-cache FP weapon geometry from load-out just before respawn.
		if(CEquipmentLoadout* pEquipmentLoadout = g_pGame->GetEquipmentLoadout())
		{
			pEquipmentLoadout->StreamFPGeometry(CEquipmentLoadout::ePGE_OnDeath, pEquipmentLoadout->GetCurrentSelectedPackageIndex());
		}
	}
}

void CLocalPlayerComponent::InitFollowCameraSettings( const IItemParamsNode* pFollowCameraNode )
{
	int numSettings = pFollowCameraNode->GetChildCount();
	m_followCameraSettings.resize(numSettings);
	for(int i = 0; i < numSettings; ++i)
	{
		const IItemParamsNode* pCameraModeNode = pFollowCameraNode->GetChild(i);
		SFollowCameraSettings& mode = m_followCameraSettings[i];
		float cameraYawDeg = 0.f;
		float cameraFovDeg = 0.f;
		int allowOrbitYaw = 0;
		int allowOrbitPitch = 0;
		int useEyeDir = 0;
		int userSelectable = 0;
		int disableHeightAdj = 0;

		bool success = true;

		success &= pCameraModeNode->GetAttribute("targetOffset", mode.m_targetOffset);
		success &= pCameraModeNode->GetAttribute("viewOffset", mode.m_viewOffset);
		success &= pCameraModeNode->GetAttribute("offsetSpeeds", mode.m_offsetSpeeds);
		success &= pCameraModeNode->GetAttribute("desiredDistance", mode.m_desiredDistance);
		success &= pCameraModeNode->GetAttribute("cameraHeightOffset", mode.m_cameraHeightOffset);
		success &= pCameraModeNode->GetAttribute("cameraYaw", cameraYawDeg);
		success &= pCameraModeNode->GetAttribute("cameraFov", cameraFovDeg);
		success &= pCameraModeNode->GetAttribute("cameraSpeed", mode.m_cameraSpeed);
		success &= pCameraModeNode->GetAttribute("cameraSpeed", mode.m_cameraSpeed);
		success &= pCameraModeNode->GetAttribute("allowOrbitYaw", allowOrbitYaw);
		success &= pCameraModeNode->GetAttribute("allowOrbitPitch", allowOrbitPitch);
		success &= pCameraModeNode->GetAttribute("useEyeDir", useEyeDir);
		success &= pCameraModeNode->GetAttribute("userSelectable", userSelectable);
		success &= pCameraModeNode->GetAttribute("disableHeightAdj", disableHeightAdj);

		CRY_ASSERT_MESSAGE(success, string().Format("CLocalPlayerComponent::InitFollowCameraSettings - Invalid follow camera settings - mode %i. Check XML data.", i+1));

		mode.m_cameraYaw = DEG2RAD(cameraYawDeg);
		mode.m_cameraFov = DEG2RAD(cameraFovDeg);
		mode.m_flags |= allowOrbitYaw > 0 ? SFollowCameraSettings::eFCF_AllowOrbitYaw : SFollowCameraSettings::eFCF_None;
		mode.m_flags |= allowOrbitPitch > 0 ? SFollowCameraSettings::eFCF_AllowOrbitPitch : SFollowCameraSettings::eFCF_None;
		mode.m_flags |= useEyeDir > 0 ? SFollowCameraSettings::eFCF_UseEyeDir : SFollowCameraSettings::eFCF_None;
		mode.m_flags |= userSelectable > 0 ? SFollowCameraSettings::eFCF_UserSelectable : SFollowCameraSettings::eFCF_None;
		mode.m_flags |= disableHeightAdj > 0 ? SFollowCameraSettings::eFCF_DisableHeightAdj : SFollowCameraSettings::eFCF_None;


		const string nameString = pCameraModeNode->GetAttribute("name");
		mode.m_crcSettingsName = CCrc32::ComputeLowercase(nameString.c_str());
#ifndef _RELEASE
		mode.m_settingsName = nameString;
#endif //_RELEASE
	}
}

void CLocalPlayerComponent::StartFreeFallDeath()
{
	m_bIsInFreeFallDeath = true;

	Vec3 currentLook = m_rPlayer.GetEntity()->GetWorldTM().GetColumn1();

	currentLook.z = tan_tpl(DEG2RAD(g_pGameCVars->pl_freeFallDeath_cameraAngle));
	currentLook.Normalize();

	m_freeFallLookTarget = currentLook;
}

void CLocalPlayerComponent::UpdateFreeFallDeath( const float frameTime )
{
	if(m_bIsInFreeFallDeath)
	{
		m_rPlayer.m_pPlayerRotation->SetForceLookAt(m_freeFallLookTarget);

		m_freeFallDeathFadeTimer += frameTime;

		if(m_rPlayer.m_stats.isRagDoll ||  (m_freeFallDeathFadeTimer > g_pGameCVars->pl_freeFallDeath_fadeTimer))
		{
			CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
			if (!pRecordingSystem || !pRecordingSystem->IsPlayingBack())	// Don't do the fade screen effect if in killcam
			{
				TriggerFadeToBlack(); 
			}

			m_rPlayer.m_pPlayerRotation->SetForceLookAt(ZERO);
			m_bIsInFreeFallDeath = false;
		}
	}
	else
	{
		m_freeFallDeathFadeTimer = 0.0f;
	}
}

void CLocalPlayerComponent::TriggerFadeToBlack()
{
	if(m_screenFadeEffectId == InvalidEffectId)
	{
		UpdateScreenFadeEffect();
	}

	SMFXRunTimeEffectParams effectParams;
	effectParams.pos = m_rPlayer.GetEntity()->GetWorldPos();
	//effectParams.soundSemantic = eSoundSemantic_HUD;

	gEnv->pGameFramework->GetIMaterialEffects()->ExecuteEffect(m_screenFadeEffectId, effectParams);
}

void CLocalPlayerComponent::UpdateFPBodyPartsVisibility()
{
	if (m_rPlayer.m_stats.isThirdPerson == false)
	{
		ICharacterInstance *pMainCharacter	= m_rPlayer.GetEntity()->GetCharacter(0);
		IAttachmentManager *pAttachmentManager		= pMainCharacter ? pMainCharacter->GetIAttachmentManager() : NULL;

		if (pAttachmentManager)
		{
			CRY_ASSERT(m_rPlayer.HasBoneID(BONE_CALF_L));
			CRY_ASSERT(m_rPlayer.HasBoneID(BONE_CALF_R));

			const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
			const Matrix34& playerWorldTM = m_rPlayer.GetEntity()->GetWorldTM();

			bool visible = true;
			if (m_rPlayer.m_linkStats.linkID == 0)
			{
				//volatile static float radius = 0.475f;
				const float radius = 0.16f;
				const Sphere calfSphereL(playerWorldTM.TransformPoint(m_rPlayer.GetBoneTransform(BONE_CALF_L).t), radius);
				const Sphere calfSpehreR(playerWorldTM.TransformPoint(m_rPlayer.GetBoneTransform(BONE_CALF_R).t), radius);

				visible = camera.IsSphereVisible_F(calfSphereL) || camera.IsSphereVisible_F(calfSpehreR);
			}

			//const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
			//IRenderAuxText::Draw2dLabel(50.0f, 50.0f, 2.0f, white, false, visible ? "Rendering complete FP body" : "Rendering only arms");

			//Early out if no change
			if (m_fpCompleteBodyVisible == visible)
				return;

			//Hide/Unhide certain parts depending on visibility
			const int kNumParstToToggle = 2;
			const char *tooglePartsTable[kNumParstToToggle] = {"lower_body", "upper_body"};
			for (int i=0; i<kNumParstToToggle; i++)
			{
				IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(tooglePartsTable[i]);
				if (pAttachment)
				{
					pAttachment->HideAttachment(!visible);
					pAttachment->HideInShadow(1);
				}
			}

			m_fpCompleteBodyVisible = visible;
		}
	}
}

void CLocalPlayerComponent::UpdateScreenFadeEffect()
{
	IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();

	if(pMaterialEffects)
	{
		m_screenFadeEffectId = pMaterialEffects->GetEffectIdByName("cw2_player_fx", "c2mp_fallDeath_fadeOut");
	}
}

void CLocalPlayerComponent::ResetScreenFX()
{
	gEnv->p3DEngine->ResetPostEffects();
	if(g_pGame->GetScreenEffects())
		g_pGame->GetScreenEffects()->ResetAllBlendGroups(true);
	m_rPlayer.GetActorParams().viewFoVScale = 1.0f;

	if(m_screenFadeEffectId != InvalidEffectId)
	{
		gEnv->pGameFramework->GetIMaterialEffects()->StopEffect(m_screenFadeEffectId);
	}
}


//
//	Client Soundmood-related code
//

void CLocalPlayerComponent::SetClientSoundmood(CPlayer::EClientSoundmoods soundmood)
{
	if(soundmood != m_soundmood)
	{
		RemoveClientSoundmood(m_soundmood);
		AddClientSoundmood(soundmood);

		m_soundmood = soundmood;
	}
}

void CLocalPlayerComponent::AddClientSoundmood(CPlayer::EClientSoundmoods soundmood)
{
	/*if(soundmood > CPlayer::ESoundmood_Invalid && soundmood < CPlayer::ESoundmood_Last)
	{
		m_soundmoods[soundmood].m_enter.Play();
	}*/
}

void CLocalPlayerComponent::RemoveClientSoundmood(CPlayer::EClientSoundmoods soundmood)
{
	REINST("needs verification!");
	//if(soundmood > CPlayer::ESoundmood_Invalid && soundmood < CPlayer::ESoundmood_Last)
	//{
	//	//Stop any looping sound that might have been playing
	//	m_soundmoods[soundmood].m_enter.Stop();
	//	//intentionally play ( so it runs execute commands that include RemoveSoundMood )
	//	m_soundmoods[soundmood].m_exit.Play();
	//}
}

void CLocalPlayerComponent::InitSoundmoods()
{
	// Do not re-use moods for other systems here.. ie. menu muted moods, as triggering the moods from elsewhere will remove them even though your player state's unchanged
	m_soundmoods[CPlayer::ESoundmood_Alive].Empty();	//Clears other soundmoods when set
	const char* lowHealthEnter = gEnv->bMultiplayer ? "Player_LowHealth_Enter_MP" : "Player_LowHealth_Enter";
	const char* lowHealthExit = gEnv->bMultiplayer ? "Player_LowHealth_Exit_MP" : "Player_LowHealth_Exit";
	m_soundmoods[CPlayer::ESoundmood_LowHealth].Set(lowHealthEnter, lowHealthExit);
	m_soundmoods[CPlayer::ESoundmood_Dead].Set(gEnv->bMultiplayer ? "Player_Death_Enter_MP" : "Player_Death_Enter", "Player_Death_Exit");
	m_soundmoods[CPlayer::ESoundmood_Killcam].Set("Killcam_Enter", "Killcam_Exit");
	m_soundmoods[CPlayer::ESoundmood_KillcamSlow].Set("Killcam_SlowMo_Enter", "Killcam_SlowMo_Exit");
	m_soundmoods[CPlayer::ESoundmood_Spectating].Set("Spectator_Enter", "Spectator_Leave");	
	m_soundmoods[CPlayer::ESoundmood_PreGame].Set("Menu_Pregame_Enter", "Menu_Pregame_Leave");
	m_soundmoods[CPlayer::ESoundmood_PostGame].Set("Menu_Postgame_Enter", "Menu_Postgame_Leave");
	m_soundmoods[CPlayer::ESoundmood_EMPBlasted].Set("Player_EMPBlasted_Enter", "Player_EMPBlasted_Leave");
	m_soundmood = CPlayer::ESoundmood_Invalid;
}


//STAP and FP IK

//------------------------------------------------------------------------------------
// Adjust the aim dir before we pass it to the torsoAim pose modifier, this allows us
// to have the weapon deviate from the camera in certain circumstances
// Should only be called once per frame as it time-steps internal vars
//------------------------------------------------------------------------------------
void CLocalPlayerComponent::AdjustTorsoAimDir(float fFrameTime, Vec3 &aimDir)
{
	const f32 HALF_PI = gf_PI * 0.5f;
	float newElevLimit = HALF_PI;

	const f32 MIN_FLATTEN_LEVEL		= -0.3f;
	const f32 MAX_FLATTEN_LEVEL		= -0.1f;
	const f32 TARGET_FLATTEN_ELEV	= -0.2f;
	const f32 LIMIT_CHANGE_RATE		= HALF_PI;

	if (g_pGameCVars->pl_swimAlignArmsToSurface && m_rPlayer.IsSwimming() && (m_rPlayer.m_playerStateSwim_WaterTestProxy.GetRelativeWaterLevel() > MIN_FLATTEN_LEVEL))
	{
		newElevLimit = (m_rPlayer.m_playerStateSwim_WaterTestProxy.GetRelativeWaterLevel() - MIN_FLATTEN_LEVEL) / (MAX_FLATTEN_LEVEL - MIN_FLATTEN_LEVEL);
		newElevLimit = LERP(gf_PI * 0.5f, TARGET_FLATTEN_ELEV, clamp_tpl(newElevLimit, 0.0f, 1.0f));
	}

	float limitDelta = LIMIT_CHANGE_RATE * fFrameTime;
	float limitDiff	 = newElevLimit - m_stapElevLimit;
	float smoothedLimit = (float) __fsel(fabs_tpl(limitDiff) - limitDelta, m_stapElevLimit + (fsgnf(limitDiff) * limitDelta), newElevLimit);
	m_stapElevLimit = smoothedLimit;

	if (smoothedLimit < HALF_PI)
	{
		//--- Need to limit, convert to yaw & elev, limit & then convert back
		float yaw, elev;
		float xy = aimDir.GetLengthSquared2D();
		if (xy > 0.001f)
		{
			yaw = atan2_tpl(aimDir.y,aimDir.x);
			elev = asin_tpl(aimDir.z);
		}
		else
		{
			yaw = 0.f;
			elev = (float)__fsel(aimDir.z, +1.f, -1.f) * (gf_PI*0.5f);
		}

		elev = min(elev, smoothedLimit);

		float sinYaw, cosYaw;
		float sinElev, cosElev;

		sincos_tpl(yaw, &sinYaw, &cosYaw);
		sincos_tpl(elev, &sinElev, &cosElev);

		aimDir.x = cosYaw * cosElev;
		aimDir.y = sinYaw * cosElev;
		aimDir.z = sinElev;
	}
}

void CLocalPlayerComponent::Update( const float frameTime )
{
	const float health = m_rPlayer.m_health.GetHealth();

	UpdatePlayerLowHealthStatus( health );

	CPlayerProgression::GetInstance()->Update( &m_rPlayer, frameTime, health );
	
	UpdateFreeFallDeath( frameTime );

	if(gEnv->bMultiplayer)
	{
		if(g_pGameCVars->pl_autoPickupItemsWhenUseHeld)
		{
			UpdateItemUsage(frameTime);
		}
	}

	UpdateFPBodyPartsVisibility();
}

void CLocalPlayerComponent::UpdateFPIKTorso(float fFrameTime, IItem * pCurrentItem, const Vec3& cameraPosition)
{
	//Get const ref instead of doing a full copy
	SMovementState info;
	m_rPlayer.m_pMovementController->GetMovementState(info);
	const QuatT &cameraTran = m_rPlayer.GetCameraTran();
	if (m_rPlayer.m_torsoAimIK.GetBlendFactor() > 0.9f)
	{
		m_lastSTAPCameraDelta = m_rPlayer.m_torsoAimIK.GetLastEffectorTransform().GetInverted() * cameraTran;
	}
	else
	{
		m_lastSTAPCameraDelta.SetIdentity();
	}

	ICharacterInstance* pCharacter = m_rPlayer.GetEntity()->GetCharacter(0);
	ICharacterInstance* pCharacterShadow = m_rPlayer.GetShadowCharacter();

	QuatT torsoOffset;
	GetFPTotalTorsoOffset(torsoOffset, pCurrentItem);

	if (pCharacter != 0)
	{
		Vec3 aimDir;

		if (m_rPlayer.m_params.mountedWeaponCameraTarget.IsZero() && (m_rPlayer.GetLinkedVehicle() == NULL))
		{
			aimDir = !m_rPlayer.m_pPlayerRotation->GetBaseQuat() * info.aimDirection;
		}
		else
		{
			aimDir = !m_rPlayer.GetEntity()->GetWorldRotation() * info.aimDirection;
		}

		AdjustTorsoAimDir(fFrameTime, aimDir);

		const bool needsPositionAdjust = !m_rPlayer.IsSliding();

		CIKTorsoAim_Helper::SIKTorsoParams IKParams(pCharacter, pCharacterShadow, aimDir, torsoOffset, cameraPosition, m_rPlayer.GetBoneID(BONE_CAMERA), m_rPlayer.GetBoneID(BONE_SPINE2), m_rPlayer.GetBoneID(BONE_SPINE), ShouldUpdateTranslationPinning(), needsPositionAdjust);
		m_rPlayer.m_torsoAimIK.Update(IKParams);
	}

#ifndef _RELEASE
	if (g_pGameCVars->pl_debug_view != 0)
	{
		CryWatch("CPlayer:UpdateFPIKTorso: RawCamera Pos(%f, %f, %f) Rot(%f, %f, %f, %f)", cameraTran.t.x, cameraTran.t.x, cameraTran.t.x, cameraTran.q.v.x, cameraTran.q.v.y, cameraTran.q.v.z, cameraTran.q.w );
		CryWatch("CPlayer:UpdateFPIKTorso: MountedTarget (%f, %f, %f)", m_rPlayer.m_params.mountedWeaponCameraTarget.x, m_rPlayer.m_params.mountedWeaponCameraTarget.y, m_rPlayer.m_params.mountedWeaponCameraTarget.z );
		CryWatch("CPlayer:UpdateFPIKTorso: CamPos(%f, %f, %f) Dir(%f, %f, %f)", cameraPosition.x, cameraPosition.y, cameraPosition.z, info.aimDirection.x, info.aimDirection.y, info.aimDirection.z);
		CryWatch("CPlayer:UpdateFPIKTorso: Anim Pos(%f, %f, %f) Rot(%f, %f, %f, %f)", m_lastSTAPCameraDelta.t.x, m_lastSTAPCameraDelta.t.y, m_lastSTAPCameraDelta.t.z, m_lastSTAPCameraDelta.q.v.x, m_lastSTAPCameraDelta.q.v.y, m_lastSTAPCameraDelta.q.v.z, m_lastSTAPCameraDelta.q.w);
		if (g_pGameCVars->pl_debug_view >= 2)
		{
			CryLog("CPlayer:UpdateFPIKTorso: CamPos(%f, %f, %f) Dir(%f, %f, %f)", cameraPosition.x, cameraPosition.y, cameraPosition.z, info.aimDirection.x, info.aimDirection.y, info.aimDirection.z);
			CryLog("CPlayer:UpdateFPIKTorso: EyeOffset(%f, %f, %f)", m_rPlayer.m_eyeOffset.x, m_rPlayer.m_eyeOffset.y, m_rPlayer.m_eyeOffset.z);
		}

		const QuatT cameraWorld = QuatT(m_rPlayer.GetEntity()->GetWorldTM()) * cameraTran;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(cameraWorld.t, 0.1f, ColorB(0, 0, 255));
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(cameraWorld.t, ColorB(255, 0, 0), cameraWorld.t + cameraWorld.q.GetColumn1(), ColorB(255, 0, 0), 3.0f);
	}

	if(g_pGameCVars->p_collclassdebug == 1)
	{
		const QuatT cameraQuatT = QuatT(m_rPlayer.GetEntity()->GetWorldTM()) * cameraTran;
		ray_hit hit;
		if(gEnv->pPhysicalWorld->RayWorldIntersection(cameraQuatT.t+(cameraQuatT.q.GetColumn1()*1.5f), cameraQuatT.q.GetColumn1()*200.f, ent_all, rwi_colltype_any(geom_collides)|rwi_force_pierceable_noncoll|rwi_stop_at_pierceable, &hit, 1 ))
		{
			if(hit.pCollider)
			{
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(hit.pt, 0.1f, ColorB(0, 0, 255));
				g_pGame->GetGamePhysicsSettings()->Debug(*hit.pCollider, true);
			}
		}
	}
#endif
}

void CLocalPlayerComponent::GetFPTotalTorsoOffset(QuatT &offset, IItem * pCurrentItem) const
{
	CItem *pItem = (CItem *)pCurrentItem;
	if (pItem)
	{
		pItem->GetFPOffset(offset);
	}
	else
	{
		offset.SetIdentity();
	}
}

bool CLocalPlayerComponent::ShouldUpdateTranslationPinning() const
{
	return !m_rPlayer.IsSliding() && !m_rPlayer.IsExitingSlide()
		&& (GetGameConstCVar(g_translationPinningEnable) != 0) 
		&& !m_rPlayer.GetActorStats()->bDisableTranslationPinning;
}


void CLocalPlayerComponent::UpdateSwimming(float fFrameTime, float breathingInterval )
{
	if (gEnv->IsEditor() && gEnv->IsEditing())
		return;

	const CWeapon* pWeapon = m_rPlayer.GetWeapon( m_rPlayer.GetCurrentItemId() );

	if( !pWeapon || pWeapon->GetEntity()->GetClass() != CItem::sNoWeaponClass )
	{
		//Select underwater weapon here
		m_rPlayer.SelectItemByName( "NoWeapon", true, true );
	}


	m_timeForBreath -= fFrameTime;

	const bool isHeadUnderWater = m_rPlayer.IsHeadUnderWater();

	if( m_timeForBreath<=0 && isHeadUnderWater )
	{
		PlayBreathingUnderwater(breathingInterval);
	}
	
	CPlayerStateSwim::UpdateAudio(m_rPlayer);
}


void CLocalPlayerComponent::PlayBreathingUnderwater(float breathingInterval)
{
	m_rPlayer.PlaySound( CPlayer::ESound_Breathing_UnderWater, true, "speed", m_rPlayer.m_stats.speedFlat );
	m_timeForBreath = breathingInterval;
}

void CLocalPlayerComponent::UpdateStumble( const float frameTime )
{
	IEntity * pEntity = m_rPlayer.GetEntity();

	IPhysicalEntity * pPhysEntity = pEntity->GetPhysics();

	if ( pPhysEntity )
	{
		pe_status_dynamics dynamics;
		pPhysEntity->GetStatus( &dynamics );
		if ( m_playerStumble.Update( frameTime, dynamics ) )
		{
			pe_action_impulse ai; 
			ai.impulse = m_playerStumble.GetCurrentActionImpulse();
			pPhysEntity->Action( &ai );
		}
	}
}


void CLocalPlayerComponent::UpdateItemUsage( const float frameTime )
{
	m_playerEntityInteraction.Update( &m_rPlayer, frameTime );
}

void CLocalPlayerComponent::UpdatePlayerLowHealthStatus( const float oldHealth )
{
	const float healthThrLow = g_pGameCVars->g_playerLowHealthThreshold;
	const float healthThrMid = g_pGameCVars->g_playerMidHealthThreshold;

	const float maxHealth = (float)m_rPlayer.GetMaxHealth();
	const float minHealth = 0; 
	const float currentHealth = m_rPlayer.m_health.GetHealth();
	const bool isDead = m_rPlayer.m_health.IsDead();
	
	CRY_ASSERT( maxHealth > minHealth );

	//Extra 'mid-low' health sound hint
	if ((currentHealth <= healthThrMid) && (oldHealth > healthThrMid))
	{
		m_rPlayer.PlaySound(CPlayer::ESound_EnterMidHealth);
		m_playedMidHealthSound = true;
	}
	else if ((oldHealth <= healthThrMid) && (currentHealth > healthThrMid) && m_playedMidHealthSound)
	{
		m_rPlayer.PlaySound(CPlayer::ESound_EnterMidHealth, false);
		m_rPlayer.PlaySound(CPlayer::ESound_ExitMidHealth);
		m_playedMidHealthSound = false;
	}

	if((currentHealth <= healthThrLow) && (oldHealth > healthThrLow))
	{
		if(!isDead)
		{
			m_rPlayer.SetClientSoundmood(CPlayer::ESoundmood_LowHealth);
		}

		IAIObject* pAI = m_rPlayer.GetEntity()->GetAI();
		if (pAI)
		{
			float mercyTimeScale = 1.0f;
			
			SAIEVENT aiEvent;
			aiEvent.fThreat = mercyTimeScale;
			pAI->Event(AIEVENT_LOWHEALTH, &aiEvent);

			CGameRules* pGameRules = g_pGame->GetGameRules();
			if(pGameRules != NULL)
			{
				IGameRulesDamageHandlingModule* pDamageHandling = pGameRules->GetDamageHandlingModule();
				if(pDamageHandling != NULL)
				{
					pDamageHandling->OnGameEvent(IGameRulesDamageHandlingModule::eGameEvent_LocalPlayerEnteredMercyTime);
				}
			}
		}
	}
	else if((currentHealth > healthThrLow) && (oldHealth <= healthThrLow))
	{
		m_rPlayer.SetClientSoundmood(CPlayer::ESoundmood_Alive);
	}

	const float k_minDamageForHit = 10.0f;	//this is partly to deal with small differences in health over the network
	if(!isDead && (currentHealth < oldHealth - k_minDamageForHit))
	{
		// Update ATL-Switch maybe?
	}

	const float defiantSkillKillLowHealth = maxHealth * g_pGameCVars->g_defiant_lowHealthFraction;

	if(currentHealth > defiantSkillKillLowHealth)
	{
		m_timeEnteredLowHealth = 0.f;
	}
	else if(m_timeEnteredLowHealth <= 0.f)
	{
		m_timeEnteredLowHealth = gEnv->pTimer->GetFrameStartTime();
	}
}

const SFollowCameraSettings& CLocalPlayerComponent::GetCurrentFollowCameraSettings() const
{
#ifndef _RELEASE
	if(g_spectate_follow_debug_enable > 0)
	{
		m_followCameraDebugSettings.m_targetOffset = Vec3(g_spectate_follow_debug_targetOffset_x, g_spectate_follow_debug_targetOffset_y, g_spectate_follow_debug_targetOffset_z);
		m_followCameraDebugSettings.m_viewOffset = Vec3(g_spectate_follow_debug_viewOffset_x, g_spectate_follow_debug_viewOffset_y, g_spectate_follow_debug_viewOffset_z);
		m_followCameraDebugSettings.m_offsetSpeeds = Vec3(g_spectate_follow_debug_offsetspeed_x, g_spectate_follow_debug_offsetspeed_y, g_spectate_follow_debug_offsetspeed_z);
		m_followCameraDebugSettings.m_desiredDistance = g_spectate_follow_debug_desiredDistance;
		m_followCameraDebugSettings.m_cameraHeightOffset = g_spectate_follow_debug_cameraHeightOffset;
		m_followCameraDebugSettings.m_cameraYaw = DEG2RAD(g_spectate_follow_debug_cameraYaw);
		m_followCameraDebugSettings.m_cameraSpeed = g_spectate_follow_debug_cameraSpeed;
		m_followCameraDebugSettings.m_cameraFov = DEG2RAD(g_spectate_follow_debug_cameraFov);

		m_followCameraDebugSettings.m_flags = SFollowCameraSettings::eFCF_None;
		m_followCameraDebugSettings.m_flags |= g_spectate_follow_debug_allowOrbitYaw > 0 ? SFollowCameraSettings::eFCF_AllowOrbitYaw : SFollowCameraSettings::eFCF_None;
		m_followCameraDebugSettings.m_flags |= g_spectate_follow_debug_allowOrbitPitch > 0 ? SFollowCameraSettings::eFCF_AllowOrbitPitch : SFollowCameraSettings::eFCF_None;
		m_followCameraDebugSettings.m_flags |= g_spectate_follow_debug_useEyeDir > 0 ? SFollowCameraSettings::eFCF_UseEyeDir : SFollowCameraSettings::eFCF_None;
		m_followCameraDebugSettings.m_flags |= g_spectate_follow_debug_userSelectable > 0 ? SFollowCameraSettings::eFCF_UserSelectable : SFollowCameraSettings::eFCF_None;

		return m_followCameraDebugSettings;
	}
	else
	{
		if(g_spectate_follow_debug_setDebugSettingsToCurrentViewSettings)
		{
			g_spectate_follow_debug_setDebugSettingsToCurrentViewSettings = 0;
			const SFollowCameraSettings& settings = m_followCameraSettings[m_currentFollowCameraSettingsIndex];
			g_spectate_follow_debug_targetOffset_x = settings.m_targetOffset.x;
			g_spectate_follow_debug_targetOffset_y = settings.m_targetOffset.y;
			g_spectate_follow_debug_targetOffset_z = settings.m_targetOffset.z;
			g_spectate_follow_debug_viewOffset_x = settings.m_viewOffset.x;
			g_spectate_follow_debug_viewOffset_y = settings.m_viewOffset.y;
			g_spectate_follow_debug_viewOffset_z = settings.m_viewOffset.z;
			g_spectate_follow_debug_offsetspeed_x = settings.m_offsetSpeeds.x;
			g_spectate_follow_debug_offsetspeed_y = settings.m_offsetSpeeds.y;
			g_spectate_follow_debug_offsetspeed_z = settings.m_offsetSpeeds.z;
			g_spectate_follow_debug_desiredDistance = settings.m_desiredDistance;
			g_spectate_follow_debug_cameraHeightOffset = settings.m_cameraHeightOffset;
			g_spectate_follow_debug_cameraYaw = settings.m_cameraYaw;
			g_spectate_follow_debug_cameraSpeed = settings.m_cameraSpeed;
			g_spectate_follow_debug_cameraFov = settings.m_cameraFov;
			g_spectate_follow_debug_allowOrbitYaw = (settings.m_flags & SFollowCameraSettings::eFCF_AllowOrbitYaw) > 0 ? 1 : 0;
			g_spectate_follow_debug_allowOrbitPitch = (settings.m_flags & SFollowCameraSettings::eFCF_AllowOrbitPitch) > 0 ? 1 : 0;
			g_spectate_follow_debug_useEyeDir = (settings.m_flags & SFollowCameraSettings::eFCF_UseEyeDir) > 0 ? 1 : 0;
		}
	}
#endif //_RELEASE

	CRY_ASSERT(m_currentFollowCameraSettingsIndex >= 0 && m_currentFollowCameraSettingsIndex < (int8)m_followCameraSettings.size());

	return m_followCameraSettings[m_currentFollowCameraSettingsIndex];
}

void CLocalPlayerComponent::ChangeCurrentFollowCameraSettings( bool increment )
{
	const int initial = m_currentFollowCameraSettingsIndex;
	bool valid = false;
	do
	{
		if(increment && ++m_currentFollowCameraSettingsIndex == m_followCameraSettings.size())
		{
			m_currentFollowCameraSettingsIndex = 0;
		}
		else if(!increment && --m_currentFollowCameraSettingsIndex < 0)
		{
			m_currentFollowCameraSettingsIndex = m_followCameraSettings.size() - 1;
		}

		const SFollowCameraSettings& settings = m_followCameraSettings[m_currentFollowCameraSettingsIndex];
		valid = (m_currentFollowCameraSettingsIndex==initial) || (settings.m_flags & SFollowCameraSettings::eFCF_UserSelectable)!=0;

	} while( !valid );
}

bool CLocalPlayerComponent::SetCurrentFollowCameraSettings( uint32 crcName )
{
	const int total = m_followCameraSettings.size();
	for( int i=0; i<total; i++ )
	{
		const SFollowCameraSettings& settings = m_followCameraSettings[i];
		if( settings.m_crcSettingsName == crcName )
		{
			m_currentFollowCameraSettingsIndex = i;
			return true;
		}
	}
	return false;
}

CPlayer::EClientSoundmoods CLocalPlayerComponent::FindClientSoundmoodBestFit() const
{
	const CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
	if(pRecordingSystem && pRecordingSystem->IsPlayingBack())
	{
		return pRecordingSystem->IsInBulletTime() ? CPlayer::ESoundmood_KillcamSlow : CPlayer::ESoundmood_Killcam;
	}

	CGameRules* pGameRules = g_pGame->GetGameRules();
	const IGameRulesStateModule *pStateModule = pGameRules ? pGameRules->GetStateModule() : NULL;
	const IGameRulesStateModule::EGR_GameState gameState = pStateModule ? pStateModule->GetGameState() : IGameRulesStateModule::EGRS_InGame;
	if(gameState == IGameRulesStateModule::EGRS_PreGame)
	{
		return CPlayer::ESoundmood_PreGame;
	}
	else if(gameState == IGameRulesStateModule::EGRS_PostGame)
	{
		return CPlayer::ESoundmood_PostGame;
	}
	else if(m_rPlayer.GetSpectatorMode() != CActor::eASM_None)
	{
		return CPlayer::ESoundmood_Spectating;
	}
	else if(m_rPlayer.IsDead())
	{
		return CPlayer::ESoundmood_Dead;
	}
	else if(m_rPlayer.GetHealth() < g_pGameCVars->g_playerLowHealthThreshold)
	{
		return CPlayer::ESoundmood_LowHealth;
	}

	return CPlayer::ESoundmood_Alive;
}

void CLocalPlayerComponent::OnClearInventory()
{
	if( CEquipmentLoadout* pLoadout = g_pGame->GetEquipmentLoadout() )
	{
		pLoadout->SetCurrentAttachmentsToUnlocked();
	}
}
