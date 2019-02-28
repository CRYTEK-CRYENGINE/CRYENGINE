// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _LIGHTNING_GAME_EFFECT_GAME_EFFECT_
#define _LIGHTNING_GAME_EFFECT_GAME_EFFECT_

#pragma once

#include "GameEffect.h"


class CLightningRenderNode;



struct SLightningStats
{
	struct SStat
	{
		SStat() : m_current(0), m_peak(0) {}
		void SetCurrent(int current)
		{
			m_current = current;
			m_peak = max(m_peak, current);
		}
		void Increment(int plus=1) {SetCurrent(m_current + plus);}
		int GetCurrent() const {return m_current;}
		int GetPeak() const {return m_peak;}
	private:
		int m_current;
		int m_peak;
	};

	void Restart()
	{
		m_activeSparks.SetCurrent(0);
		m_memory.SetCurrent(0);
		m_triCount.SetCurrent(0);
		m_branches.SetCurrent(0);
	}

	SStat m_activeSparks;
	SStat m_memory;
	SStat m_triCount;
	SStat m_branches;
};



struct SLightningParams
{
	SLightningParams();

	void Reset(XmlNodeRef node);

	uint32 m_nameCRC;

	float m_strikeTimeMin;
	float m_strikeTimeMax;
	float m_strikeFadeOut;
	int m_strikeNumSegments;
	int m_strikeNumPoints;

	float m_lightningDeviation;
	float m_lightningFuzzyness;
	float m_lightningVelocity;

	float m_branchProbability;
	int m_branchMaxLevel;
	int m_maxNumStrikes;

	float m_beamSize;
	float m_beamTexTiling;
	float m_beamTexShift;
	float m_beamTexFrames;
	float m_beamTexFPS;
};



class CLightningGameEffect : public CGameEffect
{
public:
	typedef int TIndex;

	struct STarget
	{
		STarget();
		explicit STarget(const Vec3& position);
		explicit STarget(EntityId targetEntity);
		STarget(EntityId targetEntity, int slot, const char* attachment);

		Vec3 m_position;
		EntityId m_entityId;
		int m_characterAttachmentSlot;
		uint32 m_characterAttachmentNameCRC;
	};

private:
	static const int maxNumSparks = 24;

	struct SLightningSpark
	{
		CLightningRenderNode* m_renderNode;
		STarget m_emitter;
		STarget m_receiver;
		float m_timer;
	};

public:
	CLightningGameEffect();
	virtual ~CLightningGameEffect();

	virtual void	Initialise(const SGameEffectParams* gameEffectParams = NULL) override;
	virtual const char* GetName() const override;
	virtual void Update(float frameTime) override;
	virtual void ResetRenderParameters() override { }
	void ClearSparks();

	static void LoadStaticData(IItemParamsNode* rootNode);
	static void ReloadStaticData(IItemParamsNode* rootNode);
	static void ReleaseStaticData();
	void LoadData();

	TIndex TriggerSpark(const char* presetName, IMaterial* pMaterial, const STarget& emitter, const STarget& receiver);
	void RemoveSpark(const TIndex spark);
	void SetEmitter(const TIndex spark, const STarget& target);
	void SetReceiver(const TIndex spark, const STarget& target);
	float GetSparkRemainingTime(const TIndex spark) const;
	void SetSparkDeviationMult(const TIndex spark, float deviationMult);

private:
	void UnloadData();
	int FindEmptySlot() const;
	int FindPreset(const char* name) const;
	Vec3 ComputeTargetPosition(const STarget& target);

	std::vector<SLightningParams> m_lightningParams;
	SLightningSpark m_sparks[maxNumSparks];
	SLightningStats m_stats;
};


#endif
