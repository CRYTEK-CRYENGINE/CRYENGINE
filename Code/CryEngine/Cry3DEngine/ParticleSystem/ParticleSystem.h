// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/ClassWeaver.h>
#include "ParticleCommon.h"
#include "ParticleDebug.h"
#include "ParticleJobManager.h"
#include "ParticleProfiler.h"
#include "ParticleEmitter.h"
#include "ParticleEffect.h"

namespace pfx2
{
class CParticleSystem : public Cry3DEngineBase, public IParticleSystem, ISyncMainWithRenderListener
{
	CRYINTERFACE_SIMPLE(IParticleSystem)
	CRYGENERATE_SINGLETONCLASS_GUID(CParticleSystem, "CryEngine_ParticleSystem", "cd8d738d-54b4-46f7-82ba-23ba999cf2ac"_cry_guid)

	CParticleSystem();
	~CParticleSystem();

private:
	typedef std::unordered_map<string, _smart_ptr<CParticleEffect>, stl::hash_stricmp<string>, stl::hash_stricmp<string>> TEffectNameMap;

public:
	// IParticleSystem
	PParticleEffect         CreateEffect() override;
	PParticleEffect         ConvertEffect(const ::IParticleEffect* pOldEffect, bool bReplace) override;
	void                    RenameEffect(PParticleEffect pEffect, cstr name) override;
	PParticleEffect         FindEffect(cstr name, bool bAllowLoad = true) override;
	PParticleEmitter        CreateEmitter(PParticleEffect pEffect) override;
	uint                    GetNumFeatureParams() const override { return uint(GetFeatureParams().size()); }
	SParticleFeatureParams& GetFeatureParam(uint featureIdx) const override { return GetFeatureParams()[featureIdx]; }
	TParticleAttributesPtr  CreateParticleAttributes() const override;

	void                    OnFrameStart() override;
	void                    Update() override;
	void                    Reset() override;

	void                    Serialize(TSerialize ser) override;
	bool                    SerializeFeatures(IArchive& ar, TParticleFeatures& features, cstr name, cstr label) const override;

	void                    GetStats(SParticleStats& stats) override;
	void                    GetMemoryUsage(ICrySizer* pSizer) const override;
	void                    SyncMainWithRender() override;
	// ~IParticleSystem

	struct SThreadData
	{
		TParticleHeap  memHeap;
		SParticleStats statsCPU;
		SParticleStats statsGPU;
	};

	PParticleEffect          LoadEffect(cstr effectName);

	SThreadData&             GetThreadData() { return m_threadData[JobManager::GetWorkerThreadId() + 1]; }
	SThreadData&             GetMainData()   { return m_threadData.front(); }
	SThreadData&             GetSumData()    { return m_threadData.back(); }
	CParticleJobManager&     GetJobManager() { return m_jobManager; }
	CParticleProfiler&       GetProfiler()   { return m_profiler; }

	void                     FinishUpdate();
	void                     DeferredRender(const SRenderingPassInfo& passInfo);
	float                    DisplayDebugStats(Vec2 displayLocation, float lineHeight);

	bool                     IsRuntime() const                { return m_numClears > 0 && m_numFrames > 1; }
	void                     CheckFileAccess(cstr filename = 0) const;
	void                     ClearRenderResources();

	static float             GetMaxAngularDensity(const CCamera& camera);
	QuatT                    GetLastCameraPose() const        { return m_lastCameraPose; }
	QuatT                    GetCameraMotion() const          { return m_cameraMotion; }
	const TParticleEmitters& GetActiveEmitters() const        { return m_emitters; }
	IMaterial*               GetFlareMaterial();

	typedef std::vector<SParticleFeatureParams> TFeatureParams;
	
	static TFeatureParams&               GetFeatureParams()                                    { static TFeatureParams params; return params; }
	static bool                          RegisterFeature(const SParticleFeatureParams& params) { GetFeatureParams().push_back(params); return true; }
	static const SParticleFeatureParams* FindFeatureParam(cstr groupName, cstr featureName);
	static const SParticleFeatureParams* GetDefaultFeatureParam(EFeatureType);

private:
	void              TrimEmitters(bool finished_only);

	// PFX1 to PFX2
	string ConvertPfx1Name(cstr oldEffectName);
	// ~PFX1 to PFX2

private:
	CParticleJobManager      m_jobManager;
	CParticleProfiler        m_profiler;
	TEffectNameMap           m_effects;
	TParticleEmitters        m_emitters;
	TParticleEmitters        m_newEmitters;
	std::vector<SThreadData> m_threadData;
	_smart_ptr<IMaterial>    m_pFlareMaterial;
	bool                     m_bResetEmitters = false;
	QuatT                    m_lastCameraPose = IDENTITY;
	QuatT                    m_cameraMotion   = ZERO;
	uint                     m_numClears      = 0;
	uint                     m_numFrames      = 0;
	uint                     m_nextEmitterId  = 0;
	ESystemConfigSpec        m_lastSysSpec    = END_CONFIG_SPEC_ENUM;
};

ILINE CParticleSystem* GetPSystem()
{
	static std::shared_ptr<IParticleSystem> pSystem(GetIParticleSystem());
	return static_cast<CParticleSystem*>(pSystem.get());
};

uint GetVersion(IArchive& ar);

}
