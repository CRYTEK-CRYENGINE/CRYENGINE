// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleMemory.cpp
//  Version:     v1.00
//  Created:     18/03/2010 by Corey Spink
//  Compilers:   Visual Studio.NET
//  Description: All the particle system's specific memory needs
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleMemory.h"
#include "Particle.h"
#include "Cry3DEngineBase.h"

bool g_bParticleObjectPoolInitialized = false;
CRY_ALIGN(128) char sStorageParticleObjectPool[sizeof(ParticleObjectPool)];
CryCriticalSection g_ParticlePoolInitLock;

///////////////////////////////////////////////////////////////////////////////
ParticleObjectPool& ParticleObjectAllocator()
{
	IF (g_bParticleObjectPoolInitialized == false, 0)
	{
		AUTO_LOCK(g_ParticlePoolInitLock);
		IF (g_bParticleObjectPoolInitialized == false, 0)
		{
			new(sStorageParticleObjectPool) ParticleObjectPool();
			alias_cast<ParticleObjectPool*>(sStorageParticleObjectPool)->Init(Cry3DEngineBase::GetCVars()->e_ParticlesPoolSize << 10);
			MemoryBarrier();
			g_bParticleObjectPoolInitialized = true;
		}
	}

	return *alias_cast<ParticleObjectPool*>(sStorageParticleObjectPool);
}
