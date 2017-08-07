#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"

////////////////////////////////////////////////////////
// Sample entity for creating a particle entity
////////////////////////////////////////////////////////
class CDefaultParticleEntity final
	: public CDesignerEntityComponent<IParticleEntityComponent>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CDefaultParticleEntity, IParticleEntityComponent, "ParticleEntity", "31b3ead4-c344-42f7-b794-b33746d4232b"_cry_guid);

public:
	CDefaultParticleEntity();

	// CDesignerEntityComponent
	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }

	virtual void OnResetState() override;
	// ~CDesignerEntityComponent

	// IParticleEntityComponent
	virtual void SetParticleEffectName(cstr effectName) override;
	// ~IParticleEntityComponent

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "ParticleEffect Properties"; }
	virtual void SerializeProperties(Serialization::IArchive& archive) override;
	// ~IEntityPropertyGroup

protected:
	void              Restart();
	void              Kill();
	IParticleEmitter* GetEmitter();

protected:
	string                 m_particleEffectPath;
	SpawnParams            m_spawnParams;
	TParticleAttributesPtr m_pAttributes;
	int                    m_particleSlot = -1;
	bool                   m_bActive = true;
};
