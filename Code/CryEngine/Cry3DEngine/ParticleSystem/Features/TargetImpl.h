// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace pfx2
{

ILINE Vec3 CTargetSource::GetTarget(const CParticleComponentRuntime& runtime, TParticleId particleId, bool isParentId)
{
	const CParticleEmitter& emitter = *runtime.GetEmitter();
	const Quat defaultQuat = emitter.GetLocation().q;

	if (m_source == ETargetSource::Target)
	{
		const ParticleTarget& target = emitter.GetTarget();
		const Vec3 wTarget = target.bTarget ?
		                     target.vTarget :
		                     emitter.GetPos();
		return wTarget + defaultQuat * m_offset;
	}

	const CParticleContainer* pContainer = &runtime.GetContainer();

	if (m_source == ETargetSource::Parent || isParentId)
	{
		if (!isParentId)
		{
			const IPidStream parentIds = pContainer->GetIPidStream(EPDT_ParentId);
			particleId = parentIds.Load(particleId);
		}
		pContainer = &runtime.GetParentContainer();
	}

	const IVec3Stream positions = pContainer->GetIVec3Stream(EPVF_Position);
	const IQuatStream quats = pContainer->GetIQuatStream(EPQF_Orientation, defaultQuat);

	const Quat wQuat = quats.SafeLoad(particleId);
	const Vec3 wTarget = positions.Load(particleId);

	return wTarget + wQuat * m_offset;
}

}
