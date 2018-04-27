// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
	
	-------------------------------------------------------------------------
	History:
	- 07:09:2009  : Created by Ben Johnson

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesMPActorAction.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Actor.h"
#include "Player.h"
#include "GameActions.h"
#include "IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/GameRulesMPSpawning.h"
#include "UI/UIManager.h"
#include "RecordingSystem.h"

CGameRulesMPActorAction::CGameRulesMPActorAction()
{
}

CGameRulesMPActorAction::~CGameRulesMPActorAction()
{
}

void CGameRulesMPActorAction::Init( XmlNodeRef xml )
{
}

void CGameRulesMPActorAction::PostInit()
{
}

void CGameRulesMPActorAction::OnActorAction(IActor *pActor, const ActionId& actionId, int activationMode, float value)
{
	CActor *pActorImpl = static_cast<CActor *>(pActor);
	if (pActorImpl)
	{
		EntityId  pid = pActor->GetEntityId();  // player id

		CGameRules *pGameRules = g_pGame->GetGameRules();
		IGameRulesSpectatorModule *specmod = pGameRules->GetSpectatorModule();

		if (!specmod || (pActorImpl->GetSpectatorMode() <= 0))
		{
			// Not in spectator mode

			if (pActorImpl->IsDead())
			{
				// if dead 

				CRecordingSystem *crs = g_pGame->GetRecordingSystem();

				if (crs != NULL && crs->IsPlayingBack())
				{
					// Recording system playing back 
					if (actionId == g_pGame->Actions().spectate_gen_skipdeathcam && g_pGameCVars->kc_canSkip )
						crs->StopPlayback();
				}
				else if ((actionId == g_pGame->Actions().spectate_gen_spawn || actionId == g_pGame->Actions().hud_mouseclick) && activationMode == eAAM_OnPress && pActorImpl->GetSpectatorState() != CActor::eASS_SpectatorMode)
				{
					// Revive requested.
					//	This may happen immediately or not at all.

					if (IGameRulesSpawningModule* pSpawningModule=pGameRules->GetSpawningModule())
					{
						IGameRulesStateModule*  stateModule = pGameRules->GetStateModule();
						if (!stateModule || (stateModule->GetGameState() != IGameRulesStateModule::EGRS_PostGame))
						{
							CryLog("CGameRulesMPActorAction::OnActorAction() Requesting revive");
							pSpawningModule->ClRequestRevive(pActor->GetEntityId());
						}
					}
				}
				else if ((specmod != NULL && specmod->CanChangeSpectatorMode(pid)) && (((actionId == g_pGame->Actions().spectate_gen_nextmode) || (actionId == g_pGame->Actions().spectate_gen_prevmode)) && (activationMode == eAAM_OnPress)))  
				{
					// get into spectate mode
					if (!crs || !crs->IsPlayingOrQueued())
					{
						specmod->ChangeSpectatorModeBestAvailable(pActor, false);
					}
				}
			}
		}
		else
		{
			// is spectating

			int  curspecmode = pActorImpl->GetSpectatorMode();
			int  curspecstate = pActorImpl->GetSpectatorState();

			const CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
			// actions general across almost all spectator modes
			if( (curspecmode == CActor::eASM_Killer && !g_pGameCVars->g_killercam_canSkip) || (pRecordingSystem && pRecordingSystem->IsPlayingBack()) )
			{
				// Can't change mode or respawn-request, when in KillerCam mode or watching Killcam.
			}
			else if ((actionId == g_pGame->Actions().spectate_gen_spawn) && (activationMode == eAAM_OnPress) && pActorImpl->GetSpectatorState() != CActor::eASS_SpectatorMode)
			{
				IGameRulesSpawningModule *pSpawningModule = pGameRules->GetSpawningModule();
				if (pSpawningModule)
				{
					IGameRulesStateModule*  stateModule = pGameRules->GetStateModule();
					if (!stateModule || (stateModule->GetGameState() != IGameRulesStateModule::EGRS_PostGame))
					{
						CryLog("CGameRulesMPActorAction::OnActorAction() Spectating, received spectate_gen_spawn action, requesting revive");
						pSpawningModule->ClRequestRevive(pActor->GetEntityId());
					}			
				}			
			}
			else if (((actionId == g_pGame->Actions().spectate_gen_nextmode) || (actionId == g_pGame->Actions().spectate_gen_prevmode)) && (activationMode == eAAM_OnPress))
			{
				CryLog("[tlh] > changemode button pressed");
				if (specmod->CanChangeSpectatorMode(pid))
				{
					CryLog("[tlh] > can change");
					int  mode;
					EntityId  othEntId;
					mode = specmod->GetNextMode(pid, ((actionId == g_pGame->Actions().spectate_gen_nextmode) ? 1 : -1), &othEntId);
					if (mode != curspecmode)
					{
						CryLog("[tlh] > changing to mode %d with othEnt %d",mode,othEntId);
						specmod->ChangeSpectatorMode(pActor, mode, othEntId, false);
					}
				}
			}
			else
			{
				// actions specific to individual spectator modes

				if (specmod->CanChangeSpectatorMode(pid))  // "CanChangeSpectatorMode?" is essentially "CanInteractWithSpectatorMode?" - ie. we don't want to be able to do any of this on the Join Game screen
				{
					if (curspecmode == CActor::eASM_Fixed)
					{
						int  changeCam = 0;

						if (((actionId == g_pGame->Actions().spectate_cctv_nextcam) || (actionId == g_pGame->Actions().spectate_cctv_prevcam)) && (activationMode == eAAM_OnPress))
						{
							changeCam = (actionId == g_pGame->Actions().spectate_cctv_nextcam ? 1 : -1);
						}
						else if (actionId == g_pGame->Actions().spectate_cctv_changecam_xi)
						{
							if (value >= 1.f)
							{
								changeCam = 1;
							}
							else if (value <= -1.f)
							{
								changeCam = -1;
							}
						}

						if (changeCam != 0)
						{
							EntityId  locationId;

							if (changeCam > 0)
							{
								locationId = specmod->GetNextSpectatorLocation(pActorImpl);
							}
							else
							{
								locationId = specmod->GetPrevSpectatorLocation(pActorImpl);
							}

							pActorImpl->SetSpectatorFixedLocation(locationId);
						}
					}
					else if (curspecmode == CActor::eASM_Free)
					{
						;  // none
					}
					else if (curspecmode == CActor::eASM_Follow)
					{
						int  change = 0;

						const CGameActions& actions = g_pGame->Actions();

						if (((actionId == actions.spectate_3rdperson_nextteammate) || (actionId == actions.spectate_3rdperson_prevteammate)) && (activationMode == eAAM_OnPress))
						{
							change = ((actionId == actions.spectate_3rdperson_nextteammate) ? 1 : -1);
						}
						else if (actionId == actions.spectate_3rdperson_changeteammate_xi)
						{
							if (value >= 1.f)
							{
								change = 1;
							}
							else if (value <= -1.f)
							{
								change = -1;
							}
						}
						else if(actionId == actions.xi_rotateyaw && pActorImpl->CanSpectatorOrbitYaw())
						{
							pActorImpl->SetSpectatorOrbitYawSpeed(fabs(value) > 0.2f ? value : 0.f, false);
						}
						else if(actionId == actions.rotateyaw && pActorImpl->CanSpectatorOrbitYaw())
						{
							pActorImpl->SetSpectatorOrbitYawSpeed(value * g_pGameCVars->g_spectate_follow_orbitMouseSpeedMultiplier, true);
						}
						else if(actionId == actions.xi_rotatepitch && pActorImpl->CanSpectatorOrbitPitch())
						{
							float val = fabs(value) > 0.2f ? value : 0.f;
							if(val && g_pGameCVars->cl_invertController)
							{
								val = -value;
							}
							pActorImpl->SetSpectatorOrbitPitchSpeed(val, false);
						}
						else if(actionId == actions.rotatepitch && pActorImpl->CanSpectatorOrbitPitch())
						{
							float val = fabs(value) > 0.2f ? value : 0.f;
							if(val && g_pGameCVars->cl_invertMouse)
							{
								val = -val;
							}
							pActorImpl->SetSpectatorOrbitPitchSpeed(val * g_pGameCVars->g_spectate_follow_orbitMouseSpeedMultiplier, true);
						}
						else if(actionId == actions.spectate_gen_nextcamera)
						{
							pActorImpl->ChangeCurrentFollowCameraSettings(true);
						}

						if (change != 0)
						{
							if (EntityId newTargetId=specmod->GetNextSpectatorTarget(pid, change))
							{
								pActorImpl->SetSpectatorTarget(newTargetId);
							}
						}
					}
					else if (curspecmode == CActor::eASM_Killer)
					{
						;  // none
					}
				}
			}
		}

		pGameRules->ActorActionInformOnAction(actionId, activationMode, value);
	}
}

