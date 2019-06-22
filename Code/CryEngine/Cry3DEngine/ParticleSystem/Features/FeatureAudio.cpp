// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include <CrySerialization/Decorators/ResourcesAudio.h>
#include "../ParticleComponentRuntime.h"
#include "../ParticleEmitter.h"
#include "../ParticleEffect.h"
#include "ParamMod.h"
#include <CryAudio/IObject.h>

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

		m_proxyName = pComponent->GetEffect()->GetName();
		m_proxyName.append(" : ");
		m_proxyName.append(pComponent->GetName());

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
		if (m_followParticle || m_stopOnDeath)
			TriggerFollowAudioEvents(runtime);
		else
			TriggerSingleAudioEvents(runtime);
	}

	void DestroyParticles(CParticleComponentRuntime& runtime) override
	{
		auto audioObjects = runtime.GetContainer().IStream(EPDT_AudioObject);
		for (auto particleId : runtime.FullRange())
		{
			if (auto pIObject = audioObjects.Load(particleId))
			{
				StopAudio(pIObject);
			}
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

	void TriggerSingleAudioEvents(CParticleComponentRuntime& runtime)
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const auto normAges = container.GetIFStream(EPDT_NormalAge);
		const auto hidden = runtime.GetEmitter()->IsHidden();

		for (auto particleId : runtime.FullRange())
		{
			if (m_playTrigger && !hidden && container.IsNewBorn(particleId))
			{
				Trigger(m_playTrigger, positions.Load(particleId));
			}
			if (m_stopTrigger && (hidden || IsExpired(normAges.Load(particleId))))
			{
				Trigger(m_stopTrigger, positions.Load(particleId));
			}
		}
	}

	void TriggerFollowAudioEvents(CParticleComponentRuntime& runtime)
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const auto normAges = container.GetIFStream(EPDT_NormalAge);
		auto audioObjects = container.IOStream(EPDT_AudioObject);
		const auto hidden = runtime.GetEmitter()->IsHidden();

		for (auto particleId : runtime.FullRange())
		{
			CryAudio::IObject* pIObject = audioObjects.Load(particleId);
			if (!pIObject && !hidden && m_playTrigger)
			{
				pIObject = MakeAudioObject(positions.Load(particleId));
				audioObjects.Store(particleId, pIObject);
				pIObject->ExecuteTrigger(m_playTrigger);
			}
			if (pIObject)
			{
				if (m_followParticle)
					pIObject->SetTransformation(positions.Load(particleId));
				if (hidden || IsExpired(normAges.Load(particleId)))
				{
					StopAudio(pIObject);
					audioObjects.Store(particleId, nullptr);
				}
			}
		}
	}

	void Trigger(CryAudio::ControlId id, const Vec3& position)
	{
		const CryAudio::SExecuteTriggerData data(id, m_proxyName, m_occlusionType, position, INVALID_ENTITYID, true);
		gEnv->pAudioSystem->ExecuteTriggerEx(data);
	}

	CryAudio::IObject* MakeAudioObject(const Vec3& position)
	{
		const CryAudio::SCreateObjectData data(m_proxyName, m_occlusionType, position, true);
		return gEnv->pAudioSystem->CreateObject(data);
	}

	void StopAudio(CryAudio::IObject* pIObject)
	{
		if (m_stopTrigger)
			pIObject->ExecuteTrigger(m_stopTrigger);
		else if (m_playTrigger && m_stopOnDeath)
			pIObject->StopTrigger(m_playTrigger);
		gEnv->pAudioSystem->ReleaseObject(pIObject);
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
	string                   m_proxyName;
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
		ar(Serialization::AudioParameter(m_parameterName), "Name", "Name");
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
