// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef VisionMapTypes_h
	#define VisionMapTypes_h

//! This must not exceed 31.
enum VisionMapTypes
{
	General     = 1 << 0,
	AliveAgent  = 1 << 1,
	DeadAgent   = 1 << 2,
	Player      = 1 << 3,
	Interesting = 1 << 4,
	SearchSpots = 1 << 5,
};

#endif // VisionMapTypes_h
