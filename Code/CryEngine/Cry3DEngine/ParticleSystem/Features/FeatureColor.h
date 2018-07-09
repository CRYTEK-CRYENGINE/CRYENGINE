// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleSystem/ParticleFeature.h"
#include "ParticleSystem/ParticleDataStreams.h"

namespace pfx2
{

class CFeatureFieldColor;

struct IColorModifier : public _i_reference_target_t
{
public:
	bool         IsEnabled() const                                                                            { return m_enabled; }
	virtual void AddToParam(CParticleComponent* pComponent, CFeatureFieldColor* pParam)                       {}
	virtual void Serialize(Serialization::IArchive& ar);
	virtual void Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOColorStream stream) const {}
	virtual void Sample(Vec3* samples, int samplePoints) const                                                {}
private:
	SEnable m_enabled;
};

class CFeatureFieldColor : public CParticleFeature
{
private:
	typedef _smart_ptr<IColorModifier> PColorModifier;

public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureFieldColor();

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void InitParticles(CParticleComponentRuntime& runtime) override;
	virtual void UpdateParticles(CParticleComponentRuntime& runtime) override;
	virtual void AddToInitParticles(IColorModifier* pMod);
	virtual void AddToUpdate(IColorModifier* pMod);

private:
	void Sample(Vec3* samples, const int numSamples);

	std::vector<PColorModifier>  m_modifiers;
	std::vector<IColorModifier*> m_modInit;
	std::vector<IColorModifier*> m_modUpdate;
	ColorB                       m_color;
};

}
