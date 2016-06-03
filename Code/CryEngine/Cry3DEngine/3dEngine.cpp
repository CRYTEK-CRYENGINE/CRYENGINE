// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   3dengine.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Implementation of I3DEngine interface methods
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/IGameFramework.h>
#include <CryAudio/IAudioSystem.h>
#include <CryParticleSystem/IParticlesPfx2.h>

#include "3dEngine.h"
#include "terrain.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "terrain_water.h"

#include "DecalManager.h"
#include "Vegetation.h"
#include "IndexedMesh.h"

#include "MatMan.h"

#include "Brush.h"
#include "PolygonClipContext.h"
#include "CGF/CGFLoader.h"
#include "CGF/ChunkFileWriters.h"
#include "CGF/ReadOnlyChunkFile.h"

#include "CloudRenderNode.h"
#include "CloudsManager.h"
#include "SkyLightManager.h"
#include "FogVolumeRenderNode.h"
#include "RoadRenderNode.h"
#include "DecalRenderNode.h"
#include "TimeOfDay.h"
#include "LightEntity.h"
#include "FogVolumeRenderNode.h"
#include "ObjectsTree.h"
#include "WaterVolumeRenderNode.h"
#include "WaterWaveRenderNode.h"
#include "DistanceCloudRenderNode.h"
#include "VolumeObjectRenderNode.h"
#include "WaterWaveRenderNode.h"
#include "RopeRenderNode.h"
#include "RenderMeshMerger.h"
#include "PhysCallbacks.h"
#include "DeferredCollisionEvent.h"
#include "MergedMeshRenderNode.h"
#include "BreakableGlassRenderNode.h"
#include "BreezeGenerator.h"
#include "OpticsManager.h"
#include "GeomCacheRenderNode.h"
#include "GeomCacheManager.h"
#include "ClipVolumeManager.h"
#include <CryNetwork/IRemoteCommand.h>

#if defined(FEATURE_SVO_GI)
	#include "SVO/SceneTreeManager.h"
#endif

#if !defined(EXCLUDE_DOCUMENTATION_PURPOSE)
	#include "PrismRenderNode.h"
#endif // EXCLUDE_DOCUMENTATION_PURPOSE

#include <CryThreading/IJobManager_JobDelegator.h>

threadID Cry3DEngineBase::m_nMainThreadId = 0;
bool Cry3DEngineBase::m_bRenderTypeEnabled[eERType_TypesNum];
ISystem* Cry3DEngineBase::m_pSystem = 0;
#if !defined(DEDICATED_SERVER)
IRenderer* Cry3DEngineBase::m_pRenderer = 0;
#else
IRenderer* const Cry3DEngineBase::m_pRenderer = 0;
#endif
ITimer* Cry3DEngineBase::m_pTimer = 0;
ILog* Cry3DEngineBase::m_pLog = 0;
IPhysicalWorld* Cry3DEngineBase::m_pPhysicalWorld = 0;
CTerrain* Cry3DEngineBase::m_pTerrain = 0;
CObjManager* Cry3DEngineBase::m_pObjManager = 0;
IConsole* Cry3DEngineBase::m_pConsole = 0;
C3DEngine* Cry3DEngineBase::m_p3DEngine = 0;
CVars* Cry3DEngineBase::m_pCVars = 0;
ICryPak* Cry3DEngineBase::m_pCryPak = 0;
IParticleManager* Cry3DEngineBase::m_pPartManager = 0;
std::shared_ptr<pfx2::IParticleSystem> Cry3DEngineBase::m_pParticleSystem;
extern volatile bool gParticleSystem; // HACK : check ParticleSystem.cpp for the reasoning behind this
IOpticsManager* Cry3DEngineBase::m_pOpticsManager = 0;
CDecalManager* Cry3DEngineBase::m_pDecalManager = 0;
CSkyLightManager* Cry3DEngineBase::m_pSkyLightManager = 0;
CCloudsManager* Cry3DEngineBase::m_pCloudsManager = 0;
CVisAreaManager* Cry3DEngineBase::m_pVisAreaManager = 0;
CClipVolumeManager* Cry3DEngineBase::m_pClipVolumeManager = 0;
CWaterWaveManager* Cry3DEngineBase::m_pWaterWaveManager = 0;
CRenderMeshMerger* Cry3DEngineBase::m_pRenderMeshMerger = 0;
CMatMan* Cry3DEngineBase::m_pMatMan = 0;
CMergedMeshesManager* Cry3DEngineBase::m_pMergedMeshesManager = 0;
CBreezeGenerator* Cry3DEngineBase::m_pBreezeGenerator = 0;
IStreamedObjectListener* Cry3DEngineBase::m_pStreamListener = 0;
#if defined(USE_GEOM_CACHES)
CGeomCacheManager* Cry3DEngineBase::m_pGeomCacheManager = 0;
#endif

float Cry3DEngineBase::m_fInvDissolveDistBand = 1.f;
bool Cry3DEngineBase::m_bProfilerEnabled = false;
bool Cry3DEngineBase::m_bLevelLoadingInProgress = false;
bool Cry3DEngineBase::m_bIsInRenderScene = false;
int Cry3DEngineBase::m_mergedMeshesPoolSize = 0;

int Cry3DEngineBase::m_CpuFlags = 0;
#if CRY_PLATFORM_DESKTOP
bool Cry3DEngineBase::m_bEditor = false;
#endif
ESystemConfigSpec Cry3DEngineBase::m_LightConfigSpec = CONFIG_VERYHIGH_SPEC;
int Cry3DEngineBase::m_arrInstancesCounter[eERType_TypesNum];
IGetLayerIdAtCallback* C3DEngine::m_pGetLayerIdAtCallback = 0;

#define LAST_POTENTIALLY_VISIBLE_TIME 2

// The only direct particle function needed by 3DEngine, implemented in the same DLL.
extern IParticleManager* CreateParticleManager(bool bEnable);
extern void              DestroyParticleManager(IParticleManager* pParticleManager);

namespace
{
class CLoadLogListener : public ILoaderCGFListener
{
public:
	virtual ~CLoadLogListener(){}
	virtual void Warning(const char* format) { Cry3DEngineBase::Warning("%s", format); }
	virtual void Error(const char* format)   { Cry3DEngineBase::Error("%s", format); }
	virtual bool IsValidationEnabled()       { return Cry3DEngineBase::GetCVars()->e_StatObjValidate != 0; }
};
}

///////////////////////////////////////////////////////////////////////////////
CryCriticalSectionNonRecursive g_renderNodeTempDataLock;

//////////////////////////////////////////////////////////////////////
C3DEngine::C3DEngine(ISystem* pSystem)
{
	//#if defined(_DEBUG) && CRY_PLATFORM_WINDOWS
	//	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//#endif

	// Level info
	m_fSunSpecMult = 1.f;
	m_bAreaActivationInUse = false;

	CVegetation::InitVegDecomprTable();

	Cry3DEngineBase::m_nMainThreadId = CryGetCurrentThreadId();
	Cry3DEngineBase::m_pSystem = pSystem;
#if !defined(DEDICATED_SERVER)
	Cry3DEngineBase::m_pRenderer = gEnv->pRenderer;
#endif
	Cry3DEngineBase::m_pTimer = gEnv->pTimer;
	Cry3DEngineBase::m_pLog = gEnv->pLog;
	Cry3DEngineBase::m_pPhysicalWorld = gEnv->pPhysicalWorld;
	Cry3DEngineBase::m_pConsole = gEnv->pConsole;
	Cry3DEngineBase::m_p3DEngine = this;
	Cry3DEngineBase::m_pCryPak = gEnv->pCryPak;
	Cry3DEngineBase::m_pCVars = 0;
	Cry3DEngineBase::m_pRenderMeshMerger = new CRenderMeshMerger;
	Cry3DEngineBase::m_pMergedMeshesManager = new CMergedMeshesManager;
	Cry3DEngineBase::m_pMatMan = new CMatMan;
	Cry3DEngineBase::m_pBreezeGenerator = new CBreezeGenerator;
	Cry3DEngineBase::m_pStreamListener = NULL;
	Cry3DEngineBase::m_CpuFlags = pSystem->GetCPUFlags();

	memset(Cry3DEngineBase::m_arrInstancesCounter, 0, sizeof(Cry3DEngineBase::m_arrInstancesCounter));

#if CRY_PLATFORM_DESKTOP
	m_bEditor = gEnv->IsEditor();
#endif

	m_pCVars = new CVars();

	m_pTimeOfDay = NULL;

	m_szLevelFolder[0] = 0;

	m_pSun = 0;
	m_nFlags = 0;
	m_pSkyMat = 0;
	m_pSkyLowSpecMat = 0;
	m_pTerrainWaterMat = 0;
	m_nWaterBottomTexId = 0;
	m_vSunDir = Vec3(5.f, 5.f, DISTANCE_TO_THE_SUN);
	m_vSunDirRealtime = Vec3(5.f, 5.f, DISTANCE_TO_THE_SUN).GetNormalized();

	m_pTerrain = 0;

	m_nBlackTexID = 0;

	m_nCurWind = 0;
	m_bWindJobRun = false;
	m_pWindField = NULL;

	// create components
	m_pObjManager = CryAlignedNew<CObjManager>();

	m_pDecalManager = 0;    //new CDecalManager   (m_pSystem, this);
	m_pCloudsManager = new CCloudsManager;
	m_pPartManager = 0;
	gParticleSystem = true;
	m_pOpticsManager = 0;
	m_pVisAreaManager = 0;
	m_pClipVolumeManager = new CClipVolumeManager();
	m_pSkyLightManager = CryAlignedNew<CSkyLightManager>();
	m_pWaterWaveManager = 0;

	// create REs
	m_pRESky = 0;
	m_pREHDRSky = 0;

	m_pFarTreeSprites = 0;

	m_pPhysMaterialEnumerator = 0;

	m_fMaxViewDistHighSpec = 8000;
	m_fMaxViewDistLowSpec = 1000;
	m_fTerrainDetailMaterialsViewDistRatio = 1.f;

	m_fSkyBoxAngle = 0;
	m_fSkyBoxStretching = 0;

	m_pGlobalWind = 0;
	m_vWindSpeed(1, 0, 0);

	m_bOcean = true;
	m_nOceanRenderFlags = 0;

	m_bSunShadows = m_bShowTerrainSurface = true;
	m_bSunShadowsFromTerrain = false;
	m_nSunAdditionalCascades = 0;
	m_CachedShadowsBounds.Reset();
	m_nCachedShadowsUpdateStrategy = ShadowMapFrustum::ShadowCacheData::eFullUpdate;

	m_fSunClipPlaneRange = 256.0f;
	m_fSunClipPlaneRangeShift = 0.0f;

	m_nRealLightsNum = m_nDeferredLightsNum = 0;

	union
	{
		CStatObjFoliage* CStatObjFoliage::* next;
		INT_PTR                             inext;
	} tmp;
	tmp.inext = 0;
	tmp.next = &CStatObjFoliage::m_next;
	m_pFirstFoliage = m_pLastFoliage = (CStatObjFoliage*)((INT_PTR)&m_pFirstFoliage - tmp.inext);

	m_fLightsHDRDynamicPowerFactor = 0.0f;

	m_vHDRFilmCurveParams = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	m_vHDREyeAdaptation = Vec3(0.05f, 0.8f, 0.9f);
	m_fHDRBloomAmount = 0.0f;
	m_vColorBalance = Vec3(1.0f, 1.0f, 1.0f);
	m_fHDRSaturation = 1.0f;

	m_vSkyHightlightPos.Set(0, 0, 0);
	m_vSkyHightlightCol.Set(0, 0, 0);
	m_fSkyHighlightSize = 0;

	m_volFogGlobalDensity = 0.02f;
	m_volFogGlobalDensityMultiplierLDR = 1.0f;
	m_volFogFinalDensityClamp = 1.0f;

	m_idMatLeaves = -1;

	/* // structures should not grow too much - commented out as in 64bit they do
	   assert(sizeof(CVegetation)-sizeof(IRenderNode)<=52);
	   assert(sizeof(CBrush)-sizeof(IRenderNode)<=120);
	   assert(sizeof(IRenderNode)<=96);
	 */
	m_oceanFogColor = 0.2f * Vec3(29.0f, 102.0f, 141.0f) / 255.0f;
	m_oceanFogColorShallow = Vec3(0, 0, 0); //0.5f * Vec3( 206.0f, 249.0f, 253.0f ) / 255.0f;
	m_oceanFogDensity = 0;                  //0.2f;

	m_oceanCausticsDistanceAtten = 100.0f;
	m_oceanCausticsMultiplier = 1.0f;
	m_oceanCausticsDarkeningMultiplier = 1.0f;

	m_oceanCausticHeight = 2.5f;
	m_oceanCausticDepth = 8.0f;
	m_oceanCausticIntensity = 1.0f;

	m_oceanWindDirection = 1;
	m_oceanWindSpeed = 4.0f;
	m_oceanWavesSpeed = 5.0f;
	m_oceanWavesAmount = 1.5f;
	m_oceanWavesSize = 0.75f;

	m_fRefreshSceneDataCVarsSumm = -1;
	m_nRenderTypeEnableCVarSum = -1;

	/*
	   CTerrainNode::SetProcObjChunkPool(new SProcObjChunkPool);
	   CTerrainNode::SetProcObjPoolMan(new CProcVegetPoolMan);
	 */
	m_bResetRNTmpDataPool = false;

	m_fSunDirUpdateTime = 0;
	m_vSunDirNormalized.zero();

	m_volFogRamp = Vec3(0, 100.0f, 0);
	m_volFogShadowRange = Vec3(0.1f, 0, 0);
	m_volFogShadowDarkening = Vec3(0.25f, 1, 1);
	m_volFogShadowEnable = Vec3(0, 0, 0);
	m_volFog2CtrlParams = Vec3(64.0f, 0.0f, 1.0f);
	m_volFog2ScatteringParams = Vec3(1.0f, 0.3f, 0.6f);
	m_volFog2Ramp = Vec3(0.0f, 0.0f, 0.0f);
	m_volFog2Color = Vec3(1.0f, 1.0f, 1.0f);
	m_volFog2GlobalDensity = Vec3(0.1f, 1.0f, 0.4f);
	m_volFog2HeightDensity = Vec3(0.0f, 1.0f, 0.1f);
	m_volFog2HeightDensity2 = Vec3(4000.0f, 0.0001f, 0.95f);
	m_volFog2Color1 = Vec3(1.0f, 1.0f, 1.0f);
	m_volFog2Color2 = Vec3(1.0f, 1.0f, 1.0f);
	m_nightSkyHorizonCol = Vec3(0, 0, 0);
	m_nightSkyZenithCol = Vec3(0, 0, 0);
	m_nightSkyZenithColShift = 0;
	m_nightSkyStarIntensity = 0;
	m_moonDirection = Vec3(0, 0, 0);
	m_nightMoonCol = Vec3(0, 0, 0);
	m_nightMoonSize = 0;
	m_nightMoonInnerCoronaCol = Vec3(0, 0, 0);
	m_nightMoonInnerCoronaScale = 1.0f;
	m_nightMoonOuterCoronaCol = Vec3(0, 0, 0);
	m_nightMoonOuterCoronaScale = 1.0f;
	m_moonRotationLatitude = 0;
	m_moonRotationLongitude = 0;
	m_skyboxMultiplier = 1.0f;
	m_dayNightIndicator = 1.0f;
	m_fogColor2 = Vec3(0, 0, 0);
	m_fogColorRadial = Vec3(0, 0, 0);
	m_volFogHeightDensity = Vec3(0, 1, 0);
	m_volFogHeightDensity2 = Vec3(4000.0f, 0, 0);
	m_volFogGradientCtrl = Vec3(1, 1, 1);

	m_fogColorSkylightRayleighInScatter = Vec3(0, 0, 0);

	m_vFogColor = Vec3(1.0f, 1.0f, 1.0f);
	m_vAmbGroundCol = Vec3(0.0f, 0.0f, 0.0f);

	m_dawnStart = 350.0f / 60.0f;
	m_dawnEnd = 360.0f / 60.0f;
	m_duskStart = 12.0f + 360.0f / 60.0f;
	m_duskEnd = 12.0f + 370.0f / 60.0f;

	m_fCloudShadingSunLightMultiplier = 0;
	m_fCloudShadingSkyLightMultiplier = 0;
	m_vCloudShadingCustomSunColor = Vec3(0, 0, 0);
	m_vCloudShadingCustomSkyColor = Vec3(0, 0, 0);

	m_vVolCloudAtmosphericScattering = Vec3(0, 0, 0);
	m_vVolCloudGenParams = Vec3(0, 0, 0);
	m_vVolCloudScatteringLow = Vec3(0, 0, 0);
	m_vVolCloudScatteringHigh = Vec3(0, 0, 0);
	m_vVolCloudGroundColor = Vec3(0, 0, 0);
	m_vVolCloudScatteringMulti = Vec3(0, 0, 0);
	m_vVolCloudWindAtmospheric = Vec3(0, 0, 0);
	m_vVolCloudTurbulence = Vec3(0, 0, 0);
	m_vVolCloudEnvParams = Vec3(0, 0, 0);
	m_vVolCloudGlobalNoiseScale = Vec3(0, 0, 0);
	m_vVolCloudRenderParams = Vec3(0, 0, 0);
	m_vVolCloudTurbulenceNoiseScale = Vec3(0, 0, 0);
	m_vVolCloudTurbulenceNoiseParams = Vec3(0, 0, 0);
	m_vVolCloudDensityParams = Vec3(0, 0, 0);
	m_vVolCloudMiscParams = Vec3(0, 0, 0);
	m_vVolCloudTilingSize = Vec3(0, 0, 0);
	m_vVolCloudTilingOffset = Vec3(0, 0, 0);

	m_vPrevMainFrameCamPos.Set(-1000000.f, -1000000.f, -1000000.f);
	m_fAverageCameraSpeed = 0;
	m_vAverageCameraMoveDir = Vec3(0);
	m_bContentPrecacheRequested = false;
	m_bTerrainTextureStreamingInProgress = false;
	m_bLayersActivated = false;
	m_eShadowMode = ESM_NORMAL;
	m_pSegmentsManager = 0;
	m_bSegmentOperationInProgress = false;

	ClearDebugFPSInfo();

	m_fMaxViewDistScale = 1.f;

	m_ptexIconLowMemoryUsage = NULL;
	m_ptexIconAverageMemoryUsage = NULL;
	m_ptexIconHighMemoryUsage = NULL;
	m_ptexIconEditorConnectedToConsole = NULL;
	m_pScreenshotCallback = 0;
	m_bInShutDown = false;
	m_bInUnload = false;
	m_bInLoad = false;

	m_nCloudShadowTexId = 0;

	m_nNightMoonTexId = 0;

	m_pDeferredPhysicsEventManager = new CDeferredPhysicsEventManager();

#if defined(USE_GEOM_CACHES)
	m_pGeomCacheManager = new CGeomCacheManager();
#endif

	m_LightVolumesMgr.Init();

	m_pBreakableBrushHeap = NULL;

	m_fZoomFactor = 0.0f;
	m_fPrevZoomFactor = 0.0f;
	m_bZoomInProgress = false;
	m_nZoomMode = 0;

	m_fAmbMaxHeight = 0.0f;
	m_fAmbMinHeight = 0.0f;

	m_pLightQuality = NULL;
	m_fSaturation = 0.0f;

	m_fGsmRange = 0.0f;
	m_fGsmRangeStep = 0.0f;
	m_fShadowsConstBias = 0.0f;
	m_fShadowsSlopeBias = 0.0f;
	m_nCustomShadowFrustumCount = 0;
	m_bHeightMapAoEnabled = false;

	m_nCurrentWindAreaList = 0;
	m_pLevelStatObjTable = NULL;
	m_pLevelMaterialsTable = NULL;
	m_bLevelFilesEndian = false;
}

//////////////////////////////////////////////////////////////////////
C3DEngine::~C3DEngine()
{
	m_bInShutDown = true;
	m_bInUnload = true;
	m_bInLoad = false;

	delete CTerrainNode::GetProcObjPoolMan();
	CTerrainNode::SetProcObjChunkPool(NULL);

	delete CTerrainNode::GetProcObjChunkPool();
	CTerrainNode::SetProcObjPoolMan(NULL);

	assert(IsHeapValid());

	ShutDown();

	delete m_pTimeOfDay;
	delete m_pDecalManager;
	delete m_pVisAreaManager;
	SAFE_DELETE(m_pClipVolumeManager);

	CryAlignedDelete(m_pSkyLightManager);
	m_pSkyLightManager = 0;
	for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); nSID++)
		SAFE_DELETE(m_pObjectsTree[nSID]);
	//  delete m_pSceneTree;
	delete m_pRenderMeshMerger;
	delete m_pMatMan;
	m_pMatMan = 0;
	delete m_pCloudsManager;

	delete m_pCVars;

	delete m_pDeferredPhysicsEventManager;
}

//////////////////////////////////////////////////////////////////////

void C3DEngine::RemoveEntInFoliage(int i, IPhysicalEntity* pent)
{
	IPhysicalEntity* pent1 = m_pPhysicalWorld->GetPhysicalEntityById(m_arrEntsInFoliage[i].id);
	pe_params_foreign_data pfd;
	if (!pent)
		pent = pent1;
	else if (pent != pent1)
	{
		// probably someone overwrote foreign flags
		for (i = m_arrEntsInFoliage.size() - 1; i >= 0 && pent != m_pPhysicalWorld->GetPhysicalEntityById(m_arrEntsInFoliage[i].id); i--)
			;
		if (i < 0)
			return;
	}
	if (pent && pent->GetParams(&pfd) && (pfd.iForeignFlags >> 8 & 255) == i + 1)
	{
		MARK_UNUSED pfd.pForeignData, pfd.iForeignData, pfd.iForeignFlags;
		pfd.iForeignFlagsAND = 255;
		pent->SetParams(&pfd, 1);
	}
	int j = m_arrEntsInFoliage.size() - 1;
	if (i < j)
	{
		m_arrEntsInFoliage[i] = m_arrEntsInFoliage[j];
		if ((pent = m_pPhysicalWorld->GetPhysicalEntityById(m_arrEntsInFoliage[i].id)) && pent->GetParams(&pfd) &&
		    (pfd.iForeignFlags >> 8 & 255) == j + 1)
		{
			MARK_UNUSED pfd.pForeignData, pfd.iForeignData;
			pfd.iForeignFlags = pfd.iForeignFlags & 255 | (i + 1) << 8;
			pent->SetParams(&pfd, 1);
		}
	}
	m_arrEntsInFoliage.DeleteLast();
}

bool C3DEngine::Init()
{
	m_pPartManager = CreateParticleManager(!gEnv->IsDedicated());
	m_pParticleSystem = pfx2::GetIParticleSystem();
	m_pSystem->SetIParticleManager(m_pPartManager);

	m_pOpticsManager = new COpticsManager;
	m_pSystem->SetIOpticsManager(m_pOpticsManager);

	for (int i = 0; i < eERType_TypesNum; i++)
	{
		m_bRenderTypeEnabled[i] = true;
	}

	UpdateRenderTypeEnableLookup();

	// Allocate the temporary pool used for allocations during streaming and loading
	const size_t tempPoolSize = static_cast<size_t>(GetCVars()->e_3dEngineTempPoolSize) << 10;
	CRY_ASSERT_MESSAGE(tempPoolSize != 0, "C3DEngine::Init() temp pool size is set to 0");

	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "3Engine Temp Pool");

		if (!CTemporaryPool::Initialize(tempPoolSize))
		{
			CryFatalError("C3DEngine::Init() could not initialize temporary pool");
			return false;
		}
	}

	SFrameLodInfo frameLodInfo;
	frameLodInfo.fLodRatio = GetCVars()->e_LodRatio;

	frameLodInfo.fTargetSize = GetCVars()->e_LodFaceAreaTargetSize;
	CRY_ASSERT(frameLodInfo.fTargetSize > 0.f);
	if (frameLodInfo.fTargetSize <= 0.f)
	{
		frameLodInfo.fTargetSize = 1.f;
	}

	frameLodInfo.nMinLod = GetCVars()->e_LodMin;
	frameLodInfo.nMaxLod = GetCVars()->e_LodMax;
	if (GetCVars()->e_Lods == 0)
	{
		frameLodInfo.nMinLod = 0;
		frameLodInfo.nMaxLod = 0;
	}
	SetFrameLodInfo(frameLodInfo);

	return  (true);
}

bool C3DEngine::IsCameraAnd3DEngineInvalid(const SRenderingPassInfo& passInfo, const char* szCaller)
{
	if (!m_pObjManager)
	{
		return (true);
	}

	const CCamera& rCamera = passInfo.GetCamera();
	const float MAX_M23_REPORTED = 3000000.f; // MAT => upped from 100,000 which spammed this message on spear and cityhall. Really should stop editor generating
	// water levels that trigger this message.

	if (!_finite(rCamera.GetMatrix().m03) || !_finite(rCamera.GetMatrix().m13) || !_finite(rCamera.GetMatrix().m23) || GetMaxViewDistance() <= 0 ||
	    rCamera.GetMatrix().m23 < -MAX_M23_REPORTED || rCamera.GetMatrix().m23 > MAX_M23_REPORTED || rCamera.GetFov() < 0.0001f || rCamera.GetFov() > gf_PI)
	{
		Error("Bad camera passed to 3DEngine from %s: Pos=(%.1f, %.1f, %.1f), Fov=%.1f, MaxViewDist=%.1f. Maybe the water level is too extreme.",
		      szCaller,
		      rCamera.GetMatrix().m03, rCamera.GetMatrix().m13, rCamera.GetMatrix().m23,
		      rCamera.GetFov(), _finite(rCamera.GetMatrix().m03) ? GetMaxViewDistance() : 0);
		return true;
	}

	return false;

}

void C3DEngine::OnFrameStart()
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_pPartManager)
		m_pPartManager->OnFrameStart();
	if (m_pParticleSystem)
		m_pParticleSystem->OnFrameStart();
	m_nRenderWorldUSecs = 0;
	m_pDeferredPhysicsEventManager->Update();

#if defined(USE_GEOM_CACHES)
	if (m_pGeomCacheManager)
	{
		m_pGeomCacheManager->StreamingUpdate();
	}
#endif

	UpdateWindAreas();
}

float g_oceanLevel, g_oceanStep;
float GetOceanLevelCallback(int ix, int iy)
{
	return gEnv->p3DEngine->GetAccurateOceanHeight(Vec3(ix * g_oceanStep, iy * g_oceanStep, g_oceanLevel));
	/*pe_status_waterman psw;
	   gEnv->pPhysicalWorld->GetWatermanStatus(&psw);
	   Vec2 pos = Vec2((float)ix,(float)iy)*GetFloatCVar(e_PhysOceanCell)-Vec2(psw.origin);
	   Vec2i itile(FtoI(pos.x*0.25f-0.5f), FtoI(pos.y*0.25f-0.5f));
	   if ((itile.x|itile.y)<0 || max(itile.x,itile.y)>6)
	   return g_oceanLevel;
	   SWaterTileBase *pTile = psw.pTiles[itile.x+itile.y*7];
	   if (!pTile || !pTile->bActive)
	   return g_oceanLevel;
	   Vec2i ipos(FtoI((pos.x-itile.x*4)*10.0f-0.5f), FtoI((pos.y-itile.y*4)*10.0f-0.5f));
	   return psw.origin.z+pTile->ph[itile.x+itile.y*40];*/
}
unsigned char GetOceanSurfTypeCallback(int ix, int iy)
{
	return 0;
}

void C3DEngine::Update()
{
	m_bProfilerEnabled = gEnv->pFrameProfileSystem->IsProfiling();

	FUNCTION_PROFILER_3DENGINE;

	m_LightConfigSpec = (ESystemConfigSpec)GetCurrentLightSpec();

	///	if(m_pVisAreaManager)
	//	m_pVisAreaManager->Preceche(m_pObjManager);

	if (GetObjManager())
		GetObjManager()->ClearStatObjGarbage();

	if (m_pTerrain)
	{
		m_pTerrain->Recompile_Modified_Incrementaly_RoadRenderNodes();
	}

	if (m_bEditor)
		CRoadRenderNode::FreeStaticMemoryUsage();

	if (m_pDecalManager)
		m_pDecalManager->Update(GetTimer()->GetFrameTime());

	if (GetCVars()->e_PrecacheLevel == 3)
		PrecacheLevel(true, 0, 0);

	DebugDraw_Draw();

	ProcessCVarsChange();

	pe_params_area pa;
	IPhysicalEntity* pArea;
	if ((pArea = gEnv->pPhysicalWorld->AddGlobalArea())->GetParams(&pa))
	{
		g_oceanLevel = GetWaterLevel();
		bool bOceanEnabled = GetFloatCVar(e_PhysOceanCell) > 0 && g_oceanLevel > 0;

		if (bOceanEnabled && (!pa.pGeom || g_oceanStep != GetFloatCVar(e_PhysOceanCell)))
		{
			new(&pa)pe_params_area;
			primitives::heightfield hf;
			hf.origin.zero().z = g_oceanLevel;
			g_oceanStep = GetFloatCVar(e_PhysOceanCell);
			hf.size.set((int)(8192 / g_oceanStep), (int)(8192 / g_oceanStep));
			hf.step.set(g_oceanStep, g_oceanStep);
			hf.fpGetHeightCallback = GetOceanLevelCallback;
			hf.fpGetSurfTypeCallback = GetOceanSurfTypeCallback;
			hf.Basis.SetIdentity();
			hf.bOriented = 0;
			hf.typemask = 255;
			hf.typehole = 255;
			hf.heightscale = 1.0f;
			pa.pGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::heightfield::type, &hf);
			pArea->SetParams(&pa);

			Vec4 par0, par1;
			GetOceanAnimationParams(par0, par1);
			pe_params_buoyancy pb;
			pb.waterFlow = Vec3(par1.z, par1.y, 0) * (par0.z * 0.25f); // tone down the speed
			//pArea->SetParams(&pb);
		}
		else if (!bOceanEnabled && pa.pGeom)
		{
			new(&pa)pe_params_area;
			pa.pGeom = 0;
			pArea->SetParams(&pa);
			pe_params_buoyancy pb;
			pb.waterFlow.zero();
			pArea->SetParams(&pb);
		}
	}

	CRenderMeshUtils::ClearHitCache();

	CleanUpOldDecals();

	CDecalRenderNode::ResetDecalUpdatesCounter();

	if (m_pBreezeGenerator)
		m_pBreezeGenerator->Update();

	if (m_pBreakableBrushHeap)
		m_pBreakableBrushHeap->Cleanup();

	m_bTerrainTextureStreamingInProgress = false;
	if (CTerrain* const pTerrain = GetTerrain())
	{
		if (pTerrain->GetNotReadyTextureNodesCount() > 0)
		{
			m_bTerrainTextureStreamingInProgress = true;
		}
	}
}

void C3DEngine::Tick()
{
	threadID nThreadID = 0;

	if (GetRenderer())
	{
		GetRenderer()->EF_Query(EFQ_RenderThreadList, nThreadID);
	}

	GetMatMan()->DelayedMaterialDeletion();

	// clear stored cameras from last frame
	m_RenderingPassCameras[nThreadID].resize(0);
}

void C3DEngine::ProcessCVarsChange()
{
	static int nObjectLayersActivation = -1;

	if (nObjectLayersActivation != GetCVars()->e_ObjectLayersActivation)
	{
		if (GetCVars()->e_ObjectLayersActivation == 2)
			ActivateObjectsLayer(~0, true, true, true, true, "ALL_OBJECTS");
		if (GetCVars()->e_ObjectLayersActivation == 3)
			ActivateObjectsLayer(~0, false, true, true, true, "ALL_OBJECTS");

		nObjectLayersActivation = GetCVars()->e_ObjectLayersActivation;
	}

	float fNewCVarsSumm =
	  GetCVars()->e_ShadowsCastViewDistRatio +
	  GetCVars()->e_Dissolve +
	  GetFloatCVar(e_DissolveDistMin) +
	  GetFloatCVar(e_DissolveDistMax) +
	  GetFloatCVar(e_DissolveDistband) +
	  GetCVars()->e_DissolveSpriteDistRatio +
	  GetCVars()->e_DissolveSpriteMinDist +
	  GetCVars()->e_VegetationUseTerrainColor +
	  GetCVars()->e_TerrainDetailMaterials +
	  GetCVars()->e_ViewDistRatio +
	  GetCVars()->e_ViewDistMin +
	  GetCVars()->e_ViewDistRatioDetail +
	  GetCVars()->e_ViewDistRatioVegetation +
	  GetCVars()->e_DefaultMaterial +
	  GetCVars()->e_VegetationSpritesDistanceRatio +
	  GetCVars()->e_VegetationSpritesDistanceCustomRatioMin +
	  GetCVars()->e_VegetationSpritesMinDistance +
	  GetGeomDetailScreenRes() +
	  GetCVars()->e_Portals +
	  GetCVars()->e_DebugDraw +
	  GetFloatCVar(e_ViewDistCompMaxSize) +
	  GetCVars()->e_DecalsDeferredStatic +
	  (GetRenderer() ? GetRenderer()->GetWidth() : 0);

	if (m_fRefreshSceneDataCVarsSumm != -1 && m_fRefreshSceneDataCVarsSumm != fNewCVarsSumm)
	{
		UpdateStatInstGroups();

		// re-register every instance in level
		const float terrainSize = (float)GetTerrainSize();
		GetObjManager()->ReregisterEntitiesInArea(
		  Vec3(-terrainSize, -terrainSize, -terrainSize),
		  Vec3(terrainSize * 2.f, terrainSize * 2.f, terrainSize * 2.f));

		// force recreation of terrain meshes
		if (CTerrain* const pTerrain = GetTerrain())
		{
			pTerrain->ResetTerrainVertBuffers(NULL, -1);
		}

		// refresh vegetation properties
		UpdateStatInstGroups();

		// force refresh of temporary data associated with visible objects
		MarkRNTmpDataPoolForReset();
	}

	m_fRefreshSceneDataCVarsSumm = fNewCVarsSumm;

	int nRenderTypeEnableCVarSum = (GetCVars()->e_Vegetation << 0)
	                               + (GetCVars()->e_Brushes << 1)
	                               + (GetCVars()->e_Entities << 2);

	if (m_nRenderTypeEnableCVarSum != nRenderTypeEnableCVarSum)
	{
		m_nRenderTypeEnableCVarSum = nRenderTypeEnableCVarSum;

		UpdateRenderTypeEnableLookup();
	}

	{
		float fNewCVarsSumm2 = GetCVars()->e_LodRatio +
		                       GetCVars()->e_PermanentRenderObjects;

		if (m_bEditor)
		{
			fNewCVarsSumm2 += float(int(GetSkyColor().GetLength() * 10) / 10);
		}

		static float fCVarsSumm2 = fNewCVarsSumm2;

		if (fCVarsSumm2 != fNewCVarsSumm2)
		{
			MarkRNTmpDataPoolForReset();

			fCVarsSumm2 = fNewCVarsSumm2;
		}
	}

	SFrameLodInfo frameLodInfo;
	frameLodInfo.fLodRatio = GetCVars()->e_LodRatio;

	frameLodInfo.fTargetSize = GetCVars()->e_LodFaceAreaTargetSize;
	CRY_ASSERT(frameLodInfo.fTargetSize > 0.f);
	if (frameLodInfo.fTargetSize <= 0.f)
	{
		frameLodInfo.fTargetSize = 1.f;
	}

	frameLodInfo.nMinLod = GetCVars()->e_LodMin;
	frameLodInfo.nMaxLod = GetCVars()->e_LodMax;
	if (GetCVars()->e_Lods == 0)
	{
		frameLodInfo.nMinLod = 0;
		frameLodInfo.nMaxLod = 0;
	}
	SetFrameLodInfo(frameLodInfo);
}

//////////////////////////////////////////////////////////////////////
void C3DEngine::UpdateScene(const SRenderingPassInfo& passInfo)
{
	if (GetCVars()->e_Sun)
		UpdateSunLightSource(passInfo);

	FindPotentialLightSources(passInfo);

	// Set traceable fog volume areas
	CFogVolumeRenderNode::SetTraceableArea(AABB(passInfo.GetCamera().GetPosition(), 1024.0f), passInfo);
}

//////////////////////////////////////////////////////////////////////
void C3DEngine::ShutDown()
{
	if (GetRenderer() != GetSystem()->GetIRenderer())
		CryFatalError("Renderer was deallocated before I3DEngine::ShutDown() call");

	UnlockCGFResources();

	UnloadLevel();

#if defined(USE_GEOM_CACHES)
	delete m_pGeomCacheManager;
	m_pGeomCacheManager = 0;
#endif

	DestroyParticleManager(m_pPartManager);
	m_pPartManager = nullptr;
	m_pParticleSystem.reset();
	m_pSystem->SetIParticleManager(0);

	if (m_pOpticsManager)
	{
		delete m_pOpticsManager;
		m_pOpticsManager = 0;
		m_pSystem->SetIOpticsManager(m_pOpticsManager);
	}

	CryAlignedDelete(m_pObjManager);
	m_pObjManager = 0;

	if (GetPhysicalWorld())
	{
		CPhysCallbacks::Done();
	}

	// Free the temporary pool's underlying storage
	// and reset the pool
	if (!CTemporaryPool::Shutdown())
	{
		CryFatalError("C3DEngine::Shutdown() could not shutdown temporary pool");
	}

	COctreeNode::DeallocateRenderContentQueue();
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )               //AMD Port
	#pragma warning( disable : 4311 )
#endif

#ifndef _RELEASE
void C3DEngine::ProcessStreamingLatencyTest(const CCamera& camIn, CCamera& camOut, const SRenderingPassInfo& passInfo)
{
	static float fSQTestOffset = 0;
	static PodArray<ITexture*> arrTestTextures;
	static ITexture* pTestTexture = 0;
	static ITexture* pLastNotReadyTexture = 0;
	static float fStartTime = 0;
	static float fDelayStartTime = 0;
	static size_t nMaxTexUsage = 0;

	static int nOpenRequestCount = 0;
	SStreamEngineOpenStats stats;
	gEnv->pSystem->GetStreamEngine()->GetStreamingOpenStatistics(stats);
	if (stats.nOpenRequestCount > nOpenRequestCount)
		nOpenRequestCount = stats.nOpenRequestCount;
	else
		nOpenRequestCount = max(0, nOpenRequestCount + stats.nOpenRequestCount) / 2;

	ICVar* pTSFlush = GetConsole()->GetCVar("r_TexturesStreamingDebug");

	if (GetCVars()->e_SQTestBegin == 1)
	{
		// Init waiting few seconds until streaming is stabilized and all required textures are loaded
		PrintMessage("======== Starting streaming latency test ========");
		fDelayStartTime = GetCurTimeSec();
		nMaxTexUsage = 0;
		GetCVars()->e_SQTestBegin = 2;
		PrintMessage("Waiting %.1f seconds and zero requests and no camera movement", GetCVars()->e_SQTestDelay);

		if (ICVar* pPart = GetConsole()->GetCVar("e_Particles"))
			pPart->Set(0);
		if (ICVar* pAI = GetConsole()->GetCVar("sys_AI"))
			pAI->Set(0);
	}
	else if (GetCVars()->e_SQTestBegin == 2)
	{
		// Perform waiting
		if (GetCurTimeSec() - fDelayStartTime > GetCVars()->e_SQTestDelay && !nOpenRequestCount && m_fAverageCameraSpeed < .01f)
		{
			pTSFlush->Set(0);
			GetCVars()->e_SQTestBegin = 3;
		}
		else
			pTSFlush->Set(3);
	}
	else if (GetCVars()->e_SQTestBegin == 3)
	{
		// Build a list of all important loaded textures
		PrintMessage("Collect information about loaded textures");

		fSQTestOffset = (float)GetCVars()->e_SQTestDistance;

		arrTestTextures.Clear();
		SRendererQueryGetAllTexturesParam param;

		GetRenderer()->EF_Query(EFQ_GetAllTextures, param);
		if (param.pTextures)
		{
			for (uint32 i = 0; i < param.numTextures; i++)
			{
				ITexture* pTexture = param.pTextures[i];
				if (pTexture->GetAccessFrameId() > (int)(passInfo.GetMainFrameID() - 4))
					if (pTexture->GetMinLoadedMip() <= GetCVars()->e_SQTestMip)
						if (pTexture->IsStreamable())
							if (pTexture->GetWidth() * pTexture->GetHeight() >= 256 * 256)
							{
								arrTestTextures.Add(pTexture);

								if (strstr(pTexture->GetName(), GetCVars()->e_SQTestTextureName->GetString()))
								{
									pTestTexture = pTexture;
									PrintMessage("Test texture name: %s", pTexture->GetName());
								}
							}
			}

		}

		GetRenderer()->EF_Query(EFQ_GetAllTexturesRelease, param);

		PrintMessage("%d test textures found", arrTestTextures.Count());

		PrintMessage("Teleporting camera to offset position");

		GetCVars()->e_SQTestBegin = 4;
	}
	else if (GetCVars()->e_SQTestBegin == 4)
	{
		// Init waiting few seconds until streaming is stabilized and all required textures are loaded
		fDelayStartTime = GetCurTimeSec();
		GetCVars()->e_SQTestBegin = 5;
		PrintMessage("Waiting %.1f seconds and zero requests and no camera movement", GetCVars()->e_SQTestDelay);
	}
	else if (GetCVars()->e_SQTestBegin == 5)
	{
		// Move camera to offset position and perform waiting
		Matrix34 mat = camIn.GetMatrix();
		Vec3 vPos = camIn.GetPosition() - camIn.GetViewdir() * fSQTestOffset;
		mat.SetTranslation(vPos);
		camOut.SetMatrix(mat);

		if (GetCurTimeSec() - fDelayStartTime > GetCVars()->e_SQTestDelay && !nOpenRequestCount && m_fAverageCameraSpeed < .01f)
		{
			PrintMessage("Begin camera movement");
			GetCVars()->e_SQTestBegin = 6;
			pTSFlush->Set(0);
		}
		else
			pTSFlush->Set(3);
	}
	else if (GetCVars()->e_SQTestBegin == 6)
	{
		// Process camera movement from offset position to test point
		Matrix34 mat = camIn.GetMatrix();
		Vec3 vPos = camIn.GetPosition() - camIn.GetViewdir() * fSQTestOffset;
		mat.SetTranslation(vPos);
		camOut.SetMatrix(mat);

		fSQTestOffset -= GetTimer()->GetFrameTime() * (float)GetCVars()->e_SQTestMoveSpeed;

		STextureStreamingStats statsTex(true);
		m_pRenderer->EF_Query(EFQ_GetTexStreamingInfo, statsTex);
		nMaxTexUsage = max(nMaxTexUsage, statsTex.nRequiredStreamedTexturesSize);

		if (fSQTestOffset <= 0)
		{
			PrintMessage("Finished camera movement");
			fStartTime = GetCurTimeSec();
			PrintMessage("Waiting for %d textures to stream in ...", arrTestTextures.Count());

			GetCVars()->e_SQTestBegin = 7;
			pLastNotReadyTexture = 0;
		}
	}
	else if (GetCVars()->e_SQTestBegin == 7)
	{
		// Wait until test all needed textures are loaded again

		STextureStreamingStats statsTex(true);
		m_pRenderer->EF_Query(EFQ_GetTexStreamingInfo, statsTex);
		nMaxTexUsage = max(nMaxTexUsage, statsTex.nRequiredStreamedTexturesSize);

		if (pTestTexture)
		{
			if (pTestTexture->GetMinLoadedMip() <= GetCVars()->e_SQTestMip)
			{
				PrintMessage("BINGO: Selected test texture loaded in %.1f sec", GetCurTimeSec() - fStartTime);
				pTestTexture = NULL;
				if (!arrTestTextures.Count())
				{
					GetCVars()->e_SQTestBegin = 0;
					GetConsole()->GetCVar("e_SQTestBegin")->Set(0);
				}
			}
		}

		if (arrTestTextures.Count())
		{
			int nFinishedNum = 0;
			for (int i = 0; i < arrTestTextures.Count(); i++)
			{
				if (arrTestTextures[i]->GetMinLoadedMip() <= GetCVars()->e_SQTestMip)
					nFinishedNum++;
				else
					pLastNotReadyTexture = arrTestTextures[i];
			}

			if (nFinishedNum == arrTestTextures.Count())
			{
				PrintMessage("BINGO: %d of %d test texture loaded in %.1f sec", nFinishedNum, arrTestTextures.Count(), GetCurTimeSec() - fStartTime);
				if (pLastNotReadyTexture)
					PrintMessage("LastNotReadyTexture: %s [%d x %d]", pLastNotReadyTexture->GetName(), pLastNotReadyTexture->GetWidth(), pLastNotReadyTexture->GetHeight());
				PrintMessage("MaxTexUsage: %" PRISIZE_T " MB", nMaxTexUsage / 1024 / 1024);
				arrTestTextures.Clear();

				GetCVars()->e_SQTestBegin = 0;
				GetConsole()->GetCVar("e_SQTestBegin")->Set(0);

				m_arrProcessStreamingLatencyTestResults.Add(GetCurTimeSec() - fStartTime);
				m_arrProcessStreamingLatencyTexNum.Add(nFinishedNum);

				if (GetCVars()->e_SQTestCount == 0)
				{
					const char* szTestResults = "%USER%/TestResults";
					char path[ICryPak::g_nMaxPath] = "";
					gEnv->pCryPak->AdjustFileName(string(string(szTestResults) + "\\" + "Streaming_Latency_Test.xml").c_str(), path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
					gEnv->pCryPak->MakeDir(szTestResults);

					if (FILE* f = ::fopen(path, "wb"))
					{
						float fAverTime = 0;
						for (int i = 0; i < m_arrProcessStreamingLatencyTestResults.Count(); i++)
							fAverTime += m_arrProcessStreamingLatencyTestResults[i];
						fAverTime /= m_arrProcessStreamingLatencyTestResults.Count();

						int nAverTexNum = 0;
						for (int i = 0; i < m_arrProcessStreamingLatencyTexNum.Count(); i++)
							nAverTexNum += m_arrProcessStreamingLatencyTexNum[i];
						nAverTexNum /= m_arrProcessStreamingLatencyTexNum.Count();

						fprintf(f,
						        "<phase name=\"Streaming_Latency_Test\">\n"
						        "<metrics name=\"Streaming\">\n"
						        "<metric name=\"AvrLatency\" value=\"%.1f\"/>\n"
						        "<metric name=\"AvrTexNum\" value=\"%d\"/>\n"
						        "</metrics>\n"
						        "</phase>\n",
						        fAverTime,
						        nAverTexNum);

						::fclose(f);
					}

					if (GetCVars()->e_SQTestExitOnFinish)
						GetSystem()->Quit();
				}
			}
			else if ((passInfo.GetMainFrameID() & 31) == 0)
			{
				PrintMessage("Waiting: %d of %d test texture loaded in %.1f sec", nFinishedNum, arrTestTextures.Count(), GetCurTimeSec() - fStartTime);
			}
		}
	}
}
#endif

//////////////////////////////////////////////////////////////////////
void C3DEngine::UpdateRenderingCamera(const char* szCallerName, const SRenderingPassInfo& passInfo)
{
	CCamera newCam = passInfo.GetCamera();

#if defined(FEATURE_SVO_GI)
	if (passInfo.IsGeneralPass())
		CSvoManager::Update(passInfo, newCam);
#endif

	if (GetFloatCVar(e_CameraRotationSpeed))
	{
		Matrix34 mat = passInfo.GetCamera().GetMatrix();
		Matrix33 matRot;
		matRot.SetRotationZ(-GetCurTimeSec() * GetFloatCVar(e_CameraRotationSpeed));
		newCam.SetMatrix(mat * matRot);
	}

#if !defined(_RELEASE)

	{
		//this feature move the camera along with the player to a certain position and sets the angle accordingly
		//	(does not work via goto)
		//u can switch it off again via e_CameraGoto 0
		const char* const pCamGoto = GetCVars()->e_CameraGoto->GetString();
		assert(pCamGoto);
		if (strlen(pCamGoto) > 1)
		{
			Ang3 aAngDeg;
			Vec3 vPos;
			int args = sscanf(pCamGoto, "%f %f %f %f %f %f", &vPos.x, &vPos.y, &vPos.z, &aAngDeg.x, &aAngDeg.y, &aAngDeg.z);
			if (args >= 3)
			{
				Vec3 curPos = newCam.GetPosition();
				if (fabs(vPos.x - curPos.x) > 10.f || fabs(vPos.y - curPos.y) > 10.f || fabs(vPos.z - curPos.z) > 10.f)
				{
					char buf[128];
					cry_sprintf(buf, "goto %f %f %f", vPos.x, vPos.y, vPos.z);
					gEnv->pConsole->ExecuteString(buf);
				}
				if (args >= 6)
				{
					Matrix34 mat = passInfo.GetCamera().GetMatrix();
					mat.SetTranslation(vPos);
					mat.SetRotation33(Matrix33::CreateRotationXYZ(DEG2RAD(aAngDeg)));
					newCam.SetMatrix(mat);
				}
			}
		}
	}

	// Streaming latency test
	if (GetCVars()->e_SQTestCount && !GetCVars()->e_SQTestBegin)
	{
		GetConsole()->GetCVar("e_SQTestBegin")->Set(1);
		GetConsole()->GetCVar("e_SQTestCount")->Set(GetCVars()->e_SQTestCount - 1);
	}
	if (GetCVars()->e_SQTestBegin)
		ProcessStreamingLatencyTest(passInfo.GetCamera(), newCam, passInfo);

#endif

	// set the camera if e_cameraFreeze is not set
	if (GetCVars()->e_CameraFreeze || GetCVars()->e_CoverageBufferDebugFreeze)
	{
		DrawSphere(GetRenderingCamera().GetPosition(), .05f);

		// alwasy set camera to request postion for the renderer, allows debugging with e_camerafreeze
		if (GetRenderer())
		{
			GetRenderer()->SetCamera(gEnv->pSystem->GetViewCamera());
		}
	}
	else
	{
		m_RenderingCamera = newCam;
		// alwasy set camera to request postion for the renderer, allows debugging with e_camerafreeze
		if (GetRenderer())
		{
			GetRenderer()->SetCamera(newCam);
		}
	}

	// update zoom state to be queried by client code
	m_fZoomFactor = passInfo.GetZoomFactor();
	m_bZoomInProgress = passInfo.IsZoomInProgress();

	// now we have a valid camera, we can start generation of the occlusion buffer
	// only needed for editor here, ingame we spawn the job more early
	if (passInfo.IsGeneralPass() && GetCVars()->e_StatObjBufferRenderTasks && JobManager::InvokeAsJob("CheckOcclusion"))
	{
		if (gEnv->IsEditor())
			GetObjManager()->PrepareCullbufferAsync(passInfo.GetCamera());
		else
			assert(IsEquivalent(passInfo.GetCamera().GetViewdir(), GetObjManager()->m_CullThread.GetViewDir())); // early set camera differs from current main camera - will cause occlusion errors
	}

	/////////////////////////////////////////////////////////////////////////////
	// Update Foliage
	float dt = GetTimer()->GetFrameTime();
	CStatObjFoliage* pFoliage, * pFoliageNext;
	for (pFoliage = m_pFirstFoliage; &pFoliage->m_next != &m_pFirstFoliage; pFoliage = pFoliageNext)
	{
		pFoliageNext = pFoliage->m_next;
		pFoliage->Update(dt, GetRenderingCamera());
	}
	for (int i = m_arrEntsInFoliage.size() - 1; i >= 0; i--)
		if ((m_arrEntsInFoliage[i].timeIdle += dt) > 0.3f)
			RemoveEntInFoliage(i);

	/////////////////////////////////////////////////////////////////////////////
	// update streaming priority of newly seen CRenderProxies (fix for streaming system issue)
	for (int i = 0, nSize = m_deferredRenderProxyStreamingPriorityUpdates.size(); i < nSize; ++i)
	{
		IRenderNode* pRenderNode = m_deferredRenderProxyStreamingPriorityUpdates[i];
		AABB aabb = pRenderNode->GetBBox();
		const Vec3& vCamPos = GetRenderingCamera().GetPosition();
		float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, aabb)) * passInfo.GetZoomFactor();

		GetObjManager()->UpdateRenderNodeStreamingPriority(pRenderNode, fEntDistance, 1.0f, false, passInfo);
		if (GetCVars()->e_StreamCgfDebug == 2)
			PrintMessage("C3DEngine::RegisterEntity__GetObjManager()->UpdateRenderNodeStreamingPriority %s", pRenderNode->GetName());
	}
	m_deferredRenderProxyStreamingPriorityUpdates.resize(0);

}

void C3DEngine::PrepareOcclusion(const CCamera& rCamera)
{
	if (!gEnv->IsEditor() && GetCVars()->e_StatObjBufferRenderTasks && !gEnv->IsFMVPlaying() && (!IsEquivalent(rCamera.GetPosition(), Vec3(0, 0, 0), VEC_EPSILON)
	                                                                                             || (GetRenderer() && GetRenderer()->IsPost3DRendererEnabled())))
		GetObjManager()->PrepareCullbufferAsync(rCamera);
}

void C3DEngine::EndOcclusion()
{
	GetObjManager()->EndOcclusionCulling();
}

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )                //AMD Port
#endif

IStatObj* C3DEngine::LoadStatObj(const char* szFileName, const char* szGeomName, IStatObj::SSubObject** ppSubObject, bool bUseStreaming, unsigned long nLoadingFlags)
{
	if (!szFileName || !szFileName[0])
	{
		m_pSystem->Warning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, 0, 0, "I3DEngine::LoadStatObj: filename is not specified");
		return 0;
	}

	if (!m_pObjManager)
		m_pObjManager = CryAlignedNew<CObjManager>();

	return m_pObjManager->LoadStatObj(szFileName, szGeomName, ppSubObject, bUseStreaming, nLoadingFlags);
}

IStatObj* C3DEngine::FindStatObjectByFilename(const char* filename)
{
	if (filename == NULL)
		return NULL;

	if (filename[0] == 0)
		return NULL;

	return m_pObjManager->FindStaticObjectByFilename(filename);
}

void C3DEngine::RegisterEntity(IRenderNode* pEnt, int nSID, int nSIDConsideredSafe)
{
	FUNCTION_PROFILER_3DENGINE;
	if (gEnv->mMainThreadId != CryGetCurrentThreadId())
	{
		CryFatalError("C3DEngine::RegisterEntity should only be called on main thread.");
	}

	uint32 nFrameID = gEnv->nMainFrameID;
	AsyncOctreeUpdate(pEnt, nSID, nSIDConsideredSafe, nFrameID, false);
}

void C3DEngine::UnRegisterEntityDirect(IRenderNode* pEnt)
{
	UnRegisterEntityImpl(pEnt);
}

void C3DEngine::UnRegisterEntityAsJob(IRenderNode* pEnt)
{
	AsyncOctreeUpdate(pEnt, (int)0, (int)0, (int)0, true);
}

bool C3DEngine::CreateDecalInstance(const CryEngineDecalInfo& decal, CDecal* pCallerManagedDecal)
{
	if (!GetCVars()->e_Decals && !pCallerManagedDecal)
		return false;

	return m_pDecalManager->Spawn(decal, pCallerManagedDecal);
}

void C3DEngine::SelectEntity(IRenderNode* pEntity)
{
	static IRenderNode* pSelectedNode;
	static float fLastTime;
	if (pEntity && GetCVars()->e_Decals == 3)
	{
		float fCurTime = gEnv->pTimer->GetAsyncCurTime();
		if (fCurTime - fLastTime < 1.0f)
			return;
		fLastTime = fCurTime;
		if (pSelectedNode)
			pSelectedNode->SetRndFlags(ERF_SELECTED, false);
		pEntity->SetRndFlags(ERF_SELECTED, true);
		pSelectedNode = pEntity;
	}
}

void C3DEngine::CreateDecal(const struct CryEngineDecalInfo& decal)
{
	IF (!GetCVars()->e_DecalsAllowGameDecals, 0)
		return;

	if (GetCVars()->e_Decals == 2)
	{
		IRenderNode* pRN = decal.ownerInfo.pRenderNode;
		PrintMessage("Debug: C3DEngine::CreateDecal: Pos=(%.1f,%.1f,%.1f) Size=%.2f DecalMaterial=%s HitObjectName=%s(%s)",
		             decal.vPos.x, decal.vPos.y, decal.vPos.z, decal.fSize, decal.szMaterialName,
		             pRN ? pRN->GetName() : "NULL", pRN ? pRN->GetEntityClassName() : "NULL");
	}

	assert(!decal.pExplicitRightUpFront); // only game-play decals come here

	static uint32 nGroupId = 0;
	nGroupId++;

	if ((GetCVars()->e_DecalsDeferredStatic == 1 && decal.pExplicitRightUpFront) ||
	    (GetCVars()->e_DecalsDeferredDynamic == 1 && !decal.pExplicitRightUpFront &&
	     (!decal.ownerInfo.pRenderNode || decal.ownerInfo.pRenderNode->GetRenderNodeType() == eERType_Brush || decal.fGrowTimeAlpha || decal.fSize > GetFloatCVar(e_DecalsDeferredDynamicMinSize)))
	    && !decal.bForceSingleOwner)
	{
		CryEngineDecalInfo decal_adjusted = decal;
		decal_adjusted.nGroupId = nGroupId;
		decal_adjusted.bDeferred = true;
		m_pDecalManager->SpawnHierarchical(decal_adjusted, NULL);
		return;
	}

	if (decal.ownerInfo.pRenderNode && decal.fSize > 0.5f && !decal.bForceSingleOwner)
	{
		PodArray<SRNInfo> lstEntities;
		Vec3 vRadius(decal.fSize, decal.fSize, decal.fSize);
		const AABB cExplosionBox(decal.vPos - vRadius, decal.vPos + vRadius);

		if (CVisArea* pArea = (CVisArea*)decal.ownerInfo.pRenderNode->GetEntityVisArea())
		{
			if (pArea->m_pObjectsTree)
				pArea->m_pObjectsTree->MoveObjectsIntoList(&lstEntities, &cExplosionBox, false, true, true, true);
		}
		else
			Get3DEngine()->MoveObjectsIntoListGlobal(&lstEntities, &cExplosionBox, false, true, true, true);

		for (int i = 0; i < lstEntities.Count(); i++)
		{
			// decals on statobj's of render node
			CryEngineDecalInfo decalOnRenderNode = decal;
			decalOnRenderNode.ownerInfo.pRenderNode = lstEntities[i].pNode;
			decalOnRenderNode.nGroupId = nGroupId;

			//      if(decalOnRenderNode.ownerInfo.pRenderNode->GetRenderNodeType() != decal.ownerInfo.pRenderNode->GetRenderNodeType())
			//      continue;

			if (decalOnRenderNode.ownerInfo.pRenderNode->GetRndFlags() & ERF_HIDDEN)
				continue;

			m_pDecalManager->SpawnHierarchical(decalOnRenderNode, NULL);
		}
	}
	else
	{
		CryEngineDecalInfo decalStatic = decal;
		decalStatic.nGroupId = nGroupId;
		m_pDecalManager->SpawnHierarchical(decalStatic, NULL);
	}
}

/*
   float C3DEngine::GetDayTime(float fPrevDayTime)
   {
   if(fPrevDayTime)
   {
    float fDayTimeDiff = (GetCVars()->e_day_time - fPrevDayTime);
    while(fDayTimeDiff>12)
      fDayTimeDiff-=12;
    while(fDayTimeDiff<-12)
      fDayTimeDiff+=12;
    fDayTimeDiff = (float)fabs(fDayTimeDiff);
    return fDayTimeDiff;
   }

   return GetCVars()->e_day_time;
   } */

namespace
{
inline const float GetSunSkyRel(const Vec3& crSkyColor, const Vec3& crSunColor)
{
	const float cSunMax = max(0.1f, max(crSunColor.x, max(crSunColor.y, crSunColor.z)));
	const float cSkyMax = max(0.1f, max(crSkyColor.x, max(crSkyColor.y, crSkyColor.z)));
	return cSunMax / (cSkyMax + cSunMax);
}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::SetSkyColor(Vec3 vColor)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_vSkyColor = vColor;
		m_pObjManager->m_fSunSkyRel = GetSunSkyRel(m_pObjManager->m_vSkyColor, m_pObjManager->m_vSunColor);
	}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::SetSunColor(Vec3 vColor)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_vSunColor = vColor;
		m_pObjManager->m_fSunSkyRel = GetSunSkyRel(m_pObjManager->m_vSkyColor, m_pObjManager->m_vSunColor);
	}
}

void C3DEngine::SetSkyBrightness(float fMul)
{
	if (m_pObjManager)
		m_pObjManager->m_fSkyBrightMul = fMul;
}

void C3DEngine::SetSSAOAmount(float fMul)
{
	if (m_pObjManager)
		m_pObjManager->m_fSSAOAmount = fMul;
}

void C3DEngine::SetSSAOContrast(float fMul)
{
	if (m_pObjManager)
		m_pObjManager->m_fSSAOContrast = fMul;
}

void C3DEngine::SetGIAmount(float fMul)
{
	if (m_pObjManager)
		m_pObjManager->m_fGIAmount = fMul;
}

float C3DEngine::GetTerrainElevation(float x, float y, int nSID)
{
	float fZ = 0;

	if (m_pTerrain)
		fZ = m_pTerrain->GetZApr(x, y, nSID);

	return fZ;
}

float C3DEngine::GetTerrainElevation3D(Vec3 vPos)
{
	float fZ = 0;

	if (m_pTerrain)
		fZ = m_pTerrain->GetZApr(vPos.x, vPos.y, GetDefSID());

	return fZ;
}

float C3DEngine::GetTerrainZ(int x, int y)
{
	if (x < 0 || y < 0 || x >= CTerrain::GetTerrainSize() || y >= CTerrain::GetTerrainSize())
		return TERRAIN_BOTTOM_LEVEL;
	return m_pTerrain ? m_pTerrain->GetZ(x, y, GetDefSID()) : 0;
}

bool C3DEngine::GetTerrainHole(int x, int y)
{
	return m_pTerrain ? m_pTerrain->GetHole(x, y, GetDefSID()) : false;
}

int C3DEngine::GetHeightMapUnitSize()
{
	return CTerrain::GetHeightMapUnitSize();
}

void C3DEngine::RemoveAllStaticObjects(int nSID)
{
	if (m_pTerrain)
		m_pTerrain->RemoveAllStaticObjects(nSID);
}

void C3DEngine::SetTerrainSurfaceType(int x, int y, int nType)
{
	assert(0);
}

void C3DEngine::SetTerrainSectorTexture(const int nTexSectorX, const int nTexSectorY, unsigned int textureId)
{
	if (m_pTerrain)
	{
		bool bMergeNotAllowed = m_pSegmentsManager ? false : true;
		m_pTerrain->SetTerrainSectorTexture(nTexSectorX, nTexSectorY, textureId, bMergeNotAllowed, GetDefSID());
	}
}

void C3DEngine::OnExplosion(Vec3 vPos, float fRadius, bool bDeformTerrain)
{
	if (GetCVars()->e_Decals == 2)
		PrintMessage("Debug: C3DEngine::OnExplosion: Pos=(%.1f,%.1f,%.1f) fRadius=%.2f", vPos.x, vPos.y, vPos.z, fRadius);

	if (vPos.x < 0 || vPos.x >= CTerrain::GetTerrainSize() || vPos.y < 0 || vPos.y >= CTerrain::GetTerrainSize() || fRadius <= 0)
		return; // out of terrain

	// do not create decals near the terrain holes
	{
		for (int x = int(vPos.x - fRadius); x <= int(vPos.x + fRadius) + 1; x += CTerrain::GetHeightMapUnitSize())
			for (int y = int(vPos.y - fRadius); y <= int(vPos.y + fRadius) + 1; y += CTerrain::GetHeightMapUnitSize())
				if (m_pTerrain->GetHole(x, y, GetDefSID()))
					return;
	}

	// try to remove objects around
	bool bGroundDeformationAllowed = m_pTerrain->RemoveObjectsInArea(vPos, fRadius) && bDeformTerrain && GetCVars()->e_TerrainDeformations;

	// reduce ground decals size depending on distance to the ground
	float fExploHeight = vPos.z - m_pTerrain->GetZApr(vPos.x, vPos.y, GetDefSID());

	if (bGroundDeformationAllowed && (fExploHeight > -0.1f) && fExploHeight < fRadius && fRadius > 0.125f)
	{
		m_pTerrain->MakeCrater(vPos, fRadius);

		// update roads
		{
			PodArray<IRenderNode*> lstRoads;
			AABB bbox(vPos - Vec3(fRadius, fRadius, fRadius), vPos + Vec3(fRadius, fRadius, fRadius));
			Get3DEngine()->GetObjectsByTypeGlobal(lstRoads, eERType_Road, &bbox);
			for (int i = 0; i < lstRoads.Count(); i++)
			{
				CRoadRenderNode* pRoad = (CRoadRenderNode*)lstRoads[i];
				pRoad->OnTerrainChanged();
			}
		}
	}

	// delete decals what can not be correctly updated
	Vec3 vRadius(fRadius, fRadius, fRadius);
	AABB areaBox(vPos - vRadius, vPos + vRadius);
	Get3DEngine()->DeleteDecalsInRange(&areaBox, NULL);
}

float C3DEngine::GetMaxViewDistance(bool bScaled)
{
	// spec lerp factor
	float lerpSpec = clamp_tpl(GetCVars()->e_MaxViewDistSpecLerp, 0.0f, 1.0f);

	// camera height lerp factor
	float lerpHeight = clamp_tpl(max(0.f, GetSystem()->GetViewCamera().GetPosition().z - GetWaterLevel()) / GetFloatCVar(e_MaxViewDistFullDistCamHeight), 0.0f, 1.0f);

	// lerp between specs
	float fMaxViewDist = m_fMaxViewDistLowSpec * (1.f - lerpSpec) + m_fMaxViewDistHighSpec * lerpSpec;

	// lerp between prev result and high spec
	fMaxViewDist = fMaxViewDist * (1.f - lerpHeight) + m_fMaxViewDistHighSpec * lerpHeight;

	if (bScaled)
		fMaxViewDist *= m_fMaxViewDistScale;

	// for debugging
	const float fMaxViewDistCVar = GetFloatCVar(e_MaxViewDistance);
	fMaxViewDist = (float)__fsel(fMaxViewDistCVar, fMaxViewDistCVar, fMaxViewDist);

	fMaxViewDist = (float)__fsel(fabsf(fMaxViewDist) - 0.100001f, fMaxViewDist, 0.100001f);

	return fMaxViewDist;
}

void C3DEngine::SetFrameLodInfo(const SFrameLodInfo& frameLodInfo)
{
	if (frameLodInfo.fLodRatio != m_frameLodInfo.fLodRatio || frameLodInfo.fTargetSize != m_frameLodInfo.fTargetSize)
	{
		++m_frameLodInfo.nID;
		m_frameLodInfo.fLodRatio = frameLodInfo.fLodRatio;
		m_frameLodInfo.fTargetSize = frameLodInfo.fTargetSize;
	}
	m_frameLodInfo.nMinLod = frameLodInfo.nMinLod;
	m_frameLodInfo.nMaxLod = frameLodInfo.nMaxLod;
}

void C3DEngine::SetFogColor(const Vec3& vFogColor)
{
	m_vFogColor = vFogColor;
	if (GetRenderer())
	{
		GetRenderer()->SetClearColor(m_vFogColor);
	}
}

Vec3 C3DEngine::GetFogColor()
{
	return m_vFogColor;
}

void C3DEngine::GetSkyLightParameters(Vec3& sunDir, Vec3& sunIntensity, float& Km, float& Kr, float& g, Vec3& rgbWaveLengths)
{
	CSkyLightManager::SSkyDomeCondition skyCond;
	m_pSkyLightManager->GetCurSkyDomeCondition(skyCond);

	g = skyCond.m_g;
	Km = skyCond.m_Km;
	Kr = skyCond.m_Kr;
	sunIntensity = skyCond.m_sunIntensity;
	rgbWaveLengths = skyCond.m_rgbWaveLengths;
	sunDir = skyCond.m_sunDirection;
}

void C3DEngine::SetSkyLightParameters(const Vec3& sunDir, const Vec3& sunIntensity, float Km, float Kr, float g, const Vec3& rgbWaveLengths, bool forceImmediateUpdate)
{
	CSkyLightManager::SSkyDomeCondition skyCond;

	skyCond.m_g = g;
	skyCond.m_Km = Km;
	skyCond.m_Kr = Kr;
	skyCond.m_sunIntensity = sunIntensity;
	skyCond.m_rgbWaveLengths = rgbWaveLengths;
	skyCond.m_sunDirection = sunDir;

	m_pSkyLightManager->SetSkyDomeCondition(skyCond);
	if (forceImmediateUpdate && IsHDRSkyMaterial(GetSkyMaterial()))
		m_pSkyLightManager->FullUpdate();
}

void C3DEngine::SetLightsHDRDynamicPowerFactor(const float value)
{
	m_fLightsHDRDynamicPowerFactor = value;
}

float C3DEngine::GetLightsHDRDynamicPowerFactor() const
{
	return m_fLightsHDRDynamicPowerFactor;
}

bool C3DEngine::IsTessellationAllowedForShadowMap(const SRenderingPassInfo& passInfo) const
{
#ifdef MESH_TESSELLATION_ENGINE
	SRenderingPassInfo::EShadowMapType shadowType = passInfo.GetShadowMapType();
	switch (shadowType)
	{
	case SRenderingPassInfo::SHADOW_MAP_GSM:
		return passInfo.ShadowFrustumLod() < GetCVars()->e_ShadowsTessellateCascades;
	case SRenderingPassInfo::SHADOW_MAP_LOCAL:
		return GetCVars()->e_ShadowsTessellateDLights != 0;
	case SRenderingPassInfo::SHADOW_MAP_NONE:
	default:
		return false;
	}
#endif //#ifdef MESH_TESSELLATION_ENGINE

	return false;
}

void C3DEngine::SetPhysMaterialEnumerator(IPhysMaterialEnumerator* pPhysMaterialEnumerator)
{
	m_pPhysMaterialEnumerator = pPhysMaterialEnumerator;
}

IPhysMaterialEnumerator* C3DEngine::GetPhysMaterialEnumerator()
{
	return m_pPhysMaterialEnumerator;
}

void C3DEngine::ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce)
{
	if (m_pTerrain)
		m_pTerrain->ApplyForceToEnvironment(vPos, fRadius, fAmountOfForce);
}

float C3DEngine::GetDistanceToSectorWithWater()
{
	if (!m_pTerrain || !m_pTerrain->GetParentNode(0))
		return 100000.f;

	Vec3 camPostion = GetRenderingCamera().GetPosition();
	bool bCameraInTerrainBounds = Overlap::Point_AABB2D(camPostion, m_pTerrain->GetParentNode(0)->GetBBoxVirtual());

	return (bCameraInTerrainBounds && (m_pTerrain && m_pTerrain->GetDistanceToSectorWithWater() > 0.1f))
	       ? m_pTerrain->GetDistanceToSectorWithWater() : max(camPostion.z - GetWaterLevel(), 0.1f);
}

Vec3 C3DEngine::GetSunColor() const
{
	return m_pObjManager ? m_pObjManager->m_vSunColor : Vec3(0, 0, 0);
}

float C3DEngine::GetSkyBrightness() const
{
	return m_pObjManager ? m_pObjManager->m_fSkyBrightMul : 1.0f;
}

float C3DEngine::GetSSAOAmount() const
{
	return m_pObjManager ? m_pObjManager->m_fSSAOAmount : 1.0f;
}

float C3DEngine::GetSSAOContrast() const
{
	return m_pObjManager ? m_pObjManager->m_fSSAOContrast : 1.0f;
}

float C3DEngine::GetGIAmount() const
{
	return m_pObjManager ? m_pObjManager->m_fGIAmount : 1.0f;
}

float C3DEngine::GetSunRel() const
{
	return m_pObjManager->m_fSunSkyRel;
}

void C3DEngine::SetRainParams(const SRainParams& rainParams)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_rainParams.bIgnoreVisareas = rainParams.bIgnoreVisareas;
		m_pObjManager->m_rainParams.bDisableOcclusion = rainParams.bDisableOcclusion;
		m_pObjManager->m_rainParams.qRainRotation = rainParams.qRainRotation;
		m_pObjManager->m_rainParams.vWorldPos = rainParams.vWorldPos;
		m_pObjManager->m_rainParams.vColor = rainParams.vColor;
		m_pObjManager->m_rainParams.fAmount = rainParams.fAmount;
		m_pObjManager->m_rainParams.fCurrentAmount = rainParams.fCurrentAmount;
		m_pObjManager->m_rainParams.fRadius = rainParams.fRadius;
		m_pObjManager->m_rainParams.fFakeGlossiness = rainParams.fFakeGlossiness;
		m_pObjManager->m_rainParams.fFakeReflectionAmount = rainParams.fFakeReflectionAmount;
		m_pObjManager->m_rainParams.fDiffuseDarkening = rainParams.fDiffuseDarkening;
		m_pObjManager->m_rainParams.fRainDropsAmount = rainParams.fRainDropsAmount;
		m_pObjManager->m_rainParams.fRainDropsSpeed = rainParams.fRainDropsSpeed;
		m_pObjManager->m_rainParams.fRainDropsLighting = rainParams.fRainDropsLighting;
		m_pObjManager->m_rainParams.fMistAmount = rainParams.fMistAmount;
		m_pObjManager->m_rainParams.fMistHeight = rainParams.fMistHeight;
		m_pObjManager->m_rainParams.fPuddlesAmount = rainParams.fPuddlesAmount;
		m_pObjManager->m_rainParams.fPuddlesMaskAmount = rainParams.fPuddlesMaskAmount;
		m_pObjManager->m_rainParams.fPuddlesRippleAmount = rainParams.fPuddlesRippleAmount;
		m_pObjManager->m_rainParams.fSplashesAmount = rainParams.fSplashesAmount;

		m_pObjManager->m_rainParams.nUpdateFrameID = gEnv->nMainFrameID;
	}
}

bool C3DEngine::GetRainParams(SRainParams& rainParams)
{
	bool bRet = false;
	const int nFrmID = gEnv->nMainFrameID;
	if (m_pObjManager)
	{
		// Copy shared rain data only
		rainParams.bIgnoreVisareas = m_pObjManager->m_rainParams.bIgnoreVisareas;
		rainParams.bDisableOcclusion = m_pObjManager->m_rainParams.bDisableOcclusion;
		rainParams.qRainRotation = m_pObjManager->m_rainParams.qRainRotation;
		rainParams.vWorldPos = m_pObjManager->m_rainParams.vWorldPos;
		rainParams.vColor = m_pObjManager->m_rainParams.vColor;
		rainParams.fAmount = m_pObjManager->m_rainParams.fAmount;
		rainParams.fCurrentAmount = m_pObjManager->m_rainParams.fCurrentAmount;
		rainParams.fRadius = m_pObjManager->m_rainParams.fRadius;
		rainParams.fFakeGlossiness = m_pObjManager->m_rainParams.fFakeGlossiness;
		rainParams.fFakeReflectionAmount = m_pObjManager->m_rainParams.fFakeReflectionAmount;
		rainParams.fDiffuseDarkening = m_pObjManager->m_rainParams.fDiffuseDarkening;
		rainParams.fRainDropsAmount = m_pObjManager->m_rainParams.fRainDropsAmount;
		rainParams.fRainDropsSpeed = m_pObjManager->m_rainParams.fRainDropsSpeed;
		rainParams.fRainDropsLighting = m_pObjManager->m_rainParams.fRainDropsLighting;
		rainParams.fMistAmount = m_pObjManager->m_rainParams.fMistAmount;
		rainParams.fMistHeight = m_pObjManager->m_rainParams.fMistHeight;
		rainParams.fPuddlesAmount = m_pObjManager->m_rainParams.fPuddlesAmount;
		rainParams.fPuddlesMaskAmount = m_pObjManager->m_rainParams.fPuddlesMaskAmount;
		rainParams.fPuddlesRippleAmount = m_pObjManager->m_rainParams.fPuddlesRippleAmount;
		rainParams.fSplashesAmount = m_pObjManager->m_rainParams.fSplashesAmount;

		if (!IsOutdoorVisible() && !rainParams.bIgnoreVisareas)
		{
			rainParams.fAmount = 0.f;
		}

		bRet = m_pObjManager->m_rainParams.nUpdateFrameID == nFrmID;
	}
	return bRet;
}

void C3DEngine::SetSnowSurfaceParams(const Vec3& vCenter, float fRadius, float fSnowAmount, float fFrostAmount, float fSurfaceFreezing)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_snowParams.m_vWorldPos = vCenter;
		m_pObjManager->m_snowParams.m_fRadius = fRadius;
		m_pObjManager->m_snowParams.m_fSnowAmount = fSnowAmount;
		m_pObjManager->m_snowParams.m_fFrostAmount = fFrostAmount;
		m_pObjManager->m_snowParams.m_fSurfaceFreezing = fSurfaceFreezing;
	}
}

bool C3DEngine::GetSnowSurfaceParams(Vec3& vCenter, float& fRadius, float& fSnowAmount, float& fFrostAmount, float& fSurfaceFreezing)
{
	bool bRet = false;
	if (m_pObjManager)
	{
		vCenter = m_pObjManager->m_snowParams.m_vWorldPos;
		fRadius = m_pObjManager->m_snowParams.m_fRadius;
		fSnowAmount = 0.f;
		fFrostAmount = 0.f;
		fSurfaceFreezing = 0.f;
		if (IsOutdoorVisible())
		{
			fSnowAmount = m_pObjManager->m_snowParams.m_fSnowAmount;
			fFrostAmount = m_pObjManager->m_snowParams.m_fFrostAmount;
			fSurfaceFreezing = m_pObjManager->m_snowParams.m_fSurfaceFreezing;
		}
		bRet = true;
	}
	return bRet;
}

void C3DEngine::SetSnowFallParams(int nSnowFlakeCount, float fSnowFlakeSize, float fSnowFallBrightness, float fSnowFallGravityScale, float fSnowFallWindScale, float fSnowFallTurbulence, float fSnowFallTurbulenceFreq)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_snowParams.m_nSnowFlakeCount = nSnowFlakeCount;
		m_pObjManager->m_snowParams.m_fSnowFlakeSize = fSnowFlakeSize;
		m_pObjManager->m_snowParams.m_fSnowFallBrightness = fSnowFallBrightness;
		m_pObjManager->m_snowParams.m_fSnowFallGravityScale = fSnowFallGravityScale;
		m_pObjManager->m_snowParams.m_fSnowFallWindScale = fSnowFallWindScale;
		m_pObjManager->m_snowParams.m_fSnowFallTurbulence = fSnowFallTurbulence;
		m_pObjManager->m_snowParams.m_fSnowFallTurbulenceFreq = fSnowFallTurbulenceFreq;
	}
}

bool C3DEngine::GetSnowFallParams(int& nSnowFlakeCount, float& fSnowFlakeSize, float& fSnowFallBrightness, float& fSnowFallGravityScale, float& fSnowFallWindScale, float& fSnowFallTurbulence, float& fSnowFallTurbulenceFreq)
{
	bool bRet = false;
	if (m_pObjManager)
	{
		nSnowFlakeCount = 0;
		fSnowFlakeSize = 0.f;
		fSnowFallBrightness = 0.f;
		fSnowFallGravityScale = 0.f;
		fSnowFallWindScale = 0.f;
		fSnowFallTurbulence = 0.f;
		fSnowFallTurbulenceFreq = 0.f;
		if (IsOutdoorVisible())
		{
			nSnowFlakeCount = m_pObjManager->m_snowParams.m_nSnowFlakeCount;
			fSnowFlakeSize = m_pObjManager->m_snowParams.m_fSnowFlakeSize;
			fSnowFallBrightness = m_pObjManager->m_snowParams.m_fSnowFallBrightness;
			fSnowFallGravityScale = m_pObjManager->m_snowParams.m_fSnowFallGravityScale;
			fSnowFallWindScale = m_pObjManager->m_snowParams.m_fSnowFallWindScale;
			fSnowFallTurbulence = m_pObjManager->m_snowParams.m_fSnowFallTurbulence;
			fSnowFallTurbulenceFreq = m_pObjManager->m_snowParams.m_fSnowFallTurbulenceFreq;
		}
		bRet = true;
	}
	return bRet;
}

float C3DEngine::GetTerrainTextureMultiplier(int nSID) const
{
	if (!m_pTerrain || m_bInUnload)
		return 0;

	return m_pTerrain->GetTerrainTextureMultiplier(nSID);
}

void C3DEngine::SetSunDir(const Vec3& newSunDir)
{
	Vec3 vSunDirNormalized = newSunDir.normalized();
	m_vSunDirRealtime = vSunDirNormalized;
	if (vSunDirNormalized.Dot(m_vSunDirNormalized) < GetFloatCVar(e_SunAngleSnapDot) ||
	    GetCurTimeSec() - m_fSunDirUpdateTime > GetFloatCVar(e_SunAngleSnapSec))
	{
		m_vSunDirNormalized = vSunDirNormalized;
		m_vSunDir = m_vSunDirNormalized * DISTANCE_TO_THE_SUN;

		m_fSunDirUpdateTime = GetCurTimeSec();
	}
}

Vec3 C3DEngine::GetSunDir()  const
{
	return m_vSunDir;
}

Vec3 C3DEngine::GetRealtimeSunDirNormalized() const
{
	return m_vSunDirRealtime;
}

Vec3 C3DEngine::GetAmbientColorFromPosition(const Vec3& vPos, float fRadius)
{
	Vec3 ret(0, 0, 0);

	if (m_pObjManager)
	{
		if (CVisArea* pVisArea = (CVisArea*)m_pVisAreaManager->GetVisAreaFromPos(vPos))
			ret = pVisArea->GetFinalAmbientColor();
		else
			ret = Get3DEngine()->GetSkyColor();

		assert(ret.x >= 0 && ret.y >= 0 && ret.z >= 0);
	}

	return ret;
}

void C3DEngine::FreeRenderNodeState(IRenderNode* pEnt)
{
	// make sure we don't try to update the streaming priority if an object
	// was added and removed in the same frame
	int nElementID = m_deferredRenderProxyStreamingPriorityUpdates.Find(pEnt);
	if (nElementID != -1)
		m_deferredRenderProxyStreamingPriorityUpdates.DeleteFastUnsorted(nElementID);

	m_pObjManager->RemoveFromRenderAllObjectDebugInfo(pEnt);

#if !defined(_RELEASE)
	if (!gEnv->IsDedicated())
	{
		//As render nodes can be deleted in many places, it's possible that the map of render nodes used by stats gathering (r_stats 6, perfHUD, debug gun, Statoscope) could get aliased.
		//Ensure that this node is removed from the map to prevent a dereference after deletion.
		gEnv->pRenderer->ForceRemoveNodeFromDrawCallsMap(pEnt);
	}
#endif

	m_lstAlwaysVisible.Delete(pEnt);

	if (m_pDecalManager && (pEnt->m_nInternalFlags & IRenderNode::DECAL_OWNER))
		m_pDecalManager->OnEntityDeleted(pEnt);

	if (pEnt->GetRenderNodeType() == eERType_Light && GetRenderer())
		GetRenderer()->OnEntityDeleted(pEnt);

	if (pEnt->GetRndFlags() & (ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS))
	{
		// make sure pointer to object will not be used somewhere in the renderer
#if !defined(_RELEASE)
		if (!(pEnt->GetRndFlags() & ERF_HAS_CASTSHADOWMAPS))
			Warning("IRenderNode has ERF_CASTSHADOWMAPS set but not ERF_HAS_CASTSHADOWMAPS, name: '%s', class: '%s'.", pEnt->GetName(), pEnt->GetEntityClassName());
#endif
		Get3DEngine()->OnCasterDeleted(pEnt);
	}

	UnRegisterEntityImpl(pEnt);

	if (pEnt->m_pTempData)
	{
		Get3DEngine()->FreeRenderNodeTempData(&pEnt->m_pTempData);
		assert(!pEnt->m_pTempData);
	}
}

const char* C3DEngine::GetLevelFilePath(const char* szFileName)
{
	cry_strcpy(m_sGetLevelFilePathTmpBuff, m_szLevelFolder);
	if (*szFileName && (*szFileName == '/' || *szFileName == '\\'))
		cry_strcat(m_sGetLevelFilePathTmpBuff, szFileName + 1);
	else
		cry_strcat(m_sGetLevelFilePathTmpBuff, szFileName);
	return m_sGetLevelFilePathTmpBuff;
}

void C3DEngine::SetTerrainBurnedOut(int x, int y, bool bBurnedOut)
{
	assert(!"not supported");
}

bool C3DEngine::IsTerrainBurnedOut(int x, int y)
{
	assert(!"not supported");
	return 0;//m_pTerrain ? m_pTerrain->IsBurnedOut(x, y) : 0;
}

int C3DEngine::GetTerrainSectorSize()
{
	return m_pTerrain ? m_pTerrain->GetSectorSize() : 0;
}

void C3DEngine::ActivatePortal(const Vec3& vPos, bool bActivate, const char* szEntityName)
{
	if (m_pVisAreaManager)
		m_pVisAreaManager->ActivatePortal(vPos, bActivate, szEntityName);
}

bool C3DEngine::SetStatInstGroup(int nGroupId, const IStatInstGroup& siGroup, int nSID)
{
	assert(nSID >= 0 && nSID < m_pObjManager->m_lstStaticTypes.Count());

	m_fRefreshSceneDataCVarsSumm = -100;

	m_pObjManager->m_lstStaticTypes[nSID].resize(max(nGroupId + 1, m_pObjManager->m_lstStaticTypes[nSID].Count()));

	if (nGroupId < 0 || nGroupId >= m_pObjManager->m_lstStaticTypes[nSID].Count())
		return false;

	StatInstGroup& rGroup = m_pObjManager->m_lstStaticTypes[nSID][nGroupId];

	rGroup.pStatObj = siGroup.pStatObj;

	if (rGroup.pStatObj)
		cry_strcpy(rGroup.szFileName, siGroup.pStatObj->GetFilePath());
	else
		rGroup.szFileName[0] = 0;

	rGroup.bHideability = siGroup.bHideability;
	rGroup.bHideabilitySecondary = siGroup.bHideabilitySecondary;
	rGroup.nPlayerHideable = siGroup.nPlayerHideable;
	rGroup.bPickable = siGroup.bPickable;
	rGroup.fBending = siGroup.fBending;
	rGroup.nCastShadowMinSpec = siGroup.nCastShadowMinSpec;
	rGroup.bRecvShadow = siGroup.bRecvShadow;
	rGroup.bDynamicDistanceShadows = siGroup.bDynamicDistanceShadows;

	rGroup.bUseAlphaBlending = siGroup.bUseAlphaBlending;
	rGroup.fSpriteDistRatio = siGroup.fSpriteDistRatio;
	rGroup.fLodDistRatio = siGroup.fLodDistRatio;
	rGroup.fShadowDistRatio = siGroup.fShadowDistRatio;
	rGroup.fMaxViewDistRatio = siGroup.fMaxViewDistRatio;

	rGroup.fBrightness = siGroup.fBrightness;

	_smart_ptr<IMaterial> pMaterial = rGroup.pMaterial;
	rGroup.pMaterial = siGroup.pMaterial;

	rGroup.bUseSprites = siGroup.bUseSprites;

	rGroup.fDensity = siGroup.fDensity;
	rGroup.fElevationMax = siGroup.fElevationMax;
	rGroup.fElevationMin = siGroup.fElevationMin;
	rGroup.fSize = siGroup.fSize;
	rGroup.fSizeVar = siGroup.fSizeVar;
	rGroup.fSlopeMax = siGroup.fSlopeMax;
	rGroup.fSlopeMin = siGroup.fSlopeMin;
	rGroup.fStiffness = siGroup.fStiffness;
	rGroup.fDamping = siGroup.fDamping;
	rGroup.fVariance = siGroup.fVariance;
	rGroup.fAirResistance = siGroup.fAirResistance;

	rGroup.bRandomRotation = siGroup.bRandomRotation;
	rGroup.nRotationRangeToTerrainNormal = siGroup.nRotationRangeToTerrainNormal;
	rGroup.nMaterialLayers = siGroup.nMaterialLayers;

	rGroup.bUseTerrainColor = siGroup.bUseTerrainColor && Get3DEngine()->m_bShowTerrainSurface;
	rGroup.bAllowIndoor = siGroup.bAllowIndoor;
	rGroup.fAlignToTerrainCoefficient = Get3DEngine()->m_bShowTerrainSurface ? siGroup.fAlignToTerrainCoefficient : 0.f;
	rGroup.bAutoMerged = siGroup.bAutoMerged;
	rGroup.minConfigSpec = siGroup.minConfigSpec;

	rGroup.nID = siGroup.nID;

	rGroup.Update(GetCVars(), Get3DEngine()->GetGeomDetailScreenRes());

	if (CTerrain* const pTerrain = GetTerrain())
	{
		pTerrain->MarkAllSectorsAsUncompiled(nSID);
	}

	if (gEnv->IsEditor() && pMaterial != rGroup.pMaterial)
		m_pMergedMeshesManager->ResetActiveNodes();

	MarkRNTmpDataPoolForReset();

	return true;
}

bool C3DEngine::GetStatInstGroup(int nGroupId, IStatInstGroup& siGroup, int nSID)
{
	assert(nSID >= 0 && nSID < m_pObjManager->m_lstStaticTypes.Count());

	if (nGroupId < 0 || nGroupId >= m_pObjManager->m_lstStaticTypes[nSID].Count())
		return false;

	StatInstGroup& rGroup = m_pObjManager->m_lstStaticTypes[nSID][nGroupId];

	siGroup.pStatObj = rGroup.pStatObj;
	if (siGroup.pStatObj)
		cry_strcpy(siGroup.szFileName, rGroup.pStatObj->GetFilePath());

	siGroup.bHideability = rGroup.bHideability;
	siGroup.bHideabilitySecondary = rGroup.bHideabilitySecondary;
	siGroup.nPlayerHideable = rGroup.nPlayerHideable;
	siGroup.bPickable = rGroup.bPickable;
	siGroup.fBending = rGroup.fBending;
	siGroup.nCastShadowMinSpec = rGroup.nCastShadowMinSpec;
	siGroup.bRecvShadow = rGroup.bRecvShadow;
	siGroup.bDynamicDistanceShadows = rGroup.bDynamicDistanceShadows;

	siGroup.bUseAlphaBlending = rGroup.bUseAlphaBlending;
	siGroup.fSpriteDistRatio = rGroup.fSpriteDistRatio;
	siGroup.fLodDistRatio = rGroup.fLodDistRatio;
	siGroup.fShadowDistRatio = rGroup.fShadowDistRatio;
	siGroup.fMaxViewDistRatio = rGroup.fMaxViewDistRatio;

	siGroup.fBrightness = rGroup.fBrightness;
	siGroup.pMaterial = rGroup.pMaterial;
	siGroup.bUseSprites = rGroup.bUseSprites;

	siGroup.fDensity = rGroup.fDensity;
	siGroup.fElevationMax = rGroup.fElevationMax;
	siGroup.fElevationMin = rGroup.fElevationMin;
	siGroup.fSize = rGroup.fSize;
	siGroup.fSizeVar = rGroup.fSizeVar;
	siGroup.fSlopeMax = rGroup.fSlopeMax;
	siGroup.fSlopeMin = rGroup.fSlopeMin;
	siGroup.bAutoMerged = rGroup.bAutoMerged;

	siGroup.fStiffness = rGroup.fStiffness;
	siGroup.fDamping = rGroup.fDamping;
	siGroup.fVariance = rGroup.fVariance;
	siGroup.fAirResistance = rGroup.fAirResistance;

	siGroup.nID = rGroup.nID;

	return true;
}

void C3DEngine::UpdateStatInstGroups()
{
	if (!m_pObjManager)
		return;

	for (uint32 nSID = 0; nSID < m_pObjManager->m_lstStaticTypes.size(); nSID++)
	{
		PodArray<StatInstGroup>& rGroupTable = m_pObjManager->m_lstStaticTypes[nSID];
		for (uint32 nGroupId = 0; nGroupId < rGroupTable.size(); nGroupId++)
		{
			StatInstGroup& rGroup = rGroupTable[nGroupId];
			rGroup.Update(GetCVars(), Get3DEngine()->GetGeomDetailScreenRes());
		}
	}
}

void C3DEngine::GetMemoryUsage(class ICrySizer* pSizer) const
{

#if !defined(_LIB) // Only when compiling as dynamic library
	//	{
	//SIZER_COMPONENT_NAME(pSizer,"Strings");
	//pSizer->AddObject( (this+1),string::_usedMemory(0) );
	//	}
	{
		SIZER_COMPONENT_NAME(pSizer, "STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule(&meminfo);
		pSizer->AddObject((this + 2), (size_t)meminfo.STL_wasted);
	}
#endif

	pSizer->AddObject(this, sizeof(*this) + (GetCVars()->e_StreamCgfDebug ? 100 * 1024 * 1024 : 0));
	pSizer->AddObject(m_pCVars);

	pSizer->AddObject(m_lstDynLights);
	pSizer->AddObject(m_lstDynLightsNoLight);
	pSizer->AddObject(m_lstStaticLights);
	pSizer->AddObject(m_lstAffectingLightsCombinations);
	pSizer->AddObject(m_arrLightProjFrustums);
	pSizer->AddObject(m_arrEntsInFoliage);

	pSizer->AddObject(m_lstRoadRenderNodesForUpdate);

	pSizer->AddObject(m_lstKilledVegetations);
	pSizer->AddObject(arrFPSforSaveLevelStats);
	pSizer->AddObject(m_lstAlwaysVisible);

	if (CTemporaryPool* pPool = CTemporaryPool::Get())
	{
		SIZER_COMPONENT_NAME(pSizer, "Temporary Pool");
		pPool->GetMemoryUsage(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "RenderMeshMerger");
		m_pRenderMeshMerger->GetMemoryUsage(pSizer);
	}

	CRoadRenderNode::GetMemoryUsageStatic(pSizer);

	{
		SIZER_COMPONENT_NAME(pSizer, "Particles");
		pSizer->AddObject(m_pPartManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "ParticleSystem");
		pSizer->AddObject(m_pParticleSystem);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "Optics");
		pSizer->AddObject(m_pOpticsManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "SkyLightManager");
		pSizer->AddObject(m_pSkyLightManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "DecalManager");
		pSizer->AddObject(m_pDecalManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "OutdoorObjectsTree");
		pSizer->AddObject(m_pObjectsTree);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "ObjManager");
		pSizer->AddObject(m_pObjManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "Terrain");
		pSizer->AddObject(m_pTerrain);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "VisAreas");
		pSizer->AddObject(m_pVisAreaManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "ClipVolumes");
		pSizer->AddObject(m_pClipVolumeManager);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "ProcVegetPool");
		pSizer->AddObject(CTerrainNode::GetProcObjPoolMan());
		pSizer->AddObject(CTerrainNode::GetProcObjChunkPool());
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "LayersBaseTextureData");
		pSizer->AddObject(m_arrBaseTextureData);
	}
}

void C3DEngine::GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& cstAABB)
{
	//////////////////////////////////////////////////////////////////////////
	std::vector<IRenderNode*> cFoundRenderNodes;
	unsigned int nFoundObjects(0);

	nFoundObjects = GetObjectsInBox(cstAABB);
	cFoundRenderNodes.resize(nFoundObjects, NULL);
	GetObjectsInBox(cstAABB, &cFoundRenderNodes.front());

	size_t nCurrentRenderNode(0);
	size_t nTotalNumberOfRenderNodes(0);

	nTotalNumberOfRenderNodes = cFoundRenderNodes.size();
	for (nCurrentRenderNode = 0; nCurrentRenderNode < nTotalNumberOfRenderNodes; ++nCurrentRenderNode)
	{
		IRenderNode*& piRenderNode = cFoundRenderNodes[nCurrentRenderNode];

		IMaterial* piMaterial(piRenderNode->GetMaterialOverride());
		if (!piMaterial)
		{
			piMaterial = piRenderNode->GetMaterial();
		}

		if (piMaterial)
		{
			piMaterial->GetResourceMemoryUsage(pSizer);
		}

		{
			IRenderMesh* piMesh(NULL);
			size_t nCount(0);

			piMesh = piRenderNode->GetRenderMesh(nCount);
			for (; piMesh; ++nCount, piMesh = piRenderNode->GetRenderMesh(nCount))
			{
				// Timur, RenderMesh may not be loaded due to streaming!
				piMesh->GetMemoryUsage(pSizer, IRenderMesh::MEM_USAGE_COMBINED);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (m_pTerrain)
	{
		CTerrainNode* poTerrainNode = m_pTerrain->FindMinNodeContainingBox(cstAABB, GetDefSID());
		if (poTerrainNode)
		{
			poTerrainNode->GetResourceMemoryUsage(pSizer, cstAABB);
		}
	}
}

bool C3DEngine::IsUnderWater(const Vec3& vPos) const
{
	bool bUnderWater = false;
	for (IPhysicalEntity* pArea = 0; pArea = GetPhysicalWorld()->GetNextArea(pArea); )
	{
		if (bUnderWater)
			// Must finish iteration to unlock areas.
			continue;

		pe_params_buoyancy buoy;
		if (pArea->GetParams(&buoy) && buoy.iMedium == 0)
		{
			if (!is_unused(buoy.waterPlane.n))
			{
				if (buoy.waterPlane.n * vPos < buoy.waterPlane.n * buoy.waterPlane.origin)
				{
					pe_status_contains_point st;
					st.pt = vPos;
					if (pArea->GetStatus(&st))
						bUnderWater = true;
				}
			}
		}
	}
	return bUnderWater;
}

void C3DEngine::SetOceanRenderFlags(uint8 nFlags)
{
	m_nOceanRenderFlags = nFlags;
}

uint32 C3DEngine::GetOceanVisiblePixelsCount() const
{
	return COcean::GetVisiblePixelsCount();
}

float C3DEngine::GetBottomLevel(const Vec3& referencePos, float maxRelevantDepth, int objflags)
{
	FUNCTION_PROFILER_3DENGINE;

	const float terrainWorldZ = GetTerrainElevation(referencePos.x, referencePos.y);

	const float padding = 0.2f;
	float rayLength;

	// NOTE: Terrain is above referencePos, so referencePos is probably inside a voxel or something.
	if (terrainWorldZ <= referencePos.z)
		rayLength = min(maxRelevantDepth, (referencePos.z - terrainWorldZ));
	else
		rayLength = maxRelevantDepth;

	rayLength += padding * 2.0f;

	ray_hit hit;
	int rayFlags = geom_colltype_player << rwi_colltype_bit | rwi_stop_at_pierceable;
	if (m_pPhysicalWorld->RayWorldIntersection(referencePos + Vec3(0, 0, padding), Vec3(0, 0, -rayLength),
	                                           ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid, rayFlags, &hit, 1))
	{
		return hit.pt.z;
	}

	// Terrain was above or too far below referencePos, and no solid object was close enough.
	return BOTTOM_LEVEL_UNKNOWN;
}

float C3DEngine::GetBottomLevel(const Vec3& referencePos, float maxRelevantDepth /* = 10.0f */)
{
	return GetBottomLevel(referencePos, maxRelevantDepth, ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid);
}

float C3DEngine::GetBottomLevel(const Vec3& referencePos, int objflags)
{
	return GetBottomLevel(referencePos, 10.0f, objflags);
}

#if defined(USE_GEOM_CACHES)
IGeomCache* C3DEngine::LoadGeomCache(const char* szFileName)
{
	if (!szFileName || !szFileName[0])
	{
		m_pSystem->Warning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, 0, 0, "I3DEngine::LoadGeomCache: filename is not specified");
		return 0;
	}

	return m_pGeomCacheManager->LoadGeomCache(szFileName);
}

IGeomCache* C3DEngine::FindGeomCacheByFilename(const char* szFileName)
{
	if (!szFileName || szFileName[0] == 0)
	{
		return NULL;
	}

	return m_pGeomCacheManager->FindGeomCacheByFilename(szFileName);
}
#endif

namespace
{
// The return value is the next position of the source buffer.
int ReadFromBuffer(const char* pSource, int nSourceSize, int nSourcePos, void* pDest, int nDestSize)
{
	if (pDest == 0 || nDestSize == 0)
		return 0;

	if (nSourcePos < 0 || nSourcePos >= nSourceSize)
		return 0;

	memcpy(pDest, &pSource[nSourcePos], nDestSize);

	return nSourcePos + nDestSize;
}
}

//! Load an IStatObj for the engine based on the saved version. Save function is in
//! Code\Sandbox\Plugins\CryDesigner\Core\ModelCompiler.cpp, function ModelCompiler::SaveMesh
IStatObj* C3DEngine::LoadDesignerObject(int nVersion, const char* szBinaryStream, int size)
{
	if (nVersion < 0 || nVersion > 2)
		return NULL;

	int nBufferPos = 0;
	int32 nSubObjectCount = 0;
	nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nSubObjectCount, sizeof(int32));

	IStatObj* pStatObj = gEnv->p3DEngine->CreateStatObj();
	if (!pStatObj)
		return NULL;

	std::vector<IStatObj*> statObjList;
	if (nSubObjectCount > 0)
	{
		for (int i = 0; i < nSubObjectCount; ++i)
		{
			pStatObj->AddSubObject(gEnv->p3DEngine->CreateStatObj());
			statObjList.push_back(pStatObj->GetSubObject(i)->pStatObj);
		}
		pStatObj->GetIndexedMesh()->FreeStreams();
	}
	else
	{
		statObjList.push_back(pStatObj);
	}

	if (nVersion == 2)
	{
		int32 nStaticObjFlags = 0;
		nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nStaticObjFlags, sizeof(int32));
		pStatObj->SetFlags(nStaticObjFlags);
	}

	AABB statObj_bbox(AABB::RESET);

	for (int k = 0, iCount(statObjList.size()); k < iCount; ++k)
	{
		int32 nPositionCount = 0;
		int32 nTexCoordCount = 0;
		int32 nFaceCount = 0;
		int32 nIndexCount = 0;
		int32 nTangentCount = 0;
		int32 nSubsetCount = 0;

		nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nPositionCount, sizeof(int32));
		nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nTexCoordCount, sizeof(int32));
		if (nPositionCount <= 0 || nTexCoordCount <= 0)
			return NULL;

		if (nVersion == 0)
		{
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nFaceCount, sizeof(int32));
			if (nFaceCount <= 0)
				return NULL;
		}
		else if (nVersion >= 1)
		{
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nIndexCount, sizeof(int32));
			if (nIndexCount <= 0)
				return NULL;
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nTangentCount, sizeof(int32));
		}
		nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nSubsetCount, sizeof(int32));
		IIndexedMesh* pMesh = statObjList[k]->GetIndexedMesh();
		if (!pMesh)
			return NULL;

		pMesh->FreeStreams();
		pMesh->SetVertexCount((int)nPositionCount);
		pMesh->SetFaceCount((int)nFaceCount);
		pMesh->SetIndexCount((int)nIndexCount);
		pMesh->SetTexCoordCount((int)nTexCoordCount);

		Vec3* const positions = pMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::POSITIONS);
		Vec3* const normals = pMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::NORMALS);
		SMeshTexCoord* const texcoords = pMesh->GetMesh()->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);

		nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, positions, sizeof(Vec3) * nPositionCount);
		nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, normals, sizeof(Vec3) * nPositionCount);
		nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, texcoords, sizeof(SMeshTexCoord) * nTexCoordCount);
		if (nVersion == 0)
		{
			SMeshFace* const faces = pMesh->GetMesh()->GetStreamPtr<SMeshFace>(CMesh::FACES);
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, faces, sizeof(SMeshFace) * nFaceCount);
		}
		else if (nVersion >= 1)
		{
			vtx_idx* const indices = pMesh->GetMesh()->GetStreamPtr<vtx_idx>(CMesh::INDICES);
			if (sizeof(vtx_idx) == sizeof(uint16))
			{
				nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, indices, sizeof(uint16) * nIndexCount);
			}
			else
			{
				uint16* indices16 = new uint16[nIndexCount];
				nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, indices16, sizeof(uint16) * nIndexCount);
				for (int32 i = 0; i < nIndexCount; ++i)
					indices[i] = (vtx_idx)indices16[i];
				delete[] indices16;
			}
			pMesh->SetTangentCount((int)nTangentCount);
			if (nTangentCount > 0)
			{
				SMeshTangents* const tangents = pMesh->GetMesh()->GetStreamPtr<SMeshTangents>(CMesh::TANGENTS);
				nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, tangents, sizeof(SMeshTangents) * nTangentCount);
			}
		}

		// AABB calculation
#if 0
		if (nVersion < 3)
#endif
		{
			// No BB saved for this version, so we need to recalculate manually
			pMesh->CalcBBox();

			statObjList[k]->SetBBoxMin(pMesh->GetBBox().min);
			statObjList[k]->SetBBoxMax(pMesh->GetBBox().max);
		}
		// optimization for AABB loading - keep ifdef temporarily to keep volatile changes for after GDC16
#if 0
		else
		{
			// for version 3 we saved the result in the file, so we can load it directly instead of iterating through the mesh
			Vec3 minBB, maxBB;
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &minBB, sizeof(Vec3));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &maxBB, sizeof(Vec3));

			statObjList[k]->SetBBoxMin(minBB);
			statObjList[k]->SetBBoxMax(maxBB);
		}
#endif

		statObj_bbox.Add(statObjList[k]->GetAABB());

		// subset loading
		pMesh->SetSubSetCount(nSubsetCount);

		for (int i = 0; i < nSubsetCount; ++i)
		{
			SMeshSubset subset;
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &subset.vCenter, sizeof(Vec3));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &subset.fRadius, sizeof(float));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &subset.fTexelDensity, sizeof(float));

			int32 nFirstIndexId = 0;
			int32 nNumIndices = 0;
			int32 nFirstVertId = 0;
			int32 nNumVerts = 0;
			int32 nMatID = 0;
			int32 nMatFlags = 0;
			int32 nPhysicalizeType = 0;

			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nFirstIndexId, sizeof(int32));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nNumIndices, sizeof(int32));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nFirstVertId, sizeof(int32));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nNumVerts, sizeof(int32));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nMatID, sizeof(int32));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nMatFlags, sizeof(int32));
			nBufferPos = ReadFromBuffer(szBinaryStream, size, nBufferPos, &nPhysicalizeType, sizeof(int32));

			pMesh->SetSubsetBounds(i, subset.vCenter, subset.fRadius);
			pMesh->SetSubsetIndexVertexRanges(i, (int)nFirstIndexId, (int)nNumIndices, (int)nFirstVertId, (int)nNumVerts);
			pMesh->SetSubsetMaterialId(i, nMatID == -1 ? 0 : (int)nMatID);
			pMesh->SetSubsetMaterialProperties(i, (int)nMatFlags, (int)nPhysicalizeType);
		}

		if (nVersion == 0)
		{
#if CRY_PLATFORM_WINDOWS
			pMesh->Optimize();
#endif
		}

		statObjList[k]->Invalidate(true);
	}
	pStatObj->SetBBoxMin(statObj_bbox.min);
	pStatObj->SetBBoxMax(statObj_bbox.max);

	return pStatObj;
}

float C3DEngine::GetWaterLevel(const Vec3* pvPos, IPhysicalEntity* pent, bool bAccurate)
{
	FUNCTION_PROFILER_3DENGINE;

	bool bInVisarea = m_pVisAreaManager && m_pVisAreaManager->GetVisAreaFromPos(*pvPos) != 0;

	Vec3 gravity;
	pe_params_buoyancy pb[4];
	int i, nBuoys = m_pPhysicalWorld->CheckAreas(*pvPos, gravity, pb, 4, -1, ZERO, pent);

	float max_level = (!bInVisarea) ? (bAccurate ? GetAccurateOceanHeight(*pvPos) : GetWaterLevel()) : WATER_LEVEL_UNKNOWN;

	for (i = 0; i < nBuoys; i++)
	{
		if (pb[i].iMedium == 0 && (!bInVisarea || fabs_tpl(pb[i].waterPlane.origin.x) + fabs_tpl(pb[i].waterPlane.origin.y) > 0))
		{
			float fVolumeLevel = pvPos->z - pb[i].waterPlane.n.z * (pb[i].waterPlane.n * (*pvPos - pb[i].waterPlane.origin));

			// it's not ocean
			if (pb[i].waterPlane.origin.y + pb[i].waterPlane.origin.x != 0.0f)
				max_level = max(max_level, fVolumeLevel);
		}
	}

	return max(WATER_LEVEL_UNKNOWN, max_level);
}

void C3DEngine::SetShadowsGSMCache(bool bCache)
{
	if (bCache)
		m_nGsmCache = m_pConsole->GetCVar("r_ShadowsCache")->GetIVal();
	else
		m_nGsmCache = 0;
}

float C3DEngine::GetAccurateOceanHeight(const Vec3& pCurrPos) const
{
	FUNCTION_PROFILER_3DENGINE;

	static int nFrameID = -1;
	const int nEngineFrameID = gEnv->nMainFrameID;
	static float fWaterLevel = 0;
	if (nFrameID != nEngineFrameID && m_pTerrain)
	{
		fWaterLevel = m_pTerrain->GetWaterLevel();
		nFrameID = nEngineFrameID;
	}

	const float waterLevel = fWaterLevel + COcean::GetWave(pCurrPos, gEnv->nMainFrameID);
	return waterLevel;
}

Vec4 C3DEngine::GetCausticsParams() const
{
	return Vec4(m_oceanCausticsTilling, m_oceanCausticsDistanceAtten, m_oceanCausticsMultiplier, 1.0);
}

Vec4 C3DEngine::GetOceanAnimationCausticsParams() const
{
	return Vec4(1, m_oceanCausticHeight, m_oceanCausticDepth, m_oceanCausticIntensity);
}

void C3DEngine::GetHDRSetupParams(Vec4 pParams[5]) const
{
	pParams[0] = m_vHDRFilmCurveParams;
	pParams[1] = Vec4(m_fHDRBloomAmount * 0.3f, m_fHDRBloomAmount * 0.3f, m_fHDRBloomAmount * 0.3f, m_fGrainAmount);
	pParams[2] = Vec4(m_vColorBalance, m_fHDRSaturation);
	pParams[3] = Vec4(m_vHDREyeAdaptation, 1.0f);
	pParams[4] = Vec4(m_vHDREyeAdaptationLegacy, 1.0f);
};

void C3DEngine::GetOceanAnimationParams(Vec4& pParams0, Vec4& pParams1) const
{
	static int nFrameID = -1;
	int nCurrFrameID = gEnv->nMainFrameID;

	static Vec4 s_pParams0 = Vec4(0, 0, 0, 0);
	static Vec4 s_pParams1 = Vec4(0, 0, 0, 0);

	if (nFrameID != nCurrFrameID && m_pTerrain)
	{
		sincos_tpl(m_oceanWindDirection, &s_pParams1.y, &s_pParams1.z);

		s_pParams0 = Vec4(m_oceanWindDirection, m_oceanWindSpeed, m_oceanWavesSpeed, m_oceanWavesAmount);
		s_pParams1.x = m_oceanWavesSize;
		s_pParams1.w = m_pTerrain->GetWaterLevel();

		nFrameID = nCurrFrameID;
	}

	pParams0 = s_pParams0;
	pParams1 = s_pParams1;
};

IVisArea* C3DEngine::CreateVisArea(uint64 visGUID)
{
	return m_pObjManager ? m_pVisAreaManager->CreateVisArea(visGUID) : 0;
}

void C3DEngine::DeleteVisArea(IVisArea* pVisArea)
{

	if (!m_pVisAreaManager->IsValidVisAreaPointer((CVisArea*)pVisArea))
	{
		Warning("I3DEngine::DeleteVisArea: Invalid VisArea pointer");
		return;
	}
	if (m_pObjManager)
	{
		CVisArea* pArea = (CVisArea*)pVisArea;

		//		if(pArea->m_pObjectsTree)
		//		pArea->m_pObjectsTree->FreeAreaBrushes(true);

		PodArray<SRNInfo> lstEntitiesInArea;
		if (pArea->m_pObjectsTree)
			pArea->m_pObjectsTree->MoveObjectsIntoList(&lstEntitiesInArea, NULL);

		// unregister from indoor
		for (int i = 0; i < lstEntitiesInArea.Count(); i++)
			Get3DEngine()->UnRegisterEntityDirect(lstEntitiesInArea[i].pNode);

		if (pArea->m_pObjectsTree)
			assert(pArea->m_pObjectsTree->GetObjectsCount(eMain) == 0);

		m_pVisAreaManager->DeleteVisArea((CVisArea*)pVisArea);

		for (int i = 0; i < lstEntitiesInArea.Count(); i++)
			Get3DEngine()->RegisterEntity(lstEntitiesInArea[i].pNode);
	}
}

void C3DEngine::UpdateVisArea(IVisArea* pVisArea, const Vec3* pPoints, int nCount, const char* szName,
                              const SVisAreaInfo& info, bool bReregisterObjects)
{
	if (!m_pObjManager)
		return;

	CVisArea* pArea = (CVisArea*)pVisArea;

	//	PrintMessage("C3DEngine::UpdateVisArea: %s", szName);

	Vec3 vTotalBoxMin = pArea->m_boxArea.min;
	Vec3 vTotalBoxMax = pArea->m_boxArea.max;

	m_pVisAreaManager->UpdateVisArea((CVisArea*)pVisArea, pPoints, nCount, szName, info);

	if (((CVisArea*)pVisArea)->m_pObjectsTree && ((CVisArea*)pVisArea)->m_pObjectsTree->GetObjectsCount(eMain))
	{
		// merge old and new bboxes
		vTotalBoxMin.CheckMin(pArea->m_boxArea.min);
		vTotalBoxMax.CheckMax(pArea->m_boxArea.max);
	}
	else
	{
		vTotalBoxMin = pArea->m_boxArea.min;
		vTotalBoxMax = pArea->m_boxArea.max;
	}

	if (bReregisterObjects)
		m_pObjManager->ReregisterEntitiesInArea(vTotalBoxMin - Vec3(8, 8, 8), vTotalBoxMax + Vec3(8, 8, 8));
}

IClipVolume* C3DEngine::CreateClipVolume()
{
	return m_pClipVolumeManager->CreateClipVolume();
}

void C3DEngine::DeleteClipVolume(IClipVolume* pClipVolume)
{
	m_pClipVolumeManager->DeleteClipVolume(pClipVolume);
}

void C3DEngine::UpdateClipVolume(IClipVolume* pClipVolume, _smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, bool bActive, uint32 flags, const char* szName)
{
	m_pClipVolumeManager->UpdateClipVolume(pClipVolume, pRenderMesh, pBspTree, worldTM, bActive, flags, szName);
}

void C3DEngine::ResetParticlesAndDecals()
{
	if (m_pPartManager)
		m_pPartManager->Reset();
	if (m_pParticleSystem)
		m_pParticleSystem->Reset();

	if (m_pDecalManager)
		m_pDecalManager->Reset();

	CStatObjFoliage* pFoliage, * pFoliageNext;
	for (pFoliage = m_pFirstFoliage; &pFoliage->m_next != &m_pFirstFoliage; pFoliage = pFoliageNext)
	{
		pFoliageNext = pFoliage->m_next;
		delete pFoliage;
	}

	ReRegisterKilledVegetationInstances();
}

IRenderNode* C3DEngine::CreateRenderNode(EERType type)
{
	switch (type)
	{
	case eERType_Brush:
		{
			CBrush* pBrush = new CBrush();
			return pBrush;
		}
	case eERType_Vegetation:
		{
			CVegetation* pVeget = new CVegetation();
			return pVeget;
		}
	case eERType_Cloud:
		{
			CCloudRenderNode* pCloud = new CCloudRenderNode();
			return pCloud;
		}
	case eERType_FogVolume:
		{
			CFogVolumeRenderNode* pFogVolume = new CFogVolumeRenderNode();
			return pFogVolume;
		}
	case eERType_Road:
		{
			CRoadRenderNode* pRoad = new CRoadRenderNode();
			return pRoad;
		}
	case eERType_Decal:
		{
			CDecalRenderNode* pDecal = new CDecalRenderNode();
			return pDecal;
		}
	case eERType_WaterVolume:
		{
			CWaterVolumeRenderNode* pWaterVolume = new CWaterVolumeRenderNode();
			return pWaterVolume;
		}
	case eERType_WaterWave:
		{
			CWaterWaveRenderNode* pRenderNode = new CWaterWaveRenderNode();
			return pRenderNode;
		}
	case eERType_DistanceCloud:
		{
			CDistanceCloudRenderNode* pRenderNode = new CDistanceCloudRenderNode();
			return pRenderNode;
		}
	case eERType_Rope:
		{
			IRopeRenderNode* pRenderNode = new CRopeRenderNode();
			return pRenderNode;
		}
	case eERType_VolumeObject:
		{
			IVolumeObjectRenderNode* pRenderNode = new CVolumeObjectRenderNode();
			return pRenderNode;
		}
#if !defined(EXCLUDE_DOCUMENTATION_PURPOSE)
	case eERType_PrismObject:
		{
			IPrismRenderNode* pRenderNode = new CPrismRenderNode();
			return pRenderNode;
		}
#endif // EXCLUDE_DOCUMENTATION_PURPOSE

	case eERType_BreakableGlass:
		{
			IBreakableGlassRenderNode* pRenderNode = new CBreakableGlassRenderNode();
			return pRenderNode;
		}

	case eERType_MergedMesh:
		{
			IRenderNode* pRenderNode = new CMergedMeshRenderNode();
			return pRenderNode;
		}

#if defined(USE_GEOM_CACHES)
	case eERType_GeomCache:
		{
			IRenderNode* pRenderNode = new CGeomCacheRenderNode();
			return pRenderNode;
		}
#endif
	}
	assert(!"C3DEngine::CreateRenderNode: Specified node type is not supported.");
	return 0;
}

void C3DEngine::DeleteRenderNode(IRenderNode* pRenderNode)
{
	UnRegisterEntityDirect(pRenderNode);
	delete pRenderNode;
}

void C3DEngine::SetWind(const Vec3& vWind)
{
	m_vWindSpeed = vWind;
	if (!m_vWindSpeed.IsZero())
	{
		// Maintain a large physics area for global wind.
		if (!m_pGlobalWind)
		{
			primitives::sphere geomSphere;
			geomSphere.center = Vec3(0);
			geomSphere.r = 1e7f;
			IGeometry* pGeom = m_pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &geomSphere);
			m_pGlobalWind = GetPhysicalWorld()->AddArea(pGeom, Vec3(ZERO), Quat(IDENTITY), 1.0f);

			pe_params_foreign_data fd;
			fd.iForeignFlags = PFF_OUTDOOR_AREA;
			m_pGlobalWind->SetParams(&fd);
		}

		// Set medium half-plane above water level. Set standard air density.
		pe_params_buoyancy pb;
		pb.iMedium = 1;
		pb.waterPlane.n.Set(0, 0, -1);
		pb.waterPlane.origin.Set(0, 0, GetWaterLevel());
		pb.waterFlow = m_vWindSpeed;
		pb.waterResistance = 1;
		pb.waterDensity = 1;
		m_pGlobalWind->SetParams(&pb);
	}
	else if (m_pGlobalWind)
	{
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pGlobalWind);
		m_pGlobalWind = 0;
	}
}

Vec3 C3DEngine::GetWind(const AABB& box, bool bIndoors) const
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pCVars->e_Wind)
		return Vec3(ZERO);

	Vec3 wind(0, 0, 0);
	SampleWind(&wind, 1, box, bIndoors);
	return wind;
}

Vec3 C3DEngine::GetGlobalWind(bool bIndoors) const
{
	// We assume indoor wind is zero.
	return (!m_pCVars->e_Wind || bIndoors) ? Vec3Constants<f32>::fVec3_Zero : m_vWindSpeed;
}

// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
inline void ComputerBarycentric2d(Vec2& p, Vec2& a, Vec2& b, Vec2& c, float& u, float& v, float& w)
{
	Vec2 v0 = b - a, v1 = c - a, v2 = p - a;
	float d00 = v0.Dot(v0);
	float d01 = v0.Dot(v1);
	float d11 = v1.Dot(v1);
	float d20 = v2.Dot(v0);
	float d21 = v2.Dot(v1);
	float invDenom = 1.0f / (d00 * d11 - d01 * d01);
	v = (d11 * d20 - d01 * d21) * invDenom;
	w = (d00 * d21 - d01 * d20) * invDenom;
	u = 1.0f - v - w;
}

bool C3DEngine::SampleWind(Vec3* pSamples, int nSamples, const AABB& volume, bool bIndoors) const
{
	if (!m_pCVars->e_Wind || nSamples == 0)
		return false;

	FUNCTION_PROFILER_3DENGINE;

	// Start with global wind.
	Vec3 vWind = GetGlobalWind(bIndoors);

	auto* pWindAreas = &m_outdoorWindAreas[m_nCurrentWindAreaList];
	if (bIndoors)
		pWindAreas = &m_indoorWindAreas[m_nCurrentWindAreaList];

	for (int i = 0; i < nSamples; ++i)
	{
		Vec3 pos = pSamples[i]; // Take position out of sample point,

		// Add global wind to sample point
		pSamples[i] = vWind;

		if (!m_pCVars->e_WindAreas)
			continue;

		int px = (int)pos.x;
		int py = (int)pos.y;
		// Iterate all areas, looking for wind areas. Skip first (global) area, and global wind.
		for (const auto& windArea : * pWindAreas)
		{
			if (
			  px >= windArea.x0 &&
			  px <= windArea.x1 &&
			  py >= windArea.y0 &&
			  py <= windArea.y1)
			{
				// TODO: Interpolate wind position between corner points.
				//float u, v, w;
				//ComputerBarycentric2d(pos, windArea.pos[0], windArea.pos[1], windArea.pos[2], u, v, w);
				pSamples[i] += windArea.windSpeed[4];

				//pe_status_area area;
				//area.ctr = pos;
				//if (windArea.pArea->GetStatus(&area))
				//pSamples[i] += area.pb.waterFlow;

			}
		}
	}

	return true;
}

//==============================================================================================================================================

void C3DEngine::UpdateWindGridJobEntry(Vec3 vPos)
{
	FUNCTION_PROFILER_3DENGINE

	SWindGrid& rWindGrid = m_WindGrid[m_nCurWind];

	int x;
	rWindGrid.m_vCentr = m_vWindFieldCamera;
	rWindGrid.m_vCentr.z = 0;

	static const float fBEND_RESPONSE = 0.25f;
	static const float fMAX_BENDING = 2.f;
	bool bIndoors = false;

	float fSize = (float)rWindGrid.m_nWidth * rWindGrid.m_fCellSize;
	Vec3 vHalfSize = Vec3(fSize * 0.5f, fSize * 0.5f, 0.0f);
	AABB windBox(rWindGrid.m_vCentr - vHalfSize, rWindGrid.m_vCentr + vHalfSize);

	auto* pWindAreas = &m_outdoorWindAreas[m_nCurrentWindAreaList];
	if (bIndoors)
		pWindAreas = &m_indoorWindAreas[m_nCurrentWindAreaList];

	Vec3 vGlobalWind = GetGlobalWind(bIndoors);

	if (pWindAreas->size() == 0 && vGlobalWind.IsZero())
		return;

	float fInterp = min(gEnv->pTimer->GetFrameTime() * 0.8f, 1.f);

	int nFrame = gEnv->nMainFrameID;

	// 1 Step: Rasterize wind areas
	for (const auto& windArea : * pWindAreas)
	{
		SOptimizedOutdoorWindArea WA = windArea;
		WA.point[1].x = (WA.point[0].x + WA.point[1].x) * 0.5f;
		WA.point[2] = WA.point[4];
		WA.windSpeed[2] = WA.windSpeed[4];
		WA.point[3].y = (WA.point[0].y + WA.point[3].y) * 0.5f;
		UpdateWindGridArea(rWindGrid, WA, windBox);

		WA = windArea;
		WA.point[0].x = (WA.point[0].x + WA.point[1].x) * 0.5f;
		WA.point[3] = WA.point[4];
		WA.windSpeed[3] = WA.windSpeed[4];
		WA.point[2].y = (WA.point[1].y + WA.point[2].y) * 0.5f;
		UpdateWindGridArea(rWindGrid, WA, windBox);

		WA = windArea;
		WA.point[3].x = (WA.point[3].x + WA.point[2].x) * 0.5f;
		WA.point[0] = WA.point[4];
		WA.windSpeed[0] = WA.windSpeed[4];
		WA.point[1].y = (WA.point[1].y + WA.point[2].y) * 0.5f;
		UpdateWindGridArea(rWindGrid, WA, windBox);

		WA = windArea;
		WA.point[2].x = (WA.point[3].x + WA.point[2].x) * 0.5f;
		WA.point[1] = WA.point[4];
		WA.windSpeed[1] = WA.windSpeed[4];
		WA.point[0].y = (WA.point[0].y + WA.point[3].y) * 0.5f;
		UpdateWindGridArea(rWindGrid, WA, windBox);
	}

	// 2 Step: Initialize the rest of the field by global wind value
	const int nSize = rWindGrid.m_nWidth * rWindGrid.m_nHeight;
	Vec2 vWindCur = Vec2(vGlobalWind.x, vGlobalWind.y) * GetCVars()->e_WindBendingStrength;
	for (x = 0; x < nSize; x++)
	{
		if (m_pWindAreaFrames[x] == nFrame)
			continue;
		m_pWindAreaFrames[x] = nFrame;

		// Interpolate
		Vec2 vWindInt = m_pWindField[x];
		vWindInt += (vWindCur - vWindInt) * fInterp;
		m_pWindField[x] = vWindInt;

		Vec2 vBend = vWindInt * fBEND_RESPONSE;
		vBend *= fMAX_BENDING / (fMAX_BENDING + vBend.GetLength());
		rWindGrid.m_pData[x] = CryHalf2(vBend.x, vBend.y);
	}
}

//static int g_nWindError;
void C3DEngine::UpdateWindGridArea(SWindGrid& rWindGrid, const SOptimizedOutdoorWindArea& windArea, const AABB& windBox)
{
	//FUNCTION_PROFILER_3DENGINE
	int x, y;

	const int tl = 0, tr = 1, br = 2, bl = 3;

	AABB areaBox(windArea.point[tl], windArea.point[br]);
	if (!windBox.ContainsBox2D(areaBox))
		return;

	Vec3 vGlobalWind = GetGlobalWind(false) * GetCVars()->e_WindBendingStrength;

	float fInterp = min(gEnv->pTimer->GetFrameTime() * 0.8f, 1.f);

	int nFrame = gEnv->nMainFrameID;

	static const float fBEND_RESPONSE = 0.25f;
	static const float fMAX_BENDING = 2.f;
	bool bIndoors = false;

	// Rasterized box
	AABB windBoxC = windBox;
	windBoxC.ClipToBox(areaBox);
	int nX1 = (int)windBoxC.min.x;
	int nY1 = (int)windBoxC.min.y;
	int nX2 = (int)windBoxC.max.x;
	int nY2 = (int)windBoxC.max.y;

	float fInvAreaX = 1.0f / (areaBox.max.x - areaBox.min.x);
	float fInvAreaY = 1.0f / (areaBox.max.y - areaBox.min.y);

	float fInvX = 1.0f / (windBoxC.max.x - windBoxC.min.x);

	Vec3 windTopX1, windBottomX1;
	Vec3 windTopX2, windBottomX2;
	float fLerpX1 = (windBoxC.min.x - areaBox.min.x) * fInvAreaX;     // 0..1
	float fLerpX2 = (windBoxC.max.x - areaBox.min.x) * fInvAreaX;     // 0..1
	windTopX1.SetLerp(windArea.windSpeed[tl], windArea.windSpeed[tr], fLerpX1);
	windBottomX1.SetLerp(windArea.windSpeed[bl], windArea.windSpeed[br], fLerpX1);
	windTopX2.SetLerp(windArea.windSpeed[tl], windArea.windSpeed[tr], fLerpX2);
	windBottomX2.SetLerp(windArea.windSpeed[bl], windArea.windSpeed[br], fLerpX2);

	float fAreaStrength = GetCVars()->e_WindBendingAreaStrength;

	Vec3 vGridStart = windBoxC.min - windBox.min;
	int nXGrid = (int)vGridStart.x;
	int nYGrid = (int)vGridStart.y;
	assert(nXGrid >= 0 && nXGrid + (nX2 - nX1) <= rWindGrid.m_nWidth);
	assert(nYGrid >= 0 && nYGrid + (nY2 - nY1) <= rWindGrid.m_nHeight);
	for (y = nY1; y < nY2; y++)
	{
		/*if (nYGrid >= rWindGrid.m_nHeight)
		   {
		   g_nWindError++;
		   continue;
		   }*/
		// Setup scanline gradients
		float fLerpY = ((float)y - areaBox.min.y) * fInvAreaY;      // 0..1
		Vec3 windStart, windEnd;
		windStart.SetLerp(windTopX1, windBottomX1, fLerpY);
		windEnd.SetLerp(windTopX2, windBottomX2, fLerpY);
		Vec3 windDelta = (windEnd - windStart) * fInvX;
		CryHalf2* pData = &rWindGrid.m_pData[nYGrid * rWindGrid.m_nWidth];
		int* pFrames = &m_pWindAreaFrames[nYGrid * rWindGrid.m_nWidth];
		Vec2* pField = &m_pWindField[nYGrid * rWindGrid.m_nWidth];
		int nX = nXGrid;
		for (x = nX1; x < nX2; x++)
		{
			/*if (nX >= rWindGrid.m_nWidth)
			   {
			   g_nWindError++;
			   continue;
			   }*/
			Vec2 vWindCur = Vec2(windStart.x, windStart.y) * fAreaStrength + Vec2(vGlobalWind.x, vGlobalWind.y);

			// Interpolate
			Vec2 vWindInt = pField[nX];
			vWindInt += (vWindCur - vWindInt) * fInterp;
			pField[nX] = vWindInt;

			Vec2 vBend = vWindInt * fBEND_RESPONSE;
			vBend *= fMAX_BENDING / (fMAX_BENDING + vBend.GetLength());
			pData[nX] = CryHalf2(vBend.x, vBend.y);

			pFrames[nX] = nFrame;
			nX++;

			windStart += windDelta;
		}
		nYGrid++;
	}
}

DECLARE_JOB("C3DEngine::UpdateWindGridJobEntry", TUpdateWindJob, C3DEngine::UpdateWindGridJobEntry);

void C3DEngine::StartWindGridJob(const Vec3& vPos)
{
	if (GetCVars()->e_VegetationBending != 2)
	{
		m_bWindJobRun = false;
		return;
	}
	SWindGrid& rWindGrid = m_WindGrid[m_nCurWind];
	if (!rWindGrid.m_pData)
	{
		rWindGrid.m_nWidth = 256;
		rWindGrid.m_nHeight = 256;
		rWindGrid.m_pData = new CryHalf2[rWindGrid.m_nWidth * rWindGrid.m_nHeight];
		memset(rWindGrid.m_pData, 0, rWindGrid.m_nWidth * rWindGrid.m_nHeight * sizeof(CryHalf2));
		rWindGrid.m_fCellSize = 1.0f;

		if (!m_pWindField)
		{
			m_pWindField = new Vec2[rWindGrid.m_nWidth * rWindGrid.m_nHeight];
			memset(m_pWindField, 0, rWindGrid.m_nWidth * rWindGrid.m_nHeight * sizeof(Vec2));

			m_pWindAreaFrames = new int[rWindGrid.m_nWidth * rWindGrid.m_nHeight];
			memset(m_pWindAreaFrames, 0, rWindGrid.m_nWidth * rWindGrid.m_nHeight * sizeof(int));
		}
	}
	m_vWindFieldCamera = m_RenderingCamera.GetPosition();

	/*auto* pWindAreas = &m_outdoorWindAreas[m_nCurrentWindAreaList];
	   if (pWindAreas->size())
	   {
	   static FILE *fp = NULL;
	   if (!fp)
	    fp = gEnv->pCryPak->FOpen("Wind.txt", "w");

	   if (fp)
	   {
	    gEnv->pCryPak->FPrintf(fp, "\n\n%d Wind Areas:\n", pWindAreas->size());
	   }

	   // 2 Step: Rasterize wind areas
	   for (const auto &windArea : *pWindAreas)
	   {
	    if (fp)
	    {
	      gEnv->pCryPak->FPrintf(fp, "[%.3f, %.3f] | [%.3f, %.3f] | [%.3f, %.3f] | [%.3f, %.3f] || [%.3f, %.3f]\n",
	        windArea.windSpeed[0].x, windArea.windSpeed[0].y,
	        windArea.windSpeed[1].x, windArea.windSpeed[1].y,
	        windArea.windSpeed[2].x, windArea.windSpeed[2].y,
	        windArea.windSpeed[3].x, windArea.windSpeed[3].y,
	        windArea.windSpeed[4].x, windArea.windSpeed[4].y
	        );
	    }
	   }
	   if (fp)
	   {
	    gEnv->pCryPak->FPrintf(fp, "{230, 270} = [%.3f, %.3f]\n", m_pWindField[270*512+230].x, m_pWindField[270*512+230].y);
	    gEnv->pCryPak->FFlush(fp);
	   }
	   }*/

	JobManager::SJobState* pJobState = &m_WindJobState;

	TUpdateWindJob job(vPos);
	job.RegisterJobState(pJobState);
	job.SetClassInstance(this);
	job.SetPriorityLevel(JobManager::eLowPriority);
	job.Run();

	m_bWindJobRun = true;

	//UpdateWindGridJobEntry(vPos);
	//m_bWindJobRun = false;
}
void C3DEngine::FinishWindGridJob()
{
	FUNCTION_PROFILER_3DENGINE
	if (m_bWindJobRun)
	{
		gEnv->GetJobManager()->WaitForJob(m_WindJobState);

		GetRenderer()->EF_SubmitWind(&m_WindGrid[m_nCurWind]);
		m_nCurWind = 1 - m_nCurWind;

		m_bWindJobRun = false;
	}
}

//==========================================================================================================================

void C3DEngine::SetupBending(CRenderObject*& pObj, const IRenderNode* pNode, const float fRadiusVert, const SRenderingPassInfo& passInfo, bool alreadyDuplicated)
{
	FUNCTION_PROFILER_3DENGINE;
	if (!GetCVars()->e_VegetationBending)
		return;

	if (!pNode->m_pTempData)
		return;

	SRenderNodeTempData::SUserData& userData = pNode->m_pTempData->userData;
	uint32 nCurrentBendingFrame = passInfo.GetMainFrameID();
	const bool needsUpdate = ((int)nCurrentBendingFrame - (int)userData.nBendingLastFrame) > 10;

	//const Vec3 &vObjPos = pObj->GetTranslation();
	const float fMaxViewDist = const_cast<IRenderNode*>(pNode)->GetMaxViewDist();
	const float fBendingAttenuation = 1.f - (pObj->m_fDistance / (fMaxViewDist * GetFloatCVar(e_WindBendingDistRatio)));
	uint64 bendingMaskOr = 0u;
	uint64 bendingMaskAnd = ~FOB_BENDED;

	if (fBendingAttenuation > 0.f && userData.m_Bending.m_fMainBendingScale > 0.f)
	{
		bendingMaskOr = FOB_BENDED;
		bendingMaskAnd = ~0u;

		if (needsUpdate)
		{
			userData.nBendingLastFrame = nCurrentBendingFrame;

			static const float fBEND_RESPONSE = 0.25f;
			static const float fMAX_BENDING = 2.f;
			static const float fWAVE_PARALLEL = 0.008f;
			static const float fWAVE_TRANSVERSE = 0.002f;

			//if (userData.windUpdateCounter == 0)
			{
				userData.vCurrentWind = m_p3DEngine->GetWind(pNode->GetBBox(), !!pNode->GetEntityVisArea());
			}
			//userData.windUpdateCounter++;
			//if (userData.windUpdateCounter > 5)
			//userData.windUpdateCounter = 5; // Query wind not more often then every 5th call

			// Soft clamp bending from wind amplitude.
			Vec2 vBending = Vec2(userData.vCurrentWind) * fBEND_RESPONSE;
			vBending *= fMAX_BENDING / (fMAX_BENDING + vBending.GetLength());
			vBending *= fBendingAttenuation;

			SVegetationBending* pBending = &userData.m_Bending;

			float fWaveFreq = 0.4f / (fRadiusVert + 1.f) + 0.2f;

			if (!userData.bBendingSet)
			{
				// First time shown, set full bending.
				pBending->m_vBending = vBending;
				userData.bBendingSet = true;
			}
			else
			{
				// Already visible, fade toward current value.
				float fInterp = min(gEnv->pTimer->GetFrameTime() * fWaveFreq * 0.5f, 1.f);
				pBending->m_vBending += (vBending - pBending->m_vBending) * fInterp;
			}

			pBending->m_Waves[0].m_Freq = fWaveFreq;
			//pBending->m_Waves[0].m_Phase  = vObjPos.x * 0.125f;
			pBending->m_Waves[0].m_Amp = pBending->m_vBending.x * fWAVE_PARALLEL + pBending->m_vBending.y * fWAVE_TRANSVERSE;

			pBending->m_Waves[1].m_Freq = fWaveFreq * 1.125f;
			//pBending->m_Waves[1].m_Phase  = vObjPos.y * 0.125f;
			pBending->m_Waves[1].m_Amp = pBending->m_vBending.y * fWAVE_PARALLEL - pBending->m_vBending.x * fWAVE_TRANSVERSE;

			pBending->m_fRadiusVert = fRadiusVert;
		}
	}

	SRenderObjData* pOD = pObj->GetObjData();
	if (!pOD)
		return;

	pObj->m_ObjFlags |= bendingMaskOr | FOB_DYNAMIC_OBJECT;
	pObj->m_ObjFlags &= bendingMaskAnd;
	pOD->m_bending = userData.m_Bending;
}

IVisArea* C3DEngine::GetVisAreaFromPos(const Vec3& vPos)
{
	if (m_pObjManager && m_pVisAreaManager)
		return m_pVisAreaManager->GetVisAreaFromPos(vPos);

	return 0;
}

bool C3DEngine::IntersectsVisAreas(const AABB& box, void** pNodeCache)
{
	if (m_pObjManager && m_pVisAreaManager)
		return m_pVisAreaManager->IntersectsVisAreas(box, pNodeCache);
	return false;
}

bool C3DEngine::ClipToVisAreas(IVisArea* pInside, Sphere& sphere, Vec3 const& vNormal, void* pNodeCache)
{
	if (pInside)
		return pInside->ClipToVisArea(true, sphere, vNormal);
	else if (m_pVisAreaManager)
		return m_pVisAreaManager->ClipOutsideVisAreas(sphere, vNormal, pNodeCache);
	return false;
}

bool C3DEngine::IsVisAreasConnected(IVisArea* pArea1, IVisArea* pArea2, int nMaxRecursion, bool bSkipDisabledPortals)
{
	if (pArea1 == pArea2)
		return (true);  // includes the case when both pArea1 & pArea2 are NULL (totally outside)
	                  // not considered by the other checks
	if (!pArea1 || !pArea2)
		return (false); // avoid a crash - better to put this check only
	                  // here in one place than in all the places where this function is called

	nMaxRecursion *= 2; // include portals since portals are the areas

	if (m_pObjManager && m_pVisAreaManager)
		return ((CVisArea*)pArea1)->FindVisArea((CVisArea*)pArea2, nMaxRecursion, bSkipDisabledPortals);

	return false;
}

bool C3DEngine::IsOutdoorVisible()
{
	if (m_pObjManager && m_pVisAreaManager)
		return m_pVisAreaManager->IsOutdoorAreasVisible();

	return false;
}

void C3DEngine::EnableOceanRendering(bool bOcean)
{
	m_bOcean = bOcean;
}

//////////////////////////////////////////////////////////////////////////
IMaterialHelpers& C3DEngine::GetMaterialHelpers()
{
	return Cry3DEngineBase::GetMatMan()->s_materialHelpers;
}

IMaterialManager* C3DEngine::GetMaterialManager()
{
	return Cry3DEngineBase::GetMatMan();
}

//////////////////////////////////////////////////////////////////////////

bool C3DEngine::IsTerrainHightMapModifiedByGame()
{
	return m_pTerrain ? (m_pTerrain->m_bHeightMapModified != 0) : 0;
}

void C3DEngine::CheckPhysicalized(const Vec3& vBoxMin, const Vec3& vBoxMax)
{
	/*  if(!GetCVars()->e_stream_areas)
	    return;

	   CVisArea * pVisArea = (CVisArea *)GetVisAreaFromPos((vBoxMin+vBoxMax)*0.5f);
	   if(pVisArea)
	   { // indoor
	    pVisArea->CheckPhysicalized();
	    IVisArea * arrConnections[16];
	    int nConections = pVisArea->GetRealConnections(arrConnections,16);
	    for(int i=0; i<nConections; i++)
	      ((CVisArea*)arrConnections[i])->CheckPhysicalized();
	   }
	   else
	   { // outdoor
	    CTerrainNode * arrSecInfo[] =
	    {
	      m_pTerrain->GetSecInfo(int(vBoxMin.x), int(vBoxMin.y)),
	      m_pTerrain->GetSecInfo(int(vBoxMax.x), int(vBoxMin.y)),
	      m_pTerrain->GetSecInfo(int(vBoxMin.x), int(vBoxMax.y)),
	      m_pTerrain->GetSecInfo(int(vBoxMax.x), int(vBoxMax.y))
	    };

	    for(int i=0; i<4; i++)
	      if(arrSecInfo[i])
	        arrSecInfo[i]->CheckPhysicalized();
	   }*/
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::UpdateWindAreas()
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pCVars->e_Wind)
		return;

	m_nCurrentWindAreaList = (m_nCurrentWindAreaList + 1) % 2;
	int nextWindAreaList = (m_nCurrentWindAreaList + 1) % 2;

	m_outdoorWindAreas[nextWindAreaList].clear();
	m_indoorWindAreas[nextWindAreaList].clear();

	if (m_pCVars->e_WindAreas)
	{
		// Iterate all areas, looking for wind areas. Skip first (global) area, and global wind.
		pe_params_buoyancy pb;
		IPhysicalEntity* pArea = 0;
		while (pArea = GetPhysicalWorld()->GetNextArea(pArea))
		{
			if (!pArea->GetParams(&pb))
				continue;
			if (pb.iMedium != pe_params_buoyancy::eAir)
				continue;
			if (pArea == m_pGlobalWind)
				continue;

			// Skip indoor areas.
			bool bIndoor = false;
			pe_params_foreign_data fd;
			if (pArea->GetParams(&fd) && (fd.iForeignFlags & PFF_OUTDOOR_AREA))
				bIndoor = false;

			pe_params_bbox area_bbox;
			if (!pArea->GetParams(&area_bbox))
				continue;

			Vec3 b0 = area_bbox.BBox[0];
			Vec3 b1 = area_bbox.BBox[1];

			SOptimizedOutdoorWindArea windArea;
			windArea.pArea = pArea;

			windArea.x0 = int(floorf(b0.x));
			windArea.x1 = int(ceilf(b1.x));
			windArea.y0 = int(floorf(b0.y));
			windArea.y1 = int(ceilf(b1.y));

			float z = (b0.z + b1.z) / 2.0f; // Middle of the area in z
			windArea.z = z;

			// Sample wind at the corners of the area
			pe_status_area area;
			area.size = Vec3(0.1f, 0.1f, 0.1f);
			windArea.point[0] = Vec2(b0.x, b0.y);
			windArea.point[1] = Vec2(b1.x, b0.y);
			windArea.point[2] = Vec2(b1.x, b1.y);
			windArea.point[3] = Vec2(b0.x, b1.y);
			windArea.point[4] = 0.5f * (Vec2(b0.x, b0.y) + Vec2(b1.x, b1.y));
			for (int i = 0; i < 5; ++i)
			{
				area.ctr = Vec3(windArea.point[i].x, windArea.point[i].y, windArea.z);
				if (pArea->GetStatus(&area))
					windArea.windSpeed[i] = area.pb.waterFlow;
			}
			if (bIndoor)
				m_indoorWindAreas[nextWindAreaList].push_back(windArea);
			else
				m_outdoorWindAreas[nextWindAreaList].push_back(windArea);
		}
		// Sort wind areas by bounding box min x.
		std::sort(m_outdoorWindAreas[nextWindAreaList].begin(), m_outdoorWindAreas[nextWindAreaList].end(), [](const SOptimizedOutdoorWindArea& a, const SOptimizedOutdoorWindArea& b) { return a.x0 < b.x0; });
		std::sort(m_indoorWindAreas[nextWindAreaList].begin(), m_indoorWindAreas[nextWindAreaList].end(), [](const SOptimizedOutdoorWindArea& a, const SOptimizedOutdoorWindArea& b) { return a.x0 < b.x0; });
	}
	StartWindGridJob(m_RenderingCamera.GetPosition());
}

void C3DEngine::CheckMemoryHeap()
{
	assert(IsHeapValid());
}

void C3DEngine::CloseTerrainTextureFile(int nSID)
{
	if (m_pTerrain)
		m_pTerrain->CloseTerrainTextureFile(nSID);
}

//////////////////////////////////////////////////////////////////////////
int C3DEngine::GetLoadedObjectCount()
{
	int nObjectsLoaded = m_pObjManager ? m_pObjManager->GetLoadedObjectCount() : 0;

	if (gEnv->pCharacterManager)
	{
		ICharacterManager::Statistics AnimStats;
		gEnv->pCharacterManager->GetStatistics(AnimStats);
		nObjectsLoaded += AnimStats.numAnimObjectModels + AnimStats.numCharModels;
	}

	return nObjectsLoaded;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::GetLoadedStatObjArray(IStatObj** pObjectsArray, int& nCount)
{
	if (m_pObjManager)
		m_pObjManager->GetLoadedStatObjArray(pObjectsArray, nCount);
	else
		nCount = 0;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::GetObjectsStreamingStatus(SObjectsStreamingStatus& outStatus)
{
	if (m_pObjManager)
	{
		m_pObjManager->GetObjectsStreamingStatus(outStatus);
	}
	else
	{
		memset(&outStatus, 0, sizeof(outStatus));
	}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::GetStreamingSubsystemData(int subsystem, SStremaingBandwidthData& outData)
{
	switch (subsystem)
	{
	case eStreamTaskTypeSound:
		REINST(Audio bandwidth stats)
		//REINST: C3DEngine::GetStreamingSubsystemData(subsystem, outData)
		//gEnv->pAudioSystem->GetIAudioDevice()->GetBandwidthStats(&outData.fBandwidthRequested);
		break;
	case eStreamTaskTypeGeometry:
		m_pObjManager->GetBandwidthStats(&outData.fBandwidthRequested);
		break;
	case eStreamTaskTypeTexture:
		gEnv->pRenderer->GetBandwidthStats(&outData.fBandwidthRequested);
		break;
	}

#if defined(STREAMENGINE_ENABLE_STATS)
	gEnv->pSystem->GetStreamEngine()->GetBandwidthStats((EStreamTaskType)subsystem, &outData.fBandwidthActual);
#endif
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::DeleteEntityDecals(IRenderNode* pEntity)
{
	if (m_pDecalManager && pEntity && (pEntity->m_nInternalFlags & IRenderNode::DECAL_OWNER))
		m_pDecalManager->OnEntityDeleted(pEntity);
}

void C3DEngine::CompleteObjectsGeometry()
{
	while (m_pTerrain && m_pTerrain->Recompile_Modified_Incrementaly_RoadRenderNodes())
		;

	CRoadRenderNode::FreeStaticMemoryUsage();
}

void C3DEngine::DeleteDecalsInRange(AABB* pAreaBox, IRenderNode* pEntity)
{
	if (m_pDecalManager)
		m_pDecalManager->DeleteDecalsInRange(pAreaBox, pEntity);
}

void C3DEngine::LockCGFResources()
{
	if (m_pObjManager)
		m_pObjManager->m_bLockCGFResources = 1;
}

void C3DEngine::UnlockCGFResources()
{
	if (m_pObjManager)
	{
		bool bNeedToFreeCGFs = m_pObjManager->m_bLockCGFResources != 0;
		m_pObjManager->m_bLockCGFResources = 0;
		if (bNeedToFreeCGFs)
		{
			m_pObjManager->FreeNotUsedCGFs();
		}
	}
}

void CLightEntity::ShadowMapInfo::Release(struct IRenderer* pRenderer)
{
	delete this;
}

Vec3 C3DEngine::GetTerrainSurfaceNormal(Vec3 vPos)
{
	return m_pTerrain ? m_pTerrain->GetTerrainSurfaceNormal(vPos, 0.5f * GetHeightMapUnitSize()) : Vec3(0.f, 0.f, 1.f);
}

IMemoryBlock* C3DEngine::Voxel_GetObjects(Vec3 vPos, float fRadius, int nSurfaceTypeId, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget)
{
	return 0;
}

void C3DEngine::Voxel_Paint(Vec3 vPos, float fRadius, int nSurfaceTypeId, Vec3 vBaseColor, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget, PodArray<IRenderNode*>* pBrushes, float fVoxelSize)
{
}

IVoxTerrain* C3DEngine::GetIVoxTerrain()
{
	return 0;
}

void C3DEngine::Voxel_SetFlags(bool bPhysics, bool bSimplify, bool bShadows, bool bMaterials)
{
}

//////////////////////////////////////////////////////////////////////////
IIndexedMesh* C3DEngine::CreateIndexedMesh()
{
	return new CIndexedMesh();
}

void C3DEngine::SerializeState(TSerialize ser)
{
	if (ser.IsReading())
	{
		CStatObjFoliage* pFoliage, * pFoliageNext;
		for (pFoliage = m_pFirstFoliage; &pFoliage->m_next != &m_pFirstFoliage; pFoliage = pFoliageNext)
		{
			pFoliageNext = pFoliage->m_next;
			delete pFoliage;
		}
	}

	ser.Value("m_bOcean", m_bOcean);
	ser.Value("m_moonRotationLatitude", m_moonRotationLatitude);
	ser.Value("m_moonRotationLongitude", m_moonRotationLongitude);
	ser.Value("m_eShadowMode", *(alias_cast<int*>(&m_eShadowMode)));
	if (ser.IsReading())
		UpdateMoonDirection();

	if (m_pTerrain)
		m_pTerrain->SerializeTerrainState(ser);

	if (m_pDecalManager)
		m_pDecalManager->Serialize(ser);

	m_pPartManager->Serialize(ser);
	if (m_pParticleSystem)
		m_pParticleSystem->Serialize(ser);
	m_pTimeOfDay->Serialize(ser);
}

void C3DEngine::PostSerialize(bool bReading)
{
	m_pPartManager->PostSerialize(bReading);
}

void C3DEngine::InitMaterialDefautMappingAxis(IMaterial* pMat)
{
	uint8 arrProj[3] = { 'X', 'Y', 'Z' };
	pMat->m_ucDefautMappingAxis = 'Z';
	pMat->m_fDefautMappingScale = 1.f;
	for (int c = 0; c < 3 && c < pMat->GetSubMtlCount(); c++)
	{
		IMaterial* pSubMat = (IMaterial*)pMat->GetSubMtl(c);
		pSubMat->m_ucDefautMappingAxis = arrProj[c];
		pSubMat->m_fDefautMappingScale = pMat->m_fDefautMappingScale;
	}
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* C3DEngine::CreateChunkfileContent(const char* filename)
{
	return new CContentCGF(filename);
}
void C3DEngine::ReleaseChunkfileContent(CContentCGF* pCGF)
{
	delete pCGF;
}

bool C3DEngine::LoadChunkFileContent(CContentCGF* pCGF, const char* filename, bool bNoWarningMode, bool bCopyChunkFile)
{
	CLoadLogListener listener;

	if (!pCGF)
	{
		FileWarning(0, filename, "CGF Loading Failed: no content instance passed");
	}
	else
	{
		CLoaderCGF cgf;

		CReadOnlyChunkFile* pChunkFile = new CReadOnlyChunkFile(bCopyChunkFile, bNoWarningMode);

		if (cgf.LoadCGF(pCGF, filename, *pChunkFile, &listener))
		{
			pCGF->SetChunkFile(pChunkFile);
			return true;
		}

		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "%s: Failed to load chunk file: '%s'", __FUNCTION__, cgf.GetLastError());
		delete pChunkFile;
	}

	return false;
}

bool C3DEngine::LoadChunkFileContentFromMem(CContentCGF* pCGF, const void* pData, size_t nDataLen, uint32 nLoadingFlags, bool bNoWarningMode, bool bCopyChunkFile)
{
	if (!pCGF)
	{
		FileWarning(0, "<memory>", "CGF Loading Failed: no content instance passed");
	}
	else
	{
		CLoadLogListener listener;

		CLoaderCGF cgf;
		CReadOnlyChunkFile* pChunkFile = new CReadOnlyChunkFile(bCopyChunkFile, bNoWarningMode);

		if (cgf.LoadCGFFromMem(pCGF, pData, nDataLen, *pChunkFile, &listener, nLoadingFlags))
		{
			pCGF->SetChunkFile(pChunkFile);
			return true;
		}

		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "%s: Failed to load chunk file: '%s'", __FUNCTION__, cgf.GetLastError());
		delete pChunkFile;
	}

	return false;
}

IChunkFile* C3DEngine::CreateChunkFile(bool bReadOnly)
{
	if (bReadOnly)
		return new CReadOnlyChunkFile(false);
	else
		return new CChunkFile;
}

//////////////////////////////////////////////////////////////////////////

ChunkFile::IChunkFileWriter* C3DEngine::CreateChunkFileWriter(EChunkFileFormat eFormat, ICryPak* pPak, const char* filename) const
{
	ChunkFile::CryPakFileWriter* const p = new ChunkFile::CryPakFileWriter();

	if (!p)
	{
		return 0;
	}

	if (!p->Create(pPak, filename))
	{
		delete p;
		return 0;
	}

	const ChunkFile::MemorylessChunkFileWriter::EChunkFileFormat fmt =
	  (eFormat == I3DEngine::eChunkFileFormat_0x745)
	  ? ChunkFile::MemorylessChunkFileWriter::eChunkFileFormat_0x745
	  : ChunkFile::MemorylessChunkFileWriter::eChunkFileFormat_0x746;

	return new ChunkFile::MemorylessChunkFileWriter(fmt, p);
}

void C3DEngine::ReleaseChunkFileWriter(ChunkFile::IChunkFileWriter* p) const
{
	if (p)
	{
		delete p->GetWriter();
		delete p;
	}
}

//////////////////////////////////////////////////////////////////////////

int C3DEngine::GetTerrainTextureNodeSizeMeters()
{
	if (m_pTerrain)
		return m_pTerrain->GetTerrainTextureNodeSizeMeters();
	return 0;
}

int C3DEngine::GetTerrainTextureNodeSizePixels(int nLayer)
{
	if (m_pTerrain)
		return m_pTerrain->GetTerrainTextureNodeSizePixels(nLayer, 0);
	return 0;
}

void C3DEngine::SetHeightMapMaxHeight(float fMaxHeight)
{
	if (m_pTerrain)
		m_pTerrain->SetHeightMapMaxHeight(fMaxHeight);
}

ITerrain* C3DEngine::CreateTerrain(const STerrainInfo& TerrainInfo)
{
	delete m_pTerrain;
	m_pTerrain = new CTerrain(TerrainInfo);
	return (ITerrain*)m_pTerrain;
}

void C3DEngine::DeleteTerrain()
{
	delete m_pTerrain;
	m_pTerrain = NULL;
}

void C3DEngine::SetStreamableListener(IStreamedObjectListener* pListener)
{
	m_pStreamListener = pListener;
}

void C3DEngine::PrecacheLevel(bool bPrecacheAllVisAreas, Vec3* pPrecachePoints, int nPrecachePointsNum)
{
	LOADING_TIME_PROFILE_SECTION;

	if (GetVisAreaManager())
		GetVisAreaManager()->PrecacheLevel(bPrecacheAllVisAreas, pPrecachePoints, nPrecachePointsNum);
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::SetGlobalParameter(E3DEngineParameter param, const Vec3& v)
{
	float fValue = v.x;
	switch (param)
	{
	case E3DPARAM_SKY_COLOR:
		SetSkyColor(v);
		break;
	case E3DPARAM_SUN_COLOR:
		SetSunColor(v);
		break;

	case E3DPARAM_SUN_SPECULAR_MULTIPLIER:
		m_fSunSpecMult = v.x;
		break;

	case E3DPARAM_AMBIENT_GROUND_COLOR:
		m_vAmbGroundCol = v;
		break;
	case E3DPARAM_AMBIENT_MIN_HEIGHT:
		m_fAmbMaxHeight = v.x;
		break;
	case E3DPARAM_AMBIENT_MAX_HEIGHT:
		m_fAmbMinHeight = v.x;
		break;

	case E3DPARAM_SKY_HIGHLIGHT_POS:
		m_vSkyHightlightPos = v;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_COLOR:
		m_vSkyHightlightCol = v;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_SIZE:
		m_fSkyHighlightSize = fValue > 0.0f ? fValue : 0.0f;
		break;
	case E3DPARAM_VOLFOG_RAMP:
		m_volFogRamp = v;
		break;
	case E3DPARAM_VOLFOG_SHADOW_RANGE:
		m_volFogShadowRange = v;
		break;
	case E3DPARAM_VOLFOG_SHADOW_DARKENING:
		m_volFogShadowDarkening = v;
		break;
	case E3DPARAM_VOLFOG_SHADOW_ENABLE:
		m_volFogShadowEnable = v;
		break;
	case E3DPARAM_VOLFOG2_CTRL_PARAMS:
		m_volFog2CtrlParams = v;
		break;
	case E3DPARAM_VOLFOG2_SCATTERING_PARAMS:
		m_volFog2ScatteringParams = v;
		break;
	case E3DPARAM_VOLFOG2_RAMP:
		m_volFog2Ramp = v;
		break;
	case E3DPARAM_VOLFOG2_COLOR:
		m_volFog2Color = v;
		break;
	case E3DPARAM_VOLFOG2_GLOBAL_DENSITY:
		m_volFog2GlobalDensity = v;
		break;
	case E3DPARAM_VOLFOG2_HEIGHT_DENSITY:
		m_volFog2HeightDensity = v;
		break;
	case E3DPARAM_VOLFOG2_HEIGHT_DENSITY2:
		m_volFog2HeightDensity2 = v;
		break;
	case E3DPARAM_VOLFOG2_COLOR1:
		m_volFog2Color1 = v;
		break;
	case E3DPARAM_VOLFOG2_COLOR2:
		m_volFog2Color2 = v;
		break;
	case E3DPARAM_SKYLIGHT_RAYLEIGH_INSCATTER:
		m_fogColorSkylightRayleighInScatter = v;
		break;
	case E3DPARAM_NIGHSKY_HORIZON_COLOR:
		m_nightSkyHorizonCol = v;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_COLOR:
		m_nightSkyZenithCol = v;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_SHIFT:
		m_nightSkyZenithColShift = v.x;
		break;
	case E3DPARAM_NIGHSKY_STAR_INTENSITY:
		m_nightSkyStarIntensity = v.x;
		break;
	case E3DPARAM_NIGHSKY_MOON_COLOR:
		m_nightMoonCol = v;
		break;
	case E3DPARAM_NIGHSKY_MOON_SIZE:
		m_nightMoonSize = v.x;
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR:
		m_nightMoonInnerCoronaCol = v;
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE:
		m_nightMoonInnerCoronaScale = v.x;
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR:
		m_nightMoonOuterCoronaCol = v;
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE:
		m_nightMoonOuterCoronaScale = v.x;
		break;
	case E3DPARAM_OCEANFOG_COLOR:
		m_oceanFogColor = v;
		break;
	case E3DPARAM_OCEANFOG_DENSITY:
		m_oceanFogDensity = v.x;
		break;
	case E3DPARAM_SKY_MOONROTATION:
		m_moonRotationLatitude = v.x;
		m_moonRotationLongitude = v.y;
		UpdateMoonDirection();
		break;
	case E3DPARAM_SKYBOX_MULTIPLIER:
		m_skyboxMultiplier = v.x;
		break;
	case E3DPARAM_DAY_NIGHT_INDICATOR:
		m_dayNightIndicator = v.x;
		REINST(Set daylight parameter)
		//gEnv->pSoundSystem->SetGlobalParameter("daylight", m_dayNightIndicator);
		break;
	case E3DPARAM_FOG_COLOR2:
		m_fogColor2 = v;
		break;
	case E3DPARAM_FOG_RADIAL_COLOR:
		m_fogColorRadial = v;
		break;
	case E3DPARAM_VOLFOG_HEIGHT_DENSITY:
		m_volFogHeightDensity = Vec3(v.x, v.y, 0);
		break;
	case E3DPARAM_VOLFOG_HEIGHT_DENSITY2:
		m_volFogHeightDensity2 = Vec3(v.x, v.y, 0);
		break;
	case E3DPARAM_VOLFOG_GRADIENT_CTRL:
		m_volFogGradientCtrl = v;
		break;
	case E3DPARAM_VOLFOG_GLOBAL_DENSITY:
		m_volFogGlobalDensity = v.x;
		m_volFogFinalDensityClamp = v.z;
		break;
	case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR:
		m_pPhotoFilterColor = Vec4(v.x, v.y, v.z, 1);
		SetPostEffectParamVec4("clr_ColorGrading_PhotoFilterColor", m_pPhotoFilterColor);
		break;
	case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY:
		m_fPhotoFilterColorDensity = fValue;
		SetPostEffectParam("ColorGrading_PhotoFilterColorDensity", m_fPhotoFilterColorDensity);
		break;
	case E3DPARAM_COLORGRADING_FILTERS_GRAIN:
		m_fGrainAmount = fValue;
		SetPostEffectParam("ColorGrading_GrainAmount", m_fGrainAmount);
		break;
	case E3DPARAM_SKY_SKYBOX_ANGLE: // sky box rotation
		m_fSkyBoxAngle = fValue;
		break;
	case E3DPARAM_SKY_SKYBOX_STRETCHING: // sky box stretching
		m_fSkyBoxStretching = fValue;
		break;
	case E3DPARAM_HDR_FILMCURVE_SHOULDER_SCALE:
		m_vHDRFilmCurveParams.x = v.x;
		break;
	case E3DPARAM_HDR_FILMCURVE_LINEAR_SCALE:
		m_vHDRFilmCurveParams.y = v.x;
		break;
	case E3DPARAM_HDR_FILMCURVE_TOE_SCALE:
		m_vHDRFilmCurveParams.z = v.x;
		break;
	case E3DPARAM_HDR_FILMCURVE_WHITEPOINT:
		m_vHDRFilmCurveParams.w = v.x;
		break;
	case E3DPARAM_HDR_EYEADAPTATION_PARAMS:
		m_vHDREyeAdaptation = v;
		break;
	case E3DPARAM_HDR_EYEADAPTATION_PARAMS_LEGACY:
		m_vHDREyeAdaptationLegacy = v;
		break;
	case E3DPARAM_HDR_BLOOM_AMOUNT:
		m_fHDRBloomAmount = v.x;
		break;
	case E3DPARAM_HDR_COLORGRADING_COLOR_SATURATION:
		m_fHDRSaturation = v.x;
		break;
	case E3DPARAM_HDR_COLORGRADING_COLOR_BALANCE:
		m_vColorBalance = v;
		break;
	case E3DPARAM_CLOUDSHADING_MULTIPLIERS:
		m_fCloudShadingSunLightMultiplier = (v.x >= 0) ? v.x : 0;
		m_fCloudShadingSkyLightMultiplier = (v.y >= 0) ? v.y : 0;
		break;
	case E3DPARAM_CLOUDSHADING_SUNCOLOR:
		m_vCloudShadingCustomSunColor = v;
		break;
	case E3DPARAM_CLOUDSHADING_SKYCOLOR:
		m_vCloudShadingCustomSkyColor = v;
		break;
	case E3DPARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING:
		m_vVolCloudAtmosphericScattering = v;
		break;
	case E3DPARAM_VOLCLOUD_GEN_PARAMS:
		m_vVolCloudGenParams = v;
		break;
	case E3DPARAM_VOLCLOUD_SCATTERING_LOW:
		m_vVolCloudScatteringLow = v;
		break;
	case E3DPARAM_VOLCLOUD_SCATTERING_HIGH:
		m_vVolCloudScatteringHigh = v;
		break;
	case E3DPARAM_VOLCLOUD_GROUND_COLOR:
		m_vVolCloudGroundColor = v;
		break;
	case E3DPARAM_VOLCLOUD_SCATTERING_MULTI:
		m_vVolCloudScatteringMulti = v;
		break;
	case E3DPARAM_VOLCLOUD_WIND_ATMOSPHERIC:
		m_vVolCloudWindAtmospheric = v;
		break;
	case E3DPARAM_VOLCLOUD_TURBULENCE:
		m_vVolCloudTurbulence = v;
		break;
	case E3DPARAM_VOLCLOUD_ENV_PARAMS:
		m_vVolCloudEnvParams = v;
		break;
	case E3DPARAM_VOLCLOUD_GLOBAL_NOISE_SCALE:
		m_vVolCloudGlobalNoiseScale = v;
		break;
	case E3DPARAM_VOLCLOUD_RENDER_PARAMS:
		m_vVolCloudRenderParams = v;
		break;
	case E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_SCALE:
		m_vVolCloudTurbulenceNoiseScale = v;
		break;
	case E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_PARAMS:
		m_vVolCloudTurbulenceNoiseParams = v;
		break;
	case E3DPARAM_VOLCLOUD_DENSITY_PARAMS:
		m_vVolCloudDensityParams = v;
		break;
	case E3DPARAM_VOLCLOUD_MISC_PARAM:
		m_vVolCloudMiscParams = v;
		break;
	case E3DPARAM_VOLCLOUD_TILING_SIZE:
		m_vVolCloudTilingSize = v;
		break;
	case E3DPARAM_VOLCLOUD_TILING_OFFSET:
		m_vVolCloudTilingOffset = v;
		break;
	case E3DPARAM_NIGHSKY_MOON_DIRECTION: // moon direction is fixed per level or updated via FG node (E3DPARAM_SKY_MOONROTATION)
	default:
		assert(0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::GetGlobalParameter(E3DEngineParameter param, Vec3& v)
{
	switch (param)
	{
	case E3DPARAM_SKY_COLOR:
		v = GetSkyColor();
		break;
	case E3DPARAM_SUN_COLOR:
		v = GetSunColor();
		break;

	case E3DPARAM_SUN_SPECULAR_MULTIPLIER:
		v = Vec3(m_fSunSpecMult, 0, 0);
		break;

	case E3DPARAM_AMBIENT_GROUND_COLOR:
		v = m_vAmbGroundCol;
		break;
	case E3DPARAM_AMBIENT_MIN_HEIGHT:
		v = Vec3(m_fAmbMaxHeight, 0, 0);
		break;
	case E3DPARAM_AMBIENT_MAX_HEIGHT:
		v = Vec3(m_fAmbMinHeight, 0, 0);
		break;
	case E3DPARAM_SKY_HIGHLIGHT_POS:
		v = m_vSkyHightlightPos;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_COLOR:
		v = m_vSkyHightlightCol;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_SIZE:
		v = Vec3(m_fSkyHighlightSize, 0, 0);
		break;
	case E3DPARAM_VOLFOG_RAMP:
		v = m_volFogRamp;
		break;
	case E3DPARAM_VOLFOG_SHADOW_RANGE:
		v = m_volFogShadowRange;
		break;
	case E3DPARAM_VOLFOG_SHADOW_DARKENING:
		v = m_volFogShadowDarkening;
		break;
	case E3DPARAM_VOLFOG_SHADOW_ENABLE:
		v = m_volFogShadowEnable;
		break;
	case E3DPARAM_VOLFOG2_CTRL_PARAMS:
		v = m_volFog2CtrlParams;
		break;
	case E3DPARAM_VOLFOG2_SCATTERING_PARAMS:
		v = m_volFog2ScatteringParams;
		break;
	case E3DPARAM_VOLFOG2_RAMP:
		v = m_volFog2Ramp;
		break;
	case E3DPARAM_VOLFOG2_COLOR:
		v = m_volFog2Color;
		break;
	case E3DPARAM_VOLFOG2_GLOBAL_DENSITY:
		v = m_volFog2GlobalDensity;
		break;
	case E3DPARAM_VOLFOG2_HEIGHT_DENSITY:
		v = m_volFog2HeightDensity;
		break;
	case E3DPARAM_VOLFOG2_HEIGHT_DENSITY2:
		v = m_volFog2HeightDensity2;
		break;
	case E3DPARAM_VOLFOG2_COLOR1:
		v = m_volFog2Color1;
		break;
	case E3DPARAM_VOLFOG2_COLOR2:
		v = m_volFog2Color2;
		break;
	case E3DPARAM_SKYLIGHT_RAYLEIGH_INSCATTER:
		v = m_fogColorSkylightRayleighInScatter;
		break;
	case E3DPARAM_NIGHSKY_HORIZON_COLOR:
		v = m_nightSkyHorizonCol;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_COLOR:
		v = m_nightSkyZenithCol;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_SHIFT:
		v = Vec3(m_nightSkyZenithColShift, 0, 0);
		break;
	case E3DPARAM_NIGHSKY_STAR_INTENSITY:
		v = Vec3(m_nightSkyStarIntensity, 0, 0);
		break;
	case E3DPARAM_NIGHSKY_MOON_DIRECTION:
		v = m_moonDirection;
		break;
	case E3DPARAM_NIGHSKY_MOON_COLOR:
		v = m_nightMoonCol;
		break;
	case E3DPARAM_NIGHSKY_MOON_SIZE:
		v = Vec3(m_nightMoonSize, 0, 0);
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR:
		v = m_nightMoonInnerCoronaCol;
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE:
		v = Vec3(m_nightMoonInnerCoronaScale, 0, 0);
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR:
		v = m_nightMoonOuterCoronaCol;
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE:
		v = Vec3(m_nightMoonOuterCoronaScale, 0, 0);
		break;
	case E3DPARAM_SKY_MOONROTATION:
		v = Vec3(m_moonRotationLatitude, m_moonRotationLongitude, 0);
		break;
	case E3DPARAM_OCEANFOG_COLOR:
		v = m_oceanFogColor;
		break;
	case E3DPARAM_OCEANFOG_DENSITY:
		v = Vec3(m_oceanFogDensity, 0, 0);
		break;
	case E3DPARAM_SKYBOX_MULTIPLIER:
		v = Vec3(m_skyboxMultiplier, 0, 0);
		break;
	case E3DPARAM_DAY_NIGHT_INDICATOR:
		v = Vec3(m_dayNightIndicator, 0, 0);
		break;
	case E3DPARAM_FOG_COLOR2:
		v = m_fogColor2;
		break;
	case E3DPARAM_FOG_RADIAL_COLOR:
		v = m_fogColorRadial;
		break;
	case E3DPARAM_VOLFOG_HEIGHT_DENSITY:
		v = m_volFogHeightDensity;
		break;
	case E3DPARAM_VOLFOG_HEIGHT_DENSITY2:
		v = m_volFogHeightDensity2;
		break;
	case E3DPARAM_VOLFOG_GRADIENT_CTRL:
		v = m_volFogGradientCtrl;
		break;
	case E3DPARAM_VOLFOG_GLOBAL_DENSITY:
		v = Vec3(m_volFogGlobalDensity, m_volFogGlobalDensityMultiplierLDR, m_volFogFinalDensityClamp);
		break;
	case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR:
		v = Vec3(m_pPhotoFilterColor.x, m_pPhotoFilterColor.y, m_pPhotoFilterColor.z);
		break;
	case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY:
		v = Vec3(m_fPhotoFilterColorDensity, 0, 0);
		break;
	case E3DPARAM_COLORGRADING_FILTERS_GRAIN:
		v = Vec3(m_fGrainAmount, 0, 0);
		break;
	case E3DPARAM_HDR_FILMCURVE_SHOULDER_SCALE:
		v = Vec3(m_vHDRFilmCurveParams.x, 0, 0);
		break;
	case E3DPARAM_HDR_FILMCURVE_LINEAR_SCALE:
		v = Vec3(m_vHDRFilmCurveParams.y, 0, 0);
		break;
	case E3DPARAM_HDR_FILMCURVE_TOE_SCALE:
		v = Vec3(m_vHDRFilmCurveParams.z, 0, 0);
		break;
	case E3DPARAM_HDR_FILMCURVE_WHITEPOINT:
		v = Vec3(m_vHDRFilmCurveParams.w, 0, 0);
		break;
	case E3DPARAM_HDR_EYEADAPTATION_PARAMS:
		v = m_vHDREyeAdaptation;
		break;
	case E3DPARAM_HDR_EYEADAPTATION_PARAMS_LEGACY:
		v = m_vHDREyeAdaptationLegacy;
		break;
	case E3DPARAM_HDR_BLOOM_AMOUNT:
		v = Vec3(m_fHDRBloomAmount, 0, 0);
		break;
	case E3DPARAM_HDR_COLORGRADING_COLOR_SATURATION:
		v = Vec3(m_fHDRSaturation, 0, 0);
		break;
	case E3DPARAM_HDR_COLORGRADING_COLOR_BALANCE:
		v = m_vColorBalance;
		break;
	case E3DPARAM_CLOUDSHADING_MULTIPLIERS:
		v = Vec3(m_fCloudShadingSunLightMultiplier, m_fCloudShadingSkyLightMultiplier, 0);
		break;
	case E3DPARAM_CLOUDSHADING_SUNCOLOR:
		v = m_vCloudShadingCustomSunColor;
		break;
	case E3DPARAM_CLOUDSHADING_SKYCOLOR:
		v = m_vCloudShadingCustomSkyColor;
		break;
	case E3DPARAM_VOLCLOUD_ATMOSPHERIC_SCATTERING:
		v = m_vVolCloudAtmosphericScattering;
		break;
	case E3DPARAM_VOLCLOUD_GEN_PARAMS:
		v = m_vVolCloudGenParams;
		break;
	case E3DPARAM_VOLCLOUD_SCATTERING_LOW:
		v = m_vVolCloudScatteringLow;
		break;
	case E3DPARAM_VOLCLOUD_SCATTERING_HIGH:
		v = m_vVolCloudScatteringHigh;
		break;
	case E3DPARAM_VOLCLOUD_GROUND_COLOR:
		v = m_vVolCloudGroundColor;
		break;
	case E3DPARAM_VOLCLOUD_SCATTERING_MULTI:
		v = m_vVolCloudScatteringMulti;
		break;
	case E3DPARAM_VOLCLOUD_WIND_ATMOSPHERIC:
		v = m_vVolCloudWindAtmospheric;
		break;
	case E3DPARAM_VOLCLOUD_TURBULENCE:
		v = m_vVolCloudTurbulence;
		break;
	case E3DPARAM_VOLCLOUD_ENV_PARAMS:
		v = m_vVolCloudEnvParams;
		break;
	case E3DPARAM_VOLCLOUD_GLOBAL_NOISE_SCALE:
		v = m_vVolCloudGlobalNoiseScale;
		break;
	case E3DPARAM_VOLCLOUD_RENDER_PARAMS:
		v = m_vVolCloudRenderParams;
		break;
	case E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_SCALE:
		v = m_vVolCloudTurbulenceNoiseScale;
		break;
	case E3DPARAM_VOLCLOUD_TURBULENCE_NOISE_PARAMS:
		v = m_vVolCloudTurbulenceNoiseParams;
		break;
	case E3DPARAM_VOLCLOUD_DENSITY_PARAMS:
		v = m_vVolCloudDensityParams;
		break;
	case E3DPARAM_VOLCLOUD_MISC_PARAM:
		v = m_vVolCloudMiscParams;
		break;
	case E3DPARAM_VOLCLOUD_TILING_SIZE:
		v = m_vVolCloudTilingSize;
		break;
	case E3DPARAM_VOLCLOUD_TILING_OFFSET:
		v = m_vVolCloudTilingOffset;
		break;
	default:
		assert(0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void C3DEngine::SetCachedShadowBounds(const AABB& shadowBounds, float fAdditionalCascadesScale)
{
	const Vec3 boxSize = shadowBounds.GetSize();
	const bool isValid = boxSize.x > 0 && boxSize.y > 0 && boxSize.z > 0;

	m_CachedShadowsBounds = isValid ? shadowBounds : AABB(AABB::RESET);
	m_fCachedShadowsCascadeScale = fAdditionalCascadesScale;

	m_nCachedShadowsUpdateStrategy = ShadowMapFrustum::ShadowCacheData::eFullUpdate;
}

//////////////////////////////////////////////////////////////////////////

void C3DEngine::SetRecomputeCachedShadows(uint nUpdateStrategy)
{
	m_nCachedShadowsUpdateStrategy = nUpdateStrategy;

	// refresh cached shadow casters
	if (GetCVars()->e_DynamicDistanceShadows != 0)
	{
		for (int nSID = 0; nSID < m_pObjectsTree.Count(); nSID++)
			m_pObjectsTree[nSID]->MarkAsUncompiled();
	}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::SetShadowsCascadesBias(const float* pCascadeConstBias, const float* pCascadeSlopeBias)
{
	memcpy(m_pShadowCascadeConstBias, pCascadeConstBias, sizeof(float) * MAX_SHADOW_CASCADES_NUM);
	memcpy(m_pShadowCascadeSlopeBias, pCascadeSlopeBias, sizeof(float) * MAX_SHADOW_CASCADES_NUM);
}

int C3DEngine::GetShadowsCascadeCount(const CDLight* pLight) const
{
	int nCascadeCount = m_eShadowMode == ESM_HIGHQUALITY ? MAX_GSM_LODS_NUM : GetCVars()->e_GsmLodsNum;
	return clamp_tpl(nCascadeCount, 0, MAX_GSM_LODS_NUM);
}

//////////////////////////////////////////////////////////////////////////
bool C3DEngine::CheckIntersectClouds(const Vec3& p1, const Vec3& p2)
{
	return m_pCloudsManager->CheckIntersectClouds(p1, p2);
}

void C3DEngine::OnRenderMeshDeleted(IRenderMesh* pRenderMesh)
{
	if (m_pDecalManager)
		m_pDecalManager->OnRenderMeshDeleted(pRenderMesh);
}

bool C3DEngine::RenderMeshRayIntersection(IRenderMesh* pRenderMesh, SRayHitInfo& hitInfo, IMaterial* pCustomMtl)
{
	return CRenderMeshUtils::RayIntersection(pRenderMesh, hitInfo, pCustomMtl);
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::FreeRenderNodeTempData(SRenderNodeTempData** ppTempData)
{
	m_visibleNodesManager.MarkForDelete(*ppTempData);
	*ppTempData = 0;
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::CreateRenderNodeTempData(SRenderNodeTempData** ppInputTempData, IRenderNode* pRNode, const SRenderingPassInfo& passInfo)
{
	// g_renderNodeTempDataLock lock scope
	{
		CryAutoCriticalSectionNoRecursive lock(g_renderNodeTempDataLock);

		FUNCTION_PROFILER_3DENGINE;

		if (*ppInputTempData && (*ppInputTempData)->IsValid() && !(pRNode->m_nInternalFlags & IRenderNode::PERMANENT_RO_INVALID))
			return; // check if another thread already initialized temp data

		if (pRNode)
		{
			pRNode->m_nInternalFlags &= ~IRenderNode::PERMANENT_RO_INVALID;
		}

		if (*ppInputTempData)
		{
			FreeRenderNodeTempData(ppInputTempData);
		}

		SRenderNodeTempData* pNewTempData = m_visibleNodesManager.AllocateTempData(passInfo.GetFrameID());
		m_visibleNodesManager.SetLastSeenFrame(pNewTempData, passInfo);
		pNewTempData->userData.pOwnerNode = pRNode;
		*ppInputTempData = pNewTempData;
	}

	if (pRNode)
	{
		pRNode->OnRenderNodeBecomeVisible(passInfo); // Internally uses the just assigned RNTmpData pointer i.e IRenderNode::m_pRNTmpData ...

		if (IVisArea* pVisArea = pRNode->GetEntityVisArea())
			pRNode->m_pTempData->userData.m_pClipVolume = pVisArea;
		else if (GetClipVolumeManager()->IsClipVolumeRequired(pRNode))
			GetClipVolumeManager()->UpdateEntityClipVolume(pRNode->GetPos(), pRNode);
	}
}

//////////////////////////////////////////////////////////////////////////
bool C3DEngine::CheckAndCreateRenderNodeTempData(SRenderNodeTempData** ppTempData, IRenderNode* pRNode, const SRenderingPassInfo& passInfo)
{
	assert(pRNode);

	SRenderNodeTempData* pCurrentTempData = *ppTempData;

	bool bValid = (pCurrentTempData && pCurrentTempData->IsValid() && !(pRNode->m_nInternalFlags & IRenderNode::PERMANENT_RO_INVALID));

	// detect render resources modification (for example because of mesh streaming or material editing)
	if (bValid && pCurrentTempData->userData.nStatObjLastModificationId)
	{
		if (GetObjManager()->GetResourcesModificationChecksum(pRNode) != pCurrentTempData->userData.nStatObjLastModificationId)
		{
			pRNode->InvalidatePermanentRenderObject();
			bValid = false;
		}
	}

	if (bValid)
	{
		return m_visibleNodesManager.SetLastSeenFrame(pCurrentTempData, passInfo);
	}
	else
	{
		CreateRenderNodeTempData(ppTempData, pRNode, passInfo);
	}

	return true;
}

void C3DEngine::CopyObjectsByType(EERType objType, const AABB* pBox, PodArray<IRenderNode*>* plstObjects)
{
	GetObjectsByTypeGlobal(*plstObjects, objType, pBox);

	if (GetVisAreaManager())
		GetVisAreaManager()->GetObjectsByType(*plstObjects, objType, pBox);
}

void C3DEngine::CopyObjects(const AABB* pBox, PodArray<IRenderNode*>* plstObjects)
{
	for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); nSID++)
		if (IsSegmentSafeToUse(nSID))
			m_pObjectsTree[nSID]->GetObjects(*plstObjects, pBox);

	if (GetVisAreaManager())
		GetVisAreaManager()->GetObjects(*plstObjects, pBox);
}

uint32 C3DEngine::GetObjectsByType(EERType objType, IRenderNode** pObjects)
{
	PodArray<IRenderNode*> lstObjects;
	CopyObjectsByType(objType, NULL, &lstObjects);
	if (pObjects && !lstObjects.IsEmpty())
		memcpy(pObjects, &lstObjects[0], lstObjects.GetDataSize());
	return lstObjects.Count();
}

uint32 C3DEngine::GetObjectsByTypeInBox(EERType objType, const AABB& bbox, IRenderNode** pObjects)
{
	PodArray<IRenderNode*> lstObjects;
	CopyObjectsByType(objType, &bbox, &lstObjects);
	if (pObjects && !lstObjects.IsEmpty())
		memcpy(pObjects, &lstObjects[0], lstObjects.GetDataSize());
	return lstObjects.Count();
}

void C3DEngine::GetObjectsByTypeInBox(EERType objType, const AABB& bbox, PodArray<IRenderNode*>* pLstObjects)
{
	CopyObjectsByType(objType, &bbox, pLstObjects);
}

uint32 C3DEngine::GetObjectsInBox(const AABB& bbox, IRenderNode** pObjects)
{
	PodArray<IRenderNode*> lstObjects;
	CopyObjects(&bbox, &lstObjects);
	if (pObjects && !lstObjects.IsEmpty())
		memcpy(pObjects, &lstObjects[0], lstObjects.GetDataSize());
	return lstObjects.Count();
}

uint32 C3DEngine::GetObjectsByFlags(uint dwFlags, IRenderNode** pObjects /* =0 */)
{
	PodArray<IRenderNode*> lstObjects;

	for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); nSID++)
		if (Get3DEngine()->m_pObjectsTree[nSID])
			Get3DEngine()->m_pObjectsTree[nSID]->GetObjectsByFlags(dwFlags, lstObjects);

	if (GetVisAreaManager())
		GetVisAreaManager()->GetObjectsByFlags(dwFlags, lstObjects);

	if (pObjects && !lstObjects.IsEmpty())
		memcpy(pObjects, &lstObjects[0], lstObjects.GetDataSize());
	return lstObjects.Count();
}

void C3DEngine::ObjectsTreeMarkAsUncompiled(const IRenderNode* pRenderNode)
{
	for (int nSID = 0; nSID < Cry3DEngineBase::Get3DEngine()->m_pObjectsTree.Count(); nSID++)
	{
		if (COctreeNode* curNode = Cry3DEngineBase::Get3DEngine()->m_pObjectsTree[nSID])
			curNode->MarkAsUncompiled(pRenderNode);
	}

	if(GetVisAreaManager())
		GetVisAreaManager()->MarkAllSectorsAsUncompiled(pRenderNode);
}

void C3DEngine::OnObjectModified(IRenderNode* pRenderNode, uint dwFlags)
{
	if ((dwFlags & (ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS)) != 0)
		SetRecomputeCachedShadows(ShadowMapFrustum::ShadowCacheData::eFullUpdateTimesliced);
}

int SImageInfo::GetMemoryUsage()
{
	int nSize = 0;
	if (detailInfo.pImgMips[0])
		nSize += (int)((float)(detailInfo.nDim * detailInfo.nDim * sizeof(ColorB)) * 1.3f);
	if (baseInfo.pImgMips[0])
		nSize += (int)((float)(baseInfo.nDim * baseInfo.nDim * sizeof(ColorB)) * 1.3f);
	return nSize;
}

byte** C3DEngine::AllocateMips(byte* pImage, int nDim, byte** pImageMips)
{
	memset(pImageMips, 0, SImageSubInfo::nMipsNum * sizeof(pImageMips[0]));

	pImageMips[0] = new byte[nDim * nDim * sizeof(ColorB)];
	memcpy(pImageMips[0], pImage, nDim * nDim * sizeof(ColorB));

	ColorB* pMipMain = (ColorB*)pImageMips[0];

	for (int nMip = 1; (nDim >> nMip) && nMip < SImageSubInfo::nMipsNum; nMip++)
	{
		int nDimMip = nDim >> nMip;

		int nSubSize = 1 << nMip;

		pImageMips[nMip] = new byte[nDimMip * nDimMip * sizeof(ColorB)];

		ColorB* pMipThis = (ColorB*)pImageMips[nMip];

		for (int x = 0; x < nDimMip; x++)
		{
			for (int y = 0; y < nDimMip; y++)
			{
				ColorF colSumm(0, 0, 0, 0);
				float fCount = 0;
				for (int _x = x * nSubSize - nSubSize / 2; _x < x * nSubSize + nSubSize + nSubSize / 2; _x++)
				{
					for (int _y = y * nSubSize - nSubSize / 2; _y < y * nSubSize + nSubSize + nSubSize / 2; _y++)
					{
						int nMask = nDim - 1;
						int id = (_x & nMask) * nDim + (_y & nMask);
						colSumm.r += 1.f / 255.f * pMipMain[id].r;
						colSumm.g += 1.f / 255.f * pMipMain[id].g;
						colSumm.b += 1.f / 255.f * pMipMain[id].b;
						colSumm.a += 1.f / 255.f * pMipMain[id].a;
						fCount++;
					}
				}

				colSumm /= fCount;

				colSumm.Clamp(0, 1);

				pMipThis[x * nDimMip + y] = colSumm;
			}
		}
	}

	return pImageMips;
}

void C3DEngine::SetTerrainLayerBaseTextureData(int nLayerId, byte*, int, const char* pImgFileName, IMaterial* pMat, float fBr, float fTiling, int nDetailSurfTypeId, float fLayerTilingDetail, float fSpecularAmount, float fSortOrder, ColorF layerFilterColor, float fUseRemeshing, bool bShowSelection)
{
}

SImageInfo* C3DEngine::GetBaseTextureData(int nLayerId)
{
	if (nLayerId < 0 || nLayerId >= m_arrBaseTextureData.Count())//|| !m_arrBaseTextureData[nLayerId].baseInfo.nDim)
	{
		return NULL;
	}

	return &m_arrBaseTextureData[nLayerId];
}

SImageInfo* C3DEngine::GetBaseTextureDataFromSurfType(int nLayerId)
{
	assert(nLayerId >= 0 && nLayerId < SRangeInfo::e_max_surface_types);

	if (nLayerId < 0 || nLayerId >= m_arrBaseTextureData.Count() || !m_arrBaseTextureData[nLayerId].baseInfo.nDim)
		return NULL;

	return &m_arrBaseTextureData[nLayerId];
}

void C3DEngine::RegisterForStreaming(IStreamable* pObj)
{
	if (GetObjManager())
		GetObjManager()->RegisterForStreaming(pObj);
}

void C3DEngine::UnregisterForStreaming(IStreamable* pObj)
{
	if (GetObjManager())
		GetObjManager()->UnregisterForStreaming(pObj);
}

SImageSubInfo* C3DEngine::GetImageInfo(const char* pName)
{
	if (m_imageInfos.find(string(pName)) != m_imageInfos.end())
		return m_imageInfos[string(pName)];

	return NULL;
}

SImageSubInfo* C3DEngine::RegisterImageInfo(byte** pMips, int nDim, const char* pName)
{
	if (m_imageInfos.find(string(pName)) != m_imageInfos.end())
		return m_imageInfos[string(pName)];

	assert(pMips && pMips[0]);

	SImageSubInfo* pImgSubInfo = new SImageSubInfo;

	pImgSubInfo->nDim = nDim;

	int nMipDim = pImgSubInfo->nDim;

	for (int m = 0; m < SImageSubInfo::nMipsNum && nMipDim; m++)
	{
		pImgSubInfo->pImgMips[m] = new byte[nMipDim * nMipDim * 4];

		memcpy(pImgSubInfo->pImgMips[m], pMips[m], nMipDim * nMipDim * 4);

		nMipDim /= 2;
	}

	const string strFileName = pName;

	pImgSubInfo->nReady = 1;

	m_imageInfos[strFileName] = pImgSubInfo;

	return pImgSubInfo;
}

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
const char* gVoxelEditOperationNames[eveoLast] =
{
	"None",
	"Soft Create",
	"Soft Substract",
	"Hard Create",
	"Hard Substract",
	"Material",
	"Soft Base Color",
	"Blur Positive",
	"Blur Negative",
	"Copy Terrain Pos",
	"Copy Terrain Neg",
	"Pick Height",
	"Integrate Pos",
	"Integrate Neg",
	"Force LOD",
	"Limit LOD",
};
#endif

const char* C3DEngine::GetVoxelEditOperationName(EVoxelEditOperation eOperation)
{
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	return gVoxelEditOperationNames[eOperation];
#else
	return 0;
#endif
}

void C3DEngine::SyncProcessStreamingUpdate()
{
	if (m_pObjManager)
		m_pObjManager->ProcessObjectsStreaming_Finish();
}

void C3DEngine::SetScreenshotCallback(IScreenshotCallback* pCallback)
{
	m_pScreenshotCallback = pCallback;
}

void C3DEngine::ActivateObjectsLayer(uint16 nLayerId, bool bActivate, bool bPhys, bool bObjects, bool bStaticLights, const char* pLayerName, IGeneralMemoryHeap* pHeap, bool bCheckLayerActivation /*=true*/)
{
	if (bCheckLayerActivation && !IsAreaActivationInUse())
		return;

	if (bActivate)
		m_bLayersActivated = true;

	// keep track of active layers
	m_arrObjectLayersActivity.CheckAllocated(nLayerId + 1);
	m_arrObjectLayersActivity[nLayerId].bActive = bActivate;

	if (bActivate && m_nFramesSinceLevelStart <= 1)
		m_vPrevMainFrameCamPos.Set(-1000000.f, -1000000.f, -1000000.f);

	FUNCTION_PROFILER_3DENGINE;

	PrintMessage("%s object layer %s (Id = %d) (LevelFrameId = %d)", bActivate ? "Activating" : "Deactivating", pLayerName, nLayerId, m_nFramesSinceLevelStart);
	INDENT_LOG_DURING_SCOPE();

	if (bObjects)
	{
		for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); nSID++)
			if (IsSegmentSafeToUse(nSID))
				m_pObjectsTree[nSID]->ActivateObjectsLayer(nLayerId, bActivate, bPhys, pHeap, m_arrObjectLayersActivity[nLayerId].objectsBox);

		if (m_pVisAreaManager)
			m_pVisAreaManager->ActivateObjectsLayer(nLayerId, bActivate, bPhys, pHeap, m_arrObjectLayersActivity[nLayerId].objectsBox);
	}

	if (bStaticLights)
	{
		for (size_t i = 0; i < m_lstStaticLights.size(); ++i)
		{
			ILightSource* pLight = m_lstStaticLights[i];
			if (pLight->GetLayerId() == nLayerId)
				pLight->SetRndFlags(ERF_HIDDEN, !bActivate);
		}
	}
}

bool C3DEngine::IsObjectsLayerHidden(uint16 nLayerId, const AABB& objBox)
{
	if (IsAreaActivationInUse() && nLayerId && nLayerId < 0xFFFF)
	{
		m_arrObjectLayersActivity.CheckAllocated(nLayerId + 1);

		if (m_arrObjectLayersActivity[nLayerId].objectsBox.GetVolume())
			m_arrObjectLayersActivity[nLayerId].objectsBox.Add(objBox);
		else
			m_arrObjectLayersActivity[nLayerId].objectsBox = objBox;

		return !m_arrObjectLayersActivity[nLayerId].bActive;
	}

	return false;
}

void C3DEngine::GetLayerMemoryUsage(uint16 nLayerId, ICrySizer* pSizer, int* pNumBrushes, int* pNumDecals) const
{
	if (pNumBrushes)
		*pNumBrushes = 0;
	if (pNumDecals)
		*pNumDecals = 0;

	for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); ++nSID)
		if (m_pObjectsTree[nSID])
			m_pObjectsTree[nSID]->GetLayerMemoryUsage(nLayerId, pSizer, pNumBrushes, pNumDecals);
}

void C3DEngine::SkipLayerLoading(uint16 nLayerId, bool bClearList)
{
	if (bClearList)
		m_skipedLayers.clear();
	m_skipedLayers.insert(nLayerId);
}

bool C3DEngine::IsLayerSkipped(uint16 nLayerId)
{
	return m_skipedLayers.find(nLayerId) != m_skipedLayers.end() ? true : false;
}

void C3DEngine::PrecacheCharacter(IRenderNode* pObj, const float fImportance, ICharacterInstance* pCharacter, IMaterial* pSlotMat, const Matrix34& matParent, const float fEntDistance, const float fScale, int nMaxDepth, bool bForceStreamingSystemUpdate, const SRenderingPassInfo& passInfo)
{
	if (m_pObjManager)
	{
		m_pObjManager->PrecacheCharacter(pObj, fImportance, pCharacter, pSlotMat, matParent, fEntDistance, fScale, nMaxDepth, false, true, 0);
		if (bForceStreamingSystemUpdate)
		{
			m_pObjManager->ProcessObjectsStreaming(passInfo);
		}
	}
}

void C3DEngine::PrecacheRenderNode(IRenderNode* pObj, float fEntDistanceReal)
{
	FUNCTION_PROFILER_3DENGINE;

	//	PrintMessage("==== PrecacheRenderNodePrecacheRenderNode: %s ====", pObj->GetName());

	if (m_pObjManager)
	{
		int dwOldRndFlags = pObj->m_dwRndFlags;
		pObj->m_dwRndFlags &= ~ERF_HIDDEN;

		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->pSystem->GetViewCamera());

		m_pObjManager->UpdateRenderNodeStreamingPriority(pObj, fEntDistanceReal, 1.0f, fEntDistanceReal < GetFloatCVar(e_StreamCgfFastUpdateMaxDistance), passInfo, true);
		pObj->m_dwRndFlags = dwOldRndFlags;
	}
}

void C3DEngine::CleanUpOldDecals()
{
	FUNCTION_PROFILER_3DENGINE;
	static uint32 nLastIndex = 0;
	const uint32 nDECALS_PER_FRAME = 50;

	if (uint32 nNumDecalRenderNodes = m_decalRenderNodes.size())
	{
		for (uint32 i = 0, end = __min(nDECALS_PER_FRAME, nNumDecalRenderNodes); i < end; ++i, ++nLastIndex)
		{
			// wrap around at the end to restart at the beginning
			if (nLastIndex >= nNumDecalRenderNodes)
			{
				nLastIndex = 0;
			}

			m_decalRenderNodes[nLastIndex]->CleanUpOldDecals();
		}
	}
}

void C3DEngine::UpdateRenderTypeEnableLookup()
{
	SetRenderNodeTypeEnabled(eERType_RenderProxy, (GetCVars()->e_Entities != 0));
	SetRenderNodeTypeEnabled(eERType_Brush, (GetCVars()->e_Brushes != 0));
	SetRenderNodeTypeEnabled(eERType_Vegetation, (GetCVars()->e_Vegetation != 0));
}

void C3DEngine::SetRenderNodeMaterialAtPosition(EERType eNodeType, const Vec3& vPos, IMaterial* pMat)
{
	PodArray<IRenderNode*> lstObjects;

	AABB aabbPos(vPos - Vec3(.1f, .1f, .1f), vPos + Vec3(.1f, .1f, .1f));

	GetObjectsByTypeGlobal(lstObjects, eNodeType, &aabbPos);

	if (GetVisAreaManager())
		GetVisAreaManager()->GetObjectsByType(lstObjects, eNodeType, &aabbPos);

	for (int i = 0; i < lstObjects.Count(); i++)
	{
		PrintMessage("Game changed render node material: %s EERType:%d pos: (%d,%d,%d)",
		             pMat ? pMat->GetName() : "NULL",
		             (int)eNodeType,
		             (int)vPos.x, (int)vPos.y, (int)vPos.z);

		lstObjects[i]->SetMaterial(pMat);
	}
}

void C3DEngine::OverrideCameraPrecachePoint(const Vec3& vPos)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_vStreamPreCacheCameras[0].vPosition = vPos;
		m_pObjManager->m_bCameraPrecacheOverridden = true;
	}
}

int C3DEngine::AddPrecachePoint(const Vec3& vPos, const Vec3& vDir, float fTimeOut, float fImportanceFactor)
{
	if (m_pObjManager)
	{
		if (m_pObjManager->m_vStreamPreCachePointDefs.size() >= CObjManager::MaxPrecachePoints)
		{
			size_t nOldestIdx = 0;
			int nOldestId = INT_MAX;
			for (size_t i = 1, c = m_pObjManager->m_vStreamPreCachePointDefs.size(); i < c; ++i)
			{
				if (m_pObjManager->m_vStreamPreCachePointDefs[i].nId < nOldestId)
				{
					nOldestIdx = i;
					nOldestId = m_pObjManager->m_vStreamPreCachePointDefs[i].nId;
				}
			}

			assert(nOldestIdx > 0);

			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Precache points full - evicting oldest (%f, %f, %f)",
			           m_pObjManager->m_vStreamPreCacheCameras[nOldestIdx].vPosition.x,
			           m_pObjManager->m_vStreamPreCacheCameras[nOldestIdx].vPosition.y,
			           m_pObjManager->m_vStreamPreCacheCameras[nOldestIdx].vPosition.z);

			m_pObjManager->m_vStreamPreCachePointDefs.DeleteFastUnsorted((int)nOldestIdx);
			m_pObjManager->m_vStreamPreCacheCameras.DeleteFastUnsorted((int)nOldestIdx);
		}

		SObjManPrecachePoint pp;
		pp.nId = m_pObjManager->m_nNextPrecachePointId++;
		pp.expireTime = gEnv->pTimer->GetAsyncTime() + CTimeValue(fTimeOut);
		SObjManPrecacheCamera pc;
		pc.vPosition = vPos;
		pc.bbox = AABB(vPos, GetCVars()->e_StreamPredictionBoxRadius);
		pc.vDirection = vDir;
		pc.fImportanceFactor = fImportanceFactor;
		m_pObjManager->m_vStreamPreCachePointDefs.Add(pp);
		m_pObjManager->m_vStreamPreCacheCameras.Add(pc);
		//m_pObjManager->m_bCameraPrecacheOverridden = true;

		return pp.nId;
	}

	return -1;
}

void C3DEngine::ClearPrecachePoint(int id)
{
	if (m_pObjManager)
	{
		for (size_t i = 1, c = m_pObjManager->m_vStreamPreCachePointDefs.size(); i < c; ++i)
		{
			if (m_pObjManager->m_vStreamPreCachePointDefs[i].nId == id)
			{
				m_pObjManager->m_vStreamPreCachePointDefs.DeleteFastUnsorted((int)i);
				m_pObjManager->m_vStreamPreCacheCameras.DeleteFastUnsorted((int)i);
				break;
			}
		}
	}
}

void C3DEngine::ClearAllPrecachePoints()
{
	if (m_pObjManager)
	{
		m_pObjManager->m_vStreamPreCachePointDefs.resize(1);
		m_pObjManager->m_vStreamPreCacheCameras.resize(1);
	}
}

void C3DEngine::GetPrecacheRoundIds(int pRoundIds[MAX_STREAM_PREDICTION_ZONES])
{
	if (m_pObjManager)
	{
		pRoundIds[0] = m_pObjManager->m_nUpdateStreamingPrioriryRoundIdFast;
		pRoundIds[1] = m_pObjManager->m_nUpdateStreamingPrioriryRoundId;
	}
}

void static DrawMeter(float scale, float& x, float& y, int nWidth, int nHeight, IRenderAuxGeom* pAuxRenderer)
{
	vtx_idx indLines[8] =
	{
		0, 1, 1, 2,
		2, 3, 3, 0
	};

	const int c_yStepSizeTextMeter(8);

	const float barWidth = 0.20f; //normalised screen size
	const float yellowStart = 0.5f * barWidth;
	const float redStart = 0.75f * barWidth;

	Vec3 frame[4] =
	{
		Vec3((x - 1) / (float) nWidth,      (y - 1) / (float) nHeight,                        0),
		Vec3(x / (float) nWidth + barWidth, (y - 1) / (float) nHeight,                        0),
		Vec3(x / (float) nWidth + barWidth, (y + c_yStepSizeTextMeter + 1) / (float) nHeight, 0),
		Vec3((x - 1) / (float) nWidth,      (y + c_yStepSizeTextMeter + 1) / (float) nHeight, 0)
	};

	pAuxRenderer->DrawLines(frame, 4, indLines, 8, ColorB(255, 255, 255, 255));

	// draw meter itself
	vtx_idx indTri[6] =
	{
		0, 1, 2,
		0, 2, 3
	};

	// green part (0.0 <= scale <= 0.5)
	{
		float lScale(max(min(scale, 0.5f), 0.0f));

		Vec3 bar[4] =
		{
			Vec3(x / (float) nWidth,                     y / (float) nHeight,                          0),
			Vec3(x / (float) nWidth + lScale * barWidth, y / (float) nHeight,                          0),
			Vec3(x / (float) nWidth + lScale * barWidth, (y + c_yStepSizeTextMeter) / (float) nHeight, 0),
			Vec3(x / (float) nWidth,                     (y + c_yStepSizeTextMeter) / (float) nHeight, 0)
		};
		pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, ColorB(0, 255, 0, 255));
	}

	// green to yellow part (0.5 < scale <= 0.75)
	if (scale > 0.5f)
	{
		float lScale(min(scale, 0.75f));

		Vec3 bar[4] =
		{
			Vec3(x / (float) nWidth + yellowStart,       y / (float) nHeight,                          0),
			Vec3(x / (float) nWidth + lScale * barWidth, y / (float) nHeight,                          0),
			Vec3(x / (float) nWidth + lScale * barWidth, (y + c_yStepSizeTextMeter) / (float) nHeight, 0),
			Vec3(x / (float) nWidth + yellowStart,       (y + c_yStepSizeTextMeter) / (float) nHeight, 0)
		};

		float color[4];
		// get right color
		if (lScale <= 0.5f)
		{
			color[0] = 0;
			color[1] = 1;
			color[2] = 0;
			color[3] = 1;
		}
		else if (lScale <= 0.75f)
		{
			color[0] = (lScale - 0.5f) * 4.0f;
			color[1] = 1;
			color[2] = 0;
			color[3] = 1;
		}
		else if (lScale <= 1.0f)
		{
			color[0] = 1;
			color[1] = 1 - (lScale - 0.75f) * 4.0f;
			color[2] = 0;
			color[3] = 1;
		}
		else
		{
			float time(gEnv->pTimer->GetAsyncCurTime());
			float blink(sinf(time * 6.28f) * 0.5f + 0.5f);
			color[0] = 1;
			color[1] = blink;
			color[2] = blink;
			color[3] = 1;
		}

		ColorB colSegStart(0, 255, 0, 255);
		ColorB colSegEnd((uint8) (color[0] * 255), (uint8) (color[1] * 255), (uint8) (color[2] * 255), (uint8) (color[3] * 255));

		ColorB col[4] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);
	}

	// yellow to red part (0.75 < scale <= 1.0)
	if (scale > 0.75f)
	{
		float lScale(min(scale, 1.0f));

		Vec3 bar[4] =
		{
			Vec3(x / (float) nWidth + redStart,          y / (float) nHeight,                          0),
			Vec3(x / (float) nWidth + lScale * barWidth, y / (float) nHeight,                          0),
			Vec3(x / (float) nWidth + lScale * barWidth, (y + c_yStepSizeTextMeter) / (float) nHeight, 0),
			Vec3(x / (float) nWidth + redStart,          (y + c_yStepSizeTextMeter) / (float) nHeight, 0)
		};

		float color[4];
		if (lScale <= 0.5f)
		{
			color[0] = 0;
			color[1] = 1;
			color[2] = 0;
			color[3] = 1;
		}
		else if (lScale <= 0.75f)
		{
			color[0] = (lScale - 0.5f) * 4.0f;
			color[1] = 1;
			color[2] = 0;
			color[3] = 1;
		}
		else if (lScale <= 1.0f)
		{
			color[0] = 1;
			color[1] = 1 - (lScale - 0.75f) * 4.0f;
			color[2] = 0;
			color[3] = 1;
		}
		else
		{
			float time(gEnv->pTimer->GetAsyncCurTime());
			float blink(sinf(time * 6.28f) * 0.5f + 0.5f);
			color[0] = 1;
			color[1] = blink;
			color[2] = blink;
			color[3] = 1;
		}

		ColorB colSegStart(255, 255, 0, 255);
		ColorB colSegEnd((uint8) (color[0] * 255), (uint8) (color[1] * 255), (uint8) (color[2] * 255), (uint8) (color[3] * 255));

		ColorB col[4] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);

	}
}

///////////////////////////////////////////////////////////////////////////////
CCamera* C3DEngine::GetRenderingPassCamera(const CCamera& rCamera)
{
	threadID nThreadID = 0;
	gEnv->pRenderer->EF_Query(EFQ_RenderThreadList, nThreadID);
	CCamera* pCamera = ::new(m_RenderingPassCameras[nThreadID].push_back_new())CCamera(rCamera);
	return pCamera;
}

///////////////////////////////////////////////////////////////////////////////
int C3DEngine::GetZoomMode() const
{
	return m_nZoomMode;
}

///////////////////////////////////////////////////////////////////////////////
float C3DEngine::GetPrevZoomFactor()
{
	return m_fPrevZoomFactor;
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::SetZoomMode(int nZoomMode)
{
	m_nZoomMode = nZoomMode;
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::SetPrevZoomFactor(float fZoomFactor)
{
	m_fPrevZoomFactor = fZoomFactor;
}

void C3DEngine::GetCollisionClass(SCollisionClass& collclass, int tableIndex)
{
	if ((unsigned)tableIndex < m_collisionClasses.size())
		collclass = m_collisionClasses[tableIndex];
	else
		collclass = SCollisionClass(0, 0);
}

void C3DEngine::UpdateShaderItems()
{
	if (GetMatMan())
		GetMatMan()->UpdateShaderItems();
}
///////////////////////////////////////////////////////////////////////////////
void C3DEngine::SaveInternalState(struct IDataWriteStream& writer, const AABB& filterArea, const bool bTerrain, const uint32 objectMask)
{
	ITerrain* pTerrain = GetITerrain();
	IVisAreaManager* pVisAreaManager = GetIVisAreaManager();
	if ((NULL != pTerrain) && (NULL != pVisAreaManager))
	{
		const float levelBounds = 2048.0f;
		const uint8 terrainMask = bTerrain ? 1 : 0;

		// set export parameters
		SHotUpdateInfo info;
		info.nHeigtmap = bTerrain ? 1 : 0;
		info.nObjTypeMask = objectMask;
		info.areaBox = filterArea;
		info.pVisibleLayerMask = NULL; // not used during saving

		int objectCompiledDataSize = 0;
		int terrainCompiledDataSize = pTerrain->GetCompiledDataSize(&info);
		if (terrainCompiledDataSize > 0)
		{
			std::vector<uint8> outputData;
			outputData.resize(terrainCompiledDataSize);

			std::vector<struct IStatObj*>* pTempBrushTable = NULL;
			std::vector<struct IMaterial*>* pTempMatsTable = NULL;
			std::vector<struct IStatInstGroup*>* pTempVegGroupTable = NULL;

			pTerrain->GetCompiledData(
			  &outputData[0],
			  terrainCompiledDataSize,
			  &pTempBrushTable,
			  &pTempMatsTable,
			  &pTempVegGroupTable,
			  eLittleEndian,
			  &info);

			if (pTempBrushTable && pTempMatsTable && (objectMask != 0))
			{
				// VisAreas: we need this here for things depending on pTempBrushTable and pTempMatsTable
				objectCompiledDataSize = pVisAreaManager->GetCompiledDataSize(&info);
				if (objectCompiledDataSize > 0)
				{
					outputData.resize(objectCompiledDataSize + terrainCompiledDataSize);

					pVisAreaManager->GetCompiledData(
					  &outputData[terrainCompiledDataSize],
					  objectCompiledDataSize,
					  &pTempBrushTable,
					  &pTempMatsTable,
					  &pTempVegGroupTable,
					  eLittleEndian,
					  &info);
				}
			}

			// store the data
			writer << terrainMask;
			writer << objectMask;
			writer << info.areaBox.min.x;
			writer << info.areaBox.min.y;
			writer << info.areaBox.min.z;
			writer << info.areaBox.max.x;
			writer << info.areaBox.max.y;
			writer << info.areaBox.max.z;
			writer << terrainCompiledDataSize;
			writer << objectCompiledDataSize;
			writer << outputData;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::LoadInternalState(struct IDataReadStream& reader, const uint8* pVisibleLayersMask, const uint16* pLayerIdTranslation)
{
	SHotUpdateInfo info;
	info.nHeigtmap = reader.ReadUint8();
	info.nObjTypeMask = reader.ReadUint32();
	info.areaBox.min.x = reader.ReadFloat();
	info.areaBox.min.y = reader.ReadFloat();
	info.areaBox.min.z = reader.ReadFloat();
	info.areaBox.max.x = reader.ReadFloat();
	info.areaBox.max.y = reader.ReadFloat();
	info.areaBox.max.z = reader.ReadFloat();
	info.pVisibleLayerMask = pVisibleLayersMask;
	info.pLayerIdTranslation = pLayerIdTranslation;

	const uint32 terrainDataSize = reader.ReadUint32();
	const uint32 objectDataSize = reader.ReadUint32();

	// keep the CGF resident for the duration of the update
	gEnv->p3DEngine->LockCGFResources();

	// Flush rendering thread :( another length stuff that is unfortunately needed here because it crashes without it
	gEnv->pRenderer->FlushRTCommands(true, true, true);

	std::vector<struct IStatObj*>* pStatObjTable = NULL;
	std::vector<struct IMaterial*>* pMatTable = NULL;

	// load data
	std::vector<uint8> binaryData;
	reader << binaryData;

	ITerrain* pTerrain = gEnv->p3DEngine->GetITerrain();
	if (NULL != pTerrain)
	{
		pStatObjTable = NULL;
		pMatTable = NULL;

		pTerrain->SetCompiledData(
		  &binaryData[0],
		  terrainDataSize,
		  &pStatObjTable, &pMatTable,
		  true,
		  &info);
	}

	if (objectDataSize > 0)
	{
		IVisAreaManager* pVisAreaManager = gEnv->p3DEngine->GetIVisAreaManager();
		if (NULL != pVisAreaManager)
		{
			pVisAreaManager->SetCompiledData(
			  &binaryData[terrainDataSize], // object data is stored after terrain data
			  objectDataSize,
			  &pStatObjTable,
			  &pMatTable,
			  true,
			  &info);
		}
	}

	// release the lock on the resources
	// this will also relase all unused CGF resources
	gEnv->p3DEngine->UnlockCGFResources();
}

void C3DEngine::SetSegmentsManager(ISegmentsManager* pSegmentsManager)
{
	m_pSegmentsManager = pSegmentsManager;
}

bool C3DEngine::IsSegmentOperationInProgress()
{
	return m_bSegmentOperationInProgress;
}

void C3DEngine::SetSegmentOperationInProgress(bool bActive)
{
	m_bSegmentOperationInProgress = bActive;
}

void C3DEngine::OnCameraTeleport()
{
	//MarkRNTmpDataPoolForReset();
}

#ifndef _RELEASE

//////////////////////////////////////////////////////////////////////////
// onscreen infodebug code for e_debugDraw >= 100

//////////////////////////////////////////////////////////////////////////
void C3DEngine::AddObjToDebugDrawList(SObjectInfoToAddToDebugDrawList& objInfo)
{
	m_DebugDrawListMgr.AddObject(objInfo);
}

bool CDebugDrawListMgr::m_dumpLogRequested = false;
bool CDebugDrawListMgr::m_freezeRequested = false;
bool CDebugDrawListMgr::m_unfreezeRequested = false;
uint32 CDebugDrawListMgr::m_filter = I3DEngine::DLOT_ALL;

//////////////////////////////////////////////////////////////////////////
CDebugDrawListMgr::CDebugDrawListMgr()
{
	m_isFrozen = false;
	ClearFrameData();
	ClearConsoleCommandRequestVars();
	m_assets.reserve(32);     // just a reasonable value
	m_drawBoxes.reserve(256); // just a reasonable value
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::ClearFrameData()
{
	m_counter = 0;
	m_assetCounter = 0;
	m_assets.clear();
	m_drawBoxes.clear();
	m_indexLeastValueAsset = 0;
	CheckFilterCVar();
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::ClearConsoleCommandRequestVars()
{
	m_dumpLogRequested = false;
	m_freezeRequested = false;
	m_unfreezeRequested = false;
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::AddObject(I3DEngine::SObjectInfoToAddToDebugDrawList& newObjInfo)
{
	if (m_isFrozen)
		return;

	m_lock.Lock();

	m_counter++;

	// type is not always totally determined before this point
	if (newObjInfo.pClassName)
	{
		if (strcmp(newObjInfo.pClassName, "Brush") == 0) // TODO: could not find a better way of checking if it is a brush
			newObjInfo.type = I3DEngine::DLOT_BRUSH;
		else if (strcmp(newObjInfo.pClassName, "Vegetation") == 0) // TODO: could not find a better way of checking if it is vegetation
			newObjInfo.type = I3DEngine::DLOT_VEGETATION;
	}

	if (!ShouldFilterOutObject(newObjInfo))
	{
		TAssetInfo newAsset(newObjInfo);
		TObjectDrawBoxInfo newDrawBox(newObjInfo);

		TAssetInfo* pAssetDuplicated = FindDuplicate(newAsset);
		if (pAssetDuplicated)
		{
			pAssetDuplicated->numInstances++;
			newDrawBox.assetID = pAssetDuplicated->ID;
			m_drawBoxes.push_back(newDrawBox);
			pAssetDuplicated->drawCalls = max(pAssetDuplicated->drawCalls, newAsset.drawCalls);
		}
		else
		{
			newAsset.ID = m_assetCounter;
			newDrawBox.assetID = newAsset.ID;
			bool used = false;

			// list not full, so we add
			if (m_assets.size() < uint(Cry3DEngineBase::GetCVars()->e_DebugDrawListSize))
			{
				used = true;
				m_assets.push_back(newAsset);
			}
			else // if is full, only use it if value is greater than the current minimum, and then it substitutes the lowest slot
			{
				const TAssetInfo& leastValueAsset = m_assets[m_indexLeastValueAsset];
				if (leastValueAsset < newAsset)
				{
					used = true;
					m_assets[m_indexLeastValueAsset] = newAsset;
				}
			}
			if (used)
			{
				m_assetCounter++;
				m_drawBoxes.push_back(newDrawBox);
				FindNewLeastValueAsset();
			}
		}
	}

	m_lock.Unlock();
}

//////////////////////////////////////////////////////////////////////////
CDebugDrawListMgr::TAssetInfo* CDebugDrawListMgr::FindDuplicate(const TAssetInfo& asset)
{
	std::vector<TAssetInfo>::iterator iter = m_assets.begin();
	std::vector<TAssetInfo>::const_iterator endArray = m_assets.end();

	while (iter != endArray)
	{
		TAssetInfo& currAsset = *iter;
		if (currAsset.fileName == asset.fileName)
		{
			return &currAsset;
		}
		++iter;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CDebugDrawListMgr::ShouldFilterOutObject(const I3DEngine::SObjectInfoToAddToDebugDrawList& object)
{
	if (m_filter == I3DEngine::DLOT_ALL)
		return false;

	return (m_filter & object.type) == 0;
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::FindNewLeastValueAsset()
{
	uint32 size = m_assets.size();
	for (uint32 i = 0; i < size; ++i)
	{
		const TAssetInfo& currAsset = m_assets[i];
		const TAssetInfo& leastValueAsset = m_assets[m_indexLeastValueAsset];

		if (currAsset < leastValueAsset)
			m_indexLeastValueAsset = i;
	}
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::Update()
{
	m_lock.Lock();
	if (!m_isFrozen)
		std::sort(m_assets.begin(), m_assets.end(), CDebugDrawListMgr::SortComparison);
	gEnv->pRenderer->CollectDrawCallsInfoPerNode(true);
	// it displays values from the previous frame. This mean that if it is disabled, and then enabled again later on, it will display bogus values for 1 frame...but i dont care (yet)
	float X = 10.f;
	float Y = 100.f;
	if (m_isFrozen)
		PrintText(X, Y, Col_Red, "FROZEN DEBUGINFO");
	Y += 20.f;
	PrintText(X, Y, Col_White, "total assets: %d     Ordered by:                 Showing:", m_counter);
	PrintText(X + 240, Y, Col_Yellow, GetStrCurrMode());
	TMyStandardString filterStr;
	GetStrCurrFilter(filterStr);
	PrintText(X + 420, Y, Col_Yellow, filterStr.c_str());
	Y += 20.f;
	float XName = 270;

	const char* pHeaderStr = "";
	switch (Cry3DEngineBase::GetCVars()->e_DebugDraw)
	{
	case LM_TRI_COUNT:
		pHeaderStr = "   tris  " " meshMem  " "rep  " " type        ";
		break;
	case LM_VERT_COUNT:
		pHeaderStr = "  verts  " " meshMem  " "rep  " " type        ";
		break;
	case LM_DRAWCALLS:
		pHeaderStr = "draw Calls  " "   tris  " "rep  " " type        ";
		break;
	case LM_TEXTMEM:
		pHeaderStr = "  texMem  " " meshMem  " "rep  " " type        ";
		break;
	case LM_MESHMEM:
		pHeaderStr = " meshMem  " "  texMem  " "rep  " " type        ";
		break;
	}

	PrintText(X, Y, Col_White, pHeaderStr);
	const int standardNameSize = 48;
	PrintText(XName, Y, Col_White, "Entity (class)");
	PrintText(XName + (standardNameSize + 2) * 7, Y, Col_White, "File name");

	Y += 20.f;
	for (uint32 i = 0; i < m_assets.size(); ++i)
	{
		ColorF colorLine = Col_Cyan;
		if (Cry3DEngineBase::GetCVars()->e_DebugDrawListBBoxIndex - 1 == i)
			colorLine = Col_Blue;
		const TAssetInfo& currAsset = m_assets[i];
		TMyStandardString texMemoryStr;
		TMyStandardString meshMemoryStr;
		MemToString(currAsset.texMemory, texMemoryStr);
		MemToString(currAsset.meshMemory, meshMemoryStr);

		switch (Cry3DEngineBase::GetCVars()->e_DebugDraw)
		{
		case LM_TRI_COUNT:
			PrintText(X, Y + 20.f * i, colorLine, "%7d  " "%s  " "%3d  " "%s",
			          currAsset.numTris, meshMemoryStr.c_str(), currAsset.numInstances, GetAssetTypeName(currAsset.type));
			break;
		case LM_VERT_COUNT:
			PrintText(X, Y + 20.f * i, colorLine, "%7d  " "%s  " "%3d  " "%s",
			          currAsset.numVerts, meshMemoryStr.c_str(), currAsset.numInstances, GetAssetTypeName(currAsset.type));
			break;
		case LM_DRAWCALLS:
			{
				PrintText(X, Y + 20.f * i, colorLine, "  %5d     " "%7d  " "%3d  " "%s",
				          currAsset.drawCalls, currAsset.numTris, currAsset.numInstances, GetAssetTypeName(currAsset.type));
			}
			break;
		case LM_TEXTMEM:
			PrintText(X, Y + 20.f * i, colorLine, "%s  " "%s  " "%3d  " "%s",
			          texMemoryStr.c_str(), meshMemoryStr.c_str(), currAsset.numInstances, GetAssetTypeName(currAsset.type));
			break;
		case LM_MESHMEM:
			PrintText(X, Y + 20.f * i, colorLine, "%s  " "%s  " "%3d  " "%s",
			          meshMemoryStr.c_str(), texMemoryStr.c_str(), currAsset.numInstances, GetAssetTypeName(currAsset.type));
			break;
		}

		int filenameSep = 7 * (max(standardNameSize, int(currAsset.name.length())) + 2);
		float XFileName = XName + filenameSep;

		PrintText(XName, Y + 20.f * i, colorLine, currAsset.name.c_str());
		PrintText(XFileName, Y + 20.f * i, colorLine, currAsset.fileName.c_str());
	}

	if (Cry3DEngineBase::GetCVars()->e_DebugDrawListBBoxIndex > 0 && uint(Cry3DEngineBase::GetCVars()->e_DebugDrawListBBoxIndex - 1) < m_assets.size())
	{
		const TAssetInfo& assetInfo = m_assets[Cry3DEngineBase::GetCVars()->e_DebugDrawListBBoxIndex - 1];
		uint32 numBoxesDrawn = 0;
		for (uint32 i = 0; i < m_drawBoxes.size(); ++i)
		{
			const TObjectDrawBoxInfo& drawBox = m_drawBoxes[i];
			if (drawBox.assetID == assetInfo.ID)
			{
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(drawBox.bbox, drawBox.mat, true, ColorB(0, 0, 255, 100), eBBD_Faceted);
				numBoxesDrawn++;
				if (numBoxesDrawn >= assetInfo.numInstances)
					break;
			}
		}
	}

	if (m_freezeRequested)
		m_isFrozen = true;
	if (m_unfreezeRequested)
		m_isFrozen = false;
	if (m_dumpLogRequested)
		DumpLog();

	ClearConsoleCommandRequestVars();

	if (!m_isFrozen)
	{
		ClearFrameData();
		m_assets.reserve(Cry3DEngineBase::GetCVars()->e_DebugDrawListSize);
	}
	m_lock.Unlock();
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::PrintText(float x, float y, const ColorF& fColor, const char* label_text, ...)
{
	va_list args;
	va_start(args, label_text);
	SDrawTextInfo ti;
	ti.xscale = ti.yscale = 1.2f;
	ti.flags = eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace;
	{
		ti.color[0] = fColor[0];
		ti.color[1] = fColor[1];
		ti.color[2] = fColor[2];
		ti.color[3] = fColor[3];
	}
	gEnv->pRenderer->DrawTextQueued(Vec3(x, y, 0.5f), ti, label_text, args);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::MemToString(uint32 memVal, TMyStandardString& outStr)
{
	if (memVal < 1024 * 1024)
		outStr.Format("%5.1f kb", memVal / (1024.f));
	else
		outStr.Format("%5.1f MB", memVal / (1024.f * 1024.f));
}

//////////////////////////////////////////////////////////////////////////
bool CDebugDrawListMgr::TAssetInfo::operator<(const TAssetInfo& other) const
{
	switch (Cry3DEngineBase::GetCVars()->e_DebugDraw)
	{
	case LM_TRI_COUNT:
		return (numTris < other.numTris);

	case LM_VERT_COUNT:
		return (numVerts < other.numVerts);

	case LM_DRAWCALLS:
		return (drawCalls < other.drawCalls);

	case LM_TEXTMEM:
		return (texMemory < other.texMemory);

	case LM_MESHMEM:
		return (meshMemory < other.meshMemory);

	default:
		assert(false);
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
CDebugDrawListMgr::TAssetInfo::TAssetInfo(const I3DEngine::SObjectInfoToAddToDebugDrawList& objInfo)
{
	type = objInfo.type;
	if (!objInfo.pClassName)
	{
		// custom functions to avoid any heap allocation
		MyString_Assign(name, objInfo.pName);
		MyStandardString_Concatenate(name, "(");
		MyStandardString_Concatenate(name, objInfo.pClassName);
		MyStandardString_Concatenate(name, ")");
	}

	MyFileNameString_Assign(fileName, objInfo.pFileName);

	numTris = objInfo.numTris;
	numVerts = objInfo.numVerts;
	texMemory = objInfo.texMemory;
	meshMemory = objInfo.meshMemory;
	drawCalls = gEnv->pRenderer->GetDrawCallsPerNode(objInfo.pRenderNode);
	numInstances = 1;
	ID = UNDEFINED_ASSET_ID;
}

//////////////////////////////////////////////////////////////////////////
CDebugDrawListMgr::TObjectDrawBoxInfo::TObjectDrawBoxInfo(const I3DEngine::SObjectInfoToAddToDebugDrawList& objInfo)
{
	mat.SetIdentity();
	bbox.Reset();
	if (objInfo.pMat)
		mat = *objInfo.pMat;
	if (objInfo.pBox)
		bbox = *objInfo.pBox;
	assetID = UNDEFINED_ASSET_ID;
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::MyStandardString_Concatenate(TMyStandardString& outStr, const char* pStr)
{
	if (pStr && outStr.length() < outStr.capacity())
	{
		outStr._ConcatenateInPlace(pStr, min(strlen(pStr), outStr.capacity() - outStr.length()));
	}
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::MyFileNameString_Assign(TFilenameString& outStr, const char* pStr)
{
	char tempBuf[outStr.MAX_SIZE + 1];

	uint32 outInd = 0;
	if (pStr)
	{
		while (*pStr != 0 && outInd < outStr.MAX_SIZE)
		{
			tempBuf[outInd] = *pStr;
			outInd++;
			if (*pStr == '%' && outInd < outStr.MAX_SIZE)
			{
				tempBuf[outInd] = '%';
				outInd++;
			}
			pStr++;
		}
	}
	tempBuf[outInd] = 0;
	outStr = tempBuf;
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::ConsoleCommand(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() > 1 && args->GetArg(1))
	{
		switch (toupper(args->GetArg(1)[0]))
		{
		case 'F':
			m_freezeRequested = true;
			break;
		case 'C':
			m_unfreezeRequested = true;
			break;
		case 'D':
			m_dumpLogRequested = true;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::CheckFilterCVar()
{
	ICVar* pCVar = gEnv->pConsole->GetCVar("e_debugdrawlistfilter");

	if (!pCVar)
		return;

	const char* pVal = pCVar->GetString();

	if (pVal && strcmpi(pVal, "all") == 0)
	{
		m_filter = I3DEngine::DLOT_ALL;
		return;
	}

	m_filter = 0;
	while (pVal && pVal[0])
	{
		switch (toupper(pVal[0]))
		{
		case 'B':
			m_filter |= I3DEngine::DLOT_BRUSH;
			break;
		case 'V':
			m_filter |= I3DEngine::DLOT_VEGETATION;
			break;
		case 'C':
			m_filter |= I3DEngine::DLOT_CHARACTER;
			break;
		case 'S':
			m_filter |= I3DEngine::DLOT_STATOBJ;
			break;
		}
		++pVal;
	}
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::DumpLog()
{
	TMyStandardString filterStr;
	GetStrCurrFilter(filterStr);
	CryLog("--------------------------------------------------------------------------------");
	CryLog("                           DebugDrawList infodebug");
	CryLog("--------------------------------------------------------------------------------");
	CryLog(" total objects: %d    Ordered by: %s     Showing: %s", m_counter, GetStrCurrMode(), filterStr.c_str());
	CryLog("");
	CryLog("   tris      verts   draw Calls   texMem     meshMem    type");
	CryLog(" -------   --------  ----------  --------   --------  ----------");
	for (uint32 i = 0; i < m_assets.size(); ++i)
	{
		const TAssetInfo& currAsset = m_assets[i];
		TMyStandardString texMemoryStr;
		TMyStandardString meshMemoryStr;
		MemToString(currAsset.texMemory, texMemoryStr);
		MemToString(currAsset.meshMemory, meshMemoryStr);
		CryLog("%8d  %8d     %5d     %s   %s  %s      %s    %s", currAsset.numTris, currAsset.numVerts, currAsset.drawCalls,
		       texMemoryStr.c_str(), meshMemoryStr.c_str(), GetAssetTypeName(currAsset.type), currAsset.name.c_str(), currAsset.fileName.c_str());
	}
	CryLog("--------------------------------------------------------------------------------");
}

//////////////////////////////////////////////////////////////////////////
const char* CDebugDrawListMgr::GetStrCurrMode()
{
	const char* pModesNames[] =
	{
		"Tri count",
		"Vert count",
		"Draw calls",
		"Texture memory",
		"Mesh memory"
	};

	uint32 index = Cry3DEngineBase::GetCVars()->e_DebugDraw - LM_BASENUMBER;
	uint32 numElems = sizeof(pModesNames) / sizeof *(pModesNames);
	if (index < numElems)
		return pModesNames[index];
	else
		return "<UNKNOWN>";
}

//////////////////////////////////////////////////////////////////////////
void CDebugDrawListMgr::GetStrCurrFilter(TMyStandardString& strOut)
{
	const char* pFilterNames[] =
	{
		"",
		"Brushes",
		"Vegetation",
		"Characters",
		"StatObjs"
	};

	uint32 numElems = sizeof(pFilterNames) / sizeof *(pFilterNames);
	for (uint32 i = 1, bitVal = 1; i < numElems; ++i)
	{
		if ((bitVal & m_filter) != 0)
		{
			if (strOut.size() > 0)
				strOut += "+";
			strOut += pFilterNames[i];
		}
		bitVal *= 2;
	}

	if (strOut.size() == 0)
		strOut = "ALL";
}

//////////////////////////////////////////////////////////////////////////
const char* CDebugDrawListMgr::GetAssetTypeName(I3DEngine::EDebugDrawListAssetTypes type)
{
	const char* pNames[] =
	{
		"",
		"Brush     ",
		"Vegetation",
		"Character ",
		"StatObj   "
	};

	uint32 numElems = sizeof(pNames) / sizeof *(pNames);
	for (uint32 i = 1, bitVal = 1; i < numElems; ++i)
	{
		if (bitVal == type)
			return pNames[i];
		bitVal *= 2;
	}

	return "<UNKNOWN>";
}

///////////////////////////////////////////////////////////////////////////////
float C3DEngine::GetWaterLevel()
{
	return m_pTerrain ? m_pTerrain->CTerrain::GetWaterLevel() : WATER_LEVEL_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 C3DEngine::GetSkyColor() const
{
	return m_pObjManager ? m_pObjManager->m_vSkyColor : Vec3(0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////
bool C3DEngine::IsTessellationAllowed(const CRenderObject* pObj, const SRenderingPassInfo& passInfo, bool bIgnoreShadowPass) const
{
#ifdef MESH_TESSELLATION_ENGINE
	assert(pObj && GetCVars());
	bool rendererTessellation;
	GetRenderer()->EF_Query(EFQ_MeshTessellation, rendererTessellation);
	if (pObj->m_fDistance < GetCVars()->e_TessellationMaxDistance
		&& GetCVars()->e_Tessellation
		&& rendererTessellation
		&& !(pObj->m_ObjFlags & FOB_DISSOLVE)) // dissolve is not working with tessellation for now
	{
		bool bAllowTessellation = true;

		// Check if rendering into shadow map and enable tessellation only if allowed
		if (!bIgnoreShadowPass && passInfo.IsShadowPass())
		{
			if (IsTessellationAllowedForShadowMap(passInfo))
			{
				// NOTE: This might be useful for game projects
				// Use tessellation only for objects visible in main view
				// Shadows will switch to non-tessellated when caster gets out of view
				IRenderNode* pRN = (IRenderNode*)pObj->m_pRenderNode;
				if (pRN)
					bAllowTessellation = ((pRN->GetRenderNodeType() != eERType_TerrainSector) && (pRN->GetDrawFrame() > passInfo.GetFrameID() - 10));
			}
			else
				bAllowTessellation = false;
		}

		return bAllowTessellation;
	}
#endif //#ifdef MESH_TESSELLATION_ENGINE

	return false;
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::RenderRenderNode_ShadowPass(IShadowCaster* pShadowCaster, const SRenderingPassInfo& passInfo)
{
	assert(passInfo.IsShadowPass());

	IRenderNode* pRenderNode = static_cast<IRenderNode*>(pShadowCaster);
	if ((pRenderNode->m_dwRndFlags & ERF_HIDDEN) != 0)
	{
		return;
	}

	int nStaticObjectLod = -1;
	if (passInfo.GetShadowMapType() == SRenderingPassInfo::SHADOW_MAP_CACHED)
		nStaticObjectLod = GetCVars()->e_ShadowsCacheObjectLod;
	else if (passInfo.GetShadowMapType() == SRenderingPassInfo::SHADOW_MAP_CACHED_MGPU_COPY)
		nStaticObjectLod = pRenderNode->m_cStaticShadowLod;

	if (!Get3DEngine()->CheckAndCreateRenderNodeTempData(&pRenderNode->m_pTempData, pRenderNode, passInfo))
	{
		return;
	}

	int wantedLod = pRenderNode->m_pTempData->userData.nWantedLod;
	if (pRenderNode->GetShadowLodBias() != IRenderNode::SHADOW_LODBIAS_DISABLE)
	{
		if (passInfo.IsShadowPass() && (pRenderNode->GetDrawFrame(0) < (passInfo.GetFrameID() - 10)))
			wantedLod += GetCVars()->e_ShadowsLodBiasInvis;
		wantedLod += GetCVars()->e_ShadowsLodBiasFixed;
		wantedLod += pRenderNode->GetShadowLodBias();
	}

	if (nStaticObjectLod >= 0)
		wantedLod = nStaticObjectLod;

	switch (pRenderNode->GetRenderNodeType())
	{
	case eERType_Vegetation:
	{
		CVegetation* pVegetation = static_cast<CVegetation*>(pRenderNode);
		const CLodValue lodValue = pVegetation->ComputeLod(wantedLod, passInfo);
		pVegetation->Render(passInfo, lodValue, NULL);
	}
	break;
	case eERType_Brush:
	{
		CBrush* pBrush = static_cast<CBrush*>(pRenderNode);
		const CLodValue lodValue = pBrush->ComputeLod(wantedLod, passInfo);
		pBrush->Render(lodValue, passInfo, NULL, NULL);
	}
	break;
	default:
	{
		const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
		const AABB objBox = pRenderNode->GetBBoxVirtual();
		SRendParams rParams;
		rParams.fDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, objBox)) * passInfo.GetZoomFactor();
		rParams.lodValue = pRenderNode->ComputeLod(wantedLod, passInfo);

		pRenderNode->Render(rParams, passInfo);
	}
	break;
	}
}

///////////////////////////////////////////////////////////////////////////////
ITimeOfDay* C3DEngine::GetTimeOfDay()
{
	CTimeOfDay* tod = m_pTimeOfDay;

	if (!tod)
	{
		tod = new CTimeOfDay;
		m_pTimeOfDay = tod;
	}

	return tod;
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::TraceFogVolumes(const Vec3& worldPos, ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo)
{
	CFogVolumeRenderNode::TraceFogVolumes(worldPos, fogVolumeContrib, passInfo);
}

///////////////////////////////////////////////////////////////////////////////
int C3DEngine::GetTerrainSize()
{
	return CTerrain::GetTerrainSize();
}
#include "ParticleEmitter.h"

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::AsyncOctreeUpdate(IRenderNode* pEnt, int nSID, int nSIDConsideredSafe, uint32 nFrameID, bool bUnRegisterOnly)
{
	FUNCTION_PROFILER_3DENGINE;

#ifdef _DEBUG // crash test basically
	const char* szClass = pEnt->GetEntityClassName();
	const char* szName = pEnt->GetName();
	if (!szName[0] && !szClass[0])
		Warning("I3DEngine::RegisterEntity: Entity undefined"); // do not register undefined objects
																														//  if(strstr(szName,"Dude"))
																														//  int y=0;
#endif

	IF(bUnRegisterOnly, 0)
	{
		UnRegisterEntityImpl(pEnt);
		return;
	}

	AABB aabb;
	pEnt->FillBBox(aabb);
	float fObjRadiusSqr = aabb.GetRadiusSqr();
	EERType eERType = pEnt->GetRenderNodeType();

#ifdef SUPP_HMAP_OCCL
	if (pEnt->m_pTempData)
		pEnt->m_pTempData->userData.m_OcclState.vLastVisPoint.Set(0, 0, 0);
#endif

	UpdateObjectsLayerAABB(pEnt);

	const unsigned int dwRndFlags = pEnt->GetRndFlags();

	if (!(dwRndFlags & ERF_RENDER_ALWAYS) && !(dwRndFlags & ERF_CASTSHADOWMAPS))
		if (GetCVars()->e_ObjFastRegister && pEnt->m_pOcNode && ((COctreeNode*)pEnt->m_pOcNode)->IsRightNode(aabb, fObjRadiusSqr, pEnt->m_fWSMaxViewDist))
		{
			// same octree node
			Vec3 vEntCenter = GetEntityRegisterPoint(pEnt);

			IVisArea* pVisArea = pEnt->GetEntityVisArea();
			if (pVisArea && pVisArea->IsPointInsideVisArea(vEntCenter))
				return; // same visarea

			IVisArea* pVisAreaFromPos = (!m_pVisAreaManager || dwRndFlags & ERF_OUTDOORONLY) ? NULL : GetVisAreaManager()->GetVisAreaFromPos(vEntCenter);
			if (pVisAreaFromPos == pVisArea)
			{
				// NOTE: can only get here when pVisArea==NULL due to 'same visarea' check above. So check for changed clip volume
				if (GetClipVolumeManager()->IsClipVolumeRequired(pEnt))
					GetClipVolumeManager()->UpdateEntityClipVolume(vEntCenter, pEnt);

				return; // same visarea or same outdoor
			}
		}

	if (pEnt->m_pOcNode)
	{
		UnRegisterEntityImpl(pEnt);
	}
	else if (GetCVars()->e_StreamCgf && eERType == eERType_RenderProxy)
	{
		//  Temporary solution: Force streaming priority update for objects that was not registered before
		//  and was not visible before since usual prediction system was not able to detect them

		if ((uint32)pEnt->GetDrawFrame(0) < nFrameID - 16)
		{
			// deferr the render node streaming priority update still we have a correct 3D Engine camera
			int nElementID = m_deferredRenderProxyStreamingPriorityUpdates.Find(pEnt);
			if (nElementID == -1)  // only add elements once
				m_deferredRenderProxyStreamingPriorityUpdates.push_back(pEnt);
		}

	}

	if (eERType == eERType_Vegetation)
	{
		CVegetation* pInst = (CVegetation*)pEnt;
		pInst->UpdateRndFlags();
	}

	pEnt->m_fWSMaxViewDist = pEnt->GetMaxViewDist();

	if (eERType != eERType_Light)
	{
		if (fObjRadiusSqr > sqr(MAX_VALID_OBJECT_VOLUME) || !_finite(fObjRadiusSqr))
		{
			Warning("I3DEngine::RegisterEntity: Object has invalid bbox: name: %s, class name: %s, GetRadius() = %.2f",
				pEnt->GetName(), pEnt->GetEntityClassName(), fObjRadiusSqr);
			return; // skip invalid objects - usually only objects with invalid very big scale will reach this point
		}

		if (dwRndFlags & ERF_RENDER_ALWAYS)
		{
			if (m_lstAlwaysVisible.Find(pEnt) < 0)
				m_lstAlwaysVisible.Add(pEnt);

			if (dwRndFlags & ERF_HUD)
				return;
		}
	}
	else
	{
		CLightEntity* pLight = (CLightEntity*)pEnt;
		if ((pLight->m_light.m_Flags & (DLF_IGNORES_VISAREAS | DLF_DEFERRED_LIGHT | DLF_THIS_AREA_ONLY)) == (DLF_IGNORES_VISAREAS | DLF_DEFERRED_LIGHT))
		{
			if (m_lstAlwaysVisible.Find(pEnt) < 0)
				m_lstAlwaysVisible.Add(pEnt);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Check for occlusion proxy.
	{
		CStatObj* pStatObj = (CStatObj*)pEnt->GetEntityStatObj();
		if (pStatObj)
		{
			if (pStatObj->m_bHaveOcclusionProxy)
			{
				pEnt->m_dwRndFlags |= ERF_GOOD_OCCLUDER;
				pEnt->m_nInternalFlags |= IRenderNode::HAS_OCCLUSION_PROXY;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (pEnt->m_dwRndFlags & ERF_OUTDOORONLY || !(m_pVisAreaManager && m_pVisAreaManager->SetEntityArea(pEnt, aabb, fObjRadiusSqr)))
	{
#ifndef SEG_WORLD
		if (nSID == -1)
		{
			nSID = 0;

			// if segment is not set - find best one automatically
			Vec3 vCenter = aabb.GetCenter();
			const int iTreeCount = Get3DEngine()->m_pObjectsTree.Count();
			for (int n = 0; n < iTreeCount; n++)
			{
				if (Get3DEngine()->m_pObjectsTree[n])
				{
					if (Overlap::Point_AABB2D(vCenter, m_pObjectsTree[n]->GetNodeBox()))
					{
						nSID = n;
						break;
					}
				}
			}
		}

		if (nSID >= 0 && nSID < m_pObjectsTree.Count())
		{
			if (!m_pObjectsTree[nSID])
			{
				const float terrainSize = (float)GetTerrainSize();
				m_pObjectsTree[nSID] = COctreeNode::Create(nSID, AABB(Vec3(0, 0, 0), Vec3(terrainSize, terrainSize, terrainSize)), NULL);
			}

			m_pObjectsTree[nSID]->InsertObject(pEnt, aabb, fObjRadiusSqr, aabb.GetCenter());
		}
#else
		if (gEnv->IsEditor() || eERType != eERType_Vegetation)
		{
			// CS - opt here
			if (GetITerrain())
			{
				Vec3 vCenter = aabb.GetCenter();
				nSID = GetITerrain()->WorldToSegment(vCenter, GetDefSID());
			}
		}
		else
		{
			// use specified sid from which the objects are actually serialized for vegetation objects
			CVegetation* pInst = (CVegetation*)pEnt;
			nSID = pInst->m_nStaticTypeSlot;
		}

		if (nSID >= 0 && nSID < m_pObjectsTree.Count())
		{
			//#ifdef _DEBUG
			//		  if(nSID != nSIDConsideredSafe)
			//		  {
			//			  Warning("I3DEngine::RegisterEntity: Object %s was not added to the scene", pEnt->GetName());
			//		  }
			//#endif
			// TODO: this should only work if the segment is safe or if it's unsafe and the object is being inserted by SetCompiledData...
			if (nSID == nSIDConsideredSafe || IsSegmentSafeToUse(nSID))
				m_pObjectsTree[nSID]->InsertObject(pEnt, aabb, fObjRadiusSqr, aabb.GetCenter());
		}
#endif
	}

	// update clip volume: use vis area if we have one, otherwise check if we're in the same volume as before. check other volumes as last resort only
	if (pEnt->m_pTempData)
	{
		Vec3 vEntCenter = GetEntityRegisterPoint(pEnt);
		SRenderNodeTempData::SUserData& userData = pEnt->m_pTempData->userData;

		if (IVisArea* pVisArea = pEnt->GetEntityVisArea())
			userData.m_pClipVolume = pVisArea;
		else if (GetClipVolumeManager()->IsClipVolumeRequired(pEnt))
			GetClipVolumeManager()->UpdateEntityClipVolume(vEntCenter, pEnt);
	}

	// register decals, to clean up longer not renderes decals and their render meshes
	if (eERType == eERType_Decal)
	{
		m_decalRenderNodes.push_back((IDecalRenderNode*)pEnt);
	}
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::UpdateObjectsLayerAABB(IRenderNode* pEnt)
{
	uint16 nLayerId = pEnt->GetLayerId();
	if (nLayerId && nLayerId < 0xFFFF)
	{
		m_arrObjectLayersActivity.CheckAllocated(nLayerId + 1);

		if (m_arrObjectLayersActivity[nLayerId].objectsBox.GetVolume())
			m_arrObjectLayersActivity[nLayerId].objectsBox.Add(pEnt->GetBBox());
		else
			m_arrObjectLayersActivity[nLayerId].objectsBox = pEnt->GetBBox();
	}
}

///////////////////////////////////////////////////////////////////////////////
bool C3DEngine::UnRegisterEntityImpl(IRenderNode* pEnt)
{
	// make sure we don't try to update the streaming priority if an object
	// was added and removed in the same frame
	int nElementID = m_deferredRenderProxyStreamingPriorityUpdates.Find(pEnt);
	if (nElementID != -1)
		m_deferredRenderProxyStreamingPriorityUpdates.DeleteFastUnsorted(nElementID);

	FUNCTION_PROFILER_3DENGINE;

#ifdef _DEBUG // crash test basically
	const char* szClass = pEnt->GetEntityClassName();
	const char* szName = pEnt->GetName();
	if (!szName[0] && !szClass[0])
		Warning("C3DEngine::RegisterEntity: Entity undefined");
#endif

	EERType eRenderNodeType = pEnt->GetRenderNodeType();

	bool bFound = false;
	//pEnt->PhysicalizeFoliage(false);

	if (pEnt->m_pOcNode)
		bFound = ((COctreeNode*)pEnt->m_pOcNode)->DeleteObject(pEnt);

	if (pEnt->m_dwRndFlags & ERF_RENDER_ALWAYS || (eRenderNodeType == eERType_Light) || (eRenderNodeType == eERType_FogVolume))
	{
		m_lstAlwaysVisible.Delete(pEnt);
	}

	if (eRenderNodeType == eERType_Decal)
	{
		std::vector<IDecalRenderNode*>::iterator it = std::find(m_decalRenderNodes.begin(), m_decalRenderNodes.end(), (IDecalRenderNode*)pEnt);
		if (it != m_decalRenderNodes.end())
		{
			m_decalRenderNodes.erase(it);
		}
	}

	if (CClipVolumeManager* pClipVolumeManager = GetClipVolumeManager())
	{
		pClipVolumeManager->UnregisterRenderNode(pEnt);
	}

	return bFound;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 C3DEngine::GetEntityRegisterPoint(IRenderNode* pEnt)
{
	AABB aabb;
	pEnt->FillBBox(aabb);

	Vec3 vPoint;

	if (pEnt->m_dwRndFlags & ERF_REGISTER_BY_POSITION)
	{
		vPoint = pEnt->GetPos();
		vPoint.z += 0.25f;

		if (pEnt->GetRenderNodeType() != eERType_Light)
		{
			// check for valid position
			if (aabb.GetDistanceSqr(vPoint) > sqr(128.f))
			{
				Warning("I3DEngine::RegisterEntity: invalid entity position: Name: %s, Class: %s, Pos=(%.1f,%.1f,%.1f), BoxMin=(%.1f,%.1f,%.1f), BoxMax=(%.1f,%.1f,%.1f)",
					pEnt->GetName(), pEnt->GetEntityClassName(),
					pEnt->GetPos().x, pEnt->GetPos().y, pEnt->GetPos().z,
					pEnt->GetBBox().min.x, pEnt->GetBBox().min.y, pEnt->GetBBox().min.z,
					pEnt->GetBBox().max.x, pEnt->GetBBox().max.y, pEnt->GetBBox().max.z
				);
			}
			// clamp by bbox
			vPoint.CheckMin(aabb.max);
			vPoint.CheckMax(aabb.min + Vec3(0, 0, .5f));
		}
	}
	else
		vPoint = aabb.GetCenter();

	return vPoint;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 C3DEngine::GetSunDirNormalized() const
{
	return m_vSunDirNormalized;
}

#endif //RELEASE
