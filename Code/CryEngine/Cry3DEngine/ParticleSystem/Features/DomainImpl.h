// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace pfx2
{

namespace detail
{




ILINE const CParticleContainer& GetContainer(const SUpdateContext context, EModDomain domain)
{
	return domain == EMD_PerParticle ? context.m_container : context.m_parentContainer;
}

class CSelfStreamSampler
{
public:
	CSelfStreamSampler(const SUpdateContext& context, TDataType<float> sourceStreamType, EModDomain domain = EMD_PerParticle)
		: sourceStream(GetContainer(context, domain).GetIFStream(sourceStreamType))
	{}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		return sourceStream.Load(particleId);
	}
private:
	IFStream sourceStream;
};

class CParentStreamSampler
{
public:
	CParentStreamSampler(const SUpdateContext& context, TDataType<float> sourceStreamType)
		: parentSourceStream(context.m_parentContainer.GetIFStream(sourceStreamType, 1.0f))
		, parentIds(context.m_container.GetIPidStream(EPDT_ParentId))
	{}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		const TParticleIdv parentId = parentIds.Load(particleId);
		return parentSourceStream.SafeLoad(parentId);
	}
private:
	IFStream   parentSourceStream;
	IPidStream parentIds;
};

class CParentAgeSampler
{
public:
	CParentAgeSampler(const SUpdateContext& context)
		: deltaTime(ToFloatv(context.m_deltaTime))
		, selfAges(context.m_container.GetIFStream(EPDT_NormalAge))
		, parentAges(context.m_parentContainer.GetIFStream(EPDT_NormalAge))
		, parentInvLifeTimes(context.m_parentContainer.GetIFStream(EPDT_InvLifeTime))
		, parentIds(context.m_container.GetIPidStream(EPDT_ParentId))
	{}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		const TParticleIdv parentId = parentIds.Load(particleId);
		const floatv selfAge = selfAges.Load(particleId);
		const floatv parentAge = parentAges.SafeLoad(parentId);
		const floatv parentInvLifeTime = parentInvLifeTimes.SafeLoad(parentId);

		// anti-alias parent age
		const floatv tempAntAliasParentAge = MAdd(selfAge * parentInvLifeTime, deltaTime, parentAge);
		const floatv sample = __fsel(selfAge, parentAge, tempAntAliasParentAge);
		return sample;
	}
private:
	IFStream   selfAges;
	IFStream   parentAges;
	IFStream   parentInvLifeTimes;
	IPidStream parentIds;
	floatv     deltaTime;
};

class CLevelTimeSampler
{
public:
	CLevelTimeSampler(const SUpdateContext& context, EModDomain domain)
		: deltaTime(ToFloatv(context.m_deltaTime))
		, ages(GetContainer(context, domain).GetIFStream(EPDT_NormalAge))
		, lifeTimes(GetContainer(context, domain).GetIFStream(EPDT_LifeTime))
		, levelTime(ToFloatv(context.m_time))
	{}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		return StartTime(levelTime, deltaTime, ages.Load(particleId) * lifeTimes.Load(particleId));
	}
private:
	IFStream ages;
	IFStream lifeTimes;
	floatv   levelTime;
	floatv   deltaTime;
};

class CSelfSpeedSampler
{
public:
	CSelfSpeedSampler(const SUpdateContext& context, EModDomain domain = EMD_PerParticle)
		: velocities(GetContainer(context, domain).GetIVec3Stream(EPVF_Velocity))
	{}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		const Vec3v velocity = velocities.Load(particleId);
		return velocity.GetLength();
	}
private:
	IVec3Stream velocities;
};

class CParentSpeedSampler
{
public:
	CParentSpeedSampler(const SUpdateContext& context)
		: parentVelocities(context.m_parentContainer.GetIVec3Stream(EPVF_Velocity))
		, parentIds(context.m_container.GetIPidStream(EPDT_ParentId))
	{}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		const uint32v parentId = parentIds.Load(particleId);
		const Vec3v velocity = parentVelocities.SafeLoad(parentId);
		const floatv speed = velocity.GetLength();
		const floatv sample = if_else_zero(parentId != ToUint32v(gInvalidId), speed);
		return sample;
	}
private:
	IVec3Stream parentVelocities;
	IPidStream  parentIds;
};

class CAttributeSampler
{
public:
	CAttributeSampler(const SUpdateContext& context, const CAttributeReference& attr);
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		return m_attributeValue;
	}
private:
	floatv m_attributeValue;
};

class CConstSampler
{
public:
	CConstSampler(float value)
		: m_value(ToFloatv(value)) {}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		return m_value;
	}
private:
	floatv m_value;
};

class CChaosSampler
{
public:
	CChaosSampler(const SUpdateContext& context)
		: m_chaos(context.m_spawnRngv) {}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		return m_chaos.RandUNorm();
	}
private:
	SChaosKeyV& m_chaos;
};

class CViewAngleSampler
{
public:
	CViewAngleSampler(const SUpdateContext& context, EModDomain domain = EMD_PerParticle)
		: m_positions(GetContainer(context, domain).GetIVec3Stream(EPVF_Position))
		, m_orientations(GetContainer(context, domain).GetIQuatStream(EPQF_Orientation))
		, m_cameraPosition(ToVec3v(gEnv->p3DEngine->GetRenderingCamera().GetPosition())) {}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		const Vec3v position = m_positions.Load(particleId);
		const Quatv orientation = m_orientations.Load(particleId);
		const Vec3v normal = GetColumn2(orientation);
		const Vec3v toCamera = GetNormalized(m_cameraPosition - position);
		const floatv cosAngle = fabs_tpl(toCamera.Dot(normal));
		return cosAngle;
	}
private:
	Vec3v GetColumn2(Quatv q) const 
	{
		const floatv two = ToFloatv(2.0f);
		const floatv one = ToFloatv(1.0f);
		return Vec3v(
			two * (q.v.x * q.v.z + q.v.y * q.w),
			two * (q.v.y * q.v.z - q.v.x * q.w),
			two * (q.v.z * q.v.z + q.w * q.w) - one);
	}
	Vec3v GetNormalized(Vec3v v) const
	{
		#ifdef CRY_PFX2_USE_SSE
		return v * _mm_rsqrt_ps(v.Dot(v));
		#else
		return v.GetNormalized();
		#endif
	}
	IVec3Stream m_positions;
	IQuatStream m_orientations;
	Vec3v       m_cameraPosition;
};

class CCameraDistanceSampler
{
public:
	CCameraDistanceSampler(const SUpdateContext& context, EModDomain domain = EMD_PerParticle)
		: m_positions(GetContainer(context, domain).GetIVec3Stream(EPVF_Position))
		, m_cameraPosition(ToVec3v(gEnv->p3DEngine->GetRenderingCamera().GetPosition())) {}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		const Vec3v position = m_positions.Load(particleId);
		const floatv distance = position.GetDistance(m_cameraPosition);
		return distance;
	}
private:
	IVec3Stream m_positions;
	Vec3v       m_cameraPosition;
};

}

ILINE CDomain::CDomain()
	: m_domain(EDomain::Age)
	, m_fieldSource(EDomainField(EPDT_LifeTime))
	, m_sourceOwner(EDomainOwner::Self)
	, m_sourceGlobal(EDomainGlobal::LevelTime)
	, m_domainScale(1.0f)
	, m_domainBias(0.0f)
	, m_spawnOnly(true)
{
}

template<typename TParam, typename TMod>
ILINE void CDomain::AddToParam(CParticleComponent* pComponent, TParam* pParam, TMod* pModifier)
{
	if (m_spawnOnly)
		pParam->AddToInitParticles(pModifier);
	else
		pParam->AddToUpdate(pModifier);

	CParticleComponent* pSourceComponent = m_sourceOwner == EDomainOwner::Parent ? pComponent->GetParentComponent() : pComponent;
	if (pSourceComponent)
	{
		if (m_domain == EDomain::SpawnFraction)
			pSourceComponent->AddParticleData(EPDT_SpawnFraction);
		else if (m_domain == EDomain::Speed)
			pSourceComponent->AddParticleData(EPVF_Velocity);
		else if (m_domain == EDomain::Field)
			pSourceComponent->AddParticleData((EParticleDataType)m_fieldSource);
		else if (m_domain == EDomain::ViewAngle)
			pSourceComponent->AddParticleData(EPQF_Orientation);
	}
}

ILINE EModDomain CDomain::GetDomain() const
{
	switch (m_domain)
	{
	case EDomain::Global:
	case EDomain::Attribute:
		return EMD_PerEffect;
	}

	switch (m_sourceOwner)
	{
	case EDomainOwner::Self:
		return EMD_PerParticle;
	case EDomainOwner::Parent:
		return EMD_PerInstance;
	default:
		return EMD_PerEffect;
	}
}
ILINE TDataType<float> CDomain::GetDataType() const
{
	switch (m_domain)
	{
	case EDomain::Age:
		return EPDT_NormalAge;
	case EDomain::SpawnFraction:
		return EPDT_SpawnFraction;
	case EDomain::Field:
		return TDataType<float>(m_fieldSource);
	default:
		return TDataType<float>(EParticleDataType::size());
	}
}

template<typename TBase, typename TStream>
ILINE void CDomain::Dispatch(const SUpdateContext& context, const SUpdateRange& range, TStream stream, EModDomain domain) const
{
	switch (m_domain)
	{
	case EDomain::Global:
		if (m_sourceGlobal == EDomainGlobal::LevelTime)
			((TBase*)this)->DoModify(
				context, range, stream,
				detail::CLevelTimeSampler(context, domain));
		else
			((TBase*)this)->DoModify(
				context, range, stream,
				detail::CConstSampler(GetGlobalValue(m_sourceGlobal)));			
		break;
	case EDomain::Attribute:
		((TBase*)this)->DoModify(
		  context, range, stream,
		  detail::CAttributeSampler(context, m_attribute));
		break;
	case EDomain::Random:
		((TBase*)this)->DoModify(
		  context, range, stream,
		  detail::CChaosSampler(context));
		break;
	case EDomain::ViewAngle:
		((TBase*)this)->DoModify(
			context, range, stream,
			detail::CViewAngleSampler(context, domain));
		break;
	case EDomain::CameraDistance:
		if (!(C3DEngine::GetCVars()->e_ParticlesDebug & AlphaBit('u')) || domain == EMD_PerParticle)
			((TBase*)this)->DoModify(
				context, range, stream,
				detail::CCameraDistanceSampler(context, domain));
		break;
	case EDomain::Speed:
		if (m_sourceOwner == EDomainOwner::Self || domain == EMD_PerInstance)
			((TBase*)this)->DoModify(
			  context, range, stream,
			  detail::CSelfSpeedSampler(context, domain));
		else
			((TBase*)this)->DoModify(
			  context, range, stream,
			  detail::CParentSpeedSampler(context));
		break;
	default:
		if (m_sourceOwner == EDomainOwner::Self || domain == EMD_PerInstance)
			((TBase*)this)->DoModify(
				context, range, stream,
				detail::CSelfStreamSampler(context, GetDataType(), domain));
		else if (GetDataType() == EPDT_NormalAge)
			((TBase*)this)->DoModify(
				context, range, stream,
				detail::CParentAgeSampler(context));
		else
			((TBase*)this)->DoModify(
				context, range, stream,
				detail::CParentStreamSampler(context, GetDataType()));
	}
}

}
