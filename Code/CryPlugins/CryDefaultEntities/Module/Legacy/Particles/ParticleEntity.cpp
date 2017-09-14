#include "StdAfx.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/IParticlesPfx2.h>
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ActionButton.h>

#include "ParticleEntity.h"

class CParticleRegistrator
	: public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("ParticleEffect") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class ParticleEffect, overridden by game");
			return;
		}

		IEntityClass* pClass = RegisterEntityWithDefaultComponent<CDefaultParticleEntity>("ParticleEffect", "Particles", "Particles.bmp");
		pClass->SetFlags(pClass->GetFlags() | ECLF_INVISIBLE);
	}
};

CParticleRegistrator g_particleRegistrator;

CRYREGISTER_CLASS(CDefaultParticleEntity);

CDefaultParticleEntity::CDefaultParticleEntity()
{
	m_pAttributes = pfx2::GetIParticleSystem()->CreateParticleAttributes();
}

void CDefaultParticleEntity::SetParticleEffectName(cstr effectName)
{
	m_particleEffectPath = effectName;
	OnResetState();
}

void CDefaultParticleEntity::SerializeProperties(Serialization::IArchive& archive)
{
	IEntity& entity = *GetEntity();
	string prevEffectPath = m_particleEffectPath;
	bool wasActive = m_bActive;

	if (archive.openBlock("particleButtons", "Emitter"))
	{
		archive(Serialization::ActionButton(functor(*this, &CDefaultParticleEntity::Restart)), "Restart", "^Restart");
		archive(Serialization::ActionButton(functor(*this, &CDefaultParticleEntity::Kill)), "Kill", "^Kill");
		archive.closeBlock();
	}

	archive(Serialization::ParticlePickerLegacy(m_particleEffectPath), "ParticleEffect", "ParticleEffect");
	archive(m_bActive, "Active", "Active");
	archive(m_spawnParams, "Parameters", "Parameters");

	if (archive.isInput())
	{
		if (m_particleEffectPath != prevEffectPath || wasActive != m_bActive)
		{
			OnResetState();
		}
		else
		{
			IParticleEmitter* pEmitter = entity.GetParticleEmitter(m_particleSlot);
			if (pEmitter)
				pEmitter->SetSpawnParams(m_spawnParams);
		}
	}

	archive(*m_pAttributes, "Attributes", "Attributes");
	if (m_particleSlot != -1 && (archive.isInput() || archive.isEdit()))
		GetEmitter()->GetAttributes().Reset(m_pAttributes.get());
}

void CDefaultParticleEntity::OnResetState()
{
	IEntity& entity = *GetEntity();

	// Check if we have to unload first
	if (m_particleSlot != -1)
	{
		entity.FreeSlot(m_particleSlot);
		m_particleSlot = -1;
	}

	// Check if the emitter is active
	if (!m_bActive)
		return;

	IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(m_particleEffectPath, "ParticleEntity");
	if (pEffect)
	{
		m_particleSlot = entity.LoadParticleEmitter(-1, pEffect, &m_spawnParams);
		GetEmitter()->GetAttributes().TransferInto(m_pAttributes.get());
		GetEmitter()->GetAttributes().Reset(m_pAttributes.get());
	}
}

void CDefaultParticleEntity::Restart()
{
	m_bActive = true;
	OnResetState();
}

void CDefaultParticleEntity::Kill()
{
	m_bActive = false;
	IEntity& entity = *GetEntity();
	IParticleEmitter* pEmitter = entity.GetParticleEmitter(m_particleSlot);
	if (pEmitter)
		pEmitter->Kill();
	OnResetState();
}

IParticleEmitter* CDefaultParticleEntity::GetEmitter()
{
	IEntity& entity = *GetEntity();
	return entity.GetParticleEmitter(m_particleSlot);
}
