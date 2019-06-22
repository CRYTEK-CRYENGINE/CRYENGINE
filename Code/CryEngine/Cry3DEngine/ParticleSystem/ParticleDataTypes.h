// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/FinalizingSpline.h>
#include <CrySerialization/Enum.h>
#include <CryCore/DynamicEnum.h>
#include "ParticleCommon.h"
#include "ParticleMath.h"

namespace pfx2
{

///////////////////////////////////////////////////////////////////////////
template<typename F>
struct Slope
{
	F start;
	F scale;

	Slope(F lo = convert<F>(), F hi = convert<F>(1))
		: start(lo), scale(hi - lo) {}
	template<typename F2> Slope(const Slope<F2>& o)
		: start(convert<F>(o.start)), scale(convert<F>(o.scale)) {}

	F operator()(F val) const { return MAdd(val, scale, start); }
};

///////////////////////////////////////////////////////////////////////////////
// Random number generator

struct SChaosKey
{
public:
	explicit SChaosKey(uint32 key) : m_key(key) {}
	SChaosKey(const SChaosKey& key) : m_key(key.m_key) {}

	uint32 Rand();
	uint32 Rand(uint32 range);
	float  RandUNorm();
	float  RandSNorm();
	float operator()() { return RandUNorm(); }

	struct Range: Slope<float>
	{
		Range(float lo = 0, float hi = 1);
	};
	float operator()(Range range);

private:
	uint32 m_key;
};

#ifdef CRY_PFX2_USE_SSE

struct SChaosKeyV
{
	explicit SChaosKeyV(uint32 key);
	explicit SChaosKeyV(uint32v keys) : m_keys(keys) {}
	uint32v Rand();
	uint32v Rand(uint32 range);
	floatv  RandUNorm();
	floatv  RandSNorm();

	struct Range: Slope<floatv>
	{
		Range(float lo = 0, float hi = 1);
	};
	floatv operator()(Range range);

private:
	uint32v m_keys;
};

#else

typedef SChaosKey SChaosKeyV;

#endif

template<uint Dim, typename T = std::array<float, Dim>>
struct SChaosKeyN
{
	SChaosKeyN(SChaosKey& chaos)
		: m_chaos(chaos) {}

	void SetRange(uint e, Range range, bool isOpen = false)
	{
		m_ranges[e] = SChaosKey::Range(range.start, range.end);
	}

	T operator()()
	{
		T result;
		for (uint e = 0; e < Dim; ++e)
			result[e] = m_chaos(m_ranges[e]);
		return result;
	}

private:
	SChaosKey&       m_chaos;
	SChaosKey::Range m_ranges[Dim];
};

///////////////////////////////////////////////////////////////////////////////
// Sequential number generator

template<uint Dim, typename T = std::array<float, Dim>>
struct SOrderKeyN
{
	SOrderKeyN(uint key = 0)
		: m_key(key)
	{
		for (uint e = 0; e < Dim; ++e)
			m_modulus[e] = 1;
	}

	void SetModulus(uint modulus, uint e = 0)
	{
		modulus = max(modulus, 1u);
		m_modulus[e] = modulus;
		m_scale[e] = 1.0f / float(modulus);
	}

	void SetRange(uint e, Range range, bool isOpen = false)
	{
		m_ranges[e] = Slope<float>(range.start, range.end);
		if (!isOpen && m_modulus[e] > 1)
			m_ranges[e].scale *= float(m_modulus[e] - 1) / float(m_modulus[e]);
	}

	T operator()()
	{
		T result;
		uint number = m_key++;
		for (uint e = 0; e < Dim; ++e)
		{
			float fraction = float(number % m_modulus[e]) * m_scale[e];
			result[e] = m_ranges[e](fraction);
			number = number / m_modulus[e];
		}
		return result;
	}

private:
	uint         m_key;
	uint         m_modulus[Dim];
	float        m_scale[Dim];
	Slope<float> m_ranges[Dim];
};

///////////////////////////////////////////////////////////////////////////////
// Implement EParticleDataTypes (size, position, etc) with DynamicEnum.
// Data type entries can be declared in multiple source files, with info about the data type (float, Vec3, etc)

enum EDataDomain
{
	EDD_None           = 0,      // Data is per-emitter or global

	EDD_Particle       = BIT(0), // Data is per particle
	EDD_Spawner        = BIT(1), // Data is per particle spawner

	EDD_HasUpdate      = BIT(2), // Data is updated per-frame, has additional init-value element

	EDD_ParticleUpdate = EDD_Particle | EDD_HasUpdate,
	EDD_SpawnerUpdate  = EDD_Spawner | EDD_HasUpdate
};

inline int ElementType(EDataDomain domain) { return (domain & 3) - 1; }

template<typename T>
struct ElementTypeArray: std::array<T, 2>
{
	using base = std::array<T, 2>;
	const T& operator[](EDataDomain domain) const { return base::operator[](ElementType(domain)); }
	T& operator[](EDataDomain domain) { return base::operator[](ElementType(domain)); }
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
template<typename T>
struct TDataType: EParticleDataType
{
	using EParticleDataType::EParticleDataType;
	TDataType(cstr name, EDataDomain domain = EDD_Particle)
		: EParticleDataType(name, nullptr, SDataInfo((T*)0, domain)) {}

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

	T* Cast(EParticleDataType typeIn, void* ptr) const
	{
		if (typeIn == *this)
			return (T*)ptr;
		else
			return nullptr;
	}

	struct TVarCheckArray: TVarArray<T>
	{
		TVarCheckArray(TVarArray<T> in) : TVarArray<T>(in) {}
		operator uint() const { return this->size(); }
	};
	TVarCheckArray Cast(EParticleDataType typeIn, void* ptr, SUpdateRange range) const
	{
		if (typeIn == *this)
			return TVarArray<T>((T*)ptr - range.m_begin, range.size());
		else
			return TVarArray<T>();
	}
};


// Convenient initialization of EParticleDataType
//  MakeDataType(EPDT_SpawnID, TParticleID)
//  MakeDataType(EPVF_Velocity, Vec3)

inline EDataDomain DT_FLAGS(EDataDomain domain = EDD_Particle) { return domain; } // Helper function for variadic macro

#define MakeDataType(Name, T, ...) \
	TDataType<T> Name(SkipPrefix(#Name), DT_FLAGS(__VA_ARGS__))

inline cstr SkipPrefix(cstr name)
{
	for (int i = 0; name[i]; ++i)
		if (name[i] == '_')
			return name + i + 1;
	return name;
}

// Store usage of particle data types
struct SUseData
{
	TDynArray<int16>        offsets;    // Offset of data type if used, -1 if not
	ElementTypeArray<int16> totalSizes; // Total size of data per-element data

	SUseData()
		: offsets(EParticleDataType::size(), -1)
	{
		totalSizes.fill(0);
	}
	bool Used(EParticleDataType type) const
	{
		return offsets[type] >= 0;
	}
	void AddData(EParticleDataType type)
	{
		if (!Used(type))
		{
			auto domain = type.info().domain;
			uint dim = type.info().dimension;
			int16 size = Align(type.info().typeSize, 4);
			for (uint i = 0; i < dim; ++i)
			{
				offsets[type + i] = totalSizes[domain];
				assert(totalSizes[domain] + size > totalSizes[domain]);
				totalSizes[domain] += size;
			}
		}
	}
};

using PUseData = std::shared_ptr<SUseData>;
inline PUseData NewUseData() { return std::make_shared<SUseData>(); }

struct SUseDataRef
{
	TConstArray<int16> offsets;
	int16              totalSize = 0;
	EDataDomain        domain;
	PUseData           pRefData;

	SUseDataRef() 
	{}
	SUseDataRef(const PUseData& pUseData, EDataDomain domain)
		: offsets(pUseData->offsets)
		, totalSize(pUseData->totalSizes[domain])
		, domain(domain)
		, pRefData(pUseData)
	{}
	bool Used(EParticleDataType type) const
	{
		return type.info().domain & domain && offsets[type] >= 0;
	}
};

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

// Spawner data types
extern TDataType<TParticleId>
	ESDT_ParentId;
extern TDataType<float>
	ESDT_Age;

// NormalAge functions
inline bool IsAlive(float age)   { return age < 1.0f; }
inline bool IsExpired(float age) { return age >= 1.0f; }

}

#include "ParticleDataTypesImpl.h"
