// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>

#include <CryMath/Cry_Math.h>
#include <Navigation/MNM/MNMUtils.h>

using namespace MNM;

TEST(CryMNMTest, IntersectSegmentSegment)
{
	const vector2_t A(real_t(0.0f), real_t(0.0f));
	const vector2_t B(real_t(0.0f), real_t(1.0f));
	const vector2_t C(real_t(1.0f), real_t(1.0f));
	const vector2_t D(real_t(1.0f), real_t(0.0f));
	const vector2_t E(real_t(0.5f), real_t(0.5f));

	real_t s, t;

	REQUIRE(Utils::DetailedIntersectSegmentSegment(A, B, A, B, s, t) == Utils::eIR_ParallelOrCollinearSegments);
	REQUIRE(Utils::DetailedIntersectSegmentSegment(A, B, C, D, s, t) == Utils::eIR_ParallelOrCollinearSegments);
	REQUIRE(Utils::DetailedIntersectSegmentSegment(A, B, E, D, s, t) == Utils::eIR_NoIntersection);
	REQUIRE(Utils::DetailedIntersectSegmentSegment(A, C, B, D, s, t) == Utils::eIR_Intersection);
	REQUIRE(Utils::DetailedIntersectSegmentSegment(A, B, A, C, s, t) == Utils::eIR_Intersection);
	REQUIRE(s == 0); REQUIRE(t == 0);
}

TEST(CryMNMTest, ProjectPointOnTriangleVertical)
{
	const vector3_t upOffset(real_t(0.0f), real_t(0.0f), real_t(2.0f));
	const vector3_t downOffset(real_t(0.0f), real_t(0.0f), real_t(-2.0f));
	
	// Triangle vertices
	const vector3_t A(real_t(0.0f), real_t(0.0f), real_t(0.0f));
	const vector3_t B(real_t(1.0f), real_t(0.0f), real_t(1.0f));
	const vector3_t C(real_t(0.0f), real_t(1.0f), real_t(0.0f));
	
	// Point above A
	const vector3_t D(real_t(0.0f), real_t(0.0f), real_t(1.0f));

	// Point 'inside' the triangle (when projected to xy-plane)
	const vector3_t T(real_t(0.5f), real_t(0.1f), real_t(0.0f));
	
	// Points in the middle of the triangle edges
	const vector3_t S1((A + B) * real_t(0.5f));
	const vector3_t S2((B + C) * real_t(0.5f));
	const vector3_t S3((C + A) * real_t(0.5f));
	
	vector3_t projected;

	// Point above the triangle
	REQUIRE(Utils::ProjectPointOnTriangleVertical(T + upOffset, A, B, C, projected));
	REQUIRE(projected.x == real_t(0.5f));
	REQUIRE(projected.y == real_t(0.1f));
	REQUIRE(projected.z == real_t(0.5f));

	// Point below the triangle
	REQUIRE(Utils::ProjectPointOnTriangleVertical(T + downOffset, A, B, C, projected));
	REQUIRE(projected.x == real_t(0.5f));
	REQUIRE(projected.y == real_t(0.1f));
	REQUIRE(projected.z == real_t(0.5f));

	// Point above the triangle's edge
	REQUIRE(Utils::ProjectPointOnTriangleVertical(S1 + upOffset, A, B, C, projected));
	REQUIRE(projected.x == S1.x);
	REQUIRE(projected.y == S1.y);
	REQUIRE(projected.z == S1.z);

	REQUIRE(Utils::ProjectPointOnTriangleVertical(S2 + upOffset, A, B, C, projected));
	REQUIRE(projected.x == S2.x);
	REQUIRE(projected.y == S2.y);
	REQUIRE(projected.z == S2.z);

	REQUIRE(Utils::ProjectPointOnTriangleVertical(S3 + upOffset, A, B, C, projected));
	REQUIRE(projected.x == S3.x);
	REQUIRE(projected.y == S3.y);
	REQUIRE(projected.z == S3.z);

	// Point at vertex
	REQUIRE(Utils::ProjectPointOnTriangleVertical(A + upOffset, A, B, C, projected));
	REQUIRE(projected.x == A.x);
	REQUIRE(projected.y == A.y);
	REQUIRE(projected.z == A.z);

	// Degenerate triangle tests
	REQUIRE(!Utils::ProjectPointOnTriangleVertical(S3 + upOffset, A, C, D, projected));
	REQUIRE(!Utils::ProjectPointOnTriangleVertical(A + upOffset, A, A, B, projected));
	
	REQUIRE(Utils::ProjectPointOnTriangleVertical(A + upOffset, A, A, A, projected)); // All points have same z value, z value for projected point can be set
	REQUIRE(projected.z == A.z);
}
