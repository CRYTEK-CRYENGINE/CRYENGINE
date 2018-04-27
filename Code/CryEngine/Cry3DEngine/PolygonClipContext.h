// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _3DENGINE_POLYGONCLIPCONTEXT_H_
#define _3DENGINE_POLYGONCLIPCONTEXT_H_

#if defined(LINUX)
	#include <CryCore/Platform/platform.h>
#endif

struct SPlaneObject
{
	Plane           plane;
	Vec3_tpl<uint8> vIdx1, vIdx2;

	void            Update()
	{
		//avoid breaking strict aliasing rules
		union f32_u
		{
			float  floatVal;
			uint32 uintVal;
		};
		f32_u ux;
		ux.floatVal = plane.n.x;
		f32_u uy;
		uy.floatVal = plane.n.y;
		f32_u uz;
		uz.floatVal = plane.n.z;
		uint32 bitX = ux.uintVal >> 31;
		uint32 bitY = uy.uintVal >> 31;
		uint32 bitZ = uz.uintVal >> 31;
		vIdx1.x = bitX * 3 + 0;
		vIdx2.x = (1 - bitX) * 3 + 0;
		vIdx1.y = bitY * 3 + 1;
		vIdx2.y = (1 - bitY) * 3 + 1;
		vIdx1.z = bitZ * 3 + 2;
		vIdx2.z = (1 - bitZ) * 3 + 2;
	}
};

ILINE bool IsABBBVisibleInFrontOfPlane_FAST(const AABB& objBox, const SPlaneObject& clipPlane)
{
	const f32* p = &objBox.min.x;
	if ((clipPlane.plane | Vec3(p[clipPlane.vIdx2.x], p[clipPlane.vIdx2.y], p[clipPlane.vIdx2.z])) > 0)
		return true;

	return false;
}

class CPolygonClipContext
{
public:
	CPolygonClipContext();

	void                  Reset();

	const PodArray<Vec3>& Clip(const PodArray<Vec3>& poly, const Plane* planes, size_t numPlanes);
	const PodArray<Vec3>& Clip(const Vec3& a, const Vec3& b, const Vec3& c, const Plane* planes, size_t numPlanes);

	void                  GetMemoryUsage(ICrySizer* pSizer) const;

private:
	static int  ClipEdge(const Vec3& v1, const Vec3& v2, const Plane& ClipPlane, Vec3& vRes1, Vec3& vRes2);
	static void ClipPolygon(PodArray<Vec3>& PolygonOut, const PodArray<Vec3>& pPolygon, const Plane& ClipPlane);
	PodArray<Vec3> m_lstPolygonA;
	PodArray<Vec3> m_lstPolygonB;
};

#endif // _3DENGINE_POLYGONCLIPCONTEXT_H_
