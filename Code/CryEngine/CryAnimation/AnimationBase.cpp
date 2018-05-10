// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AnimationBase.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IFacialAnimation.h>
#include "CharacterManager.h"
#include "AnimEventLoader.h"
#include "SkeletonAnim.h"
#include "AttachmentSkin.h"

#include <CryCore/Platform/platform_impl.inl> // Must be included once in the module.
#include <CrySystem/IEngineModule.h>

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListener_Animation : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_UNLOAD:
			break;
		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			{
				if (Memory::CPoolFrameLocal* pMemoryPool = CharacterInstanceProcessing::GetMemoryPool())
				{
					pMemoryPool->ReleaseBuckets();
				}

				if (!gEnv->IsEditor())
				{
					delete g_pCharacterManager;
					g_pCharacterManager = NULL;
					gEnv->pCharacterManager = NULL;
				}

				break;
			}
		case ESYSTEM_EVENT_3D_POST_RENDERING_START:
			{
				if (!g_pCharacterManager)
				{
					g_pCharacterManager = new CharacterManager;
					gEnv->pCharacterManager = g_pCharacterManager;
				}
				AnimEventLoader::SetPreLoadParticleEffects(false);
				break;
			}
		case ESYSTEM_EVENT_3D_POST_RENDERING_END:
			{
				if (Memory::CPoolFrameLocal* pMemoryPool = CharacterInstanceProcessing::GetMemoryPool())
				{
					pMemoryPool->ReleaseBuckets();
				}

				SAFE_DELETE(g_pCharacterManager);
				gEnv->pCharacterManager = NULL;
				AnimEventLoader::SetPreLoadParticleEffects(true);
				break;
			}
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				if (!g_pCharacterManager)
				{
					g_pCharacterManager = new CharacterManager;
					gEnv->pCharacterManager = g_pCharacterManager;
					gEnv->pCharacterManager->PostInit();
				}
				break;
			}
		}
	}
};
static CSystemEventListener_Animation g_system_event_listener_anim;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAnimation : public IAnimationEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IAnimationEngineModule)
	CRYINTERFACE_END()
	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAnimation, "EngineModule_CryAnimation", "9c73d2cd-142c-4256-a8f0-706d80cd7ad2"_cry_guid)

	virtual ~CEngineModule_CryAnimation()
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(&g_system_event_listener_anim);
		SAFE_RELEASE(gEnv->pCharacterManager);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName()  const override { return "CryAnimation"; };
	virtual const char* GetCategory()  const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;

		if (!CharacterInstanceProcessing::InitializeMemoryPool())
			return false;

		g_pISystem = pSystem;
		g_InitInterfaces();

		if (!g_controllerHeap.IsInitialised())
			g_controllerHeap.Init(Console::GetInst().ca_MemoryDefragPoolSize);

		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_anim, "CSystemEventListener_Animation");

		g_pCharacterManager = NULL;
		env.pCharacterManager = NULL;
		if (gEnv->IsEditor())
		{
			g_pCharacterManager = new CharacterManager;
			gEnv->pCharacterManager = g_pCharacterManager;
		}

		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAnimation)

// cached interfaces - valid during the whole session, when the character manager is alive; then get erased
ISystem* g_pISystem = NULL;
ITimer* g_pITimer = NULL;                     //module implemented in CrySystem
ILog* g_pILog = NULL;                         //module implemented in CrySystem
IConsole* g_pIConsole = NULL;                 //module implemented in CrySystem
ICryPak* g_pIPak = NULL;                      //module implemented in CrySystem
IStreamEngine* g_pIStreamEngine = NULL;       //module implemented in CrySystem

IRenderer* g_pIRenderer = NULL;
IPhysicalWorld* g_pIPhysicalWorld = NULL;
I3DEngine* g_pI3DEngine = NULL;               //Need just for loading of chunks. Should be part of CrySystem

f32 g_AverageFrameTime = 0;
CAnimation g_DefaultAnim;
CharacterManager* g_pCharacterManager;
QuatT g_IdentityQuatT = QuatT(IDENTITY);
int32 g_nRenderThreadUpdateID = 0;
DynArray<string> g_DataMismatch;
SParametricSamplerInternal* g_parametricPool = NULL;
bool* g_usedParametrics = NULL;
int32 g_totalParametrics = 0;
uint32 g_DefaultTransitionInterpolationType = (uint32)CA_Interpolation_Type::QuadraticInOut;
AABB g_IdentityAABB = AABB(ZERO, ZERO);

CControllerDefragHeap g_controllerHeap;

ILINE void g_LogToFile(const char* szFormat, ...)
{
	char szBuffer[2 * 1024];
	va_list args;
	va_start(args, szFormat);
	cry_vsprintf(szBuffer, szFormat, args);
	va_end(args);
	g_pILog->LogToFile("%s", szBuffer);
}

f32 g_fCurrTime = 0;
bool g_bProfilerOn = false;

AnimStatisticsInfo g_AnimStatisticsInfo;

// TypeInfo implementations for CryAnimation
#ifndef _LIB
	#include <CryCore/Common_TypeInfo.h>
	#include <Cry3DEngine/CGF/CGFContent_info.h>
#endif
