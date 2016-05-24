// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
// #define CRY_DEBUG_PARTICLE_SYSTEM				// enable debugging on all particle source code
// #define CRY_PFX1_BAIL_UNSUPPORTED		// disable pfx1 features that not yet supported by pfx2 for precision profiling
// #define CRY_PFX2_LOAD_PRIORITY					// when trying to load a pfx1 effect, try to load pfx2 effect with the same name first
#define CRY_PFX2_PROFILE_DETAILS // more in detail profile of pfx2. Individual features and sub update parts will appear here.
// ~compile options

#if defined(CRY_DEBUG_PARTICLE_SYSTEM) && CRY_PLATFORM_WINDOWS
	#define CRY_PFX2_DBG          \
	  __pragma(optimize("", off)) \
	  __pragma(inline_depth(0))
#else
	#define CRY_PFX2_DBG
#endif
#
#ifndef _RELEASE
	#define CRY_PFX2_ASSERT(cond) { CRY_ASSERT(cond); }
#else
	#define CRY_PFX2_ASSERT(cond)
#endif
#
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
const uint gCurrentVersion = 8;

class CParticleSystem;

class CParticleEffect;
class CParticleComponent;
class CParticleFeature;

class CParticleEmitter;
class CParticleComponentRuntime;

struct SComponentParams;

typedef _smart_ptr<CParticleFeature> TParticleFeaturePtr;

typedef uint32                       TInstanceDataOffset;
const TParticleId gInvalidId = -1;

typedef stl::HeapAllocator<stl::PSyncNone>                                    TParticleHeap;
typedef TParticleHeap::Array<TParticleId, uint, CRY_PFX2_PARTICLES_ALIGNMENT> TParticleIdArray;
typedef TParticleHeap::Array<float, uint, CRY_PFX2_PARTICLES_ALIGNMENT>       TFloatArray;

#ifdef CRY_PFX2_USE_SSE

struct TParticleGroupId
{
public:
	TParticleGroupId() {}
	TParticleGroupId(uint32 i) { id = i; }
	TParticleGroupId&                 operator=(uint32 i)                                            { id = i; return *this; }
	friend bool                       operator<(const TParticleGroupId a, const TParticleGroupId b)  { return a.id < b.id; }
	friend bool                       operator<=(const TParticleGroupId a, const TParticleGroupId b) { return a.id <= b.id; }
	friend bool                       operator>=(const TParticleGroupId a, const TParticleGroupId b) { return a.id >= b.id; }
	friend bool                       operator==(const TParticleGroupId a, const TParticleGroupId b) { return a.id == b.id; }
	TParticleGroupId&                 operator++()                                                   { ++id; return *this; }
	TParticleGroupId                  operator++(int)                                                { id++; return *this; }
	TParticleGroupId&                 operator+=(int stride)                                         { id += stride; return *this; }
	template<class type> friend type* operator+(type* ptr, TParticleGroupId id)                      { return ptr + id.id; }
	friend TParticleGroupId           operator&(TParticleGroupId id, uint32 mask)                    { return TParticleGroupId(id.id & mask); }
	uint32                            Get() const                                                    { return id; }
private:
	uint32 id;
};

#else

typedef TParticleId TParticleGroupId;

#endif

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
