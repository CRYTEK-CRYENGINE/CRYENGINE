// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   cry3dengine.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Defines the DLL entry point, implements access to other modules
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

// Must be included only once in DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include "3dEngine.h"
#include "MatMan.h"

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#define MAX_ERROR_STRING MAX_WARNING_LENGTH

// Disable printf argument verification since it is generated at runtime
#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic ignored "-Wformat-security"
	#else
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wformat-security"
	#endif
#endif
//////////////////////////////////////////////////////////////////////

struct CSystemEventListener_3DEngine : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			{
				if (Cry3DEngineBase::Get3DEngine())
					Cry3DEngineBase::Get3DEngine()->ClearDebugFPSInfo(true);
				break;
			}
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			{
				if (Cry3DEngineBase::Get3DEngine())
					Cry3DEngineBase::Get3DEngine()->ClearDebugFPSInfo();
				if (Cry3DEngineBase::GetObjManager())
					Cry3DEngineBase::GetObjManager()->FreeNotUsedCGFs();
				Cry3DEngineBase::m_bLevelLoadingInProgress = false;
				break;
			}
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				Cry3DEngineBase::m_bLevelLoadingInProgress = true;
				break;
			}
		case ESYSTEM_EVENT_LEVEL_UNLOAD:
			{
				Cry3DEngineBase::m_bLevelLoadingInProgress = true;
				break;
			}
		case ESYSTEM_EVENT_3D_POST_RENDERING_START:
			{
				Cry3DEngineBase::GetMatMan()->DoLoadSurfaceTypesInInit(false);
				break;
			}
		case ESYSTEM_EVENT_3D_POST_RENDERING_END:
			{
				SAFE_DELETE(Cry3DEngineBase::Get3DEngine()->m_pObjectsTree);

				// We have to unload physics data *before* shutting down the geom manager
				// Otherwise physical entities that are destroyed later will reference dangling geom pointers
				Cry3DEngineBase::Get3DEngine()->UnloadPhysicsData();

				if (CObjManager* pObjManager = Cry3DEngineBase::GetObjManager())
				{
					pObjManager->UnloadObjects(true);
				}

				if (IGeomManager* pGeomManager = Cry3DEngineBase::GetPhysicalWorld()->GetGeomManager())
				{
					pGeomManager->ShutDownGeoman();
				}

				if (Cry3DEngineBase::GetMatMan())
				{
					Cry3DEngineBase::GetMatMan()->ShutDown();
					Cry3DEngineBase::GetMatMan()->DoLoadSurfaceTypesInInit(true);
				}

				break;
			}
		}
	}
};
static CSystemEventListener_3DEngine g_system_event_listener_engine;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CEngineModule_Cry3DEngine : public I3DEngineModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(I3DEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_Cry3DEngine, "EngineModule_Cry3DEngine", "2d38f12a-521d-43cf-ba18-fd1fa7ea5020"_cry_guid)

	virtual ~CEngineModule_Cry3DEngine()
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(&g_system_event_listener_engine);
		SAFE_RELEASE(gEnv->p3DEngine);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override     { return "Cry3DEngine"; }
	virtual const char* GetCategory() const override { return "CryEngine"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;

		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_engine, "CSystemEventListener_3DEngine");

		C3DEngine* p3DEngine = CryAlignedNew<C3DEngine>(pSystem);
		env.p3DEngine = p3DEngine;
		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_Cry3DEngine)

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::PrintComment(const char* szText, ...)
{
	if (!szText)
		return;

	va_list args;
	va_start(args, szText);
	GetLog()->LogV(IMiniLog::eComment, szText, args);
	va_end(args);
}

void Cry3DEngineBase::PrintMessage(const char* szText, ...)
{
	if (!szText)
		return;

	va_list args;
	va_start(args, szText);
	GetLog()->LogV(GetCVars()->e_3dEngineLogAlways ? IMiniLog::eAlways : IMiniLog::eMessage, szText, args);
	va_end(args);

	GetLog()->UpdateLoadingScreen(0);
}

void Cry3DEngineBase::PrintMessagePlus(const char* szText, ...)
{
	if (!szText)
		return;

	va_list arglist;
	char buf[MAX_ERROR_STRING];
	va_start(arglist, szText);
	cry_vsprintf(buf, szText, arglist);
	va_end(arglist);
	GetLog()->LogPlus(buf);

	GetLog()->UpdateLoadingScreen(0);
}

float Cry3DEngineBase::GetCurTimeSec()
{ return (gEnv->pTimer->GetCurrTime()); }

float Cry3DEngineBase::GetCurAsyncTimeSec()
{ return (gEnv->pTimer->GetAsyncTime().GetSeconds()); }

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::Warning(const char* format, ...)
{
	if (!format)
		return;

	va_list args;
	va_start(args, format);
	// Call to validating warning of system.
	m_pSystem->WarningV(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, 0, 0, format, args);
	va_end(args);

	GetLog()->UpdateLoadingScreen(0);
}

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::Error(const char* format, ...)
{
	//  assert(!"Cry3DEngineBase::Error");

	va_list args;
	va_start(args, format);
	// Call to validating warning of system.
	m_pSystem->WarningV(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, 0, 0, format, args);
	va_end(args);

	GetLog()->UpdateLoadingScreen(0);
}

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::FileWarning(int flags, const char* file, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	// Call to validating warning of system.
	m_pSystem->WarningV(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, flags | VALIDATOR_FLAG_FILE, file, format, args);
	va_end(args);

	GetLog()->UpdateLoadingScreen(0);
}

IMaterial* Cry3DEngineBase::MakeSystemMaterialFromShader(const char* sShaderName, SInputShaderResources* Res)
{
	IMaterial* pMat = Get3DEngine()->GetMaterialManager()->CreateMaterial(sShaderName);
	//pMat->AddRef();

	if (GetRenderer())
	{
		SShaderItem si;
		si = GetRenderer()->EF_LoadShaderItem(sShaderName, true, 0, Res);
		pMat->AssignShaderItem(si);
	}

	return pMat;
}

//////////////////////////////////////////////////////////////////////////
bool Cry3DEngineBase::IsValidFile(const char* sFilename)
{
	LOADING_TIME_PROFILE_SECTION;

	return gEnv->pCryPak->IsFileExist(sFilename);
}

//////////////////////////////////////////////////////////////////////////
bool Cry3DEngineBase::IsResourceLocked(const char* sFilename)
{
	IResourceList* pResList = GetPak()->GetResourceList(ICryPak::RFOM_NextLevel);
	if (pResList)
	{
		return pResList->IsExist(sFilename);
	}
	return false;
}

void Cry3DEngineBase::DrawBBoxLabeled(const AABB& aabb, const Matrix34& m34, const ColorB& col, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char szText[256];
	cry_vsprintf(szText, format, args);
	va_end(args);
	float fColor[4] = { col[0] / 255.f, col[1] / 255.f, col[2] / 255.f, col[3] / 255.f };
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	IRenderAuxText::DrawLabelEx(m34.TransformPoint(aabb.GetCenter()), 1.3f, fColor, true, true, szText);
	GetRenderer()->GetIRenderAuxGeom()->DrawAABB(aabb, m34, false, col, eBBD_Faceted);
}

//////////////////////////////////////////////////////////////////////////
void Cry3DEngineBase::DrawBBox(const Vec3& vMin, const Vec3& vMax, ColorB col)
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vMin, vMax), false, col, eBBD_Faceted);
}

void Cry3DEngineBase::DrawBBox(const AABB& box, ColorB col)
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawAABB(box, false, col, eBBD_Faceted);
}

void Cry3DEngineBase::DrawLine(const Vec3& vMin, const Vec3& vMax, ColorB col)
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawLine(vMin, col, vMax, col);
}

void Cry3DEngineBase::DrawSphere(const Vec3& vPos, float fRadius, ColorB color)
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawSphere(vPos, fRadius, color);
}

void Cry3DEngineBase::DrawQuad(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3, ColorB color)
{
	GetRenderer()->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	GetRenderer()->GetIRenderAuxGeom()->DrawTriangle(v0, color, v2, color, v3, color);
	GetRenderer()->GetIRenderAuxGeom()->DrawTriangle(v0, color, v1, color, v2, color);
}

// Check if preloading is enabled.
bool Cry3DEngineBase::IsPreloadEnabled()
{
	bool bPreload = false;
	ICVar* pSysPreload = GetConsole()->GetCVar("sys_preload");
	if (pSysPreload && pSysPreload->GetIVal() != 0)
		bPreload = true;

	return bPreload;
}

//////////////////////////////////////////////////////////////////////////
bool Cry3DEngineBase::CheckMinSpec(uint32 nMinSpec)
{
	if (nMinSpec == CONFIG_DETAIL_SPEC && GetCVars()->e_ViewDistRatioDetail == 0)
		return false;

	if ((int)nMinSpec != 0 && GetCVars()->e_ObjQuality != 0 && (int)nMinSpec > GetCVars()->e_ObjQuality)
		return false;

	return true;
}

bool Cry3DEngineBase::IsEscapePressed()
{
#if CRY_PLATFORM_WINDOWS
	if (Cry3DEngineBase::m_bEditor && (CryGetAsyncKeyState(VK_CANCEL) & (1 << 15)) && (CryGetAsyncKeyState(VK_ESCAPE) & (1 << 15)))
	{
		Get3DEngine()->PrintMessage("*** ESC key was pressed - operation aborted ***");
		return true;
	}
#endif
	return false;
}

#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic error "-Wformat-security"
	#else
		#pragma GCC diagnostic pop
	#endif
#endif

#include <CryCore/CrtDebugStats.h>

// TypeInfo implementations for Cry3DEngine
#ifndef _LIB
	#include <CryCore/Common_TypeInfo.h>
#endif

#include <CryCore/TypeInfo_impl.h>

// Common types
#include <Cry3DEngine/IIndexedMesh_info.h>
#include <Cry3DEngine/IRenderNode_info.h>
#include <Cry3DEngine/CGF/CGFContent_info.h>

// 3DEngine types
#include "SkyLightNishita_info.h"
#include "terrain_sector_info.h"

STRUCT_INFO_BEGIN(SImageSubInfo)
VAR_INFO(nDummy)
VAR_INFO(nDim)
VAR_INFO(fTilingIn)
VAR_INFO(fTiling)
VAR_INFO(fSpecularAmount)
VAR_INFO(nSortOrder)
STRUCT_INFO_END(SImageSubInfo)

STRUCT_INFO_BEGIN(SImageInfo)
VAR_INFO(baseInfo)
VAR_INFO(detailInfo)
VAR_INFO(szDetMatName)
VAR_INFO(arrTextureId)
VAR_INFO(nPhysSurfaceType)
VAR_INFO(szBaseTexName)
VAR_INFO(fUseRemeshing)
VAR_INFO(layerFilterColor)
VAR_INFO(nLayerId)
VAR_INFO(fBr)
STRUCT_INFO_END(SImageInfo)
