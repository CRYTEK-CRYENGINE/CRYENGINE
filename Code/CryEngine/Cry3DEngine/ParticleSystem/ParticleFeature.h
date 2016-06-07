// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     24/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLEFEATURE_H
#define PARTICLEFEATURE_H

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/ClassFactory.h>

#include "ParticleSystem.h"
#include "ParticleEffect.h"
#include "ParticleComponent.h"
#include "ParticleComponentRuntime.h"

#include <CryRenderer/IGpuParticles.h>

namespace pfx2
{

struct SGpuInterfaceRef
{
	SGpuInterfaceRef(gpu_pfx2::EGpuFeatureType feature) : feature(feature) {}
	gpu_pfx2::EGpuFeatureType                          feature;
	_smart_ptr<gpu_pfx2::IParticleFeatureGpuInterface> gpuInterface;
};

class CParticleFeature : public IParticleFeature, public _i_reference_target_t
{
public:
	CParticleFeature() : m_gpuInterfaceRef(gpu_pfx2::eGpuFeatureType_None) {}
	CParticleFeature(gpu_pfx2::EGpuFeatureType feature) : m_gpuInterfaceRef(feature) {}

	// IParticleFeature
	void                                    SetEnabled(bool enabled) override                 { m_enabled.Set(enabled); }
	bool                                    IsEnabled() const override                        { return m_enabled; }
	void                                    Serialize(Serialization::IArchive& ar) override;
	uint                                    GetNumConnectors() const override                 { return 0; }
	const char*                             GetConnectorName(uint connectorId) const override { return nullptr; }
	void                                    ConnectTo(const char* pOtherName) override        {}
	void                                    DisconnectFrom(const char* pOtherName) override   {}
	gpu_pfx2::IParticleFeatureGpuInterface* GetGpuInterface() override;
	// ~IParticleFeature

	// Initialization
	virtual void         ResolveDependency(CParticleComponent* pComponent)                         {}
	virtual void         AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) {}
	virtual EFeatureType GetFeatureType()                                                          { return EFT_Generic; }

	// EUL_MainPreUpdate
	virtual void MainPreUpdate(CParticleComponentRuntime* pComponentRuntime) {}

	// EUL_InitSubInstance
	virtual void InitSubInstance(CParticleComponentRuntime* pComponentRuntime, size_t firstInstance, size_t lastInstance) {}

	// EUL_Spawn
	virtual void SpawnParticles(const SUpdateContext& context) {}

	// EUL_InitUpdate
	virtual void InitParticles(const SUpdateContext& context) {}

	// EUL_PostInitUpdate
	virtual void PostInitParticles(const SUpdateContext& context) {}

	// EUL_KillUpdate
	virtual void KillParticles(const SUpdateContext& context, TParticleId* pParticles, size_t count) {}

	// EUL_Update
	virtual void Update(const SUpdateContext& context) {}

	// EUL_PostUpdate
	virtual void PostUpdate(const SUpdateContext& context) {}

	// EUL_Render
	virtual void Render(ICommonParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, IRenderNode* pNode, const SRenderContext& renderContext) {}
	virtual void ComputeVertices(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels)  {}

	// EUL_GetExtents
	virtual void GetSpatialExtents(const SUpdateContext& context, Array<const float, uint> scales, Array<float, uint> extents) {}

protected:
	void AddNoPropertiesLabel(Serialization::IArchive& ar);

private:
	SGpuInterfaceRef m_gpuInterfaceRef;
	SEnable          m_enabled;
};

static const ColorB defaultColor = ColorB(128, 146, 165);
static const ColorB spawnColor = ColorB(128, 165, 156);
static const ColorB secondGenColor = ColorB(159, 121, 121);
static const ColorB renderFeatureColor = ColorB(232, 127, 0);
static const ColorB motionColor = ColorB(163, 156, 126);
static const ColorB appearenceColor = ColorB(147, 125, 162);
static const ColorB fieldColor = ColorB(152, 162, 125);
static const char* defaultIcon = "Editor/Icons/Particles/generic.png";

#define CRY_PFX2_DECLARE_FEATURE                                 \
  static const SParticleFeatureParams &GetStaticFeatureParams(); \
  virtual const SParticleFeatureParams& GetFeatureParams() const override;

#define CRY_PFX2_IMPLEMENT_FEATURE_INTERNAL(BaseType, Type, GroupName, FeatureName, IconName, Color, UseConnector)                \
  static struct SInit ## Type {SInit ## Type() { GetFeatureParams().push_back(Type::GetStaticFeatureParams()); } } gInit ## Type; \
  static IParticleFeature* Create ## Type() { return new Type(); }                                                                \
  const SParticleFeatureParams& Type::GetStaticFeatureParams() {                                                                  \
    static SParticleFeatureParams params;                                                                                         \
    params.m_groupName = GroupName; params.m_featureName = FeatureName; params.m_iconName = IconName;                             \
    params.m_color = Color;                                                                                                       \
    params.m_pFactory = Create ## Type;                                                                                           \
    params.m_hasComponentConnector = UseConnector;                                                                                \
    return params; }                                                                                                              \
  const SParticleFeatureParams& Type::GetFeatureParams() const { return GetStaticFeatureParams(); }                               \
  SERIALIZATION_CLASS_NAME(BaseType, Type, GroupName FeatureName, GroupName FeatureName);

#define CRY_PFX2_IMPLEMENT_FEATURE(BaseType, Type, GroupName, FeatureName, IconName, Color) \
  CRY_PFX2_IMPLEMENT_FEATURE_INTERNAL(BaseType, Type, GroupName, FeatureName, IconName, Color, false)

#define CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(BaseType, Type, GroupName, FeatureName, IconName, Color) \
  CRY_PFX2_IMPLEMENT_FEATURE_INTERNAL(BaseType, Type, GroupName, FeatureName, IconName, Color, true)

}

#endif // PARTICLEFUNCTION_H
