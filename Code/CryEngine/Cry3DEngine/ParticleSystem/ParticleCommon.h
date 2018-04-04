// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     06/04/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLECOMMON_H
#define PARTICLECOMMON_H

#pragma once

#include <CryParticleSystem/IParticlesPfx2.h>
#include <CryRenderer/RenderElements/CREParticle.h>

// compile options
#ifdef _DEBUG
	#define CRY_DEBUG_PARTICLE_SYSTEM    // enable debugging on all particle source code
#endif
// #define CRY_PFX1_BAIL_UNSUPPORTED	// disable pfx1 features that not yet supported by pfx2 for precision profiling
// #define CRY_PFX2_LOAD_PRIORITY		// when trying to load a pfx1 effect, try to load pfx2 effect with the same name first
#define CRY_PFX2_PROFILE_DETAILS        // more in detail profile of pfx2. Individual features and sub update parts will appear here.
// ~compile options

#if defined(CRY_DEBUG_PARTICLE_SYSTEM) && CRY_PLATFORM_WINDOWS
	#pragma optimize("", off)
	#pragma inline_depth(0)
	#undef ILINE
	#define ILINE inline
#endif
#define CRY_PFX2_DBG	// obsolete
#
#ifndef _RELEASE
	#define CRY_PFX2_ASSERT(cond) { CRY_ASSERT(cond); }
#else
	#define CRY_PFX2_ASSERT(cond)
#endif

#ifdef CRY_DEBUG_PARTICLE_SYSTEM
#	define CRY_PFX2_DEBUG_ONLY_ASSERT(cond) CRY_PFX2_ASSERT(cond);
#else
#	define CRY_PFX2_DEBUG_ONLY_ASSERT(cond)
#endif

#ifdef CRY_PFX2_PROFILE_DETAILS
	#define CRY_PFX2_PROFILE_DETAIL FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);
#else
	#define CRY_PFX2_PROFILE_DETAIL
#endif

#define CRY_PFX2_PARTICLES_ALIGNMENT 16

#if (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS || CRY_PLATFORM_APPLE) && !CRY_PLATFORM_NEON
	#define CRY_PFX2_USE_SSE
	#if (CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS)
		#define CRY_PFX2_USE_SEE4
	#endif
#endif
#if CRY_PLATFORM_X86
	#undef CRY_PFX2_USE_SSE // limited code generation in x86, disabling SSE entirely for now.
	#// msvc x86 only supports up to 3 arguments by copy of type __m128. More than that will require
	#// references. That makes it incompatible with Vec4_tpl.
#endif

namespace pfx2
{

const uint gMinimumVersion = 1;
const uint gCurrentVersion = 9;

class CParticleSystem;
class CParticleEffect;
class CParticleComponent;
class CParticleFeature;

class CParticleEmitter;
class CParticleComponentRuntime;

struct SComponentParams;

typedef _smart_ptr<CParticleFeature> TParticleFeaturePtr;

typedef uint32                       TInstanceDataOffset;

const TParticleId gInvalidId           = -1;
const float gInfinity                  = std::numeric_limits<float>::infinity();

using TParticleHeap                    = stl::HeapAllocator<stl::PSyncNone>;
template<typename T> using THeapArray  = TParticleHeap::Array<T, uint, CRY_PFX2_PARTICLES_ALIGNMENT>;
using TParticleIdArray                 = THeapArray<TParticleId>;
using TFloatArray                      = THeapArray<float>;

template<typename T> using TDynArray   = DynArray<T, uint>;
template<typename T> using TVarArray   = Array<T, uint>;
template<typename T> using TConstArray = Array<const T, uint>;


#ifdef CRY_PFX2_USE_SSE

struct TParticleGroupId
{
public:
	TParticleGroupId() {}
	TParticleGroupId(uint32 i) { id = i; }
	friend bool                       operator<(const TParticleGroupId a, const TParticleGroupId b)  { return a.id < b.id; }
	friend bool                       operator>(const TParticleGroupId a, const TParticleGroupId b)  { return a.id > b.id; }
	friend bool                       operator<=(const TParticleGroupId a, const TParticleGroupId b) { return a.id <= b.id; }
	friend bool                       operator>=(const TParticleGroupId a, const TParticleGroupId b) { return a.id >= b.id; }
	friend bool                       operator==(const TParticleGroupId a, const TParticleGroupId b) { return a.id == b.id; }
	friend bool                       operator!=(const TParticleGroupId a, const TParticleGroupId b) { return a.id != b.id; }
	TParticleGroupId&                 operator++()                                                   { ++id; return *this; }
	TParticleGroupId                  operator++(int)                                                { id++; return *this; }
	TParticleGroupId&                 operator+=(int stride)                                         { id += stride; return *this; }
	template<class type> friend type* operator+(type* ptr, TParticleGroupId id)                      { return ptr + id.id; }
	friend TParticleGroupId           operator&(TParticleGroupId id, uint32 mask)                    { return TParticleGroupId(id.id & mask); }
	uint32                            operator+() const                                              { return id; }
private:
	uint32 id;
};

#define CRY_PFX2_PARTICLESGROUP_STRIDE 4 // can be 8 for AVX or 64 forGPU

#else

typedef TParticleId TParticleGroupId;

#define CRY_PFX2_PARTICLESGROUP_STRIDE 1

#endif

#define CRY_PFX2_PARTICLESGROUP_LOWER(id) ((id) & ~((CRY_PFX2_PARTICLESGROUP_STRIDE - 1)))
#define CRY_PFX2_PARTICLESGROUP_UPPER(id) ((id) | ((CRY_PFX2_PARTICLESGROUP_STRIDE - 1)))


struct SRenderContext
{
	SRenderContext(const SRendParams& rParams, const SRenderingPassInfo& passInfo)
		: m_renderParams(rParams)
		, m_passInfo(passInfo)
		, m_distance(0.0f)
		, m_lightVolumeId(0)
		, m_fogVolumeId(0) {}
	const SRendParams&        m_renderParams;
	const SRenderingPassInfo& m_passInfo;
	float                     m_distance;
	uint16                    m_lightVolumeId;
	uint16                    m_fogVolumeId;
};

template<typename TIndex, int nStride = 1>
struct TIndexRange
{
	struct iterator
	{
		TIndex i;
		iterator(TIndex i) : i(i) {}
		bool operator != (iterator it) const { return i != it.i; }
		void operator ++ () { i += nStride; }
		TIndex operator*() const { return i; }
	};

	TIndex m_begin, m_end;

	iterator begin() const { return iterator(m_begin); }
	iterator end() const   { return iterator(m_end); }
	TIndex size() const    { return m_end - m_begin; }

	explicit TIndexRange(TIndex b = 0, TIndex e = 0)
		: m_begin(+b & ~(nStride - 1))
		, m_end((+e + (nStride - 1)) & ~(nStride - 1))
	{}

	template<typename TIndex2, int nStride2>
	TIndexRange(TIndexRange<TIndex2, nStride2> range)
		: TIndexRange(range.m_begin, range.m_end) {}
};

typedef TIndexRange<TParticleId> SUpdateRange;
typedef TIndexRange<TParticleGroupId, CRY_PFX2_PARTICLESGROUP_STRIDE> SGroupRange;

enum EFeatureType
{
	EFT_Generic = 0x0,        // this feature does nothing in particular. Can have many of this per component.
	EFT_Spawn   = BIT(0),     // this feature is capable of spawning particles. At least one is needed in a component.
	EFT_Size    = BIT(2),     // this feature changes particles sizes. At least one is required per component.
	EFT_Life    = BIT(3),     // this feature changes particles life time. At least one is required per component.
	EFT_Render  = BIT(4),     // this feature renders particles. Each component can only have either none or just one of this.
	EFT_Motion  = BIT(5),     // this feature moved particles around. Each component can only have either none or just one of this.
};

}

#endif // PARTICLECOMMON_H
