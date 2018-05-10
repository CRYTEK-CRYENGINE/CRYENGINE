// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVCursor.h"

namespace Designer {
namespace UVMapping
{

void UVCursor::Draw(DisplayContext& dc)
{
	const float z = 0.01f;
	Vec3 pos(GetPos().x, GetPos().y, z);

	dc.SetColor(ColorB(255, 125, 125));
	dc.DrawBall(pos, m_Radius);

	float thronLength = m_Radius * 2;

	dc.SetColor(ColorB(125, 255, 125));
	dc.DrawLine(pos - Vec3(thronLength, 0, 0), pos + Vec3(thronLength, 0, 0));
	dc.DrawLine(pos - Vec3(0, thronLength, 0), pos + Vec3(0, thronLength, 0));
}

bool UVCursor::IsPicked(const Ray& ray)
{
	AABB aabb(GetPos(), m_Radius);
	Vec3 vHit;
	return Intersect::Ray_AABB(ray, aabb, vHit);
}

void UVCursor::Transform(const Matrix33& tm)
{
	m_Pos = tm.TransformVector(Vec3(m_Pos.x, m_Pos.y, 1.0f));
}

}
}

