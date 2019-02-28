// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Maintains an interest level of an agent's perception on a
        target, used for determining the agent's target

   -------------------------------------------------------------------------
   History:
   - 02:01:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __TARGET_TRACK_H__
#define __TARGET_TRACK_H__

#include "TargetTrackCommon.h"
#include <CryAISystem/ITargetTrackManager.h>

class CDebugDrawContext;

class CTargetTrack : public ITargetTrack
{
public:
	CTargetTrack();
	~CTargetTrack();

	CWeakRef<CAIObject> GetAIObject() const     { return m_object; }
	CWeakRef<CAIObject> GetAIGroupOwner() const { return m_groupOwner; }

	// Returns the AI objectID of what should be targeted
	CWeakRef<CAIObject> GetAITarget() const;

	// TODO Consider using a struct to house this info to minimize virtual table footprint
	virtual const Vec3&          GetTargetPos() const       { return m_vTargetPos; }
	virtual const Vec3&          GetTargetDir() const       { return m_vTargetDir; }
	virtual float                GetTrackValue() const      { return m_fTrackValue; }
	virtual float                GetFirstVisualTime() const { return m_fFirstVisualTime; }
	virtual EAITargetType        GetTargetType() const      { return m_eTargetType; }
	virtual EAITargetContextType GetTargetContext() const   { return m_eTargetContextType; }
	virtual EAITargetThreat      GetTargetThreat() const    { return m_eTargetThreat; }

	virtual float                GetHighestEnvelopeValue() const;
	virtual float                GetUpdateInterval() const;

	void                         Init(tAIObjectID aiGroupOwnerId, tAIObjectID aiObjectId, uint32 uConfigHash);
	void                         ResetForPool();
	void                         Serialize(TSerialize ser);

	bool                         Update(float fCurrTime, TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy);

	// Invoke a stimulus on the track
	bool InvokeStimulus(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash);

	// Create/update the pulse for the given stimulus
	bool        TriggerPulse(uint32 uStimulusNameHash, uint32 uPulseNameHash);

	inline bool operator<(const CTargetTrack& other) const
	{
		//if (m_eTargetType != other.m_eTargetType)
		if (m_eTargetThreat != other.m_eTargetThreat)
			return m_eTargetThreat > other.m_eTargetThreat;
		return m_fTrackValue > other.m_fTrackValue;
	}

#ifdef TARGET_TRACK_DEBUG
	// Debugging
	void   SetLastDebugDrawTime(float fTime) const     { m_fLastDebugDrawTime = fTime; }
	float  GetLastDebugDrawTime() const                { return m_fLastDebugDrawTime; }
	void   SetDebugGraphIndex(uint32 uDebugGraphIndex) { m_uDebugGraphIndex = uDebugGraphIndex; }
	uint32 GetDebugGraphIndex() const                  { return m_uDebugGraphIndex; }
	void   DebugDraw(CDebugDrawContext& dc, int iIndex, float& fColumnX, float& fColumnY, TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy) const;
#endif //TARGET_TRACK_DEBUG

	// Stimuli invocation - records the last time a stimulus was invoked
	struct SStimulusInvocation
	{
		// Pulse triggers
		struct SPulseTrigger
		{
			uint32       uPulseNameHash;
			float        fTriggerTime;
			mutable bool bObsolete;

			SPulseTrigger()
				: uPulseNameHash(0)
				, fTriggerTime(0.0f)
				, bObsolete(false)
			{
			}

			SPulseTrigger(uint32 hash)
				: uPulseNameHash(hash)
				, fTriggerTime(0.0f)
				, bObsolete(false)
			{
			}

			static bool IsObsolete(const SPulseTrigger& pulseTrigger)
			{
				return pulseTrigger.bObsolete;
			}

			void Serialize(TSerialize ser);
		};
		typedef std::vector<SPulseTrigger> TPulseTriggersContainer;
		TPulseTriggersContainer                  m_pulseTriggers;

		Vec3                                     m_vLastPos;
		Vec3                                     m_vLastDir;
		TargetTrackHelpers::SEnvelopeData        m_envelopeData;
		EAITargetThreat                          m_eTargetThreat;
		EAITargetContextType                     m_eTargetContextType;
		TargetTrackHelpers::EAIEventStimulusType m_eStimulusType;
		bool                                     m_bMustRun;

		SStimulusInvocation()
			: m_vLastPos(ZERO)
			, m_vLastDir(ZERO)
			, m_eTargetThreat(AITHREAT_NONE)
			, m_eTargetContextType(AITARGET_CONTEXT_UNKNOWN)
			, m_eStimulusType(TargetTrackHelpers::eEST_Generic)
			, m_bMustRun(false)
		{
		}

		bool IsRunning(float fUpdateInterval) const;
		void Serialize(TSerialize ser);
	};

	typedef VectorMap<uint32, SStimulusInvocation> TStimuliInvocationContainer;

	TStimuliInvocationContainer& GetInvocations() { return m_StimuliInvocations; }

private:
	// Stimulus data - used for passing information about stimuli without needing to recalculate
	struct SStimData
	{
		float envelopeValue;
	};

#ifdef TARGET_TRACK_DOTARGETTHREAT
	void ModifyTargetThreat(TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy);
#endif //TARGET_TRACK_DOTARGETTHREAT

#ifdef TARGET_TRACK_DOTARGETTYPE
	bool UpdateTargetType(EAITargetType& outTargetType, EAITargetThreat eTargetThreat, const SStimulusInvocation& invoke);
#endif //TARGET_TRACK_DOTARGETTYPE

	// Update helpers for stimulus invocations
	void UpdateStimulusInvoke(SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent) const;
	void UpdateStimulusPulse(SStimulusInvocation& invoke, uint32 uPulseNameHash) const;
	void UpdatePulseValue(SStimulusInvocation::SPulseTrigger& pulseTrigger) const;

	// Helpers to calculate the current value of a stimulus invocation
	float UpdateStimulusValue(float fCurrTime, SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig, TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy, SStimData& stimData);
	float GetStimulusEnvelopeValue(float fCurrTime, const SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig) const;
	float GetStimulusPulseValue(float fCurrTime, const SStimulusInvocation& invoke, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig) const;
	float GetStimulusModifierValue(const SStimulusInvocation& invoke, TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy, const TargetTrackHelpers::STargetTrackStimulusConfig* pStimulusConfig) const;
	float GetStimulusTotalValue(float fCurrTime, float fEnvelopeValue, float fPulseValue, float fModValue) const;

	TStimuliInvocationContainer m_StimuliInvocations;

	Vec3                        m_vTargetPos;
	Vec3                        m_vTargetDir;
	EAITargetType               m_eTargetType;
	EAITargetContextType        m_eTargetContextType;
	EAITargetThreat             m_eTargetThreat;
	CWeakRef<CAIObject>         m_groupOwner;
	CWeakRef<CAIObject>         m_object;
	uint32                      m_uConfigHash;
	int                         m_iLastUpdateFrame;
	float                       m_fTrackValue;
	float                       m_fFirstVisualTime;
	float                       m_fLastVisualTime;
	float                       m_fThreatRatio;

#ifdef TARGET_TRACK_DEBUG
	// Debugging
	mutable float m_fLastDebugDrawTime;
	uint32        m_uDebugGraphIndex;
#endif //TARGET_TRACK_DEBUG
};

#endif //__TARGET_TRACK_H__
