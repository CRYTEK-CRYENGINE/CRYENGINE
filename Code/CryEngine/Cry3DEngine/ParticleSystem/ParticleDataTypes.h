// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryMath/FinalizingSpline.h>
#include <CrySerialization/Enum.h>
#include <CryCore/DynamicEnum.h>
#include "ParticleCommon.h"
#include "ParticleMath.h"

namespace pfx2
{

struct SChaosKey
{
public:
	explicit SChaosKey(uint32 key) : m_key(key) {}
	SChaosKey(const SChaosKey& key) : m_key(key.m_key) {}
	SChaosKey(SChaosKey key1, SChaosKey key2);
	SChaosKey(SChaosKey key1, SChaosKey key2, SChaosKey key3);

	uint32 Rand();
	uint32 Rand(uint32 range);
	float  RandUNorm();
	float  RandSNorm();

	struct Range
	{
		float scale, bias;
		Range(float lo, float hi);
	};
	float Rand(Range range);

	Vec2  RandCircle();
	Vec2  RandDisc();
	Vec3  RandSphere();

private:
	uint32 m_key;
};

#ifdef CRY_PFX2_USE_SSE

struct SChaosKeyV
{
	explicit SChaosKeyV(SChaosKey key);
	explicit SChaosKeyV(uint32 key);
	explicit SChaosKeyV(uint32v keys) : m_keys(keys) {}
	uint32v Rand();
	uint32v Rand(uint32 range);
	floatv  RandUNorm();
	floatv  RandSNorm();

	struct Range
	{
		floatv scale, bias;
		Range(float lo, float hi);
	};
	floatv Rand(Range range);

private:
	uint32v m_keys;
};

#else

typedef SChaosKey SChaosKeyV;

#endif

///////////////////////////////////////////////////////////////////////////////
// Implement EParticleDataTypes (size, position, etc) with DynamicEnum.
// Data type entries can be declared in multiple source files, with info about the data type (float, Vec3, etc)

enum EDataDomain
{
	EDD_None           = 0,

	EDD_PerParticle    = 1, // Data is per particle
	EDD_PerInstance    = 2, // Data is per sub-emitter

	EDD_HasUpdate      = 4, // Data is updated per-frame, has additional init-value element
	EDD_NeedsClear     = 8, // Data requires clearing after editing

	EDD_ParticleUpdate = EDD_PerParticle | EDD_HasUpdate,
	EDD_InstanceUpdate = EDD_PerInstance | EDD_HasUpdate
};

// Traits for extracting element-type info from scalar and vector types
template<typename T>
struct TDimInfo
{
	using             TElem = T;
	static const uint Dim   = 1;

	static TElem Elem(const T& t, uint e) { CRY_ASSERT(e == 0); return t; }
};

template<>
struct TDimInfo<Vec3>
{
	using             TElem = float;
	static const uint Dim = 3;

	static TElem Elem(const Vec3& t, uint e) { return t[e]; }
};

template<>
struct TDimInfo<Quat>
{
	using             TElem = float;
	static const uint Dim = 4;

	static TElem Elem(const Quat& t, uint e) { return t[e]; }
};

//! Info associated with each EParticleDataType
struct SDataInfo
{
	typedef yasli::TypeID TypeID;

	TypeID      type;
	size_t      typeSize   = 0;
	uint        dimension  = 0;
	EDataDomain domain     = EDD_None;

	SDataInfo() {}

	template<typename T>
	SDataInfo(T*, EDataDomain domain)
		: type(TypeID::get<typename TDimInfo<T>::TElem>())
		, typeSize(sizeof(typename TDimInfo<T>::TElem)), dimension(TDimInfo<T>::Dim)
		, domain(domain)
	{}

	template<typename T>
	bool isType() const         { return type == TypeID::get<T>(); }

	template<typename T>
	bool isType(uint dim) const { return type == TypeID::get<T>() && dimension == dim; }

	uint step() const           { return dimension * (domain & EDD_HasUpdate ? 2 : 1); }
};

//! EParticleDataType implemented as a DynamicEnum, with SDataInfo
typedef DynamicEnum<SDataInfo, uint, SDataInfo>
	EParticleDataType;

ILINE EParticleDataType InitType(EParticleDataType type)
{
	return type + type.info().dimension;
}

//! DataType implemented as a DynamicEnum, with SDataInfo
template<typename T, EDataDomain Domain = EDD_PerParticle>
struct TDataType: EParticleDataType
{
	using EParticleDataType::EParticleDataType;

	// Access element sub-types
	using             TElem = typename TDimInfo<T>::TElem;
	static const uint Dim   = TDimInfo<T>::Dim;

	TDataType<TElem> operator[](uint e) const
	{
		CRY_ASSERT(e < Dim);
		return TDataType<TElem>(this->value() + e);
	}

	// Get following type used for initialization
	TDataType InitType() const
	{
		CRY_ASSERT(this->info().domain & EDD_HasUpdate);
		return TDataType(this->value() + Dim);
	}
};


// Convenient initialization of EParticleDataType
//  MakeDataType(EPDT_SpawnID, TParticleID)
//  MakeDataType(EPVF_Velocity, Vec3)

inline EDataDomain PDT_FLAGS(EDataDomain domain = EDD_PerParticle) { return domain; } // Helper function for variadic macro

#define MakeDataType(Name, T, ...) \
  TDataType<T> Name(SkipPrefix(#Name), nullptr, SDataInfo((T*)0, PDT_FLAGS(__VA_ARGS__)))

inline cstr SkipPrefix(cstr name)
{
	for (int i = 0; name[i]; ++i)
		if (name[i] == '_')
			return name + i + 1;
	return name;
}

// Standard data types
extern TDataType<TParticleId>
	EPDT_SpawnId,
	EPDT_ParentId;

extern TDataType<float>
	EPDT_NormalAge,
	EPDT_LifeTime,
	EPDT_InvLifeTime,
	EPDT_SpawnFraction,
	EPDT_Random,
	EPDT_Size,
	EPDT_Angle2D,
	EPDT_Spin2D;

// Pose data types
extern TDataType<Vec3>
	EPVF_Position,
	EPVF_Velocity,
	EPVF_AngularVelocity;

extern TDataType<Quat>
	EPQF_Orientation;

// NormalAge functions
inline bool IsAlive(float age)   { return age < 1.0f; }
inline bool IsExpired(float age) { return age >= 1.0f; }

}

#include "ParticleDataTypesImpl.h"
