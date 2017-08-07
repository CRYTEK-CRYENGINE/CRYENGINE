// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     06/04/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLEEFFECT_H
#define PARTICLEEFFECT_H

#pragma once

#include "ParticleCommon.h"
#include "ParticleComponent.h"
#include "ParticleAttributes.h"

namespace pfx2
{

struct SSerializationContext
{
	SSerializationContext(uint documentVersion)
		: m_documentVersion(documentVersion) {}
	uint m_documentVersion;
};

class CParticleEffect : public IParticleEffectPfx2
{
public:
	CParticleEffect();

	// pfx2 IParticleEffect
	virtual cstr                   GetName() const override;
	virtual void                   Serialize(Serialization::IArchive& ar) override;
	virtual IParticleEmitter*      Spawn(const ParticleLoc& loc, const SpawnParams* pSpawnParams = NULL) override;
	virtual uint                   GetNumComponents() const override              { return m_components.size(); }
	virtual IParticleComponent*    GetComponent(uint componentIdx) const override { return m_components[componentIdx]; }
	virtual IParticleComponent*    AddComponent() override;
	virtual void                   RemoveComponent(uint componentIdx) override;
	virtual void                   SetChanged() override;
	virtual Serialization::SStruct GetEffectOptionsSerializer() const override;
	// ~pfx2 IParticleEffect

	// pfx1 IParticleEffect
	virtual int                   GetVersion() const override                                        { return 2; }
	virtual void                  GetMemoryUsage(ICrySizer* pSizer) const override                   {}
	virtual void                  SetName(cstr name) override;
	virtual stack_string          GetFullName() const override                                       { return GetName(); }
	virtual void                  SetEnabled(bool bEnabled) override                                 {}
	virtual bool                  IsEnabled(uint options) const override                             { return true; }
	virtual bool                  IsTemporary() const override                                       { return false; }
	virtual void                  SetParticleParams(const ParticleParams& params) override           {}
	virtual const ParticleParams& GetParticleParams() const override                                 { return GetDefaultParams(); }
	virtual const ParticleParams& GetDefaultParams() const override;
	virtual int                   GetChildCount() const override                                     { return 0; }
	virtual IParticleEffect*      GetChild(int index) const override                                 { return 0; }
	virtual void                  ClearChilds() override                                             {}
	virtual void                  InsertChild(int slot, IParticleEffect* pEffect) override           {}
	virtual int                   FindChild(IParticleEffect* pEffect) const override                 { return -1; }
	virtual void                  SetParent(IParticleEffect* pParent) override                       {}
	virtual IParticleEffect*      GetParent() const override                                         { return 0; }
	virtual bool                  LoadResources() override                                           { return true; }
	virtual void                  UnloadResources() override                                         {}
	virtual void                  Serialize(XmlNodeRef node, bool bLoading, bool bChildren) override {}
	virtual void                  Reload(bool bChildren) override                                    {}
	virtual IParticleAttributes&  GetAttributes() override                                           { return m_attributeInstance; }
	virtual bool                  IsSubstitutedPfx1() const override                                 { return m_substitutedPfx1; }
	virtual void                  SetSubstitutedPfx1(bool b) override                                { m_substitutedPfx1 = b; }
	// ~pfx1 IParticleEmitter

	void                      Compile();
	TComponents&              GetComponents()                                               { return m_components; }
	const TComponents&        GetComponents() const                                         { return m_components; }
	CParticleComponent*       FindComponentByName(const char* name) const;
	TAttributeTablePtr        GetAttributeTable() const                                     { return m_pAttributes; }
	string                    MakeUniqueName(const CParticleComponent* forComponent, const char* name);
	uint                      AddRenderObjectId();
	uint                      GetNumRenderObjectIds() const;
	float                     GetEquilibriumTime() const;
	int                       GetEditVersion() const;

private:
	string             m_name;
	TAttributeTablePtr m_pAttributes;
	CAttributeInstance m_attributeInstance;
	TComponents        m_components;
	uint               m_numRenderObjects;
	int                m_editVersion;
	bool               m_dirty;
	bool               m_substitutedPfx1;

	void               Sort();
};

}

#endif // PARTICLEEFFECT_H
