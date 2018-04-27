// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/BaseEnv_BaseEnv.h"

#include "BaseEnv/BaseEnv_AutoRegistrar.h"
//#include "BaseEnv/Utils/BaseEnv_EntityClassRegistrar.h"
//#include "BaseEnv/Utils/BaseEnv_EntityMap.h"
#include "BaseEnv/Utils/BaseEnv_SpatialIndex.h"

#include <CrySystem/IConsole.h>

namespace SchematycBaseEnv
{
	namespace UpdateFlags
	{
		const int s_None = 0;
		const int s_StagePrePhysics = eUpdateFlags_StagePrePhysics;
		const int s_StageDefaultAndPost = eUpdateFlags_StageDefaultAndPost;
		const int s_Timers = eUpdateFlags_Timers;
		const int s_SpatialIndex = eUpdateFlags_SpatialIndex;
		const int s_StagePostDebug = eUpdateFlags_StagePostDebug;

		static const int s_GameDefaultUpdate = eUpdateFlags_GameDefaultUpdate;
		static const int s_EditorGameDefaultUpdate = eUpdateFlags_EditorGameDefaultUpdate;

		static const int s_Default = eUpdateFlags_Default;
	}

	CBaseEnv* CBaseEnv::ms_pInstance = nullptr;

	CBaseEnv::CBaseEnv()
		: m_pSpatialIndex(new CSpatialIndex())
		, m_editorGameDefaultUpdateMask(UpdateFlags::s_EditorGameDefaultUpdate)
		, m_gameDefaultUpdateMask(UpdateFlags::s_GameDefaultUpdate)
	{
		// TODO : Shouldn't log recording, loading and compiling be handled automatically by the Schematyc framework?

		CRY_ASSERT(!ms_pInstance);
		ms_pInstance = this;

		if (gEnv->IsEditor())
		{
			gEnv->pSchematyc2->GetLogRecorder().Begin();
		}

		m_gameEntityClassRegistrar.Init();

		gEnv->pSchematyc2->Signals().envRefresh.Connect(Schematyc2::EnvRefreshSignal::Delegate::FromMemberFunction<CBaseEnv, &CBaseEnv::Refresh>(*this), m_connectionScope);
		Refresh(); // TODO : This should be called elsewhere!!!

		gEnv->pSchematyc2->RefreshLogFileSettings();	// TODO: moving this line?

		if (gEnv->IsEditor())
		{
			gEnv->pSchematyc2->GetLogRecorder().End();
		}

		REGISTER_CVAR(sc_Update, UpdateFlags::s_Default, VF_DEV_ONLY | VF_BITFIELD,
			"Selects which update stages are performed by the Schematyc update manager. Possible values:\n"
			"\t0 - all updates disabled;\n"
			"\t1 - default update flags (usually, same as 'qwe' flags in the pure game)\n"
			"Flags:\n"
			"\tq - PrePhysics stage update;\n"
			"\tw - Default and Post stage update\n"
			"\te - schematyc timers update\n"
			"\tr - PostDebug stage update\n"
		);
	}

	CBaseEnv::~CBaseEnv()
	{
		if (IConsole* pConsole = gEnv->pConsole)
		{
			pConsole->UnregisterVariable("sc_Update");
		}
	}

	int CBaseEnv::GetUpdateFlags() const
	{
		IF_LIKELY (sc_Update == UpdateFlags::s_Default)
		{
			return GetDefaultUpdateFlags();
		}
		else
		{
			return sc_Update;
		}
	}

	void CBaseEnv::PrePhysicsUpdate()
	{
		const int updateFlags = GetUpdateFlags();
		if(!gEnv->pGameFramework->IsGamePaused() && !gEnv->IsEditing() && ((updateFlags & UpdateFlags::s_StagePrePhysics) != 0))
		{
			Schematyc2::IUpdateScheduler& updateScheduler = gEnv->pSchematyc2->GetUpdateScheduler();
			updateScheduler.BeginFrame(gEnv->pTimer->GetFrameTime());
			updateScheduler.Update(Schematyc2::EUpdateStage::PrePhysics | Schematyc2::EUpdateDistribution::Earliest, Schematyc2::EUpdateStage::PrePhysics | Schematyc2::EUpdateDistribution::End);
		}
	}

	void CBaseEnv::Update(Schematyc2::CUpdateRelevanceContext* pRelevanceContext)
	{
		if(!gEnv->pGameFramework->IsGamePaused())
		{
			CRY_PROFILE_FUNCTION(PROFILE_GAME);

			Schematyc2::IUpdateScheduler& updateScheduler = gEnv->pSchematyc2->GetUpdateScheduler();
			if(!updateScheduler.InFrame())
			{
				updateScheduler.BeginFrame(gEnv->pTimer->GetFrameTime());
			}

			const int updateFlags = GetUpdateFlags();

			if(gEnv->IsEditing())
			{
				updateScheduler.Update(Schematyc2::EUpdateStage::Editing | Schematyc2::EUpdateDistribution::Earliest, Schematyc2::EUpdateStage::Editing | Schematyc2::EUpdateDistribution::End, pRelevanceContext);
			}
			else
			{
				if ((updateFlags & UpdateFlags::s_Timers) != 0)
				{
					gEnv->pSchematyc2->GetTimerSystem().Update();
				}
				
				if ((updateFlags & UpdateFlags::s_SpatialIndex) != 0)
				{
					m_pSpatialIndex->Update();
				}

				if ((updateFlags & UpdateFlags::s_StageDefaultAndPost) != 0)
				{
					updateScheduler.Update(Schematyc2::EUpdateStage::Default | Schematyc2::EUpdateDistribution::Earliest, Schematyc2::EUpdateStage::Post | Schematyc2::EUpdateDistribution::End, pRelevanceContext);
				}
			}

			if ((updateFlags & UpdateFlags::s_StagePostDebug) != 0)
			{
				updateScheduler.Update(Schematyc2::EUpdateStage::PostDebug | Schematyc2::EUpdateDistribution::Earliest, Schematyc2::EUpdateStage::PostDebug | Schematyc2::EUpdateDistribution::End, pRelevanceContext);
			}

			updateScheduler.EndFrame();

			gEnv->pSchematyc2->GetLog().Update();
		}
	}

	CBaseEnv& CBaseEnv::GetInstance()
	{
		CRY_ASSERT(ms_pInstance);
		return *ms_pInstance;
	}

	CSpatialIndex& CBaseEnv::GetSpatialIndex()
	{
		return *m_pSpatialIndex;
	}

/*
	CEntityClassRegistrar& CBaseEnv::GetGameEntityClassRegistrar()
	{
		return m_gameEntityClassRegistrar;
	}*/

	CEntityMap& CBaseEnv::GetGameEntityMap()
	{
		return m_gameEntityMap;
	}

	void CBaseEnv::UpdateSpatialIndex()
	{
		return m_pSpatialIndex->Update();
	}

	int CBaseEnv::GetDefaultUpdateFlags() const
	{
		return gEnv->IsEditor()
			? m_editorGameDefaultUpdateMask
			: m_gameDefaultUpdateMask;
	}

	void CBaseEnv::Refresh()
	{
		m_gameEntityClassRegistrar.Refresh();
		CAutoRegistrar::Process();
		gEnv->pSchematyc2->GetEnvRegistry().Validate();
	}
}
