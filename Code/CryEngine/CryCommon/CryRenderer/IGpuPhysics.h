// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef INTERFACE_GPU_PHYSICS
#define INTERFACE_GPU_PHYSICS

namespace gpu_physics
{

enum class EParameterType
{
	Base, Global, ParticleFluid, GridFluid
};

enum ESimulationType
{
	eSimulationType_Base,
	eSimulationType_ParticleFluid,
	eSimulationType_AMOUNT
};

enum class EBodyType
{
	Base, Fluid, Ballstic, FluidParticle
};

struct SBodyBase
{
	static const EBodyType bodyType = EBodyType::Base;
	Vec3                   x;
	Vec3                   v;
	Vec3                   xp;
};

struct SFluidBody : SBodyBase
{
	static const EBodyType bodyType = EBodyType::Fluid;
	float                  density;
	float                  densityPredicted;
	float                  alpha;
	Vec3                   vorticity;
	int                    phase;
	float                  lifeTime;
};

struct SFluidParticle : SBodyBase
{
	static const EBodyType bodyType = EBodyType::FluidParticle;
	float                  lifeTime;
};

struct SParameterBase
{
	static const EParameterType parameterType = EParameterType::Base;
};

struct SGlobalParameters : SParameterBase
{
	static const EParameterType parameterType = EParameterType::Global;
};

struct SParticleFluidParameters : SParameterBase
{
	static const EParameterType parameterType = EParameterType::ParticleFluid;
	float                       deltaTime;
	float                       stiffness;
	float                       gravityConstant;
	float                       h;
	float                       r0;
	float                       mass;
	float                       maxVelocity;
	float                       maxForce;
	float                       atmosphericDrag;
	float                       cohesion;
	float                       baroclinity;
	float                       worldOffsetX;
	float                       worldOffsetY;
	float                       worldOffsetZ;
	float                       particleInfluence;
	float                       pad;
	int                         numberOfIterations;
	int                         gridSizeX;
	int                         gridSizeY;
	int                         gridSizeZ;
};

struct SGridFluidParameters : SParameterBase
{
	static const EParameterType parameterType = EParameterType::GridFluid;
	float                       deltaTime;
	float                       cellSize;
	float                       worldOffsetX;
	float                       worldOffsetY;
	float                       worldOffsetZ;
	int                         gridSizeX;
	int                         gridSizeY;
	int                         gridSizeZ;
};

/* an instance of an on-GPU simulation */
class ISimulationInstance : public _reference_target_t
{
public:
	template<typename Parameter>
	void SetParameters(const Parameter* p)
	{
		InternalSetParameters(Parameter::parameterType, p);
	}
	virtual ~ISimulationInstance() {}

	virtual void RenderThreadUpdate(CDeviceCommandListRef RESTRICT_REFERENCE commandList) = 0;

	template<typename Body>
	void InjectBodies(const Body* b, const int numBodies)
	{
		InternalInjectBodies(Body::bodyType, b, numBodies);
	}
protected:
	virtual void InternalSetParameters(const EParameterType type, const SParameterBase* p) = 0;
	virtual void InternalInjectBodies(const EBodyType type, const SBodyBase* b, const int numBodies) = 0;
};
}

#endif
