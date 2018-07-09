// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move priority enumerations to game code, just need to keep EUpdatePriority::Default and EUpdatePriority::Dirty.
// #SchematycTODO : Clean up priority stage and distribution interface.
// #SchematycTODO : Replace CUpdateScope with scoped connection.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CryCore/Typelist.h>

#include "CrySchematyc2/IFramework.h"

namespace Schematyc2
{
	struct IUpdateScheduler;
	struct IObject;

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
			Editing    = 5 << 8,
			PrePhysics = 4 << 8,
			Default    = 3 << 8,
			Post       = 2 << 8,
			PostDebug  = 1 << 8
		};
	};

	// Update distribution.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	struct EUpdateDistribution
	{
		enum
		{
			Early1   = 10,
			Early2   =  9,
			Early3   =  8,
			Default  =  7,
			Late1    =  6,
			Late2    =  5,
			Late3    =  4,
			Late4    =  3,
			Late5    =  2,
			Late6    =  1,

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
			Input = EUpdateStage::PrePhysics | EUpdateDistribution::Early1,
			Camera = EUpdateStage::PrePhysics | EUpdateDistribution::Early2,
			PreAnimation = EUpdateStage::PrePhysics | EUpdateDistribution::Default,
			PreMovement = EUpdateStage::PrePhysics | EUpdateDistribution::Late1,
			PrePuppet = EUpdateStage::PrePhysics | EUpdateDistribution::Late2,
			Mannequin = EUpdateStage::PrePhysics | EUpdateDistribution::Late3,
			Animation = EUpdateStage::PrePhysics | EUpdateDistribution::Late4,
			Movement = EUpdateStage::PrePhysics | EUpdateDistribution::Late5,
			PostPuppet = EUpdateStage::PrePhysics | EUpdateDistribution::Late6,
			AiCommon = EUpdateStage::Default | EUpdateDistribution::Early1,
			Timers = EUpdateStage::Default | EUpdateDistribution::Early2,
			SpatialIndex = EUpdateStage::Default | EUpdateDistribution::Early3,
			Default = EUpdateStage::Default | EUpdateDistribution::Default,
			DebugRender = EUpdateStage::Post | EUpdateDistribution::Default,
			DebugOptional = EUpdateStage::PostDebug | EUpdateDistribution::Default,
			Editing = EUpdateStage::Editing | EUpdateDistribution::Default,
			Dirty = 0
		};
	};

	// Utility function for creating new update priorities.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	inline UpdatePriority CreateUpdatePriority(uint32 stage, uint32 distribution)
	{
		return stage | distribution;
	}

	// Update scope class.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CUpdateScope
	{
		friend class CUpdateScheduler;

	public:

		CUpdateScope();
		CUpdateScope(const IEntity* pEntity);
		CUpdateScope(const IEntity* pEntity, const Schematyc2::IObject* pSchematycObject);

		~CUpdateScope();

		bool IsBound() const;
		const IEntity* GetEntity() const;
		const Schematyc2::IObject* GetSchematycObj() const;
		bool IsOnlyLocalGrid() const;
		void SetExtraScopeInfo(const IEntity* pEntity, const Schematyc2::IObject* pSchematycObj, bool bOnlyLocalGrid);

	private:

		bool                            m_bIsBound;
		const IEntity*                  m_pEntity;
		const Schematyc2::IObject* m_pSchematycObject;
		bool                            m_bOnlyLocalGrid;
	};

	// Update relevance
	//////////////////////////////////////////////////////////////////////////
	struct SRelevantEntity
	{
		SRelevantEntity(EntityId _id, Vec3 _worldPos, bool isLocalEntity)
			: m_id(_id)
			, m_worldPos(_worldPos)
			, m_localEntity(isLocalEntity)
		{}

		EntityId GetEntityId() const;
		Vec3 GetEntityWorldPos() const;
		bool GetIsLocalEntity() const;
	
	private:
		EntityId m_id;
		Vec3 m_worldPos;
		bool m_localEntity;
	};

	inline EntityId SRelevantEntity::GetEntityId() const
	{
		return m_id;
	}

	inline Vec3 SRelevantEntity::GetEntityWorldPos() const
	{
		return m_worldPos;
	}

	inline bool SRelevantEntity::GetIsLocalEntity() const
	{
		return m_localEntity;
	}

	class CUpdateRelevanceContext
	{

	public:
		CUpdateRelevanceContext(SRelevantEntity* relevantEntities, size_t count, float dynamicEntitiesUpdateRangeSqr, float defaultGridDistanceUpdate)
			: m_relevantEntities(relevantEntities)
			, m_relevantEntitiesCount(count)
			, m_dynamicEntitiesUpdateRangeSqr(dynamicEntitiesUpdateRangeSqr)
			, m_defaultGridDistanceUpdate(defaultGridDistanceUpdate)
		{}

		const SRelevantEntity* GetRelevantEntities(size_t& count) const;
		float GetDynamicEntitiesUpdateRangeSqr() const;
		float GetDefaultGridDistanceUpdate() const;

	private:
		SRelevantEntity* m_relevantEntities;
		size_t m_relevantEntitiesCount;
		float m_dynamicEntitiesUpdateRangeSqr;
		float m_defaultGridDistanceUpdate;
	};

	inline const SRelevantEntity* CUpdateRelevanceContext::GetRelevantEntities(size_t& count) const
	{
		count = m_relevantEntitiesCount;
		return m_relevantEntities;
	}

	inline float CUpdateRelevanceContext::GetDynamicEntitiesUpdateRangeSqr() const
	{
		return m_dynamicEntitiesUpdateRangeSqr;
	}

	inline float CUpdateRelevanceContext::GetDefaultGridDistanceUpdate() const
	{
		return m_defaultGridDistanceUpdate;
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
	typedef TemplateUtils::CDelegate<void (const SUpdateContext&)> UpdateCallback;

	// Update filter.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	typedef TemplateUtils::CDelegate<bool ()> UpdateFilter;

	// Update statistics
	//////////////////////////////////////////////////////////////////////////
	namespace UpdateSchedulerStats
	{
		struct SUpdateStageStats
		{
			inline SUpdateStageStats()
				: beginPriority(0)
				, endPriority(0)
				, itemsToUpdateCount(0)
				, updatedItemsCount(0)
			{}

			inline SUpdateStageStats(UpdatePriority beginPriority_, UpdatePriority endPriority_)
				: beginPriority(beginPriority_)
				, endPriority(endPriority_)
				, itemsToUpdateCount(0)
				, updatedItemsCount(0)
			{}

			UpdatePriority beginPriority;
			UpdatePriority endPriority;

			size_t itemsToUpdateCount;
			size_t updatedItemsCount;
			CTimeValue updateTime;
		};

		struct IFrameUpdateStats
		{
			virtual ~IFrameUpdateStats() {}

			virtual const SUpdateStageStats* Get(size_t& outCount) const = 0;
		};
	}

	// Update scheduler interface.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	struct IUpdateScheduler
	{
		typedef std::function<bool(const Schematyc2::IObject& object)> UseRelevanceGridPredicate;
		typedef std::function<bool(const Schematyc2::IObject& object)> IsDynamicObjectPredicate;

		virtual ~IUpdateScheduler() {}

		virtual bool Connect(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency = EUpdateFrequency::EveryFrame, UpdatePriority priority = EUpdatePriority::Default, const UpdateFilter& filter = UpdateFilter()) = 0;
		virtual void Disconnect(CUpdateScope& scope) = 0;
		virtual bool ScopeDestroyed(CUpdateScope& scope) = 0;
		virtual bool InFrame() const = 0;
		virtual bool BeginFrame(float frameTime) = 0;
		virtual bool Update(UpdatePriority beginPriority = EUpdateStage::PrePhysics | EUpdateDistribution::Earliest, UpdatePriority endPriority = EUpdateStage::Post | EUpdateDistribution::End, CUpdateRelevanceContext* pRelevanceContext = nullptr) = 0;
		virtual bool EndFrame() = 0;
		virtual void VerifyCleanup() = 0;
		virtual void Reset() = 0;
		virtual const UpdateSchedulerStats::IFrameUpdateStats* GetFrameUpdateStats() const = 0;
		virtual void SetShouldUseRelevanceGridCallback(UseRelevanceGridPredicate useRelevanceGrid) = 0;
		virtual void SetIsDynamicObjectCallback(IsDynamicObjectPredicate isDynamicObject) = 0;
	};

	// Update scope functions.
	////////////////////////////////////////////////////////////////////////////////////////////////////

	inline CUpdateScope::CUpdateScope()
		: m_bIsBound(false)
		, m_pEntity(nullptr)
		, m_pSchematycObject(nullptr)
		, m_bOnlyLocalGrid(false)
	{
	}

	inline CUpdateScope::CUpdateScope(const IEntity* pEntity)
		: m_bIsBound(false)
		, m_pEntity(pEntity)
		, m_pSchematycObject(nullptr)
		, m_bOnlyLocalGrid(false)
	{
	}

	inline CUpdateScope::CUpdateScope(const IEntity* pEntity, const Schematyc2::IObject* pSchematycObject)
		: m_bIsBound(false)
		, m_pEntity(pEntity)
		, m_pSchematycObject(pSchematycObject)
		, m_bOnlyLocalGrid(false)
	{
	}

	inline CUpdateScope::~CUpdateScope()
	{
		if (m_bIsBound)
		{
			gEnv->pSchematyc2->GetUpdateScheduler().Disconnect(*this);
		}
		else if (m_pSchematycObject)
		{
			gEnv->pSchematyc2->GetUpdateScheduler().ScopeDestroyed(*this);
		}
	}	

	inline bool CUpdateScope::IsBound() const
	{
		return m_bIsBound;
	}

	inline const IEntity* CUpdateScope::GetEntity() const
	{
		return m_pEntity;
	}

	inline const Schematyc2::IObject* CUpdateScope::GetSchematycObj() const
	{
		return m_pSchematycObject;
	}

	inline bool CUpdateScope::IsOnlyLocalGrid() const
	{
		return m_bOnlyLocalGrid;
	}

	inline void CUpdateScope::SetExtraScopeInfo(const IEntity* pEntity, const Schematyc2::IObject* pSchematycObj, bool bOnlyLocalGrid)
	{
		m_pEntity = pEntity;
		m_pSchematycObject = pSchematycObj;
		m_bOnlyLocalGrid = bOnlyLocalGrid;
	}

	// Structure for recursive rule checking.
	////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename CONTEXT, typename NODE> struct SCheckRulesRecursive;

	template <typename CONTEXT, typename HEAD, typename TAIL> struct SCheckRulesRecursive<CONTEXT, NTypelist::CNode<HEAD, TAIL> >
	{
		inline bool operator () (CONTEXT context) const
		{
			return HEAD()(context) && SCheckRulesRecursive<CONTEXT, TAIL>()(context);
		}
	};

	template <typename CONTEXT, typename HEAD> struct SCheckRulesRecursive<CONTEXT, NTypelist::CNode<HEAD, NTypelist::CEnd> >
	{
		inline bool operator () (CONTEXT context) const
		{
			return HEAD()(context);
		}
	};

	// Configurable update filter.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	template <typename CONTEXT, typename RULES> class CConfigurableUpdateFilter
	{
	public:

		inline CConfigurableUpdateFilter()
			: m_context()
		{}

		inline UpdateFilter Init(CONTEXT context)
		{
			m_context = context;
			return UpdateFilter::FromConstMemberFunction<CConfigurableUpdateFilter, &CConfigurableUpdateFilter::Filter>(*this);
		}

		inline bool Filter() const
		{
			return SCheckRulesRecursive<CONTEXT, RULES>()(m_context);
		}

	protected:

		CONTEXT m_context;
	};
}
