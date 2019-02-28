// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationSystem.h"
#include "../MNM/TileGenerator.h"
#include "../MNM/NavMesh.h"
#include "DebugDrawContext.h"
#include "MNMPathfinder.h"

#include "Components/Navigation/NavigationComponent.h"

#include <CryThreading/IJobManager_JobDelegator.h>
#include <CryCore/Platform/CryWindows.h>
#include <CryInput/IHardwareMouse.h>

#define MAX_NAME_LENGTH             512
#if defined(SW_NAVMESH_USE_GUID)
	#define BAI_NAVIGATION_GUID_FLAG  (1 << 31)
#else
	#define BAI_NAVIGATION_GUID_FLAG  (1 << 30)
#endif

//#pragma optimize("", off)
//#pragma inline_depth(0)

#if !defined(_RELEASE)
	#define NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE
#endif

#if defined NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE

	#include <CrySystem/IConsole.h>

struct SAgentTypeListAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const
	{
		assert(gAIEnv.pNavigationSystem);
		return static_cast<int>(gAIEnv.pNavigationSystem->GetAgentTypeCount());
	}

	virtual const char* GetValue(int nIndex) const
	{
		NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;
		assert(pNavigationSystem);
		return pNavigationSystem->GetAgentTypeName(pNavigationSystem->GetAgentTypeID(nIndex));
	}
};

SAgentTypeListAutoComplete s_agentTypeListAutoComplete;

#endif

enum { MaxTaskCountPerWorkerThread = 12, };
enum { MaxVolumeDefCopyCount = 8 }; // volume copies for access in other threads

#if NAVIGATION_SYSTEM_PC_ONLY
void GenerateTileJob(MNM::CTileGenerator::Params params, volatile uint16* state, MNM::STile* tile, MNM::CTileGenerator::SMetaData* metaData, uint32* hashValue)
{
	if (*state != NavigationSystem::TileTaskResult::Failed)
	{
		MNM::CTileGenerator generator;
		bool result = generator.Generate(params, *tile, *metaData, hashValue);
		if (result)
			*state = NavigationSystem::TileTaskResult::Completed;
		else if (((params.flags & MNM::CTileGenerator::Params::NoHashTest) == 0) && (*hashValue == params.hashValue))
			*state = NavigationSystem::TileTaskResult::NoChanges;
		else
			*state = NavigationSystem::TileTaskResult::Failed;
	}
}

DECLARE_JOB("NavigationGeneration", NavigationGenerationJob, GenerateTileJob);
#endif

uint32 NameHash(const char* name)
{
	uint32 len = strlen(name);
	uint32 hash = 0;

	for (uint32 i = 0; i < len; ++i)
	{
		hash += (uint8)(CryStringUtils::toLowerAscii(name[i]));
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

bool ShouldBeConsideredByVoxelizer(IPhysicalEntity& physicalEntity, uint32& flags)
{
	if (physicalEntity.GetType() == PE_WHEELEDVEHICLE)
	{
		return false;
	}

	if (IEntity* pEntity = static_cast<IEntity*>(physicalEntity.GetForeignData(PHYS_FOREIGN_ID_ENTITY)))
	{
		if(pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_IGNORED_IN_NAVMESH_GENERATION)
			return false;
	}
	
	bool considerMass = (physicalEntity.GetType() == PE_RIGID);
	if (!considerMass)
	{
		pe_status_pos sp;
		if (physicalEntity.GetStatus(&sp))
		{
			considerMass = (sp.iSimClass == SC_ACTIVE_RIGID) || (sp.iSimClass == SC_SLEEPING_RIGID);
		}
	}

	if (considerMass)
	{
		pe_status_dynamics dyn;
		if (!physicalEntity.GetStatus(&dyn))
		{
			return false;
		}

		if (dyn.mass > 1e-6f)
		{
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

NavigationSystem::NavigationSystem(const char* configName)
	: m_configName(configName)
	, m_updatesManager(this)
	, m_throughput(0.0f)
	, m_cacheHitRate(0.0f)
	, m_free(0)
	, m_state(Idle)
	, m_meshes(256)                 //Same size of meshes, off-mesh and islandConnections elements
	, m_offMeshNavigationManager(256)
	, m_islandConnectionsManager()
	, m_volumes(512)
	, m_markupVolumes(512)
	, m_markupsData(512)
	, m_worldAABB(AABB::RESET)
	, m_volumeDefCopy(MaxVolumeDefCopyCount, VolumeDefCopy())
	, m_listenersList(10)
	, m_users(10)
	, m_configurationVersion(0)
	, m_isNavigationUpdatePaused(false)
	, m_tileGeneratorExtensionsContainer()
{
	SetupTasks();

	m_worldMonitor = WorldMonitor(functor(m_updatesManager, &CMNMUpdatesManager::EntityChanged));

	ReloadConfig();

	m_pEditorBackgroundUpdate = new NavigationSystemBackgroundUpdate(*this);

#ifdef NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE
	gEnv->pConsole->RegisterAutoComplete("ai_debugMNMAgentType", &s_agentTypeListAutoComplete);
#endif
}

NavigationSystem::~NavigationSystem()
{
	StopWorldMonitoring();

	Clear();

	SAFE_DELETE(m_pEditorBackgroundUpdate);

#ifdef NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE
	gEnv->pConsole->UnRegisterAutoComplete("ai_debugMNMAgentType");
#endif
}

NavigationAgentTypeID NavigationSystem::CreateAgentType(const char* name, const CreateAgentTypeParams& params)
{
	assert(name);
	AgentTypes::const_iterator it = m_agentTypes.begin();
	AgentTypes::const_iterator end = m_agentTypes.end();

	for (; it != end; ++it)
	{
		const AgentType& agentType = *it;

		if (!agentType.name.compareNoCase(name))
		{
			AIWarning("Trying to create NavigationSystem AgentType with duplicate name '%s'!", name);
			assert(0);
			return NavigationAgentTypeID();
		}
	}

	m_agentTypes.resize(m_agentTypes.size() + 1);
	AgentType& agentType = m_agentTypes.back();

	agentType.name = name;
	agentType.settings.voxelSize = params.voxelSize;
	agentType.settings.agent.radius = params.radiusVoxelCount;
	agentType.settings.agent.climbableHeight = params.climbableVoxelCount;
	agentType.settings.agent.climbableInclineGradient = params.climbableInclineGradient;
	agentType.settings.agent.climbableStepRatio = params.climbableStepRatio;
	agentType.settings.agent.height = params.heightVoxelCount;
	agentType.settings.agent.maxWaterDepth = params.maxWaterDepthVoxelCount;
	agentType.meshEntityCallback = functor(ShouldBeConsideredByVoxelizer);

	return NavigationAgentTypeID(m_agentTypes.size());
}

NavigationAgentTypeID NavigationSystem::GetAgentTypeID(const char* name) const
{
	assert(name);

	AgentTypes::const_iterator it = m_agentTypes.begin();
	AgentTypes::const_iterator end = m_agentTypes.end();

	for (; it != end; ++it)
	{
		const AgentType& agentType = *it;

		if (!agentType.name.compareNoCase(name))
			return NavigationAgentTypeID((uint32)(std::distance(m_agentTypes.begin(), it) + 1));
	}

	return NavigationAgentTypeID();
}

NavigationAgentTypeID NavigationSystem::GetAgentTypeID(size_t index) const
{
	if (index < m_agentTypes.size())
		return NavigationAgentTypeID(index + 1);

	return NavigationAgentTypeID();
}

const char* NavigationSystem::GetAgentTypeName(NavigationAgentTypeID agentTypeID) const
{
	if (agentTypeID && agentTypeID <= m_agentTypes.size())
		return m_agentTypes[agentTypeID - 1].name.c_str();

	return 0;
}

size_t NavigationSystem::GetAgentTypeCount() const
{
	return m_agentTypes.size();
}

bool NavigationSystem::GetAgentTypeProperties(const NavigationAgentTypeID agentTypeID, AgentType& agentTypeProperties) const
{
	if (agentTypeID && agentTypeID <= m_agentTypes.size())
	{
		agentTypeProperties = m_agentTypes[agentTypeID - 1];
		return true;
	}

	return false;
}

MNM::AreaAnnotation NavigationSystem::GetAreaTypeAnnotation(const NavigationAreaTypeID areaTypeID) const
{
	const MNM::SAreaType* pAreaType = areaTypeID.IsValid() ? m_annotationsLibrary.GetAreaType(areaTypeID) : nullptr;
	if (!pAreaType)
	{
		pAreaType = &m_annotationsLibrary.GetDefaultAreaType();
	}
	
	MNM::AreaAnnotation annotation;
	annotation.SetType(pAreaType->id);
	annotation.SetFlags(pAreaType->defaultFlags);
	return annotation;
}

#ifdef SW_NAVMESH_USE_GUID
NavigationMeshID NavigationSystem::CreateMesh(const char* name, NavigationAgentTypeID agentTypeID,
                                              const CreateMeshParams& params, NavigationMeshGUID guid)
{
	MeshMap::iterator it = m_swMeshes.find(guid);
	if (it != m_swMeshes.end())
	{
		return it->second;
	}
	NavigationMeshID id(++m_nextFreeMeshId);
	id = CreateMesh(name, agentTypeID, params, id, guid);
	m_swMeshes.insert(std::pair<NavigationMeshGUID, NavigationMeshID>(guid, id));
	return id;
}
#else
NavigationMeshID NavigationSystem::CreateMesh(const char* name, NavigationAgentTypeID agentTypeID,
                                              const CreateMeshParams& params)
{
	return CreateMesh(name, agentTypeID, params, NavigationMeshID(0));
}
#endif
#ifdef SW_NAVMESH_USE_GUID
NavigationMeshID NavigationSystem::CreateMesh(const char* name, NavigationAgentTypeID agentTypeID,
                                              const CreateMeshParams& params, NavigationMeshID requestedID, NavigationMeshGUID guid)
#else
NavigationMeshID NavigationSystem::CreateMesh(const char* name, NavigationAgentTypeID agentTypeID,
                                              const CreateMeshParams& params, NavigationMeshID requestedID)
#endif
{
	auto NearestFactor = [](size_t n, size_t f)
	{ 
		while (n % f)
			++f;
		return f;
	};
	
	assert(name && agentTypeID && agentTypeID <= m_agentTypes.size());

	if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
	{
		AgentType& agentType = m_agentTypes[agentTypeID - 1];

		MNM::CNavMesh::SGridParams paramsGrid;
		paramsGrid.tileSize = params.tileSize;
		paramsGrid.voxelSize = agentType.settings.voxelSize;
		paramsGrid.tileCount = params.tileCount;

		paramsGrid.voxelSize.x = NearestFactor(paramsGrid.tileSize.x * 1000, (size_t)(paramsGrid.voxelSize.x * 1000)) * 0.001f;
		paramsGrid.voxelSize.y = NearestFactor(paramsGrid.tileSize.y * 1000, (size_t)(paramsGrid.voxelSize.y * 1000)) * 0.001f;
		paramsGrid.voxelSize.z = NearestFactor(paramsGrid.tileSize.z * 1000, (size_t)(paramsGrid.voxelSize.z * 1000)) * 0.001f;

		if (!paramsGrid.voxelSize.IsEquivalent(agentType.settings.voxelSize, 0.0001f))
		{
			AIWarning("VoxelSize (%.3f, %.3f, %.3f) for agent '%s' adjusted to (%.3f, %.3f, %.3f)"
			          " - needs to be a factor of TileSize (%d, %d, %d)",
			          agentType.settings.voxelSize.x, agentType.settings.voxelSize.y, agentType.settings.voxelSize.z,
			          agentType.name.c_str(),
			          paramsGrid.voxelSize.x, paramsGrid.voxelSize.y, paramsGrid.voxelSize.z,
			          paramsGrid.tileSize.x, paramsGrid.tileSize.y, paramsGrid.tileSize.z);
		}

		NavigationMeshID id = requestedID;
		if (requestedID == NavigationMeshID(0))
			id = NavigationMeshID(m_meshes.insert(NavigationMesh(agentTypeID)));
		else
			m_meshes.insert(requestedID, NavigationMesh(agentTypeID));
		NavigationMesh& mesh = m_meshes[id];
		mesh.navMesh.Init(paramsGrid);
		mesh.name = name;
		mesh.exclusions = agentType.exclusions;
		mesh.markups = agentType.markups;

#ifdef SW_NAVMESH_USE_GUID
		agentType.meshes.push_back(AgentType::MeshInfo(guid, id, NameHash(name)));
#else
		agentType.meshes.push_back(AgentType::MeshInfo(id, NameHash(name)));
#endif

		m_offMeshNavigationManager.OnNavigationMeshCreated(id);

		return id;
	}

	return NavigationMeshID();
}

NavigationMeshID NavigationSystem::CreateMeshForVolumeAndUpdate(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, const NavigationVolumeID volumeID)
{
	if (volumeID && m_volumes.validate(volumeID))
	{
		NavigationMeshID meshID = CreateMesh(name, agentTypeID, params);
		SetMeshBoundaryVolume(meshID, volumeID);

		NavigationBoundingVolume& volume = m_volumes[volumeID];
		m_updatesManager.RequestMeshUpdate(meshID, volume.aabb);

		return meshID;
	}
	return NavigationMeshID();
}

void NavigationSystem::DestroyMesh(NavigationMeshID meshID)
{
	if (meshID && m_meshes.validate(meshID))
	{
		NavigationMesh& mesh = m_meshes[meshID];

		AILogComment("NavigationSystem::DestroyMesh meshID = %u '%s'", (unsigned int)meshID, mesh.name.c_str());

		for (size_t t = 0; t < m_runningTasks.size(); ++t)
		{
			if (m_results[m_runningTasks[t]].meshID == meshID)
				m_results[m_runningTasks[t]].state = TileTaskResult::Failed;
		}

		for (size_t t = 0; t < m_runningTasks.size(); ++t)
		{
			if (m_results[m_runningTasks[t]].meshID == meshID)
				gEnv->pJobManager->WaitForJob(m_results[m_runningTasks[t]].jobState);
		}

		AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
		AgentType::Meshes::iterator it = agentType.meshes.begin();
		AgentType::Meshes::iterator end = agentType.meshes.end();

		for (; it != end; ++it)
		{
			if (it->id == meshID)
			{
				std::swap(*it, agentType.meshes.back());
				agentType.meshes.pop_back();
				break;
			}
		}

		m_updatesManager.OnMeshDestroyed(meshID);

		m_meshes.erase(meshID);

		m_offMeshNavigationManager.OnNavigationMeshDestroyed(meshID);

		ComputeWorldAABB();
	}
}

void NavigationSystem::SetMeshEntityCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshEntityCallback& callback)
{
	if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
	{
		AgentType& agentType = m_agentTypes[agentTypeID - 1];

		agentType.meshEntityCallback = callback;
	}
}

void NavigationSystem::AddMeshChangeCallback(NavigationAgentTypeID agentTypeID,
                                             const NavigationMeshChangeCallback& callback)
{
	if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
	{
		AgentType& agentType = m_agentTypes[agentTypeID - 1];
		agentType.callbacks.AddUnique(callback);
	}
}

void NavigationSystem::RemoveMeshChangeCallback(NavigationAgentTypeID agentTypeID,
                                                const NavigationMeshChangeCallback& callback)
{
	if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
	{
		AgentType& agentType = m_agentTypes[agentTypeID - 1];
		agentType.callbacks.Remove(callback);
	}
}

void NavigationSystem::AddMeshAnnotationChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback)
{
	if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
	{
		AgentType& agentType = m_agentTypes[agentTypeID - 1];
		agentType.annotationCallbacks.AddUnique(callback);
	}
}

void NavigationSystem::RemoveMeshAnnotationChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback)
{
	if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
	{
		AgentType& agentType = m_agentTypes[agentTypeID - 1];
		agentType.annotationCallbacks.Remove(callback);
	}
}

#ifdef SW_NAVMESH_USE_GUID
void NavigationSystem::SetMeshBoundaryVolume(NavigationMeshID meshID, NavigationVolumeID volumeID, NavigationVolumeGUID volumeGUID)
#else
void NavigationSystem::SetMeshBoundaryVolume(NavigationMeshID meshID, NavigationVolumeID volumeID)
#endif
{
	if (meshID && m_meshes.validate(meshID))
	{
		NavigationMesh& mesh = m_meshes[meshID];

		if (mesh.boundary)
		{
			++mesh.version;
		}

		if (!m_volumes.validate(volumeID))
		{
			AIWarning("NavigationSystem::SetMeshBoundaryVolume: setting invalid volumeID %u for a mesh %u '%s'", (unsigned int)volumeID, (unsigned int)meshID, mesh.name.c_str());
			volumeID = NavigationVolumeID();
		}

		AILogComment("NavigationSystem::SetMeshBoundaryVolume: set volumeID %u for a mesh %u '%s'", (unsigned int)volumeID, (unsigned int)meshID, mesh.name.c_str());

		mesh.boundary = volumeID;
#ifdef SW_NAVMESH_USE_GUID
		mesh.boundaryGUID = volumeGUID;
#endif

		ComputeWorldAABB();
	}
}

#ifdef SW_NAVMESH_USE_GUID
NavigationVolumeID NavigationSystem::CreateVolume(Vec3* vertices, size_t vertexCount, float height, NavigationVolumeGUID guid)
{
	VolumeMap::iterator it = m_swVolumes.find(guid);
	if (it != m_swVolumes.end())
	{
		return it->second;
	}
	NavigationVolumeID id(++m_nextFreeVolumeId);
	id = CreateVolume(vertices, vertexCount, height, id);
	m_swVolumes.insert(std::pair<NavigationVolumeGUID, NavigationVolumeID>(guid, id));
	return id;
}
#else
NavigationVolumeID NavigationSystem::CreateVolume(Vec3* vertices, size_t vertexCount, float height)
{
	return CreateVolume(vertices, vertexCount, height, NavigationVolumeID(0));
}
#endif

NavigationVolumeID NavigationSystem::CreateVolume(Vec3* vertices, size_t vertexCount, float height, NavigationVolumeID requestedID)
{
	NavigationVolumeID id = requestedID;
	if (requestedID == NavigationVolumeID(0))
		id = NavigationVolumeID(m_volumes.insert(NavigationBoundingVolume()));
	else
		m_volumes.insert(requestedID, NavigationBoundingVolume());
	assert(id != 0);
	SetVolume(id, vertices, vertexCount, height);

	return id;
}

void NavigationSystem::DestroyVolume(NavigationVolumeID volumeID)
{
	if (volumeID && m_volumes.validate(volumeID))
	{
		NavigationBoundingVolume& volume = m_volumes[volumeID];

		AgentTypes::iterator it = m_agentTypes.begin();
		AgentTypes::iterator end = m_agentTypes.end();

		for (; it != end; ++it)
		{
			AgentType& agentType = *it;

			stl::find_and_erase(agentType.exclusions, volumeID);

			AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
			AgentType::Meshes::const_iterator mend = agentType.meshes.end();

			for (; mit != mend; ++mit)
			{
				const NavigationMeshID meshID = mit->id;
				NavigationMesh& mesh = m_meshes[meshID];

				if (mesh.boundary == volumeID)
				{
					++mesh.version;
					continue;
				}

				if (stl::find_and_erase(mesh.exclusions, volumeID))
				{
					m_updatesManager.RequestMeshUpdate(meshID, volume.aabb);
					++mesh.version;
				}
			}
		}

		if (gEnv->IsEditor())
		{
			m_volumesManager.InvalidateID(volumeID);
		}

		m_volumes.erase(volumeID);
	}
}

void NavigationSystem::SetVolume(NavigationVolumeID volumeID, Vec3* vertices, size_t vertexCount, float height)
{
	assert(vertices);

	if (volumeID && m_volumes.validate(volumeID))
	{
		bool recomputeAABB = false;

		NavigationBoundingVolume newVolume;
		newVolume.Set(vertices, vertexCount, height);

		NavigationBoundingVolume& volume = m_volumes[volumeID];

		if (!volume.GetBoundaryVertices().empty())
		{
			AgentTypes::const_iterator it = m_agentTypes.begin();
			AgentTypes::const_iterator end = m_agentTypes.end();

			for (; it != end; ++it)
			{
				const AgentType& agentType = *it;

				AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
				AgentType::Meshes::const_iterator mend = agentType.meshes.end();

				for (; mit != mend; ++mit)
				{
					const NavigationMeshID meshID = mit->id;
					NavigationMesh& mesh = m_meshes[meshID];

					if (mesh.boundary == volumeID)
					{
						++mesh.version;
						recomputeAABB = true;
						
						m_updatesManager.RequestMeshDifferenceUpdate(meshID, volume, newVolume);
					}

					if (std::find(mesh.exclusions.begin(), mesh.exclusions.end(), volumeID) != mesh.exclusions.end())
					{
						m_updatesManager.RequestMeshUpdate(meshID, volume.aabb);
						m_updatesManager.RequestMeshUpdate(meshID, newVolume.aabb);
						++mesh.version;
					}
				}
			}
		}

		newVolume.Swap(volume);

		// recompute the world bounding box after we have set the volume
		if (recomputeAABB)
			ComputeWorldAABB();
	}
}

bool NavigationSystem::ValidateVolume(NavigationVolumeID volumeID) const
{
	return m_volumes.validate(volumeID);
}

NavigationVolumeID NavigationSystem::GetVolumeID(NavigationMeshID meshID) const
{
	// This function is used to retrieve the correct ID of the volume boundary connected to the mesh.
	// After restoring the navigation data it could be that the cached volume id in the Sandbox SapeObject
	// is different from the actual one connected to the shape.
	if (meshID && m_meshes.validate(meshID))
		return m_meshes[meshID].boundary;
	else
		return NavigationVolumeID();
}

#ifdef SW_NAVMESH_USE_GUID
void NavigationSystem::SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount, NavigationVolumeID volumeID, NavigationVolumeGUID volumeGUID)
#else
void NavigationSystem::SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount, NavigationVolumeID volumeID)
#endif
{
	if (volumeID && m_volumes.validate(volumeID))
	{
		bool recomputeAABB = false;

		NavigationBoundingVolume& volume = m_volumes[volumeID];

		AgentTypes::iterator it = m_agentTypes.begin();
		AgentTypes::iterator end = m_agentTypes.end();

		for (; it != end; ++it)
		{
			AgentType& agentType = *it;

			stl::find_and_erase(agentType.exclusions, volumeID);

			AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
			AgentType::Meshes::const_iterator mend = agentType.meshes.end();

			for (; mit != mend; ++mit)
			{
				const NavigationMeshID meshID = mit->id;
				NavigationMesh& mesh = m_meshes[meshID];

				if (stl::find_and_erase(mesh.exclusions, volumeID))
				{
					m_updatesManager.RequestMeshUpdate(meshID, volume.aabb);

					++mesh.version;
				}
			}
		}

		for (size_t i = 0; i < agentTypeIDCount; ++i)
		{
			if (agentTypeIDs[i] && (agentTypeIDs[i] <= m_agentTypes.size()))
			{
				AgentType& agentType = m_agentTypes[agentTypeIDs[i] - 1];

				agentType.exclusions.push_back(volumeID);

				AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
				AgentType::Meshes::const_iterator mend = agentType.meshes.end();

				for (; mit != mend; ++mit)
				{
					const NavigationMeshID meshID = mit->id;
					NavigationMesh& mesh = m_meshes[meshID];

					mesh.exclusions.push_back(volumeID);
#ifdef SW_NAVMESH_USE_GUID
					mesh.exclusionsGUID.push_back(volumeGUID);
#endif
					++mesh.version;

					if (mesh.boundary != volumeID)
						m_updatesManager.RequestMeshUpdate(meshID, volume.aabb);
					else
					{
						AILogComment("NavigationSystem::SetExclusionVolume: volumeID %u for a mesh %u '%s'", (unsigned int)volumeID, (unsigned int)meshID, mesh.name.c_str());
						mesh.boundary = NavigationVolumeID();
						recomputeAABB = true;
					}
				}
			}
		}

		if (recomputeAABB)
			ComputeWorldAABB();
	}
}

bool NavigationSystem::GrowMarkupsIfNeeded()
{
	if (!GrowIdMapIfNeeded(m_markupVolumes))
	{
		AIWarning("NavigationSystem::CreateMarkupVolume failed. Maximum number of markup volumes reached! %zu", MarkupVolumes::max_capacity());
		return false;
	}

	if (m_markupVolumes.capacity() > m_markupsData.capacity())
	{
		// keep the same capacity for markups data
		m_markupsData.grow(m_markupVolumes.capacity() - m_markupsData.capacity());
	}
	return true;
}

NavigationVolumeID NavigationSystem::CreateMarkupVolume(NavigationVolumeID requestedID)
{
	NavigationVolumeID id = requestedID;
	if (requestedID == NavigationVolumeID(0))
	{
		if (GrowMarkupsIfNeeded())
		{
			id = NavigationVolumeID(m_markupVolumes.insert(MNM::SMarkupVolume()));
		}
		else
		{
			AIWarning("NavigationSystem::CreateMarkupVolume failed. Maximum number of markup volumes reached! %zu", MarkupVolumes::max_capacity());
			id = NavigationVolumeID();
		}
	}
	else
	{
		if (!m_markupVolumes.validate(requestedID))
		{
			if (GrowMarkupsIfNeeded())
			{
				m_markupVolumes.insert(requestedID, MNM::SMarkupVolume());
			}
			else
			{
				AIWarning("NavigationSystem::CreateMarkupVolume failed. Maximum number of markup volumes reached! %zu", MarkupVolumes::max_capacity());
				id = NavigationVolumeID();
			}
		}
	}
	
	CRY_ASSERT(id.IsValid());
	return id;
}

void NavigationSystem::SetMarkupVolume(
	const NavigationAgentTypesMask enabledAgentTypesMask,
	const Vec3* vertices, size_t vertexCount, 
	const NavigationVolumeID volumeID, const MNM::SMarkupVolumeParams& params)
{
	if (!volumeID || !m_markupVolumes.validate(volumeID))
		return;

	MNM::SMarkupVolume newVolume;
	newVolume.Set(vertices, vertexCount, params.height);
	newVolume.areaAnnotation = params.areaAnnotation;
	newVolume.bStoreTriangles = params.bStoreTriangles;
	newVolume.bExpandByAgentRadius = params.bExpandByAgentRadius;

	MNM::SMarkupVolume& volume = m_markupVolumes[volumeID];

	if (volume == newVolume)
		return; // No need to update

	for (AgentType& agentType : m_agentTypes)
	{
		stl::find_and_erase(agentType.markups, volumeID);

		for (const AgentType::MeshInfo& meshInfo : agentType.meshes)
		{
			const NavigationMeshID meshID = meshInfo.id;
			NavigationMesh& mesh = m_meshes[meshID];

			if (stl::find_and_erase(mesh.markups, volumeID))
			{
				m_updatesManager.RequestMeshUpdate(meshID, volume.aabb, false, true);
				++mesh.version;
			}
		}
	}

	if (!newVolume.GetBoundaryVertices().empty())
	{
		size_t agentsCount = m_agentTypes.size();
		for (size_t i = 0; i < agentsCount; ++i)
		{
			if (!(enabledAgentTypesMask & BIT(i)))
				continue;

			AgentType& agentType = m_agentTypes[i];
			agentType.markups.push_back(volumeID);

			for (const AgentType::MeshInfo& meshInfo : agentType.meshes)
			{
				const NavigationMeshID meshID = meshInfo.id;
				NavigationMesh& mesh = m_meshes[meshID];

				if (newVolume.Overlaps(m_volumes[mesh.boundary].aabb))
				{
					mesh.markups.push_back(volumeID);
					++mesh.version;

					m_updatesManager.RequestMeshUpdate(meshID, newVolume.aabb, false, true);
				}
			}
		}
	}
	newVolume.Swap(volume);
}

void NavigationSystem::DestroyMarkupVolume(NavigationVolumeID volumeID)
{
	if (!volumeID || !m_markupVolumes.validate(volumeID))
		return;
	
	MNM::SMarkupVolume& volume = m_markupVolumes[volumeID];

	for (AgentType& agentType : m_agentTypes)
	{
		stl::find_and_erase(agentType.markups, volumeID);

		AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
		AgentType::Meshes::const_iterator mend = agentType.meshes.end();

		for (const AgentType::MeshInfo& meshInfo : agentType.meshes)
		{
			const NavigationMeshID meshID = meshInfo.id;
			NavigationMesh& mesh = m_meshes[meshID];

			if (stl::find_and_erase(mesh.markups, volumeID))
			{
				m_updatesManager.RequestMeshUpdate(meshID, volume.aabb);
				++mesh.version;
			}
		}
	}
	if (m_markupsData.validate(volumeID))
	{
		m_markupsData.erase(volumeID);
	}
	m_markupVolumes.erase(volumeID);
}

void NavigationSystem::SetAnnotationForMarkupTriangles(NavigationVolumeID markupID, const MNM::AreaAnnotation& areaAnotation)
{
	if (!m_markupsData.validate(markupID))
		return;

	m_markupAnnotationChangesToApply[markupID] = areaAnotation;
}

void NavigationSystem::ApplyAnnotationChanges()
{	
	// We are assuming here that every triangle can be owned by at most one markup volume. 
	// Otherwise we would need to use something else then std::vector to store changed triangles.
	std::unordered_map<NavigationMeshID, std::vector<MNM::TriangleID>> changedTrianglesPerNavmeshMap;
	
	for (const auto& markupAnnotationChange : m_markupAnnotationChangesToApply)
	{
		if (!m_markupsData.validate(markupAnnotationChange.first))
			continue;

		const MNM::AreaAnnotation areaAnnotation = markupAnnotationChange.second;
		
		for (MNM::SMarkupVolumeData::MeshTriangles& meshTriangles : m_markupsData[markupAnnotationChange.first].meshTriangles)
		{
			std::vector<MNM::TileID> affectedTiles;
			NavigationMesh& mesh = m_meshes[meshTriangles.meshId];
			mesh.navMesh.SetTrianglesAnnotation(meshTriangles.triangleIds.data(), meshTriangles.triangleIds.size(), areaAnnotation, affectedTiles);

			auto& changedTriangles = changedTrianglesPerNavmeshMap[meshTriangles.meshId];
			changedTriangles.insert(changedTriangles.end(), meshTriangles.triangleIds.begin(), meshTriangles.triangleIds.end());

			AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
			for (MNM::TileID tileId : affectedTiles)
			{
				agentType.annotationCallbacks.CallSafe(mesh.agentTypeID, meshTriangles.meshId, tileId);
			}
		}
	}
	m_markupAnnotationChangesToApply.clear();

	// Update NavMesh islands
	MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
	for (auto it = changedTrianglesPerNavmeshMap.begin(); it != changedTrianglesPerNavmeshMap.end(); ++it)
	{
		const auto& changedTriangles = it->second;
		NavigationMesh& mesh = m_meshes[it->first];

		mesh.navMesh.GetIslands().UpdateIslandsForTriangles(mesh.navMesh, NavigationMeshID(it->first), changedTriangles.data(), changedTriangles.size(), islandConnections);
	}
}

/*
   void NavigationSystem::AddMeshExclusionVolume(NavigationMeshID meshID, NavigationVolumeID volumeID)
   {
   if (meshID && volumeID && m_meshes.validate(meshID) && m_volumes.validate(volumeID))
   {
   NavigationMesh& mesh = m_meshes[meshID];

   if (stl::push_back_unique(mesh.exclusions, volumeID))
   ++mesh.version;
   }
   }

   void NavigationSystem::RemoveMeshExclusionVolume(NavigationMeshID meshID, NavigationVolumeID volumeID)
   {
   if (meshID && volumeID && m_meshes.validate(meshID) && m_volumes.validate(volumeID))
   {
   NavigationMesh& mesh = m_meshes[meshID];

   if (stl::find_and_erase(mesh.exclusions, volumeID))
   ++mesh.version;
   }
   }
 */
NavigationMeshID NavigationSystem::GetMeshID(const char* name, NavigationAgentTypeID agentTypeID) const
{
	assert(name && agentTypeID && (agentTypeID <= m_agentTypes.size()));

	if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
	{
		uint32 nameHash = NameHash(name);

		const AgentType& agentType = m_agentTypes[agentTypeID - 1];
		AgentType::Meshes::const_iterator it = agentType.meshes.begin();
		AgentType::Meshes::const_iterator end = agentType.meshes.end();

		for (; it != end; ++it)
		{
			if (it->name == nameHash)
				return it->id;
		}
	}

	return NavigationMeshID();
}

const char* NavigationSystem::GetMeshName(NavigationMeshID meshID) const
{
	if (meshID && m_meshes.validate(meshID))
	{
		const NavigationMesh& mesh = m_meshes[meshID];

		return mesh.name.c_str();
	}

	return 0;
}

void NavigationSystem::SetMeshName(NavigationMeshID meshID, const char* name)
{
	if (meshID && m_meshes.validate(meshID))
	{
		NavigationMesh& mesh = m_meshes[meshID];

		mesh.name = name;

		AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
		AgentType::Meshes::iterator it = agentType.meshes.begin();
		AgentType::Meshes::iterator end = agentType.meshes.end();

		for (; it != end; ++it)
		{
			if (it->id == meshID)
			{
				it->name = NameHash(name);
				break;
			}
		}
	}
}

void NavigationSystem::ResetAllNavigationSystemUsers()
{
	for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
	{
		notifier->Reset();
	}
}

void NavigationSystem::WaitForAllNavigationSystemUsersCompleteTheirReadingAsynchronousTasks()
{
	for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
	{
		notifier->CompleteRunningTasks();
	}
}

INavigationSystem::WorkingState NavigationSystem::GetState() const
{
	return m_state;
}

void NavigationSystem::UpdateNavigationSystemUsersForSynchronousWritingOperations()
{
	for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
	{
		notifier->UpdateForSynchronousWritingOperations();
	}
}

void NavigationSystem::UpdateNavigationSystemUsersForSynchronousOrAsynchronousReadingOperations()
{
	for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
	{
		notifier->UpdateForSynchronousOrAsynchronousReadingOperation();
	}
}

void NavigationSystem::UpdateInternalNavigationSystemData(const bool blocking)
{
#if NAVIGATION_SYSTEM_PC_ONLY
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CRY_ASSERT_MESSAGE(m_pEditorBackgroundUpdate->IsRunning() == false, "Background update for editor is still running while the application has the focus!!");

	const bool editorBackgroundThreadRunning = m_pEditorBackgroundUpdate->IsRunning();
	if (editorBackgroundThreadRunning)
	{
		AIError("NavigationSystem - Editor background thread is still running, while the main thread is updating, skipping update");
		return;
	}

	ApplyAnnotationChanges();

	m_worldMonitor.FlushPendingAABBChanges();

	// Prevent multiple updates per frame
	static int lastUpdateFrameID = 0;

	const int frameID = gEnv->nMainFrameID;
	const bool doUpdate = (frameID != lastUpdateFrameID) && !(editorBackgroundThreadRunning);
	if (doUpdate)
	{
		lastUpdateFrameID = frameID;

		const float frameTime = gEnv->pTimer->GetFrameTime();

		UpdateMeshes(frameTime, blocking, gAIEnv.CVars.NavigationSystemMT != 0, false);
	}
#endif
}

void NavigationSystem::UpdateInternalSubsystems()
{
	m_offMeshNavigationManager.ProcessQueuedRequests();
}

INavigationSystem::WorkingState NavigationSystem::Update(bool blocking)
{
	// Pre update step. We need to request all our NavigationSystem users
	// to complete all their reading jobs.
	WaitForAllNavigationSystemUsersCompleteTheirReadingAsynchronousTasks();

	// step 1: update all the tasks that may write on the NavigationSystem data
	UpdateInternalNavigationSystemData(blocking);
	UpdateInternalSubsystems();

	// step 2: update all the writing tasks that needs to write some data inside the navigation system
	UpdateNavigationSystemUsersForSynchronousWritingOperations();

	// step 3: update all the tasks that needs to read only the NavigationSystem data
	// Those tasks may or may not be asynchronous tasks and the reading of the navigation data
	// may extend in time after this function is actually executed.
	UpdateNavigationSystemUsersForSynchronousOrAsynchronousReadingOperations();

	return m_state;
}

void NavigationSystem::PauseNavigationUpdate()
{
	m_isNavigationUpdatePaused = true;
	m_pEditorBackgroundUpdate->Pause(true);
}

void NavigationSystem::RestartNavigationUpdate()
{
	m_isNavigationUpdatePaused = false;
	m_pEditorBackgroundUpdate->Pause(false);
}

uint32 NavigationSystem::GetWorkingQueueSize() const 
{ 
	return (uint32)m_updatesManager.GetRequestQueueSize();
}

#if NAVIGATION_SYSTEM_PC_ONLY
void NavigationSystem::UpdateMeshes(const float frameTime, const bool blocking, const bool multiThreaded, const bool bBackground)
{
	m_updatesManager.Update();
	
	if (m_isNavigationUpdatePaused || frameTime == .0f)
		return;

	m_debugDraw.UpdateWorkingProgress(frameTime, m_updatesManager.GetRequestQueueSize());

	if (!m_updatesManager.HasUpdateRequests() && m_runningTasks.empty())
	{
		if (m_state != Idle)
		{
			// We just finished the processing of the tiles, so before being in Idle
			// we need to recompute the Islands detection
			ComputeIslands();
		}
		m_state = Idle;
		return;
	}

	size_t completed = 0;
	size_t cacheHit = 0;

	while (true)
	{
		if (!m_runningTasks.empty())
		{
			CRY_PROFILE_REGION(PROFILE_AI, "Navigation System::UpdateMeshes() - Running Task Processing");

			RunningTasks::iterator it = m_runningTasks.begin();
			RunningTasks::iterator end = m_runningTasks.end();

			for (; it != end; )
			{
				const size_t resultSlot = *it;
				assert(resultSlot < m_results.size());

				TileTaskResult& result = m_results[resultSlot];
				if (result.state == TileTaskResult::Running)
				{
					++it;
					continue;
				}

				CommitTile(result);

				{
					CRY_PROFILE_REGION_WAITING(PROFILE_AI, "Navigation System::UpdateMeshes() - Running Task Processing - WaitForJob");
					gEnv->GetJobManager()->WaitForJob(result.jobState);
				}

				++completed;
				cacheHit += result.state == TileTaskResult::NoChanges;

				MNM::STile().Swap(result.tile);

				VolumeDefCopy& def = m_volumeDefCopy[result.volumeCopy];
				--def.refCount;

				result.state = TileTaskResult::Running;
				result.next = m_free;

				m_free = static_cast<uint16>(resultSlot);

				const bool reachedLastTask = (it == m_runningTasks.end() - 1);

				std::swap(*it, m_runningTasks.back());
				m_runningTasks.pop_back();

				if (reachedLastTask)
					break;  // prevent the invalidated iterator "it" from getting compared in the for-loop

				end = m_runningTasks.end();
			}
		}

		m_throughput = completed / frameTime;
		m_cacheHitRate = cacheHit / frameTime;

		if (!m_updatesManager.HasUpdateRequests() && m_runningTasks.empty())
		{
			if (m_state != Idle)
			{
				// We just finished the processing of the tiles, so before being in Idle
				// we need to recompute the Islands detection
				ComputeIslands();
			}

			m_state = Idle;

			return;
		}

		if (m_updatesManager.HasUpdateRequests())
		{
			m_state = Working;

			CRY_PROFILE_REGION(PROFILE_AI, "Navigation System::UpdateMeshes() - Job Spawning");
			const size_t idealMinimumTaskCount = 2;
			const size_t MaxRunningTaskCount = multiThreaded ? m_maxRunningTaskCount : std::min(m_maxRunningTaskCount, idealMinimumTaskCount);

			while (m_updatesManager.HasUpdateRequests() && (m_runningTasks.size() < MaxRunningTaskCount))
			{
				const CMNMUpdatesManager::TileUpdateRequest& task = m_updatesManager.GetFrontRequest();

				if (task.IsAborted())
				{
					m_updatesManager.PopFrontRequest();
					continue;
				}

				const NavigationMesh& mesh = m_meshes[task.meshID];
				const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();

				m_runningTasks.push_back(m_free);
				CRY_ASSERT_MESSAGE(m_free < m_results.size(), "Index out of array bounds!");
				TileTaskResult& result = m_results[m_free];
				m_free = result.next;

				if (!SpawnJob(result, task.meshID, paramsGrid, task.x, task.y, task.z, multiThreaded, task.CheckFlag(CMNMUpdatesManager::TileUpdateRequest::EFlag::MarkupUpdate)))
				{
					result.state = TileTaskResult::Running;
					result.next = m_free;

					m_free = m_runningTasks.back();
					m_runningTasks.pop_back();
					break;
				}

				m_updatesManager.PopFrontRequest();
			}

			// keep main thread busy too if we're blocking
			if (blocking && m_updatesManager.HasUpdateRequests() && multiThreaded)
			{
				while (m_updatesManager.HasUpdateRequests())
				{
					const CMNMUpdatesManager::TileUpdateRequest& task = m_updatesManager.GetFrontRequest();

					if (task.IsAborted())
					{
						m_updatesManager.PopFrontRequest();
						continue;
					}

					NavigationMesh& mesh = m_meshes[task.meshID];
					const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();

					TileTaskResult result;
					if (!SpawnJob(result, task.meshID, paramsGrid, task.x, task.y, task.z, false, task.CheckFlag(CMNMUpdatesManager::TileUpdateRequest::EFlag::MarkupUpdate)))
						break;

					CommitTile(result);

					m_updatesManager.PopFrontRequest();
					break;
				}
			}
		}

		if (blocking && (m_updatesManager.HasUpdateRequests() || !m_runningTasks.empty()))
			continue;

		m_state = m_runningTasks.empty() ? Idle : Working;

		return;
	}

}

void NavigationSystem::SetupGenerator(
	NavigationMeshID meshID, const MNM::CNavMesh::SGridParams& paramsGrid,
	uint16 x, uint16 y, uint16 z, MNM::CTileGenerator::Params& params,
	const VolumeDefCopy& defCopy, bool bMarkupUpdate)
{
	const NavigationMesh& mesh = m_meshes[meshID];
	const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];

	params.origin = paramsGrid.origin + Vec3i(x * paramsGrid.tileSize.x, y * paramsGrid.tileSize.y, z * paramsGrid.tileSize.z);
	params.voxelSize = paramsGrid.voxelSize;
	params.sizeX = paramsGrid.tileSize.x;
	params.sizeY = paramsGrid.tileSize.y;
	params.sizeZ = paramsGrid.tileSize.z;
	params.boundary = &defCopy.boundary;
	params.exclusions = defCopy.exclusions.data();
	params.exclusionCount = static_cast<uint16>(defCopy.exclusions.size());

	const MNM::SAreaType& defaultAreaType = m_annotationsLibrary.GetDefaultAreaType();
	params.defaultAreaAnotation.SetType(defaultAreaType.id);
	params.defaultAreaAnotation.SetFlags(defaultAreaType.defaultFlags);

	CRY_ASSERT(defCopy.markups.size() == defCopy.markupIds.size());
	params.markups = defCopy.markups.data();
	params.markupIds = defCopy.markupIds.data();
	params.markupsCount = static_cast<uint16>(defCopy.markups.size());

	if (bMarkupUpdate)
	{
		params.flags |= MNM::CTileGenerator::Params::NoHashTest;
	}

	params.agent = agentType.settings.agent;
	params.callback = agentType.meshEntityCallback;

	if (MNM::TileID tileID = mesh.navMesh.GetTileID(x, y, z))
		params.hashValue = mesh.navMesh.GetTile(tileID).GetHashValue();
	else
		params.flags |= MNM::CTileGenerator::Params::NoHashTest;

	params.pTileGeneratorExtensions = &m_tileGeneratorExtensionsContainer;
	params.navAgentTypeId = mesh.agentTypeID;
}

bool NavigationSystem::SpawnJob(TileTaskResult& result, NavigationMeshID meshID, const MNM::CNavMesh::SGridParams& paramsGrid,
                                uint16 x, uint16 y, uint16 z, bool bMt, bool bMarkupUpdate)
{
	result.x = x;
	result.y = y;
	result.z = z;

	result.meshID = meshID;
	result.state = TileTaskResult::Running;
	result.hashValue = 0;

	const NavigationMesh& mesh = m_meshes[meshID];

	size_t firstFree = ~0ul;
	size_t index = ~0ul;

	for (size_t i = 0; i < MaxVolumeDefCopyCount; ++i)
	{
		VolumeDefCopy& def = m_volumeDefCopy[i];
		if ((def.meshID == meshID) && (def.version == mesh.version))
		{
			index = i;
			break;
		}
		else if ((firstFree == ~0ul) && !def.refCount)
			firstFree = i;
	}

	if ((index == ~0ul) && (firstFree == ~0ul))
		return false;

	VolumeDefCopy* def = 0;

	if (index != ~0ul)
		def = &m_volumeDefCopy[index];
	else
	{
		index = firstFree;

		if (!m_volumes.validate(mesh.boundary))
		{
			AIWarning("NavigationSystem::SpawnJob: Detected non-valid mesh boundary volume (%u) for mesh %u '%s', skipping", (unsigned int)mesh.boundary, (unsigned int)meshID, mesh.name.c_str());
			return false;
		}

		def = &m_volumeDefCopy[index];
		def->meshID = meshID;
		def->version = mesh.version;
		def->boundary = m_volumes[mesh.boundary];
		def->exclusions.clear();
		def->exclusions.reserve(mesh.exclusions.size());

		NavigationMesh::ExclusionVolumes::const_iterator eit = mesh.exclusions.begin();
		NavigationMesh::ExclusionVolumes::const_iterator eend = mesh.exclusions.end();

		for (; eit != eend; ++eit)
		{
			const NavigationVolumeID exclusionVolumeID = *eit;
			assert(m_volumes.validate(exclusionVolumeID));

			if (m_volumes.validate(exclusionVolumeID))
			{
				def->exclusions.push_back(m_volumes[exclusionVolumeID]);
			}
			else
			{
				CryLogAlways("NavigationSystem::SpawnJob(): Detected non-valid exclusion volume (%d) for mesh '%s', skipping", (uint32)exclusionVolumeID, mesh.name.c_str());
			}
		}

		def->markups.clear();
		def->markupIds.clear();
		def->markups.reserve(mesh.markups.size());
		def->markupIds.reserve(mesh.markups.size());
		for (const NavigationVolumeID& markupVolumeId : mesh.markups)
		{
			CRY_ASSERT(m_markupVolumes.validate(markupVolumeId));

			def->markupIds.push_back(markupVolumeId);
			def->markups.push_back(m_markupVolumes[markupVolumeId]);
		}
	}

	result.volumeCopy = static_cast<uint16>(index);

	++def->refCount;

	MNM::CTileGenerator::Params params;	
	SetupGenerator(meshID, paramsGrid, x, y, z, params, *def, bMarkupUpdate);
	if (bMt)
	{
		NavigationGenerationJob job(params, &result.state, &result.tile, &result.metaData, &result.hashValue);
		job.RegisterJobState(&result.jobState);
		job.SetPriorityLevel(JobManager::eStreamPriority);
		job.Run();
	}
	else
	{
		GenerateTileJob(params, &result.state, &result.tile, &result.metaData, &result.hashValue);
	}

	if (gAIEnv.CVars.DebugDrawNavigation)
	{
		if (gEnv->pRenderer)
		{
			if (IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom())
			{
				pRenderAuxGeom->DrawAABB(AABB(params.origin, params.origin + Vec3((float)params.sizeX, (float)params.sizeY, (float)params.sizeZ)),
				                         IDENTITY, false, Col_Red, eBBD_Faceted);
			}
		}
	}

	return true;
}

void NavigationSystem::CommitMarkupData(const TileTaskResult& result, const MNM::TileID tileId)
{
	CRY_ASSERT(m_markupsData.capacity() == m_markupVolumes.capacity());
	
	const VolumeDefCopy& def = m_volumeDefCopy[result.volumeCopy];
	const NavigationMesh& mesh = m_meshes[result.meshID];

	MNM::SMarkupVolumeData::MeshTriangles* pMeshMarkupTriangles = nullptr;
	
	// Remove old triangle ids for the tile first
	for (NavigationVolumeID markupID : def.markupIds)
	{
		if (m_markupsData.validate(markupID))
		{
			// Markup volume should be valid too at this point
			CRY_ASSERT(m_markupVolumes.validate(markupID));

			MNM::SMarkupVolumeData& markupData = m_markupsData[markupID];

			for (MNM::SMarkupVolumeData::MeshTriangles& meshMarkupTriangles : markupData.meshTriangles)
			{
				if (meshMarkupTriangles.meshId == result.meshID)
				{
					auto endIt = meshMarkupTriangles.triangleIds.end();
					for (auto it = meshMarkupTriangles.triangleIds.begin(); it != endIt;)
					{
						if (MNM::ComputeTileID(*it) == tileId)
						{
							endIt = endIt - 1;
							std::iter_swap(it, endIt);
							continue;
						}
						++it;
					}
					if (endIt != meshMarkupTriangles.triangleIds.end())
					{
						meshMarkupTriangles.triangleIds.erase(endIt, meshMarkupTriangles.triangleIds.end());
					}
					break;
				}
			}
		}
	}

	for (const MNM::CTileGenerator::SMetaData::SMarkupTriangles& markupTriangles : result.metaData.markupTriangles)
	{
		NavigationVolumeID markupID = def.markupIds[markupTriangles.markupIdx];

		if(!m_markupVolumes.validate(markupID))
			continue; // Markup volume was removed before tile generation finished

		if (!m_markupsData.validate(markupID))
		{
			m_markupsData.insert(markupID, MNM::SMarkupVolumeData());
		}

		MNM::SMarkupVolumeData::MeshTriangles* pMeshMarkupTriangles = nullptr;
		MNM::SMarkupVolumeData& markupData = m_markupsData[markupID];
		for (MNM::SMarkupVolumeData::MeshTriangles& meshMarkupTriangles : markupData.meshTriangles)
		{
			if (meshMarkupTriangles.meshId == result.meshID)
			{
				pMeshMarkupTriangles = &meshMarkupTriangles;
				break;
			}
		}
		if(!pMeshMarkupTriangles)
		{
			markupData.meshTriangles.emplace_back(MNM::SMarkupVolumeData::MeshTriangles(result.meshID));
			pMeshMarkupTriangles = &markupData.meshTriangles.back();
		}

		// Add new triangle ids
		for (uint16 triangleIdx : markupTriangles.trianglesIdx)
		{
			pMeshMarkupTriangles->triangleIds.push_back(MNM::ComputeTriangleID(tileId, triangleIdx));
		}
	}
}

void NavigationSystem::CommitTile(TileTaskResult& result)
{
	// The mesh for this tile has been destroyed, it doesn't make sense to commit the tile
	const bool nonValidTile = (m_meshes.validate(result.meshID) == false);
	if (nonValidTile)
	{
		return;
	}

	NavigationMesh& mesh = m_meshes[result.meshID];

	switch (result.state)
	{
	case TileTaskResult::Completed:
		{
			CRY_PROFILE_REGION(PROFILE_AI, "Navigation System::CommitTile() - Running Task Processing - ConnectToNetwork");

			MNM::TileID tileID = mesh.navMesh.SetTile(result.x, result.y, result.z, result.tile);
			mesh.navMesh.ConnectToNetwork(tileID);

			CommitMarkupData(result, tileID);

			m_offMeshNavigationManager.RefreshConnections(result.meshID, tileID);
			gAIEnv.pMNMPathfinder->OnNavigationMeshChanged(result.meshID, tileID);

			AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
			agentType.callbacks.CallSafe(mesh.agentTypeID, result.meshID, tileID);
		}
		break;
	case TileTaskResult::Failed:
		{
			CRY_PROFILE_REGION(PROFILE_AI, "Navigation System::CommitTile() - Running Task Processing - ClearTile");

			if (MNM::TileID tileID = mesh.navMesh.GetTileID(result.x, result.y, result.z))
			{
				mesh.navMesh.ClearTile(tileID);

				m_offMeshNavigationManager.RefreshConnections(result.meshID, tileID);
				gAIEnv.pMNMPathfinder->OnNavigationMeshChanged(result.meshID, tileID);

				AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
				agentType.callbacks.CallSafe(mesh.agentTypeID, result.meshID, tileID);
			}
		}
		break;
	case TileTaskResult::NoChanges:
		break;
	default:
		assert(0);
		break;
	}
}
#endif

void NavigationSystem::ProcessQueuedMeshUpdates()
{
#if NAVIGATION_SYSTEM_PC_ONLY
	do
	{
		UpdateMeshes(0.0333f, false, gAIEnv.CVars.NavigationSystemMT != 0, false);
	}
	while (m_state == INavigationSystem::Working);
#endif
}

size_t NavigationSystem::QueueMeshUpdate(NavigationMeshID meshID, const AABB& aabb)
{
	AIWarning("NavigationSystem::QueueMeshUpdate() is deprecated! INavigationUpdatesManager::RequestMeshUpdate should be used instead");
	m_updatesManager.RequestMeshUpdate(meshID, aabb);
	return 0;
}

void NavigationSystem::StopAllTasks()
{
	for (size_t t = 0; t < m_runningTasks.size(); ++t)
		m_results[m_runningTasks[t]].state = TileTaskResult::Failed;

	for (size_t t = 0; t < m_runningTasks.size(); ++t)
		gEnv->pJobManager->WaitForJob(m_results[m_runningTasks[t]].jobState);

	for (size_t t = 0; t < m_runningTasks.size(); ++t)
		m_results[m_runningTasks[t]].tile.Destroy();

	m_runningTasks.clear();
}

void NavigationSystem::ComputeIslands()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_islandConnectionsManager.Reset();

	AgentTypes::const_iterator it = m_agentTypes.begin();
	AgentTypes::const_iterator end = m_agentTypes.end();

	for (; it != end; ++it)
	{
		const AgentType& agentType = *it;
		AgentType::Meshes::const_iterator itMesh = agentType.meshes.begin();
		AgentType::Meshes::const_iterator endMesh = agentType.meshes.end();
		for (; itMesh != endMesh; ++itMesh)
		{
			if (itMesh->id && m_meshes.validate(itMesh->id))
			{
				NavigationMeshID meshID = itMesh->id;
				MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
				NavigationMesh& mesh = m_meshes[meshID];
				mesh.navMesh.GetIslands().ComputeStaticIslandsAndConnections(mesh.navMesh, meshID, m_offMeshNavigationManager, islandConnections);
			}
		}
	}
}

void NavigationSystem::AddIslandConnectionsBetweenTriangles(const NavigationMeshID& meshID, const MNM::TriangleID startingTriangleID,
                                                            const MNM::TriangleID endingTriangleID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_meshes.validate(meshID))
	{
		NavigationMesh& mesh = m_meshes[meshID];
		MNM::Tile::STriangle startingTriangle, endingTriangle;
		if (mesh.navMesh.GetTriangle(startingTriangleID, startingTriangle))
		{
			if (mesh.navMesh.GetTriangle(endingTriangleID, endingTriangle))
			{
				MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);

				MNM::STile& tile = mesh.navMesh.GetTile(MNM::ComputeTileID(startingTriangleID));
				for (uint16 l = 0; l < startingTriangle.linkCount; ++l)
				{
					const MNM::Tile::SLink& link = tile.GetLinks()[startingTriangle.firstLink + l];
					if (link.side == MNM::Tile::SLink::OffMesh)
					{
#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
						AILogCommentID("<MNM:OffMeshLink>", "NavigationSystem::AddIslandConnectionsBetweenTriangles link from %u to %u (mesh %u)", startingTriangle.islandID, endingTriangle.islandID, meshID);
#endif
						MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);
						MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
						MNM::OffMeshNavigation::QueryLinksResult linksResult = offMeshNavigation.GetLinksForTriangle(startingTriangleID, link.triangle);
						while (MNM::WayTriangleData nextTri = linksResult.GetNextTriangle())
						{
							if (nextTri.triangleID == endingTriangleID)
							{
								const MNM::OffMeshLink* pLink = m_offMeshNavigationManager.GetOffMeshLink(nextTri.offMeshLinkID);
								assert(pLink);
								MNM::IslandConnections::Link islandLink(nextTri.triangleID, nextTri.offMeshLinkID, endingIslandID, endingTriangle.areaAnnotation, pLink->GetEntityIdForOffMeshLink(), 1);
								MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
								islandConnections.SetOneWayOffmeshConnectionBetweenIslands(startingIslandID, islandLink);
							}
						}
					}
				}
			}
		}
	}
}

void NavigationSystem::RemoveAllIslandConnectionsForObject(const NavigationMeshID& meshID, const uint32 objectId)
{
	MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
	islandConnections.RemoveAllIslandConnectionsForObject(meshID, objectId);
}

void NavigationSystem::RemoveIslandsConnectionBetweenTriangles(const NavigationMeshID& meshID, const MNM::TriangleID startingTriangleID,
                                                               const MNM::TriangleID endingTriangleID)
{
	// NOTE pavloi 2016.02.05: be advised, that this function is not use anywhere. It should be called before triangles are unlinked
	// from each other, but currently OffMeshNavigationManager first unlinks triangles and only then unlinks islands.

	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_meshes.validate(meshID))
	{
		NavigationMesh& mesh = m_meshes[meshID];
		MNM::Tile::STriangle startingTriangle, endingTriangle;
		if (mesh.navMesh.GetTriangle(startingTriangleID, startingTriangle))
		{
			if (mesh.navMesh.GetTriangle(endingTriangleID, endingTriangle))
			{
				MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);

				MNM::STile& tile = mesh.navMesh.GetTile(MNM::ComputeTileID(startingTriangleID));
				for (uint16 l = 0; l < startingTriangle.linkCount; ++l)
				{
					const MNM::Tile::SLink& link = tile.GetLinks()[startingTriangle.firstLink + l];
					if (link.side == MNM::Tile::SLink::OffMesh)
					{
#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
						AILogCommentID("<MNM:OffMeshLink>", "NavigationSystem::RemoveIslandsConnectionBetweenTriangles link from %u to %u (mesh %u)", startingTriangle.islandID, endingTriangle.islandID, meshID);
#endif
						MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);
						MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
						MNM::OffMeshNavigation::QueryLinksResult linksResult = offMeshNavigation.GetLinksForTriangle(startingTriangleID, link.triangle);
						while (MNM::WayTriangleData nextTri = linksResult.GetNextTriangle())
						{
							if (nextTri.triangleID == endingTriangleID)
							{
								const MNM::OffMeshLink* pLink = m_offMeshNavigationManager.GetOffMeshLink(nextTri.offMeshLinkID);
								assert(pLink);
								MNM::IslandConnections::Link islandLink(nextTri.triangleID, nextTri.offMeshLinkID, endingIslandID, endingTriangle.areaAnnotation, pLink->GetEntityIdForOffMeshLink(), 1);
								MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
								islandConnections.RemoveOneWayConnectionBetweenIslands(startingIslandID, islandLink);
							}
						}
					}
				}
			}
		}
	}
}

void NavigationSystem::AddOffMeshLinkIslandConnectionsBetweenTriangles(
  const NavigationMeshID& meshID,
  const MNM::TriangleID startingTriangleID,
  const MNM::TriangleID endingTriangleID,
  const MNM::OffMeshLinkID& linkID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
	{
		bool bLinkIsFound = false;
		// Next piece code is an almost exact copy from AddIslandConnectionsBetweenTriangles()
		if (m_meshes.validate(meshID))
		{
			const NavigationMesh& mesh = m_meshes[meshID];
			MNM::Tile::STriangle startingTriangle, endingTriangle;
			if (mesh.navMesh.GetTriangle(startingTriangleID, startingTriangle))
			{
				if (mesh.navMesh.GetTriangle(endingTriangleID, endingTriangle))
				{
					const MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);
					const MNM::STile& tile = mesh.navMesh.GetTile(MNM::ComputeTileID(startingTriangleID));
					for (uint16 l = 0; l < startingTriangle.linkCount && !bLinkIsFound; ++l)
					{
						const MNM::Tile::SLink& link = tile.GetLinks()[startingTriangle.firstLink + l];
						if (link.side == MNM::Tile::SLink::OffMesh)
						{
							const MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);
							const MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
							const MNM::OffMeshNavigation::QueryLinksResult linksResult = offMeshNavigation.GetLinksForTriangle(startingTriangleID, link.triangle);
							while (MNM::WayTriangleData nextTri = linksResult.GetNextTriangle())
							{
								if (nextTri.triangleID == endingTriangleID)
								{
									if (nextTri.offMeshLinkID == linkID)
									{
										if (const MNM::OffMeshLink* pLink = m_offMeshNavigationManager.GetOffMeshLink(nextTri.offMeshLinkID))
										{
											bLinkIsFound = true;
											break;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (!bLinkIsFound)
		{
			// It's expected, that the triangles in tiles are already linked together before this function is called.
			// But we haven't found a link during validation.
			AIErrorID("<MNM:OffMeshLink>", "NavigationSystem::AddOffMeshLinkIslandConnectionsBetweenTriangles is called with wrong input");
		}
	}
#endif // DEBUG_MNM_DATA_CONSISTENCY_ENABLED

	// TODO pavloi 2016.02.05: whole piece is suboptimal - this function is called from m_offMeshNavigationManager already, where
	// it linked triangles and had full info about them. I leave it like this to be consistent with AddIslandConnectionsBetweenTriangles()
	if (m_meshes.validate(meshID))
	{
		NavigationMesh& mesh = m_meshes[meshID];
		MNM::Tile::STriangle startingTriangle, endingTriangle;
		if (mesh.navMesh.GetTriangle(startingTriangleID, startingTriangle))
		{
			if (mesh.navMesh.GetTriangle(endingTriangleID, endingTriangle))
			{
				const MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);
				const MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);

				const MNM::OffMeshLink* pLink = m_offMeshNavigationManager.GetOffMeshLink(linkID);
				if (pLink)
				{
					const MNM::IslandConnections::Link islandLink(endingTriangleID, linkID, endingIslandID, endingTriangle.areaAnnotation, pLink->GetEntityIdForOffMeshLink(), 1);
					MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
					islandConnections.SetOneWayOffmeshConnectionBetweenIslands(startingIslandID, islandLink);
				}
			}
		}
	}
}

void NavigationSystem::RemoveOffMeshLinkIslandsConnectionBetweenTriangles(
  const NavigationMeshID& meshID,
  const MNM::TriangleID startingTriangleID,
  const MNM::TriangleID endingTriangleID,
  const MNM::OffMeshLinkID& linkID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
	{
		bool bLinkIsFound = false;
		// Next piece code is an almost exact copy from RemoveIslandConnectionsBetweenTriangles()
		if (m_meshes.validate(meshID))
		{
			const NavigationMesh& mesh = m_meshes[meshID];
			MNM::Tile::STriangle startingTriangle, endingTriangle;
			if (mesh.navMesh.GetTriangle(startingTriangleID, startingTriangle))
			{
				if (mesh.navMesh.GetTriangle(endingTriangleID, endingTriangle))
				{
					const MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);

					const MNM::STile& tile = mesh.navMesh.GetTile(MNM::ComputeTileID(startingTriangleID));
					for (uint16 l = 0; l < startingTriangle.linkCount && !bLinkIsFound; ++l)
					{
						const MNM::Tile::SLink& link = tile.GetLinks()[startingTriangle.firstLink + l];
						if (link.side == MNM::Tile::SLink::OffMesh)
						{
							const MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);
							const MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
							const MNM::OffMeshNavigation::QueryLinksResult linksResult = offMeshNavigation.GetLinksForTriangle(startingTriangleID, link.triangle);
							while (MNM::WayTriangleData nextTri = linksResult.GetNextTriangle())
							{
								if (nextTri.triangleID == endingTriangleID)
								{
									if (nextTri.offMeshLinkID == linkID)
									{
										if (const MNM::OffMeshLink* pLink = m_offMeshNavigationManager.GetOffMeshLink(nextTri.offMeshLinkID))
										{
											bLinkIsFound = true;
											break;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (bLinkIsFound)
		{
			// It's expected, that the triangles in tiles are already unlinked from each other before this function is called.
			// But validation actually found, that the link is still there.
			AIErrorID("<MNM:OffMeshLink>", "NavigationSystem::RemoveOffMeshLinkIslandsConnectionBetweenTriangles is called with wrong input");
		}
	}
#endif // DEBUG_MNM_DATA_CONSISTENCY_ENABLED

	// TODO pavloi 2016.02.05: whole piece is suboptimal - this function is called from m_offMeshNavigationManager already, where
	// it unlinked triangles and had full info about them. I leave it like this to be consistent with RemoveIslandConnectionsBetweenTriangles()
	if (m_meshes.validate(meshID))
	{
		NavigationMesh& mesh = m_meshes[meshID];
		MNM::Tile::STriangle startingTriangle, endingTriangle;
		if (mesh.navMesh.GetTriangle(startingTriangleID, startingTriangle))
		{
			if (mesh.navMesh.GetTriangle(endingTriangleID, endingTriangle))
			{
				const MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);
				const MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);

				const MNM::OffMeshLink* pLink = m_offMeshNavigationManager.GetOffMeshLink(linkID);
				if (pLink)
				{
					const MNM::IslandConnections::Link islandLink(endingTriangleID, linkID, endingIslandID, endingTriangle.areaAnnotation, pLink->GetEntityIdForOffMeshLink(), 1);
					MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
					islandConnections.RemoveOneWayConnectionBetweenIslands(startingIslandID, islandLink);
				}
			}
		}
	}
}

#if MNM_USE_EXPORT_INFORMATION
void NavigationSystem::ComputeAccessibility(const Vec3& debugLocation, NavigationAgentTypeID agentTypeId /*= NavigationAgentTypeID(0)*/)
{
	NavigationMeshID meshId = GetEnclosingMeshID(agentTypeId, debugLocation);
	if (meshId)
	{
		NavigationMesh& mesh = GetMesh(meshId);
		const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();
		const MNM::OffMeshNavigation& offMeshNavigation = GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshId);

		const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x), MNM::real_t(paramsGrid.origin.y), MNM::real_t(paramsGrid.origin.z));
		const Vec3& voxelSize = mesh.navMesh.GetGridParams().voxelSize;
		const MNM::vector3_t seedLocation(MNM::real_t(debugLocation.x), MNM::real_t(debugLocation.y), MNM::real_t(debugLocation.z));

		const uint16 agentHeightUnits = GetAgentHeightInVoxelUnits(agentTypeId);

		const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentHeightUnits, voxelSize.z);
		const MNM::real_t verticalDownwardRange(verticalRange);

		AgentType agentTypeProperties;
		const bool arePropertiesValid = GetAgentTypeProperties(agentTypeId, agentTypeProperties);
		assert(arePropertiesValid);
		const uint16 minZOffsetMultiplier(2);
		const uint16 zOffsetMultiplier = min(minZOffsetMultiplier, (uint16)agentTypeProperties.settings.agent.height);
		const MNM::real_t verticalUpwardRange = arePropertiesValid ? MNM::real_t(zOffsetMultiplier * agentTypeProperties.settings.voxelSize.z) : MNM::real_t(.2f);

		MNM::TriangleID seedTriangleID = mesh.navMesh.GetTriangleAt(seedLocation - origin, verticalDownwardRange, verticalUpwardRange, nullptr);

		if (seedTriangleID)
		{
			MNM::CNavMesh::AccessibilityRequest inputRequest(seedTriangleID, offMeshNavigation);
			mesh.navMesh.ComputeAccessibility(inputRequest);
		}
	}
}

void NavigationSystem::ClearAllAccessibility(uint8 resetValue)
{
	AgentTypes::const_iterator it = m_agentTypes.begin();
	AgentTypes::const_iterator end = m_agentTypes.end();

	for (; it != end; ++it)
	{
		const AgentType& agentType = *it;
		AgentType::Meshes::const_iterator itMesh = agentType.meshes.begin();
		AgentType::Meshes::const_iterator endMesh = agentType.meshes.end();
		for (; itMesh != endMesh; ++itMesh)
		{
			if (itMesh->id && m_meshes.validate(itMesh->id))
			{
				NavigationMesh& mesh = m_meshes[itMesh->id];
				mesh.navMesh.ResetAccessibility(resetValue);
			}
		}
	}
}
#endif

void NavigationSystem::CalculateAccessibility()
{
#if MNM_USE_EXPORT_INFORMATION
	ClearAllAccessibility(MNM::CNavMesh::eARNotAccessible);

	//TODO: Should be navigation seeds rather registered in navigation system?
	std::vector<std::pair<Vec3, NavigationAgentTypeID>> seeds(10);
	GetAISystem()->GetNavigationSeeds(seeds);

	for (size_t i = 0, count = seeds.size(); i < count; ++i)
	{
		if (seeds[i].second == NavigationAgentTypeID(0))
		{
			for (const AgentType& agentType : m_agentTypes)
			{
				ComputeAccessibility(seeds[i].first, GetAgentTypeID(agentType.name));
			}
		}
		else
		{
			ComputeAccessibility(seeds[i].first, seeds[i].second);
		}
	}
#endif //MNM_USE_EXPORT_INFORMATION
}

bool NavigationSystem::IsInUse() const
{
	return m_meshes.size() != 0;
}

MNM::TileID NavigationSystem::GetTileIdWhereLocationIsAtForMesh(NavigationMeshID meshID, const Vec3& location, const INavMeshQueryFilter* pFilter)
{
	NavigationMesh& mesh = GetMesh(meshID);

	const MNM::real_t range = MNM::real_t(1.0f);
	MNM::TriangleID triangleID = mesh.navMesh.GetTriangleAt(location, range, range, pFilter);

	return MNM::ComputeTileID(triangleID);
}

void NavigationSystem::GetTileBoundsForMesh(NavigationMeshID meshID, MNM::TileID tileID, AABB& bounds) const
{
	const NavigationMesh& mesh = GetMesh(meshID);
	const MNM::vector3_t coords = mesh.navMesh.GetTileContainerCoordinates(tileID);

	const MNM::CNavMesh::SGridParams& params = mesh.navMesh.GetGridParams();

	Vec3 minPos((float)params.tileSize.x * coords.x.as_float(), (float)params.tileSize.y * coords.y.as_float(), (float)params.tileSize.z * coords.z.as_float());
	minPos += params.origin;

	bounds = AABB(minPos, minPos + Vec3((float)params.tileSize.x, (float)params.tileSize.y, (float)params.tileSize.z));
}

NavigationMesh& NavigationSystem::GetMesh(const NavigationMeshID& meshID)
{
	return const_cast<NavigationMesh&>(static_cast<const NavigationSystem*>(this)->GetMesh(meshID));
}

const NavigationMesh& NavigationSystem::GetMesh(const NavigationMeshID& meshID) const
{
	if (meshID && m_meshes.validate(meshID))
		return m_meshes[meshID];

	assert(0);
	static NavigationMesh dummy(NavigationAgentTypeID(0));

	return dummy;
}

NavigationMeshID NavigationSystem::GetEnclosingMeshID(NavigationAgentTypeID agentTypeID, const Vec3& location) const
{
	if (agentTypeID && agentTypeID <= m_agentTypes.size())
	{
		const AgentType& agentType = m_agentTypes[agentTypeID - 1];

		AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
		AgentType::Meshes::const_iterator mend = agentType.meshes.end();

		for (; mit != mend; ++mit)
		{
			const NavigationMeshID meshID = mit->id;
			const NavigationMesh& mesh = m_meshes[meshID];
			const NavigationVolumeID boundaryID = mesh.boundary;

			if (boundaryID && m_volumes[boundaryID].Contains(location))
				return meshID;
		}
	}

	return NavigationMeshID();
}

bool NavigationSystem::IsLocationInMesh(NavigationMeshID meshID, const Vec3& location) const
{
	if (meshID && m_meshes.validate(meshID))
	{
		const NavigationMesh& mesh = m_meshes[meshID];
		const NavigationVolumeID boundaryID = mesh.boundary;

		return (boundaryID && m_volumes[boundaryID].Contains(location));
	}

	return false;
}

MNM::TriangleID NavigationSystem::GetClosestMeshLocation(NavigationMeshID meshID, const Vec3& location, float vrange,
                                                         float hrange, const INavMeshQueryFilter* pFilter, Vec3* meshLocation, float* distance) const
{
	if (meshID && m_meshes.validate(meshID))
	{
		MNM::vector3_t loc(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z));
		const NavigationMesh& mesh = m_meshes[meshID];
		MNM::real_t verticalRange(vrange);
		if (const MNM::TriangleID enclosingTriID = mesh.navMesh.GetTriangleAt(loc, verticalRange, verticalRange, pFilter))
		{
			if (meshLocation)
				*meshLocation = location;

			if (distance)
				*distance = 0.0f;

			return enclosingTriID;
		}
		else
		{
			MNM::real_t distanceFixed;
			MNM::vector3_t closest;

			if (const MNM::TriangleID closestTriID = mesh.navMesh.GetClosestTriangle(loc, MNM::real_t(vrange), MNM::real_t(hrange), pFilter, &distanceFixed, &closest))
			{
				if (meshLocation)
					*meshLocation = closest.GetVec3();

				if (distance)
					*distance = distanceFixed.as_float();

				return closestTriID;
			}
		}
	}

	return MNM::TriangleID(0);
}

bool NavigationSystem::AgentTypeSupportSmartObjectUserClass(NavigationAgentTypeID agentTypeID, const char* smartObjectUserClass) const
{
	if (agentTypeID && agentTypeID <= m_agentTypes.size())
	{
		const AgentType& agentType = m_agentTypes[agentTypeID - 1];
		AgentType::SmartObjectUserClasses::const_iterator cit = agentType.smartObjectUserClasses.begin();
		AgentType::SmartObjectUserClasses::const_iterator end = agentType.smartObjectUserClasses.end();

		for (; cit != end; ++cit)
		{
			if (strcmp((*cit).c_str(), smartObjectUserClass))
			{
				continue;
			}

			return true;
		}
	}

	return false;
}

uint16 NavigationSystem::GetAgentRadiusInVoxelUnits(NavigationAgentTypeID agentTypeID) const
{
	if (agentTypeID && agentTypeID <= m_agentTypes.size())
	{
		return m_agentTypes[agentTypeID - 1].settings.agent.radius;
	}
	return 0;
}

uint16 NavigationSystem::GetAgentHeightInVoxelUnits(NavigationAgentTypeID agentTypeID) const
{
	if (agentTypeID && agentTypeID <= m_agentTypes.size())
	{
		return m_agentTypes[agentTypeID - 1].settings.agent.height;
	}
	return 0;
}

void NavigationSystem::Clear()
{
	StopAllTasks();
	SetupTasks();

	m_updatesManager.Clear();

	AgentTypes::iterator it = m_agentTypes.begin();
	AgentTypes::iterator end = m_agentTypes.end();
	for (; it != end; ++it)
	{
		AgentType& agentType = *it;
		agentType.meshes.clear();
		agentType.exclusions.clear();
		agentType.markups.clear();
	}

	for (uint16 i = 0; i < m_meshes.capacity(); ++i)
	{
		if (!m_meshes.index_free(i))
			DestroyMesh(NavigationMeshID(m_meshes.get_index_id(i)));
	}

	for (uint16 i = 0; i < m_volumes.capacity(); ++i)
	{
		if (!m_volumes.index_free(i))
			DestroyVolume(NavigationVolumeID(m_volumes.get_index_id(i)));
	}

	for (uint16 i = 0; i < m_markupVolumes.capacity(); ++i)
	{
		if (!m_markupVolumes.index_free(i))
			DestroyMarkupVolume(NavigationVolumeID(m_markupVolumes.get_index_id(i)));
	}
	m_markupVolumes.clear();
	m_markupsData.clear();

	m_volumesManager.Clear();

#ifdef SW_NAVMESH_USE_GUID
	m_swMeshes.clear();
	m_swVolumes.clear();

	m_nextFreeMeshId = 0;
	m_nextFreeVolumeId = 0;
#endif

	m_worldAABB = AABB::RESET;

	m_volumeDefCopy.clear();
	m_volumeDefCopy.resize(MaxVolumeDefCopyCount, VolumeDefCopy());

	m_offMeshNavigationManager.Clear();
	m_islandConnectionsManager.Reset();

	ResetAllNavigationSystemUsers();
}

void NavigationSystem::ClearAndNotify()
{
	Clear();
	UpdateAllListener(NavigationCleared);

	//////////////////////////////////////////////////////////////////////////
	//After the 'clear' we need to re-enable and register smart objects again

	m_offMeshNavigationManager.Enable();
	gAIEnv.pSmartObjectManager->SoftReset();
}

bool NavigationSystem::ReloadConfig()
{
	// TODO pavloi 2016.03.09: this function should be refactored.
	// Proper way of doing things should be something like this:
	// 1) load config data into an array of CreateAgentTypeParams, do nothing, if there are errors
	// 2) pass array of CreateAgentTypeParams into different publically available function, which will:
	//    a) notify interested listeners, that we're about to wipe out and reload navigation;
	//    b) clear current data;
	//    c) replace agent settings;
	//    d) kick-off navmesh regeneration (if required/enabled);
	//    e) notify listeners, that new settings are available.
	// This way:
	// 1) loading and application of config will be decoupled from each other, so we will be able to put data in descriptor database (if we want);
	// 2) it will be possible to reliably reload config and provide better editor tools, which react to config changes in runtime;
	// 3) continuous nav mesh update can be taken away from editor's CAIManager to navigation system.

	Clear();

	m_annotationsLibrary.Clear();
	m_agentTypes.clear();

	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(m_configName.c_str());
	if (!rootNode)
	{
		AIWarning("Failed to open XML file '%s'...", m_configName.c_str());

		return false;
	}

	const char* tagName = rootNode->getTag();
	if (stricmp(tagName, "Navigation"))
	{
		AIWarning(
			"Unexpected tag '%s' found at line %d while parsing Navigation XML '%s'...",
			rootNode->getTag(), rootNode->getLine(), m_configName.c_str());
		return false;
	}

	rootNode->getAttr("version", m_configurationVersion);

	//Area Flags
	if (XmlNodeRef childNode = rootNode->findChild("AreaFlags"))
	{
		size_t areaFlagsCount = childNode->getChildCount();
		for (size_t at = 0; at < areaFlagsCount; ++at)
		{
			XmlNodeRef areaFlagNode = childNode->getChild(at);
			uint32 id = 0;
			const char* szFlagName = nullptr;
			if (!areaFlagNode->getAttr("name", &szFlagName))
			{
				AIWarning("Missing 'name' attribute for 'Flag' tag at line %d while parsing NavigationXML '%s'...",
					areaFlagNode->getLine(), m_configName.c_str());
				return false;
			}

			if (!areaFlagNode->getAttr("id", id))
			{
				AIWarning("Missing 'id' attribute for 'Flag' tag at line %d while parsing NavigationXML '%s'...",
					areaFlagNode->getLine(), m_configName.c_str());
				return false;
			}
			m_annotationsLibrary.CreateAreaFlag(id, szFlagName);
		}
	}

	// Custom types from from the config
	if (XmlNodeRef childNode = rootNode->findChild("AreaTypes"))
	{
		ColorB color;
		if (childNode->getAttr("defaultColor", color))
		{
			m_annotationsLibrary.SetDefaultAreaColor(color);
		}
		
		size_t areaTypeCount = childNode->getChildCount();
		for (size_t at = 0; at < areaTypeCount; ++at)
		{
			XmlNodeRef areaTypeNode = childNode->getChild(at);

			const char* szAreaName = nullptr;
			if (!areaTypeNode->getAttr("name", &szAreaName))
			{
				AIWarning("Missing 'name' attribute for 'AreaType' tag at line %d while parsing NavigationXML '%s'...",
					areaTypeNode->getLine(), m_configName.c_str());
				return false;
			}

			uint32 id = 0;
			if (!areaTypeNode->getAttr("id", id))
			{
				AIWarning("Missing 'id' attribute for 'AreaType' tag at line %d while parsing NavigationXML '%s'...",
					areaTypeNode->getLine(), m_configName.c_str());
				return false;
			}

			bool bHasColor = areaTypeNode->getAttr("color", color);

			uint32 areaFlags = 0;
			for (size_t childIdx = 0; childIdx < (size_t)areaTypeNode->getChildCount(); ++childIdx)
			{
				XmlNodeRef agentTypeChildNode = areaTypeNode->getChild(childIdx);

				if (!stricmp(agentTypeChildNode->getTag(), "Flag"))
				{
					const char* szFlagName = agentTypeChildNode->getContent();

					NavigationAreaFlagID flagId = m_annotationsLibrary.GetAreaFlagID(szFlagName);
					if (flagId.IsValid())
					{
						const MNM::SAreaFlag* pFlag = m_annotationsLibrary.GetAreaFlag(flagId);
						areaFlags |= pFlag->value;
					}
					else
					{
						AIWarning("Undefined flag '%s' for area type '%s' tag at line %d while parsing NavigationXML '%s'...",
							szFlagName, szAreaName, agentTypeChildNode->getLine(), m_configName.c_str());
					}
				}
			}
			m_annotationsLibrary.CreateAreaType(id, szAreaName, areaFlags, bHasColor ? &color : nullptr);
		}
	}

	//Agent Types
	if (XmlNodeRef childNode = rootNode->findChild("AgentTypes"))
	{
		size_t agentTypeCount = childNode->getChildCount();

		for (size_t at = 0; at < agentTypeCount; ++at)
		{
			XmlNodeRef agentTypeNode = childNode->getChild(at);

			if (!agentTypeNode->haveAttr("name"))
			{
				AIWarning("Missing 'name' attribute for 'AgentType' tag at line %d while parsing NavigationXML '%s'...",
					agentTypeNode->getLine(), m_configName.c_str());

				return false;
			}

			const char* name = 0;
			INavigationSystem::CreateAgentTypeParams params;

			for (size_t attr = 0; attr < (size_t)agentTypeNode->getNumAttributes(); ++attr)
			{
				const char* attrName = 0;
				const char* attrValue = 0;

				if (!agentTypeNode->getAttributeByIndex(attr, &attrName, &attrValue))
					continue;

				bool valid = false;
				if (!stricmp(attrName, "name"))
				{
					if (attrValue && *attrValue)
					{
						valid = true;
						name = attrValue;
					}
				}
				else if (!stricmp(attrName, "radius"))
				{
					int sradius = 0;
					if (sscanf(attrValue, "%d", &sradius) == 1 && sradius > 0)
					{
						valid = true;
						params.radiusVoxelCount = sradius;
					}
				}
				else if (!stricmp(attrName, "height"))
				{
					int sheight = 0;
					if (sscanf(attrValue, "%d", &sheight) == 1 && sheight > 0)
					{
						valid = true;
						params.heightVoxelCount = sheight;
					}
				}
				else if (!stricmp(attrName, "climbableHeight"))
				{
					int sclimbableheight = 0;
					if (sscanf(attrValue, "%d", &sclimbableheight) == 1 && sclimbableheight >= 0)
					{
						valid = true;
						params.climbableVoxelCount = sclimbableheight;
					}
				}
				else if (!stricmp(attrName, "climbableInclineGradient"))
				{
					float sclimbableinclinegradient = 0.f;
					if (sscanf(attrValue, "%f", &sclimbableinclinegradient) == 1)
					{
						valid = true;
						params.climbableInclineGradient = sclimbableinclinegradient;
					}
				}
				else if (!stricmp(attrName, "climbableStepRatio"))
				{
					float sclimbablestepratio = 0.f;
					if (sscanf(attrValue, "%f", &sclimbablestepratio) == 1)
					{
						valid = true;
						params.climbableStepRatio = sclimbablestepratio;
					}
				}
				else if (!stricmp(attrName, "maxWaterDepth"))
				{
					int smaxwaterdepth = 0;
					if (sscanf(attrValue, "%d", &smaxwaterdepth) == 1 && smaxwaterdepth >= 0)
					{
						valid = true;
						params.maxWaterDepthVoxelCount = smaxwaterdepth;
					}
				}
				else if (!stricmp(attrName, "voxelSize"))
				{
					float x, y, z;
					int c = sscanf(attrValue, "%g,%g,%g", &x, &y, &z);

					valid = (c == 1) || (c == 3);
					if (c == 1)
						params.voxelSize = Vec3(x);
					else if (c == 3)
						params.voxelSize = Vec3(x, y, z);
				}
				else
				{
					AIWarning(
						"Unknown attribute '%s' for '%s' tag found at line %d while parsing Navigation XML '%s'...",
						attrName, agentTypeNode->getTag(), agentTypeNode->getLine(), m_configName.c_str());

					return false;
				}

				if (!valid)
				{
					AIWarning("Invalid '%s' attribute value for '%s' tag at line %d while parsing NavigationXML '%s'...",
						attrName, agentTypeNode->getTag(), agentTypeNode->getLine(), m_configName.c_str());

					return false;
				}
			}

			for (size_t childIdx = 0; childIdx < m_agentTypes.size(); ++childIdx)
			{
				const AgentType& agentType = m_agentTypes[childIdx];

				assert(name);

				if (!stricmp(agentType.name.c_str(), name))
				{
					AIWarning("AgentType '%s' redefinition at line %d while parsing NavigationXML '%s'...",
						name, agentTypeNode->getLine(), m_configName.c_str());

					return false;
				}
			}

			NavigationAgentTypeID agentTypeID = CreateAgentType(name, params);
			if (!agentTypeID)
				return false;

			//////////////////////////////////////////////////////////////////////////
			/// Add supported SO classes for this AgentType/Mesh

			for (size_t childIdx = 0; childIdx < (size_t)agentTypeNode->getChildCount(); ++childIdx)
			{
				XmlNodeRef agentTypeChildNode = agentTypeNode->getChild(childIdx);

				if (!stricmp(agentTypeChildNode->getTag(), "SmartObjectUserClasses"))
				{
					AgentType& agentType = m_agentTypes[agentTypeID - 1];

					size_t soClassesCount = agentTypeChildNode->getChildCount();
					agentType.smartObjectUserClasses.reserve(soClassesCount);

					for (size_t socIdx = 0; socIdx < soClassesCount; ++socIdx)
					{
						XmlNodeRef smartObjectClassNode = agentTypeChildNode->getChild(socIdx);

						if (!stricmp(smartObjectClassNode->getTag(), "class") && smartObjectClassNode->haveAttr("name"))
						{
							stl::push_back_unique(agentType.smartObjectUserClasses, smartObjectClassNode->getAttr("name"));
						}
					}
				}
			}
		}
	}
	return true;
}

void NavigationSystem::ComputeWorldAABB()
{
	AgentTypes::const_iterator it = m_agentTypes.begin();
	AgentTypes::const_iterator end = m_agentTypes.end();

	m_worldAABB = AABB(AABB::RESET);

	for (; it != end; ++it)
	{
		const AgentType& agentType = *it;

		AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
		AgentType::Meshes::const_iterator mend = agentType.meshes.end();

		for (; mit != mend; ++mit)
		{
			const NavigationMeshID meshID = mit->id;
			const NavigationMesh& mesh = m_meshes[meshID];

			if (mesh.boundary)
				m_worldAABB.Add(m_volumes[mesh.boundary].aabb);
		}
	}
}

void NavigationSystem::SetupTasks()
{
	// the number of parallel tasks while allowing maximization of tile throughput
	// might also slow down the main thread due to extra time processing them but also because
	// the connection of each tile to network is done on the main thread
	// NOTE ChrisR: capped to half the amount. The execution time of a tile job desperately needs to optimized,
	// it cannot not take more than the frame time because it'll stall the system. No clever priority scheme
	// will ever,ever,ever make that efficient.
	// PeteB: Optimized the tile job time enough to make it viable to do more than one per frame. Added CVar
	//        multiplier to allow people to control it based on the speed of their machine.
	m_maxRunningTaskCount = (gEnv->pJobManager->GetNumWorkerThreads() * 3 / 4) * gAIEnv.CVars.NavGenThreadJobs;
	m_results.resize(m_maxRunningTaskCount);

	for (uint16 i = 0; i < m_results.size(); ++i)
		m_results[i].next = i + 1;

	m_runningTasks.reserve(m_maxRunningTaskCount);

	m_free = 0;
}

bool NavigationSystem::RegisterArea(const char* shapeName, NavigationVolumeID& outVolumeId)
{
	return m_volumesManager.RegisterArea(shapeName, outVolumeId);
}

void NavigationSystem::UnRegisterArea(const char* shapeName)
{
	m_volumesManager.UnRegisterArea(shapeName);
}

NavigationVolumeID NavigationSystem::GetAreaId(const char* shapeName) const
{
	return m_volumesManager.GetAreaID(shapeName);
}

void NavigationSystem::SetAreaId(const char* shapeName, NavigationVolumeID id)
{
	m_volumesManager.SetAreaID(shapeName, id);
}

void NavigationSystem::UpdateAreaNameForId(const NavigationVolumeID id, const char* newShapeName)
{
	m_volumesManager.UpdateNameForAreaID(id, newShapeName);
}

void NavigationSystem::RemoveLoadedMeshesWithoutRegisteredAreas()
{
	std::vector<NavigationVolumeID> volumesToRemove;
	m_volumesManager.GetLoadedUnregisteredVolumes(volumesToRemove);

	if (!volumesToRemove.empty())
	{
		std::vector<NavigationMeshID> meshesToRemove;

		for (const NavigationVolumeID& volumeId : volumesToRemove)
		{
			AILogComment("Removing Navigation Volume id = %u because it was created by loaded unregistered navigation area.",
			             (uint32)volumeId);

			for (const AgentType& agentType : m_agentTypes)
			{
				for (const AgentType::MeshInfo& meshInfo : agentType.meshes)
				{
					const NavigationMeshID meshID = meshInfo.id;
					const NavigationMesh& mesh = m_meshes[meshID];

					if (mesh.boundary == volumeId)
					{
						meshesToRemove.push_back(meshID);
						AILogComment("Removing NavMesh '%s' (meshId = %u, agent = %s) because it uses loaded unregistered navigation area id = %u.",
						             mesh.name.c_str(), (uint32)meshID, agentType.name.c_str(), (uint32)volumeId);
					}
				}
			}
		}

		for (const NavigationMeshID& meshId : meshesToRemove)
		{
			DestroyMesh(meshId);
		}

		for (const NavigationVolumeID& volumeId : volumesToRemove)
		{
			DestroyVolume(volumeId);
		}
	}

	m_volumesManager.ClearLoadedAreas();
}

bool NavigationSystem::RegisterEntityMarkups(const IEntity& owningEntity, const char** shapeNamesArray, const size_t count, NavigationVolumeID* pOutIdsArray)
{
	return m_volumesManager.RegisterEntityMarkups(*this, owningEntity, shapeNamesArray, count, pOutIdsArray);
}

void NavigationSystem::UnregisterEntityMarkups(const IEntity& owningEntity)
{
	m_volumesManager.UnregisterEntityMarkups(*this, owningEntity);
}

void NavigationSystem::StartWorldMonitoring()
{
	m_worldMonitor.Start();
}

void NavigationSystem::StopWorldMonitoring()
{
	m_worldMonitor.Stop();
}

bool NavigationSystem::GetClosestPointInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float vrange, float hrange, Vec3* meshLocation, const INavMeshQueryFilter* pFilter, float minIslandArea) const
{
	const NavigationMeshID meshID = GetEnclosingMeshID(agentID, location);
	if (meshID && m_meshes.validate(meshID))
	{
		MNM::vector3_t loc(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z));
		const NavigationMesh& mesh = m_meshes[meshID];
		MNM::real_t verticalRange(vrange);

		//first check vertical range, because if we are over navmesh, we want that one
		if (const MNM::TriangleID enclosingTriID = mesh.navMesh.GetTriangleAt(loc, verticalRange, verticalRange, pFilter, minIslandArea))
		{
			MNM::vector3_t v0, v1, v2;
			mesh.navMesh.GetVertices(enclosingTriID, v0, v1, v2);
			MNM::vector3_t closest = ClosestPtPointTriangle(loc, v0, v1, v2);

			if (meshLocation)
			{
				*meshLocation = closest.GetVec3();
			}
			return true;
		}
		else
		{
			MNM::vector3_t closest;

			if (const MNM::TriangleID closestTriID = mesh.navMesh.GetClosestTriangle(loc, MNM::real_t(vrange), MNM::real_t(hrange), pFilter, nullptr, &closest, minIslandArea))
			{
				if (meshLocation)
				{
					*meshLocation = closest.GetVec3();
				}
				return true;
			}
		}
	}

	return false;
}

bool NavigationSystem::IsPointReachableFromPosition(const NavigationAgentTypeID agentID, const IEntity* pEntityToTestOffGridLinks, const Vec3& startLocation, const Vec3& endLocation, const INavMeshQueryFilter* pFilter) const
{
	const float horizontalRange = 1.0f;
	const float verticalRange = 1.0f;

	// TODO: should we use filter in GetClosestMeshLocation functions too?

	MNM::GlobalIslandID startingIslandID;
	const NavigationMeshID startingMeshID = GetEnclosingMeshID(agentID, startLocation);
	if (startingMeshID)
	{
		const MNM::TriangleID triangleID = GetClosestMeshLocation(startingMeshID, startLocation, verticalRange, horizontalRange, nullptr, nullptr, nullptr);
		const NavigationMesh& mesh = m_meshes[startingMeshID];
		const MNM::CNavMesh& navMesh = mesh.navMesh;
		MNM::Tile::STriangle triangle;
		if (triangleID && navMesh.GetTriangle(triangleID, triangle) && (triangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID))
		{
			startingIslandID = MNM::GlobalIslandID(startingMeshID, triangle.islandID);
		}
	}

	MNM::GlobalIslandID endingIslandID;
	const NavigationMeshID endingMeshID = GetEnclosingMeshID(agentID, endLocation);
	if (endingMeshID)
	{
		const MNM::TriangleID triangleID = GetClosestMeshLocation(endingMeshID, endLocation, verticalRange, horizontalRange, nullptr, nullptr, nullptr);
		const NavigationMesh& mesh = m_meshes[endingMeshID];
		const MNM::CNavMesh& navMesh = mesh.navMesh;
		MNM::Tile::STriangle triangle;
		if (triangleID && navMesh.GetTriangle(triangleID, triangle) && (triangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID))
		{
			endingIslandID = MNM::GlobalIslandID(endingMeshID, triangle.islandID);
		}
	}

	return m_islandConnectionsManager.AreIslandsConnected(pEntityToTestOffGridLinks, startingIslandID, endingIslandID, pFilter);
}

MNM::GlobalIslandID NavigationSystem::GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location)
{
	const float horizontalRange = 1.0f;
	const float verticalRange = 1.0f;

	// TODO: should we use filter in GetClosestMeshLocation function too?

	MNM::GlobalIslandID startingIslandID;
	const NavigationMeshID startingMeshID = GetEnclosingMeshID(agentID, location);
	if (startingMeshID)
	{
		const MNM::TriangleID triangleID = GetClosestMeshLocation(startingMeshID, location, verticalRange, horizontalRange, nullptr, nullptr, nullptr);
		const NavigationMesh& mesh = m_meshes[startingMeshID];
		const MNM::CNavMesh& navMesh = mesh.navMesh;
		MNM::Tile::STriangle triangle;
		if (triangleID && navMesh.GetTriangle(triangleID, triangle) && (triangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID))
		{
			startingIslandID = MNM::GlobalIslandID(startingMeshID, triangle.islandID);
		}
	}

	return startingIslandID;
}

bool NavigationSystem::IsLocationValidInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter, float downRange, float upRange) const
{
	if (const NavigationMeshID meshID = GetEnclosingMeshID(agentID, location))
	{
		if (m_meshes.validate(meshID))
		{
			MNM::vector3_t loc(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z));
			const NavigationMesh& mesh = m_meshes[meshID];
			const MNM::TriangleID enclosingTriID = mesh.navMesh.GetTriangleAt(loc, MNM::real_t(downRange), MNM::real_t(upRange), pFilter);
			return enclosingTriID != 0;
		}
	}

	return false;
}

MNM::TriangleID NavigationSystem::GetTriangleIDWhereLocationIsAtForMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter)
{
	NavigationMeshID meshId = GetEnclosingMeshID(agentID, location);
	if (meshId)
	{
		NavigationMesh& mesh = GetMesh(meshId);
		const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();
		const MNM::OffMeshNavigation& offMeshNavigation = GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshId);

		const Vec3& voxelSize = mesh.navMesh.GetGridParams().voxelSize;
		const uint16 agentHeightUnits = GetAgentHeightInVoxelUnits(agentID);

		const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentHeightUnits, voxelSize.z);
		const MNM::real_t verticalDownwardRange(verticalRange);

		AgentType agentTypeProperties;
		const bool arePropertiesValid = GetAgentTypeProperties(agentID, agentTypeProperties);
		assert(arePropertiesValid);
		const uint16 minZOffsetMultiplier(2);
		const uint16 zOffsetMultiplier = min(minZOffsetMultiplier, (uint16)agentTypeProperties.settings.agent.height);
		const MNM::real_t verticalUpwardRange = arePropertiesValid ? MNM::real_t(zOffsetMultiplier * agentTypeProperties.settings.voxelSize.z) : MNM::real_t(.2f);

		return mesh.navMesh.GetTriangleAt(location - paramsGrid.origin, verticalDownwardRange, verticalUpwardRange, pFilter);
	}

	return MNM::TriangleID(0);
}

bool NavigationSystem::SnapToNavMesh(const NavigationAgentTypeID agentID, const Vec3& position, const INavMeshQueryFilter* pFilter, const SSnapToNavMeshRulesInfo& snappingRules, Vec3& snappedPosition, MNM::TriangleID* pTriangleId) const
{
	//TODO: don't we need to expand NavMeshes' bboxes when searching for nearest NavMesh? (input position may be outside of mesh volume)
	NavigationMeshID meshId = GetEnclosingMeshID(agentID, position);
	if (!meshId)
		return false;

	const NavigationMesh& mesh = GetMesh(meshId);
	const MNM::CNavMesh& navMesh = mesh.navMesh;
	const MNM::CNavMesh::SGridParams& paramsGrid = navMesh.GetGridParams();

	const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x), MNM::real_t(paramsGrid.origin.y), MNM::real_t(paramsGrid.origin.z));

	AgentType agentTypeProperties;
	const bool arePropertiesValid = GetAgentTypeProperties(agentID, agentTypeProperties);
	CRY_ASSERT(arePropertiesValid);

	const MNM::real_t horizontalRange = MNMUtils::CalculateMinHorizontalRange(agentTypeProperties.settings.agent.radius, paramsGrid.voxelSize.x);
	const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentTypeProperties.settings.agent.height, paramsGrid.voxelSize.z);

	MNM::vector3_t navMeshPos;
	MNM::aabb_t aroundPositionAABB(MNM::vector3_t(-horizontalRange, -horizontalRange, -verticalRange), MNM::vector3_t(horizontalRange, horizontalRange, verticalRange));
	if (navMesh.SnapPosition(MNM::vector3_t(position) - origin, aroundPositionAABB, snappingRules, pFilter, navMeshPos, pTriangleId))
	{
		snappedPosition = navMeshPos.GetVec3();
		return true;
	}
	return false;
}

bool NavigationSystem::NavMeshTestRaycastHit(NavigationAgentTypeID agentTypeID, const Vec3& startPos, const Vec3& toPos, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const
{
	NavigationMeshID meshId = GetEnclosingMeshID(agentTypeID, startPos);
	if (!meshId)
		return false;

	const NavigationMesh& mesh = GetMesh(meshId);
	const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();
	const MNM::OffMeshNavigation& offMeshNavigation = GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshId);

	const Vec3& voxelSize = mesh.navMesh.GetGridParams().voxelSize;
	const uint16 agentHeightUnits = GetAgentHeightInVoxelUnits(agentTypeID);

	const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentHeightUnits, voxelSize.z);
	const MNM::real_t verticalDownwardRange(verticalRange);

	AgentType agentTypeProperties;
	const bool arePropertiesValid = GetAgentTypeProperties(agentTypeID, agentTypeProperties);
	assert(arePropertiesValid);
	const uint16 minZOffsetMultiplier(2);
	const uint16 zOffsetMultiplier = min(minZOffsetMultiplier, (uint16)agentTypeProperties.settings.agent.height);
	const MNM::real_t verticalUpwardRange = arePropertiesValid ? MNM::real_t(zOffsetMultiplier * agentTypeProperties.settings.voxelSize.z) : MNM::real_t(.2f);

	MNM::TriangleID startTriangle = mesh.navMesh.GetTriangleAt(startPos - paramsGrid.origin, verticalDownwardRange, verticalUpwardRange, pFilter);
	MNM::TriangleID endTriangle = mesh.navMesh.GetTriangleAt(toPos - paramsGrid.origin, verticalDownwardRange, verticalUpwardRange, pFilter);
	
	MNM::CNavMesh::RayCastRequest<512> request;
	MNM::CNavMesh::ERayCastResult result = mesh.navMesh.RayCast(startPos - paramsGrid.origin, startTriangle, toPos - paramsGrid.origin, endTriangle, request, pFilter);
	
	bool isHit = result == MNM::CNavMesh::eRayCastResult_Hit;
	if (isHit && pOutHit)
	{
		const float t = request.hit.distance.as_float();
		
		pOutHit->distance = t;
		pOutHit->position = startPos + (toPos - startPos) * t;

		if (request.hit.triangleID != MNM::Constants::InvalidTriangleID && request.hit.edge != MNM::Constants::InvalidEdgeIndex)
		{
			MNM::vector3_t verts[3];
			mesh.navMesh.GetVertices(request.hit.triangleID, verts);
			const Vec3 edgeDir = (verts[(request.hit.edge + 1) % 3] - verts[request.hit.edge]).GetVec3();
			const Vec3 edgeNormal(-edgeDir.y, edgeDir.x, 0.0f);

			pOutHit->normal2D = edgeNormal.GetNormalized();
		}
		else
		{
			pOutHit->normal2D = (startPos - toPos).GetNormalized();
		}
	}
	return isHit;
}

const MNM::INavMesh* NavigationSystem::GetMNMNavMesh(NavigationMeshID meshID) const
{
	if (m_meshes.validate(meshID))
	{
		const NavigationMesh& mesh = m_meshes[meshID];
		return &mesh.navMesh;
	}
	return nullptr;
}

size_t NavigationSystem::GetTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const Vec3& location, const AABB& searchAABB, Vec3* centerLocations, size_t maxCenterLocationCount, const INavMeshQueryFilter* pFilter, float minIslandArea) const
{
	if (m_meshes.validate(meshID))
	{
		const MNM::vector3_t min(MNM::real_t(searchAABB.min.x), MNM::real_t(searchAABB.min.y), MNM::real_t(searchAABB.min.z));
		const MNM::vector3_t max(MNM::real_t(searchAABB.max.x), MNM::real_t(searchAABB.max.y), MNM::real_t(searchAABB.max.z));
		const NavigationMesh& mesh = m_meshes[meshID];
		const MNM::aabb_t aabb(min, max);
		const size_t maxTriangleCount = 4096;
		MNM::TriangleID triangleIDs[maxTriangleCount];
		const size_t triangleCount = mesh.navMesh.GetTriangles(aabb, triangleIDs, maxTriangleCount, pFilter, minIslandArea);
		MNM::Tile::STriangle triangle;

		if (triangleCount > 0)
		{
			MNM::vector3_t a, b, c;

			size_t i = 0;
			size_t num_tris = 0;
			for (i = 0; i < triangleCount; ++i)
			{
				mesh.navMesh.GetVertices(triangleIDs[i], a, b, c);
				centerLocations[num_tris] = ((a + b + c) * MNM::real_t(0.33333f)).GetVec3();
				num_tris++;

				if (num_tris == maxCenterLocationCount)
					return num_tris;
			}

			return num_tris;
		}
	}

	return 0;
}

size_t NavigationSystem::GetTriangleBorders(const NavigationMeshID meshID, const AABB& aabb, Vec3* pBorders, size_t maxBorderCount, const INavMeshQueryFilter* pFilter, float minIslandArea /*= 0.0f*/) const
{
	size_t numBorders = 0;
	if (!m_meshes.validate(meshID))
		return numBorders;

	const MNM::vector3_t min(MNM::real_t(aabb.min.x), MNM::real_t(aabb.min.y), MNM::real_t(aabb.min.z));
	const MNM::vector3_t max(MNM::real_t(aabb.max.x), MNM::real_t(aabb.max.y), MNM::real_t(aabb.max.z));
	const NavigationMesh& mesh = m_meshes[meshID];
	const MNM::aabb_t mnmAABB(min, max);

	return mesh.navMesh.GetMeshBorders(mnmAABB, pFilter, pBorders, maxBorderCount, minIslandArea);
}

size_t NavigationSystem::GetTriangleInfo(const NavigationMeshID meshID, const AABB& aabb, Vec3* centerLocations, uint32* islandids, size_t max_count, const INavMeshQueryFilter* pFilter, float minIslandArea) const
{
	if (m_meshes.validate(meshID))
	{
		const MNM::vector3_t min(MNM::real_t(aabb.min.x), MNM::real_t(aabb.min.y), MNM::real_t(aabb.min.z));
		const MNM::vector3_t max(MNM::real_t(aabb.max.x), MNM::real_t(aabb.max.y), MNM::real_t(aabb.max.z));
		const NavigationMesh& mesh = m_meshes[meshID];
		const MNM::aabb_t aabb(min, max);
		const size_t maxTriangleCount = 4096;
		MNM::TriangleID triangleIDs[maxTriangleCount];
		const size_t triangleCount = mesh.navMesh.GetTriangles(aabb, triangleIDs, maxTriangleCount, pFilter, minIslandArea);
		MNM::Tile::STriangle triangle;

		if (triangleCount > 0)
		{
			MNM::vector3_t a, b, c;

			size_t i = 0;
			size_t num_tris = 0;
			for (i = 0; i < triangleCount; ++i)
			{
				mesh.navMesh.GetTriangle(triangleIDs[i], triangle);
				mesh.navMesh.GetVertices(triangleIDs[i], a, b, c);
				centerLocations[num_tris] = ((a + b + c) * MNM::real_t(0.33333f)).GetVec3();
				islandids[num_tris] = triangle.islandID;
				num_tris++;

				if (num_tris == max_count)
					return num_tris;
			}

			return num_tris;
		}
	}

	return 0;
}

// Helper function to read various navigationId types from file without creating intermediate uint32.
template<typename TId>
static void ReadNavigationIdType(CCryFile& file, TId& outId)
{
	static_assert(sizeof(TId) == sizeof(uint32), "Navigation ID underlying type have changed");
	uint32 id;
	file.ReadType(&id);
	outId = TId(id);
}

template<typename TId>
static void WriteNavigationIdType(CCryFile& file, const TId& id)
{
	static_assert(sizeof(TId) == sizeof(uint32), "Navigation ID underlying type have changed");
	const uint32 uid = id;
	file.WriteType<uint32>(&uid);
}

bool NavigationSystem::ReadFromFile(const char* fileName, bool bAfterExporting)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Navigation Meshes (Read File)");

	bool fileLoaded = false;

	m_pEditorBackgroundUpdate->Pause(true);

	m_volumesManager.ClearLoadedAreas();

	CCryFile file;
	if (false != file.Open(fileName, "rb"))
	{
		bool fileVersionCompatible = true;

		uint16 nFileVersion = eBAINavigationFileVersion::CURRENT;
		file.ReadType(&nFileVersion);

		//Verify version of exported file in first place
		if (nFileVersion < eBAINavigationFileVersion::FIRST_COMPATIBLE)
		{
			AIWarning("Wrong BAI file version (found %d expected at least %d)!! Regenerate Navigation data in the editor.",
			          nFileVersion, eBAINavigationFileVersion::FIRST_COMPATIBLE);

			fileVersionCompatible = false;
		}
		else
		{
			uint32 nConfigurationVersion = 0;
			file.ReadType(&nConfigurationVersion);

			if (nConfigurationVersion != m_configurationVersion)
			{
				AIWarning("Navigation.xml config version mismatch (found %d expected %d)!! Regenerate Navigation data in the editor.",
				          nConfigurationVersion, m_configurationVersion);

				// In the launcher we still read the navigation data even if the configuration file
				// contains different version than the exported one
				if (gEnv->IsEditor())
					fileVersionCompatible = false;
			}
#ifdef SW_NAVMESH_USE_GUID
			else
			{
				uint32 useGUID = 0;
				file.ReadType(&useGUID);
				if (useGUID != BAI_NAVIGATION_GUID_FLAG)
				{
					AIWarning("Navigation GUID config mismatch (found %d expected %d)!! Regenerate Navigation data in the editor.",
					          useGUID, BAI_NAVIGATION_GUID_FLAG);

					fileVersionCompatible = false;
				}
			}
#endif
		}

		if (fileVersionCompatible)
		{
			// Loading boundary volumes, their ID's and names
			{
				std::vector<Vec3> volumeVerticesBuffer;
				std::vector<char> volumeAreaNameBuffer;
				string volumeAreaName;

				uint32 usedVolumesCount;
				file.ReadType(&usedVolumesCount);

				for (uint32 idx = 0; idx < usedVolumesCount; ++idx)
				{
					// Read volume data
					NavigationVolumeID volumeId;
					float volumeHeight;
					uint32 verticesCount;
					uint32 volumeAreaNameSize;

					ReadNavigationIdType(file, volumeId);
					file.ReadType(&volumeHeight);
					file.ReadType(&verticesCount);

					volumeVerticesBuffer.resize(verticesCount);
					for (uint32 vtxIdx = 0; vtxIdx < verticesCount; ++vtxIdx)
					{
						Vec3& vtx = volumeVerticesBuffer[vtxIdx];
						file.ReadType(&vtx.x, 3);
					}

					file.ReadType(&volumeAreaNameSize);
					if (volumeAreaNameSize > 0)
					{
						volumeAreaNameBuffer.resize(volumeAreaNameSize, '\0');
						file.ReadType(&volumeAreaNameBuffer.front(), volumeAreaNameSize);

						volumeAreaName.assign(&volumeAreaNameBuffer.front(), (&volumeAreaNameBuffer.back()) + 1);
					}
					else
					{
						volumeAreaName.clear();
					}

					// Create volume

					if (volumeId == NavigationVolumeID())
					{
						AIWarning("NavigationSystem::ReadFromFile: file contains invalid Navigation Volume ID");
						continue;
					}

					if (m_volumes.validate(volumeId))
					{
						AIWarning("NavigationSystem::ReadFromFile: Navigation Volume with volumeId=%u (name '%s') is already registered", (unsigned int)volumeId, volumeAreaName.c_str());
						continue;
					}

					const NavigationVolumeID createdVolumeId = CreateVolume(&volumeVerticesBuffer.front(), verticesCount, volumeHeight, volumeId);
					CRY_ASSERT(volumeId == createdVolumeId);

					m_volumesManager.RegisterAreaFromLoadedData(volumeAreaName.c_str(), volumeId);
				}
			}

			{
				std::vector<Vec3> markupVerticesBuffer;

				uint32 markupsCount;
				uint32 markupsCapacity;
				file.ReadType(&markupsCount);
				file.ReadType(&markupsCapacity);

				if (markupsCapacity > m_markupVolumes.capacity())
				{
					m_markupVolumes.grow(markupsCapacity - m_markupVolumes.capacity());
				}
				if (markupsCapacity > m_markupsData.capacity())
				{
					m_markupsData.grow(markupsCapacity - m_markupsData.capacity());
				}

				for (uint32 idx = 0; idx < markupsCount; ++idx)
				{
					NavigationVolumeID markupId;
					MNM::SMarkupVolumeParams params;
					uint32 verticesCount;
					MNM::AreaAnnotation::value_type areaAnnotation;
					
					ReadNavigationIdType(file, markupId);
					
					file.ReadType(&params.height);
					file.ReadType(&areaAnnotation);
					file.ReadType(&params.bExpandByAgentRadius);
					file.ReadType(&params.bStoreTriangles);
					file.ReadType(&verticesCount);

					params.areaAnnotation = areaAnnotation;

					markupVerticesBuffer.resize(verticesCount);
					for (uint32 vtxIdx = 0; vtxIdx < verticesCount; ++vtxIdx)
					{
						Vec3& vtx = markupVerticesBuffer[vtxIdx];
						file.ReadType(&vtx.x, 3);
					}

					CRY_ASSERT_MESSAGE(markupId != NavigationVolumeID(), "Markup volume with invalid id loaded!");
					CRY_ASSERT_MESSAGE(!m_markupVolumes.validate(markupId), "Markup volume with the same id was already loaded!");

					CreateMarkupVolume(markupId);
					SetMarkupVolume(0, markupVerticesBuffer.data(), verticesCount, markupId, params);
				}
			}

			uint32 agentsCount;
			file.ReadType(&agentsCount);
			for (uint32 i = 0; i < agentsCount; ++i)
			{
				uint32 nameLength;
				file.ReadType(&nameLength);
				char agentName[MAX_NAME_LENGTH];
				nameLength = std::min(nameLength, (uint32)MAX_NAME_LENGTH - 1);
				file.ReadType(agentName, nameLength);
				agentName[nameLength] = '\0';

				// Reading total amount of memory used for the current agent
				uint32 totalAgentMemory = 0;
				file.ReadType(&totalAgentMemory);

				const size_t fileSeekPositionForNextAgent = file.GetPosition() + totalAgentMemory;
				const NavigationAgentTypeID agentTypeID = GetAgentTypeID(agentName);
				if (agentTypeID == 0)
				{
					AIWarning("The agent '%s' doesn't exist between the ones loaded from the Navigation.xml", agentName);
					file.Seek(fileSeekPositionForNextAgent, SEEK_SET);
					continue;
				}

				{
					// Reading markup volumes
					AgentType::MarkupVolumes markups;
					uint32 markupsCount;
					file.ReadType(&markupsCount);
					markups.reserve(markupsCount);
					for (uint32 mIdx = 0; mIdx < markupsCount; ++mIdx)
					{
						NavigationVolumeID markupId;
						ReadNavigationIdType(file, markupId);
						markups.push_back(markupId);
					}
					m_agentTypes[agentTypeID - 1].markups = markups;
				}

				// ---------------------------------------------
				// Reading navmesh for the different agents type

				uint32 meshesCount = 0;
				file.ReadType(&meshesCount);

				for (uint32 meshCounter = 0; meshCounter < meshesCount; ++meshCounter)
				{
					// Reading mesh id
#ifdef SW_NAVMESH_USE_GUID
					NavigationMeshGUID meshGUID = 0;
					file.ReadType(&meshGUID);
#else
					uint32 meshIDuint32 = 0;
					file.ReadType(&meshIDuint32);
#endif

					// Reading mesh name
					uint32 meshNameLength = 0;
					file.ReadType(&meshNameLength);
					char meshName[MAX_NAME_LENGTH];
					meshNameLength = std::min(meshNameLength, (uint32)MAX_NAME_LENGTH - 1);
					file.ReadType(meshName, meshNameLength);
					meshName[meshNameLength] = '\0';

					// Reading the amount of islands in the mesh
					MNM::StaticIslandID totalIslands = 0;
					file.ReadType(&totalIslands);

					// Reading total mesh memory
					uint32 totalMeshMemory = 0;
					file.ReadType(&totalMeshMemory);

					const size_t fileSeekPositionForNextMesh = file.GetPosition() + totalMeshMemory;

					// Reading mesh boundary
#ifdef SW_NAVMESH_USE_GUID
					NavigationVolumeGUID boundaryGUID = 0;
					file.ReadType(&boundaryGUID);
#else
					NavigationVolumeID boundaryID;
					ReadNavigationIdType(file, boundaryID);
#endif

					{
						if (m_volumesManager.GetLoadedAreaID(meshName) != boundaryID)
						{
							AIWarning("The NavMesh '%s' (agent = '%s', meshId = %u, boundaryVolumeId = %u) and the loaded corresponding Navigation Area have different IDs. Data might be corrupted.",
							          meshName, agentName, meshIDuint32, (uint32)boundaryID);
						}

						const NavigationVolumeID existingAreaId = m_volumesManager.GetAreaID(meshName);
						if (existingAreaId != boundaryID)
						{
							if (!m_volumesManager.IsAreaPresent(meshName))
							{
								if (!m_volumesManager.IsLoadedAreaPresent(meshName))
								{
									AIWarning("The NavMesh '%s' (agent = '%s', meshId = %u, boundaryVolumeId = %u) doesn't have a loaded corresponding Navigation Area. Data might be corrupted.",
									          meshName, agentName, meshIDuint32, (uint32)boundaryID);
								}
							}
							else
							{
								if (existingAreaId == NavigationVolumeID() && bAfterExporting)
								{
									// Expected situation
								}
								else
								{
									AIWarning("The NavMesh '%s' (agent = '%s', meshId = %u, boundaryVolumeId = %u) and the existing corresponding Navigation Area have different IDs. Data might be corrupted.",
									          meshName, agentName, meshIDuint32, (uint32)boundaryID);
								}
							}
						}
					}

					// Reading mesh exclusion shapes
					uint32 exclusionShapesCount = 0;
					file.ReadType(&exclusionShapesCount);
					AgentType::ExclusionVolumes exclusions;
#ifdef SW_NAVMESH_USE_GUID
					for (uint32 exclusionsCounter = 0; exclusionsCounter < exclusionShapesCount; ++exclusionsCounter)
					{
						NavigationVolumeGUID exclusionGUID = 0;
						file.ReadType(&exclusionGUID);
						VolumeMap::iterator it = m_swVolumes.find(exclusionGUID);
						if (it != m_swVolumes.end())
							exclusions.push_back(it->second);
					}
#else
					exclusions.reserve(exclusionShapesCount);
					for (uint32 exclusionsCounter = 0; exclusionsCounter < exclusionShapesCount; ++exclusionsCounter)
					{
						NavigationVolumeID exclusionId;
						ReadNavigationIdType(file, exclusionId);
						// Save the exclusion shape with the read ID
						exclusions.push_back(exclusionId);
					}
#endif
					m_agentTypes[agentTypeID - 1].exclusions = exclusions;

					NavigationMesh::Markups markups;
					{
						// Reading markup volumes
						uint32 markupsCount;
						file.ReadType(&markupsCount);
						markups.reserve(markupsCount);
						for (uint32 mIdx = 0; mIdx < markupsCount; ++mIdx)
						{
							NavigationVolumeID markupId;
							ReadNavigationIdType(file, markupId);
							markups.push_back(markupId);
						}
					}

					// Reading tile count
					uint32 tilesCount = 0;
					file.ReadType(&tilesCount);

					// Reading NavMesh grid params
					MNM::CNavMesh::SGridParams params;
					file.ReadType(&(params.origin.x));
					file.ReadType(&(params.origin.y));
					file.ReadType(&(params.origin.z));
					file.ReadType(&(params.tileSize.x));
					file.ReadType(&(params.tileSize.y));
					file.ReadType(&(params.tileSize.z));
					file.ReadType(&(params.voxelSize.x));
					file.ReadType(&(params.voxelSize.y));
					file.ReadType(&(params.voxelSize.z));
					file.ReadType(&(params.tileCount));
					// If we are full reloading the mnm then we also want to create a new grid with the parameters
					// written in the file

					CreateMeshParams createParams;
					createParams.origin = params.origin;
					createParams.tileSize = params.tileSize;
					createParams.tileCount = tilesCount;

#ifdef SW_NAVMESH_USE_GUID
					NavigationMeshID newMeshID = CreateMesh(meshName, agentTypeID, createParams, meshGUID);
#else
					NavigationMeshID newMeshID = NavigationMeshID(meshIDuint32);
					if (!m_meshes.validate(meshIDuint32))
						newMeshID = CreateMesh(meshName, agentTypeID, createParams, newMeshID);
#endif

					if (newMeshID == 0)
					{
						AIWarning("Unable to create mesh '%s'", meshName);
						file.Seek(fileSeekPositionForNextMesh, SEEK_SET);
						continue;
					}

#if !defined(SW_NAVMESH_USE_GUID)
					if (newMeshID != meshIDuint32)
					{
						AIWarning("The restored mesh has a different ID compared to the saved one.");
					}
#endif

					NavigationMesh& mesh = m_meshes[newMeshID];
#ifdef SW_NAVMESH_USE_GUID
					SetMeshBoundaryVolume(newMeshID, boundaryID, boundaryGUID);
#else
					SetMeshBoundaryVolume(newMeshID, boundaryID);
#endif
					mesh.markups = markups;
					mesh.exclusions = exclusions;
					mesh.navMesh.GetIslands().SetTotalIslands(totalIslands);
					for (uint32 j = 0; j < tilesCount; ++j)
					{
						// Reading Tile indexes
						uint16 x, y, z;
						uint32 hashValue;
						file.ReadType(&x);
						file.ReadType(&y);
						file.ReadType(&z);
						file.ReadType(&hashValue);

						// Reading triangles
						uint16 triangleCount = 0;
						file.ReadType(&triangleCount);
						std::unique_ptr<MNM::Tile::STriangle[]> pTriangles;
						if (triangleCount)
						{
							pTriangles.reset(new MNM::Tile::STriangle[triangleCount]);
							file.ReadType(pTriangles.get(), triangleCount);
						}

						// Reading Vertices
						uint16 vertexCount = 0;
						file.ReadType(&vertexCount);
						std::unique_ptr<MNM::Tile::Vertex[]> pVertices;
						if (vertexCount)
						{
							pVertices.reset(new MNM::Tile::Vertex[vertexCount]);
							file.ReadType(pVertices.get(), vertexCount);
						}

						// Reading Links
						uint16 linkCount;
						file.ReadType(&linkCount);
						std::unique_ptr<MNM::Tile::SLink[]> pLinks;
						if (linkCount)
						{
							pLinks.reset(new MNM::Tile::SLink[linkCount]);
							file.ReadType(pLinks.get(), linkCount);
						}

						// Reading nodes
						uint16 nodeCount;
						file.ReadType(&nodeCount);
						std::unique_ptr<MNM::Tile::SBVNode[]> pNodes;
						if (nodeCount)
						{
							pNodes.reset(new MNM::Tile::SBVNode[nodeCount]);
							file.ReadType(pNodes.get(), nodeCount);
						}

						// Creating and swapping the tile
						MNM::STile tile = MNM::STile();
						tile.SetTriangles(std::move(pTriangles), triangleCount);
						tile.SetVertices(std::move(pVertices), vertexCount);
						tile.SetLinks(std::move(pLinks), linkCount);
						tile.SetNodes(std::move(pNodes), nodeCount);
						tile.SetHashValue(hashValue);

						mesh.navMesh.SetTile(x, y, z, tile);
					}
				}
			}

			// Read markups data
			uint32 dataCount;
			file.ReadType(&dataCount);

			CRY_ASSERT(m_markupsData.capacity() == m_markupVolumes.capacity());
			CRY_ASSERT(dataCount <= m_markupsData.capacity());

			for (uint32 idx = 0; idx < dataCount; ++idx)
			{
				NavigationVolumeID markupId;
				ReadNavigationIdType(file, markupId);

				m_markupsData.insert(markupId, MNM::SMarkupVolumeData());

				MNM::SMarkupVolumeData& markupData = m_markupsData[markupId];

				uint32 meshTrianglesCount;
				file.ReadType(&meshTrianglesCount);

				for (uint32 meshTriIdx = 0; meshTriIdx < meshTrianglesCount; ++meshTriIdx)
				{
					NavigationMeshID meshId;
					ReadNavigationIdType(file, meshId);

					const MNM::CNavMesh& navMesh = m_meshes[meshId].navMesh;

					MNM::SMarkupVolumeData::MeshTriangles meshTriangles(meshId);

					uint32 trianglesCount;
					file.ReadType(&trianglesCount);

					meshTriangles.triangleIds.reserve(trianglesCount);
					for (uint32 triIdx = 0; triIdx < trianglesCount; ++triIdx)
					{
						uint32 x, y, z;
						uint16 triIndex;
						file.ReadType(&x);
						file.ReadType(&y);
						file.ReadType(&z);
						file.ReadType(&triIndex);

						const MNM::TileID tileId = navMesh.GetTileID(x, y, z);
						CRY_ASSERT(tileId);

						const MNM::TriangleID triId = MNM::ComputeTriangleID(tileId, triIndex);

						meshTriangles.triangleIds.push_back(triId);
					}
					markupData.meshTriangles.push_back(meshTriangles);
				}
			}

			m_volumesManager.LoadData(file, nFileVersion);

			fileLoaded = true;
		}

		file.Close();
	}

	m_volumesManager.ValidateAndSanitizeLoadedAreas(*this);

	ENavigationEvent navigationEvent = (bAfterExporting) ? MeshReloadedAfterExporting : MeshReloaded;
	UpdateAllListener(navigationEvent);

	m_offMeshNavigationManager.OnNavigationLoadedComplete();

	//TODO: consider saving island connectivity in the navmesh
	ComputeIslands();

	m_pEditorBackgroundUpdate->Pause(false);

	return fileLoaded;
}

//////////////////////////////////////////////////////////////////////////
/// From the original tile, it generates triangles and links with no off-mesh information
/// Returns the new number of links (triangle count is the same)
#define DO_FILTERING_CONSISTENCY_CHECK 0

uint16 FilterOffMeshLinksForTile(const MNM::STile& tile, MNM::Tile::STriangle* pTrianglesBuffer, uint16 trianglesBufferSize, MNM::Tile::SLink* pLinksBuffer, uint16 linksBufferSize)
{
	assert(pTrianglesBuffer);
	assert(pLinksBuffer);
	assert(tile.GetTrianglesCount() <= trianglesBufferSize);
	assert(tile.GetLinksCount() <= linksBufferSize);

	uint16 newLinkCount = 0;
	uint16 offMeshLinksCount = 0;

	const uint16 trianglesCount = tile.GetTrianglesCount();
	const MNM::Tile::STriangle* pTriangles = tile.GetTriangles();

	if (const MNM::Tile::SLink* pLinks = tile.GetLinks())
	{
		const uint16 linksCount = tile.GetLinksCount();

		//Re-adjust link indices for triangles
		for (uint16 t = 0; t < trianglesCount; ++t)
		{
			const MNM::Tile::STriangle& triangle = pTriangles[t];
			pTrianglesBuffer[t] = triangle;

			pTrianglesBuffer[t].firstLink = triangle.firstLink - offMeshLinksCount;

			if ((triangle.linkCount > 0) && (pLinks[triangle.firstLink].side == MNM::Tile::SLink::OffMesh))
			{
				pTrianglesBuffer[t].linkCount--;
				offMeshLinksCount++;
			}
		}

		//Now copy links except off-mesh ones
		for (uint16 l = 0; l < linksCount; ++l)
		{
			const MNM::Tile::SLink& link = pLinks[l];

			if (link.side != MNM::Tile::SLink::OffMesh)
			{
				pLinksBuffer[newLinkCount] = link;
				newLinkCount++;
			}
		}
	}
	else
	{
		//Just copy the triangles as they are
		memcpy(pTrianglesBuffer, pTriangles, sizeof(MNM::Tile::STriangle) * trianglesCount);
	}

#if DO_FILTERING_CONSISTENCY_CHECK
	if (newLinkCount > 0)
	{
		for (uint16 i = 0; i < trianglesCount; ++i)
		{
			if ((pTrianglesBuffer[i].firstLink + pTrianglesBuffer[i].linkCount) > newLinkCount)
			{
				__debugbreak();
			}
		}
	}
#endif

	assert(newLinkCount == (tile.GetLinksCount() - offMeshLinksCount));

	return newLinkCount;
}

void NavigationSystem::GatherNavigationVolumesToSave(std::vector<NavigationVolumeID>& usedVolumes) const
{
	// #MNM_TODO pavloi 2016.10.21: it may be faster to iterate through m_volumes and gather all volumes from there.
	// But there is no guarantee yet, that the registered volumes are actually used.

	usedVolumes.reserve(m_volumes.size() * m_agentTypes.size());

	for (const AgentType& agentType : m_agentTypes)
	{
		for (const AgentType::MeshInfo& meshInfo : agentType.meshes)
		{
			CRY_ASSERT(m_meshes.validate(meshInfo.id));
			const NavigationMesh& mesh = m_meshes.get(meshInfo.id);

			CRY_ASSERT(mesh.boundary);
			CRY_ASSERT(m_volumes.validate(mesh.boundary));
			if (mesh.boundary && m_volumes.validate(mesh.boundary))
			{
				usedVolumes.push_back(mesh.boundary);
			}
		}

		for (const NavigationVolumeID& volumeId : agentType.exclusions)
		{
			CRY_ASSERT(volumeId);
			CRY_ASSERT(m_volumes.validate(volumeId));
			if (volumeId && m_volumes.validate(volumeId))
			{
				usedVolumes.push_back(volumeId);
			}
		}
	}
	std::sort(usedVolumes.begin(), usedVolumes.end());
	auto lastUniqueIter = std::unique(usedVolumes.begin(), usedVolumes.end());
	usedVolumes.erase(lastUniqueIter, usedVolumes.end());

	if (usedVolumes.size() != m_volumes.size())
	{
		AIWarning("NavigationSystem::GatherNavigationVolumesToSave: there are registered navigation volumes, which are not used by any navigation mesh. They will not be saved.");
	}
}

bool NavigationSystem::SaveToFile(const char* fileName) const PREFAST_SUPPRESS_WARNING(6262)
{
#if NAVIGATION_SYSTEM_PC_ONLY

	m_pEditorBackgroundUpdate->Pause(true);

	CCryFile file;
	if (false != file.Open(fileName, "wb"))
	{
		const int maxTriangles = 1024;
		const int maxLinks = maxTriangles * 6;
		MNM::Tile::STriangle triangleBuffer[maxTriangles];
		MNM::Tile::SLink linkBuffer[maxLinks];

		// Saving file data version
		uint16 nFileVersion = eBAINavigationFileVersion::CURRENT;
		file.Write(&nFileVersion, sizeof(nFileVersion));
		file.Write(&m_configurationVersion, sizeof(m_configurationVersion));
	#ifdef SW_NAVMESH_USE_GUID
		uint32 useGUID = BAI_NAVIGATION_GUID_FLAG;
		file.Write(&useGUID, sizeof(useGUID));
	#endif

		// Saving boundary volumes, their ID's and names
		{
			std::vector<NavigationVolumeID> usedVolumes;
			GatherNavigationVolumesToSave(*&usedVolumes);

			const uint32 usedVolumesCount = static_cast<uint32>(usedVolumes.size());
			file.WriteType(&usedVolumesCount);

			string volumeAreaName;
			for (uint32 idx = 0; idx < usedVolumesCount; ++idx)
			{
				const NavigationVolumeID volumeId = usedVolumes[idx];
				CRY_ASSERT(m_volumes.validate(volumeId));
				const MNM::BoundingVolume& volume = m_volumes.get(volumeId);

				const uint32 verticesCount = volume.GetBoundaryVertices().size();

				volumeAreaName.clear();
				m_volumesManager.GetAreaName(volumeId, *&volumeAreaName);
				const uint32 volumeAreaNameSize = static_cast<uint32>(volumeAreaName.size());

				WriteNavigationIdType(file, volumeId);
				file.WriteType(&volume.height);
				file.WriteType(&verticesCount);
				for (const Vec3& vertex : volume.GetBoundaryVertices())
				{
					file.WriteType(&vertex.x, 3);
				}
				file.WriteType(&volumeAreaNameSize);
				file.WriteType(volumeAreaName.c_str(), volumeAreaNameSize);
			}
		}

		// Saving markup boundary areas and data
		{
			//TODO: gather markup volumes to save
			const uint32 markupsCount = static_cast<uint32>(m_markupVolumes.size());
			const uint32 markupsCapacity = static_cast<uint32>(m_markupVolumes.capacity());
			file.WriteType(&markupsCount);
			file.WriteType(&markupsCapacity);

			for (uint32 idx = 0; idx < m_markupVolumes.capacity(); ++idx)
			{
				if(m_markupVolumes.index_free(idx))
					continue;

				const MNM::SMarkupVolume& markupVolume = m_markupVolumes.get_index(idx);
				NavigationVolumeID volumeId = NavigationVolumeID(m_markupVolumes.get_index_id(idx));

				const uint32 verticesCount = markupVolume.GetBoundaryVertices().size();
				MNM::AreaAnnotation::value_type areaAnnotation = markupVolume.areaAnnotation.GetRawValue();

				WriteNavigationIdType(file, volumeId);
				file.WriteType(&markupVolume.height);
				file.WriteType(&areaAnnotation);
				file.WriteType(&markupVolume.bExpandByAgentRadius);
				file.WriteType(&markupVolume.bStoreTriangles);
				file.WriteType(&verticesCount);
				for (const Vec3& vertex : markupVolume.GetBoundaryVertices())
				{
					file.WriteType(&vertex.x, 3);
				}
			}
		}

		// Saving number of agents
		uint32 agentsCount = static_cast<uint32>(GetAgentTypeCount());
		file.Write(&agentsCount, sizeof(agentsCount));
		std::vector<string> agentsNamesList;

		AgentTypes::const_iterator typeIt = m_agentTypes.begin();
		AgentTypes::const_iterator typeEnd = m_agentTypes.end();

		for (; typeIt != typeEnd; ++typeIt)
		{
			const AgentType& agentType = *typeIt;
			uint32 nameLength = agentType.name.length();
			nameLength = std::min(nameLength, (uint32)MAX_NAME_LENGTH - 1);
			// Saving name length and the name itself
			file.Write(&nameLength, sizeof(nameLength));
			file.Write(agentType.name.c_str(), sizeof(char) * nameLength);

			// Saving the amount of memory this agent is using inside the file to be able to skip it during loading
			uint32 totalAgentMemory = 0;
			size_t totalAgentMemoryPositionInFile = file.GetPosition();
			file.Write(&totalAgentMemory, sizeof(totalAgentMemory));

			AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
			AgentType::Meshes::const_iterator mend = agentType.meshes.end();
			{
				// Saving markup volumes for agent
				uint32 markupsCount = agentType.markups.size();
				file.WriteType(&markupsCount);
				for (NavigationVolumeID markupID : agentType.markups)
				{
					WriteNavigationIdType(file, markupID);
				}
			}

			uint32 meshesCount = agentType.meshes.size();
			file.Write(&meshesCount, sizeof(meshesCount));

			for (; mit != mend; ++mit)
			{
				const uint32 meshIDuint32 = mit->id;
				const NavigationMesh& mesh = m_meshes[NavigationMeshID(meshIDuint32)];
				const MNM::BoundingVolume& volume = m_volumes[mesh.boundary];
				const MNM::CNavMesh& navMesh = mesh.navMesh;

				// Saving mesh id
	#ifdef SW_NAVMESH_USE_GUID
				const NavigationMeshGUID meshGUID = mit->guid;
				file.Write(&meshGUID, sizeof(meshGUID));
	#else
				file.Write(&meshIDuint32, sizeof(meshIDuint32));
	#endif

				// Saving mesh name
				uint32 meshNameLength = mesh.name.length();
				meshNameLength = std::min(meshNameLength, (uint32)MAX_NAME_LENGTH - 1);
				file.Write(&meshNameLength, sizeof(meshNameLength));
				file.Write(mesh.name.c_str(), sizeof(char) * meshNameLength);

				// Saving total islands
				uint32 totalIslands = mesh.navMesh.GetIslands().GetTotalIslands();
				file.Write(&totalIslands, sizeof(totalIslands));

				uint32 totalMeshMemory = 0;
				size_t totalMeshMemoryPositionInFile = file.GetPosition();
				file.Write(&totalMeshMemory, sizeof(totalMeshMemory));

				// Saving mesh boundary id
				/*
				   Let's check if this boundary id matches the id of the
				   volume stored in the volumes manager.
				   It's an additional check for the consistency of the
				   saved binary data.
				 */

				if (!m_volumes.validate(mesh.boundary) || m_volumesManager.GetAreaID(mesh.name.c_str()) != mesh.boundary)
				{
					CryMessageBox("Sandbox detected a possible data corruption during the save of the navigation mesh."
					              "Trigger a full rebuild and re-export to engine to fix"
					              " the binary data associated with the MNM.", "Navigation Save Error");
				}
	#ifdef SW_NAVMESH_USE_GUID
				file.Write(&mesh.boundaryGUID, sizeof(mesh.boundaryGUID));
	#else
				WriteNavigationIdType(file, mesh.boundary);
	#endif

				// Saving mesh exclusion shapes
	#ifdef SW_NAVMESH_USE_GUID
				uint32 exclusionShapesCount = mesh.exclusionsGUID.size();
				file.Write(&exclusionShapesCount, sizeof(exclusionShapesCount));
				for (uint32 exclusionCounter = 0; exclusionCounter < exclusionShapesCount; ++exclusionCounter)
				{
					NavigationVolumeGUID exclusionGuid = mesh.exclusionsGUID[exclusionCounter];
					file.Write(&(exclusionGuid), sizeof(exclusionGuid));
				}
	#else
				{
					// Figure out which of the exclusion volume IDs are valid in order to export only those.
					// This check will also fix maps that get exported after 2016-11-23. All maps prior to that date might contain invalid exclusion volume IDs and need to get exported again.
					NavigationMesh::ExclusionVolumes validExlusionVolumes;
					for (NavigationVolumeID volumeID : mesh.exclusions)
					{
						if (m_volumes.validate(volumeID))
						{
							validExlusionVolumes.push_back(volumeID);
						}
					}
					
					// Now export only the valid exclusion volume IDs.
					uint32 exclusionShapesCount = validExlusionVolumes.size();
					file.Write(&exclusionShapesCount, sizeof(exclusionShapesCount));
					for (uint32 exclusionCounter = 0; exclusionCounter < exclusionShapesCount; ++exclusionCounter)
					{
						WriteNavigationIdType(file, validExlusionVolumes[exclusionCounter]);
					}
				}
	#endif

				{
					// Saving markup volumes for mesh
					uint32 markupsCount = mesh.markups.size();
					file.WriteType(&markupsCount);
					for (NavigationVolumeID markupID : mesh.markups)
					{
						WriteNavigationIdType(file, markupID);
					}
				}

				// Saving tiles count
				uint32 tileCount = navMesh.GetTileCount();
				file.Write(&tileCount, sizeof(tileCount));

				// Saving grid params (Not all of this params are actually important to recreate the mesh but
				// we save all for possible further utilization)
				const MNM::CNavMesh::SGridParams paramsGrid = navMesh.GetGridParams();
				file.Write(&(paramsGrid.origin.x), sizeof(paramsGrid.origin.x));
				file.Write(&(paramsGrid.origin.y), sizeof(paramsGrid.origin.y));
				file.Write(&(paramsGrid.origin.z), sizeof(paramsGrid.origin.z));
				file.Write(&(paramsGrid.tileSize.x), sizeof(paramsGrid.tileSize.x));
				file.Write(&(paramsGrid.tileSize.y), sizeof(paramsGrid.tileSize.y));
				file.Write(&(paramsGrid.tileSize.z), sizeof(paramsGrid.tileSize.z));
				file.Write(&(paramsGrid.voxelSize.x), sizeof(paramsGrid.voxelSize.x));
				file.Write(&(paramsGrid.voxelSize.y), sizeof(paramsGrid.voxelSize.y));
				file.Write(&(paramsGrid.voxelSize.z), sizeof(paramsGrid.voxelSize.z));
				file.Write(&(paramsGrid.tileCount), sizeof(paramsGrid.tileCount));

				const AABB& boundary = m_volumes[mesh.boundary].aabb;

				Vec3 bmin(std::max(0.0f, boundary.min.x - paramsGrid.origin.x),
				          std::max(0.0f, boundary.min.y - paramsGrid.origin.y),
				          std::max(0.0f, boundary.min.z - paramsGrid.origin.z));

				Vec3 bmax(std::max(0.0f, boundary.max.x - paramsGrid.origin.x),
				          std::max(0.0f, boundary.max.y - paramsGrid.origin.y),
				          std::max(0.0f, boundary.max.z - paramsGrid.origin.z));

				uint16 xmin = (uint16)(floor_tpl(bmin.x / (float)paramsGrid.tileSize.x));
				uint16 xmax = (uint16)(floor_tpl(bmax.x / (float)paramsGrid.tileSize.x));

				uint16 ymin = (uint16)(floor_tpl(bmin.y / (float)paramsGrid.tileSize.y));
				uint16 ymax = (uint16)(floor_tpl(bmax.y / (float)paramsGrid.tileSize.y));

				uint16 zmin = (uint16)(floor_tpl(bmin.z / (float)paramsGrid.tileSize.z));
				uint16 zmax = (uint16)(floor_tpl(bmax.z / (float)paramsGrid.tileSize.z));

				for (uint16 x = xmin; x < xmax + 1; ++x)
				{
					for (uint16 y = ymin; y < ymax + 1; ++y)
					{
						for (uint16 z = zmin; z < zmax + 1; ++z)
						{
							MNM::TileID i = navMesh.GetTileID(x, y, z);
							// Skipping tile id that are not used (This should never happen now)
							if (i == 0)
							{
								continue;
							}

							// Saving tile indexes
							file.Write(&x, sizeof(x));
							file.Write(&y, sizeof(y));
							file.Write(&z, sizeof(z));
							const MNM::STile& tile = navMesh.GetTile(i);
							const uint32 tileHashValue = tile.GetHashValue();
							file.Write(&tileHashValue, sizeof(tileHashValue));

							// NOTE pavloi 2016.07.22: triangles and links are not saved as is - instead they are filtered and copied into triangleBuffer and linkBuffer
							const uint16 saveLinkCount = FilterOffMeshLinksForTile(tile, triangleBuffer, maxTriangles, linkBuffer, maxLinks);

							// Saving triangles
							const uint16 trianglesCount = tile.GetTrianglesCount();
							file.Write(&trianglesCount, sizeof(trianglesCount));
							file.Write(triangleBuffer, sizeof(MNM::Tile::STriangle) * trianglesCount);

							// Saving vertices
							const MNM::Tile::Vertex* pVertices = tile.GetVertices();
							const uint16 verticesCount = tile.GetVerticesCount();
							file.Write(&verticesCount, sizeof(verticesCount));
							file.Write(pVertices, sizeof(MNM::Tile::Vertex) * verticesCount);

							// Saving links
							file.Write(&saveLinkCount, sizeof(saveLinkCount));
							file.Write(linkBuffer, sizeof(MNM::Tile::SLink) * saveLinkCount);

							// Saving nodes
							const MNM::Tile::SBVNode* pNodes = tile.GetBVNodes();
							const uint16 nodesCount = tile.GetBVNodesCount();
							file.Write(&nodesCount, sizeof(nodesCount));
							file.Write(pNodes, sizeof(MNM::Tile::SBVNode) * nodesCount);

							// Compile-time asserts to catch data type changes - don't forget to bump BAI file version number
							static_assert(sizeof(uint16) == sizeof(tile.GetLinksCount()), "Invalid type size!");
							static_assert(sizeof(uint16) == sizeof(tile.GetTrianglesCount()), "Invalid type size!");
							static_assert(sizeof(uint16) == sizeof(tile.GetVerticesCount()), "Invalid type size!");
							static_assert(sizeof(uint16) == sizeof(tile.GetBVNodesCount()), "Invalid type size!");
							static_assert(sizeof(uint16) == sizeof(trianglesCount), "Invalid type size!");
							static_assert(sizeof(uint16) == sizeof(verticesCount), "Invalid type size!");
							static_assert(sizeof(uint16) == sizeof(saveLinkCount), "Invalid type size!");
							static_assert(sizeof(uint16) == sizeof(nodesCount), "Invalid type size!");
							static_assert(sizeof(MNM::Tile::Vertex) == 6, "Invalid type size!");
							static_assert(sizeof(MNM::Tile::STriangle) == 16, "Invalid type size!");
							static_assert(sizeof(MNM::Tile::SLink) == 2, "Invalid type size!");
							static_assert(sizeof(MNM::Tile::SBVNode) == 14, "Invalid type size!");
							static_assert(sizeof(uint32) == sizeof(tile.GetHashValue()), "Invalid type size!");
						}
					}

				}
				size_t endingMeshDataPosition = file.GetPosition();
				totalMeshMemory = endingMeshDataPosition - totalMeshMemoryPositionInFile - sizeof(totalMeshMemory);
				file.Seek(totalMeshMemoryPositionInFile, SEEK_SET);
				file.Write(&totalMeshMemory, sizeof(totalMeshMemory));
				file.Seek(endingMeshDataPosition, SEEK_SET);
			}

			size_t endingAgentDataPosition = file.GetPosition();
			totalAgentMemory = endingAgentDataPosition - totalAgentMemoryPositionInFile - sizeof(totalAgentMemory);
			file.Seek(totalAgentMemoryPositionInFile, SEEK_SET);
			file.Write(&totalAgentMemory, sizeof(totalAgentMemory));
			file.Seek(endingAgentDataPosition, SEEK_SET);
		}

		const uint32 dataCount = static_cast<uint32>(m_markupsData.size());
		file.WriteType(&dataCount);

		for (uint32 idx = 0; idx < m_markupsData.capacity(); ++idx)
		{
			if (m_markupsData.index_free(idx))
				continue;

			const MNM::SMarkupVolumeData& markupData = m_markupsData.get_index(idx);
			NavigationVolumeID markupId = NavigationVolumeID(m_markupsData.get_index_id(idx));

			WriteNavigationIdType(file, markupId);

			const uint32 meshTrianglesCount = markupData.meshTriangles.size();
			file.WriteType(&meshTrianglesCount);

			for (const MNM::SMarkupVolumeData::MeshTriangles& meshTriangles : markupData.meshTriangles)
			{
				const MNM::CNavMesh& navMesh = m_meshes[meshTriangles.meshId].navMesh;

				WriteNavigationIdType(file, meshTriangles.meshId);

				const uint32 trianglesCount = meshTriangles.triangleIds.size();
				file.WriteType(&trianglesCount);
				for (MNM::TriangleID triId : meshTriangles.triangleIds)
				{
					const MNM::TileID tileId = MNM::ComputeTileID(triId);
					const uint16 triIndex = MNM::ComputeTriangleIndex(triId);

					const MNM::vector3_t tileCoords = navMesh.GetTileContainerCoordinates(tileId);
					const uint32 x = tileCoords.x.as_uint();
					const uint32 y = tileCoords.y.as_uint();
					const uint32 z = tileCoords.z.as_uint();

					file.WriteType(&x);
					file.WriteType(&y);
					file.WriteType(&z);
					file.WriteType(&triIndex);
				}
			}
		}

		m_volumesManager.SaveData(file, nFileVersion);

		file.Close();
	}

	m_pEditorBackgroundUpdate->Pause(false);

#endif
	return true;
}

void NavigationSystem::UpdateAllListener(const ENavigationEvent event)
{
	for (NavigationListeners::Notifier notifier(m_listenersList); notifier.IsValid(); notifier.Next())
	{
		notifier->OnNavigationEvent(event);
	}
}

void NavigationSystem::DebugDraw()
{
	m_debugDraw.DebugDraw(*this);
}

void NavigationSystem::Reset()
{
	ResetAllNavigationSystemUsers();
}

void NavigationSystem::GetMemoryStatistics(ICrySizer* pSizer)
{
#if DEBUG_MNM_ENABLED
	size_t totalMemory = 0;
	for (uint16 i = 0; i < m_meshes.capacity(); ++i)
	{
		if (!m_meshes.index_free(i))
		{
			const NavigationMeshID meshID(m_meshes.get_index_id(i));

			const NavigationMesh& mesh = m_meshes[meshID];
			const MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
			const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];

			const NavigationMesh::ProfileMemoryStats meshMemStats = mesh.GetMemoryStats(pSizer);
			const MNM::OffMeshNavigation::ProfileMemoryStats offMeshMemStats = offMeshNavigation.GetMemoryStats(pSizer);
			const OffMeshNavigationManager::ProfileMemoryStats linksMemStats = m_offMeshNavigationManager.GetMemoryStats(pSizer, meshID);

			totalMemory = meshMemStats.totalNavigationMeshMemory + offMeshMemStats.totalSize + linksMemStats.totalSize;
		}
	}
#endif
}

void NavigationSystem::SetDebugDisplayAgentType(NavigationAgentTypeID agentTypeID)
{
	m_debugDraw.SetAgentType(agentTypeID);
}

NavigationAgentTypeID NavigationSystem::GetDebugDisplayAgentType() const
{
	return m_debugDraw.GetAgentType();
}

void NavigationSystem::OffsetBoundingVolume(const Vec3& additionalOffset, const NavigationVolumeID volumeId)
{
	if (volumeId && m_volumes.validate(volumeId))
	{
		m_volumes[volumeId].OffsetVerticesAndAABB(additionalOffset);
	}
}

void NavigationSystem::OffsetAllMeshes(const Vec3& additionalOffset)
{
	AgentTypes::iterator it = m_agentTypes.begin();
	AgentTypes::iterator end = m_agentTypes.end();

	std::set<NavigationVolumeID> volumeSet;
	for (; it != end; ++it)
	{
		AgentType& agentType = *it;

		AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
		AgentType::Meshes::const_iterator mend = agentType.meshes.end();

		for (; mit != mend; ++mit)
		{
			const NavigationMeshID meshId = mit->id;
			if (meshId && m_meshes.validate(meshId))
			{
				NavigationMesh& mesh = m_meshes[meshId];
				mesh.navMesh.OffsetOrigin(additionalOffset);
				NavigationVolumeID volumeId = mesh.boundary;
				volumeSet.insert(volumeId);
			}
		}
	}

	std::set<NavigationVolumeID>::iterator volumeIt = volumeSet.begin();
	std::set<NavigationVolumeID>::iterator volumeEnd = volumeSet.end();
	for (; volumeIt != volumeEnd; ++volumeIt)
	{
		OffsetBoundingVolume(additionalOffset, *volumeIt);
	}
}

TileGeneratorExtensionID NavigationSystem::RegisterTileGeneratorExtension(MNM::TileGenerator::IExtension& extension)
{
	const TileGeneratorExtensionID newId = TileGeneratorExtensionID(m_tileGeneratorExtensionsContainer.idCounter + 1);

	if (newId != TileGeneratorExtensionID())
	{
		m_tileGeneratorExtensionsContainer.idCounter = newId;
	}
	else
	{
		CRY_ASSERT_MESSAGE(newId != TileGeneratorExtensionID(), "TileGeneratorExtensionID counter is exausted");
		return TileGeneratorExtensionID();
	}

	{
		AUTO_MODIFYLOCK(m_tileGeneratorExtensionsContainer.extensionsLock);

		m_tileGeneratorExtensionsContainer.extensions[newId] = &extension;
		return newId;
	}
}

bool NavigationSystem::UnRegisterTileGeneratorExtension(const TileGeneratorExtensionID extensionId)
{
	AUTO_MODIFYLOCK(m_tileGeneratorExtensionsContainer.extensionsLock);
	auto iter = m_tileGeneratorExtensionsContainer.extensions.find(extensionId);
	if (iter != m_tileGeneratorExtensionsContainer.extensions.end())
	{
		m_tileGeneratorExtensionsContainer.extensions.erase(extensionId);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if DEBUG_MNM_ENABLED

// Only needed for the NavigationSystemDebugDraw
	#include "Navigation/PathHolder.h"
	#include "Navigation/MNM/OffGridLinks.h"

void NavigationSystemDebugDraw::DebugDraw(NavigationSystem& navigationSystem)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	const bool validDebugAgent = m_agentTypeID && (m_agentTypeID <= navigationSystem.GetAgentTypeCount());

	if (validDebugAgent)
	{
		static int lastFrameID = 0;

		const int frameID = gEnv->pRenderer->GetFrameID();
		if (frameID == lastFrameID)
			return;

		lastFrameID = frameID;

		MNM::TileID excludeTileID(0);
		DebugDrawSettings settings = GetDebugDrawSettings(navigationSystem);

		if (settings.Valid())
		{
			excludeTileID = DebugDrawTileGeneration(navigationSystem, settings);
		}

		DebugDrawRayCast(navigationSystem, settings);

		DebugDrawPathFinder(navigationSystem, settings);

		DebugDrawClosestPoint(navigationSystem, settings);
		DebugDrawGroundPoint(navigationSystem, settings);
		DebugDrawSnapToNavmesh(navigationSystem, settings);

		DebugDrawIslandConnection(navigationSystem, settings);

		DebugDrawNavigationMeshesForSelectedAgent(navigationSystem, excludeTileID);

		DebugDrawTriangleOnCursor(navigationSystem);

		m_progress.Draw();

		if (navigationSystem.IsInUse())
		{
			navigationSystem.m_offMeshNavigationManager.UpdateEditorDebugHelpers();
		}
	}

	DebugDrawNavigationSystemState(navigationSystem);

	DebugDrawMemoryStats(navigationSystem);
}

void NavigationSystemDebugDraw::UpdateWorkingProgress(const float frameTime, const size_t queueSize)
{
	m_progress.Update(frameTime, queueSize);
}

MNM::TileID NavigationSystemDebugDraw::DebugDrawTileGeneration(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
	MNM::TileID debugTileID(0);
	#if DEBUG_MNM_ENABLED

	// TODO pavloi 2016.03.09: instead of calling GetAsyncKeyState(), register for events with GetISystem()->GetIInput()->AddEventListener().

	static MNM::CTileGenerator debugGenerator;
	static MNM::TileID tileID(0);
	static bool prevKeyState = false;
	static size_t drawMode = (size_t)MNM::CTileGenerator::EDrawMode::DrawNone;
	static bool bDrawAdditionalInfo = false;

	NavigationMesh& mesh = navigationSystem.m_meshes[settings.meshID];
	const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();
	const AgentType& agentType = navigationSystem.m_agentTypes[m_agentTypeID - 1];

	bool forceGeneration = settings.forceGeneration;
	size_t selectedX = settings.selectedX;
	size_t selectedY = settings.selectedY;
	size_t selectedZ = settings.selectedZ;

	CDebugDrawContext dc;

	bool keyState = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;

	if (keyState && keyState != prevKeyState)
	{
		if (((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0))
		{
			forceGeneration = true;
		}
		else if (((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0))
			--drawMode;
		else
			++drawMode;

		if (drawMode == (size_t)MNM::CTileGenerator::EDrawMode::LastDrawMode)
			drawMode = (size_t)MNM::CTileGenerator::EDrawMode::DrawNone;
		else if (drawMode > (size_t)MNM::CTileGenerator::EDrawMode::LastDrawMode)
			drawMode = (size_t)MNM::CTileGenerator::EDrawMode::LastDrawMode - 1;
	}

	prevKeyState = keyState;

	{
		static size_t selectPrevKeyState = 0;
		size_t selectKeyState = 0;

		selectKeyState |= ((GetAsyncKeyState(VK_UP) & 0x8000) != 0);
		selectKeyState |= 2 * ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0);
		selectKeyState |= 4 * ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
		selectKeyState |= 8 * ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0);
		selectKeyState |= 16 * ((GetAsyncKeyState(VK_ADD) & 0x8000) != 0);
		selectKeyState |= 32 * ((GetAsyncKeyState(VK_SUBTRACT) & 0x8000) != 0);

		int dirX = 0;
		int dirY = 0;
		int dirZ = 0;

		if ((selectKeyState & 1 << 0) && ((selectPrevKeyState & 1 << 0) == 0))
			dirY += 1;
		if ((selectKeyState & 1 << 1) && ((selectPrevKeyState & 1 << 1) == 0))
			dirX += 1;
		if ((selectKeyState & 1 << 2) && ((selectPrevKeyState & 1 << 2) == 0))
			dirY -= 1;
		if ((selectKeyState & 1 << 3) && ((selectPrevKeyState & 1 << 3) == 0))
			dirX -= 1;
		if ((selectKeyState & 1 << 4) && ((selectPrevKeyState & 1 << 4) == 0))
			dirZ += 1;
		if ((selectKeyState & 1 << 5) && ((selectPrevKeyState & 1 << 5) == 0))
			dirZ -= 1;

		if (!mesh.boundary)
		{
			selectedX += dirX;
			selectedY += dirY;
			selectedZ += dirZ;
		}
		else
		{
			Vec3 tileOrigin = paramsGrid.origin +
			                  Vec3((float)(selectedX + dirX) * paramsGrid.tileSize.x,
			                       (float)(selectedY + dirY) * paramsGrid.tileSize.y,
			                       (float)(selectedZ + dirZ) * paramsGrid.tileSize.z);

			if (navigationSystem.m_volumes[mesh.boundary].Contains(tileOrigin))
			{
				selectedX += dirX;
				selectedY += dirY;
				selectedZ += dirZ;
			}
		}

		selectPrevKeyState = selectKeyState;
	}

	{
		static bool bPrevAdditionalInfoKeyState = false;
		bool bAdditionalInfoKeyState = (GetAsyncKeyState(VK_DIVIDE) & 0x8000) != 0;
		if (bAdditionalInfoKeyState && bAdditionalInfoKeyState != bPrevAdditionalInfoKeyState)
		{
			bDrawAdditionalInfo = !bDrawAdditionalInfo;
		}
		bPrevAdditionalInfoKeyState = bAdditionalInfoKeyState;
	}

	if (forceGeneration)
	{
		tileID = 0;
		debugGenerator = MNM::CTileGenerator();

		MNM::STile tile;
		MNM::CTileGenerator::SMetaData metaData;
		MNM::CTileGenerator::Params params;

		NavigationSystem::VolumeDefCopy def;
		if (mesh.boundary)
		{
			def.boundary = navigationSystem.m_volumes[mesh.boundary];
		}

		def.exclusions.resize(mesh.exclusions.size());
		for (size_t eindex = 0; eindex < mesh.exclusions.size(); ++eindex)
		{
			def.exclusions[eindex] = navigationSystem.m_volumes[mesh.exclusions[eindex]];
		}

		def.markups.resize(mesh.markups.size());
		def.markupIds.resize(mesh.markups.size());
		for (size_t mindex = 0; mindex < mesh.markups.size(); ++mindex)
		{
			def.markups[mindex] = navigationSystem.m_markupVolumes[mesh.markups[mindex]];
			def.markupIds[mindex] = mesh.markups[mindex];
		}
		navigationSystem.SetupGenerator(settings.meshID, paramsGrid, static_cast<uint16>(selectedX), static_cast<uint16>(selectedY), static_cast<uint16>(selectedZ), params, def, false);

		params.flags |= MNM::CTileGenerator::Params::DebugInfo | MNM::CTileGenerator::Params::NoHashTest;

		if (debugGenerator.Generate(params, tile, metaData, 0))
		{
			tileID = mesh.navMesh.SetTile(selectedX, selectedY, selectedZ, tile);

			mesh.navMesh.ConnectToNetwork(tileID);
		}
		else if (tileID = mesh.navMesh.GetTileID(selectedX, selectedY, selectedZ))
			mesh.navMesh.ClearTile(tileID);
	}

	debugGenerator.Draw((MNM::CTileGenerator::EDrawMode)drawMode, bDrawAdditionalInfo);

	const char* drawModeName = "None";

	switch ((MNM::CTileGenerator::EDrawMode)drawMode)
	{
	case MNM::CTileGenerator::EDrawMode::DrawNone:
		break;

	case MNM::CTileGenerator::EDrawMode::DrawRawInputGeometry:
		drawModeName = "Raw Input Geometry";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawRawVoxels:
		drawModeName = "Raw Voxels";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawFilteredVoxels:
		drawModeName = "Filtered Voxels - filtered after walkable test";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawFlaggedVoxels:
		drawModeName = "Flagged Voxels";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawDistanceTransform:
		drawModeName = "Distance Transform";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawPainting:
		drawModeName = "Painting";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawSegmentation:
		drawModeName = "Segmentation";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawNumberedContourVertices:
	case MNM::CTileGenerator::EDrawMode::DrawContourVertices:
		drawModeName = "Contour Vertices";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawTracers:
		drawModeName = "Tracers";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawSimplifiedContours:
		drawModeName = "Simplified Contours";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawTriangulation:
		drawModeName = "Triangulation";
		break;

	case MNM::CTileGenerator::EDrawMode::DrawBVTree:
		drawModeName = "BV Tree";
		break;

	case MNM::CTileGenerator::EDrawMode::LastDrawMode:
	default:
		drawModeName = "Unknown";
		break;
	}

	dc->Draw2dLabel(10.0f, 5.0f, 1.6f, Col_White, false, "TileID %d - Drawing %s", tileID, drawModeName);

	const MNM::CTileGenerator::ProfilerType& profilerInfo = debugGenerator.GetProfiler();

	dc->Draw2dLabel(10.0f, 28.0f, 1.25f, Col_White, false,
	                "Total: %.1f - Voxelizer(%.2fK tris): %.1f - Filter: %.1f\n"
	                "Contour(%d regs): %.1f - Simplify: %.1f\n"
	                "Triangulate(%d vtx/%d tris): %.1f - BVTree(%d nodes): %.1f",
	                profilerInfo.GetTotalElapsed().GetMilliSeconds(),
	                profilerInfo[MNM::CTileGenerator::VoxelizationTriCount] / 1000.0f,
	                profilerInfo[MNM::CTileGenerator::Voxelization].elapsed.GetMilliSeconds(),
	                profilerInfo[MNM::CTileGenerator::Filter].elapsed.GetMilliSeconds(),
	                profilerInfo[MNM::CTileGenerator::RegionCount],
	                profilerInfo[MNM::CTileGenerator::ContourExtraction].elapsed.GetMilliSeconds(),
	                profilerInfo[MNM::CTileGenerator::Simplification].elapsed.GetMilliSeconds(),
	                profilerInfo[MNM::CTileGenerator::VertexCount],
	                profilerInfo[MNM::CTileGenerator::TriangleCount],
	                profilerInfo[MNM::CTileGenerator::Triangulation].elapsed.GetMilliSeconds(),
	                profilerInfo[MNM::CTileGenerator::BVTreeNodeCount],
	                profilerInfo[MNM::CTileGenerator::BVTreeConstruction].elapsed.GetMilliSeconds()
	                );

	dc->Draw2dLabel(10.0f, 84.0f, 1.4f, Col_White, false,
	                "Peak Memory: %.2fKB", profilerInfo.GetMemoryPeak() / 1024.0f);

	size_t vertexMemory = profilerInfo[MNM::CTileGenerator::VertexMemory].used;
	size_t triangleMemory = profilerInfo[MNM::CTileGenerator::TriangleMemory].used;
	size_t bvTreeMemory = profilerInfo[MNM::CTileGenerator::BVTreeMemory].used;
	size_t tileMemory = vertexMemory + triangleMemory + bvTreeMemory;

	dc->Draw2dLabel(10.0f, 98.0f, 1.4f, Col_White, false,
	                "Tile Memory: %.2fKB (Vtx: %db, Tri: %db, BVT: %db)",
	                tileMemory / 1024.0f,
	                vertexMemory,
	                triangleMemory,
	                bvTreeMemory);

	if (drawMode != (size_t)MNM::CTileGenerator::EDrawMode::DrawNone)
	{
		debugTileID = mesh.navMesh.GetTileID(selectedX, selectedY, selectedZ);
	}
	#endif // DEBUG_MNM_ENABLED

	return debugTileID;
}

void NavigationSystemDebugDraw::DebugDrawRayCast(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
	CAISystem::SObjectDebugParams debugObjectStart;
	if (!GetAISystem()->GetObjectDebugParamsFromName("MNMRayStart", debugObjectStart))
		return;

	CAISystem::SObjectDebugParams debugObjectEnd;
	if (!GetAISystem()->GetObjectDebugParamsFromName("MNMRayEnd", debugObjectEnd))
		return;

	NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObjectStart.objectPos);
	IF_UNLIKELY(!meshID)
		return;

	NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
	const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();
	const MNM::CNavMesh& navMesh = mesh.navMesh;

	const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
		MNM::real_t(paramsGrid.origin.y),
		MNM::real_t(paramsGrid.origin.z));
	const MNM::vector3_t originOffset = origin + MNM::vector3_t(0, 0, MNM::real_t::fraction(725, 10000));

	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	const Vec3& startLoc = debugObjectStart.objectPos;
	const Vec3& endLoc = debugObjectEnd.objectPos;

	const MNM::real_t range = MNM::real_t(1.0f);

	MNM::vector3_t start = MNM::vector3_t(
		MNM::real_t(startLoc.x), MNM::real_t(startLoc.y), MNM::real_t(startLoc.z)) - origin;
	MNM::vector3_t end = MNM::vector3_t(
		MNM::real_t(endLoc.x), MNM::real_t(endLoc.y), MNM::real_t(endLoc.z)) - origin;

	const INavMeshQueryFilter* pDebugQueryFilter = GetDebugQueryFilter("MNMDebugQueryFilter");

	MNM::TriangleID triStart = navMesh.GetTriangleAt(start, range, range, pDebugQueryFilter);
	if (!triStart)
		return;

	if (triStart)
	{
		SAuxGeomRenderFlags oldFlags = renderAuxGeom->GetRenderFlags();

		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		renderFlags.SetAlphaBlendMode(e_AlphaBlended);
		renderAuxGeom->SetRenderFlags(renderFlags);

		MNM::TriangleID triEnd = navMesh.GetTriangleAt(end, range, range, pDebugQueryFilter);

		MNM::CNavMesh::RayCastRequest<512> raycastRequest;
		MNM::CNavMesh::ERayCastResult result = navMesh.RayCast(start, triStart, end, triEnd, raycastRequest, pDebugQueryFilter);

		for (size_t i = 0; i < raycastRequest.wayTriCount; ++i)
		{
			MNM::vector3_t a, b, c;

			navMesh.GetVertices(raycastRequest.way[i], a, b, c);

			renderAuxGeom->DrawTriangle(
				(a + originOffset).GetVec3(), ColorB(Col_Maroon, 0.5f),
				(b + originOffset).GetVec3(), ColorB(Col_Maroon, 0.5f),
				(c + originOffset).GetVec3(), ColorB(Col_Maroon, 0.5f));
		}

		if (triStart)
		{
			MNM::vector3_t a, b, c;
			navMesh.GetVertices(triStart, a, b, c);

			renderAuxGeom->DrawTriangle(
				(a + originOffset).GetVec3(), ColorB(Col_GreenYellow, 0.5f),
				(b + originOffset).GetVec3(), ColorB(Col_GreenYellow, 0.5f),
				(c + originOffset).GetVec3(), ColorB(Col_GreenYellow, 0.5f));
		}

		if (triEnd)
		{
			MNM::vector3_t a, b, c;
			navMesh.GetVertices(triEnd, a, b, c);

			renderAuxGeom->DrawTriangle(
				(a + originOffset).GetVec3(), ColorB(Col_Red, 0.5f),
				(b + originOffset).GetVec3(), ColorB(Col_Red, 0.5f),
				(c + originOffset).GetVec3(), ColorB(Col_Red, 0.5f));
		}

		const Vec3 offset(0.0f, 0.0f, 0.085f);

		if (result == MNM::CNavMesh::eRayCastResult_NoHit)
		{
			renderAuxGeom->DrawLine(startLoc + offset, Col_YellowGreen, endLoc + offset, Col_YellowGreen, 8.0f);
		}
		else
		{
			const MNM::CNavMesh::RayHit& hit = raycastRequest.hit;
			Vec3 hitLoc = (result == MNM::CNavMesh::eRayCastResult_Hit) ? startLoc + ((endLoc - startLoc) * hit.distance.as_float()) : startLoc;
			renderAuxGeom->DrawLine(startLoc + offset, Col_YellowGreen, hitLoc + offset, Col_YellowGreen, 8.0f);
			renderAuxGeom->DrawLine(hitLoc + offset, Col_Red, endLoc + offset, Col_Red, 8.0f);
		}

		renderAuxGeom->SetRenderFlags(oldFlags);
	}
}

void NavigationSystemDebugDraw::DebugDrawClosestPoint(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
	CAISystem::SObjectDebugParams debugObject;
	if (!GetAISystem()->GetObjectDebugParamsFromName("MNMClosestPoint", debugObject))
		return;
	
	NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObject.objectPos);
	IF_UNLIKELY(!meshID)
		return;

	NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
	const MNM::CNavMesh& navMesh = mesh.navMesh;
	const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();

	const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
		MNM::real_t(paramsGrid.origin.y),
		MNM::real_t(paramsGrid.origin.z));
	const MNM::vector3_t originOffset = origin + MNM::vector3_t(0, 0, MNM::real_t::fraction(725, 10000));

	const Vec3& startLoc = debugObject.entityPos;
	const MNM::vector3_t fixedPointStartLoc(MNM::real_t(startLoc.x), MNM::real_t(startLoc.y), MNM::real_t(startLoc.z));
	const MNM::real_t range = MNM::real_t(5.0f);

	const INavMeshQueryFilter* pDebugQueryFilter = GetDebugQueryFilter("MNMDebugQueryFilter");

	MNM::real_t distance(.0f);
	MNM::vector3_t closestPosition;
	if (MNM::TriangleID closestTriangle = navMesh.GetClosestTriangle(fixedPointStartLoc, range, range, pDebugQueryFilter, &distance, &closestPosition))
	{
		IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		const Vec3 verticalOffset = Vec3(.0f, .0f, .1f);
		const Vec3 endPos(closestPosition.GetVec3() + origin.GetVec3());
		renderAuxGeom->DrawSphere(endPos + verticalOffset, 0.05f, ColorB(Col_Red));
		renderAuxGeom->DrawSphere(fixedPointStartLoc.GetVec3() + verticalOffset, 0.05f, ColorB(Col_Black));

		CDebugDrawContext dc;
		dc->Draw2dLabel(10.0f, 10.0f, 1.3f, Col_White, false,
			"Distance of the ending result position from the original one: %f", distance.as_float());
	}
}

void NavigationSystemDebugDraw::DebugDrawGroundPoint(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
	CAISystem::SObjectDebugParams debugObject;
	if (!GetAISystem()->GetObjectDebugParamsFromName("MNMGroundPoint", debugObject))
		return;
	
	NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObject.objectPos);
	IF_UNLIKELY(!meshID)
		return;

	NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
	const MNM::CNavMesh& navMesh = mesh.navMesh;
	const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();

	const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
		MNM::real_t(paramsGrid.origin.y), MNM::real_t(paramsGrid.origin.z));
	const Vec3& startLoc = debugObject.entityPos;

	const INavMeshQueryFilter* pDebugQueryFilter = GetDebugQueryFilter("MNMDebugQueryFilter");

	Vec3 closestPosition;
	if (navigationSystem.GetClosestMeshLocation(meshID, startLoc, 100.0f, 0.25f, pDebugQueryFilter, &closestPosition, nullptr))
	{
		IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		const Vec3 verticalOffset = Vec3(.0f, .0f, .1f);
		const Vec3 endPos(closestPosition + origin.GetVec3());
		renderAuxGeom->DrawSphere(endPos + verticalOffset, 0.05f, ColorB(Col_Red));
		renderAuxGeom->DrawSphere(startLoc, 0.05f, ColorB(Col_Black));
	}
}

void NavigationSystemDebugDraw::DebugDrawSnapToNavmesh(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
	CAISystem::SObjectDebugParams debugObject;
	if (!GetAISystem()->GetObjectDebugParamsFromName("MNMSnappedPoint", debugObject))
		return;

	SSnapToNavMeshRulesInfo snappingRules;
	snappingRules.bVerticalSearch = true;
	snappingRules.bBoxSearch = true;

	const INavMeshQueryFilter* pDebugQueryFilter = GetDebugQueryFilter("MNMDebugQueryFilter");

	Vec3 snappedPosition;
	MNM::TriangleID triangleId;
	if (navigationSystem.SnapToNavMesh(m_agentTypeID, debugObject.objectPos, pDebugQueryFilter, snappingRules, snappedPosition, &triangleId))
	{
		IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		const Vec3 verticalOffset = Vec3(.0f, .0f, .1f);
		renderAuxGeom->DrawSphere(snappedPosition + verticalOffset, 0.05f, ColorB(Col_Red));
		renderAuxGeom->DrawSphere(debugObject.objectPos + verticalOffset, 0.05f, ColorB(Col_Black));
	}
}

void NavigationSystemDebugDraw::DebugDrawPathFinder(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
	CAISystem::SObjectDebugParams debugObjectStart;
	if (!GetAISystem()->GetObjectDebugParamsFromName("MNMPathStart", debugObjectStart))
		return;

	CAISystem::SObjectDebugParams debugObjectEnd;
	if (!GetAISystem()->GetObjectDebugParamsFromName("MNMPathEnd", debugObjectEnd))
		return;
	
	NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObjectStart.objectPos);
	IF_UNLIKELY(!meshID)
		return;

	NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
	const MNM::CNavMesh& navMesh = mesh.navMesh;
	const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();

	const OffMeshNavigationManager* offMeshNavigationManager = navigationSystem.GetOffMeshNavigationManager();
	assert(offMeshNavigationManager);
	const MNM::OffMeshNavigation& offMeshNavigation = offMeshNavigationManager->GetOffMeshNavigationForMesh(meshID);

	const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
		MNM::real_t(paramsGrid.origin.y),
		MNM::real_t(paramsGrid.origin.z));
	const bool bOffsetTriangleUp = false;
	const MNM::vector3_t originOffset = origin + MNM::vector3_t(0, 0, MNM::real_t::fraction(725, 10000));

	auto drawTriangle = [originOffset, bOffsetTriangleUp](IRenderAuxGeom* renderAuxGeom, const MNM::vector3_t& a, const MNM::vector3_t& b, const MNM::vector3_t& c, const ColorB& color)
	{
		Vec3 va, vb, vc;
		if (bOffsetTriangleUp)
		{
			va = (a + originOffset).GetVec3();
			vb = (b + originOffset).GetVec3();
			vc = (c + originOffset).GetVec3();
		}
		else
		{
			const Vec3 vao = a.GetVec3();
			const Vec3 vbo = b.GetVec3();
			const Vec3 vco = c.GetVec3();

			Triangle t(vao, vbo, vco);
			const Vec3 n = t.GetNormal() * 0.07f;

			va = vao + n;
			vb = vbo + n;
			vc = vco + n;
		}
		renderAuxGeom->DrawTriangle(va, color, vb, color, vc, color);
	};

	auto drawPath = [](IRenderAuxGeom* pRenderAuxGeom, const CPathHolder<PathPointDescriptor>& path, const ColorB& color, const Vec3& offset)
	{
		const size_t pathSize = path.Size();
		if (pathSize > 0)
		{
			const float radius = 0.015f;

			for (size_t j = 0; j < pathSize - 1; ++j)
			{
				const Vec3 start = path.At(j);
				const Vec3 end = path.At(j + 1);
				pRenderAuxGeom->DrawLine(start + offset, color, end + offset, color, 4.0f);
				pRenderAuxGeom->DrawSphere(start + offset, radius, color);
			}

			pRenderAuxGeom->DrawSphere(path.At(pathSize - 1) + offset, radius, color);
		}
	};

	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	const Vec3& startLoc = debugObjectStart.entityPos;
	const Vec3& endLoc = debugObjectEnd.objectPos;

	const MNM::real_t hrange = MNM::real_t(1.0f);
	const MNM::real_t vrange = MNM::real_t(1.0f);

	const INavMeshQueryFilter* pDebugQueryFilter = GetDebugQueryFilter("MNMDebugQueryFilter");

	MNM::vector3_t fixedPointStartLoc;
	const MNM::TriangleID triStart = navMesh.GetClosestTriangle(
		MNM::vector3_t(startLoc) - origin, vrange, hrange, pDebugQueryFilter, nullptr, &fixedPointStartLoc);
	//fixedPointStartLoc += origin;

	if (triStart)
	{
		MNM::vector3_t a, b, c;
		navMesh.GetVertices(triStart, a, b, c);

		drawTriangle(renderAuxGeom, a, b, c, ColorB(ColorF(Col_GreenYellow, 0.5f)));
	}

	MNM::vector3_t fixedPointEndLoc;
	const MNM::TriangleID triEnd = navMesh.GetClosestTriangle(
		MNM::vector3_t(endLoc) - origin, vrange, hrange, pDebugQueryFilter, nullptr, &fixedPointEndLoc);

	if (triEnd)
	{
		MNM::vector3_t a, b, c;
		navMesh.GetVertices(triEnd, a, b, c);

		drawTriangle(renderAuxGeom, a, b, c, ColorB(Col_MidnightBlue));
	}

	CTimeValue timeTotal(0ll);
	CTimeValue stringPullingTotalTime(0ll);
	float totalPathLength = 0;
	if (triStart && triEnd)
	{
		const MNM::vector3_t startToEnd = (fixedPointStartLoc - fixedPointEndLoc);
		const MNM::real_t startToEndDist = startToEnd.lenNoOverflow();
		MNM::CNavMesh::WayQueryWorkingSet workingSet;
		workingSet.aStarOpenList.SetFrameTimeQuota(0.0f);
		workingSet.aStarOpenList.SetUpForPathSolving(navMesh.GetTriangleCount(), triStart, fixedPointStartLoc, startToEndDist);

		CTimeValue timeStart = gEnv->pTimer->GetAsyncTime();

		MNM::DangerousAreasList dangersInfo;
		MNM::DangerAreaConstPtr info;
		const Vec3& cameraPos = gAIEnv.GetDebugRenderer()->GetCameraPos(); // To simulate the player position and evaluate the path generation
		info.reset(new MNM::DangerAreaT<MNM::eWCT_Direction>(cameraPos, 0.0f, gAIEnv.CVars.PathfinderDangerCostForAttentionTarget));
		dangersInfo.push_back(info);

		// This object is used to simulate the explosive threat and debug draw the behavior of the pathfinding
		CAISystem::SObjectDebugParams debugObjectExplosiveThreat;
		if (GetAISystem()->GetObjectDebugParamsFromName("MNMPathExplosiveThreat", debugObjectExplosiveThreat))
		{
			info.reset(new MNM::DangerAreaT<MNM::eWCT_Range>(debugObjectExplosiveThreat.objectPos,
				gAIEnv.CVars.PathfinderExplosiveDangerRadius, gAIEnv.CVars.PathfinderDangerCostForExplosives));
			dangersInfo.push_back(info);
		}

		const size_t k_MaxWaySize = 512;
		const float pathSharingPenalty = .0f;
		const float pathLinkSharingPenalty = .0f;

		MNM::CNavMesh::WayQueryRequest inputParams(
			debugObjectStart.entityId,
			triStart, startLoc, triEnd, endLoc,
			offMeshNavigation, *offMeshNavigationManager, 
			dangersInfo, pDebugQueryFilter, MNMCustomPathCostComputerSharedPtr());  // no custom cost-computer (where should we get it from!?));

		MNM::CNavMesh::WayQueryResult result(k_MaxWaySize);

		const bool hasPathfindingFinished = (navMesh.FindWay(inputParams, workingSet, result) == MNM::CNavMesh::eWQR_Done);

		CTimeValue timeEnd = gEnv->pTimer->GetAsyncTime();
		timeTotal = timeEnd - timeStart;

		assert(hasPathfindingFinished);

		const MNM::WayTriangleData* const pOutputWay = result.GetWayData();
		const size_t outputWaySize = result.GetWaySize();

		for (size_t i = 0; i < outputWaySize; ++i)
		{
			if ((pOutputWay[i].triangleID != triStart) && (pOutputWay[i].triangleID != triEnd))
			{
				MNM::vector3_t a, b, c;

				navMesh.GetVertices(pOutputWay[i].triangleID, a, b, c);

				drawTriangle(renderAuxGeom, a, b, c, ColorB(ColorF(Col_Maroon, 0.5f)));
			}
		}

		const bool bPathFound = (result.GetWaySize() != 0);
		if (bPathFound)
		{
			CPathHolder<PathPointDescriptor> outputPath;
			if (CMNMPathfinder::ConstructPathFromFoundWay(result, navMesh, offMeshNavigationManager, fixedPointStartLoc.GetVec3(), fixedPointEndLoc.GetVec3(), *&outputPath))
			{
				const Vec3 pathVerticalOffset = Vec3(.0f, .0f, .1f);
				drawPath(renderAuxGeom, outputPath, Col_Gray, pathVerticalOffset);

				const bool bBeautifyPath = (gAIEnv.CVars.BeautifyPath != 0);
				CTimeValue stringPullingStartTime = gEnv->pTimer->GetAsyncTime();
				if (bBeautifyPath)
				{
					outputPath.PullPathOnNavigationMesh(navMesh, gAIEnv.CVars.PathStringPullingIterations, nullptr);
				}
				stringPullingTotalTime = gEnv->pTimer->GetAsyncTime() - stringPullingStartTime;

				if (bBeautifyPath)
				{
					drawPath(renderAuxGeom, outputPath, Col_Black, pathVerticalOffset);
				}

				const size_t pathSize = outputPath.Size();
				for (size_t j = 0; pathSize > 0 && j < pathSize - 1; ++j)
				{
					const Vec3 start = outputPath.At(j);
					const Vec3 end = outputPath.At(j + 1);
					totalPathLength += Distance::Point_Point(start, end);
				}
			}
		}
	}

	const stack_string predictionName = gAIEnv.CVars.MNMPathfinderPositionInTrianglePredictionType ? "Advanced prediction" : "Triangle Center";

	CDebugDrawContext dc;

	dc->Draw2dLabel(10.0f, 172.0f, 1.3f, Col_White, false,
		"Start: %08x  -  End: %08x - Total Pathfinding time: %.4fms -- Type of prediction for the point inside each triangle: %s", triStart, triEnd, timeTotal.GetMilliSeconds(), predictionName.c_str());
	dc->Draw2dLabel(10.0f, 184.0f, 1.3f, Col_White, false,
		"String pulling operation - Iteration %d  -  Total time: %.4fms -- Total Length: %f", gAIEnv.CVars.PathStringPullingIterations, stringPullingTotalTime.GetMilliSeconds(), totalPathLength);
}

static bool FindObjectToTestIslandConnectivity(const char* szName, Vec3& outPos, IEntity** ppOutEntityToTestOffGridLinks)
{
	CAISystem::SObjectDebugParams debugObjectParams;
	if (GetAISystem()->GetObjectDebugParamsFromName(szName, debugObjectParams))
	{
		outPos = debugObjectParams.objectPos;

		if (ppOutEntityToTestOffGridLinks)
		{
			*ppOutEntityToTestOffGridLinks = gEnv->pEntitySystem->GetEntity(debugObjectParams.entityId);
		}
		return true;
	}
	else if (IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(szName))
	{
		outPos = pEntity->GetWorldPos();

		if (ppOutEntityToTestOffGridLinks)
		{
			*ppOutEntityToTestOffGridLinks = pEntity;
		}
		return true;
	}
	return false;
}

void NavigationSystemDebugDraw::DebugDrawIslandConnection(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
	// NOTE: difference between two possible start entities:
	// - "MNMIslandStart" is used during island connectivity check to traverse OffNavMesh links;
	// - "MNMIslandStartAnyLink" is not used during connectivity check, so any OffNavMesh link is considered to be traversable all the time.
	const char* szStartName = "MNMIslandStart";
	const char* szStartNameAnyLink = "MNMIslandStartAnyLink";
	const char* szEndName = "MNMIslandEnd";

	Vec3 startPos;
	Vec3 endPos;
	IEntity* pEntityToTestOffGridLinksOrNull = nullptr;

	if (!FindObjectToTestIslandConnectivity(szStartName, startPos, &pEntityToTestOffGridLinksOrNull))
	{
		if (!FindObjectToTestIslandConnectivity(szStartNameAnyLink, startPos, nullptr))
		{
			return;
		}
	}

	if (!FindObjectToTestIslandConnectivity(szEndName, endPos, nullptr))
	{
		return;
	}

	const INavMeshQueryFilter* pDebugQueryFilter = GetDebugQueryFilter("MNMDebugQueryFilter");

	const bool isReachable = gAIEnv.pNavigationSystem->IsPointReachableFromPosition(m_agentTypeID, pEntityToTestOffGridLinksOrNull, startPos, endPos, pDebugQueryFilter);

	CDebugDrawContext dc;
	dc->Draw2dLabel(10.0f, 250.0f, 1.6f, isReachable ? Col_ForestGreen : Col_VioletRed, false, isReachable ? "The two islands ARE connected" : "The two islands ARE NOT connected");
}

struct DefaultTriangleColorSelector : public MNM::ITriangleColorSelector
{
	DefaultTriangleColorSelector(NavigationSystem& navigationSystem, const char* szFlagToDraw) 
		: m_navigationSystem(navigationSystem)
	{
		const MNM::CAnnotationsLibrary& annotationsLib = navigationSystem.GetAnnotations();
		NavigationAreaFlagID flagId = annotationsLib.GetAreaFlagID(szFlagToDraw);
		const MNM::SAreaFlag* pFlag = annotationsLib.GetAreaFlag(flagId);
		if (pFlag)
		{
			m_flagsToDrawMask = pFlag->value;
		}
	}
	
	virtual ColorB GetAnnotationColor(MNM::AreaAnnotation annotation) const
	{
		const MNM::CAnnotationsLibrary& annotationsLib = m_navigationSystem.GetAnnotations();

		MNM::AreaAnnotation::value_type flags = annotation.GetFlags();
		MNM::AreaAnnotation::value_type flagsToDraw = flags & m_flagsToDrawMask;
		ColorB color;
		if (annotationsLib.GetFirstFlagColor(flagsToDraw, color))
		{
			return color;
		}
		annotationsLib.GetAreaColor(annotation, color);
		return color;
	};

	NavigationSystem& m_navigationSystem;
	MNM::AreaAnnotation::value_type m_flagsToDrawMask = 0;
};

void NavigationSystemDebugDraw::DebugDrawNavigationMeshesForSelectedAgent(NavigationSystem& navigationSystem, MNM::TileID excludeTileID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	DefaultTriangleColorSelector colorSelector(navigationSystem, gAIEnv.CVars.MNMDebugDrawFlag);
	
	AgentType& agentType = navigationSystem.m_agentTypes[m_agentTypeID - 1];
	AgentType::Meshes::const_iterator it = agentType.meshes.begin();
	AgentType::Meshes::const_iterator end = agentType.meshes.end();

	const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();

	for (; it != end; ++it)
	{
		const NavigationMesh& mesh = navigationSystem.GetMesh(it->id);

		if(!viewCamera.IsAABBVisible_F(navigationSystem.m_volumes[mesh.boundary].aabb))
			continue;

		size_t drawFlag = MNM::STile::DrawTriangles | MNM::STile::DrawMeshBoundaries;
		if (gAIEnv.CVars.MNMDebugAccessibility)
			drawFlag |= MNM::STile::DrawAccessibility;

		switch (gAIEnv.CVars.DebugDrawNavigation)
		{
		case 0:
		case 1:
			mesh.navMesh.Draw(drawFlag, colorSelector, excludeTileID);
			break;
		case 2:
			mesh.navMesh.Draw(drawFlag | MNM::STile::DrawInternalLinks, colorSelector, excludeTileID);
			break;
		case 3:
			mesh.navMesh.Draw(drawFlag | MNM::STile::DrawInternalLinks |
			                  MNM::STile::DrawExternalLinks | MNM::STile::DrawOffMeshLinks, colorSelector, excludeTileID);
			break;
		case 4:
			mesh.navMesh.Draw(drawFlag | MNM::STile::DrawInternalLinks |
			                  MNM::STile::DrawExternalLinks | MNM::STile::DrawOffMeshLinks |
			                  MNM::STile::DrawTrianglesId, colorSelector, excludeTileID);
			break;
		case 5:
			mesh.navMesh.Draw(drawFlag | MNM::STile::DrawInternalLinks | MNM::STile::DrawExternalLinks |
			                  MNM::STile::DrawOffMeshLinks | MNM::STile::DrawTrianglesId | MNM::STile::DrawIslandsId, colorSelector, excludeTileID);
			break;
		case 6:
			mesh.navMesh.Draw(drawFlag | MNM::STile::DrawInternalLinks |
			                  MNM::STile::DrawExternalLinks | MNM::STile::DrawOffMeshLinks | MNM::STile::DrawTriangleBackfaces, colorSelector, excludeTileID);
			break;

		default:
			break;
		}
	}
}

void NavigationSystemDebugDraw::DebugDrawTriangleOnCursor(NavigationSystem& navigationSystem)
{
	if (!gAIEnv.CVars.DebugTriangleOnCursor)
		return;

	CDebugDrawContext dc;
	float yPos = gEnv->pRenderer->GetHeight() - 100.0f;
	const float textSize = 1.2f;
	const float xPos = 200.0f;

	dc->Draw2dLabel(xPos - 2.0f, yPos, 1.6f, Col_White, false, "NavMesh Triangle Info");
	yPos += 20.0f;
	
	const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
	
	Vec3 rayStartPos = viewCamera.GetPosition();
	Vec3 rayDir = viewCamera.GetViewdir();

	if (gEnv->IsEditing())
	{
		float mouseX, mouseY;
		gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);

		// Invert mouse Y
		mouseY = gEnv->pRenderer->GetHeight() - mouseY;

		Vec3 pos0(0.0f);
		Vec3 pos1(0.0f);
		gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 0, &pos0.x, &pos0.y, &pos0.z);
		gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 1, &pos1.x, &pos1.y, &pos1.z);

		rayStartPos = pos0;
		rayDir = pos1 - pos0;
	}
	rayDir.SetLength(200);
	
	ray_hit hit;
	if (gAIEnv.pWorld->RayWorldIntersection(rayStartPos, rayDir, ent_static | ent_terrain | ent_sleeping_rigid, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1))
	{
		if (NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, hit.pt))
		{
			if (MNM::TriangleID triangleID = navigationSystem.GetClosestMeshLocation(meshID, hit.pt, 1.0f, 1.0f, nullptr, nullptr, nullptr))
			{
				const MNM::CNavMesh& navMesh = navigationSystem.GetMesh(meshID).navMesh;
				const MNM::vector3_t tileCoords = navMesh.GetTileContainerCoordinates(MNM::ComputeTileID(triangleID));
				dc->Draw2dLabel(xPos, yPos, textSize, Col_White, false, "Tile Coords: x %d, y %d, z %d", tileCoords.x.as_int(), tileCoords.y.as_int(), tileCoords.z.as_int());
				yPos += 15.0f;
				dc->Draw2dLabel(xPos, yPos, textSize, Col_White, false, "Triangle ID: %u", triangleID);
				yPos += 15.0f;

				MNM::Tile::STriangle triangleData;
				if (navMesh.GetTriangle(triangleID, triangleData))
				{
					const MNM::CAnnotationsLibrary& annotationsLib = navigationSystem.GetAnnotations();
					
					const MNM::SAreaType* pAreaType = annotationsLib.GetAreaType(triangleData.areaAnnotation.GetType());
					if (pAreaType)
					{
						dc->Draw2dLabel(xPos, yPos, textSize, Col_White, false, "Area Type: '%s'", pAreaType->name.c_str());
						yPos += 15.0f;
					}
					else
					{
						dc->Draw2dLabel(xPos, yPos, textSize, Col_White, false, "Area Type: -- Not defined --");
						yPos += 15.0f;
					}

					string flagNames;
					const MNM::AreaAnnotation::value_type triFlags = triangleData.areaAnnotation.GetFlags();
					for (size_t i = 0, count = annotationsLib.GetAreaFlagCount(); i < count; ++i)
					{
						const MNM::SAreaFlag* pAreaFlag = annotationsLib.GetAreaFlag(i);
						if (pAreaFlag->value & triFlags)
						{
							if (!flagNames.IsEmpty())
							{
								flagNames.Append(", ");
							}
							flagNames.Append("'");
							flagNames.Append(pAreaFlag->name);
							flagNames.Append("'");
						}
					}
					if (flagNames.IsEmpty())
					{
						flagNames = "---";
					}
					dc->Draw2dLabel(xPos, yPos, textSize, Col_White, false, "Area Flags: %s", flagNames.c_str());
					yPos += 15.0f;

					MNM::vector3_t mnmVertices[3];
					if (navMesh.GetVertices(triangleID, mnmVertices))
					{
						const Vec3 offset(0.0f, 0.0f, 0.055f);
						const Vec3 offsetMid(0.0f, 0.0f, 0.005f);
						IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();

						Vec3 vertices[3];
						vertices[0] = mnmVertices[0].GetVec3() + offset;
						vertices[1] = mnmVertices[1].GetVec3() + offset;
						vertices[2] = mnmVertices[2].GetVec3() + offset;

						ColorB color;
						navigationSystem.GetAnnotations().GetAreaColor(triangleData.areaAnnotation, color);

						renderAuxGeom.DrawPolyline(vertices, 3, true, color, 8.0f);

						vertices[0] += offsetMid;
						vertices[1] += offsetMid;
						vertices[2] += offsetMid;
						renderAuxGeom.DrawPolyline(vertices, 3, true, Col_White, 2.0f);
					}
				}
				return;
			}
		}
	}
	
	dc->Draw2dLabel(xPos, yPos, textSize, Col_White, false, "---");
	yPos += 15.0f;
}

void NavigationSystemDebugDraw::DebugDrawNavigationSystemState(NavigationSystem& navigationSystem)
{
	CDebugDrawContext dc;
	
	if (gAIEnv.CVars.DebugDrawNavigation)
	{
		switch (navigationSystem.m_state)
		{
		case NavigationSystem::Working:
			dc->Draw2dLabel(10.0f, 300.0f, 1.6f, Col_Yellow, false, "Navigation System Working");
			dc->Draw2dLabel(10.0f, 322.0f, 1.2f, Col_White, false, "Processing: %d\nRemaining: %d\nThroughput: %.2f/s\n"
				"Cache Hits: %.2f/s",
				navigationSystem.m_runningTasks.size(), navigationSystem.GetWorkingQueueSize(), navigationSystem.m_throughput, navigationSystem.m_cacheHitRate);
			break;
		case NavigationSystem::Idle:
			dc->Draw2dLabel(10.0f, 300.0f, 1.6f, Col_ForestGreen, false, "Navigation System Idle");
			break;
		default:
			assert(0);
			break;
		}
		static_cast<CMNMUpdatesManager*>(navigationSystem.GetUpdateManager())->DebugDraw();
	}

	// Draw annotations legend
	auto DrawLabel = [&dc](float posX, float posY, const Vec2& size, const ColorF& color, float fontSize, const char* szLabel)
	{
		dc->Draw2dImage(posX - 1.0f, posY - 1.0f, size.x + 2.0f, size.y + 2.0f, -1, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
		dc->Draw2dImage(posX, posY, size.x, size.y, -1, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, color.r, color.g, color.b, 1.0f);
		dc->Draw2dLabel(posX + size.x + 5.0f, posY, fontSize, Col_White, false, szLabel);
	};

	const MNM::CAnnotationsLibrary& annotationsLib = navigationSystem.GetAnnotations();

	const size_t areaTypesCount = annotationsLib.GetAreaTypeCount();
	const float lineHeight = 25.0f;
	const float yPos = gEnv->pRenderer->GetHeight() - float(areaTypesCount + 1) * lineHeight - 20.0f;

	dc->Draw2dLabel(10.0f, yPos, 1.6f, Col_White, false, "Area Types");

	ColorB color;
	MNM::AreaAnnotation annotation;
	
	for (size_t i = 0; i < areaTypesCount; ++i)
	{
		const MNM::SAreaType* pAreaType = annotationsLib.GetAreaType(i);
		annotation.SetType(pAreaType->id);

		annotationsLib.GetAreaColor(annotation, color);

		DrawLabel(10.0f, yPos + lineHeight * (i + 1), Vec2(13.0f), color, 1.3f, pAreaType->name.c_str());
	}
}

void NavigationSystemDebugDraw::DebugDrawMemoryStats(NavigationSystem& navigationSystem)
{
	if (gAIEnv.CVars.MNMProfileMemory)
	{
		const float kbInvert = 1.0f / 1024.0f;

		const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const float grey[4] = { 0.65f, 0.65f, 0.65f, 1.0f };

		float posY = 60.f;
		float posX = 30.f;

		size_t totalNavigationSystemMemory = 0;

		ICrySizer* pSizer = gEnv->pSystem->CreateSizer();

		for (uint16 i = 0; i < navigationSystem.m_meshes.capacity(); ++i)
		{
			if (!navigationSystem.m_meshes.index_free(i))
			{
				const NavigationMeshID meshID(navigationSystem.m_meshes.get_index_id(i));

				const NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
				const MNM::OffMeshNavigation& offMeshNavigation = navigationSystem.GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshID);
				const AgentType& agentType = navigationSystem.m_agentTypes[mesh.agentTypeID - 1];

				const NavigationMesh::ProfileMemoryStats meshMemStats = mesh.GetMemoryStats(pSizer);
				const MNM::OffMeshNavigation::ProfileMemoryStats offMeshMemStats = offMeshNavigation.GetMemoryStats(pSizer);
				const OffMeshNavigationManager::ProfileMemoryStats linkMemStats = navigationSystem.GetOffMeshNavigationManager()->GetMemoryStats(pSizer, meshID);

				size_t totalMemory = meshMemStats.totalNavigationMeshMemory + offMeshMemStats.totalSize + linkMemStats.totalSize;

				IRenderAuxText::Draw2dLabel(posX, posY, 1.3f, white, false, "Mesh: %s Agent: %s - Total Memory %.3f KB : Mesh %.3f KB / Grid %.3f KB / OffMesh %.3f",
				                            mesh.name.c_str(), agentType.name.c_str(),
				                            totalMemory * kbInvert, meshMemStats.totalNavigationMeshMemory * kbInvert, meshMemStats.navMeshProfiler.GetMemoryUsage() * kbInvert, (offMeshMemStats.totalSize + linkMemStats.totalSize) * kbInvert);
				posY += 12.0f;
				IRenderAuxText::Draw2dLabel(posX, posY, 1.3f, grey, false, "Tiles [%d] / Vertices [%d] - %.3f KB / Triangles [%d] - %.3f KB / Links [%d] - %.3f KB / BVNodes [%d] - %.3f KB",
				                            meshMemStats.navMeshProfiler[MNM::CNavMesh::TileCount],
				                            meshMemStats.navMeshProfiler[MNM::CNavMesh::VertexCount], meshMemStats.navMeshProfiler[MNM::CNavMesh::VertexMemory].used * kbInvert,
				                            meshMemStats.navMeshProfiler[MNM::CNavMesh::TriangleCount], meshMemStats.navMeshProfiler[MNM::CNavMesh::TriangleMemory].used * kbInvert,
				                            meshMemStats.navMeshProfiler[MNM::CNavMesh::LinkCount], meshMemStats.navMeshProfiler[MNM::CNavMesh::LinkMemory].used * kbInvert,
				                            meshMemStats.navMeshProfiler[MNM::CNavMesh::BVTreeNodeCount], meshMemStats.navMeshProfiler[MNM::CNavMesh::BVTreeMemory].used * kbInvert
				                            );

				posY += 12.0f;
				IRenderAuxText::Draw2dLabel(posX, posY, 1.3f, grey, false, "OffMesh Memory : Tile Links %.3f KB / Object Info %.3f KB",
				                            offMeshMemStats.offMeshTileLinksMemory * kbInvert,
				                            linkMemStats.linkInfoSize * kbInvert
				                            );
				posY += 13.0f;

				totalNavigationSystemMemory += totalMemory;
			}
		}

		//TODO: Add Navigation system itself (internal containers and others)

		IRenderAuxText::Draw2dLabel(40.0f, 20.0f, 1.5f, white, false, "Navigation System: %.3f KB", totalNavigationSystemMemory * kbInvert);

		pSizer->Release();
	}
}

const INavMeshQueryFilter* NavigationSystemDebugDraw::GetDebugQueryFilter(const char* szName) const
{
	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(szName);
	if (!pEntity)
		return nullptr;

	CEntityAINavigationComponent* pNavigationComponent = pEntity->GetComponent<CEntityAINavigationComponent>();
	if (!pNavigationComponent)
		return nullptr;

	return pNavigationComponent->GetNavigationQueryFilter();
}

NavigationSystemDebugDraw::DebugDrawSettings NavigationSystemDebugDraw::GetDebugDrawSettings(NavigationSystem& navigationSystem)
{
	static Vec3 lastLocation(ZERO);

	static size_t selectedX = 0;
	static size_t selectedY = 0;
	static size_t selectedZ = 0;

	DebugDrawSettings settings;
	settings.forceGeneration = false;

	CAISystem::SObjectDebugParams MNMDebugLocatorParams;
	if (GetAISystem()->GetObjectDebugParamsFromName("MNMDebugLocator", MNMDebugLocatorParams))
	{
		const Vec3& debugLocation = MNMDebugLocatorParams.objectPos;
		if ((lastLocation - debugLocation).len2() > 0.00001f)
		{
			settings.meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugLocation);

			if (settings.meshID)
			{
				const NavigationMesh& mesh = navigationSystem.GetMesh(settings.meshID);
				const MNM::CNavMesh::SGridParams& params = mesh.navMesh.GetGridParams();

				size_t x = (size_t)((debugLocation.x - params.origin.x) / (float)params.tileSize.x);
				size_t y = (size_t)((debugLocation.y - params.origin.y) / (float)params.tileSize.y);
				size_t z = (size_t)((debugLocation.z - params.origin.z) / (float)params.tileSize.z);

				if ((x != selectedX) || (y != selectedY) || (z != selectedZ))
				{
					settings.forceGeneration = true;

					selectedX = x;
					selectedY = y;
					selectedZ = z;
				}
				lastLocation = debugLocation;
			}
			else
			{
				lastLocation.zero();
			}
		}
	}
	else
	{
		lastLocation.zero();
	}

	settings.selectedX = selectedX;
	settings.selectedY = selectedY;
	settings.selectedZ = selectedZ;

	if (!settings.meshID)
	{
		settings.meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, lastLocation);
	}

	return settings;
}

//////////////////////////////////////////////////////////////////////////

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::Update(const float frameTime, const size_t queueSize)
{
	m_currentQueueSize = queueSize;
	m_initialQueueSize = (queueSize > 0) ? max(m_initialQueueSize, queueSize) : 0;

	const float updateTime = (queueSize > 0) ? frameTime : -2.0f * frameTime;
	m_timeUpdating = clamp_tpl(m_timeUpdating + updateTime, 0.0f, 1.0f);
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::Draw()
{
	const bool draw = (m_timeUpdating > 0.0f);

	if (!draw)
		return;

	BeginDraw();

	IRenderAuxGeom *pAux = gEnv->pRenderer->GetIRenderAuxGeom();
	const CCamera& rCamera = pAux->GetCamera();

	const float width  = (float)rCamera.GetViewSurfaceX();
	const float height = (float)rCamera.GetViewSurfaceZ();

	const ColorB backGroundColor(0, 255, 0, CLAMP((int)(0.35f * m_timeUpdating * 255.0f), 0, 255));
	const ColorB progressColor(0, 255, 0, CLAMP((int)(0.8f * m_timeUpdating * 255.0f), 0, 255));

	const float progressFraction = (m_initialQueueSize > 0) ? clamp_tpl(1.0f - ((float)m_currentQueueSize / (float)m_initialQueueSize), 0.0f, 1.0f) : 1.0f;

	Vec2 progressBarLocation(0.1f * width, 0.91f * height);
	Vec2 progressBarSize(0.2f * width, 0.025f * height);

	const float white[4] = { 1.0f, 1.0f, 1.0f, 0.85f * m_timeUpdating };

	IRenderAuxText::Draw2dLabel(progressBarLocation.x, progressBarLocation.y - 18.0f, 1.4f, white, false, "Processing Navigation Meshes");

	DrawQuad(progressBarLocation, progressBarSize, backGroundColor);
	DrawQuad(progressBarLocation, Vec2(progressBarSize.x * progressFraction, progressBarSize.y), progressColor);

	EndDraw();
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::BeginDraw()
{
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAux)
	{
		m_oldRenderFlags = pRenderAux->GetRenderFlags();

		SAuxGeomRenderFlags newFlags = e_Def2DPublicRenderflags;
		newFlags.SetAlphaBlendMode(e_AlphaBlended);

		pRenderAux->SetRenderFlags(newFlags);
	}
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::EndDraw()
{
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAux)
	{
		pRenderAux->SetRenderFlags(m_oldRenderFlags);
	}
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::DrawQuad(const Vec2& origin, const Vec2& size, const ColorB& color)
{
	if (IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom())
	{
		Vec3 quadVertices[4];
		const vtx_idx auxIndices[6] = { 2, 1, 0, 2, 3, 1 };

		quadVertices[0] = Vec3(origin.x, origin.y, 1.0f);
		quadVertices[1] = Vec3(origin.x + size.x, origin.y, 1.0f);
		quadVertices[2] = Vec3(origin.x, origin.y + size.y, 1.0f);
		quadVertices[3] = Vec3(origin.x + size.x, origin.y + size.y, 1.0f);

		pRenderAux->DrawTriangles(quadVertices, 4, auxIndices, 6, color);
	}
}

//////////////////////////////////////////////////////////////////////////

NavigationMesh::ProfileMemoryStats NavigationMesh::GetMemoryStats(ICrySizer* pSizer) const
{
	ProfileMemoryStats memoryStats(navMesh.GetProfiler());

	size_t initialSize = pSizer->GetTotalSize();
	{
		pSizer->AddObjectSize(this);
		pSizer->AddObject(&navMesh, memoryStats.navMeshProfiler.GetMemoryUsage());
		pSizer->AddContainer(exclusions);
		pSizer->AddString(name);

		memoryStats.totalNavigationMeshMemory = pSizer->GetTotalSize() - initialSize;
	}

	return memoryStats;
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if NAVIGATION_SYSTEM_EDITOR_BACKGROUND_UPDATE

void NavigationSystemBackgroundUpdate::Thread::ThreadEntry()
{
	while (!m_requestedStop)
	{
		if (m_navigationSystem.GetState() == INavigationSystem::Working)
		{
			const CTimeValue startedUpdate = gEnv->pTimer->GetAsyncTime();

			m_navigationSystem.UpdateMeshes(0.0333f, false, true, true);

			const CTimeValue lastUpdateTime = gEnv->pTimer->GetAsyncTime() - startedUpdate;

			const unsigned int sleepTime = max(10u, min(0u, 33u - (unsigned int)lastUpdateTime.GetMilliSeconds()));

			CrySleep(sleepTime);
		}
		else
		{
			CrySleep(50);
		}
	}
}

void NavigationSystemBackgroundUpdate::Thread::SignalStopWork()
{
	m_requestedStop = true;
}

//////////////////////////////////////////////////////////////////////////

bool NavigationSystemBackgroundUpdate::Start()
{
	if (m_pBackgroundThread == NULL)
	{
		m_pBackgroundThread = new Thread(m_navigationSystem);
		if (!gEnv->pThreadManager->SpawnThread(m_pBackgroundThread, "NavigationSystemBackgroundUpdate"))
		{
			CRY_ASSERT_MESSAGE(false, "Error spawning \"NavigationSystemBackgroundUpdate\" thread.");
			delete m_pBackgroundThread;
			m_pBackgroundThread = NULL;
			return false;
		}

		return true;
	}

	return false;
}

bool NavigationSystemBackgroundUpdate::Stop()
{
	if (m_pBackgroundThread != NULL)
	{
		m_pBackgroundThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(m_pBackgroundThread, eJM_Join);

		delete m_pBackgroundThread;
		m_pBackgroundThread = NULL;

		return true;
	}

	return false;
}

void NavigationSystemBackgroundUpdate::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	CRY_ASSERT(gEnv->IsEditor());

	if (event == ESYSTEM_EVENT_LEVEL_UNLOAD ||
	    event == ESYSTEM_EVENT_LEVEL_LOAD_START)
	{
		Pause(true);
	}
	else if (event == ESYSTEM_EVENT_LEVEL_LOAD_END)
	{
		Pause(false);
	}
	else if (event == ESYSTEM_EVENT_CHANGE_FOCUS)
	{
		// wparam != 0 is focused, wparam == 0 is not focused
		const bool startBackGroundUpdate = (wparam == 0) && (gAIEnv.CVars.MNMEditorBackgroundUpdate != 0) && (m_navigationSystem.GetState() == INavigationSystem::Working) && !m_paused;

		if (startBackGroundUpdate)
		{
			if (Start())
			{
				CryLog("NavMesh generation background thread started");
			}
		}
		else
		{
			if (Stop())
			{
				CryLog("NavMesh generation background thread stopped");
			}
		}
	}
}

void NavigationSystemBackgroundUpdate::RegisterAsSystemListener()
{
	if (IsEnabled())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "NavigationSystemBackgroundUpdate");
	}
}

void NavigationSystemBackgroundUpdate::RemoveAsSystemListener()
{
	if (IsEnabled())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}
}

#endif