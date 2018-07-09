// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//-------------------------------------------------------------------------
//
// Description:
//  Basic type definitions used by the CryMannequin AnimationGraph adapter
//  classes
//
////////////////////////////////////////////////////////////////////////////
#ifndef __MANNEQUINAGDEFS_H__
#define __MANNEQUINAGDEFS_H__
#pragma once

namespace MannequinAG
{

// ============================================================================
// ============================================================================

typedef CryFixedStringT<64> TKeyValue;

enum ESupportedInputID
{
	eSIID_Action = 0, // has to start with zero (is an assumption all over the place).,
	eSIID_Signal,

	eSIID_COUNT,
};

enum EAIActionType
{
	EAT_Looping,
	EAT_OneShot,
};
// ============================================================================
// ============================================================================

} // namespace MannequinAG

#endif // __MANNEQUINAGDEFS_H__
