// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace pfx2
{

namespace detail
{

///////////////////////////////////////////////////////////////////////////////
// Sources
struct SelfSource
{
	SelfSource(const CParticleComponentRuntime& runtime) {}
	static const CParticleContainer& Container(const CParticleComponentRuntime& runtime)
	{
		return runtime.Container();
	}
	TParticleGroupId Id(TParticleGroupId id) const
	{
		return id;
	}
};

struct ParticleParentSource
{
	ParticleParentSource(const CParticleComponentRuntime& runtime)
		: parentIds(runtime.IStream(EPDT_ParentId)) {}
	static const CParticleContainer& Container(const CParticleComponentRuntime& runtime)
	{
		return runtime.ParentContainer();
	}
	TParticleIdv Id(TParticleGroupId id) const
	{
		return parentIds.Load(id);
	}

private:
	IPidStream parentIds;
};

struct SpawnerParentSource
{
	SpawnerParentSource(const CParticleComponentRuntime& runtime)
		: parentIds(runtime.IStream(ESDT_ParentId)) {}
	static const CParticleContainer& Container(const CParticleComponentRuntime& runtime)
	{
		return runtime.ParentContainer();
	}
	TParticleIdv Id(TParticleGroupId id) const
	{
	#ifdef CRY_PFX2_USE_SSE
		return convert<TParticleIdv>(ParentId(+id), ParentId(+id + 1), ParentId(+id + 2), ParentId(+id + 3));
	#else
		return ParentId(+id);
	#endif
	}

private:
	IPidStream parentIds;

	TParticleId ParentId(uint spawnerId) const
	{
		return parentIds.SafeLoad(spawnerId);
	}
};


///////////////////////////////////////////////////////////////////////////////
// Samplers

struct Stream
{
	template<typename Source>
	struct Sampler : Source
	{
		Sampler(const CParticleComponentRuntime& runtime, TDataType<float> sourceStreamType)
			: Source(runtime)
			, sourceStream(Source::Container(runtime).IStream(sourceStreamType))
		{}
		ILINE floatv Sample(TParticleGroupId particleId) const
		{
			return sourceStream.SafeLoad(Source::Id(particleId));
		}
	private:
		IFStream sourceStream;
	};
};

class CParentAgeSampler
{
public:
	CParentAgeSampler(const CParticleComponentRuntime& runtime)
		: deltaTime(ToFloatv(runtime.DeltaTime()))
		, selfAges(runtime.IStream(EPDT_NormalAge))
		, parentAges(runtime.ParentContainer().IStream(EPDT_NormalAge))
		, parentInvLifeTimes(runtime.ParentContainer().IStream(EPDT_InvLifeTime))
		, parentIds(runtime.IStream(EPDT_ParentId))
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

struct LevelTime
{
	template<typename Source>
	struct Sampler: Source
	{
		Sampler(const CParticleComponentRuntime& runtime)
			: Source(runtime)
			, deltaTime(ToFloatv(runtime.DeltaTime()))
			, ages(Source::Container(runtime).GetIFStream(EPDT_NormalAge))
			, lifeTimes(Source::Container(runtime).GetIFStream(EPDT_LifeTime))
			, levelTime(ToFloatv(runtime.GetEmitter()->GetTime()))
		{}
		ILINE floatv Sample(TParticleGroupId particleId) const
		{
			const auto id = Source::Id(particleId);
			return StartTime(levelTime, deltaTime, ages.SafeLoad(id) * lifeTimes.SafeLoad(id));
		}

	private:
		IFStream ages;
		IFStream lifeTimes;
		floatv   levelTime;
		floatv   deltaTime;
	};
};

struct Speed
{
	template<typename Source>
	struct Sampler: Source
	{
		Sampler(const CParticleComponentRuntime& runtime)
			: Source(runtime)
			, velocities(Source::Container(runtime).IStream(EPVF_Velocity))
		{}
		ILINE floatv Sample(TParticleGroupId particleId) const
		{
			const Vec3v velocity = velocities.SafeLoad(Source::Id(particleId));
			return velocity.GetLength();
		}
	private:
		IVec3Stream velocities;
	};
};

class CAttributeSampler
{
public:
	CAttributeSampler(const CParticleComponentRuntime& runtime, const CAttributeReference& attr);
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

class ParticleIdSampler
{
public:
	ParticleIdSampler(uint count)
		: m_scale(convert<floatv>(1.0f / (count - 1))) {}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		float index = float(+particleId);
	#ifdef CRY_PFX2_USE_SSE
		return convert<floatv>(index, index + 1.0f, index + 2.0f, index + 3.0f) * m_scale;
	#else
		return index * m_scale;
	#endif
	}
private:
	floatv m_scale;
};

class CChaosSampler
{
public:
	CChaosSampler(const CParticleComponentRuntime& runtime)
		: m_chaos(runtime.ChaosV()) {}
	ILINE floatv Sample(TParticleGroupId particleId) const
	{
		return m_chaos.RandUNorm();
	}
private:
	SChaosKeyV& m_chaos;
};

namespace NSpawnId
{
	struct SamplerBase
	{
		SamplerBase(uint modulus)
			: m_modulus(convert<uint32v>(modulus))
			, m_scale(convert<floatv>(rcp(float(modulus))))
		{
		}
		ILINE floatv Convert(uint32v spawnId) const
		{
			const uint32v mod = spawnId % m_modulus;
			return convert<floatv>(mod) * m_scale;
		}
	private:
		uint32v m_modulus;
		floatv  m_scale;
	};

	template<typename Source>
	struct Sampler: Source, SamplerBase
	{
		Sampler(const CParticleComponentRuntime& runtime, uint modulus)
			: Source(runtime)
			, SamplerBase(modulus)
			, m_spawnIds(Source::Container(runtime).IStream(EPDT_SpawnId))
		{}
		ILINE floatv Sample(TParticleGroupId particleId) const
		{
			const TParticleIdv spawnId = m_spawnIds.SafeLoad(Source::Id(particleId));
			return Convert(spawnId);
		}
	private:
		IPidStream m_spawnIds;
	};

	template<>
	struct Sampler<SelfSource>: SelfSource, SamplerBase
	{
		Sampler(const CParticleComponentRuntime& runtime, uint modulus)
			: SelfSource(runtime)
			, SamplerBase(modulus)
		{
			uint32 idOffset = SelfSource::Container(runtime).GetSpawnIdOffset();
			idOffset %= modulus;
		#ifdef CRY_PFX2_USE_SSE
			m_idOffsets = convert<uint32v>(idOffset) + convert<uint32v>(0, 1, 2, 3);
		#else
			m_idOffsets = idOffset;
		#endif
		}
		ILINE floatv Sample(TParticleGroupId particleId) const
		{
			const uint32v spawnId = convert<uint32v>(+SelfSource::Id(particleId)) + m_idOffsets;
			return Convert(spawnId);
		}
	private:
		uint32v m_idOffsets;
	};
};

struct SpawnId
{
	// Hack because specializing a struct is not allowed in class scope.
	template<typename Source> using Sampler = NSpawnId::Sampler<Source>;
};


struct ViewAngle
{
	template<typename Source>
	struct Sampler: Source
	{
		Sampler(const CParticleComponentRuntime& runtime)
			: Source(runtime)
			, m_positions(Source::Container(runtime).GetIVec3Stream(EPVF_Position))
			, m_orientations(Source::Container(runtime).GetIQuatStream(EPQF_Orientation))
			, m_cameraPosition(ToVec3v(gEnv->p3DEngine->GetRenderingCamera().GetPosition())) {}
		ILINE floatv Sample(TParticleGroupId particleId) const
		{
			const auto id = Source::Id(particleId);
			const Vec3v position = m_positions.SafeLoad(id);
			const Quatv orientation = m_orientations.SafeLoad(id);
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
};

struct CameraDistance
{
	template<typename Source>
	struct Sampler: Source
	{
		Sampler(const CParticleComponentRuntime& runtime)
			: Source(runtime)
			, m_positions(Source::Container(runtime).GetIVec3Stream(EPVF_Position))
			, m_cameraPosition(ToVec3v(gEnv->p3DEngine->GetRenderingCamera().GetPosition())) {}
		ILINE floatv Sample(TParticleGroupId particleId) const
		{
			const Vec3v position = m_positions.SafeLoad(Source::Id(particleId));
			const floatv distance = position.GetDistance(m_cameraPosition);
			return distance;
		}
	private:
		IVec3Stream m_positions;
		Vec3v       m_cameraPosition;
	};
};

}

///////////////////////////////////////////////////////////////////////////////

template<typename Child, typename T, typename TFrom>
template<typename S, typename ...Args>
void TModFunction<Child, T, TFrom>::ModifyFromSampler(const CParticleComponentRuntime& runtime, const SUpdateRange& range, TIOStream<T> stream, EDataDomain domain, Args ...args) const
{
	if (domain & EDD_Spawner)
		Child().DoModify(
			runtime, range, stream,
			typename S::template Sampler<detail::SpawnerParentSource>(runtime, args...));
	else if (m_sourceOwner == EDomainOwner::Parent)
		Child().DoModify(
			runtime, range, stream,
			typename S::template Sampler<detail::ParticleParentSource>(runtime, args...));
	else
		Child().DoModify(
			runtime, range, stream,
			typename S::template Sampler<detail::SelfSource>(runtime, args...));
}

template<typename Child, typename T, typename TFrom>
void TModFunction<Child, T, TFrom>::Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, TIOStream<T> stream, EDataDomain domain) const
{
	switch (m_domain)
	{
	case EDomain::Global:
		if (m_sourceGlobal == EDomainGlobal::LevelTime)
			ModifyFromSampler<detail::LevelTime>(runtime, range, stream, domain);
		else
			Child().DoModify(
				runtime, range, stream,
				detail::CConstSampler(GetGlobalValue(m_sourceGlobal)));			
		break;
	case EDomain::Attribute:
		Child().DoModify(
		  runtime, range, stream,
		  detail::CAttributeSampler(runtime, m_attribute));
		break;
	case EDomain::Random:
		Child().DoModify(
		  runtime, range, stream,
		  detail::CChaosSampler(runtime));
		break;
	case EDomain::SpawnId:
		ModifyFromSampler<detail::SpawnId>(runtime, range, stream, domain, m_idModulus);
		break;
	case EDomain::ViewAngle:
		ModifyFromSampler<detail::ViewAngle>(runtime, range, stream, domain);
		break;
	case EDomain::CameraDistance:
		ModifyFromSampler<detail::CameraDistance>(runtime, range, stream, domain);
		break;
	case EDomain::Speed:
		ModifyFromSampler<detail::Speed>(runtime, range, stream, domain);
		break;
	case EDomain::Age:
		if (domain & EDD_Particle && m_sourceOwner == EDomainOwner::Parent)
		{
			Child().DoModify(
				runtime, range, stream,
				detail::CParentAgeSampler(runtime));
			break;
		}
	default:
		ModifyFromSampler<detail::Stream>(runtime, range, stream, domain, GetDataType());
	}
}

template<typename Child, typename T, typename TFrom>
void TModFunction<Child, T, TFrom>::Sample(TVarArray<TFrom> samples) const
{
	if (samples.empty() || m_domain != EDomain::Age && m_sourceOwner != EDomainOwner::Self)
		return;

	static const CParticleComponentRuntime* s_pNone = nullptr;

	Child().DoModify(
		*s_pNone,
		SUpdateRange(0, samples.size()),
		TIOStream<TFrom>(samples.data()),
		detail::ParticleIdSampler(samples.size()));
}


}
