// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverSystem.h"
#include "CoverSampler.h"
#include "CoverSurface.h"
#include "CoverUser.h" // TODO: remove
#include "PipeUser.h"

#include "DebugDrawContext.h"

const uint32 BAI_COVER_FILE_VERSION_READ = 2;
const uint16 MAX_CACHED_COVERS = 4096;

struct CoverSystemPhysListener
{
	static int RemoveEntityParts(const EventPhys* event)
	{
		const EventPhysRemoveEntityParts* removeEvent = static_cast<const EventPhysRemoveEntityParts*>(event);

		pe_status_pos pos;

		// TODO: This is currently brute-forced to the size of the whole entity
		// Need to find a way to localize it only near the place the break happened
		if (removeEvent->pEntity->GetStatus(&pos))
		{
			Vec3 center = pos.pos + (pos.BBox[0] + pos.BBox[1]) * 0.5f;
			float radius = (pos.BBox[0] - pos.BBox[1]).len();

			//GetAISystem()->AddDebugSphere(pos.pos, 0.25f, 128, 64, 192, 5);
			//GetAISystem()->AddDebugSphere(pos.pos, radius, 64, 32, 128, 5);

			gAIEnv.pCoverSystem->BreakEvent(center, radius);
		}

		return 1;
	}
};

CCoverSystem::CCoverSystem(const char* configFileName)
	: m_configName(configFileName)
{
	ReloadConfig();
	ClearAndReserveCoverLocationCache();

	gEnv->pPhysicalWorld->AddEventClient(EventPhysRemoveEntityParts::id, CoverSystemPhysListener::RemoveEntityParts, true,
	                                     FLT_MAX);
}

CCoverSystem::~CCoverSystem()
{
	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysRemoveEntityParts::id, CoverSystemPhysListener::RemoveEntityParts, true);
}

ICoverUser* CCoverSystem::RegisterEntity(const EntityId entityId, const ICoverUser::Params& params)
{
	auto findIt = m_coverUsers.find(entityId);
	if (findIt != m_coverUsers.end())
	{
		findIt->second->SetParams(params);
		return findIt->second.get();
	}

	auto ret = m_coverUsers.insert(std::make_pair(entityId, std::make_shared<CoverUser>(params)));
	return ret.first->second.get();
}

void CCoverSystem::UnregisterEntity(const EntityId entityId)
{
	m_coverUsers.erase(entityId);
}

ICoverUser* CCoverSystem::GetRegisteredCoverUser(const EntityId entityId) const
{
	auto findIt = m_coverUsers.find(entityId);
	if (findIt != m_coverUsers.end())
	{
		return findIt->second.get();
	}
	return nullptr;
}

ICoverSampler* CCoverSystem::CreateCoverSampler(const char* samplerName)
{
	if (!stricmp(samplerName, "default"))
		return new CoverSampler();

	return 0;
}

bool CCoverSystem::ReloadConfig()
{
	const char* fileName = m_configName.c_str();

	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(fileName);

	if (!rootNode)
	{
		AIWarning("Failed to open XML file '%s'...", fileName);

		return false;
	}

	m_dynamicSurfaceEntityClasses.clear();

	const char* tagName = rootNode->getTag();

	if (!stricmp(tagName, "Cover"))
	{
		int configNodeCount = rootNode->getChildCount();

		for (int i = 0; i < configNodeCount; ++i)
		{
			XmlNodeRef configNode = rootNode->getChild(i);

			if (!stricmp(configNode->getTag(), "DynamicSurfaceEntities"))
			{
				int dynamicSurfaceNodeCount = configNode->getChildCount();

				for (int k = 0; k < dynamicSurfaceNodeCount; ++k)
				{
					XmlNodeRef dynamicSurfaceEntityNode = configNode->getChild(k);

					if (!stricmp(dynamicSurfaceEntityNode->getTag(), "EntityClass"))
					{
						const char* name;
						if (!dynamicSurfaceEntityNode->getAttr("name", &name))
						{
							AIWarning("Missing 'name' attribute for 'EntityClass' tag in file '%s' at line %d...", fileName,
							          dynamicSurfaceEntityNode->getLine());

							return false;
						}

						if (IEntityClass* entityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name))
							m_dynamicSurfaceEntityClasses.push_back(entityClass);
						else
						{
							AIWarning("Unknown entity class '%s' in file '%s' at line %d...", name, fileName,
							          dynamicSurfaceEntityNode->getLine());

							return false;
						}
					}
					else
					{
						AIWarning("Unexpected tag '%s' in file '%s' at line %d...", dynamicSurfaceEntityNode->getTag(),
						          fileName, dynamicSurfaceEntityNode->getLine());

						return false;
					}
				}
			}
			else
			{
				AIWarning("Unexpected tag '%s' in file '%s' at line %d...", configNode->getTag(),
				          fileName, configNode->getLine());

				return false;
			}
		}
	}
	else
	{
		AIWarning("Unexpected tag '%s' in file '%s' at line %d...", rootNode->getTag(),
		          fileName, rootNode->getLine());

		return false;
	}

	return true;
}

void CCoverSystem::Reset()
{
	for (const OccupiedCover::value_type& occupiedCover : m_occupied)
	{
		m_locations.insert(GetCoverLocation(occupiedCover.first), occupiedCover.first);
	}
	m_occupied.clear();
	
	for (CoverUsersMap::value_type& coverUser : m_coverUsers)
	{
		coverUser.second->Reset();
	}

	ClearAndReserveCoverLocationCache();

	ResetDynamicCover();
}

void CCoverSystem::Clear()
{
	CoverSurface::FreeStaticData();
	stl::free_container(m_surfaces);
	stl::free_container(m_freeIDs);

	m_locations.clear();
	m_occupied.clear();
	ClearAndReserveCoverLocationCache();

	m_dynamicCoverManager.Clear();
	
	for (CoverUsersMap::value_type& coverUser : m_coverUsers)
	{
		coverUser.second->Reset();
	}
	m_coverSurfaceListeners.clear();
}

bool CCoverSystem::ReadSurfacesFromFile(const char* fileName)
{
	CTimeValue startTime = gEnv->pTimer->GetAsyncTime();

	assert(m_surfaces.empty());

	CCryFile file;

	if (!file.Open(fileName, "rb"))
	{
		AIWarning("Could not read AI Cover Surfaces. [%s]", fileName);

		return false;
	}

	uint32 fileVersion = 0;

	file.ReadType(&fileVersion);

	if (fileVersion < BAI_COVER_FILE_VERSION_READ)
	{
		AIWarning("Wrong BAI file version '%d'. Please regenerate AI Cover Surfaces in the Editor.", fileVersion);

		return false;
	}

	uint32 surfaceID = 0;

	uint32 surfaceCount = 0;
	file.ReadType(&surfaceCount);

	if (!surfaceCount)
		return true;

	m_surfaces.reserve(surfaceCount + PreallocatedDynamicCount);
	m_surfaces.resize(surfaceCount);

	std::vector<ICoverSampler::Sample> samples;

	for (uint32 i = 0; i < surfaceCount; ++i)
	{
		ICoverSystem::SurfaceInfo surfaceInfo;

		file.ReadRaw(&surfaceInfo.sampleCount, sizeof(surfaceInfo.sampleCount) + sizeof(surfaceInfo.flags));

		SwapEndian(surfaceInfo.sampleCount);
		SwapEndian(surfaceInfo.flags);

		if (!surfaceInfo.sampleCount)
			continue;

		samples.resize(surfaceInfo.sampleCount);

		for (uint s = 0; s < surfaceInfo.sampleCount; ++s)
		{
			ICoverSampler::Sample& sample = samples[s];

			file.ReadRaw(&sample.position, sizeof(sample.position) + sizeof(sample.height) + sizeof(sample.flags));

			SwapEndian(sample.position);
			SwapEndian(sample.height);
			SwapEndian(sample.flags);
		}

		surfaceInfo.samples = &samples.front();

		UpdateSurface(CoverSurfaceID(++surfaceID), surfaceInfo);
	}

	CTimeValue totalTime = gEnv->pTimer->GetAsyncTime() - startTime;

	AILogAlways("Loaded %" PRISIZE_T " AI Cover Surfaces in %g seconds...", m_surfaces.size(), totalTime.GetSeconds());

	return true;
}

void CCoverSystem::BreakEvent(const Vec3& position, float radius)
{
	m_dynamicCoverManager.BreakEvent(position, radius);
}

void CCoverSystem::AddCoverEntity(EntityId entityID)
{
	m_dynamicCoverManager.AddEntity(entityID);
}

void CCoverSystem::RemoveCoverEntity(EntityId entityID)
{
	m_dynamicCoverManager.RemoveEntity(entityID);
}

CoverSurfaceID CCoverSystem::AddSurface(const SurfaceInfo& surfaceInfo)
{
	CoverSurfaceID surfaceID;

	if (m_freeIDs.empty())
	{
		surfaceID = CoverSurfaceID(m_surfaces.size() + 1);

		if (m_surfaces.size() < m_surfaces.capacity())
			m_surfaces.resize(m_surfaces.size() + 1);
		else
		{
			m_surfaces.reserve(m_surfaces.capacity() + PreallocatedDynamicCount);
			m_surfaces.resize(m_surfaces.size() + 1);
		}
	}
	else
	{
		surfaceID = m_freeIDs.front();

		std::swap(m_freeIDs.front(), m_freeIDs.back());
		m_freeIDs.pop_back();
	}

	CoverSurface(surfaceInfo).Swap(m_surfaces[surfaceID - 1]);
	CoverSurface& surface = m_surfaces[surfaceID - 1];

	AddLocations(surfaceID, surface);

	if (surface.GetFlags() & ICoverSystem::SurfaceInfo::Dynamic)
		AddDynamicSurface(surfaceID, m_surfaces[surfaceID - 1]);

	return surfaceID;
}

void CCoverSystem::RemoveSurface(const CoverSurfaceID& surfaceID)
{
	if ((surfaceID > 0) && (surfaceID <= m_surfaces.size()))
	{
		NotifyCoverUsers(surfaceID);
		NotifyCoverSurfaceListeners(surfaceID);

		m_dynamicCoverManager.RemoveSurfaceValidationSegments(surfaceID);

		RemoveLocations(surfaceID, m_surfaces[surfaceID - 1]);

		CoverSurface().Swap(m_surfaces[surfaceID - 1]);
		assert(std::find(m_freeIDs.begin(), m_freeIDs.end(), surfaceID) == m_freeIDs.end());
		m_freeIDs.push_back(surfaceID);
	}
}

void CCoverSystem::UpdateSurface(const CoverSurfaceID& surfaceID, const SurfaceInfo& surfaceInfo)
{
	if ((surfaceID > 0) && (surfaceID <= m_surfaces.size()))
	{
		NotifyCoverUsers(surfaceID);
		NotifyCoverSurfaceListeners(surfaceID);

		CoverSurface& surface = m_surfaces[surfaceID - 1];

		m_dynamicCoverManager.RemoveSurfaceValidationSegments(surfaceID);

		RemoveLocations(surfaceID, surface);

		CoverSurface(surfaceInfo).Swap(surface);

		AddLocations(surfaceID, surface);
		AddDynamicSurface(surfaceID, surface);
	}
}

uint32 CCoverSystem::GetSurfaceCount() const
{
	return m_surfaces.size() - m_freeIDs.size();
}

bool CCoverSystem::GetSurfaceInfo(const CoverSurfaceID& surfaceID, SurfaceInfo* surfaceInfo) const
{
	if ((surfaceID > 0) && (surfaceID <= m_surfaces.size()))
	{
		const CoverSurface& surface = m_surfaces[surfaceID - 1];

		return surface.GetSurfaceInfo(surfaceInfo);
	}

	return false;
}

void CCoverSystem::SetCoverOccupied(const CoverID& coverID, bool occupied, const CoverUser& occupant)
{
	const ICoverUser::Params& occupantParams = occupant.GetParams();
	const EntityId occupantEntityId = occupantParams.userID;

	if (occupied)
	{
		OccupantInfo occupantInfo;
		occupantInfo.entityId = occupantEntityId;
		occupantInfo.pos = GetCoverLocation(coverID, occupantParams.distanceToCover);
		occupantInfo.radius = occupantParams.inCoverRadius;

		std::pair<OccupiedCover::iterator, bool> iresult = m_occupied.insert(OccupiedCover::value_type(coverID, std::move(occupantInfo)));
		if (!iresult.second)
		{
			if (iresult.first->second.entityId != occupantEntityId)
			{
				AIWarning("Trying to set occupied an already occupied CoverID!");
			}
		}
		else
		{
			Locations::iterator it = m_locations.find(GetCoverLocation(coverID), coverID);
			if (it != m_locations.end())
				m_locations.erase(it);
		}
	}
	else
	{
		OccupiedCover::iterator it = m_occupied.find(coverID);

		if (it != m_occupied.end())
		{
			if (occupantEntityId == it->second.entityId)
			{
				m_occupied.erase(it);
				m_locations.insert(GetCoverLocation(coverID), coverID);
			}
			else
			{
				AIWarning("Trying to set unoccupied a CoverID by someone who is not the owner!");
			}
		}
	}
}

bool CCoverSystem::IsCoverOccupied(const CoverID& coverID) const
{
	return m_occupied.find(coverID) != m_occupied.end();
}

EntityId CCoverSystem::GetCoverOccupant(const CoverID& coverID) const
{
	OccupiedCover::const_iterator it = m_occupied.find(coverID);
	if (it != m_occupied.end())
		return it->second.entityId;

	return 0;
}

bool CCoverSystem::IsCoverPhysicallyOccupiedByAnyOtherCoverUser(const CoverID& coverID, const ICoverUser& coverUserSearchingForEmptySpace) const
{
	const ICoverUser::Params& params = coverUserSearchingForEmptySpace.GetParams();
	const Vec3 locationToTest = GetCoverLocation(coverID, params.distanceToCover);
	const float occupyRadius = params.inCoverRadius + gAIEnv.CVars.CoverSpacing;
	const EntityId entityIdToSkip = params.userID;

	for (auto it = m_occupied.cbegin(); it != m_occupied.cend(); ++it)
	{
		const OccupantInfo& occupantInfo = it->second;

		if (occupantInfo.entityId == entityIdToSkip)
			continue;

		if ((locationToTest - occupantInfo.pos).GetLengthSquared2D() > sqr(occupantInfo.radius + occupyRadius))
			continue;

		if (fabsf(locationToTest.z - occupantInfo.pos.z) >= 3.0f)	// TODO: turn this hardcoded height of 3.0f meters into a parameter of this function
			continue;

		return true;	// occupied
	}

	return false;	// not occupied
}

uint32 CCoverSystem::GetCover(const Vec3& center, float range, const Vec3* eyes, uint32 eyeCount, float distanceToCover,
                              Vec3* locations, uint32 maxLocationCount, uint32 maxLocationsPerSurface) const
{
	m_externalQueryBuffer.resize(0);

	uint32 count = m_locations.query_sphere(center, range, m_externalQueryBuffer);
	uint32 outputCount = 0;

	CoverCollection::const_iterator it = m_externalQueryBuffer.begin();
	CoverCollection::const_iterator end = m_externalQueryBuffer.end();

	if (!maxLocationsPerSurface)
	{
		for (; it != end; ++it)
		{
			const CoverSurface& surface = GetCoverSurface(*it);
			Vec3 location = GetCoverLocation(*it, distanceToCover, 0, 0);

			bool inCover = true;

			for (uint32 e = 0; e < eyeCount; ++e)
			{
				if (!surface.IsPointInCover(eyes[e], location))
				{
					inCover = false;
					break;
				}
			}

			if (inCover)
				locations[outputCount++] = location;
		}
	}
	else
	{
		std::sort(m_externalQueryBuffer.begin(), m_externalQueryBuffer.end());

		while ((it != end) && (outputCount < maxLocationCount))
		{
			CoverSurfaceID currentID = GetSurfaceID(*it);
			const CoverSurface& surface = GetCoverSurface(currentID);

			CoverCollection::const_iterator nextSurfaceIt = it;

			while ((nextSurfaceIt != end) && GetSurfaceID(*nextSurfaceIt) == currentID)
				++nextSurfaceIt;

			uint32 surfaceLocationCount = static_cast<uint32>(nextSurfaceIt - it);
			uint32 step = 1;

			if (surfaceLocationCount > maxLocationsPerSurface)
				step = surfaceLocationCount / maxLocationsPerSurface;

			uint32 surfaceOutputCount = 0;
			uint32 k = (surfaceLocationCount - (step * (maxLocationsPerSurface - 1))) >> 1;

			while ((k < surfaceLocationCount) && (outputCount < maxLocationCount) && (surfaceOutputCount < maxLocationsPerSurface))
			{
				Vec3 location = GetCoverLocation(*(it + k), distanceToCover, 0, 0);

				bool inCover = true;

				for (uint32 e = 0; e < eyeCount; ++e)
				{
					if (!surface.IsPointInCover(eyes[e], location))
					{
						inCover = false;
						break;
					}
				}

				if (inCover)
				{
					locations[outputCount++] = location;
					++surfaceOutputCount;
				}

				k += step;
			}

			it = nextSurfaceIt;
		}
	}

	return outputCount;
}

void CCoverSystem::DrawSurface(const CoverSurfaceID& surfaceID)
{
	if ((surfaceID > 0) && (surfaceID <= m_surfaces.size()))
	{
		CoverSurface& surface = m_surfaces[surfaceID - 1];
		surface.DebugDraw();

		CoverPath path;
		surface.GenerateCoverPath(0.5f, &path, false);
		path.DebugDraw();
	}
}

void CCoverSystem::Update(float updateTime)
{
	for (auto& coverUser : m_coverUsers)
	{
		coverUser.second->Update(updateTime);
	}
	m_dynamicCoverManager.Update(updateTime);
}

void CCoverSystem::DebugDraw()
{
#ifdef CRYAISYSTEM_DEBUG
	if (gAIEnv.CVars.DebugDrawCoverOccupancy > 0)
	{
		CDebugDrawContext dc;

		OccupiedCover::const_iterator it = m_occupied.begin();
		OccupiedCover::const_iterator end = m_occupied.end();

		for (; it != end; ++it)
		{
			const CoverID& coverID = it->first;
			const EntityId occupierID = it->second.entityId;

			const Vec3 location = GetCoverLocation(coverID);
			
			if (IEntity* pOccupierEntity = gEnv->pEntitySystem->GetEntity(occupierID))
			{
				IAIObject* occupier = pOccupierEntity->GetAI();

				const ColorB color = (occupier && occupier->IsEnabled()) ? Col_Orange : Col_Red;
				dc->DrawSphere(location, 1.0f, color);

				if (occupier)
				{
					dc->DrawLine(location, color, occupier->GetPos(), color);
				}
			}
		}
	}

	if (gAIEnv.CVars.DebugDrawCover == 5)
	{
		CDebugDrawContext dc;
		dc->TextToScreen(0, 60, "CoverLocationCache size: %" PRISIZE_T, m_coverLocationCache.size());
	}

	if (gAIEnv.CVars.DebugDrawCover != 2)
		return;

	const CCamera& cam = gEnv->pSystem->GetViewCamera();
	const Vec3& pos = cam.GetPosition();

	Vec3 eyes[5];
	uint32 eyeCount = 0;

	CAISystem::SObjectDebugParams testCoverEyeParams;
	if (GetAISystem()->GetObjectDebugParamsFromName("TestCoverEye", testCoverEyeParams))
	{
		eyes[eyeCount++] = testCoverEyeParams.objectPos;

		for (uint32 i = 1; i < 5; ++i)
		{
			stack_string name;
			name.Format("TestCoverEye%d", i);

			if (GetAISystem()->GetObjectDebugParamsFromName(name.c_str(), testCoverEyeParams))
			{
				eyes[eyeCount++] = testCoverEyeParams.objectPos;
			}
		}
	}

	uint32 surfaceCount = m_surfaces.size();

	for (uint32 i = 0; i != surfaceCount; ++i)
	{
		CoverSurface& surface = m_surfaces[i];

		if (!surface.IsValid())
			continue;

		if ((surface.GetAABB().GetCenter() - pos).len2() >= sqr(75.0f))
			continue;

		if (cam.IsAABBVisible_FH(surface.GetAABB()) == CULL_EXCLUSION)
			continue;

		surface.DebugDraw();

		CoverPath path;
		surface.GenerateCoverPath(0.5f, &path, false);
		path.DebugDraw();

		DebugDrawSurfaceEffectiveHeight(surface, eyes, eyeCount);
	}

	const size_t MaxTestCoverEntityCount = 16;

	IEntity* coverEntities[MaxTestCoverEntityCount];
	size_t entityCount = 0;

	if (IEntity* entity = gEnv->pEntitySystem->FindEntityByName("TestCoverEntity"))
	{
		coverEntities[entityCount++] = entity;

		for (uint32 i = 1; i < MaxTestCoverEntityCount; ++i)
		{
			stack_string name;
			name.Format("TestCoverEntity%u", i);

			if (entity = gEnv->pEntitySystem->FindEntityByName(name.c_str()))
				coverEntities[entityCount++] = entity;
		}
	}

	for (size_t i = 0; i < entityCount; ++i)
	{
		if (IPhysicalEntity* physicalEntity = coverEntities[i]->GetPhysics())
		{
			IEntity* entity = coverEntities[i];
			pe_status_nparts nparts;

			if (int partCount = physicalEntity->GetStatus(&nparts))
			{
				AABB localBB(AABB::RESET);

				pe_status_pos pp;
				primitives::box box;

				for (int p = 0; p < partCount; ++p)
				{
					pp.ipart = p;
					pp.flags = status_local;

					if (physicalEntity->GetStatus(&pp))
					{
						if (IGeometry* geometry = pp.pGeomProxy ? pp.pGeomProxy : pp.pGeom)
						{
							geometry->GetBBox(&box);

							Vec3 center = box.center * pp.scale;
							Vec3 size = box.size * pp.scale;

							center = pp.pos + pp.q * center;
							Matrix33 orientationTM = Matrix33(pp.q) * box.Basis.GetTransposed();

							localBB.Add(center + orientationTM * Vec3(size.x, size.y, size.z));
							localBB.Add(center + orientationTM * Vec3(size.x, size.y, -size.z));
							localBB.Add(center + orientationTM * Vec3(size.x, -size.y, size.z));
							localBB.Add(center + orientationTM * Vec3(size.x, -size.y, -size.z));
							localBB.Add(center + orientationTM * Vec3(-size.x, size.y, size.z));
							localBB.Add(center + orientationTM * Vec3(-size.x, size.y, -size.z));
							localBB.Add(center + orientationTM * Vec3(-size.x, -size.y, size.z));
							localBB.Add(center + orientationTM * Vec3(-size.x, -size.y, -size.z));
						}
					}
				}

				Matrix34 worldTM = entity->GetWorldTM();

				OBB obb = OBB::CreateOBBfromAABB(Matrix33(worldTM), localBB);

				{
					MARK_UNUSED pp.ipart;
					MARK_UNUSED pp.flags;

					physicalEntity->GetStatus(&pp);

					ICoverSampler::Params params;
					params.position = worldTM.GetTranslation() + obb.m33.TransformVector(obb.c) + (obb.m33.GetColumn0() * -obb.h.x) + obb.m33.GetColumn2() * -obb.h.z;
					params.position.z = pp.BBox[0].z + pp.pos.z;

					params.direction = obb.m33.GetColumn0();
					params.direction.z = 0.0f;
					params.direction.normalize();
					params.referenceEntity = entity;
					params.heightAccuracy = 0.075f;
					params.maxStartHeight = params.minHeight;

					ICoverSampler* sampler = gAIEnv.pCoverSystem->CreateCoverSampler("Default");

					if (sampler->StartSampling(params) == ICoverSampler::InProgress)
					{
						while (sampler->Update(0.0001f) == ICoverSampler::InProgress)
							;

						sampler->DebugDraw();

						if (sampler->GetState() == ICoverSampler::Finished)
						{
							ICoverSystem::SurfaceInfo surfaceInfo;
							surfaceInfo.flags = sampler->GetSurfaceFlags();
							surfaceInfo.samples = sampler->GetSamples();
							surfaceInfo.sampleCount = sampler->GetSampleCount();

							CoverSurface surface(surfaceInfo);
							surface.DebugDraw();
						}
					}

					CDebugDrawContext dc;
					dc->DrawSphere(params.position, 0.025f, Col_CadetBlue);
					dc->DrawLine(params.position, Col_CadetBlue, params.position + params.direction * params.limitDepth,
					             Col_CadetBlue);
				}
			}
		}
	}
#endif // CRYAISYSTEM_DEBUG
}

void CCoverSystem::DebugDrawSurfaceEffectiveHeight(const CoverSurface& surface, const Vec3* eyes, uint32 eyeCount)
{
	if (eyeCount <= 0)
		return;

	ICoverUser::Params userParams;
	
	//Draw effective height in cover locations based on cover eyes similar how it is in CoverUser class
	for (uint32 i = 0; i < surface.GetLocationCount(); ++i)
	{
		Vec3 coverNormal;
		const Vec3 coverLocation = surface.GetLocation(i, 0.5f, nullptr, &coverNormal);

		bool bIsCompromised = false;
		for (uint32 i = 0; i < eyeCount; ++i)
		{
			if (!surface.IsCircleInCover(eyes[i], coverLocation, userParams.inCoverRadius))
			{
				bIsCompromised = true;
				break;
			}
		}
		if (!bIsCompromised)
		{
			if (coverNormal.dot(eyes[0] - coverLocation) >= 0.0001f)
				bIsCompromised = true;
		}
		if (!bIsCompromised)
		{
			float effectiveHeightSqr = FLT_MAX;
			for (uint32 i = 0; i < eyeCount; ++i)
			{
				float heightSq;
				if (!surface.GetCoverOcclusionAt(eyes[i], coverLocation, &heightSq))
				{
					effectiveHeightSqr = FLT_MAX;
					break;
				}
				if (heightSq <= effectiveHeightSqr)
					effectiveHeightSqr = heightSq;
			}

			if (effectiveHeightSqr > 0.0f && effectiveHeightSqr < FLT_MAX)
			{
				CDebugDrawContext dc;
				const float effectiveHeight = sqrt_tpl(effectiveHeightSqr);
				const Vec3 top = coverLocation + CoverUp * effectiveHeight;

				dc->DrawLine(coverLocation, Col_LimeGreen, top, Col_LimeGreen, 25.0f);
				dc->Draw3dLabel(top, 1.5f, "%.2f", effectiveHeight);
			}
		}
	}
}

void CCoverSystem::DebugDrawCoverUser(const EntityId entityId)
{
	auto findIt = m_coverUsers.find(entityId);
	if (findIt != m_coverUsers.end())
	{
		findIt->second->DebugDraw();
	}
}

bool CCoverSystem::IsDynamicSurfaceEntity(IEntity* entity) const
{
	return stl::find(m_dynamicSurfaceEntityClasses, entity->GetClass());
}

void CCoverSystem::AddLocations(const CoverSurfaceID& surfaceID, const CoverSurface& surface)
{
	uint32 locationCount = surface.GetLocationCount();

	for (uint32 i = 0; i < locationCount; ++i)
		m_locations.insert(surface.GetLocation(i), CoverID((surfaceID << CoverIDSurfaceIDShift) | i));
}

void CCoverSystem::RemoveLocations(const CoverSurfaceID& surfaceID, const CoverSurface& surface)
{
	for (uint32 i = 0; i < surface.GetLocationCount(); ++i)
	{
		Locations::iterator it = m_locations.find(surface.GetLocation(i), CoverID((surfaceID << CoverIDSurfaceIDShift) | i));
		if (it != m_locations.end())
			m_locations.erase(it);
	}
}

void CCoverSystem::AddDynamicSurface(const CoverSurfaceID& surfaceID, const CoverSurface& surface)
{
	for (uint32 i = 0; i < surface.GetSegmentCount(); ++i)
	{
		const CoverSurface::Segment& segment = surface.GetSegment(i);

		if (segment.flags & CoverSurface::Segment::Dynamic)
		{
			ICoverSystem::SurfaceInfo surfaceInfo;

			if (surface.GetSurfaceInfo(&surfaceInfo))
			{
				const ICoverSampler::Sample& left = surfaceInfo.samples[segment.leftIdx];
				const ICoverSampler::Sample& right = surfaceInfo.samples[segment.rightIdx];

				DynamicCoverManager::ValidationSegment validationSegment;
				validationSegment.center = (left.position + right.position) * 0.5f;
				validationSegment.normal = segment.normal;
				validationSegment.height = (left.GetHeight() + right.GetHeight()) * 0.5f;
				validationSegment.length = segment.length;
				validationSegment.surfaceID = surfaceID;
				validationSegment.segmentIdx = i;

				m_dynamicCoverManager.AddValidationSegment(validationSegment);
			}
		}
	}
}

void CCoverSystem::ResetDynamicSurface(const CoverSurfaceID& surfaceID, CoverSurface& surface)
{
	for (uint32 i = 0; i < surface.GetSegmentCount(); ++i)
	{
		CoverSurface::Segment& segment = surface.GetSegment(i);

		if (segment.flags & CoverSurface::Segment::Dynamic)
			segment.flags &= ~CoverSurface::Segment::Disabled;
	}
}

void CCoverSystem::ResetDynamicCover()
{
	m_dynamicCoverManager.Clear();

	Surfaces::iterator it = m_surfaces.begin();
	Surfaces::iterator end = m_surfaces.end();

	for (; it != end; ++it)
	{
		CoverSurface& surface = *it;

		if (surface.IsValid() && (surface.GetFlags() & ICoverSystem::SurfaceInfo::Dynamic))
		{
			CoverSurfaceID surfaceID((uint32)std::distance(m_surfaces.begin(), it) + 1);

			ResetDynamicSurface(surfaceID, surface);
			AddDynamicSurface(surfaceID, surface);
		}
	}
}

void CCoverSystem::NotifyCoverUsers(const CoverSurfaceID& surfaceID)
{
	if (m_occupied.empty())
		return;

	// Make copy of the container for iterating because otherwise iterator may be invalidated inside calling callbacks
	decltype(m_occupied) occupiedTemp(m_occupied);
	for (OccupiedCover::const_iterator it = occupiedTemp.begin(); it != occupiedTemp.end(); ++it)
	{
		const CoverID coverID = it->first;
		const OccupantInfo& occupantInfo = it->second;

		if (GetSurfaceID(coverID) == surfaceID)
		{
			CoverUsersMap::const_iterator findIt = m_coverUsers.find(occupantInfo.entityId);
			if (findIt != m_coverUsers.end())
			{
				CoverUser* pCoverUser = findIt->second.get();
				if (pCoverUser->GetParams().activeCoverInvalidateCallback)
				{
					pCoverUser->GetParams().activeCoverInvalidateCallback(coverID, pCoverUser);
				}
			}
			else
			{
				CRY_ASSERT_MESSAGE(false, "Cover id=%u is occupied by unregistered entity id=%u", static_cast<uint32>(coverID), static_cast<uint32>(occupantInfo.entityId));
			}
		}
	}
}

void CCoverSystem::NotifyCoverSurfaceListeners(const CoverSurfaceID& surfaceID)
{
	if (m_coverSurfaceListeners.empty())
		return;

	for (auto it = m_coverSurfaceListeners.begin(); it != m_coverSurfaceListeners.end(); )
	{
		ICoverSurfaceListener* pListener = *it++;
		pListener->OnCoverSurfaceChangedOrRemoved(surfaceID);
	}
}

Vec3 CCoverSystem::GetAndCacheCoverLocation(const CoverID& coverID, float offset /* = 0.0f */, float* height /* = 0 */, Vec3* normal /* = 0 */) const
{
	CachedCoverLocationValues cachedValue;
	if (m_coverLocationCache.find(coverID) != m_coverLocationCache.end())
	{
		cachedValue = m_coverLocationCache[coverID];
	}
	else
	{
		CoverSurfaceID surfaceID(coverID >> CoverIDSurfaceIDShift);
		if (surfaceID <= m_surfaces.size())
		{
			if (m_coverLocationCache.size() == MAX_CACHED_COVERS)
				m_coverLocationCache.clear();

			float cachedHeight(.0f);
			Vec3 cachedNormal(ZERO);
			cachedValue.location = m_surfaces[surfaceID - 1].GetLocation(coverID & CoverIDLocationIDMask, 0.0f, &cachedHeight, &cachedNormal);
			cachedValue.height = cachedHeight;
			cachedValue.normal = cachedNormal;
			m_coverLocationCache[coverID] = cachedValue;
		}
		else
		{
			if (normal)
				normal->zero();
			return Vec3Constants<float>::fVec3_Zero;
		}
	}
	if (height)
		*height = cachedValue.height;
	if (normal)
		*normal = cachedValue.normal;

	const float tempOffset = static_cast<float>(__fsel(offset - 0.001f, offset, .0f));
	return cachedValue.location + cachedValue.normal * tempOffset;
}

void CCoverSystem::ClearAndReserveCoverLocationCache()
{
	stl::free_container(m_coverLocationCache);
	m_coverLocationCache.reserve(MAX_CACHED_COVERS);
}

const CoverPath& CCoverSystem::CacheCoverPath(const CoverSurfaceID& surfaceID, const CoverSurface& surface,
                                              float distanceToCover) const
{
	PathCache::iterator it = m_pathCache.begin();
	PathCache::iterator end = m_pathCache.end();

	CoverPath* path = 0;

	for (; it != end; ++it)
	{
		PathCacheEntry& entry = *it;

		if (entry.surfaceID == surfaceID)
		{
			Paths& paths = entry.paths;
			std::pair<Paths::iterator, bool> iresult = paths.insert(Paths::value_type(distanceToCover, CoverPath()));

			path = &iresult.first->second;

			if (!iresult.second)
				return *path;
		}
	}

	if (!path)
	{
		m_pathCache.push_front(PathCacheEntry());
		PathCacheEntry& front = m_pathCache.front();

		front.surfaceID = surfaceID;
		std::pair<Paths::iterator, bool> iresult = front.paths.insert(Paths::value_type(distanceToCover, CoverPath()));

		path = &iresult.first->second;
	}

	while (m_pathCache.size() >= 15)
		m_pathCache.pop_back();

	surface.GenerateCoverPath(distanceToCover, path);

	return *path;
}
