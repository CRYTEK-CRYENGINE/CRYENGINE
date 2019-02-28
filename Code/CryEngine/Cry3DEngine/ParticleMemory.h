// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleMemory.h
//  Version:     v1.00
//  Created:     18/03/2010 by Corey Spink
//  Compilers:   Visual Studio.NET
//  Description: All the particle system's specific memory needs
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particlememory_h__
#define __particlememory_h__
#pragma once

#include "ParticleFixedSizeElementPool.h"

class ParticleObjectPool;
ParticleObjectPool& ParticleObjectAllocator();

#endif // __particlememory_h__
