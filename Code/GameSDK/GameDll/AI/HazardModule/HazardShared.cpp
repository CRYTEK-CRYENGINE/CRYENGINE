// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Various class and types that are shared between the various hazard module classes.

#include "StdAfx.h"

#include "HazardShared.h"


using namespace HazardSystem;


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardProjectileID -- HazardProjectileID -- HazardProjectileID --
//
// ============================================================================
// ============================================================================
// ============================================================================


void HazardProjectileID::Serialize(TSerialize ser)
{
	ser.BeginGroup("HazardProjectileID");

	ser.Value("ID", m_ID);

	ser.EndGroup();
}


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardSphereID -- HazardSphereID -- HazardSphereID -- HazardSphereID --
//
// ============================================================================
// ============================================================================
// ============================================================================


void HazardSphereID::Serialize(TSerialize ser)
{
	ser.BeginGroup("HazardSphereID");

	ser.Value("ID", m_ID);

	ser.EndGroup();
}
