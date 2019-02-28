// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
// All the definitions used by the audition map and its sub-component.

#pragma once

// When this label is not 0 then we include code for debugging generic information
// in the audition map.
#if !defined(_RELEASE)
#define AUDITION_MAP_GENERIC_DEBUGGING (1)
#else
#define AUDITION_MAP_GENERIC_DEBUGGING (0)
#endif


// When this label is not 0 then we include code for debugging ray-casts that are
// send by the audition map.
#if !defined(_RELEASE)
#define AUDITION_MAP_RAY_CAST_DEBUGGING (1)
#else
#define AUDITION_MAP_RAY_CAST_DEBUGGING (0)
#endif


// When this label is not 0 then we include code that stores a human readable
// debug name with the 'audition IDs' that identify each unique listener.
#if !defined(_RELEASE)
#define AUDITION_MAP_STORE_DEBUG_NAMES_FOR_LISTENER_INSTANCE_ID (1)
#else
#define AUDITION_MAP_STORE_DEBUG_NAMES_FOR_LISTENER_INSTANCE_ID (0)
#endif
