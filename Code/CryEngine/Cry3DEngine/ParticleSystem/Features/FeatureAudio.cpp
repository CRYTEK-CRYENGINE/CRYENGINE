// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include <CrySerialization/Decorators/ResourcesAudio.h>
#include <CryAudio/IObject.h>
#include "../ParticleComponentRuntime.h"
#include "../ParticleEffect.h"
#include "ParamMod.h"

namespace CryAudio
{
	SERIALIZATION_ENUM_IMPLEMENT(EOcclusionType,
		Ignore = 1,
		Adaptive,
		Low,
		Medium,
		High)
}

namespace pfx2
{
static const ColorB audioColor = ColorB(172, 196, 138);

MakeDataType(EPDT_AudioObject, CryAudio::IObject*);

class CFeatureAudioTrigger final : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		m_playTrigger.Resolve();
		m_stopTrigger.Resolve();
		if (GetNumResources())
		{
			pComponent->MainPreUpdate.add(this);
			pComponent->AddParticleData(EPVF_Position);
			if (m_followParticle || m_stopOnDeath)
			{
				pComponent->AddParticleData(EPDT_AudioObject);
				pComponent->InitParticles.add(this);
				pComponent->DestroyParticles.add(this);
			}
		}
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		m_playTrigger.Serialize(ar, "PlayTrigger", "Play Trigger");
		m_stopTrigger.Serialize(ar, "StopTrigger", "Stop Trigger");

		if (ar.isInput())
			VersionFix(ar);

		if (m_playTrigger.HasName())
		{
			ar(m_followParticle, "FollowParticle", "Follow Particle");
			if (m_stopTrigger.HasName())
				m_stopOnDeath = false;
			else
				ar(m_stopOnDeath, "StopOnDeath", "Stop on Death");
		}
		else
			m_followParticle = m_stopOnDeath = false;

		ar(m_occlusionType, "Occlusion", "Occlusion");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		runtime.GetContainer().FillData(EPDT_AudioObject, (CryAudio::IObject*)0, runtime.SpawnedRange());
	}

	void MainPreUpdate(CParticleComponentRuntime& runtime) override
	{
		CryStackStringT<char, 512> proxyName;
		proxyName.append(runtime.GetComponent()->GetEffect()->GetName());
		proxyName.append(" : ");
		proxyName.append(runtime.GetComponent()->GetName());

		if (m_followParticle || m_stopOnDeath)
			TriggerFollowAudioEvents(runtime, proxyName);
		else
			TriggerSingleAudioEvents(runtime, proxyName);
	}

	void DestroyParticles(CParticleComponentRuntime& runtime) override
	{
		auto audioObjects = runtime.GetContainer().IStream(EPDT_AudioObject);
		for (auto particleId : runtime.FullRange())
		{
			if (auto pIObject = audioObjects.Load(particleId))
				gEnv->pAudioSystem->ReleaseObject(pIObject);
		}
	}

	uint GetNumResources() const override
	{
		return !!m_playTrigger + !!m_stopTrigger;
	}

	cstr GetResourceName(uint resourceId) const override
	{
		if (m_playTrigger)
			if (resourceId-- == 0)
				return m_playTrigger.m_name;
		if (m_stopTrigger)
			if (resourceId-- == 0)
				return m_stopTrigger.m_name;
		return nullptr;
	}

private:

	void VersionFix(Serialization::IArchive& ar)
	{
		// Back-compatibility without changing version number
		string triggerType;
		ar(triggerType, "Trigger", "Trigger");
		if (triggerType == "OnSpawn")
		{
			m_playTrigger.Serialize(ar, "Name", "Name");
		}
		else if (triggerType == "OnDeath")
		{
			m_stopTrigger.Serialize(ar, "Name", "Name");
		}
	}

	void TriggerSingleAudioEvents(CParticleComponentRuntime& runtime, cstr proxyName)
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const auto normAges = container.GetIFStream(EPDT_NormalAge);

		for (auto particleId : runtime.FullRange())
		{
			if (m_playTrigger && container.IsNewBorn(particleId))
			{
				Trigger(m_playTrigger, proxyName, positions.Load(particleId));
			}
			if (m_stopTrigger && IsExpired(normAges.Load(particleId)))
			{
				Trigger(m_stopTrigger, proxyName, positions.Load(particleId));
			}
		}
	}

	void TriggerFollowAudioEvents(CParticleComponentRuntime& runtime, cstr proxyName)
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const auto normAges = container.GetIFStream(EPDT_NormalAge);
		auto audioObjects = container.IOStream(EPDT_AudioObject);

		for (auto particleId : runtime.FullRange())
		{
			CryAudio::IObject* pIObject = audioObjects.Load(particleId);
			if (container.IsNewBorn(particleId))
			{
				if (m_playTrigger && !pIObject)
				{
					pIObject = MakeAudioObject(proxyName, positions.Load(particleId));
					audioObjects.Store(particleId, pIObject);
					pIObject->ExecuteTrigger(m_playTrigger);
				}
			}
			if (pIObject)
			{
				if (m_followParticle)
					pIObject->SetTransformation(positions.Load(particleId));
				if (IsExpired(normAges.Load(particleId)))
				{
					if (m_stopOnDeath)
						pIObject->StopTrigger(m_playTrigger);
					if (m_stopTrigger)
						pIObject->ExecuteTrigger(m_stopTrigger);
					gEnv->pAudioSystem->ReleaseObject(pIObject);
					audioObjects.Store(particleId, nullptr);
				}
			}
		}
	}

	void Trigger(CryAudio::ControlId id, cstr proxyName, const Vec3& position)
	{
		const CryAudio::SExecuteTriggerData data(id, proxyName, m_occlusionType, position, INVALID_ENTITYID, true);
		gEnv->pAudioSystem->ExecuteTriggerEx(data);
	}

	CryAudio::IObject* MakeAudioObject(cstr proxyName, const Vec3& position)
	{
		const CryAudio::SCreateObjectData data(proxyName, m_occlusionType, position, INVALID_ENTITYID, true);
		return gEnv->pAudioSystem->CreateObject(data);
	}


	struct SAudioTrigger
	{
		string              m_name;
		CryAudio::ControlId m_id    = CryAudio::InvalidControlId;

		void Serialize(Serialization::IArchive& ar, cstr name, cstr label)
		{
			ar(Serialization::AudioTrigger(m_name), name, label);
		}

		void Resolve()
		{
			m_id = CryAudio::StringToId(m_name.c_str());
		}
		bool HasName() const                 { return !m_name.empty(); }
		operator CryAudio::ControlId() const { return m_id; }
	};

	SAudioTrigger            m_playTrigger;
	SAudioTrigger            m_stopTrigger;
	bool                     m_followParticle = true;
	bool                     m_stopOnDeath    = true;
	CryAudio::EOcclusionType m_occlusionType  = CryAudio::EOcclusionType::Ignore;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAudioTrigger, "Audio", "Trigger", colorAudio);

class CFeatureAudioParameter : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureAudioParameter()
		: m_value(1.0f)
		, m_parameterId(CryAudio::InvalidControlId) {}

	void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		if (!m_parameterName.empty())
		{
			m_parameterId = CryAudio::StringToId(m_parameterName.c_str());
			pComponent->MainPreUpdate.add(this);
			m_value.AddToComponent(pComponent, this);
		}
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(Serialization::AudioRTPC(m_parameterName), "Name", "Name");
		ar(m_value, "Value", "Value");
	}

	void MainPreUpdate(CParticleComponentRuntime& runtime) override
	{
		CParticleContainer& container = runtime.GetContainer();
		if (!container.HasData(EPDT_AudioObject))
			return;

		auto audioObjects = container.IOStream(EPDT_AudioObject);
		STempUpdateBuffer<float> values(runtime, m_value, runtime.FullRange());

		for (auto particleId : runtime.FullRange())
		{
			if (auto* pIObject = audioObjects.Load(particleId))
			{
				pIObject->SetParameter(m_parameterId, values[particleId]);
			}
		}
	}

private:
	string                                m_parameterName;
	CryAudio::ControlId                   m_parameterId;
	CParamMod<EDD_ParticleUpdate, SFloat> m_value;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAudioParameter, "Audio", "Rtpc", colorAudio);
}// namespace pfx2
