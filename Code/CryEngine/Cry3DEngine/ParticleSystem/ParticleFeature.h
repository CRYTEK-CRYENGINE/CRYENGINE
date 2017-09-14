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
	void                                    SetGpuInterfaceNeeded(bool gpuInterface)          { m_gpuInterfaceNeeded = gpuInterface; }
	uint                                    GetNumResources() const override                  { return 0; }
	const char*                             GetResourceName(uint resourceId) const override   { return nullptr; }
	gpu_pfx2::IParticleFeatureGpuInterface* GetGpuInterface() override;
	// ~IParticleFeature

	// Initialization
	virtual bool              VersionValidate(CParticleComponent* pComponent)                           { return true; }
	virtual void              ResolveDependency(CParticleComponent* pComponent)                         {}
	virtual void              AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) {}
	virtual EFeatureType      GetFeatureType()                                                          { return EFT_Generic; }
	virtual bool              CanMakeRuntime(CParticleEmitter* pEmitter) const                          { return true; }

	// EUL_MainPreUpdate
	virtual void MainPreUpdate(CParticleComponentRuntime* pComponentRuntime) {}

	// EUL_InitSubInstance
	virtual void InitSubInstance(CParticleComponentRuntime* pComponentRuntime, size_t firstInstance, size_t lastInstance) {}

	// EUL_GetExtents
	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) {}

	// EUL_GetEmitOffset
	virtual Vec3 GetEmitOffset(const SUpdateContext& context, TParticleId parentId) { return Vec3(0); }

	// EUL_Spawn
	virtual void SpawnParticles(const SUpdateContext& context) {}

	// EUL_InitUpdate
	virtual void InitParticles(const SUpdateContext& context) {}

	// EUL_PostInitUpdate
	virtual void PostInitParticles(const SUpdateContext& context) {}

	// EUL_KillUpdate
	virtual void KillParticles(const SUpdateContext& context, TConstArray<TParticleId> particleIds) {}

	// EUL_PreUpdate
	virtual void PreUpdate(const SUpdateContext& context) {}

	// EUL_Update
	virtual void Update(const SUpdateContext& context) {}

	// EUL_PostUpdate
	virtual void PostUpdate(const SUpdateContext& context) {}

	// EUL_ComputeBounds
	virtual void ComputeBounds(IParticleComponentRuntime* pComponentRuntime, AABB& bounds) {}

	// EUL_Render
	virtual void PrepareRenderObjects(CParticleEmitter* pEmitter, CParticleComponent* pComponent)                                                                            {}
	virtual void ResetRenderObjects(CParticleEmitter* pEmitter, CParticleComponent* pComponent)                                                                              {}
	virtual void Render(CParticleEmitter* pEmitter, IParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, const SRenderContext& renderContext) {}
	virtual void ComputeVertices(CParticleComponentRuntime* pComponentRuntime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels)          {}

protected:
	void AddNoPropertiesLabel(Serialization::IArchive& ar);

private:
	SGpuInterfaceRef m_gpuInterfaceRef;
	SEnable          m_enabled;
	bool             m_gpuInterfaceNeeded;
};

ILINE ColorB HexToColor(uint hex)
{
	return ColorB((hex >> 16) & 0xff, (hex >> 8) & 0xff, hex & 0xff);
}

static const ColorB colorAppearance = HexToColor(0x00ffba);
static const ColorB colorField      = HexToColor(0x02ba25);
static const ColorB colorRender     = HexToColor(0xc2fbbe);
static const ColorB colorLocation   = HexToColor(0x30a8fd);
static const ColorB colorAngles     = HexToColor(0x84d7fa);
static const ColorB colorSpawn      = HexToColor(0xfc7070);
static const ColorB colorLife       = HexToColor(0xd0c0ac);
static const ColorB colorKill       = HexToColor(0xd0c0ac);
static const ColorB colorVelocity   = HexToColor(0xcea639);
static const ColorB colorMotion     = HexToColor(0xfb9563);
static const ColorB colorLight      = HexToColor(0xfffdd0);
static const ColorB colorAudio      = HexToColor(0xd671f7);
static const ColorB colorGeneral    = HexToColor(0xececec);
static const ColorB colorChild      = HexToColor(0xc0c0c0);
static const ColorB colorProject    = HexToColor(0xc0c0c0);
static const ColorB colorGPU        = HexToColor(0x00e87e);
static const ColorB colorComponent  = HexToColor(0x000000);

#define CRY_PFX2_DECLARE_FEATURE \
  struct SFeatureParams; \
  virtual const SParticleFeatureParams& GetFeatureParams() const override;

#define CRY_PFX2_IMPLEMENT_FEATURE_INTERNAL(BaseType, Type, GroupName, FeatureName, Color, UseConnector, DefaultForType) \
  struct Type::SFeatureParams: SParticleFeatureParams { SFeatureParams() {                                               \
    m_groupName = GroupName;                                                                                             \
    m_featureName = FeatureName;                                                                                         \
    m_color = Color;                                                                                                     \
    m_pFactory = []() -> IParticleFeature* { return new Type(); };                                                       \
    m_hasComponentConnector = UseConnector;                                                                              \
    m_defaultForType = DefaultForType;                                                                                   \
  } };                                                                                                                   \
  const SParticleFeatureParams& Type::GetFeatureParams() const { static Type::SFeatureParams params; return params; }    \
  static bool sInit ## Type = CParticleSystem::RegisterFeature(Type::SFeatureParams());                                  \
  SERIALIZATION_CLASS_NAME(BaseType, Type, GroupName FeatureName, GroupName FeatureName);                                \

#define CRY_PFX2_IMPLEMENT_FEATURE(BaseType, Type, GroupName, FeatureName, Color) \
  CRY_PFX2_IMPLEMENT_FEATURE_INTERNAL(BaseType, Type, GroupName, FeatureName, Color, false, 0)

#define CRY_PFX2_IMPLEMENT_FEATURE_DEFAULT(BaseType, Type, GroupName, FeatureName, Color, ForType) \
  CRY_PFX2_IMPLEMENT_FEATURE_INTERNAL(BaseType, Type, GroupName, FeatureName, Color, false, ForType)

#define CRY_PFX2_IMPLEMENT_FEATURE_WITH_CONNECTOR(BaseType, Type, GroupName, FeatureName, Color) \
  CRY_PFX2_IMPLEMENT_FEATURE_INTERNAL(BaseType, Type, GroupName, FeatureName, Color, true, 0)

#define CRY_PFX2_LEGACY_FEATURE(BaseType, NewType, LegacyName)           \
	SERIALIZATION_CLASS_NAME(BaseType, NewType, LegacyName, LegacyName);

}

#endif // PARTICLEFUNCTION_H
