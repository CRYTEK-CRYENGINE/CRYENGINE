// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleEffect.h"
#include <CryCore/BitFiddling.h>
#include "ParticleList.h"
#include "ParticleEnviron.h"
#include "ParticleMemory.h"
#include <CrySystem/Profilers/IPerfHud.h>

#if !defined(_RELEASE)
// when not in release, collect information about vertice/indice pool usage
	#define PARTICLE_COLLECT_VERT_IND_POOL_USAGE
#endif

class CParticleEmitter;
class CParticleContainer;
class CParticleBatchDataManager;

//PERFHUD
class CParticleWidget : public ICryPerfHUDWidget
{
public:
	CParticleWidget(minigui::IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud, CParticleManager* m_pPartManager);
	~CParticleWidget();

	virtual void Reset() {}
	virtual void Update();
	virtual bool ShouldUpdate();
	virtual void LoadBudgets(XmlNodeRef perfXML) {}
	virtual void SaveStats(XmlNodeRef statsXML)  {}
	virtual void Enable(int mode);
	virtual void Disable()                       { m_pTable->Hide(true); }

protected:

	minigui::IMiniTable*         m_pTable;
	CParticleManager*            m_pPartMgr;

	EPerfHUD_ParticleDisplayMode m_displayMode;
};

// data to store vertices/indices pool usage
struct SVertexIndexPoolUsage
{
	uint32      nVertexMemory;
	uint32      nIndexMemory;
	const char* pContainerName;
};

//////////////////////////////////////////////////////////////////////////
// Helper class for particle job management
class CParticleBatchDataManager : public Cry3DEngineBase, public IParticleManager
{
public:

	CParticleBatchDataManager()
		: m_nUsedStates(0)
	{
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			for (int j = 0; j < MAX_RECURSION_LEVELS; ++j)
			{
				m_ParticlesJobStart[i][j] = 0;
			}
		}
	}

	// Free all memory used by manager
	void                     ResetData();

	JobManager::SJobState*   AddUpdateJob(CParticleEmitter* pEmitter);

	SAddParticlesToSceneJob& GetParticlesToSceneJob(const SRenderingPassInfo& passInfo)
	{ return *m_ParticlesToScene[passInfo.ThreadID()].push_back(); }

	void SyncAllUpdateParticlesJobs();

	void GetMemoryUsage(ICrySizer* pSizer) const;

	// IParticleManager partial implementation
	virtual void PrepareForRender(const SRenderingPassInfo& passInfo);
	virtual void FinishParticleRenderTasks(const SRenderingPassInfo& passInfo);

private:

	JobManager::SJobState* GetJobState()
	{
		assert(m_nUsedStates <= m_UpdateParticleStates.size());
		if (m_nUsedStates == m_UpdateParticleStates.size())
			m_UpdateParticleStates.push_back(new JobManager::SJobState);

		JobManager::SJobState* pJobState = m_UpdateParticleStates[m_nUsedStates++];
		assert(pJobState);

		return pJobState;
	}

	DynArray<SAddParticlesToSceneJob> m_ParticlesToScene[RT_COMMAND_BUF_COUNT];
	int                               m_ParticlesJobStart[RT_COMMAND_BUF_COUNT][MAX_RECURSION_LEVELS];
	DynArray<JobManager::SJobState*>  m_UpdateParticleStates;
	int                               m_nUsedStates;
};

//////////////////////////////////////////////////////////////////////////
// Top class of particle system
class CParticleManager : public IVisAreaCallback, public CParticleBatchDataManager
{
public:
	CParticleManager(bool bEnable);
	~CParticleManager();

	friend class CParticleWidget;

	// Provide access to extended CPartManager functions only to particle classes.
	ILINE static CParticleManager* Instance()
	{
		return static_cast<CParticleManager*>(m_pPartManager);
	}

	//////////////////////////////////////////////////////////////////////////
	// IParticleManager implementation

	void                   SetDefaultEffect(const IParticleEffect* pEffect);
	const IParticleEffect* GetDefaultEffect()
	{ return m_pDefaultEffect; }
	const ParticleParams&  GetDefaultParams(int nVersion = 0) const
	{ return GetDefaultParams(ParticleParams::EInheritance::System, nVersion); }

	IParticleEffect*           CreateEffect();
	void                       DeleteEffect(IParticleEffect* pEffect);
	IParticleEffect*           FindEffect(cstr sEffectName, cstr sSource = "", bool bLoad = true);
	IParticleEffect*           LoadEffect(cstr sEffectName, XmlNodeRef& effectNode, bool bLoadResources, const cstr sSource = NULL);
	bool                       LoadLibrary(cstr sParticlesLibrary, XmlNodeRef& libNode, bool bLoadResources);
	bool                       LoadLibrary(cstr sParticlesLibrary, cstr sParticlesLibraryFile = NULL, bool bLoadResources = false);
	void                       ClearCachedLibraries();

	IParticleEmitter*          CreateEmitter(const ParticleLoc& loc, const ParticleParams& Params, const SpawnParams* pSpawnParams = NULL);
	void                       DeleteEmitter(IParticleEmitter* pEmitter);
	void                       DeleteEmitters(FEmitterFilter filter);
	IParticleEmitter*          SerializeEmitter(TSerialize ser, IParticleEmitter* pEmitter = NULL);
	void                       FinishParticleRenderTasks(const SRenderingPassInfo& passInfo);

	// Processing
	void Update();
	void RenderDebugInfo();

	void OnFrameStart();
	void Reset();
	void ClearRenderResources(bool bForceClear);
	void ClearDeferredReleaseResources();
	void Serialize(TSerialize ser);
	void PostSerialize(bool bReading);

	void SetTimer(ITimer* pTimer) { g_pParticleTimer = pTimer; };

	// Stats
	void GetMemoryUsage(ICrySizer* pSizer) const;
	void GetCounts(SParticleCounts& counts);
	void DisplayStats(Vec2& location, float lineHeight);

	void ListEmitters(cstr sDesc = "", bool bForce = false);
	void ListEffects();
	void PrintParticleMemory();

	typedef VectorMap<const IParticleEffect*, SParticleCounts> TEffectStats;
	void CollectEffectStats(TEffectStats& mapEffectStats, size_t iSortField) const;

	//PerfHUD
	virtual void CreatePerfHUDWidget();

	// Summary:
	//	 Registers new particle events listener.
	virtual void AddEventListener(IParticleEffectListener* pListener);
	virtual void RemoveEventListener(IParticleEffectListener* pListener);

	//////////////////////////////////////////////////////////////////////////
	// IVisAreaCallback implementation
	void OnVisAreaDeleted(IVisArea* pVisArea);

	//////////////////////////////////////////////////////////////////////////
	// Particle effects.
	//////////////////////////////////////////////////////////////////////////
	void       RenameEffect(CParticleEffect* pEffect, cstr sNewName);
	XmlNodeRef ReadLibrary(cstr sParticlesLibrary);
	XmlNodeRef ReadEffectNode(cstr sEffectName);

	void       SetDefaultEffect(cstr sEffectName)
	{
		SetDefaultEffect(FindEffect(sEffectName));
	}
	const ParticleParams& GetDefaultParams(ParticleParams::EInheritance eInheritance, int nVersion = 0) const;
	void                  ReloadAllEffects();

	//////////////////////////////////////////////////////////////////////////
	// Emitters.
	//////////////////////////////////////////////////////////////////////////
	CParticleEmitter*   CreateEmitter(const ParticleLoc& loc, const IParticleEffect* pEffect, const SpawnParams* pSpawnParams = NULL);
	void                UpdateEmitters(IParticleEffect* pEffect);

	SWorldPhysEnviron const& GetPhysEnviron() { return m_PhysEnv; }

#ifdef bEVENT_TIMINGS
	struct SEventTiming
	{
		const CParticleEffect* pEffect;
		uint32                 nContainerId;
		threadID               nThread;
		cstr                   sEvent;
		float                  timeStart, timeEnd;
	};
	int        AddEventTiming(cstr sEvent, const CParticleContainer* pCont);

	inline int StartEventTiming(cstr sEvent, const CParticleContainer* pCont)
	{
		if (pCont && (GetCVars()->e_ParticlesDebug & AlphaBits('ed')))
		{
			WriteLock lock(m_EventLock);
			return AddEventTiming(sEvent, pCont) * m_iEventSwitch;
		}
		else
			return 0;
	}
	inline void EndEventTiming(int iEvent)
	{
		WriteLock lock(m_EventLock);
		iEvent *= m_iEventSwitch;
		if (iEvent > 0)
			m_aEvents[iEvent].timeEnd = GetParticleTimer()->GetAsyncCurTime();
	}
#endif

	ILINE uint32 GetAllowedEnvironmentFlags() const
	{
		return m_nAllowedEnvironmentFlags;
	}
	ILINE TrinaryFlags<uint64> const& GetRenderFlags() const
	{
		return m_RenderFlags;
	}
	static float GetMaxAngularDensity(const CCamera& camera)
	{
		return camera.GetAngularResolution() / max(GetCVars()->e_ParticlesMinDrawPixels, 0.125f) * 2.0f;
	}
	bool CanAccessFiles(cstr sObject, cstr sSource = "") const;

	// light profiler functions
	ILINE virtual SParticleLightProfileCounts& GetLightProfileCounts() { return m_lightProfile; }

	// functions to collect which particle container need the most memory
	// to store vertices/indices
	void         DumpAndResetVertexIndexPoolUsage();
	virtual void AddVertexIndexPoolUsageEntry(uint32 nVertexMemory, uint32 nIndexMemory, const char* pContainerName);
	virtual void MarkAsOutOfMemory();

private:

	struct CCompCStr
	{
		bool operator()(cstr a, cstr b) const
		{
			return stricmp(a, b) < 0;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Map of particle effect case-insensitive name to interface pointer.
	// The name key points to the name string in the actual effect,
	// so there is no string duplication.
	//typedef std::map< cstr, _smart_ptr<CParticleEffect>, CCompCStr > TEffectsList;
	// special key class, use md5 instead of a strcmp as an map key
	class SEffectsKey
	{
	public:
		SEffectsKey() { u64[0] = 0; u64[1] = 0; }
		SEffectsKey(const cstr& sName);

		bool operator<(const SEffectsKey& rOther) const
		{
			return u64[0] == rOther.u64[0] ? u64[1] < rOther.u64[1] : u64[0] < rOther.u64[0];
		}

		void GetMemoryUsage(ICrySizer* pSizer) const {}
	private:
		union
		{
			char   c16[16];
			uint64 u64[2];
		};
	};

	typedef VectorMap<SEffectsKey, _smart_ptr<CParticleEffect>> TEffectsList;
	TEffectsList                m_Effects;

	_smart_ptr<CParticleEffect> m_pDefaultEffect;
	ParticleParams const*       m_pLastDefaultParams;

	typedef std::list<IParticleEffectListener*> TListenersList;
	TListenersList m_ListenersList;

	//////////////////////////////////////////////////////////////////////////
	// Loaded particle libs.
	std::map<string, XmlNodeRef, stl::less_stricmp<string>> m_LoadedLibs;

	//////////////////////////////////////////////////////////////////////////
	// Particle effects emitters, top-level only.
	//////////////////////////////////////////////////////////////////////////
	ParticleList<CParticleEmitter>
	                      m_Emitters;

	bool                  m_bEnabled;
	bool                  m_bRegisteredListener;
	_smart_ptr<IMaterial> m_pPartLightShader;

	// Force features on/off depending on engine config.
	uint32               m_nAllowedEnvironmentFlags;        // Which particle features are allowed.
	TrinaryFlags<uint64> m_RenderFlags;                     // OS_ and FOB_ flags.

	bool                 m_bParticleTessellation = false;   // tessellation feature is allowed to use.

	SWorldPhysEnviron    m_PhysEnv;                         // Per-frame computed physics area information.
	std::shared_ptr<pfx2::IParticleSystem>
		                 m_pParticleSystem;

	bool IsRuntime() const
	{
		ESystemGlobalState eState = gEnv->pSystem->GetSystemGlobalState();
		return !gEnv->IsEditing() && (eState > ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END);
	}

	void UpdateEngineData();

	CParticleEffect* FindLoadedEffect(cstr sEffectName);
	void             EraseEmitter(CParticleEmitter* pEmitter);

	// Console command interface.
	static void CmdParticleListEffects(IConsoleCmdArgs*)
	{
		Instance()->ListEffects();
	}

	static void CmdParticleListEmitters(IConsoleCmdArgs*)
	{
		Instance()->ListEmitters("", true);
	}
	static void CmdParticleMemory(IConsoleCmdArgs*)
	{
		Instance()->PrintParticleMemory();
	}

	bool LoadPreloadLibList(const cstr filename, const bool bLoadResources);

#ifdef bEVENT_TIMINGS
	DynArray<SEventTiming> m_aEvents;
	int                    m_iEventSwitch;
	CSpinLock              m_EventLock;
	float                  m_timeThreshold;

	void LogEvents();
#endif

	SParticleCounts  m_GlobalCounts;

	CParticleWidget* m_pWidget;

	// per frame lightwight timing informations
	SParticleLightProfileCounts m_lightProfile;

#if defined(PARTICLE_COLLECT_VERT_IND_POOL_USAGE)
	enum { nVertexIndexPoolUsageEntries = 5};
	SVertexIndexPoolUsage m_arrVertexIndexPoolUsage[nVertexIndexPoolUsageEntries];
	uint32                m_nRequieredVertexPoolMemory;
	uint32                m_nRequieredIndexPoolMemory;
	uint32                m_nRendererParticleContainer;
	bool                  m_bOutOfVertexIndexPoolMemory;
#endif
};

#ifdef bEVENT_TIMINGS

class CEventProfilerSection : public CFrameProfilerSection, public Cry3DEngineBase
{
public:
	CEventProfilerSection(CFrameProfiler* pProfiler, const CParticleContainer* pCont = 0)
		: CFrameProfilerSection(pProfiler)
	{
		m_iEvent = m_pPartManager->StartEventTiming(pProfiler->m_name, pCont);
	}
	~CEventProfilerSection()
	{
		m_pPartManager->EndEventTiming(m_iEvent);
	}
protected:
	int m_iEvent;
};

	#define FUNCTION_PROFILER_CONTAINER(pCont)                                                               \
	  static CFrameProfiler staticFrameProfiler(PROFILE_PARTICLE, EProfileDescription::UNDEFINED, __FUNC__); \
	  CEventProfilerSection eventProfilerSection(&staticFrameProfiler, pCont);

#else

	#define FUNCTION_PROFILER_CONTAINER(pCont) CRY_PROFILE_FUNCTION(PROFILE_PARTICLE)

#endif
