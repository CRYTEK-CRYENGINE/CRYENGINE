// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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
private:
	typedef _smart_ptr<CParticleComponent> TComponentPtr;
	typedef std::vector<TComponentPtr>     TComponents;

public:
	CParticleEffect();

	// pfx2 IParticleEmitter
	virtual cstr                   GetName() const override;
	virtual void                   Serialize(Serialization::IArchive& ar) override;
	virtual IParticleEmitter*      Spawn(const ParticleLoc& loc, const SpawnParams* pSpawnParams = NULL) override;
	virtual uint                   GetNumComponents() const override              { return m_components.size(); }
	virtual IParticleComponent*    GetComponent(uint componentIdx) const override { return m_components[componentIdx]; }
	virtual void                   AddComponent(uint componentIdx) override;
	virtual void                   RemoveComponent(uint componentIdx) override;
	virtual void                   SetChanged() override;
	virtual Serialization::SStruct GetEffectOptionsSerializer() const override;
	// ~pfx2 IParticleEmitter

	// pfx1 IParticleEmitter
	virtual int                   GetVersion() const override                                        { return 2; }
	virtual void                  GetMemoryUsage(ICrySizer* pSizer) const override                   {}
	virtual void                  SetName(cstr name) override;
	virtual stack_string          GetFullName() const override                                       { return GetName(); }
	virtual void                  SetEnabled(bool bEnabled) override                                 {}
	virtual bool                  IsEnabled() const override                                         { return true; }
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
	// ~pfx1 IParticleEmitter

	void                      Compile();
	CParticleComponent*       GetCComponent(TComponentId componentIdx)       { return m_components[componentIdx]; }
	const CParticleComponent* GetCComponent(TComponentId componentIdx) const { return m_components[componentIdx]; }
	TComponentId              FindComponentIdByName(const char* name) const;
	TAttributeTablePtr        GetAttributeTable() const                      { return m_pAttributes; }
	string                    MakeUniqueName(TComponentId forComponentId, const char* name);
	uint                      AddRenderObjectId();
	uint                      GetNumRenderObjectIds() const;
	float                     GetEquilibriumTime() const;
	int                       GetEditVersion() const;

	int m_id;

private:
	TAttributeTablePtr m_pAttributes;
	CAttributeInstance m_attributeInstance;
	TComponents        m_components;
	string             m_name;
	uint               m_numRenderObjects;
	int                m_editVersion;
	bool               m_dirty;
};

}

#endif // PARTICLEEFFECT_H
