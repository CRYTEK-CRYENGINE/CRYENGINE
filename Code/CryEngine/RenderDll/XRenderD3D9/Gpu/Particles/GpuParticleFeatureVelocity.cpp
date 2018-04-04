// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "GpuParticleFeatureVelocity.h"
#include "GpuParticleComponentRuntime.h"

namespace gpu_pfx2
{

void gpu_pfx2::CFeatureVelocityInherit::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& params = pRuntime->GetParticleInitializationParameters();
	params.velocityScale = GetParameters().scale;
}

void CFeatureVelocityCone::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& params = pRuntime->GetParticleInitializationParameters();
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_VelocityCone);
	params.angle = GetParameters().angle;
	params.velocity = GetParameters().velocity;
}

void CFeatureVelocityDirectional::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& params = pRuntime->GetParticleInitializationParameters();
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_VelocityDirectional);
	params.direction = GetParameters().direction;
	params.directionScale = GetParameters().scale;
}

void CFeatureVelocityOmniDirectional::InitParticles(const SUpdateContext& context)
{
	CParticleComponentRuntime* pRuntime = static_cast<CParticleComponentRuntime*>(context.pRuntime);
	auto& params = pRuntime->GetParticleInitializationParameters();
	pRuntime->SetInitializationFlags(eFeatureInitializationFlags_VelocityOmniDirectional);
	params.omniVelocity = GetParameters().velocity;
}

}
