// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SensorSystem.h"

#include <Cry3DEngine/I3DEngine.h>

#include "SchematycEntitySensorVolumeComponent.h"
#include "SensorMap.h"
#include "SensorTagLibrary.h"

namespace Cry
{
	namespace SensorSystem
	{
		static int sensor_Debug = 0;
		static float sensor_DebugRange = 100.0f;

		CSensorSystem::CSensorSystem()
		{
			CRY_ASSERT(!ms_pInstance);
			ms_pInstance = this;

			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CSensorSystem");

			m_pTagLibrary.reset(new CSensorTagLibrary());

			// #TODO : Remove/replace hard-coded tags!!!
			m_pTagLibrary->CreateTag("Default");
			m_pTagLibrary->CreateTag("Player");
			m_pTagLibrary->CreateTag("Trigger");
			m_pTagLibrary->CreateTag("Interactive");
			m_pTagLibrary->CreateTag("Flammable");
			m_pTagLibrary->CreateTag("Flame");

			SSensorMapParams sensorMapParams; // #TODO : We should be able to configure these parameters, even if it's just via c_var for now.
			sensorMapParams.volumeCount = 1000;
			sensorMapParams.octreeDepth = 6;
			m_pMap.reset(new CSensorMap(*m_pTagLibrary, sensorMapParams));

			const char* szDebugDescription = "Sensor debug draw configuration:\n"
				"\tm = draw map stats\n"
				"\tv = draw volumes\n"
				"\te = draw volume entities\n"
				"\tt = draw volume tags\n"
				"\to = draw map octree\n"
				"\ts = draw stray volumes\n"
				"\ti = interactive";

			REGISTER_CVAR(sensor_Debug, sensor_Debug, VF_BITFIELD, szDebugDescription);
			REGISTER_CVAR(sensor_DebugRange, sensor_DebugRange, VF_NULL, "Sensor debug range");

			REGISTER_COMMAND("sensor_SetOctreeDepth", CSensorSystem::SetOctreeDepthCommand, 0, "Set depth of sensor map octree");
			REGISTER_COMMAND("sensor_SetOctreeBounds", CSensorSystem::SetOctreeBoundsCommand, 0, "Set bounds of sensor map octree");
		}

		CSensorSystem::~CSensorSystem()
		{
			if (gEnv->pSchematyc != nullptr)
			{
				gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(GetSchematycPackageGUID());
			}

			if (gEnv->pConsole)
			{
				gEnv->pConsole->UnregisterVariable("sensor_Debug");
				gEnv->pConsole->UnregisterVariable("sensor_DebugRange");

				gEnv->pConsole->RemoveCommand("sensor_SetOctreeDepth");
				gEnv->pConsole->RemoveCommand("sensor_SetOctreeBounds");
			}

			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

			CRY_ASSERT(ms_pInstance == this);
			ms_pInstance = nullptr;
		}

		ISensorTagLibrary& CSensorSystem::GetTagLibrary()
		{
			return *m_pTagLibrary;
		}

		ISensorMap& CSensorSystem::GetMap()
		{
			return *m_pMap;
		}

		void CSensorSystem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
		{
			switch (event)
			{
				case ESYSTEM_EVENT_REGISTER_SCHEMATYC_ENV:
				{
					const char* szName = "SensorSystem";
					const char* szDescription = "Sensor system";
					Schematyc::EnvPackageCallback callback = SCHEMATYC_MEMBER_DELEGATE(&CSensorSystem::RegisterSchematycEnvPackage, *this);
					gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(SCHEMATYC_MAKE_ENV_PACKAGE(GetSchematycPackageGUID(), szName, Schematyc::g_szCrytek, szDescription, callback));
					break;
				}
				case ESYSTEM_EVENT_LEVEL_LOAD_END:
				{
					LOADING_TIME_PROFILE_SECTION_NAMED("CSensorSystem::OnSystemEvent() ESYSTEM_EVENT_LEVEL_LOAD_END");
					const float terrainSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSize());
					const AABB worldBounds(Vec3(0.0f, 0.0f, -200.0f), Vec3(terrainSize, terrainSize, 200.0f));
					m_pMap->SetOctreeBounds(worldBounds);
					break;
				}
			}
		}

		void CSensorSystem::Update()
		{
			m_pMap->Update();

			SensorMapDebugFlags mapDebugFlags;
			if ((sensor_Debug & AlphaBit('m')) != 0)
			{
				mapDebugFlags.Add(ESensorMapDebugFlags::DrawStats);
			}
			if ((sensor_Debug & AlphaBit('v')) != 0)
			{
				mapDebugFlags.Add(ESensorMapDebugFlags::DrawVolumes);
			}
			if ((sensor_Debug & AlphaBit('e')) != 0)
			{
				mapDebugFlags.Add(ESensorMapDebugFlags::DrawVolumeEntities);
			}
			if ((sensor_Debug & AlphaBit('t')) != 0)
			{
				mapDebugFlags.Add(ESensorMapDebugFlags::DrawVolumeTags);
			}
			if ((sensor_Debug & AlphaBit('o')) != 0)
			{
				mapDebugFlags.Add(ESensorMapDebugFlags::DrawOctree);
			}
			if ((sensor_Debug & AlphaBit('s')) != 0)
			{
				mapDebugFlags.Add(ESensorMapDebugFlags::DrawStrayVolumes);
			}
			if ((sensor_Debug & AlphaBit('i')) != 0)
			{
				mapDebugFlags.Add(ESensorMapDebugFlags::Interactive);
			}

			m_pMap->Debug(sensor_DebugRange, mapDebugFlags);
		}

		CSensorSystem& CSensorSystem::GetInstance()
		{
			CRY_ASSERT(ms_pInstance);
			return *ms_pInstance;
		}

		void CSensorSystem::RegisterSchematycEnvPackage(Schematyc::IEnvRegistrar& registrar)
		{
			CSchematycEntitySensorVolumeComponent::Register(registrar);
		}

		void CSensorSystem::SetOctreeDepthCommand(IConsoleCmdArgs* pArgs)
		{
			if (ms_pInstance)
			{
				if (pArgs->GetArgCount() == 2)
				{
					const uint32 depth = atol(pArgs->GetArg(1));

					ms_pInstance->m_pMap->SetOctreeDepth(depth);
				}
			}
		}

		void CSensorSystem::SetOctreeBoundsCommand(IConsoleCmdArgs* pArgs)
		{
			if (ms_pInstance)
			{
				if (pArgs->GetArgCount() == 7)
				{
					AABB bounds;

					bounds.min.x = static_cast<float>(atof(pArgs->GetArg(1)));
					bounds.min.y = static_cast<float>(atof(pArgs->GetArg(2)));
					bounds.min.z = static_cast<float>(atof(pArgs->GetArg(3)));

					bounds.max.x = static_cast<float>(atof(pArgs->GetArg(4)));
					bounds.max.y = static_cast<float>(atof(pArgs->GetArg(5)));
					bounds.max.z = static_cast<float>(atof(pArgs->GetArg(6)));

					ms_pInstance->m_pMap->SetOctreeBounds(bounds);
				}
			}
		}

		CSensorSystem* CSensorSystem::ms_pInstance = nullptr;
	}
}