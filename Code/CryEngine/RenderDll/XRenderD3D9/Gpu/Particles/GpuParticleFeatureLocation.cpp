// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "GpuParticleFeatureLocation.h"
#include "GpuParticleComponentRuntime.h"

namespace gpu_pfx2
{

void CFeatureLocationOffset::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& initParams = pRuntime->GetParticleInitializationParameters();
	auto& params = GetParameters();
	initParams.offset = params.offset;
	initParams.scale.x = params.scale;
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_LocationOffset);
}

void CFeatureLocationBox::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& initParams = pRuntime->GetParticleInitializationParameters();
	auto& params = GetParameters();
	initParams.box = params.box;
	initParams.scale.x = params.scale;
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_LocationBox);
}

void CFeatureLocationSphere::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& initParams = pRuntime->GetParticleInitializationParameters();
	auto& params = GetParameters();
	initParams.scale = params.scale;
	initParams.radius = params.radius;
	initParams.velocity = params.velocity;
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_LocationSphere);
}

void CFeatureLocationCircle::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& initParams = pRuntime->GetParticleInitializationParameters();
	auto& params = GetParameters();
	initParams.scale.x = params.scale.x;
	initParams.scale.y = params.scale.y;
	initParams.radius = params.radius;
	initParams.velocity = params.velocity;
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_LocationCircle);
}

void CFeatureLocationNoise::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& initParams = pRuntime->GetParticleInitializationParameters();
	auto& params = GetParameters();
	initParams.amplitude = params.amplitude;
	initParams.noiseSize = params.size;
	initParams.rate = params.rate;
	initParams.octaves = params.octaves;
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_LocationNoise);
}

}
