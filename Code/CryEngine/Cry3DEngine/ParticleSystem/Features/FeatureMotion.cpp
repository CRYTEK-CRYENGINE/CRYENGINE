// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureMotion.h"
#include "ParticleSystem/ParticleEmitter.h"
#include <CrySerialization/Math.h>
#include <CrySystem/CryUnitTest.h>

namespace pfx2
{

void ILocalEffector::Serialize(Serialization::IArchive& ar)
{
	ar(m_enabled);
}

//////////////////////////////////////////////////////////////////////////
// CFeatureMotionPhysics

MakeDataType(EPDT_Gravity,       float, EDataFlags::BHasInit);
MakeDataType(EPDT_Drag,          float, EDataFlags::BHasInit);
MakeDataType(EPVF_Acceleration,  Vec3);
MakeDataType(EPVF_VelocityField, Vec3);
MakeDataType(EPVF_PositionPrev,  Vec3);

extern TDataType<IMeshObj*> EPDT_MeshGeometry;

CFeatureMotionPhysics::CFeatureMotionPhysics()
	: m_gravity(0.0f)
	, m_drag(0.0f)
	, m_windMultiplier(1.0f)
	, m_angularDragMultiplier(1.0f)
	, m_uniformAcceleration(ZERO)
	, m_uniformWind(ZERO)
	, m_perParticleForceComputation(true)
{
}

void CFeatureMotionPhysics::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->UpdateParticles.add(this);

	m_gravity.AddToComponent(pComponent, this, EPDT_Gravity);
	m_drag.AddToComponent(pComponent, this, EPDT_Drag);

	m_environFlags = 0;
	if (m_gravity.GetBaseValue() != 0.0f)
	{
		m_environFlags |= ENV_GRAVITY;
		if (m_perParticleForceComputation)
			pComponent->AddParticleData(EPVF_Acceleration);
	}
	if (m_drag.GetBaseValue() * m_windMultiplier != 0.0f)
	{
		m_environFlags |= ENV_WIND;
		if (m_perParticleForceComputation)
			pComponent->AddParticleData(EPVF_VelocityField);
	}
	pComponent->GetEffect()->AddEnvironFlags(m_environFlags);

	auto it = std::remove_if(m_localEffectors.begin(), m_localEffectors.end(), [](const PLocalEffector& ptr) { return !ptr; });
	m_localEffectors.erase(it, m_localEffectors.end());
	m_computeList.clear();
	m_moveList.clear();
	for (PLocalEffector pEffector : m_localEffectors)
	{
		if (pEffector->IsEnabled())
			pEffector->AddToMotionFeature(pComponent, this);
	}

	if (auto pInt = MakeGpuInterface(pComponent, gpu_pfx2::eGpuFeatureType_Motion))
	{
		gpu_pfx2::SFeatureParametersMotionPhysics params;
		params.gravity = m_gravity.GetBaseValue();
		params.drag = m_drag.GetBaseValue();
		params.uniformAcceleration = m_uniformAcceleration;
		params.uniformWind = m_uniformWind;
		params.windMultiplier = m_windMultiplier;
		pInt->SetParameters(params);

		for (PLocalEffector pEffector : m_localEffectors)
		{
			if (pEffector->IsEnabled())
				pEffector->SetParameters(pInt);
		}
	}
}

void CFeatureMotionPhysics::Serialize(Serialization::IArchive& ar)
{
	CParticleFeature::Serialize(ar);
	ar(m_gravity, "gravity", "Gravity Scale");
	ar(m_drag, "drag", "Drag");
	ar(m_windMultiplier, "windMultiplier", "Level Wind Scale");
	ar(m_angularDragMultiplier, "AngularDragMultiplier", "Angular Drag Multiplier");
	ar(m_perParticleForceComputation, "perParticleForceComputation", "Per-Particle Force Computation");
	ar(m_uniformAcceleration, "UniformAcceleration", "Uniform Acceleration");
	ar(m_uniformWind, "UniformWind", "Uniform Wind");
	ar(m_localEffectors, "localEffectors", "Local Effectors");
}

void CFeatureMotionPhysics::InitParticles(const SUpdateContext& context)
{
	m_gravity.InitParticles(context, EPDT_Gravity);
	m_drag.InitParticles(context, EPDT_Drag);
}

void CFeatureMotionPhysics::UpdateParticles(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = context.m_container;
	m_gravity.Update(context, EPDT_Gravity);
	m_drag.Update(context, EPDT_Drag);

	container.CopyData(EPVF_PositionPrev, EPVF_Position, context.m_updateRange);

	if (!m_moveList.empty())
	{
		STempVec3Stream moveBefore(context.m_pMemHeap, context.m_updateRange);
		STempVec3Stream moveAfter(context.m_pMemHeap, context.m_updateRange);
		moveBefore.Clear(context.m_updateRange);
		moveAfter.Clear(context.m_updateRange);

		for (ILocalEffector* pEffector : m_moveList)
			pEffector->ComputeMove(context, moveBefore.GetIOVec3Stream(), 0.0f);
		
		Integrate(context);
		
		for (ILocalEffector* pEffector : m_moveList)
			pEffector->ComputeMove(context, moveAfter.GetIOVec3Stream(), context.m_deltaTime);

		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		for (auto particleGroupId : context.GetUpdateGroupRange())
		{
			const Vec3v position0 = positions.Load(particleGroupId);
			const Vec3v move0 = moveBefore.GetIOVec3Stream().Load(particleGroupId);
			const Vec3v move1 = moveAfter.GetIOVec3Stream().Load(particleGroupId);
			const Vec3v position1 = position0 + (move1 - move0);
			positions.Store(particleGroupId, position1);
		}
	}
	else
	{
		Integrate(context);
	}
}

uint CFeatureMotionPhysics::ComputeEffectors(const SUpdateContext& context, const SArea& area) const
{
	CRY_PFX2_PROFILE_DETAIL;

	// Get area center
	Vec3v center = area.m_vCenter;
	if (!area.m_pEnviron->IsCurrent())
	{
		pe_status_pos spos;
		if (area.m_pArea->GetStatus(&spos))
			center = spos.pos;
	}

	auto hasGravity = area.m_nFlags & ENV_GRAVITY;
	auto hasWind = area.m_nFlags & ENV_WIND;
	assert(hasGravity || hasWind);
	assert(!(hasGravity && hasWind));

	CParticleContainer& container = context.m_container;
	IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	IOVec3Stream fieldStream = container.GetIOVec3Stream(hasGravity ? EPVF_Acceleration : EPVF_VelocityField);
	Vec3v force = hasGravity ? area.m_Forces.vAccel : area.m_Forces.vWind;
	Vec3v forceVec = force * ToFloatv(!area.m_bRadial);
	floatv forceRad = force.z * ToFloatv(area.m_bRadial);
	Matrix33_tpl<floatv> matToLocal = area.m_matToLocal;

	for (auto particleId : context.GetUpdateGroupRange())
	{
		Vec3v posRel = positions.Load(particleId) - center;
		Vec3v posLoc = matToLocal * posRel;
		floatv dist = area.m_nGeomShape == GEOM_BOX ? 
			max(max(abs(posLoc.x), abs(posLoc.y)), abs(posLoc.z)) :
			posLoc.GetLengthFast();
		if (Any(dist < ToFloatv(1)))
		{
			floatv strength = saturate((ToFloatv(1) - dist) * ToFloatv(area.m_fFalloffScale));
			Vec3v particleForce = (forceVec + posRel * forceRad) * strength;
			fieldStream.Store(particleId, fieldStream.Load(particleId) + particleForce);
		}
	}
	return area.m_nFlags;
}

void CFeatureMotionPhysics::AddToComputeList(ILocalEffector* pEffector)
{
	if (std::find(m_computeList.begin(), m_computeList.end(), pEffector) == m_computeList.end())
		m_computeList.push_back(pEffector);
}

void CFeatureMotionPhysics::AddToMoveList(ILocalEffector* pEffector)
{
	if (std::find(m_moveList.begin(), m_moveList.end(), pEffector) == m_moveList.end())
		m_moveList.push_back(pEffector);
}

///////////////////////////////////////////////////////////////////////////////////////////////
/*
	Analytic integration with drag force:
		G == external acceleration
		W == wind
		d == air drag coefficient

		P, V, A == object position, velocity, acceleration

	A = G + (W - V) d
	
	Solution to V(t):
		dV/dt = G + (W - V) d
		V = V0 + A0/d (1 - e^(-d t))
			A0/d == G/d + (W - V0) = VT - V0
			VT == terminal velocity: A = G + (W - G/d - W)) d = 0
	Solution to P(t):
		P(t) = P0 + Int[0,t] V(t) dt
			= P0 + V0 t + A0/d (t + e^(-d t)/d - 1/d)

	To be stable when d = 0, and compute in terms of (d t), rewrite as:
		V(t) = V0 + A0 t (1 - e^(-d t)) / (d t)
		P(t) = P0 + V0 t + A0 t^2 (d t - 1 + e^(-d t) / (d t)^2

	We must stably evaluate the expressions:
		F(x) = (1 - e^(-x)) / x
		G(x) = (x - 1 + e^(-x)) / x^2
		F = 1 - G x

	So, finally:
		V(t) = V0 + A0 t (1 - d t G(d t))
		P(t) = P0 + V0 t + A0 t^2 G(d t)
*/

/*
	Approximate exponential-type function, in a limited range, with a reciprocal function:

	f(x) = b / (x + a) + c, within range 0 to x1.
	Match function at x = 0, x1, and a chosen midpoint xm

	From Wolfram, the solution is:
		a = (x1 xm (y1 - ym)) / (x1 (ym - y0) + xm (y0 - y1))

	b and c are then more accurately computed with a simple linear fit
		b = (y1 - y0) / (/(x1 + a) - /(x0 + a))
			= (y1 -y0) / (/(x1 + a) - /a)
		c = y0 - b/a
*/

template<typename T> T DragVelAdjust(T in) { return in ? -expm1(-in) / in : T(1); }
template<typename T> T DragAccAdjust(T in) { return in ? (expm1(-in) + in) / sqr(in) : T(0.5); }

template<typename T>
void ReciprocalCoeffs(T coeffs[3], f64 y0, f64 xm, f64 ym, f64 x1, f64 y1)
{
	f64 d = xm * (y0 - y1) + x1 * (ym - y0);
	f64 a = d ? xm * x1 * (y1 - ym) / d : 1.0;

	coeffs[0] = convert<T>(a);
	if (coeffs[0])
	{
		T u0 = rcp_fast(coeffs[0]),
		  u1 = rcp_fast(coeffs[0] + convert<T>(x1));
		coeffs[1] = T(u1 != u0 ? (y1 - y0) / (u1 - u0) : T(0));
		coeffs[2] = T(y0 - coeffs[1] * u0);
	}
	else
	{
		coeffs[0] = T(1);
		coeffs[1] = T(0);
		coeffs[2] = T(y0);
	}
}

template<typename T>
void DragAdjustCoeffs(T coeffs[3], f64 x1)
{
	f64 xm = x1 < 4.0 ? x1 * 0.5 : sqrt(x1);
	f64 ym = DragAccAdjust(xm);
	f64 y1 = DragAccAdjust(x1);
	ReciprocalCoeffs(coeffs, 0.5, xm, ym, x1, y1);
}

template<typename T>
ILINE void DragAdjust(T& velAdjust, T& accAdjust, T in, const T coeffs[6])
{
	accAdjust = MAdd(rcp_fast(in + coeffs[0]), coeffs[1], coeffs[2]);
	velAdjust = convert<T>(1.0) - accAdjust * in;
}

CRY_UNIT_TEST(DragFast)
{
	float x1 = 1.0e-12f;
	for (int i = 0; i < 24; ++i, x1 *= 10.f)
	{
		float coeffs[3];
		DragAdjustCoeffs(coeffs, x1);

		float errMax = 0.f;
		for (float x = 0.0f; x <= x1; x += x1 * 0.01f)
		{
			float ve = (float)DragVelAdjust((f64)x);
			float ae = (float)DragAccAdjust((f64)x);

			float va, aa;
			DragAdjust(va, aa, x, coeffs);

			float err = abs(va - ve);
			errMax = max(errMax, err);

			err = abs(aa - ae);
			errMax = max(errMax, err);
		}
		assert(errMax <= 0.01f * max(x1, 1.0f));
	}
}

void CFeatureMotionPhysics::Integrate(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleEmitter* pEmitter = context.m_runtime.GetEmitter();
	CParticleContainer& container = context.m_container;

	IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
	IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
	IVec3Stream velocityField = container.GetIVec3Stream(EPVF_VelocityField);
	IVec3Stream accelerations = container.GetIVec3Stream(EPVF_Acceleration);
	IFStream gravities = container.GetIFStream(EPDT_Gravity, 0.0f);
	IFStream drags = container.GetIFStream(EPDT_Drag);
	IFStream normAges = container.GetIFStream(EPDT_NormalAge);
	IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);
	const floatv deltaTime = ToFloatv(context.m_deltaTime);

	uint effectorFlags = 0;
	for (ILocalEffector* pEffector : m_computeList)
		effectorFlags |= pEffector->ComputeEffector(context, velocityField, accelerations);

	const auto& physEnv = pEmitter->GetPhysicsEnv();
	SPhysForces uniformForces;
	if (m_perParticleForceComputation && !(C3DEngine::GetCVars()->e_ParticlesDebug & AlphaBit('f')))
	{
		// Get per-particle forces for each area
		uniformForces = physEnv.m_UniformForces; // Base forces ignore non-uniform areas
		physEnv.ForNonumiformAreas(context.m_runtime.GetBounds(), m_environFlags, 
			[&](const SArea& area)
			{
				effectorFlags |= ComputeEffectors(context, area); 
			}
		);
	}
	else
	{
		// Get average forces for each area
		physEnv.GetForces(uniformForces, context.m_runtime.GetBounds(), m_environFlags);
	}

	const Vec3v uniformAccel = ToVec3v(m_uniformAcceleration);
	const Vec3v uniformGravity = ToVec3v(uniformForces.vAccel);
	const Vec3v uniformWind = ToVec3v(uniformForces.vWind * m_windMultiplier + m_uniformWind);

	floatv coeffsv[3];
	const float maxDragFactor = m_drag.GetValueRange(context).end * context.m_deltaTime;
	if (maxDragFactor)
	{
		// Approximate e^(-d t) with b/(d t + a) + c
		float coeffs[3];
		DragAdjustCoeffs(coeffs, maxDragFactor);
		for (int i = 0; i < 3; ++i)
			coeffsv[i] = ToFloatv(coeffs[i]);
	}

	const bool isLinear = !(m_environFlags | effectorFlags) && !m_drag.GetBaseValue() && m_uniformAcceleration.IsZero();

	// Integrate positions and velocities
	for (auto particleGroupId : context.GetUpdateGroupRange())
	{
		const floatv dT = DeltaTime(deltaTime, particleGroupId, normAges, lifeTimes);
		const Vec3v p0 = positions.Load(particleGroupId);
		const Vec3v v0 = velocities.Load(particleGroupId);
		
		if (isLinear)
		{
			const Vec3v p1 = MAdd(v0, dT, p0);
			positions.Store(particleGroupId, p1);
			continue;
		}

		const floatv gravMult = gravities.SafeLoad(particleGroupId);
		Vec3v accel = MAdd(uniformGravity, gravMult, uniformAccel);
		if (effectorFlags & ENV_GRAVITY)
			accel += accelerations.Load(particleGroupId);

		Vec3v v1, p1;
		if (maxDragFactor)
		{
			// Fast approximation using acceleration and drag
			const floatv drag = drags.SafeLoad(particleGroupId);
			const floatv dragT = drag * dT;

			Vec3v vWind = uniformWind;
			if (effectorFlags & ENV_WIND)
				vWind += velocityField.Load(particleGroupId);
			accel += (vWind - v0) * drag;

			floatv dv, da; DragAdjust(dv, da, dragT, coeffsv);
			v1 = MAdd(accel, dT * dv, v0);
			p1 = MAdd(v0 + accel * (dT * da), dT, p0);
		}
		else
		{
			// Quadratic integration using acceleration
			v1 = MAdd(accel, dT, v0);
			p1 = MAdd(v0 + v1, dT * ToFloatv(0.5f), p0);
		}

		positions.Store(particleGroupId, p1);
		velocities.Store(particleGroupId, v1);
	}

	// Integrate angles
	const bool spin2D = container.HasData(EPDT_Spin2D) && container.HasData(EPDT_Angle2D);
	const bool spin3D = container.HasData(EPVF_AngularVelocity) && container.HasData(EPQF_Orientation);
	if (spin2D || spin3D)
	{
		if (m_angularDragMultiplier == 0.0f)
			AngularLinearIntegral(context);
		else
			AngularDragFastIntegral(context);
	}
}

void CFeatureMotionPhysics::AngularLinearIntegral(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = context.m_container;
	const IFStream normAges = container.GetIFStream(EPDT_NormalAge);
	const IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);
	const IFStream spins = container.GetIFStream(EPDT_Spin2D);
	const IVec3Stream angularVelocities = container.GetIVec3Stream(EPVF_AngularVelocity);
	const floatv deltaTime = ToFloatv(context.m_deltaTime);
	IOFStream angles = container.GetIOFStream(EPDT_Angle2D);
	IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);

	const bool spin2D = container.HasData(EPDT_Spin2D) && container.HasData(EPDT_Angle2D);
	const bool spin3D = container.HasData(EPVF_AngularVelocity) && container.HasData(EPQF_Orientation);

	for (auto particleGroupId : context.GetUpdateGroupRange())
	{
		const floatv dT = DeltaTime(deltaTime, particleGroupId, normAges, lifeTimes);

		if (spin2D)
		{
			const floatv spin0 = spins.Load(particleGroupId);
			const floatv angle0 = angles.Load(particleGroupId);
			const floatv angle1 = MAdd(spin0, deltaTime, angle0);
			angles.Store(particleGroupId, angle1);
		}

		if (spin3D)
		{
			const Vec3v angularVelocity = angularVelocities.Load(particleGroupId);
			const Quatv orientation0 = orientations.Load(particleGroupId);
			const Quatv orientation1 = AddAngularVelocity(orientation0, angularVelocity, deltaTime);
			orientations.Store(particleGroupId, orientation1);
		}
	}
}

void CFeatureMotionPhysics::AngularDragFastIntegral(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = context.m_container;
	const IFStream normAges = container.GetIFStream(EPDT_NormalAge);
	const IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);
	const IFStream drags = container.GetIFStream(EPDT_Drag);
	const floatv deltaTime = ToFloatv(context.m_deltaTime);
	IOFStream spins = container.GetIOFStream(EPDT_Spin2D);
	IOVec3Stream angularVelocities = container.GetIOVec3Stream(EPVF_AngularVelocity);
	IOFStream angles = container.GetIOFStream(EPDT_Angle2D);
	IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);

	const float maxDragFactor = m_drag.GetValueRange(context).end * context.m_deltaTime;
	const floatv dragReduction = ToFloatv(div_min(1.0f - exp_tpl(-maxDragFactor), maxDragFactor, 1.0f) * m_angularDragMultiplier);

	const bool spin2D = container.HasData(EPDT_Spin2D) && container.HasData(EPDT_Angle2D);
	const bool spin3D = container.HasData(EPVF_AngularVelocity) && container.HasData(EPQF_Orientation);

	for (auto particleGroupId : context.GetUpdateGroupRange())
	{
		const floatv dT = DeltaTime(deltaTime, particleGroupId, normAges, lifeTimes);
		const floatv drag = drags.SafeLoad(particleGroupId) * dragReduction;

		if (spin2D)
		{
			const floatv angle0 = angles.Load(particleGroupId);
			const floatv spin0 = spins.Load(particleGroupId);
			const floatv a = -spin0 * drag;
			const floatv spin1 = MAdd(a, dT, spin0);
			const floatv angle1 = MAdd(spin1, deltaTime, angle0);
			angles.Store(particleGroupId, angle1);
			spins.Store(particleGroupId, spin1);
		}

		if (spin3D)
		{
			const Quatv orientation0 = orientations.Load(particleGroupId);
			const Vec3v angularVelocity0 = angularVelocities.Load(particleGroupId);
			const Vec3v a = -angularVelocity0 * drag;
			const Vec3v angularVelocity1 = MAdd(a, dT, angularVelocity0);
			const Quatv orientation1 = AddAngularVelocity(orientation0, angularVelocity1, deltaTime);
			orientations.Store(particleGroupId, orientation1);
			angularVelocities.Store(particleGroupId, angularVelocity1);
		}
	}
}


CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureMotionPhysics, "Motion", "Physics", colorMotion);

//////////////////////////////////////////////////////////////////////////
// CFeatureMotionCryPhysics

MakeDataType(EPDT_PhysicalEntity, IPhysicalEntity*);

void PopulateSurfaceTypes()
{
	// Populate enum on first serialization call.
	if (!ESurfaceType::count() && gEnv)
	{
		// Trigger surface types loading.
		gEnv->p3DEngine->GetMaterialManager()->GetDefaultLayersMaterial();

		ISurfaceTypeEnumerator* pSurfaceTypeEnum = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator();
		for (ISurfaceType* pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
		{
			int value = pSurfaceType->GetId();
			cstr name = pSurfaceType->GetName();
			if (strlen(name) >= 4 && !strncmp(name, "mat_", 4))
				name += 4;
			ESurfaceType::container().add(value, name, name);
		}

		pSurfaceTypeEnum->Release();
	}
}

CFeatureMotionCryPhysics::CFeatureMotionCryPhysics()
	: m_physicsType(EPhysicsType::Particle)
	, m_gravity(1.0f)
	, m_drag(0.0f)
	, m_density(1.0f)
	, m_thickness(0.0f)
	, m_uniformAcceleration(ZERO)
{
}

void CFeatureMotionCryPhysics::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->PostInitParticles.add(this);
	pComponent->UpdateParticles.add(this);
	pComponent->DestroyParticles.add(this);
	pComponent->AddParticleData(EPDT_PhysicalEntity);
	pComponent->AddParticleData(EPVF_Position);
	pComponent->AddParticleData(EPVF_Velocity);
	pComponent->AddParticleData(EPVF_AngularVelocity);
	pComponent->AddParticleData(EPQF_Orientation);
}

void CFeatureMotionCryPhysics::Serialize(Serialization::IArchive& ar)
{
	PopulateSurfaceTypes();

	CParticleFeature::Serialize(ar);
	ar(m_physicsType, "PhysicsType", "Physics Type");
	ar(m_surfaceType, "SurfaceType", "Surface Type");
	ar(m_gravity, "gravity", "Gravity Scale");
	ar(m_drag, "drag", "Drag");
	ar(m_density, "density", "Density (g/ml)");
	ar(m_thickness, "thickness", "Thickness");
	ar(m_uniformAcceleration, "UniformAcceleration", "Uniform Acceleration");
}

void CFeatureMotionCryPhysics::PostInitParticles(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	IPhysicalWorld* pPhysicalWorld = gEnv->pPhysicalWorld;
	CParticleEmitter* pEmitter = context.m_runtime.GetEmitter();
	const SPhysEnviron& physicsEnv = pEmitter->GetPhysicsEnv();

	CParticleContainer& container = context.m_container;
	auto physicalEntities = container.IOStream(EPDT_PhysicalEntity);
	const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	const IVec3Stream velocities = container.GetIVec3Stream(EPVF_Velocity);
	const IVec3Stream angularVelocities = container.GetIVec3Stream(EPVF_AngularVelocity);
	const IQuatStream orientations = container.GetIQuatStream(EPQF_Orientation);
	const IFStream sizes = container.GetIFStream(EPDT_Size);
	const TIStream<IMeshObj*> meshes = container.IStream(EPDT_MeshGeometry);

	const float sphereVolume = 4.0f / 3.0f * gf_PI;
	const Vec3 acceleration = physicsEnv.m_UniformForces.vAccel * m_gravity + m_uniformAcceleration;
	const int surfaceTypeId = m_surfaceType;

	for (auto particleId : context.GetSpawnedRange())
	{
		// Check if mesh geometry exists
		pe_type physicsType = PE_PARTICLE;
		phys_geometry* pGeom = nullptr;
		if (m_physicsType == EPhysicsType::Mesh)
		{
			if (IMeshObj* pMesh = meshes.SafeLoad(particleId))
			{
				pGeom = pMesh->GetPhysGeom();
			}
			if (!pGeom && context.m_params.m_pMesh)
				pGeom = context.m_params.m_pMesh->GetPhysGeom();
			if (pGeom)
				physicsType = PE_RIGID;
		}

		const float size = sizes.Load(particleId);

		pe_params_pos paramsPos;
		paramsPos.pos = positions.Load(particleId);
		paramsPos.q = orientations.Load(particleId);
		paramsPos.scale = size;

		IPhysicalEntity* pPhysicalEntity = pPhysicalWorld->CreatePhysicalEntity(
			physicsType,
			&paramsPos);
		if (!pPhysicalEntity)
			continue;

		if (physicsType == PE_RIGID)
		{
			// 3D mesh physics
			pe_geomparams paramsGeom;

			paramsGeom.density = m_density;
			paramsGeom.flagsCollider = geom_colltype_debris;
			paramsGeom.flags &= ~geom_colltype_debris; // don't collide with other particles.

			// Override surface index if specified.
			paramsGeom.surface_idx = surfaceTypeId;
			paramsGeom.pMatMapping = non_const(&surfaceTypeId);
			paramsGeom.nMats = 1;

			pPhysicalEntity->AddGeometry(pGeom, &paramsGeom, 0);

			pe_simulation_params paramsSim;
			paramsSim.minEnergy = (0.2f) * (0.2f);
			paramsSim.damping = paramsSim.dampingFreefall = m_drag;
			paramsSim.gravity = paramsSim.gravityFreefall = acceleration;  // Note: currently doesn't work for rigid body

			pPhysicalEntity->SetParams(&paramsSim);
		}
		else
		{
			// Particle (sphere) physics
			pe_params_particle paramsParticle;

			// Compute particle mass from volume of object.
			paramsParticle.mass = m_density * cube(size) * sphereVolume;
			paramsParticle.thickness = m_thickness * size;

			paramsParticle.surface_idx = surfaceTypeId;
			paramsParticle.flags = particle_no_path_alignment;
			paramsParticle.kAirResistance = m_drag;
			paramsParticle.gravity = acceleration;

			pPhysicalEntity->SetParams(&paramsParticle);
		}

		pe_action_set_velocity paramsVel;
		paramsVel.v = velocities.Load(particleId);
		paramsVel.w = angularVelocities.Load(particleId);

		pPhysicalEntity->Action(&paramsVel);

		pe_params_flags paramsFlags;
		paramsFlags.flagsOR = pef_never_affect_triggers | pef_log_collisions;
		pPhysicalEntity->SetParams(&paramsFlags);

		pPhysicalEntity->AddRef();

		physicalEntities.Store(particleId, pPhysicalEntity);
	}
}

void CFeatureMotionCryPhysics::UpdateParticles(const SUpdateContext& context)
{
	CRY_PFX2_PROFILE_DETAIL;

	IPhysicalWorld* pPhysicalWorld = gEnv->pPhysicalWorld;
	CParticleContainer& container = context.m_container;
	auto physicalEntities = container.IOStream(EPDT_PhysicalEntity);
	IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
	IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
	IOVec3Stream angularVelocities = container.GetIOVec3Stream(EPVF_AngularVelocity);
	IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);
	const IFStream ages = container.GetIFStream(EPDT_NormalAge);

	for (auto particleId : context.GetUpdateRange())
	{
		IPhysicalEntity* pPhysicalEntity = physicalEntities.Load(particleId);
		if (!pPhysicalEntity)
			continue;

		pe_status_pos statusPosition;
		if (pPhysicalEntity->GetStatus(&statusPosition))
		{
			positions.Store(particleId, statusPosition.pos);
			orientations.Store(particleId, statusPosition.q);
		}

		pe_status_dynamics statusDynamics;
		if (pPhysicalEntity->GetStatus(&statusDynamics))
		{
			velocities.Store(particleId, statusDynamics.v);
			angularVelocities.Store(particleId, statusDynamics.w);
		}

		if (IsExpired(ages.Load(particleId)))
		{
			pPhysicalWorld->DestroyPhysicalEntity(pPhysicalEntity);
			physicalEntities.Store(particleId, 0);
		}
	}
}

void CFeatureMotionCryPhysics::DestroyParticles(const SUpdateContext& context)
{
	auto physicalEntities = context.m_container.IStream(EPDT_PhysicalEntity);
	for (auto particleId : context.GetUpdateRange())
	{
		if (auto pPhysicalEntity = physicalEntities.Load(particleId))
			gEnv->pPhysicalWorld->DestroyPhysicalEntity(pPhysicalEntity);
	}
}

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureMotionCryPhysics, "Motion", "CryPhysics", colorMotion);

//////////////////////////////////////////////////////////////////////////
// CFeatureMoveRelativeToEmitter

MakeDataType(EPVF_ParentPosition,    Vec3);
MakeDataType(EPQF_ParentOrientation, Quat);

class CFeatureMoveRelativeToEmitter : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

		CFeatureMoveRelativeToEmitter()
		: m_positionInherit(1.0f)
		, m_velocityInherit(1.0f)
		, m_angularInherit(0.0f)
		, m_velocityInheritAfterDeath(0.0f) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		const bool inheritPositions = (m_positionInherit != 0.0f);
		const bool inheritVelocities = (m_velocityInherit != 0.0f);
		const bool inheritAngles = (m_angularInherit != 0.0f);
		const bool inheritVelOnDeath = (m_velocityInheritAfterDeath != 0.0f);

		if (inheritPositions || inheritVelocities || inheritAngles)
		{
			pComponent->PreUpdateParticles.add(this);
			pComponent->AddParticleData(EPQF_ParentOrientation);
		}
		if (inheritPositions)
			pComponent->AddParticleData(EPVF_ParentPosition);
		if (inheritAngles)
			pComponent->AddParticleData(EPQF_Orientation);
		if (inheritVelOnDeath)
			pComponent->UpdateParticles.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_positionInherit, "PositionInherit", "Position Inherit");
		ar(m_velocityInherit, "VelocityInherit", "Velocity Inherit");
		ar(m_angularInherit, "AngularInherit", "Angular Inherit");
		ar(m_velocityInheritAfterDeath, "VelocityInheritAfterDeath", "Velocity Inherit After Death");
	}

	virtual void PreUpdateParticles(const SUpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& container = context.m_container;
		const CParticleContainer& parentContainer = context.m_parentContainer;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IFStream parentAges = parentContainer.GetIFStream(EPDT_NormalAge);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		const IQuatStream parentOrientations = parentContainer.GetIQuatStream(EPQF_Orientation);
		IOVec3Stream parentPrevPositions = container.GetIOVec3Stream(EPVF_ParentPosition);
		IOQuatStream parentPrevOrientations = container.GetIOQuatStream(EPQF_ParentOrientation);
		IOVec3Stream worldPositions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream worldVelocities = container.GetIOVec3Stream(EPVF_Velocity);
		IOQuatStream worldOrientations = container.GetIOQuatStream(EPQF_Orientation);

		const bool inheritPositions = (m_positionInherit != 0.0f);
		const bool inheritVelocities = (m_velocityInherit != 0.0f);
		const bool inheritAngles = (m_angularInherit != 0.0f);

		for (auto particleId : context.GetUpdateRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			if (parentId == gInvalidId)
				continue;

			const Quat parentOrientation = parentOrientations.SafeLoad(parentId);
			const Quat parentPrevOrientation = parentPrevOrientations.Load(particleId);
			const Quat deltaOrientation = parentOrientation * parentPrevOrientation.GetInverted();
			parentPrevOrientations.Store(particleId, parentOrientation);

			if (inheritPositions)
			{
				const Vec3 parentPos = parentPositions.SafeLoad(parentId);
				const Vec3 parentPrevPos = parentPrevPositions.Load(particleId);
				const Vec3 wPosition0 = worldPositions.Load(particleId);
				const Vec3 wPosition1 = Lerp(wPosition0, 
					deltaOrientation * (wPosition0 - parentPrevPos) + parentPos, 
					m_positionInherit);
				worldPositions.Store(particleId, wPosition1);
				parentPrevPositions.Store(particleId, parentPos);
			}

			if (inheritVelocities)
			{
				const Vec3 wVelocity0 = worldVelocities.Load(particleId);
				const Vec3 wVelocity1 = Lerp(
					wVelocity0, deltaOrientation * wVelocity0,
					m_velocityInherit);
				worldVelocities.Store(particleId, wVelocity1);
			}

			if (inheritAngles)
			{
				const Quat wOrientation0 = worldOrientations.Load(particleId);
				const Quat wOrientation1 = Lerp(
					wOrientation0, deltaOrientation * wOrientation0,
					m_angularInherit);
				worldOrientations.Store(particleId, wOrientation1);
			}
		}
	}

	virtual void UpdateParticles(const SUpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& container = context.m_container;
		const CParticleContainer& parentContainer = context.m_parentContainer;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IFStream parentAges = parentContainer.GetIFStream(EPDT_NormalAge);
		const IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);
		IOVec3Stream worldVelocities = container.GetIOVec3Stream(EPVF_Velocity);

		for (auto particleId : context.GetUpdateRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			if (parentId != gInvalidId && IsExpired(parentAges.Load(parentId)))
			{
				const Vec3 wParentVelocity = parentVelocities.Load(parentId);
				const Vec3 wVelocity0 = worldVelocities.Load(particleId);
				const Vec3 wVelocity1 = wParentVelocity * m_velocityInheritAfterDeath + wVelocity0;
				worldVelocities.Store(particleId, wVelocity1);
			}
		}
	}

private:
	SFloat m_positionInherit;
	SFloat m_velocityInherit;
	SFloat m_angularInherit;
	SFloat m_velocityInheritAfterDeath;
};

CRY_PFX2_LEGACY_FEATURE(CFeatureMoveRelativeToEmitter, "Velocity", "MoveRelativeToEmitter");
CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureMoveRelativeToEmitter, "Motion", "MoveRelativeToEmitter", colorMotion);

}
