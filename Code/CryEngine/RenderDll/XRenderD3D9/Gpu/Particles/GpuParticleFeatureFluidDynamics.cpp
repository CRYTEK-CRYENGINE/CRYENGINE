// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuParticleFeatureFluidDynamics.h"
#include "GpuParticleComponentRuntime.h"

namespace gpu_pfx2
{

void CFeatureMotionFluidDynamics::Update(const gpu_pfx2::SUpdateContext& context, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CParticleComponentRuntime* pRuntime = (CParticleComponentRuntime*)(context.pRuntime);
	gpu_physics::CParticleFluidSimulation* pFluidSim = pRuntime->GetFluidSimulation();
	if (!pFluidSim)
		pFluidSim = pRuntime->CreateFluidSimulation();

	Vec3 emitterPos = pRuntime->GetPos();
	auto& p = GetParameters();

	// [PFX2_TODO_GPU]: This needs to change, spawning should be its separate component
	// and spawning needs to be more configurable and coherent with the rest of the system
	CRY_ASSERT(p.numSpawnParticles < 512);
	gpu_physics::SFluidBody bodies[512];
	for (int i = 0; i < p.numSpawnParticles; ++i)
	{
		float randX = cry_random(-1.0f, 1.0f);
		float randY = cry_random(-1.0f, 1.0f);
		float randZ = cry_random(-1.0f, 1.0f);
		bodies[i].x = emitterPos +
		              Vec3(randX, randY, randZ) * p.spread;
		bodies[i].lifeTime = 0.0f;
		bodies[i].v = p.initialVelocity;
		bodies[i].xp = bodies[i].x + bodies[i].v * 0.016;
		bodies[i].phase = 0;
		bodies[i].vorticity = Vec3(0.0f, 0.0f, 0.0f);
	}
	pFluidSim->InjectBodies(bodies, p.numSpawnParticles);

	gpu_physics::SParticleFluidParameters params;
	params.stiffness = p.stiffness;
	params.gravityConstant = p.gravityConstant;
	params.h = p.h;
	params.r0 = p.r0;
	params.mass = p.mass;
	params.maxVelocity = p.maxVelocity;
	params.maxForce = p.maxForce;
	params.atmosphericDrag = p.atmosphericDrag;
	params.cohesion = p.cohesion;
	params.numberOfIterations = p.numberOfIterations;
	params.baroclinity = p.baroclinity;
	params.gridSizeX = p.gridSizeX;
	params.gridSizeY = p.gridSizeY;
	params.gridSizeZ = p.gridSizeZ;
	params.deltaTime = context.deltaTime;
	params.worldOffsetX = emitterPos[0] - 0.5f * params.gridSizeX * params.h;
	params.worldOffsetY = emitterPos[1] - 0.5f * params.gridSizeY * params.h;
	params.worldOffsetZ = emitterPos[2] - 0.5f * params.gridSizeZ * params.h;
	params.particleInfluence = p.particleInfluence;
	pFluidSim->SetParameters(&params);
	pRuntime->FluidCollisions(commandList);
	pFluidSim->RenderThreadUpdate(commandList);
	pRuntime->EvolveParticles(commandList);
}

void CFeatureMotionFluidDynamics::Initialize() {}
}
