// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "HitDeathReactionsSystem.h"
#include "HitDeathReactions.h"
#include <CryString/NameCRCHelper.h>
#include <CrySystem/ICryMiniGUI.h>
#include <CrySystem/Profilers/IPerfHud.h>

#include "Actor.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "Player.h"

// Unnamed namespace for constants
namespace
{
const char HIT_DEATH_REACTIONS_SCRIPT_FILE[] = "Scripts/GameRules/HitDeathReactions.lua";

const char REACTIONS_PRELOAD_LIST_FILE[] = "Libs/HitDeathReactionsData/ReactionsPreloadList.xml";
const char REACTIONS_PRELOAD_LIST_FILE_MP[] = "Libs/HitDeathReactionsData/ReactionsPreloadListMP.xml";
const char PRELOAD_CHARACTER_FILE[] = "characterFile";
const char PRELOAD_REACTIONS_FILE[] = "reactionsFile";

const char LOAD_XML_DATA_FUNCTION[] = "LoadXMLData";

const char REACTIONS_DATA_FILE_PROPERTY[] = "fileHitDeathReactionsParamsDataFile";
const char ACTOR_PROPERTIES_TABLE[] = "Properties";
const char ACTOR_PROPERTIES_DAMAGE_TABLE[] = "Damage";
const char ACTOR_PROPERTIES_DAMAGE_MAXHEALTH[] = "health";

const char ACTOR_HIT_DEATH_REACTIONS_PARAMS[] = "hitDeathReactionsParams";

// Config params strings
const char HIT_DEATH_REACTIONS_CONFIG[] = "HitDeathReactionsConfig";

const char COLLISION_BONE_PROPERTY[] = "collisionBone";
const char COLLISION_RADIUS_PROPERTY[] = "collisionRadius";
const char COLLISION_VERTICAL_OFFSET[] = "collisionVerticalOffset";
const char COLL_MAX_HORZ_ANGLE_PROPERTY[] = "collMaxHorzAngle";
const char COLL_MAX_MOV_ANGLE_PROPERTY[] = "collMaxMovAngle";
const char COLL_REACTION_START_DIST[] = "collReactionStartDist";
const char MAX_REACTION_TIME_PROPERTY[] = "maximumReactionTime";
const char MANQ_TARGET_TAG[] = "manqTargetTag";
const char MANQ_SLAVE_ADB[] = "animDatabaseSlave";
const char MAX_RAGDOLL_TIME[] = "maxRagdollTime";

// Reaction params strings
const char HIT_REACTIONS_PARAMS[] = "HitReactionParams";
const char DEATH_REACTIONS_PARAMS[] = "DeathReactionParams";
const char COLLISION_REACTIONS_PARAMS[] = "CollisionReactionParams";

const char VALIDATION_SECTION[] = "ValidationSection";
const char VALIDATION_FUNC_PROPERTY[] = "validationFunc";
const char REACTION_FUNC_PROPERTY[] = "reactionFunc";
const char REACTION_END_FUNC_PROPERTY[] = "reactionEndFunc";
const char AISIGNAL_PROPERTY[] = "AISignal";
const char MINIMUM_SPEED_PROPERTY[] = "minimumSpeed";
const char MAXIMUM_SPEED_PROPERTY[] = "maximumSpeed";
const char ALLOWED_PARTS_ARRAY[] = "AllowedParts";
const char MOVEMENT_DIRECTION_PROPERTY[] = "movementDirection";
const char SHOT_ORIGIN_PROPERTY[] = "shotOrigin";
const char PROBABILITY_PERCENT_PROPERTY[] = "probabilityPercent";
const char ALLOWED_STANCES_ARRAY[] = "AllowedStances";
const char ALLOWED_HIT_TYPES_ARRAY[] = "AllowedHitTypes";
const char ALLOWED_PROJECTILES_ARRAY[] = "AllowedProjectiles";
const char ALLOWED_WEAPONS_ARRAY[] = "AllowedWeapons";
const char SNAP_ORIENTATION_ANGLE[] = "snapOrientationAngle";
const char SNAP_TO_MOVEMENT_DIR[] = "snapToMovementDir";
const char MINIMUM_DAMAGE_PROPERTY[] = "minimumDamage";
const char MAXIMUM_DAMAGE_PROPERTY[] = "maximumDamage";
const char ONLY_ON_HEALTH_THRESHOLDS[] = "OnlyWhenPassingHealthThresholds";
const char RAGDOLL_ON_COLLISION_PROPERTY[] = "ragdollOnCollision";    // Does exactly the same that endReactionOnCollision, but is more descriptive for death reactions
const char COLLISION_CHECK_INTERSECTION_WITH_GROUND[] = "collisionCheckIntersectionWithGround";
const char REACTION_ON_COLLISION_PROPERTY[] = "reactionOnCollision";
const char PAUSE_AI_PROPERTY[] = "pauseAI";
const char ONLY_IF_USING_MOUNTED_ITEM_PROPERTY[] = "onlyIfUsingMountedItem";
const char AIR_STATE_PROPERTY[] = "airState";
const char DESTRUCTIBLE_EVENT_PROPERTY[] = "destructibleEvent";
const char MINIMUM_DISTANCE_PROPERTY[] = "minimumDistanceToShooter";
const char MAXIMUM_DISTANCE_PROPERTY[] = "maximumDistanceToShooter";
const char NO_RAGDOLL_ON_END_PROPERTY[] = "noRagdollOnEnd";
const char REACTION_FINISHES_AIMING_PROPERTY[] = "reactionFinishesAiming";
const char END_VELOCITY_PROPERTY[] = "endVelocity";

const char VARIATIONS_ARRAY[] = "Variations";
const char VARIATION_NAME[] = "name";
const char VARIATION_VALUE[] = "value";

const char REACTION_ANIM_NAME_PROPERTY[] = "animName";
const char REACTION_ANIM_PROPERTY[] = "ReactionAnim";
const char REACTION_ANIM_ADDITIVE_ANIM[] = "additive";
const char REACTION_ANIM_NO_ANIM_CAMERA[] = "noAnimCamera";
const char REACTION_ANIM_LAYER[] = "layer";
const char REACTION_ANIM_OVERRIDE_TRANS_TIME_TO_AG[] = "overrideTransTimeToAG";
const char ANIM_NAME_ARRAY[] = "AnimNames";
const char ANIM_NAME_PROPERTY[] = "name";
const char ANIM_VARIANTS_PROPERTY[] = "variants";

// Crymann stuff
const char MANQ_TAG_FRAGMENT[] = "hitDeath";
const char MANQ_TAG_FILENAME[] = "Animations/Mannequin/ADB/hitDeathTags.xml";
const char MANQ_REACTION[] = "manqReaction";

const float HYSTERESIS_REQUEST_TIMER_SECONDS = 0.5f;
const float HYSTERESIS_RELEASE_TIMER_SECONDS = 3.0f;

struct FAllowedParts
{
	FAllowedParts(ICharacterInstance* pCharInstance)
		: m_pCharInstance(pCharInstance) {}
	int operator()(const char* pName) const
	{
		int iPartId = m_pCharInstance->GetIDefaultSkeleton().GetJointIDByName(pName);

		// [*DavidR | 12/Nov/2009] ToDo: Log iPartId == -1 without spamming
		if (iPartId != -1)
		{
			return iPartId;
		}
		else
		{
			// perhaps it's an attachment?
			const int FIRST_ATTACHMENT_PARTID = 1000;

			IAttachmentManager* pAttachmentManager = m_pCharInstance->GetIAttachmentManager();
			int32 iAttachmentIdx = pAttachmentManager->GetIndexByName(pName);
			if (iAttachmentIdx != -1)
			{
				return(iAttachmentIdx + FIRST_ATTACHMENT_PARTID);
			}
		}

		return -1;
	}

private:
	ICharacterInstance* m_pCharInstance;
};

struct FAllowedHitTypes
{
	explicit FAllowedHitTypes(const CGameRules* pGameRules) : m_pGameRules(pGameRules){}
	int operator()(const char* pName) const
	{
		const int hitTypeID = m_pGameRules->GetHitTypeId(pName);
		if (hitTypeID > 0)
		{
			return hitTypeID;
		}
		return -1;
	}
private:
	const CGameRules* m_pGameRules;
};

struct FAllowedStances
{
	explicit FAllowedStances(IScriptSystem* piScriptSystem) : m_piScriptSystem(piScriptSystem){}
	int operator()(const char* pName) const
	{
		int iStance = -1;
		if (m_piScriptSystem->GetGlobalValue(pName, iStance))
		{
			return iStance;
		}
		return -1;
	}
private:
	IScriptSystem* m_piScriptSystem;

};

struct FAllowedNetClass
{
	explicit FAllowedNetClass(const IGameFramework* piGameFramework) : m_piGameFramework(piGameFramework){}
	int operator()(const char* pName) const
	{
		uint16 uClassId;
		if (g_pGame->GetIGameFramework()->GetNetworkSafeClassId(uClassId, pName))
		{
			return int(uClassId);
		}
		return -1;
	}

private:
	const IGameFramework* m_piGameFramework;
};

TagID GenerateMannequinTargetTag(const FragmentID fragID, const IActionController* piActionController, const char* pTargetTag)
{
	if (piActionController)
	{
		uint32 crc = CCrc32::ComputeLowercase(pTargetTag);

		return piActionController->GetFragTagID(fragID, crc);
	}
	return TAG_ID_INVALID;
}
}

// Suppress passedByValue for smart pointers like ScriptTablePtr
// cppcheck-suppress passedByValue
template void CHitDeathReactionsSystem::FindAndSetTag(const ScriptTablePtr pScriptTable, const CHitDeathReactionsSystem::TTagToMapTag* pTagMap, const CTagDefinition* pTagDefinition, TagState& tagState, SReactionParams::IdContainer& container, const FAllowedParts& functor) const;
// cppcheck-suppress passedByValue
template void CHitDeathReactionsSystem::FindAndSetTag(const ScriptTablePtr pScriptTable, const CHitDeathReactionsSystem::TTagToMapTag* pTagMap, const CTagDefinition* pTagDefinition, TagState& tagState, SReactionParams::IdContainer& container, const FAllowedHitTypes& functor) const;
// cppcheck-suppress passedByValue
template void CHitDeathReactionsSystem::FindAndSetTag(const ScriptTablePtr pScriptTable, const CHitDeathReactionsSystem::TTagToMapTag* pTagMap, const CTagDefinition* pTagDefinition, TagState& tagState, SReactionParams::IdContainer& container, const FAllowedStances& functor) const;
// cppcheck-suppress passedByValue
template void CHitDeathReactionsSystem::FindAndSetTag(const ScriptTablePtr pScriptTable, const CHitDeathReactionsSystem::TTagToMapTag* pTagMap, const CTagDefinition* pTagDefinition, TagState& tagState, SReactionParams::IdContainer& container, const FAllowedNetClass& functor) const;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactionsSystem::SReactionsProfile::~SReactionsProfile()
{
	if (timerId)
	{
		gEnv->pGameFramework->RemoveTimer(timerId);
		timerId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::SReactionsProfile::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));

	if (!pHitReactions.expired())
		s->AddObject(pHitReactions.lock().get());

	if (!pDeathReactions.expired())
		s->AddObject(pDeathReactions.lock().get());

	if (!pCollisionReactions.expired())
		s->AddObject(pCollisionReactions.lock().get());

	if (!pHitDeathReactionsConfig.expired())
		s->AddObject(pHitDeathReactionsConfig.lock().get());

	s->AddContainer(entitiesUsingProfile);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct CHitDeathReactionsSystem::SPredGetMemoryUsage
{
	SPredGetMemoryUsage(ICrySizer* s) : m_pCrySizer(s) {}
	void operator()(const ProfilesContainer::value_type& item)
	{
		item.second.GetMemoryUsage(m_pCrySizer);
	}

	ICrySizer* m_pCrySizer;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#ifndef _RELEASE
struct CHitDeathReactionsSystem::SPredGetAnims : public std::unary_function<void, const ReactionsContainer::value_type&>
{
	typedef std::set<uint32> AnimCRCsContainer;

	SPredGetAnims(AnimCRCsContainer& totalAnims, AnimCRCsContainer& usedAnims, uint& redundantAnimations, EntityId entityId, uint32& animationsSizeInMemory) :
		m_totalAnimIDs(totalAnims), m_usedAnimIDs(usedAnims),
		m_redundantAnimations(redundantAnimations), m_animationsSizeInMemory(animationsSizeInMemory), m_pAnimationSet(NULL)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		CRY_ASSERT((entityId == 0) || pEntity);
		ICharacterInstance* pCharInst = pEntity ? pEntity->GetCharacter(0) : NULL;
		m_pAnimationSet = pCharInst ? pCharInst->GetIAnimationSet() : NULL;
	}

	void operator()(const ReactionsContainer::value_type& item)
	{
		const SReactionParams::SReactionAnim& reactionAnim = *(item.reactionAnim);
		if (!reactionAnim.animCRCs.empty())
		{
			const uint previousTotalNumberAnims = m_totalAnimIDs.size();

			AnimCRCsContainer reactionAnims(reactionAnim.animCRCs.begin(), reactionAnim.animCRCs.end());
			m_totalAnimIDs.insert(reactionAnims.begin(), reactionAnims.end());

			const uint insertedAnims = m_totalAnimIDs.size() - previousTotalNumberAnims;
			m_redundantAnimations += reactionAnims.size() - insertedAnims;

			const int iNextIdx = reactionAnim.GetNextReactionAnimIndex();
			if (iNextIdx != -1)
			{
				const uint32 animCRC = reactionAnim.animCRCs[iNextIdx];
				m_usedAnimIDs.insert(animCRC);

				if (m_pAnimationSet)
				{
					int animID = m_pAnimationSet->GetAnimIDByCRC(animCRC);
					if (animID != -1)
						m_animationsSizeInMemory += m_pAnimationSet->GetAnimationSize(animID);
				}
			}
		}
	}

private:
	AnimCRCsContainer&   m_totalAnimIDs;
	AnimCRCsContainer&   m_usedAnimIDs;
	uint&                m_redundantAnimations;
	uint32&              m_animationsSizeInMemory;
	const IAnimationSet* m_pAnimationSet;
};

//////////////////////////////////////////////////////////////////////////
// PERFHUD Widget for showing stats about system streaming and stuff
//////////////////////////////////////////////////////////////////////////
class CHitDeathReactionsSystem::CHitDeathReactionsDebugWidget : public ICryPerfHUDWidget
{
public:
	virtual void Reset()                         {}
	virtual void LoadBudgets(XmlNodeRef perfXML) {}
	virtual void SaveStats(XmlNodeRef statsXML)  {}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	CHitDeathReactionsDebugWidget(minigui::IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud, CHitDeathReactionsSystem& hitDeathReactionsSystem) :
		m_hitDeathReactionsSystem(hitDeathReactionsSystem),
		m_pTable(NULL), m_pInfoBox(NULL)
	{
		bool bTableCtrl = true;
		const char MENU_ITEM_TITLE[] = "HitDeathReactions Streaming";

		if (bTableCtrl)
		{
			m_pTable = pPerfHud->CreateTableMenuItem(pParentMenu, MENU_ITEM_TITLE);
			CRY_ASSERT(m_pTable);

			m_pTable->AddColumn("HitDeathReactions profile/Entity name");
			m_pTable->AddColumn("Alive");
			m_pTable->AddColumn("AI Enabled");
			m_pTable->AddColumn("NotInPool");
		}
		else
		{
			m_pInfoBox = pPerfHud->CreateInfoMenuItem(pParentMenu, MENU_ITEM_TITLE, NULL, minigui::Rect(45, 350, 100, 400));
			CRY_ASSERT(m_pInfoBox);
		}

		pPerfHud->AddWidget(this);
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	void Update()
	{
		if (m_pTable)
			m_pTable->ClearTable();
		else
			m_pInfoBox->ClearEntries();

		std::for_each(m_hitDeathReactionsSystem.m_reactionProfiles.begin(), m_hitDeathReactionsSystem.m_reactionProfiles.end(), SPredPrintStreamingStats(*this));
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	bool ShouldUpdate()
	{
		return m_pTable ? !m_pTable->IsHidden() : !m_pInfoBox->IsHidden();
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	void Enable(int mode)
	{
		if (m_pTable)
			m_pTable->Hide(false);
		else
			m_pInfoBox->Hide(false);
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	void Disable()
	{
		if (m_pTable)
			m_pTable->Hide(true);
		else
			m_pInfoBox->Hide(false);
	}

private:
	struct SPredPrintStreamingStats : public std::unary_function<void, const ProfilesContainersItem&>
	{
		SPredPrintStreamingStats(CHitDeathReactionsDebugWidget& widget) : m_widget(widget) {}

		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		void operator()(const ProfilesContainersItem& profilePair) const
		{
			const ProfileId profileId = profilePair.first;
			const SReactionsProfile& profile = profilePair.second;
			if (profile.IsValid())
			{
				CryFixedStringT<256> text;

				const float fTextSize = 12.0f;

				ColorB textColor = Col_CadetBlue;
				std::map<ProfileId, string>::const_iterator itFind = m_widget.m_hitDeathReactionsSystem.m_profileIdToReactionFileMap.find(profileId);
				if (itFind != m_widget.m_hitDeathReactionsSystem.m_profileIdToReactionFileMap.end())
				{
					const bool bNewStreamingPolicy = (m_widget.m_hitDeathReactionsSystem.GetStreamingPolicy() == SCVars::eHDRSP_ActorsAliveAndNotInPool);

					// Print profile name
					const bool bEntitiesLockingAnims = !bNewStreamingPolicy || (profile.iRefCount > 0);
					if (bEntitiesLockingAnims)
						textColor = !profile.timerId ? Col_LightBlue : Col_DimGray;
					else
						textColor = !profile.timerId ? Col_DimGray : Col_LightBlue;

					text.Format("%s%s", bEntitiesLockingAnims ? "+" : "-", itFind->second.c_str());
					if (m_widget.m_pTable)
					{
						m_widget.m_pTable->AddData(eSTC_Name, textColor, text.c_str());
						m_widget.m_pTable->AddData(eSTC_Alive, textColor, "");
						m_widget.m_pTable->AddData(eSTC_AIProxyEnabled, textColor, "");
						m_widget.m_pTable->AddData(eSTC_OutOfEntityPool, textColor, "");
					}
					else
					{
						m_widget.m_pInfoBox->AddEntry(text.c_str(), textColor, fTextSize);
					}

					// Print info about entities locking profile's reaction anims
					if (bNewStreamingPolicy)
					{
						const char YES[] = "YES";
						const char NO[] = " NO";
						SReactionsProfile::entitiesUsingProfileContainer::const_iterator itEnd = profile.entitiesUsingProfile.end();
						for (SReactionsProfile::entitiesUsingProfileContainer::const_iterator it = profile.entitiesUsingProfile.begin(); it != itEnd; ++it)
						{
							textColor = m_widget.m_hitDeathReactionsSystem.FlagsValidateLocking(it->second) ? Col_LightBlue : Col_DimGray;
							textColor.ScaleCol(0.7f);

							CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(it->first));
							CRY_ASSERT(pActor);
							if (pActor)
							{
								const IAIObject* pAIObject = pActor->GetEntity()->GetAI();
								const IAIActorProxy* pAIProxy = pAIObject ? pAIObject->GetProxy() : NULL;

								if (m_widget.m_pTable)
								{
									const bool bAlive = (it->second & eRRF_Alive) != 0;
									const bool bAliveCorrect = bAlive == !pActor->IsDead();

									const bool bAIEnabled = (it->second & eRRF_AIEnabled) != 0;
									const bool bAIEnabledCorrect = bAIEnabled == (pAIProxy && pAIProxy->IsEnabled());

									const bool bNotInPool = (it->second & eRRF_OutFromPool) != 0;
									const bool bNotInPoolCorrect = bNotInPool == !pActor->IsPoolEntity();

									m_widget.m_pTable->AddData(eSTC_Name, textColor, "  %s", pActor->GetEntity()->GetName());
									m_widget.m_pTable->AddData(eSTC_Alive, bAliveCorrect ? textColor : ColorB(Col_Red), bAlive ? YES : NO);
									m_widget.m_pTable->AddData(eSTC_AIProxyEnabled, bAIEnabledCorrect ? textColor : ColorB(Col_Red), bAIEnabled ? YES : NO);
									m_widget.m_pTable->AddData(eSTC_OutOfEntityPool, bNotInPoolCorrect ? textColor : ColorB(Col_Red), bNotInPool ? YES : NO);
								}
								else
								{
									text.Format("  %s -- Alive[%s] -- AIProxy[%s] -- Pool[%s]", pActor->GetEntity()->GetName(),
									            !pActor->IsDead() ? YES : NO, (pAIProxy && pAIProxy->IsEnabled()) ? YES : NO, !pActor->IsPoolEntity() ? NO : YES);
									m_widget.m_pInfoBox->AddEntry(text.c_str(), textColor, fTextSize);
								}
							}
						}
					}
				}
			}
		}

	private:
		enum EStatsTableColumn
		{
			eSTC_Name = 0,
			eSTC_Alive,
			eSTC_AIProxyEnabled,
			eSTC_OutOfEntityPool,
		};

		CHitDeathReactionsDebugWidget& m_widget;
	};

	minigui::IMiniTable*      m_pTable;
	minigui::IMiniInfoBox*    m_pInfoBox;
	CHitDeathReactionsSystem& m_hitDeathReactionsSystem;
};

#endif // #ifndef _RELEASE

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct CHitDeathReactionsSystem::SPredRequestAnims : public std::unary_function<void, const ReactionsContainer::value_type&>
{
	SPredRequestAnims(bool bRequest, EntityId entityId, const IAnimationDatabase* piAnimDB)
		: m_bRequest(bRequest)
		, m_pAnimSet(NULL)
		, m_pActionController(NULL)
		, m_pOptionalAnimDB(NULL)
	{
		if (bRequest)
		{
			IActor* piActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(entityId);
			if (piActor)
			{
				IEntity* pEntity = piActor->GetEntity();
				ICharacterInstance* pCharInst = pEntity ? pEntity->GetCharacter(0) : NULL;
				m_pAnimSet = pCharInst ? pCharInst->GetIAnimationSet() : NULL;
				m_pActionController = piActor->GetAnimatedCharacter()->GetActionController();
				CRY_ASSERT(m_pAnimSet);
				CRY_ASSERT(m_pActionController);
			}
		}
	}

	void operator()(const ReactionsContainer::value_type& item)
	{
		SReactionParams::SReactionAnim& reactionAnim = *(item.reactionAnim);
		if (!reactionAnim.animCRCs.empty())
		{
			if (m_bRequest)
			{
				if (m_pAnimSet && (reactionAnim.GetNextReactionAnimIndex() == -1))
					reactionAnim.RequestNextAnim(m_pAnimSet);
			}
			else
			{
				reactionAnim.ReleaseRequestedAnims();
			}
		}
		else if (item.mannequinData.actionType != SReactionParams::SMannequinData::EActionType_Invalid)
		{
			SReactionParams::SMannequinData& mannequinData = const_cast<SReactionParams::SMannequinData&>(item.mannequinData);
			if (m_bRequest)
			{
				if (m_pActionController)
				{
					mannequinData.Initialize(m_pActionController);
					if (m_pOptionalAnimDB)
					{
						mannequinData.AddDB(m_pActionController, m_pOptionalAnimDB);
					}
				}
			}
			else
			{
				mannequinData.ReleaseRequestedAnims();
			}
		}
	}

private:
	bool                      m_bRequest;
	const IAnimationSet*      m_pAnimSet;
	const IActionController*  m_pActionController;
	const IAnimationDatabase* m_pOptionalAnimDB;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
inline void AnimIDError(const char* animName)
{
#ifndef _RELEASE
	if (g_pGameCVars->g_animatorDebug)
	{
		static const ColorF col(1.0f, 0.0f, 0.0f, 1.0f);
		g_pGame->GetIGameFramework()->GetIPersistantDebug()->Add2DText(string().Format("Missing %s", animName).c_str(), 1.0f, col, 10.0f);
	}

	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[CHitDeathReactions] Missing anim: %s", animName);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::Warning(const char* szFormat, ...)
{
#ifndef _RELEASE
	if (!gEnv || !gEnv->pSystem || !szFormat)
		return;

	va_list args;
	va_start(args, szFormat);
	GetISystem()->WarningV(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, 0, 0, (string("[CHitDeathReactions] ") + szFormat).c_str(), args);
	va_end(args);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactionsSystem::CHitDeathReactionsSystem() : m_streamingEnabled(g_pGameCVars->g_hitDeathReactions_streaming)
{
	m_failSafeProfile.pHitReactions.reset(new ReactionsContainer);
	m_failSafeProfile.pDeathReactions.reset(new ReactionsContainer);
	m_failSafeProfile.pCollisionReactions.reset(new ReactionsContainer);
	m_failSafeProfile.pHitDeathReactionsConfig.reset(new SHitDeathReactionsConfig);

	// Execute scripts
	ExecuteHitDeathReactionsScripts(false);

#ifndef _RELEASE
	m_pWidget = NULL;
	ICryPerfHUD* pPerfHUD = gEnv->pSystem->GetPerfHUD();
	if (pPerfHUD)
	{
		minigui::IMiniCtrl* pGameMenu = pPerfHUD->GetMenu("Game");
		if (!pGameMenu)
			pGameMenu = pPerfHUD->CreateMenu("Game");

		m_pWidget = new CHitDeathReactionsDebugWidget(pGameMenu, pPerfHUD, *this);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CHitDeathReactionsSystem::~CHitDeathReactionsSystem()
{
#ifndef _RELEASE
	if (m_pWidget)
		gEnv->pSystem->GetPerfHUD()->RemoveWidget(m_pWidget);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::OnToggleGameMode()
{
	PreloadData();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::Reset()
{
	stl::free_container(m_reactionProfiles);

#ifndef _RELEASE
	stl::free_container(m_profileIdToReactionFileMap);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
ProfileId CHitDeathReactionsSystem::GetReactionParamsForActor(const CActor& actor, ReactionsContainerConstPtr& pHitReactions, ReactionsContainerConstPtr& pDeathReactions, ReactionsContainerConstPtr& pCollisionReactions, SHitDeathReactionsConfigConstPtr& pHitDeathReactionsConfig)
{
	bool bSuccess = false;

	ProfileId profileId = GetActorProfileId(actor);
	if (profileId != INVALID_PROFILE_ID)
	{
		ProfilesContainer::iterator itFind = m_reactionProfiles.find(profileId);
		if (itFind != m_reactionProfiles.end())
		{
			const SReactionsProfile& sharedReactions = itFind->second;
			if (sharedReactions.IsValid())
			{
				actor.GetEntity()->GetScriptTable()->SetValue(ACTOR_HIT_DEATH_REACTIONS_PARAMS, sharedReactions.pHitAndDeathReactionsTable);

				pHitReactions = sharedReactions.pHitReactions.lock();
				pDeathReactions = sharedReactions.pDeathReactions.lock();
				pCollisionReactions = sharedReactions.pCollisionReactions.lock();
				pHitDeathReactionsConfig = sharedReactions.pHitDeathReactionsConfig.lock();

				bSuccess = true;
				return profileId;
			}
			else
			{
				m_reactionProfiles.erase(itFind);
			}
		}

		// Instantiate new params
		{
#ifndef _RELEASE
			ICharacterInstance* pMainChar = actor.GetEntity()->GetCharacter(0);
			if (pMainChar)
			{
				ScriptTablePtr pActorScriptTable = actor.GetEntity()->GetScriptTable();
				CRY_ASSERT(pActorScriptTable.GetPtr());

				ScriptAnyValue propertiesTable;
				pActorScriptTable->GetValueAny(ACTOR_PROPERTIES_TABLE, propertiesTable);

				const char* szReactionsDataFilePath = NULL;
				if (propertiesTable.GetType() == EScriptAnyType::Table)
				{
					propertiesTable.GetScriptTable()->GetValue(REACTIONS_DATA_FILE_PROPERTY, szReactionsDataFilePath);
				}

				if (g_pGameCVars->g_hitDeathReactions_logReactionAnimsOnLoading && szReactionsDataFilePath)
				{
					const char* szFilePath = pMainChar->GetIDefaultSkeleton().GetModelFilePath();
					CryLogAlways("[HitDeathReactionsSystem] Instancing %s (%s)", szReactionsDataFilePath, szFilePath);
				}

				m_profileIdToReactionFileMap.insert(std::make_pair(profileId, szReactionsDataFilePath));
			}
#endif

			// Seed the random generator with the key obtained for this reaction params instance
			m_pseudoRandom.seed(gEnv->bNoRandomSeed ? 0 : profileId);

			MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "HitDeathReactions_SharedData");

			// Instantiate the shared reaction containers
			ReactionsContainerPtr pNewHitReactions(new ReactionsContainer);
			ReactionsContainerPtr pNewDeathReactions(new ReactionsContainer);
			ReactionsContainerPtr pNewCollisionReactions(new ReactionsContainer);
			SHitDeathReactionsConfigPtr pNewHitDeathReactionsConfig(new SHitDeathReactionsConfig);

			// Fill death and hit reactions params script table
			ScriptTablePtr hitAndDeathReactions = LoadReactionsScriptTable(actor);
			if (hitAndDeathReactions)
			{
				// Parse configuration struct
				LoadHitDeathReactionsConfig(actor, hitAndDeathReactions, pNewHitDeathReactionsConfig);

				// Parse and create hit and death reactions params
				LoadHitDeathReactionsParams(actor, hitAndDeathReactions, pNewHitDeathReactionsConfig, pNewHitReactions, pNewDeathReactions, pNewCollisionReactions);

				// Insert it on the pool
				ProfilesContainersItem newProfile(profileId, SReactionsProfile(pNewHitReactions, pNewDeathReactions, pNewCollisionReactions, hitAndDeathReactions, pNewHitDeathReactionsConfig));
				bSuccess = m_reactionProfiles.insert(newProfile).second;
				CRY_ASSERT(bSuccess);

				actor.GetEntity()->GetScriptTable()->SetValue(ACTOR_HIT_DEATH_REACTIONS_PARAMS, newProfile.second.pHitAndDeathReactionsTable);
			}
			else
				Warning("Couldn't load the reactions table for actor %s", actor.GetEntity()->GetName());

			// if the process failed these will be empty
			pHitReactions = pNewHitReactions;
			pDeathReactions = pNewDeathReactions;
			pCollisionReactions = pNewCollisionReactions;
			pHitDeathReactionsConfig = pNewHitDeathReactionsConfig;
		}
	}
	else
	{
		Warning("Couldn't get unique key for actor %s's reactions. This actor won't have any hit/death reactions", actor.GetEntity()->GetName());

		// we will still write a valid pointer on the passed-by-ref pointers, the caller assume they as valid
		pHitReactions = m_failSafeProfile.pHitReactions;
		pDeathReactions = m_failSafeProfile.pDeathReactions;
		pCollisionReactions = m_failSafeProfile.pCollisionReactions;
		pHitDeathReactionsConfig = m_failSafeProfile.pHitDeathReactionsConfig;
	}

	return bSuccess ? profileId : INVALID_PROFILE_ID;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::RequestReactionAnimsForActor(const CActor& actor, uint32 requestFlags)
{
	if (GetStreamingPolicy() == SCVars::eHDRSP_ActorsAliveAndNotInPool)
	{
		ProfileId profileId = GetActorProfileId(actor);
		if (profileId != INVALID_PROFILE_ID)
		{
			ProfilesContainer::iterator itFind = m_reactionProfiles.find(profileId);
			if (itFind != m_reactionProfiles.end())
			{
				SReactionsProfile& profile = itFind->second;
				if (profile.IsValid())
				{
					const EntityId entityId = actor.GetEntityId();
					SReactionsProfile::entitiesUsingProfileContainer::iterator itEnt = profile.entitiesUsingProfile.find(entityId);
					if (itEnt == profile.entitiesUsingProfile.end())
					{
						itEnt = (profile.entitiesUsingProfile.insert(std::make_pair(entityId, 0))).first;
					}

					const uint32 oldFlags = itEnt->second;
					const bool bReactionAnimsWereLocked = FlagsValidateLocking(oldFlags);
					itEnt->second = oldFlags | requestFlags;

					if (!bReactionAnimsWereLocked && FlagsValidateLocking(itEnt->second))
					{
						if (profile.iRefCount == 0)
						{
							// Instead of immediately requesting the animations, we allow some hysteresis by delaying the
							// actual request some time (HYSTERESIS_REQUEST_TIMER_SECONDS). This is to avoid rapid changes
							// that could lead to expensive request/release/request... sequences on a short period of time

							// The logic assumes the request/release timer requests are interleaved, so if there's a timer already
							// present that means this request is actually neutralizing a previous release
							if (profile.timerId)
							{
								gEnv->pGameFramework->RemoveTimer(profile.timerId);
								profile.timerId = 0;
							}
							else
							{
								profile.timerId = gEnv->pGameFramework->AddTimer(CTimeValue(HYSTERESIS_REQUEST_TIMER_SECONDS),
								                                                             false, functor(*this, &CHitDeathReactionsSystem::OnRequestAnimsTimer), reinterpret_cast<void*>(static_cast<uintptr_t>(profileId)));
							}
						}

						++profile.iRefCount;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::ReleaseReactionAnimsForActor(const CActor& actor, uint32 requestFlags)
{
	if (GetStreamingPolicy() == SCVars::eHDRSP_ActorsAliveAndNotInPool)
	{
		ProfileId profileId = GetActorProfileId(actor);
		if (profileId != INVALID_PROFILE_ID)
		{
			ProfilesContainer::iterator itFind = m_reactionProfiles.find(profileId);
			if (itFind != m_reactionProfiles.end())
			{
				SReactionsProfile& profile = itFind->second;
				if (profile.IsValid())
				{
					const EntityId entityId = actor.GetEntityId();
					SReactionsProfile::entitiesUsingProfileContainer::iterator itEnt = profile.entitiesUsingProfile.find(entityId);
					if (itEnt != profile.entitiesUsingProfile.end())
					{
						const uint32 oldFlags = itEnt->second;
						const bool bReactionAnimsWereLocked = FlagsValidateLocking(oldFlags);
						itEnt->second = oldFlags & ~requestFlags;

						if (bReactionAnimsWereLocked && !FlagsValidateLocking(itEnt->second))
						{
							CRY_ASSERT(profile.iRefCount > 0);

							if (profile.iRefCount == 1)
							{
								// Instead of immediately releasing the animations, we allow some hysteresis by delaying the
								// actual release some time (HYSTERESIS_RELEASE_TIMER_SECONDS). This is to avoid rapid changes
								// that could lead to expensive request/release/request... sequences on a short period of time

								// The logic assumes the request/release timer requests are interleaved, so if there's a timer already
								// present that means this release is actually neutralizing a previous request
								if (profile.timerId)
								{
									gEnv->pGameFramework->RemoveTimer(profile.timerId);
									profile.timerId = 0;
								}
								else
								{
									profile.timerId = gEnv->pGameFramework->AddTimer(CTimeValue(HYSTERESIS_RELEASE_TIMER_SECONDS),
									                                                             false, functor(*this, &CHitDeathReactionsSystem::OnReleaseAnimsTimer), reinterpret_cast<void*>(static_cast<uintptr_t>(profileId)));
								}
							}

							--profile.iRefCount;
						}

						if (itEnt->second == 0)
						{
							profile.entitiesUsingProfile.erase(entityId);
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Reload the data structure and scripts
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::Reload()
{
	ExecuteHitDeathReactionsScripts(true);

	m_reactionProfiles.clear();
	m_reactionsScriptTableCache.clear();

	m_streamingEnabled = g_pGameCVars->g_hitDeathReactions_streaming;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::PreloadData()
{
	// Clear the existing cache ready for reload
	stl::free_container(m_reactionsScriptTableCache);

	m_streamingEnabled = g_pGameCVars->g_hitDeathReactions_streaming;

	const char* preloadList = gEnv->bMultiplayer ? REACTIONS_PRELOAD_LIST_FILE_MP : REACTIONS_PRELOAD_LIST_FILE;
	// Cache the reaction params script tables from the XML data files which filepath is
	// specified on the preload list
	XmlNodeRef xmlNode = GetISystem()->LoadXmlFromFile(preloadList);
	if (xmlNode)
	{
		const int iEntries = xmlNode->getChildCount();
		for (int i = 0; i < iEntries; ++i)
		{
			const XmlNodeRef pairElement = xmlNode->getChild(i);
			if (pairElement->haveAttr(PRELOAD_REACTIONS_FILE))
			{
				const char* pReactionsFile = pairElement->getAttr(PRELOAD_REACTIONS_FILE);
				LoadReactionsScriptTable(pReactionsFile);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::PreloadActorData(SmartScriptTable pActorPropertiesTable)
{
	if (!pActorPropertiesTable)
		return;

	const char* szReactionsDataFilePath = NULL;
	pActorPropertiesTable->GetValue(REACTIONS_DATA_FILE_PROPERTY, szReactionsDataFilePath);

	if (!szReactionsDataFilePath || strcmp(szReactionsDataFilePath, "") == 0)
		return;

	ScriptTablePtr pHitDeathReactionsTable = LoadReactionsScriptTable(szReactionsDataFilePath);
	if (!pHitDeathReactionsTable)
		return;

	ScriptTablePtr pReactionsConfigTable;
	if (!pHitDeathReactionsTable->GetValue(HIT_DEATH_REACTIONS_CONFIG, pReactionsConfigTable))
		return;

	const char* szAnimDatabase = NULL;
	if (pReactionsConfigTable->GetValue(MANQ_SLAVE_ADB, szAnimDatabase) && szAnimDatabase)
	{
		IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
		mannequinSys.GetAnimationDatabaseManager().Load(szAnimDatabase);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "HitDeathReactionsSystem");

	s->AddObject(this, sizeof(*this));
	std::for_each(m_reactionProfiles.begin(), m_reactionProfiles.end(), SPredGetMemoryUsage(s));

	{
		SIZER_SUBCOMPONENT_NAME(s, "HitDeathReactionInstances");
		IActorIteratorPtr pIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
		while (IActor* pIActor = pIt->Next())
		{
			if (pIActor->GetActorClass() == CPlayer::GetActorClassType())
			{
				CPlayer* pActor = static_cast<CPlayer*>(pIActor);
				CHitDeathReactionsPtr pHitDeathReactions = pActor->GetHitDeathReactions();
				if (pHitDeathReactions)
					s->AddObject(pHitDeathReactions);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::DumpHitDeathReactionsAssetUsage() const
{
#ifndef _RELEASE
	uint totalAnimsCount = 0;
	uint usedAnimsCount = 0;

	const bool bStreaming = IsStreamingEnabled();
	CryLogAlways("[HitDeathReactionsSystem] Logging stats for reaction anims. Streaming: %s", bStreaming ? "enabled" : "disabled");
	ProfilesContainer::const_iterator itEnd = m_reactionProfiles.end();
	for (ProfilesContainer::const_iterator it = m_reactionProfiles.begin(); it != itEnd; ++it)
	{
		const SReactionsProfile& profile = it->second;
		if (profile.IsValid())
		{
			EntityId entityUsingProfile = 0;
			if (profile.iRefCount > 0)
			{
				// grab the first EntityId locking the profile reaction anims
				SReactionsProfile::entitiesUsingProfileContainer::const_iterator iterEnd = profile.entitiesUsingProfile.end();
				for (SReactionsProfile::entitiesUsingProfileContainer::const_iterator iter = profile.entitiesUsingProfile.begin(); (iter != iterEnd) && (entityUsingProfile == 0); ++iter)
					if (FlagsValidateLocking(iter->second))
						entityUsingProfile = iter->first;
			}

			uint redundantAnims = 0;
			uint32 reactionAnimsSizeInMemory = 0;
			SPredGetAnims::AnimCRCsContainer profileTotalAnims;
			SPredGetAnims::AnimCRCsContainer profileUsedAnims;
			SPredGetAnims getAnimsFunctor(profileTotalAnims, profileUsedAnims, redundantAnims, entityUsingProfile, reactionAnimsSizeInMemory);
			std::for_each(profile.pHitReactions.lock()->begin(), profile.pHitReactions.lock()->end(), getAnimsFunctor);
			std::for_each(profile.pDeathReactions.lock()->begin(), profile.pDeathReactions.lock()->end(), getAnimsFunctor);
			std::for_each(profile.pCollisionReactions.lock()->begin(), profile.pCollisionReactions.lock()->end(), getAnimsFunctor);

			const int iUsedAnims = profileUsedAnims.size();
			const int iTotalAnims = profileTotalAnims.size();
			std::map<ProfileId, string>::const_iterator itFind = m_profileIdToReactionFileMap.find(it->first);
			CRY_ASSERT(itFind != m_profileIdToReactionFileMap.end());
			const string& sReactionsProfile = (itFind != m_profileIdToReactionFileMap.end()) ? itFind->second : string(CryStackStringT<char, 9>().Format("%X", it->first));
			if (bStreaming)
			{
				CryLogAlways("[HitDeathReactionsSystem] %s has %i/%i reaction animations loaded. Redundant anims: %u. Size in memory: %u KiB", sReactionsProfile.c_str(), iUsedAnims, iTotalAnims, redundantAnims, reactionAnimsSizeInMemory / 1024U);
			}
			else
			{
				CryLogAlways("[HitDeathReactionsSystem] %s has %i reaction animations loaded. Redundant anims: %u. Size in memory: %u KiB", sReactionsProfile.c_str(), iTotalAnims, redundantAnims, reactionAnimsSizeInMemory / 1024U);
			}

			totalAnimsCount += profileTotalAnims.size();
			usedAnimsCount += profileUsedAnims.size();
		}
	}

	if (bStreaming)
	{
		CryLogAlways("[HitDeathReactionsSystem] Total usage stats: %i/%i assets (Saving memory for %i variations)", usedAnimsCount, totalAnimsCount, totalAnimsCount - usedAnimsCount);
	}
	else
	{
		CryLogAlways("[HitDeathReactionsSystem] Total usage stats: %i assets", totalAnimsCount);
	}

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::ExecuteHitDeathReactionsScripts(bool bForceReload)
{
	if (!gEnv->pScriptSystem->ExecuteFile(HIT_DEATH_REACTIONS_SCRIPT_FILE, true, bForceReload))
		Warning("Error executing HitDeathReactions script file");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
ProfileId CHitDeathReactionsSystem::GetActorProfileId(const CActor& actor) const
{
	// Reaction params are dependent on:
	// - Reaction params data file
	CRY_ASSERT(actor.GetActorClass() == CPlayer::GetActorClassType());

	CHitDeathReactionsConstPtr pHitDeathReactions = static_cast<const CPlayer&>(actor).GetHitDeathReactions();
#ifndef _DEBUG
	// Get cached profileId from the Actor's HitDeathReaction object
	ProfileId key = (pHitDeathReactions != NULL) ? pHitDeathReactions->GetProfileId() : INVALID_PROFILE_ID;
	if (key != INVALID_PROFILE_ID)
	{
		return key;
	}
	else
#else
	ProfileId key = INVALID_PROFILE_ID;
#endif
	{
		// Check for animated character validity too. Is as basic as having a character instance for the HitDeathReactions
		// This way the CHitDeathReactions class can assume the AnimatedCharacter pointer of its actor is always valid as
		// long as it has reactions
		const IAnimatedCharacter* pAnimChar = actor.GetAnimatedCharacter();
		if (pAnimChar)
		{
			ScriptTablePtr pActorScriptTable = actor.GetEntity()->GetScriptTable();
			CRY_ASSERT(pActorScriptTable.GetPtr());

			ScriptAnyValue propertiesTable;
			pActorScriptTable->GetValueAny(ACTOR_PROPERTIES_TABLE, propertiesTable);

			const char* szReactionsDataFilePath = NULL;
			if ((propertiesTable.GetType() == EScriptAnyType::Table) && propertiesTable.GetScriptTable()->GetValue(REACTIONS_DATA_FILE_PROPERTY, szReactionsDataFilePath))
			{
				CryPathString sReactionsDataFilePath(szReactionsDataFilePath);
				PathUtil::UnifyFilePath(sReactionsDataFilePath);

				key = CCrc32::Compute(sReactionsDataFilePath);
			}
			else
			{
				Warning("Couldn't find %s field on %s properties table", REACTIONS_DATA_FILE_PROPERTY, actor.GetEntity()->GetName());
			}
		}
		else
		{
			Warning("Couldn't obtain an Animated Character object on actor %s while calculating its unique key", actor.GetEntity()->GetName());
		}
	}

#ifdef _DEBUG
	if (pHitDeathReactions != NULL)
	{
		ProfileId cachedProfileId = pHitDeathReactions->GetProfileId();
		CRY_ASSERT_TRACE((cachedProfileId == INVALID_PROFILE_ID) || (cachedProfileId == key), ("IMPORTANT ASSERT! %s actor's cached ProfileId doesn't match its actual ProfileId!", actor.GetEntity()->GetName()));
	}
#endif

	return key;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
ScriptTablePtr CHitDeathReactionsSystem::LoadReactionsScriptTable(const CActor& actor) const
{
	ScriptTablePtr pActorScriptTable = actor.GetEntity()->GetScriptTable();
	CRY_ASSERT(pActorScriptTable.GetPtr());

	ScriptAnyValue propertiesTable;
	pActorScriptTable->GetValueAny(ACTOR_PROPERTIES_TABLE, propertiesTable);

	const char* szReactionsDataFilePath = NULL;
	if ((propertiesTable.GetType() == EScriptAnyType::Table) && propertiesTable.GetScriptTable()->GetValue(REACTIONS_DATA_FILE_PROPERTY, szReactionsDataFilePath))
	{
		CryPathString sReactionsDataFile(szReactionsDataFilePath);
		PathUtil::UnifyFilePath(sReactionsDataFile);

		return LoadReactionsScriptTable(sReactionsDataFile.c_str());
	}
	else
		return NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
ScriptTablePtr CHitDeathReactionsSystem::LoadReactionsScriptTable(const char* szReactionsDataFile) const
{
	CryPathString sReactionsDataFile(szReactionsDataFile);
	PathUtil::UnifyFilePath(sReactionsDataFile);

	ScriptTablePtr reactionsParamsTable;
	FileToScriptTableMap::const_iterator itFind = m_reactionsScriptTableCache.find(CONST_TEMP_STRING(sReactionsDataFile.c_str()));
	if (itFind == m_reactionsScriptTableCache.end())
	{
		SmartScriptTable pHitDeathReactionsTable;
		HSCRIPTFUNCTION loadXMLDataFnc = NULL;
		if (gEnv->pScriptSystem->GetGlobalValue(HIT_DEATH_REACTIONS_SCRIPT_TABLE, pHitDeathReactionsTable) &&
		    pHitDeathReactionsTable->GetValue(LOAD_XML_DATA_FUNCTION, loadXMLDataFnc))
		{
			MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "HitDeathReactions_SharedReactionTables");

			// [*DavidR | 23/Jun/2010] ToDo: We should expose CryAction's XMLLoadScript functionality so it can be used outside
			// that project. The only way to use it currently is through lua binds, hence the following call
			Script::CallReturn(gEnv->pScriptSystem, loadXMLDataFnc, pHitDeathReactionsTable, sReactionsDataFile.c_str(), reactionsParamsTable);
			if (reactionsParamsTable)
			{
				m_reactionsScriptTableCache.insert(std::make_pair(sReactionsDataFile.c_str(), reactionsParamsTable));
			}
		}
	}
	else
		reactionsParamsTable = itFind->second;

	return reactionsParamsTable;
}

//////////////////////////////////////////////////////////////////////////

void CHitDeathReactionsSystem::GenerateTagMapping(ScriptTablePtr pTags, const char* pArrayName, const int tagType, STagMappingHelper& tagMappingHelper)
{
	CRY_ASSERT_MESSAGE(tagType < STagMappingHelper::ETagType_NUM, "TagType index out of range!");

	const char* pTagName = NULL;
	if (pTags->GetValue(VARIATION_VALUE, pTagName))
	{
		{
			ScriptTablePtr pAllowedParts;
			pTags->GetValue(pArrayName, pAllowedParts);

			TTagToMapTag* pNameToTags = NULL;
			if (tagMappingHelper.m_tagMapping[tagType] != NULL)
			{
				pNameToTags = tagMappingHelper.m_tagMapping[tagType];
			}
			else
			{
				pNameToTags = new TTagToMapTag();
			}

			// Generate the name to tag map.
			const int count = pAllowedParts->Count();
			for (int i = 0; i < count; ++i)
			{
				const char* pName = NULL;
				if (pAllowedParts->GetAt(i + 1, pName))
				{
					// multimap so inserting on the same tag is safe!
					pNameToTags->insert(TTagToMapTag::value_type(pTagName, pName));

#ifdef DEBUG_MANQ_TAGS
					Warning("  Mapping <%s> from <%s>", pTagName, pName);
#endif
				}
			}

			if (tagMappingHelper.m_tagMapping[tagType] == NULL)
			{
				if (pNameToTags->empty())
				{
					SAFE_DELETE(pNameToTags);
				}
				else
				{
					tagMappingHelper.m_tagMapping[tagType] = pNameToTags;
				}
			}
		}
	}
}

void CHitDeathReactionsSystem::LoadTagMapping(const CActor& actor, ScriptTablePtr pHitDeathReactionsTable, STagMappingHelper* pTagMappingHelper)
{
	ScriptTablePtr pTagMap;
	if (pHitDeathReactionsTable->GetValue("TagMap", pTagMap))
	{
		IScriptTable::Iterator it = pTagMap->BeginIteration();

		for (; pTagMap->MoveNext(it); )
		{
			CRY_ASSERT(it.value.GetType() == EScriptAnyType::Table);

			ScriptTablePtr pTags = it.value.GetScriptTable();

			if (pTags->HaveValue(ALLOWED_PARTS_ARRAY))
			{
				// Find Part Mapping.
				ICharacterInstance* pMainChar = actor.GetEntity()->GetCharacter(0);
				ISkeletonPose* pSkeletonPose = pMainChar ? pMainChar->GetISkeletonPose() : NULL;

				if (pSkeletonPose)
				{
#ifdef DEBUG_MANQ_TAGS
					Warning("Generating TagMap For Parts");
#endif

					GenerateTagMapping(pTags, ALLOWED_PARTS_ARRAY, STagMappingHelper::ETagType_Part, *pTagMappingHelper);
				}
			}

			if (pTags->HaveValue(ALLOWED_HIT_TYPES_ARRAY))
			{
#ifdef DEBUG_MANQ_TAGS
				Warning("Generating TagMap For HitTypes");
#endif

				GenerateTagMapping(pTags, ALLOWED_HIT_TYPES_ARRAY, STagMappingHelper::ETagType_HitType, *pTagMappingHelper);
			}

			if (pTags->HaveValue(ALLOWED_PROJECTILES_ARRAY))
			{
#ifdef DEBUG_MANQ_TAGS
				Warning("Generating TagMap For Projectiles");
#endif

				GenerateTagMapping(pTags, ALLOWED_PROJECTILES_ARRAY, STagMappingHelper::ETagType_Projectile, *pTagMappingHelper);
			}

			if (pTags->HaveValue(ALLOWED_WEAPONS_ARRAY))
			{
#ifdef DEBUG_MANQ_TAGS
				Warning("Generating TagMap For Weapons");
#endif

				GenerateTagMapping(pTags, ALLOWED_WEAPONS_ARRAY, STagMappingHelper::ETagType_Weapon, *pTagMappingHelper);
			}

			if (pTags->HaveValue(ALLOWED_STANCES_ARRAY))
			{
#ifdef DEBUG_MANQ_TAGS
				Warning("Generating TagMap For Stances");
#endif

				GenerateTagMapping(pTags, ALLOWED_STANCES_ARRAY, STagMappingHelper::ETagType_Stances, *pTagMappingHelper);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactionsSystem::LoadHitDeathReactionsParams(const CActor& actor, ScriptTablePtr pHitDeathReactionsTable, SHitDeathReactionsConfigConstPtr pNewHitDeathReactionsConfig, ReactionsContainerPtr pHitReactions, ReactionsContainerPtr pDeathReactions, ReactionsContainerPtr pCollisionReactions)
{
	CRY_ASSERT(pHitDeathReactionsTable.GetPtr());
	CRY_ASSERT(pHitReactions.get());
	CRY_ASSERT(pDeathReactions.get());
	CRY_ASSERT(pCollisionReactions.get());

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	const CTagDefinition* pTagDefinition = mannequinSys.GetAnimationDatabaseManager().LoadTagDefs(MANQ_TAG_FILENAME, true);

	if (pTagDefinition)
	{
		STagMappingHelper tagMapping(pTagDefinition);
		LoadTagMapping(actor, pHitDeathReactionsTable, &tagMapping);

		// [*DavidR | 23/Feb/2010] CryShared pointer doesn't have and overload for unary operator *
		LoadReactionsParams(actor, tagMapping, pHitDeathReactionsTable, pNewHitDeathReactionsConfig, DEATH_REACTIONS_PARAMS, true, 0, CHitDeathReactions::eRT_Death, *(pDeathReactions.get()));
		LoadReactionsParams(actor, tagMapping, pHitDeathReactionsTable, pNewHitDeathReactionsConfig, COLLISION_REACTIONS_PARAMS, true, pDeathReactions->size(), CHitDeathReactions::eRT_Collision, *(pCollisionReactions.get()));
		LoadReactionsParams(actor, tagMapping, pHitDeathReactionsTable, pNewHitDeathReactionsConfig, HIT_REACTIONS_PARAMS, false, 0, CHitDeathReactions::eRT_Hit, *(pHitReactions.get()));
	}
	else
	{
		Warning("TagDef not present, abortin load of reactions");
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CHitDeathReactionsSystem::LoadHitDeathReactionsConfig(const CActor& actor, ScriptTablePtr pHitDeathReactionsTable, SHitDeathReactionsConfigPtr pHitDeathReactionsConfig)
{
	CRY_ASSERT(pHitDeathReactionsTable.GetPtr());
	CRY_ASSERT(pHitDeathReactionsConfig.get());

	bool bSuccess = false;

	const char* szCollisionBone = NULL;
	ScriptTablePtr pReactionsConfigTable;
	if (pHitDeathReactionsTable->GetValue(HIT_DEATH_REACTIONS_CONFIG, pReactionsConfigTable))
	{
		// collision volume reference bone
		if (pReactionsConfigTable->HaveValue(COLLISION_BONE_PROPERTY))
			pReactionsConfigTable->GetValue(COLLISION_BONE_PROPERTY, szCollisionBone);

		// Collision volume radius
		if (pReactionsConfigTable->HaveValue(COLLISION_RADIUS_PROPERTY))
			pReactionsConfigTable->GetValue(COLLISION_RADIUS_PROPERTY, pHitDeathReactionsConfig->fCollisionRadius);

		// Collision volume vertical offset
		if (pReactionsConfigTable->HaveValue(COLLISION_VERTICAL_OFFSET))
			pReactionsConfigTable->GetValue(COLLISION_VERTICAL_OFFSET, pHitDeathReactionsConfig->fCollisionVerticalOffset);

		if (pReactionsConfigTable->HaveValue(COLL_MAX_HORZ_ANGLE_PROPERTY))
		{
			float fCollMaxHorzAngle = 20.0f;
			pReactionsConfigTable->GetValue(COLL_MAX_HORZ_ANGLE_PROPERTY, fCollMaxHorzAngle);

			pHitDeathReactionsConfig->fCollMaxHorzAngleSin = sin(DEG2RAD(fabs_tpl(fCollMaxHorzAngle)));
		}

		if (pReactionsConfigTable->HaveValue(COLL_MAX_MOV_ANGLE_PROPERTY))
		{
			float fCollMaxMovAngle = 45.0f;
			pReactionsConfigTable->GetValue(COLL_MAX_MOV_ANGLE_PROPERTY, fCollMaxMovAngle);

			pHitDeathReactionsConfig->fCollMaxMovAngleCos = cos(DEG2RAD(fabs_tpl(fCollMaxMovAngle)));
		}

		if (pReactionsConfigTable->HaveValue(COLL_REACTION_START_DIST))
			pReactionsConfigTable->GetValue(COLL_REACTION_START_DIST, pHitDeathReactionsConfig->fCollReactionStartDist);

		if (pReactionsConfigTable->HaveValue(MAX_REACTION_TIME_PROPERTY))
			pReactionsConfigTable->GetValue(MAX_REACTION_TIME_PROPERTY, pHitDeathReactionsConfig->fMaximumReactionTime);

		if (pReactionsConfigTable->HaveValue(MAX_RAGDOLL_TIME))
			pReactionsConfigTable->GetValue(MAX_RAGDOLL_TIME, pHitDeathReactionsConfig->fEndRagdollTime);

		const char* szAnimDatabase = NULL;
		if (pReactionsConfigTable->GetValue(MANQ_SLAVE_ADB, szAnimDatabase) && szAnimDatabase)
		{
			IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
			pHitDeathReactionsConfig->piOptionalAnimationADB = mannequinSys.GetAnimationDatabaseManager().Load(szAnimDatabase);
		}

		if (const IActionController* piActionController = actor.GetAnimatedCharacter()->GetActionController())
		{
			const uint32 MANQ_HITDEATH_FRAGMENT = CCrc32::Compute(MANQ_TAG_FRAGMENT);
			pHitDeathReactionsConfig->fragmentID = piActionController->GetFragID(MANQ_HITDEATH_FRAGMENT);
		}

		if (pReactionsConfigTable->HaveValue(MANQ_TARGET_TAG))
		{
			const char* szManqTargetTag = NULL;
			pReactionsConfigTable->GetValue(MANQ_TARGET_TAG, szManqTargetTag);

			// Sadly, can't store the fragID as it might be used on a different action controller.
			pHitDeathReactionsConfig->manqTargetCRC = CCrc32::ComputeLowercase(szManqTargetTag);
		}
	}

	ICharacterInstance* pMainChar = actor.GetEntity()->GetCharacter(0);
	CRY_ASSERT(pMainChar);
	IDefaultSkeleton* pIDefaultSkeleton = pMainChar ? &pMainChar->GetIDefaultSkeleton() : NULL;
	if (pIDefaultSkeleton)
	{
		pHitDeathReactionsConfig->iCollisionBoneId = pIDefaultSkeleton->GetJointIDByName(szCollisionBone ? szCollisionBone : "Bip01 Spine1");

		if ((pHitDeathReactionsConfig->iCollisionBoneId == -1) && szCollisionBone)
			Warning("Error finding collision bone (%s) for character %s", szCollisionBone, actor.GetEntity()->GetName());
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::LoadReactionsParams(const CActor& actor, const STagMappingHelper& tagMapping, IScriptTable* pHitDeathReactionsTable, SHitDeathReactionsConfigConstPtr pNewHitDeathReactionsConfig, const char* szReactionParamsName, bool bDeathReactions, ReactionId baseReactionId, int reactionType, ReactionsContainer& reactions)
{
	// Store list of reaction descriptions
	ScriptTablePtr pReactionsTable;
	if (pHitDeathReactionsTable->GetValue(szReactionParamsName, pReactionsTable))
	{
		IScriptTable::Iterator it = pReactionsTable->BeginIteration();

		for (; pReactionsTable->MoveNext(it); )
		{
			CRY_ASSERT(it.value.GetType() == EScriptAnyType::Table);

			const ReactionId thisReactionId = (ReactionId(reactions.size() + 1) + baseReactionId) * (bDeathReactions ? -1 : 1);

			SReactionParams reactionParams;
			GetReactionParamsFromScript(actor, tagMapping, it.value.GetScriptTable(), pNewHitDeathReactionsConfig, reactionParams, thisReactionId);

			it.value.GetScriptTable()->SetValue(REACTION_ID, thisReactionId);

			if (reactionParams.mannequinData.actionType != SReactionParams::SMannequinData::EActionType_Invalid)
			{
				const char* pTypeTagName = NULL;

				switch ((CHitDeathReactions::EReactionType)reactionType)
				{
				case CHitDeathReactions::eRT_Hit:
					pTypeTagName = "hit";
					break;
				case CHitDeathReactions::eRT_Death:
					pTypeTagName = "death";
					break;
				case CHitDeathReactions::eRT_Collision:
					pTypeTagName = "collisionreaction";
					break;
				}

				if (pTypeTagName)
				{
					const TagID tagID = tagMapping.m_pTagDefinition->Find(pTypeTagName);
					tagMapping.m_pTagDefinition->Set(reactionParams.mannequinData.tagState, tagID, true);
				}
			}

			reactions.push_back(reactionParams);
		}
		pReactionsTable->EndIteration(it);

		// Shrink capacity excess
		ReactionsContainer(reactions).swap(reactions);

#ifndef _RELEASE
		// Log loaded anims, if needed
		if (g_pGameCVars->g_hitDeathReactions_logReactionAnimsOnLoading)
		{
			const bool bLogFilePaths = g_pGameCVars->g_hitDeathReactions_logReactionAnimsOnLoading == SCVars::eHDRLRAT_LogFilePaths;

			ICharacterInstance* pMainChar = actor.GetEntity()->GetCharacter(0);
			CRY_ASSERT(pMainChar);
			IAnimationSet* pAnimSet = pMainChar ? pMainChar->GetIAnimationSet() : NULL;
			if (pAnimSet)
			{
				// avoid logging the same anim several times in the same character/reaction-file
				typedef VectorMap<uint32, const char*> animationsLogEntries;
				animationsLogEntries usedReactionAnims;

				ReactionsContainer::const_iterator itReactionsEnd = reactions.end();
				for (ReactionsContainer::const_iterator itReactions = reactions.begin(); itReactions != itReactionsEnd; ++itReactions)
				{
					const SReactionParams::SReactionAnim& reactionAnim = *itReactions->reactionAnim;
					if (!reactionAnim.animCRCs.empty())
					{
						SReactionParams::AnimCRCContainer::const_iterator iterEnd = reactionAnim.animCRCs.end();
						for (SReactionParams::AnimCRCContainer::const_iterator iter = reactionAnim.animCRCs.begin(); iter != iterEnd; ++iter)
						{
							uint32 animCRC = *iter;
							const char* szAnim = bLogFilePaths ? pAnimSet->GetFilePathByID(pAnimSet->GetAnimIDByCRC(animCRC)) : pAnimSet->GetNameByAnimID(pAnimSet->GetAnimIDByCRC(animCRC));
							usedReactionAnims.insert(animationsLogEntries::value_type(animCRC, szAnim));
						}
					}
				}

				if (!usedReactionAnims.empty())
				{
					CryLogAlways("* %s non-animation-graph-triggered animations:", szReactionParamsName);

					// Print
					animationsLogEntries::const_iterator itLogsEnd = usedReactionAnims.end();
					for (animationsLogEntries::const_iterator itLogs = usedReactionAnims.begin(); itLogs != itLogsEnd; ++itLogs)
					{
						const char* szAnimName = itLogs->second;
						CryLogAlways("--- %s", szAnimName);
					}
				}
			}
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::PreProcessStanceParams(SmartScriptTable pReactionTable) const
{
	// Parse the stance array strings to the value hold by the lua global variable with that name.
	// Transform the every stance array item to hold the value of that global variable (or -1 if not found, not likely to happen)
	ScriptTablePtr pAllowedStancesArray;
	pReactionTable->GetValue(ALLOWED_STANCES_ARRAY, pAllowedStancesArray);

	int iCount = pAllowedStancesArray ? pAllowedStancesArray->Count() : 0;
	for (int i = 0; i < iCount; ++i)
	{
		// Is this table hasn't been processed yet this element will be a string, the target is transform it to a stance id
		if (pAllowedStancesArray->GetAtType(i + 1) == svtString)
		{
			int iStance = -1;
			const char* szStanceGlobalValue = NULL;
			pAllowedStancesArray->GetAt(i + 1, szStanceGlobalValue);
			gEnv->pScriptSystem->GetGlobalValue(szStanceGlobalValue, iStance);

			// we might have non-stance data in the allowed stances (e.g. mannequin tags).
			if (iStance != -1)
			{
				pAllowedStancesArray->SetAt(i + 1, iStance);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Suppress passedByValue for smart pointers like ScriptTablePtr
// cppcheck-suppress passedByValue
void CHitDeathReactionsSystem::GetReactionParamsFromScript(const CActor& actor, const STagMappingHelper& tagMapping, const ScriptTablePtr pScriptTable, SHitDeathReactionsConfigConstPtr pNewHitDeathReactionsConfig, SReactionParams& reactionParams, ReactionId reactionId) const
{
	CRY_ASSERT(pScriptTable.GetPtr());

	reactionParams.Reset();

	// Cache scriptTablePtr
	reactionParams.reactionScriptTable = pScriptTable;

	// Test for mannequin reaction first!
	if (pScriptTable->HaveValue(MANQ_REACTION))
	{
		int reactionType = -1;
		pScriptTable->GetValue(MANQ_REACTION, reactionType);

		SReactionParams::SMannequinData& manqData = reactionParams.mannequinData;
		if ((reactionType < 0) || (reactionType > SReactionParams::SMannequinData::EActionType_Last))
		{
			Warning("Invalid or missing %s setting: %d. (allowed values are 0 to %d)", MANQ_REACTION, reactionType, SReactionParams::SMannequinData::EActionType_Last);
		}
		else
		{
			manqData.actionType = SReactionParams::SMannequinData::EActionType(reactionType);
		}
	}

	// Cache validation properties
	{
		if (pScriptTable->HaveValue(VALIDATION_SECTION))
		{
			// Child validation params
			ScriptTablePtr pValidationParamsArray;
			pScriptTable->GetValue(VALIDATION_SECTION, pValidationParamsArray);

			int iCount = pValidationParamsArray->Count();
			if (iCount > 0)
			{
				reactionParams.validationParams.reserve(iCount);
				for (int i = 0; i < iCount; ++i)
				{
					ScriptTablePtr pValidationParams;
					if (pValidationParamsArray->GetAt(i + 1, pValidationParams))
					{
						GetValidationParamsFromScript(pValidationParams, tagMapping, reactionParams, actor, reactionId);
					}
				}
			}
		}
		else
		{
			reactionParams.validationParams.reserve(1);

			// Root validation params
			GetValidationParamsFromScript(pScriptTable, tagMapping, reactionParams, actor, reactionId);
		}
	}

	// Cache default execution properties
	if (pScriptTable->HaveValue(REACTION_FUNC_PROPERTY))
	{
		const char* szExecutionFunc = NULL;
		if (pScriptTable->GetValue(REACTION_FUNC_PROPERTY, szExecutionFunc) && szExecutionFunc && (szExecutionFunc[0] != '\0'))
			reactionParams.sCustomExecutionFunc = szExecutionFunc;
	}

	if (pScriptTable->HaveValue(REACTION_END_FUNC_PROPERTY))
	{
		const char* szExecutionFunc = NULL;
		if (pScriptTable->GetValue(REACTION_END_FUNC_PROPERTY, szExecutionFunc) && szExecutionFunc && (szExecutionFunc[0] != '\0'))
			reactionParams.sCustomExecutionEndFunc = szExecutionFunc;
	}

	if (pScriptTable->HaveValue(AISIGNAL_PROPERTY))
	{
		const char* szSignal = NULL;
		if (pScriptTable->GetValue(AISIGNAL_PROPERTY, szSignal) && szSignal && (szSignal[0] != '\0'))
			reactionParams.sCustomAISignal = szSignal;
	}

	if (pScriptTable->HaveValue(REACTION_ON_COLLISION_PROPERTY) ||
	    pScriptTable->HaveValue(RAGDOLL_ON_COLLISION_PROPERTY))
	{
		unsigned int reactionOnCollision = NO_COLLISION_REACTION;
		bool bRagdollOnCollision = false;

		if (!pScriptTable->GetValue(REACTION_ON_COLLISION_PROPERTY, reactionOnCollision))
			pScriptTable->GetValue(RAGDOLL_ON_COLLISION_PROPERTY, bRagdollOnCollision);

		reactionParams.reactionOnCollision = bRagdollOnCollision ? 1 : reactionOnCollision;
	}

	if (pScriptTable->HaveValue(COLLISION_CHECK_INTERSECTION_WITH_GROUND))
	{
		bool bCollisionCheckIntersectionWithGround = false;

		pScriptTable->GetValue(COLLISION_CHECK_INTERSECTION_WITH_GROUND, bCollisionCheckIntersectionWithGround);

		if (bCollisionCheckIntersectionWithGround)
		{
			reactionParams.flags |= SReactionParams::CollisionCheckIntersectionWithGround;
		}
	}

	if (pScriptTable->HaveValue(PAUSE_AI_PROPERTY))
	{
		pScriptTable->GetValue(PAUSE_AI_PROPERTY, reactionParams.bPauseAI);
	}

	if (pScriptTable->HaveValue(NO_RAGDOLL_ON_END_PROPERTY))
	{
		bool bNoRagdollOnEnd = false;
		pScriptTable->GetValue(NO_RAGDOLL_ON_END_PROPERTY, bNoRagdollOnEnd);
		reactionParams.flags |= static_cast<int>(bNoRagdollOnEnd) * SReactionParams::NoRagdollOnEnd;
	}

	if (pScriptTable->HaveValue(REACTION_FINISHES_AIMING_PROPERTY))
	{
		bool bReactionFinishesAiming = false;
		pScriptTable->GetValue(REACTION_FINISHES_AIMING_PROPERTY, bReactionFinishesAiming);
		reactionParams.flags |= static_cast<int>(!bReactionFinishesAiming) * SReactionParams::ReactionFinishesNotAiming;
	}

	if (pScriptTable->HaveValue(END_VELOCITY_PROPERTY))
	{
		pScriptTable->GetValue(END_VELOCITY_PROPERTY, reactionParams.endVelocity);
	}

	if (reactionParams.mannequinData.actionType != SReactionParams::SMannequinData::EActionType_Invalid)
	{
		GenerateDirectionalMannequinTagsFromReactionParams(actor, tagMapping, reactionParams, reactionParams.mannequinData.tagState);

#ifdef DEBUG_MANQ_TAGS
		const int BUFFER_SIZE = 1024;
		char buffer[BUFFER_SIZE] = { 0 };
		tagMapping.m_pTagDefinition->FlagsToTagList(reactionParams.mannequinData.tagState, buffer, BUFFER_SIZE);

		if (strlen(buffer) == 0)
		{
			Warning("We found no tagState for this reaction!");
		}

		Warning("Actor: <%s>; Tags <%s>", actor.GetEntity()->GetName(), buffer);
#endif
	}

	reactionParams.reactionAnim.reset(new SReactionParams::SReactionAnim);
	GetReactionAnimParamsFromScript(actor, pScriptTable, *reactionParams.reactionAnim);

	if (pScriptTable->HaveValue(SNAP_ORIENTATION_ANGLE))
	{
		int angle = 0;
		pScriptTable->GetValue(SNAP_ORIENTATION_ANGLE, angle);
		reactionParams.orientationSnapAngle = DEG2RAD(static_cast<float>(angle));
		reactionParams.flags |= SReactionParams::OrientateToHitDir;
	}

	// Orientate to movement dir has priority over orientation to movement dir
	if (pScriptTable->HaveValue(SNAP_TO_MOVEMENT_DIR))
	{
#ifndef _RELEASE
		if (reactionParams.flags & SReactionParams::OrientateToHitDir)
		{
			ScriptTablePtr pActorScriptTable = actor.GetEntity()->GetScriptTable();
			CRY_ASSERT(pActorScriptTable.GetPtr());

			ScriptAnyValue propertiesTable;
			pActorScriptTable->GetValueAny(ACTOR_PROPERTIES_TABLE, propertiesTable);

			const char* szReactionsDataFilePath = NULL;
			if ((propertiesTable.GetType() == EScriptAnyType::Table) && propertiesTable.GetScriptTable()->GetValue(REACTIONS_DATA_FILE_PROPERTY, szReactionsDataFilePath))
			{
				Warning("Both %s and %s properties were used in a reaction. Only %s will have any effect! While reading %s", SNAP_ORIENTATION_ANGLE, SNAP_TO_MOVEMENT_DIR, SNAP_TO_MOVEMENT_DIR, szReactionsDataFilePath);
			}
			else
			{
				Warning("Both %s and %s properties were used in a reaction. Only %s will have any effect!", SNAP_ORIENTATION_ANGLE, SNAP_TO_MOVEMENT_DIR, SNAP_TO_MOVEMENT_DIR);
			}
		}
#endif

		int angle = 0;
		pScriptTable->GetValue(SNAP_TO_MOVEMENT_DIR, angle);
		reactionParams.orientationSnapAngle = DEG2RAD(static_cast<float>(angle));
		reactionParams.flags &= ~SReactionParams::OrientateToHitDir;
		reactionParams.flags |= SReactionParams::OrientateToMovementDir;
	}
}

template<typename FUNC>
void CHitDeathReactionsSystem::FindAndSetTag(const ScriptTablePtr pScriptTable, const CHitDeathReactionsSystem::TTagToMapTag* pTagMap, const CTagDefinition* pTagDefinition, TagState& tagState, SReactionParams::IdContainer& container, const FUNC& functor) const
{
	const int iCount = pScriptTable->Count();

	const char* pFoundTag = NULL;

	for (int i = 0; i < iCount; ++i)
	{
		const char* tagName = NULL;
		if (pScriptTable->GetAt(i + 1, tagName))
		{
			const char* pTag = NULL;

			// find the map tag if we can.
			if (pTagMap)
			{
				const string tagString(tagName);
				TTagToMapTag::const_iterator it = pTagMap->find(tagString);
				const TTagToMapTag::const_iterator iEnd = pTagMap->end();

				if (it != iEnd)
				{
					pTag = it->first.c_str();
				}

				for (; (it != iEnd) && (it->first == tagString); ++it)
				{
					const int id = functor(it->second.c_str());

					if (id != -1)
					{
						container.insert(id);
					}
				}
			}

			if (pTag == NULL)
			{
				// probably a raw tag.
				pTag = tagName;

				const int id = functor(tagName);

				if (id != -1)
				{
					container.insert(id);
				}
			}

			const TagID tagID = pTagDefinition->Find(pTag);
			if (tagID == TAG_ID_INVALID)
			{
				Warning("Didn't find Tag <%s> in TagDef.", pTag);

				// not a valid meta tag if not in the tagmap.
				continue;
			}

			if (pFoundTag != NULL && stricmp(pTag, pFoundTag) != 0)
			{
				Warning("Found more than one tag in the container! <%s> <%s>", pFoundTag, pTag);
			}

			pFoundTag = pTag;

			if (i == 0)
			{
				pTagDefinition->Set(tagState, tagID, true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Suppress passedByValue for smart pointers like ScriptTablePtr
// cppcheck-suppress passedByValue
bool CHitDeathReactionsSystem::GetValidationParamsFromScript(const ScriptTablePtr pScriptTable, const STagMappingHelper& tagMapping, SReactionParams& reactionParams, const CActor& actor, ReactionId reactionId) const
{
	//	PreProcessStanceParams(pScriptTable);

	const bool bIsMannequinReaction = (reactionParams.mannequinData.actionType != SReactionParams::SMannequinData::EActionType_Invalid);
	bool bHadValidationParams = false;

	SReactionParams::SValidationParams validationParams;

	validationParams.Reset();

	// Cache scriptTablePtr
	validationParams.validationParamsScriptTable = pScriptTable;

	if (pScriptTable->HaveValue(VALIDATION_FUNC_PROPERTY))
	{
		const char* szValidationFunc = NULL;
		if (pScriptTable->GetValue(VALIDATION_FUNC_PROPERTY, szValidationFunc) && szValidationFunc && (szValidationFunc[0] != '\0'))
		{
			validationParams.sCustomValidationFunc = szValidationFunc;

			bHadValidationParams = true;
		}
	}

	if (pScriptTable->HaveValue(MINIMUM_SPEED_PROPERTY))
	{
		pScriptTable->GetValue(MINIMUM_SPEED_PROPERTY, validationParams.fMinimumSpeedAllowed);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(MAXIMUM_SPEED_PROPERTY))
	{
		pScriptTable->GetValue(MAXIMUM_SPEED_PROPERTY, validationParams.fMaximumSpeedAllowed);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(MINIMUM_DAMAGE_PROPERTY))
	{
		pScriptTable->GetValue(MINIMUM_DAMAGE_PROPERTY, validationParams.fMinimumDamageAllowed);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(MAXIMUM_DAMAGE_PROPERTY))
	{
		pScriptTable->GetValue(MAXIMUM_DAMAGE_PROPERTY, validationParams.fMaximumDamageAllowed);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(ONLY_ON_HEALTH_THRESHOLDS))
	{
		ScriptTablePtr pHealthThresholdsArray;
		pScriptTable->GetValue(ONLY_ON_HEALTH_THRESHOLDS, pHealthThresholdsArray);

		int iCount = pHealthThresholdsArray->Count();
		if (iCount > 0)
		{
			bHadValidationParams = true;

			validationParams.healthThresholds.reserve(iCount);

			// const float fActorMaxHealth = actor.GetMaxHealth();
			// [*DavidR | 8/Nov/2010] Unfortunately, on initialization maxHealth hasn't been set yet, since it's set on
			// the ScriptPRoxy initialization(which always happens last), from OnInit methods. We need to obtain it from the script
			ScriptTablePtr pActorScriptTable = actor.GetEntity()->GetScriptTable();
			CRY_ASSERT(pActorScriptTable.GetPtr());

			float fActorMaxHealth = actor.GetMaxHealth();
			ScriptTablePtr propertiesTable;
			ScriptTablePtr propertiesDamageTable;
			if (pActorScriptTable->GetValue(ACTOR_PROPERTIES_TABLE, propertiesTable) &&
			    propertiesTable->GetValue(ACTOR_PROPERTIES_DAMAGE_TABLE, propertiesDamageTable))
				propertiesDamageTable->GetValue(ACTOR_PROPERTIES_DAMAGE_MAXHEALTH, fActorMaxHealth);

			for (int i = 0; i < iCount; ++i)
			{
				float fThreshold = -1.0f;
				if (pHealthThresholdsArray->GetAt(i + 1, fThreshold))
				{
					Limit(fThreshold, 0.0f, fActorMaxHealth);

					// If the specified value is lower or equal to 1.0f then it's a decimal percentage ([0.0, 1.0])
					// If is greater then is an absolute health value. Specifying an absolute value of 1.0 makes no sense
					// since it's impossible to have less health than 1 without being dead
					if (fThreshold <= 1.0f)
					{
						fThreshold *= fActorMaxHealth;
					}

					if (fThreshold > 0.0f)
						validationParams.healthThresholds.insert(fThreshold);
				}
			}
		}
	}

	if (pScriptTable->HaveValue(MINIMUM_DISTANCE_PROPERTY))
	{
		pScriptTable->GetValue(MINIMUM_DISTANCE_PROPERTY, validationParams.fMinimumDistance);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(MAXIMUM_DISTANCE_PROPERTY))
	{
		pScriptTable->GetValue(MAXIMUM_DISTANCE_PROPERTY, validationParams.fMaximumDistance);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(ALLOWED_PARTS_ARRAY))
	{
		FillAllowedPartIds(actor, pScriptTable, tagMapping, bIsMannequinReaction, reactionParams, validationParams);
		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(MOVEMENT_DIRECTION_PROPERTY))
	{
		const char* szMovementDirection = NULL;
		pScriptTable->GetValue(MOVEMENT_DIRECTION_PROPERTY, szMovementDirection);

		validationParams.movementDir = GetCardinalDirectionFromString(szMovementDirection);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(SHOT_ORIGIN_PROPERTY))
	{
		const char* szShotOrigin = NULL;
		pScriptTable->GetValue(SHOT_ORIGIN_PROPERTY, szShotOrigin);

		validationParams.shotOrigin = GetCardinalDirectionFromString(szShotOrigin);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(PROBABILITY_PERCENT_PROPERTY))
	{
		pScriptTable->GetValue(PROBABILITY_PERCENT_PROPERTY, validationParams.fProbability);
		Limit(validationParams.fProbability, 0.0f, 1.0f);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(ALLOWED_STANCES_ARRAY))
	{
		ScriptTablePtr pAllowedStancesArray;
		pScriptTable->GetValue(ALLOWED_STANCES_ARRAY, pAllowedStancesArray);

		if (bIsMannequinReaction)
		{
			FindAndSetTag(pAllowedStancesArray, tagMapping.m_tagMapping[STagMappingHelper::ETagType_Stances], tagMapping.m_pTagDefinition, reactionParams.mannequinData.tagState, validationParams.allowedStances, FAllowedStances(gEnv->pScriptSystem));
		}
		else
		{
			int iCount = pAllowedStancesArray->Count();
			for (int i = 0; i < iCount; ++i)
			{
				int iStance = -1;
				pAllowedStancesArray->GetAt(i + 1, iStance);
				if (pAllowedStancesArray->GetAtType(i + 1) == svtString)
				{
					const char* szStanceGlobalValue = NULL;
					pAllowedStancesArray->GetAt(i + 1, szStanceGlobalValue);

					gEnv->pScriptSystem->GetGlobalValue(szStanceGlobalValue, iStance);
					if (iStance != -1)
					{
						validationParams.allowedStances.insert(static_cast<EStance>(iStance));
					}
				}
				else
				{
					Warning("Found non-string in hitDeath for stances - not sure why");
				}
			}
		}

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(ALLOWED_HIT_TYPES_ARRAY))
	{
		ScriptTablePtr pAllowedHitTypesArray;
		pScriptTable->GetValue(ALLOWED_HIT_TYPES_ARRAY, pAllowedHitTypesArray);

		if (bIsMannequinReaction)
		{
			FindAndSetTag(pAllowedHitTypesArray, tagMapping.m_tagMapping[STagMappingHelper::ETagType_HitType], tagMapping.m_pTagDefinition, reactionParams.mannequinData.tagState, validationParams.allowedHitTypes, FAllowedHitTypes(g_pGame->GetGameRules()));
		}
		else
		{
			const int iCount = pAllowedHitTypesArray->Count();
			for (int i = 0; i < iCount; ++i)
			{
				const char* szHitType = NULL;
				if (pAllowedHitTypesArray->GetAt(i + 1, szHitType))
				{
					validationParams.allowedHitTypes.insert(g_pGame->GetGameRules()->GetHitTypeId(szHitType));
				}
			}
		}

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(ALLOWED_PROJECTILES_ARRAY))
	{
		ScriptTablePtr pAllowedProjectilesArray;
		pScriptTable->GetValue(ALLOWED_PROJECTILES_ARRAY, pAllowedProjectilesArray);

		if (bIsMannequinReaction)
		{
			FindAndSetTag(pAllowedProjectilesArray, tagMapping.m_tagMapping[STagMappingHelper::ETagType_Projectile], tagMapping.m_pTagDefinition, reactionParams.mannequinData.tagState, validationParams.allowedProjectiles, FAllowedNetClass(g_pGame->GetIGameFramework()));
		}
		else
		{
			int iCount = pAllowedProjectilesArray->Count();
			for (int i = 0; i < iCount; ++i)
			{
				const char* szProjClass = NULL;
				pAllowedProjectilesArray->GetAt(i + 1, szProjClass);

				uint16 uProjClassId = 0;
				if (g_pGame->GetIGameFramework()->GetNetworkSafeClassId(uProjClassId, szProjClass))
				{
					validationParams.allowedProjectiles.insert(uProjClassId);
				}
			}
		}

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(ALLOWED_WEAPONS_ARRAY))
	{
		ScriptTablePtr pAllowedWeapons;
		pScriptTable->GetValue(ALLOWED_WEAPONS_ARRAY, pAllowedWeapons);

		if (bIsMannequinReaction)
		{
			FindAndSetTag(pAllowedWeapons, tagMapping.m_tagMapping[STagMappingHelper::ETagType_Weapon], tagMapping.m_pTagDefinition, reactionParams.mannequinData.tagState, validationParams.allowedWeapons, FAllowedNetClass(g_pGame->GetIGameFramework()));
		}
		else
		{
			int iCount = pAllowedWeapons->Count();
			for (int i = 0; i < iCount; ++i)
			{
				const char* szWeaponClass = NULL;
				pAllowedWeapons->GetAt(i + 1, szWeaponClass);

				uint16 uProjClassId = 0;
				if (g_pGame->GetIGameFramework()->GetNetworkSafeClassId(uProjClassId, szWeaponClass))
				{
					validationParams.allowedWeapons.insert(uProjClassId);
				}
			}
		}

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(ONLY_IF_USING_MOUNTED_ITEM_PROPERTY))
	{
		pScriptTable->GetValue(ONLY_IF_USING_MOUNTED_ITEM_PROPERTY, validationParams.bAllowOnlyWhenUsingMountedItems);
		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(AIR_STATE_PROPERTY))
	{
		const char* szAirState = NULL;
		pScriptTable->GetValue(AIR_STATE_PROPERTY, szAirState);

		validationParams.airState = GetAirStateFromString(szAirState);

		bHadValidationParams = true;
	}

	if (pScriptTable->HaveValue(DESTRUCTIBLE_EVENT_PROPERTY))
	{
		const char* szDestructibleEvent = NULL;
		if (pScriptTable->GetValue(DESTRUCTIBLE_EVENT_PROPERTY, szDestructibleEvent) && szDestructibleEvent && (szDestructibleEvent[0] != '\0'))
		{
			validationParams.destructibleEvent = CCrc32::ComputeLowercase(szDestructibleEvent);
			const TagID tagID = tagMapping.m_pTagDefinition->Find(szDestructibleEvent);
			if (tagID != TAG_ID_INVALID)
			{
				tagMapping.m_pTagDefinition->Set(reactionParams.mannequinData.tagState, tagID, true);
			}
			else
			{
				Warning("Tag not found in tagdef <%s>", szDestructibleEvent);
			}
		}

		bHadValidationParams = true;
	}

	if (bHadValidationParams)
	{
		validationParams.validationParamsScriptTable->SetValue(REACTION_ID, reactionId);
		reactionParams.validationParams.push_back(validationParams);
	}

	return bHadValidationParams;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::GetReactionAnimParamsFromScript(const CActor& actor, ScriptTablePtr pScriptTable, SReactionParams::SReactionAnim& reactionAnim) const
{
	ICharacterInstance* pMainChar = actor.GetEntity()->GetCharacter(0);
	CRY_ASSERT(pMainChar);
	IAnimationSet* pAnimSet = pMainChar ? pMainChar->GetIAnimationSet() : NULL;
	if (pAnimSet)
	{
		if (pScriptTable->HaveValue(REACTION_ANIM_NAME_PROPERTY))
		{
			// Kept for backwards compatibility and to help keeping the reactions as simple as possible
			const char* szAnimName = NULL;
			if (pScriptTable->GetValue(REACTION_ANIM_NAME_PROPERTY, szAnimName))
			{
				int iAnimID = pAnimSet->GetAnimIDByName(szAnimName);
				if (iAnimID >= 0)
				{
					uint32 animCRC = NameCRCHelper::GetCRC(szAnimName);
					reactionAnim.animCRCs.push_back(animCRC);
				}
				else
				{
					AnimIDError(szAnimName);
				}
			}
		}

		if (pScriptTable->HaveValue(REACTION_ANIM_PROPERTY))
		{
			ScriptAnyValue reactionAnimTable;
			if (pScriptTable->GetValueAny(REACTION_ANIM_PROPERTY, reactionAnimTable) && (reactionAnimTable.GetType() == EScriptAnyType::Table))
			{
				ScriptTablePtr pReactionAnimTable(reactionAnimTable.GetScriptTable());

				// Additive anim?
				if (pReactionAnimTable->HaveValue(REACTION_ANIM_ADDITIVE_ANIM))
				{
					pReactionAnimTable->GetValue(REACTION_ANIM_ADDITIVE_ANIM, reactionAnim.bAdditive);
				}

				// Animation layer
				if (pReactionAnimTable->HaveValue(REACTION_ANIM_LAYER))
				{
					pReactionAnimTable->GetValue(REACTION_ANIM_LAYER, reactionAnim.iLayer);
				}

				// Used for overriding the transition time the animation on the current AG state is going to use when resumed
				if (pReactionAnimTable->HaveValue(REACTION_ANIM_OVERRIDE_TRANS_TIME_TO_AG))
				{
					pReactionAnimTable->GetValue(REACTION_ANIM_OVERRIDE_TRANS_TIME_TO_AG, reactionAnim.fOverrideTransTimeToAG);
				}

				// Flag to force no anim-controlled camera on 1st person players
				if (pReactionAnimTable->HaveValue(REACTION_ANIM_NO_ANIM_CAMERA))
				{
					pReactionAnimTable->GetValue(REACTION_ANIM_NO_ANIM_CAMERA, reactionAnim.bNoAnimCamera);
				}

				// List of animations and their variation
				if (pReactionAnimTable->HaveValue(ANIM_NAME_ARRAY))
				{
					ScriptTablePtr pAnimationArray;
					pReactionAnimTable->GetValue(ANIM_NAME_ARRAY, pAnimationArray);

					int iCount = pAnimationArray->Count();
					for (int i = 0; i < iCount; ++i)
					{
						ScriptTablePtr pAnimation;
						if (pAnimationArray->GetAt(i + 1, pAnimation))
						{
							const char* szReactionAnim = NULL;
							if (pAnimation->GetValue(ANIM_NAME_PROPERTY, szReactionAnim) && szReactionAnim && (szReactionAnim[0] != '\0'))
							{
								int variants = 0;
								if (pAnimation->GetValue(ANIM_VARIANTS_PROPERTY, variants))
								{
									//--- Load in all variants
									CryPathString variantName;

									for (int k = 0; k < variants; k++)
									{
										variantName.Format("%s%d", szReactionAnim, k + 1);

										int animID = pAnimSet->GetAnimIDByName(variantName);
										if (animID >= 0)
										{
											uint32 animCRC = NameCRCHelper::GetCRC(variantName);
											reactionAnim.animCRCs.push_back(animCRC);
										}
										else
										{
											AnimIDError(variantName);
										}
									}
								}
								else
								{
									//--- Load in the single animation
									int iAnimID = pAnimSet->GetAnimIDByName(szReactionAnim);
									if (iAnimID >= 0)
									{
										uint32 animCRC = NameCRCHelper::GetCRC(szReactionAnim);
										reactionAnim.animCRCs.push_back(animCRC);
									}
									else
									{
										AnimIDError(szReactionAnim);
									}
								}
							}
						}
					}

					// Shrink capacity excess
					SReactionParams::AnimCRCContainer(reactionAnim.animCRCs).swap(reactionAnim.animCRCs);

					// Shuffle IDs
					SRandomGeneratorFunct randomFunctor(m_pseudoRandom);
					std::random_shuffle(reactionAnim.animCRCs.begin(), reactionAnim.animCRCs.end(), randomFunctor);
				}
			}
		}

		// Request loading of first asset of this set on creation
		if (GetStreamingPolicy() != SCVars::eHDRSP_ActorsAliveAndNotInPool)
			reactionAnim.RequestNextAnim(pAnimSet);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Suppress passedByValue for smart pointers like ScriptTablePtr
// cppcheck-suppress passedByValue
void CHitDeathReactionsSystem::FillAllowedPartIds(const CActor& actor, const ScriptTablePtr pScriptTable, const STagMappingHelper& tagMapping, bool bIsMannequinReaction, SReactionParams& reactionParams, SReactionParams::SValidationParams& validationParams) const
{
	ScriptTablePtr pAllowedPartArray;
	pScriptTable->GetValue(ALLOWED_PARTS_ARRAY, pAllowedPartArray);

	ICharacterInstance* pMainChar = actor.GetEntity()->GetCharacter(0);
	if (pMainChar)
	{
		if (bIsMannequinReaction)
		{
			FindAndSetTag(pAllowedPartArray, tagMapping.m_tagMapping[STagMappingHelper::ETagType_Part], tagMapping.m_pTagDefinition, reactionParams.mannequinData.tagState, validationParams.allowedPartIds, FAllowedParts(pMainChar));
		}
		else
		{
			int iCount = pAllowedPartArray->Count();
			for (int i = 0; i < iCount; ++i)
			{
				const char* szPartName = NULL;
				pAllowedPartArray->GetAt(i + 1, szPartName);

				int iPartId = pMainChar->GetIDefaultSkeleton().GetJointIDByName(szPartName);

				// [*DavidR | 12/Nov/2009] ToDo: Log iPartId == -1 without spamming
				if (iPartId != -1)
				{
					validationParams.allowedPartIds.insert(iPartId);
				}
				else
				{
					// perhaps it's an attachment?
					const int FIRST_ATTACHMENT_PARTID = 1000;

					IAttachmentManager* pAttachmentManager = pMainChar->GetIAttachmentManager();
					int32 iAttachmentIdx = pAttachmentManager->GetIndexByName(szPartName);
					if (iAttachmentIdx != -1)
					{
						validationParams.allowedPartIds.insert(iAttachmentIdx + FIRST_ATTACHMENT_PARTID);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
ECardinalDirection CHitDeathReactionsSystem::GetCardinalDirectionFromString(const char* szCardinalDirection) const
{
	ECardinalDirection cardinalDirection = eCD_Invalid;

	if (szCardinalDirection)
	{
		if (strcmp(szCardinalDirection, "left") == 0)
			cardinalDirection = eCD_Left;
		else if (strcmp(szCardinalDirection, "right") == 0)
			cardinalDirection = eCD_Right;
		else if (strcmp(szCardinalDirection, "forward") == 0)
			cardinalDirection = eCD_Forward;
		else if (strcmp(szCardinalDirection, "back") == 0)
			cardinalDirection = eCD_Back;

		else if (strcmp(szCardinalDirection, "leftSide") == 0)
			cardinalDirection = eCD_LeftSide;
		else if (strcmp(szCardinalDirection, "rightSide") == 0)
			cardinalDirection = eCD_RightSide;
		else if (strcmp(szCardinalDirection, "ahead") == 0)
			cardinalDirection = eCD_Ahead;
		else if (strcmp(szCardinalDirection, "behind") == 0)
			cardinalDirection = eCD_Behind;
	}

	return cardinalDirection;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
EAirState CHitDeathReactionsSystem::GetAirStateFromString(const char* szAirState) const
{
	EAirState airState = eAS_Unset;
	if (szAirState)
	{
		if (!strcmp(szAirState, "onGround"))
			airState = eAS_OnGround;
		else if (!strcmp(szAirState, "onGroundOrObject"))
			airState = eAS_OnGroundOrObject;
		else if (!strcmp(szAirState, "onObject"))
			airState = eAS_OnObject;
		else if (!strcmp(szAirState, "inAir"))
			airState = eAS_InAir;
	}
	return airState;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::OnRequestAnimsTimer(void* pUserData, IGameFramework::TimerID handler)
{
	ProfileId profileId = (ProfileId)(TRUNCATE_PTR)pUserData;
	ProfilesContainer::iterator itFind = m_reactionProfiles.find(profileId);
	CRY_ASSERT(itFind != m_reactionProfiles.end());
	if (itFind != m_reactionProfiles.end())
	{
		SReactionsProfile& profile = itFind->second;

		profile.timerId = 0;

		CRY_ASSERT(profile.iRefCount > 0);
		if (profile.IsValid() && (profile.iRefCount > 0))
		{
			// Seed the random generator with the key obtained for this reaction params instance. It will be used
			// for the request of the reaction anims (we need to randomly select one variation on reactions using
			// more than one animation), it's sure it will be the same across the network
			g_pGame->GetHitDeathReactionsSystem().GetRandomGenerator().seed(gEnv->bNoRandomSeed ? 0 : profileId);

			if (g_pGameCVars->g_hitDeathReactions_usePrecaching > 0)
			{
				// Find a valid entity ID which still has a CharInst.
				EntityId validEntityId = profile.entitiesUsingProfile.begin()->first;
				SReactionsProfile::entitiesUsingProfileContainer::const_iterator it = profile.entitiesUsingProfile.begin();
				const SReactionsProfile::entitiesUsingProfileContainer::const_iterator iEnd = profile.entitiesUsingProfile.end();
				for (; it != iEnd; ++it)
				{
					IEntity* piEntity = gEnv->pEntitySystem->GetEntity(it->first);
					if (piEntity)
					{
						ICharacterInstance* piCharInst = piEntity->GetCharacter(0);
						if (piCharInst)
						{
							validEntityId = piEntity->GetId();
							break;
						}
					}
				}

				// Lock reaction anims
				SPredRequestAnims requestPredicate(true, validEntityId, profile.pHitDeathReactionsConfig.lock()->piOptionalAnimationADB);
				std::for_each(profile.pHitReactions.lock()->begin(), profile.pHitReactions.lock()->end(), requestPredicate);
				std::for_each(profile.pDeathReactions.lock()->begin(), profile.pDeathReactions.lock()->end(), requestPredicate);
				std::for_each(profile.pCollisionReactions.lock()->begin(), profile.pCollisionReactions.lock()->end(), requestPredicate);
			}

#ifndef _RELEASE
			if (g_pGameCVars->g_hitDeathReactions_debug)
			{
				std::map<ProfileId, string>::const_iterator it = m_profileIdToReactionFileMap.find(profileId);
				CRY_ASSERT(it != m_profileIdToReactionFileMap.end());
				if (it != m_profileIdToReactionFileMap.end())
				{
					CryStackStringT<char, 128> debugText;
					debugText.Format("[HitDeathReactionsSystem] REQUEST of animations for profile %s", it->second.c_str());
					CryLogAlways("%s", debugText.c_str());
				}
			}
#endif
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CHitDeathReactionsSystem::OnReleaseAnimsTimer(void* pUserData, IGameFramework::TimerID handler)
{
	ProfileId profileId = (ProfileId)(TRUNCATE_PTR)pUserData;
	ProfilesContainer::iterator itFind = m_reactionProfiles.find(profileId);
	CRY_ASSERT(itFind != m_reactionProfiles.end());
	if (itFind != m_reactionProfiles.end())
	{
		SReactionsProfile& profile = itFind->second;

		profile.timerId = 0;

		CRY_ASSERT(profile.iRefCount == 0);
		if (profile.IsValid() && (profile.iRefCount == 0))
		{
			// UnLock reaction anims
			SPredRequestAnims releasePredicate(false, 0, NULL);
			std::for_each(profile.pHitReactions.lock()->begin(), profile.pHitReactions.lock()->end(), releasePredicate);
			std::for_each(profile.pDeathReactions.lock()->begin(), profile.pDeathReactions.lock()->end(), releasePredicate);
			std::for_each(profile.pCollisionReactions.lock()->begin(), profile.pCollisionReactions.lock()->end(), releasePredicate);

#ifndef _RELEASE
			if (g_pGameCVars->g_hitDeathReactions_debug)
			{
				std::map<ProfileId, string>::const_iterator it = m_profileIdToReactionFileMap.find(profileId);
				CRY_ASSERT(it != m_profileIdToReactionFileMap.end());
				if (it != m_profileIdToReactionFileMap.end())
				{
					CryStackStringT<char, 128> debugText;
					debugText.Format("[HitDeathReactionsSystem] RELEASE of animations for profile %s", it->second.c_str());
					CryLogAlways("%s", debugText.c_str());
				}
			}
#endif
		}
	}
}

void CHitDeathReactionsSystem::GenerateDirectionalMannequinTagsFromReactionParams(const CActor& actor, const STagMappingHelper& tagMapping, const SReactionParams& reactionParams, TagState& tagState) const
{
	if (tagMapping.m_pTagDefinition)
	{
		SReactionParams::ValidationParamsList::const_iterator it = reactionParams.validationParams.begin();
		const SReactionParams::ValidationParamsList::const_iterator iEnd = reactionParams.validationParams.end();
		for (; it != iEnd; ++it)
		{
			const SReactionParams::SValidationParams& validationParams = *it;

			AddDirectionToTagState(validationParams.shotOrigin, "so_", tagMapping, tagState);
			AddDirectionToTagState(validationParams.movementDir, "md_", tagMapping, tagState);
		}
	}
}

void CHitDeathReactionsSystem::AddDirectionToTagState(const ECardinalDirection direction, const char* pPrefix, const STagMappingHelper& tagMapping, TagState& tagState) const
{
	const char* pShotOrigin = NULL;
	switch (direction)
	{
	case eCD_Forward:
		pShotOrigin = "forward";
		break;
	case eCD_Back:
		pShotOrigin = "back";
		break;
	case eCD_Left:
		pShotOrigin = "left";
		break;
	case eCD_Right:
		pShotOrigin = "right";
		break;
	case eCD_Ahead:
		pShotOrigin = "ahead";
		break;
	case eCD_Behind:
		pShotOrigin = "behind";
		break;
	case eCD_LeftSide:
		pShotOrigin = "leftside";
		break;
	case eCD_RightSide:
		pShotOrigin = "rightside";
		break;
	}
	if (pShotOrigin)
	{
		string tmp(pPrefix);
		tmp += pShotOrigin;

		const TagID tagID = tagMapping.m_pTagDefinition->Find(tmp.c_str());

		if (tagID != TAG_ID_INVALID)
		{
			tagMapping.m_pTagDefinition->Set(tagState, tagID, true);
		}
	}
}
