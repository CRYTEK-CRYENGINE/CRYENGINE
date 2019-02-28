// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   3dengine.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef C3DENGINE_H
#define C3DENGINE_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryThreading/CryThreadSafeRendererContainer.h>
#include <CryCore/Containers/CryListenerSet.h>
#include "VisibleRenderNodeManager.h"
#include "LightVolumeManager.h"

#ifdef DrawText
	#undef DrawText
#endif //DrawText

// forward declaration
struct SNodeInfo;
class CStitchedImage;
class CWaterRippleManager;

struct SEntInFoliage
{
	int   id;
	float timeIdle;

	void  GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
};

class CMemoryBlock : public IMemoryBlock
{
public:
	virtual void* GetData() { return m_pData; }
	virtual int   GetSize() { return m_nSize; }
	virtual ~CMemoryBlock() { delete[] m_pData; }

	CMemoryBlock() { m_pData = 0; m_nSize = 0; }
	CMemoryBlock(const void* pData, int nSize)
	{
		m_pData = 0;
		m_nSize = 0;
		SetData(pData, nSize);
	}
	void SetData(const void* pData, int nSize)
	{
		delete[] m_pData;
		m_pData = new uint8[nSize];
		memcpy(m_pData, pData, nSize);
		m_nSize = nSize;
	}
	void Free()
	{
		delete[] m_pData;
		m_pData = NULL;
		m_nSize = 0;
	}
	void Allocate(int nSize)
	{
		delete[] m_pData;
		m_pData = new uint8[nSize];
		memset(m_pData, 0, nSize);
		m_nSize = nSize;
	}

	static CMemoryBlock* CompressToMemBlock(void* pData, int nSize, ISystem* pSystem)
	{
		CMemoryBlock* pMemBlock = NULL;
		uint8* pTmp = new uint8[nSize + 4];
		size_t nComprSize = nSize;
		*(uint32*)pTmp = nSize;
		if (pSystem->CompressDataBlock(pData, nSize, pTmp + 4, nComprSize))
		{
			pMemBlock = new CMemoryBlock(pTmp, nComprSize + 4);
		}

		delete[] pTmp;
		return pMemBlock;
	}

	static CMemoryBlock* DecompressFromMemBlock(CMemoryBlock* pMemBlock, ISystem* pSystem)
	{
		size_t nUncompSize = *(uint32*)pMemBlock->GetData();
		SwapEndian(nUncompSize);
		CMemoryBlock* pResult = new CMemoryBlock;
		pResult->Allocate(nUncompSize);
		if (!pSystem->DecompressDataBlock((byte*)pMemBlock->GetData() + 4, pMemBlock->GetSize() - 4, pResult->GetData(), nUncompSize))
		{
			assert(!"CMemoryBlock::DecompressFromMemBlock failed");
			delete pResult;
			pResult = NULL;
		}

		return pResult;
	}

	uint8* m_pData;
	int    m_nSize;
};

struct SOptimizedOutdoorWindArea
{
	int              x0, x1, y0, y1; // 2d rectangle extents for the wind area
	Vec2             point[5];       // Extent points of the wind area (index 4 is center)
	Vec3             windSpeed[5];   // Wind speed at every corner and center.
	float            z;
	IPhysicalEntity* pArea;// Physical area
};

struct DLightAmount
{
	SRenderLight* pLight;
	float         fAmount;
};

struct SImageSubInfo
{
	SImageSubInfo() { memset(this, 0, sizeof(*this)); fTiling = fTilingIn = 1.f; }

	static const int nMipsNum = 4;

	union
	{
		byte* pImgMips[nMipsNum];
		int   pImgMipsSizeKeeper[8];
	};

	float fAmount;
	int   nReady;
	int   nDummy[4];

	union
	{
		IMaterial* pMat;
		int        pMatSizeKeeper[2];
	};

	int   nDim;
	float fTilingIn;
	float fTiling;
	float fSpecularAmount;
	int   nSortOrder;
	int   nAlignFix;

	AUTO_STRUCT_INFO;
};

struct SImageInfo : public Cry3DEngineBase
{
	SImageInfo()
	{
		szDetMatName[0] = szBaseTexName[0] = nPhysSurfaceType = 0;
		nLayerId = 0;
		fUseRemeshing = 0;
		fBr = 1.f;
		layerFilterColor = Col_White;
		nDetailSurfTypeId = 0;
		ZeroStruct(arrTextureId);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }

	SImageSubInfo baseInfo;
	SImageSubInfo detailInfo;

	char          szDetMatName[128 - 20];

	int           arrTextureId[4];
	int           nPhysSurfaceType;

	char          szBaseTexName[128];

	float         fUseRemeshing;
	ColorF        layerFilterColor;
	int           nLayerId;
	float         fBr;
	int           nDetailSurfTypeId;

	int GetMemoryUsage();

	AUTO_STRUCT_INFO;
};

struct SSceneFrustum
{
	uint32*      pRgbImage;
	uint32       nRgbWidth, nRgbHeight;

	float*       pDepthImage;
	uint32       nDepthWidth, nDepthHeight;

	CCamera      camera;

	IRenderMesh* pRM;
	IMaterial*   pMaterial;

	float        fDistance;
	int          nId;

	static int   Compare(const void* v1, const void* v2)
	{
		SSceneFrustum* p[2] = { (SSceneFrustum*)v1, (SSceneFrustum*)v2 };

		if (p[0]->fDistance > p[1]->fDistance)
			return 1;
		if (p[0]->fDistance < p[1]->fDistance)
			return -1;

		if (p[0]->nId > p[1]->nId)
			return 1;
		if (p[0]->nId < p[1]->nId)
			return -1;

		return 0;
	}
};

struct SPerObjectShadow
{
	IShadowCaster* pCaster;
	float          fConstBias;
	float          fSlopeBias;
	float          fJitter;
	Vec3           vBBoxScale;
	uint           nTexSize;
};
//////////////////////////////////////////////////////////////////////

// onscreen infodebug for e_debugDraw >= 100
#ifndef _RELEASE
class CDebugDrawListMgr
{
	typedef CryFixedStringT<64>  TMyStandardString;
	typedef CryFixedStringT<128> TFilenameString;

public:

	CDebugDrawListMgr();
	void        Update();
	void        AddObject(I3DEngine::SObjectInfoToAddToDebugDrawList& objInfo);
	void        DumpLog();
	bool        IsEnabled() const { return Cry3DEngineBase::GetCVars()->e_DebugDraw >= LM_BASENUMBER; }
	static void ConsoleCommand(IConsoleCmdArgs* args);

private:

	enum { UNDEFINED_ASSET_ID = 0xffffffff };

	struct TAssetInfo
	{
		TMyStandardString                   name;
		TFilenameString                     fileName;
		uint32                              numTris;
		uint32                              numVerts;
		uint32                              texMemory;
		uint32                              meshMemory;
		uint32                              drawCalls;
		uint32                              numInstances;
		I3DEngine::EDebugDrawListAssetTypes type;
		uint32                              ID; // to identify which drawBoxes belong to this asset

		TAssetInfo(const I3DEngine::SObjectInfoToAddToDebugDrawList& objInfo);
		bool operator<(const TAssetInfo& other) const;
	};

	struct TObjectDrawBoxInfo
	{
		Matrix34 mat;
		AABB     bbox;
		uint32   assetID;

		TObjectDrawBoxInfo(const I3DEngine::SObjectInfoToAddToDebugDrawList& objInfo);
	};

	void        FindNewLeastValueAsset();
	void        ClearFrameData();
	void        ClearConsoleCommandRequestVars();
	static bool SortComparison(const TAssetInfo& A, const TAssetInfo& B) { return B < A; }
	const char* GetStrCurrMode();
	void        GetStrCurrFilter(TMyStandardString& strOut);
	bool        ShouldFilterOutObject(const I3DEngine::SObjectInfoToAddToDebugDrawList& newObject);
	void        MemToString(uint32 memVal, TMyStandardString& outStr);
	static void PrintText(float x, float y, const ColorF& fColor, const char* label_text, ...);
	const char* GetAssetTypeName(I3DEngine::EDebugDrawListAssetTypes type);
	TAssetInfo* FindDuplicate(const TAssetInfo& object);
	void        CheckFilterCVar();

	// to avoid any heap allocation
	static void                   MyStandardString_Concatenate(TMyStandardString& outStr, const char* str);
	static void                   MyFileNameString_Assign(TFilenameString& outStr, const char* pStr);

	template<class T> static void MyString_Assign(T& outStr, const char* pStr)
	{
		if (pStr)
			outStr._Assign(pStr, min(strlen(pStr), outStr.capacity()));
		else
			outStr = "";
	}

	enum EListModes
	{
		LM_BASENUMBER = 100,
		LM_TRI_COUNT  = LM_BASENUMBER,
		LM_VERT_COUNT,
		LM_DRAWCALLS,
		LM_TEXTMEM,
		LM_MESHMEM
	};

	bool                            m_isFrozen;
	uint32                          m_counter;
	uint32                          m_assetCounter;
	uint32                          m_indexLeastValueAsset;
	std::vector<TAssetInfo>         m_assets;
	std::vector<TObjectDrawBoxInfo> m_drawBoxes;
	CryCriticalSection              m_lock;

	static bool                     m_dumpLogRequested;
	static bool                     m_freezeRequested;
	static bool                     m_unfreezeRequested;
	static uint32                   m_filter;
};
#endif //_RELEASE

//////////////////////////////////////////////////////////////////////
class C3DEngine : public I3DEngine, public Cry3DEngineBase
{
	// IProcess Implementation
	void SetFlags(int flags) { m_nFlags = flags; }
	int  GetFlags(void)      { return m_nFlags; }
	int m_nFlags;

public:

	// I3DEngine interface implementation
	virtual bool      Init();
	virtual void      OnFrameStart();
	virtual void      Update();
	virtual void      RenderWorld(const int nRenderFlags, const SRenderingPassInfo& passInfo, const char* szDebugName);
	virtual void      PreWorldStreamUpdate(const CCamera& cam);
	virtual void      WorldStreamUpdate();
	virtual void      ShutDown();
	virtual void      Release() { CryAlignedDelete(this); };
	virtual void      SetLevelPath(const char* szFolderName);
	virtual bool      LoadLevel(const char* szFolderName, const char* szMissionName);
	virtual void      UnloadLevel();
	virtual void      PostLoadLevel();
	virtual bool      InitLevelForEditor(const char* szFolderName, const char* szMissionName);
	virtual void      DisplayInfo(float& fTextPosX, float& fTextPosY, float& fTextStepY, const bool bEnhanced);
	virtual IStatObj* LoadStatObj(const char* szFileName, const char* szGeomName = NULL, /*[Out]*/ IStatObj::SSubObject** ppSubObject = NULL, bool bUseStreaming = true, unsigned long nLoadingFlags = 0);
	virtual IStatObj* FindStatObjectByFilename(const char* filename);
	virtual void      RegisterEntity(IRenderNode* pEnt);
	virtual void      SelectEntity(IRenderNode* pEnt);

#ifndef _RELEASE
	virtual void AddObjToDebugDrawList(SObjectInfoToAddToDebugDrawList& objInfo);
	virtual bool IsDebugDrawListEnabled() const { return m_DebugDrawListMgr.IsEnabled(); }
#endif

	virtual void   UnRegisterEntityDirect(IRenderNode* pEnt);
	virtual void   UnRegisterEntityAsJob(IRenderNode* pEnt);

	virtual void   AddWaterRipple(const Vec3& vPos, float scale, float strength);

	virtual bool   IsUnderWater(const Vec3& vPos) const;
	virtual void   SetOceanRenderFlags(uint8 nFlags);
	virtual uint8  GetOceanRenderFlags() const { return m_nOceanRenderFlags; }
	virtual uint32 GetOceanVisiblePixelsCount() const;
	virtual float  GetBottomLevel(const Vec3& referencePos, float maxRelevantDepth, int objtypes);
	virtual float  GetBottomLevel(const Vec3& referencePos, float maxRelevantDepth /* = 10.0f*/);
	virtual float  GetBottomLevel(const Vec3& referencePos, int objflags);

#if defined(USE_GEOM_CACHES)
	virtual IGeomCache* LoadGeomCache(const char* szFileName);
	virtual IGeomCache* FindGeomCacheByFilename(const char* szFileName);
#endif

	virtual IStatObj* LoadDesignerObject(int nVersion, const char* szBinaryStream, int size);

	void              AsyncOctreeUpdate(IRenderNode* pEnt, uint32 nFrameID, bool bUnRegisterOnly);
	bool              UnRegisterEntityImpl(IRenderNode* pEnt);
	virtual void      UpdateObjectsLayerAABB(IRenderNode* pEnt);

	// Fast option - use if just ocean height required
	virtual float                    GetWaterLevel();
	// This will return ocean height or water volume height, optional for accurate water height query
	virtual float                    GetWaterLevel(const Vec3* pvPos, IPhysicalEntity* pent = NULL, bool bAccurate = false);
	// Only use for Accurate query - this will return exact ocean height
	virtual float                    GetAccurateOceanHeight(const Vec3& pCurrPos) const;

	virtual Vec4                     GetCausticsParams() const;
	virtual Vec4                     GetOceanAnimationCausticsParams() const;
	virtual void                     GetOceanAnimationParams(Vec4& pParams0, Vec4& pParams1) const;
	virtual void                     GetHDRSetupParams(Vec4 pParams[5]) const;
	virtual void                     CreateDecal(const CryEngineDecalInfo& Decal);
	virtual void                     DrawFarTrees(const SRenderingPassInfo& passInfo);
	virtual void                     GenerateFarTrees(const SRenderingPassInfo& passInfo);
	virtual float                    GetTerrainElevation(float x, float y);
	virtual float                    GetTerrainElevation3D(Vec3 vPos);
	virtual float                    GetTerrainZ(float x, float y);
	virtual bool                     GetTerrainHole(float x, float y);
	virtual float                    GetHeightMapUnitSize();
	virtual int                      GetTerrainSize();
	virtual void                     SetSunDir(const Vec3& newSunOffset);
	virtual Vec3                     GetSunDir() const;
	virtual Vec3                     GetSunDirNormalized() const;
	virtual Vec3                     GetRealtimeSunDirNormalized() const;
	virtual void                     SetSkyColor(Vec3 vColor);
	virtual void                     SetSunColor(Vec3 vColor);
	virtual void                     SetSkyBrightness(float fMul);
	virtual void                     SetSSAOAmount(float fMul);
	virtual void                     SetSSAOContrast(float fMul);
	virtual void                     SetGIAmount(float fMul);
	virtual float                    GetSunRel() const;
	virtual void                     SetRainParams(const SRainParams& rainParams);
	virtual bool                     GetRainParams(SRainParams& rainParams);
	virtual void                     SetSnowSurfaceParams(const Vec3& vCenter, float fRadius, float fSnowAmount, float fFrostAmount, float fSurfaceFreezing);
	virtual bool                     GetSnowSurfaceParams(Vec3& vCenter, float& fRadius, float& fSnowAmount, float& fFrostAmount, float& fSurfaceFreezing);
	virtual void                     SetSnowFallParams(int nSnowFlakeCount, float fSnowFlakeSize, float fSnowFallBrightness, float fSnowFallGravityScale, float fSnowFallWindScale, float fSnowFallTurbulence, float fSnowFallTurbulenceFreq);
	virtual bool                     GetSnowFallParams(int& nSnowFlakeCount, float& fSnowFlakeSize, float& fSnowFallBrightness, float& fSnowFallGravityScale, float& fSnowFallWindScale, float& fSnowFallTurbulence, float& fSnowFallTurbulenceFreq);
	virtual void                     OnExplosion(Vec3 vPos, float fRadius, bool bDeformTerrain = true);
	//! For editor
	virtual void                     RemoveAllStaticObjects();
	virtual void                     SetTerrainSurfaceType(int x, int y, int nType);
	virtual void                     SetTerrainSectorTexture(const int nTexSectorX, const int nTexSectorY, unsigned int textureId);
	virtual void                     SetPhysMaterialEnumerator(IPhysMaterialEnumerator* pPhysMaterialEnumerator);
	virtual IPhysMaterialEnumerator* GetPhysMaterialEnumerator();
	virtual void                     LoadMissionDataFromXMLNode(const char* szMissionName);

	void                             AddDynamicLightSource(const SRenderLight& LSource, ILightSource* pEnt, int nEntityLightId, float fFadeout, const SRenderingPassInfo& passInfo);

	inline void                      AddLightToRenderer(const SRenderLight& light, float fMult, const SRenderingPassInfo& passInfo)
	{
		const uint32 nLightID = passInfo.GetIRenderView()->GetLightsCount(eDLT_DeferredLight);
		//passInfo.GetIRenderView()->AddLight(eDLT_DeferredLight,light);
		GetRenderer()->EF_AddDeferredLight(light, fMult, passInfo);
		Get3DEngine()->m_LightVolumesMgr.RegisterLight(light, nLightID, passInfo);
		m_nDeferredLightsNum++;
	}

	virtual void                 ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce);
	virtual void                 SetMaxViewDistanceScale(float fScale) { m_fMaxViewDistScale = fScale; }
	virtual float                GetMaxViewDistance(bool bScaled = true);
	virtual const SFrameLodInfo& GetFrameLodInfo() const               { return m_frameLodInfo; }
	virtual void                 SetFrameLodInfo(const SFrameLodInfo& frameLodInfo);
	virtual void                 SetFogColor(const Vec3& vFogColor);
	virtual Vec3                 GetFogColor();
	virtual float                GetDistanceToSectorWithWater();

	virtual void                 GetSkyLightParameters(Vec3& sunDir, Vec3& sunIntensity, float& Km, float& Kr, float& g, Vec3& rgbWaveLengths);
	virtual void                 SetSkyLightParameters(const Vec3& sunDir, const Vec3& sunIntensity, float Km, float Kr, float g, const Vec3& rgbWaveLengths, bool forceImmediateUpdate = false);

	void                         SetLightsHDRDynamicPowerFactor(const float value);
	virtual float                GetLightsHDRDynamicPowerFactor() const;

	// Return true if tessellation is allowed (by cvars) into currently set shadow map LOD
	bool                                   IsTessellationAllowedForShadowMap(const SRenderingPassInfo& passInfo) const;
	// Return true if tessellation is allowed for given render object
	virtual bool                           IsTessellationAllowed(const CRenderObject* pObj, const SRenderingPassInfo& passInfo, bool bIgnoreShadowPass = false) const;

	virtual bool                           IsStatObjBufferRenderTasksAllowed() const;

	virtual void                           SetRenderNodeMaterialAtPosition(EERType eNodeType, const Vec3& vPos, IMaterial* pMat);
	virtual void                           OverrideCameraPrecachePoint(const Vec3& vPos);
	virtual int                            AddPrecachePoint(const Vec3& vPos, const Vec3& vDir, float fTimeOut = 3.f, float fImportanceFactor = 1.0f);
	virtual void                           ClearPrecachePoint(int id);
	virtual void                           ClearAllPrecachePoints();
	virtual void                           GetPrecacheRoundIds(int pRoundIds[MAX_STREAM_PREDICTION_ZONES]);

	virtual void                           TraceFogVolumes(const Vec3& worldPos, ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo);

	virtual Vec3                           GetSkyColor() const;
	virtual Vec3                           GetSunColor() const;
	virtual float                          GetSkyBrightness() const;
	virtual float                          GetSSAOAmount() const;
	virtual float                          GetSSAOContrast() const;
	virtual float                          GetGIAmount() const;
	virtual float                          GetTerrainTextureMultiplier() const;

	virtual Vec3                           GetAmbientColorFromPosition(const Vec3& vPos, float fRadius = 1.f);
	virtual void                           FreeRenderNodeState(IRenderNode* pEnt);
	virtual const char*                    GetLevelFilePath(const char* szFileName);
	virtual void                           SetTerrainBurnedOut(int x, int y, bool bBurnedOut);
	virtual bool                           IsTerrainBurnedOut(int x, int y);
	virtual int                            GetTerrainSectorSize();
	virtual void                           LoadTerrainSurfacesFromXML(XmlNodeRef pDoc, bool bUpdateTerrain);
	virtual bool                           SetStatInstGroup(int nGroupId, const IStatInstGroup& siGroup);
	virtual bool                           GetStatInstGroup(int nGroupId, IStatInstGroup& siGroup);
	virtual void                           ActivatePortal(const Vec3& vPos, bool bActivate, const char* szEntityName);
	virtual void                           ActivateOcclusionAreas(IVisAreaTestCallback* pTest, bool bActivate);
	virtual void                           GetMemoryUsage(ICrySizer* pSizer) const;
	virtual void                           GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& cstAABB);
	virtual IVisArea*                      CreateVisArea(uint64 visGUID);
	virtual void                           DeleteVisArea(IVisArea* pVisArea);
	virtual void                           UpdateVisArea(IVisArea* pArea, const Vec3* pPoints, int nCount, const char* szName,
	                                                     const SVisAreaInfo& info, bool bReregisterObjects);
	virtual IClipVolume*                   CreateClipVolume();
	virtual void                           DeleteClipVolume(IClipVolume* pClipVolume);
	virtual void                           UpdateClipVolume(IClipVolume* pClipVolume, _smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, uint8 viewDistRatio, bool bActive, uint32 flags, const char* szName);
	virtual void                           ResetParticlesAndDecals();
	virtual IRenderNode*                   CreateRenderNode(EERType type);
	virtual void                           DeleteRenderNode(IRenderNode* pRenderNode);
	void                                   TickDelayedRenderNodeDeletion();
	virtual void                           SetWind(const Vec3& vWind);
	virtual Vec3                           GetWind(const AABB& box, bool bIndoors) const;
	virtual void                           AddForcedWindArea(const Vec3& vPos, float fAmountOfForce, float fRadius);

	void                                   StartWindGridJob(const Vec3& vPos);
	void                                   FinishWindGridJob();
	void                                   UpdateWindGridJobEntry(Vec3 vPos);
	void                                   UpdateWindGridArea(SWindGrid& rWindGrid, const SOptimizedOutdoorWindArea& windArea, const AABB& windBox);
	void                                   RasterWindAreas(std::vector<SOptimizedOutdoorWindArea>* pWindAreas, const Vec3& vGlobalWind);

	virtual Vec3                           GetGlobalWind(bool bIndoors) const;
	virtual bool                           SampleWind(Vec3* pSamples, int nSamples, const AABB& volume, bool bIndoors) const;
	virtual IVisArea*                      GetVisAreaFromPos(const Vec3& vPos);
	virtual bool                           IntersectsVisAreas(const AABB& box, void** pNodeCache = 0);
	virtual bool                           ClipToVisAreas(IVisArea* pInside, Sphere& sphere, Vec3 const& vNormal, void* pNodeCache = 0);
	virtual bool                           IsVisAreasConnected(IVisArea* pArea1, IVisArea* pArea2, int nMaxReqursion, bool bSkipDisabledPortals);
	void                                   EnableOceanRendering(bool bOcean); // todo: remove

	virtual struct ILightSource*           CreateLightSource();
	virtual void                           DeleteLightSource(ILightSource* pLightSource);
	virtual const PodArray<SRenderLight*>* GetStaticLightSources();
	virtual bool                           IsTerrainHightMapModifiedByGame();
	virtual bool                           RestoreTerrainFromDisk();
	virtual void                           CheckMemoryHeap();
	virtual void                           CloseTerrainTextureFile();
	virtual int                            GetLoadedObjectCount();
	virtual void                           GetLoadedStatObjArray(IStatObj** pObjectsArray, int& nCount);
	virtual void                           GetObjectsStreamingStatus(SObjectsStreamingStatus& outStatus);
	virtual void                           GetStreamingSubsystemData(int subsystem, SStremaingBandwidthData& outData);
	virtual void                           DeleteEntityDecals(IRenderNode* pEntity);
	virtual void                           DeleteDecalsInRange(AABB* pAreaBox, IRenderNode* pEntity);
	virtual void                           CompleteObjectsGeometry();
	virtual void                           LockCGFResources();
	virtual void                           UnlockCGFResources();

	virtual void                           SerializeState(TSerialize ser);
	virtual void                           PostSerialize(bool bReading);

	virtual void                           SetHeightMapMaxHeight(float fMaxHeight);

	virtual void                           SetStreamableListener(IStreamedObjectListener* pListener);

	//////////////////////////////////////////////////////////////////////////
	// Materials access.
	virtual IMaterialHelpers& GetMaterialHelpers();
	virtual IMaterialManager* GetMaterialManager();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// CGF Loader.
	//////////////////////////////////////////////////////////////////////////
	virtual CContentCGF* CreateChunkfileContent(const char* filename);
	virtual void         ReleaseChunkfileContent(CContentCGF*);
	virtual bool         LoadChunkFileContent(CContentCGF* pCGF, const char* filename, bool bNoWarningMode = false, bool bCopyChunkFile = true);
	virtual bool         LoadChunkFileContentFromMem(CContentCGF* pCGF, const void* pData, size_t nDataLen, uint32 nLoadingFlags, bool bNoWarningMode = false, bool bCopyChunkFile = true);
	//////////////////////////////////////////////////////////////////////////
	virtual IChunkFile*  CreateChunkFile(bool bReadOnly = false);

	//////////////////////////////////////////////////////////////////////////
	// Chunk file writer.
	//////////////////////////////////////////////////////////////////////////
	virtual ChunkFile::IChunkFileWriter* CreateChunkFileWriter(EChunkFileFormat eFormat, ICryPak* pPak, const char* filename) const;
	virtual void                         ReleaseChunkFileWriter(ChunkFile::IChunkFileWriter* p) const;

	//////////////////////////////////////////////////////////////////////////
	// Post processing effects interfaces

	virtual void   SetPostEffectParam(const char* pParam, float fValue, bool bForceValue = false) const;
	virtual void   SetPostEffectParamVec4(const char* pParam, const Vec4& pValue, bool bForceValue = false) const;
	virtual void   SetPostEffectParamString(const char* pParam, const char* pszArg) const;

	virtual void   GetPostEffectParam(const char* pParam, float& fValue) const;
	virtual void   GetPostEffectParamVec4(const char* pParam, Vec4& pValue) const;
	virtual void   GetPostEffectParamString(const char* pParam, const char*& pszArg) const;

	virtual int32  GetPostEffectID(const char* pPostEffectName);

	virtual void   ResetPostEffects(bool bOnSpecChange = false) const;

	virtual void   SetShadowsGSMCache(bool bCache);
	virtual void   SetCachedShadowBounds(const AABB& shadowBounds, float fAdditionalCascadesScale);
	virtual void   SetRecomputeCachedShadows(uint nUpdateStrategy = 0);
	virtual void   InvalidateShadowCacheData();
	void           SetShadowsCascadesBias(const float* pCascadeConstBias, const float* pCascadeSlopeBias);
	const float*   GetShadowsCascadesConstBias() const { return m_pShadowCascadeConstBias; }
	const float*   GetShadowsCascadesSlopeBias() const { return m_pShadowCascadeSlopeBias; }
	int            GetShadowsCascadeCount(const SRenderLight* pLight) const;

	virtual uint32 GetObjectsByType(EERType objType, IRenderNode** pObjects);
	virtual uint32 GetObjectsByTypeInBox(EERType objType, const AABB& bbox, IRenderNode** pObjects, uint64 dwFlags = ~0);
	virtual uint32 GetObjectsInBox(const AABB& bbox, IRenderNode** pObjects = 0);
	virtual uint32 GetObjectsByFlags(uint dwFlags, IRenderNode** pObjects = 0);
	virtual void   OnObjectModified(IRenderNode* pRenderNode, IRenderNode::RenderFlagsType dwFlags);

	virtual void   ActivateObjectsLayer(uint16 nLayerId, bool bActivate, bool bPhys, bool bObjects, bool bStaticLights, const char* pLayerName, IGeneralMemoryHeap* pHeap = NULL, bool bCheckLayerActivation = true);
	bool           IsObjectsLayerHidden(uint16 nLayerId, const AABB& objBox);
	virtual void   GetLayerMemoryUsage(uint16 nLayerId, ICrySizer* pSizer, int* pNumBrushes, int* pNumDecals) const;
	virtual void   SkipLayerLoading(uint16 nLayerId, bool bClearList);
	bool           IsLayerSkipped(uint16 nLayerId);

	//////////////////////////////////////////////////////////////////////////

	virtual int  GetTerrainTextureNodeSizeMeters();
	virtual int  GetTerrainTextureNodeSizePixels(int nLayer);

	virtual void SetTerrainLayerBaseTextureData(int nLayerId, byte* pImage, int nDim, const char* nImgFileName, IMaterial* pMat, float fBr, float fTiling, int nDetailSurfTypeId, float fTilingDetail, float fSpecularAmount, float fSortOrder, ColorF layerFilterColor, float fUseRemeshing, bool bShowSelection);
	SImageInfo*  GetBaseTextureData(int nLayerId);
	SImageInfo*  GetBaseTextureDataFromSurfType(int nSurfTypeId);

	const char*  GetLevelFolder() { return m_szLevelFolder; }

	bool         SaveCGF(std::vector<IStatObj*>& pObjs);

	virtual bool IsAreaActivationInUse() { return m_bAreaActivationInUse && GetCVars()->e_ObjectLayersActivation; }

	int          GetCurrentLightSpec()
	{
		return CONFIG_VERYHIGH_SPEC; // very high spec.
	}

	void         UpdateRenderingCamera(const char* szCallerName, const SRenderingPassInfo& passInfo);
	virtual void PrepareOcclusion(const CCamera& rCamera);
	virtual void EndOcclusion();
#ifndef _RELEASE
	void         ProcessStreamingLatencyTest(const CCamera& camIn, CCamera& camOut, const SRenderingPassInfo& passInfo);
#endif

	void ScreenShotHighRes(CStitchedImage* pStitchedImage, const int nRenderFlags, const SRenderingPassInfo& passInfo, uint32 SliceCount, f32 fTransitionSize);

	// cylindrical mapping made by multiple slices rendered and distorted
	// Returns:
	//   true=mode is active, stop normal rendering, false=mode is not active
	bool         ScreenShotPanorama(CStitchedImage* pStitchedImage, const int nRenderFlags, const SRenderingPassInfo& passInfo, uint32 SliceCount, f32 fTransitionSize);
	// Render simple top-down screenshot for map overviews
	bool         ScreenShotMap(CStitchedImage* pStitchedImage, const int nRenderFlags, const SRenderingPassInfo& passInfo, uint32 SliceCount, f32 fTransitionSize);

	void         ScreenshotDispatcher(const int nRenderFlags, const SRenderingPassInfo& passInfo);

	virtual void FillDebugFPSInfo(SDebugFPSInfo& info);

	void         ClearDebugFPSInfo(bool bUnload = false)
	{
		m_fAverageFPS = 0.0f;
		m_fMinFPS = m_fMinFPSDecay = 999.f;
		m_fMaxFPS = m_fMaxFPSDecay = 0.0f;
		if (bUnload)
			stl::free_container(arrFPSforSaveLevelStats);
		else
			arrFPSforSaveLevelStats.clear();
	}

	void ClearPrecacheInfo()
	{
		m_nFramesSinceLevelStart = 0;
		m_nStreamingFramesSinceLevelStart = 0;
		m_bPreCacheEndEventSent = false;
		m_fTimeStateStarted = 0.0f;
	}

	virtual const CCamera& GetRenderingCamera() const { return m_RenderingCamera; }
	virtual float          GetZoomFactor() const      { return m_fZoomFactor; }
	virtual float          IsZoomInProgress()  const  { return m_bZoomInProgress; }
	virtual void           Tick();

	virtual void           UpdateShaderItems();
	void                   GetCollisionClass(SCollisionClass& collclass, int tableIndex);

	void                   SetRecomputeCachedShadows(IRenderNode* pNode, uint updateStrategy);

public:
	C3DEngine(ISystem* pSystem);
	~C3DEngine();

	virtual void RenderScene(const int nRenderFlags, const SRenderingPassInfo& passInfo);
	virtual void DebugDraw_UpdateDebugNode();

	void         DebugDraw_Draw();
	bool         IsOutdoorVisible();
	void         RenderSkyBox(IMaterial* pMat, const SRenderingPassInfo& passInfo);
	int          GetStreamingFramesSinceLevelStart() { return m_nStreamingFramesSinceLevelStart; }
	int          GetRenderFramesSinceLevelStart()    { return m_nFramesSinceLevelStart; }

	bool CreateDecalInstance(const CryEngineDecalInfo &DecalInfo, class CDecal * pCallerManagedDecal);
	//void CreateDecalOnCharacterComponents(ICharacterInstance * pChar, const struct CryEngineDecalInfo & decal);
	Vec3 GetTerrainSurfaceNormal(Vec3 vPos);
	void LoadEnvironmentSettingsFromXML(XmlNodeRef pInputNode);
#if defined(FEATURE_SVO_GI)
	void LoadTISettings(XmlNodeRef pInputNode);
#endif
	void LoadDefaultAssets();

	// access to components
	ILINE static CVars*            GetCVars()              { return m_pCVars; }
	ILINE CVisAreaManager*         GetVisAreaManager()     { return m_pVisAreaManager; }
	ILINE CClipVolumeManager*      GetClipVolumeManager()  { return m_pClipVolumeManager; }
	ILINE PodArray<ILightSource*>* GetLightEntities()      { return &m_lstStaticLights; }

	ILINE IGeneralMemoryHeap*      GetBreakableBrushHeap() { return m_pBreakableBrushHeap; }

	virtual void                   OnCameraTeleport();

	// this data is stored in memory for instances streaming
	std::vector<struct IStatObj*>* m_pLevelStatObjTable;
	std::vector<IMaterial*>*       m_pLevelMaterialsTable;
	bool                           m_bLevelFilesEndian;
	struct SLayerActivityInfo
	{
		SLayerActivityInfo() { objectsBox.Reset(); bActive = false; }
		AABB objectsBox;
		bool bActive;
	};
	PodArray<SLayerActivityInfo> m_arrObjectLayersActivity;
	uint                         m_objectLayersModificationId;
	bool                         m_bAreaActivationInUse;

	// Level info
	float m_fSkyBoxAngle,
	      m_fSkyBoxStretching;

	float                 m_fMaxViewDistScale;
	float                 m_fMaxViewDistHighSpec;
	float                 m_fMaxViewDistLowSpec;
	float                 m_fTerrainDetailMaterialsViewDistRatio;

	float                 m_volFogGlobalDensity;
	float                 m_volFogGlobalDensityMultiplierLDR;
	float                 m_volFogFinalDensityClamp;

	int                   m_nCurWind;         // Current wind-field buffer Id
	SWindGrid             m_WindGrid[2];      // Wind field double-buffered
	Vec2*                 m_pWindField;       // Old wind speed values for interpolation
	int*                  m_pWindAreaFrames;  // Area frames for rest updates
	Vec3                  m_vWindFieldCamera; // Wind field camera for interpolation
	JobManager::SJobState m_WindJobState;
	bool                  m_bWindJobRun;

	float                 m_fCloudShadingSunLightMultiplier;
	float                 m_fCloudShadingSkyLightMultiplier;
	Vec3                  m_vCloudShadingCustomSunColor;
	Vec3                  m_vCloudShadingCustomSkyColor;

	Vec3                  m_vVolCloudAtmosphericScattering;
	Vec3                  m_vVolCloudGenParams;
	Vec3                  m_vVolCloudScatteringLow;
	Vec3                  m_vVolCloudScatteringHigh;
	Vec3                  m_vVolCloudGroundColor;
	Vec3                  m_vVolCloudScatteringMulti;
	Vec3                  m_vVolCloudWindAtmospheric;
	Vec3                  m_vVolCloudTurbulence;
	Vec3                  m_vVolCloudEnvParams;
	Vec3                  m_vVolCloudGlobalNoiseScale;
	Vec3                  m_vVolCloudRenderParams;
	Vec3                  m_vVolCloudTurbulenceNoiseScale;
	Vec3                  m_vVolCloudTurbulenceNoiseParams;
	Vec3                  m_vVolCloudDensityParams;
	Vec3                  m_vVolCloudMiscParams;
	Vec3                  m_vVolCloudTilingSize;
	Vec3                  m_vVolCloudTilingOffset;

	Vec3                  m_vFogColor;
	Vec3                  m_vDefFogColor;
	Vec3                  m_vSunDir;
	Vec3                  m_vSunDirNormalized;
	float                 m_fSunDirUpdateTime;
	Vec3                  m_vSunDirRealtime;
	Vec3                  m_vWindSpeed;

	Vec3                  m_volFogRamp;
	Vec3                  m_volFogShadowRange;
	Vec3                  m_volFogShadowDarkening;
	Vec3                  m_volFogShadowEnable;

	Vec3                  m_volFog2CtrlParams;
	Vec3                  m_volFog2ScatteringParams;
	Vec3                  m_volFog2Ramp;
	Vec3                  m_volFog2Color;
	Vec3                  m_volFog2GlobalDensity;
	Vec3                  m_volFog2HeightDensity;
	Vec3                  m_volFog2HeightDensity2;
	Vec3                  m_volFog2Color1;
	Vec3                  m_volFog2Color2;

	Vec3                  m_nightSkyHorizonCol;
	Vec3                  m_nightSkyZenithCol;
	float                 m_nightSkyZenithColShift;
	float                 m_nightSkyStarIntensity;
	Vec3                  m_nightMoonCol;
	float                 m_nightMoonSize;
	Vec3                  m_nightMoonInnerCoronaCol;
	float                 m_nightMoonInnerCoronaScale;
	Vec3                  m_nightMoonOuterCoronaCol;
	float                 m_nightMoonOuterCoronaScale;

	float                 m_moonRotationLatitude;
	float                 m_moonRotationLongitude;
	Vec3                  m_moonDirection;
	int                   m_nWaterBottomTexId;
	int                   m_nNightMoonTexId;
	bool                  m_bShowTerrainSurface;
	float                 m_fSunClipPlaneRange;
	float                 m_fSunClipPlaneRangeShift;
	bool                  m_bSunShadows;
	bool                  m_bSunShadowsFromTerrain;

	int                   m_nCloudShadowTexId;

	float                 m_fGsmRange;
	float                 m_fGsmRangeStep;
	float                 m_fShadowsConstBias;
	float                 m_fShadowsSlopeBias;

	int                   m_nSunAdditionalCascades;
	int                   m_nGsmCache;
	Vec3                  m_oceanFogColor;
	Vec3                  m_oceanFogColorShallow;
	float                 m_oceanFogDensity;
	float                 m_skyboxMultiplier;
	float                 m_dayNightIndicator;
	bool                  m_bHeightMapAoEnabled;
	bool                  m_bIntegrateObjectsIntoTerrain;

	Vec3                  m_fogColor2;
	Vec3                  m_fogColorRadial;
	Vec3                  m_volFogHeightDensity;
	Vec3                  m_volFogHeightDensity2;
	Vec3                  m_volFogGradientCtrl;

	Vec3                  m_fogColorSkylightRayleighInScatter;

	float                 m_oceanCausticsDistanceAtten;
	float                 m_oceanCausticsMultiplier;
	float                 m_oceanCausticsDarkeningMultiplier;
	float                 m_oceanCausticsTilling;
	float                 m_oceanCausticHeight;
	float                 m_oceanCausticDepth;
	float                 m_oceanCausticIntensity;

	string                m_skyMatName;
	string                m_skyLowSpecMatName;

	float                 m_oceanWindDirection;
	float                 m_oceanWindSpeed;
	float                 m_oceanWavesAmount;
	float                 m_oceanWavesSize;

	float                 m_dawnStart;
	float                 m_dawnEnd;
	float                 m_duskStart;
	float                 m_duskEnd;

	// film characteristic curve tweakables
	Vec4  m_vHDRFilmCurveParams;
	Vec3  m_vHDREyeAdaptation;
	Vec3  m_vHDREyeAdaptationLegacy;
	float m_fHDRBloomAmount;

	// hdr color grading
	Vec3  m_vColorBalance;
	float m_fHDRSaturation;

#ifndef _RELEASE
	CDebugDrawListMgr m_DebugDrawListMgr;
#endif

#define MAX_SHADOW_CASCADES_NUM 20
	float m_pShadowCascadeConstBias[MAX_SHADOW_CASCADES_NUM];
	float m_pShadowCascadeSlopeBias[MAX_SHADOW_CASCADES_NUM];

	AABB  m_CachedShadowsBounds;
	uint  m_nCachedShadowsUpdateStrategy;
	float m_fCachedShadowsCascadeScale;

	// special case for combat mode adjustments
	float m_fSaturation;
	Vec4  m_pPhotoFilterColor;
	float m_fPhotoFilterColorDensity;
	float m_fGrainAmount;
	float m_fSunSpecMult;

	// Level shaders
	_smart_ptr<IMaterial> m_pTerrainWaterMat;
	_smart_ptr<IMaterial> m_pSkyMat;
	_smart_ptr<IMaterial> m_pSkyLowSpecMat;
	_smart_ptr<IMaterial> m_pSunMat;

	// Fog Materials
	_smart_ptr<IMaterial> m_pMatFogVolEllipsoid;
	_smart_ptr<IMaterial> m_pMatFogVolBox;

	void CleanLevelShaders()
	{
		m_pTerrainWaterMat = 0;
		m_pSkyMat = 0;
		m_pSkyLowSpecMat = 0;
		m_pSunMat = 0;

		m_pMatFogVolEllipsoid = 0;
		m_pMatFogVolBox = 0;
	}

	// Render elements
	CRESky*    m_pRESky;
	CREHDRSky* m_pREHDRSky;

	int        m_nDeferredLightsNum;

private:
	// not sorted

	void  LoadTimeOfDaySettingsFromXML(XmlNodeRef node);
	char* GetXMLAttribText(XmlNodeRef pInputNode, const char* szLevel1, const char* szLevel2, const char* szDefaultValue);
	char* GetXMLAttribText(XmlNodeRef pInputNode, const char* szLevel1, const char* szLevel2, const char* szLevel3, const char* szDefaultValue);
	bool  GetXMLAttribBool(XmlNodeRef pInputNode, const char* szLevel1, const char* szLevel2, bool bDefaultValue);

	// without calling high level functions like panorama screenshot
	void RenderInternal(const int nRenderFlags, const SRenderingPassInfo& passInfo, const char* szDebugName);

	void RegisterLightSourceInSectors(SRenderLight* pDynLight, const SRenderingPassInfo& passInfo);

	bool IsCameraAnd3DEngineInvalid(const SRenderingPassInfo& passInfo, const char* szCaller);

	void DebugDrawStreaming(const SRenderingPassInfo& passInfo);
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	void ResetCasterCombinationsCache();

	void FindPotentialLightSources(const SRenderingPassInfo& passInfo);
	void LoadParticleEffects(const char* szFolderName);

	void UpdateSunLightSource(const SRenderingPassInfo& passInfo);

	// Query physics for physical air affecting area and create an optimized query structure for 3d engine
	void UpdateWindAreas();

	void UpdateMoonDirection();

	// Copy objects from tree
	void CopyObjectsByType(EERType objType, const AABB* pBox, PodArray<IRenderNode*>* plistObjects, uint64 dwFlags = ~0);
	void CopyObjects(const AABB* pBox, PodArray<IRenderNode*>* plistObjects);

	void CleanUpOldDecals();
public:
	// functions SRenderingPass
	virtual CCamera* GetRenderingPassCamera(const CCamera& rCamera);
	virtual int      GetZoomMode() const;
	virtual float    GetPrevZoomFactor();
	virtual void     SetZoomMode(int nZoomMode);
	virtual void     SetPrevZoomFactor(float fZoomFactor);

#if defined(FEATURE_SVO_GI)
	virtual bool GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D);
	virtual void GetSvoBricksForUpdate(PodArray<SSvoNodeInfo>& arrNodeInfo, float fNodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut);
	virtual bool IsSvoReady(bool testPostponed) const;
	virtual int  GetSvoCompiledData(ICryArchive* pArchive);
#endif
	// LiveCreate
	virtual void SaveInternalState(struct IDataWriteStream& writer, const AABB& filterArea, const bool bTerrain, const uint32 objectMask);
	virtual void LoadInternalState(struct IDataReadStream& reader, const uint8* pVisibleLayersMask, const uint16* pLayerIdTranslation);

	void         SetupLightScissors(SRenderLight* pLight, const SRenderingPassInfo& passInfo);
	bool         IsTerrainTextureStreamingInProgress() { return m_bTerrainTextureStreamingInProgress; }

	bool         IsTerrainSyncLoad()                   { return m_bContentPrecacheRequested && GetCVars()->e_AutoPrecacheTerrainAndProcVeget; }
	bool         IsShadersSyncLoad()                   { return m_bContentPrecacheRequested && GetCVars()->e_AutoPrecacheTexturesAndShaders; }
	bool         IsStatObjSyncLoad()                   { return m_bContentPrecacheRequested && GetCVars()->e_AutoPrecacheCgf; }
	float        GetAverageCameraSpeed()               { return m_fAverageCameraSpeed; }
	Vec3         GetAverageCameraMoveDir()             { return m_vAverageCameraMoveDir; }

	typedef std::map<uint64, int> ShadowFrustumListsCacheUsers;
	ShadowFrustumListsCacheUsers m_FrustumsCacheUsers[2];

	class CStatObjFoliage*       m_pFirstFoliage, * m_pLastFoliage;
	PodArray<SEntInFoliage>      m_arrEntsInFoliage;
	void RemoveEntInFoliage(int i, IPhysicalEntity* pent = 0);

	PodArray<class CRoadRenderNode*> m_lstRoadRenderNodesForUpdate;

	struct ILightSource*            GetSunEntity();
	PodArray<struct ILightSource*>* GetAffectingLights(const AABB& bbox, bool bAllowSun, const SRenderingPassInfo& passInfo);
	void                            UregisterLightFromAccessabilityCache(ILightSource* pLight);
	void                            OnCasterDeleted(IShadowCaster* pCaster);

	virtual void                    ResetCoverageBufferSignalVariables();

	void                            UpdateScene(const SRenderingPassInfo& passInfo);
	void                            UpdateLightSources(const SRenderingPassInfo& passInfo);
	void                            PrepareLightSourcesForRendering_0(const SRenderingPassInfo& passInfo);
	void                            PrepareLightSourcesForRendering_1(const SRenderingPassInfo& passInfo);
	void                            InitShadowFrustums(const SRenderingPassInfo& passInfo);
	void                            PrepareShadowPasses(const SRenderingPassInfo& passInfo, uint32& nTimeSlicedShadowsUpdatedThisFrame, std::vector<std::pair<ShadowMapFrustum*, const class CLightEntity*>>& shadowFrustums, std::vector<SRenderingPassInfo>& shadowPassInfo);

	///////////////////////////////////////////////////////////////////////////////

	virtual void            GetLightVolumes(threadID nThreadID, SLightVolume*& pLightVols, uint32& nNumVols);
	const CLightVolumesMgr& GetLightVolumeManager() const { return m_LightVolumesMgr; }
	CLightVolumesMgr&       GetLightVolumeManager()       { return m_LightVolumesMgr; }

	///////////////////////////////////////////////////////////////////////////////

	void                             FreeLightSourceComponents(SRenderLight* pLight, bool bDeleteLight = true);
	void                             RemoveEntityLightSources(IRenderNode* pEntity);

	void                             CheckPhysicalized(const Vec3& vBoxMin, const Vec3& vBoxMax);

	virtual PodArray<SRenderLight*>* GetDynamicLightSources() { return &m_lstDynLights; }

	int                              GetRealLightsNum()       { return m_nRealLightsNum; }
	void                             SetupClearColor(const SRenderingPassInfo& passInfo);
	void                             CheckAddLight(SRenderLight* pLight, const SRenderingPassInfo& passInfo);

	void                             DrawTextRightAligned(const float x, const float y, const char* format, ...) PRINTF_PARAMS(4, 5);
	void                             DrawTextRightAligned(const float x, const float y, const float scale, const ColorF& color, const char* format, ...) PRINTF_PARAMS(6, 7);
	void                             DrawTextLeftAligned(const float x, const float y, const float scale, const ColorF& color, const char* format, ...) PRINTF_PARAMS(6, 7);
	void                             DrawTextAligned(int flags, const float x, const float y, const float scale, const ColorF& color, const char* format, ...) PRINTF_PARAMS(7, 8);

	float                            GetLightAmount(SRenderLight* pLight, const AABB& objBox);

	IStatObj*                        CreateStatObj();
	virtual IStatObj*                CreateStatObjOptionalIndexedMesh(bool createIndexedMesh);

	IStatObj*                        UpdateDeformableStatObj(IGeometry* pPhysGeom, bop_meshupdate* pLastUpdate = 0, IFoliage* pSrcFoliage = 0);

	// Creates a new indexed mesh.
	IIndexedMesh*                 CreateIndexedMesh();

	void                          InitMaterialDefautMappingAxis(IMaterial* pMat);

	virtual ITerrain*             GetITerrain()             { return (ITerrain*)m_pTerrain; }
	virtual IVisAreaManager*      GetIVisAreaManager()      { return (IVisAreaManager*)m_pVisAreaManager; }
	virtual IMergedMeshesManager* GetIMergedMeshesManager() { return (IMergedMeshesManager*)m_pMergedMeshesManager; }

	virtual ITerrain*             CreateTerrain(const STerrainInfo& TerrainInfo);
	void                          DeleteTerrain();
	bool                          LoadTerrain(XmlNodeRef pDoc, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable);
	bool                          LoadVisAreas(std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable);
	bool                          LoadUsedShadersList();
	bool                          PrecreateDecals();
	void                          LoadPhysicsData();
	void                          UnloadPhysicsData();
	void                          LoadFlaresData();
	void                          FreeFoliages();

	void                          LoadCollisionClasses(XmlNodeRef node);

	virtual float                 GetLightAmountInRange(const Vec3& pPos, float fRange, bool bAccurate = 0);

	virtual void                  PrecacheLevel(bool bPrecacheAllVisAreas, Vec3* pPrecachePoints, int nPrecachePointsNum);
	virtual void                  ProposeContentPrecache()     { m_bContentPrecacheRequested = true; }
	bool                          IsContentPrecacheRequested() { return m_bContentPrecacheRequested; }

	virtual ITimeOfDay*           GetTimeOfDay();
	//! [GDC09]: Return SkyBox material
	virtual IMaterial*            GetSkyMaterial();
	virtual void                  SetSkyMaterial(IMaterial* pSkyMat);
	bool                          IsHDRSkyMaterial(IMaterial* pMat) const;

	using I3DEngine::SetGlobalParameter;
	virtual void                     SetGlobalParameter(E3DEngineParameter param, const Vec3& v);
	using I3DEngine::GetGlobalParameter;
	virtual void                     GetGlobalParameter(E3DEngineParameter param, Vec3& v);
	virtual void                     SetShadowMode(EShadowMode shadowMode) { m_eShadowMode = shadowMode; }
	virtual EShadowMode              GetShadowMode() const                 { return m_eShadowMode; }
	virtual void                     AddPerObjectShadow(IShadowCaster* pCaster, float fConstBias, float fSlopeBias, float fJitter, const Vec3& vBBoxScale, uint nTexSize);
	virtual void                     RemovePerObjectShadow(IShadowCaster* pCaster);
	virtual struct SPerObjectShadow* GetPerObjectShadow(IShadowCaster* pCaster);
	virtual void                     GetCustomShadowMapFrustums(ShadowMapFrustum**& arrFrustums, int& nFrustumCount);
	virtual int                      SaveStatObj(IStatObj* pStatObj, TSerialize ser);
	virtual IStatObj*                LoadStatObj(TSerialize ser);

	virtual void                     OnRenderMeshDeleted(IRenderMesh* pRenderMesh);
	virtual bool                     RenderMeshRayIntersection(IRenderMesh* pRenderMesh, SRayHitInfo& hitInfo, IMaterial* pCustomMtl = 0);
	virtual void                     OnEntityDeleted(struct IEntity* pEntity);
	virtual const char*              GetVoxelEditOperationName(EVoxelEditOperation eOperation);

	virtual void                     SetEditorHeightmapCallback(IEditorHeightmap* pCallBack) { m_pEditorHeightmap = pCallBack; }
	static IEditorHeightmap* m_pEditorHeightmap;

	virtual IParticleManager* GetParticleManager() { return m_pPartManager; }
	virtual IOpticsManager*   GetOpticsManager()   { return m_pOpticsManager; }

	virtual void              RegisterForStreaming(IStreamable* pObj);
	virtual void              UnregisterForStreaming(IStreamable* pObj);

	virtual void              PrecacheCharacter(IRenderNode* pObj, const float fImportance, ICharacterInstance* pCharacter, IMaterial* pSlotMat, const Matrix34& matParent, const float fEntDistance, const float fScale, int nMaxDepth, bool bForceStreamingSystemUpdate, const SRenderingPassInfo& passInfo);
	virtual void              PrecacheRenderNode(IRenderNode* pObj, float fEntDistanceReal);

	void                      MarkRNTmpDataPoolForReset() { m_bResetRNTmpDataPool = true; }

	bool                      IsObjectsTreeValid()        { return m_pObjectsTree != nullptr; }
	class COctreeNode*   GetObjectsTree()            { return m_pObjectsTree; }

	static void          GetObjectsByTypeGlobal(PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, bool* pInstStreamReady = NULL, uint64 dwFlags = ~0);
	static void          MoveObjectsIntoListGlobal(PodArray<SRNInfo>* plstResultEntities, const AABB* pAreaBox, bool bRemoveObjects = false, bool bSkipDecals = false, bool bSkip_ERF_NO_DECALNODE_DECALS = false, bool bSkipDynamicObjects = false, EERType eRNType = eERType_TypesNum);

	SRenderNodeTempData* CreateRenderNodeTempData(IRenderNode* pRNode, const SRenderingPassInfo& passInfo);
	SRenderNodeTempData* CheckAndCreateRenderNodeTempData(IRenderNode* pRNode, const SRenderingPassInfo& passInfo);

	void                 UpdateRNTmpDataPool(bool bFreeAll);

	void                 UpdateStatInstGroups();
	void                 UpdateRenderTypeEnableLookup();
	void                 ProcessOcean(const SRenderingPassInfo& passInfo);
	void                 ReRegisterKilledVegetationInstances();
	Vec3                 GetEntityRegisterPoint(IRenderNode* pEnt);

	void                 RenderRenderNode_ShadowPass(IShadowCaster* pRNode, const SRenderingPassInfo& passInfo);
	void                 ProcessCVarsChange();
	ILINE int            GetGeomDetailScreenRes()
	{
		if (GetCVars()->e_ForceDetailLevelForScreenRes)
		{
			return GetCVars()->e_ForceDetailLevelForScreenRes;
		}
		else if (GetRenderer())
		{
			return std::max(GetRenderer()->GetOverlayWidth(), GetRenderer()->GetOverlayHeight());
		}
		return 1;
	}

	int                                   GetBlackTexID()   { return m_nBlackTexID; }
	int                                   GetBlackCMTexID() { return m_nBlackCMTexID; }
	int                                   GetWhiteTexID()   { return m_nWhiteTexID; }

	virtual void                          SyncProcessStreamingUpdate();

	virtual void                          SetScreenshotCallback(IScreenshotCallback* pCallback);

	virtual void                          RegisterRenderNodeStatusListener(IRenderNodeStatusListener* pListener, EERType renderNodeType);
	virtual void                          UnregisterRenderNodeStatusListener(IRenderNodeStatusListener* pListener, EERType renderNodeType);

	virtual IDeferredPhysicsEventManager* GetDeferredPhysicsEventManager() { return m_pDeferredPhysicsEventManager; }

	void                                  PrintDebugInfo(const SRenderingPassInfo& passInfo);

public:
	//////////////////////////////////////////////////////////////////////////
	// PUBLIC DATA
	//////////////////////////////////////////////////////////////////////////
	class COctreeNode*             m_pObjectsTree = nullptr;

	int                            m_idMatLeaves; // for shooting foliages
	bool                           m_bResetRNTmpDataPool;

	float                          m_fRefreshSceneDataCVarsSumm;
	int                            m_nRenderTypeEnableCVarSum;

	PodArray<IRenderNode*>         m_lstAlwaysVisible;
	PodArray<IRenderNode*>         m_lstKilledVegetations;
	PodArray<SPerObjectShadow>     m_lstPerObjectShadows;
	std::vector<ShadowMapFrustum*> m_lstCustomShadowFrustums;
	int                            m_nCustomShadowFrustumCount;
	uint32                         m_onePassShadowFrustumsCount = 0;

	PodArray<SImageInfo>           m_arrBaseTextureData;

	bool                           m_bInShutDown;
	bool                           m_bInUnload;
	bool                           m_bInLoad;

private:
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE DATA
	//////////////////////////////////////////////////////////////////////////
	class CLightEntity* m_pSun;

	std::vector<byte>   arrFPSforSaveLevelStats;
	PodArray<float>     m_arrProcessStreamingLatencyTestResults;
	PodArray<int>       m_arrProcessStreamingLatencyTexNum;

	// fields which are used by SRenderingPass to store over frame information
	CThreadSafeRendererContainer<CCamera> m_RenderingPassCameras[2];                 // camera storage for SRenderingPass, the cameras cannot be stored on stack to allow job execution

	float m_fZoomFactor;                                // zoom factor of m_RenderingCamera
	float m_fPrevZoomFactor;                            // zoom factor of m_RenderingCamera from last frame
	bool  m_bZoomInProgress;                            // indicates if the RenderingCamera is currently zooming
	int   m_nZoomMode;                                  // the zoom level of the camera (0-4) 0: no zoom, 4: full zoom

	// cameras used by 3DEngine
	CCamera                m_RenderingCamera;           // Camera used for Rendering on 3DEngine Side, normaly equal to the viewcamera, except if frozen with e_camerafreeze

	PodArray<IRenderNode*> m_deferredRenderProxyStreamingPriorityUpdates;     // deferred streaming priority updates for newly seen CRenderProxies

	float                  m_fLightsHDRDynamicPowerFactor; // lights hdr exponent/exposure

	int                    m_nBlackTexID;
	int                    m_nBlackCMTexID;
	int                    m_nWhiteTexID;

	char                   m_sGetLevelFilePathTmpBuff[MAX_PATH_LENGTH];
	char                   m_szLevelFolder[_MAX_PATH];

	bool                   m_bOcean; // todo: remove

	Vec3                   m_vSkyHightlightPos;
	Vec3                   m_vSkyHightlightCol;
	float                  m_fSkyHighlightSize;
	Vec3                   m_vAmbGroundCol;
	float                  m_fAmbMaxHeight;
	float                  m_fAmbMinHeight;
	IPhysicalEntity*       m_pGlobalWind;
	uint8                  m_nOceanRenderFlags;
	Vec3                   m_vPrevMainFrameCamPos;
	float                  m_fAverageCameraSpeed;
	Vec3                   m_vAverageCameraMoveDir;
	EShadowMode            m_eShadowMode;
	bool                   m_bLayersActivated;
	bool                   m_bContentPrecacheRequested;
	bool                   m_bTerrainTextureStreamingInProgress;

	// interfaces
	IPhysMaterialEnumerator* m_pPhysMaterialEnumerator;

	// data containers
	PodArray<SRenderLight*>                   m_lstDynLights;
	PodArray<SRenderLight*>                   m_lstDynLightsNoLight;
	int                                       m_nRealLightsNum;

	PodArray<ILightSource*>                   m_lstStaticLights;
	PodArray<PodArray<struct ILightSource*>*> m_lstAffectingLightsCombinations;
	PodArray<SRenderLight*>                   m_tmpLstLights;
	PodArray<struct ILightSource*>            m_tmpLstAffectingLights;

	PodArray<SCollisionClass>                 m_collisionClasses;

#define MAX_LIGHTS_NUM 32
	PodArray<CCamera> m_arrLightProjFrustums;

	class CTimeOfDay* m_pTimeOfDay;

	ICVar*            m_pLightQuality;

	// FPS for savelevelstats

	float                          m_fAverageFPS;
	float                          m_fMinFPS, m_fMinFPSDecay;
	float                          m_fMaxFPS, m_fMaxFPSDecay;
	int                            m_nFramesSinceLevelStart;
	int                            m_nStreamingFramesSinceLevelStart;
	bool                           m_bPreCacheEndEventSent;
	float                          m_fTimeStateStarted;
	uint32                         m_nRenderWorldUSecs;
	SFrameLodInfo                  m_frameLodInfo;

	ITexture*                      m_ptexIconLowMemoryUsage;
	ITexture*                      m_ptexIconAverageMemoryUsage;
	ITexture*                      m_ptexIconHighMemoryUsage;
	ITexture*                      m_ptexIconEditorConnectedToConsole;

	std::vector<IDecalRenderNode*> m_decalRenderNodes; // list of registered decal render nodes, used to clean up longer not drawn decals
	std::vector<IRenderNode*>      m_renderNodesToDelete[2];    // delay deletion of rendernodes by 1 frame to make sure 
	uint32                         m_renderNodesToDeleteID = 0; // they can be safely used on renderthread 

	SImageSubInfo* RegisterImageInfo(byte** pMips, int nDim, const char* pName);
	SImageSubInfo* GetImageInfo(const char* pName);
	std::map<string, SImageSubInfo*> m_imageInfos;
	byte**         AllocateMips(byte* pImage, int nDim, byte** pImageMips);
	IScreenshotCallback*             m_pScreenshotCallback;

	typedef CListenerSet<IRenderNodeStatusListener*> TRenderNodeStatusListeners;
	typedef std::vector<TRenderNodeStatusListeners>  TRenderNodeStatusListenersArray;
	TRenderNodeStatusListenersArray        m_renderNodeStatusListenersArray;

	OcclusionTestClient                    m_OceanOcclTestVar;

	IDeferredPhysicsEventManager*          m_pDeferredPhysicsEventManager;

	std::set<uint16>                       m_skipedLayers;

	IGeneralMemoryHeap*                    m_pBreakableBrushHeap;

	CVisibleRenderNodesManager             m_visibleNodesManager;

	int                                    m_nCurrentWindAreaList;
	std::vector<SOptimizedOutdoorWindArea> m_outdoorWindAreas[2];
	std::vector<SOptimizedOutdoorWindArea> m_indoorWindAreas[2];
	std::vector<SOptimizedOutdoorWindArea> m_forcedWindAreas;

	CLightVolumesMgr                       m_LightVolumesMgr;

	std::unique_ptr<CWaterRippleManager>   m_pWaterRippleManager;

	friend struct SRenderNodeTempData;
};

#endif // C3DENGINE_H
