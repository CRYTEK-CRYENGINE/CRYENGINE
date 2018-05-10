// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Manages and updates target track groups for agents to
        determine which target an agent should select

   -------------------------------------------------------------------------
   History:
   - 02:01:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __TARGET_TRACK_MANAGER_H__
#define __TARGET_TRACK_MANAGER_H__

#include <CryAISystem/ITargetTrackManager.h>
#include "TargetTrackCommon.h"

class CTargetTrackGroup;
class CTargetTrack;
struct ITargetTrackModifier;

struct SAIEVENT;
struct SAIPotentialTarget;

class CTargetTrackManager : public ITargetTrackManager
{
public:
	CTargetTrackManager();
	~CTargetTrackManager();

	void Init();
	void Shutdown();
	void Reset(IAISystem::EResetReason reason);
	void DebugDraw();
	bool ReloadConfig();
	void OnObjectRemoved(CAIObject* pObject);
	void Serialize(TSerialize ser);

	bool IsEnabled() const;

	// Threat modifier
	virtual void                SetTargetTrackThreatModifier(ITargetTrackThreatModifier* pModifier);
	virtual void                ClearTargetTrackThreatModifier();
	ITargetTrackThreatModifier* GetTargetTrackThreatModifier() const { return m_pThreatModifier; }

	// Target group accessors
	virtual bool  SetTargetClassThreat(tAIObjectID aiObjectId, float fClassThreat);
	virtual float GetTargetClassThreat(tAIObjectID aiObjectId) const;
	virtual int   GetTargetLimit(tAIObjectID aiObjectId) const;

	// Agent registration to use target tracks
	bool RegisterAgent(tAIObjectID aiObjectId, const char* szConfig, int nTargetLimit = 0);
	bool UnregisterAgent(tAIObjectID aiObjectId);
	bool ResetAgent(tAIObjectID aiObjectId);
	bool SetAgentEnabled(tAIObjectID aiObjectId, bool bEnable);

	// Incoming stimulus handling
	virtual bool HandleStimulusEventInRange(tAIObjectID aiTargetId, const char* szStimulusName, const TargetTrackHelpers::SStimulusEvent& eventInfo, float fRadius);
	virtual bool HandleStimulusEventForAgent(tAIObjectID aiAgentId, tAIObjectID aiTargetId, const char* szStimulusName, const TargetTrackHelpers::SStimulusEvent& eventInfo);
	virtual bool TriggerPulse(tAIObjectID aiObjectId, tAIObjectID targetId, const char* szStimulusName, const char* szPulseName);

	bool         HandleStimulusFromAIEvent(tAIObjectID aiObjectId, const SAIEVENT* pAIEvent, TargetTrackHelpers::EAIEventStimulusType eType);

	// Outgoing desired target handling
	void   Update(tAIObjectID aiObjectId);
	void   ShareFreshestTargetData();
	void   PullDownThreatLevel(const tAIObjectID aiObjectIdForTargetTrackGroup, const EAITargetThreat maxAllowedThreat);
	bool   GetDesiredTarget(tAIObjectID aiObjectId, uint32 uDesiredTargetMethod, CWeakRef<CAIObject>& outTarget, SAIPotentialTarget*& pOutTargetInfo);
	uint32 GetBestTargets(tAIObjectID aiObjectId, uint32 uDesiredTargetMethod, tAIObjectID* bestTargets, uint32 maxCount);
	int    GetDesiredTargetCount(tAIObjectID aiTargetId, tAIObjectID aiIgnoreId = 0) const;
	int    GetPotentialTargetCount(tAIObjectID aiTargetId, tAIObjectID aiIgnoreId = 0) const;
	int    GetPotentialTargetCountFromFaction(tAIObjectID aiTargetId, const char* factionName, tAIObjectID aiIgnoreId = 0) const;

private:
	// Target track pool management
	CTargetTrack* GetUnusedTargetTrackFromPool();
	void          AddTargetTrackToPool(CTargetTrack* pTrack);

	class CTargetTrackPoolProxy : public TargetTrackHelpers::ITargetTrackPoolProxy
	{
	public:
		CTargetTrackPoolProxy(CTargetTrackManager* pManager) : m_pManager(pManager)
		{
			assert(m_pManager);
		}

		virtual CTargetTrack* GetUnusedTargetTrackFromPool()
		{
			return m_pManager->GetUnusedTargetTrackFromPool();
		}

		virtual void AddTargetTrackToPool(CTargetTrack* pTrack)
		{
			m_pManager->AddTargetTrackToPool(pTrack);
		}

	private:
		CTargetTrackManager* m_pManager;
	};

private:
	// Target track config accessing
	bool                        GetTargetTrackConfig(uint32 uNameHash, TargetTrackHelpers::STargetTrackConfig const*& pOutConfig) const;
	bool                        GetTargetTrackStimulusConfig(uint32 uNameHash, uint32 uStimulusHash, TargetTrackHelpers::STargetTrackStimulusConfig const*& pOutConfig) const;
	const ITargetTrackModifier* GetTargetTrackModifier(uint32 uId) const;

	class CTargetTrackConfigProxy : public TargetTrackHelpers::ITargetTrackConfigProxy
	{
	public:
		CTargetTrackConfigProxy(CTargetTrackManager* pManager) : m_pManager(pManager)
		{
			assert(m_pManager);
		}

		bool GetTargetTrackConfig(uint32 uNameHash, TargetTrackHelpers::STargetTrackConfig const*& pOutConfig) const
		{
			return m_pManager->GetTargetTrackConfig(uNameHash, pOutConfig);
		}

		bool GetTargetTrackStimulusConfig(uint32 uNameHash, uint32 uStimulusHash, TargetTrackHelpers::STargetTrackStimulusConfig const*& pOutConfig) const
		{
			return m_pManager->GetTargetTrackStimulusConfig(uNameHash, uStimulusHash, pOutConfig);
		}

		const ITargetTrackModifier* GetTargetTrackModifier(uint32 uId) const
		{
			return m_pManager->GetTargetTrackModifier(uId);
		}

		void ModifyTargetThreat(IAIObject& ownerAI, IAIObject& targetAI, const ITargetTrack& track, float& outThreatRatio, EAITargetThreat& outThreat) const;

	private:
		CTargetTrackManager* m_pManager;
	};

private:
	// Config load helpers
	bool LoadConfigs(XmlNodeRef& pRoot);
	bool LoadConfigStimuli(TargetTrackHelpers::STargetTrackConfig* pConfig, XmlNodeRef& pStimuliElement, bool bHasTemplate);
	bool LoadConfigModifiers(TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig, XmlNodeRef& pModifiersElement);
	bool LoadConfigPulses(TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig, XmlNodeRef& pPulsesElement);
	bool LoadConfigReleaseThreatLevels(TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig, XmlNodeRef& pReleaseThreatLevelsElement);
	bool ApplyStimulusTemplates();
	bool ApplyStimulusTemplate(TargetTrackHelpers::STargetTrackConfig* pConfig, const TargetTrackHelpers::STargetTrackConfig* pParent);

	// Serialize helpers
	void Serialize_Write(TSerialize ser);
	void Serialize_Read(TSerialize ser);
	bool RegisterAgent(tAIObjectID aiObjectId, uint32 uConfigHash, int nTargetLimit = 0);

	// Stimulus helpers
	static tAIObjectID GetAIObjectId(EntityId entityId);
	bool               ShouldStimulusBeHandled(tAIObjectID aiObjectID, const TargetTrackHelpers::SStimulusEvent& stimulusEvent, const float maxRadius = FLT_MAX);
	bool               HandleStimulusEvent(CTargetTrackGroup* pGroup, TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent);
	bool               CheckConfigUsesStimulus(uint32 uConfigHash, uint32 uStimulusNameHash) const;
	bool               CheckStimulusHostile(tAIObjectID aiObjectId, tAIObjectID aiTargetId, uint32 uConfigHash, uint32 uStimulusNameHash) const;
	bool               TranslateVisualStimulusIfCanBeHandled(TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, const SAIEVENT* pAIEvent) const;
	bool               TranslateSoundStimulusIfCanBeHandled(TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, const SAIEVENT* pAIEvent) const;
	bool               TranslateBulletRainStimulusIfCanBeHandled(TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, const SAIEVENT* pAIEvent) const;

	static uint32      GetConfigNameHash(const char* sName);
	static uint32      GetStimulusNameHash(const char* sStimulus);
	static uint32      GetPulseNameHash(const char* sPulse);

	void               PrepareModifiers();
	void               DeleteConfigs();
	void               DeleteAgents();
	void               ResetFreshestTargetData();

	typedef std::map<uint32, TargetTrackHelpers::STargetTrackConfig*> TConfigContainer;
	TConfigContainer m_Configs;

	typedef std::map<tAIObjectID, CTargetTrackGroup*> TAgentContainer;
	TAgentContainer m_Agents;

	typedef std::vector<CTargetTrack*> TTargetTrackPoolContainer;
	TTargetTrackPoolContainer m_TargetTrackPool;

	typedef std::vector<ITargetTrackModifier*> TModifierContainer;
	TModifierContainer m_Modifiers;

	typedef std::map<tAIObjectID, float> TClassThreatContainer;
	TClassThreatContainer       m_ClassThreatValues;

	ITargetTrackThreatModifier* m_pThreatModifier;

	CTargetTrackPoolProxy*      m_pTrackPoolProxy;
	CTargetTrackConfigProxy*    m_pTrackConfigProxy;

	struct FreshData
	{
		FreshData()
			: timeOfFreshestVisualStimulus(0.0f)
			, freshestVisualPosition(ZERO)
			, freshestVisualDirection(ZERO)
		{
		}

		float timeOfFreshestVisualStimulus;
		Vec3  freshestVisualPosition;
		Vec3  freshestVisualDirection;
	};

	typedef tAIObjectID                            TargetAIObjectID;
	typedef VectorMap<TargetAIObjectID, FreshData> DataPerTarget;
	DataPerTarget m_dataPerTarget;

#ifdef TARGET_TRACK_DEBUG
	void DebugDrawConfig(int nMode);
	void DebugDrawTargets(int nMode, char const* szAgentName);
	void DebugDrawAgent(char const* szAgentName);

	// Used to give the agent being debugged one last frame to clean up when debug drawing is turned off for him
	tAIObjectID m_uLastDebugAgent;
#endif //TARGET_TRACK_DEBUG
};

#endif //__TARGET_TRACK_MANAGER_H__
