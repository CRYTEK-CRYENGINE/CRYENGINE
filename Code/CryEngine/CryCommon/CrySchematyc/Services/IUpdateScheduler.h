// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once


#include <CryCore/Typelist.h>

#include "CrySchematyc/ICore.h"
#include "CrySchematyc/Utils/ScopedConnection.h"

namespace Schematyc
{
struct IUpdateScheduler;

// Update frequency.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef uint16 UpdateFrequency;

// Enumeration of update frequencies.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct EUpdateFrequency
{
	enum
	{
		EveryFrame = 0,
		EveryTwoFrames,
		EveryFourFrames,
		EveryEightFrames,
		Count,
		Invalid
	};
};

// Update priority.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef uint16 UpdatePriority;

// Update stages.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct EUpdateStage
{
	enum
	{
		Editing    = 4 << 8,
		PrePhysics = 3 << 8,
		Default    = 2 << 8,
		Post       = 1 << 8
	};
};

// Update distribution.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct EUpdateDistribution
{
	enum
	{
		Early1   = 8,
		Early2   = 7,
		Early3   = 6,
		Default  = 5,
		Late1    = 4,
		Late2    = 3,
		Late3    = 2,
		Late4    = 1,

		Earliest = 0xff,
		Latest   = 0x01,
		End      = 0x00
	};
};

// Update priorities.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct EUpdatePriority
{
	enum
	{
		Input        = EUpdateStage::PrePhysics | EUpdateDistribution::Early1,
		Camera       = EUpdateStage::PrePhysics | EUpdateDistribution::Early2,
		PreAnimation = EUpdateStage::PrePhysics | EUpdateDistribution::Default,
		PreMovement  = EUpdateStage::PrePhysics | EUpdateDistribution::Late1,
		Mannequin    = EUpdateStage::PrePhysics | EUpdateDistribution::Late2,
		Animation    = EUpdateStage::PrePhysics | EUpdateDistribution::Late3,
		Movement     = EUpdateStage::PrePhysics | EUpdateDistribution::Late4,
		Default      = EUpdateStage::Default | EUpdateDistribution::Default,
		DebugRender  = EUpdateStage::Post | EUpdateDistribution::Default,
		Editing      = EUpdateStage::Editing | EUpdateDistribution::Default,
		Dirty        = 0
	};
};

// Utility function for creating new update priorities.
////////////////////////////////////////////////////////////////////////////////////////////////////
inline UpdatePriority CreateUpdatePriority(uint32 stage, uint32 distribution)
{
	return stage | distribution;
}

// Update context structure.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUpdateContext
{
	inline SUpdateContext()
		: cumulativeFrameTime(0.0f)
		, frameTime(0.0f)
	{}

	inline SUpdateContext(float _cumulativeFrameTime, float _frameTime)
		: cumulativeFrameTime(_cumulativeFrameTime)
		, frameTime(_frameTime)
	{}

	float cumulativeFrameTime;
	float frameTime;
};

// Update callback.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::function<void (const SUpdateContext&)> UpdateCallback;

// Update filter.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::function<bool ()> UpdateFilter;

// Update parameters.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUpdateParams
{
	explicit inline SUpdateParams(const UpdateCallback& _callback, CConnectionScope& _scope, UpdateFrequency _frequency = EUpdateFrequency::EveryFrame, UpdatePriority _priority = EUpdatePriority::Default)
		: callback(_callback)
		, scope(_scope)
		, frequency(_frequency)
		, priority(_priority)
	{}

	UpdateCallback    callback;
	CConnectionScope& scope;
	UpdateFrequency   frequency;
	UpdatePriority    priority;
	UpdateFilter      filter;
};

// Update scheduler interface.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct IUpdateScheduler
{
	virtual ~IUpdateScheduler() {}

	virtual bool Connect(const SUpdateParams& params) = 0;
	virtual void Disconnect(CConnectionScope& scope) = 0;
	virtual bool InFrame() const = 0;
	virtual bool BeginFrame(float frameTime) = 0;
	virtual bool Update(UpdatePriority beginPriority = EUpdateStage::PrePhysics | EUpdateDistribution::Earliest, UpdatePriority endPriority = EUpdateStage::Post | EUpdateDistribution::End) = 0;
	virtual bool EndFrame() = 0;
	virtual void VerifyCleanup() = 0;
	virtual void Reset() = 0;
};

// Structure for recursive rule checking.
////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename CONTEXT, typename NODE> struct SCheckRulesRecursive;

template<typename CONTEXT, typename HEAD, typename TAIL> struct SCheckRulesRecursive<CONTEXT, NTypelist::CNode<HEAD, TAIL>>
{
	inline bool operator()(CONTEXT context) const
	{
		return HEAD()(context) && SCheckRulesRecursive<CONTEXT, TAIL>()(context);
	}
};

template<typename CONTEXT, typename HEAD> struct SCheckRulesRecursive<CONTEXT, NTypelist::CNode<HEAD, NTypelist::CEnd>>
{
	inline bool operator()(CONTEXT context) const
	{
		return HEAD()(context);
	}
};

// Configurable update filter.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename CONTEXT, typename RULES> class CConfigurableUpdateFilter
{
public:

	inline CConfigurableUpdateFilter()
		: m_context()
	{}

	inline UpdateFilter Init(CONTEXT context)
	{
		m_context = context;
		return [this]() -> bool { return this->Filter();  };
	}

	inline bool Filter() const
	{
		return SCheckRulesRecursive<CONTEXT, RULES>()(m_context);
	}

private:

	CONTEXT m_context;
};
} // Schematyc
