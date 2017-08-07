// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryParticleSystem/IParticlesPfx2.h>
#include <CryRenderer/RenderElements/CREParticle.h>

namespace gpu_pfx2
{

class IParticleComponentRuntime;

#define LIST_OF_FEATURE_TYPES \
  X(Dummy)                    \
  X(SpawnCount)               \
  X(SpawnRate)                \
  X(Motion)                   \
  X(RenderGpu)                \
  X(Color)                    \
  X(VelocityCone)             \
  X(VelocityDirectional)      \
  X(VelocityOmniDirectional)  \
  X(VelocityInherit)          \
  X(FieldSize)                \
  X(FieldPixelSize)           \
  X(FieldOpacity)             \
  X(LifeTime)                 \
  X(Collision)                \
  X(LocationOffset)           \
  X(LocationBox)              \
  X(LocationSphere)           \
  X(LocationCircle)           \
  X(LocationNoise)            \
  X(MotionFluidDynamics)

enum EGpuFeatureType
{
	eGpuFeatureType_None,
#define X(name) eGpuFeatureType_ ## name,
	LIST_OF_FEATURE_TYPES
#undef X
	eGpuFeatureType_COUNT
};

enum class ESortMode
{
    None,
    BackToFront,
    FrontToBack,
    OldToNew,
    NewToOld
};

enum class EFacingMode
{
	Screen,
	Velocity
};

struct SComponentParams
{
	SComponentParams()
		: usesGpuImplementation(false), maxParticles(0), maxNewBorns(0), sortMode(ESortMode::None), version(0) {}
	bool           usesGpuImplementation;
	int            maxParticles;
	int            maxNewBorns;
	ESortMode      sortMode;
	EFacingMode    facingMode;
	int            version;
};

struct SInitialData
{
	Vec3 position;
	Vec3 velocity;
};

static const int kNumModifierSamples = 16;

struct SEnvironmentParameters
{
	SEnvironmentParameters() : physAccel(0.0f, 0.0f, 0.0f), physWind(0.0f, 0.0f, 0.0f) {}
	Vec3 physAccel;
	Vec3 physWind;
};

class IParticleComponentRuntime : public ::pfx2::IParticleComponentRuntime
{
public:
	enum class EState
	{
		Uninitialized,
		Ready
	};
	virtual gpu_pfx2::IParticleComponentRuntime* GetGpuRuntime() override { return this; }
	virtual void                                 UpdateEmitterData() = 0;
	virtual EState                               GetState() const = 0;
	virtual bool                                 HasParticles() = 0;
	virtual void                                 SetEnvironmentParameters(const SEnvironmentParameters& params) = 0;
	virtual void                                 AccumStats(pfx2::SParticleStats& stats) = 0;
};

enum class ESpawnRateMode
{
	ParticlesPerSecond,
	SecondPerParticle
};

enum EGravityType
{
	eGravityType_Spherical   = 0,
	eGravityType_Cylindrical = 1
};

enum EVortexDirection
{
	eVortexDirection_Clockwise = 0,
	eVortexDirection_CounterClockwise = 1
};

#define LIST_OF_PARAMETER_TYPES \
  X(Spawn,                      \
    float amount;               \
    float delay;                \
    float duration;             \
    float restart;              \
    bool useDelay;              \
    bool useDuration;           \
    bool useRestart; )          \
  X(SpawnMode,                  \
    ESpawnRateMode mode; )      \
  X(Scale,                      \
    f32 scale; )                \
  X(ColorTable,                 \
    Vec3 * samples;             \
    uint32 numSamples; )        \
  X(SizeTable,                  \
    float* samples;             \
    uint32 numSamples; )        \
  X(PixelSize,                  \
    float minSize;              \
    float maxSize;              \
    bool affectOpacity; )       \
  X(Opacity,                    \
    Vec2 alphaScale;            \
    Vec2 clipLow;               \
    Vec2 clipRange;             \
    float* samples;             \
    uint32 numSamples; )        \
  X(LifeTime,                   \
    f32 lifeTime; )             \
  X(MotionPhysics,              \
    Vec3 uniformAcceleration;   \
    float pad1;                 \
    Vec3 uniformWind;           \
    float pad2;                 \
    float gravity;              \
    float drag;                 \
    float windMultiplier; )     \
  X(MotionPhysicsBrownian,      \
    Vec3 scale;                 \
    float speed; )              \
  X(MotionPhysicsSimplex,       \
    Vec3 scale;                 \
    float speed;                \
    float size;                 \
    float rate;                 \
    int octaves; )              \
  X(MotionPhysicsCurl,          \
    Vec3 scale;                 \
    float speed;                \
    float size;                 \
    float rate;                 \
    int octaves; )              \
  X(MotionPhysicsGravity,       \
    Vec3 axis;                  \
    float acceleration;         \
    float decay;                \
    float pad[3];               \
    int gravityType; )          \
  X(MotionPhysicsVortex,        \
    Vec3 axis;                  \
    float speed;                \
    float decay;                \
    float pad[3];               \
    int vortexDirection; )      \
  X(LocationOffset,             \
    Vec3 offset;                \
    float scale; )              \
  X(LocationBox,                \
    Vec3 box;                   \
    float scale; )              \
  X(LocationSphere,             \
    Vec3 scale;                 \
    float radius;               \
    float velocity; )           \
  X(LocationCircle,             \
    Vec2 scale;                 \
    float radius;               \
    float velocity; )           \
  X(LocationNoise,              \
    float amplitude;            \
    float size;                 \
    float rate;                 \
    int octaves; )              \
  X(MotionFluidDynamics,        \
    Vec3 initialVelocity;       \
    float stiffness;            \
    float gravityConstant;      \
    float h;                    \
    float r0;                   \
    float mass;                 \
    float maxVelocity;          \
    float maxForce;             \
    float atmosphericDrag;      \
    float cohesion;             \
    float baroclinity;          \
    float spread;               \
    float particleInfluence;    \
    int numberOfIterations;     \
    int gridSizeX;              \
    int gridSizeY;              \
    int gridSizeZ;              \
    int numSpawnParticles; )    \
  X(Collision,                  \
    float offset;               \
    float radius;               \
    float restitution; )        \
  X(VelocityCone,               \
    float angle;                \
    float velocity; )           \
  X(VelocityDirectional,        \
    Vec3 direction;             \
    float scale; )              \
  X(VelocityOmniDirectional,    \
    float velocity; )

enum class EParameterType
{
#define X(name, ...) name,
	LIST_OF_PARAMETER_TYPES
#undef X
};

struct SFeatureParametersBase
{
	template<class Parameters>
	const Parameters& GetParameters() const
	{
		const Parameters& result = static_cast<const Parameters&>(*this);
		// sanity check that Parameters is a derived class of
		// SFeatureParametersBase
		const SFeatureParametersBase& sanityCheck = result;
		return result;
	}
};

#define X(name, ...)                                         \
  struct SFeatureParameters ## name : SFeatureParametersBase \
  {                                                          \
    static const EParameterType type = EParameterType::name; \
    __VA_ARGS__                                              \
  };
LIST_OF_PARAMETER_TYPES
#undef X

struct SSpawnData;

struct IParticleFeatureGpuInterface : public _i_multithread_reference_target_t
{
public:
	virtual ~IParticleFeatureGpuInterface() {}

	// a feature that implements this can switch the whole component into GPU mode
	virtual bool IsGpuEnabling() { return false; }

	// called from host feature
	template<class Parameters>
	void SetParameters(const Parameters& parameters)
	{
		InternalSetParameters(Parameters::type, parameters);
	}
protected:
	virtual void InternalSetParameters(const EParameterType type, const SFeatureParametersBase& p) {};
};

// interface of GPU particle system
class IManager
{
public:
	enum class EState
	{
		Uninitialized,
		Ready
	};

	virtual void BeginFrame() = 0;

	virtual _smart_ptr<IParticleComponentRuntime>
	CreateParticleComponentRuntime(
		IParticleEmitter* pEmitter,
		pfx2::IParticleComponent* pComponent,
		const SComponentParams& params) = 0;

	virtual _smart_ptr<IParticleFeatureGpuInterface>
	CreateParticleFeatureGpuInterface(EGpuFeatureType) = 0;
};
}
