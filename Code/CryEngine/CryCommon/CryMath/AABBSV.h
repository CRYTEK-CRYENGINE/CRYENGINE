// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:AABBSV.h
//	Description: shadow volume AABB functionality for overlap testings
//
//	History:
//	-Feb 15,2004:Created by Michael Glueck, code provided by Ivo Herzeg
//
//////////////////////////////////////////////////////////////////////

#ifndef AABBSV_H
#define AABBSV_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Cry_Geo.h"

struct Shadowvolume
{
	uint32 sideamount;
	uint32 nplanes;

	Plane  oplanes[10];
};

namespace NAABB_SV
{
//! Calculate a ShadowVolume using an AABB and a point-light.
//! The planes of the AABB facing away from the point-light are the far-planes
//! of the ShadowVolume. There can be 3-6 far-planes.
void AABB_ReceiverShadowVolume(const Vec3& PointLight, const AABB& Occluder, Shadowvolume& sv);

//! Calculate a ShadowVolume using an AABB and a point-light.
//! The planes of the AABB facing the point-light are the near-planes of the ShadowVolume.
//! There can be 1-3 near-planes. The far-plane is defined by lightrange.
void AABB_ShadowVolume(const Vec3& PointLight, const AABB& Occluder, Shadowvolume& sv, f32 lightrange);

//! This is the "fast" version to check if an AABB is overlapping a shadowvolume.
bool Is_AABB_In_ShadowVolume(const Shadowvolume& sv, const AABB& Receiver);

//! This is the "hierarchical" check.
char Is_AABB_In_ShadowVolume_hierarchical(const Shadowvolume& sv, const AABB& Receiver);
}

inline void NAABB_SV::AABB_ReceiverShadowVolume(const Vec3& PointLight, const AABB& Occluder, Shadowvolume& sv)
{
	sv.sideamount = 0;
	sv.nplanes = 0;

	//! Check if PointLight is in front of any occluder plane or inside occluder.
	uint32 front = 0;
	if (PointLight.x < Occluder.min.x)  front |= 0x01;
	if (PointLight.x > Occluder.max.x)  front |= 0x02;
	if (PointLight.y < Occluder.min.y)  front |= 0x04;
	if (PointLight.y > Occluder.max.y)  front |= 0x08;
	if (PointLight.z < Occluder.min.z)  front |= 0x10;
	if (PointLight.z > Occluder.max.z)  front |= 0x20;

	sv.sideamount = BoxSides[(front << 3) + 7];

	uint32 back = front ^ 0x3f;
	if (back & 0x01) { sv.oplanes[sv.nplanes].SetPlane(Vec3(-1, +0, +0), Occluder.min); sv.nplanes++; }
	if (back & 0x02) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+1, +0, +0), Occluder.max); sv.nplanes++; }
	if (back & 0x04) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, -1, +0), Occluder.min); sv.nplanes++; }
	if (back & 0x08) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, +1, +0), Occluder.max); sv.nplanes++; }
	if (back & 0x10) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, +0, -1), Occluder.min); sv.nplanes++; }
	if (back & 0x20) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, +0, +1), Occluder.max); sv.nplanes++; }

	if (front == 0) return;   // Light is inside occluder.

	// All 8 vertices of a AABB.
	Vec3 o[8] =
	{
		Vec3(Occluder.min.x, Occluder.min.y, Occluder.min.z),
		Vec3(Occluder.max.x, Occluder.min.y, Occluder.min.z),
		Vec3(Occluder.min.x, Occluder.max.y, Occluder.min.z),
		Vec3(Occluder.max.x, Occluder.max.y, Occluder.min.z),
		Vec3(Occluder.min.x, Occluder.min.y, Occluder.max.z),
		Vec3(Occluder.max.x, Occluder.min.y, Occluder.max.z),
		Vec3(Occluder.min.x, Occluder.max.y, Occluder.max.z),
		Vec3(Occluder.max.x, Occluder.max.y, Occluder.max.z)
	};

	//! Find the silhouette-vertices of the occluder-AABB.
	uint32 p0 = BoxSides[(front << 3) + 0];
	uint32 p1 = BoxSides[(front << 3) + 1];
	uint32 p2 = BoxSides[(front << 3) + 2];
	uint32 p3 = BoxSides[(front << 3) + 3];
	uint32 p4 = BoxSides[(front << 3) + 4];
	uint32 p5 = BoxSides[(front << 3) + 5];

	float a;
	if (sv.sideamount == 4)
	{
		//sv.oplanes[sv.nplanes+0]	=	Plane::CreatePlane( o[p0],o[p1], PointLight );
		//sv.oplanes[sv.nplanes+1]	=	Plane::CreatePlane( o[p1],o[p2], PointLight );
		//sv.oplanes[sv.nplanes+2]	=	Plane::CreatePlane( o[p2],o[p3], PointLight );
		//sv.oplanes[sv.nplanes+3]	=	Plane::CreatePlane( o[p3],o[p0], PointLight );
		sv.sideamount = 0;
		a = (o[p1] - o[p0]) | (o[p0] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p0], o[p1], PointLight); sv.sideamount++; }
		a = (o[p2] - o[p1]) | (o[p1] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p1], o[p2], PointLight); sv.sideamount++; }
		a = (o[p3] - o[p2]) | (o[p2] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p2], o[p3], PointLight); sv.sideamount++; }
		a = (o[p0] - o[p3]) | (o[p3] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p3], o[p0], PointLight); sv.sideamount++; }
	}

	if (sv.sideamount == 6)
	{
		//sv.oplanes[sv.nplanes+0]	=	Plane::CreatePlane( o[p0],o[p1], PointLight );
		//sv.oplanes[sv.nplanes+1]	=	Plane::CreatePlane( o[p1],o[p2], PointLight );
		//sv.oplanes[sv.nplanes+2]	=	Plane::CreatePlane( o[p2],o[p3], PointLight );
		//sv.oplanes[sv.nplanes+3]	=	Plane::CreatePlane( o[p3],o[p4], PointLight );
		//sv.oplanes[sv.nplanes+4]	=	Plane::CreatePlane( o[p4],o[p5], PointLight );
		//sv.oplanes[sv.nplanes+5]	=	Plane::CreatePlane( o[p5],o[p0], PointLight );

		sv.sideamount = 0;
		a = (o[p1] - o[p0]) | (o[p0] - PointLight);
		assert(sv.nplanes + sv.sideamount < 10);
		PREFAST_ASSUME(sv.nplanes + sv.sideamount < 10);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p0], o[p1], PointLight); sv.sideamount++; }
		a = (o[p2] - o[p1]) | (o[p1] - PointLight);
		assert(sv.nplanes + sv.sideamount < 10);
		PREFAST_ASSUME(sv.nplanes + sv.sideamount < 10);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p1], o[p2], PointLight); sv.sideamount++; }
		a = (o[p3] - o[p2]) | (o[p2] - PointLight);
		assert(sv.nplanes + sv.sideamount < 10);
		PREFAST_ASSUME(sv.nplanes + sv.sideamount < 10);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p2], o[p3], PointLight); sv.sideamount++; }
		a = (o[p4] - o[p3]) | (o[p3] - PointLight);
		assert(sv.nplanes + sv.sideamount < 10);
		PREFAST_ASSUME(sv.nplanes + sv.sideamount < 10);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p3], o[p4], PointLight); sv.sideamount++; }
		a = (o[p5] - o[p4]) | (o[p4] - PointLight);
		assert(sv.nplanes + sv.sideamount < 10);
		PREFAST_ASSUME(sv.nplanes + sv.sideamount < 10);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p4], o[p5], PointLight); sv.sideamount++; }
		a = (o[p0] - o[p5]) | (o[p5] - PointLight);
		assert(sv.nplanes + sv.sideamount < 10);
		PREFAST_ASSUME(sv.nplanes + sv.sideamount < 10);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p5], o[p0], PointLight); sv.sideamount++; }

	}
}

inline void NAABB_SV::AABB_ShadowVolume(const Vec3& PointLight, const AABB& Occluder, Shadowvolume& sv, f32 lightrange)
{
	sv.sideamount = 0;
	sv.nplanes = 0;

	//! Heck if PointLight is in front of any occluder plane or inside occluder.
	uint32 front = 0;
	if (PointLight.x < Occluder.min.x)  front |= 0x01;
	if (PointLight.x > Occluder.max.x)  front |= 0x02;
	if (PointLight.y < Occluder.min.y)  front |= 0x04;
	if (PointLight.y > Occluder.max.y)  front |= 0x08;
	if (PointLight.z < Occluder.min.z)  front |= 0x10;
	if (PointLight.z > Occluder.max.z)  front |= 0x20;
	if (front == 0) return; // Light is inside occluder.
	sv.sideamount = BoxSides[(front << 3) + 7];

	if (front & 0x01) { sv.oplanes[sv.nplanes].SetPlane(Vec3(-1, +0, +0), Occluder.min); sv.nplanes++; }
	if (front & 0x02) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+1, +0, +0), Occluder.max); sv.nplanes++; }
	if (front & 0x04) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, -1, +0), Occluder.min); sv.nplanes++; }
	if (front & 0x08) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, +1, +0), Occluder.max); sv.nplanes++; }
	if (front & 0x10) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, +0, -1), Occluder.min); sv.nplanes++; }
	if (front & 0x20) { sv.oplanes[sv.nplanes].SetPlane(Vec3(+0, +0, +1), Occluder.max); sv.nplanes++; }

	// All 8 vertices of a AABB.
	Vec3 o[8] =
	{
		Vec3(Occluder.min.x, Occluder.min.y, Occluder.min.z),
		Vec3(Occluder.max.x, Occluder.min.y, Occluder.min.z),
		Vec3(Occluder.min.x, Occluder.max.y, Occluder.min.z),
		Vec3(Occluder.max.x, Occluder.max.y, Occluder.min.z),
		Vec3(Occluder.min.x, Occluder.min.y, Occluder.max.z),
		Vec3(Occluder.max.x, Occluder.min.y, Occluder.max.z),
		Vec3(Occluder.min.x, Occluder.max.y, Occluder.max.z),
		Vec3(Occluder.max.x, Occluder.max.y, Occluder.max.z)
	};

	//! Find the silhouette-vertices of the occluder-AABB.
	uint32 p0 = BoxSides[(front << 3) + 0];
	uint32 p1 = BoxSides[(front << 3) + 1];
	uint32 p2 = BoxSides[(front << 3) + 2];
	uint32 p3 = BoxSides[(front << 3) + 3];
	uint32 p4 = BoxSides[(front << 3) + 4];
	uint32 p5 = BoxSides[(front << 3) + 5];

	// The new center-position in world-space.
	Vec3 MiddleOfOccluder = (Occluder.max + Occluder.min) * 0.5f;
	sv.oplanes[sv.nplanes] = Plane::CreatePlane((MiddleOfOccluder - PointLight).GetNormalized(), (MiddleOfOccluder - PointLight).GetNormalized() * lightrange + PointLight);
	sv.nplanes++;

	float a;
	if (sv.sideamount == 4)
	{
		sv.sideamount = 0;
		a = (o[p1] - o[p0]) | (o[p0] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p0], o[p1], PointLight); sv.sideamount++; }
		a = (o[p2] - o[p1]) | (o[p1] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p1], o[p2], PointLight); sv.sideamount++; }
		a = (o[p3] - o[p2]) | (o[p2] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p2], o[p3], PointLight); sv.sideamount++; }
		a = (o[p0] - o[p3]) | (o[p3] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p3], o[p0], PointLight); sv.sideamount++; }
	}

	if (sv.sideamount == 6)
	{
		sv.sideamount = 0;
		a = (o[p1] - o[p0]) | (o[p0] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p0], o[p1], PointLight); sv.sideamount++; }
		a = (o[p2] - o[p1]) | (o[p1] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p1], o[p2], PointLight); sv.sideamount++; }
		a = (o[p3] - o[p2]) | (o[p2] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p2], o[p3], PointLight); sv.sideamount++; }
		a = (o[p4] - o[p3]) | (o[p3] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p3], o[p4], PointLight); sv.sideamount++; }
		a = (o[p5] - o[p4]) | (o[p4] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p4], o[p5], PointLight); sv.sideamount++; }
		a = (o[p0] - o[p5]) | (o[p5] - PointLight);
		if (a) { sv.oplanes[sv.nplanes + sv.sideamount] = Plane::CreatePlane(o[p5], o[p0], PointLight); sv.sideamount++; }
	}
}

inline bool NAABB_SV::Is_AABB_In_ShadowVolume(const Shadowvolume& sv, const AABB& Receiver)
{
	uint32 pa = sv.sideamount + sv.nplanes;

	f32 d;
	const Vec3* pAABB = &Receiver.min;

	union f32_u
	{
		float  floatVal;
		uint32 uintVal;
	};

	//! Check if receiver-AABB is in front of any of these planes.
	for (uint32 x = 0; x < pa; x++)
	{
		d = sv.oplanes[x].d;

		// Avoid breaking strict aliasing rules.
		f32_u ux;
		ux.floatVal = sv.oplanes[x].n.x;
		f32_u uy;
		uy.floatVal = sv.oplanes[x].n.y;
		f32_u uz;
		uz.floatVal = sv.oplanes[x].n.z;
		const uint32 bitX = ux.uintVal >> 31;
		const uint32 bitY = uy.uintVal >> 31;
		const uint32 bitZ = uz.uintVal >> 31;

		d += sv.oplanes[x].n.x * pAABB[bitX].x;
		d += sv.oplanes[x].n.y * pAABB[bitY].y;
		d += sv.oplanes[x].n.z * pAABB[bitZ].z;
		if (d > 0) return CULL_EXCLUSION;
	}
	return CULL_OVERLAP;
}

inline char NAABB_SV::Is_AABB_In_ShadowVolume_hierarchical(const Shadowvolume& sv, const AABB& Receiver)
{
	uint32 pa = sv.sideamount + sv.nplanes;
	const Vec3* pAABB = &Receiver.min;

	f32 dot1, dot2;
	uint32 notOverlap = 0x80000000; // Will be reset to 0 if there's at least one overlapping.

	union f32_u
	{
		float  floatVal;
		uint32 uintVal;
	};

	//! Check if receiver-AABB is in front of any of these planes.
	for (uint32 x = 0; x < pa; x++)
	{
		dot1 = dot2 = sv.oplanes[x].d;

		// Avoid breaking strict aliasing rules.
		f32_u ux;
		ux.floatVal = sv.oplanes[x].n.x;
		f32_u uy;
		uy.floatVal = sv.oplanes[x].n.y;
		f32_u uz;
		uz.floatVal = sv.oplanes[x].n.z;
		const uint32 bitX = ux.uintVal >> 31;
		const uint32 bitY = uy.uintVal >> 31;
		const uint32 bitZ = uz.uintVal >> 31;

		dot1 += sv.oplanes[x].n.x * pAABB[0 + bitX].x;
		dot2 += sv.oplanes[x].n.x * pAABB[1 - bitX].x;
		dot1 += sv.oplanes[x].n.y * pAABB[0 + bitY].y;
		dot2 += sv.oplanes[x].n.y * pAABB[1 - bitY].y;
		dot1 += sv.oplanes[x].n.z * pAABB[0 + bitZ].z;
		dot2 += sv.oplanes[x].n.z * pAABB[1 - bitZ].z;
		PREFAST_SUPPRESS_WARNING(6001) f32_u d;
		d.floatVal = dot1;
		if (!(d.uintVal & 0x80000000)) return CULL_EXCLUSION;
		PREFAST_SUPPRESS_WARNING(6001) f32_u d2;
		d2.floatVal = dot2;
		notOverlap &= d2.uintVal;
	}
	if (notOverlap) return CULL_INCLUSION;
	return CULL_OVERLAP;
}

#endif
