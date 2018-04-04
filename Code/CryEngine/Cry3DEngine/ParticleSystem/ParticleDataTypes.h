// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     24/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLEDATATYPES_H
#define PARTICLEDATATYPES_H

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

enum EState
{
	ESB_Alive   = BIT(0),   // this particle is alive
	ESB_Dead    = BIT(1),   // this particle is either dead or expired
	ESB_NewBorn = BIT(2),   // this particle appeared this frame
	ESB_Update  = BIT(7),   // this particle can be updated

	ES_Empty    = 0,                                    // nobody home
	ES_Dead     = ESB_Dead,                             // particle allocated but dead
	ES_Alive    = ESB_Update | ESB_Alive,               // regular living particle
	ES_Expired  = ESB_Update | ESB_Dead,                // passed the age time but will only be dead next frame
	ES_NewBorn  = ESB_Update | ESB_NewBorn | ESB_Alive, // particle appeared this frame
};

///////////////////////////////////////////////////////////////////////////////
// Implement EParticleDataTypes (size, position, etc) with DynamicEnum.
// Data type entries can be declared in multiple source files, with info about the data type (float, Vec3, etc)

enum BHasInit {}; // boolean argument indicating data type has an extra init field

//! Info associated with each EParticleDataType
struct SParticleDataInfo
{
	typedef yasli::TypeID TypeID;

	TypeID   type;
	size_t   sizeOf;
	uint     dimension;
	BHasInit hasInit;

	SParticleDataInfo()
		: dimension(0), hasInit(BHasInit(false)) {}

	template<typename T>
	SParticleDataInfo(T*, uint dim = 1, BHasInit init = BHasInit(false))
		: type(TypeID::get<T>()), sizeOf(sizeof(T)), dimension(dim), hasInit(init) {}

	template<typename T>
	bool isType() const { return type == TypeID::get<T>(); }

	template<typename T>
	bool   isType(uint dim) const { return type == TypeID::get<T>() && dimension == dim; }

	uint   step() const           { return dimension * (1 + hasInit); }
	size_t typeSize() const       { return sizeOf; }
};

template<typename T = float, uint dim = 1, BHasInit init = BHasInit(false)>
struct TParticleDataInfo : SParticleDataInfo
{
	TParticleDataInfo()
		: SParticleDataInfo((T*)0, dim, init) {}
};

//! EParticleDataType implemented as a DynamicEnum, with SParticleDataInfo
typedef DynamicEnum<SParticleDataInfo, uint, SParticleDataInfo>
  EParticleDataType;

ILINE EParticleDataType InitType(EParticleDataType type)
{
	return type + type.info().dimension;
}

// Convenient initialization of EParticleDataType
//  EParticleDataType PDT(EPDT_SpawnID, TParticleID, 1)
//  EParticleDataType PDT(EPVF_Velocity, float, 3)

#define PDT(Name, ...) \
  Name(SkipPrefix( # Name), 0, TParticleDataInfo<__VA_ARGS__>())

inline cstr SkipPrefix(cstr name)
{
	for (int i = 0; name[i]; ++i)
		if (name[i] == '_')
			return name + i + 1;
	return name;
}

// Standard data types
extern EParticleDataType
  EPDT_SpawnId,
  EPDT_ParentId,
  EPDT_State,
  EPDT_NormalAge,
  EPDT_LifeTime,
  EPDT_InvLifeTime,
  EPDT_SpawnFraction,
  EPDT_Random,
  EPDT_Size,
  EPDT_Angle2D,
  EPDT_Spin2D;

// pose data types
extern EParticleDataType
  EPVF_Position,
  EPVF_Velocity,
  EPQF_Orientation,
  EPVF_AngularVelocity,
  EPVF_LocalPosition,
  EPVF_LocalVelocity,
  EPQF_LocalOrientation;

}

#include "ParticleDataTypesImpl.h"

#endif // PARTICLEDATATYPES_H
