// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifndef STANDALONE_PHYSICS

#include "cvars.h"
#include <CrySystem/ConsoleRegistration.h>

namespace PhysicsCVars
{

	void Register(PhysicsVars* pVars)
	{
		assert(pVars);

		REGISTER_CVAR2("p_fly_mode", &pVars->bFlyMode, pVars->bFlyMode, VF_CHEAT,
			"Toggles fly mode.\n"
			"Usage: p_fly_mode [0/1]");
		REGISTER_CVAR2("p_collision_mode", &pVars->iCollisionMode, pVars->iCollisionMode, VF_CHEAT,
			"This variable is obsolete.");
		REGISTER_CVAR2("p_single_step_mode", &pVars->bSingleStepMode, pVars->bSingleStepMode, VF_CHEAT,
			"Toggles physics system 'single step' mode."
			"Usage: p_single_step_mode [0/1]\n"
			"Default is 0 (off). Set to 1 to switch physics system (except\n"
			"players) to single step mode. Each step must be explicitly\n"
			"requested with a 'p_do_step' instruction.");
		REGISTER_CVAR2("p_do_step", &pVars->bDoStep, pVars->bDoStep, VF_CHEAT,
			"Steps physics system forward when in single step mode.\n"
			"Usage: p_do_step 1\n"
			"Default is 0 (off). Each 'p_do_step 1' instruction allows\n"
			"the physics system to advance a single step.");
		REGISTER_CVAR2("p_fixed_timestep", &pVars->fixedTimestep, pVars->fixedTimestep, VF_CHEAT,
			"Toggles fixed time step mode."
			"Usage: p_fixed_timestep [0/1]\n"
			"Forces fixed time step when set to 1. When set to 0, the\n"
			"time step is variable, based on the frame rate.");
		REGISTER_CVAR2("p_draw_helpers_num", &pVars->iDrawHelpers, pVars->iDrawHelpers, VF_CHEAT,
			"Toggles display of various physical helpers. The value is a bitmask:\n"
			"bit 0  - show contact points\n"
			"bit 1  - show physical geometry\n"
			"bit 8  - show helpers for static objects\n"
			"bit 9  - show helpers for sleeping physicalized objects (rigid bodies, ragdolls)\n"
			"bit 10 - show helpers for active physicalized objects\n"
			"bit 11 - show helpers for players\n"
			"bit 12 - show helpers for independent entities (alive physical skeletons,particles,ropes)\n"
			"bits 16-31 - level of bounding volume trees to display (if 0, it just shows geometry)\n"
			"Examples: show static objects - 258, show active rigid bodies - 1026, show players - 2050");
		REGISTER_CVAR2("p_draw_helpers_opacity", &pVars->drawHelpersOpacity, pVars->drawHelpersOpacity, VF_CHEAT,
			"Opacity of physical helpers (see p_draw_helpers).\n"
			"Usage: p_draw_helpers_opacity [0..1]\n"
			"0.0 indicates full transparency, 1.0 full opacity (default) - e.g., 0.5 indicates half transparency.");
		REGISTER_CVAR2("p_check_out_of_bounds", &pVars->iOutOfBounds, pVars->iOutOfBounds, 0,
			"Check for physics entities outside world (terrain) grid:\n"
			"1 - Enable raycasts; 2 - Enable proximity checks; 3 - Both");
		REGISTER_CVAR2("p_max_contact_gap", &pVars->maxContactGap, pVars->maxContactGap, 0,
			"Sets the gap, enforced whenever possible, between\n"
			"contacting physical objects."
			"Usage: p_max_contact_gap 0.01\n"
			"This variable is used for internal tweaking only.");
		REGISTER_CVAR2("p_max_contact_gap_player", &pVars->maxContactGapPlayer, pVars->maxContactGapPlayer, 0,
			"Sets the safe contact gap for player collisions with\n"
			"the physical environment."
			"Usage: p_max_contact_gap_player 0.01\n"
			"This variable is used for internal tweaking only.");
		REGISTER_CVAR2("p_gravity_z", &pVars->gravity.z, pVars->gravity.z, 0, "");
		REGISTER_CVAR2("p_max_substeps", &pVars->nMaxSubsteps, pVars->nMaxSubsteps, 0,
			"Limits the number of substeps allowed in variable time step mode.\n"
			"Usage: p_max_substeps 5\n"
			"Objects that are not allowed to perform time steps\n"
			"beyond some value make several substeps.");
		REGISTER_CVAR2("p_prohibit_unprojection", &pVars->bProhibitUnprojection, pVars->bProhibitUnprojection, 0,
			"This variable is obsolete.");
		REGISTER_CVAR2("p_enforce_contacts", &pVars->bEnforceContacts, pVars->bEnforceContacts, 0,
			"This variable is obsolete.");
		REGISTER_CVAR2("p_damping_group_size", &pVars->nGroupDamping, pVars->nGroupDamping, 0,
			"Sets contacting objects group size\n"
			"before group damping is used."
			"Usage: p_damping_group_size 3\n"
			"Used for internal tweaking only.");
		REGISTER_CVAR2("p_group_damping", &pVars->groupDamping, pVars->groupDamping, 0,
			"Toggles damping for object groups.\n"
			"Usage: p_group_damping [0/1]\n"
			"Default is 1 (on). Used for internal tweaking only.");
		REGISTER_CVAR2("p_max_substeps_large_group", &pVars->nMaxSubstepsLargeGroup, pVars->nMaxSubstepsLargeGroup, 0,
			"Limits the number of substeps large groups of objects can make");
		REGISTER_CVAR2("p_num_bodies_large_group", &pVars->nBodiesLargeGroup, pVars->nBodiesLargeGroup, 0,
			"Group size to be used with p_max_substeps_large_group, in bodies");
		REGISTER_CVAR2("p_break_on_validation", &pVars->bBreakOnValidation, pVars->bBreakOnValidation, 0,
			"Toggles break on validation error.\n"
			"Usage: p_break_on_validation [0/1]\n"
			"Default is 0 (off). Issues CryDebugBreak() call in case of\n"
			"a physics parameter validation error.");
		REGISTER_CVAR2("p_time_granularity", &pVars->timeGranularity, pVars->timeGranularity, 0,
			"Sets physical time step granularity.\n"
			"Usage: p_time_granularity [0..0.1]\n"
			"Used for internal tweaking only.");
		REGISTER_CVAR2("p_list_active_objects", &pVars->bLogActiveObjects, pVars->bLogActiveObjects, VF_NULL, "1 - list normal objects, 2 - list independent objects, 3 - both");
		REGISTER_CVAR2("p_profile_entities", &pVars->bProfileEntities, pVars->bProfileEntities, 0,
			"Enables per-entity time step profiling");
		REGISTER_CVAR2("p_profile_functions", &pVars->bProfileFunx, pVars->bProfileFunx, 0,
			"Enables detailed profiling of physical environment-sampling functions");
		REGISTER_CVAR2("p_profile", &pVars->bProfileGroups, pVars->bProfileGroups, 0,
			"Enables group profiling of physical entities");
		REGISTER_CVAR2("p_GEB_max_cells", &pVars->nGEBMaxCells, pVars->nGEBMaxCells, 0,
			"Specifies the cell number threshold after which GetEntitiesInBox issues a warning");
		REGISTER_CVAR2("p_max_velocity", &pVars->maxVel, pVars->maxVel, 0,
			"Clamps physicalized objects' velocities to this value");
		REGISTER_CVAR2("p_max_player_velocity", &pVars->maxVelPlayers, pVars->maxVelPlayers, 0,
			"Clamps players' velocities to this value");
		REGISTER_CVAR2("p_max_bone_velocity", &pVars->maxVelBones, pVars->maxVelBones, 0,
			"Clamps character bone velocities estimated from animations");
		REGISTER_CVAR2("p_force_sync", &pVars->bForceSyncPhysics, 0, 0, "Forces main thread to wait on physics if not completed in time");

		REGISTER_CVAR2("p_max_MC_iters", &pVars->nMaxMCiters, pVars->nMaxMCiters, 0,
			"Specifies the maximum number of microcontact solver iterations *per contact*");
		REGISTER_CVAR2("p_min_MC_iters", &pVars->nMinMCiters, pVars->nMinMCiters, 0,
			"Specifies the minmum number of microcontact solver iterations *per contact set* (this has precedence over p_max_mc_iters)");
		REGISTER_CVAR2("p_accuracy_MC", &pVars->accuracyMC, pVars->accuracyMC, 0,
			"Desired accuracy of microcontact solver (velocity-related, m/s)");
		REGISTER_CVAR2("p_accuracy_LCPCG", &pVars->accuracyLCPCG, pVars->accuracyLCPCG, 0,
			"Desired accuracy of LCP CG solver (velocity-related, m/s)");
		REGISTER_CVAR2("p_max_contacts", &pVars->nMaxContacts, pVars->nMaxContacts, 0,
			"Maximum contact number, after which contact reduction mode is activated");
		REGISTER_CVAR2("p_max_plane_contacts", &pVars->nMaxPlaneContacts, pVars->nMaxPlaneContacts, 0,
			"Maximum number of contacts lying in one plane between two rigid bodies\n"
			"(the system tries to remove the least important contacts to get to this value)");
		REGISTER_CVAR2("p_max_plane_contacts_distress", &pVars->nMaxPlaneContactsDistress, pVars->nMaxPlaneContactsDistress, 0,
			"Same as p_max_plane_contacts, but is effective if total number of contacts is above p_max_contacts");
		REGISTER_CVAR2("p_max_LCPCG_subiters", &pVars->nMaxLCPCGsubiters, pVars->nMaxLCPCGsubiters, 0,
			"Limits the number of LCP CG solver inner iterations (should be of the order of the number of contacts)");
		REGISTER_CVAR2("p_max_LCPCG_subiters_final", &pVars->nMaxLCPCGsubitersFinal, pVars->nMaxLCPCGsubitersFinal, 0,
			"Limits the number of LCP CG solver inner iterations during the final iteration (should be of the order of the number of contacts)");
		REGISTER_CVAR2("p_max_LCPCG_microiters", &pVars->nMaxLCPCGmicroiters, pVars->nMaxLCPCGmicroiters, 0,
			"Limits the total number of per-contact iterations during one LCP CG iteration\n"
			"(number of microiters = number of subiters * number of contacts)");
		REGISTER_CVAR2("p_max_LCPCG_microiters_final", &pVars->nMaxLCPCGmicroitersFinal, pVars->nMaxLCPCGmicroitersFinal, 0,
			"Same as p_max_LCPCG_microiters, but for the final LCP CG iteration");
		REGISTER_CVAR2("p_max_LCPCG_iters", &pVars->nMaxLCPCGiters, pVars->nMaxLCPCGiters, 0,
			"Maximum number of LCP CG iterations");
		REGISTER_CVAR2("p_min_LCPCG_improvement", &pVars->minLCPCGimprovement, pVars->minLCPCGimprovement, 0,
			"Defines a required residual squared length improvement, in fractions of 1");
		REGISTER_CVAR2("p_max_LCPCG_fruitless_iters", &pVars->nMaxLCPCGFruitlessIters, pVars->nMaxLCPCGFruitlessIters, 0,
			"Maximum number of LCP CG iterations w/o improvement (defined by p_min_LCPCGimprovement)");
		REGISTER_CVAR2("p_accuracy_LCPCG_no_improvement", &pVars->accuracyLCPCGnoimprovement, pVars->accuracyLCPCGnoimprovement, 0,
			"Required LCP CG accuracy that allows to stop if there was no improvement after p_max_LCPCG_fruitless_iters");
		REGISTER_CVAR2("p_min_separation_speed", &pVars->minSeparationSpeed, pVars->minSeparationSpeed, 0,
			"Used a threshold in some places (namely, to determine when a particle\n"
			"goes to rest, and a sliding condition in microcontact solver)");
		REGISTER_CVAR2("p_use_distance_contacts", &pVars->bUseDistanceContacts, pVars->bUseDistanceContacts, 0,
			"Allows to use distance-based contacts (is forced off in multiplayer)");
		REGISTER_CVAR2("p_unproj_vel_scale", &pVars->unprojVelScale, pVars->unprojVelScale, 0,
			"Requested unprojection velocity is set equal to penetration depth multiplied by this number");
		REGISTER_CVAR2("p_max_unproj_vel", &pVars->maxUnprojVel, pVars->maxUnprojVel, 0,
			"Limits the maximum unprojection velocity request");
		REGISTER_CVAR2("p_penalty_scale", &pVars->penaltyScale, pVars->penaltyScale, 0,
			"Scales the penalty impulse for objects that use the simple solver");
		REGISTER_CVAR2("p_max_contact_gap_simple", &pVars->maxContactGapSimple, pVars->maxContactGapSimple, 0,
			"Specifies the maximum contact gap for objects that use the simple solver");
		REGISTER_CVAR2("p_skip_redundant_colldet", &pVars->bSkipRedundantColldet, pVars->bSkipRedundantColldet, 0,
			"Specifies whether to skip furher collision checks between two convex objects using the simple solver\n"
			"when they have enough contacts between them");
		REGISTER_CVAR2("p_limit_simple_solver_energy", &pVars->bLimitSimpleSolverEnergy, pVars->bLimitSimpleSolverEnergy, 0,
			"Specifies whether the energy added by the simple solver is limited (0 or 1)");
		REGISTER_CVAR2("p_max_world_step", &pVars->maxWorldStep, pVars->maxWorldStep, 0,
			"Specifies the maximum step physical world can make (larger steps will be truncated)");
		REGISTER_CVAR2("p_use_unproj_vel", &pVars->bCGUnprojVel, pVars->bCGUnprojVel, 0, "internal solver tweak");
		REGISTER_CVAR2("p_tick_breakable", &pVars->tickBreakable, pVars->tickBreakable, 0,
			"Sets the breakable objects structure update interval");
		REGISTER_CVAR2("p_log_lattice_tension", &pVars->bLogLatticeTension, pVars->bLogLatticeTension, 0,
			"If set, breakable objects will log tensions at the weakest spots");
		REGISTER_CVAR2("p_debug_joints", &pVars->bLogLatticeTension, pVars->bLogLatticeTension, 0,
			"If set, breakable objects will log tensions at the weakest spots");
		REGISTER_CVAR2("p_lattice_max_iters", &pVars->nMaxLatticeIters, pVars->nMaxLatticeIters, 0,
			"Limits the number of iterations of lattice tension solver");
		REGISTER_CVAR2("p_max_entity_cells", &pVars->nMaxEntityCells, pVars->nMaxEntityCells, 0,
			"Limits the number of entity grid cells an entity can occupy");
		REGISTER_CVAR2("p_max_MC_mass_ratio", &pVars->maxMCMassRatio, pVars->maxMCMassRatio, 0,
			"Maximum mass ratio between objects in an island that MC solver is considered safe to handle");
		REGISTER_CVAR2("p_max_MC_vel", &pVars->maxMCVel, pVars->maxMCVel, 0,
			"Maximum object velocity in an island that MC solver is considered safe to handle");
		REGISTER_CVAR2("p_max_LCPCG_contacts", &pVars->maxLCPCGContacts, pVars->maxLCPCGContacts, 0,
			"Maximum number of contacts that LCPCG solver is allowed to handle");
		REGISTER_CVAR2("p_mass_decay_prepasses", &pVars->massDecayPrepasses, pVars->massDecayPrepasses, 0,
			"Number of solver passes over all contacts before mass decay is considered");
		REGISTER_CVAR2("p_mass_decay_min_level", &pVars->massDecayMinLevel, pVars->massDecayMinLevel, 0,
			"Minimum required group 'depth' to trigger mass decay");
		REGISTER_CVAR2("p_mass_decay_max_level", &pVars->massDecayMaxLevel, pVars->massDecayMaxLevel, 0,
			"Maximum group 'depth', after which mass decay is no longer applied");
		REGISTER_CVAR2("p_mass_decay", &pVars->massDecay, pVars->massDecay, 0,
			"Per depth level mass decay in rigidbody solver (improves tall stack stability). <0 - stabilizing decay, 1 = no decay, >1 - negative decay (mass increase)");
		REGISTER_CVAR2("p_mass_decay_heavy_threshold", &pVars->massDecayHeavyThresh, pVars->massDecayHeavyThresh, 0,
			"Mass difference for assigning objects 0 depth level for mass decay purposes");
		REGISTER_CVAR2("p_approx_caps_len", &pVars->approxCapsLen, pVars->approxCapsLen, 0,
			"Breakable trees are approximated with capsules of this length (0 disables approximation)");
		REGISTER_CVAR2("p_max_approx_caps", &pVars->nMaxApproxCaps, pVars->nMaxApproxCaps, 0,
			"Maximum number of capsule approximation levels for breakable trees");
		REGISTER_CVAR2("p_players_can_break", &pVars->bPlayersCanBreak, pVars->bPlayersCanBreak, 0,
			"Whether living entities are allowed to break static objects with breakable joints");
		REGISTER_CVAR2("p_max_debris_mass", &pVars->massLimitDebris, 10.0f, 0,
			"Broken pieces with mass<=this limit use debris collision settings");
		REGISTER_CVAR2("p_max_object_splashes", &pVars->maxSplashesPerObj, pVars->maxSplashesPerObj, 0,
			"Specifies how many splash events one entity is allowed to generate");
		REGISTER_CVAR2("p_splash_dist0", &pVars->splashDist0, pVars->splashDist0, 0,
			"Range start for splash event distance culling");
		REGISTER_CVAR2("p_splash_force0", &pVars->minSplashForce0, pVars->minSplashForce0, 0,
			"Minimum water hit force to generate splash events at p_splash_dist0");
		REGISTER_CVAR2("p_splash_vel0", &pVars->minSplashVel0, pVars->minSplashVel0, 0,
			"Minimum water hit velocity to generate splash events at p_splash_dist0");
		REGISTER_CVAR2("p_splash_dist1", &pVars->splashDist1, pVars->splashDist1, 0,
			"Range end for splash event distance culling");
		REGISTER_CVAR2("p_splash_force1", &pVars->minSplashForce1, pVars->minSplashForce1, 0,
			"Minimum water hit force to generate splash events at p_splash_dist1");
		REGISTER_CVAR2("p_splash_vel1", &pVars->minSplashVel1, pVars->minSplashVel1, 0,
			"Minimum water hit velocity to generate splash events at p_splash_dist1");
		REGISTER_CVAR2("p_joint_gravity_step", &pVars->jointGravityStep, pVars->jointGravityStep, 0,
			"Time step used for gravity in breakable joints (larger = stronger gravity effects)");
		REGISTER_CVAR2("p_debug_explosions", &pVars->bDebugExplosions, pVars->bDebugExplosions, 0,
			"Turns on explosions debug mode");
		REGISTER_CVAR2("p_num_threads", &pVars->numThreads, pVars->numThreads, 0,
			"The number of internal physics threads");
		REGISTER_CVAR2("p_joint_damage_accum", &pVars->jointDmgAccum, pVars->jointDmgAccum, 0,
			"Default fraction of damage (tension) accumulated on a breakable joint");
		REGISTER_CVAR2("p_joint_damage_accum_threshold", &pVars->jointDmgAccumThresh, pVars->jointDmgAccumThresh, 0,
			"Default damage threshold (0..1) for p_joint_damage_accum");
		REGISTER_CVAR2("p_rope_collider_size_limit", &pVars->maxRopeColliderSize, pVars->maxRopeColliderSize, 0,
			"Disables rope collisions with meshes having more triangles than this (0-skip the check)");

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
		REGISTER_CVAR2("p_net_interp", &pVars->netInterpTime, pVars->netInterpTime, 0,
			"The amount of time which the client will lag behind received packet updates. High values result in smoother movement but introduces additional lag as a trade-off.");
		REGISTER_CVAR2("p_net_extrapmax", &pVars->netExtrapMaxTime, pVars->netExtrapMaxTime, 0,
			"The maximum amount of time the client is allowed to extrapolate the position based on last received packet.");
		REGISTER_CVAR2("p_net_sequencefrequency", &pVars->netSequenceFrequency, pVars->netSequenceFrequency, 0,
			"The frequency at which sequence numbers increase per second, higher values add accuracy but go too high and the sequence numbers will wrap round too fast");
		REGISTER_CVAR2("p_net_debugDraw", &pVars->netDebugDraw, pVars->netDebugDraw, 0,
			"Draw some debug graphics to help diagnose issues (requires p_draw_helpers to be switch on to work, e.g. p_draw_helpers rR_b)");
#else
		REGISTER_CVAR2("p_net_minsnapdist", &pVars->netMinSnapDist, pVars->netMinSnapDist, 0,
			"Minimum distance between server position and client position at which to start snapping");
		REGISTER_CVAR2("p_net_velsnapmul", &pVars->netVelSnapMul, pVars->netVelSnapMul, 0,
			"Multiplier to expand the p_net_minsnapdist based on the objects velocity");
		REGISTER_CVAR2("p_net_minsnapdot", &pVars->netMinSnapDot, pVars->netMinSnapDot, 0,
			"Minimum quat dot product between server orientation and client orientation at which to start snapping");
		REGISTER_CVAR2("p_net_angsnapmul", &pVars->netAngSnapMul, pVars->netAngSnapMul, 0,
			"Multiplier to expand the p_net_minsnapdot based on the objects angular velocity");
		REGISTER_CVAR2("p_net_smoothtime", &pVars->netSmoothTime, pVars->netSmoothTime, 0,
			"How much time should non-snapped positions take to synchronize completely?");
#endif

		REGISTER_CVAR2("p_ent_grid_use_obb", &pVars->bEntGridUseOBB, pVars->bEntGridUseOBB, 0,
			"Whether to use OBBs rather than AABBs for the entity grid setup for brushes");
		REGISTER_CVAR2("p_num_startup_overload_checks", &pVars->nStartupOverloadChecks, pVars->nStartupOverloadChecks, 0,
			"For this many frames after loading a level, check if the physics gets overloaded and freezes non-player physicalized objects that are slow enough");
		REGISTER_CVAR2("p_break_on_awake_ent_id", &pVars->idEntBreakOnAwake, pVars->idEntBreakOnAwake, 0,
			"Sets the id of the entity that will trigger debug break if awoken");

		pVars->flagsColliderDebris = geom_colltype_debris;
		pVars->flagsANDDebris = ~(geom_colltype_vehicle | geom_colltype6);
		pVars->ticksPerSecond = gEnv->pTimer->GetTicksPerSecond();

		if (gEnv->IsEditor())
		{
			// Setup physical grid for Editor.
			int nCellSize = 16;

			gEnv->pPhysicalWorld->SetupEntityGrid(2, Vec3(0, 0, 0), (2048) / nCellSize, (2048) / nCellSize, (float)nCellSize, (float)nCellSize);
			gEnv->pConsole->CreateKeyBind("comma", "#System.SetCVar(\"p_single_step_mode\",1-System.GetCVar(\"p_single_step_mode\"));");
			gEnv->pConsole->CreateKeyBind("period", "p_do_step 1");
		}
	}

}

#endif
