// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/IAuditionMap.h>

namespace Perception
{

inline void ReflectType(Schematyc::CTypeDesc<EStimulusObstructionHandling>& desc)
{
	desc.SetGUID("B741A80F-FBB3-40B2-BCAA-6757DA3B4434"_cry_guid);

	desc.AddConstant(EStimulusObstructionHandling::IgnoreAllObstructions, "IgnoreAllObstructions", "Ignore All Obstructions");
	desc.AddConstant(EStimulusObstructionHandling::RayCastWithLinearFallOff, "RayCastWithLinearFallOff", "Raycast with Linear Falloff");
}
}

namespace PerceptionSystemSchematyc
{
	void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope);
}