// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CoverSampler_h__
#define __CoverSampler_h__
#pragma once

#include "Cover.h"
#include "CoverSurface.h"
#include "CoverPath.h"

class CoverSampler
	: public ICoverSampler
{
public:
	CoverSampler();
	virtual void                         Release();

	virtual ESamplerState                StartSampling(const ICoverSampler::Params& params);
	virtual ESamplerState                Update(float timeLimitPerFrame, float timeLimitTotal);
	virtual ESamplerState                GetState() const;

	virtual uint32                       GetSampleCount() const;
	virtual const ICoverSampler::Sample* GetSamples() const;
	virtual const AABB& GetAABB() const;
	virtual uint32      GetSurfaceFlags() const;
	virtual void        DebugDraw() const;

private:
	virtual ~CoverSampler();

	enum SurfaceType
	{
		SolidSurface,
		DynamicSurface,
	};

	struct IntersectionResult
	{
		Vec3        position;
		Vec3        normal;

		float       distance;
		SurfaceType surfaceType;
	};

	enum EInternalState
	{
		SamplerReady = 0,
		SamplerStarting,
		SamplerLeft,
		SamplerLeftEdge,
		SamplerRight,
		SamplerRightEdge,
		SamplerSimplifySurface,
		SamplerFinished,
		SamplerError,
	};

	enum EDirection
	{
		Left = 0,
		Right,
	};

	bool        OverlapCylinder(const Vec3& bottomCenter, const Vec3& dir, float length, float radius, uint32 collisionFlags) const;
	bool        IntersectSweptSphere(const Vec3& center, const Vec3& dir, float radius, IPhysicalEntity** entityList,
	                                 size_t entityCount, IntersectionResult* result) const;
	bool        IntersectRay(const Vec3& center, const Vec3& dir, uint32 collisionFlags, IntersectionResult* result) const;

	SurfaceType GetSurfaceType(IPhysicalEntity* physicalEntity, int ipart) const;

	float       SampleHeightInterval(const Vec3& position, const Vec3& dir, float interval, IPhysicalEntity** entityList,
	                                 size_t entityCount, float* depth, Vec3* normal, SurfaceType* surfaceType);
	float       SampleHeight(const Vec3& position, float heightInterval, float maxHeight, float* depth,
	                         Vec3* averageNormal, SurfaceType* surfaceType);
	bool        SampleFloor(const Vec3& position, float searchHeight, float searchRadius, Vec3* floor, Vec3* normal);
	float       SampleWidthInterval(EDirection direction, float interval, float maxHeight, Vec3& floor,
	                                Vec3& floorNormal, Vec3& averageNormal);
	bool        SampleWidth(EDirection direction, float widthInterval, float maxHeight);
	bool        CheckClearance(const Vec3& floor, const Vec3& floorNormal, const Vec3& dir, float maxHeight);
	bool        Adjust(const Vec3& floor, const Vec3& floorNormal, const Vec3& wantedDirection, float weight);
	bool        CanSampleHeight(const Vec3& floor, const Vec3& direction, EDirection samplingDirection) const;
	bool        ValidateSample(const Vec3& floor, const Vec3& direction, const Sample& sample,
	                           EDirection samplingDirection, float* deltaSq) const;
	void        Simplify();

	ESamplerState m_externalState;
	Params        m_params;

	typedef std::deque<CoverSampler::Sample> TempSamples;
	TempSamples m_tempSamples;

	typedef std::vector<ICoverSampler::Sample> Samples;
	Samples m_samples;

	struct SamplingState
	{
		SamplingState()
			: origin(ZERO)
			, direction(ZERO)
			, right(ZERO)
			, floor(ZERO)
			, solidDirection(ZERO)
			, solidRight(ZERO)
			, solidFloor(ZERO)
			, initialDirection(ZERO)
			, initialRight(ZERO)
			, initialFloor(ZERO)
			, internalState(SamplerReady)
			, length(0.0f)
			, depth(0.0f)
			, edgeHeight(0.0f)
			, limitLength(0.0f)
			, surfaceNormal(ZERO)
			, looped(false)
			, totalTime((int64)0ll)
			, originalSurfaceSamples(0)
			, updateCount(0)
			, pwiCount(0)
			, rwiCount(0)
		{
		}

		Vec3           origin;

		Vec3           direction;
		Vec3           right;
		Vec3           floor;

		Vec3           solidDirection; // solid means it's not at the edge
		Vec3           solidRight;
		Vec3           solidFloor;

		Vec3           initialDirection;
		Vec3           initialRight;
		Vec3           initialFloor;

		EInternalState internalState;

		float          length;
		float          depth;
		float          edgeHeight;
		float          limitLength;

		Vec3           surfaceNormal;
		bool           looped;

		CTimeValue     totalTime;
		uint32         originalSurfaceSamples;
		uint32         updateCount;
		mutable uint32 pwiCount;
		mutable uint32 rwiCount;
	};

	SamplingState m_state;
	AABB          m_aabb;
	uint32        m_flags;
};

#endif //__CoverSampler_h__
