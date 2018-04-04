// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "FloorHeightCache.h"
#include "DebugDrawContext.h"

const float FloorHeightCache::CellSize = 0.25f;
const float FloorHeightCache::InvCellSize = 1.0f / FloorHeightCache::CellSize;
const float FloorHeightCache::HalfCellSize = FloorHeightCache::CellSize * 0.5f;

void FloorHeightCache::Reset()
{
	m_floorHeights.clear();
}

void FloorHeightCache::SetHeight(const Vec3& position, float height)
{
	m_floorHeights.insert(CacheEntry(position, height));
}

bool FloorHeightCache::GetHeight(const Vec3& position, float& height) const
{
	FloorHeights::const_iterator it = m_floorHeights.find(CacheEntry(position));
	if (it == m_floorHeights.end())
		return false;

	height = it->height;

	return true;
}

Vec3 FloorHeightCache::GetCellCenter(const Vec3& position) const
{
	float x = floor_tpl(position.x * InvCellSize) * CellSize + HalfCellSize;
	float y = floor_tpl(position.y * InvCellSize) * CellSize + HalfCellSize;

	return Vec3(x, y, position.z);
}

AABB FloorHeightCache::GetAABB(Vec3 position) const
{
	float x = floor_tpl(position.x * InvCellSize) * CellSize;
	float y = floor_tpl(position.y * InvCellSize) * CellSize;

	return AABB(Vec3(x, y, position.z), Vec3(x + CellSize, y + CellSize, position.z));
}

void FloorHeightCache::Draw(const ColorB& colorGood, const ColorB& colorBad)
{
	FloorHeights::iterator it = m_floorHeights.begin();
	FloorHeights::iterator end = m_floorHeights.end();

	for (; it != end; ++it)
	{
		const CacheEntry& entry = *it;

		const ColorB color = entry.height < FLT_MAX ? colorGood : colorBad;
		Vec3 center = GetCellCenter(entry.x, entry.y);
		center.z = entry.height < FLT_MAX ? entry.height : (float)entry.z + 0.5f;

		const Vec3 v0 = Vec3(center.x - HalfCellSize, center.y - HalfCellSize, center.z + 0.015f);
		const Vec3 v1 = Vec3(center.x - HalfCellSize, center.y + HalfCellSize, center.z + 0.015f);
		const Vec3 v2 = Vec3(center.x + HalfCellSize, center.y + HalfCellSize, center.z + 0.015f);
		const Vec3 v3 = Vec3(center.x + HalfCellSize, center.y - HalfCellSize, center.z + 0.015f);

		CDebugDrawContext dc;

		dc->SetBackFaceCulling(false);
		dc->SetDepthWrite(false);
		dc->DrawTriangle(v0, color, v1, color, v2, color);
		dc->DrawTriangle(v0, color, v2, color, v3, color);
	}
}

size_t FloorHeightCache::GetMemoryUsage() const
{
	// not sure if its only 3 pointers overhead for each node?
	return m_floorHeights.size() * (sizeof(FloorHeights::value_type) + sizeof(void*) * 3);
}
