// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	VehicleSystem CVars
   -------------------------------------------------------------------------
   History:
   - 02:01:2007  12:47 : Created by MichaelR

*************************************************************************/

#include "StdAfx.h"
#include "VehicleCVars.h"
#include "VehicleSystem.h"
#include "Vehicle.h"

CVehicleCVars* CVehicleCVars::s_pThis = 0;

#if ENABLE_VEHICLE_DEBUG
void OnDebugDrawVarChanged(ICVar* pVar);
#endif

void OnDriverControlledGunsChanged(ICVar* pVar);
void CmdExitPlayer(IConsoleCmdArgs* pArgs);

//------------------------------------------------------------------------
CVehicleCVars::CVehicleCVars()
{
	assert(s_pThis == 0);
	s_pThis = this;

	IConsole* pConsole = gEnv->pConsole;
	assert(pConsole);

#if ENABLE_VEHICLE_DEBUG
	// debug draw
	REGISTER_STRING("v_debugVehicle", "", 0, "Vehicle entity name to use for debugging output");
	REGISTER_CVAR_CB(v_draw_components, 0, VF_DUMPTODISK, "Enables/disables display of components and their damage count", OnDebugDrawVarChanged);
	REGISTER_CVAR_CB(v_draw_helpers, 0, 0, "Enables/disables display of vehicle helpers", OnDebugDrawVarChanged);
	REGISTER_CVAR_CB(v_draw_seats, 0, 0, "Enables/disables display of seat positions", OnDebugDrawVarChanged);
	REGISTER_CVAR(v_draw_tm, 0, 0, "Enables/disables drawing of local part matrices");
	REGISTER_CVAR(v_draw_passengers, 0, VF_CHEAT, "draw passenger TMs set by VehicleSeat");
	REGISTER_CVAR(v_debugdraw, 0, VF_DUMPTODISK,
	              "Displays vehicle status info on HUD\n"
	              "Values:\n"
	              "1:  common stuff\n"
	              "2:  vehicle particles\n"
	              "3:  parts\n"
	              "4:  views\n"
	              "6:  parts + partIds\n"
	              "7:  parts + transformations and bboxes\n"
	              "8:  component damage\n"
	              "10: vehicle editor");
	REGISTER_CVAR(v_debug_mem, 0, 0, "display memory statistic for vehicles");

	REGISTER_CVAR(v_debugViewDetach, 0, VF_CHEAT | VF_DUMPTODISK, "Freeze vehicle camera position, (1) fixed rotation, (2) track the vehicle");
	REGISTER_CVAR(v_debugViewAbove, 0, VF_CHEAT | VF_DUMPTODISK, "Debug camera, looking down on vehicle");
	REGISTER_CVAR(v_debugViewAboveH, 10.f, VF_CHEAT | VF_DUMPTODISK, "Debug camera, looking down on vehicle, height ");
	REGISTER_CVAR(v_debugCollisionDamage, 0, VF_CHEAT, "Enable debug output for vehicle collisions");

	REGISTER_CVAR(v_debug_flip_over, 0, VF_CHEAT, "flip over the current vehicle that the player is driving");
	REGISTER_CVAR(v_debug_reorient, 0, VF_CHEAT, "reset the orientation of the vehicle that the player is driving so that it is upright again");

	REGISTER_COMMAND("v_dump_classes", CVehicleSystem::DumpClasses, 0, "Outputs a list of vehicle classes in use");
#endif

	// dev vars
	REGISTER_CVAR(v_lights, 2, 0, "Controls vehicle lights.\n"
	                              "0: disable all lights\n"
	                              "1: enable lights only for local player\n"
	                              "2: enable all lights");
	REGISTER_CVAR(v_lights_enable_always, 0, VF_CHEAT, "Vehicle lights are always on (debugging)");
	REGISTER_CVAR(v_autoDisable, 1, VF_CHEAT, "Enables/disables vehicle autodisabling");
	REGISTER_CVAR(v_set_passenger_tm, 1, VF_CHEAT, "enable/disable passenger entity tm update");
	REGISTER_CVAR(v_disable_hull, 0, 0, "Disable hull proxies");
	REGISTER_CVAR(v_staticTreadDeform, 1, 0, "enables physics-based deforming of threads even for *non-moving* vehicles");
	REGISTER_CVAR(v_show_all, 0, VF_CHEAT, "");
	REGISTER_CVAR(v_transitionAnimations, 1, VF_CHEAT, "Enables enter/exit transition animations for vehicles");
	REGISTER_CVAR(v_playerTransitions, 1, VF_CHEAT, "Enables enter/exit transition animations for vehicles");
	REGISTER_CVAR(v_ragdollPassengers, 0, VF_CHEAT, "Forces vehicle passenger to detach and ragdoll when they die inside of a vehicle");
	REGISTER_CVAR(v_goliathMode, 0, VF_CHEAT, "Makes all vehicles invincible");
	REGISTER_CVAR(v_enableMannequin, 1, VF_CHEAT, "Enables enter/exit transition animations for vehicles");
	REGISTER_CVAR(v_serverControlled, 0, VF_CHEAT | VF_REQUIRE_LEVEL_RELOAD, "Makes the vehicles server authoritative, clients only send inputs to the server who then drives the vehicles");
	REGISTER_CVAR(v_clientPredict, 1, VF_CHEAT, "Enable client-side prediction on vehicle movement");
	REGISTER_CVAR(v_clientPredictSmoothing, 1, VF_CHEAT, "Enable client-side prediction smoothing on vehicle movement");
	REGISTER_CVAR(v_testClientPredict, 0, VF_CHEAT, "Test client-side prediction on a listen server with no clients, the value represents the number of frames to rewind and replay");
	REGISTER_CVAR(v_clientPredictSmoothingConst, 8.0f, VF_CHEAT, "The amount of smoothing to use, lower values result in smoothing looking movement but more lag behind the true position");
	REGISTER_CVAR(v_clientPredictAdditionalTime, 0.033f, VF_CHEAT, "Additional time offset to calibrate client prediction, will be added to ping and p_net_interp cvar");
	REGISTER_CVAR(v_clientPredictMaxTime, 0.5f, VF_CHEAT, "The maximum time the client can predict ahead of the server position (should be roughly equal to the maximum ping we expect in the real world)");

	// for tweaking
	REGISTER_CVAR(v_slipSlopeFront, 0.f, VF_CHEAT, "coefficient for slip friction slope calculation (front wheels)");
	REGISTER_CVAR(v_slipSlopeRear, 0.f, VF_CHEAT, "coefficient for slip friction slope calculation (rear wheels)");
	REGISTER_CVAR(v_slipFrictionModFront, 0.f, VF_CHEAT, "if non-zero, used as slip friction modifier (front wheels)");
	REGISTER_CVAR(v_slipFrictionModRear, 0.f, VF_CHEAT, "if non-zero, used as slip friction modifier (rear wheels)");

	REGISTER_CVAR(v_FlippedExplosionTimeToExplode, 20.f, VF_CHEAT, "The number of seconds to wait after a vehicle is flipped to attempt exploding");
	REGISTER_CVAR(v_FlippedExplosionPlayerMinDistance, 25.f, VF_CHEAT, "If a player is within this distance then don't explode yet");
	REGISTER_CVAR(v_FlippedExplosionRetryTimeMS, 10000, VF_CHEAT, "If a nearby player blocked explosion then try again after this time period");

	REGISTER_COMMAND("v_reload_system", "VehicleSystem.ReloadVehicleSystem()", 0, "Reloads VehicleSystem script");
	REGISTER_COMMAND("v_exit_player", CmdExitPlayer, VF_CHEAT, "Makes the local player exit his current vehicle.");

	REGISTER_CVAR(v_disableEntry, 0, 0, "Don't allow players to enter vehicles");

	REGISTER_CVAR(v_vehicle_quality, 4, 0, "Geometry/Physics quality (1-lowspec, 4-highspec)");
	REGISTER_CVAR_CB(v_driverControlledMountedGuns, 1, VF_CHEAT, "Specifies if the driver can control the vehicles mounted gun when driving without gunner.", OnDriverControlledGunsChanged);
	REGISTER_CVAR(v_independentMountedGuns, 1, 0, "Whether mounted gunners operate their turret independently from the parent vehicle");

	REGISTER_CVAR(v_debugSuspensionIK, 0, 0, "Debug draw the suspension ik");
}

//------------------------------------------------------------------------
CVehicleCVars::~CVehicleCVars()
{
	assert(s_pThis != 0);
	s_pThis = 0;

	IConsole* pConsole = gEnv->pConsole;

	pConsole->RemoveCommand("v_tpvDist");
	pConsole->RemoveCommand("v_tpvHeight");
	pConsole->RemoveCommand("v_reload_system");
	pConsole->RemoveCommand("v_exit_player");

#if ENABLE_VEHICLE_DEBUG
	pConsole->UnregisterVariable("v_debugVehicle", true);
	pConsole->UnregisterVariable("v_draw_components", true);
	pConsole->UnregisterVariable("v_draw_helpers", true);
	pConsole->UnregisterVariable("v_draw_seats", true);
	pConsole->UnregisterVariable("v_draw_tm", true);
	pConsole->UnregisterVariable("v_draw_passengers", true);
	pConsole->UnregisterVariable("v_debugdraw", true);
	pConsole->UnregisterVariable("v_debug_mem", true);
#endif

	pConsole->UnregisterVariable("v_lights", true);
	pConsole->UnregisterVariable("v_lights_enable_always", true);
	pConsole->UnregisterVariable("v_autoDisable", true);
	pConsole->UnregisterVariable("v_set_passenger_tm", true);
	pConsole->UnregisterVariable("v_disable_hull", true);
	pConsole->UnregisterVariable("v_staticTreadDeform", true);
	pConsole->UnregisterVariable("v_transitionAnimations", true);
	pConsole->UnregisterVariable("v_ragdollPassengers", true);
	pConsole->UnregisterVariable("v_goliathMode", true);

	pConsole->UnregisterVariable("v_slipSlopeFront", true);
	pConsole->UnregisterVariable("v_slipSlopeRear", true);
	pConsole->UnregisterVariable("v_slipFrictionModFront", true);
	pConsole->UnregisterVariable("v_slipFrictionModRear", true);

	pConsole->UnregisterVariable("v_vehicle_quality", true);
	pConsole->UnregisterVariable("v_driverControlledMountedGuns", true);

	pConsole->UnregisterVariable("v_debugSuspensionIK", true);
}

#if ENABLE_VEHICLE_DEBUG
//------------------------------------------------------------------------
void OnDebugDrawVarChanged(ICVar* pVar)
{
	if (pVar->GetIVal())
	{
		IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();

		IVehicleIteratorPtr pIter = pVehicleSystem->CreateVehicleIterator();
		while (IVehicle* pVehicle = pIter->Next())
		{
			pVehicle->NeedsUpdate();
		}
	}
}
#endif

//------------------------------------------------------------------------
void OnDriverControlledGunsChanged(ICVar* pVar)
{
	if (gEnv->bMultiplayer)
		return;

	if (IActor* pActor = CCryAction::GetCryAction()->GetClientActor())
	{
		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
		{
			SVehicleEventParams params;
			params.bParam = (pVar->GetIVal() != 0);

			pVehicle->BroadcastVehicleEvent(eVE_ToggleDriverControlledGuns, params);
		}
	}
}

//------------------------------------------------------------------------
void CmdExitPlayer(IConsoleCmdArgs* pArgs)
{
	if (IActor* pActor = CCryAction::GetCryAction()->GetClientActor())
	{
		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
			pVehicle->ExitVehicleAtPosition(pActor->GetEntityId(), ZERO);
	}
}
