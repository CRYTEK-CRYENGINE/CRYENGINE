// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace pfx2
{

namespace detail
{




class CSelfStreamSampler
{
public:
	CSelfStreamSampler(CParticleComponentRuntime& runtime, TDataType<float> sourceStreamType, EDataDomain domain = EDD_PerParticle)
		: sourceStream(runtime.GetContainer(domain).GetIFStream(sourceStreamType))
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
	CParentStreamSampler(CParticleComponentRuntime& runtime, TDataType<float> sourceStreamType)
		: parentSourceStream(runtime.GetParentContainer().GetIFStream(sourceStreamType, 1.0f))
		, parentIds(runtime.GetContainer().GetIPidStream(EPDT_ParentId))
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
	CParentAgeSampler(CParticleComponentRuntime& runtime)
		: deltaTime(ToFloatv(runtime.DeltaTime()))
		, selfAges(runtime.GetContainer().GetIFStream(EPDT_NormalAge))
		, parentAges(runtime.GetParentContainer().GetIFStream(EPDT_NormalAge))
		, parentInvLifeTimes(runtime.GetParentContainer().GetIFStream(EPDT_InvLifeTime))
		, parentIds(runtime.GetContainer().GetIPidStream(EPDT_ParentId))
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
	CLevelTimeSampler(CParticleComponentRuntime& runtime, EDataDomain domain)
		: deltaTime(ToFloatv(runtime.DeltaTime()))
		, ages(runtime.GetContainer(domain).GetIFStream(EPDT_NormalAge))
		, lifeTimes(runtime.GetContainer(domain).GetIFStream(EPDT_LifeTime))
		, levelTime(ToFloatv(runtime.GetEmitter()->GetTime()))
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
	CSelfSpeedSampler(CParticleComponentRuntime& runtime, EDataDomain domain = EDD_PerParticle)
		: velocities(runtime.GetContainer(domain).GetIVec3Stream(EPVF_Velocity))
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
	CParentSpeedSampler(CParticleComponentRuntime& runtime)
		: parentVelocities(runtime.GetParentContainer().GetIVec3Stream(EPVF_Velocity))
		, parentIds(runtime.GetContainer().GetIPidStream(EPDT_ParentId))
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
	CAttributeSampler(CParticleComponentRuntime& runtime, const CAttributeReference& attr);
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
	CChaosSampler(CParticleComponentRuntime& runtime)
		: m_chaos(runtime.ChaosV()) {}
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
	CViewAngleSampler(CParticleComponentRuntime& runtime, EDataDomain domain = EDD_PerParticle)
		: m_positions(runtime.GetContainer(domain).GetIVec3Stream(EPVF_Position))
		, m_orientations(runtime.GetContainer(domain).GetIQuatStream(EPQF_Orientation))
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
	CCameraDistanceSampler(CParticleComponentRuntime& runtime, EDataDomain domain = EDD_PerParticle)
		: m_positions(runtime.GetContainer(domain).GetIVec3Stream(EPVF_Position))
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

inline CDomain::CDomain()
	: m_domain(EDomain::Age)
	, m_fieldSource(EDomainField(EPDT_LifeTime))
	, m_sourceOwner(EDomainOwner::Self)
	, m_sourceGlobal(EDomainGlobal::LevelTime)
	, m_domainScale(1.0f)
	, m_domainBias(0.0f)
	, m_spawnOnly(true)
{
}

inline void CDomain::AddToParam(CParticleComponent* pComponent)
{
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

inline EDataDomain CDomain::GetDomain() const
{
	EDataDomain update = m_spawnOnly ? EDD_None : EDD_HasUpdate;

	switch (m_domain)
	{
	case EDomain::Global:
	case EDomain::Attribute:
		return update;
	}

	switch (m_sourceOwner)
	{
	case EDomainOwner::Self:
		return EDataDomain(EDD_PerParticle | update);
	case EDomainOwner::Parent:
		return EDataDomain(EDD_PerInstance | update);
	default:
		return update;
	}
}

inline TDataType<float> CDomain::GetDataType() const
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
void CDomain::Dispatch(CParticleComponentRuntime& runtime, const SUpdateRange& range, TStream stream, EDataDomain domain) const
{
	switch (m_domain)
	{
	case EDomain::Global:
		if (m_sourceGlobal == EDomainGlobal::LevelTime)
			((TBase*)this)->DoModify(
				runtime, range, stream,
				detail::CLevelTimeSampler(runtime, domain));
		else
			((TBase*)this)->DoModify(
				runtime, range, stream,
				detail::CConstSampler(GetGlobalValue(m_sourceGlobal)));			
		break;
	case EDomain::Attribute:
		((TBase*)this)->DoModify(
		  runtime, range, stream,
		  detail::CAttributeSampler(runtime, m_attribute));
		break;
	case EDomain::Random:
		((TBase*)this)->DoModify(
		  runtime, range, stream,
		  detail::CChaosSampler(runtime));
		break;
	case EDomain::ViewAngle:
		((TBase*)this)->DoModify(
			runtime, range, stream,
			detail::CViewAngleSampler(runtime, domain));
		break;
	case EDomain::CameraDistance:
		if (!(C3DEngine::GetCVars()->e_ParticlesDebug & AlphaBit('u')) || domain & EDD_PerParticle)
			((TBase*)this)->DoModify(
				runtime, range, stream,
				detail::CCameraDistanceSampler(runtime, domain));
		break;
	case EDomain::Speed:
		if (m_sourceOwner == EDomainOwner::Self || domain & EDD_PerInstance)
			((TBase*)this)->DoModify(
			  runtime, range, stream,
			  detail::CSelfSpeedSampler(runtime, domain));
		else
			((TBase*)this)->DoModify(
			  runtime, range, stream,
			  detail::CParentSpeedSampler(runtime));
		break;
	default:
		if (m_sourceOwner == EDomainOwner::Self || domain & EDD_PerInstance)
			((TBase*)this)->DoModify(
				runtime, range, stream,
				detail::CSelfStreamSampler(runtime, GetDataType(), domain));
		else if (GetDataType() == EPDT_NormalAge)
			((TBase*)this)->DoModify(
				runtime, range, stream,
				detail::CParentAgeSampler(runtime));
		else
			((TBase*)this)->DoModify(
				runtime, range, stream,
				detail::CParentStreamSampler(runtime, GetDataType()));
	}
}

}
