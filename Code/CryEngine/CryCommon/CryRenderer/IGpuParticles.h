// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryParticleSystem/IParticlesPfx2.h>
#include <CryRenderer/RenderElements/CREParticle.h>

namespace gpu_pfx2
{

using namespace pfx2;

#define LIST_OF_FEATURE_TYPES \
  X(Dummy)                    \
  X(Motion)                   \
  X(Color)                    \
  X(FieldSize)                \
  X(FieldPixelSize)           \
  X(FieldOpacity)             \
  X(Collision)                \
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
		: maxParticles(0), maxNewBorns(0), sortMode(ESortMode::None), facingMode(EFacingMode::Screen), version(0) {}
	int            maxParticles;
	int            maxNewBorns;
	ESortMode      sortMode;
	EFacingMode    facingMode;
	int            version;
};

struct SParticleInitializationParameters
{
	Vec3  offset;
	float velocity;
	Vec3  box;
	float velocityScale;
	Vec3  scale;
	float radius;
	Vec3  color;
	float size;
	Vec3  direction;
	float opacity;
	float angle;
	float directionScale;
	float omniVelocity;
	float amplitude;
	float noiseSize;
	float rate;
	int   octaves;
};

struct SParticleParameters
{
	Matrix44  viewProjection;
	Quat      emitterOrientation;
	Vec3      emitterPosition;
	f32       deltaTime;
	Vec3      physAccel;
	f32       currentTime;
	Vec3      physWind;
	float     farToNearDistance;
	Vec3      cameraPosition;
	float     lifeTime;
	int32     numParticles;
	int32     numNewBorns;
	int32     numKilled;
	ESortMode sortMode;
	int32     managerSlot;
};

enum EFeatureInitializationFlags
{
	eFeatureInitializationFlags_LocationOffset          = 0x04,
	eFeatureInitializationFlags_LocationBox             = 0x08,
	eFeatureInitializationFlags_LocationSphere          = 0x10,
	eFeatureInitializationFlags_LocationCircle          = 0x20,
	eFeatureInitializationFlags_LocationNoise           = 0x200,
	eFeatureInitializationFlags_VelocityCone            = 0x40,
	eFeatureInitializationFlags_VelocityDirectional     = 0x80,
	eFeatureInitializationFlags_VelocityOmniDirectional = 0x100,
};

enum EFeatureUpdateFlags
{
	eFeatureUpdateFlags_Motion                = 0x04,
	eFeatureUpdateFlags_Color                 = 0x08,
	eFeatureUpdateFlags_Size                  = 0x10,
	eFeatureUpdateFlags_Opacity               = 0x20,
	eFeatureUpdateFlags_Motion_LinearIntegral = 0x40,
	eFeatureUpdateFlags_Motion_DragFast       = 0x80,
	eFeatureUpdateFlags_Motion_Brownian       = 0x100,
	eFeatureUpdateFlags_Motion_Simplex        = 0x200,
	eFeatureUpdateFlags_Motion_Curl           = 0x400,
	eFeatureUpdateFlags_Motion_Gravity        = 0x800,
	eFeatureUpdateFlags_Motion_Vortex         = 0x1000,
	EFeatureUpdateFlags_Collision_ScreenSpace = 0x2000,
	EFeatureUpdateFlags_PixelSize             = 0x4000
};

// Params set by particle features
// Includes InitializationParams, a subset of ParticleParams, and UpdateFlags
struct SUpdateParams : SParticleInitializationParameters
{
	float  lifeTime;
	Quat   emitterOrientation;
	Vec3   emitterPosition;
	Vec3   physAccel;
	Vec3   physWind;

	uint32 initFlags;
	uint32 updateFlags;

	SUpdateParams()
	{
		ZeroStruct(*this);
		color   = Vec3(1);
		size    = 1;
		opacity = 1;
		emitterOrientation = Quat(IDENTITY);
	}
};

static const int kNumModifierSamples = 16;

class IParticleComponentRuntime : public _i_reference_target_t
{
public:
	virtual             ~IParticleComponentRuntime() {}

	virtual bool        IsValidForParams(const SComponentParams& parameters) = 0;

	virtual void        UpdateData(const SUpdateParams& params, TConstArray<SSpawnEntry> entries, TConstArray<SParentData> parentData) = 0;
	
	virtual bool        HasParticles() = 0;
	virtual const AABB& GetBounds() const = 0;
	virtual void        AccumStats(SParticleStats& stats) = 0;
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

struct IParticleFeature : public _i_multithread_reference_target_t
{
public:
	virtual ~IParticleFeature() {}

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
	virtual void BeginFrame() = 0;

	virtual IParticleComponentRuntime* CreateParticleContainer(const SComponentParams& params, TConstArray<IParticleFeature*> features) = 0;
	virtual IParticleFeature* CreateParticleFeature(EGpuFeatureType) = 0;
};
}
