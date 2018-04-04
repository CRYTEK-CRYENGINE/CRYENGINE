// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

namespace pfx2
{

ILINE void KillOnParentDeath(const SUpdateContext& context)
{
	CParticleContainer& container = context.m_container;
	const CParticleContainer& parentContainer = context.m_parentContainer;
	const auto parentStates = parentContainer.GetTIStream<uint8>(EPDT_State);
	const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
	IOFStream ages = container.GetIOFStream(EPDT_NormalAge);

	CRY_PFX2_FOR_ACTIVE_PARTICLES(context)
	{
		const TParticleId parentId = parentIds.Load(particleId);
		const uint8 parentState = (parentId != gInvalidId) ? parentStates.Load(parentId) : ES_Expired;
		if ((parentState & ESB_Dead) != 0)
			ages.Store(particleId, 1.0f);
	}
	CRY_PFX2_FOR_END;
}

}
