// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include <CrySerialization/Decorators/ResourcesAudio.h>
#include <CryAudio/IObject.h>
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
using IOAudioObjects = TIOStream<CryAudio::IObject*>;

EParticleDataType PDT(EPDT_AudioObject, CryAudio::IObject*);

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
			pComponent->AddToUpdateList(EUL_MainPreUpdate, this);
			pComponent->AddParticleData(EPVF_Position);
			if (m_followParticle || m_stopOnDeath)
			{
				pComponent->AddParticleData(EPDT_AudioObject);
				pComponent->AddToUpdateList(EUL_InitUpdate, this);
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

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		context.m_container.FillData(EPDT_AudioObject, (CryAudio::IObject*)0, context.GetSpawnedRange());
	}

	void MainPreUpdate(CParticleComponentRuntime* pComponentRuntime) override
	{
		CryStackStringT<char, 512> proxyName;
		proxyName.append(pComponentRuntime->GetComponent()->GetEffect()->GetName());
		proxyName.append(" : ");
		proxyName.append(pComponentRuntime->GetComponent()->GetName());

		if (m_followParticle || m_stopOnDeath)
			TriggerFollowAudioEvents(pComponentRuntime, proxyName);
		else
			TriggerSingleAudioEvents(pComponentRuntime, proxyName);
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

	void TriggerSingleAudioEvents(CParticleComponentRuntime* pComponentRuntime, cstr proxyName)
	{
		CRY_PFX2_PROFILE_DETAIL;

		const SUpdateContext context(pComponentRuntime);
		CParticleContainer& container = context.m_container;
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const auto states = container.GetTIStream<uint8>(EPDT_State);

		for (auto particleId : context.GetUpdateRange())
		{
			const uint8 state = states.Load(particleId);
			if (m_playTrigger && state == ES_NewBorn)
			{
				Trigger(m_playTrigger, proxyName, positions.Load(particleId));
			}
			else if (m_stopTrigger && state == ES_Expired)
			{
				Trigger(m_stopTrigger, proxyName, positions.Load(particleId));
			}
		}
	}

	void TriggerFollowAudioEvents(CParticleComponentRuntime* pComponentRuntime, cstr proxyName)
	{
		CRY_PFX2_PROFILE_DETAIL;

		const SUpdateContext context(pComponentRuntime);
		CParticleContainer& container = context.m_container;
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const auto states = container.GetTIStream<uint8>(EPDT_State);
		IOAudioObjects audioObjects = container.GetTIOStream<CryAudio::IObject*>(EPDT_AudioObject);

		for (auto particleId : context.GetUpdateRange())
		{
			CryAudio::IObject* pIObject = audioObjects.Load(particleId);
			const uint8 state = states.Load(particleId);
			switch (state)
			{
			case ES_NewBorn:
				if (m_playTrigger && !pIObject)
				{
					pIObject = MakeAudioObject(proxyName, positions.Load(particleId));
					audioObjects.Store(particleId, pIObject);
					pIObject->ExecuteTrigger(m_playTrigger);
				}
				break;
			case ES_Alive:
				if (m_followParticle && pIObject)
					pIObject->SetTransformation(positions.Load(particleId));
				break;
			case ES_Expired:
				if (m_stopTrigger && pIObject)
					pIObject->ExecuteTrigger(m_stopTrigger);
				break;
			case ES_Dead:
				if (pIObject)
				{
					if (m_stopOnDeath)
						pIObject->StopTrigger(m_playTrigger);
					gEnv->pAudioSystem->ReleaseObject(pIObject);
					audioObjects.Store(particleId, nullptr);
				}
			}
		}
	}

	void Trigger(CryAudio::ControlId id, cstr proxyName, const Vec3& position)
	{
		const CryAudio::SExecuteTriggerData data(proxyName, m_occlusionType, position, true, id);
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
			pComponent->AddToUpdateList(EUL_MainPreUpdate, this);
			m_value.AddToComponent(pComponent, this);
		}
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(Serialization::AudioRTPC(m_parameterName), "Name", "Name");
		ar(m_value, "Value", "Value");
	}

	void MainPreUpdate(CParticleComponentRuntime* pComponentRuntime) override
	{
		const SUpdateContext context(pComponentRuntime);
		CParticleContainer& container = context.m_container;
		if (!container.HasData(EPDT_AudioObject))
			return;
		IOAudioObjects audioObjects = container.GetTIOStream<CryAudio::IObject*>(EPDT_AudioObject);

		STempModBuffer<float> values(context, m_value);
		values.ModifyUpdate(context, m_value, container.GetFullRange());

		for (auto particleId : context.GetUpdateRange())
		{
			const float value = values.m_stream.Load(particleId);
			CryAudio::IObject* const pIObject = audioObjects.Load(particleId);
			if (pIObject != nullptr)
			{
				pIObject->SetParameter(m_parameterId, value);
			}
		}
	}

private:
	string                               m_parameterName;
	CryAudio::ControlId                  m_parameterId;
	CParamMod<SModParticleField, SFloat> m_value;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAudioParameter, "Audio", "Rtpc", colorAudio);
}// namespace pfx2
