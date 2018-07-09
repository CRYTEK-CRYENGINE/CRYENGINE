// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CCharInstance;

class CTransitionQueue
{
private:

	_smart_ptr<CAnimationSet> m_pAnimationSet;

public://private:
	DynArray<CAnimation> m_animations;
	f32                  m_fLayerPlaybackScale;    //scaling of the playback speed for all animations in a layer
	f32                  m_fLayerTransitionTime;   //time for the whole layer to fade In and Out
	f32                  m_fLayerTransitionWeight; //the current Blend-Weight during the transition of the layer. 1.0f == fully blended in / 0.0f == deactivated
	f32                  m_fLayerBlendWeight;      //this is an "intensity" blend-weight that we set manually. You can use even negative Blend-Weights on this.
	bool                 m_bActive;

public:
	CTransitionQueue();

public:
	void              SetAnimationSet(CAnimationSet* pAnimationSet) { m_pAnimationSet = pAnimationSet; }

	void              Reset();

	void              PushAnimation(const CAnimation& animation);
	bool              RemoveAnimation(uint index, bool bForce = false);
	CAnimation&       GetAnimation(uint index);
	const CAnimation& GetAnimation(uint index) const;
	uint              GetAnimationCount() const { return uint(m_animations.size()); }
	void              Clear();

public://private:
	void UnloadAnimationAssets(int index);
	void RemoveDelayConditions();

	// TEMP: Hack for TrackView
public:
	void ManualSeekAnimation(uint index, float time2, bool bTriggerEvents, CCharInstance& instance);

	// manually defines how much the first and the second animation in the transition queue is mixed
	// 0.0f means only animation [0], 1.0f means only animation [1]
	float m_manualMixingWeight;

	void SetFirstLayer();
	void ApplyManualMixingWeight(uint numAnims);
	void SetManualMixingWeight(float weight) { m_manualMixingWeight = clamp_tpl(weight, 0.0f, 1.0f); };
	void TransitionsBetweenSeveralAnimations(uint numAnims);
	void AdjustTransitionWeights(uint numAnims);
	void UpdateTransitionTime(uint numAnims, float fDeltaTime, float trackViewExclusive, float originalDeltaTime);
	void AdjustAnimationTimeForTimeWarpedAnimations(uint numAnims);
};

class CPoseModifierQueue
{
public:
	struct SEntry
	{
		const char*               name;
		IAnimationPoseModifierPtr poseModifier;

		void                      GetMemoryUsage(ICrySizer* pSizer) const;
	};

public:
	CPoseModifierQueue();
	~CPoseModifierQueue();

public:
	const SEntry* GetEntries() const    { return m_entries[m_currentIndex].empty() ? NULL : &m_entries[m_currentIndex][0]; }
	uint          GetEntryCount() const { return uint(m_entries[m_currentIndex].size()); }

	bool          Push(const IAnimationPoseModifierPtr pPoseModifier, const char* name, const bool bQueued);
	void          Prepare(const SAnimationPoseModifierParams& params);
	void          Synchronize();

	void          SwapBuffersAndClearActive();
	void          ClearAllPoseModifiers();

	uint32        GetSize();
	void          AddToSizer(ICrySizer* pSizer) const;

private:
	DynArray<SEntry> m_entries[2];
	bool             m_currentIndex;
};
