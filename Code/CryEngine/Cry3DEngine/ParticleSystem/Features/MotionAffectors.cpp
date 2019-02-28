// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureMotion.h"
#include "Target.h"
#include <CryMath/SNoise.h>
#include <CrySerialization/Math.h>

CRY_PFX2_DBG

namespace pfx2
{

SERIALIZATION_DECLARE_ENUM(ETurbulenceMode,
	Brownian,
	Simplex,
	SimplexCurl
	)

class CEffectorTurbulence : public ILocalEffector
{
private:
	typedef TValue<uint, THardLimits<1, 6>> UIntOctaves;

public:
	CEffectorTurbulence()
		: m_scale(1.0f, 1.0f, 1.0f)
		, m_speed(1.0f)
		, m_size(1.0f)
		, m_rate(0.0f)
		, m_octaves(1)
		, m_mode(ETurbulenceMode::Brownian) {}

	virtual void AddToMotionFeature(CParticleComponent* pComponent, IFeatureMotion* pFeature) override
	{
		pFeature->AddToComputeList(this);
		pComponent->AddParticleData(EPVF_Position);
		if (IsSimplex())
			pComponent->AddParticleData(EPVF_VelocityField);
		else
			pComponent->AddParticleData(EPVF_Acceleration);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ILocalEffector::Serialize(ar);
		const bool isUI = (ar.isEdit() && ar.isOutput());

		ar(m_mode, "mode", "Mode");

		ar(m_speed, "speed", "Speed");
		if (!isUI || IsSimplex())
		{
			ar(m_size, "size", "Size");
			ar(m_rate, "rate", "Rate");
			ar(m_octaves, "octaves", "Octaves");
		}
		ar(m_scale, "scale", "Scale");
	}

	virtual void SetParameters(gpu_pfx2::IParticleFeature* gpuInterface) const override
	{

		switch (m_mode)
		{
		case ETurbulenceMode::Brownian:
		{
			gpu_pfx2::SFeatureParametersMotionPhysicsBrownian params;
			params.scale = m_scale;
			params.speed = m_speed;
			gpuInterface->SetParameters(params);
		}
		break;
		case ETurbulenceMode::Simplex:
		{
			gpu_pfx2::SFeatureParametersMotionPhysicsSimplex params;
			params.scale = m_scale;
			params.speed = m_speed;
			params.size = m_size;
			params.rate = m_rate;
			params.octaves = m_octaves;
			gpuInterface->SetParameters(params);
		}
		break;
		case ETurbulenceMode::SimplexCurl:
		{
			gpu_pfx2::SFeatureParametersMotionPhysicsCurl params;
			params.scale = m_scale;
			params.speed = m_speed;
			params.size = m_size;
			params.rate = m_rate;
			params.octaves = m_octaves;
			gpuInterface->SetParameters(params);
		}
		break;
		default:
			CRY_ASSERT(0);
		}
	}

	virtual uint ComputeEffector(CParticleComponentRuntime& runtime, IOVec3Stream localVelocities, IOVec3Stream localAccelerations) override
	{
		switch (m_mode)
		{
		case ETurbulenceMode::Brownian:
			Brownian(runtime, localAccelerations);
			return ENV_GRAVITY;
		case ETurbulenceMode::Simplex:
			ComputeSimplex(runtime, localVelocities, &Potential);
			return ENV_WIND;
		case ETurbulenceMode::SimplexCurl:
			ComputeSimplex(runtime, localVelocities, &Curl);
			return ENV_WIND;
		}
		return 0;
	}

private:
	void Brownian(CParticleComponentRuntime& runtime, IOVec3Stream localAccelerations)
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const float time = max(1.0f / 1024.0f, runtime.DeltaTime());
		const floatv speed = ToFloatv(m_speed * isqrt_tpl(time));

		for (auto particleGroupId : runtime.FullRangeV())
		{
			const Vec3v position = positions.Load(particleGroupId);
			const Vec3v accel0 = localAccelerations.Load(particleGroupId);
			const floatv keyX = runtime.ChaosV().RandSNorm();
			const floatv keyY = runtime.ChaosV().RandSNorm();
			const floatv keyZ = runtime.ChaosV().RandSNorm();
			const Vec3v accel1 = MAdd(Vec3v(keyX, keyY, keyZ), speed, accel0);
			localAccelerations.Store(particleGroupId, accel1);
		}
	}

	template<typename FieldFn>
	void ComputeSimplex(CParticleComponentRuntime& runtime, IOVec3Stream localVelocities, FieldFn fieldFn)
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& container = runtime.GetContainer();
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		const float maxSize = (float)(1 << 12);
		const float minSize = rcp_fast(maxSize); // small enough and prevents SIMD exceptions
		const floatv time = ToFloatv(fmodf(runtime.GetEmitter()->GetTime() * m_rate * minSize, 1.0f) * maxSize);
		const floatv invSize = ToFloatv(rcp_fast(std::max(minSize, float(m_size))));
		const floatv speed = ToFloatv(m_speed);
		const floatv delta = ToFloatv(m_rate * runtime.DeltaTime());
		const uint octaves = m_octaves;
		const Vec3v scale = ToVec3v(m_scale);
		const IFStream ages = container.GetIFStream(EPDT_NormalAge);
		const IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);

		for (auto particleGroupId : runtime.FullRangeV())
		{
			const floatv age = ages.Load(particleGroupId);
			const floatv lifeTime = lifeTimes.Load(particleGroupId);
			const Vec3v position = positions.Load(particleGroupId);
			const Vec3v velocity0 = localVelocities.Load(particleGroupId);

			Vec4v sample;
			sample.x = Mul(position.x, invSize);
			sample.y = Mul(position.y, invSize);
			sample.z = Mul(position.z, invSize);
			sample.w = StartTime(time, delta, age * lifeTime);

			Vec3v fieldSample = Fractal(sample, octaves, fieldFn);
			fieldSample.x *= scale.x;
			fieldSample.y *= scale.y;
			fieldSample.z *= scale.z;
			const Vec3v velocity1 = MAdd(fieldSample, speed, velocity0);
			localVelocities.Store(particleGroupId, velocity1);
		}
	}

	ILINE static Vec3v Potential(const Vec4v sample)
	{
		const Vec4v offy = ToVec4v(Vec4(149, 311, 191, 491));
		const Vec4v offz = ToVec4v(Vec4(233, 197, 43, 59));
		const Vec3v potential = Vec3v(
			SNoise(sample),
			SNoise(Add(sample, offy)),
			SNoise(Add(sample, offz)));
		return potential;
	}

	ILINE static Vec3v Curl(const Vec4v sample)
	{
		Vec4v gradX, gradY, gradZ;
		const Vec4v offy = ToVec4v(Vec4(149, 311, 191, 491));
		const Vec4v offz = ToVec4v(Vec4(233, 197, 43, 59));
		SNoise(sample, &gradX);
		SNoise(Add(sample, offy), &gradY);
		SNoise(Add(sample, offz), &gradZ);

		Vec3v curl;
		curl.x = Sub(gradY.z, gradZ.y);
		curl.y = Sub(gradZ.x, gradX.z);
		curl.z = Sub(gradX.y, gradY.x);

		return curl;
	}

	template<typename FieldFn>
	ILINE static Vec3v Fractal(const Vec4v sample, const uint octaves, FieldFn fieldFn)
	{
		Vec3v total = ToVec3v(Vec3(ZERO));
		floatv mult = ToFloatv(1.0f);
		floatv totalMult = ToFloatv(0.0f);
		for (uint i = 0; i < octaves; ++i)
		{
			totalMult = Add(mult, totalMult);
			mult = Mul(ToFloatv(0.5f), mult);
		}
		floatv size = ToFloatv(1.0f);
		mult = rcp_fast(totalMult);
		for (uint i = 0; i < octaves; ++i)
		{
			total = MAdd(fieldFn(sample*size), mult, total);
			mult *= ToFloatv(0.5f);
			size *= ToFloatv(2.0f);
		}
		return total;
	}

	ILINE bool IsSimplex() const
	{
		return (m_mode == ETurbulenceMode::Simplex) || (m_mode == ETurbulenceMode::SimplexCurl);
	}

	Vec3            m_scale;
	UFloat10        m_speed;
	UFloat10        m_size;
	UFloat10        m_rate;
	UIntOctaves     m_octaves;
	ETurbulenceMode m_mode;
};

SERIALIZATION_CLASS_NAME(ILocalEffector, CEffectorTurbulence, "Turbulence", "Turbulence");

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(EGravityType,
	Spherical,
	Cylindrical
	)

class CEffectorGravity : public ILocalEffector
{
public:
	CEffectorGravity()
		: m_acceleration(1.0f)
		, m_decay(10.0f)
		, m_type(EGravityType::Spherical)
		, m_axis(0.0f, 0.0f, 1.0f) {}

	virtual void AddToMotionFeature(CParticleComponent* pComponent, IFeatureMotion* pFeature) override
	{
		pFeature->AddToComputeList(this);
		pComponent->AddParticleData(EPVF_Position);
		pComponent->AddParticleData(EPVF_Acceleration);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ILocalEffector::Serialize(ar);
		ar(m_targetSource, "Target", "Target");
		ar(m_type, "Type", "Type");
		ar(m_acceleration, "Acceleration", "Acceleration");
		ar(m_decay, "Decay", "Decay");
		if (m_type == EGravityType::Cylindrical)
			ar(m_axis, "Axis", "Axis");
	}

	virtual void SetParameters(gpu_pfx2::IParticleFeature* gpuInterface) const override
	{
		gpu_pfx2::SFeatureParametersMotionPhysicsGravity params;
		params.gravityType =
			(
				(m_type == EGravityType::Cylindrical) ?
				gpu_pfx2::eGravityType_Cylindrical :
				gpu_pfx2::eGravityType_Spherical
				);
		params.acceleration = m_acceleration;
		params.decay = m_decay;
		params.axis = m_axis;
		gpuInterface->SetParameters(params);
	}

	virtual uint ComputeEffector(CParticleComponentRuntime& runtime, IOVec3Stream localVelocities, IOVec3Stream localAccelerations) override
	{
		switch (m_type)
		{
		case EGravityType::Spherical:
			ComputeGravity<false>(runtime, localAccelerations);
			return ENV_GRAVITY;
		case EGravityType::Cylindrical:
			ComputeGravity<true>(runtime, localAccelerations);
			return ENV_GRAVITY;
		}
		return 0;
	}

private:
	template<const bool useAxis>
	ILINE void ComputeGravity(CParticleComponentRuntime& runtime, IOVec3Stream localAccelerations)
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CParticleContainer& container = runtime.GetContainer();
		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		// m_decay is actually the distance at which gravity is halved.
		const float decay = rcp_fast(m_decay * m_decay);

		for (auto particleId : runtime.FullRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			if (parentId != gInvalidId)
			{
				const Vec3 position = positions.Load(particleId);
				const Vec3 targetPosition = m_targetSource.GetTarget(runtime, particleId);
				const Vec3 accel0 = localAccelerations.Load(particleId);

				Vec3 accelVec;
				if (useAxis)
				{
					const Quat wQuat = parentQuats.SafeLoad(parentId);
					const Vec3 axis = wQuat * m_axis;
					accelVec = (targetPosition + axis * axis.Dot(position - targetPosition)) - position;
				}
				else
				{
					accelVec = targetPosition - position;
				}

				const float distanceSqr = accelVec.GetLengthSquared();
				const float gravity = rcp_fast(1.0f + decay * distanceSqr) * m_acceleration;
				const Vec3 accel1 = accel0 + accelVec.GetNormalized() * gravity;
				localAccelerations.Store(particleId, accel1);
			}
		}
	}

	CTargetSource m_targetSource;
	Vec3          m_axis;
	SFloat10      m_acceleration;
	UFloat100     m_decay;
	EGravityType  m_type;
};

SERIALIZATION_CLASS_NAME(ILocalEffector, CEffectorGravity, "Gravity", "Gravity");

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(EVortexDirection,
	ClockWise,
	CounterClockWise
	)

class CEffectorVortex : public ILocalEffector
{
public:
	CEffectorVortex()
		: m_speed(1.0f)
		, m_decay(10.0f)
		, m_direction(EVortexDirection::ClockWise)
		, m_axis(0.0f, 0.0f, 1.0f) {}

	virtual void AddToMotionFeature(CParticleComponent* pComponent, IFeatureMotion* pFeature) override
	{
		pFeature->AddToComputeList(this);
		pComponent->AddParticleData(EPVF_Position);
		pComponent->AddParticleData(EPVF_VelocityField);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ILocalEffector::Serialize(ar);
		ar(m_targetSource, "Target", "Target");
		ar(m_speed, "Speed", "Speed");
		ar(m_decay, "Decay", "Decay");
		ar(m_direction, "Direction", "Direction");
		ar(m_axis, "Axis", "Axis");
	}

	virtual void SetParameters(gpu_pfx2::IParticleFeature* gpuInterface) const override
	{
		gpu_pfx2::SFeatureParametersMotionPhysicsVortex params;
		params.vortexDirection =
			(
				(m_direction == EVortexDirection::ClockWise) ?
				gpu_pfx2::eVortexDirection_Clockwise :
				gpu_pfx2::eVortexDirection_CounterClockwise
				);
		params.speed = m_speed;
		params.decay = m_decay;
		params.axis = m_axis;
		gpuInterface->SetParameters(params);
	}

	virtual uint ComputeEffector(CParticleComponentRuntime& runtime, IOVec3Stream localVelocities, IOVec3Stream localAccelerations) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CParticleContainer& container = runtime.GetContainer();
		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		// m_decay is actually the distance at which gravity is halved.
		const float decay = rcp_fast(m_decay * m_decay);
		const float speed = m_speed * (m_direction == EVortexDirection::ClockWise ? -1.0f : 1.0f);

		for (auto particleId : runtime.FullRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			if (parentId != gInvalidId)
			{
				const Vec3 position = positions.Load(particleId);
				const Vec3 targetPosition = m_targetSource.GetTarget(runtime, particleId);
				const Quat wQuat = parentQuats.SafeLoad(parentId);
				const Vec3 velocity0 = localVelocities.Load(particleId);

				const Vec3 axis = wQuat * m_axis;
				const Vec3 toAxis = (targetPosition + axis * axis.Dot(position - targetPosition)) - position;
				const float distanceSqr = toAxis.GetLengthSquared();
				const float vortexSpeed = rcp_fast(1.0f + decay * distanceSqr) * speed;
				const Vec3 velocity1 = velocity0 + toAxis.GetNormalized().Cross(axis) * vortexSpeed;
				localVelocities.Store(particleId, velocity1);
			}
		}
		return ENV_WIND;
	}

private:
	CTargetSource    m_targetSource;
	Vec3             m_axis;
	UFloat10         m_speed;
	UFloat100        m_decay;
	EVortexDirection m_direction;
};

SERIALIZATION_CLASS_NAME(ILocalEffector, CEffectorVortex, "Vortex", "Vortex");

//////////////////////////////////////////////////////////////////////////

class CEffectorSpiral : public ILocalEffector
{
public:
	CEffectorSpiral()
		: m_size(1.0f)
		, m_speed(1.0f) {}

	virtual void AddToMotionFeature(CParticleComponent* pComponent, IFeatureMotion* pFeature) override
	{
		pFeature->AddToMoveList(this);
		pComponent->AddParticleData(EPVF_Position);
		pComponent->AddParticleData(EPVF_Acceleration);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ILocalEffector::Serialize(ar);
		ar(m_size, "Size", "Size");
		ar(m_speed, "Speed", "Speed");
	}

	virtual void ComputeMove(CParticleComponentRuntime& runtime, IOVec3Stream localMoves, float fTime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream velocities = container.GetIVec3Stream(EPVF_Velocity);
		const IFStream normAges = container.GetIFStream(EPDT_NormalAge);
		const IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);
		const float speed = m_speed.Get();
		const float size = m_size;

		for (auto particleId : runtime.FullRange())
		{
			const Vec3 velocity = velocities.Load(particleId);
			const float age = normAges.Load(particleId) * lifeTimes.Load(particleId);
			const float angle = speed * (age + fTime);
			const float rotateSin = sin_tpl(angle) * size;
			const float rotateCos = cos_tpl(angle) * size;
			const Vec3 xAxis = velocity.GetOrthogonal().GetNormalized();
			const Vec3 yAxis = xAxis.Cross(velocity).GetNormalized();
			const Vec3 move = xAxis * rotateCos + yAxis * rotateSin;
			localMoves.Store(particleId, move);
		}
	}

private:
	UFloat10 m_size;
	SAngle   m_speed;
};

SERIALIZATION_CLASS_NAME(ILocalEffector, CEffectorSpiral, "Spiral", "Spiral");

}
