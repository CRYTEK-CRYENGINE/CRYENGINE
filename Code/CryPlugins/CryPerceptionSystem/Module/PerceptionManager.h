// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/IPerceptionManager.h"
#include "PerceptionActor.h"

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/ValueHistory.h>
#include <CryAISystem/IAISystemComponent.h>

struct IAIActor;
struct IAIObject;

class CScriptBind_PerceptionManager;

class CPerceptionManager : public IPerceptionManager, public IAISystemComponent, public ISystemEventListener
{
public:
	CPerceptionManager();
	virtual ~CPerceptionManager() override;

	static CPerceptionManager* GetInstance() { return s_pInstance; }

	// IPerceptionManager
	virtual void RegisterStimulus(const SAIStimulus& stim) override;
	virtual void IgnoreStimulusFrom(EntityId sourceId, EAIStimulusType type, float time) override;
	virtual bool IsPointInRadiusOfStimulus(EAIStimulusType type, const Vec3& pos) const override;

	virtual bool RegisterStimulusDesc(EAIStimulusType type, const SAIStimulusTypeDesc& desc) override;

	virtual void RegisterAIStimulusEventListener(const EventCallback& eventCallback, const SAIStimulusEventListenerParams& params) override;
	virtual void UnregisterAIStimulusEventListener(const EventCallback& eventCallback) override;

	virtual void SetPriorityObjectType(uint16 type) override;
	// !IPerceptionManager

	void SetActorState(IAIObject* pAIObject, bool enabled);
	void ResetActor(IAIObject* pAIObject);

private:
	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// !ISystemEventListener

	// IAISystemComponent
	virtual void Reset(IAISystem::EResetReason reason) override;
	virtual void Update(float deltaTime) override;
	virtual void Serialize(TSerialize ser) override;
	virtual void ActorUpdate(IAIObject* pAIObject, IAIObject::EUpdateType type, float deltaTime) override;
	virtual bool WantActorUpdates(IAIObject::EUpdateType type) override { return type == IAIObject::Full; }
	virtual void DebugDraw(IAIDebugRenderer* pDebugRenderer) override;
	// !IAISystemComponent

	void OnAIObjectCreated(IAIObject* pAIObject);
	void OnAIObjectRemoved(IAIObject* pAIObject);

private:
	//! The type is sometimes converted to a mask and stored in an dword (32bits), no more than 32 subtypes.
	static const uint32 AI_MAX_STIMULI = 32;

	typedef std::map<EntityId, float>                         StimulusIgnoreMap;
	typedef std::unordered_map<tAIObjectID, CPerceptionActor> PerceptionActorsMap;

	struct SStimulusRecord
	{
		EntityId      sourceId;
		EntityId      targetId;
		Vec3          pos;
		Vec3          dir;
		float         radius;
		float         t;
		unsigned char type;
		unsigned char subType;
		unsigned char flags;
		unsigned char dispatched;

		void          Serialize(TSerialize ser)
		{
			ser.BeginGroup("Stim");
			ser.Value("sourceId", sourceId);
			ser.Value("targetId", targetId);
			ser.Value("pos", pos);
			ser.Value("dir", dir);
			ser.Value("radius", radius);
			ser.Value("t", t);
			ser.Value("type", type);
			ser.Value("subType", subType);
			ser.Value("flags", flags);
			ser.Value("dispatched", dispatched);
			ser.EndGroup();
		}
	};

	CPerceptionActor* GetPerceptionActor(IAIObject* pAIObject);
	void              GatherProbableTargetsForActor(IAIActor* pAIActor);

	void              UpdateIncomingStimuli();
	void              UpdateStimuli(float deltaTime);
	void              UpdatePriorityTargets();

	void              HandleSound(const SStimulusRecord& stim);
	void              HandleCollision(const SStimulusRecord& stim);
	void              HandleExplosion(const SStimulusRecord& stim);
	void              HandleBulletHit(const SStimulusRecord& stim);
	void              HandleBulletWhizz(const SStimulusRecord& stim);
	void              HandleGrenade(const SStimulusRecord& stim);
	void              VisCheckBroadPhase(float dt);
	/// Checks if the sound is occluded.
	bool              IsSoundOccluded(IAIActor* pAIActor, const Vec3& vSoundPos);
	/// Suppresses the sound radius based on sound suppressors.
	float             SupressSound(const Vec3& pos, float radius);
	void              NotifyAIEventListeners(const SStimulusRecord& stim, float threat);
	void              InitCommonTypeDescs();
	bool              FilterStimulus(SAIStimulus* stim);

	/// Records a stimulus event to the AI recorder
	void RecordStimulusEvent(const SStimulusRecord& stim, float fRadius, IAIObject& receiver) const;
	void SetLastExplosionPosition(const Vec3& position, IAIObject* pAIObject) const;

	// We currently consider a stimulus visible if it's inbetween 160 degrees
	// left or right of the view direction (Similar to the grenades check)
	// and if the raycast to the stimulus position is successful
	bool IsStimulusVisible(const SStimulusRecord& stim, const IAIObject* pAIObject);

	void DebugDrawPerception(IAIDebugRenderer* pDebugRenderer, int mode);
	void DebugDrawPerformance(IAIDebugRenderer* pDebugRenderer, int mode);

	std::map<EventCallback, SAIStimulusEventListenerParams> m_eventListeners;

	SAIStimulusTypeDesc            m_stimulusTypes[AI_MAX_STIMULI];
	StimulusIgnoreMap              m_ignoreStimuliFrom[AI_MAX_STIMULI];

	std::vector<SStimulusRecord>   m_stimuli[AI_MAX_STIMULI];
	std::vector<SAIStimulus>       m_incomingStimuli;

	float                          m_visBroadPhaseDt;

	std::vector<uint16>            m_priorityObjectTypes;
	std::vector<IAIObject*>        m_priorityTargets;
	std::vector<IAIObject*>        m_targetEntities;

	std::vector<IAIObject*>        m_probableTargets;

	PerceptionActorsMap            m_actorComponents;
	CScriptBind_PerceptionManager* m_pScriptBind;

	static const int               PERF_TRACKER_SAMPLE_COUNT = 200;

	class CPerfTracker
	{
		int                m_count, m_countMax;
		CValueHistory<int> m_hist;
	public:
		inline CPerfTracker() : m_count(0), m_countMax(0), m_hist(PERF_TRACKER_SAMPLE_COUNT, 0) {}
		inline ~CPerfTracker() {}

		inline void Inc(int n = 1)
		{
			m_count += n;
		}

		inline void Update()
		{
			m_countMax = max(m_count, m_countMax);
			m_hist.Sample(m_count);
			m_count = 0;
		}

		inline void Reset()
		{
			m_count = m_countMax = 0;
			m_hist.Reset();
		}

		inline int                       GetCount() const    { return m_count; }
		inline int                       GetCountMax() const { return m_countMax; }
		inline const CValueHistory<int>& GetHist() const     { return m_hist; }
	};

	enum Trackers
	{
		PERFTRACK_VIS_CHECKS,
		PERFTRACK_UPDATES,
		PERFTRACK_INCOMING_STIMS,
		PERFTRACK_STIMS,
		COUNT_PERFTRACK // must be last
	};

	struct CVars
	{
		void Init();

		DeclareConstIntCVar(DebugPerceptionManager, 0);

		DeclareConstIntCVar(DrawCollisionEvents, 0);
		DeclareConstIntCVar(DrawBulletEvents, 0);
		DeclareConstIntCVar(DrawSoundEvents, 0);
		DeclareConstIntCVar(DrawGrenadeEvents, 0);
		DeclareConstIntCVar(DrawExplosions, 0);
	}
	m_cVars;

	struct PerfStats
	{
		inline void Update()
		{
			for (unsigned i = 0; i < COUNT_PERFTRACK; ++i)
				trackers[i].Update();
		}

		CPerfTracker trackers[COUNT_PERFTRACK];
	};

	PerfStats                  m_stats;

	bool m_bRegistered;
	static CPerceptionManager* s_pInstance;
};
