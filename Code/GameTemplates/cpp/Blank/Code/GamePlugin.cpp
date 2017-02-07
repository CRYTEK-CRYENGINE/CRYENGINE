#include "StdAfx.h"
#include "GamePlugin.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

IEntityRegistrator *IEntityRegistrator::g_pFirst = nullptr;
IEntityRegistrator *IEntityRegistrator::g_pLast = nullptr;

CGamePlugin::~CGamePlugin()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	IEntityRegistrator* pTemp = IEntityRegistrator::g_pFirst;
	while (pTemp != nullptr)
	{
		pTemp->Unregister();
		pTemp = pTemp->m_pNext;
	}
}

bool CGamePlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);

	return true;
}

void CGamePlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT:
		{
			// Register entities
			IEntityRegistrator* pTemp = IEntityRegistrator::g_pFirst;
			while (pTemp != nullptr)
			{
				pTemp->Register();
				pTemp = pTemp->m_pNext;
			}

			// Don't need to load the map in editor
			if (!gEnv->IsEditor())
			{
				gEnv->pConsole->ExecuteString("map example", false, true);
			}
		}
		break;
	}
}

CRYREGISTER_SINGLETON_CLASS(CGamePlugin)