// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CoverSurface_h__
#define __CoverSurface_h__
#pragma once

#include "Cover.h"
#include "CoverPath.h"

class CoverSurface
{
	typedef ICoverSampler::Sample Sample;
public:
	CoverSurface();
	CoverSurface(const ICoverSystem::SurfaceInfo& surfaceInfo);

	static void FreeStaticData()
	{
		stl::free_container(s_simplifiedPoints);
	}

	struct Segment
	{
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wconstant-conversion"
#endif

#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic ignored "-Woverflow"
	#else
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Woverflow"
	#endif
#endif
		Segment()
		{
			normal = ZERO;
			leftIdx = std::numeric_limits<uint16>::max();
			rightIdx = std::numeric_limits<uint16>::max();
			flags = 0;
			length = 0;
		}
#if defined(__clang__)
	#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic error "-Woverflow"
	#else
		#pragma GCC diagnostic pop
	#endif
#endif

		Segment(const Vec3& n, float len, uint16 left, uint16 right, uint16 _flags)
			: normal(n)
			, length(len)
			, leftIdx(left)
			, rightIdx(right)
			, flags(_flags)
		{
		}

		enum Flags
		{
			Dynamic  = 1 << 0,
			Disabled = 1 << 1,
		};

		Vec3   normal;
		float  length;

		uint16 leftIdx  : 14;
		uint16 rightIdx : 14;
		uint16 flags    : 4;
	};

	bool           IsValid() const;
	void           Clear();
	void           Swap(CoverSurface& other);

	uint32         GetSampleCount() const;
	bool           GetSurfaceInfo(ICoverSystem::SurfaceInfo* surfaceInfo) const;

	uint32         GetSegmentCount() const;

	Segment&       GetSegment(uint16 segmentIdx);
	const Segment& GetSegment(uint16 segmentIdx) const;

	const AABB&    GetAABB() const;
	const uint32   GetFlags() const;

	void           ReserveSamples(uint32 sampleCount);
	void           AddSample(const Sample& sample);
	void           Generate();
	void           GenerateLocations();

	uint32         GetLocationCount() const;
	Vec3           GetLocation(uint16 location, float offset = 0.0f, float* height = 0, Vec3* normal = 0) const;

	bool           IsPointInCover(const Vec3& eye, const Vec3& point) const;
	bool           IsCircleInCover(const Vec3& eye, const Vec3& center, float radius) const;
	bool           CalculatePathCoverage(const Vec3& eye, const CoverPath& coverPath, CoverInterval* interval) const;
	bool           GetCoverOcclusionAt(const Vec3& eye, const Vec3& point, float* heightSq) const;
	bool           GetCoverOcclusionAt(const Vec3& eye, const Vec3& center, float radius, float* heightSq) const;

	bool           GenerateCoverPath(float distanceToCover, CoverPath* path, bool skipSimplify = false) const;

	void           DebugDraw() const;

private:
	typedef std::vector<Vec3> Points;
	void SimplifyCoverPath(Points& points) const;

	static Points s_simplifiedPoints;

	bool ILINE IsPointBehindSegment(const Vec3& eye, const Vec3& point, const Segment& segment) const
	{
		const Sample& left = m_samples[segment.leftIdx];
		const Sample& right = m_samples[segment.rightIdx];

		Lineseg ray(eye, point);
		Lineseg seg(left.position, right.position);

		if (!Overlap::Lineseg_Lineseg2D(ray, seg))
			return false;

		const float BottomGuard = 2.0f;

		Vec3 leftBottom = left.position;
		leftBottom.z -= BottomGuard;
		Vec3 rightBottom = right.position;
		rightBottom.z -= BottomGuard;

		Vec3 leftTop = left.position;
		leftTop.z += static_cast<float>(left.GetHeightInteger()) * Sample::GetHeightToFloatConverter();

		Vec3 hit;

		if (!Intersect::Lineseg_Triangle(ray, rightBottom, leftTop, leftBottom, hit))
		{
			Vec3 rightTop = right.position;
			rightTop.z += static_cast<float>(right.GetHeightInteger()) * Sample::GetHeightToFloatConverter();

			if (!Intersect::Lineseg_Triangle(ray, rightBottom, rightTop, leftTop, hit))
				return false;
		}

		return true;
	}

	bool ILINE GetOcclusionBehindSegment(const Vec3& eye, const Vec3& point, const Segment& segment, float* heightSq) const
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		const Sample& left = m_samples[segment.leftIdx];
		const Sample& right = m_samples[segment.rightIdx];

		Lineseg ray(eye, point);
		Lineseg seg(left.position, right.position);

		float a;
		float b;

		if (!Intersect::Lineseg_Lineseg2D(ray, seg, a, b))
			return false;

		const float BottomGuard = 2.0f;

		Vec3 leftBottom = left.position;
		leftBottom.z -= BottomGuard;
		Vec3 rightBottom = right.position;
		rightBottom.z -= BottomGuard;

		Vec3 leftTop = left.position;

		float leftHeight = static_cast<float>(left.GetHeightInteger()) * Sample::GetHeightToFloatConverter();
		float rightHeight = static_cast<float>(right.GetHeightInteger()) * Sample::GetHeightToFloatConverter();
		leftTop.z += leftHeight;

		Vec3 hit;

		if (!Intersect::Lineseg_Triangle(ray, rightBottom, leftTop, leftBottom, hit))
		{
			Vec3 rightTop = right.position;
			rightTop.z += rightHeight;

			if (!Intersect::Lineseg_Triangle(ray, rightBottom, rightTop, leftTop, hit))
				return false;
		}

		float h = leftHeight + b * (rightHeight - leftHeight);

		Vec3 top = left.position + b * (right.position - left.position);
		top.z += h;

		Vec3 dir = (top - eye) * 9999.0f;

		float t;
		*heightSq = Distance::Point_LinesegSq(point, Lineseg(eye, eye + dir), t);

		return true;
	}

	bool IntersectCoverPlane(const Vec3& origin, const Vec3& length, const Plane& plane, Vec3* point, float* t) const;
	void FindCoverPlanes(const Vec3& eye, Plane& left, Plane& right) const;

	typedef std::vector<Sample> Samples;
	Samples m_samples;

	typedef std::vector<Segment> Segments;
	Segments m_segments;

	struct Location
	{
		enum
		{
			IntegerPartBitCount = 12,
		};

		enum Flags
		{
			LeftEdge  = 1 << 0,
			RightEdge = 1 << 1,
		};

		uint16 segmentIdx;
		uint16 offset;
		uint16 height;
		uint16 flags;

		Location()
		{
		}

		Location(const uint16 _segmentIdx, float _offset, float _height)
			: segmentIdx(_segmentIdx)
		{
			SetOffset(_offset);
			SetHeight(_height);
		}

		ILINE void SetOffset(float _offset)
		{
			assert((0.0f <= _offset) && (_offset <= 1.0f));
			offset = (uint16)(_offset * (float)0xffff);
		}

		ILINE float GetOffset() const
		{
			return offset * (1.0f / (float)0xffff);
		}

		ILINE void SetHeight(float _height)
		{
			height = (uint16)(_height * ((1 << IntegerPartBitCount) - 1));
		}

		ILINE float GetHeight() const
		{
			return height * (1.0f / (float)((1 << IntegerPartBitCount) - 1));
		}
	};

	typedef std::vector<Location> Locations;
	Locations m_locations;

	AABB      m_aabb;
	uint32    m_flags;
};

#endif //__CoverSurface_h__
