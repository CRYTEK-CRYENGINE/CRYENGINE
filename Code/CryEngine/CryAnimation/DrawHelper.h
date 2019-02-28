// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Skeleton {
class CPoseData;
}

namespace DrawHelper
{

void DepthTest(bool bTest);
void DepthWrite(bool bWrite);

struct Segment
{
	Vec3   positions[2];
	ColorB colors[2];

	Segment()
	{
		positions[0] = Vec3(0.0f);
		positions[1] = Vec3(0.0f);

		colors[0] = ColorB(0xff, 0xff, 0xff, 0xff);
		colors[1] = ColorB(0xff, 0xff, 0xff, 0xff);
	}
};
void Draw(const Segment& segment);

struct Sphere
{
	Vec3   position;
	float  radius;
	ColorB color;

	Sphere() :
		position(0.0f),
		radius(0.5f),
		color(0xff, 0xff, 0xff, 0xff)
	{
	}
};
void Draw(const Sphere& sphere);

struct Cylinder
{
	Vec3   position;
	Vec3   direction;
	float  radius;
	float  height;
	ColorB color;

	Cylinder() :
		position(0.0f),
		direction(0.0f, 0.0f, 1.0f),
		radius(0.5f),
		height(1.0f),
		color(0xff, 0xff, 0xff, 0xff)
	{
	}
};
void Draw(const Cylinder& cylinder);

struct Cone
{
	Vec3   position;
	Vec3   direction;
	float  radius;
	float  height;
	ColorB color;

	Cone() :
		position(0.0f),
		direction(0.0f, 0.0f, 1.0f),
		radius(0.5f),
		height(1.0f),
		color(0xff, 0xff, 0xff, 0xff)
	{
	}
};
void Draw(const Cone& cylinder);

void Frame(const Vec3& position, const Vec3& axisX, const Vec3& axisY, const Vec3& axisZ);
void Frame(const Matrix34& location, const Vec3& scale);
void Frame(const QuatT& location, const Vec3& scale);

void Arrow(const QuatT& location, const Vec3& direction, float length, ColorB color);

void CurvedArrow(const QuatT& location, float moveSpeed, float travelAngle, float turnSpeed, float slope, ColorB color);

void Pose(const CDefaultSkeleton& rDefaultSkeleton, const Skeleton::CPoseData& poseData, const QuatT& location, ColorB color);

} // namespace DrawHelper
