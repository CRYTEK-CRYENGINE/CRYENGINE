// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __FloorHeightCache_h__
#define __FloorHeightCache_h__

class FloorHeightCache
{
public:
	void   Reset();
	void   SetHeight(const Vec3& position, float height);
	bool   GetHeight(const Vec3& position, float& height) const;

	Vec3   GetCellCenter(const Vec3& position) const;
	AABB   GetAABB(Vec3 position) const;

	void   Draw(const ColorB& colorGood, const ColorB& colorBad);
	size_t GetMemoryUsage() const;

private:
	static const float CellSize;
	static const float InvCellSize;
	static const float HalfCellSize;

	Vec3 GetCellCenter(uint16 x, uint16 y) const
	{
		return Vec3(x * CellSize + HalfCellSize, y * CellSize + HalfCellSize, 0.0f);
	}

	struct CacheEntry
	{
		CacheEntry()
		{
		}

		CacheEntry(Vec3 position, float _height = 0.0f)
			: height(_height)
		{
			x = (uint16)(position.x * InvCellSize);
			y = (uint16)(position.y * InvCellSize);
			z = (uint16)(position.z);
		}

		bool operator<(const CacheEntry& rhs) const
		{
			if (x != rhs.x)
				return x < rhs.x;

			if (y != rhs.y)
				return y < rhs.y;

			return z < rhs.z;
		}

		uint16 x;
		uint16 y;
		uint16 z;

		float  height;
	};

	typedef std::set<CacheEntry> FloorHeights;
	FloorHeights m_floorHeights;
};

#endif
