// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
This file holds shared data and functionality across all actor using the 
HitDeathReactions system
-------------------------------------------------------------------------
History:
- 22:2:2010	13:01 : Created by David Ramos
*************************************************************************/
#pragma once
#ifndef __HIT_DEATH_REACTIONS_SYSTEM_H
#define __HIT_DEATH_REACTIONS_SYSTEM_H

#include "HitDeathReactionsDefs.h"
#include "CustomReactionFunctions.h"
#include <CryCore/Containers/VectorMap.h>
#include "ICryMannequin.h"

#include <Utility/CryHash.h>

struct IDefaultSkeleton;
class CActor;

#ifdef USER_stephenn
	#define DEBUG_MANQ_TAGS
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CHitDeathReactionsSystem
{
public:
	typedef std::multimap< string, string >		TTagToMapTag;

	static void														Warning(const char* szFormat, ...);


	CHitDeathReactionsSystem();
	~CHitDeathReactionsSystem();

	void																	OnToggleGameMode();
	void																	Reset();

	ProfileId															GetReactionParamsForActor(const CActor& actor, ReactionsContainerConstPtr& pHitReactions, ReactionsContainerConstPtr& pDeathReactions, ReactionsContainerConstPtr& pCollisionReactions, SHitDeathReactionsConfigConstPtr& pHitDeathReactionsConfig);
	void																	RequestReactionAnimsForActor(const CActor& actor, uint32 requestFlags);
	void																	ReleaseReactionAnimsForActor(const CActor& actor, uint32 requestFlags);

	void																	Reload();
	void																	PreloadData();
	void																	PreloadActorData(SmartScriptTable pActorPropertiesTable);

	void																	DumpHitDeathReactionsAssetUsage() const;

	ILINE bool														IsStreamingEnabled() const { return static_cast<uint8>(GetStreamingPolicy()) != 0U; }

	void																	GetMemoryUsage(ICrySizer * s) const;

	ILINE CCustomReactionFunctions&				GetCustomReactionFunctions() { return m_customReactionFunctions; }
	ILINE const CCustomReactionFunctions&	GetCustomReactionFunctions() const { return m_customReactionFunctions; }

	CMTRand_int32&												GetRandomGenerator() { return m_pseudoRandom; }

private:
	// private types
	struct SReactionsProfile
	{
		typedef std::map<EntityId, uint32> entitiesUsingProfileContainer;

		SReactionsProfile() : timerId(0), iRefCount(0) {}
		SReactionsProfile(ReactionsContainerConstPtr pHitReactions, ReactionsContainerConstPtr pDeathReactions, ReactionsContainerConstPtr pCollisionReactions, ScriptTablePtr pHitAndDeathReactionsTable, SHitDeathReactionsConfigConstPtr	pHitDeathReactionsConfig) : 
		pHitReactions(pHitReactions), pDeathReactions(pDeathReactions), pCollisionReactions(pCollisionReactions), pHitAndDeathReactionsTable(pHitAndDeathReactionsTable), pHitDeathReactionsConfig(pHitDeathReactionsConfig), timerId(0), iRefCount(0) {}
		~SReactionsProfile();

		void				GetMemoryUsage(ICrySizer * s) const;
		ILINE bool	IsValid() const 
		{
			return !(pHitReactions.expired() || pDeathReactions.expired() || pCollisionReactions.expired() || pHitDeathReactionsConfig.expired());
		}

		ScriptTablePtr												pHitAndDeathReactionsTable;

		ReactionsContainerConstWeakPtr				pHitReactions;
		ReactionsContainerConstWeakPtr				pDeathReactions;
		ReactionsContainerConstWeakPtr				pCollisionReactions;

		SHitDeathReactionsConfigConstWeakPtr	pHitDeathReactionsConfig;

		entitiesUsingProfileContainer					entitiesUsingProfile;
		int																		iRefCount;

		IGameFramework::TimerID								timerId;
	};

	struct SFailSafeProfile
	{
		ReactionsContainerConstPtr				pHitReactions;
		ReactionsContainerConstPtr				pDeathReactions;
		ReactionsContainerConstPtr				pCollisionReactions;

		SHitDeathReactionsConfigConstPtr	pHitDeathReactionsConfig;
	};

	struct STagMappingHelper
	{
		enum ETagType
		{
			ETagType_Part = 0,
			ETagType_HitType,
			ETagType_Weapon,
			ETagType_Projectile,
			ETagType_Collision,
			ETagType_Stances,

			ETagType_NUM
		};

		CHitDeathReactionsSystem::TTagToMapTag* m_tagMapping[ETagType_NUM];
		const CTagDefinition* m_pTagDefinition;

		explicit STagMappingHelper( const CTagDefinition* pTagDefinition )
			: m_pTagDefinition(pTagDefinition)
		{
			for( int i=0; i<ETagType_NUM; ++i )
			{
				m_tagMapping[i] = NULL;
			}
		}

		~STagMappingHelper()
		{
			for( int i=0; i<ETagType_NUM; ++i )
			{
				SAFE_DELETE( m_tagMapping[i] );
			}
		}
	};

	struct SPredGetMemoryUsage;
	struct SPredGetAnims;
	struct SPredRequestAnims;


	typedef std::map<ProfileId, SReactionsProfile>					ProfilesContainer;
	typedef ProfilesContainer::value_type										ProfilesContainersItem;
	typedef VectorMap<string, ScriptTablePtr>								FileToScriptTableMap;

	// Private methods
	void								ExecuteHitDeathReactionsScripts(bool bForceReload);
	ProfileId						GetActorProfileId(const CActor& actor) const;
	ScriptTablePtr			LoadReactionsScriptTable(const CActor& actor) const;
	ScriptTablePtr			LoadReactionsScriptTable(const char* szReactionsDataFile) const;
	bool								LoadHitDeathReactionsParams(const CActor& actor, ScriptTablePtr pHitDeathReactionsTable, SHitDeathReactionsConfigConstPtr pNewHitDeathReactionsConfig, ReactionsContainerPtr pHitReactions, ReactionsContainerPtr pDeathReactions, ReactionsContainerPtr pCollisionReactions);
	bool								LoadHitDeathReactionsConfig(const CActor& actor, ScriptTablePtr pHitDeathReactionsTable, SHitDeathReactionsConfigPtr pHitDeathReactionsConfig);
	void								LoadReactionsParams(const CActor& actor, const STagMappingHelper& tagMapping, IScriptTable* pHitDeathReactionsTable, SHitDeathReactionsConfigConstPtr pNewHitDeathReactionsConfig, const char* szReactionParamsName, bool bDeathReactions, ReactionId baseReactionId, int reactionType, ReactionsContainer& reactions);

	// Suppress passedByValue for smart pointers like ScriptTablePtr
	// cppcheck-suppress passedByValue
	void								GetReactionParamsFromScript(const CActor& actor, const STagMappingHelper& tagMapping, const ScriptTablePtr pScriptTable, SHitDeathReactionsConfigConstPtr pNewHitDeathReactionsConfig, SReactionParams& reactionParams, ReactionId reactionId) const;
	// cppcheck-suppress passedByValue
	bool								GetValidationParamsFromScript( const ScriptTablePtr pScriptTable, const STagMappingHelper& tagMapping, SReactionParams& reactionParams, const CActor& actor, ReactionId reactionId) const;
	// cppcheck-suppress passedByValue
	void								GetReactionAnimParamsFromScript(const CActor& actor, ScriptTablePtr pScriptTable, SReactionParams::SReactionAnim& reactionAnim) const;
	ILINE	uint8					GetStreamingPolicy() const { return m_streamingEnabled; }

	void								PreProcessStanceParams(SmartScriptTable pReactionTable) const;
	// cppcheck-suppress passedByValue
	void								FillAllowedPartIds(const CActor& actor, const ScriptTablePtr pScriptTable, const STagMappingHelper& tagMapping, bool bIsMannequinReaction, SReactionParams& reactionParams, SReactionParams::SValidationParams& validationParams) const;
	ECardinalDirection	GetCardinalDirectionFromString(const char* szCardinalDirection) const;
	EAirState						GetAirStateFromString(const char* szAirState) const;

	ILINE bool					FlagsValidateLocking(uint32 flags) const { return flags == ((eRRF_Alive | eRRF_AIEnabled) | (!gEnv->bMultiplayer * eRRF_OutFromPool)); }
	void								OnRequestAnimsTimer(void* pUserData, IGameFramework::TimerID handler);
	void								OnReleaseAnimsTimer(void* pUserData, IGameFramework::TimerID handler);
	void								GenerateDirectionalMannequinTagsFromReactionParams( const CActor& actor, const STagMappingHelper& tagMapping, const SReactionParams& reactionParams, TagState& tagState ) const;
	void								AddDirectionToTagState( const ECardinalDirection direction, const char* pPrefix, const STagMappingHelper& tagMapping, TagState& tagState ) const;
	template< typename FUNC >
	// cppcheck-suppress passedByValue
	void								FindAndSetTag( const ScriptTablePtr pScriptTable, const CHitDeathReactionsSystem::TTagToMapTag* pTagMap, const CTagDefinition* pTagDefinition, TagState& tagState, SReactionParams::IdContainer& container, const FUNC& functor ) const;

	void								GenerateTagMapping( ScriptTablePtr pTags, const char* pArrayName, const int tagType, STagMappingHelper& tagMappingHelper );
	void								LoadTagMapping( const CActor& actor, ScriptTablePtr pHitDeathReactionsTable, STagMappingHelper* pTagMappingHelper );

	ProfilesContainer 							m_reactionProfiles;
	SFailSafeProfile								m_failSafeProfile;

	CCustomReactionFunctions				m_customReactionFunctions;

	mutable FileToScriptTableMap		m_reactionsScriptTableCache;

	mutable CMTRand_int32						m_pseudoRandom;

	uint8														m_streamingEnabled;

#ifndef _RELEASE
	std::map<ProfileId, string>			m_profileIdToReactionFileMap; // debug attribute

	class CHitDeathReactionsDebugWidget;
	CHitDeathReactionsDebugWidget*	m_pWidget;
#endif // #ifndef _RELEASE
};

#endif // __HIT_DEATH_REACTIONS_SYSTEM_H
