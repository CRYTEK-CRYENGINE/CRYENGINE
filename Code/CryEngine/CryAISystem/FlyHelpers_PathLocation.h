// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLY_HELPERS__PATH_LOCATION__H__
#define __FLY_HELPERS__PATH_LOCATION__H__

#include "FlyHelpers_Path.h"

namespace FlyHelpers
{

struct PathLocation
{
	PathLocation()
		: segmentIndex(0)
		, normalizedSegmentPosition(0)
	{
	}

	bool operator<(const PathLocation& rhs) const
	{
		return ((segmentIndex < rhs.segmentIndex) || (segmentIndex == rhs.segmentIndex && normalizedSegmentPosition < rhs.normalizedSegmentPosition));
	}

	bool operator<=(const PathLocation& rhs) const
	{
		return ((segmentIndex < rhs.segmentIndex) || (segmentIndex == rhs.segmentIndex && normalizedSegmentPosition <= rhs.normalizedSegmentPosition));
	}

	bool operator==(const PathLocation& rhs) const
	{
		return (segmentIndex == rhs.segmentIndex && normalizedSegmentPosition == rhs.normalizedSegmentPosition);
	}

	Vec3 CalculatePathPosition(const Path& path) const
	{
		const Lineseg segment = path.GetSegment(segmentIndex);
		return segment.GetPoint(normalizedSegmentPosition);
	}

	Vec3 CalculatePathPositionCatmullRomLooping(const Path& path) const
	{
		const size_t segmentCount = path.GetSegmentCount();
		const size_t i0 = (segmentCount + segmentIndex - 1) % segmentCount;
		const size_t i1 = segmentIndex;
		const size_t i2 = segmentIndex + 1;
		const size_t i3 = (segmentIndex + 2) % segmentCount;

		return CalculatePathPositionCatmullRomImpl(path, i0, i1, i2, i3);
	}

	Vec3 CalculatePathPositionCatmullRom(const Path& path) const
	{
		const size_t segmentCount = path.GetSegmentCount();
		const size_t i0 = (0 < segmentIndex) ? segmentIndex - 1 : 0;
		const size_t i1 = segmentIndex;
		const size_t i2 = segmentIndex + 1;
		const size_t i3 = min(segmentIndex + 2, segmentCount);

		return CalculatePathPositionCatmullRomImpl(path, i0, i1, i2, i3);
	}

	float CalculateDistanceToPreviousPathPoint(const Path& path) const
	{
		return (path.GetSegmentLength(segmentIndex) * normalizedSegmentPosition);
	}

	float CalculateDistanceToNextPathPoint(const Path& path) const
	{
		return (path.GetSegmentLength(segmentIndex) * (1.0f - normalizedSegmentPosition));
	}

	float CalculateDistanceAlongPathTo(const Path& path, const PathLocation& other) const
	{
		if (other < *this)
		{
			return other.CalculateDistanceAlongPathTo(path, *this);
		}
		else
		{
			if (segmentIndex == other.segmentIndex)
			{
				return path.GetSegmentLength(segmentIndex) * (other.normalizedSegmentPosition - normalizedSegmentPosition);
			}
			else
			{
				float distance = CalculateDistanceToNextPathPoint(path);
				for (size_t i = segmentIndex + 1; i < other.segmentIndex; ++i)
				{
					distance += path.GetSegmentLength(i);
				}
				distance += other.CalculateDistanceToPreviousPathPoint(path);
				return distance;
			}
		}
	}

	float CalculateDistanceAlongPathInTheCurrentDirectionTo(const Path& path, const PathLocation& other) const
	{
		const float distance = CalculateDistanceAlongPathTo(path, other);
		if (*this <= other)
		{
			return distance;
		}
		else
		{
			const float totalDistance = path.GetTotalPathDistance();
			return totalDistance - distance;
		}
	}

	size_t segmentIndex;
	float  normalizedSegmentPosition;

private:
	Vec3 CalculatePathPositionCatmullRomImpl(const Path& path, const size_t i0, const size_t i1, const size_t i2, const size_t i3) const
	{
		const float x = normalizedSegmentPosition;
		const float xx = x * x;
		const float xxx = xx * x;

		const Vec3& p0 = path.GetPoint(i0);
		const Vec3& p1 = path.GetPoint(i1);
		const Vec3& p2 = path.GetPoint(i2);
		const Vec3& p3 = path.GetPoint(i3);

		return 0.5f * ((2 * p1) + (-p0 + p2) * x + (2 * p0 - 5 * p1 + 4 * p2 - p3) * xx + (-p0 + 3 * p1 - 3 * p2 + p3) * xxx);
	}
};

}

#endif
