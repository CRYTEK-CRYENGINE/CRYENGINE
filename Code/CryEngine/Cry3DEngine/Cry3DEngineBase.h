// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   cry3denginebase.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Access to external stuff used by 3d engine. Most 3d engine classes
//               are derived from this base class to access other interfaces
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _Cry3DEngineBase_h_
#define _Cry3DEngineBase_h_

#include "3DEngineMemory.h"

struct ISystem;
struct IRenderer;
struct ILog;
struct IPhysicalWorld;
struct ITimer;
struct IConsole;
struct I3DEngine;
struct CVars;
struct CVisAreaManager;
namespace pfx2{
struct IParticleSystem;
}
class CTerrain;
class CIndirectLighting;
class CObjManager;
class C3DEngine;
class CParticleManager;
class CDecalManager;
class CRainManager;
class CSkyLightManager;
class CWaterWaveManager;
class CRenderMeshMerger;
class CMergedMeshesManager;
class CGeomCacheManager;
class CBreezeGenerator;
class CMatMan;
class CClipVolumeManager;

#define DISTANCE_TO_THE_SUN 1000000

#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	#define OBJMAN_STREAM_STATS
#endif

struct Cry3DEngineBase
{
	static ISystem*                               m_pSystem;
#if !defined(DEDICATED_SERVER)
	static IRenderer*                             m_pRenderer;
#else
	static IRenderer* const                       m_pRenderer;
#endif
	static ITimer*                                m_pTimer;
	static ILog*                                  m_pLog;
	static IPhysicalWorld*                        m_pPhysicalWorld;
	static IConsole*                              m_pConsole;
	static C3DEngine*                             m_p3DEngine;
	static CVars*                                 m_pCVars;
	static ICryPak*                               m_pCryPak;
	static CObjManager*                           m_pObjManager;
	static CTerrain*                              m_pTerrain;
	static IParticleManager*                      m_pPartManager;
	static std::shared_ptr<pfx2::IParticleSystem> m_pParticleSystem;
	static IOpticsManager*                        m_pOpticsManager;
	static CDecalManager*                         m_pDecalManager;
	static CVisAreaManager*                       m_pVisAreaManager;
	static CClipVolumeManager*                    m_pClipVolumeManager;
	static CMatMan*                               m_pMatMan;
	static CSkyLightManager*                      m_pSkyLightManager;
	static CWaterWaveManager*                     m_pWaterWaveManager;
	static CRenderMeshMerger*                     m_pRenderMeshMerger;
	static CMergedMeshesManager*                  m_pMergedMeshesManager;
	static CBreezeGenerator*                      m_pBreezeGenerator;
	static IStreamedObjectListener*               m_pStreamListener;
#if defined(USE_GEOM_CACHES)
	static CGeomCacheManager*                     m_pGeomCacheManager;
#endif

	static bool              m_bProfilerEnabled;
	static threadID          m_nMainThreadId;
	static bool              m_bLevelLoadingInProgress;
	static bool              m_bIsInRenderScene;
	static bool              m_bAsyncOctreeUpdates;
	static bool              m_bRenderTypeEnabled[eERType_TypesNum];
	static int               m_mergedMeshesPoolSize;

	static ESystemConfigSpec m_LightConfigSpec;
#if CRY_PLATFORM_DESKTOP
	static bool              m_bEditor;
#else
	static const bool        m_bEditor = false;
#endif
	static int               m_arrInstancesCounter[eERType_TypesNum];

	// components access
	ILINE static ISystem*            GetSystem()                 { return m_pSystem; }
	ILINE static IRenderer*          GetRenderer()               { return m_pRenderer; }
	ILINE static ITimer*             GetTimer()                  { return m_pTimer; }
	ILINE static ILog*               GetLog()                    { return m_pLog; }

	inline static IPhysicalWorld*    GetPhysicalWorld()          { return m_pPhysicalWorld; }
	inline static IConsole*          GetConsole()                { return m_pConsole; }
	inline static C3DEngine*         Get3DEngine()               { return m_p3DEngine; }
	inline static CObjManager*       GetObjManager()             { return m_pObjManager; };
	inline static CTerrain*          GetTerrain()                { return m_pTerrain; };
	inline static CVars*             GetCVars()                  { return m_pCVars; }
	inline static CVisAreaManager*   GetVisAreaManager()         { return m_pVisAreaManager; }
	inline static ICryPak*           GetPak()                    { return m_pCryPak; }
	inline static CMatMan*           GetMatMan()                 { return m_pMatMan; }
	inline static CWaterWaveManager* GetWaterWaveManager()       { return m_pWaterWaveManager; };
	inline static CRenderMeshMerger* GetSharedRenderMeshMerger() { return m_pRenderMeshMerger; };
	inline static CTemporaryPool*    GetTemporaryPool()          { return CTemporaryPool::Get(); };

#if defined(USE_GEOM_CACHES)
	inline static CGeomCacheManager* GetGeomCacheManager() { return m_pGeomCacheManager; };
#endif

	inline static int GetMergedMeshesPoolSize()                               { return m_mergedMeshesPoolSize; }
	ILINE static bool IsRenderNodeTypeEnabled(EERType rnType)                 { return m_bRenderTypeEnabled[(int)rnType]; }
	ILINE static void SetRenderNodeTypeEnabled(EERType rnType, bool bEnabled) { m_bRenderTypeEnabled[(int)rnType] = bEnabled; }

	static float      GetCurTimeSec();
	static float      GetCurAsyncTimeSec();

	static void       PrintMessage(const char* szText, ...) PRINTF_PARAMS(1, 2);
	static void       PrintMessagePlus(const char* szText, ...) PRINTF_PARAMS(1, 2);
	static void       PrintComment(const char* szText, ...) PRINTF_PARAMS(1, 2);

	// Validator warning.
	static void    Warning(const char* format, ...) PRINTF_PARAMS(1, 2);
	static void    Error(const char* format, ...) PRINTF_PARAMS(1, 2);
	static void    FileWarning(int flags, const char* file, const char* format, ...) PRINTF_PARAMS(3, 4);

	CRenderObject* GetIdentityCRenderObject(const SRenderingPassInfo &passInfo)
	{
		CRenderObject* pCRenderObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
		if (!pCRenderObject)
			return NULL;
		pCRenderObject->SetMatrix(Matrix34::CreateIdentity(), passInfo);
		return pCRenderObject;
	}

	static bool IsValidFile(const char* sFilename);
	static bool IsResourceLocked(const char* sFilename);

	static bool IsPreloadEnabled();

	IMaterial*  MakeSystemMaterialFromShader(const char* sShaderName, SInputShaderResources* Res = NULL);
	static void DrawBBoxLabeled(const AABB& aabb, const Matrix34& m34, const ColorB& col, const char* format, ...) PRINTF_PARAMS(4, 5);
	static void DrawBBox(const Vec3& vMin, const Vec3& vMax, ColorB col = Col_White);
	static void DrawBBox(const AABB& box, ColorB col = Col_White);
	static void DrawLine(const Vec3& vMin, const Vec3& vMax, ColorB col = Col_White);
	static void DrawSphere(const Vec3& vPos, float fRadius, ColorB color = ColorB(255, 255, 255, 255));
	static void DrawQuad(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3, ColorB color);

	int&        GetInstCount(EERType eType)                            { return m_arrInstancesCounter[eType]; }

	uint32      GetMinSpecFromRenderNodeFlags(uint64 dwRndFlags) const { return (dwRndFlags & ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT; }
	static bool CheckMinSpec(uint32 nMinSpec);

	static bool IsEscapePressed();

	size_t      fread(
	  void* buffer,
	  size_t elementSize,
	  size_t count,
	  FILE* stream)
	{
		size_t res = ::fread(buffer, elementSize, count, stream);
		if (res != count)
			Error("fread() failed");
		return res;
	}

	int fseek(
	  FILE* stream,
	  long offset,
	  int whence
	  )
	{
		int res = ::fseek(stream, offset, whence);
		if (res != 0)
			Error("fseek() failed");
		return res;
	}

};

#endif // _Cry3DEngineBase_h_
