// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   TacticalPointQueryEnum.h
   $Id$
   $DateTime$
   Description: The core enum of the TPS system, listing all query tokens
             Never used it before? There a comprehensive technical word doc!
             If you are ADDING a query, read the short notes below
   ---------------------------------------------------------------------
   History:
   - 30:07:2007 : Separated out by Matthew Jack

 *********************************************************************/

/* Adding a Query - quickstart - MTJ 11/09/2007

   1. Add the query to the appropriate section of the enum below. NOTE that the values and order are important!
   2. Add an entry in the keyword map, to allow the query to be parsed (CTacticalPointSystem::CTacticalPointSystem())
   3. Give it a cost ordering (WIP) for performance purposes (also CTacticalPointSystem::CTacticalPointSystem())
   4. Enter the query in the appropriate switch statement in BoolProperty(), BoolTest(), RealProperty(), RealMeasure().
   5. If it's real-valued, give an _appropriate_ output range in the RealRange() switch statement.
   6. Run.
 */

#pragma once

#ifndef __TacticalPointQueryEnum_H__
	#define __TacticalPointQueryEnum_H__

	#if _MSC_VER > 1000
		#pragma once
	#endif

	#include <CryAISystem/ITacticalPointSystem.h>

typedef uint32 TTacticalPointQuery;

/*
   This would leave space for 4096 generators without the aux object, but only 8 with.
   Also, in the case of those _O queries, the object would need to be removed to access the actual query
 */

// When adding to this enum, remember to add the corresponding string for parsing

// The below is NOT a misguided attempt at performance optimization.
// It is a way to greatly simplify the code by making every token output by the string parser
// of a single type which contains simple flags to differentiate types of token and can immediately
// be stored in a Criterion without requiring any further types or classes.
// Saves a whole bunch of functions and if/elses to maintain typing.
// ( But it certainly shouldn't be any slower either mind you ;-) )

enum ETacticalPointQuery
{
	eTPQ_None = 0,                    // Useful to indicate "none" or "unrecognised"
	eTPQ_Glue,                        // Useful token to indicate "glue word" but this is never stored
	eTPQ_Around,                      // We take careful note of parsing this particular glueword for Generation,
	//  to make sure that the Object and aux Object (if any) don't get confused
	//  but again it is not explicitly stored

	eTPQ_MASK_QUERYSPACE_GEN_O = 0x0000000F,    // Query space for 16 Generator O queries
	eTPQ_MASK_OBJECT_AUX       = 0x00000FF0,    // Only within an eTPQ_FLAG_GENERATOR_O, these nibbles hold an auxiliary object
	eTPQ_MASK_QUERYSPACE       = 0x00000FFF,    // Query space for 4096 queries

	eTPQ_FLAG_PROP_BOOL        = 0x00001000,    // Flags queries that depend on no Object and are Boolean-valued
	eTPQ_PB_CoverSoft,
	eTPQ_PB_CoverSuperior,
	eTPQ_PB_CoverInferior,
	eTPQ_PB_CurrentlyUsedObject,
	eTPQ_PB_Reachable,
	eTPQ_PB_IsInNavigationMesh,               // Checks if a point can be associated with a valid mesh triangle
	eTPQ_GAMESTART_PROP_BOOL,

	eTPQ_FLAG_PROP_REAL = 0x00002000,           // Flags queries that depend on no Object and are Real-valued
	eTPQ_PR_CoverRadius,
	eTPQ_PR_CoverDensity,
	eTPQ_PR_BulletImpacts,
	eTPQ_PR_HostilesDistance,
	eTPQ_PR_FriendlyDistance,
	eTPQ_PR_CameraVisibility,
	eTPQ_PR_CameraCenter,
	eTPQ_PR_Random,
	eTPQ_PR_Type,
	eTPQ_GAMESTART_PROP_REAL,

	eTPQ_FLAG_TEST = 0x00004000,              // Flags queries that use an Object and are Boolean-valued
	eTPQ_T_Visible,
	eTPQ_T_CanShoot,
	eTPQ_T_CanShootTwoRayTest,                // Performs two raycasts horizontally spaced apart from the location of the weapon.
	eTPQ_T_Towards,
	eTPQ_T_CanReachBefore,
	eTPQ_T_CrossesLineOfFire,
	eTPQ_T_HasShootingPosture,
	eTPQ_T_OtherSide,
	eTPQ_GAMESTART_TEST,

	eTPQ_FLAG_MEASURE = 0x00008000,             // Flags queries that use an Object and are Real-valued
	eTPQ_M_Distance,
	eTPQ_M_Distance2d,
	eTPQ_M_PathDistance,
	eTPQ_M_ChangeInDistance,
	eTPQ_M_DistanceInDirection,
	eTPQ_M_DistanceLeft,
	eTPQ_M_RatioOfDistanceFromActorAndDistance,
	eTPQ_M_Directness,
	eTPQ_M_Dot,
	eTPQ_M_ObjectsDot,
	eTPQ_M_ObjectsMoveDirDot,
	eTPQ_M_HeightRelative,
	eTPQ_M_AngleOfElevation,
	eTPQ_M_PointDirDot,
	eTPQ_M_CoverHeight,
	eTPQ_M_EffectiveCoverHeight,
	eTPQ_M_DistanceToRay,                     // Smallest distance to ray constructed from an object's pivot and its forward direction.
	eTPQ_M_AbsDistanceToPlaneAtClosestRayPos, // Absolute distance to the plane that is placed at the actor's closest point on a ray
	// (the normal of the plane is that of the ray's direction).
	eTPQ_GAMESTART_MEASURE,

	eTPQ_FLAG_GENERATOR = 0x00010000,           // Flags queries that generate points
	eTPQ_G_Grid,
	eTPQ_G_Entities,
	eTPQ_G_Indoor,
	eTPQ_G_CurrentPos,
	eTPQ_G_CurrentCover,
	eTPQ_G_CurrentFormationPos,
	eTPQ_G_Objects,
	eTPQ_G_PointsInNavigationMesh,
	eTPQ_G_PureGrid,
	eTPQ_GAMESTART_GENERATOR,

	eTPQ_FLAG_GENERATOR_O = 0x00020000,           // Flags queries that generate points referring to an object
	eTPQ_GO_Hidespots,
	eTPQ_GO_Cover,
	eTPQ_GAMESTART_GENERATOR_O,

	eTPQ_MASK_QUERY_TYPE     = 0x000FF000,    // Mask to remove any Limit flags and leave just Query type
	eTPQ_MASK_QUERY          = 0x000FFFFF,    // Mask to remove any Limit flags and leave just Query

	eTPQ_FLAG_PRIMARY_OBJECT = 0x00100000,          // Starts range describing the primary Object
	eTPQ_O_Actor,
	eTPQ_O_AttentionTarget,                 // Valid to leave these nibbles blank, for no Object
	eTPQ_O_RealTarget,
	eTPQ_O_ReferencePoint,
	eTPQ_O_ReferencePointOffsettedByItsForwardDirection,
	eTPQ_O_CurrentFormationRef,
	eTPQ_O_Player,
	eTPQ_O_Leader,
	eTPQ_O_LastOp,
	eTPQ_O_None,
	eTPQ_O_Entity,
	eTPQ_GAMESTART_PRIMARY_OBJECT,

	eTPQ_MASK_OBJECT = 0x0FF00000,            // Mask to just leave the primary Object

	eTPQ_L_Min       = 0x10000000,          // Start range describing any Limit that should be applied
	eTPQ_L_Max       = 0x20000000,          // Limits effectively provide conversion from Real to Boolean
	eTPQ_L_Equal     = 0x30000000,          // Limits effectively provide conversion from Real to Boolean

	eTPQ_MASK_LIMIT  = 0xF0000000,            // Mask to remove Query and leave just Limit flags
};

enum ETacticalPointQueryParameter
{
	eTPQP_Invalid        = 0,

	eTPQP_FLAG_PROP_BOOL = 0x00001000,

	eTPQP_FLAG_PROP_REAL = 0x00002000,
	eTPQP_ObjectsType,
	eTPQP_Density,
	eTPQP_Height,
	eTPQP_HorizontalSpacing,

	eTPQP_FLAG_PROP_STRING = 0x00004000,
	eTPQP_OptionLabel,
	eTPQP_TagPointPostfix,
	eTPQP_ExtenderStringParameter,
	eTPQP_NavigationAgentType,

	eTPQP_MASK_TYPE = 0x000FF000,
};

enum ETPSRelativeValueSource
{
	eTPSRVS_Invalid      = 0,

	eTPSRVS_objectRadius = 0x00000001,
};

#endif // __TacticalPointQueryEnum_H__
