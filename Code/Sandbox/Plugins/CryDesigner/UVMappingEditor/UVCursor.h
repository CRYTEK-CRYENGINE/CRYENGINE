// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer {
namespace UVMapping
{

class UVCursor : public _i_reference_target_t
{
public:
	UVCursor() : m_Radius(0.01f), m_Pos(Vec2(-0.125f, -0.125f)) {}

	const Vec3& GetPos() const           { return m_Pos; }
	void        SetPos(const Vec3& pos)  { m_Pos = pos;  }
	void        SetRadius(float fRadius) { m_Radius = fRadius; }
	void        Draw(DisplayContext& dc);
	bool        IsPicked(const Ray& ray);
	void        Transform(const Matrix33& tm);

private:
	float m_Radius;
	Vec3  m_Pos;
};

typedef _smart_ptr<UVCursor> UVCursorPtr;

}
}

