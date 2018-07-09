// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Voxelizer.h"
#include "HashComputer.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

namespace MNM
{
Voxelizer::Voxelizer()
	: m_volumeAABB(AABB::RESET)
	, m_voxelConv(ZERO)
	, m_voxelSize(ZERO)
	, m_voxelSpaceSize(ZERO)
#if DEBUG_MNM_ENABLED
	, m_pDebugRawGeometry(nullptr)
#endif // DEBUG_MNM_ENABLED
{
}

void Voxelizer::Reset()
{
	m_spanGrid.Reset(0, 0);
}

void Voxelizer::Start(const AABB& volume, const Vec3& voxelSize)
{
	const Vec3 spaceAbs((volume.max - volume.min).abs());
	const Vec3 voxelConv(1.0f / voxelSize.x, 1.0f / voxelSize.y, 1.0f / voxelSize.z);
	m_voxelConv = voxelConv;

	m_voxelSpaceSize((int)((spaceAbs.x * voxelConv.x) + 0.5f), (int)((spaceAbs.y * voxelConv.y) + 0.5f), (int)((spaceAbs.z * voxelConv.z) + 0.5f));
	m_spanGrid.Reset((size_t)m_voxelSpaceSize.x, (size_t)(m_voxelSpaceSize.y));

	m_volumeAABB = volume;
	m_voxelSize = voxelSize;
}

Vec3i GetVec3iFromVec3(const Vec3& vector)
{
	return Vec3i((int)vector.x, (int)vector.y, (int)vector.z);
}

void Voxelizer::RasterizeTriangle(const Vec3 v0World, const Vec3 v1World, const Vec3 v2World)
{
	const Vec3 minTriangleBoundingBoxWorld(Minimize(v0World, v1World, v2World));
	const Vec3 maxTriangleBoundingBoxWorld(Maximize(v0World, v1World, v2World));

	if (!Overlap::AABB_AABB(AABB(minTriangleBoundingBoxWorld, maxTriangleBoundingBoxWorld), m_volumeAABB))
		return;

#if DEBUG_MNM_ENABLED
	if (m_pDebugRawGeometry)
	{
		m_pDebugRawGeometry->emplace_back(v0World, v1World, v2World);
	}
#endif // DEBUG_MNM_ENABLED

#if 0 // see notes below, why it's disabled
	// NOTE pavloi 2016.03.16: shift volume and triangle into Vec3(ZERO) to avoid
	// issues, that happen, when same geometry results in different voxelization
	// in overlapping parts of neighbor tiles.

	// NOTE pavloi 2016.06.09: I hoped, that the math will behave more stable this way.
	// But it didn't really solved the issue - some test cases were fixed, but then some other broke.

	const Vec3 spaceMinWorld = m_volumeAABB.min;
	const Vec3 spaceMin(ZERO);
#else
	const Vec3 spaceMinWorld(ZERO);
	const Vec3 spaceMin = m_volumeAABB.min;
#endif

	const Vec3 v0 = v0World - spaceMinWorld;
	const Vec3 v1 = v1World - spaceMinWorld;
	const Vec3 v2 = v2World - spaceMinWorld;

	const Vec3 minTriangleBoundingBox = minTriangleBoundingBoxWorld - spaceMinWorld;
	const Vec3 maxTriangleBoundingBox = maxTriangleBoundingBoxWorld - spaceMinWorld;

	const Vec3 e0(v1 - v0);
	const Vec3 e1(v2 - v1);
	const Vec3 e2(v0 - v2);

	const Vec3 n = e2.Cross(e0);

	const bool backface = n.z < 0.0f;

	const Vec3 voxelSize = m_voxelSize;
	const Vec3 voxelConv = m_voxelConv;
	const Vec3i voxelSpaceSize = m_voxelSpaceSize;

	// The absolute value of the voxelMin vector represents the amount of voxels that
	// can fit in the spaceMin to vertexMin vector.
	const Vec3 voxelMin((minTriangleBoundingBox - spaceMin).CompMul(voxelConv));
	// The absolute value of the voxelMin vector represents the amount of voxels that
	// can fit in the spaceMin to vertexMax vector.
	const Vec3 voxelMax((maxTriangleBoundingBox - spaceMin).CompMul(voxelConv));

	// Now we try to see how many voxels we actually need to compute. The volumeAABB can fit voxels with index
	// that can vary from 0 to voxelSpaceSize - Vec3(1)
	const Vec3i minVoxelIndex(Maximize<int>(GetVec3iFromVec3(voxelMin), Vec3i(0)));
	const Vec3i maxVoxelIndex(Minimize<int>(GetVec3iFromVec3(voxelMax), voxelSpaceSize - Vec3i(1)));

	// This represent the voxel size vector
	const Vec3 dp(voxelSize);

	// c is a vector pointing to the furthest edge of the voxel in the direction
	// of the triangle normal.
	const Vec3 c(n.x > 0.0f ? dp.x : 0.0f, n.y > 0.0f ? dp.y : 0.0f, n.z > 0.0f ? dp.z : 0.0f);
	// (dp - c) is the vector pointing to the edge opposed to the one pointed by c
	// Basically firstVericalLimitForTriangleRasterization and secondVerticalLimitForTriangleRasterization
	// represents the length (amplified by the length of the normal) of the projection of the two
	// vectors that starts from v0 and point to two opposite edges on the voxel placed in the origin.
	// This creates a range of 2 values into which the calculation of the voxel vertical position needs
	// to fall to be accepted
	const float firstVericalLimitForTriangleRasterization = n.Dot(c - v0);
	const float secondVerticalLimitForTriangleRasterization = n.Dot(dp - c - v0);

	// These bool values identify if the triangle points need to be considered in
	// clockwise or counterclockwise order for the normal/distance calculation
	// for the respective plane
	const bool xycw = n.z < 0.0f;
	const bool xzcw = n.y > 0.0f;
	const bool yzcw = n.x < 0.0f;

	const bool zPlanar = minVoxelIndex.z == maxVoxelIndex.z;

	// ne0_xy means normal of the edge e0 on the xy plane. The normal points
	// to the internal part of the triangle
	Vec2 ne0_xy, ne0_xz, ne0_yz;
	// de0_xy means distance from the edge0 on the xy plane
	float de0_xy, de0_xz, de0_yz;
	Evaluate2DEdge(ne0_xy, de0_xy, xycw, Vec2(e0.x, e0.y), Vec2(v0.x, v0.y), Vec2(dp.x, dp.y));

	if (!zPlanar)
	{
		Evaluate2DEdge(ne0_xz, de0_xz, xzcw, Vec2(e0.x, e0.z), Vec2(v0.x, v0.z), Vec2(dp.x, dp.z));
		Evaluate2DEdge(ne0_yz, de0_yz, yzcw, Vec2(e0.y, e0.z), Vec2(v0.y, v0.z), Vec2(dp.y, dp.z));
	}

	Vec2 ne1_xy, ne1_xz, ne1_yz;
	float de1_xy, de1_xz, de1_yz;
	Evaluate2DEdge(ne1_xy, de1_xy, xycw, Vec2(e1.x, e1.y), Vec2(v1.x, v1.y), Vec2(dp.x, dp.y));

	if (!zPlanar)
	{
		Evaluate2DEdge(ne1_xz, de1_xz, xzcw, Vec2(e1.x, e1.z), Vec2(v1.x, v1.z), Vec2(dp.x, dp.z));
		Evaluate2DEdge(ne1_yz, de1_yz, yzcw, Vec2(e1.y, e1.z), Vec2(v1.y, v1.z), Vec2(dp.y, dp.z));
	}

	Vec2 ne2_xy, ne2_xz, ne2_yz;
	float de2_xy, de2_xz, de2_yz;
	Evaluate2DEdge(ne2_xy, de2_xy, xycw, Vec2(e2.x, e2.y), Vec2(v2.x, v2.y), Vec2(dp.x, dp.y));

	if (!zPlanar)
	{
		Evaluate2DEdge(ne2_xz, de2_xz, xzcw, Vec2(e2.x, e2.z), Vec2(v2.x, v2.z), Vec2(dp.x, dp.z));
		Evaluate2DEdge(ne2_yz, de2_yz, yzcw, Vec2(e2.y, e2.z), Vec2(v2.y, v2.z), Vec2(dp.y, dp.z));
	}

	{
		for (int y = minVoxelIndex.y; y <= maxVoxelIndex.y; ++y)
		{
			const float minY = spaceMin.y + y * voxelSize.y;

			if ((minY + dp.y < minTriangleBoundingBox.y) || (minY > maxTriangleBoundingBox.y))
				continue;

			for (int x = minVoxelIndex.x; x <= maxVoxelIndex.x; ++x)
			{
				const float minX = spaceMin.x + x * voxelSize.x;

				if ((minX + dp.x < minTriangleBoundingBox.x) || (minX > maxTriangleBoundingBox.x))
					continue;

				if (ne0_xy.Dot(Vec2(minX, minY)) + de0_xy < 0.0f)
					continue;
				if (ne1_xy.Dot(Vec2(minX, minY)) + de1_xy < 0.0f)
					continue;
				if (ne2_xy.Dot(Vec2(minX, minY)) + de2_xy < 0.0f)
					continue;

				if (zPlanar)
				{
					m_spanGrid.AddVoxel(x, y, minVoxelIndex.z, backface);

					continue;
				}

				bool wasPreviousVoxelBelowTheTriangle = true;

				for (int z = minVoxelIndex.z; z <= maxVoxelIndex.z; ++z)
				{
					const float minZ = spaceMin.z + z * voxelSize.z;

					if ((minZ + dp.z < minTriangleBoundingBox.z) || (minZ > maxTriangleBoundingBox.z))
						continue;

					// This projection value is amplified by the n length (it is not normalized)
					float currentVoxelProjectedOnTriangleNormal = n.Dot(Vec3(minX, minY, minZ));

					// Here we check if the current voxel is containing the triangle
					// in-between his height limits.
					const float firstDistance = (currentVoxelProjectedOnTriangleNormal + firstVericalLimitForTriangleRasterization);
					const float secondDistance = (currentVoxelProjectedOnTriangleNormal + secondVerticalLimitForTriangleRasterization);
					const bool isVoxelAboveOrBelowTheTriangle = firstDistance * secondDistance > 0.0f;
					if (isVoxelAboveOrBelowTheTriangle)
					{
						// We start the voxelization process from the bottom of a tile to the top.
						// This allows us to consider the first voxel always below the triangle we are considering.
						// For small voxels and triangles with tiny slopes, due to numerical errors,
						// we could end up skipping the voxel that correctly rasterizes a particular point.
						// So if we pass directly from a situation in which we were below the triangle
						// and we are now above it, then we don't skip the voxel and we continue to check
						// if the other requirements are fulfilled.
						if (wasPreviousVoxelBelowTheTriangle)
						{
							const bool isTheCurrentVoxelAboveTheTriangle = firstDistance > 0.0f && secondDistance > 0.0f;
							if (isTheCurrentVoxelAboveTheTriangle)
							{
								wasPreviousVoxelBelowTheTriangle = false;
							}
						}
						else
						{
							continue;
						}
					}

					wasPreviousVoxelBelowTheTriangle = false;

					if (ne0_xz.Dot(Vec2(minX, minZ)) + de0_xz < 0.0f)
						continue;
					if (ne1_xz.Dot(Vec2(minX, minZ)) + de1_xz < 0.0f)
						continue;
					// cppcheck-suppress uninitvar
					if (ne2_xz.Dot(Vec2(minX, minZ)) + de2_xz < 0.0f)
						continue;

					if (ne0_yz.Dot(Vec2(minY, minZ)) + de0_yz < 0.0f)
						continue;
					if (ne1_yz.Dot(Vec2(minY, minZ)) + de1_yz < 0.0f)
						continue;
					// cppcheck-suppress uninitvar
					if (ne2_yz.Dot(Vec2(minY, minZ)) + de2_yz < 0.0f)
						continue;

					m_spanGrid.AddVoxel(x, y, z, backface);
				}
			}
		}
	}
}

static const uint32 BoxTriIndices[] =
{
	2, 1, 0,
	0, 3, 2,
	3, 0, 7,
	0, 4, 7,
	0, 1, 5,
	0, 5, 4,
	1, 2, 5,
	6, 5, 2,
	7, 2, 3,
	7, 6, 2,
	7, 4, 5,
	7, 5, 6
};

static inline bool HasNoRotOrScale(const Matrix33& m)
{
	return ((m.m00 == 1.0f) && (m.m11 == 1.0f) && (m.m22 == 1.0f) &&
	        (m.m01 == 0.0f) && (m.m02 == 0.0f) &&
	        (m.m10 == 0.0f) && (m.m12 == 0.0f) &&
	        (m.m20 == 0.0f) && (m.m21 == 0.0f));
}

bool VoxelizeEntity(IPhysicalEntity& physicalEntity, pe_status_dynamics& statusDynamics, pe_status_pos& statusPosition)
{
	if (physicalEntity.GetType() == PE_WHEELEDVEHICLE)
	{
		return false;
	}

	bool considerMass = (physicalEntity.GetType() == PE_RIGID);
	if (!considerMass)
	{
		if (physicalEntity.GetStatus(&statusPosition))
		{
			considerMass = (statusPosition.iSimClass == SC_ACTIVE_RIGID) || (statusPosition.iSimClass == SC_SLEEPING_RIGID);
		}
	}

	if (considerMass)
	{
		if (!physicalEntity.GetStatus(&statusDynamics))
			return false;

		if (statusDynamics.mass > 1e-6f)
			return false;
	}

	return true;
}

PREFAST_SUPPRESS_WARNING(6262)
size_t WorldVoxelizer::ProcessGeometry(uint32 hashValueSeed /* = 0 */, uint32 hashTest /* = 0 */, uint32* hashValue /* = 0 */, NavigationMeshEntityCallback pEntityCallback /* = NULL */)
{
	size_t triCount = 0;

	const size_t MaxConsideredEntityCount = 2048;
	IPhysicalEntity* entities[MaxConsideredEntityCount] = { 0 };
	IPhysicalEntity** entityList = &entities[0];
	int entityCount = gEnv->pPhysicalWorld->GetEntitiesInBox(m_volumeAABB.min, m_volumeAABB.max, entityList,
	                                                         ent_static | ent_terrain | ent_sleeping_rigid | ent_rigid | ent_allocate_list | ent_addref_results, MaxConsideredEntityCount);

	pe_status_pos sp;
	pe_status_dynamics dyn;

	Matrix34 worldTM;
	sp.pMtx3x4 = &worldTM;

	//references to geoms need to be incremented because physical entity can potentially drop it's geometry anytime when this method is executed in different thread
	sp.flags = status_addref_geoms;

	HashComputer hash(hashValueSeed);
	hash.Add((uint32)entityCount);

	const size_t MaxTerrainAABBCount = 16;
	AABB terrainAABB[MaxTerrainAABBCount];
	size_t terrainAABBCount = 0;

	uint32 flags = 0;   // not used yet
	for (int i = 0; i < entityCount; ++i)
	{
		IPhysicalEntity* entity = entityList[i];

		if (pEntityCallback && pEntityCallback(*entity, flags) == false)
		{
			entity->Release();
			entityList[i] = NULL;
			continue;
		}

		sp.ipart = 0;
		MARK_UNUSED sp.partid;

		while (entity->GetStatus(&sp))
		{
			if (sp.pGeomProxy && (sp.flagsOR & geom_colltype_player))
			{
				if (sp.pGeomProxy->GetType() == GEOM_HEIGHTFIELD)
				{
					AABB aabb;
					uint32 terrainHash = ComputeTerrainHashAndAABB(sp.pGeomProxy, aabb);
					if (terrainAABBCount < MaxTerrainAABBCount)
						terrainAABB[terrainAABBCount++] = aabb;
					
					hash.Add(terrainHash);
				}
				else
				{
					hash.Add(sp.BBox[0]);
					hash.Add(sp.BBox[1]);
					hash.Add((uint32)sp.iSimClass);
					hash.Add(worldTM);
				}
			}

			++sp.ipart;
			MARK_UNUSED sp.partid;

			if (sp.pGeomProxy) sp.pGeomProxy->Release();
			if (sp.pGeom) sp.pGeom->Release();
		}
		MARK_UNUSED sp.ipart;
	}

	hash.Complete();

	if (hashValue)
		*hashValue = hash.GetValue();

	// NOTE pavloi 2016.07.13: if external meshes present, this branch will almost always be executes
	if (hashTest != hash.GetValue())
	{
		terrainAABBCount = 0;

		for (int i = 0; i < entityCount; ++i)
		{
			IPhysicalEntity* entity = entityList[i];
			if (!entity)
				continue;

			sp.ipart = 0;
			MARK_UNUSED sp.partid;

			while (entity->GetStatus(&sp))
			{
				if (sp.pGeomProxy && (sp.flagsOR & geom_colltype_player))
				{
					if (sp.pGeomProxy->GetType() == GEOM_HEIGHTFIELD)
					{
						if (terrainAABBCount < MaxTerrainAABBCount)
						{
							const AABB& aabb = terrainAABB[terrainAABBCount++];

							if (aabb.GetSize().len2() <= 0.0f)
							{
								++sp.ipart;
								MARK_UNUSED sp.partid;

								if (sp.pGeomProxy) sp.pGeomProxy->Release();
								if (sp.pGeom) sp.pGeom->Release();
								continue;
							}
						}
					}

					triCount += VoxelizeGeometry(sp.pGeomProxy, worldTM);
				}

				++sp.ipart;
				MARK_UNUSED sp.partid;

				if (sp.pGeomProxy) sp.pGeomProxy->Release();
				if (sp.pGeom) sp.pGeom->Release();
			}
			MARK_UNUSED sp.ipart;
		}
	}

	for (int i = 0; i < entityCount; ++i)
	{
		if (entityList[i])
			entityList[i]->Release();
	}

	if (entities != entityList)
		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(entityList);

	return triCount;
}

struct PhysicsVolume
{
	PhysicsVolume()
		: area(NULL)
		, volume(0)
	{
	}

	PhysicsVolume(const IPhysicalEntity* _area, float _volume, const primitives::plane& _plane)
		: area(_area)
		, volume(_volume)
		, plane(_plane)
	{
	}

	bool operator<(const PhysicsVolume& other) const
	{
		return volume < other.volume;
	}

	const IPhysicalEntity* area;
	float                  volume;
	primitives::plane      plane;
};

void WorldVoxelizer::CalculateWaterDepth()
{
	const size_t width = m_spanGrid.GetWidth();
	const size_t height = m_spanGrid.GetHeight();
	const float oceanLevel = gEnv->p3DEngine->GetWaterLevel();

	const size_t MaxAreaCount = 64;
	IPhysicalEntity* areas[MaxAreaCount];

	IPhysicalEntity** areaList = &areas[0];
	size_t areaCount = (size_t)gEnv->pPhysicalWorld->GetEntitiesInBox(m_volumeAABB.min, m_volumeAABB.max, areaList,
	                                                                  ent_areas | ent_allocate_list | ent_addref_results, MaxAreaCount);

	if (areaCount > MaxAreaCount)
	{

		assert(areas != &areaList[0]);
		memcpy(areas, areaList, sizeof(IPhysicalEntity*) * MaxAreaCount);
		for (size_t i = MaxAreaCount; i < areaCount; ++i)
		{
			if (areaList[i])
				areaList[i]->Release();
		}

		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(areaList);

		areaList = areas;
		areaCount = MaxAreaCount;
	}

	const size_t MaxConsiderVolumes = 16;

	size_t volumeCount = 0;
	PhysicsVolume volumes[MaxConsiderVolumes];

	pe_params_buoyancy pb;
	pe_params_area pa;

	for (size_t i = 0; i < areaCount; ++i)
	{
		const IPhysicalEntity* area = areaList[i];

		if (area->GetParams(&pb))
		{
			if ((pb.iMedium == 0) && !is_unused(pb.waterPlane.origin))
			{
				if (volumeCount < MaxConsiderVolumes)
				{
					float volume = 0.0f;

					if (area->GetParams(&pa))
					{
						if (pa.pGeom)
							volume = pa.pGeom->GetVolume();
					}

					volumes[volumeCount++] = PhysicsVolume(area, volume, pb.waterPlane);
				}
			}
		}
	}

	const Vec3 spaceMin = m_volumeAABB.min;
	if (volumeCount || spaceMin.z < oceanLevel)
	{
		std::sort(&volumes[0], &volumes[0] + volumeCount);

		const Vec3 voxelSize = m_voxelSize;
		const Vec3 voxelConv = m_voxelConv;

		pe_status_contains_point scp;

		for (size_t y = 0; y < height; ++y)
		{
			for (size_t x = 0; x < width; ++x)
			{
				for (DynamicSpanGrid::Element* span = m_spanGrid[x + y * width]; span; span = span->next)
				{
					const Vec3 top = spaceMin + Vec3(x * voxelSize.x, y * voxelSize.y, span->top * voxelSize.z);
					size_t depth = (top.z >= oceanLevel) ? 0 : (size_t)((oceanLevel - top.z) * voxelConv.z);

					scp.pt = top;
					for (size_t i = 0; i < volumeCount; ++i)
					{
						float vdepth = volumes[i].plane.n.z * (volumes[i].plane.n * (volumes[i].plane.origin - top));

						if (volumes[i].area->GetStatus(&scp))
						{
							depth = (vdepth <= 0.0f) ? 0 : (size_t)(vdepth * voxelConv.z);
							break;
						}
					}

					if (depth > DynamicSpanGrid::Element::MaxWaterDepth)
						depth = DynamicSpanGrid::Element::MaxWaterDepth;

					span->depth = depth;
				}
			}
		}
	}

	for (size_t i = 0; i < areaCount; ++i)
	{
		if (areaList[i])
			areaList[i]->Release();
	}

	if (areas != areaList)
		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(areaList);
}

void WorldVoxelizer::VoxelizeGeometry(const Vec3* vertices, size_t triCount, const Matrix34& worldTM)
{
	if (HasNoRotOrScale(Matrix33(worldTM)))
	{
		Vec3 offset = worldTM.GetTranslation();

		if (offset.IsZero())
		{
			for (size_t i = 0; i < triCount; ++i)
				RasterizeTriangle(vertices[i * 3 + 0],
				                  vertices[i * 3 + 1],
				                  vertices[i * 3 + 2]);
		}
		else
		{
			for (size_t i = 0; i < triCount; ++i)
				RasterizeTriangle(vertices[i * 3 + 0] + offset,
				                  vertices[i * 3 + 1] + offset,
				                  vertices[i * 3 + 2] + offset);
		}
	}
	else
	{
		for (size_t i = 0; i < triCount; ++i)
		{
			RasterizeTriangle(worldTM.TransformPoint(vertices[i * 3 + 0]),
			                  worldTM.TransformPoint(vertices[i * 3 + 1]),
			                  worldTM.TransformPoint(vertices[i * 3 + 2]));
		}
	}
}

void WorldVoxelizer::VoxelizeGeometry(const strided_pointer<Vec3>& vertices, const index_t* indices, size_t triCount, const Matrix34& worldTM)
{
	if (HasNoRotOrScale(Matrix33(worldTM)))
	{
		Vec3 offset = worldTM.GetTranslation();

		if (offset.IsZero())
		{
			for (size_t i = 0; i < triCount; ++i)
				RasterizeTriangle(vertices[indices[i * 3 + 0]], vertices[indices[i * 3 + 1]], vertices[indices[i * 3 + 2]]);
		}
		else
		{
			for (size_t i = 0; i < triCount; ++i)
			{
				RasterizeTriangle(vertices[indices[i * 3 + 0]] + offset,
				                  vertices[indices[i * 3 + 1]] + offset,
				                  vertices[indices[i * 3 + 2]] + offset);
			}
		}
	}
	else
	{
		for (size_t i = 0; i < triCount; ++i)
		{
			RasterizeTriangle(worldTM.TransformPoint(vertices[indices[i * 3 + 0]]),
			                  worldTM.TransformPoint(vertices[indices[i * 3 + 1]]),
			                  worldTM.TransformPoint(vertices[indices[i * 3 + 2]]));
		}
	}
}

void WorldVoxelizer::VoxelizeGeometry(const Vec3* vertices, const uint32* indices, size_t triCount, const Matrix34& worldTM)
{
	if (HasNoRotOrScale(Matrix33(worldTM)))
	{
		Vec3 offset = worldTM.GetTranslation();

		if (offset.IsZero())
		{
			for (size_t i = 0; i < triCount; ++i)
				RasterizeTriangle(vertices[indices[i * 3 + 0]], vertices[indices[i * 3 + 1]], vertices[indices[i * 3 + 2]]);
		}
		else
		{
			for (size_t i = 0; i < triCount; ++i)
			{
				RasterizeTriangle(vertices[indices[i * 3 + 0]] + offset,
				                  vertices[indices[i * 3 + 1]] + offset,
				                  vertices[indices[i * 3 + 2]] + offset);
			}
		}
	}
	else
	{
		for (size_t i = 0; i < triCount; ++i)
		{
			RasterizeTriangle(worldTM.TransformPoint(vertices[indices[i * 3 + 0]]),
			                  worldTM.TransformPoint(vertices[indices[i * 3 + 1]]),
			                  worldTM.TransformPoint(vertices[indices[i * 3 + 2]]));
		}
	}
}

uint32 WorldVoxelizer::ComputeTerrainHashAndAABB(IGeometry* geometry, AABB& aabb)
{
	aabb.Reset();
	
	primitives::heightfield* phf = (primitives::heightfield*)geometry->GetData();
	if (!phf) return 0;

	HashComputer hash;

	const int minX = max(0, (int)((m_volumeAABB.min.x - phf->origin.x) * phf->stepr.x));
	const int minY = max(0, (int)((m_volumeAABB.min.y - phf->origin.y) * phf->stepr.y));
	const int maxX = min((int)((m_volumeAABB.max.x - phf->origin.x) * phf->stepr.x), phf->size.x);
	const int maxY = min((int)((m_volumeAABB.max.y - phf->origin.y) * phf->stepr.y), phf->size.y);

	const Vec3 origin = phf->origin;

	const float xStep = (float)phf->step.x;
	const float yStep = (float)phf->step.y;

	float zMin = FLT_MAX;
	float zMax = -FLT_MAX;

	if (phf->fpGetSurfTypeCallback && phf->fpGetHeightCallback)
	{
		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				if (phf->fpGetSurfTypeCallback(x, y) != phf->typehole)
				{
					const float height = phf->getheight(x, y);
					zMin = min(zMin, height);
					zMax = max(zMax, height);
					hash.Add(height);
				}
			}
		}
	}
	else if (phf->fpGetSurfTypeCallback)
	{
		float* heightData = (float*)phf->fpGetHeightCallback;

		assert(heightData);
		PREFAST_ASSUME(heightData);

		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				const float height = heightData[Vec2i(x, y) * phf->stride];
				zMin = min(zMin, height);
				zMax = max(zMax, height);
				hash.Add(height);
			}
		}
	}
	else
	{
		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				float height = phf->getheight(x, y);
				zMin = min(zMin, height);
				zMax = max(zMax, height);
				hash.Add(height);
			}
		}
	}

	const Vec3 aabbMin = origin + Vec3(minX * xStep, minY * yStep, zMin * phf->heightscale);
	const Vec3 aabbMax = origin + Vec3(maxX * xStep, maxY * yStep, zMax * phf->heightscale);
	
	AABB terrainAABB(aabbMin, aabbMax);

	if (Overlap::AABB_AABB(m_volumeAABB, terrainAABB))
	{
		aabb = terrainAABB;
		hash.Complete();
		return hash.GetValue();
	}
	return 0;
}

#pragma warning (push)
#pragma warning (disable: 6262)
size_t WorldVoxelizer::VoxelizeTerrain(IGeometry* geometry, const Matrix34& worldTM)
{
	primitives::heightfield* phf = (primitives::heightfield*)geometry->GetData();

	const int minX = max(0, (int)((m_volumeAABB.min.x - phf->origin.x) * phf->stepr.x));
	const int minY = max(0, (int)((m_volumeAABB.min.y - phf->origin.y) * phf->stepr.y));
	const int maxX = min((int)((m_volumeAABB.max.x - phf->origin.x) * phf->stepr.x), (phf->size.x - 1));
	const int maxY = min((int)((m_volumeAABB.max.y - phf->origin.y) * phf->stepr.y), (phf->size.y - 1));

	const Vec3 origin = phf->origin;

	const float xStep = (float)phf->step.x;
	const float yStep = (float)phf->step.y;

	size_t faceCount = 0;
	size_t totalFaceCount = 0;

	const size_t maxFacesCount = 1024;
	const size_t maxVertexCount = maxFacesCount * 4;
	Vec3 vertices[maxVertexCount];

	if (phf->fpGetSurfTypeCallback && phf->fpGetHeightCallback)
	{
		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				if (phf->fpGetSurfTypeCallback(x, y) != phf->typehole)
				{
					const Vec3 v0 = origin + Vec3(x * xStep, y * yStep, phf->getheight(x, y) * phf->heightscale);
					const Vec3 v1 = origin + Vec3(x * xStep, (y + 1) * yStep, phf->getheight(x, y + 1) * phf->heightscale);
					const Vec3 v2 = origin + Vec3((x + 1) * xStep, y * yStep, phf->getheight(x + 1, y) * phf->heightscale);
					const Vec3 v3 = origin + Vec3((x + 1) * xStep, (y + 1) * yStep, phf->getheight(x + 1, y + 1) * phf->heightscale);

					CRY_ASSERT(faceCount < maxFacesCount);

					vertices[(faceCount << 2) + 0] = v0;
					vertices[(faceCount << 2) + 1] = v1;
					vertices[(faceCount << 2) + 2] = v2;
					vertices[(faceCount << 2) + 3] = v3;

					if (++faceCount >= maxFacesCount)
					{
						for (size_t i = 0; i < faceCount; ++i)
						{
							RasterizeTriangle(vertices[(i << 2) + 0], vertices[(i << 2) + 2], vertices[(i << 2) + 1]);
							RasterizeTriangle(vertices[(i << 2) + 1], vertices[(i << 2) + 2], vertices[(i << 2) + 3]);
						}
						totalFaceCount += faceCount;
						faceCount = 0;
					}
				}
			}
		}
	}
	else if (phf->fpGetSurfTypeCallback)
	{
		float* height = (float*)phf->fpGetHeightCallback;

		CRY_ASSERT(height);
		PREFAST_ASSUME(height);

		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				const Vec3 v0 = origin + Vec3(x * xStep, y * yStep, height[Vec2i(x, y) * phf->stride] * phf->heightscale);
				const Vec3 v1 = origin + Vec3(x * xStep, (y + 1) * yStep, height[Vec2i(x, y + 1) * phf->stride] * phf->heightscale);
				const Vec3 v2 = origin + Vec3((x + 1) * xStep, y * yStep, height[Vec2i(x + 1, y) * phf->stride] * phf->heightscale);
				const Vec3 v3 = origin + Vec3((x + 1) * xStep, (y + 1) * yStep, height[Vec2i(x + 1, y + 1) * phf->stride] * phf->heightscale);

				CRY_ASSERT(faceCount < maxFacesCount);

				vertices[(faceCount << 2) + 0] = v0;
				vertices[(faceCount << 2) + 1] = v1;
				vertices[(faceCount << 2) + 2] = v2;
				vertices[(faceCount << 2) + 3] = v3;

				if (++faceCount >= maxFacesCount)
				{
					for (size_t i = 0; i < faceCount; ++i)
					{
						RasterizeTriangle(vertices[(i << 2) + 0], vertices[(i << 2) + 2], vertices[(i << 2) + 1]);
						RasterizeTriangle(vertices[(i << 2) + 1], vertices[(i << 2) + 2], vertices[(i << 2) + 3]);
					}
					totalFaceCount += faceCount;
					faceCount = 0;
				}
			}
		}
	}
	else
	{
		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				const Vec3 v0 = origin + Vec3(x * xStep, y * yStep, phf->getheight(x, y) * phf->heightscale);
				const Vec3 v1 = origin + Vec3(x * xStep, (y + 1) * yStep, phf->getheight(x, y + 1) * phf->heightscale);
				const Vec3 v2 = origin + Vec3((x + 1) * xStep, y * yStep, phf->getheight(x + 1, y) * phf->heightscale);
				const Vec3 v3 = origin + Vec3((x + 1) * xStep, (y + 1) * yStep, phf->getheight(x + 1, y + 1) * phf->heightscale);

				assert(faceCount < maxFacesCount);

				vertices[(faceCount << 2) + 0] = v0;
				vertices[(faceCount << 2) + 1] = v1;
				vertices[(faceCount << 2) + 2] = v2;
				vertices[(faceCount << 2) + 3] = v3;

				if (++faceCount >= maxFacesCount)
				{
					for (size_t i = 0; i < faceCount; ++i)
					{
						RasterizeTriangle(vertices[(i << 2) + 0], vertices[(i << 2) + 2], vertices[(i << 2) + 1]);
						RasterizeTriangle(vertices[(i << 2) + 1], vertices[(i << 2) + 2], vertices[(i << 2) + 3]);
					}
					totalFaceCount += faceCount;
					faceCount = 0;
				}
			}
		}
	}

	for (size_t i = 0; i < faceCount; ++i)
	{
		RasterizeTriangle(vertices[(i << 2) + 0], vertices[(i << 2) + 2], vertices[(i << 2) + 1]);
		RasterizeTriangle(vertices[(i << 2) + 1], vertices[(i << 2) + 2], vertices[(i << 2) + 3]);
	}
	totalFaceCount += faceCount;

	return totalFaceCount << 1;
}
#pragma warning (pop)

#pragma warning (push)
#pragma warning (disable: 6262)
size_t WorldVoxelizer::VoxelizeGeometry(IGeometry* geometry, const Matrix34& worldTM)
{
	size_t triangleCount = 0;
	switch (geometry->GetType())
	{
	case GEOM_TRIMESH:
	case GEOM_VOXELGRID:
		{
			const mesh_data* mesh = static_cast<const mesh_data*>(geometry->GetData());

			VoxelizeGeometry(mesh->pVertices, mesh->pIndices, mesh->nTris, worldTM);

			return mesh->nTris;
		}
		break;
	case GEOM_BOX:
		{
			const primitives::box& box = *static_cast<const primitives::box*>(geometry->GetData());

			Vec3 vertices[] =
			{
				Vec3(-box.size.x, -box.size.y, -box.size.z),
				Vec3(box.size.x,  -box.size.y, -box.size.z),
				Vec3(box.size.x,  box.size.y,  -box.size.z),
				Vec3(-box.size.x, box.size.y,  -box.size.z),

				Vec3(-box.size.x, -box.size.y, box.size.z),
				Vec3(box.size.x,  -box.size.y, box.size.z),
				Vec3(box.size.x,  box.size.y,  box.size.z),
				Vec3(-box.size.x, box.size.y,  box.size.z),
			};

			Matrix34 boxTM(worldTM);

			if (box.bOriented)
			{
				boxTM = Matrix34(box.Basis.GetTransposed());
				boxTM.SetTranslation(box.center);
				boxTM = worldTM * boxTM;
			}
			else
				boxTM.AddTranslation(boxTM.TransformVector(box.center));

			VoxelizeGeometry(vertices, &BoxTriIndices[0], 12, boxTM);

			return 12;
		}
		break;
	case GEOM_SPHERE:
		{
			const primitives::sphere& sphere = *static_cast<const primitives::sphere*>(geometry->GetData());

			const Vec3 center = sphere.center;
			const float r = sphere.r;

			const size_t stacks = 48;
			const size_t slices = 48;

			const size_t vertexCount = slices * (stacks - 2) + 2;
			Vec3 vertices[vertexCount];

			const size_t indexCount = (slices - 1) * (stacks - 2) * 6;
			uint32 indices[indexCount];

			vertices[0] = center;
			vertices[0].y += r;
			vertices[1] = center;
			vertices[1].y -= r;

			size_t v = 2;
			for (size_t j = 1; j < stacks - 1; ++j)
			{
				for (size_t i = 0; i < slices; ++i)
				{
					float theta = (j / (float)(stacks - 1)) * 3.14159265f;
					float phi = (i / (float)(slices - 1)) * 2.0f * 3.14159265f;

					float stheta, ctheta;
					sincos_tpl(theta, &stheta, &ctheta);

					float sphi, cphi;
					sincos_tpl(phi, &sphi, &cphi);

					Vec3 point = center + Vec3(stheta * cphi * r, ctheta * r, -stheta * sphi * r);
					vertices[v++] = point;
				}
			}

			size_t n = 0;

			for (size_t i = 0; i < slices - 1; ++i)
			{
				indices[n++] = 0;
				indices[n++] = i + 2;
				indices[n++] = i + 3;

				indices[n++] = (stacks - 3) * slices + i + 3;
				indices[n++] = (stacks - 3) * slices + i + 2;
				indices[n++] = 1;
			}

			for (size_t j = 0; j < stacks - 3; ++j)
			{
				for (size_t i = 0; i < slices - 1; ++i)
				{
					indices[n++] = (j + 1) * slices + i + 3;
					indices[n++] = j * slices + i + 3;
					indices[n++] = (j + 1) * slices + i + 2;
					indices[n++] = j * slices + i + 3;
					indices[n++] = j * slices + i + 2;
					indices[n++] = (j + 1) * slices + i + 2;
				}
			}

			VoxelizeGeometry(vertices, indices, indexCount / 3, worldTM);
			triangleCount = vertexCount / 3;
		}
		break;
	case GEOM_CYLINDER:
		{
			const primitives::cylinder& cylinder = *static_cast<const primitives::cylinder*>(geometry->GetData());

			const Vec3 base = cylinder.center - cylinder.axis * cylinder.hh;
			const Vec3 top = cylinder.center + cylinder.axis * cylinder.hh;

			Vec3 n = cylinder.axis;
			Vec3 a = n.GetOrthogonal();

			Vec3 b = a.Cross(n);
			a = n.Cross(b);

			a.Normalize();
			b.Normalize();

			const size_t slices = 64;
			const float invSlices = 1.0f / (float)slices;

			const float r = cylinder.r;

			const size_t vertexCount = 2 + slices * 4;
			Vec3 vertices[vertexCount];

			const size_t indexCount = slices * 12;
			uint32 indices[indexCount];

			vertices[0] = base;
			vertices[1] = top;

			size_t v = 2;
			for (size_t i = 0; i < slices; ++i)
			{
				float theta0 = i * (3.14159265f * 2.0f * invSlices);
				float theta1 = (i + 1) * (3.14159265f * 2.0f * invSlices);

				float ctheta0, stheta0;
				sincos_tpl(theta0, &stheta0, &ctheta0);
				vertices[v++] = top + a * (r * ctheta0) + b * (r * stheta0);
				vertices[v++] = base + a * (r * ctheta0) + b * (r * stheta0);

				float ctheta1, stheta1;
				sincos_tpl(theta1, &stheta1, &ctheta1);
				vertices[v++] = base + a * (r * ctheta1) + b * (r * stheta1);
				vertices[v++] = top + a * (r * ctheta1) + b * (r * stheta1);
			}

			size_t t = 0;
			for (size_t i = 0; i < slices; ++i)
			{
				indices[t++] = 0;
				indices[t++] = 2 + i * 4 + 1;
				indices[t++] = 2 + i * 4 + 2;

				indices[t++] = 2 + i * 4 + 2;
				indices[t++] = 2 + i * 4 + 1;
				indices[t++] = 2 + i * 4 + 0;

				indices[t++] = 2 + i * 4 + 3;
				indices[t++] = 2 + i * 4 + 2;
				indices[t++] = 2 + i * 4 + 0;

				indices[t++] = 2 + i * 4 + 3;
				indices[t++] = 2 + i * 4 + 0;
				indices[t++] = 1;
			}

			VoxelizeGeometry(vertices, indices, indexCount / 3, worldTM);
			triangleCount = vertexCount / 3;
			return triangleCount;
		}
		break;
	case GEOM_CAPSULE:
		{
			const primitives::capsule& capsule = *static_cast<const primitives::capsule*>(geometry->GetData());

			const Vec3 base = capsule.center - capsule.axis * capsule.hh;
			const Vec3 top = capsule.center + capsule.axis * capsule.hh;

			Vec3 n = capsule.axis;
			Vec3 a = n.GetOrthogonal();

			Vec3 b = a.Cross(n);
			a = n.Cross(b);

			a.Normalize();
			b.Normalize();
			n.Normalize();

			const size_t stacks = 48;
			const size_t slices = 64;
			const float invSlices = 1.0f / (float)slices;

			const float r = capsule.r;

			{
				const size_t vertexCount = 2 + slices * 4;
				Vec3 vertices[vertexCount];

				const size_t indexCount = slices * 6;
				uint32 indices[indexCount];

				vertices[0] = base;
				vertices[1] = top;

				size_t v = 2;
				for (size_t i = 0; i < slices; ++i)
				{
					float theta0 = i * (3.14159265f * 2.0f * invSlices);
					float theta1 = (i + 1) * (3.14159265f * 2.0f * invSlices);

					float ctheta0, stheta0;
					sincos_tpl(theta0, &stheta0, &ctheta0);
					vertices[v++] = top + a * (r * ctheta0) + b * (r * stheta0);
					vertices[v++] = base + a * (r * ctheta0) + b * (r * stheta0);

					float ctheta1, stheta1;
					sincos_tpl(theta1, &stheta1, &ctheta1);
					vertices[v++] = base + a * (r * ctheta1) + b * (r * stheta1);
					vertices[v++] = top + a * (r * ctheta1) + b * (r * stheta1);
				}

				size_t t = 0;
				for (size_t i = 0; i < slices; ++i)
				{
					indices[t++] = 2 + i * 4 + 2;
					indices[t++] = 2 + i * 4 + 1;
					indices[t++] = 2 + i * 4 + 0;

					indices[t++] = 2 + i * 4 + 3;
					indices[t++] = 2 + i * 4 + 2;
					indices[t++] = 2 + i * 4 + 0;
				}

				VoxelizeGeometry(vertices, indices, indexCount / 3, worldTM);
				triangleCount += vertexCount / 3;
			}

			{
				// Bottom semi-sphere
				const Vec3 baseCenter = base;
				Vec3 baseDirection = baseCenter - capsule.center;
				baseDirection.Normalize();

				const size_t vertexCount = slices * (stacks - 2) + 2;
				Vec3 vertices[vertexCount];

				const size_t indexCount = (slices - 1) * (stacks - 3) * 6 + 6 * (slices - 1);
				uint32 indices[indexCount];

				vertices[0] = baseCenter + a * r;
				vertices[1] = baseCenter - a * r;

				size_t v = 2;
				for (size_t j = 1; j <= stacks - 2; ++j)
				{
					for (size_t i = 0; i < slices; ++i)
					{
						float theta = (j / (float)(stacks - 1)) * 3.14159265f;
						float phi = (i / (float)(slices - 1)) * 3.14159265f;

						float stheta, ctheta;
						sincos_tpl(theta, &stheta, &ctheta);

						float sphi, cphi;
						sincos_tpl(phi, &sphi, &cphi);

						const Vec3 point = baseCenter + a * ctheta * r + b * stheta * cphi * r + baseDirection * stheta * sphi * r;
						vertices[v++] = point;
					}
				}

				size_t t = 0;
				for (size_t i = 0; i < slices - 1; ++i)
				{
					indices[t++] = i + 2;
					indices[t++] = i + 3;
					indices[t++] = 0;

					indices[t++] = 1;
					indices[t++] = (stacks - 3) * slices + i + 3;
					indices[t++] = (stacks - 3) * slices + i + 2;
				}

				for (size_t j = 0; j < stacks - 3; ++j)
				{
					for (size_t i = 0; i < slices - 1; ++i)
					{
						indices[t++] = (j + 1) * slices + i + 3;
						indices[t++] = j * slices + i + 3;
						indices[t++] = (j + 1) * slices + i + 2;

						indices[t++] = j * slices + i + 3;
						indices[t++] = j * slices + i + 2;
						indices[t++] = (j + 1) * slices + i + 2;
					}
				}

				VoxelizeGeometry(vertices, indices, indexCount / 3, worldTM);
				triangleCount += vertexCount / 3;
			}

			{
				// Top semi-sphere
				const Vec3 topCenter = top;
				Vec3 topDirection = topCenter - capsule.center;
				topDirection.Normalize();

				const size_t vertexCount = slices * (stacks - 2) + 2;
				Vec3 vertices[vertexCount];

				const size_t indexCount = (slices - 1) * (stacks - 3) * 6 + 6 * (slices - 1);
				uint32 indices[indexCount];

				vertices[0] = topCenter + a * r;
				vertices[1] = topCenter - a * r;

				size_t v = 2;
				for (size_t j = 1; j <= stacks - 2; ++j)
				{
					for (size_t i = 0; i < slices; ++i)
					{
						float theta = (j / (float)(stacks - 1)) * 3.14159265f;
						float phi = (i / (float)(slices - 1)) * 3.14159265f;

						float stheta, ctheta;
						sincos_tpl(theta, &stheta, &ctheta);

						float sphi, cphi;
						sincos_tpl(phi, &sphi, &cphi);

						const Vec3 point = topCenter + a * ctheta * r + b * stheta * cphi * r + topDirection * stheta * sphi * r;
						vertices[v++] = point;
					}
				}

				size_t t = 0;
				for (size_t i = 0; i < slices - 1; ++i)
				{
					indices[t++] = 0;
					indices[t++] = i + 3;
					indices[t++] = i + 2;

					indices[t++] = (stacks - 3) * slices + i + 2;
					indices[t++] = (stacks - 3) * slices + i + 3;
					indices[t++] = 1;
				}

				for (size_t j = 0; j < stacks - 3; ++j)
				{
					for (size_t i = 0; i < slices - 1; ++i)
					{
						indices[t++] = (j + 1) * slices + i + 2;
						indices[t++] = j * slices + i + 3;
						indices[t++] = (j + 1) * slices + i + 3;
						indices[t++] = (j + 1) * slices + i + 2;
						indices[t++] = j * slices + i + 2;
						indices[t++] = j * slices + i + 3;
					}
				}

				VoxelizeGeometry(vertices, indices, indexCount / 3, worldTM);
				triangleCount += vertexCount / 3;
			}
			return triangleCount;
		}
		break;
	case GEOM_HEIGHTFIELD:
		return VoxelizeTerrain(geometry, worldTM);
		break;
	}

	return triangleCount;
}
#pragma warning (pop)
}
