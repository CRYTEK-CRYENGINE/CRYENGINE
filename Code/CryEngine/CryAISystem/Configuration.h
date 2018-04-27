// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Configuration.h
//  Created:     02/08/2008 by Matthew
//  Description: Simple struct for storing fundamental AI system settings
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AICONFIGURATION
#define __AICONFIGURATION

#pragma once

// These values should not change and are carefully chosen but arbitrary
enum EConfigCompatabilityMode
{
	ECCM_NONE    = 0,
	ECCM_CRYSIS  = 2,
	ECCM_GAME04  = 4,
	ECCM_WARFACE = 7,
	ECCM_CRYSIS2 = 8
};

struct SConfiguration
{
	SConfiguration()
		: eCompatibilityMode(ECCM_NONE)
	{
	}

	EConfigCompatabilityMode eCompatibilityMode;

	// Should probably include logging and debugging flags
};

#endif // __AICONFIGURATION
