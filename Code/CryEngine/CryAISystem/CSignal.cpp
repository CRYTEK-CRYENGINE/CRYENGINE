// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CSignal.h"
#include <CryCore/CryCrc32.h>

namespace AISignals
{
	//====================================================================
	// AISIGNAL_EXTRA_DATA
	//====================================================================
	AISignalExtraData::SignalExtraDataAlloc AISignalExtraData::m_signalExtraDataAlloc;

	void AISignalExtraData::CleanupPool()
	{
		m_signalExtraDataAlloc.FreeMemoryIfEmpty();
	}

	AISignalExtraData::AISignalExtraData()
	{
		point.zero();
		point2.zero();
		fValue = 0.0f;
		nID = 0;
		iValue = 0;
		iValue2 = 0;
		sObjectName = NULL;
	}

	AISignalExtraData::~AISignalExtraData()
	{
		if (sObjectName)
			delete[] sObjectName;
	}

	void AISignalExtraData::SetObjectName(const char* szObjectName)
	{
		if (sObjectName)
		{
			delete[] sObjectName;
			sObjectName = NULL;
		}
		if (szObjectName && *szObjectName)
		{
			sObjectName = new char[strlen(szObjectName) + 1];
			strcpy(sObjectName, szObjectName);
		}
	}
	
	void AISignalExtraData::Serialize(TSerialize ser)
	{
		ser.Value("point", point);
		ser.Value("point2", point2);
		//	ScriptHandle nID;
		ser.Value("fValue", fValue);
		ser.Value("iValue", iValue);
		ser.Value("iValue2", iValue2);
		string objectNameString(sObjectName);
		ser.Value("sObjectName", objectNameString);

		// (MATT) This change in #149616 will probably break save compatibility for Crysis - however, we may find a way to do without these strings anyway.
		// I believe it's the only part of the change that will actually impact the saves - the rest could be left as-is. {2008/01/14:18:14:18}
		string sStringData1(string1);
		ser.Value("string1", sStringData1);

		string sStringData2(string2);
		ser.Value("string2", sStringData2);

		if (ser.IsReading())
		{
			SetObjectName(objectNameString.c_str());
			string1 = sStringData1.c_str();
			string1 = sStringData2.c_str();
		}
	}

	AISignalExtraData& AISignalExtraData::operator=(const AISignalExtraData& other)
	{
		point = other.point;
		point2 = other.point2;
		nID = other.nID;
		fValue = other.fValue;
		iValue = other.iValue;
		iValue2 = other.iValue2;
		string1 = other.string1;
		string2 = other.string2;
		SetObjectName(other.sObjectName);
		return *this;
	};

	void AISignalExtraData::ToScriptTable(SmartScriptTable& table) const
	{
		CScriptSetGetChain chain(table);
		{
			chain.SetValue("id", nID);
			chain.SetValue("fValue", fValue);
			chain.SetValue("iValue", iValue);
			chain.SetValue("iValue2", iValue2);
			chain.SetValue("string1", string1);
			chain.SetValue("string2", string2);

			if (sObjectName && sObjectName[0])
				chain.SetValue("ObjectName", sObjectName);
			else
				chain.SetToNull("ObjectName");

			Script::SetCachedVector(point, chain, "point");
			Script::SetCachedVector(point2, chain, "point2");
		}
	}

	void AISignalExtraData::FromScriptTable(const SmartScriptTable& table)
	{
		point.zero();
		point2.zero();

		CScriptSetGetChain chain(table);
		{
			chain.GetValue("id", nID);
			chain.GetValue("fValue", fValue);
			chain.GetValue("iValue", iValue);
			chain.GetValue("iValue2", iValue2);
			chain.GetValue("string1", string1);
			chain.GetValue("string2", string2);
			chain.GetValue("point", point);
			chain.GetValue("point2", point2);

			const char* sTableObjectName;
			if (chain.GetValue("ObjectName", sTableObjectName) && sTableObjectName[0])
				SetObjectName(sTableObjectName);
		}
	}

	//====================================================================
	// CSignalDescription
	//====================================================================
		
	const char* CSignalDescription::GetName() const
	{
		return m_name.c_str();
	}

	uint32 CSignalDescription::GetCrc() const
	{
		return m_crc;
	}

	ESignalTag CSignalDescription::GetTag() const
	{
		return m_tag;
	}

	bool CSignalDescription::IsNone() const 
	{
		return m_tag == ESignalTag::NONE && strcmp(m_name, "") == 0;
	}

	CSignalDescription::CSignalDescription(const ESignalTag tag_, const string& name_)
		: m_tag{ tag_ }
		, m_name{ name_ }
		, m_crc{ CCrc32::Compute(m_name) }
	{}

	//====================================================================
	// CAISignal
	//====================================================================

	CSignal::CSignal(const int nSignal, const ISignalDescription& signalDescription, const tAIObjectID senderID, IAISignalExtraData* pEData)
		: m_nSignal(nSignal)
		, m_pSignalDescription(&signalDescription)
		, m_senderID(senderID)
		, m_pExtraData(pEData)
	{
	}

	const int CSignal::GetNSignal() const
	{
		return m_nSignal;
	}

	const ISignalDescription& CSignal::GetSignalDescription() const
	{
		return *m_pSignalDescription;
	}

	const EntityId CSignal::GetSenderID() const
	{
		return m_senderID;
	}

	IAISignalExtraData* CSignal::GetExtraData()
	{
		return m_pExtraData;
	}

	void CSignal::SetExtraData(IAISignalExtraData* val)
	{
		m_pExtraData = val;
	}

	inline bool CSignal::HasSameDescription(const ISignal* pOther) const
	{
		return GetSignalDescription() == pOther->GetSignalDescription();
	}

	void CSignal::Serialize(TSerialize ser)
	{

		int nSignal = GetNSignal();
		string name = GetSignalDescription().GetName();
		EntityId senderId = GetSenderID();

		ser.Value("nSignal", nSignal);
		ser.Value("strText", name);
		ser.Value("senderID", senderId);

		if (ser.IsReading())
		{
			m_nSignal = nSignal;
			m_senderID = senderId;
			if (name != GetSignalDescription().GetName())
			{
				m_pSignalDescription = &gEnv->pAISystem->GetSignalManager()->RegisterGameSignalDescription(name);
			}
		}

		if (ser.IsReading())
		{
			if (GetExtraData())
				delete (AISignalExtraData*)GetExtraData();
			SetExtraData(new AISignalExtraData);
		}

		if (GetExtraData())
			GetExtraData()->Serialize(ser);
		else
		{
			AISignalExtraData dummy;
			dummy.Serialize(ser);
		}
	}

	//====================================================================
	// CSignalManager
	//====================================================================

	CSignalManager::CSignalManager()
	{
		RegisterBuiltInSignalDescriptions();
	}

	CSignalManager::~CSignalManager()
	{
		DeregisterBuiltInSignalDescriptions();
	}

	const SBuiltInSignalsDescriptions& CSignalManager::GetBuiltInSignalDescriptions() const
	{
		return m_builtInSignals;
	}


	size_t CSignalManager::GetSignalDescriptionsCount() const
	{
		return m_signalDescriptionsVec.size();
	}

	const ISignalDescription& CSignalManager::GetSignalDescription(const size_t index) const
	{
		if (index >= m_signalDescriptionsVec.size())
		{
			CRY_ASSERT_MESSAGE(index >= m_signalDescriptionsVec.size(), "Index must be smaller than size of the vector");
			return m_builtInSignals.GetNone();
		}

		return *m_signalDescriptionsVec[index];
	}

	const ISignalDescription& CSignalManager::GetSignalDescription(const char * szSignalDescName) const
	{
		CrcToSignalDescriptionUniquePtrMap::const_iterator it = m_signalDescriptionMap.find(CCrc32::Compute(szSignalDescName));

		if (it == m_signalDescriptionMap.end())
		{
			CRY_ASSERT_MESSAGE(it == m_signalDescriptionMap.end(), "Called GetSignalDescription with a non-registered Signal Name '%s'", szSignalDescName);
			gEnv->pLog->LogWarning("Called GetSignalDescription with a non-registered Signal Name '%s'", szSignalDescName);
			return GetBuiltInSignalDescriptions().GetNone();
		}

		return *it->second;
	}

	const ISignalDescription& CSignalManager::RegisterGameSignalDescription(const char* szSignalName)
	{
		return *RegisterSignalDescription(ESignalTag::GAME, szSignalName);
	}

	void CSignalManager::DeregisterGameSignalDescription(const ISignalDescription& signalDescription)
	{
		m_signalDescriptionMap.erase(signalDescription.GetCrc());
		m_signalDescriptionsVec.erase(std::remove(m_signalDescriptionsVec.begin(), m_signalDescriptionsVec.end(), &signalDescription), m_signalDescriptionsVec.end());
	}

#ifdef SIGNAL_MANAGER_DEBUG
	SignalSharedPtr CSignalManager::CreateSignal(const int nSignal, const ISignalDescription& signalDescription, const EntityId senderID, IAISignalExtraData* pEData)
	{
		std::shared_ptr<CSignal> pSignal = std::shared_ptr<CSignal>(new CSignal(nSignal, signalDescription, senderID, pEData));
		AddSignalToDebugHistory(*pSignal);
		return pSignal;
	}
#else
	SignalSharedPtr CSignalManager::CreateSignal(const int nSignal, const ISignalDescription& signalDescription, const EntityId senderID, IAISignalExtraData* pEData) const
	{
		return std::shared_ptr<CSignal>(new CSignal(nSignal, signalDescription, senderID, pEData));
	}
#endif //SIGNAL_MANAGER_DEBUG

	SignalSharedPtr CSignalManager::CreateSignal_DEPRECATED(const int nSignal, const char* szCustomSignalTypeName, const EntityId senderID, IAISignalExtraData* pEData)
	{
		const ISignalDescription& signalDesc = GetSignalDescription(szCustomSignalTypeName);

		if (signalDesc.IsNone())
		{
			const ISignalDescription& newSignalDesc = RegisterGameSignalDescription(szCustomSignalTypeName);
			return CreateSignal(nSignal, newSignalDesc, senderID, pEData);
		}

		return CreateSignal(nSignal, signalDesc, senderID, pEData);
	}

#ifdef SIGNAL_MANAGER_DEBUG
	void CSignalManager::DebugDrawSignalsHistory() const
	{
		if (!gAIEnv.CVars.DebugDrawSignalsHistory)
		{
			return;
		}
		
		// Setup
		const ColorF headerColor = Col_Red;
		const ColorF newSignalColor = Col_White;
		const ColorF oldSignalColor = Col_Gray;


		const float spacing = 20.0f;

		const float headerTextSize = 1.75f;
		const float signalTextSize = 1.5f;

		IRenderer* pRenderer = gEnv->pRenderer;
		IRenderAuxGeom* pAux = pRenderer->GetIRenderAuxGeom();

		SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
		SAuxGeomRenderFlags renderFlagsRestore = flags;

		flags.SetMode2D3DFlag(e_Mode2D);
		flags.SetDrawInFrontMode(e_DrawInFrontOn);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pAux->SetRenderFlags(flags);
		pAux->SetRenderFlags(renderFlagsRestore);

		const int screenX = 0;
		const int screenY = 0;
		const int screenWidth = pAux->GetCamera().GetViewSurfaceX();
		const int screenHeight = pAux->GetCamera().GetViewSurfaceZ();
		const Vec2 viewportOrigin(static_cast <float> (screenX), static_cast <float> (screenY));
		const Vec2 viewportSize(static_cast <float> (screenWidth), static_cast <float> (screenHeight));

		Vec2 screenPosition = Vec2(10, 10);
		IRenderAuxText::Draw2dLabel(screenPosition.x, screenPosition.y, headerTextSize, headerColor, false, "Signals history");
		screenPosition.y += spacing;
		screenPosition.y += spacing;

		for (const auto& signalDebug : m_debugSignalsHistory)
		{
			const float timeBeforeSignalSwitchesToDifferentColor = 5.0f;
			const CTimeValue now = GetAISystem()->GetFrameStartTime();
			const float elapsedTime = now.GetDifferenceInSeconds(signalDebug.timestamp);
			const float blendFactor = fmin(elapsedTime, timeBeforeSignalSwitchesToDifferentColor) / timeBeforeSignalSwitchesToDifferentColor;

			const ColorF blendedColor = ((1.0f - blendFactor) * newSignalColor) + ( blendFactor * oldSignalColor);
			IRenderAuxText::Draw2dLabel(
				screenPosition.x, 
				screenPosition.y, 
				signalTextSize, 
				blendedColor, 
				false, 
				"N: %d \tSenderId: %u \t%s", signalDebug.signal.GetNSignal(), signalDebug.signal.GetSenderID(), signalDebug.signal.GetSignalDescription().GetName()
			);
			screenPosition.y += spacing;
		}
	}
#endif //SIGNAL_MANAGER_DEBUG

	const ISignalDescription* CSignalManager::RegisterSignalDescription(const ESignalTag signalTag,  const char* signalDescriptionName)
	{
		const CSignalDescription signalDescription(signalTag, signalDescriptionName);
		std::pair<CrcToSignalDescriptionUniquePtrMap::iterator, bool> inserted = m_signalDescriptionMap.insert(
			std::make_pair(
				signalDescription.GetCrc(),
				std::unique_ptr<CSignalDescription>(new CSignalDescription(signalDescription.GetTag(), signalDescription.GetName()))
			)
		);

		if (inserted.second)
		{
			m_signalDescriptionsVec.push_back(inserted.first->second.get());
		}

		return inserted.first->second.get();
	}

	void CSignalManager::RegisterBuiltInSignalDescriptions()
	{
		// ESignalTag::NONE
		m_builtInSignals.SetNone(RegisterSignalDescription(AISignals::ESignalTag::NONE, ""));

		// ESignalTag::GameSDK
		m_builtInSignals.SetOnActJoinFormation(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "ACT_JOIN_FORMATION"));
		m_builtInSignals.SetOnAloneSignal(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "aloneSignal"));
		m_builtInSignals.SetOnAssignedSearchSpotSeen(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnAssignedSearchSpotSeen"));
		m_builtInSignals.SetOnAssignedSearchSpotSeenBySomeoneElse(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnAssignedSearchSpotSeenBySomeoneElse"));
		m_builtInSignals.SetOnEnableFire(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnEnableFire"));
		m_builtInSignals.SetOnEnterSignal(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "enterSignal"));
		m_builtInSignals.SetOnFallAndPlay(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnFallAndPlay"));
		m_builtInSignals.SetOnFinishedHitDeathReaction(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnFinishedHitDeathReaction"));
		m_builtInSignals.SetOnGrabbedByPlayer(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnGrabbedByPlayer"));
		m_builtInSignals.SetOnGroupMemberDied(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "GroupMemberDied"));
		m_builtInSignals.SetOnGroupMemberEnteredCombat(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "GroupMemberEnteredCombat"));
		m_builtInSignals.SetOnGroupMemberGrabbedByPlayer(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "GroupMemberGrabbedByPlayer"));
		m_builtInSignals.SetOnHitDeathReactionInterrupted(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnHitDeathReactionInterrupted"));
		m_builtInSignals.SetOnHitMountedGun(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnHitMountedGun"));
		m_builtInSignals.SetOnIncomingProjectile(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnIncomingProjectile"));
		m_builtInSignals.SetOnInTargetFov(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnInTargetFov"));
		m_builtInSignals.SetOnLeaveSignal(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "leaveSignal"));
		m_builtInSignals.SetOnMeleePerformed(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnMeleePerformed"));
		m_builtInSignals.SetOnMeleeKnockedDownTarget(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnMeleeKnockedDownTarget"));
		m_builtInSignals.SetOnNotAloneSignal(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "notAloneSignal"));
		m_builtInSignals.SetOnNotInTargetFov(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnNotInTargetFov"));
		m_builtInSignals.SetOnNotVisibleFromTarget(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnNotVisibleFromTarget"));
		m_builtInSignals.SetOnReload(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnReload"));
		m_builtInSignals.SetOnReloadDone(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnReloadDone"));
		m_builtInSignals.SetOnSawObjectMove(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnSawObjectMove"));
		m_builtInSignals.SetOnSpottedDeadGroupMember(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "SpottedDeadGroupMember"));
		m_builtInSignals.SetOnTargetSearchSpotSeen(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnTargetSearchSpotSeen"));
		m_builtInSignals.SetOnTargetedByTurret(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnTargetedByTurret"));
		m_builtInSignals.SetOnThrownObjectSeen(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnThrownObjectSeen"));
		m_builtInSignals.SetOnTurretHasBeenDestroyed(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnTurretHasBeenDestroyed"));
		m_builtInSignals.SetOnTurretHasBeenDestroyedByThePlayer(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnTurretHasBeenDestroyedByThePlayer"));
		m_builtInSignals.SetOnVisibleFromTarget(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnVisibleFromTarget"));
		m_builtInSignals.SetOnWaterRippleSeen(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "OnWaterRippleSeen"));
		m_builtInSignals.SetOnWatchMeDie(RegisterSignalDescription(AISignals::ESignalTag::GAME_SDK, "WatchMeDie"));

		//ESignalTag::CRY_ENGINE
		m_builtInSignals.SetOnActionCreated(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnActionCreated"));
		m_builtInSignals.SetOnActionDone(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnActionDone"));
		m_builtInSignals.SetOnActionEnd(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnActionEnd"));
		m_builtInSignals.SetOnActionStart(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnActionStart"));
		m_builtInSignals.SetOnAttentionTargetThreatChanged(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnAttentionTargetThreatChanged"));
		m_builtInSignals.SetOnBulletRain(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnBulletRain"));
		m_builtInSignals.SetOnCloakedTargetSeen(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnCloakedTargetSeen"));
		m_builtInSignals.SetOnCloseCollision(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnCloseCollision"));
		m_builtInSignals.SetOnCloseContact(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnCloseContact"));
		m_builtInSignals.SetOnCoverCompromised(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnCoverCompromised"));
		m_builtInSignals.SetOnEMPGrenadeThrown(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnEMPGrenadeThrown"));
		m_builtInSignals.SetOnEnemyHeard(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnEnemyHeard"));
		m_builtInSignals.SetOnEnemyMemory(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnEnemyMemory"));
		m_builtInSignals.SetOnEnemySeen(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnEnemySeen"));
		m_builtInSignals.SetOnEnterCover(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnEnterCover"));
		m_builtInSignals.SetOnEnterNavSO(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnEnterNavSO"));
		m_builtInSignals.SetOnExposedToExplosion(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnExposedToExplosion"));
		m_builtInSignals.SetOnExposedToFlashBang(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnExposedToFlashBang"));
		m_builtInSignals.SetOnExposedToSmoke(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnExposedToSmoke"));
		m_builtInSignals.SetOnFlashbangGrenadeThrown(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnFlashbangGrenadeThrown"));
		m_builtInSignals.SetOnFragGrenadeThrown(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnFragGrenadeThrown"));
		m_builtInSignals.SetOnGrenadeDanger(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGrenadeDanger"));
		m_builtInSignals.SetOnGrenadeThrown(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGrenadeThrown"));
		m_builtInSignals.SetOnGroupChanged(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGroupChanged"));
		m_builtInSignals.SetOnGroupMemberMutilated(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGroupMemberMutilated"));
		m_builtInSignals.SetOnGroupTarget(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGroupTarget"));
		m_builtInSignals.SetOnGroupTargetMemory(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGroupTargetMemory"));
		m_builtInSignals.SetOnGroupTargetNone(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGroupTargetNone"));
		m_builtInSignals.SetOnGroupTargetSound(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGroupTargetSound"));
		m_builtInSignals.SetOnGroupTargetVisual(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnGroupTargetVisual"));
		m_builtInSignals.SetOnInterestingSoundHeard(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnInterestingSoundHeard"));
		m_builtInSignals.SetOnLeaveCover(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnLeaveCover"));
		m_builtInSignals.SetOnLeaveNavSO(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnLeaveNavSO"));
		m_builtInSignals.SetOnLostSightOfTarget(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnLostSightOfTarget"));
		m_builtInSignals.SetOnLowAmmo(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnLowAmmo"));
		m_builtInSignals.SetOnMeleeExecuted(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnMeleeExecuted"));
		m_builtInSignals.SetOnMemoryMoved(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnMemoryMoved"));
		m_builtInSignals.SetOnMovementPlanProduced(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "MovementPlanProduced"));
		m_builtInSignals.SetOnMovingInCover(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnMovingInCover"));
		m_builtInSignals.SetOnMovingToCover(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnMovingToCover"));
		m_builtInSignals.SetOnNewAttentionTarget(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnNewAttentionTarget"));
		m_builtInSignals.SetOnNoTarget(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnNoTarget"));
		m_builtInSignals.SetOnNoTargetAwareness(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnNoTargetAwareness"));
		m_builtInSignals.SetOnNoTargetVisible(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnNoTargetVisible"));
		m_builtInSignals.SetOnObjectSeen(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnObjectSeen"));
		m_builtInSignals.SetOnOutOfAmmo(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnOutOfAmmo"));
		m_builtInSignals.SetOnPlayerGoingAway(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnPlayerGoingAway"));
		m_builtInSignals.SetOnPlayerLooking(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnPlayerLooking"));
		m_builtInSignals.SetOnPlayerLookingAway(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnPlayerLookingAway"));
		m_builtInSignals.SetOnPlayerSticking(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnPlayerSticking"));
		m_builtInSignals.SetOnReloaded(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnReloaded"));
		m_builtInSignals.SetOnSeenByEnemy(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnSeenByEnemy"));
		m_builtInSignals.SetOnShapeDisabled(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnShapeDisabled"));
		m_builtInSignals.SetOnShapeEnabled(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnShapeEnabled"));
		m_builtInSignals.SetOnSmokeGrenadeThrown(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnSmokeGrenadeThrown"));
		m_builtInSignals.SetOnSomethingSeen(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnSomethingSeen"));
		m_builtInSignals.SetOnSuspectedSeen(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnSuspectedSeen"));
		m_builtInSignals.SetOnSuspectedSoundHeard(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnSuspectedSoundHeard"));
		m_builtInSignals.SetOnTargetApproaching(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetApproaching"));
		m_builtInSignals.SetOnTargetArmoredHit(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetArmoredHit"));
		m_builtInSignals.SetOnTargetCloaked(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetCloaked"));
		m_builtInSignals.SetOnTargetDead(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetDead"));
		m_builtInSignals.SetOnTargetFleeing(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetFleeing"));
		m_builtInSignals.SetOnTargetInTerritory(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetInTerritory"));
		m_builtInSignals.SetOnTargetOutOfTerritory(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetOutOfTerritory"));
		m_builtInSignals.SetOnTargetStampMelee(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetStampMelee"));
		m_builtInSignals.SetOnTargetTooClose(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetTooClose"));
		m_builtInSignals.SetOnTargetTooFar(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetTooFar"));
		m_builtInSignals.SetOnTargetUncloaked(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnTargetUncloaked"));
		m_builtInSignals.SetOnThreateningSeen(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnThreateningSeen"));
		m_builtInSignals.SetOnThreateningSoundHeard(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnThreateningSoundHeard"));
		m_builtInSignals.SetOnUpdateItems(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnUpdateItems"));
		m_builtInSignals.SetOnUseSmartObject(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "OnUseSmartObject"));
		m_builtInSignals.SetOnWeaponEndBurst(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "WPN_END_BURST"));
		m_builtInSignals.SetOnWeaponMelee(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "WPN_MELEE"));
		m_builtInSignals.SetOnWeaponShoot(RegisterSignalDescription(AISignals::ESignalTag::CRY_ENGINE, "WPN_SHOOT"));

		// ESignalTag::DEPRECATED
		m_builtInSignals.SetOnAbortAction_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnAbortAction"));
		m_builtInSignals.SetOnActAlerted_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_ALERTED"));
		m_builtInSignals.SetOnActAnim_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_ANIM"));
		m_builtInSignals.SetOnActAnimex_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_ANIMEX"));
		m_builtInSignals.SetOnActChaseTarget_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_CHASETARGET"));
		m_builtInSignals.SetOnActDialog_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_DIALOG"));
		m_builtInSignals.SetOnActDialogOver_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnActDialogOver"));
		m_builtInSignals.SetOnActDropObject_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_DROP_OBJECT"));
		m_builtInSignals.SetOnActDummy_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_DUMMY"));
		m_builtInSignals.SetOnActEnterVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_ENTERVEHICLE"));
		m_builtInSignals.SetOnActExecute_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_EXECUTE"));
		m_builtInSignals.SetOnActExitVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_EXITVEHICLE"));
		m_builtInSignals.SetOnActFollowPath_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_FOLLOWPATH"));
		m_builtInSignals.SetOnActUseObject_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_USEOBJECT"));
		m_builtInSignals.SetOnActVehicleStickPath_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ACT_VEHICLESTICKPATH"));
		m_builtInSignals.SetOnAddDangerPoint_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "AddDangerPoint"));
		m_builtInSignals.SetOnAttackShootSpot_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnAttackShootSpot"));
		m_builtInSignals.SetOnAttackSwitchPosition_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnAttackSwitchPosition"));
		m_builtInSignals.SetOnAvoidDanger_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnAvoidDanger"));
		m_builtInSignals.SetOnBackOffFailed_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnBackOffFailed"));
		m_builtInSignals.SetOnBehind_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnBehind"));
		m_builtInSignals.SetOnCallReinforcements_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnCallReinforcements"));
		m_builtInSignals.SetOnChangeStance_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnChangeStance"));
		m_builtInSignals.SetOnChargeBailOut_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnChargeBailOut"));
		m_builtInSignals.SetOnChargeHit_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnChargeHit"));
		m_builtInSignals.SetOnChargeMiss_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnChargeMiss"));
		m_builtInSignals.SetOnChargeStart_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnChargeStart"));
		m_builtInSignals.SetOnCheckDeadBody_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnCheckDeadBody"));
		m_builtInSignals.SetOnCheckDeadTarget_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnCheckDeadTarget"));
		m_builtInSignals.SetOnClearSpotList_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnClearSpotList"));
		m_builtInSignals.SetOnCombatTargetDisabled_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "CombatTargetDisabled"));
		m_builtInSignals.SetOnCombatTargetEnabled_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "CombatTargetEnabled"));
		m_builtInSignals.SetOnCoverReached_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnCoverReached"));
		m_builtInSignals.SetOnEndPathOffset_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnEndPathOffset"));
		m_builtInSignals.SetOnEndVehicleDanger_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnEndVehicleDanger"));
		m_builtInSignals.SetOnExecutingSpecialAction_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnExecutingSpecialAction"));
		m_builtInSignals.SetOnFireDisabled_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnFireDisabled"));
		m_builtInSignals.SetOnFiringAllowed_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "FiringAllowed"));
		m_builtInSignals.SetOnFiringNotAllowed_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "FiringNotAllowed"));
		m_builtInSignals.SetOnForcedExecute_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnForcedExecute"));
		m_builtInSignals.SetOnForcedExecuteComplete_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnForcedExecuteComplete"));
		m_builtInSignals.SetOnFormationPointReached_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnFormationPointReached"));
		m_builtInSignals.SetOnKeepEnabled_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnKeepEnabled"));
		m_builtInSignals.SetOnLeaderActionCompleted_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLeaderActionCompleted"));
		m_builtInSignals.SetOnLeaderActionFailed_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLeaderActionFailed"));
		m_builtInSignals.SetOnLeaderAssigned_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLeaderAssigned"));
		m_builtInSignals.SetOnLeaderDeassigned_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLeaderDeassigned"));
		m_builtInSignals.SetOnLeaderDied_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLeaderDied"));
		m_builtInSignals.SetOnLeaderTooFar_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLeaderTooFar"));
		m_builtInSignals.SetOnLeftLean_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLeftLean"));
		m_builtInSignals.SetOnLowHideSpot_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnLowHideSpot"));
		m_builtInSignals.SetOnNavTypeChanged_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNavTypeChanged"));
		m_builtInSignals.SetOnNoAimPosture_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNoAimPosture"));
		m_builtInSignals.SetOnNoFormationPoint_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNoFormationPoint"));
		m_builtInSignals.SetOnNoGroupTarget_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNoGroupTarget"));
		m_builtInSignals.SetOnNoPathFound_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNoPathFound"));
		m_builtInSignals.SetOnNoPeekPosture_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNoPeekPosture"));
		m_builtInSignals.SetOnNotBehind_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNotBehind"));
		m_builtInSignals.SetOnOrdSearch_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORD_SEARCH"));
		m_builtInSignals.SetOnOrderAcquireTarget_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORDER_ACQUIRE_TARGET"));
		m_builtInSignals.SetOnOrderAttack_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORD_ATTACK"));
		m_builtInSignals.SetOnOrderDone_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORD_DONE"));
		m_builtInSignals.SetOnOrderFail_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORD_FAIL"));
		m_builtInSignals.SetOnOrderSearch_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORDER_SEARCH"));
		m_builtInSignals.SetOnPathFindAtStart_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnPathFindAtStart"));
		m_builtInSignals.SetOnPathFound_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnPathFound"));
		m_builtInSignals.SetOnRPTLeaderDead_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "RPT_LeaderDead"));
		m_builtInSignals.SetOnRequestReinforcementTriggered_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnRequestReinforcementTriggered"));
		m_builtInSignals.SetOnRequestUpdate_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnRequestUpdate"));
		m_builtInSignals.SetOnRequestUpdateAlternative_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnRequestUpdateAlternative"));
		m_builtInSignals.SetOnRequestUpdateTowards_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnRequestUpdateTowards"));
		m_builtInSignals.SetOnRightLean_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnRightLean"));
		m_builtInSignals.SetOnSameHidespotAgain_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSameHidespotAgain"));
		m_builtInSignals.SetOnScaleFormation_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnScaleFormation"));
		m_builtInSignals.SetOnSetMinDistanceToTarget_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "SetMinDistanceToTarget"));
		m_builtInSignals.SetOnSetUnitProperties_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSetUnitProperties"));
		m_builtInSignals.SetOnSpecialAction_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSpecialAction"));
		m_builtInSignals.SetOnSpecialActionDone_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSpecialActionDone"));
		m_builtInSignals.SetOnStartFollowPath_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "StartFollowPath"));
		m_builtInSignals.SetOnStartForceFire_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "StartForceFire"));
		m_builtInSignals.SetOnStopFollowPath_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "StopFollowPath"));
		m_builtInSignals.SetOnStopForceFire_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "StopForceFire"));
		m_builtInSignals.SetOnSurpriseAction_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "SURPRISE_ACTION"));
		m_builtInSignals.SetOnTPSDestFound_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnTPSDestFound"));
		m_builtInSignals.SetOnTPSDestNotFound_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnTPSDestNotFound"));
		m_builtInSignals.SetOnTPSDestReached_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnTPSDestReached"));
		m_builtInSignals.SetOnTargetNavTypeChanged_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnTargetNavTypeChanged"));
		m_builtInSignals.SetOnUnitBusy_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnUnitBusy"));
		m_builtInSignals.SetOnUnitDamaged_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnUnitDamaged"));
		m_builtInSignals.SetOnUnitDied_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnUnitDied"));
		m_builtInSignals.SetOnUnitMoving_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnUnitMoving"));
		m_builtInSignals.SetOnUnitResumed_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnUnitResumed"));
		m_builtInSignals.SetOnUnitStop_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnUnitStop"));
		m_builtInSignals.SetOnUnitSuspended_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnUnitSuspended"));
		m_builtInSignals.SetOnVehicleDanger_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnVehicleDanger"));

		// Used in Lua
		m_builtInSignals.SetOnAIAgressive_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "AI_AGGRESSIVE"));
		m_builtInSignals.SetOnAlertStatus_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnAlertStatus_"));
		m_builtInSignals.SetOnAtCloseRange_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "AtCloseRange"));
		m_builtInSignals.SetOnAtOptimalRange_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "AtOptimalRange"));
		m_builtInSignals.SetOnBodyFallSound_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnBodyFallSound"));
		m_builtInSignals.SetOnChangeSetEnd_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ChangeSeatEnd"));
		m_builtInSignals.SetOnCheckNextWeaponAccessory_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "CheckNextWeaponAccessory"));
		m_builtInSignals.SetOnCommandTold_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ON_COMMAND_TOLD"));
		m_builtInSignals.SetOnControllVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "controll_vehicle"));
		m_builtInSignals.SetOnDamage_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnDamage"));
		m_builtInSignals.SetOnDeadMemberSpottedBySomeoneElse_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnDeadMemberSpottedBySomeoneElse"));
		m_builtInSignals.SetOnDisableFire_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnDisableFire"));
		m_builtInSignals.SetOnDoExitVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "do_exit_vehicle"));
		m_builtInSignals.SetOnDriverEntered_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnDriverEntered"));
		m_builtInSignals.SetOnDriverOut_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "DRIVER_OUT"));
		m_builtInSignals.SetOnDroneSeekCommand_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnDroneSeekCommand"));
		m_builtInSignals.SetOnDropped_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnDropped"));
		m_builtInSignals.SetOnEMPGrenadeThrownInGroup_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "EMPGrenadeThrownInGroup"));
		m_builtInSignals.SetOnEnableAlertStatus_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnEnableAlertStatus"));
		m_builtInSignals.SetOnEnemyDamage_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnEnemyDamage"));
		m_builtInSignals.SetOnEnemyDied_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnEnemyDied"));
		m_builtInSignals.SetOnEnteredVehicleGunner_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "entered_vehicle_gunner"));
		m_builtInSignals.SetOnEnteredVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "entered_vehicle"));
		m_builtInSignals.SetOnEnteringEnd_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ENTERING_END"));
		m_builtInSignals.SetOnEnteringVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ENTERING_VEHICLE"));
		m_builtInSignals.SetOnExitVehicleStand_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "EXIT_VEHICLE_STAND"));
		m_builtInSignals.SetOnExitedVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "exited_vehicle"));
		m_builtInSignals.SetOnExitingEnd_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "EXITING_END"));
		m_builtInSignals.SetOnFlashGrenadeThrownInGroup_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "FlashGrenadeThrownInGroup"));
		m_builtInSignals.SetOnFollowLeader_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "FOLLOW_LEADER"));
		m_builtInSignals.SetOnFragGrenadeThrownInGroup_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "FragGrenadeThrownInGroup"));
		m_builtInSignals.SetOnFriendlyDamageByPlayer_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnFriendlyDamageByPlayer"));
		m_builtInSignals.SetOnFriendlyDamage_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnFriendlyDamage"));
		m_builtInSignals.SetOnGoToGrabbed_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "GO_TO_GRABBED"));
		m_builtInSignals.SetOnGoToSeek_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "GO_TO_SEEK"));
		m_builtInSignals.SetOnGroupMemberDiedOnAGL_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnGroupMemberDiedOnAGL"));
		m_builtInSignals.SetOnGroupMemberDiedOnHMG_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnGroupMemberDiedOnHMG"));
		m_builtInSignals.SetOnGroupMemberDiedOnMountedWeapon_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnGroupMemberDiedOnMountedWeapon"));
		m_builtInSignals.SetOnGunnerLostTarget_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "GunnerLostTarget"));
		m_builtInSignals.SetOnInVehicleFoundTarget_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "INVEHICLEGUNNER_FOUND_TARGET"));
		m_builtInSignals.SetOnInVehicleRequestStartFire_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "INVEHICLE_REQUEST_START_FIRE"));
		m_builtInSignals.SetOnInVehicleRequestStopFire_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "INVEHICLE_REQUEST_STOP_FIRE"));
		m_builtInSignals.SetOnIncomingFire_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "INCOMING_FIRE"));
		m_builtInSignals.SetOnJoinTeam_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnJoinTeam"));
		m_builtInSignals.SetOnJustConstructed_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "just_constructed"));
		m_builtInSignals.SetOnLeaveMountedWeapon_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "LeaveMountedWeapon"));
		m_builtInSignals.SetOnLowAmmoFinished_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "LowAmmoFinished"));
		m_builtInSignals.SetOnLowAmmoStart_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "LowAmmoStart"));
		m_builtInSignals.SetOnNearbyWaterRippleSeen_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnNearbyWaterRippleSeen"));
		m_builtInSignals.SetOnNewSpawn_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "NEW_SPAWN"));
		m_builtInSignals.SetOnNoSearchSpotsAvailable_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "NoSearchSpotsAvailable"));
		m_builtInSignals.SetOnOrderExitVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORDER_EXIT_VEHICLE"));
		m_builtInSignals.SetOnOrderFollow_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORDER_FOLLOW"));
		m_builtInSignals.SetOnOrderLeaveVehicle_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORD_LEAVE_VEHICLE"));
		m_builtInSignals.SetOnOrderMove_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORDER_MOVE"));
		m_builtInSignals.SetOnOrderUse_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "ORD_USE"));
		m_builtInSignals.SetOnPlayerDied_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnPlayerDied"));
		m_builtInSignals.SetOnPlayerHit_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnPlayerHit"));
		m_builtInSignals.SetOnPlayerNiceShot_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnPlayerNiceShot"));
		m_builtInSignals.SetOnPlayerTeamKill_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnPlayerTeamKill"));
		m_builtInSignals.SetOnPrepareForMountedWeaponUse_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "PrepareForMountedWeaponUse"));
		m_builtInSignals.SetOnRefPointReached_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "REFPOINT_REACHED"));
		m_builtInSignals.SetOnRelocate_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "Relocate"));
		m_builtInSignals.SetOnRequestJoinTeam_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "REQUEST_JOIN_TEAM"));
		m_builtInSignals.SetOnResetAssignment_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnResetAssignment"));
		m_builtInSignals.SetOnSharedUseThisMountedWeapon_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "SHARED_USE_THIS_MOUNTED_WEAPON"));
		m_builtInSignals.SetOnSmokeGrenadeThrownInGroup_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "SmokeGrenadeThrownInGroup"));
		m_builtInSignals.SetOnSomebodyDied_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSomebodyDied"));
		m_builtInSignals.SetOnSpawn_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSpawn"));
		m_builtInSignals.SetOnSquadmateDied_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSquadmateDied"));
		m_builtInSignals.SetOnStopAndExit_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "STOP_AND_EXIT"));
		m_builtInSignals.SetOnSuspiciousActivity_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnSuspiciousActivity"));
		m_builtInSignals.SetOnTargetLost_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnTargetLost"));
		m_builtInSignals.SetOnTargetNotVisible_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "OnTargetNotVisible"));
		m_builtInSignals.SetOnTooFarFromWeapon_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "TOO_FAR_FROM_WEAPON"));
		m_builtInSignals.SetOnUnloadDone_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "UNLOAD_DONE"));
		m_builtInSignals.SetOnUseMountedWeapon2_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "USE_MOUNTED_WEAPON"));
		m_builtInSignals.SetOnUseMountedWeapon_DEPRECATED(RegisterSignalDescription(AISignals::ESignalTag::DEPRECATED, "UseMountedWeapon"));
	}

	void CSignalManager::DeregisterBuiltInSignalDescriptions()
	{
		// ESignalTag::GAME_SDK
		m_builtInSignals.SetOnActJoinFormation(nullptr);
		m_builtInSignals.SetOnAloneSignal(nullptr);
		m_builtInSignals.SetOnAssignedSearchSpotSeen(nullptr);
		m_builtInSignals.SetOnAssignedSearchSpotSeenBySomeoneElse(nullptr);
		m_builtInSignals.SetOnEnableFire(nullptr);
		m_builtInSignals.SetOnEnterSignal(nullptr);
		m_builtInSignals.SetOnFallAndPlay(nullptr);
		m_builtInSignals.SetOnFinishedHitDeathReaction(nullptr);
		m_builtInSignals.SetOnGrabbedByPlayer(nullptr);
		m_builtInSignals.SetOnGroupMemberDied(nullptr);
		m_builtInSignals.SetOnGroupMemberEnteredCombat(nullptr);
		m_builtInSignals.SetOnGroupMemberGrabbedByPlayer(nullptr);
		m_builtInSignals.SetOnHitDeathReactionInterrupted(nullptr);
		m_builtInSignals.SetOnHitMountedGun(nullptr);
		m_builtInSignals.SetOnIncomingProjectile(nullptr);
		m_builtInSignals.SetOnInTargetFov(nullptr);
		m_builtInSignals.SetOnLeaveSignal(nullptr);
		m_builtInSignals.SetOnMeleePerformed(nullptr);
		m_builtInSignals.SetOnMeleeKnockedDownTarget(nullptr);
		m_builtInSignals.SetOnNotAloneSignal(nullptr);
		m_builtInSignals.SetOnNotInTargetFov(nullptr);
		m_builtInSignals.SetOnNotVisibleFromTarget(nullptr);
		m_builtInSignals.SetOnReload(nullptr);
		m_builtInSignals.SetOnReloadDone(nullptr);
		m_builtInSignals.SetOnSawObjectMove(nullptr);
		m_builtInSignals.SetOnSpottedDeadGroupMember(nullptr);
		m_builtInSignals.SetOnTargetSearchSpotSeen(nullptr);
		m_builtInSignals.SetOnTargetedByTurret(nullptr);
		m_builtInSignals.SetOnThrownObjectSeen(nullptr);
		m_builtInSignals.SetOnTurretHasBeenDestroyed(nullptr);
		m_builtInSignals.SetOnTurretHasBeenDestroyedByThePlayer(nullptr);
		m_builtInSignals.SetOnVisibleFromTarget(nullptr);
		m_builtInSignals.SetOnWaterRippleSeen(nullptr);
		m_builtInSignals.SetOnWatchMeDie(nullptr);

		//ESignalTag::CRY_ENGINE
		m_builtInSignals.SetOnActionCreated(nullptr);
		m_builtInSignals.SetOnActionDone(nullptr);
		m_builtInSignals.SetOnActionEnd(nullptr);
		m_builtInSignals.SetOnActionStart(nullptr);
		m_builtInSignals.SetOnAttentionTargetThreatChanged(nullptr);
		m_builtInSignals.SetOnBulletRain(nullptr);
		m_builtInSignals.SetOnCloakedTargetSeen(nullptr);
		m_builtInSignals.SetOnCloseCollision(nullptr);
		m_builtInSignals.SetOnCloseContact(nullptr);
		m_builtInSignals.SetOnCoverCompromised(nullptr);
		m_builtInSignals.SetOnEMPGrenadeThrown(nullptr);
		m_builtInSignals.SetOnEnemyHeard(nullptr);
		m_builtInSignals.SetOnEnemyMemory(nullptr);
		m_builtInSignals.SetOnEnemySeen(nullptr);
		m_builtInSignals.SetOnEnterCover(nullptr);
		m_builtInSignals.SetOnEnterNavSO(nullptr);
		m_builtInSignals.SetOnExposedToExplosion(nullptr);
		m_builtInSignals.SetOnExposedToFlashBang(nullptr);
		m_builtInSignals.SetOnExposedToSmoke(nullptr);
		m_builtInSignals.SetOnFlashbangGrenadeThrown(nullptr);
		m_builtInSignals.SetOnFragGrenadeThrown(nullptr);
		m_builtInSignals.SetOnGrenadeDanger(nullptr);
		m_builtInSignals.SetOnGrenadeThrown(nullptr);
		m_builtInSignals.SetOnGroupChanged(nullptr);
		m_builtInSignals.SetOnGroupMemberMutilated(nullptr);
		m_builtInSignals.SetOnGroupTarget(nullptr);
		m_builtInSignals.SetOnGroupTargetMemory(nullptr);
		m_builtInSignals.SetOnGroupTargetNone(nullptr);
		m_builtInSignals.SetOnGroupTargetSound(nullptr);
		m_builtInSignals.SetOnGroupTargetVisual(nullptr);
		m_builtInSignals.SetOnInterestingSoundHeard(nullptr);
		m_builtInSignals.SetOnLeaveCover(nullptr);
		m_builtInSignals.SetOnLeaveNavSO(nullptr);
		m_builtInSignals.SetOnLostSightOfTarget(nullptr);
		m_builtInSignals.SetOnLowAmmo(nullptr);
		m_builtInSignals.SetOnMeleeExecuted(nullptr);
		m_builtInSignals.SetOnMemoryMoved(nullptr);
		m_builtInSignals.SetOnMovementPlanProduced(nullptr);
		m_builtInSignals.SetOnMovingInCover(nullptr);
		m_builtInSignals.SetOnMovingToCover(nullptr);
		m_builtInSignals.SetOnNewAttentionTarget(nullptr);
		m_builtInSignals.SetOnNoTarget(nullptr);
		m_builtInSignals.SetOnNoTargetAwareness(nullptr);
		m_builtInSignals.SetOnNoTargetVisible(nullptr);
		m_builtInSignals.SetOnObjectSeen(nullptr);
		m_builtInSignals.SetOnOutOfAmmo(nullptr);
		m_builtInSignals.SetOnPlayerGoingAway(nullptr);
		m_builtInSignals.SetOnPlayerLooking(nullptr);
		m_builtInSignals.SetOnPlayerLookingAway(nullptr);
		m_builtInSignals.SetOnPlayerSticking(nullptr);
		m_builtInSignals.SetOnReloaded(nullptr);
		m_builtInSignals.SetOnSeenByEnemy(nullptr);
		m_builtInSignals.SetOnShapeDisabled(nullptr);
		m_builtInSignals.SetOnShapeEnabled(nullptr);
		m_builtInSignals.SetOnSmokeGrenadeThrown(nullptr);
		m_builtInSignals.SetOnSomethingSeen(nullptr);
		m_builtInSignals.SetOnSuspectedSeen(nullptr);
		m_builtInSignals.SetOnSuspectedSoundHeard(nullptr);
		m_builtInSignals.SetOnTargetApproaching(nullptr);
		m_builtInSignals.SetOnTargetArmoredHit(nullptr);
		m_builtInSignals.SetOnTargetCloaked(nullptr);
		m_builtInSignals.SetOnTargetDead(nullptr);
		m_builtInSignals.SetOnTargetFleeing(nullptr);
		m_builtInSignals.SetOnTargetInTerritory(nullptr);
		m_builtInSignals.SetOnTargetOutOfTerritory(nullptr);
		m_builtInSignals.SetOnTargetStampMelee(nullptr);
		m_builtInSignals.SetOnTargetTooClose(nullptr);
		m_builtInSignals.SetOnTargetTooFar(nullptr);
		m_builtInSignals.SetOnTargetUncloaked(nullptr);
		m_builtInSignals.SetOnThreateningSeen(nullptr);
		m_builtInSignals.SetOnThreateningSoundHeard(nullptr);
		m_builtInSignals.SetOnUpdateItems(nullptr);
		m_builtInSignals.SetOnUseSmartObject(nullptr);
		m_builtInSignals.SetOnWeaponEndBurst(nullptr);
		m_builtInSignals.SetOnWeaponMelee(nullptr);
		m_builtInSignals.SetOnWeaponShoot(nullptr);

		// ESignalTag::DEPRECATED
		m_builtInSignals.SetOnAbortAction_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActAlerted_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActAnim_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActAnimex_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActChaseTarget_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActDialog_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActDialogOver_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActDropObject_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActDummy_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActEnterVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActExecute_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActExitVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActFollowPath_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActUseObject_DEPRECATED(nullptr);
		m_builtInSignals.SetOnActVehicleStickPath_DEPRECATED(nullptr);
		m_builtInSignals.SetOnAddDangerPoint_DEPRECATED(nullptr);
		m_builtInSignals.SetOnAttackShootSpot_DEPRECATED(nullptr);
		m_builtInSignals.SetOnAttackSwitchPosition_DEPRECATED(nullptr);
		m_builtInSignals.SetOnAvoidDanger_DEPRECATED(nullptr);
		m_builtInSignals.SetOnBackOffFailed_DEPRECATED(nullptr);
		m_builtInSignals.SetOnBehind_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCallReinforcements_DEPRECATED(nullptr);
		m_builtInSignals.SetOnChangeStance_DEPRECATED(nullptr);
		m_builtInSignals.SetOnChargeBailOut_DEPRECATED(nullptr);
		m_builtInSignals.SetOnChargeHit_DEPRECATED(nullptr);
		m_builtInSignals.SetOnChargeMiss_DEPRECATED(nullptr);
		m_builtInSignals.SetOnChargeStart_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCheckDeadBody_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCheckDeadTarget_DEPRECATED(nullptr);
		m_builtInSignals.SetOnClearSpotList_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCombatTargetDisabled_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCombatTargetEnabled_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCoverReached_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEndPathOffset_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEndVehicleDanger_DEPRECATED(nullptr);
		m_builtInSignals.SetOnExecutingSpecialAction_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFireDisabled_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFiringAllowed_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFiringNotAllowed_DEPRECATED(nullptr);
		m_builtInSignals.SetOnForcedExecute_DEPRECATED(nullptr);
		m_builtInSignals.SetOnForcedExecuteComplete_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFormationPointReached_DEPRECATED(nullptr);
		m_builtInSignals.SetOnKeepEnabled_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeaderActionCompleted_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeaderActionFailed_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeaderAssigned_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeaderDeassigned_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeaderDied_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeaderTooFar_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeftLean_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLowHideSpot_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNavTypeChanged_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNoAimPosture_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNoFormationPoint_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNoGroupTarget_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNoPathFound_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNoPeekPosture_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNotBehind_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrdSearch_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderAcquireTarget_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderAttack_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderDone_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderFail_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderSearch_DEPRECATED(nullptr);
		m_builtInSignals.SetOnPathFindAtStart_DEPRECATED(nullptr);
		m_builtInSignals.SetOnPathFound_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRPTLeaderDead_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRequestReinforcementTriggered_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRequestUpdate_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRequestUpdateAlternative_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRequestUpdateTowards_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRightLean_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSameHidespotAgain_DEPRECATED(nullptr);
		m_builtInSignals.SetOnScaleFormation_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSetMinDistanceToTarget_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSetUnitProperties_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSpecialAction_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSpecialActionDone_DEPRECATED(nullptr);
		m_builtInSignals.SetOnStartFollowPath_DEPRECATED(nullptr);
		m_builtInSignals.SetOnStartForceFire_DEPRECATED(nullptr);
		m_builtInSignals.SetOnStopFollowPath_DEPRECATED(nullptr);
		m_builtInSignals.SetOnStopForceFire_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSurpriseAction_DEPRECATED(nullptr);
		m_builtInSignals.SetOnTPSDestFound_DEPRECATED(nullptr);
		m_builtInSignals.SetOnTPSDestNotFound_DEPRECATED(nullptr);
		m_builtInSignals.SetOnTPSDestReached_DEPRECATED(nullptr);
		m_builtInSignals.SetOnTargetNavTypeChanged_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnitBusy_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnitDamaged_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnitDied_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnitMoving_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnitResumed_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnitStop_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnitSuspended_DEPRECATED(nullptr);
		m_builtInSignals.SetOnVehicleDanger_DEPRECATED(nullptr);

		// Lua
		m_builtInSignals.SetOnAIAgressive_DEPRECATED(nullptr);
		m_builtInSignals.SetOnAlertStatus_DEPRECATED(nullptr);
		m_builtInSignals.SetOnAtCloseRange_DEPRECATED(nullptr);
		m_builtInSignals.SetOnAtOptimalRange_DEPRECATED(nullptr);
		m_builtInSignals.SetOnBodyFallSound_DEPRECATED(nullptr);
		m_builtInSignals.SetOnChangeSetEnd_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCheckNextWeaponAccessory_DEPRECATED(nullptr);
		m_builtInSignals.SetOnCommandTold_DEPRECATED(nullptr);
		m_builtInSignals.SetOnControllVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDamage_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDeadMemberSpottedBySomeoneElse_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDisableFire_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDoExitVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDriverEntered_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDriverOut_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDroneSeekCommand_DEPRECATED(nullptr);
		m_builtInSignals.SetOnDropped_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEMPGrenadeThrownInGroup_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEnableAlertStatus_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEnemyDamage_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEnemyDied_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEnteredVehicleGunner_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEnteredVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEnteringEnd_DEPRECATED(nullptr);
		m_builtInSignals.SetOnEnteringVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnExitVehicleStand_DEPRECATED(nullptr);
		m_builtInSignals.SetOnExitedVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnExitingEnd_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFlashGrenadeThrownInGroup_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFollowLeader_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFragGrenadeThrownInGroup_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFriendlyDamageByPlayer_DEPRECATED(nullptr);
		m_builtInSignals.SetOnFriendlyDamage_DEPRECATED(nullptr);
		m_builtInSignals.SetOnGoToGrabbed_DEPRECATED(nullptr);
		m_builtInSignals.SetOnGoToSeek_DEPRECATED(nullptr);
		m_builtInSignals.SetOnGroupMemberDiedOnAGL_DEPRECATED(nullptr);
		m_builtInSignals.SetOnGroupMemberDiedOnHMG_DEPRECATED(nullptr);
		m_builtInSignals.SetOnGroupMemberDiedOnMountedWeapon_DEPRECATED(nullptr);
		m_builtInSignals.SetOnGunnerLostTarget_DEPRECATED(nullptr);
		m_builtInSignals.SetOnInVehicleFoundTarget_DEPRECATED(nullptr);
		m_builtInSignals.SetOnInVehicleRequestStartFire_DEPRECATED(nullptr);
		m_builtInSignals.SetOnInVehicleRequestStopFire_DEPRECATED(nullptr);
		m_builtInSignals.SetOnIncomingFire_DEPRECATED(nullptr);
		m_builtInSignals.SetOnJoinTeam_DEPRECATED(nullptr);
		m_builtInSignals.SetOnJustConstructed_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLeaveMountedWeapon_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLowAmmoFinished_DEPRECATED(nullptr);
		m_builtInSignals.SetOnLowAmmoStart_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNearbyWaterRippleSeen_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNewSpawn_DEPRECATED(nullptr);
		m_builtInSignals.SetOnNoSearchSpotsAvailable_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderExitVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderFollow_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderLeaveVehicle_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderMove_DEPRECATED(nullptr);
		m_builtInSignals.SetOnOrderUse_DEPRECATED(nullptr);
		m_builtInSignals.SetOnPlayerDied_DEPRECATED(nullptr);
		m_builtInSignals.SetOnPlayerHit_DEPRECATED(nullptr);
		m_builtInSignals.SetOnPlayerNiceShot_DEPRECATED(nullptr);
		m_builtInSignals.SetOnPlayerTeamKill_DEPRECATED(nullptr);
		m_builtInSignals.SetOnPrepareForMountedWeaponUse_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRefPointReached_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRelocate_DEPRECATED(nullptr);
		m_builtInSignals.SetOnRequestJoinTeam_DEPRECATED(nullptr);
		m_builtInSignals.SetOnResetAssignment_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSharedUseThisMountedWeapon_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSmokeGrenadeThrownInGroup_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSomebodyDied_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSpawn_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSquadmateDied_DEPRECATED(nullptr);
		m_builtInSignals.SetOnStopAndExit_DEPRECATED(nullptr);
		m_builtInSignals.SetOnSuspiciousActivity_DEPRECATED(nullptr);
		m_builtInSignals.SetOnTargetLost_DEPRECATED(nullptr);
		m_builtInSignals.SetOnTargetNotVisible_DEPRECATED(nullptr);
		m_builtInSignals.SetOnTooFarFromWeapon_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUnloadDone_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUseMountedWeapon2_DEPRECATED(nullptr);
		m_builtInSignals.SetOnUseMountedWeapon_DEPRECATED(nullptr);

		m_signalDescriptionMap.clear();
	}

#ifdef SIGNAL_MANAGER_DEBUG
	void CSignalManager::AddSignalToDebugHistory(const CSignal& signal)
	{
		const size_t debugDrawSignalsListCountMax = 20;
		if (m_debugSignalsHistory.size() > debugDrawSignalsListCountMax)
		{
			m_debugSignalsHistory.pop_back();
		}
		m_debugSignalsHistory.emplace_front(signal, GetAISystem()->GetFrameStartTimeSeconds());
	}
#endif //SIGNAL_MANAGER_DEBUG

	SERIALIZATION_ENUM_BEGIN(ESignalFilter, "Signal Filter")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_SENDER, "sender", "Sender")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_LASTOP, "last_op", "LastOp")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_GROUPONLY, "group_only", "Group")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_FACTIONONLY, "faction_only", "FactionOnly")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_ANYONEINCOMM, "anyone_in_comm", "AnyoneInComm")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_SUPERGROUP, "super_group", "SuperGroup")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_SUPERFACTION, "super_faction", "SuperFaction")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_SUPERTARGET, "super_target", "SuperTarget")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_NEARESTGROUP, "nearest_group", "NearestGroup")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_NEARESTSPECIES, "nearest_species", "NearestSpecies")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_NEARESTINCOMM, "nearest_in_comm", "NearestInComm")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_HALFOFGROUP, "half_of_group", "HalfOfGroup")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_GROUPONLY_EXCEPT, "group_only_except", "GroupExcludingSender")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_ANYONEINCOMM_EXCEPT, "anyone_in_comm_except", "AnyoneInCommExcept")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_LEADERENTITY, "leader_entity", "Leader Entity")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_NEARESTINCOMM_FACTION, "nearest_in_comm_faction", "NearestInCommFaction")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_NEARESTINCOMM_LOOKING, "nearest_in_comm_looking", "NearestInCommLooking")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_FORMATION, "formation", "Formation")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_FORMATION_EXCEPT, "formation_except", "FormationExcept")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_READABILITY, "redability", "Redability")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_READABILITYAT, "redability_at", "Redability At")
		SERIALIZATION_ENUM(ESignalFilter::SIGNALFILTER_READABILITYRESPONSE, "redability_response", "RedabilityResponse")
	SERIALIZATION_ENUM_END()
}
